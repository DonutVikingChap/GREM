// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_UNICODE_HPP
#define GREM_CORE_FORMATS_UNICODE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <iterator> // std::input_iterator_tag, std::forward_iterator_tag
#include <new>      // std::launder

namespace grem::unicode {

/**
 * Check if a 32-bit unsigned integer value falls within the valid ranges for a
 * Unicode code point.
 *
 * \param codePoint 32-bit code point value to check.
 *
 * \return true if the code unit is a valid code point, false otherwise.
 */
[[nodiscard]] constexpr bool isValidCodePoint(char32_t codePoint) noexcept {
	return codePoint <= 0x10FFFF && (codePoint < 0xD800 || codePoint > 0xDFFF);
}

/**
 * Invalid code point value, used as a return value in Unicode decoding
 * algorithms for conveying encoding errors.
 */
inline constexpr char32_t CODE_POINT_ERROR{0xFFFFFFFF};

/**
 * Decode a single Unicode code point from an iterator of UTF-8 code units in a
 * UTF-8-encoded string.
 *
 * \param it input iterator to a sequence of UTF-8 code units. The expression
 *        `*it++` must be convertible to char8_t.
 * \param end end iterator or sentinel that marks the end of the UTF-8 code unit
 *        sequence.
 *
 * \return a pair of:
 *         - the decoded Unicode code point, or #CODE_POINT_ERROR on failure to
 *           decode a code point due to an encoding error in the UTF-8 string,
 *           and
 *         - the input iterator, positioned at the start of the next UTF-8 code
 *           unit after the parsed code point sequence.
 *
 * \throws any exception thrown by the iterator implementation.
 */
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
[[nodiscard]] constexpr Pair<char32_t, InputIterator> decodeCodePointFromUTF8(InputIterator it, Sentinel end) {
	if (it == end) {
		[[unlikely]] return {CODE_POINT_ERROR, it}; // Reached end.
	}
	char32_t codePoint{};
	const char8_t c0 = static_cast<char8_t>(*it++);
	if ((c0 & 0b10000000u) == 0) { // 0-127
		[[likely]] codePoint = static_cast<char32_t>(c0);
	} else if ((c0 & 0b11100000u) == 0b11000000u) { // 128-2047
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c1 = static_cast<char8_t>(*it++);
		if ((c1 & 0b11000000u) != 0b10000000u) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Invalid continuation.
		}
		codePoint = ((c0 & 0b00011111u) << 6) | (c1 & 0b00111111u);
		if (codePoint < 128) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Overlong sequence.
		}
	} else if ((c0 & 0b11110000u) == 0b11100000u) { // 2048-65535
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c1 = static_cast<char8_t>(*it++);
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c2 = static_cast<char8_t>(*it++);
		if ((c1 & 0b11000000u) != 0b10000000u || (c2 & 0b11000000u) != 0b10000000u) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Invalid continuation.
		}
		codePoint = ((c0 & 0b00001111u) << 12) | ((c1 & 0b00111111u) << 6) | (c2 & 0b00111111u);
		if (codePoint < 2048) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Overlong sequence.
		}
		if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Surrogate code point.
		}
	} else if ((c0 & 0b11111000u) == 0b11110000u) { // 65536-1114111
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c1 = static_cast<char8_t>(*it++);
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c2 = static_cast<char8_t>(*it++);
		if (it == end) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Missing continuation.
		}
		const char8_t c3 = static_cast<char8_t>(*it++);
		if ((c1 & 0b11000000u) != 0b10000000u || (c2 & 0b11000000u) != 0b10000000u || (c3 & 0b11000000u) != 0b10000000u) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Invalid continuation.
		}
		codePoint = ((c0 & 0b00000111u) << 18) | ((c1 & 0b00111111u) << 12) | ((c2 & 0b00111111u) << 6) | (c3 & 0b00111111u);
		if (codePoint < 65536) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Overlong sequence.
		}
		if (codePoint > 1114111) {
			[[unlikely]] return {CODE_POINT_ERROR, it}; // Invalid code point.
		}
	} else {
		[[unlikely]] return {CODE_POINT_ERROR, it}; // Invalid code unit.
	}
	return {codePoint, it};
}

/**
 * Encode a Unicode code point into a sequence of UTF-8 code units.
 *
 * \param codePoint code point to encode.
 *
 * \return a list of up to 4 UTF-8 code units.
 *
 * \note The returned array of code units is NOT guaranteed to be
 *       null-terminated. Its size method must be used to determine the actual
 *       length of the code point sequence.
 */
[[nodiscard]] constexpr InplaceBuffer<char8_t, 4> encodeUTF8FromCodePoint(char32_t codePoint) noexcept {
	if (codePoint <= 0x7F) {
		[[likely]] return {
			static_cast<char8_t>(codePoint),
		};
	}
	if (codePoint <= 0x7FF) {
		return {
			static_cast<char8_t>((codePoint >> 6) + 192),
			static_cast<char8_t>((codePoint & 63) + 128),
		};
	}
	if (codePoint <= 0xFFFF) {
		return {
			static_cast<char8_t>((codePoint >> 12) + 224),
			static_cast<char8_t>(((codePoint >> 6) & 63) + 128),
			static_cast<char8_t>((codePoint & 63) + 128),
		};
	}
	return {
		static_cast<char8_t>((codePoint >> 18) + 240),
		static_cast<char8_t>(((codePoint >> 12) & 63) + 128),
		static_cast<char8_t>(((codePoint >> 6) & 63) + 128),
		static_cast<char8_t>((codePoint & 63) + 128),
	};
}

