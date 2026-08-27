// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SPSC_SLIDING_WINDOW_QUEUE_HPP
#define GREM_CORE_DATA_SPSC_SLIDING_WINDOW_QUEUE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/synchronization.hpp>
#else
#include <GREM/core/assertions.hpp>
#endif
#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <iterator>    // std::random_access_iterator_tag
#include <type_traits> // std::is_..._v
#include <utility>     // std::move, std::forward, std::declval

namespace grem {

namespace detail {

template <typename T, std::size_t N>
class SPSCSlidingWindowQueueIterator {
public:
	static_assert(N >= 2 && (N & (N - 1)) == 0, "SPSCSlidingWindowQueue buffer size must be a positive power of 2!");

	using difference_type = std::size_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	SPSCSlidingWindowQueueIterator() = default;

	constexpr SPSCSlidingWindowQueueIterator(pointer ringBuffer, std::size_t index) noexcept
		: ringBuffer(ringBuffer)
		, index(index) {}

	constexpr operator SPSCSlidingWindowQueueIterator<const T, N>() const noexcept requires(!std::is_const_v<T>) {
		return SPSCSlidingWindowQueueIterator<const T, N>{ringBuffer, index};
	}

	[[nodiscard]] constexpr reference operator*() const {
		return ringBuffer[index];
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return &ringBuffer[index];
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return ringBuffer[(index + n) & (N - 1)];
	}

	constexpr SPSCSlidingWindowQueueIterator& operator++() {
		index = (index - 1) & (N - 1);
		return *this;
	}

	constexpr SPSCSlidingWindowQueueIterator& operator--() {
		index = (index + 1) & (N - 1);
		return *this;
	}

	constexpr SPSCSlidingWindowQueueIterator operator++(int) {
		SPSCSlidingWindowQueueIterator old = *this;
		++*this;
		return old;
	}

	constexpr SPSCSlidingWindowQueueIterator operator--(int) {
		SPSCSlidingWindowQueueIterator old = *this;
		--*this;
		return old;
	}

	constexpr SPSCSlidingWindowQueueIterator& operator+=(difference_type n) {
		index = (index + n) & (N - 1);
		return *this;
	}

	constexpr SPSCSlidingWindowQueueIterator& operator-=(difference_type n) {
		index = (index - n) & (N - 1);
		return *this;
	}

	[[nodiscard]] friend constexpr SPSCSlidingWindowQueueIterator operator+(SPSCSlidingWindowQueueIterator a, difference_type b) {
		return SPSCSlidingWindowQueueIterator{a.ringBuffer, (a.index + b) & (N - 1)};
	}

	[[nodiscard]] friend constexpr SPSCSlidingWindowQueueIterator operator+(difference_type a, SPSCSlidingWindowQueueIterator b) {
		return SPSCSlidingWindowQueueIterator{b.ringBuffer, (a + b.index) & (N - 1)};
	}

	[[nodiscard]] friend constexpr SPSCSlidingWindowQueueIterator operator-(SPSCSlidingWindowQueueIterator a, difference_type b) {
		return SPSCSlidingWindowQueueIterator{a.ringBuffer, (a.index - b) & (N - 1)};
	}

	[[nodiscard]] friend constexpr difference_type operator-(SPSCSlidingWindowQueueIterator a, SPSCSlidingWindowQueueIterator b) {
		GREM_ASSERT(a.ringBuffer = b.ringBuffer);
		return (a.index - b.index) & (N - 1);
	}

	[[nodiscard]] friend constexpr bool operator==(SPSCSlidingWindowQueueIterator a, SPSCSlidingWindowQueueIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index == b.index;
	}

	[[nodiscard]] friend constexpr auto operator<=>(SPSCSlidingWindowQueueIterator a, SPSCSlidingWindowQueueIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index <=> b.index;
	}

private:
	pointer ringBuffer;
	std::size_t index;
};

} // namespace detail

template <typename T, std::size_t N, std::size_t W>
class SPSCSlidingWindowQueue {
public:
	static_assert(N >= 2 && (N & (N - 1)) == 0, "SPSCSlidingWindowQueue buffer size must be a positive power of 2!");
	static_assert(W > 0 && W < N, "SPSCSlidingWindowQueue window size must be positive and smaller than the buffer size!");

	using size_type = std::size_t;
	using difference_type = std::size_t;
	using value_type = T;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using iterator = detail::SPSCSlidingWindowQueueIterator<T, N>;
	using const_iterator = detail::SPSCSlidingWindowQueueIterator<const T, N>;

