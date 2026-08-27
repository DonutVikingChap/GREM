// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_INPLACE_BUFFER_HPP
#define GREM_CORE_DATA_INPLACE_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::rotate, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::move, std::copy, std::fill_n
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <cstring>          // std::memcpy
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::reverse_iterator, std::input_iterator, std::sentinel_for, std::iterator_traits, std::random_access_iterator_tag, std::begin, std::end
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v, std::conditional_t
#include <utility>          // std::move, std::exchange, std::forward

namespace grem {

template <typename T, std::size_t N>
class InplaceBuffer {
private:
	// clang-format off
    using Size =
        std::conditional_t<N <= 255ull, std::uint8_t,
        std::conditional_t<N <= 65535ull, std::uint16_t,
        std::conditional_t<N <= 4294967295ull, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr InplaceBuffer() noexcept = default;

	constexpr InplaceBuffer(size_type count, const T& value) {
		resize(count, value);
	}

	constexpr explicit InplaceBuffer(size_type count) {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr InplaceBuffer(InputIterator first, Sentinel last) {
		assign(first, last);
	}

	constexpr InplaceBuffer(std::initializer_list<T> ilist)
		: InplaceBuffer(ilist.begin(), ilist.end()) {}

	constexpr ~InplaceBuffer() = default;

	constexpr ~InplaceBuffer() requires(!std::is_trivially_destructible_v<T>) {
		clear();
	}

	constexpr InplaceBuffer(const InplaceBuffer&) = default;

	constexpr InplaceBuffer(const InplaceBuffer& other) requires(!std::is_trivially_copyable_v<T>)
		: InplaceBuffer(other.begin(), other.end()) {}

	constexpr InplaceBuffer(InplaceBuffer&&) noexcept = default;

	constexpr InplaceBuffer(InplaceBuffer&& other) noexcept(
		std::is_nothrow_move_constructible_v<T>) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		requires(!std::is_trivially_copyable_v<T>) {
		*this = std::move(other);
	}

	constexpr InplaceBuffer& operator=(const InplaceBuffer&) = default;

	constexpr InplaceBuffer& operator=(const InplaceBuffer& other) requires(!std::is_trivially_copyable_v<T>) {
		if (this == &other) {
			return *this;
		}
		assign(other.begin(), other.end());
		return *this;
	}

	constexpr InplaceBuffer& operator=(InplaceBuffer&&) noexcept = default;

	constexpr InplaceBuffer& operator=(InplaceBuffer&& other) noexcept(
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
		std::move(other.begin(), other.end(), elements);
		elementCount = std::exchange(other.elementCount, Size{0});
		return *this;
	}

	constexpr InplaceBuffer& operator=(std::initializer_list<T> ilist) {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count < size()) {
				elementCount = static_cast<Size>(count);
			}
			std::fill_n(begin(), size(), value);
		} else {
			clear();
		}
		for (size_type index = size(); index < count; ++index) {
			push_back(value);
		}
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr void assign(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if constexpr (std::is_copy_assignable_v<T>) {
				if (count < size()) {
					elementCount = static_cast<Size>(count);
				}
				for (size_type index = 0; index < size(); ++index) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
					elements[index] = *first++;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
				}
			} else {
				clear();
			}
			insert(end(), first, last);
		} else {
			clear();
			insert(end(), first, last);
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
		return elements[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < size());
		return elements[pos];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < size());
		return elements[pos];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return elements[size() - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return elements[size() - 1];
	}

	[[nodiscard]] constexpr pointer data() noexcept {
		return elements;
	}

