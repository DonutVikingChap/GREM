// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_FONT_2D_HPP
#define GREM_GRAPHICS_2D_FONT_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/resource/AtlasPacker.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

/**
 * Configuration options for a Font2D.
 */
struct Font2DOptions {
	/**
	 * Configuration options for the font's internal atlas packer.
	 */
	resource::AtlasPackerOptions atlasPackerOptions{
		.initialResolution = 128,
		.padding = 1,
		.alignment = 1,
	};

	/**
	 * Use bilinear filtering rather than nearest-neighbor interpolation when
	 * rendering text at a non-1:1 scale using this font.
	 *
	 * When set to true, this will cause scaled text to appear smoother compared
	 * to regular blocky nearest-neighbor scaling. Using linear filtering can
	 * help reduce aliasing artifacts on the glyph edges, but also makes the
	 * text more blurry.
	 *
	 * \note Regardless of this option, The best results are usually achieved
	 *       when text is rendered at an appropriate character size to begin
	 *       with, rather than relying on scaling.
	 */
	bool useLinearFiltering = false;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Font2DOptions& other) const = default;
};

/**
 * Typeface describing an assortment of character glyphs that may be rendered
 * on-demand into an expanding texture atlas, for use in Text2D rendering.
 */
class Font2D {
public:
	/**
	 * Information about a single glyph's entry in the texture atlas.
	 */
	struct RenderedGlyphInfo {
		vec2 positionInAtlas; ///< Position of this glyph's rectangle in the texture atlas, in texels.
		vec2 sizeInAtlas;     ///< Size of this glyph's rectangle in the texture atlas, in texels.
	};

	/**
	 * Dimensions of a single glyph in this font, for shaping text.
	 */
	struct GlyphMetrics {
		vec2 size;     ///< Size of this glyph's rectangle when rendered, in pixels.
		vec2 bearing;  ///< Offset from the baseline to apply to the glyph's rectangle position when rendering this glyph.
		float advance; ///< Horizontal offset to apply in order to advance to the next glyph position, excluding any kerning.
	};

	/**
	 * Vertical dimensions for shaping lines of text with this font.
	 */
	struct LineMetrics {
		float ascender;  ///< Vertical offset from the baseline to the visual top of the text.
		float descender; ///< Vertical offset from the baseline to the visual bottom of the text.
		float height;    ///< Vertical offset to apply in order to advance to the next line.
	};

	/**
	 * Load a font from a file.
	 *
	 * The supported file formats are:
	 * - TrueType (.ttf)
	 * - OpenType (.otf)
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the font file to load.
	 * \param options font options, see Font2DOptions.
	 *
	 * \throws File::Error on failure to open the file.
	 * \throws graphics::Error on failure to load a font from the file.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The only supported text encoding is Unicode.
	 * \note Only TrueType fonts are fully supported. OpenType extensions that
	 *       are not a part of TrueType may not work.
	 */
	GREM_API(graphics_2d) Font2D(const Filesystem& filesystem, CStringView filepath, const Font2DOptions& options = {});

	/** Destructor. */
	~Font2D() = default;

	/**
	 * Copying a font is not allowed, since the memory location of the file
	 * contents must remain stable.
	 */
	Font2D(const Font2D&) = delete;

	/** Move constructor. */
	Font2D(Font2D&& other) noexcept = default;

	/**
	 * Copying a font is not allowed, since the memory location of the file
	 * contents must remain stable.
	 */
	Font2D& operator=(const Font2D& other) = delete;

	/** Move assignment. */
	Font2D& operator=(Font2D&& other) noexcept = default;

	/**
	 * Remove all rendered glyphs in the font and reset the texture atlas.
	 */
	void clearRenderedGlyphs() noexcept;

	/**
	 * Look up the information about a glyph's entry in the texture atlas for a
	 * specific code point.
	 *
	 * \param characterSize character size of the glyph to search for.
	 * \param codePoint Unicode code point of the glyph to search for.
	 *
	 * \return the rendered glyph information, see RenderedGlyphInfo, or an
	 *         empty optional if the glyph has not been rendered.
	 *
	 * \sa renderGlyph()
	 * \sa getAtlasTexture()
	 */
	[[nodiscard]] GREM_API(graphics_2d) Optional<RenderedGlyphInfo> findRenderedGlyphInfo(uint32_t characterSize, char32_t codePoint) const noexcept;

