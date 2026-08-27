// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_ALGORITHMS_HPP
#define GREM_CORE_ALGORITHMS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Pair.hpp>

#include <algorithm>  // std::...
#include <cstddef>    // std::size_t
#include <functional> // std::hash, std::invoke
#include <iterator>   // std::begin, std::end, std::size, std::next, std::distance, std::iterator_traits, std::random_access_iterator_tag
#include <numeric>    // std::accumulate, std::reduce, std::iota
#include <utility>    // std::move, std::forward, std::swap

namespace grem {

struct EqualTo {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) == std::forward<B>(b)) {
		return std::forward<A>(a) == std::forward<B>(b);
	}
};
inline constexpr EqualTo EQUAL_TO{};

struct NotEqualTo {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) != std::forward<B>(b)) {
		return std::forward<A>(a) != std::forward<B>(b);
	}
};
inline constexpr NotEqualTo NOT_EQUAL_TO{};

struct LessThan {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) < std::forward<B>(b)) {
		return std::forward<A>(a) < std::forward<B>(b);
	}
};
inline constexpr LessThan LESS_THAN{};

struct LessThanOrEqualTo {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) <= std::forward<B>(b)) {
		return std::forward<A>(a) <= std::forward<B>(b);
	}
};
inline constexpr LessThanOrEqualTo LESS_THAN_OR_EQUAL_TO{};

struct GreaterThan {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) > std::forward<B>(b)) {
		return std::forward<A>(a) > std::forward<B>(b);
	}
};
inline constexpr GreaterThan GREATER_THAN{};

struct GreaterThanOrEqualTo {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) >= std::forward<B>(b)) {
		return std::forward<A>(a) >= std::forward<B>(b);
	}
};
inline constexpr GreaterThanOrEqualTo GREATER_THAN_OR_EQUAL_TO{};

struct Add {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) + std::forward<B>(b)) {
		return std::forward<A>(a) + std::forward<B>(b);
	}
};
inline constexpr Add ADD{};

struct Subtract {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) - std::forward<B>(b)) {
		return std::forward<A>(a) - std::forward<B>(b);
	}
};
inline constexpr Subtract SUBTRACT{};

struct Multiply {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) * std::forward<B>(b)) {
		return std::forward<A>(a) * std::forward<B>(b);
	}
};
inline constexpr Multiply MULTIPLY{};

struct Divide {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) / std::forward<B>(b)) {
		return std::forward<A>(a) / std::forward<B>(b);
	}
};
inline constexpr Divide DIVIDE{};

struct Modulo {
	template <typename A, typename B>
	[[nodiscard]] constexpr auto operator()(A&& a, B&& b) const -> decltype(std::forward<A>(a) % std::forward<B>(b)) {
		return std::forward<A>(a) % std::forward<B>(b);
	}
};
inline constexpr Modulo MODULO{};

struct Negate {
	template <typename A>
	[[nodiscard]] constexpr auto operator()(A&& a) const -> decltype(-std::forward<A>(a)) {
		return -std::forward<A>(a);
	}
};
inline constexpr Negate NEGATE{};

