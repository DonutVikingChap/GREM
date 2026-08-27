// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_INPLACE_RING_BUFFER_HPP
#define GREM_CORE_DATA_INPLACE_RING_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::rotate, std::shift_left, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::move
#include <climits>          // CHAR_BIT
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::iterator_traits, std::random_access_iterator_tag, std::reverse_iterator, std::input_iterator, std::sentinel_for, std::begin, std::end
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v, std::conditional_t
#include <utility>          // std::move, std::forward

namespace grem {

namespace detail {

template <typename T, std::size_t Capacity>
class InplaceRingBufferIterator {
private:
	// clang-format off
    using Size =
        std::conditional_t<Capacity <= 255ull, std::uint8_t,
        std::conditional_t<Capacity <= 65535ull, std::uint16_t,
        std::conditional_t<Capacity <= 4294967295ull, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

public:
	static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "InplaceRingBufferIterator capacity must be a positive power of 2!");

	using difference_type = std::ptrdiff_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	InplaceRingBufferIterator() = default;

	constexpr InplaceRingBufferIterator(pointer ringBuffer, Size index) noexcept
		: ringBuffer(ringBuffer)
		, index(index) {}

	constexpr operator InplaceRingBufferIterator<const T, Capacity>() const noexcept requires(!std::is_const_v<T>) {
		return InplaceRingBufferIterator<const T, Capacity>{ringBuffer, index};
	}

	[[nodiscard]] constexpr reference operator*() const {
		return ringBuffer[index & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return &ringBuffer[index & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return ringBuffer[static_cast<Size>(index + static_cast<Size>(n)) & static_cast<Size>(Capacity - 1)];
	}

	constexpr InplaceRingBufferIterator& operator++() {
		++index;
		return *this;
	}

	constexpr InplaceRingBufferIterator& operator--() {
		--index;
		return *this;
	}

	constexpr InplaceRingBufferIterator operator++(int) {
		return InplaceRingBufferIterator{ringBuffer, index++};
	}

	constexpr InplaceRingBufferIterator operator--(int) {
		return InplaceRingBufferIterator{ringBuffer, index--};
	}

	constexpr InplaceRingBufferIterator& operator+=(difference_type n) {
		index += static_cast<Size>(n);
		return *this;
	}

	constexpr InplaceRingBufferIterator& operator-=(difference_type n) {
		index -= static_cast<Size>(n);
		return *this;
	}

	[[nodiscard]] friend constexpr InplaceRingBufferIterator operator+(InplaceRingBufferIterator a, difference_type b) {
		return InplaceRingBufferIterator{a.ringBuffer, static_cast<Size>(a.index + static_cast<Size>(b))};
	}

	[[nodiscard]] friend constexpr InplaceRingBufferIterator operator+(difference_type a, InplaceRingBufferIterator b) {
		return InplaceRingBufferIterator{b.ringBuffer, static_cast<Size>(static_cast<Size>(a) + b.index)};
	}

	[[nodiscard]] friend constexpr InplaceRingBufferIterator operator-(InplaceRingBufferIterator a, difference_type b) {
		return InplaceRingBufferIterator{a.ringBuffer, static_cast<Size>(a.index - static_cast<Size>(b))};
	}

	[[nodiscard]] friend constexpr difference_type operator-(InplaceRingBufferIterator a, InplaceRingBufferIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return static_cast<difference_type>(static_cast<Size>(a.index - b.index));
	}

	[[nodiscard]] friend constexpr bool operator==(InplaceRingBufferIterator a, InplaceRingBufferIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index == b.index;
	}

	[[nodiscard]] friend constexpr auto operator<=>(InplaceRingBufferIterator a, InplaceRingBufferIterator b) {
		GREM_ASSERT(a.ringBuffer == b.ringBuffer);
		return a.index <=> b.index;
	}

private:
	pointer ringBuffer;
	Size index;
};

} // namespace detail

template <typename T, std::size_t N>
class InplaceRingBuffer {
private:
	static constexpr std::size_t Capacity = [] {
		std::size_t capacity = N;
		--capacity;
		for (std::size_t i = 1; i < sizeof(capacity) * CHAR_BIT; i *= 2) {
			capacity |= capacity >> i;
		}
		++capacity;
		GREM_ASSERT(capacity > 0);
		return capacity;
	}();

