// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SPSC_QUEUE_HPP
#define GREM_CORE_DATA_SPSC_QUEUE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/synchronization.hpp>
#else
#include <GREM/core/assertions.hpp>
#endif
#include <cstddef>     // std::size_t
#include <type_traits> // std::is_nothrow_assignable_v
#include <utility>     // std::move, std::forward, std::declval

namespace grem {

template <typename T, std::size_t N>
class SPSCQueue {
public:
	static_assert(N >= 2 && (N & (N - 1)) == 0, "SPSCQueue buffer size must be a positive power of 2!");

	class Producer {
	public:
		template <typename Function>
		void waitEnqueueWith(Function&& function) requires(noexcept(std::forward<Function>(function)(std::declval<T&>()))) {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::RELAXED);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			queue.readIndex.wait(nextWriteIndex, MemoryOrder::ACQUIRE);
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			queue.writeIndex.store(nextWriteIndex, MemoryOrder::RELEASE);
			queue.writeIndex.notify_all();
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			GREM_ASSERT(queue.readIndex != nextWriteIndex);
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			queue.writeIndex = nextWriteIndex;
#endif
		}

		template <typename U>
		void waitEnqueue(U&& value) requires(std::is_nothrow_assignable_v<T&, decltype(std::forward<U>(value))>) { // NOLINT(cppcoreguidelines-missing-std-forward)
			waitEnqueueWith([&value](T& output) noexcept -> void { output = std::forward<U>(value); });
		}

		template <typename Function>
		bool tryEnqueueWith(Function&& function) requires(noexcept(std::forward<Function>(function)(std::declval<T&>()))) {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::RELAXED);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			if (queue.readIndex.load(MemoryOrder::ACQUIRE) == nextWriteIndex) {
				return false;
			}
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			queue.writeIndex.store(nextWriteIndex, MemoryOrder::RELEASE);
			queue.writeIndex.notify_all();
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			if (queue.readIndex == nextWriteIndex) {
				return false;
			}
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			queue.writeIndex = nextWriteIndex;
#endif
			return true;
		}

		template <typename U>
		[[nodiscard]] bool tryEnqueue(U&& value) requires(std::is_nothrow_assignable_v<T&, decltype(std::forward<U>(value))>) { // NOLINT(cppcoreguidelines-missing-std-forward)
			return tryEnqueueWith([&value](T& output) noexcept -> void { output = std::forward<U>(value); });
		}

	private:
		friend SPSCQueue;

		constexpr explicit Producer(SPSCQueue& queue) noexcept
			: queue(queue) {}

		SPSCQueue& queue;
	};

	class Consumer {
	public:
		template <typename U>
		void waitDequeue(U& value) requires(std::is_nothrow_assignable_v<U&, T &&>) {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
			queue.writeIndex.wait(currentReadIndex, MemoryOrder::ACQUIRE);
			value = std::move(queue.ringBuffer[currentReadIndex]);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex.store(nextReadIndex, MemoryOrder::RELEASE);
			queue.readIndex.notify_all();
#else
			const std::size_t currentReadIndex = queue.readIndex;
			GREM_ASSERT(queue.writeIndex != currentReadIndex);
			value = std::move(queue.ringBuffer[currentReadIndex]);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex = nextReadIndex;
#endif
		}

		template <typename U>
		[[nodiscard]] bool tryDequeue(U& value) requires(std::is_nothrow_assignable_v<U&, T &&>) {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
			if (queue.writeIndex.load(MemoryOrder::ACQUIRE) == currentReadIndex) {
				return false;
			}
			value = std::move(queue.ringBuffer[currentReadIndex]);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex.store(nextReadIndex, MemoryOrder::RELEASE);
			queue.readIndex.notify_all();
#else
			const std::size_t currentReadIndex = queue.readIndex;
			if (queue.writeIndex == currentReadIndex) {
				return false;
			}
			value = std::move(queue.ringBuffer[currentReadIndex]);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex = nextReadIndex;
#endif
			return true;
		}

	private:
		friend SPSCQueue;

		constexpr explicit Consumer(SPSCQueue& queue) noexcept
			: queue(queue) {}

		SPSCQueue& queue;
	};

	[[nodiscard]] Producer producer() {
		return Producer{*this};
	}

	[[nodiscard]] Consumer consumer() {
		return Consumer{*this};
	}

	void clear() {
#ifdef GREM_USE_MULTITHREADING
		readIndex.store(writeIndex.load(MemoryOrder::ACQUIRE), MemoryOrder::RELEASE);
		readIndex.notify_all();
#else
		readIndex = writeIndex;
#endif
	}

private:
	Array<T, N> ringBuffer;
#ifdef GREM_USE_MULTITHREADING
	alignas(64) Atomic<std::size_t> readIndex{};
	alignas(64) Atomic<std::size_t> writeIndex{};
#else
	std::size_t readIndex = 0;
	std::size_t writeIndex = 0;
#endif
};

} // namespace grem

#endif
