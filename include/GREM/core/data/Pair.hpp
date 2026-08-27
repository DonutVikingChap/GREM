// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_PAIR_HPP
#define GREM_CORE_DATA_PAIR_HPP

#include <GREM/build_config.hpp>

#include <compare>     // std::three_way_comparable_with, std::weak_ordering, std::common_comparison_category_t
#include <cstddef>     // std::size_t
#include <tuple>       // std::tuple
#include <type_traits> // std::is_..._v, std::basic_common_reference, std::common_reference_t
#include <utility>     // std::piecewise_construct_t, std::index_sequence, std::make_index_sequence, std::forward, std::swap

namespace grem {

namespace detail {

template <typename T>
concept default_copy_list_initializable = std::is_default_constructible_v<T> && requires(void (*f)(T)) { f({}); };

template <typename B>
concept boolean_testable_implementation = std::is_convertible_v<B, bool>;

template <typename B>
concept boolean_testable = boolean_testable_implementation<B> && requires(B&& b) {
	{ !std::forward<B>(b) } -> boolean_testable_implementation;
};

constexpr auto synthThreeWay = []<typename T, typename U>(const T& t, const U& u) requires(requires {
	{ t < u } -> boolean_testable;
	{ u < t } -> boolean_testable;
}) {
	if constexpr (std::three_way_comparable_with<T, U>) {
		return t <=> u;
	} else {
		if (t < u) {
			return std::weak_ordering::less;
		}
		if (u < t) {
			return std::weak_ordering::greater;
		}
		return std::weak_ordering::equivalent;
	}
};

template <typename T, typename U = T>
using SynthThreeWayResult = decltype(synthThreeWay(std::declval<T&>(), std::declval<U&>()));

} // namespace detail

template <typename T1, typename T2 = T1>
struct Pair {
	using first_type = T1;
	using second_type = T2;

	T1 first;
	T2 second;

	constexpr explicit(!detail::default_copy_list_initializable<T1> || !detail::default_copy_list_initializable<T2>) Pair()
		requires(std::is_default_constructible_v<T1> && std::is_default_constructible_v<T2>)
		: first()
		, second() {}

	constexpr explicit(!std::is_convertible_v<const T1&, T1> || !std::is_convertible_v<const T2&, T2>) Pair(const T1& x, const T2& y)
		requires(std::is_copy_constructible_v<T1> && std::is_copy_constructible_v<T2>)
		: first(x)
		, second(y) {}

	template <typename U1, typename U2>
	constexpr explicit(!std::is_convertible_v<U1, T1> || !std::is_convertible_v<U2, T2>) Pair(U1&& x, U2&& y)
		requires(std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>)
		: first(std::forward<U1>(x))
		, second(std::forward<U2>(y)) {}

	template <typename U1, typename U2>
	constexpr explicit(!std::is_convertible_v<U1&, T1> || !std::is_convertible_v<U2&, T2>) Pair(Pair<U1, U2>& p)
		requires(std::is_constructible_v<T1, U1&> && std::is_constructible_v<T2, U2&>)
		: first(p.first)
		, second(p.second) {}

	template <typename U1, typename U2>
	constexpr explicit(!std::is_convertible_v<const U1&, T1> || !std::is_convertible_v<const U2&, T2>) Pair(const Pair<U1, U2>& p)
		requires(std::is_constructible_v<T1, const U1&> && std::is_constructible_v<T2, const U2&>)
		: first(p.first)
		, second(p.second) {}

	template <typename U1, typename U2>
	constexpr explicit(!std::is_convertible_v<U1, T1> || !std::is_convertible_v<U2, T2>) Pair(Pair<U1, U2>&& p) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(std::is_constructible_v<T1, U1> && std::is_constructible_v<T2, U2>)
		: first(std::forward<U1>(p.first))
		, second(std::forward<U2>(p.second)) {}

