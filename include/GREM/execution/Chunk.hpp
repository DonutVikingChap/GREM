// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_CHUNK_HPP
#define GREM_EXECUTION_CHUNK_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/resource.hpp>

#include <functional>  // std::invoke, std::identity
#include <iterator>    // std::begin, std::size, std::prev
#include <memory>      // std::to_address
#include <type_traits> // std::remove_const_t, std::false_type, std::true_type
#include <utility>     // std::declval

namespace grem::execution {

template <typename Resource, auto Projection = std::identity{}>
class Chunk {
public:
	using resource_type = std::remove_const_t<Resource>;
	using iterator = decltype(std::begin(std::invoke(Projection, std::declval<Resource&>())));
	using size_type = decltype(std::size(std::invoke(Projection, std::declval<Resource&>())));
	using difference_type = iter_difference_t<iterator>;
	static_assert(resource<resource_type>, "Chunk must reference a valid resource type.");
	static_assert(random_access_iterator<iterator>, "Chunk iterator category must be random-access.");

	constexpr Chunk(Resource& resource, size_t chunkIndex, size_t chunkCount)
		: Chunk(std::invoke(Projection, resource), chunkIndex, chunkCount, 0) {}

	[[nodiscard]] constexpr iterator begin() const noexcept {
		return i;
	}

	[[nodiscard]] constexpr iterator end() const noexcept {
		return s;
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return i == s;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return static_cast<size_type>(s - i);
	}

	constexpr auto data() const requires(contiguous_iterator<iterator>) {
		return std::to_address(i);
	}

	constexpr decltype(auto) front() const {
		return *i;
	}

	constexpr decltype(auto) back() const {
		return *std::prev(s);
	}

	constexpr decltype(auto) operator[](size_type pos) const {
		return i[static_cast<difference_type>(pos)];
	}

private:
	constexpr Chunk(auto&& range, size_t chunkIndex, size_t chunkCount, int)
		: i(std::begin(range))
		, s(i) {
		const size_t rangeSize = static_cast<size_t>(std::size(range));
		const size_t chunkSize = (rangeSize + chunkCount - 1) / chunkCount;
		const size_t chunkBegin = min(chunkIndex * chunkSize, rangeSize);
		const size_t chunkEnd = min(chunkBegin + chunkSize, rangeSize);
		i += static_cast<difference_type>(chunkBegin);
		s += static_cast<difference_type>(chunkEnd);
	}

	iterator i;
	iterator s;
};

template <typename T>
struct is_chunk : std::false_type {};

template <typename Resource, auto Projection>
struct is_chunk<Chunk<Resource, Projection>> : std::true_type {};

template <typename T>
inline constexpr bool is_chunk_v = is_chunk<T>::value;

} // namespace grem::execution

#endif
