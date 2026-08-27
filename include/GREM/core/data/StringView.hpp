// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_STRING_VIEW_HPP
#define GREM_CORE_DATA_STRING_VIEW_HPP

#include <GREM/build_config.hpp>

#include <string_view> // std::basic_string_view, std::char_traits

namespace grem {

template <typename CharT, typename Traits = std::char_traits<CharT>>
using StringViewBase = std::basic_string_view<CharT, Traits>;

using StringView = StringViewBase<char, std::char_traits<char>>;
using UTF8StringView = StringViewBase<char8_t, std::char_traits<char8_t>>;
using UTF16StringView = StringViewBase<char16_t, std::char_traits<char16_t>>;
using UTF32StringView = StringViewBase<char32_t, std::char_traits<char32_t>>;
using WStringView = StringViewBase<wchar_t, std::char_traits<wchar_t>>;

} // namespace grem

#endif
