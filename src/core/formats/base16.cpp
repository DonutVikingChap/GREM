// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/base16.hpp>
#include <GREM/core/fundamentals.hpp>

#include <stdexcept> // std::invalid_argument

namespace grem::base16 {

String encodeLowercase(StringView data) {
	constexpr CStringView HEXADECIMAL_DIGITS = "0123456789abcdef";
	String result{};
	result.reserve(data.size() * 2);
	for (const char ch : data) {
		result.push_back(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) >> 4) & 0x0F]);
		result.push_back(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) & 0x0F)]);
	}
	return result;
}

String encodeUppercase(StringView data) {
	constexpr CStringView HEXADECIMAL_DIGITS = "0123456789ABCDEF";
	String result{};
	result.reserve(data.size() * 2);
	for (const char ch : data) {
		result.push_back(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) >> 4) & 0x0F]);
		result.push_back(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) & 0x0F)]);
	}
	return result;
}

String decode(StringView string) {
	const size_t size = string.size();
	if (size % 2 != 0) {
		throw std::invalid_argument{"Invalid Base16 string length."};
	}
	String result{};
	for (size_t i = 0; i < size;) {
		const int highNibble = getHexadecimalDigitValue(string[i++]).value_or(0);
		const int lowNibble = getHexadecimalDigitValue(string[i++]).value_or(0);
		result.push_back(static_cast<char>((highNibble << 4) | lowNibble));
	}
	return result;
}

} // namespace grem::base16
