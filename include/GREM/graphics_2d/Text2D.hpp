// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_TEXT_2D_HPP
#define GREM_GRAPHICS_2D_TEXT_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>

#include <new> // std::launder

namespace grem::graphics {

class Font2D; // Forward declaration, to avoid including Font2D.hpp.

/**
 * Horizontal text alignment mode.
 */
enum class TextAlignHorizontal : uint8_t {
	NONE,             ///< No horizontal alignment.
	FIRST_LINE_START, ///< Align to the start of the first glyph in the first line of the text.
	FIRST_LINE_END,   ///< Align to the end of the last glyph in the first line of the text.
	LAST_LINE_START,  ///< Align to the start of the first glyph in the last line of the text.
	LAST_LINE_END,    ///< Align to the end of the last glyph in the last line of the text.
	LEFT,             ///< Align to the left of the bounding box of the text.
	CENTER,           ///< Align to the horizontal center of the bounding box of the text.
	RIGHT,            ///< Align to the right of the bounding box of the text.
};

/**
 * Vertical text alignment mode.
 */
enum class TextAlignVertical : uint8_t {
	NONE,            ///< No vertical alignment.
	FIRST_LINE_TOP,  ///< Align to the ascender of the first line in the text.
	FIRST_LINE_BASE, ///< Align to the baseline of the first line in the text.
	LAST_LINE_TOP,   ///< Align to the ascender of the last line in the text.
	LAST_LINE_BASE,  ///< Align to the baseline of the last line in the text.
	BOTTOM,          ///< Align to the bottom of the bounding box of the text.
	CENTER,          ///< Align to the vertical center of the bounding box of the text.
	TOP,             ///< Align to the top of the bounding box of the text.
};

/**
 * Text alignment mode.
 */
struct TextAlign {
	static const TextAlign NONE;                                ///< No alignment.
	static const TextAlign FIRST_LINE_START_TOP;                ///< Align to the start of the first line's ascender.
	static const TextAlign FIRST_LINE_START_BASE;               ///< Align to the start of the first line's baseline.
	static const TextAlign FIRST_LINE_END_TOP;                  ///< Align to the end of the first line's ascender.
	static const TextAlign FIRST_LINE_END_BASE;                 ///< Align to the end of the first line's baseline.
	static const TextAlign LAST_LINE_START_TOP;                 ///< Align to the start of the last line's ascender.
	static const TextAlign LAST_LINE_START_BASE;                ///< Align to the start of the last line's baseline.
	static const TextAlign LAST_LINE_END_TOP;                   ///< Align to the end of the last line's ascender.
	static const TextAlign LAST_LINE_END_BASE;                  ///< Align to the end of the last line's baseline.
	static const TextAlign CENTER_HORIZONTALLY_FIRST_LINE_TOP;  ///< Align to the horizontal center of the bounding box on the first line's ascender.
	static const TextAlign CENTER_HORIZONTALLY_FIRST_LINE_BASE; ///< Align to the horizontal center of the bounding box on the first line's baseline.
	static const TextAlign CENTER_HORIZONTALLY_LAST_LINE_TOP;   ///< Align to the horizontal center of the bounding box on the last line's ascender.
	static const TextAlign CENTER_HORIZONTALLY_LAST_LINE_BASE;  ///< Align to the horizontal center of the bounding box on the last line's baseline.
	static const TextAlign CENTER;                              ///< Align to the center of the bounding box of the text.
	static const TextAlign CENTER_HORIZONTALLY;                 ///< Align to the horizontal center of the bounding box of the text.
	static const TextAlign CENTER_VERTICALLY;                   ///< Align to the vertical center of the bounding box of the text.
	static const TextAlign CENTER_HORIZONTALLY_BOTTOM;          ///< Align to the bottom center of the bounding box of the text.
	static const TextAlign CENTER_HORIZONTALLY_TOP;             ///< Align to the top center of the bounding box of the text.
	static const TextAlign CENTER_VERTICALLY_LEFT;              ///< Align to the left center of the bounding box of the text.
	static const TextAlign CENTER_VERTICALLY_RIGHT;             ///< Align to the right center of the bounding box of the text.
	static const TextAlign LEFT;                                ///< Align to the left of the bounding box of the text.
	static const TextAlign RIGHT;                               ///< Align to the right of the bounding box of the text.
	static const TextAlign BOTTOM;                              ///< Align to the bottom of the bounding box of the text.
	static const TextAlign TOP;                                 ///< Align to the top of the bounding box of the text.

