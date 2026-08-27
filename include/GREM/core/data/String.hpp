// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_STRING_HPP
#define GREM_CORE_DATA_STRING_HPP

#include <GREM/build_config.hpp>

#include <memory_resource> // std::pmr::polymorphic_allocator
#include <string>          // std::basic_string, std::char_traits, std::allocator, std::to_string

namespace grem {

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
using StringBase = std::basic_string<CharT, Traits, Allocator>;

using String = StringBase<char, std::char_traits<char>, std::allocator<char>>;
using UTF8String = StringBase<char8_t, std::char_traits<char8_t>, std::allocator<char8_t>>;
using UTF16String = StringBase<char16_t, std::char_traits<char16_t>, std::allocator<char16_t>>;
using UTF32String = StringBase<char32_t, std::char_traits<char32_t>, std::allocator<char32_t>>;
using WString = StringBase<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>>;

[[nodiscard]] inline String toString(int value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(long value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(long long value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(unsigned value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(unsigned long value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(unsigned long long value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(float value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(double value) {
	return std::to_string(value);
}

[[nodiscard]] inline String toString(long double value) {
	return std::to_string(value);
}

} // namespace grem

namespace grem::pmr {

template <typename CharT, typename Traits = std::char_traits<CharT>>
using StringBase = grem::StringBase<CharT, Traits, std::pmr::polymorphic_allocator<CharT>>;

using String = StringBase<char, std::char_traits<char>>;
using UTF8String = StringBase<char8_t, std::char_traits<char8_t>>;
using UTF16String = StringBase<char16_t, std::char_traits<char16_t>>;
using UTF32String = StringBase<char32_t, std::char_traits<char32_t>>;
using WString = StringBase<wchar_t, std::char_traits<wchar_t>>;

} // namespace grem::pmr

#endif
