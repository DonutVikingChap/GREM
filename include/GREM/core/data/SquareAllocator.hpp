// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SQUARE_ALLOCATOR_HPP
#define GREM_CORE_DATA_SQUARE_ALLOCATOR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>

#include <new>     // std::bad_array_new_length
#include <utility> // std::move

namespace grem {

template <typename Coordinate>
struct SquareAllocation {
	Coordinate x{};
	Coordinate y{};
	Coordinate allocatedWidth{};
};

template <typename Coordinate>
class SquareAllocator {
public:
	SquareAllocator() noexcept = default;
	~SquareAllocator() = default;

	SquareAllocator(const SquareAllocator&) = delete;
	SquareAllocator(SquareAllocator&&) noexcept = default;
	SquareAllocator& operator=(const SquareAllocator&) = delete;
	SquareAllocator& operator=(SquareAllocator&&) noexcept = default;

	explicit SquareAllocator(Coordinate initialWidth)
		: fullWidth(roundUpToPowerOf2(initialWidth)) {}

	void clear() noexcept {
		fullSquare.clear(fullWidth);
	}

	[[nodiscard]] Optional<SquareAllocation<Coordinate>> allocateSquare(Coordinate width) {
		if (width == 0) {
			return SquareAllocation<Coordinate>{};
		}

		const Coordinate allocatedWidth = roundUpToPowerOf2(width);
		if (allocatedWidth == 0) {
			throw std::bad_array_new_length{};
		}

		return fullSquare.allocate(0, 0, fullWidth, allocatedWidth);
	}

	void deallocateSquare(SquareAllocation<Coordinate> allocation) noexcept {
		GREM_ASSERT(allocation.allocatedWidth <= fullWidth);
		GREM_ASSERT(allocation.x <= fullWidth - allocation.allocatedWidth);
		GREM_ASSERT(allocation.y <= fullWidth - allocation.allocatedWidth);

		if (allocation.allocatedWidth == 0) {
			return;
		}

		GREM_ASSERT(isPowerOf2(allocation.allocatedWidth));
		fullSquare.deallocate(allocation.x, allocation.y, fullWidth, allocation.allocatedWidth);
	}

	void expandTo(Coordinate newWidth) {
		GREM_ASSERT(newWidth >= fullWidth);

		if (fullWidth == 0) {
			const Coordinate newFullWidth = roundUpToPowerOf2(newWidth);
			if ((newFullWidth == 0 && newWidth > 0) || newFullWidth > Limits<Coordinate>::MAX / newFullWidth) {
				throw std::bad_array_new_length{};
			}
			fullWidth = newFullWidth;
			fullSquare.largestAvailableWidth = newFullWidth;
			return;
		}

		while (newWidth > fullWidth) {
			if (fullWidth > Limits<Coordinate>::MAX / 2) {
				throw std::bad_array_new_length{};
			}
			const Coordinate newFullWidth = fullWidth * 2;
			if (newFullWidth > Limits<Coordinate>::MAX / newFullWidth) {
				throw std::bad_array_new_length{};
			}
			Square newFullSquare{(fullSquare.largestAvailableWidth == fullWidth) ? newFullWidth : fullWidth};
			newFullSquare.quadrants = UniquePointer<Array<Square, 4>>::create(Array{std::move(fullSquare), Square{fullWidth}, Square{fullWidth}, Square{fullWidth}});
			fullSquare = std::move(newFullSquare);
			fullWidth = newFullWidth;
		}
	}

	[[nodiscard]] Coordinate getFullWidth() const noexcept {
		return fullWidth;
	}

private:
	struct Square {
		Coordinate largestAvailableWidth;
		UniquePointer<Array<Square, 4>> quadrants{};

		explicit Square(Coordinate largestAvailableWidth)
			: largestAvailableWidth(largestAvailableWidth) {}

		void clear(Coordinate width) noexcept {
			largestAvailableWidth = width;
			if (quadrants) {
				const Coordinate quadrantWidth = width / 2;
				for (Square& quadrant : *quadrants) {
					quadrant.clear(quadrantWidth);
				}
			}
		}

