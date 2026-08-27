// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics_2d/Font2D.hpp>

#include <schrift.h> // SFT..., sft_...

namespace grem::graphics {

Font2D::Font2D(const Filesystem& filesystem, CStringView filepath, const Font2DOptions& options)
	: atlasPacker(options.atlasPackerOptions)
	, options(options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load font {}", filepath));

	fontFileContents = filesystem.readInputFile(filepath);
	font.reset(sft_loadmem(fontFileContents.data(), fontFileContents.size()));
	if (!font) {
		throw graphics::Error{formatString("Failed to load font \"{}\".", filepath)};
	}
}

void Font2D::clearRenderedGlyphs() noexcept {
	atlasPacker = resource::AtlasPacker{options.atlasPackerOptions};
	atlasTexture = {};
	renderedGlyphs.clear();
}

Optional<Font2D::RenderedGlyphInfo> Font2D::findRenderedGlyphInfo(uint32_t characterSize, char32_t codePoint) const noexcept {
	const RenderedGlyphKey renderedGlyphKey{.characterSize = characterSize, .codePoint = codePoint};
	if (const auto it = renderedGlyphs.find(renderedGlyphKey); it != renderedGlyphs.end()) {
		return it->second;
	}
	return {};
}

Pair<Font2D::RenderedGlyphInfo, bool> Font2D::renderGlyph(Device& device, uint32_t characterSize, char32_t codePoint) {
	const RenderedGlyphKey renderedGlyphKey{.characterSize = characterSize, .codePoint = codePoint};
	const auto [it, inserted] = renderedGlyphs.emplace(renderedGlyphKey, RenderedGlyphInfo{});
	if (!inserted) {
		return {it->second, false};
	}

	GREM_PROFILE_FUNCTION();
	try {
		const SFT sft{
			.font = static_cast<SFT_Font*>(font.get()),
			.xScale = static_cast<double>(characterSize),
			.yScale = static_cast<double>(characterSize),
			.xOffset = 0.0,
			.yOffset = 0.0,
			.flags = SFT_DOWNWARD_Y,
		};

		SFT_Glyph glyph{};
		if (sft_lookup(&sft, SFT_UChar{codePoint}, &glyph) != 0) {
			throw graphics::Error{formatString("Failed to lookup font glyph for code point U+{:04X}", static_cast<uint32_t>(codePoint))};
		}

		SFT_GMetrics gmetrics{};
		sft_gmetrics(&sft, glyph, &gmetrics);

		const uint32_t width = static_cast<uint32_t>(gmetrics.minWidth);
		const uint32_t height = static_cast<uint32_t>(gmetrics.minHeight);
		const auto [offset, resized] = atlasPacker.insertRectangle(Extent2D{width, height});
		if (!atlasTexture || resized) {
			Texture oldAtlasTexture = std::move(atlasTexture);
			atlasTexture = Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R8_UNORM, Extent2D{atlasPacker.getResolution()}, 1, ClearValues{},
				TextureSamplerOptions{
					.minificationFilter = (options.useLinearFiltering) ? TextureFilter::LINEAR : TextureFilter::NEAREST,
					.magnificationFilter = (options.useLinearFiltering) ? TextureFilter::LINEAR : TextureFilter::NEAREST,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
				});
			if (oldAtlasTexture) {
				atlasTexture.pasteTexture(oldAtlasTexture);
			}
		}

		if (width > 0 && height > 0) {
			Allocation<byte> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
			if (sft_render(&sft, glyph, SFT_Image{.pixels = pixels.data(), .width = static_cast<int>(width), .height = static_cast<int>(height)}) != 0) {
				throw graphics::Error{formatString("Failed to render font glyph for code point U+{:04X}", static_cast<uint32_t>(codePoint))};
			}
			atlasTexture.pasteImage(Extent2D{.width = width, .height = height}, pixels.data(), offset);
		}

		const RenderedGlyphInfo renderedGlyphInfo{
			.positionInAtlas{static_cast<float>(offset.x), static_cast<float>(offset.y)},
			.sizeInAtlas{static_cast<float>(width), static_cast<float>(height)},
		};
		it->second = renderedGlyphInfo;
		return {renderedGlyphInfo, true};
	} catch (...) {
		renderedGlyphs.erase(it);
		throw;
	}
}

