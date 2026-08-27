// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_TUPLE_HPP
#define GREM_CORE_DATA_TUPLE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Pair.hpp>

#include <compare>     // std::strong_ordering, std::common_comparison_category_t
#include <cstddef>     // std::size_t
#include <functional>  // std::invoke
#include <type_traits> // std::is_..._v, std::remove_..._t, std::integral_constant, std::true_type, std::false_type, std::unwrap_ref_decay_t, std::decay_t
#include <utility>     // std::move, std::forward, std::declval, std::in_place..., std::tuple_size, std::tuple_element, std::...index_sequence

namespace grem {

namespace detail {

struct ConvertingTag {};

template <typename... Ts>
struct TupleStorage;

template <>
struct TupleStorage<> {
	constexpr TupleStorage() = default;

	constexpr TupleStorage(std::in_place_t) noexcept {}

	void assign(const TupleStorage<>&) {}

	void assign(TupleStorage<>&&) {} // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)

	[[nodiscard]] static constexpr auto isConvertibleFrom() {
		return std::true_type{};
	}

	[[nodiscard]] static constexpr auto isConstructibleFrom() {
		return std::true_type{};
	}

	[[nodiscard]] static constexpr auto isCopyAssignableFrom() {
		return std::true_type{};
	}

	[[nodiscard]] static constexpr auto isMoveAssignableFrom() {
		return std::true_type{};
	}
};

template <typename First, typename... Rest>
struct TupleStorage<First, Rest...> {
	First head;
	[[no_unique_address]] TupleStorage<Rest...> tail;

	constexpr TupleStorage() = default;

	template <typename Arg, typename... Args>
	constexpr TupleStorage(std::in_place_t, Arg&& arg, Args&&... args)
		: head(std::forward<Arg>(arg))
		, tail(std::in_place, std::forward<Args>(args)...) {}

	constexpr TupleStorage(ConvertingTag, auto&& other)
		: head(std::forward<decltype(other)>(other).head)
		, tail(ConvertingTag{}, std::forward<decltype(other)>(other).tail) {}

	template <typename... Us>
	void assign(const TupleStorage<Us...>& other) {
		head = other.head;
		tail.assign(other.tail);
	}

	template <typename... Us>
	void assign(TupleStorage<Us...>&& other) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		head = std::move(other.head);
		tail.assign(std::move(other.tail));
	}

	template <typename Arg, typename... Args>
	[[nodiscard]] static constexpr auto isConvertibleFrom([[maybe_unused]] Arg&& arg, [[maybe_unused]] Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
		if constexpr (std::is_convertible_v<decltype(std::forward<Arg>(arg)), First> && decltype(TupleStorage<Rest...>::isConvertibleFrom(std::forward<Args>(args)...))::value) {
			return std::true_type{};
		} else {
			return std::false_type{};
		}
	}

	template <typename Arg, typename... Args>
	[[nodiscard]] static constexpr auto isConstructibleFrom([[maybe_unused]] Arg&& arg, [[maybe_unused]] Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
		if constexpr (
			std::is_constructible_v<First, decltype(std::forward<Arg>(arg))> && decltype(TupleStorage<Rest...>::isConstructibleFrom(std::forward<Args>(args)...))::value) {
			return std::true_type{};
		} else {
			return std::false_type{};
		}
	}

	template <typename Arg, typename... Args>
	[[nodiscard]] static constexpr auto isCopyAssignableFrom([[maybe_unused]] Arg&& arg, [[maybe_unused]] Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
		if constexpr (
			std::is_assignable_v<First&, const std::remove_reference_t<Arg>&> && decltype(TupleStorage<Rest...>::isCopyAssignableFrom(std::forward<Args>(args)...))::value) {
			return std::true_type{};
		} else {
			return std::false_type{};
		}
	}

