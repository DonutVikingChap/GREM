// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SMALL_ARRAY_LIST_HPP
#define GREM_CORE_DATA_SMALL_ARRAY_LIST_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::max, std::rotate, std::shift_left, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::fill_n
#include <cstddef>          // std::size_t
#include <cstdint>          // std::uint32_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::..._iterator, std::sentinel_for, std::iterator_traits, std::..._iterator_tag, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v
#include <utility>          // std::move, std::move_if_noexcept, std::forward, std::exchange, std::swap

namespace grem {

template <typename T, std::size_t SmallCapacity, typename Allocator = std::allocator<T>>
class SmallArrayList {
private:
	using Size = std::uint32_t; // If you need more than 4 billion elements, use SmallBuffer instead.

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

	SmallArrayList() noexcept(noexcept(Allocator()))
		: SmallArrayList(Allocator()) {}

	explicit SmallArrayList(const Allocator& allocator) noexcept
		: allocator(allocator) {
		elements = reinterpret_cast<T*>(smallBuffer);
	}

	SmallArrayList(size_type count, const T& value, const Allocator& allocator = Allocator())
		: SmallArrayList(allocator) {
		resize(count, value);
	}

	explicit SmallArrayList(size_type count, const Allocator& allocator = Allocator())
		: SmallArrayList(allocator) {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	SmallArrayList(InputIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: SmallArrayList(allocator) {
		assign(first, last);
	}

	SmallArrayList(std::initializer_list<T> ilist, const Allocator& allocator = Allocator())
		: SmallArrayList(ilist.begin(), ilist.end(), allocator) {}

	~SmallArrayList() {
		reset();
	}

	SmallArrayList(const SmallArrayList& other)
		: SmallArrayList(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	SmallArrayList(const SmallArrayList& other, const Allocator& allocator)
		: SmallArrayList(other.begin(), other.end(), allocator) {}

	SmallArrayList(SmallArrayList&& other)                //
		noexcept(std::is_nothrow_move_constructible_v<T>) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		: SmallArrayList(std::move(other), other.get_allocator()) {}

	SmallArrayList(SmallArrayList&& other, const Allocator& allocator) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: allocator(allocator) {
		if (other.capacity() <= SmallCapacity) {
			T* const otherElements = std::launder(reinterpret_cast<T*>(other.smallBuffer));
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				while (elementCount < other.elementCount) {
					std::allocator_traits<Allocator>::construct(this->allocator, reinterpret_cast<T*>(smallBuffer) + elementCount, std::move(otherElements[elementCount]));
					++elementCount;
				}
			} else {
				try {
					while (elementCount < other.elementCount) {
						std::allocator_traits<Allocator>::construct(this->allocator, reinterpret_cast<T*>(smallBuffer) + elementCount, otherElements[elementCount]);
						++elementCount;
					}
				} catch (...) {
					while (elementCount-- > 0) {
						std::allocator_traits<Allocator>::destroy(this->allocator, reinterpret_cast<T*>(smallBuffer) + elementCount);
					}
					throw;
				}
			}
			for (Size i = 0; i < other.elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(this->allocator, otherElements + i);
			}
			other.elementCount = 0;
			elements = std::launder(reinterpret_cast<T*>(smallBuffer));
			elementCapacity = other.elementCapacity;
		} else {
			elements = std::exchange(other.elements, std::launder(reinterpret_cast<T*>(other.smallBuffer)));
			elementCount = std::exchange(other.elementCount, Size{0});
			elementCapacity = std::exchange(other.elementCapacity, Size{SmallCapacity});
		}
	}

	SmallArrayList& operator=(const SmallArrayList& other) {
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

	SmallArrayList& operator=(SmallArrayList&& other) noexcept(
		std::is_nothrow_move_constructible_v<T> && // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value)) {
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				clear();
				reserveAtLeast(other.size());
				for (T& element : other) {
					push_back(std::move_if_noexcept(element));
				}
				other.clear();
				return *this;
			}
		}
		reset();
		if (other.capacity() <= SmallCapacity) {
			T* const otherElements = std::launder(reinterpret_cast<T*>(other.smallBuffer));
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				while (elementCount < other.elementCount) {
					std::allocator_traits<Allocator>::construct(allocator, reinterpret_cast<T*>(smallBuffer) + elementCount, std::move(otherElements[elementCount]));
					++elementCount;
				}
			} else {
				try {
					while (elementCount < other.elementCount) {
						std::allocator_traits<Allocator>::construct(allocator, reinterpret_cast<T*>(smallBuffer) + elementCount, otherElements[elementCount]);
						++elementCount;
					}
				} catch (...) {
					while (elementCount-- > 0) {
						std::allocator_traits<Allocator>::destroy(allocator, reinterpret_cast<T*>(smallBuffer) + elementCount);
					}
					throw;
				}
			}
			for (Size i = 0; i < other.elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(allocator, otherElements + i);
			}
			other.elementCount = 0;
			elements = std::launder(reinterpret_cast<T*>(smallBuffer));
			elementCapacity = other.elementCapacity;
		} else {
			elements = std::exchange(other.elements, std::launder(reinterpret_cast<T*>(other.smallBuffer)));
			elementCount = std::exchange(other.elementCount, Size{0});
			elementCapacity = std::exchange(other.elementCapacity, Size{SmallCapacity});
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	SmallArrayList& operator=(std::initializer_list<T> ilist) {
		assign(ilist);
		return *this;
	}

	void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			while (size() > count) {
				pop_back();
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
	void assign(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if constexpr (std::is_copy_assignable_v<T>) {
				while (size() > count) {
					pop_back();
				}
				for (size_type index = 0; index < size(); ++index) {
					(*this)[index] = *first++;
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

	void assign(std::initializer_list<T> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename R>
	void assign_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		assign(std::begin(r), std::end(r));
	}

	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] reference at(size_type pos) {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] reference operator[](size_type pos) {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < size());
		return elements[pos];
	}

	[[nodiscard]] const_reference operator[](size_type pos) const {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < size());
		return elements[pos];
	}

	[[nodiscard]] reference front() {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] const_reference front() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[0];
	}

	[[nodiscard]] reference back() {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[elementCount - 1];
	}

	[[nodiscard]] const_reference back() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(!empty());
		return elements[elementCount - 1];
	}

	[[nodiscard]] pointer data() noexcept {
		return elements;
	}

	[[nodiscard]] const_pointer data() const noexcept {
		return elements;
	}

	[[nodiscard]] iterator begin() noexcept {
		return elements;
	}

	[[nodiscard]] const_iterator begin() const noexcept {
		return elements;
	}

	[[nodiscard]] const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] iterator end() noexcept {
		return elements + elementCount;
	}

