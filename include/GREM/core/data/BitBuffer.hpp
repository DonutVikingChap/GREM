// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_BIT_BUFFER_HPP
#define GREM_CORE_DATA_BIT_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <algorithm>        // std::min, std::max, std::rotate, std::equal, std::lexicographical_compare, std::remove, std::remove_if, std::copy, std::move, std::fill_n
#include <bit>              // std::countr_zero
#include <cstddef>          // std::size_t, std::max_align_t
#include <cstring>          // std::memcpy
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::reverse_iterator, std::input_iterator, std::sentinel_for, std::iterator_traits, std::random_access_iterator_tag, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address, std::uninitialized_...
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::out_of_range, std::length_error
#include <stdlib.h>         // malloc, realloc, free // NOLINT(modernize-deprecated-headers)
#include <type_traits>      // std::make_signed_t, std::is_..._v
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

template <typename Allocator = std::allocator<bool>>
class BitBufferBase {
private:
	using Integer = std::size_t;
	using IntegerAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Integer>;
	using IntegerPointer = typename std::allocator_traits<IntegerAllocator>::pointer;

public:
	using value_type = bool;
	using allocator_type = Allocator;
	using size_type = typename std::allocator_traits<IntegerAllocator>::size_type;
	using difference_type = typename std::allocator_traits<IntegerAllocator>::difference_type;

private:
	static constexpr size_type INTEGER_BITS = std::numeric_limits<Integer>::digits;
	static constexpr size_type INTEGER_BIT_SHIFT = std::countr_zero(INTEGER_BITS);

public:
	class reference {
	public:
		constexpr reference(Integer* bits, size_type bitIndex) noexcept
			: bits(bits)
			, bitIndex(bitIndex) {}

		constexpr operator bool() const noexcept {
			return ((*bits >> bitIndex) & 1) != 0;
		}

		constexpr reference& operator=(bool newValue) noexcept {
			*bits = (*bits & ~(Integer{1} << bitIndex)) | (static_cast<Integer>(newValue) << bitIndex);
			return *this;
		}

		constexpr const reference& operator=(bool newValue) const noexcept { // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
			*bits = (*bits & ~(Integer{1} << bitIndex)) | (static_cast<Integer>(newValue) << bitIndex);
			return *this;
		}

		constexpr void flip() noexcept {
			*bits ^= (Integer{1} << bitIndex);
		}

	private:
		Integer* bits;
		size_type bitIndex;
	};

	using const_reference = bool;

	struct pointer {
		reference ref;

		[[nodiscard]] constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	struct const_pointer {
		const_reference ref;

		[[nodiscard]] constexpr const_reference* operator->() noexcept {
			return &ref;
		}
	};

private:
	template <bool Const>
	class Iterator {
	private:
		static constexpr size_type INTEGER_BITS = BitBufferBase::INTEGER_BITS;
		static constexpr size_type INTEGER_BIT_SHIFT = BitBufferBase::INTEGER_BIT_SHIFT;

		using BitsPointer = std::conditional_t<Const, const Integer*, Integer*>;

	public:
		using difference_type = std::ptrdiff_t;
		using value_type = bool;
		using reference = std::conditional_t<Const, typename BitBufferBase::const_reference, typename BitBufferBase::reference>;
		using iterator_category = std::random_access_iterator_tag;
		using pointer = std::conditional_t<Const, typename BitBufferBase::const_pointer, typename BitBufferBase::pointer>;

		Iterator() noexcept = default;

		constexpr Iterator(BitsPointer bits, size_type bitIndex) noexcept
			: bits(bits)
			, bitIndex(bitIndex) {}

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{bits, bitIndex};
		}

		[[nodiscard]] constexpr reference operator*() const {
			if constexpr (Const) {
				return ((*bits >> bitIndex) & 1) != 0;
			} else {
				return reference{bits, bitIndex};
			}
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return pointer{**this};
		}

		[[nodiscard]] constexpr reference operator[](difference_type n) const {
			return *(*this + n);
		}

		constexpr Iterator& operator++() {
			if (++bitIndex == INTEGER_BITS) {
				bitIndex = 0;
				++bits;
			}
			return *this;
		}

		constexpr Iterator& operator--() {
			if (bitIndex-- == 0) {
				bitIndex = INTEGER_BITS - 1;
				--bits;
			}
			return *this;
		}

		constexpr Iterator operator++(int) {
			Iterator old = *this;
			++*this;
			return old;
		}

		constexpr Iterator operator--(int) {
			Iterator old = *this;
			--*this;
			return old;
		}

		constexpr Iterator& operator+=(difference_type n) {
			bitIndex += static_cast<size_type>(static_cast<std::make_signed_t<size_type>>(n));
			bits += static_cast<std::make_signed_t<size_type>>(bitIndex) >> INTEGER_BIT_SHIFT;
			bitIndex &= (INTEGER_BITS - 1);
			return *this;
		}

		constexpr Iterator& operator-=(difference_type n) {
			return *this += -n;
		}

