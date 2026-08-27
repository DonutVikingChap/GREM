// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/unicode.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>

namespace grem::graphics {

Text2D::ShapeResult Text2D::append(Font2D& font, uint32_t characterSize, UTF8StringView string, vec2 offset, vec2 scale) {
	const size_t baseShapedGlyphOffset = shapedGlyphsInfo.size();
	const size_t baseShapedLineOffset = shapedLinesInfo.size();
	const Box<2, float> previousBoundingBox = boundingBox;
	try {
		const float startX = offset.x;
		const Font2D::LineMetrics lineMetrics = font.getLineMetrics(characterSize);
		const float lineAscender = lineMetrics.ascender * scale.y;
		const float lineDescender = lineMetrics.descender * scale.y;
		const float lineHeight = round(lineMetrics.height * scale.y);

		shapedLinesInfo.push_back(Text2D::ShapedLineInfo{
			.shapedOffset = offset,
			.shapedSize{0.0f, lineHeight},
			.shapedAscender = lineAscender,
			.shapedDescender = lineDescender,
			.shapedGlyphOffset = 0,
			.stringOffset = 0,
		});

		const unicode::UTF8View codePoints{string};
		for (auto it = codePoints.begin(); it != codePoints.end();) {
			const size_t stringOffset = static_cast<size_t>(it.base() - codePoints.begin().base());
			if (const char32_t codePoint = *it++; codePoint == '\n') {
				const float lineWidth = floor(offset.x - startX);
				shapedLinesInfo.back().shapedSize.x = lineWidth;

				offset.x = startX;
				offset.y -= lineHeight;
				shapedLinesInfo.push_back(Text2D::ShapedLineInfo{
					.shapedOffset = offset,
					.shapedSize{0.0f, lineHeight},
					.shapedAscender = lineAscender,
					.shapedDescender = lineDescender,
					.shapedGlyphOffset = shapedGlyphsInfo.size(),
					.stringOffset = static_cast<size_t>(it.base() - codePoints.begin().base()),
				});
			} else {
				const Font2D::GlyphMetrics& glyphMetrics = font.getGlyphMetrics(characterSize, codePoint);
				const vec2 kerning = font.getKerning(characterSize, codePoint, (it == codePoints.end()) ? char32_t{0} : *it);
				const vec2 shapedOffset = floor(offset + glyphMetrics.bearing * scale);
				const vec2 shapedSize = glyphMetrics.size * scale;
				const vec2 shapedAdvance = vec2{glyphMetrics.advance + kerning.x, kerning.y} * scale;

				const vec2 shapedMin{offset.x, offset.y + lineDescender};
				const vec2 shapedMax{offset.x + shapedAdvance.x, offset.y + lineAscender};
				if (shapedGlyphs.empty()) {
					boundingBox.min = shapedMin;
					boundingBox.max = shapedMax;
				} else {
					boundingBox.min = min(boundingBox.min, shapedMin);
					boundingBox.max = max(boundingBox.max, shapedMax);
				}

				shapedGlyphs.push_back(Text2D::ShapedGlyph{
					.font = &font,
					.shapedOffset = shapedOffset,
					.shapedSize = shapedSize,
					.characterSize = characterSize,
					.codePoint = codePoint,
				});
				shapedGlyphsInfo.push_back(Text2D::ShapedGlyphInfo{
					.shapedOffset = shapedOffset,
					.shapedAdvance = shapedAdvance,
					.shapedLineIndex = shapedLinesInfo.size() - 1,
					.stringOffset = stringOffset,
				});

				offset += shapedAdvance;
			}
		}

		const float lineWidth = floor(offset.x - startX);
		shapedLinesInfo.back().shapedSize.x = lineWidth;
	} catch (...) {
		shapedGlyphs.resize(baseShapedGlyphOffset);
		shapedGlyphsInfo.resize(baseShapedGlyphOffset);
		shapedLinesInfo.resize(baseShapedLineOffset);
		boundingBox = previousBoundingBox;
		throw;
	}
	return {
		.shapedGlyphOffset = baseShapedGlyphOffset,
		.shapedLineOffset = baseShapedLineOffset,
	};
}

} // namespace grem::graphics