		[[nodiscard]] Optional<SquareAllocation<Coordinate>> allocate(Coordinate x, Coordinate y, Coordinate width, Coordinate allocatedWidth) {
			if (largestAvailableWidth < allocatedWidth) {
				return {};
			}
			if (width == allocatedWidth) {
				largestAvailableWidth = 0;
				return SquareAllocation<Coordinate>{.x = x, .y = y, .allocatedWidth = allocatedWidth};
			}
			const Coordinate quadrantWidth = width / 2;
			if (!quadrants) {
				quadrants = UniquePointer<Array<Square, 4>>::create(Array{Square{quadrantWidth}, Square{quadrantWidth}, Square{quadrantWidth}, Square{quadrantWidth}});
			}
			Optional<SquareAllocation<Coordinate>> result = (*quadrants)[0].allocate(x, y, quadrantWidth, allocatedWidth);
			if (!result) {
				result = (*quadrants)[1].allocate(x + quadrantWidth, y, quadrantWidth, allocatedWidth);
				if (!result) {
					result = (*quadrants)[2].allocate(x, y + quadrantWidth, quadrantWidth, allocatedWidth);
					if (!result) {
						result = (*quadrants)[3].allocate(x + quadrantWidth, y + quadrantWidth, quadrantWidth, allocatedWidth);
					}
				}
			}
			const Array quadrantLargestAvailableWidths{
				(*quadrants)[0].largestAvailableWidth,
				(*quadrants)[1].largestAvailableWidth,
				(*quadrants)[2].largestAvailableWidth,
				(*quadrants)[3].largestAvailableWidth,
			};
			largestAvailableWidth =
				max(max(max(quadrantLargestAvailableWidths[0], quadrantLargestAvailableWidths[1]), quadrantLargestAvailableWidths[2]), quadrantLargestAvailableWidths[3]);
			return result;
		}

		void deallocate(Coordinate localX, Coordinate localY, Coordinate width, Coordinate allocatedWidth) noexcept {
			GREM_ASSERT(width >= allocatedWidth);
			if (width == allocatedWidth) {
				GREM_ASSERT(localX == 0 && localY == 0);
				largestAvailableWidth = width;
				return;
			}
			const Coordinate quadrantWidth = width / 2;
			GREM_ASSERT(quadrants);
			if (localY < quadrantWidth) {
				if (localX < quadrantWidth) {
					(*quadrants)[0].deallocate(localX, localY, quadrantWidth, allocatedWidth);
				} else {
					(*quadrants)[1].deallocate(localX - quadrantWidth, localY, quadrantWidth, allocatedWidth);
				}
			} else {
				if (localX < quadrantWidth) {
					(*quadrants)[2].deallocate(localX, localY - quadrantWidth, quadrantWidth, allocatedWidth);
				} else {
					(*quadrants)[3].deallocate(localX - quadrantWidth, localY - quadrantWidth, quadrantWidth, allocatedWidth);
				}
			}
			const Array quadrantLargestAvailableWidths{
				(*quadrants)[0].largestAvailableWidth,
				(*quadrants)[1].largestAvailableWidth,
				(*quadrants)[2].largestAvailableWidth,
				(*quadrants)[3].largestAvailableWidth,
			};
			if (quadrantLargestAvailableWidths[0] == quadrantWidth && quadrantLargestAvailableWidths[1] == quadrantWidth && quadrantLargestAvailableWidths[2] == quadrantWidth &&
				quadrantLargestAvailableWidths[3] == quadrantWidth) {
				largestAvailableWidth = width;
			} else {
				largestAvailableWidth =
					max(max(max(quadrantLargestAvailableWidths[0], quadrantLargestAvailableWidths[1]), quadrantLargestAvailableWidths[2]), quadrantLargestAvailableWidths[3]);
			}
		}
	};

	Coordinate fullWidth{};
	Square fullSquare{fullWidth};
};

} // namespace grem

#endif