	class WindowView {
	public:
		using size_type = SPSCSlidingWindowQueue::size_type;
		using difference_type = SPSCSlidingWindowQueue::difference_type;
		using value_type = SPSCSlidingWindowQueue::value_type;
		using pointer = SPSCSlidingWindowQueue::const_pointer;
		using const_pointer = SPSCSlidingWindowQueue::const_pointer;
		using reference = SPSCSlidingWindowQueue::const_reference;
		using const_reference = SPSCSlidingWindowQueue::const_reference;
		using iterator = SPSCSlidingWindowQueue::const_iterator;
		using const_iterator = SPSCSlidingWindowQueue::const_iterator;

		[[nodiscard]] constexpr iterator begin() const noexcept {
			return first;
		}

		[[nodiscard]] constexpr iterator end() const noexcept {
			return last;
		}

		[[nodiscard]] constexpr size_type size() const noexcept {
			return static_cast<size_type>(last - first);
		}

		[[nodiscard]] constexpr bool empty() const noexcept {
			return size() == 0;
		}

		[[nodiscard]] constexpr reference operator[](size_type n) const {
			return first[static_cast<difference_type>(n)];
		}

		[[nodiscard]] constexpr reference front() const {
			return *first;
		}

		[[nodiscard]] constexpr reference back() const {
			return *(last - 1);
		}

	private:
		friend SPSCSlidingWindowQueue;

		constexpr WindowView(const_iterator first, const_iterator last) noexcept
			: first(first)
			, last(last) {}

		const_iterator first;
		const_iterator last;
	};

	class Producer {
	public:
		[[nodiscard]] WindowView lookBehind() const {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::RELAXED);
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
#endif
			return WindowView{
				const_iterator{queue.ringBuffer.data(), currentWriteIndex - N},
				const_iterator{queue.ringBuffer.data(), currentWriteIndex},
			};
		}

		template <typename Function>
		void waitEnqueueWith(Function&& function) requires(noexcept(std::forward<Function>(function)(std::declval<T&>()))) {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::RELAXED);
			queue.readIndex.wait((currentWriteIndex + W) & (N - 1), MemoryOrder::ACQUIRE);
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			queue.writeIndex.store(nextWriteIndex, MemoryOrder::RELEASE);
			queue.writeIndex.notify_all();
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
			GREM_ASSERT(queue.readIndex != ((currentWriteIndex + W) & (N - 1)));
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
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
			if (queue.readIndex.load(MemoryOrder::ACQUIRE) == ((currentWriteIndex + W) & (N - 1))) {
				return false;
			}
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			queue.writeIndex.store(nextWriteIndex, MemoryOrder::RELEASE);
			queue.writeIndex.notify_all();
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
			if (queue.readIndex == ((currentWriteIndex + W) & (N - 1))) {
				return false;
			}
			std::forward<Function>(function)(queue.ringBuffer[currentWriteIndex]);
			const std::size_t nextWriteIndex = (currentWriteIndex + 1) & (N - 1);
			queue.writeIndex = nextWriteIndex;
#endif
			return true;
		}

		template <typename U>
		[[nodiscard]] bool tryEnqueue(U&& value) requires(std::is_nothrow_assignable_v<T&, decltype(std::forward<U>(value))>) { // NOLINT(cppcoreguidelines-missing-std-forward)
			return tryEnqueueWith([&value](T& output) noexcept -> void { output = std::forward<U>(value); });
		}

	private:
		friend SPSCSlidingWindowQueue;

		constexpr explicit Producer(SPSCSlidingWindowQueue& queue) noexcept
			: queue(queue) {}

