// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ARRAY_LIST_HPP
#define GREM_CORE_DATA_ARRAY_LIST_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::max, std::rotate, std::shift_left, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::fill_n
#include <cstdint>          // std::uint32_t
#include <cstring>          // std::memcpy
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::reverse_iterator, std::input_iterator, std::sentinel_for, std::iterator_traits, std::random_access_iterator_tag, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address, std::uninitialized_..., std::destroy
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <new>              // std::launder
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v, std::is_constant_evaluated
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

template <typename T, typename Allocator = std::allocator<T>>
class ArrayList {
private:
	using Size = std::uint32_t; // If you need to be able to support more than 4 billion elements, use Buffer instead.

public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = typename std::allocator_traits<Allocator>::size_type;
	using difference_type = typename std::allocator_traits<Allocator>::difference_type;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = typename std::allocator_traits<Allocator>::pointer;
	using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr ArrayList() noexcept = default;

	constexpr explicit ArrayList(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	constexpr ArrayList(size_type count, const T& value, const Allocator& allocator = Allocator())
		: ArrayList(allocator) {
		resize(count, value);
	}

	constexpr explicit ArrayList(size_type count, const Allocator& allocator = Allocator())
		: ArrayList(allocator) {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr ArrayList(InputIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: ArrayList(allocator) {
		assign(first, last);
	}

	constexpr ArrayList(std::initializer_list<T> ilist, const Allocator& allocator = Allocator())
		: ArrayList(ilist.begin(), ilist.end(), allocator) {}

	constexpr ~ArrayList() {
		reset();
	}

	constexpr ArrayList(const ArrayList& other)
		: ArrayList(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr ArrayList(const ArrayList& other, const Allocator& allocator)
		: ArrayList(other.begin(), other.end(), allocator) {}

	constexpr ArrayList(ArrayList&& other) noexcept
		: ArrayList(std::move(other), other.get_allocator()) {}

	constexpr ArrayList(ArrayList&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: elements(std::exchange(other.elements, nullptr))
		, elementCount(std::exchange(other.elementCount, Size{0}))
		, elementCapacity(std::exchange(other.elementCapacity, Size{0}))
		, allocator(allocator) {}

	constexpr ArrayList& operator=(const ArrayList& other) {
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

	constexpr ArrayList& operator=(ArrayList&& other) noexcept(
		std::allocator_traits<
			Allocator>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<Allocator>::is_always_equal::value) {
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				clear();
				reserve(other.size());
				moveAppend(other.begin(), other.end());
				other.clear();
				return *this;
			}
		}
		reset();
		elements = std::exchange(other.elements, nullptr);
		elementCount = std::exchange(other.elementCount, Size{0});
		elementCapacity = std::exchange(other.elementCapacity, Size{0});
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	constexpr ArrayList& operator=(std::initializer_list<T> ilist) noexcept {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			while (size() > count) {
				pop_back();
			}
			reserve(count);
			std::fill_n(begin(), size(), value);
		} else {
			clear();
			reserve(count);
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
				while (size() > count) {
					pop_back();
				}
				reserve(count);
				for (size_type index = 0; index < size(); ++index) {
					elements[index] = *first++;
				}
			} else {
				clear();
				reserve(count);
			}
			copyAppend(first, last);
		} else {
			clear();
			copyAppend(first, last);
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
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < elementCount);
		return elements[pos];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < elementCount);
		return elements[pos];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[elementCount - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[elementCount - 1];
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
		return elements + elementCount;
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return elements + elementCount;
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
		return elementCount == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return static_cast<size_type>(elementCount);
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return std::min({
			static_cast<size_type>(std::numeric_limits<Size>::max()),
			static_cast<size_type>(std::numeric_limits<difference_type>::max()),
			static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(T)),
			static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator)),
		});
	}

