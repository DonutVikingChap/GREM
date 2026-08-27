// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_TILESETS_HPP
#define GREM_EXAMPLES_TILES_TILESETS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/graphics.hpp>
#include <GREM/resource.hpp>

struct TilesetOptions {
	Extent2D tileSizeInPixels;
	Extent2D tileGapInPixels;
	bool useMipmap;
};

struct TilesetsOptions {
	Extent2D maxTilesetSizeInPixels;
};

class Tilesets {
public:
	struct TilesetInfo {
		Extent2D tilesetSizeInPixels;
		Extent2D tileSizeInPixels;
		Extent2D tileGapInPixels;
		bool useMipmap;
	};

	using Index = uint8_t;

	static constexpr Index MAX_TILESET_COUNT = 16;
	static constexpr res::ImageFormat IMAGE_FORMAT = res::ImageFormat::R8G8B8A8_UINT;
	static constexpr gfx::TextureFormat TEXTURE_FORMAT = gfx::Texture::getInternalFormat(IMAGE_FORMAT, Color::TransferFunction::SRGB);

	Tilesets(gfx::Device& device, const TilesetsOptions& options)
		: options(options)
		, tilesetArrayTexture(gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D_ARRAY, TEXTURE_FORMAT,
			  Extent3D{options.maxTilesetSizeInPixels.width, options.maxTilesetSizeInPixels.height, MAX_TILESET_COUNT},
			  res::Image::getMaxMipLevelCount(options.maxTilesetSizeInPixels), gfx::ClearValues{}, gfx::TextureSamplerOptions::UNFILTERED)) {}

	void clear() {
		tilesetArrayTexture.fill(gfx::ClearValues{});
		tilesets.clear();
	}

	[[nodiscard]] Index loadTileset(const Filesystem& filesystem, CStringView filepath, const TilesetOptions& tilesetOptions) {
		GREM_PROFILE_FUNCTION();

		const Extent2D tileSpacingInPixels = tilesetOptions.tileSizeInPixels + tilesetOptions.tileGapInPixels;
		if (tilesetOptions.useMipmap && (!isPowerOf2(tileSpacingInPixels.width) || !isPowerOf2(tileSpacingInPixels.height))) {
			throw Error{"Tileset cannot use mipmap with non-power-of-2 tile spacing."};
		}

		if (tilesets.size() >= MAX_TILESET_COUNT) {
			throw Error{"Maximum tileset count exceeded."};
		}
		res::Image image{filesystem, filepath, {.requiredType = res::ImageType::IMAGE_2D, .requiredFormat = IMAGE_FORMAT}};
		image.transformFromStraightToPremultipliedAlpha(Color::TransferFunction::SRGB);
		const Index tilesetIndex = static_cast<Index>(tilesets.size());
		tilesetArrayTexture.pasteImage(image.getSize2D(), image.data(), {0, 0, tilesetIndex});
		tilesets.push_back(TilesetInfo{
			.tilesetSizeInPixels = image.getSize2D(),
			.tileSizeInPixels = tilesetOptions.tileSizeInPixels,
			.tileGapInPixels = tilesetOptions.tileGapInPixels,
			.useMipmap = tilesetOptions.useMipmap,
		});
		return tilesetIndex;
	}

	[[nodiscard]] const gfx::Texture& getTilesetArrayTexture() const noexcept {
		return tilesetArrayTexture;
	}

	[[nodiscard]] Index size() const noexcept {
		return static_cast<Index>(tilesets.size());
	}

	[[nodiscard]] const TilesetInfo& operator[](Index tilesetIndex) const {
		return tilesets[tilesetIndex];
	}

private:
	TilesetsOptions options;
	gfx::Texture tilesetArrayTexture{};
	InplaceArrayList<TilesetInfo, MAX_TILESET_COUNT> tilesets{};
};

#endif
