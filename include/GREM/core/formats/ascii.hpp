// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_ASCII_HPP
#define GREM_CORE_FORMATS_ASCII_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <limits> // std::numeric_limits

namespace grem::ascii {

/**
 * Check if a character value represents a valid character in the ASCII table
 * (values 0-127).
 *
 * \param ch character to check.
 *
 * \return true if the character is valid, false otherwise.
 *
 * \note The null character ('\0') is considered valid.
 */
[[nodiscard]] constexpr bool isValidCharacter(char ch) noexcept {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtype-limits"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
	if constexpr (std::numeric_limits<char>::is_signed) {
		return ch >= 0;
	} else {
		return ch <= 127;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

/**
 * Check if a character value represents a control character in the ASCII table
 * (values 0-31 and 127).
 *
 * \param ch character to check.
 *
 * \return true if the character is an ASCII control character, false otherwise.
 */
[[nodiscard]] constexpr bool isControlCharacter(char ch) noexcept {
	if constexpr (std::numeric_limits<char>::is_signed) {
		return ch >= 0 && (ch <= 31 || ch == 127);
	} else {
		return ch <= 31 || ch == 127;
	}
}

/**
 * Check if a character value represents a printable character in the ASCII
 * table (values 32-126).
 *
 * \param ch character to check.
 *
 * \return true if the character is a printable ASCII character, false
 *         otherwise.
 *
 * \note Control characters, like horizontal tab ('\\t') or line feed ('\\n'), are
 *       NOT considered printable. However, space (' ') is.
 */
[[nodiscard]] constexpr bool isPrintableCharacter(char ch) noexcept {
	return ch >= 32 && ch <= 126;
}

/**
 * Check if a character is an uppercase ASCII letter (A-Z).
 *
 * \param ch character to check.
 *
 * \return true if the character is an uppercase ASCII letter, false otherwise.
 */
[[nodiscard]] constexpr bool isUppercase(char ch) noexcept {
	return ch >= 'A' && ch <= 'Z';
}

/**
 * Check if a character is a lowercase ASCII letter (a-z).
 *
 * \param ch character to check.
 *
 * \return true if the character is a lowercase ASCII letter, false otherwise.
 */
[[nodiscard]] constexpr bool isLowercase(char ch) noexcept {
	return ch >= 'a' && ch <= 'z';
}

/**
 * Check if a character is an ASCII letter (A-Z or a-z).
 *
 * \param ch character to check.
 *
 * \return true if the character is an ASCII letter, false otherwise.
 */
[[nodiscard]] constexpr bool isLetter(char ch) noexcept {
	return isUppercase(ch) || isLowercase(ch);
}

/**
 * Check if a character is an ASCII decimal numeric digit (0-9).
 *
 * \param ch character to check.
 *
 * \return true if the character is an ASCII decimal digit, false otherwise.
 */
[[nodiscard]] constexpr bool isDecimalDigit(char ch) noexcept {
	return ch >= '0' && ch <= '9';
}

/**
 * Convert an uppercase ASCII letter (A-Z) to its corresponding lowercase ASCII
 * letter (a-z).
 *
 * \param ch character to convert. Must be a valid uppercase ASCII letter.
 *
 * \return the lowercase version of the given uppercase letter.
 */
[[nodiscard]] constexpr char convertUppercaseToLowercaseCharacter(char ch) {
	GREM_ASSERT(isUppercase(ch));
	return static_cast<char>(ch + ('a' - 'A'));
}

/**
 * Convert a lowercase ASCII letter (a-z) to its corresponding uppercase ASCII
 * letter (A-Z).
 *
 * \param ch character to convert. Must be a valid lowercase ASCII letter.
 *
 * \return the uppercase version of the given lowercase letter.
 */
[[nodiscard]] constexpr char convertLowercaseToUppercaseCharacter(char ch) {
	GREM_ASSERT(isLowercase(ch));
	return static_cast<char>(ch - ('a' - 'A'));
}

/**
 * Convert a character to its corresponding lowercase ASCII letter (a-z) if it
 * has one.
 *
 * \param ch character to convert.
 *
 * \return the lowercase version of the given character if it is an uppercase
 *         ASCII letter, or a copy of the given character otherwise.
 */
[[nodiscard]] constexpr char convertToLowercaseCharacter(char ch) noexcept {
	return (isUppercase(ch)) ? convertUppercaseToLowercaseCharacter(ch) : ch;
}

/**
 * Convert a character to its corresponding uppercase ASCII letter (A-Z) if it
 * has one.
 *
 * \param ch character to convert.
 *
 * \return the uppercase version of the given character if it is a lowercase
 *         ASCII letter, or a copy of the given character otherwise.
 */
[[nodiscard]] constexpr char convertToUppercaseCharacter(char ch) noexcept {
	return (isLowercase(ch)) ? convertLowercaseToUppercaseCharacter(ch) : ch;
}

/**
 * Convert a character to its opposite case if it is an ASCII letter.
 *
 * \param ch character to convert.
 *
 * \return the lowercase version of the given character if it is an uppercase
 *         ASCII letter, or the uppercase version of the given character if it
 *         is a lowercase ASCII letter, or a copy of the given character
 *         otherwise.
 */
[[nodiscard]] constexpr char convertToOppositeCaseCharacter(char ch) noexcept {
	if (isUppercase(ch)) {
		return convertUppercaseToLowercaseCharacter(ch);
	}
	if (isLowercase(ch)) {
		return convertLowercaseToUppercaseCharacter(ch);
	}
	return ch;
}

/**
 * Convert each character in a string to its corresponding lowercase ASCII
 * letter (a-z) if it has one.
 *
 * \param string string to convert.
 *
 * \return a string containing the lowercase version of each character in the
 *         given string that is an uppercase ASCII letter, or a copy for each
 *         character that isn't.
 *
 * \throws std::length_error if the length of the given string exceeds the
 *         maximum supported allocated string size.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] constexpr String convertToLowercaseString(StringView string) {
	String result{};
	result.resize(string.size());
	for (size_t i = 0; i < string.size(); ++i) {
		result[i] = convertToLowercaseCharacter(string[i]);
	}
	return result;
}

/**
 * Convert each character in a string to its corresponding uppercase ASCII
 * letter (A-Z) if it has one.
 *
 * \param string string to convert.
 *
 * \return a string containing the uppercase version of each character in the
 *         given string that is a lowercase ASCII letter, or a copy for each
 *         character that isn't.
 *
 * \throws std::length_error if the length of the given string exceeds the
 *         maximum supported allocated string size.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] constexpr String convertToUppercaseString(StringView string) {
	String result{};
	result.resize(string.size());
	for (size_t i = 0; i < string.size(); ++i) {
		result[i] = convertToUppercaseCharacter(string[i]);
	}
	return result;
}

/**
 * Check if two characters are equal if any ASCII letters are treated as the
 * same case, i.e. ignoring whether they are lowercase (a-z) or uppercase (A-Z).
 *
 * \param a first character.
 * \param b second character.
 *
 * \return true if the given characters are equivalent when ignoring case, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool caseInsensitiveEqual(char a, char b) noexcept {
	return convertToLowercaseCharacter(a) == convertToLowercaseCharacter(b);
}

/**
 * Check if two null-terminated strings are equal if any ASCII letters are
 * treated as the same case, i.e. ignoring whether they are lowercase (a-z) or
 * uppercase (A-Z).
 *
 * \param a first string. Must be null-terminated. Must not be nullptr.
 * \param b second string. Must be null-terminated. Must not be nullptr.
 *
 * \return true if the given strings are equivalent when ignoring case, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool caseInsensitiveEqual(const char* a, const char* b) {
	GREM_ASSERT(a);
	GREM_ASSERT(b);
	while (*a != '\0' && *b != '\0') {
		if (!caseInsensitiveEqual(*a, *b)) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

/**
 * Check if two strings are equal if any ASCII letters are treated as the same
 * case, i.e. ignoring whether they are lowercase (a-z) or uppercase (A-Z).
 *
 * \param a first string.
 * \param b second string.
 *
 * \return true if the given strings are equivalent when ignoring case, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool caseInsensitiveEqual(StringView a, StringView b) {
	if (a.size() != b.size()) {
		return false;
	}
	const char* pA = a.data();
	const char* pB = b.data();
	const char* const endA = pA + a.size();
	while (pA != endA) {
		if (!caseInsensitiveEqual(*pA, *pB)) {
			return false;
		}
		++pA;
		++pB;
	}
	return true;
}

} // namespace grem::ascii

#endif
