// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_BIT_ARRAY_HPP
#define GREM_CORE_DATA_BIT_ARRAY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <cstdint>     // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <functional>  // std::hash
#include <iterator>    // std::reverse_iterator
#include <limits>      // std::numeric_limits
#include <stdexcept>   // std::out_of_range
#include <type_traits> // std::make_signed_t
#include <utility>     // std::swap

namespace grem {

template <std::size_t N>
class BitArray {
public:
	using value_type = bool;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	// clang-format off
    using integer_type =
        std::conditional_t<N <= 8, std::uint8_t,
        std::conditional_t<N <= 16, std::uint16_t,
        std::conditional_t<N <= 32, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

private:
	static constexpr size_type INTEGER_BITS = std::numeric_limits<integer_type>::digits;
	static constexpr size_type INTEGER_COUNT = (N + 63) / 64;

public:
	class reference {
	public:
		constexpr reference(integer_type* bits, size_type bitIndex) noexcept
			: bits(bits)
			, bitIndex(bitIndex) {}

		constexpr operator bool() const noexcept {
			return ((*bits >> bitIndex) & 1) != 0;
		}

		constexpr reference& operator=(bool newValue) noexcept {
			*bits = (*bits & ~(integer_type{1} << bitIndex)) | (static_cast<integer_type>(newValue) << bitIndex);
			return *this;
		}

		constexpr const reference& operator=(bool newValue) const noexcept { // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
			*bits = (*bits & ~(integer_type{1} << bitIndex)) | (static_cast<integer_type>(newValue) << bitIndex);
			return *this;
		}

		constexpr void flip() noexcept {
			*bits ^= (integer_type{1} << bitIndex);
		}

	private:
		integer_type* bits;
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
		using BitsPointer = std::conditional_t<Const, const integer_type*, integer_type*>;

	public:
		using difference_type = std::ptrdiff_t;
		using value_type = bool;
		using reference = std::conditional_t<Const, typename BitArray::const_reference, typename BitArray::reference>;
		using iterator_category = std::random_access_iterator_tag;
		using pointer = std::conditional_t<Const, typename BitArray::const_pointer, typename BitArray::pointer>;

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
			if constexpr (N <= 64) {
				++bitIndex;
			} else {
				if (++bitIndex == 64) {
					bitIndex = 0;
					++bits;
				}
			}
			return *this;
		}

