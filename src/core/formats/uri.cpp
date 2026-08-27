// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/base16.hpp>
#include <GREM/core/formats/uri.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::uri {

String percentEncode(StringView data) {
	constexpr CStringView ALLOWED_SPECIAL_CHARACTERS = "-_.~!#$&'()*+,/:;=?@[]";
	constexpr CStringView HEXADECIMAL_CHARACTERS = "0123456789ABCDEF";
	String result{};
	for (const char ch : data) {
		if (base16::isHexadecimalDigit(ch) || ALLOWED_SPECIAL_CHARACTERS.contains(ch)) {
			result.push_back(ch);
		} else {
			result.push_back('%');
			result.push_back(HEXADECIMAL_CHARACTERS[bit_cast<uint8_t>(ch) >> 4]);
			result.push_back(HEXADECIMAL_CHARACTERS[bit_cast<uint8_t>(ch) & 0x0F]);
		}
	}
	return result;
}

String percentDecode(StringView string) {
	String result{};
	for (auto it = string.begin(); it != string.end();) {
		char ch = *it++;
		if (ch == '%' && it != string.end() && it + 1 != string.end()) {
			const int highNibble = base16::getHexadecimalDigitValue(*it++).value_or(0);
			const int lowNibble = base16::getHexadecimalDigitValue(*it++).value_or(0);
			ch = static_cast<char>((highNibble << 4) | lowNibble);
		}
		result.push_back(ch);
	}
	return result;
}

} // namespace grem::uri
