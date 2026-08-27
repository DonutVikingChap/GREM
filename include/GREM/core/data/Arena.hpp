// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ARENA_HPP
#define GREM_CORE_DATA_ARENA_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstddef>         // std::size_t
#include <memory>          // std::align
#include <memory_resource> // std::pmr::memory_resource, std::pmr::get_default_resource
#include <new>             // std::bad_array_new_length
#include <utility>         // std::exchange

namespace grem {

template <typename T>
class ArenaAllocator; // Forward declaration.

class ArenaResource : public std::pmr::memory_resource {
public:
	GREM_ALWAYS_INLINE ArenaResource(Span<byte> initialMemory, size_t nextExtraMemoryChunkSize, std::pmr::memory_resource* memoryResource) noexcept
		: remainingMemoryBegin(initialMemory.data())
		, remainingMemorySize(initialMemory.size())
		, nextExtraMemoryChunkSize(nextExtraMemoryChunkSize)
		, memoryResource(memoryResource) {
		GREM_ASSERT(memoryResource);
	}

	~ArenaResource() override {
		for (const ExtraMemoryAllocation& chunk : extraMemory) {
			memoryResource->deallocate(chunk.memory, chunk.size, chunk.alignment);
		}
	}

	ArenaResource(const ArenaResource&) = delete;
	ArenaResource& operator=(const ArenaResource&) = delete;

	ArenaResource(ArenaResource&& other) noexcept
		: remainingMemoryBegin(std::exchange(other.remainingMemoryBegin, nullptr))
		, remainingMemorySize(std::exchange(other.remainingMemorySize, size_t{0}))
		, nextExtraMemoryChunkSize(std::exchange(other.nextExtraMemoryChunkSize, size_t{0}))
		, memoryResource(std::exchange(other.memoryResource, nullptr))
		, extraMemory(std::exchange(other.extraMemory, {}))
		, nextAvailableExtraMemoryIndex(std::exchange(other.nextAvailableExtraMemoryIndex, size_t{0})) {}

	ArenaResource& operator=(ArenaResource&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		remainingMemoryBegin = std::exchange(other.remainingMemoryBegin, nullptr);
		remainingMemorySize = std::exchange(other.remainingMemorySize, size_t{0});
		nextExtraMemoryChunkSize = std::exchange(other.nextExtraMemoryChunkSize, size_t{0});
		memoryResource = std::exchange(other.memoryResource, nullptr);
		extraMemory = std::exchange(other.extraMemory, {});
		nextAvailableExtraMemoryIndex = std::exchange(other.nextAvailableExtraMemoryIndex, size_t{0});
		return *this;
	}

	GREM_ALWAYS_INLINE void reset(Span<byte> initialMemory) noexcept {
		remainingMemoryBegin = initialMemory.data();
		remainingMemorySize = initialMemory.size();
		nextAvailableExtraMemoryIndex = 0;
	}

protected:
	void* do_allocate(std::size_t size, std::size_t alignment) final {
		if (size == 0) {
			[[unlikely]];
			size = 1;
		}

		void* result = std::align(alignment, size, remainingMemoryBegin, remainingMemorySize);
		if (!result) {
			[[unlikely]];
			result = allocateExtraMemory(size, alignment);
		}
		remainingMemoryBegin = static_cast<byte*>(remainingMemoryBegin) + size;
		remainingMemorySize -= size;
		return result;
	}

	void do_deallocate(void*, std::size_t, std::size_t) final {}