[[nodiscard]] constexpr std::size_t combineHashes(std::size_t hash, convertible_to<std::size_t> auto... otherHashes) {
	((hash ^= std::size_t{otherHashes} + (hash << 6) + (hash >> 2)), ...);
	return hash;
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr std::size_t getHash(const T& value, const Ts&... otherValues) {
	return combineHashes(std::hash<T>{}(value), std::hash<Ts>{}(otherValues)...);
}

[[nodiscard]] constexpr std::size_t getRangeHash(input_range auto&& range) {
	auto it = std::begin(range);
	auto end = std::end(range);
	if (it != end) {
		std::size_t result = getHash(*it++);
		while (it != end) {
			result = combineHashes(result, getHash(*it++));
		}
		return result;
	}
	return 0;
}

constexpr void iterSwap(auto itA, auto itB) {
	using std::swap;
	swap(*itA, *itB);
}

constexpr void swapRanges(input_range auto&& rangeA, input_range auto&& rangeB) {
	auto itA = std::begin(rangeA);
	auto endA = std::end(rangeA);
	auto itB = std::begin(rangeB);
	auto endB = std::end(rangeB);
	while (itA != endA && itB != endB) {
		iterSwap(itA, itB);
		++itA;
		++itB;
	}
}

constexpr void forEach(input_range auto&& range, auto operation) {
	std::for_each(std::begin(range), std::end(range), std::move(operation));
}

constexpr auto transform(input_range auto&& range, auto outputIterator, auto transformationFunction) {
	return std::transform(std::begin(range), std::end(range), std::move(outputIterator), std::move(transformationFunction));
}

constexpr auto transform(input_range auto&& rangeA, input_range auto&& rangeB, auto outputIterator, auto transformationFunction) {
	auto itA = std::begin(rangeA);
	auto endA = std::end(rangeA);
	auto itB = std::begin(rangeB);
	auto endB = std::end(rangeB);
	while (itA != endA && itB != endB) {
		*outputIterator++ = std::invoke(transformationFunction, *itA, *itB);
		++itA;
		++itB;
	}
	return outputIterator;
}

[[nodiscard]] constexpr auto accumulate(input_range auto&& range, auto initialValue) {
	return std::accumulate(std::begin(range), std::end(range), std::move(initialValue));
}

[[nodiscard]] constexpr auto accumulate(input_range auto&& range, auto initialValue, auto reductionFunction) {
	return std::accumulate(std::begin(range), std::end(range), std::move(initialValue), std::move(reductionFunction));
}

[[nodiscard]] constexpr auto reduce(input_range auto&& range) {
	return std::reduce(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr auto reduce(input_range auto&& range, auto initialValue) {
	return std::reduce(std::begin(range), std::end(range), std::move(initialValue));
}

[[nodiscard]] constexpr auto reduce(input_range auto&& range, auto initialValue, auto reductionFunction) {
	return std::reduce(std::begin(range), std::end(range), std::move(initialValue), std::move(reductionFunction));
}

[[nodiscard]] constexpr bool anyOf(input_range auto&& range, auto predicate) {
	return std::any_of(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr bool allOf(input_range auto&& range, auto predicate) {
	return std::all_of(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr bool noneOf(input_range auto&& range, auto predicate) {
	return std::none_of(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr auto count(input_range auto&& range, const auto& value) {
	return std::count(std::begin(range), std::end(range), value);
}

[[nodiscard]] constexpr auto countIf(input_range auto&& range, auto predicate) {
	return std::count_if(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr auto find(input_range auto&& range, const auto& value) {
	return std::find(std::begin(range), std::end(range), value);
}

template <auto Member>
[[nodiscard]] constexpr auto findBy(input_range auto&& range, const auto& value) {
	return std::find_if(std::begin(range), std::end(range), [&](const auto& element) -> bool { return element.*Member == value; });
}

[[nodiscard]] constexpr auto findIf(input_range auto&& range, auto predicate) {
	return std::find_if(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr auto findIfNot(input_range auto&& range, auto predicate) {
	return std::find_if_not(std::begin(range), std::end(range), std::move(predicate));
}

[[nodiscard]] constexpr auto findFirstOf(input_range auto&& range, forward_range auto&& values) {
	return std::find_first_of(std::begin(range), std::end(range), std::begin(values), std::end(values));
}

[[nodiscard]] constexpr auto findFirstOf(input_range auto&& range, forward_range auto&& values, auto equal) {
	return std::find_first_of(std::begin(range), std::end(range), std::begin(values), std::end(values), std::move(equal));
}

[[nodiscard]] constexpr auto findEnd(forward_range auto&& range, forward_range auto&& values) {
	return std::find_end(std::begin(range), std::end(range), std::begin(values), std::end(values));
}

[[nodiscard]] constexpr auto findEnd(forward_range auto&& range, forward_range auto&& values, auto equal) {
	return std::find_end(std::begin(range), std::end(range), std::begin(values), std::end(values), std::move(equal));
}

[[nodiscard]] constexpr auto search(forward_range auto&& range, forward_range auto&& sequence) {
	return std::search(std::begin(range), std::end(range), std::begin(sequence), std::end(sequence));
}

[[nodiscard]] constexpr auto search(forward_range auto&& range, forward_range auto&& sequence, auto equal) {
	return std::search(std::begin(range), std::end(range), std::begin(sequence), std::end(sequence), std::move(equal));
}

[[nodiscard]] constexpr auto mismatch(input_range auto&& range, input_range auto&& sequence) {
	return std::mismatch(std::begin(range), std::end(range), std::begin(sequence), std::end(sequence));
}

[[nodiscard]] constexpr auto mismatch(input_range auto&& range, input_range auto&& sequence, auto equal) {
	return std::mismatch(std::begin(range), std::end(range), std::begin(sequence), std::end(sequence), std::move(equal));
}

[[nodiscard]] constexpr auto adjacentFind(forward_range auto&& range) {
	return std::adjacent_find(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr auto adjacentFind(forward_range auto&& range, auto equal) {
	return std::adjacent_find(std::begin(range), std::end(range), std::move(equal));
}

[[nodiscard]] constexpr bool contains(input_range auto&& range, const auto& value) {
	auto end = std::end(range);
	return std::find(std::begin(range), end, value) != end;
}

template <auto Member>
[[nodiscard]] constexpr auto containsBy(input_range auto&& range, const auto& value) {
	auto end = std::end(range);
	return std::find_if(std::begin(range), end, [&](const auto& element) -> bool { return element.*Member == value; }) != end;
}

[[nodiscard]] constexpr bool contains(input_range auto&& range, const auto& value, auto equal) {
	auto end = std::end(range);
	return std::find_if(std::begin(range), end, [&](const auto& v) -> bool { return std::invoke(equal, v, value); }) != end;
}

[[nodiscard]] constexpr bool containsSubrange(forward_range auto&& range, forward_range auto&& sequence) {
	auto sequenceBegin = std::begin(sequence);
	auto sequenceEnd = std::end(sequence);
	if (sequenceBegin == sequenceEnd) {
		return true;
	}
	auto end = std::end(range);
	return std::search(std::begin(range), end, std::begin(sequence), std::move(sequenceBegin), std::move(sequenceEnd)) != end;
}

[[nodiscard]] constexpr bool containsSubrange(forward_range auto&& range, forward_range auto&& sequence, auto equal) {
	auto sequenceBegin = std::begin(sequence);
	auto sequenceEnd = std::end(sequence);
	if (sequenceBegin == sequenceEnd) {
		return true;
	}
	auto end = std::end(range);
	return std::search(std::begin(range), end, std::begin(sequence), std::move(sequenceBegin), std::move(sequenceEnd), std::move(equal)) != end;
}

[[nodiscard]] constexpr bool startsWith(input_range auto&& range, input_range auto&& sequence) {
	auto sequenceEnd = std::end(sequence);
	return std::mismatch(std::begin(range), std::end(range), std::begin(sequence), std::begin(sequence), sequenceEnd).second == sequenceEnd;
}

[[nodiscard]] constexpr bool startsWith(input_range auto&& range, input_range auto&& sequence, auto equal) {
	auto sequenceEnd = std::end(sequence);
	return std::mismatch(std::begin(range), std::end(range), std::begin(sequence), std::begin(sequence), sequenceEnd, std::move(equal)).second == sequenceEnd;
}

[[nodiscard]] constexpr bool endsWith(input_range auto&& range, input_range auto&& sequence) {
	const auto rangeSize = std::size(range);
	const auto sequenceSize = std::size(sequence);
	return sequenceSize <= rangeSize && std::equal(std::next(std::begin(range), (rangeSize - sequenceSize)), std::end(range), std::begin(sequence), std::end(sequence));
}

[[nodiscard]] constexpr bool endsWith(input_range auto&& range, input_range auto&& sequence, auto equal) {
	const auto rangeSize = std::size(range);
	const auto sequenceSize = std::size(sequence);
	return sequenceSize <= rangeSize &&
	       std::equal(std::next(std::begin(range), (rangeSize - sequenceSize)), std::end(range), std::begin(sequence), std::end(sequence), std::move(equal));
}

[[nodiscard]] constexpr bool equal(input_range auto&& rangeA, input_range auto&& rangeB) {
	return std::equal(std::begin(rangeA), std::end(rangeA), std::begin(rangeB), std::end(rangeB));
}

[[nodiscard]] constexpr bool equal(input_range auto&& rangeA, input_range auto&& rangeB, auto equal) {
	return std::equal(std::begin(rangeA), std::end(rangeA), std::begin(rangeB), std::end(rangeB), std::move(equal));
}

[[nodiscard]] constexpr bool lexicographicalCompare(input_range auto&& rangeA, input_range auto&& rangeB) {
	return std::lexicographical_compare(std::begin(rangeA), std::end(rangeA), std::begin(rangeB), std::end(rangeB));
}

[[nodiscard]] constexpr bool lexicographicalCompare(input_range auto&& rangeA, input_range auto&& rangeB, auto compare) {
	return std::lexicographical_compare(std::begin(rangeA), std::end(rangeA), std::begin(rangeB), std::end(rangeB), std::move(compare));
}

[[nodiscard]] constexpr auto minElement(forward_range auto&& range) {
	return std::min_element(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr auto minElement(forward_range auto&& range, auto compare) {
	return std::min_element(std::begin(range), std::end(range), std::move(compare));
}

[[nodiscard]] constexpr auto maxElement(forward_range auto&& range) {
	return std::max_element(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr auto maxElement(forward_range auto&& range, auto compare) {
	return std::max_element(std::begin(range), std::end(range), std::move(compare));
}

[[nodiscard]] constexpr auto minmaxElement(forward_range auto&& range) {
	return std::minmax_element(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr auto minmaxElement(forward_range auto&& range, auto compare) {
	return std::minmax_element(std::begin(range), std::end(range), std::move(compare));
}

[[nodiscard]] constexpr bool isSorted(forward_range auto&& range) {
	return std::is_sorted(std::begin(range), std::end(range));
}

[[nodiscard]] constexpr bool isSorted(forward_range auto&& range, auto compare) {
	return std::is_sorted(std::begin(range), std::end(range), std::move(compare));
}

[[nodiscard]] constexpr bool isPartitioned(input_range auto&& range, auto compare) {
	return std::is_partitioned(std::begin(range), std::end(range), std::move(compare));
}

[[nodiscard]] constexpr bool includes(input_range auto&& sortedRange, input_range auto&& sortedSequence) {
	return std::includes(std::begin(sortedRange), std::end(sortedRange), std::begin(sortedSequence), std::end(sortedSequence));
}

[[nodiscard]] constexpr bool includes(input_range auto&& sortedRange, input_range auto&& sortedSequence, auto compare) {
	return std::includes(std::begin(sortedRange), std::end(sortedRange), std::begin(sortedSequence), std::end(sortedSequence), std::move(compare));
}

[[nodiscard]] constexpr bool binarySearch(forward_range auto&& sortedRange, const auto& value) {
	return std::binary_search(std::begin(sortedRange), std::end(sortedRange), value);
}

[[nodiscard]] constexpr bool binarySearch(forward_range auto&& sortedRange, const auto& value, auto compare) {
	return std::binary_search(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
}

[[nodiscard]] constexpr auto lowerBound(forward_range auto&& sortedRange, const auto& value, auto compare) {
	// Apparently, Clang optimizes std::lower_bound well, but the other compilers don't.
	// Meanwhile, the if-statement in the custom implementation below gets optimized into a CMOV under all compilers, except for Clang (as of 21.1.0).
	// Funny how that works out.
#ifdef __clang__
	return std::lower_bound(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
#else
	if constexpr (convertible_to<typename std::iterator_traits<decltype(std::begin(sortedRange))>::iterator_category, std::random_access_iterator_tag>) {
		auto it = std::begin(sortedRange);
		auto n = std::end(sortedRange) - it;
		if (n == 0) {
			return it;
		}
		while (n > 1) {
			const auto middle = n / 2;
			if (compare(it[middle], value)) {
				it += middle;
			}
			n -= middle;
		}
		return (compare(*it, value)) ? it + 1 : it;
	} else {
		return std::lower_bound(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
	}
#endif
}

[[nodiscard]] constexpr auto lowerBound(forward_range auto&& sortedRange, const auto& value) {
	return lowerBound(std::forward<decltype(sortedRange)>(sortedRange), value, LESS_THAN);
}

[[nodiscard]] constexpr auto upperBound(forward_range auto&& sortedRange, const auto& value) {
	return std::upper_bound(std::begin(sortedRange), std::end(sortedRange), value);
}

[[nodiscard]] constexpr auto upperBound(forward_range auto&& sortedRange, const auto& value, auto compare) {
	return std::upper_bound(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
}

[[nodiscard]] constexpr auto equalRange(forward_range auto&& sortedRange, const auto& value, auto compare) {
#ifdef __clang__ // See the comment in lowerBound().
	auto [begin, end] = std::equal_range(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
	return Pair{std::move(begin), std::move(end)};
#else
	if constexpr (convertible_to<typename std::iterator_traits<decltype(std::begin(sortedRange))>::iterator_category, std::random_access_iterator_tag>) {
		auto begin = lowerBound(std::forward<decltype(sortedRange)>(sortedRange), value, compare);
		auto end = std::upper_bound(begin, std::end(sortedRange), value, std::move(compare));
		return Pair{std::move(begin), std::move(end)};
	} else {
		const auto [begin, end] = std::equal_range(std::begin(sortedRange), std::end(sortedRange), value, std::move(compare));
		return Pair{std::move(begin), std::move(end)};
	}
#endif
}

[[nodiscard]] constexpr auto equalRange(forward_range auto&& sortedRange, const auto& value) {
	return equalRange(std::forward<decltype(sortedRange)>(sortedRange), value, LESS_THAN);
}

[[nodiscard]] constexpr auto partitionPoint(forward_range auto&& partitionedRange, auto compare) {
	return std::partition_point(std::begin(partitionedRange), std::end(partitionedRange), std::move(compare));
}

constexpr auto merge(input_range auto&& sortedRangeA, input_range auto&& sortedRangeB, auto outputIterator) {
	return std::merge(std::begin(sortedRangeA), std::end(sortedRangeA), std::begin(sortedRangeB), std::end(sortedRangeB), std::move(outputIterator));
}

constexpr auto merge(input_range auto&& sortedRangeA, input_range auto&& sortedRangeB, auto outputIterator, auto compare) {
	return std::merge(std::begin(sortedRangeA), std::end(sortedRangeA), std::begin(sortedRangeB), std::end(sortedRangeB), std::move(outputIterator), std::move(compare));
}

constexpr auto inplaceMerge(bidirectional_range auto&& sortedRange, auto middleIterator) {
	return std::inplace_merge(std::begin(sortedRange), std::move(middleIterator), std::end(sortedRange));
}

constexpr auto inplaceMerge(bidirectional_range auto&& sortedRange, auto middleIterator, auto compare) {
	return std::inplace_merge(std::begin(sortedRange), std::move(middleIterator), std::end(sortedRange), std::move(compare));
}

constexpr void sort(random_access_range auto&& range) {
	std::sort(std::begin(range), std::end(range));
}

constexpr void sort(random_access_range auto&& range, auto compare) {
	std::sort(std::begin(range), std::end(range), std::move(compare));
}

template <auto... Members>
constexpr void sortByAscending(random_access_range auto&& range) {
	std::sort(std::begin(range), std::end(range), [](const auto& a, const auto& b) -> bool { return (a.*....*Members) < (b.*....*Members); });
}

template <auto... Members>
constexpr void sortByDescending(random_access_range auto&& range) {
	std::sort(std::begin(range), std::end(range), [](const auto& a, const auto& b) -> bool { return (a.*....*Members) > (b.*....*Members); });
}

constexpr void stableSort(random_access_range auto&& range) {
	std::stable_sort(std::begin(range), std::end(range));
}

constexpr void stableSort(random_access_range auto&& range, auto compare) {
	std::stable_sort(std::begin(range), std::end(range), std::move(compare));
}

template <auto... Members>
constexpr void stableSortByAscending(random_access_range auto&& range) {
	std::stable_sort(std::begin(range), std::end(range), [](const auto& a, const auto& b) -> bool { return (a.*....*Members) < (b.*....*Members); });
}

template <auto... Members>
constexpr void stableSortByDescending(random_access_range auto&& range) {
	std::stable_sort(std::begin(range), std::end(range), [](const auto& a, const auto& b) -> bool { return (a.*....*Members) > (b.*....*Members); });
}

constexpr void partialSort(random_access_range auto&& range, auto middleIterator) {
	std::partial_sort(std::begin(range), middleIterator, std::end(range));
}

constexpr void partialSort(random_access_range auto&& range, auto middleIterator, auto compare) {
	std::partial_sort(std::begin(range), middleIterator, std::end(range), std::move(compare));
}

constexpr auto partialSortCopy(input_range auto&& range, random_access_range auto&& outputRange) {
	return std::partial_sort_copy(std::begin(range), std::end(range), std::begin(outputRange), std::end(outputRange));
}

constexpr auto partialSortCopy(input_range auto&& range, random_access_range auto&& outputRange, auto compare) {
	return std::partial_sort_copy(std::begin(range), std::end(range), std::begin(outputRange), std::end(outputRange), std::move(compare));
}

constexpr auto partition(forward_range auto&& range, auto compare) {
	return std::partition(std::begin(range), std::end(range), std::move(compare));
}

constexpr auto stablePartition(bidirectional_range auto&& range, auto compare) {
	return std::stable_partition(std::begin(range), std::end(range), std::move(compare));
}

constexpr auto partitionCopy(input_range auto&& range, auto trueOutputIterator, auto falseOutputIterator, auto compare) {
	return std::partition_copy(std::begin(range), std::end(range), std::move(trueOutputIterator), std::move(falseOutputIterator), std::move(compare));
}

constexpr auto nthElement(random_access_range auto&& range, auto nthIterator) {
	return std::nth_element(std::begin(range), std::move(nthIterator), std::end(range));
}

constexpr auto nthElement(random_access_range auto&& range, auto nthIterator, auto compare) {
	return std::nth_element(std::begin(range), std::move(nthIterator), std::end(range), std::move(compare));
}

constexpr auto unique(forward_range auto&& range) {
	return std::unique(std::begin(range), std::end(range));
}

constexpr auto unique(forward_range auto&& range, auto equal) {
	return std::unique(std::begin(range), std::end(range), std::move(equal));
}

constexpr auto uniqueCopy(input_range auto&& range, auto outputIterator) {
	return std::unique_copy(std::begin(range), std::end(range), std::move(outputIterator));
}

constexpr auto uniqueCopy(input_range auto&& range, auto outputIterator, auto equal) {
	return std::unique_copy(std::begin(range), std::end(range), std::move(outputIterator), std::move(equal));
}

constexpr auto remove(forward_range auto&& range, const auto& value) {
	return std::remove(std::begin(range), std::end(range), value);
}

constexpr auto removeIf(forward_range auto&& range, auto predicate) {
	return std::remove_if(std::begin(range), std::end(range), std::move(predicate));
}

constexpr auto removeCopy(input_range auto&& range, auto outputIterator, const auto& value) {
	return std::remove_copy(std::begin(range), std::end(range), std::move(outputIterator), value);
}

constexpr auto removeCopyIf(input_range auto&& range, auto outputIterator, auto predicate) {
	return std::remove_copy_if(std::begin(range), std::end(range), std::move(outputIterator), std::move(predicate));
}

constexpr auto replace(forward_range auto&& range, const auto& oldValue, const auto& newValue) {
	return std::replace(std::begin(range), std::end(range), oldValue, newValue);
}

constexpr auto replaceIf(forward_range auto&& range, auto predicate, const auto& newValue) {
	return std::replace_if(std::begin(range), std::end(range), std::move(predicate), newValue);
}

constexpr auto replaceCopy(input_range auto&& range, auto outputIterator, const auto& oldValue, const auto& newValue) {
	return std::replace_copy(std::begin(range), std::end(range), std::move(outputIterator), oldValue, newValue);
}

constexpr auto replaceCopyIf(input_range auto&& range, auto outputIterator, auto predicate, const auto& newValue) {
	return std::replace_copy_if(std::begin(range), std::end(range), std::move(outputIterator), std::move(predicate), newValue);
}

inline void shuffle(random_access_range auto&& range, auto&& uniformRandomBitGenerator) {
	std::shuffle(std::begin(range), std::end(range), std::forward<decltype(uniformRandomBitGenerator)>(uniformRandomBitGenerator));
}

inline auto sample(forward_range auto&& range, auto outputIterator, auto n, auto&& uniformRandomBitGenerator) {
	return std::sample(std::begin(range), std::end(range), std::move(outputIterator), std::move(n), std::forward<decltype(uniformRandomBitGenerator)>(uniformRandomBitGenerator));
}

constexpr void reverse(bidirectional_range auto&& range) {
	std::reverse(std::begin(range), std::end(range));
}

constexpr auto reverseCopy(bidirectional_range auto&& range, auto outputIterator) {
	return std::reverse_copy(std::begin(range), std::end(range), std::move(outputIterator));
}

constexpr auto rotate(forward_range auto&& range, auto middle) {
	return std::rotate(std::begin(range), std::move(middle), std::end(range));
}

constexpr auto rotateCopy(forward_range auto&& range, auto middle, auto outputIterator) {
	return std::rotate_copy(std::begin(range), std::move(middle), std::end(range), std::move(outputIterator));
}

constexpr auto shiftLeft(forward_range auto&& range, auto n) {
	return std::shift_left(std::begin(range), std::end(range), n);
}

constexpr auto shiftRight(forward_range auto&& range, auto n) {
	return std::shift_right(std::begin(range), std::end(range), n);
}

constexpr auto copy(input_range auto&& range, auto outputIterator) {
	return std::copy(std::begin(range), std::end(range), std::move(outputIterator));
}

constexpr auto copyIf(input_range auto&& range, auto outputIterator, auto predicate) {
	return std::copy_if(std::begin(range), std::end(range), std::move(outputIterator), std::move(predicate));
}

constexpr auto copyBackward(bidirectional_range auto&& range, auto outputEnd) {
	return std::copy_backward(std::begin(range), std::end(range), std::move(outputEnd));
}

constexpr auto move(input_range auto&& range, auto outputIterator) {
	return std::move(std::begin(range), std::end(range), std::move(outputIterator));
}

constexpr auto moveBackward(bidirectional_range auto&& range, auto outputEnd) {
	return std::move_backward(std::begin(range), std::end(range), std::move(outputEnd));
}

constexpr void fill(forward_range auto&& range, const auto& value) {
	std::fill(std::begin(range), std::end(range), value);
}

constexpr void iota(forward_range auto&& range, auto firstValue) {
	std::iota(std::begin(range), std::end(range), std::move(firstValue));
}

constexpr void generate(forward_range auto&& range, auto generate) {
	std::generate(std::begin(range), std::end(range), std::move(generate));
}

} // namespace grem

#endif
