// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_RANGE_ALLOCATOR_HPP
#define GREM_CORE_DATA_RANGE_ALLOCATOR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SmallBuffer.hpp>

#include <algorithm> // std::lower_bound
#include <limits>    // std::numeric_limits
#include <utility>   // std::declval

namespace grem {

template <typename Index>
struct IndexAllocation {
	Index index{};
};

template <typename Index>
struct RangeAllocation {
	Index begin{};
	Index end{};
};

template <typename Index>
class RangeAllocator {
public:
	using size_type = decltype(std::declval<Index>() - std::declval<Index>());

	RangeAllocator() noexcept = default;
	~RangeAllocator() = default;

	RangeAllocator(const RangeAllocator&) = delete;
	RangeAllocator(RangeAllocator&&) noexcept = default;
	RangeAllocator& operator=(const RangeAllocator&) = delete;
	RangeAllocator& operator=(RangeAllocator&&) noexcept = default;

	explicit RangeAllocator(Index begin, Index end)
		: fullRange{.begin = begin, .end = end} {}

	void clear() noexcept {
		fullRange = {
			.begin = 0,
			.end = std::numeric_limits<Index>::max(),
		};
		freeRanges.clear();
		freeRanges.push_back(fullRange);
		allocatedRangeCount = 0;
	}

	[[nodiscard]] Optional<IndexAllocation<Index>> allocateIndex() {
		if (const Optional<RangeAllocation<Index>> result = allocateRange(1)) {
			return IndexAllocation<Index>{.index = result->begin};
		}
		return {};
	}

	[[nodiscard]] Optional<RangeAllocation<Index>> allocateRange(size_type size) {
		if (size == 0) {
			return RangeAllocation<Index>{};
		}

		size_t smallestFittingFreeRangeIndex = freeRanges.size();
		for (size_t freeRangeIndex = 0; freeRangeIndex < freeRanges.size(); ++freeRangeIndex) {
			const size_type freeRangeSize = freeRanges[freeRangeIndex].end - freeRanges[freeRangeIndex].begin;
			if (freeRangeSize >= size) {
				if (freeRangeSize == size) {
					smallestFittingFreeRangeIndex = freeRangeIndex;
					break;
				}
				if (smallestFittingFreeRangeIndex == freeRanges.size() ||
					freeRangeSize < freeRanges[smallestFittingFreeRangeIndex].end - freeRanges[smallestFittingFreeRangeIndex].begin) {
					smallestFittingFreeRangeIndex = freeRangeIndex;
				}
			}
		}
		if (smallestFittingFreeRangeIndex == freeRanges.size()) {
			return {};
		}
		const Index freeRangeBegin = freeRanges[smallestFittingFreeRangeIndex].begin;
		const Index freeRangeEnd = freeRanges[smallestFittingFreeRangeIndex].end;
		++allocatedRangeCount;
		if (allocatedRangeCount + 1 > freeRanges.capacity()) {
			freeRanges.reserve(allocatedRangeCount + 1);
		}
		if (freeRangeEnd - freeRangeBegin == size) {
			freeRanges.erase(freeRanges.begin() + static_cast<typename decltype(freeRanges)::difference_type>(smallestFittingFreeRangeIndex));
		} else {
			freeRanges[smallestFittingFreeRangeIndex].begin += size;
		}
		return RangeAllocation<Index>{.begin = freeRangeBegin, .end = freeRangeBegin + size};
	}

	void deallocateIndex(IndexAllocation<Index> allocation) noexcept {
		deallocateRange(RangeAllocation<Index>{.begin = allocation.index, .end = allocation.index + 1});
	}

	void deallocateRange(RangeAllocation<Index> allocation) noexcept {
		GREM_ASSERT(allocation.begin <= allocation.end);
		GREM_ASSERT(fullRange.begin < fullRange.end);
		GREM_ASSERT(allocation.begin >= fullRange.begin);
		GREM_ASSERT(allocation.end <= fullRange.end);

		if (allocation.begin == allocation.end) {
			return;
		}

		GREM_ASSERT(allocatedRangeCount > 0);

		const auto it = std::lower_bound(freeRanges.begin(), freeRanges.end(), allocation,
			[](const RangeAllocation<Index>& a, const RangeAllocation<Index>& b) -> bool { return a.begin < b.begin; });

		if (it != freeRanges.begin() && allocation.begin == (it - 1)->end) {
			if (it != freeRanges.end() && allocation.end == it->begin) {
				(it - 1)->end = it->end;
				freeRanges.erase(it);
			} else {
				(it - 1)->end = allocation.end;
			}
			--allocatedRangeCount;
			return;
		}

		if (it != freeRanges.end() && allocation.end == it->begin) {
			if (it != freeRanges.begin() && allocation.begin == (it - 1)->end) {
				it->begin = (it - 1)->begin;
				freeRanges.erase(it - 1);
			} else {
				it->begin = allocation.begin;
			}
			--allocatedRangeCount;
			return;
		}

		GREM_ASSERT((it == freeRanges.begin() || allocation.begin > (it - 1)->end) && //
					(it == freeRanges.end() || allocation.end < it->begin));
		freeRanges.insert(it, allocation);
		--allocatedRangeCount;
	}

	void expandFront(Index newBegin) {
		if (freeRanges.empty()) {
			freeRanges.push_back(RangeAllocation<Index>{.begin = newBegin, .end = fullRange.end});
		} else {
			freeRanges.front().begin = newBegin;
		}
		fullRange.begin = newBegin;
	}

	void expandBack(Index newEnd) {
		if (freeRanges.empty()) {
			freeRanges.push_back(RangeAllocation<Index>{.begin = fullRange.end, .end = newEnd});
		} else {
			freeRanges.back().end = newEnd;
		}
		fullRange.end = newEnd;
	}

	[[nodiscard]] Index getFullRangeBegin() const noexcept {
		return fullRange.begin;
	}

	[[nodiscard]] Index getFullRangeEnd() const noexcept {
		return fullRange.end;
	}

	[[nodiscard]] Index getUsedRangeBegin() const noexcept {
		if (freeRanges.empty() || freeRanges.front().begin != fullRange.begin) {
			return fullRange.begin;
		}
		return freeRanges.front().end;
	}

	[[nodiscard]] Index getUsedRangeEnd() const noexcept {
		if (freeRanges.empty() || freeRanges.back().end != fullRange.end) {
			return fullRange.end;
		}
		return freeRanges.back().begin;
	}

private:
	RangeAllocation<Index> fullRange{
		.begin = 0,
		.end = std::numeric_limits<Index>::max(),
	};
	SmallBuffer<RangeAllocation<Index>, 2> freeRanges{fullRange};
	size_t allocatedRangeCount = 0;
};

} // namespace grem

#endif