		constexpr Iterator& operator--() {
			if constexpr (N <= 64) {
				--bitIndex;
			} else {
				if (bitIndex-- == 0) {
					bitIndex = 63;
					--bits;
				}
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
			if constexpr (N > 64) {
				bits += static_cast<std::make_signed_t<size_type>>(bitIndex) >> 6;
				bitIndex &= size_type{63};
			}
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
			if constexpr (N <= 64) {
				return static_cast<difference_type>(static_cast<std::make_signed_t<size_type>>(a.bitIndex - b.bitIndex));
			} else {
				return static_cast<difference_type>(a.bits - b.bits) * 64 + static_cast<difference_type>(static_cast<std::make_signed_t<size_type>>(a.bitIndex - b.bitIndex));
			}
		}

		[[nodiscard]] friend constexpr bool operator==(Iterator a, Iterator b) {
			if constexpr (N <= 64) {
				GREM_ASSERT(a.bits == b.bits);
				return a.bitIndex == b.bitIndex;
			} else {
				return a.bits == b.bits && a.bitIndex == b.bitIndex;
			}
		}

		[[nodiscard]] friend constexpr auto operator<=>(Iterator a, Iterator b) {
			if constexpr (N <= 64) {
				GREM_ASSERT(a.bits == b.bits);
				return a.bitIndex <=> b.bitIndex;
			} else {
				return (a.bits == b.bits) ? a.bitIndex <=> b.bitIndex : a.bits <=> b.bits;
			}
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

	constexpr BitArray() noexcept = default;

	constexpr explicit BitArray(integer_type firstBits) noexcept
		: bits{firstBits} {}

	[[nodiscard]] constexpr integer_type toInteger() const noexcept {
		return bits[0];
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= N) {
			throw std::out_of_range{"pos >= size()"};
		}
		return begin()[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= N) {
			throw std::out_of_range{"pos >= size()"};
		}
		return begin()[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < N);
		return begin()[static_cast<difference_type>(pos)];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < N);
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
		return begin()[N - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return begin()[N - 1];
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
		if constexpr (N <= 64) {
			return iterator{bits, N};
		} else {
			return iterator{bits + N / 64, N % 64};
		}
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		if constexpr (N <= 64) {
			return const_iterator{bits, N};
		} else {
			return const_iterator{bits + N / 64, N % 64};
		}
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
		return N == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return N;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return N;
	}

	constexpr void fill(bool value) {
		if (value) {
			constexpr size_type QUOTIENT_BITS = N / INTEGER_BITS;
			constexpr size_type REMAINDER_BITS = N % INTEGER_BITS;
			for (size_type i = 0; i < QUOTIENT_BITS; ++i) {
				bits[i] = std::numeric_limits<integer_type>::max();
			}
			if constexpr (REMAINDER_BITS > 0) {
				bits[QUOTIENT_BITS] = std::numeric_limits<integer_type>::max() >> (INTEGER_BITS - REMAINDER_BITS);
			}
		} else {
			for (integer_type& b : bits) {
				b = 0;
			}
		}
	}

	constexpr void swap(BitArray& other) noexcept {
		std::swap(bits, other.bits);
	}

	friend constexpr void swap(BitArray& a, BitArray& b) noexcept {
		a.swap(b);
	}

	constexpr BitArray& operator&=(const BitArray& other) noexcept {
		for (size_type i = 0; i < INTEGER_COUNT; ++i) {
			bits[i] &= other.bits[i];
		}
		return *this;
	}

	[[nodiscard]] friend constexpr BitArray operator&(const BitArray& a, const BitArray& b) noexcept {
		BitArray result = a;
		result &= b;
		return result;
	}

	constexpr BitArray& operator|=(const BitArray& other) noexcept {
		for (size_type i = 0; i < INTEGER_COUNT; ++i) {
			bits[i] |= other.bits[i];
		}
		return *this;
	}

	[[nodiscard]] friend constexpr BitArray operator|(const BitArray& a, const BitArray& b) noexcept {
		BitArray result = a;
		result |= b;
		return result;
	}

	constexpr BitArray& operator^=(const BitArray& other) noexcept {
		for (size_type i = 0; i < INTEGER_COUNT; ++i) {
			bits[i] ^= other.bits[i];
		}
		return *this;
	}

	[[nodiscard]] friend constexpr BitArray operator^(const BitArray& a, const BitArray& b) noexcept {
		BitArray result = a;
		result ^= b;
		return result;
	}

	[[nodiscard]] constexpr BitArray operator~() noexcept {
		BitArray result{};
		constexpr size_type QUOTIENT_BITS = N / INTEGER_BITS;
		constexpr size_type REMAINDER_BITS = N % INTEGER_BITS;
		for (size_type i = 0; i < QUOTIENT_BITS; ++i) {
			result.bits[i] = ~bits[i];
		}
		if constexpr (REMAINDER_BITS > 0) {
			result.bits[QUOTIENT_BITS] = ~bits[QUOTIENT_BITS] & (std::numeric_limits<integer_type>::max() >> (INTEGER_BITS - REMAINDER_BITS));
		}
		return result;
	}

	[[nodiscard]] bool operator==(const BitArray& other) const = default;

private:
	integer_type bits[INTEGER_COUNT]{};
};

} // namespace grem

template <std::size_t N>
struct std::hash<grem::BitArray<N>> {
	[[nodiscard]] std::size_t operator()(const grem::BitArray<N>& bitArray) const {
		return hasher(bitArray.toInteger());
	}

private:
	[[no_unique_address]] std::hash<typename grem::BitArray<N>::integer_type> hasher;
};

#endif
