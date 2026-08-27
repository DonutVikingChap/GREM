// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_CONSTANT_STRING_HPP
#define GREM_CORE_DATA_CONSTANT_STRING_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

#include <algorithm>   // std::equal, std::lexicographical_compare
#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <iosfwd>      // std::basic_ostream
#include <iterator>    // std::size, std::reverse_iterator
#include <string>      // std::char_traits
#include <type_traits> // std::remove_cvref_t, std::integral_constant

namespace grem {

namespace detail {

template <typename T>
struct constant_string_length;

template <typename T>
inline constexpr std::size_t constant_string_length_v = constant_string_length<T>::value;

template <std::size_t N>
struct constant_string_length<char[N]> : std::integral_constant<std::size_t, N - 1> {};

template <std::size_t N>
struct constant_string_length<const char[N]> : std::integral_constant<std::size_t, N - 1> {};

} // namespace detail

template <typename CharT, std::size_t Length>
class ConstantString {
private:
	using Traits = std::char_traits<CharT>;

public:
	using value_type = CharT;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = CharT*;
	using const_pointer = const CharT*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	static constexpr size_type npos = static_cast<size_type>(-1);

	constexpr ConstantString() noexcept = default;

	template <typename StringLike>
	constexpr ConstantString(const StringLike& str) noexcept {
		GREM_ASSERT(std::size(str) >= Length);
		for (size_type i = 0; i < Length; ++i) {
			characters[i] = str[i];
		}
	}

	constexpr operator StringViewBase<CharT, Traits>() const noexcept {
		return StringViewBase<CharT, Traits>{data(), size()};
	}

	constexpr operator CStringViewBase<CharT, Traits>() const noexcept {
		return CStringViewBase<CharT, Traits>{c_str()};
	}

	[[nodiscard]] constexpr pointer data() noexcept {
		return characters;
	}

	[[nodiscard]] constexpr const_pointer data() const noexcept {
		return characters;
	}

	[[nodiscard]] constexpr const_pointer c_str() const noexcept {
		return characters;
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) noexcept {
		GREM_ASSERT(pos < size());
		return characters[pos];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
		GREM_ASSERT(pos < size());
		return characters[pos];
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		return static_cast<StringViewBase<CharT, Traits>>(*this).at(pos);
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).at(pos);
	}

	[[nodiscard]] constexpr reference front() {
		static_assert(Length > 0);
		return characters[0];
	}

	[[nodiscard]] constexpr const_reference front() const {
		static_assert(Length > 0);
		return characters[0];
	}

	[[nodiscard]] constexpr reference back() {
		static_assert(Length > 0);
		return characters[Length - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		static_assert(Length > 0);
		return characters[Length - 1];
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return Length;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return Length;
	}

	[[nodiscard]] constexpr size_type length() const noexcept {
		return Length;
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return Length == 0;
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return data();
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return data();
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return data() + Length;
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return data() + Length;
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

	[[nodiscard]] constexpr bool operator==(const ConstantString& other) const noexcept {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const ConstantString& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const ConstantString& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const ConstantString& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const ConstantString& other) const {
		return !(*this < other);
	}

	constexpr size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).copy(dest, count, pos);
	}

