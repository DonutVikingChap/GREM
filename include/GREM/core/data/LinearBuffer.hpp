// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_LINEAR_BUFFER_HPP
#define GREM_CORE_DATA_LINEAR_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Span.hpp>

#include <algorithm>       // std::max
#include <cstddef>         // std::size_t, std::ptrdiff_t, std::byte
#include <cstdint>         // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <cstring>         // std::memcpy
#include <functional>      // std::invoke
#include <memory>          // std::align
#include <memory_resource> // std::pmr::memory_resource
#include <new>             // std::launder
#include <type_traits>     // std::is_..._v, std::remove_..._t, std::false_type, std::true_type, std::integral_constant, std::common_type_t
#include <utility>         // std::forward, std::declval, std::...index_sequence, std::in_place_index...

namespace grem {

template <typename... Ts>
class LinearBuffer;

namespace detail {

template <typename T, std::size_t Index, typename... Ts>
struct LinearBufferIndexImpl;

template <typename T, std::size_t Index, typename First, typename... Rest>
struct LinearBufferIndexImpl<T, Index, First, Rest...> : LinearBufferIndexImpl<T, Index + 1, Rest...> {};

template <typename T, std::size_t Index, typename... Rest>
struct LinearBufferIndexImpl<T, Index, T, Rest...> : std::integral_constant<std::size_t, Index> {};

template <typename T>
struct LinearBufferMinElementSize : std::integral_constant<std::size_t, sizeof(T)> {};

template <typename T>
struct LinearBufferMinElementSize<T[]> : std::integral_constant<std::size_t, sizeof(std::size_t)> {};

template <typename T>
struct LinearBufferVisitorParameterType {
	using type = const T;
};

template <typename T>
struct LinearBufferVisitorParameterType<T[]> {
	using type = Span<const T>;
};

} // namespace detail

/// \cond
template <typename T, typename B>
struct linear_buffer_has_alternative;

template <typename T, typename First, typename... Rest>
struct linear_buffer_has_alternative<T, LinearBuffer<First, Rest...>> : linear_buffer_has_alternative<T, LinearBuffer<Rest...>> {};

template <typename T>
struct linear_buffer_has_alternative<T, LinearBuffer<>> : std::false_type {};

template <typename T, typename... Rest>
struct linear_buffer_has_alternative<T, LinearBuffer<T, Rest...>> : std::true_type {};
/// \endcond

template <typename T, typename B>
inline constexpr bool linear_buffer_has_alternative_v = linear_buffer_has_alternative<T, B>::value;

/// \cond
template <typename T, typename B>
struct linear_buffer_index;

template <typename T, typename... Ts>
struct linear_buffer_index<T, LinearBuffer<Ts...>> : detail::LinearBufferIndexImpl<T, 0, Ts...> {};
/// \endcond

template <typename T, typename B>
inline constexpr std::size_t linear_buffer_index_v = linear_buffer_index<T, B>::value;

/// \cond
template <std::size_t Index, typename B>
struct linear_buffer_alternative;

template <std::size_t Index, typename First, typename... Rest>
struct linear_buffer_alternative<Index, LinearBuffer<First, Rest...>> : linear_buffer_alternative<Index - 1, LinearBuffer<Rest...>> {};

template <typename T, typename... Rest>
struct linear_buffer_alternative<0, LinearBuffer<T, Rest...>> {
	using type = T;
};
/// \endcond

template <std::size_t Index, typename B>
using linear_buffer_alternative_t = typename linear_buffer_alternative<Index, B>::type;

/// \cond
template <typename B>
struct linear_buffer_size;

template <typename... Ts>
struct linear_buffer_size<LinearBuffer<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};
/// \endcond

template <typename B>
inline constexpr std::size_t linear_buffer_size_v = linear_buffer_size<B>::value;

template <typename... Ts>
class LinearBuffer {
public:
	static_assert((std::is_trivially_copyable_v<std::remove_extent_t<Ts>> && ...), "LinearBuffer requires all element types to be trivially copyable.");

	// clang-format off
    using index_type =
        std::conditional_t<sizeof...(Ts) < 255ull, std::uint8_t,
        std::conditional_t<sizeof...(Ts) < 65535ull, std::uint16_t,
        std::conditional_t<sizeof...(Ts) < 4294967295ull, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

