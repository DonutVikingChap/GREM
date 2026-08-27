// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_ATLAS_PACKER_HPP
#define GREM_RESOURCE_ATLAS_PACKER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::resource {

/**
 * Configuration options for an AtlasPacker.
 */
struct AtlasPackerOptions {
	/**
	 * Initial width of the square atlas region, in pixels.
	 *
	 * \warning Must be positive.
	 */
	uint32_t initialResolution = 128;

	/**
	 * Empty space to reserve between inserted rectangles, in pixels.
	 */
	uint32_t padding = 0;

	/**
	 * Align padded rectangles to a multiple of this number of pixels.
	 *
	 * \warning Must be a power of 2.
	 */
	uint32_t alignment = 1;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const AtlasPackerOptions& other) const = default;
};

/**
 * Axis-aligned rectangle packer for expandable square texture atlases.
 */
class AtlasPacker {
public:
	/**
	 * Construct an atlas packer with the default options.
	 */
	AtlasPacker() noexcept
		: AtlasPacker(AtlasPackerOptions{}) {}

	/**
	 * Construct an atlas packer.
	 *
	 * \param options atlas packer options, see AtlasPackerOptions.
	 */
	explicit AtlasPacker(const AtlasPackerOptions& options)
		: resolution(options.initialResolution)
		, padding(options.padding)
		, alignment(options.alignment) {
		GREM_ASSERT(options.initialResolution > 0);
		GREM_ASSERT(isPowerOf2(options.alignment));
	}

	/**
	 * Result of the insertRectangle() function.
	 */
	struct InsertRectangleResult {
		/**
		 * The offset, in pixels, from the bottom left of the atlas where the
		 * new rectangle was inserted.
		 */
		Offset2D offset;

		/**
		 * Whether the atlas needed to grow in order to accommodate the new
		 * rectangle or not. If true, the new required resolution can be queried
		 * by calling getResolution().
		 */
		bool resized;
	};

	/**
	 * Find and reserve a suitable space for a new axis-aligned rectangle to be
	 * inserted into the atlas.
	 *
	 * \param size size of the new rectangle, in pixels.
	 *
	 * \return see InsertRectangleResult.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] InsertRectangleResult insertRectangle(Extent2D size) {
		const Extent2D paddedSize{
			.width = roundUpToMultiple(size.width + padding * 2, alignment),
			.height = roundUpToMultiple(size.height + padding * 2, alignment),
		};

		Optional<size_t> foundRow = findSuitableRow(paddedSize);

		bool resized = false;
		if (!foundRow) {
			const uint32_t newRowBottom = (rows.empty()) ? uint32_t{0} : rows.back().top;
			const uint32_t newRowHeight = paddedSize.height + paddedSize.height / uint32_t{8};
			while (paddedSize.width > resolution || newRowHeight > resolution - newRowBottom) {
				if (resolution > Limits<uint32_t>::MAX / GROWTH_FACTOR) {
					throw std::length_error{"Maximum atlas resolution exceeded."};
				}
				resolution *= GROWTH_FACTOR;
				resized = true;
			}
			foundRow = rows.size();
			rows.push_back(Row{.top = newRowBottom + newRowHeight, .width = 0});
		}
		const size_t rowIndex = *foundRow;

		Row& row = rows[rowIndex];
		const uint32_t rowBottom = (rowIndex == 0) ? uint32_t{0} : rows[rowIndex - 1].top;
		const uint32_t x = row.width + padding;
		const uint32_t y = rowBottom + padding;

		row.width += paddedSize.width;

		return InsertRectangleResult{
			.offset{.x = static_cast<int32_t>(x), .y = static_cast<int32_t>(y)},
			.resized = resized,
		};
	}

	/**
	 * Get the current required resolution of the atlas.
	 *
	 * \return the width of the square atlas region, in pixels.
	 */
	[[nodiscard]] uint32_t getResolution() const noexcept {
		return resolution;
	}

private:
	static constexpr uint32_t GROWTH_FACTOR = 2;
	static constexpr float MINIMUM_ROW_HEIGHT_RATIO = 0.7f;

	struct Row {
		uint32_t top;
		uint32_t width;
	};

	[[nodiscard]] Optional<size_t> findSuitableRow(Extent2D paddedSize) const {
		for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
			const Row& row = rows[rowIndex];
			const uint32_t rowHeight = (rowIndex == 0) ? row.top : row.top - rows[rowIndex - 1].top;
			if (const float heightRatio = static_cast<float>(paddedSize.height) / static_cast<float>(rowHeight);
				heightRatio >= MINIMUM_ROW_HEIGHT_RATIO && paddedSize.height <= rowHeight && paddedSize.width <= resolution - row.width) {
				return rowIndex;
			}
		}
		return {};
	}

	Buffer<Row> rows{};
	uint32_t resolution;
	uint32_t padding;
	uint32_t alignment;
};

} // namespace grem::resource

#endif