	template <typename Arg, typename... Args>
	[[nodiscard]] static constexpr auto isMoveAssignableFrom([[maybe_unused]] Arg&& arg, [[maybe_unused]] Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
		if constexpr (std::is_assignable_v<First&, std::remove_reference_t<Arg>> && decltype(TupleStorage<Rest...>::isCopyAssignableFrom(std::forward<Args>(args)...))::value) {
			return std::true_type{};
		} else {
			return std::false_type{};
		}
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr auto& get() & noexcept {
		if constexpr (Index == 0) {
			return head;
		} else {
			return tail.template get<Index - 1>();
		}
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr const auto& get() const& noexcept {
		if constexpr (Index == 0) {
			return head;
		} else {
			return tail.template get<Index - 1>();
		}
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr auto&& get() && noexcept {
		if constexpr (Index == 0) {
			return std::forward<decltype(head)>(head);
		} else {
			return std::move(tail).template get<Index - 1>();
		}
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr const auto&& get() const&& noexcept {
		if constexpr (Index == 0) {
			return std::forward<const decltype(head)>(head);
		} else {
			return std::move(tail).template get<Index - 1>();
		}
	}

	template <typename T>
	[[nodiscard]] constexpr T& get() & noexcept {
		if constexpr (std::is_same_v<T, First>) {
			return head;
		} else {
			return tail.template get<T>();
		}
	}

	template <typename T>
	[[nodiscard]] constexpr const T& get() const& noexcept {
		if constexpr (std::is_same_v<T, First>) {
			return head;
		} else {
			return tail.template get<T>();
		}
	}

	template <typename T>
	[[nodiscard]] constexpr T&& get() && noexcept {
		if constexpr (std::is_same_v<T, First>) {
			return std::forward<decltype(head)>(head);
		} else {
			return std::move(tail).template get<T>();
		}
	}

	template <typename T>
	[[nodiscard]] constexpr const T&& get() const&& noexcept {
		if constexpr (std::is_same_v<T, First>) {
			return std::forward<const decltype(head)>(head);
		} else {
			return std::move(tail).template get<T>();
		}
	}
};

template <typename T, typename... Ts>
struct type_count;

template <typename T>
struct type_count<T> : std::integral_constant<std::size_t, 0> {};

template <typename T, typename First, typename... Rest>
struct type_count<T, First, Rest...> : type_count<T, Rest...> {};

template <typename T, typename... Rest>
struct type_count<T, T, Rest...> : std::integral_constant<std::size_t, 1 + type_count<T, Rest...>::value> {};

template <typename T, typename... Ts>
inline constexpr std::size_t type_count_v = type_count<T, Ts...>::value;

} // namespace detail

template <typename... Ts>
class Tuple {
private:
	using Storage = detail::TupleStorage<Ts...>;

public:
	static constexpr std::size_t SIZE = sizeof...(Ts);

	constexpr explicit((!detail::default_copy_list_initializable<Ts> || ... || false)) //
		Tuple()                                                                        //
		requires((std::is_default_constructible_v<Ts> && ... && true))                 //
		= default;

	constexpr explicit((!std::is_convertible_v<const Ts&, Ts> || ... || false))          //
		Tuple(const Ts&... ts)                                                           //
		requires(sizeof...(Ts) > 0 && (std::is_copy_constructible_v<Ts> && ... && true)) //
		: storage(std::in_place, ts...) {}

	template <typename... Us>
	constexpr explicit((!std::is_convertible_v<Us, Ts> || ... || false))                                                  //
		Tuple(Us&&... us)                                                                                                 //
		requires(sizeof...(Ts) == sizeof...(Us) && sizeof...(Ts) > 0 && (std::is_constructible_v<Ts, Us> && ... && true)) //
		: storage(std::in_place, std::forward<Us>(us)...) {}

	template <typename... Us>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::declval<Us>()...))::value) //
		Tuple(Tuple<Us...>& other)                                                          //
		requires(sizeof...(Ts) == sizeof...(Us) && decltype(Storage::isConstructibleFrom(std::declval<Us>()...))::value &&
				 (sizeof...(Ts) != 1 || (!decltype(Storage::isConvertibleFrom(std::declval<Us>()...))::value && (!std::is_same_v<Ts, Us> && ... && true)))) //
		: storage(detail::ConvertingTag{}, other.storage) {}

