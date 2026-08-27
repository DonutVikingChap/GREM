// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_DOUBLE_ENDED_QUEUE_HPP
#define GREM_CORE_DATA_DOUBLE_ENDED_QUEUE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::rotate, std::shift_left, std::equal, std::lexicographical_compare, std::remove, std::remove_if
#include <climits>          // CHAR_BIT
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::iterator_traits, std::random_access_iterator_tag, std::reverse_iterator, std::input_iterator, std::sentinel_for, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <new>              // std::launder
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

namespace detail {

template <typename T>
class DoubleEndedQueueIterator {
public:
	using difference_type = std::ptrdiff_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	DoubleEndedQueueIterator() = default;

	constexpr DoubleEndedQueueIterator(pointer ringBuffer, std::size_t mask, std::size_t index) noexcept
		: ringBuffer(ringBuffer)
		, mask(mask)
		, index(index) {}

	constexpr operator DoubleEndedQueueIterator<const T>() const noexcept requires(!std::is_const_v<T>) {
		return DoubleEndedQueueIterator<const T>{ringBuffer, mask, index};
	}

	[[nodiscard]] constexpr reference operator*() const {
		return ringBuffer[index & mask];
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return &ringBuffer[index & mask];
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return ringBuffer[(index + static_cast<std::size_t>(n)) & mask];
	}

	constexpr DoubleEndedQueueIterator& operator++() {
		++index;
		return *this;
	}

	constexpr DoubleEndedQueueIterator& operator--() {
		--index;
		return *this;
	}

	constexpr DoubleEndedQueueIterator operator++(int) {
		return DoubleEndedQueueIterator{ringBuffer, mask, index++};
	}

	constexpr DoubleEndedQueueIterator operator--(int) {
		return DoubleEndedQueueIterator{ringBuffer, mask, index--};
	}

	constexpr DoubleEndedQueueIterator& operator+=(difference_type n) {
		index += static_cast<std::size_t>(n);
		return *this;
	}

	constexpr DoubleEndedQueueIterator& operator-=(difference_type n) {
		index -= static_cast<std::size_t>(n);
		return *this;
	}