	// clang-format off
    using Size =
        std::conditional_t<Capacity <= 255ull, std::uint8_t,
        std::conditional_t<Capacity <= 65535ull, std::uint16_t,
        std::conditional_t<Capacity <= 4294967295ull, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = detail::InplaceRingBufferIterator<T, Capacity>;
	using const_iterator = detail::InplaceRingBufferIterator<const T, Capacity>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr InplaceRingBuffer() = default;

	constexpr InplaceRingBuffer(size_type count, const T& value)
		: InplaceRingBuffer() {
		assign(count, value);
	}

	constexpr explicit InplaceRingBuffer(size_type count)
		: InplaceRingBuffer() {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr InplaceRingBuffer(InputIterator first, Sentinel last)
		: InplaceRingBuffer() {
		assign(first, last);
	}

	constexpr InplaceRingBuffer(const InplaceRingBuffer&) = default;

	constexpr InplaceRingBuffer(const InplaceRingBuffer& other) requires(!std::is_trivially_copyable_v<T>)
		: InplaceRingBuffer(other.begin(), other.end()) {}

	constexpr InplaceRingBuffer(InplaceRingBuffer&&) noexcept = default;

	constexpr InplaceRingBuffer(InplaceRingBuffer&& other) noexcept(
		std::is_nothrow_move_constructible_v<T>) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		requires(!std::is_trivially_copyable_v<T>)
		: InplaceRingBuffer() {
		for (T& element : other) {
			push_back(std::move(element));
		}
		other.clear();
	}

	constexpr InplaceRingBuffer(std::initializer_list<T> ilist)
		: InplaceRingBuffer(ilist.begin(), ilist.end()) {}

	constexpr ~InplaceRingBuffer() = default;

	constexpr ~InplaceRingBuffer() requires(!std::is_trivially_destructible_v<T>) {
		clear();
	}

	constexpr InplaceRingBuffer& operator=(const InplaceRingBuffer&) = default;

	constexpr InplaceRingBuffer& operator=(const InplaceRingBuffer& other) requires(!std::is_trivially_copyable_v<T>) {
		if (this == &other) {
			return *this;
		}
		assign(other.begin(), other.end());
		return *this;
	}

	constexpr InplaceRingBuffer& operator=(InplaceRingBuffer&&) noexcept = default;

	constexpr InplaceRingBuffer& operator=(InplaceRingBuffer&& other) noexcept(
		std::is_nothrow_move_constructible_v<T> && // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		std::is_nothrow_move_assignable_v<T>) requires(!std::is_trivially_copyable_v<T>) {
		if (this == &other) {
			return *this;
		}
		if (size() == other.size()) {
			std::move(other.begin(), other.end(), begin());
			other.clear();
			return *this;
		}
		clear();
		for (T& element : other) {
			push_back(std::move(element));
		}
		other.clear();
		return *this;
	}

	constexpr InplaceRingBuffer& operator=(std::initializer_list<T> ilist) {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count <= size()) {
				resize(count);
				for (T& element : *this) {
					element = value;
				}
				return;
			}
		}
		clear();
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
					resize(count);
					for (size_type i = 0; i < count; ++i) {
						(*this)[i] = first[i];
					}
					return;
				}
			}
			clear();
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
		return ringBuffer[static_cast<Size>(beginIndex + static_cast<Size>(pos)) & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < size());
		return ringBuffer[static_cast<Size>(beginIndex + static_cast<Size>(pos)) & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return ringBuffer[beginIndex & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return ringBuffer[beginIndex & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return ringBuffer[static_cast<Size>(endIndex - 1) & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return ringBuffer[static_cast<Size>(endIndex - 1) & static_cast<Size>(Capacity - 1)];
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{ringBuffer, beginIndex};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{ringBuffer, beginIndex};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{ringBuffer, endIndex};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{ringBuffer, endIndex};
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
		return static_cast<size_type>(static_cast<Size>(endIndex - beginIndex));
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return N;
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return N;
	}

	constexpr void clear() noexcept {
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

	constexpr iterator insert_unspecified_value(const_iterator pos) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			push_front_unspecified_value();
			return begin();
		}
		push_back_unspecified_value();
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - 1, itEnd);
		return it;
	}

	constexpr iterator insert_unspecified_value(const_iterator pos, size_type count) {
		const difference_type offset = pos - cbegin();
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		if (offset == 0) {
			while (count-- > 0) {
				push_front_unspecified_value();
			}
			return begin();
		}
		for (size_type i = 0; i < count; ++i) {
			push_back_unspecified_value();
		}
		const iterator it = begin() + offset;
		const iterator itEnd = end();
		std::rotate(it, itEnd - static_cast<difference_type>(count), itEnd);
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

	constexpr reference push_back_and_overwrite(auto overwrite) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		GREM_ASSERT(size() < capacity());
		T& storage = ringBuffer[endIndex & static_cast<Size>(Capacity - 1)];
		overwrite(storage);
		++endIndex;
		return storage;
	}

	constexpr reference push_back_unspecified_value() {
		return push_back_and_overwrite([](T&) -> void {});
	}

	constexpr void push_back(const T& value) {
		push_back_and_overwrite([&](T& storage) -> void { storage = value; });
	}

	constexpr void push_back(T&& value) {
		push_back_and_overwrite([&](T& storage) -> void { storage = std::move(value); });
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--endIndex;
	}

	constexpr reference push_front_and_overwrite(auto overwrite) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		GREM_ASSERT(size() < capacity());
		T& storage = ringBuffer[(beginIndex - 1) & static_cast<Size>(Capacity - 1)];
		overwrite(storage);
		--beginIndex;
		return storage;
	}

