// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_BUFFER_HPP
#define GREM_CORE_DATA_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::max, std::rotate, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::copy, std::move, std::fill_n
#include <cstddef>          // std::max_align_t
#include <cstring>          // std::memcpy
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::reverse_iterator, std::input_iterator, std::sentinel_for, std::iterator_traits, std::random_access_iterator_tag, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address, std::uninitialized_...
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <new>              // std::launder, std::bad_alloc, std::bad_array_new_length
#include <stdexcept>        // std::out_of_range, std::length_error
#include <stdlib.h>         // malloc, realloc, free // NOLINT(modernize-deprecated-headers)
#include <type_traits>      // std::is_..._v
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

template <typename T, typename Allocator = std::allocator<T>>
class Buffer {
private:
	static constexpr bool USE_MALLOC_AND_REALLOC = std::is_same_v<Allocator, std::allocator<T>> && std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T> &&
	                                               std::is_nothrow_default_constructible_v<T> && alignof(T) <= alignof(std::max_align_t);

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

	Buffer() noexcept = default;

	explicit Buffer(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	Buffer(size_type count, const T& value, const Allocator& allocator = Allocator())
		: Buffer(allocator) {
		resize(count, value);
	}

	explicit Buffer(size_type count, const Allocator& allocator = Allocator())
		: Buffer(allocator) {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	Buffer(InputIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: Buffer(allocator) {
		assign(first, last);
	}

	Buffer(std::initializer_list<T> ilist, const Allocator& allocator = Allocator())
		: Buffer(ilist.begin(), ilist.end(), allocator) {}

	~Buffer() {
		reset();
	}

	Buffer(const Buffer& other)
		: Buffer(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	Buffer(const Buffer& other, const Allocator& allocator)
		: Buffer(other.begin(), other.end(), allocator) {}

	Buffer(Buffer&& other) noexcept
		: Buffer(std::move(other), other.get_allocator()) {}

	Buffer(Buffer&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: elements(std::exchange(other.elements, nullptr))
		, elementCount(std::exchange(other.elementCount, size_type{0}))
		, elementCapacity(std::exchange(other.elementCapacity, size_type{0}))
		, allocator(allocator) {}

	Buffer& operator=(const Buffer& other) {
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

	Buffer& operator=(Buffer&& other) noexcept(
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
		elementCount = std::exchange(other.elementCount, size_type{0});
		elementCapacity = std::exchange(other.elementCapacity, size_type{0});
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	Buffer& operator=(std::initializer_list<T> ilist) noexcept {
		assign(ilist);
		return *this;
	}

	void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count < elementCount) {
				elementCount = count;
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
	void assign(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			if constexpr (std::is_copy_assignable_v<T>) {
				if (count < elementCount) {
					elementCount = count;
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
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] const_reference at(size_type pos) const {
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return elements[pos];
	}

	[[nodiscard]] reference operator[](size_type pos) {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < elementCount);
		return elements[pos];
	}

	[[nodiscard]] const_reference operator[](size_type pos) const {
		GREM_ASSERT(elements);
		GREM_ASSERT(pos < elementCount);
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
		return elementCount;
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
			if constexpr (USE_MALLOC_AND_REALLOC) {
				if (newCapacity > std::numeric_limits<size_type>::max() / sizeof(T)) {
					throw std::bad_array_new_length{};
				}
				static_assert(std::is_trivially_copyable_v<T>);
				static_assert(std::is_trivially_destructible_v<T>);
				void* const newMemory = realloc(elements, newCapacity * sizeof(T)); // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
				if (!newMemory) {
					throw std::bad_alloc{};
				}
				elements = static_cast<pointer>(newMemory);
				static_assert(std::is_nothrow_default_constructible_v<T>);
				if constexpr (std::is_trivially_default_constructible_v<T>) {
					std::uninitialized_default_construct(std::to_address(elements) + elementCapacity, std::to_address(elements) + newCapacity);
					elementCapacity = newCapacity;
				} else {
					while (elementCapacity < newCapacity) {
						std::allocator_traits<Allocator>::construct(this->allocator, std::to_address(elements) + elementCapacity);
						++elementCapacity;
					}
				}
			} else {
				if constexpr (std::is_nothrow_move_constructible_v<T>) {
					*this = Buffer(MoveTag{}, begin(), end(), newCapacity, allocator);
				} else {
					*this = Buffer(cbegin(), cend(), newCapacity, allocator);
				}
			}
		}
	}

	[[nodiscard]] size_type capacity() const noexcept {
		return elementCapacity;
	}

	void shrink_to_fit() {
		if (size() < capacity()) {
			if constexpr (std::is_nothrow_move_constructible_v<T>) {
				*this = Buffer(MoveTag{}, begin(), end(), size(), allocator);
			} else {
				*this = Buffer(cbegin(), cend(), size(), allocator);
			}
		}
	}

	void clear() noexcept {
		elementCount = 0;
	}

	iterator insert(const_iterator pos, const T& value) {
		const difference_type offset = pos - cbegin();
		push_back(value);
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	iterator insert(const_iterator pos, T&& value) {
		const difference_type offset = pos - cbegin();
		push_back(std::move(value));
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
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
		copyAppend(first, last);
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

	iterator insert_unspecified_value(const_iterator pos) {
		const difference_type offset = pos - cbegin();
		push_back_unspecified_value();
		const iterator it = begin() + offset;
		std::rotate(it, end() - 1, end());
		return it;
	}

	iterator insert_unspecified_value(const_iterator pos, size_type count) {
		const difference_type offset = pos - cbegin();
		if (count > max_size() - size()) {
			throw std::length_error{"count > max_size() - size()"};
		}
		reserveAtLeast(size() + count);
		elementCount += count;
		const iterator it = begin() + offset;
		std::rotate(it, end() - static_cast<difference_type>(count), end());
		return it;
	}

	iterator erase(const_iterator pos) {
		GREM_ASSERT(!empty());
		const difference_type offset = pos - cbegin();
		const iterator it = begin() + offset;
		std::rotate(it, it + 1, end());
		--elementCount;
		return it;
	}

	iterator erase(const_iterator first, const_iterator last) {
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

	reference push_back_and_overwrite(auto overwrite) {
		reserveAtLeast(size() + 1);
		T& storage = elements[elementCount];
		overwrite(storage);
		++elementCount;
		return storage;
	}

	reference push_back_unspecified_value() {
		return push_back_and_overwrite([](T&) -> void {});
	}

	void push_back(const T& value) {
		push_back_and_overwrite([&](T& storage) -> void { storage = value; });
	}

	void push_back(T&& value) {
		push_back_and_overwrite([&](T& storage) -> void { storage = std::move(value); });
	}

	void pop_back() {
		GREM_ASSERT(!empty());
		--elementCount;
	}

	void resize(size_type count) {
		if (count < elementCount) {
			elementCount = count;
		} else if (count > elementCount) {
			reserveAtLeast(count);
			elementCount = count;
		}
	}

	void resize(size_type count, const value_type& value) {
		resize_and_overwrite_added_values(count, [&](T& storage) -> void { storage = value; });
	}

	void resize_and_overwrite_added_values(size_type count, auto overwrite) {
		if (count < elementCount) {
			elementCount = count;
		} else if (count > elementCount) {
			reserveAtLeast(count);
			while (elementCount < count) {
				overwrite(elements[elementCount]);
				++elementCount;
			}
		}
	}

	void swap(Buffer& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value || // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
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

	[[nodiscard]] bool operator==(const Buffer& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<(const Buffer& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<=(const Buffer& other) const {
		return !(other < *this);
	}

	[[nodiscard]] bool operator>(const Buffer& other) const {
		return other < *this;
	}

	[[nodiscard]] bool operator>=(const Buffer& other) const {
		return !(*this < other);
	}

	friend void swap(Buffer& a, Buffer& b) noexcept {
		a.swap(b);
	}

	template <typename U>
	friend size_type erase(Buffer& c, const U& value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend size_type erase_if(Buffer& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	struct MoveTag {};

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	Buffer(MoveTag, InputIterator first, Sentinel last, size_type capacity, const Allocator& allocator)
		: Buffer(allocator) {
		if constexpr (USE_MALLOC_AND_REALLOC) {
			if (capacity > std::numeric_limits<size_type>::max() / sizeof(T)) {
				throw std::bad_array_new_length{};
			}
			void* const newMemory = malloc(capacity * sizeof(T)); // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
			if (!newMemory) {
				throw std::bad_alloc{};
			}
			elements = static_cast<pointer>(newMemory);
		} else {
			elements = std::allocator_traits<Allocator>::allocate(this->allocator, capacity); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		}
		if constexpr (std::is_trivially_default_constructible_v<T>) {
			std::uninitialized_default_construct(std::to_address(elements), std::to_address(elements) + capacity);
			elementCapacity = capacity;
		} else {
			while (elementCapacity < capacity) {
				std::allocator_traits<Allocator>::construct(this->allocator, std::to_address(elements) + elementCapacity);
				++elementCapacity;
			}
		}
		moveAppend(first, last);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	Buffer(InputIterator first, Sentinel last, size_type capacity, const Allocator& allocator)
		: Buffer(allocator) {
		if constexpr (USE_MALLOC_AND_REALLOC) {
			if (capacity > std::numeric_limits<size_type>::max() / sizeof(T)) {
				throw std::bad_array_new_length{};
			}
			void* const newMemory = malloc(capacity * sizeof(T)); // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
			if (!newMemory) {
				throw std::bad_alloc{};
			}
			elements = static_cast<pointer>(newMemory);
		} else {
			elements = std::allocator_traits<Allocator>::allocate(this->allocator, capacity); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		}
		if constexpr (std::is_trivially_default_constructible_v<T>) {
			std::uninitialized_default_construct(std::to_address(elements), std::to_address(elements) + capacity);
			elementCapacity = capacity;
		} else {
			while (elementCapacity < capacity) {
				std::allocator_traits<Allocator>::construct(this->allocator, std::to_address(elements) + elementCapacity);
				++elementCapacity;
			}
		}
		copyAppend(first, last);
	}

	void reset() noexcept {
		if (elements) {
			T* const pBegin = std::to_address(elements);
			T* const pEnd = pBegin + elementCapacity;
			for (T* p = pBegin; p != pEnd; ++p) {
				std::allocator_traits<Allocator>::destroy(allocator, p);
			}
			if constexpr (USE_MALLOC_AND_REALLOC) {
				free(elements); // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
			} else {
				std::allocator_traits<Allocator>::deallocate(allocator, elements, elementCapacity);
			}
			elements = nullptr;
			elementCount = 0;
			elementCapacity = 0;
		}
	}

	void reserveAtLeast(size_type newCapacity) {
		if (const size_type cap = capacity(); newCapacity > cap) {
			const size_type maxSize = max_size();
			const size_type halfCap = cap / 2;
			if (newCapacity > maxSize || halfCap > maxSize - cap) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			newCapacity = std::max(newCapacity, cap + halfCap);
			if constexpr (USE_MALLOC_AND_REALLOC) {
				if (newCapacity > std::numeric_limits<size_type>::max() / sizeof(T)) {
					throw std::bad_array_new_length{};
				}
				static_assert(std::is_trivially_copyable_v<T>);
				static_assert(std::is_trivially_destructible_v<T>);
				void* const newMemory = realloc(elements, newCapacity * sizeof(T)); // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
				if (!newMemory) {
					throw std::bad_alloc{};
				}
				elements = static_cast<pointer>(newMemory);
				static_assert(std::is_nothrow_default_constructible_v<T>);
				while (elementCapacity < newCapacity) {
					std::allocator_traits<Allocator>::construct(this->allocator, std::to_address(elements) + elementCapacity);
					++elementCapacity;
				}
			} else {
				if constexpr (std::is_nothrow_move_constructible_v<T>) {
					*this = Buffer(MoveTag{}, begin(), end(), newCapacity, allocator);
				} else {
					*this = Buffer(cbegin(), cend(), newCapacity, allocator);
				}
			}
		}
	}

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
			reserveAtLeast(newSize);
			if constexpr (std::is_trivially_copyable_v<T> && requires { std::to_address(first); }) {
				std::memcpy(std::to_address(elements) + offset, std::to_address(first), count * sizeof(T));
			} else {
				std::copy(first, last, elements + offset);
			}
			elementCount = newSize;
		} else {
			while (first != last) {
				push_back(*first++);
			}
		}
	}

	template <typename InputIterator, typename Sentinel>
	void moveAppend(InputIterator first, Sentinel last) {
		if constexpr (
			std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag> && std::is_nothrow_move_assignable_v<T>) {
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
				std::memcpy(std::to_address(elements) + offset, std::to_address(first), count * sizeof(T));
			} else {
				std::move(first, last, elements + offset);
			}
			elementCount = newSize;
		} else {
			while (first != last) {
				push_back(std::move(*first++));
			}
		}
	}

	pointer elements = nullptr;
	size_type elementCount = 0;
	size_type elementCapacity = 0;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename T>
using Buffer = grem::Buffer<T, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