	template <typename... Args1, typename... Args2>
	constexpr Pair(std::piecewise_construct_t piecewiseConstruct, std::tuple<Args1...> firstArgs, std::tuple<Args2...> secondArgs)
		: Pair(piecewiseConstruct, firstArgs, std::make_index_sequence<sizeof...(Args1)>{}, secondArgs, std::make_index_sequence<sizeof...(Args2)>{}) {}

	constexpr Pair(const Pair& p) = default;
	constexpr Pair(Pair&& p) = default;

	constexpr ~Pair() = default;

	constexpr Pair& operator=(const Pair& other) requires(std::is_copy_assignable_v<T1> && std::is_copy_assignable_v<T2>) = default;
	constexpr Pair& operator=(const Pair& other) requires(!std::is_copy_assignable_v<T1> || !std::is_copy_assignable_v<T2>) = delete;

	template <typename U1, typename U2>
	constexpr Pair& operator=(const Pair<U1, U2>& other) requires(std::is_assignable_v<T1&, const U1&> && std::is_assignable_v<T2&, const U2&>) {
		first = other.first;
		second = other.second;
		return *this;
	}

	constexpr Pair& operator=(Pair&& other) noexcept(std::is_nothrow_move_assignable_v<T1> && std::is_nothrow_move_assignable_v<T2>)
		requires(std::is_move_assignable_v<T1> && std::is_move_assignable_v<T2>) = default;

	template <typename U1, typename U2>
	constexpr Pair& operator=(Pair<U1, U2>&& p) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(std::is_assignable_v<T1&, U1> && std::is_assignable_v<T2&, U2>) {
		first = std::forward<U1>(p.first);
		second = std::forward<U2>(p.second);
	}

	constexpr void swap(Pair& other) noexcept(std::is_nothrow_swappable_v<T1> && std::is_nothrow_swappable_v<T2>) {
		using std::swap;
		swap(first, other.first);
		swap(second, other.second);
	}

	friend void swap(Pair& a, Pair& b) noexcept(noexcept(a.swap(b))) requires(std::is_swappable_v<T1> && std::is_swappable_v<T2>) {
		a.swap(b);
	}

	[[nodiscard]] friend constexpr Pair<T2, T1> reversed(const Pair& pair) {
		return {pair.second, pair.first};
	}

private:
	template <typename... Args1, typename... Args2, std::size_t... Indices1, std::size_t... Indices2>
	constexpr Pair(std::piecewise_construct_t, std::tuple<Args1...>& firstArgs, std::index_sequence<Indices1...>, std::tuple<Args2...>& secondArgs,
		std::index_sequence<Indices2...>)
		: first(std::forward<Args1>(std::get<Indices1>(firstArgs))...)
		, second(std::forward<Args2>(std::get<Indices2>(secondArgs))...) {}
};

template <typename T1, typename T2>
Pair(T1, T2) -> Pair<T1, T2>;

} // namespace grem

template <typename T1, typename T2>
struct std::tuple_size<grem::Pair<T1, T2>> : std::integral_constant<std::size_t, 2> {};

template <typename T1, typename T2>
struct std::tuple_element<0, grem::Pair<T1, T2>> {
	using type = T1;
};

template <typename T1, typename T2>
struct std::tuple_element<1, grem::Pair<T1, T2>> {
	using type = T2;
};

template <typename T1, typename T2, typename U1, typename U2>
requires(requires { typename grem::Pair<std::common_type_t<T1, U1>, std::common_type_t<T2, U2>>; }) struct std::common_type<grem::Pair<T1, T2>, grem::Pair<U1, U2>> {
	using type = grem::Pair<std::common_type_t<T1, U1>, std::common_type_t<T2, U2>>;
};

template <typename T1, typename T2, typename U1, typename U2, template <typename> typename TQual, template <typename> typename UQual>
requires(requires { typename grem::Pair<std::common_reference_t<TQual<T1>, UQual<U1>>, std::common_reference_t<TQual<T2>, UQual<U2>>>; })
struct std::basic_common_reference<grem::Pair<T1, T2>, grem::Pair<U1, U2>, TQual, UQual> {
	using type = grem::Pair<std::common_reference_t<TQual<T1>, UQual<U1>>, std::common_reference_t<TQual<T2>, UQual<U2>>>;
};