		[[nodiscard]] friend constexpr Iterator operator+(Iterator a, difference_type b) {
			return a += b;
		}

		[[nodiscard]] friend constexpr Iterator operator+(difference_type a, Iterator b) {
			return b += a;
		}

		[[nodiscard]] friend constexpr Iterator operator-(Iterator a, difference_type b) {
			return a -= b;
		}

		[[nodiscard]] friend constexpr difference_type operator-(Iterator a, Iterator b) {
			return static_cast<difference_type>(a.bits - b.bits) * difference_type{INTEGER_BITS} +
			       static_cast<difference_type>(static_cast<std::make_signed_t<size_type>>(a.bitIndex - b.bitIndex));
		}

		[[nodiscard]] friend constexpr bool operator==(Iterator a, Iterator b) {
			return a.bits == b.bits && a.bitIndex == b.bitIndex;
		}

		[[nodiscard]] friend constexpr auto operator<=>(Iterator a, Iterator b) {
			return (a.bits == b.bits) ? a.bitIndex <=> b.bitIndex : a.bits <=> b.bits;
		}

	private:
		BitsPointer bits;
		size_type bitIndex;
	};

public:
	using iterator = Iterator<false>;
	using const_iterator = Iterator<true>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr BitBufferBase() noexcept = default;