	constexpr void reserve(size_type newCapacity) {
		if (newCapacity > capacity()) {
			if (newCapacity > max_size()) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				*this = ArrayList(MoveTag{}, begin(), end(), newCapacity, allocator);
			} else {
				*this = ArrayList(cbegin(), cend(), newCapacity, allocator);
			}
		}
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return static_cast<size_type>(elementCapacity);
	}

	constexpr void shrink_to_fit() {
		if (size() < capacity()) {
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				*this = ArrayList(MoveTag{}, begin(), end(), size(), allocator);
			} else {
				*this = ArrayList(cbegin(), cend(), size(), allocator);
			}
		}
	}

	constexpr void clear() noexcept {
		if (elements) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			if constexpr (std::is_trivially_destructible_v<T>) {
				std::destroy(pBegin, pEnd);
			} else {
				for (T* p = pBegin; p != pEnd; ++p) {
					std::allocator_traits<Allocator>::destroy(allocator, p);
				}
			}
			elementCount = 0;
		}
	}

	constexpr iterator insert(const_iterator pos, const T& value) {
		return emplace(pos, value);
	}

	constexpr iterator insert(const_iterator pos, T&& value) {
		return emplace(pos, std::move(value));
	}

	constexpr iterator insert(const_iterator pos, size_type count, const T& value) {
		const difference_type offset = pos - cbegin();
		if (count > max_size() - size()) {
			throw std::length_error{"count > max_size() - size()"};
		}
		reserveAtLeast(size() + count);
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
		const iterator it = begin() + offset;
		const size_type count = size() - static_cast<size_type>(offset);
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

	template <typename... Args>
	constexpr iterator emplace(const_iterator pos, Args&&... args) {
		const difference_type offset = pos - cbegin();
		emplace_back(std::forward<Args>(args)...);
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	constexpr iterator erase(const_iterator pos) {
		GREM_ASSERT(!empty());
		const difference_type offset = pos - cbegin();
		const iterator it = begin() + offset;
		std::shift_left(it, end(), 1);
		pop_back();
		return it;
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		const size_type count = static_cast<size_type>(last - first);
		const difference_type offset = first - cbegin();
		const iterator it = begin() + offset;
		if (count > 0) {
			GREM_ASSERT(count <= size());
			const iterator newEnd = std::shift_left(it, end(), static_cast<difference_type>(count));
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			T* const pNewEnd = pBegin + (newEnd - begin());
			if constexpr (std::is_trivially_destructible_v<T>) {
				std::destroy(pNewEnd, pEnd);
			} else {
				for (T* p = pNewEnd; p != pEnd; ++p) {
					std::allocator_traits<Allocator>::destroy(allocator, p);
				}
			}
			elementCount -= static_cast<Size>(count);
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
		reserveAtLeast(size() + 1);
		T* const p = std::to_address(elements) + elementCount;
		std::allocator_traits<Allocator>::construct(allocator, p, std::forward<Args>(args)...);
		++elementCount;
		return *std::launder(p);
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--elementCount;
		std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + elementCount);
	}

	constexpr void resize(size_type count) {
		if (count < size()) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			T* const pNewEnd = pBegin + count;
			if constexpr (std::is_trivially_destructible_v<T>) {
				std::destroy(pNewEnd, pEnd);
			} else {
				for (T* p = pNewEnd; p != pEnd; ++p) {
					std::allocator_traits<Allocator>::destroy(allocator, p);
				}
			}
			elementCount = static_cast<Size>(count);
		} else if (count > size()) {
			reserveAtLeast(count);
			if constexpr (std::is_trivially_default_constructible_v<T>) {
				std::uninitialized_default_construct(std::to_address(elements) + elementCount, std::to_address(elements) + count);
				elementCount = static_cast<Size>(count);
			} else {
				while (elementCount < count) {
					std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + elementCount);
					++elementCount;
				}
			}
		}
	}

	constexpr void resize(size_type count, const value_type& value) {
		if (count < size()) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			T* const pNewEnd = pBegin + count;
			if constexpr (std::is_trivially_destructible_v<T>) {
				std::destroy(pNewEnd, pEnd);
			} else {
				for (T* p = pNewEnd; p != pEnd; ++p) {
					std::allocator_traits<Allocator>::destroy(allocator, p);
				}
			}
			elementCount = static_cast<Size>(count);
		} else if (count > size()) {
			reserveAtLeast(count);
			if constexpr (std::is_trivially_copy_constructible_v<T>) {
				std::uninitialized_fill(std::to_address(elements) + elementCount, std::to_address(elements) + count, value);
				elementCount = static_cast<Size>(count);
			} else {
				while (elementCount < count) {
					std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + elementCount, value);
					++elementCount;
				}
			}
		}
	}

	constexpr void swap(ArrayList& other) noexcept(
		std::allocator_traits<Allocator>::propagate_on_container_swap::value || // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		(std::allocator_traits<Allocator>::is_always_equal::value && std::is_nothrow_swappable_v<T>)) {
		if (this == &other) {
			return;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_swap::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				throw std::logic_error{"get_allocator() != other.get_allocator()"};
			}
		}
		using std::swap;
		swap(elements, other.elements);
		swap(elementCount, other.elementCount);
		swap(elementCapacity, other.elementCapacity);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		}
	}

	[[nodiscard]] constexpr bool operator==(const ArrayList& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const ArrayList& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const ArrayList& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const ArrayList& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const ArrayList& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(ArrayList& a, ArrayList& b) noexcept {
		a.swap(b);
	}

	template <typename U>
	friend constexpr size_type erase(ArrayList& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(ArrayList& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	struct MoveTag {};

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr ArrayList(MoveTag, InputIterator first, Sentinel last, size_type capacity, const Allocator& allocator)
		: ArrayList(allocator) {
		elements = std::allocator_traits<Allocator>::allocate(this->allocator, capacity); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		elementCapacity = static_cast<Size>(capacity);                                    // NOLINT(cppcoreguidelines-prefer-member-initializer)
		moveAppend(first, last);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr ArrayList(InputIterator first, Sentinel last, size_type capacity, const Allocator& allocator)
		: ArrayList(allocator) {
		elements = std::allocator_traits<Allocator>::allocate(this->allocator, capacity); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		elementCapacity = static_cast<Size>(capacity);                                    // NOLINT(cppcoreguidelines-prefer-member-initializer)
		copyAppend(first, last);
	}

	constexpr void reset() noexcept {
		if (elements) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			if constexpr (std::is_trivially_destructible_v<T>) {
				std::destroy(pBegin, pEnd);
			} else {
				for (T* p = pBegin; p != pEnd; ++p) {
					std::allocator_traits<Allocator>::destroy(allocator, p);
				}
			}
			std::allocator_traits<Allocator>::deallocate(allocator, elements, static_cast<size_type>(elementCapacity));
			elements = nullptr;
			elementCount = 0;
			elementCapacity = 0;
		}
	}

	constexpr void reserveAtLeast(size_type newCapacity) {
		if (const size_type cap = capacity(); newCapacity > cap) {
			const size_type maxSize = max_size();
			const size_type halfCap = cap / 2;
			if (newCapacity > maxSize || halfCap > maxSize - cap) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			newCapacity = std::max(newCapacity, cap + halfCap);
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				*this = ArrayList(MoveTag{}, begin(), end(), newCapacity, allocator);
			} else {
				*this = ArrayList(cbegin(), cend(), newCapacity, allocator);
			}
		}
	}

	template <typename InputIterator, typename Sentinel>
	constexpr void copyAppend(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if (count == 0) {
				return;
			}
			const size_type offset = size();
			if (count > max_size() - offset) {
				throw std::length_error{"count > max_size() - size()"};
			}
			const size_type newSize = offset + count;
			reserveAtLeast(newSize);
			if constexpr (std::is_trivially_copyable_v<T> && requires { std::to_address(first); }) {
				if (!std::is_constant_evaluated()) {
					std::memcpy(std::to_address(elements) + offset, std::to_address(first), count * sizeof(T));
					elementCount = static_cast<Size>(newSize);
					return;
				}
			}
		}
		while (first != last) {
			push_back(*first++);
		}
	}

	template <typename InputIterator, typename Sentinel>
	constexpr void moveAppend(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if (count == 0) {
				return;
			}
			const size_type offset = size();
			if (count > max_size() - offset) {
				throw std::length_error{"count > max_size() - size()"};
			}
			const size_type newSize = offset + count;
			reserveAtLeast(newSize);
			if constexpr (std::is_trivially_copyable_v<T> && requires { std::to_address(first); }) {
				if (!std::is_constant_evaluated()) {
					std::memcpy(std::to_address(elements) + offset, std::to_address(first), count * sizeof(T));
					elementCount = static_cast<Size>(newSize);
					return;
				}
			}
		}
		while (first != last) {
			push_back(std::move(*first++));
		}
	}

	pointer elements = nullptr;
	Size elementCount = 0;
	Size elementCapacity = 0;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename T>
using ArrayList = grem::ArrayList<T, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