	TextAlignHorizontal horizontal = TextAlignHorizontal::NONE; ///< Horizontal alignment mode.
	TextAlignVertical vertical = TextAlignVertical::NONE;       ///< Vertical alignment mode.
};

inline constexpr TextAlign TextAlign::NONE{
	.horizontal = TextAlignHorizontal::NONE,
	.vertical = TextAlignVertical::NONE,
};

inline constexpr TextAlign TextAlign::FIRST_LINE_START_TOP{
	.horizontal = TextAlignHorizontal::FIRST_LINE_START,
	.vertical = TextAlignVertical::FIRST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::FIRST_LINE_START_BASE{
	.horizontal = TextAlignHorizontal::FIRST_LINE_START,
	.vertical = TextAlignVertical::FIRST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::FIRST_LINE_END_TOP{
	.horizontal = TextAlignHorizontal::FIRST_LINE_END,
	.vertical = TextAlignVertical::FIRST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::FIRST_LINE_END_BASE{
	.horizontal = TextAlignHorizontal::FIRST_LINE_END,
	.vertical = TextAlignVertical::FIRST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::LAST_LINE_START_TOP{
	.horizontal = TextAlignHorizontal::LAST_LINE_START,
	.vertical = TextAlignVertical::LAST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::LAST_LINE_START_BASE{
	.horizontal = TextAlignHorizontal::LAST_LINE_START,
	.vertical = TextAlignVertical::LAST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::LAST_LINE_END_TOP{
	.horizontal = TextAlignHorizontal::LAST_LINE_END,
	.vertical = TextAlignVertical::LAST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::LAST_LINE_END_BASE{
	.horizontal = TextAlignHorizontal::LAST_LINE_END,
	.vertical = TextAlignVertical::LAST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::FIRST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_BASE{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::FIRST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_LAST_LINE_TOP{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::LAST_LINE_TOP,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_LAST_LINE_BASE{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::LAST_LINE_BASE,
};

inline constexpr TextAlign TextAlign::CENTER{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::CENTER,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::NONE,
};

inline constexpr TextAlign TextAlign::CENTER_VERTICALLY{
	.horizontal = TextAlignHorizontal::NONE,
	.vertical = TextAlignVertical::CENTER,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_BOTTOM{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::BOTTOM,
};

inline constexpr TextAlign TextAlign::CENTER_HORIZONTALLY_TOP{
	.horizontal = TextAlignHorizontal::CENTER,
	.vertical = TextAlignVertical::TOP,
};

inline constexpr TextAlign TextAlign::CENTER_VERTICALLY_LEFT{
	.horizontal = TextAlignHorizontal::LEFT,
	.vertical = TextAlignVertical::CENTER,
};

inline constexpr TextAlign TextAlign::CENTER_VERTICALLY_RIGHT{
	.horizontal = TextAlignHorizontal::RIGHT,
	.vertical = TextAlignVertical::CENTER,
};

inline constexpr TextAlign TextAlign::LEFT{
	.horizontal = TextAlignHorizontal::LEFT,
	.vertical = TextAlignVertical::NONE,
};

inline constexpr TextAlign TextAlign::RIGHT{
	.horizontal = TextAlignHorizontal::RIGHT,
	.vertical = TextAlignVertical::NONE,
};

inline constexpr TextAlign TextAlign::BOTTOM{
	.horizontal = TextAlignHorizontal::NONE,
	.vertical = TextAlignVertical::BOTTOM,
};

inline constexpr TextAlign TextAlign::TOP{
	.horizontal = TextAlignHorizontal::NONE,
	.vertical = TextAlignVertical::TOP,
};

/**
 * Facility for shaping text, according to a Font2D, into renderable glyphs.
 */
class Text2D {
public:
	/**
	 * Data required to render a single shaped glyph relative to at any given
	 * starting position.
	 *
	 * \sa ShapedGlyphInfo
	 * \sa ShapedLineInfo
	 */
	struct ShapedGlyph {
		Font2D* font;           ///< Non-owning read-only non-null pointer to the font used to shape this glyph.
		vec2 shapedOffset;      ///< Scaled offset from the starting position to draw this glyph at, in pixels.
		vec2 shapedSize;        ///< Scaled size of this glyph's rectangle, in pixels.
		uint32_t characterSize; ///< Character size that this glyph was shaped at.
		char32_t codePoint;     ///< Unicode code point of this glyph.
	};

	/**
	 * Additional information about a single shaped glyph, including some data
	 * that is not strictly required for simple rendering.
	 *
	 * \sa ShapedGlyph
	 * \sa ShapedLineInfo
	 */
	struct ShapedGlyphInfo {
		vec2 shapedOffset;      ///< Scaled offset from the starting position to draw this glyph at, in pixels.
		vec2 shapedAdvance;     ///< Scaled offset to apply in order to advance to the next glyph position, including kerning.
		size_t shapedLineIndex; ///< Index of the ShapedLineInfo corresponding to the line that this glyph is part of.
		size_t stringOffset;    ///< Byte offset in the input string of the first code unit that this glyph originated from.
	};

	/**
	 * Information about a line of shaped glyphs, including some data that is
	 * not strictly required for simple rendering.
	 *
	 * \sa ShapedGlyph
	 * \sa ShapedGlyphInfo
	 */
	struct ShapedLineInfo {
		vec2 shapedOffset;        ///< Scaled offset of the baseline at the start of this line of text.
		vec2 shapedSize;          ///< Scaled total size of this line.
		float shapedAscender;     ///< Scaled total ascender of this line.
		float shapedDescender;    ///< Scaled total descender of this line.
		size_t shapedGlyphOffset; ///< Index of the ShapedGlyph and ShapedGlyphInfo corresponding to the first glyph that is part of this line.
		size_t stringOffset;      ///< Byte offset in the input string of the first code unit that the first glyph that is part of this line originated from.
	};

	/**
	 * Result of the text shaping functions.
	 */
	struct ShapeResult {
		/**
		 * Index, into the lists returned by getShapedGlyphs() and
		 * getShapedGlyphsInfo(), of the ShapedGlyph and ShapedGlyphInfo
		 * corresponding to the first glyph that was shaped.
		 *
		 * If no glyphs were shaped, this is the index that the first glyph
		 * would have had if it was shaped, i.e. the previous size of the lists.
		 */
		size_t shapedGlyphOffset;

		/**
		 * Index, into the list returned by getShapedLinesInfo(), of the
		 * ShapedLineInfo corresponding to the first line that was shaped.
		 */
		size_t shapedLineOffset;
	};

	/**
	 * Construct an empty text.
	 */
	Text2D() noexcept = default;

	/**
	 * Construct a shaped text from a UTF-8 string.
	 *
	 * \param font font to shape the glyphs with.
	 * \param characterSize character size to shape the glyphs at.
	 * \param string UTF-8 encoded text string to shape.
	 * \param offset relative offset from the starting position to begin shaping
	 *        at.
	 * \param scale scaling to apply to the size of the shaped glyphs. The
	 *        result is affected by Font2DOptions::useLinearFiltering.
	 *
	 * \throws graphics::Error on failure to shape a glyph.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be shaped
	 *       incorrectly. Only one Unicode code point is shaped at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \remark The best visual results are usually achieved when the text is
	 *         shaped at an appropriate character size to begin with, rather
	 *         than relying on the scaling of this function. As such, the scale
	 *         parameter should generally be kept at (1, 1) unless many
	 *         different character sizes are used with this font and there is a
	 *         strict requirement on the maximum size of the texture atlas.
	 *
	 * \sa assign()
	 * \sa append()
	 */
	Text2D(Font2D& font, uint32_t characterSize, UTF8StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f}) {
		append(font, characterSize, string, offset, scale);
	}

	/**
	 * Helper overload of Text2D() that takes an arbitrary byte string and
	 * interprets it as UTF-8.
	 *
	 * \sa Text2D(const Font2D&, uint32_t, UTF8StringView, vec2, vec2)
	 */
	Text2D(Font2D& font, uint32_t characterSize, StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f}) {
		append(font, characterSize, string, offset, scale);
	}

	/**
	 * Erase all shaped glyphs and reset the text to an empty state.
	 */
	void clear() noexcept {
		shapedGlyphs.clear();
		shapedGlyphsInfo.clear();
		shapedLinesInfo.clear();
		boundingBox = {};
	}

	/**
	 * Helper function that is equivalent to clear() followed by append().
	 *
	 * \sa clear()
	 * \sa append()
	 */
	ShapeResult assign(Font2D& font, uint32_t characterSize, StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f}) {
		clear();
		return append(font, characterSize, string, offset, scale);
	}

	/**
	 * Helper function that is equivalent to clear() followed by append().
	 *
	 * \sa clear()
	 * \sa append()
	 */
	ShapeResult assign(Font2D& font, uint32_t characterSize, UTF8StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f}) {
		clear();
		return append(font, characterSize, string, offset, scale);
	}

	/**
	 * Use a font to shape a string of UTF-8 encoded text into a sequence of
	 * glyphs that are ready to be drawn at a given offset, relative to any
	 * starting position.
	 *
	 * \param font font to shape the glyphs with.
	 * \param characterSize character size to shape the glyphs at.
	 * \param string UTF-8 encoded text string to shape.
	 * \param offset relative offset from the starting position to begin shaping
	 *        at.
	 * \param scale scaling to apply to the size of the shaped glyphs. The
	 *        result is affected by Font2DOptions::useLinearFiltering.
	 *
	 * \return see ShapeResult.
	 *
	 * \throws graphics::Error on failure to shape a glyph.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be shaped
	 *       incorrectly. Only one Unicode code point is shaped at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \remark The best visual results are usually achieved when the text is
	 *         shaped at an appropriate character size to begin with, rather
	 *         than relying on the scaling of this function. As such, the scale
	 *         parameter should generally be kept at (1, 1) unless many
	 *         different character sizes are used with this font and there is a
	 *         strict requirement on the maximum size of the texture atlas.
	 *
	 * \sa getShapedGlyphs()
	 * \sa getShapedGlyphsInfo()
	 * \sa getShapedLinesInfo()
	 * \sa getBoundingBox()
	 */
	GREM_API(graphics_2d) ShapeResult append(Font2D& font, uint32_t characterSize, UTF8StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f});

	/**
	 * Helper overload of append() that takes an arbitrary byte string and
	 * interprets it as UTF-8.
	 *
	 * \sa append(Font2D&, uint32_t, UTF8StringView, vec2, vec2)
	 */
	ShapeResult append(Font2D& font, uint32_t characterSize, StringView string, vec2 offset = {0.0f, 0.0f}, vec2 scale = {1.0f, 1.0f}) {
		static_assert(sizeof(char) == sizeof(char8_t));
		static_assert(alignof(char) == alignof(char8_t));
		return append(font, characterSize, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()}, offset, scale);
	}

	/**
	 * Get the list of ShapedGlyph data for all shaped glyphs.
	 *
	 * \return a non-owning read-only random-access view over the ShapedGlyph
	 *         data.
	 *
	 * \sa assign()
	 * \sa append()
	 * \sa getShapedGlyphsInfo()
	 * \sa getShapedLinesInfo()
	 */
	[[nodiscard]] Span<const ShapedGlyph> getShapedGlyphs() const noexcept {
		return shapedGlyphs;
	}

	/**
	 * Get the list of ShapedGlyphInfo data for all shaped glyphs.
	 *
	 * \return a non-owning read-only random-access view over the
	 *         ShapedGlyphInfo data.
	 *
	 * \sa assign()
	 * \sa append()
	 * \sa getShapedGlyphs()
	 * \sa getShapedLinesInfo()
	 */
	[[nodiscard]] Span<const ShapedGlyphInfo> getShapedGlyphsInfo() const noexcept {
		return shapedGlyphsInfo;
	}

	/**
	 * Get the list of ShapedLineInfo data for all shaped lines.
	 *
	 * \return a non-owning read-only random-access view over the ShapedLineInfo
	 *         data.
	 *
	 * \sa assign()
	 * \sa append()
	 * \sa getShapedGlyphs()
	 * \sa getShapedGlyphsInfo()
	 */
	[[nodiscard]] Span<const ShapedLineInfo> getShapedLinesInfo() const noexcept {
		return shapedLinesInfo;
	}

	/**
	 * Get the scaled bounding box of the shaped text.
	 *
	 * \return the smallest axis-aligned box that spans all shaped glyph
	 *         rectangles of this text, relative to the start of the baseline.
	 */
	[[nodiscard]] Box<2, float> getBoundingBox() const noexcept {
		return boundingBox;
	}

	/**
	 * Get the scaled offset where the next line would have ended up if another
	 * line had followed the last shaped line of text.
	 *
	 * \return the next line offset.
	 */
	[[nodiscard]] vec2 getNextLineOffset() const noexcept {
		if (!shapedLinesInfo.empty()) {
			const ShapedLineInfo& lastLine = shapedLinesInfo.back();
			return {
				lastLine.shapedOffset.x,
				lastLine.shapedOffset.y - lastLine.shapedSize.y,
			};
		}
		return {};
	}

	/**
	 * Get the scaled horizontal offset to apply when rendering the text with a
	 * specific horizontal alignment.
	 *
	 * \param horizontalAlignment horizontal alignment mode.
	 *
	 * \return the horizontal offset to apply to each rendered glyph.
	 */
	[[nodiscard]] float getHorizontalAlignmentOffset(TextAlignHorizontal horizontalAlignment) const {
		switch (horizontalAlignment) {
			case TextAlignHorizontal::NONE: break;
			case TextAlignHorizontal::FIRST_LINE_START:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& firstLine = shapedLinesInfo.front();
					return round(-firstLine.shapedOffset.x);
				}
				break;
			case TextAlignHorizontal::FIRST_LINE_END:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& firstLine = shapedLinesInfo.front();
					return round(-(firstLine.shapedOffset.x + firstLine.shapedSize.x));
				}
				break;
			case TextAlignHorizontal::LAST_LINE_START:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& lastLine = shapedLinesInfo.back();
					return round(-lastLine.shapedOffset.x);
				}
				break;
			case TextAlignHorizontal::LAST_LINE_END:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& lastLine = shapedLinesInfo.back();
					return round(-(lastLine.shapedOffset.x + lastLine.shapedSize.x));
				}
				break;
			case TextAlignHorizontal::LEFT: return round(-boundingBox.min.x);
			case TextAlignHorizontal::CENTER: return round(-0.5f * (boundingBox.max.x - boundingBox.min.x) - boundingBox.min.x);
			case TextAlignHorizontal::RIGHT: return round(-boundingBox.max.x);
		}
		return 0.0f;
	}

	/**
	 * Get the scaled vertical offset to apply when rendering the text with a
	 * specific vertical alignment.
	 *
	 * \param verticalAlignment vertical alignment mode.
	 *
	 * \return the vertical offset to apply to each rendered glyph.
	 */
	[[nodiscard]] float getVerticalAlignmentOffset(TextAlignVertical verticalAlignment) const {
		switch (verticalAlignment) {
			case TextAlignVertical::NONE: break;
			case TextAlignVertical::FIRST_LINE_TOP:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& firstLine = shapedLinesInfo.front();
					return round(-(firstLine.shapedOffset.y + firstLine.shapedAscender));
				}
				break;
			case TextAlignVertical::FIRST_LINE_BASE:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& firstLine = shapedLinesInfo.front();
					return round(-firstLine.shapedOffset.y);
				}
				break;
			case TextAlignVertical::LAST_LINE_TOP:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& lastLine = shapedLinesInfo.back();
					return round(-(lastLine.shapedOffset.y + lastLine.shapedAscender));
				}
				break;
			case TextAlignVertical::LAST_LINE_BASE:
				if (!shapedLinesInfo.empty()) {
					const ShapedLineInfo& lastLine = shapedLinesInfo.back();
					return round(-lastLine.shapedOffset.y);
				}
				break;
			case TextAlignVertical::BOTTOM: return round(-boundingBox.min.y);
			case TextAlignVertical::CENTER: return round(-0.5f * (boundingBox.max.y - boundingBox.min.y) - boundingBox.min.y);
			case TextAlignVertical::TOP: return round(-boundingBox.max.y);
		}
		return 0.0f;
	}

	/**
	 * Get the scaled offset to apply when rendering the text with a specific
	 * alignment.
	 *
	 * \param alignment alignment mode.
	 *
	 * \return the offset to apply to each rendered glyph.
	 */
	[[nodiscard]] vec2 getAlignmentOffset(TextAlign alignment) const noexcept {
		return {getHorizontalAlignmentOffset(alignment.horizontal), getVerticalAlignmentOffset(alignment.vertical)};
	}

private:
	Buffer<ShapedGlyph> shapedGlyphs{};
	Buffer<ShapedGlyphInfo> shapedGlyphsInfo{};
	Buffer<ShapedLineInfo> shapedLinesInfo{};
	Box<2, float> boundingBox{};
};

} // namespace grem::graphics

#endif