	static constexpr index_type npos = sizeof...(Ts);

	explicit LinearBuffer(std::pmr::memory_resource* memoryResource, std::size_t nextChunkSize = 64) noexcept
		: memoryResource(memoryResource)
		, nextChunkSize(std::max(nextChunkSize, MIN_CHUNK_SIZE)) {
		GREM_ASSERT(memoryResource);
	}

	~LinearBuffer() = default;

	LinearBuffer(const LinearBuffer&) = delete;
	LinearBuffer(LinearBuffer&&) = delete;
	LinearBuffer& operator=(const LinearBuffer&) = delete;
	LinearBuffer& operator=(LinearBuffer&&) = delete;

	template <typename T>
	void push_back(const T& value) requires(!std::is_unbounded_array_v<T> && linear_buffer_has_alternative_v<T, LinearBuffer>) {
		constexpr std::size_t REQUIRED_SIZE = sizeof(TypeHeader) + sizeof(T) + sizeof(TypeHeader) + sizeof(ChunkTrailer);
		const std::size_t remainingMemorySize = static_cast<std::size_t>(remainingMemoryEnd - remainingMemoryBegin);
		if (remainingMemorySize < REQUIRED_SIZE) {
			[[unlikely]];
			const std::size_t newChunkSize = std::max(REQUIRED_SIZE, nextChunkSize);
			if (firstChunkBegin) {
				GREM_ASSERT(remainingMemorySize >= sizeof(TypeHeader) + sizeof(ChunkTrailer));
				std::byte* const oldChunkTrailer = remainingMemoryBegin;
				std::byte* const newChunkBegin = allocateChunk(newChunkSize);
				std::byte* const newChunkEnd = newChunkBegin + newChunkSize;
				const TypeHeader typeHeader{.typeIndex = npos};
				std::memcpy(oldChunkTrailer, &typeHeader, sizeof(TypeHeader));
				const ChunkTrailer chunkTrailer{.nextChunkBegin = newChunkBegin, .nextChunkEnd = newChunkEnd};
				std::memcpy(oldChunkTrailer + sizeof(TypeHeader), &chunkTrailer, sizeof(ChunkTrailer));
			} else {
				firstChunkBegin = allocateChunk(newChunkSize);
				firstChunkEnd = firstChunkBegin + newChunkSize;
			}
		}
		const TypeHeader typeHeader{.typeIndex = linear_buffer_index_v<T, LinearBuffer>};
		std::memcpy(remainingMemoryBegin, &typeHeader, sizeof(TypeHeader));
		std::memcpy(remainingMemoryBegin + sizeof(TypeHeader), &value, sizeof(T));
		remainingMemoryBegin += sizeof(TypeHeader) + sizeof(T);
	}

	template <typename T, typename... Args>
	void emplace_back(Args&&... args) requires(!std::is_unbounded_array_v<T> && linear_buffer_has_alternative_v<T, LinearBuffer> && std::is_constructible_v<T, Args...>) {
		return push_back<T>(T{std::forward<Args>(args)...});
	}

