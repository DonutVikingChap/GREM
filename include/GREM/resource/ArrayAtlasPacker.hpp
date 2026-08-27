// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_ARRAY_ATLAS_PACKER_HPP
#define GREM_RESOURCE_ARRAY_ATLAS_PACKER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::resource {

/**
 * Configuration options for an ArrayAtlasPacker.
 */
struct ArrayAtlasPackerOptions {
	/**
	 * Initial width of the square atlas region, in pixels.
	 *
	 * \warning Must be positive.
	 */
	uint32_t initialResolution = 128;

	/**
	 * Initial number of array layers in the atlas.
	 *
	 * \warning Must be positive.
	 */
	uint32_t initialDepth = 1;

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
	[[nodiscard]] constexpr bool operator==(const ArrayAtlasPackerOptions& other) const = default;
};

/**
 * Axis-aligned rectangle packer for expandable square texture atlases with
 * array layers.
 */
class ArrayAtlasPacker {
public:
	/**
	 * Construct an atlas packer with the default options.
	 */
	ArrayAtlasPacker() noexcept
		: ArrayAtlasPacker(ArrayAtlasPackerOptions{}) {}

	/**
	 * Construct an atlas packer.
	 *
	 * \param options atlas packer options, see ArrayAtlasPackerOptions.
	 */
	explicit ArrayAtlasPacker(const ArrayAtlasPackerOptions& options)
		: resolution(options.initialResolution)
		, depth(options.initialDepth)
		, padding(options.padding)
		, alignment(options.alignment) {
		GREM_ASSERT(options.initialResolution > 0);
		GREM_ASSERT(options.initialDepth > 0);
		GREM_ASSERT(isPowerOf2(options.alignment));
	}

	/**
	 * Result of the insertBox() function.
	 */
	struct InsertBoxResult {
		/**
		 * The offset, in pixels and layers, from the bottom left of the first
		 * layer of the atlas where the new box was inserted.
		 */
		Offset3D offset;

		/**
		 * Whether the atlas needed to grow in order to accommodate the new
		 * box or not. If true, the new required resolution and depth can be
		 * queried by calling getResolution() and getDepth().
		 */
		bool resized;
	};

	/**
	 * Find and reserve a suitable space for a new axis-aligned box to be
	 * inserted into the atlas.
	 *
	 * \param size size of the new box, in pixels and layers.
	 *
	 * \return see InsertBoxResult.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] InsertBoxResult insertBox(Extent3D size) {
		const Extent3D paddedSize{
			.width = roundUpToMultiple(size.width + padding * 2, alignment),
			.height = roundUpToMultiple(size.height + padding * 2, alignment),
			.depth = size.depth,
		};

		bool resized = false;
		while (paddedSize.width > resolution || paddedSize.height > resolution) {
			if (resolution > Limits<uint32_t>::MAX / GROWTH_FACTOR) {
				throw std::length_error{"Maximum atlas resolution exceeded."};
			}
			resolution *= GROWTH_FACTOR;
			resized = true;
		}

		Optional<FindSuitableRowResult> foundRow = findSuitableRow(paddedSize);

		if (!foundRow) {
			if (const Optional<size_t> foundLayer = findSuitableLayer(paddedSize)) {
				Layer& layer = layers[*foundLayer];
				const uint32_t newRowBottom = (layer.rows.empty()) ? uint32_t{0} : layer.rows.back().top;
				const uint32_t newRowHeight = min(paddedSize.height + paddedSize.height / uint32_t{8}, resolution - newRowBottom);
				const uint32_t newRowTop = newRowBottom + newRowHeight;
				foundRow = FindSuitableRowResult{.layerIndex = *foundLayer, .rowIndex = static_cast<uint32_t>(layer.rows.size())};
				layer.rows.push_back(Row{.top = newRowTop, .width = 0});
			} else {
				const uint32_t newRowHeight = min(paddedSize.height + paddedSize.height / uint32_t{8}, resolution);
				const uint32_t newLayerZBegin = (layers.empty()) ? uint32_t{0} : layers.back().zEnd;
				const uint32_t newLayerDepth = roundUpToPowerOf2(size.depth);
				if (newLayerDepth == 0 || newLayerDepth > Limits<uint32_t>::MAX - depth) {
					throw std::length_error{"Maximum atlas depth exceeded."};
				}
				const uint32_t newLayerZEnd = newLayerZBegin + newLayerDepth;
				if (newLayerZEnd > depth) {
					depth = newLayerZEnd;
					resized = true;
				}
				const size_t layerIndex = layers.size();
				layers.push_back(Layer{.zEnd = newLayerZEnd, .rows{Row{.top = newRowHeight, .width = 0}}});
				foundRow = FindSuitableRowResult{.layerIndex = layerIndex, .rowIndex = 0};
			}
		}

		const size_t layerIndex = foundRow->layerIndex;
		const size_t rowIndex = foundRow->rowIndex;

		Layer& layer = layers[layerIndex];
		Row& row = layer.rows[rowIndex];
		const uint32_t rowBottom = (rowIndex == 0) ? uint32_t{0} : layer.rows[rowIndex - 1].top;
		const uint32_t x = row.width + padding;
		const uint32_t y = rowBottom + padding;
		const uint32_t z = (layerIndex == 0) ? uint32_t{0} : layers[layerIndex - 1].zEnd;

		row.width += paddedSize.width;

		return InsertBoxResult{
			.offset{.x = static_cast<int32_t>(x), .y = static_cast<int32_t>(y), .z = static_cast<int32_t>(z)},
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

	/**
	 * Get the current required depth of the atlas.
	 *
	 * \return the number of array layers in the atlas.
	 */
	[[nodiscard]] uint32_t getDepth() const noexcept {
		return depth;
	}

private:
	static constexpr uint32_t GROWTH_FACTOR = 2;
	static constexpr float MINIMUM_ROW_HEIGHT_RATIO = 0.7f;
	static constexpr float MINIMUM_LAYER_DEPTH_RATIO = 0.7f;