	constexpr reference push_front_unspecified_value() {
		return push_front_and_overwrite([](T&) -> void {});
	}

	constexpr void push_front(const T& value) {
		push_front_and_overwrite([&](T& storage) -> void { storage = value; });
	}

	constexpr void push_front(T&& value) {
		push_front_and_overwrite([&](T& storage) -> void { storage = std::move(value); });
	}

	constexpr void pop_front() {
		GREM_ASSERT(!empty());
		++beginIndex;
	}

	constexpr void resize(size_type count) {
		if (count < size()) {
			for (size_type i = size() - count; i-- > 0;) {
				pop_back();
			}
		} else {
			for (size_type i = count - size(); i-- > 0;) {
				push_back_unspecified_value();
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

	constexpr void resize_and_overwrite_added_values(size_type count, auto overwrite) {
		if (count < size()) {
			for (size_type i = size() - count; i-- > 0;) {
				pop_back();
			}
		} else {
			for (size_type i = count - size(); i-- > 0;) {
				push_back_and_overwrite(overwrite);
			}
		}
	}

	constexpr void swap(InplaceRingBuffer& other) noexcept(std::is_nothrow_move_constructible_v<T>) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		InplaceRingBuffer temporary = std::move(*this);
		*this = std::move(other);
		other = std::move(temporary);
	}

	[[nodiscard]] constexpr bool operator==(const InplaceRingBuffer& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const InplaceRingBuffer& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const InplaceRingBuffer& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const InplaceRingBuffer& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const InplaceRingBuffer& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(InplaceRingBuffer& a, InplaceRingBuffer& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	template <typename U>
	friend constexpr size_type erase(InplaceRingBuffer& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(InplaceRingBuffer& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	Size beginIndex = 0;
	Size endIndex = 0;
	T ringBuffer[Capacity];
};

} // namespace grem

#endif