	[[nodiscard]] constexpr const_pointer data() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return elements;
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return elements + size();
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return elements + size();
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
		return const_reverse_iterator{cend()};
	}

	[[nodiscard]] constexpr reverse_iterator rend() noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
		return const_reverse_iterator{cbegin()};
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return static_cast<size_type>(elementCount);
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return N;
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return N;
	}

	constexpr void clear() noexcept {
		elementCount = 0;
	}

	constexpr iterator insert(const_iterator pos, const T& value) {
		const difference_type offset = pos - cbegin();
		push_back(value);
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	constexpr iterator insert(const_iterator pos, T&& value) {
		const difference_type offset = pos - cbegin();
		push_back(std::move(value));
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	constexpr iterator insert(const_iterator pos, size_type count, const T& value) {
		const difference_type offset = pos - cbegin();
		if (count > max_size() - size()) {
			throw std::length_error{"count > max_size() - size()"};
		}
		for (size_type i = 0; i < count; ++i) {
			push_back(value);
		}
		const iterator it = begin() + offset;
		std::rotate(it, end() - static_cast<difference_type>(count), end());
		return it;
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr iterator insert(const_iterator pos, InputIterator first, Sentinel last) {
		const difference_type offset = pos - cbegin();
		copyAppend(first, last);
		const size_type count = size() - static_cast<size_type>(offset);
		const iterator it = begin() + offset;
		std::rotate(it, end() - static_cast<difference_type>(count), end());
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
		push_back_unspecified_value();
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	constexpr iterator insert_unspecified_value(const_iterator pos, size_type count) {
		const difference_type offset = pos - cbegin();
		if (count > max_size() - size()) {
			throw std::length_error{"count > max_size() - size()"};
		}
		elementCount = static_cast<Size>(size() + count);
		const iterator it = begin() + offset;
		std::rotate(it, end() - static_cast<difference_type>(count), end());
		return it;
	}

	constexpr iterator erase(const_iterator pos) {
		GREM_ASSERT(!empty());
		const difference_type offset = pos - cbegin();
		const iterator it = begin() + offset;
		std::rotate(it, it + 1, end());
		--elementCount;
		return it;
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		const difference_type offset = first - cbegin();
		const difference_type count = last - first;
		GREM_ASSERT(offset <= static_cast<difference_type>(size()));
		GREM_ASSERT(count >= 0);
		GREM_ASSERT(offset + count <= static_cast<difference_type>(size()));
		const iterator it = begin() + offset;
		std::rotate(it, it + count, end());
		elementCount -= count;
		return it;
	}

	constexpr reference push_back_and_overwrite(auto overwrite) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		T& result = elements[elementCount];
		overwrite(result);
		++elementCount;
		return result;
	}

	constexpr reference push_back_unspecified_value() {
		return push_back_and_overwrite([](T&) -> void {});
	}

	constexpr void push_back(const T& value) {
		push_back_and_overwrite([&](T& storage) { storage = value; });
	}

	constexpr void push_back(T&& value) {
		push_back_and_overwrite([&](T& storage) { storage = std::move(value); });
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--elementCount;
	}

	constexpr void resize(size_type count) {
		if (count > max_size()) {
			throw std::length_error{"count > max_size()"};
		}
		elementCount = static_cast<Size>(count);
	}

	constexpr void resize(size_type count, const value_type& value) {
		if (count > size()) {
			if (count > max_size()) {
				throw std::length_error{"count > max_size()"};
			}
			std::fill_n(elements + size(), count - size(), value);
		}
		elementCount = static_cast<Size>(count);
	}

	constexpr void resize_and_overwrite_added_values(size_type count, auto overwrite) {
		if (count > size()) {
			if (count > max_size()) {
				throw std::length_error{"count > max_size()"};
			}
			for (Size i = elementCount; i < count; ++i) {
				overwrite(elements[i]);
			}
		}
		elementCount = static_cast<Size>(count);
	}

	constexpr void swap(InplaceBuffer& other) noexcept(std::is_nothrow_move_constructible_v<T>) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		InplaceBuffer temporary = std::move(*this);
		*this = std::move(other);
		other = std::move(temporary);
	}

	[[nodiscard]] constexpr bool operator==(const InplaceBuffer& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const InplaceBuffer& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const InplaceBuffer& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const InplaceBuffer& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const InplaceBuffer& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(InplaceBuffer& a, InplaceBuffer& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	template <typename U>
	friend constexpr size_type erase(InplaceBuffer& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(InplaceBuffer& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	template <typename InputIterator, typename Sentinel>
	void copyAppend(InputIterator first, Sentinel last) {
		if constexpr (
			std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag> && std::is_nothrow_copy_assignable_v<T>) {
			const size_type count = static_cast<size_type>(last - first);
			if (count == 0) {
				return;
			}
			const size_type offset = size();
			if (count > max_size() - offset) {
				throw std::length_error{"count > max_size() - size()"};
			}
			const size_type newSize = offset + count;
			if constexpr (std::is_trivially_copyable_v<T> && requires { std::to_address(first); }) {
				std::memcpy(data() + offset, std::to_address(first), count * sizeof(T));
			} else {
				std::copy(first, last, data() + offset);
			}
			elementCount = static_cast<Size>(newSize);
		} else {
			while (first != last) {
				push_back(*first++);
			}
		}
	}

	Size elementCount = 0;
	T elements[N];
};

} // namespace grem

#endif