	[[nodiscard]] constexpr StringViewBase<CharT, Traits> substr(size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).substr(pos);
	}

	[[nodiscard]] constexpr StringViewBase<CharT, Traits> substr(size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).substr(pos, count);
	}

	[[nodiscard]] constexpr int compare(StringViewBase<CharT, Traits> v) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(v);
	}

	[[nodiscard]] constexpr int compare(CStringViewBase<CharT, Traits> v) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(v);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr int compare(const StringBase<CharT, Traits, Allocator>& v) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(v);
	}

	[[nodiscard]] constexpr int compare(const ConstantString& v) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(v);
	}

	[[nodiscard]] constexpr int compare(size_type pos1, size_type count1, const ConstantString& v) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(pos1, count1, v);
	}

	[[nodiscard]] constexpr int compare(size_type pos1, size_type count1, const ConstantString& v, size_type pos2, size_type count2) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(pos1, count1, v, pos2, count2);
	}

	[[nodiscard]] constexpr int compare(const CharT* s) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(s);
	}

	[[nodiscard]] constexpr int compare(size_type pos1, size_type count1, const CharT* s) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(pos1, count1, s);
	}

	[[nodiscard]] constexpr int compare(size_type pos1, size_type count1, const CharT* s, size_type count2) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).compare(pos1, count1, s, count2);
	}

	[[nodiscard]] constexpr bool starts_with(StringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).starts_with(sv);
	}

	[[nodiscard]] constexpr bool starts_with(CStringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).starts_with(sv);
	}

	[[nodiscard]] constexpr bool starts_with(const ConstantString& sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).starts_with(sv);
	}

	[[nodiscard]] constexpr bool starts_with(CharT ch) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).starts_with(ch);
	}

	[[nodiscard]] constexpr bool starts_with(const CharT* s) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).starts_with(s);
	}

	[[nodiscard]] constexpr bool ends_with(StringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(sv);
	}

	[[nodiscard]] constexpr bool ends_with(CStringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(sv);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr bool ends_with(const StringBase<CharT, Traits, Allocator>& s) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(s);
	}

	[[nodiscard]] constexpr bool ends_with(const ConstantString& sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(sv);
	}

	[[nodiscard]] constexpr bool ends_with(CharT ch) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(ch);
	}

	[[nodiscard]] constexpr bool ends_with(const CharT* s) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(s);
	}

	[[nodiscard]] constexpr bool contains(StringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(sv);
	}

	[[nodiscard]] constexpr bool contains(CStringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(sv);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr bool contains(const StringBase<CharT, Traits, Allocator>& s) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(s);
	}

	[[nodiscard]] constexpr bool contains(const ConstantString& sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(sv);
	}

	[[nodiscard]] constexpr bool contains(CharT c) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(c);
	}

	[[nodiscard]] constexpr bool contains(const CharT* s) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).contains(s);
	}

	[[nodiscard]] constexpr size_type find(StringViewBase<CharT, Traits> sv, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(sv, pos);
	}

	[[nodiscard]] constexpr size_type find(CStringViewBase<CharT, Traits> sv, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(sv, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type find(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(s, pos);
	}

	[[nodiscard]] constexpr size_type find(const ConstantString& sv, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(sv, pos);
	}

	[[nodiscard]] constexpr size_type find(CharT ch, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(ch, pos);
	}

	[[nodiscard]] constexpr size_type find(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(s, pos, count);
	}

	[[nodiscard]] constexpr size_type find(const CharT* s, size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find(s, pos);
	}

	[[nodiscard]] constexpr size_type rfind(StringViewBase<CharT, Traits> sv, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(sv, pos);
	}

	[[nodiscard]] constexpr size_type rfind(CStringViewBase<CharT, Traits> sv, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(sv, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type rfind(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos);
	}

	[[nodiscard]] constexpr size_type rfind(const ConstantString& sv, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(sv, pos);
	}

	[[nodiscard]] constexpr size_type rfind(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(ch, pos);
	}

	[[nodiscard]] constexpr size_type rfind(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos, count);
	}

	[[nodiscard]] constexpr size_type rfind(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos);
	}

	[[nodiscard]] constexpr size_type find_first_of(StringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_first_of(CStringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type find_first_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_first_of(const ConstantString& v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_first_of(CharT ch, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(ch, pos);
	}

	[[nodiscard]] constexpr size_type find_first_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(s, pos, count);
	}

	[[nodiscard]] constexpr size_type find_first_of(const CharT* s, size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_last_of(StringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_last_of(CStringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type find_last_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_last_of(const ConstantString& v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_last_of(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(ch, pos);
	}

	[[nodiscard]] constexpr size_type find_last_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos, count);
	}

	[[nodiscard]] constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(StringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(CStringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type find_first_not_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(const ConstantString& v, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(ch, pos);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(s, pos, count);
	}

	[[nodiscard]] constexpr size_type find_first_not_of(const CharT* s, size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_first_not_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(StringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(CStringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] constexpr size_type find_last_not_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(const ConstantString& v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(v, pos);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(ch, pos);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos, count);
	}

	[[nodiscard]] constexpr size_type find_last_not_of(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos);
	}

	friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& stream, const ConstantString& v) {
		return stream << v.c_str();
	}

	[[nodiscard]] friend constexpr bool operator==(const ConstantString& a, const ConstantString& b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] friend constexpr bool operator==(const ConstantString& a, StringViewBase<CharT, Traits> b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] friend constexpr bool operator==(StringViewBase<CharT, Traits> a, const ConstantString& b) noexcept {
		return b == a;
	}

	[[nodiscard]] friend constexpr bool operator==(const ConstantString& a, CStringViewBase<CharT, Traits> b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] friend constexpr bool operator==(CStringViewBase<CharT, Traits> a, const ConstantString& b) noexcept {
		return b == a;
	}

	template <typename Allocator>
	[[nodiscard]] friend constexpr bool operator==(const ConstantString& a, const StringBase<CharT, Traits, Allocator>& b) noexcept {
		return a.compare(b) == 0;
	}

	template <typename Allocator>
	[[nodiscard]] friend constexpr bool operator==(const StringBase<CharT, Traits, Allocator>& a, const ConstantString& b) noexcept {
		return b == a;
	}

	[[nodiscard]] friend constexpr bool operator==(const ConstantString& a, const CharT* b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] friend constexpr bool operator==(const CharT* a, const ConstantString& b) noexcept {
		return b == a;
	}

	[[nodiscard]] friend constexpr auto operator<=>(const ConstantString& a, const ConstantString& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(const ConstantString& a, StringViewBase<CharT, Traits> b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(StringViewBase<CharT, Traits> a, const ConstantString& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(const ConstantString& a, CStringViewBase<CharT, Traits> b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(CStringViewBase<CharT, Traits> a, const ConstantString& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	template <typename Allocator>
	[[nodiscard]] friend constexpr auto operator<=>(const ConstantString& a, const StringBase<CharT, Traits, Allocator>& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	template <typename Allocator>
	[[nodiscard]] friend constexpr auto operator<=>(const StringBase<CharT, Traits, Allocator>& a, const ConstantString& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(const ConstantString& a, const CharT* b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] friend constexpr auto operator<=>(const CharT* a, const ConstantString& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(CStringViewBase{a}.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(CStringViewBase{a}.compare(b) <=> 0);
		}
	}

	char characters[Length + 1]{};
};

namespace detail {

template <typename CharT, std::size_t Length>
struct constant_string_length<ConstantString<CharT, Length>> : std::integral_constant<std::size_t, Length> {};

template <typename CharT, std::size_t Length>
struct constant_string_length<const ConstantString<CharT, Length>> : std::integral_constant<std::size_t, Length> {};

} // namespace detail

template <typename StringLike>
ConstantString(const StringLike& str) -> ConstantString<std::remove_cvref_t<decltype(str[0])>, detail::constant_string_length_v<StringLike>>;

namespace detail {

template <typename CharT, typename... StringLikes>
constexpr auto constantConcat(const StringLikes&... strs) {
	ConstantString<CharT, (detail::constant_string_length_v<StringLikes> + ...)> result{};
	std::size_t offset = 0;
	(
		[&]<typename StringLike>(const StringLike& str) {
			std::size_t i = 0;
			while (i < detail::constant_string_length_v<StringLike>) {
				result[offset++] = str[i++];
			}
		}(strs),
		...);
	return result;
}

} // namespace detail

template <typename CharT, std::size_t LengthA, std::size_t LengthB>
constexpr auto operator+(const ConstantString<CharT, LengthA>& a, const ConstantString<CharT, LengthB>& b) {
	return detail::constantConcat<CharT>(a, b);
}

template <typename CharT, std::size_t LengthA, std::size_t N>
constexpr auto operator+(const ConstantString<CharT, LengthA>& a, const CharT (&b)[N]) {
	return detail::constantConcat<CharT>(a, b);
}

template <typename CharT, std::size_t N, std::size_t LengthB>
constexpr auto operator+(const CharT (&a)[N], const ConstantString<CharT, LengthB>& b) {
	return detail::constantConcat<CharT>(a, b);
}

} // namespace grem

#endif