	[[nodiscard]] friend constexpr DoubleEndedQueueIterator operator+(DoubleEndedQueueIterator a, difference_type b) {
		return DoubleEndedQueueIterator{a.ringBuffer, a.mask, a.index + static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr DoubleEndedQueueIterator operator+(difference_type a, DoubleEndedQueueIterator b) {
		return DoubleEndedQueueIterator{b.ringBuffer, b.mask, static_cast<std::size_t>(a) + b.index};
	}

	[[nodiscard]] friend constexpr DoubleEndedQueueIterator operator-(DoubleEndedQueueIterator a, difference_type b) {
		return DoubleEndedQueueIterator{a.ringBuffer, a.mask, a.index - static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr difference_type operator-(DoubleEndedQueueIterator a, DoubleEndedQueueIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return static_cast<difference_type>(a.index - b.index);
	}

	[[nodiscard]] friend constexpr bool operator==(DoubleEndedQueueIterator a, DoubleEndedQueueIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index == b.index;
	}

	[[nodiscard]] friend constexpr auto operator<=>(DoubleEndedQueueIterator a, DoubleEndedQueueIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index <=> b.index;
	}

private:
	pointer ringBuffer;
	std::size_t mask;
	std::size_t index;
};

} // namespace detail

template <typename T, typename Allocator = std::allocator<T>>
class DoubleEndedQueue {
public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = typename std::allocator_traits<Allocator>::pointer;
	using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
	using iterator = detail::DoubleEndedQueueIterator<T>;
	using const_iterator = detail::DoubleEndedQueueIterator<const T>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr DoubleEndedQueue() = default;

	constexpr explicit DoubleEndedQueue(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	constexpr DoubleEndedQueue(size_type count, const T& value, const Allocator& allocator = Allocator())
		: DoubleEndedQueue(allocator) {
		assign(count, value);
	}

	constexpr explicit DoubleEndedQueue(size_type count, const Allocator& allocator = Allocator())
		: DoubleEndedQueue(allocator) {
		reserve(count);
		while (count-- > 0) {
			emplace_back();
		}
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr DoubleEndedQueue(InputIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: DoubleEndedQueue(allocator) {
		assign(first, last);
	}

	constexpr DoubleEndedQueue(const DoubleEndedQueue& other)
		: DoubleEndedQueue(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr DoubleEndedQueue(const DoubleEndedQueue& other, const Allocator& allocator)
		: DoubleEndedQueue(other.begin(), other.end(), allocator) {}

	constexpr DoubleEndedQueue(DoubleEndedQueue&& other) noexcept
		: ringBuffer(std::exchange(other.ringBuffer, nullptr))
		, mask(std::exchange(other.mask, size_type{0}))
		, beginIndex(std::exchange(other.beginIndex, size_type{0}))
		, endIndex(std::exchange(other.endIndex, size_type{0}))
		, allocator(other.get_allocator()) {}

	constexpr DoubleEndedQueue(DoubleEndedQueue&& other, const Allocator& allocator) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: DoubleEndedQueue(allocator) {
		if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.get_allocator()) {
				reserve(other.size());
				for (T& element : other) {
					push_back(std::move(element));
				}
				other.clear();
				return;
			}
		}
		ringBuffer = std::exchange(other.ringBuffer, nullptr);
		mask = std::exchange(other.mask, size_type{0});
		beginIndex = std::exchange(other.beginIndex, size_type{0});
		endIndex = std::exchange(other.endIndex, size_type{0});
	}

	constexpr DoubleEndedQueue(std::initializer_list<T> ilist, const Allocator& allocator = Allocator())
		: DoubleEndedQueue(ilist.begin(), ilist.end(), allocator) {}

	constexpr ~DoubleEndedQueue() {
		reset();
	}

	constexpr DoubleEndedQueue& operator=(const DoubleEndedQueue& other) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					reset();
				}
			}
			allocator = other.get_allocator();
		}
		assign(other.begin(), other.end());
		return *this;
	}

	constexpr DoubleEndedQueue& operator=(DoubleEndedQueue&& other) noexcept(
		std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					clear();
					reserve(other.size());
					for (T& element : other) {
						push_back(std::move(element));
					}
					other.clear();
					return *this;
				}
			}
		}
		reset();
		ringBuffer = std::exchange(other.ringBuffer, nullptr);
		mask = std::exchange(other.mask, size_type{0});
		beginIndex = std::exchange(other.beginIndex, size_type{0});
		endIndex = std::exchange(other.endIndex, size_type{0});
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.get_allocator();
		}
		return *this;
	}

	constexpr DoubleEndedQueue& operator=(std::initializer_list<T> ilist) {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count <= size()) {
				while (size() > count) {
					pop_back();
				}
				for (T& element : *this) {
					element = value;
				}
				return;
			}
		}
		clear();
		reserve(count);
		while (count-- > 0) {
			push_back(value);
		}
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr void assign(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if constexpr (std::is_copy_assignable_v<T>) {
				if (count <= size()) {
					while (size() > count) {
						pop_back();
					}
					for (size_type i = 0; i < count; ++i) {
						(*this)[i] = first[i];
					}
					return;
				}
			}
			clear();
			reserve(count);
			for (size_type i = 0; i < count; ++i) {
				push_back(first[i]);
			}
		} else {
			clear();
			while (first != last) {
				push_back(*first++);
			}
		}
	}

	constexpr void assign(std::initializer_list<T> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename R>
	constexpr void assign_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		assign(std::begin(r), std::end(r));
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < size());
		return ringBuffer[(beginIndex + pos) & mask];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < size());
		return ringBuffer[(beginIndex + pos) & mask];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return ringBuffer[beginIndex & mask];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return ringBuffer[beginIndex & mask];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return ringBuffer[(endIndex - 1) & mask];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return ringBuffer[(endIndex - 1) & mask];
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{ringBuffer, mask, beginIndex};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{ringBuffer, mask, beginIndex};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{ringBuffer, mask, endIndex};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{ringBuffer, mask, endIndex};
	}

	[[nodiscard]] constexpr const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
		return reverse_iterator{end()};
	}

	[[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator{end()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	[[nodiscard]] constexpr reverse_iterator rend() noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
		return rend();
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return beginIndex == endIndex;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return endIndex - beginIndex;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return std::min({
			static_cast<size_type>(std::numeric_limits<difference_type>::max()),
			static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(T)),
			static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator)),
		});
	}

	constexpr void reserve(size_type newCapacity) {
		--newCapacity;
		for (std::size_t i = 1; i < sizeof(newCapacity) * CHAR_BIT; i *= 2) {
			newCapacity |= newCapacity >> i;
		}
		++newCapacity;
		if (newCapacity == 0) {
			throw std::length_error{"Maximum capacity exceeded."};
		}
		reallocate(std::max(size_type{2}, newCapacity));
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return (mask == 0) ? 0 : mask + 1;
	}

	constexpr void shrink_to_fit() {
		if (size() < capacity()) {
			*this = DoubleEndedQueue{*this};
		}
	}

	constexpr void clear() noexcept {
		while (beginIndex < endIndex) {
			std::allocator_traits<Allocator>::destroy(allocator, std::to_address(ringBuffer) + (beginIndex & mask));
			++beginIndex;
		}
		beginIndex = 0;
		endIndex = 0;
	}

	constexpr iterator insert(const_iterator pos, const T& value) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			push_front(value);
			return begin();
		}
		push_back(value);
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - 1, itEnd);
		return it;
	}

	constexpr iterator insert(const_iterator pos, T&& value) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			push_front(std::move(value));
			return begin();
		}
		push_back(std::move(value));
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - 1, itEnd);
		return it;
	}

	constexpr iterator insert(const_iterator pos, size_type count, const T& value) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			while (count-- > 0) {
				push_front(value);
			}
			return begin();
		}
		for (size_type i = 0; i < count; ++i) {
			push_back(value);
		}
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - static_cast<difference_type>(count), itEnd);
		return it;
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr iterator insert(const_iterator pos, InputIterator first, Sentinel last) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		size_type count = 0;
		while (first != last) {
			push_back(*first++);
			++count;
		}
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - static_cast<difference_type>(count), itEnd);
		return it;
	}

	constexpr iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
		return insert(pos, ilist.begin(), ilist.end());
	}

	template <typename R>
	constexpr iterator insert_range(const_iterator pos, R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		return insert(pos, std::begin(r), std::end(r));
	}

	template <typename R>
	constexpr void append_range(R&& r) {
		insert_range(end(), std::forward<R>(r));
	}

	template <typename... Args>
	constexpr iterator emplace(const_iterator pos, Args&&... args) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			emplace_front(std::forward<Args>(args)...);
			return begin();
		}
		emplace_back(std::forward<Args>(args)...);
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - 1, itEnd);
		return it;
	}

	constexpr iterator erase(const_iterator pos) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset < static_cast<difference_type>(size()));
		if (offset == 0) {
			pop_front();
			return begin();
		}
		const iterator it = begin() + offset;
		std::shift_left(it, end(), 1);
		pop_back();
		return it;
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		const difference_type offset = first - cbegin();
		const difference_type count = last - first;
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		GREM_ASSERT(count >= 0);
		GREM_ASSERT(offset + count <= static_cast<difference_type>(size()));
		if (offset == 0) {
			for (difference_type i = 0; i < count; ++i) {
				pop_front();
			}
			return begin();
		}
		const iterator it = begin() + offset;
		std::shift_left(it, end(), count);
		for (difference_type i = 0; i < count; ++i) {
			pop_back();
		}
		return it;
	}

	constexpr void push_back(const T& value) {
		emplace_back(value);
	}

	constexpr void push_back(T&& value) {
		emplace_back(std::move(value));
	}

	template <typename... Args>
	constexpr reference emplace_back(Args&&... args) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		if (size() >= capacity()) {
			GREM_ASSERT(size() == capacity());
			growCapacity();
		}
		GREM_ASSERT(size() < capacity());
		T* const p = std::to_address(ringBuffer) + (endIndex & mask);
		std::allocator_traits<Allocator>::construct(allocator, p, std::forward<Args>(args)...);
		++endIndex;
		return *std::launder(p);
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--endIndex;
		std::allocator_traits<Allocator>::destroy(allocator, std::to_address(ringBuffer) + (endIndex & mask));
	}

	constexpr void push_front(const T& value) {
		emplace_front(value);
	}

	constexpr void push_front(T&& value) {
		emplace_front(std::move(value));
	}

	template <typename... Args>
	constexpr reference emplace_front(Args&&... args) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		if (size() >= capacity()) {
			GREM_ASSERT(size() == capacity());
			growCapacity();
		}
		GREM_ASSERT(size() < capacity());
		T* const p = std::to_address(ringBuffer) + ((beginIndex - 1) & mask);
		std::allocator_traits<Allocator>::construct(allocator, p, std::forward<Args>(args)...);
		--beginIndex;
		return *std::launder(p);
	}

	constexpr void pop_front() {
		GREM_ASSERT(!empty());
		std::allocator_traits<Allocator>::destroy(allocator, std::to_address(ringBuffer) + (beginIndex & mask));
		++beginIndex;
	}

	constexpr void resize(size_type count) {
		if (count < size()) {
			for (size_type i = size() - count; i-- > 0;) {
				pop_back();
			}
		} else {
			for (size_type i = count - size(); i-- > 0;) {
				emplace_back();
			}
		}
	}

	constexpr void resize(size_type count, const T& value) {
		if (count < size()) {
			for (size_type i = size() - count; i-- > 0;) {
				pop_back();
			}
		} else {
			for (size_type i = count - size(); i-- > 0;) {
				push_back(value);
			}
		}
	}

	constexpr void swap(DoubleEndedQueue& other) noexcept(
		std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		using std::swap;
		swap(ringBuffer, other.ringBuffer);
		swap(mask, other.mask);
		swap(beginIndex, other.beginIndex);
		swap(endIndex, other.endIndex);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		} else if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			GREM_ASSERT(allocator == other.allocator);
		}
	}

	[[nodiscard]] constexpr bool operator==(const DoubleEndedQueue& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const DoubleEndedQueue& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const DoubleEndedQueue& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const DoubleEndedQueue& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const DoubleEndedQueue& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(DoubleEndedQueue& a, DoubleEndedQueue& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	template <typename U>
	friend constexpr size_type erase(DoubleEndedQueue& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(DoubleEndedQueue& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	constexpr void reset() noexcept {
		clear();
		if (ringBuffer) {
			std::allocator_traits<Allocator>::deallocate(allocator, ringBuffer, capacity());
			ringBuffer = nullptr;
			mask = 0;
		}
	}

	constexpr void growCapacity() {
		const size_type newCapacity = (mask + 1) * 2;
		if (newCapacity == 0) {
			throw std::length_error{"Maximum capacity exceeded."};
		}
		reallocate(newCapacity);
	}

	constexpr void reallocate(size_type newCapacity) {
		GREM_ASSERT(newCapacity >= 2 && (newCapacity & (newCapacity - 1)) == 0);
		const pointer newRingBuffer = std::allocator_traits<Allocator>::allocate(allocator, newCapacity);
		T* const pNewBegin = std::to_address(newRingBuffer);
		const size_type count = size();
		for (size_type i = 0; i < count; ++i) {
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				std::allocator_traits<Allocator>::construct(allocator, pNewBegin + i, std::move(ringBuffer[(beginIndex + i) & mask]));
			} else {
				try {
					std::allocator_traits<Allocator>::construct(allocator, pNewBegin + i, std::move(ringBuffer[(beginIndex + i) & mask]));
				} catch (...) {
					while (i-- > 0) {
						std::allocator_traits<Allocator>::destroy(allocator, pNewBegin + i);
					}
					std::allocator_traits<Allocator>::deallocate(allocator, newRingBuffer, newCapacity);
					throw;
				}
			}
		}
		reset();
		ringBuffer = newRingBuffer;
		mask = newCapacity - 1;
		beginIndex = 0;
		endIndex = count;
	}

	pointer ringBuffer = nullptr;
	size_type mask = 0;
	size_type beginIndex = 0;
	size_type endIndex = 0;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename T>
using DoubleEndedQueue = grem::DoubleEndedQueue<T, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