		SPSCSlidingWindowQueue& queue;
	};

	class ConstProducer {
	public:
		[[nodiscard]] WindowView lookBehind() const {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::RELAXED);
#else
			const std::size_t currentWriteIndex = queue.writeIndex;
#endif
			return WindowView{
				const_iterator{queue.ringBuffer.data(), currentWriteIndex - N},
				const_iterator{queue.ringBuffer.data(), currentWriteIndex},
			};
		}

	private:
		friend SPSCSlidingWindowQueue;

		constexpr explicit ConstProducer(const SPSCSlidingWindowQueue& queue) noexcept
			: queue(queue) {}

		const SPSCSlidingWindowQueue& queue;
	};

	class Consumer {
	public:
		[[nodiscard]] WindowView getWindow() const {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
#else
			const std::size_t currentReadIndex = queue.readIndex;
#endif
			return WindowView{
				const_iterator{queue.ringBuffer.data(), currentReadIndex - W},
				const_iterator{queue.ringBuffer.data(), currentReadIndex},
			};
		}

		size_type advanceWindowToMostRecent() {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
			const std::size_t currentWriteIndex = queue.writeIndex.load(MemoryOrder::ACQUIRE);
			queue.readIndex.store(currentWriteIndex, MemoryOrder::RELEASE);
			queue.readIndex.notify_all();
#else
			const std::size_t currentReadIndex = queue.readIndex;
			const std::size_t currentWriteIndex = queue.writeIndex;
			queue.readIndex = currentWriteIndex;
#endif
			return (currentWriteIndex - currentReadIndex) & (N - 1);
		}

		void waitAdvanceWindow() {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
			queue.writeIndex.wait(currentReadIndex, MemoryOrder::ACQUIRE);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex.store(nextReadIndex, MemoryOrder::RELEASE);
			queue.readIndex.notify_all();
#else
			const std::size_t currentReadIndex = queue.readIndex;
			GREM_ASSERT(queue.writeIndex != currentReadIndex);
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex = nextReadIndex;
#endif
		}

		[[nodiscard]] bool tryAdvanceWindow() {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
			if (queue.writeIndex.load(MemoryOrder::ACQUIRE) == currentReadIndex) {
				return false;
			}
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex.store(nextReadIndex, MemoryOrder::RELEASE);
			queue.readIndex.notify_all();
#else
			const std::size_t currentReadIndex = queue.readIndex;
			if (queue.writeIndex == currentReadIndex) {
				return false;
			}
			const std::size_t nextReadIndex = (currentReadIndex + 1) & (N - 1);
			queue.readIndex = nextReadIndex;
#endif
			return true;
		}

	private:
		friend SPSCSlidingWindowQueue;

		constexpr explicit Consumer(SPSCSlidingWindowQueue& queue) noexcept
			: queue(queue) {}

		SPSCSlidingWindowQueue& queue;
	};

	class ConstConsumer {
	public:
		[[nodiscard]] WindowView getWindow() const {
#ifdef GREM_USE_MULTITHREADING
			const std::size_t currentReadIndex = queue.readIndex.load(MemoryOrder::RELAXED);
#else
			const std::size_t currentReadIndex = queue.readIndex;
#endif
			return WindowView{
				const_iterator{queue.ringBuffer.data(), currentReadIndex - W},
				const_iterator{queue.ringBuffer.data(), currentReadIndex},
			};
		}

	private:
		friend SPSCSlidingWindowQueue;

		constexpr explicit ConstConsumer(const SPSCSlidingWindowQueue& queue) noexcept
			: queue(queue) {}

		const SPSCSlidingWindowQueue& queue;
	};

	[[nodiscard]] Producer producer() {
		return Producer{*this};
	}

	[[nodiscard]] ConstProducer producer() const {
		return ConstProducer{*this};
	}

	[[nodiscard]] Consumer consumer() {
		return Consumer{*this};
	}

	[[nodiscard]] ConstConsumer consumer() const {
		return ConstConsumer{*this};
	}

	[[nodiscard]] constexpr size_type getWindowSize() const noexcept {
		return W;
	}

	template <typename Function>
	void unsynchronizedFillWith(Function&& function) requires(noexcept(function(std::declval<T&>()))) { // NOLINT(cppcoreguidelines-missing-std-forward)
#ifdef GREM_USE_MULTITHREADING
		const std::size_t currentWriteIndex = writeIndex.load(MemoryOrder::ACQUIRE);
		readIndex.store(currentWriteIndex, MemoryOrder::RELEASE);
		for (std::size_t i = W; i-- > 0;) {
			function(ringBuffer[(currentWriteIndex - 1 - i) & (N - 1)]);
		}
#else
		const std::size_t currentWriteIndex = writeIndex;
		readIndex = currentWriteIndex;
		for (std::size_t i = W; i-- > 0;) {
			function(ringBuffer[(currentWriteIndex - 1 - i) & (N - 1)]);
		}
#endif
	}

	template <typename U>
	void unsynchronizedFill(const U& value) requires(std::is_nothrow_assignable_v<T&, const U&>) {
		unsynchronizedFillWith([&value](T& output) noexcept -> void { output = value; });
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
