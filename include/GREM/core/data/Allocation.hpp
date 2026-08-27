// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ALLOCATION_HPP
#define GREM_CORE_DATA_ALLOCATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::fill_n, std::equal, std::lexicographical_compare
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::reverse_iterator, std::input_iterator, std::random_access_iterator, std::sentinel_for, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address, std::uninitialized_..., std::destroy
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

template <typename T, typename Allocator = std::allocator<T>>
class Allocation {
public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = typename std::allocator_traits<Allocator>::pointer;
	using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr Allocation() noexcept = default;

	constexpr explicit Allocation(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	constexpr explicit Allocation(size_type count, const Allocator& allocator = Allocator())
		: Allocation(allocator) {
		if (count > 0) {
			if (count > max_size()) {
				throw std::length_error{"count > max_size()"};
			}
			const pointer newElements = std::allocator_traits<Allocator>::allocate(this->allocator, count);
			T* const pNewBegin = std::to_address(newElements);
			T* const pNewEnd = pNewBegin + count;
			if constexpr (std::is_trivially_default_constructible_v<T>) {
				std::uninitialized_default_construct(pNewBegin, pNewEnd);
			} else {
				T* pNew = pNewBegin;
				try {
					while (pNew != pNewEnd) {
						std::allocator_traits<Allocator>::construct(this->allocator, pNew++);
					}
				} catch (...) {
					while (pNew != pNewBegin) {
						std::allocator_traits<Allocator>::destroy(this->allocator, --pNew);
					}
					std::allocator_traits<Allocator>::deallocate(this->allocator, newElements, count);
					throw;
				}
			}
			elements = newElements;
			elementCount = count;
		}
	}

	constexpr Allocation(size_type count, const T& value, const Allocator& allocator = Allocator())
		: Allocation(allocator) {
		assign(count, value);
	}

	template <std::input_iterator InputIterator>
	constexpr Allocation(InputIterator first, size_type count, const Allocator& allocator = Allocator())
		: Allocation(allocator) {
		assign(first, count);
	}

	template <std::random_access_iterator RandomAccessIterator, std::sentinel_for<RandomAccessIterator> Sentinel>
	constexpr Allocation(RandomAccessIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: Allocation(allocator) {
		assign(first, last);
	}

	constexpr Allocation(const Allocation& other)
		: Allocation(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr Allocation(const Allocation& other, const Allocator& allocator)
		: Allocation(other.begin(), other.end(), allocator) {}

	constexpr Allocation(Allocation&& other) noexcept
		: Allocation(std::move(other), other.get_allocator()) {}

	constexpr Allocation(Allocation&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: elements(std::exchange(other.elements, nullptr))
		, elementCount(std::exchange(other.elementCount, size_type{0}))
		, allocator(allocator) {}

	constexpr Allocation(std::initializer_list<T> ilist, const Allocator& allocator = Allocator())
		: Allocation(ilist.begin(), ilist.end(), allocator) {}

	constexpr ~Allocation() {
		clear();
	}

	constexpr Allocation& operator=(const Allocation& other) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					clear();
				}
			}
			allocator = other.get_allocator();
		}
		assign(other.begin(), other.end());
		return *this;
	}

