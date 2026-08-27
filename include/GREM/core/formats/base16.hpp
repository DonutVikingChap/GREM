// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_BASE16_HPP
#define GREM_CORE_FORMATS_BASE16_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::base16 {

/**
 * Check if a character value represents a valid hexadecimal Base16 character
 * (ASCII values '0'-'9', 'A'-'F' or 'a'-'f').
 *
 * \param ch character to check.
 *
 * \return true if the character is valid, false otherwise.
 */
[[nodiscard]] constexpr bool isHexadecimalDigit(char ch) noexcept {
	return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

/**
 * Get the hexadecimal digit value of a Base16 character.
 * (ASCII values '0'-'9', 'A'-'F' or 'a'-'f').
 *
 * \param ch character to get the value of.
 *
 * \return an integer value between 0 and 15 (inclusive), or an empty optional
 *         if the given character was invalid.
 */
[[nodiscard]] constexpr Optional<int> getHexadecimalDigitValue(char ch) noexcept {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'A' && ch <= 'F') {
		return 10 + ch - 'A';
	}
	if (ch >= 'a' && ch <= 'f') {
		return 10 + ch - 'a';
	}
	return {};
}

/**
 * Encode arbitrary data into a lowercase hexadecimal Base16 string.
 *
 * \param data input data to encode.
 *
 * \return a Base16 string containing the encoded representation of the input.
 *
 * \throws std::length_error if the length of the encoded string exceeds the
 *         maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) String encodeLowercase(StringView data);

/**
 * Encode arbitrary data into an uppercase hexadecimal Base16 string.
 *
 * \param data input data to encode.
 *
 * \return a Base16 string containing the encoded representation of the input.
 *
 * \throws std::length_error if the length of the encoded string exceeds the
 *         maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) String encodeUppercase(StringView data);

/**
 * Decode the original data from a lowercase or uppercase hexadecimal Base16
 * string.
 *
 * \param string Base16 string to decode.
 *
 * \return a byte sequence containing the data decoded from the Base16 string.
 *
 * \throws std::invalid_argument if the length of the Base16 string is not
 *         divisible by 2.
 * \throws std::length_error if the length of the decoded byte sequence exceeds
 *         the maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) String decode(StringView string);

} // namespace grem::base16

#endif