/**
 * Sentinel type for UTF8Iterator.
 */
struct UTF8Sentinel {};

/**
 * Iterator type for decoding Unicode code points from a UTF-8 string, wrapping
 * an existing iterator for UTF-8 code units.
 *
 * \tparam BaseIterator underlying UTF-8 code unit iterator type to wrap.
 * \tparam BaseSentinel sentinel type for BaseIterator.
 */
template <typename BaseIterator, typename BaseSentinel = BaseIterator>
class UTF8Iterator {
public:
	using difference_type = ptrdiff_t;
	using value_type = char32_t;
	using reference = const value_type&;
	using pointer = const value_type*;
	using iterator_category = std::input_iterator_tag;
	using sentinel = UTF8Sentinel;

	constexpr UTF8Iterator() = default;

	constexpr UTF8Iterator(BaseIterator it, BaseSentinel end)
		: it(it)
		, end(end) {}

	[[nodiscard]] constexpr bool operator==(const UTF8Iterator& other) const {
		if (it == end || other.it == other.end) {
			return it == other.it && hasCodePoint == other.hasCodePoint;
		}
		ensureCodePoint();
		other.ensureCodePoint();
		return it == other.it;
	}

	[[nodiscard]] constexpr bool operator==(const UTF8Sentinel&) const {
		return it == end && !hasCodePoint;
	}

	[[nodiscard]] constexpr reference operator*() const {
		ensureCodePoint();
		return codePoint;
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return &**this;
	}

	constexpr UTF8Iterator& operator++() {
		if (!hasCodePoint) {
			const auto [newCodePoint, newIt] = decodeCodePointFromUTF8(it, end);
			it = newIt;
		}
		hasCodePoint = false;
		return *this;
	}

	constexpr UTF8Iterator operator++(int) {
		ensureCodePoint();
		UTF8Iterator old = *this;
		hasCodePoint = false;
		return old;
	}

private:
	void ensureCodePoint() const {
		if (!hasCodePoint) {
			const auto [newCodePoint, newIt] = decodeCodePointFromUTF8(it, end);
			codePoint = newCodePoint;
			hasCodePoint = true;
			it = newIt;
		}
	}

	mutable BaseIterator it{};
	BaseSentinel end{};
	mutable char32_t codePoint{};
	mutable bool hasCodePoint = false;
};

/**
 * Specialization of UTF8Iterator that provides forward iteration capability
 * (and an optimized implementation) for base iterators that support it.
 *
 * \tparam BaseIterator underlying UTF-8 code unit input iterator type to wrap.
 * \tparam BaseSentinel sentinel type for BaseIterator.
 */
template <typename BaseIterator, typename BaseSentinel>
requires forward_iterator<BaseIterator> //
class UTF8Iterator<BaseIterator, BaseSentinel> {
public:
	using difference_type = ptrdiff_t;
	using value_type = char32_t;
	using reference = const value_type&;
	using pointer = const value_type*;
	using iterator_category = std::forward_iterator_tag;
	using sentinel = UTF8Sentinel;

	constexpr UTF8Iterator() = default;

	constexpr UTF8Iterator(BaseIterator it, BaseSentinel end)
		: it(it)
		, next(it)
		, end(end) {
		++*this;
	}

	[[nodiscard]] constexpr bool operator==(const UTF8Iterator& other) const {
		return it == other.it;
	}

	[[nodiscard]] constexpr bool operator==(const UTF8Sentinel&) const {
		return it == end;
	}

	[[nodiscard]] constexpr reference operator*() const {
		return codePoint;
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return &**this;
	}

	constexpr UTF8Iterator& operator++() {
		it = next;
		const auto [newCodePoint, newNext] = decodeCodePointFromUTF8(next, end);
		codePoint = newCodePoint;
		next = newNext;
		return *this;
	}

	constexpr UTF8Iterator operator++(int) {
		UTF8Iterator old = *this;
		++*this;
		return old;
	}

	[[nodiscard]] constexpr BaseIterator base() const {
		return it;
	}

private:
	BaseIterator it{};
	BaseIterator next{};
	BaseSentinel end{};
	char32_t codePoint{};
};

/**
 * Non-owning view type for decoding Unicode code points from a contiguous UTF-8
 * string.
 */
class UTF8View {
public:
	using iterator = UTF8Iterator<const char8_t*>;
	using difference_type = typename iterator::difference_type;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using pointer = typename iterator::pointer;
	using iterator_category = typename iterator::iterator_category;
	using sentinel = typename iterator::sentinel;

	constexpr UTF8View() noexcept = default;

	constexpr explicit UTF8View(UTF8StringView str) noexcept
		: it(str.data(), str.data() + str.size()) {}

	explicit UTF8View(StringView str) noexcept
		: it(std::launder(reinterpret_cast<const char8_t*>(str.data())), std::launder(reinterpret_cast<const char8_t*>(str.data() + str.size()))) {
		static_assert(sizeof(char) == sizeof(char8_t));
		static_assert(alignof(char) == alignof(char8_t));
	}

	[[nodiscard]] constexpr const iterator& begin() const noexcept {
		return it;
	}

	[[nodiscard]] constexpr sentinel end() const noexcept { // NOLINT(readability-convert-member-functions-to-static)
		return {};
	}

private:
	UTF8Iterator<const char8_t*> it{};
};

} // namespace grem::unicode

#endif