	constexpr Allocation& operator=(Allocation&& other) noexcept(
		std::allocator_traits<
			Allocator>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<Allocator>::is_always_equal::value) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value) {
			clear();
			elements = std::exchange(other.elements, nullptr);
			elementCount = std::exchange(other.elementCount, size_type{0});
			if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
				allocator = other.allocator;
			}
		} else {
			if (allocator == other.allocator) {
				clear();
				elements = std::exchange(other.elements, nullptr);
				elementCount = std::exchange(other.elementCount, size_type{0});
			} else {
				GREM_ASSERT(other.elements);
				*this = Allocation(other, allocator);
			}
		}
		return *this;
	}

	constexpr Allocation& operator=(std::initializer_list<T> ilist) {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, const T& value) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count == size()) {
				for (T& element : *this) {
					element = value;
				}
				return;
			}
		}
		if (count > 0) {
			if (count > max_size()) {
				throw std::length_error{"count > max_size()"};
			}
			const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, count);
			T* const pNewBegin = std::to_address(newElements);
			T* const pNewEnd = pNewBegin + count;
			if constexpr (std::is_trivially_copy_constructible_v<T>) {
				std::uninitialized_fill(pNewBegin, pNewEnd, value);
			} else {
				T* pNew = pNewBegin;
				try {
					while (pNew != pNewEnd) {
						std::allocator_traits<Allocator>::construct(allocator, pNew++, value);
					}
				} catch (...) {
					while (pNew != pNewBegin) {
						std::allocator_traits<Allocator>::destroy(allocator, --pNew);
					}
					std::allocator_traits<Allocator>::deallocate(allocator, newElements, count);
					throw;
				}
			}
			clear();
			elements = newElements;
			elementCount = count;
		} else {
			clear();
		}
	}

	template <std::input_iterator InputIterator>
	constexpr void assign(InputIterator first, size_type count) {
		if constexpr (std::is_copy_assignable_v<T>) {
			if (count == size()) {
				for (T& element : *this) {
					element = *first++;
				}
				return;
			}
		}
		if (count > 0) {
			if (count > max_size()) {
				throw std::length_error{"count > max_size()"};
			}
			const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, count);
			T* const pNewBegin = std::to_address(newElements);
			if constexpr (std::is_trivially_copy_constructible_v<T>) {
				std::uninitialized_copy_n(first, count, pNewBegin);
			} else {
				T* const pNewEnd = pNewBegin + count;
				T* pNew = pNewBegin;
				try {
					while (pNew != pNewEnd) {
						std::allocator_traits<Allocator>::construct(allocator, pNew++, *first++);
					}
				} catch (...) {
					while (pNew != pNewBegin) {
						std::allocator_traits<Allocator>::destroy(allocator, --pNew);
					}
					std::allocator_traits<Allocator>::deallocate(allocator, newElements, count);
					throw;
				}
			}
			clear();
			elements = newElements;
			elementCount = count;
		} else {
			clear();
		}
	}

	template <std::random_access_iterator RandomAccessIterator, std::sentinel_for<RandomAccessIterator> Sentinel>
	constexpr void assign(RandomAccessIterator first, Sentinel last) {
		assign(first, static_cast<size_type>(last - first));
	}

	constexpr void assign(std::initializer_list<T> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename R>
	constexpr void assign_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		assign(std::begin(r), std::end(r));
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
			std::allocator_traits<Allocator>::deallocate(allocator, elements, elementCount);
			elements = nullptr;
			elementCount = 0;
		}
	}

	constexpr void resize(size_type count) {
		if (count != size()) {
			if (count > 0) {
				if (count > max_size()) {
					throw std::length_error{"count > max_size()"};
				}
				const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, count);
				T* const pNewBegin = std::to_address(newElements);
				T* const pOldBegin = std::to_address(elements);
				T* const pNewEnd = pNewBegin + count;
				T* const pOldEnd = pOldBegin + elementCount;
				if constexpr (std::is_trivially_move_constructible_v<T> && std::is_trivially_default_constructible_v<T>) {
					if (count > elementCount) {
						std::uninitialized_move(pOldBegin, pOldEnd, pNewBegin);
						std::uninitialized_default_construct(pNewBegin + elementCount, pNewEnd);
					} else {
						std::uninitialized_move(pOldBegin, pOldBegin + count, pNewBegin);
					}
				} else {
					T* pNew = pNewBegin;
					T* pOld = pOldBegin;
					try {
						while (pNew != pNewEnd && pOld != pOldEnd) {
							if constexpr (std::is_nothrow_move_assignable_v<T>) {
								std::allocator_traits<Allocator>::construct(allocator, pNew++, std::move(*pOld++));
							} else {
								std::allocator_traits<Allocator>::construct(allocator, pNew++, *pOld++);
							}
						}
						while (pNew != pNewEnd) {
							std::allocator_traits<Allocator>::construct(allocator, pNew++);
						}
					} catch (...) {
						while (pNew != pNewBegin) {
							std::allocator_traits<Allocator>::destroy(allocator, --pNew);
						}
						std::allocator_traits<Allocator>::deallocate(allocator, newElements, count);
						throw;
					}
				}
				clear();
				elements = newElements;
				elementCount = count;
			} else {
				clear();
			}
		}
	}

	constexpr void resize(size_type count, const T& value) {
		if (count != size()) {
			if (count > 0) {
				if (count > max_size()) {
					throw std::length_error{"count > max_size()"};
				}
				const pointer newElements = std::allocator_traits<Allocator>::allocate(allocator, count);
				T* const pNewBegin = std::to_address(newElements);
				T* const pNewEnd = pNewBegin + count;
				T* const pOldBegin = std::to_address(elements);
				T* const pOldEnd = pOldBegin + elementCount;
				if constexpr (std::is_trivially_move_constructible_v<T> && std::is_trivially_copy_constructible_v<T>) {
					if (count > elementCount) {
						std::uninitialized_move(pOldBegin, pOldEnd, pNewBegin);
						std::uninitialized_fill(pNewBegin + elementCount, pNewEnd, value);
					} else {
						std::uninitialized_move(pOldBegin, pOldBegin + count, pNewBegin);
					}
				} else {
					T* pNew = pNewBegin;
					T* pOld = pOldBegin;
					try {
						while (pNew != pNewEnd && pOld != pOldEnd) {
							if constexpr (std::is_nothrow_move_assignable_v<T>) {
								std::allocator_traits<Allocator>::construct(allocator, pNew++, std::move(*pOld++));
							} else {
								std::allocator_traits<Allocator>::construct(allocator, pNew++, *pOld++);
							}
						}
						while (pNew != pNewEnd) {
							std::allocator_traits<Allocator>::construct(allocator, pNew++, value);
						}
					} catch (...) {
						while (pNew != pNewBegin) {
							std::allocator_traits<Allocator>::destroy(allocator, --pNew);
						}
						std::allocator_traits<Allocator>::deallocate(allocator, newElements, count);
						throw;
					}
				}
				clear();
				elements = newElements;
				elementCount = count;
			} else {
				clear();
			}
		}
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= elementCount) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
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
		GREM_ASSERT(elementCount > 0);
		return elements[0];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(elementCount > 0);
		return elements[0];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(elements);
		GREM_ASSERT(elementCount > 0);
		return elements[elementCount - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(elements);
		GREM_ASSERT(elementCount > 0);
		return elements[elementCount - 1];
	}

	[[nodiscard]] constexpr pointer data() noexcept {
		return elements;
	}

	[[nodiscard]] constexpr const_pointer data() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{elements};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{elements};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{elements + elementCount};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{elements + elementCount};
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
		return elementCount == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elementCount;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return std::min({
			static_cast<size_type>(std::numeric_limits<difference_type>::max()),
			static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(T)),
			static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator)),
		});
	}

	constexpr void fill(const T& value) {
		std::fill_n(elements, elementCount, value);
	}

	constexpr void swap(Allocation& other) noexcept(
		std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		using std::swap;
		swap(elements, other.elements);
		swap(elementCount, other.elementCount);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		} else if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			GREM_ASSERT(allocator == other.allocator);
		}
	}

	friend constexpr void swap(Allocation& a, Allocation& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	[[nodiscard]] constexpr bool operator==(const Allocation& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const Allocation& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const Allocation& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const Allocation& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const Allocation& other) const {
		return !(*this < other);
	}

	constexpr pointer release() noexcept {
		const pointer result = std::exchange(elements, nullptr);
		elementCount = 0;
		return result;
	}

private:
	pointer elements = nullptr;
	size_type elementCount = 0;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename T>
using Allocation = grem::Allocation<T, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