	/**
	 * Render the glyph for a specific character and store it in the texture
	 * atlas, if it has not already been rendered.
	 *
	 * \param device device to use for rendering the glyph. Must outlive the
	 *        font.
	 * \param characterSize character size to render the glyph at.
	 * \param codePoint Unicode code point of the glyph to render.
	 *
	 * \return a pair of:
	 *         - information about the rendered glyph, and
	 *         - a bool that is true if the glyph was actually rendered, or
	 *           false if the glyph had already been rendered previously.
	 *
	 * \throws graphics::Error on failure to render the glyph.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the specified glyph has already been rendered previously, no
	 *       modification is made, and the already rendered glyph is returned.
	 *       In this case, the function is guaranteed to not throw any
	 *       exceptions.
	 *
	 * \sa findRenderedGlyphInfo()
	 * \sa getGlyphMetrics()
	 * \sa getAtlasTexture()
	 */
	GREM_API(graphics_2d) Pair<RenderedGlyphInfo, bool> renderGlyph(Device& device, uint32_t characterSize, char32_t codePoint);

	/**
	 * Update the configuration options of this font and recreate the texture
	 * atlas if necessary to apply the new sampler options.
	 *
	 * \param device device to use for the texture atlas. Must outlive the font.
	 * \param newOptions new font options, see Font2DOptions.
	 *
	 * \throws graphics::Error on failure to render the glyphs.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa getOptions()
	 */
	GREM_API(graphics_2d) void setOptions(Device& device, const Font2DOptions& newOptions);

	/**
	 * Get the dimensions of a single glyph in this font, for shaping text.
	 *
	 * \param characterSize character size to get the glyph metrics of.
	 * \param codePoint Unicode code point to get the glyph metrics of.
	 *
	 * \return the glyph metrics of the given code point at the given character
	 *         size, see GlyphMetrics.
	 *
	 * \sa findRenderedGlyphInfo()
	 * \sa renderGlyph()
	 */
	[[nodiscard]] GREM_API(graphics_2d) GlyphMetrics getGlyphMetrics(uint32_t characterSize, char32_t codePoint) const noexcept;

	/**
	 * Get the vertical dimensions for shaping lines of text with this font.
	 *
	 * \param characterSize character size to get the line metrics of.
	 *
	 * \return the line metrics at the given character size, see LineMetrics.
	 */
	[[nodiscard]] GREM_API(graphics_2d) LineMetrics getLineMetrics(uint32_t characterSize) const noexcept;

	/**
	 * Get the kerning offset to use between a pair of adjacent character glyphs
	 * while shaping text.
	 *
	 * \param characterSize character size of the glyphs to get the kerning of.
	 * \param left Unicode code point of the left glyph in the adjacent pair.
	 * \param right Unicode code point of the right glyph in the adjacent pair.
	 *
	 * \return if the font contains a valid glyph for both the left and the
	 *         right characters, returns the additional offset to advance the
	 *         position by when going from the left glyph to the right glyph.
	 *         Otherwise, returns (0, 0).
	 */
	[[nodiscard]] GREM_API(graphics_2d) vec2 getKerning(uint32_t characterSize, char32_t left, char32_t right) const noexcept;

	/**
	 * Get the texture atlas to use when rendering glyphs from this font.
	 *
	 * \return a read-only reference to a square texture containing all loaded
	 *         glyphs.
	 *
	 * \sa findRenderedGlyphInfo()
	 * \sa renderGlyph()
	 */
	[[nodiscard]] const Texture& getAtlasTexture() const noexcept {
		return atlasTexture;
	}

	/**
	 * Get the current configuration options of the font.
	 *
	 * \return the current font options.
	 *
	 * \sa setOptions()
	 */
	[[nodiscard]] Font2DOptions getOptions() const noexcept {
		return options;
	}

private:
	struct FontDeleter {
		GREM_API(graphics_2d) void operator()(void* handle) const noexcept;
	};

	struct RenderedGlyphKey {
		uint32_t characterSize;
		char32_t codePoint;

		[[nodiscard]] constexpr auto operator<=>(const RenderedGlyphKey&) const = default;
	};

	Allocation<byte> fontFileContents{};
	UniqueHandle<void*, FontDeleter> font{};
	resource::AtlasPacker atlasPacker;
	Texture atlasTexture{};
	OrderedMap<RenderedGlyphKey, RenderedGlyphInfo> renderedGlyphs{};
	Font2DOptions options;
};

} // namespace grem::graphics

#endif