void Font2D::setOptions(Device& device, const Font2DOptions& newOptions) {
	(void)device;
	if (newOptions.useLinearFiltering != options.useLinearFiltering) {
		atlasTexture = atlasTexture.copyWithSamplerOptions(TextureSamplerOptions{
			.minificationFilter = (newOptions.useLinearFiltering) ? TextureFilter::LINEAR : TextureFilter::NEAREST,
			.magnificationFilter = (newOptions.useLinearFiltering) ? TextureFilter::LINEAR : TextureFilter::NEAREST,
			.mipmapMode = TextureMipmapMode::NONE,
			.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.maxAnisotropy = 1.0f,
		});
	}
	options = newOptions;
}

Font2D::GlyphMetrics Font2D::getGlyphMetrics(uint32_t characterSize, char32_t codePoint) const noexcept {
	const SFT sft{
		.font = static_cast<SFT_Font*>(font.get()),
		.xScale = static_cast<double>(characterSize),
		.yScale = static_cast<double>(characterSize),
		.xOffset = 0.0,
		.yOffset = 0.0,
		.flags = SFT_DOWNWARD_Y,
	};

	SFT_Glyph glyph{};
	if (sft_lookup(&sft, SFT_UChar{codePoint}, &glyph) != 0) {
		return {.size{0.0f, 0.0f}, .bearing{0.0f, 0.0f}, .advance = 0.0f};
	}

	SFT_GMetrics gmetrics{};
	sft_gmetrics(&sft, glyph, &gmetrics);

	const uint32_t width = static_cast<uint32_t>(gmetrics.minWidth);
	const uint32_t height = static_cast<uint32_t>(gmetrics.minHeight);
	return {
		.size{static_cast<float>(width), static_cast<float>(height)},
		.bearing{static_cast<float>(gmetrics.leftSideBearing), static_cast<float>(-gmetrics.minHeight - gmetrics.yOffset)},
		.advance = static_cast<float>(gmetrics.advanceWidth),
	};
}

Font2D::LineMetrics Font2D::getLineMetrics(uint32_t characterSize) const noexcept {
	const SFT sft{
		.font = static_cast<SFT_Font*>(font.get()),
		.xScale = static_cast<double>(characterSize),
		.yScale = static_cast<double>(characterSize),
		.xOffset = 0.0,
		.yOffset = 0.0,
		.flags = SFT_DOWNWARD_Y,
	};

	SFT_LMetrics lmetrics{};
	if (sft_lmetrics(&sft, &lmetrics) != 0) {
		return {.ascender = 0.0f, .descender = 0.0f, .height = 0.0f};
	}

	return {
		.ascender = static_cast<float>(lmetrics.ascender),
		.descender = static_cast<float>(lmetrics.descender),
		.height = static_cast<float>(lmetrics.ascender - lmetrics.descender + lmetrics.lineGap),
	};
}

vec2 Font2D::getKerning(uint32_t characterSize, char32_t left, char32_t right) const noexcept {
	const SFT sft{
		.font = static_cast<SFT_Font*>(font.get()),
		.xScale = static_cast<double>(characterSize),
		.yScale = static_cast<double>(characterSize),
		.xOffset = 0.0,
		.yOffset = 0.0,
		.flags = SFT_DOWNWARD_Y,
	};

	SFT_Glyph leftGlyph{};
	if (sft_lookup(&sft, SFT_UChar{left}, &leftGlyph) != 0) {
		return {0.0f, 0.0f};
	}

	SFT_Glyph rightGlyph{};
	if (sft_lookup(&sft, SFT_UChar{right}, &rightGlyph) != 0) {
		return {0.0f, 0.0f};
	}

	SFT_Kerning kerning{};
	if (sft_kerning(&sft, leftGlyph, rightGlyph, &kerning) != 0) {
		return {0.0f, 0.0f};
	}

	return {static_cast<float>(kerning.xShift), static_cast<float>(kerning.yShift)};
}

void Font2D::FontDeleter::operator()(void* handle) const noexcept {
	sft_freefont(static_cast<SFT_Font*>(handle));
}

} // namespace grem::graphics