	struct Row {
		uint32_t top;
		uint32_t width;
	};

	struct Layer {
		uint32_t zEnd;
		Buffer<Row> rows;
	};

	struct FindSuitableRowResult {
		size_t layerIndex;
		size_t rowIndex;
	};

	[[nodiscard]] Optional<FindSuitableRowResult> findSuitableRow(Extent3D paddedSize) const {
		for (size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
			const Layer& layer = layers[layerIndex];
			const uint32_t layerDepth = (layerIndex == 0) ? layer.zEnd : layer.zEnd - layers[layerIndex - 1].zEnd;
			if (const float depthRatio = static_cast<float>(paddedSize.depth) / static_cast<float>(layerDepth);
				depthRatio >= MINIMUM_LAYER_DEPTH_RATIO && paddedSize.depth <= layerDepth) {
				for (size_t rowIndex = 0; rowIndex < layer.rows.size(); ++rowIndex) {
					const Row& row = layer.rows[rowIndex];
					const uint32_t rowHeight = (rowIndex == 0) ? row.top : row.top - layer.rows[rowIndex - 1].top;
					if (const float heightRatio = static_cast<float>(paddedSize.height) / static_cast<float>(rowHeight);
						heightRatio >= MINIMUM_ROW_HEIGHT_RATIO && paddedSize.height <= rowHeight && paddedSize.width <= resolution - row.width) {
						return FindSuitableRowResult{.layerIndex = layerIndex, .rowIndex = rowIndex};
					}
				}
			}
		}
		return {};
	}

	[[nodiscard]] Optional<size_t> findSuitableLayer(Extent3D paddedSize) const {
		for (size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
			const Layer& layer = layers[layerIndex];
			const uint32_t layerDepth = (layerIndex == 0) ? layer.zEnd : layer.zEnd - layers[layerIndex - 1].zEnd;
			if (const float depthRatio = static_cast<float>(paddedSize.depth) / static_cast<float>(layerDepth);
				depthRatio >= MINIMUM_LAYER_DEPTH_RATIO && paddedSize.depth <= layerDepth) {
				if (layer.rows.empty() || paddedSize.height <= resolution - layer.rows.back().top) {
					return layerIndex;
				}
			}
		}
		return {};
	}

	ArrayList<Layer> layers{};
	uint32_t resolution;
	uint32_t depth;
	uint32_t padding;
	uint32_t alignment;
};

} // namespace grem::resource

#endif