	[[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final {
		return this == &other;
	}

private:
	template <typename T>
	friend class ArenaAllocator;

	struct ExtraMemoryAllocation {
		void* memory;
		size_t size;
		size_t alignment;
	};

	[[nodiscard]] GREM_NOINLINE void* allocateExtraMemory(size_t size, size_t alignment) {
		ExtraMemoryAllocation* foundChunk = nullptr;
		size_t chunkIndex = nextAvailableExtraMemoryIndex;
		while (chunkIndex < extraMemory.size()) {
			ExtraMemoryAllocation& chunk = extraMemory[chunkIndex++];
			if (chunk.size >= size && chunk.alignment >= alignment) {
				foundChunk = &chunk;
				nextAvailableExtraMemoryIndex = chunkIndex;
				break;
			}
		}

		if (!foundChunk) {
			const size_t newChunkSize = max(size, nextExtraMemoryChunkSize);
			const size_t newChunkAlignment = max(alignment, alignof(max_align_t));
			if (extraMemory.capacity() < 4) {
				extraMemory.reserve(4);
			}
			void* const newChunkMemory = memoryResource->allocate(newChunkSize, newChunkAlignment);
			try {
				foundChunk = &extraMemory.emplace_back(ExtraMemoryAllocation{
					.memory = newChunkMemory,
					.size = newChunkSize,
					.alignment = newChunkAlignment,
				});
			} catch (...) {
				memoryResource->deallocate(newChunkMemory, newChunkSize, newChunkAlignment);
				throw;
			}
			nextExtraMemoryChunkSize += nextExtraMemoryChunkSize / 2;
			nextAvailableExtraMemoryIndex = chunkIndex + 1;
		}

		remainingMemoryBegin = foundChunk->memory;
		remainingMemorySize = foundChunk->size;
		return remainingMemoryBegin;
	}

	void* remainingMemoryBegin;
	size_t remainingMemorySize;
	size_t nextExtraMemoryChunkSize;
	std::pmr::memory_resource* memoryResource;
	pmr::ArrayList<ExtraMemoryAllocation> extraMemory{memoryResource};
	size_t nextAvailableExtraMemoryIndex = 0;
};

template <size_t InplaceSize = 1024, size_t InplaceAlignment = alignof(max_align_t)>
class Arena : public ArenaResource {
public:
	static constexpr size_t INPLACE_SIZE = InplaceSize;
	static constexpr size_t INPLACE_ALIGNMENT = InplaceAlignment;

	GREM_ALWAYS_INLINE Arena() noexcept
		: Arena(std::pmr::get_default_resource()) {}

	GREM_ALWAYS_INLINE explicit Arena(std::pmr::memory_resource* memoryResource) noexcept
		: Arena(max(size_t{1024}, InplaceSize + InplaceSize / 2), memoryResource) {}

	GREM_ALWAYS_INLINE explicit Arena(size_t nextExtraMemoryChunkSize) noexcept
		: Arena(nextExtraMemoryChunkSize, std::pmr::get_default_resource()) {}

	GREM_ALWAYS_INLINE Arena(size_t nextExtraMemoryChunkSize, std::pmr::memory_resource* memoryResource) noexcept
		: ArenaResource(initialMemory, nextExtraMemoryChunkSize, memoryResource) {}

	~Arena() override = default;

	Arena(const Arena&) = delete;
	Arena(Arena&&) = delete;
	Arena& operator=(const Arena&) = delete;
	Arena& operator=(Arena&&) = delete;

	GREM_ALWAYS_INLINE void release() noexcept {
		reset(initialMemory);
	}

private:
	alignas(InplaceAlignment) byte initialMemory[InplaceSize];
};

template <size_t InplaceAlignment>
class Arena<0, InplaceAlignment> : public ArenaResource {
public:
	static constexpr size_t INPLACE_SIZE = 0;
	static constexpr size_t INPLACE_ALIGNMENT = 0;

	GREM_ALWAYS_INLINE Arena() noexcept
		: Arena(std::pmr::get_default_resource()) {}

	GREM_ALWAYS_INLINE explicit Arena(std::pmr::memory_resource* memoryResource) noexcept
		: Arena(size_t{1024}, memoryResource) {}

	GREM_ALWAYS_INLINE explicit Arena(size_t nextExtraMemoryChunkSize) noexcept
		: Arena(nextExtraMemoryChunkSize, std::pmr::get_default_resource()) {}

	GREM_ALWAYS_INLINE Arena(size_t nextExtraMemoryChunkSize, std::pmr::memory_resource* memoryResource) noexcept
		: ArenaResource({}, nextExtraMemoryChunkSize, memoryResource) {}

	~Arena() override = default;

	Arena(const Arena&) = delete;
	Arena(Arena&&) noexcept = default;
	Arena& operator=(const Arena&) = delete;
	Arena& operator=(Arena&&) noexcept = default;

	GREM_ALWAYS_INLINE void release() noexcept {
		reset({});
	}
};

template <typename T>
class ArenaAllocator {
public:
	using value_type = T;

	GREM_ALWAYS_INLINE ArenaAllocator(ArenaResource* arena) noexcept
		: arena(arena) {
		GREM_ASSERT(arena);
	}

	template <typename U>
	GREM_ALWAYS_INLINE ArenaAllocator(const ArenaAllocator<U>& other) noexcept
		: arena(other.arena) {}

	~ArenaAllocator() = default;

	ArenaAllocator(const ArenaAllocator& other) noexcept = default;
	ArenaAllocator(ArenaAllocator&& other) noexcept = default;
	ArenaAllocator& operator=(const ArenaAllocator& other) noexcept = default;
	ArenaAllocator& operator=(ArenaAllocator&& other) noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE T* allocate(std::size_t n) {
		if (n > Limits<std::size_t>::MAX / sizeof(T)) {
			throw std::bad_array_new_length{};
		}
		return static_cast<T*>(arena->ArenaResource::do_allocate(n * sizeof(T), alignof(T)));
	}

	GREM_ALWAYS_INLINE void deallocate(T*, std::size_t) noexcept {}

	template <typename U>
	[[nodiscard]] GREM_ALWAYS_INLINE bool operator==(const ArenaAllocator<U>& other) const noexcept {
		return arena == other.arena;
	}

private:
	template <typename U>
	friend class ArenaAllocator;

	ArenaResource* arena;
};

} // namespace grem

#endif
