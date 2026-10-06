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

/**
 * %Subrange of a resource, corresponding to one of a fixed number of chunks
 * that the resource has been divded into.
 *
 * \tparam Resource resource type that the chunk is a subrange of, potentially
 *         const-qualified to indicate read-only access.
 * \tparam Projection function object that gets the full range of elements from
 *         the resource that is to be chunked. The default projection assumes
 *         that the range is the resource itself.
 */
template <typename Resource, auto Projection = std::identity{}>
class Chunk {
public:
	using resource_type = std::remove_const_t<Resource>;
	using iterator = decltype(std::begin(std::invoke(Projection, std::declval<Resource&>())));
	using size_type = decltype(std::size(std::invoke(Projection, std::declval<Resource&>())));
	using difference_type = iter_difference_t<iterator>;
	static_assert(resource<resource_type>, "Chunk must reference a valid resource type.");
	static_assert(random_access_iterator<iterator>, "Chunk iterator category must be random-access.");

	/**
	 * Construct a resource chunk.
	 *
	 * \param resource reference to the resource to chunk. Must outlive the
	 *        chunk's use.
	 * \param chunkIndex index of the specific chunk that this subrange
	 *        references. Must be less than `chunkCount`.
	 * \param chunkCount number of chunks that the resource has been divided
	 *        into. Must be positive, and greater than chunkIndex.
	 */
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

/// \cond
template <typename T>
struct is_chunk : std::false_type {};

template <typename Resource, auto Projection>
struct is_chunk<Chunk<Resource, Projection>> : std::true_type {};
/// \endcond

/**
 * Boolean that evaluates to true if the given type is a specialization of
 * Chunk.
 *
 * \tparam T type to check.
 */
template <typename T>
inline constexpr bool is_chunk_v = is_chunk<T>::value;

} // namespace grem::execution

#endif