	constexpr explicit BitBufferBase(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	constexpr BitBufferBase(size_type count, bool value, const Allocator& allocator = Allocator())
		: BitBufferBase(allocator) {
		resize(count, value);
	}

	constexpr explicit BitBufferBase(size_type count, const Allocator& allocator = Allocator())
		: BitBufferBase(allocator) {
		resize(count);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr BitBufferBase(InputIterator first, Sentinel last, const Allocator& allocator = Allocator())
		: BitBufferBase(allocator) {
		assign(first, last);
	}

	constexpr BitBufferBase(std::initializer_list<bool> ilist, const Allocator& allocator = Allocator())
		: BitBufferBase(ilist.begin(), ilist.end(), allocator) {}

	constexpr ~BitBufferBase() {
		reset();
	}

	constexpr BitBufferBase(const BitBufferBase& other)
		: BitBufferBase(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr BitBufferBase(const BitBufferBase& other, const Allocator& allocator)
		: BitBufferBase(other.begin(), other.end(), allocator) {}

	constexpr BitBufferBase(BitBufferBase&& other) noexcept
		: BitBufferBase(std::move(other), other.get_allocator()) {}

	constexpr BitBufferBase(BitBufferBase&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: bits(std::exchange(other.bits, nullptr))
		, bitCount(std::exchange(other.bitCount, size_type{0}))
		, bitCapacity(std::exchange(other.bitCapacity, size_type{0}))
		, allocator(allocator) {}

	constexpr BitBufferBase& operator=(const BitBufferBase& other) {
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

	constexpr BitBufferBase& operator=(BitBufferBase&& other) noexcept(
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
				copyAppend(other.begin(), other.end());
				other.clear();
				return *this;
			}
		}
		reset();
		bits = std::exchange(other.bits, nullptr);
		bitCount = std::exchange(other.bitCount, size_type{0});
		bitCapacity = std::exchange(other.bitCapacity, size_type{0});
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	constexpr BitBufferBase& operator=(std::initializer_list<bool> ilist) noexcept {
		assign(ilist);
		return *this;
	}

	constexpr void assign(size_type count, bool value) {
		while (size() > count) {
			pop_back();
		}
		reserve(count);
		std::fill_n(begin(), size(), value);
		for (size_type index = size(); index < count; ++index) {
			push_back(value);
		}
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr void assign(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			const size_type count = static_cast<size_type>(last - first);
			while (size() > count) {
				pop_back();
			}
			reserve(count);
			for (size_type index = 0; index < size(); ++index) {
				begin()[index] = *first++;
			}
			copyAppend(first, last);
		} else {
			clear();
			copyAppend(first, last);
		}
	}

	constexpr void assign(std::initializer_list<bool> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename R>
	constexpr void assign_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		assign(std::begin(r), std::end(r));
	}

	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return begin()[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return begin()[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < size());
		return begin()[static_cast<difference_type>(pos)];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < size());
		return begin()[static_cast<difference_type>(pos)];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return *begin();
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return *begin();
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return begin()[static_cast<difference_type>(size() - 1)];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return begin()[static_cast<difference_type>(size() - 1)];
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{bits, 0};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{bits, 0};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		const size_type n = size();
		return iterator{bits + n / INTEGER_BITS, n % INTEGER_BITS};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		const size_type n = size();
		return const_iterator{bits + n / INTEGER_BITS, n % INTEGER_BITS};
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
		return bitCount == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return bitCount;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		const size_type maxIntegerCount = std::min(static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(Integer)),
			static_cast<size_type>(std::allocator_traits<IntegerAllocator>::max_size(allocator)));
		const size_type maxBitCount =
			(maxIntegerCount >= std::numeric_limits<size_type>::max() / INTEGER_BITS) ? std::numeric_limits<size_type>::max() : maxIntegerCount * INTEGER_BITS;
		return std::min(maxBitCount, size_type{std::numeric_limits<difference_type>::max()});
	}

	constexpr void reserve(size_type newCapacity) {
		if (newCapacity > capacity()) {
			if (newCapacity > max_size()) {
				throw std::length_error{"newCapacity > max_size()"};
			}
			*this = BitBufferBase(cbegin(), cend(), newCapacity, allocator);
		}
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return bitCapacity;
	}

	constexpr void shrink_to_fit() {
		if (size() < capacity()) {
			*this = BitBufferBase(cbegin(), cend(), size(), allocator);
		}
	}

	constexpr void clear() noexcept {
		bitCount = 0;
	}

	constexpr iterator insert(const_iterator pos, bool value) {
		return emplace(pos, value);
	}

	constexpr iterator insert(const_iterator pos, size_type count, bool value) {
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

	constexpr iterator insert(const_iterator pos, std::initializer_list<bool> ilist) {
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
		GREM_ASSERT(count <= size());
		bitCount -= count;
		return it;
	}

	constexpr void push_back(bool value) {
		emplace_back(value);
	}

	template <typename... Args>
	constexpr reference emplace_back(Args&&... args) {
		reserveAtLeast(size() + 1);
		reference result = begin()[bitCount];
		result = bool(std::forward<Args>(args)...);
		++bitCount;
		return result;
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--bitCount;
	}

	constexpr void resize(size_type count) {
		if (count < size()) {
			bitCount = count;
		} else if (count > size()) {
			reserveAtLeast(count);
			bitCount = count;
		}
	}

	constexpr void resize(size_type count, bool value) {
		if (count < size()) {
			bitCount = count;
		} else if (count > size()) {
			reserveAtLeast(count);
			while (bitCount < count) {
				begin()[static_cast<difference_type>(bitCount)] = value;
				++bitCount;
			}
		}
	}

	constexpr void swap(BitBufferBase& other) noexcept(
		std::allocator_traits<Allocator>::propagate_on_container_swap::value || // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		std::allocator_traits<Allocator>::is_always_equal::value) {
		if (this == &other) {
			return;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_swap::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				throw std::logic_error{"get_allocator() != other.get_allocator()"};
			}
		}
		using std::swap;
		swap(bits, other.bits);
		swap(bitCount, other.bitCount);
		swap(bitCapacity, other.bitCapacity);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		}
	}

	[[nodiscard]] bool operator==(const BitBufferBase& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<(const BitBufferBase& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] bool operator<=(const BitBufferBase& other) const {
		return !(other < *this);
	}

	[[nodiscard]] bool operator>(const BitBufferBase& other) const {
		return other < *this;
	}

	[[nodiscard]] bool operator>=(const BitBufferBase& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(BitBufferBase& a, BitBufferBase& b) noexcept {
		a.swap(b);
	}

	friend size_type erase(BitBufferBase& c, bool value) {
		const iterator it = std::remove(c.begin(), c.end(), value);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

	template <typename Predicate>
	friend size_type erase_if(BitBufferBase& c, Predicate predicate) {
		const iterator it = std::remove_if(c.begin(), c.end(), predicate);
		const size_type result = static_cast<size_type>(c.end() - it);
		c.erase(it, c.end());
		return result;
	}

private:
	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	BitBufferBase(InputIterator first, Sentinel last, size_type capacity, const Allocator& allocator)
		: BitBufferBase(allocator) {
		bits =
			std::allocator_traits<IntegerAllocator>::allocate(this->allocator, (capacity + INTEGER_BITS - 1) / INTEGER_BITS); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		bitCapacity = capacity;                                                                                               // NOLINT(cppcoreguidelines-prefer-member-initializer)
		copyAppend(first, last);
	}

	void reset() noexcept {
		if (bits) {
			std::allocator_traits<IntegerAllocator>::deallocate(allocator, bits, (bitCapacity + INTEGER_BITS - 1) / INTEGER_BITS);
			bits = nullptr;
			bitCount = 0;
			bitCapacity = 0;
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
			*this = BitBufferBase(cbegin(), cend(), newCapacity, allocator);
		}
	}

	template <typename InputIterator, typename Sentinel>
	void copyAppend(InputIterator first, Sentinel last) {
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
			std::copy(first, last, begin() + static_cast<difference_type>(offset));
			bitCount = newSize;
		} else {
			while (first != last) {
				push_back(*first++);
			}
		}
	}

	IntegerPointer bits = nullptr;
	size_type bitCount = 0;
	size_type bitCapacity = 0;
	[[no_unique_address]] IntegerAllocator allocator;
};

using BitBuffer = BitBufferBase<>;

} // namespace grem

namespace grem::pmr {

using BitBuffer = grem::BitBufferBase<std::pmr::polymorphic_allocator<bool>>;

} // namespace grem::pmr

#endif