	template <typename T>
	Span<const T> append(Span<const T> values) requires(linear_buffer_has_alternative_v<T[], LinearBuffer>) {
		constexpr std::size_t MIN_REQUIRED_SIZE = sizeof(TypeHeader) + sizeof(ArrayHeader) + sizeof(TypeHeader) + sizeof(ChunkTrailer);
		const std::size_t remainingMemorySize = static_cast<std::size_t>(remainingMemoryEnd - remainingMemoryBegin);
		if (remainingMemorySize < MIN_REQUIRED_SIZE ||
			(!values.empty() &&
				!getAligned(alignof(T), values.size_bytes(), remainingMemoryBegin + sizeof(TypeHeader) + sizeof(ArrayHeader), remainingMemorySize - MIN_REQUIRED_SIZE))) {
			[[unlikely]];
			const std::size_t requiredSize = sizeof(TypeHeader) + sizeof(ArrayHeader) + alignof(T) - 1 + values.size_bytes() + sizeof(TypeHeader) + sizeof(ChunkTrailer);
			const std::size_t newChunkSize = std::max(requiredSize, nextChunkSize);
			if (firstChunkBegin) {
				GREM_ASSERT(remainingMemorySize >= sizeof(TypeHeader) + sizeof(ChunkTrailer));
				std::byte* const oldChunkTrailer = remainingMemoryBegin;
				std::byte* const newChunkBegin = allocateChunk(newChunkSize);
				std::byte* const newChunkEnd = newChunkBegin + newChunkSize;
				const TypeHeader typeHeader{.typeIndex = npos};
				std::memcpy(oldChunkTrailer, &typeHeader, sizeof(TypeHeader));
				const ChunkTrailer chunkTrailer{.nextChunkBegin = newChunkBegin, .nextChunkEnd = newChunkEnd};
				std::memcpy(oldChunkTrailer + sizeof(TypeHeader), &chunkTrailer, sizeof(ChunkTrailer));
			} else {
				firstChunkBegin = allocateChunk(newChunkSize);
				firstChunkEnd = firstChunkBegin + newChunkSize;
			}
		}
		const TypeHeader typeHeader{.typeIndex = linear_buffer_index_v<T[], LinearBuffer>};
		std::memcpy(remainingMemoryBegin, &typeHeader, sizeof(TypeHeader));
		const ArrayHeader arrayHeader{.elementCount = values.size()};
		std::memcpy(remainingMemoryBegin + sizeof(TypeHeader), &arrayHeader, sizeof(ArrayHeader));
		Span<const T> result{};
		if (values.empty()) {
			remainingMemoryBegin = remainingMemoryBegin + sizeof(TypeHeader) + sizeof(ArrayHeader);
		} else {
			void* const alignedPointer = getAligned(alignof(T), values.size_bytes(), remainingMemoryBegin + sizeof(TypeHeader) + sizeof(ArrayHeader),
				static_cast<std::size_t>(remainingMemoryEnd - remainingMemoryBegin) - MIN_REQUIRED_SIZE);
			GREM_ASSERT(alignedPointer);
			std::memcpy(alignedPointer, values.data(), values.size_bytes());
			result = Span<const T>{std::launder(reinterpret_cast<const T*>(static_cast<std::byte*>(alignedPointer))), values.size()};
			remainingMemoryBegin = static_cast<std::byte*>(alignedPointer) + values.size_bytes();
		}
		return result;
	}

	template <typename T>
	Span<const T> append(Span<T> values) requires(!std::is_const_v<T> && linear_buffer_has_alternative_v<T[], LinearBuffer>) {
		return append(Span<const T>{values});
	}

