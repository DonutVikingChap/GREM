// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_C_STRING_VIEW_HPP
#define GREM_CORE_DATA_C_STRING_VIEW_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

#include <compare>  // std::weak_ordering
#include <cstddef>  // std::size_t, std::ptrdiff_t, std::nullptr_t
#include <iosfwd>   // std::basic_ostream
#include <iterator> // std::reverse_iterator
#include <limits>   // std::numeric_limits
#include <string>   // std::char_traits
#include <utility>  // std::swap

namespace grem {

namespace detail {

template <typename CharT, typename Traits>
struct CStringViewSentinel {
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const CharT* it) const {
		GREM_ASSERT(it);
		return Traits::eq(*it, CharT());
	}
};

} // namespace detail

template <typename CharT, typename Traits = std::char_traits<CharT>>
class CStringViewBase {
public:
	using traits_type = Traits;
	using value_type = CharT;
	using pointer = CharT*;
	using const_pointer = const CharT*;
	using reference = CharT&;
	using const_reference = const CharT&;
	using const_iterator = const_pointer;
	using iterator = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using reverse_iterator = const_reverse_iterator;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using sentinel = detail::CStringViewSentinel<CharT, Traits>;

	static constexpr size_type npos = static_cast<size_type>(-1);

	GREM_ALWAYS_INLINE constexpr CStringViewBase() noexcept = default;

	GREM_ALWAYS_INLINE constexpr CStringViewBase(const CharT* s)
		: string(s) {}

	template <typename Allocator>
	GREM_ALWAYS_INLINE constexpr CStringViewBase(const StringBase<CharT, Traits, Allocator>& s)
		: string(s.c_str()) {
		GREM_ASSERT(string);
	}

	GREM_ALWAYS_INLINE constexpr CStringViewBase(std::nullptr_t) = delete;