	template <typename... Us>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::declval<const Us>()...))::value) //
		Tuple(const Tuple<Us...>& other)                                                          //
		requires(sizeof...(Ts) == sizeof...(Us) && decltype(Storage::isConstructibleFrom(std::declval<const Us>()...))::value &&
				 (sizeof...(Ts) != 1 || (!decltype(Storage::isConvertibleFrom(std::declval<const Us>()...))::value && (!std::is_same_v<Ts, Us> && ... && true)))) //
		: storage(detail::ConvertingTag{}, other.storage) {}

	template <typename... Us>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::move(std::declval<Us>())...))::value) //
		Tuple(Tuple<Us...>&& other)                                                                    // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(sizeof...(Ts) == sizeof...(Us) && decltype(Storage::isConstructibleFrom(std::move(std::declval<Us>())...))::value &&
				 (sizeof...(Ts) != 1 || (!decltype(Storage::isConvertibleFrom(std::move(std::declval<Us>())...))::value && (!std::is_same_v<Ts, Us> && ... && true)))) //
		: storage(detail::ConvertingTag{}, other.storage) {}

	template <typename... Us>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::move(std::declval<Us>())...))::value) //
		Tuple(const Tuple<Us...>&& other)                                                              //
		requires(sizeof...(Ts) == sizeof...(Us) && decltype(Storage::isConstructibleFrom(std::move(std::declval<const Us>())...))::value &&
				 (sizeof...(Ts) != 1 || (!decltype(Storage::isConvertibleFrom(std::move(std::declval<const Us>())...))::value && (!std::is_same_v<Ts, Us> && ... && true)))) //
		: storage(detail::ConvertingTag{}, other.storage) {}

	template <typename U1, typename U2>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::declval<U1>(), std::declval<U2>()))::value)                  //
		Tuple(Pair<U1, U2>& p)                                                                                                //
		requires(sizeof...(Ts) == 2 && decltype(Storage::isConstructibleFrom(std::declval<U1>(), std::declval<U2>()))::value) //
		: storage(std::in_place, p.first, p.second) {}

	template <typename U1, typename U2>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::declval<const U1>(), std::declval<const U2>()))::value)                  //
		Tuple(const Pair<U1, U2>& p)                                                                                                      //
		requires(sizeof...(Ts) == 2 && decltype(Storage::isConstructibleFrom(std::declval<const U1>(), std::declval<const U2>()))::value) //
		: storage(std::in_place, p.first, p.second) {}

	template <typename U1, typename U2>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::move(std::declval<U1>()), std::move(std::declval<U2>())))::value) //
		Tuple(Pair<U1, U2>&& p) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(sizeof...(Ts) == 2 && decltype(Storage::isConstructibleFrom(std::move(std::declval<U1>()), std::move(std::declval<U2>())))::value) //
		: storage(std::in_place, std::move(p.first), std::move(p.second)) {}

	template <typename U1, typename U2>
	constexpr explicit(!decltype(Storage::isConvertibleFrom(std::move(std::declval<const U1>()), std::move(std::declval<const U2>())))::value)                  //
		Tuple(const Pair<U1, U2>&& p)                                                                                                                           //
		requires(sizeof...(Ts) == 2 && decltype(Storage::isConstructibleFrom(std::move(std::declval<const U1>()), std::move(std::declval<const U2>())))::value) //
		: storage(std::in_place, std::move(p.first), std::move(p.second)) {}

	constexpr Tuple(const Tuple&)                                    //
		requires(!(std::is_copy_constructible_v<Ts> && ... && true)) //
		= delete;
	constexpr Tuple(const Tuple&)                                   //
		requires((std::is_copy_constructible_v<Ts> && ... && true)) //
		= default;

	constexpr Tuple(Tuple&&)                                                //
		noexcept((std::is_nothrow_move_constructible_v<Ts> && ... && true)) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		requires((std::is_move_constructible_v<Ts> && ... && true))         //
		= default;

	constexpr ~Tuple() = default;

	constexpr Tuple& operator=(const Tuple&)                      //
		requires(!(std::is_copy_assignable_v<Ts> && ... && true)) //
		= delete;
	constexpr Tuple& operator=(const Tuple&)                     //
		requires((std::is_copy_assignable_v<Ts> && ... && true)) //
		= default;

	constexpr Tuple& operator=(Tuple&&)                           //
		requires(!(std::is_move_assignable_v<Ts> && ... && true)) //
		= delete;
	constexpr Tuple& operator=(Tuple&&)                                  //
		noexcept((std::is_nothrow_move_assignable_v<Ts> && ... && true)) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		requires((std::is_move_assignable_v<Ts> && ... && true))         //
		= default;

	template <typename... Us>
	constexpr Tuple& operator=(const Tuple<Us...>& other) //
		requires(sizeof...(Ts) == sizeof...(Us) && (std::is_assignable_v<Ts&, const Us&> && ... && true)) {
		storage.assign(other.storage);
		return *this;
	}

	template <typename... Us>
	constexpr Tuple& operator=(Tuple<Us...>&& other) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(sizeof...(Ts) == sizeof...(Us) && (std::is_assignable_v<Ts&, Us> && ... && true)) {
		storage.assign(std::move(other.storage));
		return *this;
	}

	template <typename U1, typename U2>
	constexpr Tuple& operator=(const Pair<U1, U2>& p) //
		requires(sizeof...(Ts) == 2 && decltype(Storage::isCopyAssignableFrom(p.first, p.second))::value) {
		storage.head = p.first;
		storage.tail.head = p.second;
		return *this;
	}

	template <typename U1, typename U2>
	constexpr Tuple& operator=(Pair<U1, U2>&& p) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(sizeof...(Ts) == 2 && decltype(Storage::isMoveAssignableFrom(std::move(p.first), std::move(p.second)))::value) {
		storage.head = std::move(p.first);
		storage.tail.head = std::move(p.second);
		return *this;
	}

	constexpr void swap(Tuple& other) noexcept((std::is_nothrow_swappable_v<Ts> && ... && true)) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		using std::swap;
		swap(storage, other.storage);
	}

	friend constexpr void swap(Tuple& a, Tuple& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr auto& get() & noexcept {
		static_assert(Index < sizeof...(Ts), "The specified index must be in range of the tuple.");
		return storage.template get<Index>();
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr const auto& get() const& noexcept {
		static_assert(Index < sizeof...(Ts), "The specified index must be in range of the tuple.");
		return storage.template get<Index>();
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr auto&& get() && noexcept {
		static_assert(Index < sizeof...(Ts), "The specified index must be in range of the tuple.");
		return std::move(storage).template get<Index>();
	}

	template <std::size_t Index>
	[[nodiscard]] constexpr const auto&& get() const&& noexcept {
		static_assert(Index < sizeof...(Ts), "The specified index must be in range of the tuple.");
		return std::move(storage).template get<Index>();
	}

	template <typename T>
	[[nodiscard]] constexpr T& get() & noexcept {
		constexpr std::size_t OCCURANCES = detail::type_count_v<T, Ts...>;
		static_assert(OCCURANCES != 0, "The specified type must exist in the tuple.");
		static_assert(OCCURANCES == 1, "The specified type must occur exactly once in the tuple.");
		return storage.template get<T>();
	}

	template <typename T>
	[[nodiscard]] constexpr const T& get() const& noexcept {
		constexpr std::size_t OCCURANCES = detail::type_count_v<T, Ts...>;
		static_assert(OCCURANCES != 0, "The specified type must exist in the tuple.");
		static_assert(OCCURANCES == 1, "The specified type must occur exactly once in the tuple.");
		return storage.template get<T>();
	}

	template <typename T>
	[[nodiscard]] constexpr T&& get() && noexcept {
		constexpr std::size_t OCCURANCES = detail::type_count_v<T, Ts...>;
		static_assert(OCCURANCES != 0, "The specified type must exist in the tuple.");
		static_assert(OCCURANCES == 1, "The specified type must occur exactly once in the tuple.");
		return std::move(storage).template get<T>();
	}

	template <typename T>
	[[nodiscard]] constexpr const T&& get() const&& noexcept {
		constexpr std::size_t OCCURANCES = detail::type_count_v<T, Ts...>;
		static_assert(OCCURANCES != 0, "The specified type must exist in the tuple.");
		static_assert(OCCURANCES == 1, "The specified type must occur exactly once in the tuple.");
		return std::move(storage).template get<T>();
	}

private:
	Storage storage{};
};

template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;

template <typename T1, typename T2>
Tuple(Pair<T1, T2>) -> Tuple<T1, T2>;

template <typename... TTypes, typename... UTypes>
[[nodiscard]] constexpr bool operator==(const Tuple<TTypes...>& a, const Tuple<UTypes...>& b) {
	static_assert(sizeof...(TTypes) == sizeof...(UTypes));
	return []<std::size_t... Indices>(const Tuple<TTypes...>& t, const Tuple<UTypes...>& u, std::index_sequence<Indices...>) -> bool {
		return ((get<Indices>(t) == get<Indices>(u)) && ... && true);
	}(a, b, std::make_index_sequence<sizeof...(TTypes)>{});
}

template <typename... TTypes, typename... UTypes>
[[nodiscard]] constexpr std::common_comparison_category_t<detail::SynthThreeWayResult<TTypes, UTypes>...> operator<=>(const Tuple<TTypes...>& a, const Tuple<UTypes...>& b) {
	static_assert(sizeof...(TTypes) == sizeof...(UTypes));
	return []<std::size_t... Indices>(const Tuple<TTypes...>& t, const Tuple<UTypes...>& u, std::index_sequence<Indices...>) -> auto {
		std::common_comparison_category_t<detail::SynthThreeWayResult<TTypes, UTypes>...> result = std::strong_ordering::equal;
		(void)((result = detail::synthThreeWay(std::get<Indices>(t), std::get<Indices>(u)), result != 0) || ...);
		return result;
	}(a, b, std::make_index_sequence<sizeof...(TTypes)>{});
}

struct Ignore {
	template <typename T>
	constexpr void operator=(  // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
		T&&) const noexcept {} // NOLINT(cppcoreguidelines-missing-std-forward)
};

inline constexpr Ignore ignore{};

template <typename... Ts>
[[nodiscard]] constexpr Tuple<std::unwrap_ref_decay_t<Ts>...> make_tuple(Ts&&... ts) {
	return Tuple<std::unwrap_ref_decay_t<Ts>...>(std::forward<Ts>(ts)...);
}

template <typename... Ts>
[[nodiscard]] constexpr Tuple<Ts&...> tie(Ts&... ts) {
	return {ts...};
}

template <typename... Ts>
[[nodiscard]] constexpr Tuple<Ts&&...> forward_as_tuple(Ts&&... ts) {
	return Tuple<Ts&&...>(std::forward<Ts>(ts)...);
}

namespace detail {

template <std::size_t SearchIndex, std::size_t TupleIndex, typename First, typename... Rest>
struct tuple_index_and_offset : tuple_index_and_offset<SearchIndex - std::tuple_size_v<std::remove_cvref_t<First>>, TupleIndex + 1, Rest...> {};

template <std::size_t SearchIndex, std::size_t TupleIndex, typename First, typename... Rest>
requires(SearchIndex < std::tuple_size_v<std::remove_cvref_t<First>>) struct tuple_index_and_offset<SearchIndex, TupleIndex, First, Rest...> {
	static constexpr std::size_t TUPLE_INDEX = TupleIndex;
	static constexpr std::size_t TUPLE_OFFSET = SearchIndex;
};

template <std::size_t Index, typename... Tuples>
[[nodiscard]] constexpr decltype(auto) getAbsolute(Tuple<Tuples&...>& tupleOfTuples) noexcept {
	using TupleIndexAndOffset = detail::tuple_index_and_offset<Index, 0, Tuples...>;
	constexpr std::size_t TUPLE_INDEX = TupleIndexAndOffset::TUPLE_INDEX;
	constexpr std::size_t TUPLE_OFFSET = TupleIndexAndOffset::TUPLE_OFFSET;
	return get<TUPLE_OFFSET>(get<TUPLE_INDEX>(tupleOfTuples));
}

} // namespace detail

template <typename... Tuples>
[[nodiscard]] constexpr auto tuple_cat(Tuples&&... tuples) { // NOLINT(cppcoreguidelines-missing-std-forward)
	if constexpr (sizeof...(Tuples) == 0) {
		return Tuple<>{};
	} else {
		constexpr std::size_t TOTAL_SIZE = (std::tuple_size_v<std::remove_cvref_t<Tuples>> + ... + std::size_t{0});
		auto tupleOfTuples = tie(tuples...);
		return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
			return Tuple{detail::getAbsolute<Indices>(tupleOfTuples)...};
		}(std::make_index_sequence<TOTAL_SIZE>{});
	}
}

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr decltype(auto) get(Tuple<Ts...>& t) noexcept {
	return t.template get<Index>();
}

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr decltype(auto) get(const Tuple<Ts...>& t) noexcept {
	return t.template get<Index>();
}

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr decltype(auto) get(Tuple<Ts...>&& t) noexcept {
	return std::move(t).template get<Index>();
}

template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr decltype(auto) get(const Tuple<Ts...>&& t) noexcept {
	return std::move(t).template get<Index>();
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr T& get(Tuple<Ts...>& t) noexcept {
	return t.template get<T>();
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr const T& get(const Tuple<Ts...>& t) noexcept {
	return t.template get<T>();
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr T&& get(Tuple<Ts...>&& t) noexcept {
	return std::move(t).template get<T>();
}

template <typename T, typename... Ts>
[[nodiscard]] constexpr const T&& get(const Tuple<Ts...>&& t) noexcept {
	return std::move(t).template get<T>();
}

template <typename Function, typename... Ts>
constexpr decltype(auto) apply(Function&& function, Tuple<Ts...>& tuple) {
	return []<typename F, std::size_t... Indices>(F&& f, Tuple<Ts...>& t, std::index_sequence<Indices...>) -> decltype(auto) {
		return std::invoke(std::forward<F>(f), get<Indices>(t)...);
	}(std::forward<Function>(function), tuple, std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename Function, typename... Ts>
constexpr decltype(auto) apply(Function&& function, const Tuple<Ts...>& tuple) {
	return []<typename F, std::size_t... Indices>(F&& f, const Tuple<Ts...>& t, std::index_sequence<Indices...>) -> decltype(auto) {
		return std::invoke(std::forward<F>(f), get<Indices>(t)...);
	}(std::forward<Function>(function), tuple, std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename Function, typename... Ts>
constexpr decltype(auto) apply(Function&& function, Tuple<Ts...>&& tuple) {
	return []<typename F, std::size_t... Indices>(F&& f, Tuple<Ts...>&& t, std::index_sequence<Indices...>) -> decltype(auto) {
		return std::invoke(std::forward<F>(f), get<Indices>(std::move(t))...);
	}(std::forward<Function>(function), std::move(tuple), std::make_index_sequence<sizeof...(Ts)>{});
}

template <typename Function, typename... Ts>
constexpr decltype(auto) apply(Function&& function, const Tuple<Ts...>&& tuple) {
	return []<typename F, std::size_t... Indices>(F&& f, const Tuple<Ts...>&& t, std::index_sequence<Indices...>) -> decltype(auto) {
		return std::invoke(std::forward<F>(f), get<Indices>(std::move(t))...);
	}(std::forward<Function>(function), std::move(tuple), std::make_index_sequence<sizeof...(Ts)>{});
}

} // namespace grem

template <typename... Ts>
struct std::tuple_size<grem::Tuple<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <typename First, typename... Rest>
struct std::tuple_element<0, grem::Tuple<First, Rest...>> {
	using type = First;
};

template <std::size_t Index, typename First, typename... Rest>
struct std::tuple_element<Index, grem::Tuple<First, Rest...>> : tuple_element<Index - 1, grem::Tuple<Rest...>> {};

namespace grem {

using std::tuple_element;
using std::tuple_element_t;
using std::tuple_size;
using std::tuple_size_v;

} // namespace grem

#endif
