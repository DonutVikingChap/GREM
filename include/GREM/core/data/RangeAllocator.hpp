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
	using size_type = decltype(std::declval<Index>() - std::declval<Index>());

	Index index = std::numeric_limits<Index>::max();

	[[nodiscard]] constexpr size_type size() const noexcept {
		return (index == std::numeric_limits<Index>::max()) ? size_type{0} : size_type{1};
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return index == std::numeric_limits<Index>::max();
	}
};

template <typename Index>
struct RangeAllocation {
	using size_type = decltype(std::declval<Index>() - std::declval<Index>());

	Index begin{};
	Index end{};

	[[nodiscard]] constexpr size_type size() const noexcept {
		return end - begin;
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return begin == end;
	}
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
			const size_type freeRangeSize = freeRanges[freeRangeIndex].size();
			if (freeRangeSize >= size) {
				if (freeRangeSize == size) {
					smallestFittingFreeRangeIndex = freeRangeIndex;
					break;
				}
				if (smallestFittingFreeRangeIndex == freeRanges.size() || freeRangeSize < freeRanges[smallestFittingFreeRangeIndex].size()) {
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
		if (allocation.empty()) {
			return;
		}

		deallocateRange(RangeAllocation<Index>{.begin = allocation.index, .end = allocation.index + 1});
	}

	void deallocateRange(RangeAllocation<Index> allocation) noexcept {
		GREM_ASSERT(allocation.begin <= allocation.end);
		GREM_ASSERT(fullRange.begin < fullRange.end);
		GREM_ASSERT(allocation.begin >= fullRange.begin);
		GREM_ASSERT(allocation.end <= fullRange.end);

		if (allocation.empty()) {
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

	void expandFrontTo(Index newBegin) {
		GREM_ASSERT(newBegin <= fullRange.begin);
		if (newBegin < fullRange.begin) {
			if (freeRanges.empty()) {
				freeRanges.push_back(RangeAllocation<Index>{.begin = newBegin, .end = fullRange.begin});
			} else {
				freeRanges.front().begin = newBegin;
			}
			fullRange.begin = newBegin;
		}
	}

	void expandBackTo(Index newEnd) {
		GREM_ASSERT(newEnd >= fullRange.end);
		if (newEnd > fullRange.end) {
			if (freeRanges.empty()) {
				freeRanges.push_back(RangeAllocation<Index>{.begin = fullRange.end, .end = newEnd});
			} else {
				freeRanges.back().end = newEnd;
			}
			fullRange.end = newEnd;
		}
	}

	[[nodiscard]] Index getFullRangeBegin() const noexcept {
		return fullRange.begin;
	}

	[[nodiscard]] Index getFullRangeEnd() const noexcept {
		return fullRange.end;
	}

	[[nodiscard]] size_type getFullRangeSize() const noexcept {
		return fullRange.size();
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

	[[nodiscard]] size_type getUsedRangeSize() const noexcept {
		return getUsedRangeEnd() - getUsedRangeBegin();
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