	template <typename Visitor>
	auto visit(Visitor&& visitor) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		using R = std::common_type_t<decltype(std::invoke(std::forward<Visitor>(visitor), std::declval<typename detail::LinearBufferVisitorParameterType<Ts>::type>()))...>;
		const std::byte* const end = remainingMemoryBegin;
		const std::byte* chunkEnd = firstChunkEnd;
		for (const std::byte* pointer = firstChunkBegin; pointer != end;) {
			GREM_ASSERT(static_cast<std::size_t>(chunkEnd - pointer) >= sizeof(TypeHeader));
			TypeHeader typeHeader{};
			std::memcpy(&typeHeader, pointer, sizeof(TypeHeader));
			pointer += sizeof(TypeHeader);
			const auto apply = [&]<std::size_t Index>(std::in_place_index_t<Index>) -> void {
				using MaybeArrayT = linear_buffer_alternative_t<Index, LinearBuffer>;
				if constexpr (std::is_unbounded_array_v<MaybeArrayT>) {
					using T = std::remove_extent_t<MaybeArrayT>;
					GREM_ASSERT(static_cast<std::size_t>(chunkEnd - pointer) >= sizeof(ArrayHeader));
					ArrayHeader arrayHeader{};
					std::memcpy(&arrayHeader, pointer, sizeof(ArrayHeader));
					pointer += sizeof(ArrayHeader);
					GREM_ASSERT(static_cast<std::size_t>(chunkEnd - pointer) >= arrayHeader.elementCount * sizeof(T));
					Span<const T> values{};
					if (arrayHeader.elementCount > 0) {
						if constexpr (alignof(T) > 1) {
							void* const aligned = getAligned(alignof(T), arrayHeader.elementCount * sizeof(T), const_cast<void*>(static_cast<const void*>(pointer)),
								static_cast<std::size_t>(chunkEnd - pointer) - (sizeof(TypeHeader) + sizeof(ChunkTrailer)));
							GREM_ASSERT(aligned);
							pointer = static_cast<const std::byte*>(aligned);
						}
						values = Span{std::launder(reinterpret_cast<const T*>(pointer)), arrayHeader.elementCount};
					}
					if constexpr (std::is_void_v<R>) {
						std::invoke(std::forward<Visitor>(visitor), values);
						pointer += arrayHeader.elementCount * sizeof(T);
					} else {
						if (std::invoke(std::forward<Visitor>(visitor), values)) {
							pointer += arrayHeader.elementCount * sizeof(T);
						} else {
							pointer = end;
						}
					}
				} else {
					using T = MaybeArrayT;
					GREM_ASSERT(static_cast<std::size_t>(chunkEnd - pointer) >= sizeof(T));
					alignas(T) std::byte storage[sizeof(T)];
					std::memcpy(storage, pointer, sizeof(T));
					const T& value = *std::launder(reinterpret_cast<const T*>(storage));
					if constexpr (std::is_void_v<R>) {
						std::invoke(std::forward<Visitor>(visitor), value);
						pointer += sizeof(T);
					} else {
						if (std::invoke(std::forward<Visitor>(visitor), value)) {
							pointer += sizeof(T);
						} else {
							pointer = end;
						}
					}
				}
			};
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((typeHeader.typeIndex == Indices) ? (apply(std::in_place_index<Indices>), true) : false) || ...)) {
					[[unlikely]];
					GREM_ASSERT(typeHeader.typeIndex == npos);
					GREM_ASSERT(static_cast<std::size_t>(chunkEnd - pointer) >= sizeof(ChunkTrailer));
					ChunkTrailer chunkTrailer{};
					std::memcpy(&chunkTrailer, pointer, sizeof(ChunkTrailer));
					GREM_ASSERT(chunkTrailer.nextChunkBegin);
					GREM_ASSERT(chunkTrailer.nextChunkEnd);
					GREM_ASSERT(chunkTrailer.nextChunkEnd - chunkTrailer.nextChunkBegin >= std::ptrdiff_t{MIN_CHUNK_SIZE});
					pointer = chunkTrailer.nextChunkBegin;
					chunkEnd = chunkTrailer.nextChunkEnd;
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
		}
		if constexpr (!std::is_void_v<R>) {
			return true;
		}
	}

private:
	struct TypeHeader {
		index_type typeIndex;
	};

	struct ArrayHeader {
		std::size_t elementCount;
	};

	struct ChunkTrailer {
		std::byte* nextChunkBegin;
		std::byte* nextChunkEnd;
	};

	static constexpr std::size_t MIN_CHUNK_SIZE = std::max({(sizeof(TypeHeader) + detail::LinearBufferMinElementSize<Ts>::value + sizeof(TypeHeader) + sizeof(ChunkTrailer))...});

	[[nodiscard]] static void* getAligned(std::size_t alignment, std::size_t size, void* pointer, std::size_t space) {
		return std::align(alignment, size, pointer, space);
	}

	[[nodiscard]] std::byte* allocateChunk(std::size_t newChunkSize) {
		GREM_ASSERT(memoryResource);
		std::byte* const newChunk = static_cast<std::byte*>(memoryResource->allocate(newChunkSize, 1));
		remainingMemoryBegin = newChunk;
		remainingMemoryEnd = newChunk + newChunkSize;
		nextChunkSize += nextChunkSize / 2;
		return newChunk;
	}

	std::pmr::memory_resource* memoryResource;
	std::byte* firstChunkBegin = nullptr;
	std::byte* firstChunkEnd = nullptr;
	std::byte* remainingMemoryBegin = nullptr;
	std::byte* remainingMemoryEnd = nullptr;
	std::size_t nextChunkSize;
};

} // namespace grem

#endif
