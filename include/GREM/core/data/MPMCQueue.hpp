// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_MPMC_QUEUE_HPP
#define GREM_CORE_DATA_MPMC_QUEUE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/synchronization.hpp>
#else
#include <GREM/core/assertions.hpp>
#endif
#include <algorithm>   // std::max
#include <cstddef>     // std::size_t
#include <type_traits> // std::is_..._v
#include <utility>     // std::move, std::forward, std::declval

namespace grem {

template <typename T, std::size_t N>
class MPMCQueue {
public:
	static_assert(N >= 2 && (N & (N - 1)) == 0, "MPMCQueue buffer size must be a positive power of 2!");

	template <typename Function>
	void waitEnqueueWith(Function&& function) requires(noexcept(std::forward<Function>(function)(std::declval<T&>()))) {
#ifdef GREM_USE_MULTITHREADING
		const std::size_t currentWriteIndex = writeIndex.fetch_add(1, MemoryOrder::RELAXED);
		const std::size_t writeGeneration = (currentWriteIndex / N) * 2;
		Storage& storage = ringBuffer[currentWriteIndex & (N - 1)];
		while (storage.generation.load(MemoryOrder::ACQUIRE) != writeGeneration) {
		}
		std::forward<Function>(function)(storage.value);
		storage.generation.store(writeGeneration + 1, MemoryOrder::RELEASE);
#else
		GREM_ASSERT(writeIndex - readIndex < N);
		const std::size_t currentWriteIndex = writeIndex++;
		T& value = ringBuffer[currentWriteIndex & (N - 1)];
		std::forward<Function>(function)(value);
#endif
	}

	template <typename U>
	void waitEnqueue(U&& value) requires(std::is_nothrow_assignable_v<T&, decltype(std::forward<U>(value))>) { // NOLINT(cppcoreguidelines-missing-std-forward)
		waitEnqueueWith([&value](T& output) noexcept -> void { output = std::forward<U>(value); });
	}

	template <typename Function>
	bool tryEnqueueWith(Function&& function) requires(noexcept(std::forward<Function>(function)(std::declval<T&>()))) {
#ifdef GREM_USE_MULTITHREADING
		std::size_t currentWriteIndex = writeIndex.fetch_add(1, MemoryOrder::RELAXED);
		while (true) {
			const std::size_t writeGeneration = (currentWriteIndex / N) * 2;
			Storage& storage = ringBuffer[currentWriteIndex & (N - 1)];
			if (storage.generation.load(MemoryOrder::ACQUIRE) == writeGeneration) {
				if (writeIndex.compare_exchange_strong(currentWriteIndex, currentWriteIndex + 1, MemoryOrder::ACQUIRE)) {
					std::forward<Function>(function)(storage.value);
					storage.generation.store(writeGeneration + 1, MemoryOrder::RELEASE);
					return true;
				}
			} else {
				const std::size_t nextWriteIndex = writeIndex.load(MemoryOrder::ACQUIRE);
				if (nextWriteIndex == currentWriteIndex) {
					break;
				}
				currentWriteIndex = nextWriteIndex;
			}
		}
		return false;
#else
		if (writeIndex - readIndex >= N) {
			return false;
		}
		std::size_t currentWriteIndex = writeIndex++;
		T& value = ringBuffer[currentWriteIndex & (N - 1)];
		std::forward<Function>(function)(value);
		return true;
#endif
	}

	template <typename U>
	[[nodiscard]] bool tryEnqueue(U&& value) requires(std::is_nothrow_assignable_v<T&, decltype(std::forward<U>(value))>) { // NOLINT(cppcoreguidelines-missing-std-forward)
		return tryEnqueueWith([&value](T& output) noexcept -> void { output = std::forward<U>(value); });
	}

	template <typename U>
	void waitDequeue(U& value) requires(std::is_nothrow_assignable_v<U&, T &&>) {
#ifdef GREM_USE_MULTITHREADING
		const std::size_t currentReadIndex = readIndex.fetch_add(1, MemoryOrder::RELAXED);
		const std::size_t readGeneration = (currentReadIndex / N) * 2 + 1;
		Storage& storage = ringBuffer[currentReadIndex & (N - 1)];
		while (storage.generation.load(MemoryOrder::ACQUIRE) != readGeneration) {
		}
		value = std::move(storage.value);
		storage.generation.store(readGeneration + 1, MemoryOrder::RELEASE);
#else
		GREM_ASSERT(readIndex < writeIndex);
		const std::size_t currentReadIndex = readIndex++;
		value = std::move(ringBuffer[currentReadIndex & (N - 1)]);
#endif
	}

	template <typename U>
	[[nodiscard]] bool tryDequeue(U& value) requires(std::is_nothrow_assignable_v<U&, T &&>) {
#ifdef GREM_USE_MULTITHREADING
		std::size_t currentReadIndex = readIndex.load(MemoryOrder::RELAXED);
		while (true) {
			const std::size_t readGeneration = (currentReadIndex / N) * 2 + 1;
			Storage& storage = ringBuffer[currentReadIndex & (N - 1)];
			if (storage.generation.load(MemoryOrder::ACQUIRE) == readGeneration) {
				if (readIndex.compare_exchange_strong(currentReadIndex, currentReadIndex + 1, MemoryOrder::ACQUIRE)) {
					value = std::move(storage.value);
					storage.generation.store(readGeneration + 1, MemoryOrder::RELEASE);
					return true;
				}
			} else {
				const std::size_t nextReadIndex = readIndex.load(MemoryOrder::ACQUIRE);
				if (nextReadIndex == currentReadIndex) {
					break;
				}
				currentReadIndex = nextReadIndex;
			}
		}
#else
		if (readIndex >= writeIndex) {
			return false;
		}
		std::size_t currentReadIndex = readIndex++;
		value = std::move(ringBuffer[currentReadIndex & (N - 1)]);
#endif
		return false;
	}

	[[nodiscard]] std::size_t getEstimatedSize() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return writeIndex.load(MemoryOrder::RELAXED) - readIndex.load(MemoryOrder::RELAXED);
#else
		return writeIndex - readIndex;
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	struct alignas(std::max(std::size_t{64}, alignof(T))) Storage {
		T value;
		Atomic<std::size_t> generation{};
	};

	Array<Storage, N> ringBuffer;
	alignas(64) Atomic<std::size_t> readIndex{};
	alignas(64) Atomic<std::size_t> writeIndex{};
#else
	Array<T, N> ringBuffer;
	std::size_t readIndex = 0;
	std::size_t writeIndex = 0;
#endif
};

} // namespace grem

#endif