	[[nodiscard]] const_iterator end() const noexcept {
		return elements + elementCount;
	}

	[[nodiscard]] const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] reverse_iterator rbegin() noexcept {
		return reverse_iterator{end()};
	}

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator{end()};
	}

	[[nodiscard]] const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	[[nodiscard]] reverse_iterator rend() noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] const_reverse_iterator crend() const noexcept {
		return rend();
	}

	[[nodiscard]] bool empty() const noexcept {
		return elementCount == 0;
	}

	[[nodiscard]] size_type size() const noexcept {
		return static_cast<size_type>(elementCount);
	}

	[[nodiscard]] size_type max_size() const noexcept {
		return std::min({
			static_cast<size_type>(std::numeric_limits<difference_type>::max()),
			static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(T)),
			static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator)),
		});
	}

	void reserve(size_type newCapacity) {
		if (newCapacity > capacity()) {
			if (newCapacity > max_size()) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, newCapacity);
			Size newElementCount = 0;
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				while (newElementCount < elementCount) {
					std::allocator_traits<Allocator>::construct(allocator, std::to_address(newElements) + newElementCount, std::move(elements[newElementCount]));
					++newElementCount;
				}
			} else {
				try {
					while (newElementCount < elementCount) {
						std::allocator_traits<Allocator>::construct(allocator, std::to_address(newElements) + newElementCount, elements[newElementCount]);
						++newElementCount;
					}
				} catch (...) {
					while (newElementCount-- > 0) {
						std::allocator_traits<Allocator>::destroy(allocator, std::to_address(newElements) + newElementCount);
					}
					std::allocator_traits<Allocator>::deallocate(allocator, newElements, newCapacity);
					throw;
				}
			}
			for (Size i = 0; i < elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + i);
			}
			if (capacity() > SmallCapacity) {
				std::allocator_traits<Allocator>::deallocate(allocator, elements, capacity());
			}
			elements = newElements;
			elementCapacity = static_cast<Size>(newCapacity);
		}
	}

	[[nodiscard]] size_type capacity() const noexcept {
		return static_cast<size_type>(elementCapacity);
	}

	void shrink_to_fit() {
		if (size() < capacity()) {
			*this = SmallArrayList{std::make_move_iterator(begin()), std::make_move_iterator(end()), get_allocator()};
		}
	}

	void clear() noexcept {
		if (elements) {
			for (Size i = 0; i < elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + i);
			}
			elementCount = 0;
		}
	}

	iterator insert(const_iterator pos, const T& value) {
		return emplace(pos, value);
	}

	iterator insert(const_iterator pos, T&& value) {
		return emplace(pos, std::move(value));
	}

	iterator insert(const_iterator pos, size_type count, const T& value) {
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
	iterator insert(const_iterator pos, InputIterator first, Sentinel last) {
		const difference_type offset = pos - cbegin();
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if (count > max_size() - size()) {
				throw std::length_error{"count > max_size() - size()"};
			}
			reserveAtLeast(size() + static_cast<size_type>(last - first));
		}
		while (first != last) {
			push_back(*first++);
		}
		const iterator it = begin() + offset;
		const size_type count = size() - static_cast<size_type>(offset);
		std::rotate(it, end() - static_cast<difference_type>(count), end());
		return it;
	}

	iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
		return insert(pos, ilist.begin(), ilist.end());
	}

	template <typename R>
	iterator insert_range(const_iterator pos, R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		return insert(pos, std::begin(r), std::end(r));
	}

	template <typename R>
	void append_range(R&& r) {
		insert_range(end(), std::forward<R>(r));
	}

	template <typename... Args>
	iterator emplace(const_iterator pos, Args&&... args) {
		const difference_type offset = pos - cbegin();
		emplace_back(std::forward<Args>(args)...);
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	iterator erase(const_iterator pos) {
		GREM_ASSERT(!empty());
		const difference_type offset = pos - cbegin();
		const iterator it = begin() + offset;
		std::shift_left(it, end(), 1);
		pop_back();
		return it;
	}

	iterator erase(const_iterator first, const_iterator last) {
		const size_type count = static_cast<size_type>(last - first);
		const difference_type offset = first - cbegin();
		const iterator it = begin() + offset;
		if (count > 0) {
			GREM_ASSERT(count <= size());
			const iterator newEnd = std::shift_left(it, end(), static_cast<difference_type>(count));
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			for (T* p = pBegin + (newEnd - begin()); p != pEnd; ++p) {
				std::allocator_traits<Allocator>::destroy(allocator, p);
			}
			elementCount -= static_cast<Size>(count);
		}
		return it;
	}

	void push_back(const T& value) {
		emplace_back(value);
	}

	void push_back(T&& value) {
		emplace_back(std::move(value));
	}

	template <typename... Args>
	reference emplace_back(Args&&... args) {
		reserveAtLeast(size() + 1);
		T* const p = std::to_address(elements) + elementCount;
		std::allocator_traits<Allocator>::construct(allocator, p, std::forward<Args>(args)...);
		++elementCount;
		return *std::launder(p);
	}

	void pop_back() {
		GREM_ASSERT(!empty());
		--elementCount;
		std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + elementCount);
	}

	void resize(size_type count) {
		if (count < size()) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			for (T* p = pBegin + count; p != pEnd; ++p) {
				std::allocator_traits<Allocator>::destroy(allocator, p);
			}
			elementCount = static_cast<Size>(count);
		} else if (count > size()) {
			reserveAtLeast(count);
			while (elementCount < count) {
				std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + elementCount);
				++elementCount;
			}
		}
	}

	void resize(size_type count, const value_type& value) {
		if (count < size()) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCount;
			for (T* p = pBegin + count; p != pEnd; ++p) {
				std::allocator_traits<Allocator>::destroy(allocator, p);
			}
			elementCount = static_cast<Size>(count);
		} else if (count > size()) {
			reserveAtLeast(count);
			while (elementCount < count) {
				std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + elementCount, value);
				++elementCount;
			}
		}
	}

	void swap(SmallArrayList& other) noexcept(
		std::allocator_traits<Allocator>::propagate_on_container_swap::value || // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		(std::allocator_traits<Allocator>::is_always_equal::value && std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>)) {
		if (this == &other) {
			return;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_swap::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				throw std::logic_error{"get_allocator() != other.get_allocator()"};
			}
		}
		if (capacity() <= SmallCapacity || other.capacity() <= SmallCapacity) {
			SmallArrayList temporary = std::move(*this);
			*this = std::move(other);
			other = std::move(temporary);
		} else {
			using std::swap;
			swap(elements, other.elements);
			swap(elementCount, other.elementCount);
			swap(elementCapacity, other.elementCapacity);
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			using std::swap;
			swap(allocator, other.allocator);
		}
	}

	[[nodiscard]] bool operator==(const SmallArrayList& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<(const SmallArrayList& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<=(const SmallArrayList& other) const {
		return !(other < *this);
	}

	[[nodiscard]] bool operator>(const SmallArrayList& other) const {
		return other < *this;
	}

	[[nodiscard]] bool operator>=(const SmallArrayList& other) const {
		return !(*this < other);
	}

	friend void swap(SmallArrayList& a, SmallArrayList& b) noexcept {
		a.swap(b);
	}

	template <typename U>
	friend size_type erase(SmallArrayList& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend size_type erase_if(SmallArrayList& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	void reset() noexcept {
		T* const pBegin = std::to_address(elements);
		T* const pEnd = pBegin + elementCount;
		for (T* p = pBegin; p != pEnd; ++p) {
			std::allocator_traits<Allocator>::destroy(allocator, p);
		}
		if (capacity() > SmallCapacity) {
			std::allocator_traits<Allocator>::deallocate(allocator, elements, capacity());
		}
		elements = reinterpret_cast<T*>(smallBuffer);
		elementCount = 0;
		elementCapacity = SmallCapacity;
	}

	void reserveAtLeast(size_type newCapacity) {
		if (const size_type cap = capacity(); newCapacity > cap) {
			GREM_ASSERT(newCapacity > SmallCapacity);
			const size_type maxSize = max_size();
			const size_type halfCap = cap / 2;
			if (newCapacity > maxSize || halfCap > maxSize - cap) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			newCapacity = std::max(newCapacity, cap + halfCap);
			const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, newCapacity);
			Size newElementCount = 0;
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				while (newElementCount < elementCount) {
					std::allocator_traits<Allocator>::construct(allocator, std::to_address(newElements) + newElementCount, std::move(elements[newElementCount]));
					++newElementCount;
				}
			} else {
				try {
					while (newElementCount < elementCount) {
						std::allocator_traits<Allocator>::construct(allocator, std::to_address(newElements) + newElementCount, elements[newElementCount]);
						++newElementCount;
					}
				} catch (...) {
					while (newElementCount-- > 0) {
						std::allocator_traits<Allocator>::destroy(allocator, std::to_address(newElements) + newElementCount);
					}
					std::allocator_traits<Allocator>::deallocate(allocator, newElements, newCapacity);
					throw;
				}
			}
			for (Size i = 0; i < elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + i);
			}
			if (capacity() > SmallCapacity) {
				std::allocator_traits<Allocator>::deallocate(allocator, elements, capacity());
			}
			elements = newElements;
			elementCapacity = static_cast<Size>(newCapacity);
		}
	}

	pointer elements = nullptr;
	Size elementCount = 0;
	Size elementCapacity = SmallCapacity;
	alignas(T) std::byte smallBuffer[SmallCapacity * sizeof(T)];
	[[no_unique_address]] allocator_type allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename T, std::size_t SmallCapacity>
using SmallArrayList = grem::SmallArrayList<T, SmallCapacity, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