	GREM_ALWAYS_INLINE constexpr operator StringViewBase<CharT, Traits>() const {
		GREM_ASSERT(string);
		return StringViewBase<CharT, Traits>{string};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_iterator begin() const noexcept {
		return const_iterator{string};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr sentinel end() const noexcept {
		return sentinel{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr sentinel cend() const noexcept {
		return end();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator{const_iterator{string + size()}};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reverse_iterator crend() const noexcept {
		return rend();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(string);
		return string[pos];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reference at(size_type pos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).at(pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return string[0];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return string[size() - 1];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_pointer data() const noexcept {
		return string;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const_pointer c_str() const noexcept {
		return string;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type size() const noexcept {
		GREM_ASSERT(string);
		return Traits::length(string);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type length() const noexcept {
		return size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type max_size() const noexcept {
		constexpr size_type MAX_DIFFERENCE = static_cast<size_type>(std::numeric_limits<difference_type>::max());
		constexpr size_type MAX_COUNT = static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(value_type));
		return ((MAX_DIFFERENCE < MAX_COUNT) ? MAX_DIFFERENCE : MAX_COUNT) - 1;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool empty() const noexcept {
		GREM_ASSERT(string);
		return Traits::eq(string[0], CharT());
	}

	GREM_ALWAYS_INLINE constexpr void remove_prefix(size_type n) {
		GREM_ASSERT(string);
		string += n;
	}

	GREM_ALWAYS_INLINE constexpr void swap(CStringViewBase& v) noexcept {
		using std::swap;
		swap(string, v.string);
	}

	GREM_ALWAYS_INLINE constexpr size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).copy(dest, count, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr CStringViewBase substr(size_type pos = 0) const {
		GREM_ASSERT(string);
		return CStringViewBase{string + pos};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr StringViewBase<CharT, Traits> substr(size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).substr(pos, count);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(StringViewBase<CharT, Traits> v) const {
		return compareSubString(v.begin(), v.end());
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(const StringBase<CharT, Traits, Allocator>& v) const {
		return compareSubString(v.begin(), v.end());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(CStringViewBase v) const noexcept {
		return compareSubString(v.begin(), v.end());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(size_type pos1, size_type count1, CStringViewBase v) const {
		return substr(pos1, count1).compare(static_cast<StringViewBase<CharT, Traits>>(v));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(size_type pos1, size_type count1, CStringViewBase v, size_type pos2, size_type count2) const {
		return substr(pos1, count1).compare(v.substr(pos2, count2));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(const CharT* s) const {
		return compare(CStringViewBase{s});
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(size_type pos1, size_type count1, const CharT* s) const {
		return substr(pos1, count1).compare(StringViewBase<CharT, Traits>{s});
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int compare(size_type pos1, size_type count1, const CharT* s, size_type count2) const {
		return substr(pos1, count1).compare(StringViewBase<CharT, Traits>{s, count2});
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool starts_with(StringViewBase<CharT, Traits> sv) const noexcept {
		return startsWithSubString(sv.begin(), sv.end());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool starts_with(CStringViewBase sv) const noexcept {
		return startsWithSubString(sv.begin(), sv.end());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool starts_with(CharT ch) const noexcept {
		return startsWithCharacter(ch);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool starts_with(const CharT* s) const {
		return starts_with(CStringViewBase{s});
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool ends_with(StringViewBase<CharT, Traits> sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(sv);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool ends_with(const StringBase<CharT, Traits, Allocator>& s) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(s);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool ends_with(CStringViewBase sv) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(static_cast<StringViewBase<CharT, Traits>>(sv));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool ends_with(CharT ch) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(ch);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool ends_with(const CharT* s) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).ends_with(s);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(StringViewBase<CharT, Traits> sv) const noexcept {
		return find(sv) != npos;
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(const StringBase<CharT, Traits, Allocator>& s) const noexcept {
		return find(s) != npos;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(CStringViewBase sv) const noexcept {
		return find(sv) != npos;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(CharT c) const noexcept {
		return find(c) != npos;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(const CharT* s) const noexcept {
		return find(s) != npos;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(StringViewBase<CharT, Traits> sv, size_type pos = 0) const noexcept {
		return findSubString(sv.begin(), sv.end(), pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return findSubString(s.begin(), s.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(CStringViewBase sv, size_type pos = 0) const noexcept {
		return findSubString(sv.begin(), sv.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(CharT ch, size_type pos = 0) const noexcept {
		return findCharacter(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(const CharT* s, size_type pos, size_type count) const {
		return find(StringViewBase<CharT, Traits>{s, count}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find(const CharT* s, size_type pos = 0) const {
		return find(CStringViewBase{s}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(StringViewBase<CharT, Traits> sv, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(sv, pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(CStringViewBase sv, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(sv.c_str(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos, count);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type rfind(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).rfind(s, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(StringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return findFirstOfSubString(v.begin(), v.end(), pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return findFirstOfSubString(s.begin(), s.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(CStringViewBase v, size_type pos = 0) const noexcept {
		return findFirstOfSubString(v.begin(), v.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(CharT ch, size_type pos = 0) const noexcept {
		return find(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(const CharT* s, size_type pos, size_type count) const {
		return find_first_of(StringViewBase<CharT, Traits>{s, count}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_of(const CharT* s, size_type pos = 0) const {
		return find_first_of(CStringViewBase{s}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(StringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(CStringViewBase v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(v.c_str(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos, count);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_of(s, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(StringViewBase<CharT, Traits> v, size_type pos = 0) const noexcept {
		return findFirstNotOfSubString(v.begin(), v.end(), pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = 0) const noexcept {
		return findFirstNotOfSubString(s.begin(), s.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(CStringViewBase v, size_type pos = 0) const noexcept {
		return findFirstNotOfSubString(v.begin(), v.end(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept {
		return findFirstNotOfCharacter(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const {
		return find_first_not_of(StringViewBase<CharT, Traits>{s, count}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_first_not_of(const CharT* s, size_type pos = 0) const {
		return find_first_not_of(CStringViewBase{s}, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(StringViewBase<CharT, Traits> v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(v, pos);
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(const StringBase<CharT, Traits, Allocator>& s, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(CStringViewBase v, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(v.c_str(), pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(CharT ch, size_type pos = npos) const noexcept {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(ch, pos);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos, count);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type find_last_not_of(const CharT* s, size_type pos = npos) const {
		return static_cast<StringViewBase<CharT, Traits>>(*this).find_last_not_of(s, pos);
	}

	GREM_ALWAYS_INLINE friend constexpr void swap(CStringViewBase& a, CStringViewBase& b) noexcept {
		a.swap(b);
	}

	GREM_ALWAYS_INLINE friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& stream, CStringViewBase v) {
		return stream << v.c_str();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(CStringViewBase a, CStringViewBase b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(CStringViewBase a, StringViewBase<CharT, Traits> b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(StringViewBase<CharT, Traits> a, CStringViewBase b) noexcept {
		return a.compare(b) == 0;
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(CStringViewBase a, const StringBase<CharT, Traits, Allocator>& b) noexcept {
		return a.compare(b) == 0;
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(const StringBase<CharT, Traits, Allocator>& a, CStringViewBase b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(CStringViewBase a, const CharT* b) noexcept {
		return a.compare(b) == 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(const CharT* a, CStringViewBase b) noexcept {
		return CStringViewBase{a}.compare(b) == 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(CStringViewBase a, CStringViewBase b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(CStringViewBase a, StringViewBase<CharT, Traits> b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(StringViewBase<CharT, Traits> a, CStringViewBase b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(CStringViewBase a, const StringBase<CharT, Traits, Allocator>& b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	template <typename Allocator>
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(const StringBase<CharT, Traits, Allocator>& a, CStringViewBase b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(CStringViewBase a, const CharT* b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(a.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(a.compare(b) <=> 0);
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(const CharT* a, CStringViewBase b) noexcept {
		if constexpr (requires { typename Traits::comparison_category; }) {
			return static_cast<typename Traits::comparison_category>(CStringViewBase{a}.compare(b) <=> 0);
		} else {
			return static_cast<std::weak_ordering>(CStringViewBase{a}.compare(b) <=> 0);
		}
	}

private:
	[[nodiscard]] constexpr int compareSubString(auto first, auto last) const {
		GREM_ASSERT(string);
		const CharT* p = string;
		while (true) {
			if (Traits::eq(*p, CharT())) {
				return (first == last) ? 0 : -1;
			}
			if (first == last) {
				return (Traits::eq(*p, CharT())) ? 0 : 1;
			}
			if (!Traits::eq(*p, *first)) {
				return (Traits::lt(*p, *first)) ? -1 : 1;
			}
			++p;
			++first;
		}
		return 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool startsWithCharacter(CharT ch) const {
		GREM_ASSERT(string);
		return !Traits::eq(string[0], CharT()) && Traits::eq(string[0], ch);
	}

	[[nodiscard]] constexpr bool startsWithSubString(auto first, auto last) const {
		GREM_ASSERT(string);
		const CharT* p = string;
		while (true) {
			if (first == last) {
				return true;
			}
			if (Traits::eq(*p, CharT()) || !Traits::eq(*p, *first)) {
				return false;
			}
			++p;
			++first;
		}
		return false;
	}

	[[nodiscard]] constexpr size_type findCharacter(CharT ch, size_type pos) const {
		GREM_ASSERT(string);
		while (!Traits::eq(string[pos], CharT())) {
			if (Traits::eq(string[pos], ch)) {
				return pos;
			}
			++pos;
		}
		return npos;
	}

	[[nodiscard]] constexpr size_type findSubString(auto first, auto last, size_type pos) const {
		GREM_ASSERT(string);
		while (true) {
			size_type subPos = pos;
			auto it = first;
			while (true) {
				if (Traits::eq(string[subPos], CharT())) {
					return (it == last) ? pos : npos;
				}
				if (it == last) {
					return pos;
				}
				if (!Traits::eq(string[subPos], *it)) {
					break;
				}
				++subPos;
				++it;
			}
			++pos;
		}
		return npos;
	}

	[[nodiscard]] constexpr size_type findFirstOfSubString(auto first, auto last, size_type pos) const {
		GREM_ASSERT(string);
		while (!Traits::eq(string[pos], CharT())) {
			const CharT ch = string[pos];
			for (auto it = first; it != last; ++it) {
				if (Traits::eq(ch, *it)) {
					return pos;
				}
			}
			++pos;
		}
		return npos;
	}

	[[nodiscard]] constexpr size_type findFirstNotOfCharacter(CharT ch, size_type pos) const {
		GREM_ASSERT(string);
		while (!Traits::eq(string[pos], CharT())) {
			if (!Traits::eq(string[pos], ch)) {
				return pos;
			}
			++pos;
		}
		return npos;
	}

	[[nodiscard]] constexpr size_type findFirstNotOfSubString(auto first, auto last, size_type pos) const {
		GREM_ASSERT(string);
		while (!Traits::eq(string[pos], CharT())) {
			const CharT ch = string[pos];
			auto it = first;
			for (; it != last; ++it) {
				if (Traits::eq(ch, *it)) {
					break;
				}
			}
			if (it == last) {
				return pos;
			}
			++pos;
		}
		return npos;
	}

	static constexpr CharT EMPTY_STRING_DATA = CharT();

	const_pointer string = &EMPTY_STRING_DATA;
};

using CStringView = CStringViewBase<char, std::char_traits<char>>;
using UTF8CStringView = CStringViewBase<char8_t, std::char_traits<char8_t>>;
using UTF16CStringView = CStringViewBase<char16_t, std::char_traits<char16_t>>;
using UTF32CStringView = CStringViewBase<char32_t, std::char_traits<char32_t>>;
using WCStringView = CStringViewBase<wchar_t, std::char_traits<wchar_t>>;

} // namespace grem

template <typename CharT, typename Traits>
struct std::hash<grem::CStringViewBase<CharT, Traits>> {
	[[nodiscard]] std::size_t operator()(const grem::CStringViewBase<CharT, Traits>& sv) const {
		return hasher(static_cast<grem::StringViewBase<CharT, Traits>>(sv));
	}

private:
	[[no_unique_address]] std::hash<grem::StringViewBase<CharT, Traits>> hasher;
};

#endif