namespace grem {

using std::tuple_element;
using std::tuple_element_t;
using std::tuple_size;
using std::tuple_size_v;

template <typename T1, typename T2, typename U1, typename U2>
[[nodiscard]] constexpr bool operator==(const Pair<T1, T2>& a, const Pair<U1, U2>& b) {
	return a.first == b.first && a.second == b.second;
}

template <typename T1, typename T2, typename U1, typename U2>
[[nodiscard]] constexpr std::common_comparison_category_t<detail::SynthThreeWayResult<T1, U1>, detail::SynthThreeWayResult<T2, U2>> operator<=>(const Pair<T1, T2>& a,
	const Pair<U1, U2>& b) {
	std::common_comparison_category_t<detail::SynthThreeWayResult<T1, U1>, detail::SynthThreeWayResult<T2, U2>> result = a.first <=> b.first;
	if (result == 0) {
		result = a.second <=> b.second;
	}
	return result;
}

template <typename T1, typename T2>
constexpr Pair<std::unwrap_ref_decay_t<T1>, std::unwrap_ref_decay_t<T2>> make_pair(T1&& x, T2&& y) {
	return {std::forward<T1>(x), std::forward<T2>(y)};
}

template <std::size_t Index, typename T1, typename T2>
[[nodiscard]] constexpr tuple_element_t<Index, Pair<T1, T2>>& get(Pair<T1, T2>& t) noexcept {
	static_assert(Index == 0 || Index == 1);
	if constexpr (Index == 0) {
		return t.first;
	} else {
		return t.second;
	}
}

template <std::size_t Index, typename T1, typename T2>
[[nodiscard]] constexpr const tuple_element_t<Index, Pair<T1, T2>>& get(const Pair<T1, T2>& t) noexcept {
	static_assert(Index == 0 || Index == 1);
	if constexpr (Index == 0) {
		return t.first;
	} else {
		return t.second;
	}
}

template <std::size_t Index, typename T1, typename T2>
[[nodiscard]] constexpr tuple_element_t<Index, Pair<T1, T2>>&& get(Pair<T1, T2>&& t) noexcept {
	static_assert(Index == 0 || Index == 1);
	if constexpr (Index == 0) {
		return std::move(t).first;
	} else {
		return std::move(t).second;
	}
}

template <std::size_t Index, typename T1, typename T2>
[[nodiscard]] constexpr const tuple_element_t<Index, Pair<T1, T2>>&& get(const Pair<T1, T2>&& t) noexcept {
	static_assert(Index == 0 || Index == 1);
	if constexpr (Index == 0) {
		return std::move(t).first;
	} else {
		return std::move(t).second;
	}
}

template <typename T, typename U>
[[nodiscard]] constexpr T& get(Pair<T, U>& t) noexcept {
	return t.first;
}

template <typename T, typename U>
[[nodiscard]] constexpr const T& get(const Pair<T, U>& t) noexcept {
	return t.first;
}

template <typename T, typename U>
[[nodiscard]] constexpr T&& get(Pair<T, U>&& t) noexcept {
	return std::move(t).first;
}

template <typename T, typename U>
[[nodiscard]] constexpr const T&& get(const Pair<T, U>&& t) noexcept {
	return std::move(t).first;
}

template <typename T, typename U>
[[nodiscard]] constexpr T& get(Pair<U, T>& t) noexcept {
	return t.second;
}

template <typename T, typename U>
[[nodiscard]] constexpr const T& get(const Pair<U, T>& t) noexcept {
	return t.second;
}

template <typename T, typename U>
[[nodiscard]] constexpr T&& get(Pair<U, T>&& t) noexcept {
	return std::move(t).second;
}

template <typename T, typename U>
[[nodiscard]] constexpr const T&& get(const Pair<U, T>&& t) noexcept {
	return std::move(t).second;
}

} // namespace grem

#endif
