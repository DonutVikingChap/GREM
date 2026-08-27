// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_METAPROGRAMMING_HPP
#define GREM_CORE_METAPROGRAMMING_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/Variant.hpp>

#include <cstddef>         // std::size_t
#include <memory>          // std::construct_at, std::destroy_at
#include <source_location> // std::source_location
#include <stdexcept>       // std::out_of_range
#include <type_traits> // std::conditional_t, std::bool_constant, std::..._type, std::is_..._v, std::integral_constant, std::remove_..._t, std::common_reference_t, std::is_constant_evaluated
#include <utility> // std::index_sequence, std::make_index_sequence, std::declval

/// \cond
struct GREM_private_MetaReflectionStruct {
	char GREM_private_metaReflectionField;
};
enum class GREM_private_MetaReflectionEnum { // NOLINT(performance-enum-size)
	GREM_PRIVATE_META_REFLECTION_ENUMERAND,
};
/// \endcond

namespace grem::meta {

namespace detail {
struct UnusedType {};
} // namespace detail

/**
 * Type-dependent boolean that always evaluates to false.
 *
 * \tparam Ts type(s) that the value conceptually "depends" on.
 *
 * \warning Must not be specialized.
 *
 * \remark This is mainly intended for use with static_assert() in templates,
 *         for example to raise a compile-time error in the last `else` of an
 *         `if constexpr` chain as follows:
 *         `static_assert(meta::always_false_v<T>, "Bad type.");`.
 *         Note that a simple `static_assert(false, "Bad type");` doesn't work
 *         since the expression doesn't depend on any template arguments, and
 *         the compiler can therefore infer that it fails unequivocally for all
 *         possible instantiations of the template. Making the expression depend
 *         on a template argument circumvents this, since the compiler then has
 *         to assume that there could be some specialization of `always_false_v`
 *         for some unknown future type that makes it evaluate to `true`
 *         (although we never actually define such a specialization, except for
 *         detail::UnusedType).
 */
template <typename... Ts>
inline constexpr bool always_false_v = false;

template <>
inline constexpr bool always_false_v<detail::UnusedType> = true;

/**
 * Construct a fixed-size array of N elements of type T, where each element is
 * constructed from the same arguments.
 *
 * \tparam N size of the array to construct.
 * \tparam T element type of the array to construct.
 *
 * \param args arguments to pass to each element's constructor.
 *
 * \throws any exception thrown by the element constructors or by making an
 *         array from the results.
 */
template <size_t N, typename T, typename... Args>
[[nodiscard]] constexpr auto createN(Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
	return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		const auto createElement = [&]() -> T {
			return T(args...);
		};
		return Array<T, N>{{((void)Indices, createElement())...}};
	}(std::make_index_sequence<N>{});
}

/**
 * Execute a function for each element in a given array and return an array
 * containing the results.
 *
 * \param array array to transform.
 * \param fn function to execute. Must accept a reference to the array's element
 *        type as a parameter and return a non-void value.
 *
 * \throws any exception thrown by fn or by making an array from the results.
 */
template <typename T, size_t N>
[[nodiscard]] constexpr auto transform(Array<T, N>& array, auto fn) {
	return [&array, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return Array{fn(array[Indices])...};
	}(std::make_index_sequence<N>{});
}

/**
 * Execute a function for each element in a given array and return an array
 * containing the results.
 *
 * \param array array to transform.
 * \param fn function to execute. Must accept a reference to the array's element
 *        type as a parameter and return a non-void value.
 *
 * \throws any exception thrown by fn or by making an array from the results.
 */
template <typename T, size_t N>
[[nodiscard]] constexpr auto transform(const Array<T, N>& array, auto fn) {
	return [&array, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return Array{fn(array[Indices])...};
	}(std::make_index_sequence<N>{});
}

/**
 * Execute a function for each element in a given array and return an array
 * containing the results.
 *
 * \param array array to transform.
 * \param fn function to execute. Must accept a reference to the array's element
 *        type as a parameter and return a non-void value.
 *
 * \throws any exception thrown by fn or by making an array from the results.
 */
template <typename T, size_t N>
[[nodiscard]] constexpr auto transform(Array<T, N>&& array, auto fn) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	return [&array, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return Array{fn(std::move(array[Indices]))...};
	}(std::make_index_sequence<N>{});
}

/**
 * Execute a function for each element in a given array and return an array
 * containing the results.
 *
 * \param array array to transform.
 * \param fn function to execute. Must accept a reference to the array's element
 *        type as a parameter and return a non-void value.
 *
 * \throws any exception thrown by fn or by making an array from the results.
 */
template <typename T, size_t N>
[[nodiscard]] constexpr auto transform(const Array<T, N>&& array, auto fn) {
	return [&array, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return Array{fn(std::move(array[Indices]))...};
	}(std::make_index_sequence<N>{});
}

/**
 * Execute a function for each element in a given tuple and return a tuple
 * containing the results.
 *
 * \param tuple tuple to transform.
 * \param fn function to execute. Must accept a reference to each of the types
 *        in the tuple as a parameter and return a non-void value.
 *
 * \throws any exception thrown by fn or by making a tuple from the results.
 */
[[nodiscard]] constexpr auto transform(auto&& tuple, auto fn) {
	return [&tuple, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		return grem::make_tuple(fn(get<Indices>(tuple))...);
	}(std::make_index_sequence<tuple_size_v<std::remove_cvref_t<decltype(tuple)>>>{});
}

/**
 * Execute a function for each element in a given tuple, sequentially.
 *
 * \param tuple tuple to iterate.
 * \param fn function to execute. Must accept a reference to each of the types
 *        in the tuple as a parameter.
 *
 * \throws any exception thrown by fn.
 */
constexpr void forEach(auto&& tuple, auto fn) {
	[&tuple, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
		(fn(get<Indices>(tuple)), ...);
	}(std::make_index_sequence<tuple_size_v<std::remove_cvref_t<decltype(tuple)>>>{});
}

/**
 * Wrapper type for a compile-time constant value.
 *
 * \tparam X constant value to wrap.
 */
template <auto X>
struct Constant {
	static constexpr const auto& value = X;

	using type = Constant;
	using value_type = decltype(X);

	constexpr operator value_type() const noexcept {
		return value;
	}

	constexpr value_type operator()() const noexcept {
		return value;
	}
};

/**
 * Wrapper value for a compile-time constant value.
 *
 * \tparam X constant value to wrap.
 */
template <auto X>
constexpr Constant<X> CONSTANT{};

/**
 * Execute a function for each index in the sequence from 0 up to, but not
 * including, a given count N.
 *
 * \tparam N the number of indices in the sequence.
 *
 * \param fn function to execute. Must accept an object of type
 *        `meta::Constant<I>` as a parameter, where I is any integer from 0 up
 *        to, but not including, N.
 *
 * \throws any exception thrown by fn.
 */
template <std::size_t N>
constexpr void forEachIndex(auto fn) {
	[fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
		(fn(CONSTANT<Indices>), ...);
	}(std::make_index_sequence<N>{});
}

/**
 * Construct a fixed-size array, using a function to create each of its
 * elements, optionally based on the element index.
 *
 * \tparam N size of the array to generate.
 * \tparam T element type of the array to generate.
 *
 * \param fn function that generates each element. May optionally accept the
 *        index of the array as either a size_t parameter or meta::Constant
 *        parameter with a non-type template parameter of type size_t as its
 *        template argument.
 *
 * \throws any exception thrown by fn or by making an array from the results.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr auto generateN(auto fn) {
	if constexpr (N == 0) {
		return Array<T, 0>{};
	} else {
		return [fn]<std::size_t... Indices>(std::index_sequence<Indices...>) {
			if constexpr (requires {
							  { fn(meta::CONSTANT<0>) } -> convertible_to<T>;
						  }) {
				return Array<T, N>{{fn(meta::CONSTANT<Indices>)...}};
			} else {
				return Array<T, N>{{((void)Indices, fn())...}};
			}
		}(std::make_index_sequence<N>{});
	}
}

/**
 * Execute a function associated with one specific index in the sequence from 0
 * up to, but not including, a given count N.
 *
 * \tparam N the number of indices in the sequence.
 *
 * \param index the index to execute the corresponding function overload of.
 * \param fn function to execute. Must accept an object of type
 *        `meta::Constant<I>` as a parameter, where I is any integer from 0 up
 *        to, but not including, N.
 *
 * \return the result of fn.
 *
 * \throws std::out_of_range if `index >= N`.
 * \throws any exception thrown by fn.
 */
template <std::size_t N>
constexpr decltype(auto) visitIndex(std::size_t index, auto fn) {
	return [index, fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> decltype(auto) {
		using R = std::common_reference_t<decltype(fn(CONSTANT<Indices>))...>;
		if constexpr (std::is_void_v<R>) {
			if (!(((index == Indices) ? (fn(CONSTANT<Indices>), true) : false) || ...)) {
				throw std::out_of_range{"index >= N"};
			}
		} else if constexpr (std::is_lvalue_reference_v<R>) {
			std::remove_reference_t<R>* result = nullptr;
			if (!(((index == Indices) ? ((result = &fn(CONSTANT<Indices>)), true) : false) || ...)) {
				throw std::out_of_range{"index >= N"};
			}
			return *result;
		} else {
			struct Result {
				union MaybeValue {
					R actual;
					char dummy{};

					constexpr ~MaybeValue() {}
				};

				MaybeValue value{};
				bool hasValue = false;

				constexpr ~Result() {
					if (hasValue) {
						std::destroy_at(&value.actual);
					}
				}
			} result{};
			if (!(((index == Indices) ? (std::construct_at(&result.value.actual, fn(CONSTANT<Indices>)), true) : false) || ...)) {
				throw std::out_of_range{"index >= N"};
			}
			result.hasValue = true;
			return R{std::move(result.value.actual)};
		}
	}(std::make_index_sequence<N>{});
}

/**
 * Type identity wrapper.
 *
 * \tparam T type to wrap.
 */
template <typename T>
struct Type {
	using type = T;
};

/**
 * Wrapper value for a type identity wrapper.
 *
 * \tparam T type to wrap.
 */
template <typename T>
constexpr Type<T> TYPE{};

/**
 * Compile-time list of types.
 *
 * \tparam Ts pack of types to wrap.
 */
template <typename... Ts>
struct TypeList {
	using type = TypeList;
};

/**
 * Wrapper value for a compile-time list of types.
 *
 * \tparam Ts pack of types to wrap.
 */
template <typename... Ts>
constexpr TypeList<Ts...> TYPE_LIST{};

/// \cond
template <typename T>
struct is_type_list : std::false_type {};

template <typename... Ts>
struct is_type_list<TypeList<Ts...>> : std::true_type {};
/// \endcond

/**
 * Boolean that evaluates to true if the given type is a specialization of
 * meta::TypeList.
 *
 * \tparam T type to check.
 */
template <typename T>
inline constexpr bool is_type_list_v = is_type_list<T>::value;

/**
 * Concept that checks if a given type is a specialization of meta::TypeList.
 *
 * \tparam T type to check.
 */
template <typename T>
concept type_list = is_type_list_v<T>;

/// \cond
template <typename List>
struct type_list_size;

template <typename... Ts>
struct type_list_size<TypeList<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};
/// \endcond

/**
 * Number of types in a specialization of meta::TypeList.
 *
 * \tparam List type list to get the size of. Must be a specialization of
 *         meta::TypeList.
 */
template <typename List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

/// \cond
template <typename List>
struct type_list_empty;

template <typename... Ts>
struct type_list_empty<TypeList<Ts...>> : std::false_type {};

template <>
struct type_list_empty<TypeList<>> : std::true_type {};
/// \endcond

/**
 * Whether a specialization of meta::TypeList is empty or not.
 *
 * \tparam List type list to check.
 */
template <typename List>
inline constexpr bool type_list_empty_v = type_list_empty<List>::value;

namespace detail {

template <std::size_t Index, typename This, typename... Rest>
struct TypeListTypeImpl : TypeListTypeImpl<Index - 1, Rest...> {};

template <typename This, typename... Rest>
struct TypeListTypeImpl<0, This, Rest...> {
	using type = This;
};

} // namespace detail

/// \cond
template <typename List, std::size_t Index>
struct type_list_type;

template <typename... Ts, std::size_t Index>
struct type_list_type<TypeList<Ts...>, Index> : detail::TypeListTypeImpl<Index, Ts...> {};
/// \endcond

/**
 * Type at a specific index of a specialization of meta::TypeList.
 *
 * \tparam List type list to get the type from. Must be a specialization of
 *         meta::TypeList.
 * \tparam Index index of the type to get. Must be less than the type list size.
 */
template <typename List, std::size_t Index>
using type_list_type_t = typename type_list_type<List, Index>::type;

namespace detail {

template <typename T, typename This, typename... Rest>
struct TypeListContainsImpl : std::conditional_t<std::is_same_v<T, This>, std::true_type, TypeListContainsImpl<T, Rest...>> {};

template <typename T, typename This>
struct TypeListContainsImpl<T, This> : std::bool_constant<std::is_same_v<T, This>> {};

} // namespace detail

/// \cond
template <typename List, typename T>
struct type_list_contains;

template <typename... Ts, typename T>
struct type_list_contains<TypeList<Ts...>, T> : detail::TypeListContainsImpl<T, Ts...> {};
/// \endcond

/**
 * Whether a specialization of meta::TypeList contains a specific type.
 *
 * \tparam List type list to check. Must be a specialization of meta::TypeList.
 * \tparam T type to check for.
 */
template <typename List, typename T>
inline constexpr bool type_list_contains_v = type_list_contains<List, T>::value;

namespace detail {

template <std::size_t Index, typename T, typename... Ts>
[[nodiscard]] consteval std::size_t getTypeListIndex() noexcept {
	static_assert(type_list_contains_v<TypeList<Ts...>, T>, "TypeList does not contain the given type.");
	if constexpr (std::is_same_v<T, type_list_type_t<TypeList<Ts...>, Index>>) {
		return Index;
	} else if constexpr (Index + 1 < sizeof...(Ts)) {
		return getTypeListIndex<Index + 1, T, Ts...>();
	}
}

} // namespace detail

/// \cond
template <typename List, typename T>
struct type_list_index;

template <typename... Ts, typename T>
struct type_list_index<TypeList<Ts...>, T> : std::integral_constant<std::size_t, detail::getTypeListIndex<0, T, Ts...>()> {};
/// \endcond

/**
 * Index of the first occurence of a specific type in a specialization of
 * meta::TypeList.
 *
 * \tparam List type list to get the index from. Must be a specialization of
 *         meta::TypeList.
 * \tparam T type to get the index of. Must be a type that the type list
 *         contains.
 */
template <typename List, typename T>
inline constexpr std::size_t type_list_index_v = type_list_index<List, T>::value;

namespace detail {

template <std::size_t Index, typename T, typename... Ts>
[[nodiscard]] consteval std::size_t getTypeListReverseIndex() noexcept {
	static_assert(type_list_contains_v<TypeList<Ts...>, T>, "TypeList does not contain the given type.");
	if constexpr (std::is_same_v<T, type_list_type_t<TypeList<Ts...>, Index>>) {
		return Index;
	} else if constexpr (Index > 0) {
		return getTypeListReverseIndex<Index - 1, T, Ts...>();
	}
}

} // namespace detail

/// \cond
template <typename List, typename T>
struct type_list_rindex;

template <typename... Ts, typename T>
struct type_list_rindex<TypeList<Ts...>, T> : std::integral_constant<std::size_t, detail::getTypeListReverseIndex<sizeof...(Ts) - 1, T, Ts...>()> {};
/// \endcond

/**
 * Index of the last occurence of a specific type in a specialization of
 * meta::TypeList.
 *
 * \tparam List type list to get the index from. Must be a specialization of
 *         meta::TypeList.
 * \tparam T type to get the index of. Must be a type that the type list
 *         contains.
 */
template <typename List, typename T>
inline constexpr std::size_t type_list_rindex_v = type_list_rindex<List, T>::value;

namespace detail {

template <std::size_t Index, typename T, typename... Ts>
[[nodiscard]] consteval std::size_t getTypeListCount() noexcept {
	if constexpr (Index >= sizeof...(Ts)) {
		return 0;
	} else if constexpr (std::is_same_v<T, type_list_type_t<TypeList<Ts...>, Index>>) {
		return 1 + getTypeListCount<Index + 1, T, Ts...>();
	} else {
		return getTypeListCount<Index + 1, T, Ts...>();
	}
}

} // namespace detail

/// \cond
template <typename List, typename T>
struct type_list_count;

template <typename... Ts, typename T>
struct type_list_count<TypeList<Ts...>, T> : std::integral_constant<std::size_t, detail::getTypeListCount<0, T, Ts...>()> {};
/// \endcond

/**
 * Number of occurences of a sepcific type in a specialization of
 * meta::TypeList.
 *
 * \tparam List type list. Must be a specialization of meta::TypeList.
 * \tparam T type to count the number of occurences of.
 */
template <typename List, typename T>
inline constexpr std::size_t type_list_count_v = type_list_count<List, T>::value;

/// \cond
template <typename... Lists>
struct type_list_concat;

template <>
struct type_list_concat<> {
	using type = TypeList<>;
};

template <typename... Ts>
struct type_list_concat<TypeList<Ts...>> {
	using type = TypeList<Ts...>;
};

template <typename... Ts, typename... Us, typename... Rest>
struct type_list_concat<TypeList<Ts...>, TypeList<Us...>, Rest...> : type_list_concat<TypeList<Ts..., Us...>, Rest...> {};
/// \endcond

/**
 * Concatenation of a sequence of specializations of meta::TypeList into a
 * single meta::TypeList.
 *
 * \tparam Lists type lists to concatenate. Must all be specializations of
 *         meta::TypeList.
 */
template <typename... Lists>
using type_list_concat_t = typename type_list_concat<Lists...>::type;

/// \cond
template <typename List, typename PrefixList>
struct type_list_starts_with;

template <typename... Ts, typename... Us>
struct type_list_starts_with<TypeList<Ts...>, TypeList<Us...>> : std::false_type {};

template <typename... Ts>
struct type_list_starts_with<TypeList<Ts...>, TypeList<>> : std::true_type {};

template <typename First, typename... Rest, typename... RestPrefix>
struct type_list_starts_with<TypeList<First, Rest...>, TypeList<First, RestPrefix...>> : type_list_starts_with<TypeList<Rest...>, TypeList<RestPrefix...>> {};
/// \endcond

/**
 * Whether a specialization of meta::TypeList starts with the types in another
 * type list.
 *
 * \tparam List type list to check. Must be a specialization of meta::TypeList.
 * \tparam PrefixList list of prefix types to check for. Must be a
 *         specialization of meta::TypeList.
 */
template <typename List, typename PrefixList>
inline constexpr bool type_list_starts_with_v = type_list_starts_with<List, PrefixList>::value;

static_assert(type_list_starts_with_v<TypeList<int, float, double>, TypeList<int, float, double>>);
static_assert(type_list_starts_with_v<TypeList<int, float, double>, TypeList<int, float>>);
static_assert(type_list_starts_with_v<TypeList<int, float, double>, TypeList<int>>);
static_assert(type_list_starts_with_v<TypeList<int, float, double>, TypeList<>>);
static_assert(!type_list_starts_with_v<TypeList<int, float, double>, TypeList<int, float, double, int>>);
static_assert(!type_list_starts_with_v<TypeList<int, float, double>, TypeList<int, float, int>>);
static_assert(!type_list_starts_with_v<TypeList<int, float, double>, TypeList<int, int>>);

/// \cond
template <typename List, typename T>
struct type_list_starts_with_type;

template <typename... Ts, typename U>
struct type_list_starts_with_type<TypeList<Ts...>, U> : std::false_type {};

template <typename T, typename... Rest>
struct type_list_starts_with_type<TypeList<T, Rest...>, T> : std::true_type {};
/// \endcond

/**
 * Whether a specialization of meta::TypeList starts with a specific type.
 *
 * \tparam List type list to check. Must be a specialization of meta::TypeList.
 * \tparam T first type to check for.
 */
template <typename List, typename T>
inline constexpr bool type_list_starts_with_type_v = type_list_starts_with_type<List, T>::value;

static_assert(type_list_starts_with_type_v<TypeList<int, float, double>, int>);
static_assert(!type_list_starts_with_type_v<TypeList<int, float, double>, float>);

/**
 * Execute a function for each type in a type list.
 *
 * \tparam List TypeList type.
 *
 * \param fn function to execute. Must accept an object of type `meta::Type<T>`
 *        as a parameter, where T is any type in the given type list.
 *
 * \throws any exception thrown by fn.
 */
template <typename List>
constexpr void forEachType(auto fn) {
	[fn]<typename... Ts>(TypeList<Ts...>) -> void {
		(fn(TYPE<Ts>), ...);
	}(List{});
}

namespace detail {

struct Init {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
	template <typename T>
	constexpr operator T() const noexcept;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

template <std::size_t Index>
struct AggregateSizeTag : AggregateSizeTag<Index - 1> {};

template <>
struct AggregateSizeTag<0> {};

// clang-format off
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<32>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 32; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<31>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 31; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<30>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 30; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<29>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 29; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<28>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 28; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<27>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 27; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<26>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 26; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<25>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 25; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<24>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 24; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<23>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 23; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<22>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 22; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<21>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 21; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<20>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 20; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<19>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 19; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<18>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 18; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<17>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 17; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<16>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 16; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<15>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 15; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<14>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 14; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<13>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 13; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<12>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 12; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<11>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 11; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag<10>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 10; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 9>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 9; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 8>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 8; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 7>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 7; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 6>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 6; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 5>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 5; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 4>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}, Init{}}, std::size_t{}) { return 4; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 3>) noexcept -> decltype(Aggregate{Init{}, Init{}, Init{}}, std::size_t{}) { return 3; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 2>) noexcept -> decltype(Aggregate{Init{}, Init{}}, std::size_t{}) { return 2; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 1>) noexcept -> decltype(Aggregate{Init{}}, std::size_t{}) { return 1; }
template <typename Aggregate> [[nodiscard]] consteval auto getAggregateSize(AggregateSizeTag< 0>) noexcept -> decltype(Aggregate{}, std::size_t{}) { return 0; }
// clang-format on

template <typename T>
[[nodiscard]] consteval StringView getTypeDependentFunctionName() noexcept {
	return std::source_location::current().function_name();
}

template <auto Value>
[[nodiscard]] consteval StringView getValueDependentFunctionName() noexcept {
	return std::source_location::current().function_name();
}

template <typename T>
struct Reference {
	T& reference;
};

template <typename T>
struct DummyWrapper {
	const T value;
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"
#endif
template <typename T>
GREM_EXPORT extern const DummyWrapper<T> DUMMY_VALUE_WRAPPER;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

template <typename T>
[[nodiscard]] consteval const T& getDummyValue() noexcept {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif
	return DUMMY_VALUE_WRAPPER<T>.value;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

struct PrimitiveTypeNameExtraction {
private:
	static constexpr StringView TEST_TYPE_NAME = "char";
	static constexpr StringView TEST_DEPENDENT_FUNCTION_NAME = getTypeDependentFunctionName<char>();

public:
	static constexpr size_t OFFSET = TEST_DEPENDENT_FUNCTION_NAME.find(TEST_TYPE_NAME);
	static constexpr StringView SUFFIX = TEST_DEPENDENT_FUNCTION_NAME.substr(OFFSET + TEST_TYPE_NAME.size());
};

struct ClassTypeNameExtraction {
private:
	static constexpr StringView TEST_TYPE_NAME = "GREM_private_MetaReflectionStruct";
	static constexpr StringView TEST_DEPENDENT_FUNCTION_NAME = getTypeDependentFunctionName<GREM_private_MetaReflectionStruct>();

public:
	static constexpr size_t OFFSET = [] {
		const size_t i = TEST_DEPENDENT_FUNCTION_NAME.find(TEST_TYPE_NAME);
		if (i > 6 && TEST_DEPENDENT_FUNCTION_NAME.substr(i - 6).starts_with("class ")) {
			return i - 6;
		}
		if (i > 7 && TEST_DEPENDENT_FUNCTION_NAME.substr(i - 7).starts_with("struct ")) {
			return i - 7;
		}
		return i;
	}();
	static constexpr StringView SUFFIX = TEST_DEPENDENT_FUNCTION_NAME.substr(TEST_DEPENDENT_FUNCTION_NAME.find(TEST_TYPE_NAME) + TEST_TYPE_NAME.size());
};

struct EnumTypeNameExtraction {
private:
	static constexpr StringView TEST_TYPE_NAME = "GREM_private_MetaReflectionEnum";
	static constexpr StringView TEST_DEPENDENT_FUNCTION_NAME = getTypeDependentFunctionName<GREM_private_MetaReflectionEnum>();

public:
	static constexpr size_t OFFSET = [] {
		const size_t i = TEST_DEPENDENT_FUNCTION_NAME.find(TEST_TYPE_NAME);
		if (i > 5 && TEST_DEPENDENT_FUNCTION_NAME.substr(i - 5).starts_with("enum ")) {
			return i - 5;
		}
		return i;
	}();
	static constexpr StringView SUFFIX = TEST_DEPENDENT_FUNCTION_NAME.substr(TEST_DEPENDENT_FUNCTION_NAME.find(TEST_TYPE_NAME) + TEST_TYPE_NAME.size());
};

template <typename T>
struct TypeNameExtraction : PrimitiveTypeNameExtraction {};

template <typename Class>
requires std::is_class_v<Class> struct TypeNameExtraction<Class> : ClassTypeNameExtraction {};

template <enumeration Enum>
struct TypeNameExtraction<Enum> : EnumTypeNameExtraction {};

struct AggregateFieldNameExtraction {
private:
	static constexpr StringView TEST_FIELD_NAME = "GREM_private_metaReflectionField";
	static constexpr StringView TEST_DEPENDENT_FUNCTION_NAME =
		getValueDependentFunctionName<Reference{getDummyValue<GREM_private_MetaReflectionStruct>().GREM_private_metaReflectionField}>();
	static constexpr size_t TEST_OFFSET = TEST_DEPENDENT_FUNCTION_NAME.find(TEST_FIELD_NAME);

public:
	static constexpr char PREFIX = TEST_DEPENDENT_FUNCTION_NAME[TEST_OFFSET - 1];
	static constexpr StringView SUFFIX = TEST_DEPENDENT_FUNCTION_NAME.substr(TEST_OFFSET + TEST_FIELD_NAME.size());
};

struct EnumerandNameExtraction {
private:
	static constexpr StringView TEST_ENUMERAND_NAME = "GREM_private_MetaReflectionEnum::GREM_PRIVATE_META_REFLECTION_ENUMERAND";
	static constexpr StringView TEST_DEPENDENT_FUNCTION_NAME = getValueDependentFunctionName<GREM_private_MetaReflectionEnum::GREM_PRIVATE_META_REFLECTION_ENUMERAND>();

public:
	static constexpr size_t OFFSET = TEST_DEPENDENT_FUNCTION_NAME.find(TEST_ENUMERAND_NAME);
	static constexpr size_t SUFFIX_LENGTH = TEST_DEPENDENT_FUNCTION_NAME.size() - OFFSET - TEST_ENUMERAND_NAME.size();
};

template <typename R, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (*)(Args...));

template <typename R, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (*)(Args...) noexcept);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (U::*)(Args...));

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (U::*)(Args...) noexcept);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (U::*)(Args...) const);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval TypeList<Args...> getFunctionParameterTypes(R (U::*)(Args...) const noexcept);

template <typename Function>
[[nodiscard]] consteval auto getFunctionParameterTypes(Function) -> decltype(getFunctionParameterTypes(&Function::operator()));

template <typename R, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (*)(Args...));

template <typename R, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (*)(Args...) noexcept);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (U::*)(Args...));

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (U::*)(Args...) noexcept);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (U::*)(Args...) const);

template <typename R, typename U, typename... Args>
[[nodiscard]] consteval R getFunctionReturnType(R (U::*)(Args...) const noexcept);

template <typename Function>
[[nodiscard]] consteval auto getFunctionReturnType(Function) -> decltype(getFunctionReturnType(&Function::operator()));

template <typename... Ts>
[[nodiscard]] consteval auto getVariantAlternativeTypes(const Variant<Ts...>&) -> TypeList<Ts...>;

template <typename... Ts>
[[nodiscard]] consteval auto getTupleElementTypes(const Tuple<Ts...>&) -> TypeList<Ts...>;

template <typename... Ts>
[[nodiscard]] consteval Tuple<Ts...> getTupleOfVariantAlternatives(const Variant<Ts...>&);

} // namespace detail

/// \cond
template <aggregate Aggregate>
struct aggregate_size : std::integral_constant<std::size_t, detail::getAggregateSize<Aggregate>(detail::AggregateSizeTag<32>{})> {};
/// \endcond

/**
 * The number of fields in a given aggregate type.
 *
 * \tparam T aggregate type to get the number of fields in.
 *
 * \warning Only sizes up to 32 are supported. If the template is instantiated
 *          with an aggregate type containing more than 32 fields, the program
 *          is ill-formed.
 */
template <typename Aggregate>
inline constexpr std::size_t aggregate_size_v = aggregate_size<Aggregate>::value;

/**
 * Execute a function for each field index of an aggregate.
 *
 * \tparam Aggregate type to iterate the field indices of.
 *
 * \param fn function to execute. Must accept an object of type
 *        `meta::Constant<I>` as a parameter, where I is any integer from 0 up
 *        to, but not including, the number of fields in the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachFieldIndex(auto fn) {
	static_assert(aggregate<Aggregate>);
	forEachIndex<aggregate_size_v<Aggregate>>(fn);
}

namespace detail {

[[nodiscard]] consteval auto getAggregateFieldTypes(auto&& value) noexcept {
	using Aggregate = std::remove_cvref_t<decltype(value)>;
	static_assert(aggregate<Aggregate>);
	constexpr std::size_t FIELD_COUNT = aggregate_size_v<Aggregate>;
	if constexpr (FIELD_COUNT == 32) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae, af] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa), decltype(ab), decltype(ac), decltype(ad), decltype(ae), decltype(af)>;
	} else if constexpr (FIELD_COUNT == 31) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa), decltype(ab), decltype(ac), decltype(ad), decltype(ae)>;
	} else if constexpr (FIELD_COUNT == 30) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa), decltype(ab), decltype(ac), decltype(ad)>;
	} else if constexpr (FIELD_COUNT == 29) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa), decltype(ab), decltype(ac)>;
	} else if constexpr (FIELD_COUNT == 28) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa), decltype(ab)>;
	} else if constexpr (FIELD_COUNT == 27) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z), decltype(aa)>;
	} else if constexpr (FIELD_COUNT == 26) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x), decltype(y),
			decltype(z)>;
	} else if constexpr (FIELD_COUNT == 25) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x),
			decltype(y)>;
	} else if constexpr (FIELD_COUNT == 24) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w), decltype(x)>;
	} else if constexpr (FIELD_COUNT == 23) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v), decltype(w)>;
	} else if constexpr (FIELD_COUNT == 22) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u), decltype(v)>;
	} else if constexpr (FIELD_COUNT == 21) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t), decltype(u)>;
	} else if constexpr (FIELD_COUNT == 20) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s), decltype(t)>;
	} else if constexpr (FIELD_COUNT == 19) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r), decltype(s)>;
	} else if constexpr (FIELD_COUNT == 18) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q), decltype(r)>;
	} else if constexpr (FIELD_COUNT == 17) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p), decltype(q)>;
	} else if constexpr (FIELD_COUNT == 16) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o), decltype(p)>;
	} else if constexpr (FIELD_COUNT == 15) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n), decltype(o)>;
	} else if constexpr (FIELD_COUNT == 14) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m), decltype(n)>;
	} else if constexpr (FIELD_COUNT == 13) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k), decltype(l),
			decltype(m)>;
	} else if constexpr (FIELD_COUNT == 12) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k, l] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k),
			decltype(l)>;
	} else if constexpr (FIELD_COUNT == 11) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j, k] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k)>;
	} else if constexpr (FIELD_COUNT == 10) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i, j] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j)>;
	} else if constexpr (FIELD_COUNT == 9) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h, i] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i)>;
	} else if constexpr (FIELD_COUNT == 8) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g, h] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h)>;
	} else if constexpr (FIELD_COUNT == 7) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f, g] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g)>;
	} else if constexpr (FIELD_COUNT == 6) {
		[[maybe_unused]] auto&& [a, b, c, d, e, f] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f)>;
	} else if constexpr (FIELD_COUNT == 5) {
		[[maybe_unused]] auto&& [a, b, c, d, e] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d), decltype(e)>;
	} else if constexpr (FIELD_COUNT == 4) {
		[[maybe_unused]] auto&& [a, b, c, d] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c), decltype(d)>;
	} else if constexpr (FIELD_COUNT == 3) {
		[[maybe_unused]] auto&& [a, b, c] = value;
		return TYPE_LIST<decltype(a), decltype(b), decltype(c)>;
	} else if constexpr (FIELD_COUNT == 2) {
		[[maybe_unused]] auto&& [a, b] = value;
		return TYPE_LIST<decltype(a), decltype(b)>;
	} else if constexpr (FIELD_COUNT == 1) {
		[[maybe_unused]] auto&& [a] = value;
		return TYPE_LIST<decltype(a)>;
	} else {
		return TYPE_LIST<>;
	}
}

} // namespace detail

/// \cond
template <aggregate Aggregate>
struct aggregate_field_types {
	using type = decltype(detail::getAggregateFieldTypes(std::declval<Aggregate>()));
};
/// \endcond

/**
 * Type list containing the types of the fields of a given aggregate type.
 *
 * \tparam Aggregate aggregate type to get the field types of.
 *
 * \warning Only sizes up to 32 are supported. If the template is instantiated
 *          with an aggregate type containing more than 32 fields, the program
 *          is ill-formed.
 */
template <typename Aggregate>
using aggregate_field_types_t = typename aggregate_field_types<Aggregate>::type;

/// \cond
template <std::size_t Index, aggregate Aggregate>
struct aggregate_field_type {
	using type = type_list_type_t<aggregate_field_types_t<Aggregate>, Index>;
};
/// \endcond

/**
 * Type of a given field of an aggregate type.
 *
 * \tparam Index index of the field to get the type of.
 * \tparam Aggregate aggregate type to get the field type of.
 *
 * \warning Only sizes up to 32 are supported. If the template is instantiated
 *          with an aggregate type containing more than 32 fields, the program
 *          is ill-formed.
 */
template <std::size_t Index, typename Aggregate>
using aggregate_field_type_t = typename aggregate_field_type<Index, Aggregate>::type;

/**
 * Execute a function for each field type of an aggregate.
 *
 * \tparam Aggregate type to iterate the field types of.
 *
 * \param fn function to execute. Must accept an object of type `meta::Type<T>`
 *        as a parameter, where T is any field type of the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachFieldType(auto fn) {
	static_assert(aggregate<Aggregate>);
	forEachType<aggregate_field_types_t<Aggregate>>(fn);
}

/**
 * Execute a function for each field type of an aggregate, also including the
 * field index.
 *
 * \tparam Aggregate type to iterate the indexed field types of.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, the number of fields in the aggregate.
 *        - an object of type `meta::Type<T>`, where T is any field type of the
 *          aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachIndexedFieldType(auto fn) {
	forEachFieldIndex<Aggregate>([fn](auto index) -> void { //
		fn(index, TYPE<aggregate_field_type_t<index, Aggregate>>);
	});
}

/**
 * Get a tuple of references to each of the fields of an aggregate.
 *
 * \param value forwarding reference to an object of an aggregate type.
 *
 * \return a tuple where each element is an lvalue reference to the respective
 *         field of the aggregate, based on the declaration order of the fields.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 */
[[nodiscard]] constexpr auto getFields(auto&& value) noexcept {
	using Aggregate = std::remove_cvref_t<decltype(value)>;
	static_assert(aggregate<Aggregate>);
	constexpr std::size_t FIELD_COUNT = aggregate_size_v<Aggregate>;
	if constexpr (FIELD_COUNT == 32) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae, af] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae, af);
	} else if constexpr (FIELD_COUNT == 31) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad, ae);
	} else if constexpr (FIELD_COUNT == 30) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac, ad);
	} else if constexpr (FIELD_COUNT == 29) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab, ac);
	} else if constexpr (FIELD_COUNT == 28) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa, ab);
	} else if constexpr (FIELD_COUNT == 27) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, aa);
	} else if constexpr (FIELD_COUNT == 26) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z);
	} else if constexpr (FIELD_COUNT == 25) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y);
	} else if constexpr (FIELD_COUNT == 24) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x);
	} else if constexpr (FIELD_COUNT == 23) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w);
	} else if constexpr (FIELD_COUNT == 22) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v);
	} else if constexpr (FIELD_COUNT == 21) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u);
	} else if constexpr (FIELD_COUNT == 20) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t);
	} else if constexpr (FIELD_COUNT == 19) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s);
	} else if constexpr (FIELD_COUNT == 18) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r);
	} else if constexpr (FIELD_COUNT == 17) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q);
	} else if constexpr (FIELD_COUNT == 16) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
	} else if constexpr (FIELD_COUNT == 15) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
	} else if constexpr (FIELD_COUNT == 14) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n);
	} else if constexpr (FIELD_COUNT == 13) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l, m);
	} else if constexpr (FIELD_COUNT == 12) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k, l] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k, l);
	} else if constexpr (FIELD_COUNT == 11) {
		auto&& [a, b, c, d, e, f, g, h, i, j, k] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j, k);
	} else if constexpr (FIELD_COUNT == 10) {
		auto&& [a, b, c, d, e, f, g, h, i, j] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i, j);
	} else if constexpr (FIELD_COUNT == 9) {
		auto&& [a, b, c, d, e, f, g, h, i] = value;
		return grem::tie(a, b, c, d, e, f, g, h, i);
	} else if constexpr (FIELD_COUNT == 8) {
		auto&& [a, b, c, d, e, f, g, h] = value;
		return grem::tie(a, b, c, d, e, f, g, h);
	} else if constexpr (FIELD_COUNT == 7) {
		auto&& [a, b, c, d, e, f, g] = value;
		return grem::tie(a, b, c, d, e, f, g);
	} else if constexpr (FIELD_COUNT == 6) {
		auto&& [a, b, c, d, e, f] = value;
		return grem::tie(a, b, c, d, e, f);
	} else if constexpr (FIELD_COUNT == 5) {
		auto&& [a, b, c, d, e] = value;
		return grem::tie(a, b, c, d, e);
	} else if constexpr (FIELD_COUNT == 4) {
		auto&& [a, b, c, d] = value;
		return grem::tie(a, b, c, d);
	} else if constexpr (FIELD_COUNT == 3) {
		auto&& [a, b, c] = value;
		return grem::tie(a, b, c);
	} else if constexpr (FIELD_COUNT == 2) {
		auto&& [a, b] = value;
		return grem::tie(a, b);
	} else if constexpr (FIELD_COUNT == 1) {
		auto&& [a] = value;
		return grem::tie(a);
	} else {
		return grem::tie();
	}
}

/**
 * Execute a function for each field of an aggregate.
 *
 * \param value forwarding reference to an object of an aggregate type.
 * \param fn function to execute. Must accept a reference to each field type of
 *        the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachField(Aggregate&& value, auto fn) {
	forEach(getFields(std::forward<Aggregate>(value)), fn);
}

/**
 * Execute a function for each field of an aggregate, also including the field
 * index.
 *
 * \param value forwarding reference to an object of an aggregate type.
 * \param fn function to execute. Must accept the two following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, the number of fields in the aggregate.
 *        - a reference to each field type of the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachIndexedField(Aggregate&& value, auto fn) {
	auto&& fields = getFields(std::forward<Aggregate>(value));
	forEachIndex<meta::aggregate_size_v<std::remove_cvref_t<Aggregate>>>([&fields, fn](auto index) -> void { //
		fn(index, get<index>(fields));
	});
}

namespace detail {

// clang-format off
template <typename T> struct remove_qualifiers { using type = T; };
template <typename T> struct remove_qualifiers<const T> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<volatile T> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const volatile T> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<T&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const T&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<volatile T&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const volatile T&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<T&&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const T&&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<volatile T&&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const volatile T&&> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<T*> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const T*> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<volatile T*> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const volatile T*> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<T[]> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const T[]> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<volatile T[]> : remove_qualifiers<T> {};
template <typename T> struct remove_qualifiers<const volatile T[]> : remove_qualifiers<T> {};
template <typename T, size_t N> struct remove_qualifiers<T[N]> : remove_qualifiers<T> {};
template <typename T, size_t N> struct remove_qualifiers<const T[N]> : remove_qualifiers<T> {};
template <typename T, size_t N> struct remove_qualifiers<volatile T[N]> : remove_qualifiers<T> {};
template <typename T, size_t N> struct remove_qualifiers<const volatile T[N]> : remove_qualifiers<T> {};
// clang-format on

template <typename T>
using remove_qualifiers_t = typename remove_qualifiers<T>::type;

[[nodiscard]] constexpr StringView extractUnqualifiedTypeName(StringView name, size_t offset, StringView suffix) {
	name.remove_prefix(offset);
	name = name.substr(0, name.rfind(suffix));
	while (true) {
		while (name.starts_with(' ')) {
			name.remove_prefix(1);
		}
		if (name.starts_with("const")) {
			name.remove_prefix(5);
		} else if (name.starts_with("volatile")) {
			name.remove_prefix(8);
		} else if (name.starts_with("struct")) {
			name.remove_prefix(6);
		} else if (name.starts_with("class")) {
			name.remove_prefix(5);
		} else if (name.starts_with("enum")) {
			name.remove_prefix(4);
		} else {
			break;
		}
	}
	name.remove_prefix(name.rfind(':') + 1);
	name = name.substr(0, name.find_first_of(" <[*&"));
	return name;
}

template <typename T>
[[nodiscard]] constexpr StringView getUnqualifiedTypeName() noexcept {
	using X = remove_qualifiers_t<T>;
	if constexpr (std::is_same_v<X, signed char>) {
		return "signed char";
	} else if constexpr (std::is_same_v<X, unsigned char>) {
		return "unsigned char";
	} else if constexpr (std::is_same_v<X, short>) {
		return "short";
	} else if constexpr (std::is_same_v<X, unsigned short>) {
		return "unsigned short";
	} else if constexpr (std::is_same_v<X, int>) {
		return "int";
	} else if constexpr (std::is_same_v<X, unsigned int>) {
		return "unsigned int";
	} else if constexpr (std::is_same_v<X, long>) {
		return "long";
	} else if constexpr (std::is_same_v<X, unsigned long>) {
		return "unsigned long";
	} else if constexpr (std::is_same_v<X, long long>) {
		return "long long";
	} else if constexpr (std::is_same_v<X, unsigned long long>) {
		return "unsigned long long";
	} else if constexpr (std::is_same_v<X, char>) {
		return "char";
	} else if constexpr (std::is_same_v<X, wchar_t>) {
		return "wchar_t";
	} else if constexpr (std::is_same_v<X, char8_t>) {
		return "char8_t";
	} else if constexpr (std::is_same_v<X, char16_t>) {
		return "char16_t";
	} else if constexpr (std::is_same_v<X, char32_t>) {
		return "char32_t";
	} else if constexpr (std::is_same_v<X, float>) {
		return "float";
	} else if constexpr (std::is_same_v<X, double>) {
		return "double";
	} else if constexpr (std::is_same_v<X, long double>) {
		return "long double";
	} else {
		using Extraction = TypeNameExtraction<X>;
		constexpr StringView NAME = extractUnqualifiedTypeName(getTypeDependentFunctionName<X>(), Extraction::OFFSET, Extraction::SUFFIX);
		if (std::is_constant_evaluated()) {
			return NAME;
		}
		return [NAME]() -> StringView {
			static constexpr ConstantString<char, NAME.size()> RESULT = NAME;
			return RESULT;
		}();
	}
}

[[nodiscard]] constexpr StringView extractAggregateFieldName(StringView name, char prefix, StringView suffix) {
	name = name.substr(0, name.find(suffix));
	name.remove_prefix(name.rfind(prefix) + 1);
	return name;
}

template <std::size_t Index, typename Aggregate>
[[nodiscard]] constexpr StringView getAggregateFieldName() noexcept {
	using Extraction = AggregateFieldNameExtraction;
	constexpr StringView NAME = extractAggregateFieldName(getValueDependentFunctionName<Reference{get<Index>(getFields(getDummyValue<std::remove_cvref_t<Aggregate>>()))}>(),
		Extraction::PREFIX, Extraction::SUFFIX);
	if (std::is_constant_evaluated()) {
		return NAME;
	}
	return [NAME]() -> StringView {
		static constexpr ConstantString<char, NAME.size()> RESULT = NAME;
		return RESULT;
	}();
}

[[nodiscard]] constexpr StringView extractEnumerandFieldName(StringView name, size_t offset, size_t suffixLength) {
	name.remove_prefix(offset);
	name.remove_suffix(suffixLength);
	name.remove_prefix(name.rfind(':') + 1);
	return name;
}

template <std::size_t Index, typename Enum>
[[nodiscard]] constexpr StringView getEnumerandNameAt() noexcept {
	using Extraction = EnumerandNameExtraction;
	constexpr StringView NAME = extractEnumerandFieldName(getValueDependentFunctionName<std::remove_cvref_t<Enum>{Index}>(), Extraction::OFFSET, Extraction::SUFFIX_LENGTH);
	if (std::is_constant_evaluated()) {
		return NAME;
	}
	return [NAME]() -> StringView {
		static constexpr ConstantString<char, NAME.size()> RESULT = NAME;
		return RESULT;
	}();
}

struct TestStruct {
	StringView myFieldABC;
	std::size_t myField123;
};

enum TestEnum : int { // NOLINT(performance-enum-size)
	MY_ENUMERAND_ABC,
	MY_ENUMERAND_123,
};

enum class TestEnumClass : int { // NOLINT(performance-enum-size)
	MY_ENUMERAND_ABC,
	MY_ENUMERAND_123,
};

template <typename Target, typename Object>
[[nodiscard]] consteval Target getMemberPointerTarget(Target Object::*) noexcept;

template <typename Target, typename Object>
[[nodiscard]] consteval Object getMemberPointerObject(Target Object::*) noexcept;

} // namespace detail

/// \cond
template <typename T>
struct unqualified_type_name {
	using value_type = StringView;
	using type = unqualified_type_name;

	static constexpr StringView value = detail::getUnqualifiedTypeName<T>();

	constexpr operator value_type() const noexcept {
		return value;
	}

	[[nodiscard]] constexpr value_type operator()() const noexcept {
		return value;
	}
};
/// \endcond

/**
 * Unqualified name of a type.
 *
 * \tparam T type to get the name of.
 *
 * \note The returned type name will not contain template arguments, namespace
 *       specifiers, cv-qualifiers, reference qualifiers, pointer qualifiers or
 *       array extents.
 */
template <typename T>
inline constexpr auto unqualified_type_name_v = ConstantString<char, unqualified_type_name<T>::value.size()>{unqualified_type_name<T>::value};

static_assert(unqualified_type_name_v<int> == "int");
static_assert(unqualified_type_name_v<const volatile int* const* volatile* const volatile*&&> == "int");
static_assert(unqualified_type_name_v<detail::TestStruct> == "TestStruct");
static_assert(unqualified_type_name_v<const volatile detail::TestStruct* const* volatile* const volatile*&&> == "TestStruct");
static_assert(unqualified_type_name_v<detail::TestEnum> == "TestEnum");
static_assert(unqualified_type_name_v<const volatile detail::TestEnum* const* volatile* const volatile*&&> == "TestEnum");
static_assert(unqualified_type_name_v<detail::TestEnumClass> == "TestEnumClass");
static_assert(unqualified_type_name_v<const volatile detail::TestEnumClass* const* volatile* const volatile*&&> == "TestEnumClass");

/// \cond
template <std::size_t Index, aggregate Aggregate>
requires default_initializable<Aggregate> //
struct aggregate_field_name {
	using value_type = StringView;
	using type = aggregate_field_name;

	static constexpr StringView value = detail::getAggregateFieldName<Index, Aggregate>();

	constexpr operator value_type() const noexcept {
		return value;
	}

	[[nodiscard]] constexpr value_type operator()() const noexcept {
		return value;
	}
};
/// \endcond

/**
 * Name of the field at a given index of an aggregate type.
 *
 * \tparam Index index of the field to get the name of.
 * \tparam Aggregate default-constructible aggregate type containing the field
 *         to get the name of.
 *
 * \warning Only sizes up to 32 are supported. If the template is instantiated
 *          with an aggregate type containing more than 32 fields, the program
 *          is ill-formed.
 */
template <std::size_t Index, typename Aggregate>
inline constexpr auto aggregate_field_name_v = ConstantString<char, aggregate_field_name<Index, Aggregate>::value.size()>{aggregate_field_name<Index, Aggregate>::value};

static_assert(aggregate_field_name_v<0, GREM_private_MetaReflectionStruct> == "GREM_private_metaReflectionField");
static_assert(aggregate_field_name_v<0, detail::TestStruct> == "myFieldABC");
static_assert(aggregate_field_name_v<1, detail::TestStruct> == "myField123");

/**
 * Execute a function for each field index of an aggregate, also including the
 * field name.
 *
 * \tparam Aggregate type to iterate the named field indices of.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - a StringView of each field name.
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, the number of fields in the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachNamedFieldIndex(auto fn) {
	forEachFieldIndex<Aggregate>([fn](auto index) -> void { //
		fn(StringView{aggregate_field_name_v<index, Aggregate>}, index);
	});
}

/**
 * Execute a function for each field type of an aggregate, also including the
 * field name.
 *
 * \tparam Aggregate type to iterate the named field types of.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - a StringView of each field name.
 *        - an object of type `meta::Type<T>`, where T is any field type of the
 *          aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachNamedFieldType(auto fn) {
	forEachFieldIndex<Aggregate>([fn](auto index) -> void { //
		fn(StringView{aggregate_field_name_v<index, Aggregate>}, TYPE<aggregate_field_type_t<index, Aggregate>>);
	});
}

/**
 * Execute a function for each field type of an aggregate, also including the
 * field name and index.
 *
 * \tparam Aggregate type to iterate the indexed named field types of.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, the number of fields in the aggregate.
 *        - a StringView of each field name.
 *        - an object of type `meta::Type<T>`, where T is any field type of the
 *          aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachIndexedNamedFieldType(auto fn) {
	forEachFieldIndex<Aggregate>([fn](auto index) -> void { //
		fn(index, StringView{aggregate_field_name_v<index, Aggregate>}, TYPE<aggregate_field_type_t<index, Aggregate>>);
	});
}

/**
 * Execute a function for each field of an aggregate, also including the field
 * name.
 *
 * \param value forwarding reference to an object of an aggregate type.
 * \param fn function to execute. Must accept the two following parameters:
 *        - a StringView of each field name.
 *        - a reference to each field type of the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachNamedField(Aggregate&& value, auto fn) {
	using A = std::remove_cvref_t<Aggregate>;
	auto&& fields = getFields(std::forward<Aggregate>(value));
	forEachIndex<meta::aggregate_size_v<A>>([&fields, fn](auto index) -> void { //
		fn(StringView{aggregate_field_name_v<index, A>}, get<index>(fields));
	});
}

/**
 * Execute a function for each field of an aggregate, also including the field
 * index and name.
 *
 * \param value forwarding reference to an object of an aggregate type.
 * \param fn function to execute. Must accept the three following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, the number of fields in the aggregate.
 *        - a StringView of each field name.
 *        - a reference to each field type of the aggregate.
 *
 * \warning Only aggregate sizes up to 32 are supported. If the function is
 *          instantiated with an aggregate type containing more than 32 fields,
 *          the program is ill-formed.
 *
 * \throws any exception thrown by fn.
 */
template <typename Aggregate>
constexpr void forEachIndexedNamedField(Aggregate&& value, auto fn) {
	using A = std::remove_cvref_t<Aggregate>;
	auto&& fields = getFields(std::forward<Aggregate>(value));
	forEachIndex<meta::aggregate_size_v<A>>([&fields, fn](auto index) -> void { //
		fn(index, StringView{aggregate_field_name_v<index, A>}, get<index>(fields));
	});
}

/// \cond
template <std::size_t Index, enumeration Enum>
struct enumerand_name_at {
	using value_type = StringView;
	using type = enumerand_name_at;

	static constexpr StringView value = detail::getEnumerandNameAt<Index, Enum>();

	constexpr operator value_type() const noexcept {
		return value;
	}

	[[nodiscard]] constexpr value_type operator()() const noexcept {
		return value;
	}
};
/// \endcond

/**
 * Name of the enumerand at a given index of an enum type.
 *
 * \tparam Index index of the enumerand to get the name of. Must be a valid
 *         index for the given enum type.
 * \tparam Enum enum type containing the enumerand to get the name of. Must have
 *         a fixed underlying type.
 */
template <std::size_t Index, typename Enum>
inline constexpr auto enumerand_name_at_v = ConstantString<char, enumerand_name_at<Index, Enum>::value.size()>{enumerand_name_at<Index, Enum>::value};

static_assert(enumerand_name_at_v<0, GREM_private_MetaReflectionEnum> == "GREM_PRIVATE_META_REFLECTION_ENUMERAND");
static_assert(enumerand_name_at_v<0, detail::TestEnum> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_at_v<1, detail::TestEnum> == "MY_ENUMERAND_123");
static_assert(enumerand_name_at_v<0, detail::TestEnumClass> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_at_v<1, detail::TestEnumClass> == "MY_ENUMERAND_123");

/// \cond
template <auto Enumerand>
struct enumerand_name {
	static_assert(enumeration<decltype(Enumerand)>, "enumerand_name argument must be a value of an enum type.");
};

template <auto Enumerand>
requires enumeration<decltype(Enumerand)>
struct enumerand_name<Enumerand> : enumerand_name_at<static_cast<size_t>(static_cast<std::underlying_type_t<decltype(Enumerand)>>(Enumerand)), decltype(Enumerand)> {};

template <auto Enumerand>
requires enumeration<std::remove_cvref_t<decltype(decltype(Enumerand)::value)>> struct enumerand_name<Enumerand> : enumerand_name<decltype(Enumerand)::value> {};
/// \endcond

/**
 * Name of a given enumerand of an enum type.
 *
 * \tparam Enumerand value of the enumerand to get the name of.
 */
template <auto Enumerand>
inline constexpr auto enumerand_name_v = ConstantString<char, enumerand_name<Enumerand>::value.size()>{enumerand_name<Enumerand>::value};

static_assert(enumerand_name_v<GREM_private_MetaReflectionEnum::GREM_PRIVATE_META_REFLECTION_ENUMERAND> == "GREM_PRIVATE_META_REFLECTION_ENUMERAND");
static_assert(enumerand_name_v<detail::MY_ENUMERAND_ABC> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_v<detail::MY_ENUMERAND_123> == "MY_ENUMERAND_123");
static_assert(enumerand_name_v<detail::TestEnumClass::MY_ENUMERAND_ABC> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_v<detail::TestEnumClass::MY_ENUMERAND_123> == "MY_ENUMERAND_123");

static_assert(enumerand_name_v<CONSTANT<GREM_private_MetaReflectionEnum::GREM_PRIVATE_META_REFLECTION_ENUMERAND>> == "GREM_PRIVATE_META_REFLECTION_ENUMERAND");
static_assert(enumerand_name_v<CONSTANT<detail::MY_ENUMERAND_ABC>> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_v<CONSTANT<detail::MY_ENUMERAND_123>> == "MY_ENUMERAND_123");
static_assert(enumerand_name_v<CONSTANT<detail::TestEnumClass::MY_ENUMERAND_ABC>> == "MY_ENUMERAND_ABC");
static_assert(enumerand_name_v<CONSTANT<detail::TestEnumClass::MY_ENUMERAND_123>> == "MY_ENUMERAND_123");

namespace detail {

template <typename Enum, std::size_t I>
[[nodiscard]] constexpr std::size_t getEnumSize() noexcept {
	if constexpr (enumerand_name_at_v<I, Enum>.find(')') == StringView::npos) {
		return getEnumSize<Enum, I + 1>();
	} else {
		return I;
	}
}

} // namespace detail

/// \cond
template <enumeration Enum>
struct enum_size : std::integral_constant<std::size_t, detail::getEnumSize<Enum, 0>()> {};
/// \endcond

/**
 * The number of consecutive valid enumerands starting at value 0 in a given
 * enum type.
 *
 * \tparam T enum type to get the number of enumerands in.
 */
template <typename Enum>
inline constexpr std::size_t enum_size_v = enum_size<Enum>::value;

static_assert(enum_size_v<GREM_private_MetaReflectionEnum> == 1);
static_assert(enum_size_v<detail::TestEnum> == 2);
static_assert(enum_size_v<detail::TestEnumClass> == 2);

/**
 * Execute a function associated with one specific enumerand in the sequence of
 * valid enumerands starting at value 0 in a given enum type.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param value the enumerand to execute the corresponding function overload of.
 * \param fn function to execute. Must accept an object of type
 *        `meta::Constant<Enumerand>` as a parameter, where Enumerand is any
 *        enumeration value from 0 up to, but not including,
 *        `meta::enum_size_v<Enum>`.
 *
 * \return the result of fn.
 *
 * \throws std::out_of_range if
 *         `static_cast<std::size_t>(value) >= meta::enum_size_v<Enum>`.
 * \throws any exception thrown by fn.
 */
template <enumeration Enum>
constexpr decltype(auto) visitEnumerand(Enum value, auto fn) {
	return visitIndex<meta::enum_size_v<Enum>>(static_cast<size_t>(value), [fn](auto index) -> decltype(auto) { return fn(CONSTANT<Enum{index()}>); });
}

/**
 * Execute a function for each consecutive valid enumerand starting at
 * value 0 in a given enum type.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param fn function to execute. Must accept an object of type
 *        `meta::Constant<Enumerand>` as a parameter, where Enumerand is any
 *        enumeration value from 0 up to, but not including,
 *        `meta::enum_size_v<Enum>`.
 *
 * \throws any exception thrown by fn.
 */
template <enumeration Enum>
constexpr void forEachEnumerand(auto fn) {
	[fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
		(fn(CONSTANT<Enum{Indices}>), ...);
	}(std::make_index_sequence<enum_size_v<Enum>>{});
}

/**
 * Execute a function for each consecutive valid enumerand starting at
 * value 0 in a given enum type, also including the enumerand index.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, `meta::enum_size_v<Enum>`.
 *        - an object of type `meta::Constant<Enumerand>`, where Enumerand is
 *          any enumeration value from 0 up to, but not including,
 *          `meta::enum_size_v<Enum>`.
 *
 * \throws any exception thrown by fn.
 */
template <enumeration Enum>
constexpr void forEachIndexedEnumerand(auto fn) {
	[fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
		(fn(CONSTANT<Indices>, CONSTANT<Enum{Indices}>), ...);
	}(std::make_index_sequence<enum_size_v<Enum>>{});
}

/**
 * Execute a function for each consecutive valid enumerand starting at
 * value 0 in a given enum type, also including the enumerand name.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param fn function to execute. Must accept the two following parameters:
 *        - a StringView of each enumerand name.
 *        - an object of type `meta::Constant<Enumerand>`, where Enumerand is
 *          any enumeration value from 0 up to, but not including,
 *          `meta::enum_size_v<Enum>`.
 *
 * \throws any exception thrown by fn.
 */
template <enumeration Enum>
constexpr void forEachNamedEnumerand(auto fn) {
	[fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
		(fn(StringView{enumerand_name_at_v<Indices, Enum>}, CONSTANT<Enum{Indices}>), ...);
	}(std::make_index_sequence<enum_size_v<Enum>>{});
}

/**
 * Execute a function for each consecutive valid enumerand starting at
 * value 0 in a given enum type, also including the enumerand index and name.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param fn function to execute. Must accept the three following parameters:
 *        - an object of type `meta::Constant<I>`, where I is any integer from 0
 *          up to, but not including, `meta::enum_size_v<Enum>`.
 *        - a StringView of each enumerand name.
 *        - an object of type `meta::Constant<Enumerand>`, where Enumerand is
 *          any enumeration value from 0 up to, but not including,
 *          `meta::enum_size_v<Enum>`.
 *
 * \throws any exception thrown by fn.
 */
template <enumeration Enum>
constexpr void forEachIndexedNamedEnumerand(auto fn) {
	[fn]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
		(fn(CONSTANT<Indices>, StringView{enumerand_name_at_v<Indices, Enum>}, CONSTANT<Enum{Indices}>), ...);
	}(std::make_index_sequence<enum_size_v<Enum>>{});
}

/**
 * Get the name of a specific enumerand in a given enum type.
 *
 * \tparam Enum enum type. Must have a fixed underlying type.
 *
 * \param value the enumerand to get the name of.
 *
 * \return the unqualified name of the given enumerand, with static storage
 *         duration.
 *
 * \throws std::out_of_range if
 *         `static_cast<std::size_t>(value) >= meta::enum_size_v<Enum>`.
 */
template <enumeration Enum>
[[nodiscard]] constexpr CStringView getEnumerandName(Enum value) {
	return visitEnumerand(value, [&]<Enum Enumerand>(Constant<Enumerand>) -> CStringView { return enumerand_name_v<Enumerand>; });
}

/// \cond
template <auto Member>
struct member_pointer_target_type {
	using type = decltype(detail::getMemberPointerTarget(Member));
};
/// \endcond

/**
 * Target member type of a given pointer-to-member.
 *
 * \tparam Pointer to member to get the target type of.
 */
template <auto Member>
using member_pointer_target_type_t = typename member_pointer_target_type<Member>::type;

/// \cond
template <auto Member>
struct member_pointer_object_type {
	using type = decltype(detail::getMemberPointerObject(Member));
};
/// \endcond

/**
 * Object type of a given pointer-to-member.
 *
 * \tparam Pointer to member to get the object type of.
 */
template <auto Member>
using member_pointer_object_type_t = typename member_pointer_object_type<Member>::type;

namespace detail {

template <auto Member>
[[nodiscard]] constexpr size_t getMemberPointerIndex() noexcept {
	using Object = member_pointer_object_type_t<Member>;
	const auto dummyFields = getFields(getDummyValue<Object>());
	const auto dummyMemberPointer = &(getDummyValue<Object>().*Member);
	size_t result = 0;
	forEachIndex<aggregate_size_v<Object>>([&result, &dummyFields, dummyMemberPointer](auto index) -> void {
		if constexpr (same_as<std::remove_cvref_t<decltype(&get<index>(dummyFields))>, std::remove_cvref_t<decltype(dummyMemberPointer)>>) {
			if (&get<index>(dummyFields) == dummyMemberPointer) {
				result = index;
			}
		}
	});
	return result;
}

} // namespace detail

/// \cond
template <auto Member>
struct member_pointer_index : std::integral_constant<size_t, detail::getMemberPointerIndex<Member>()> {};
/// \endcond

/**
 * Index of the field referenced by a given pointer-to-member in its outer
 * struct.
 *
 * \tparam Pointer to member to get the index of. Its object type must be an
 *         aggregate type.
 */
template <auto Member>
inline constexpr size_t member_pointer_index_v = member_pointer_index<Member>::value;

static_assert(member_pointer_index_v<&detail::TestStruct::myFieldABC> == 0);
static_assert(member_pointer_index_v<&detail::TestStruct::myField123> == 1);

/// \cond
template <typename Function>
struct function_parameter_types {
	using type = decltype(detail::getFunctionParameterTypes(std::declval<Function>()));
};
/// \endcond

/**
 * List of parameter types of a given function.
 *
 * \tparam Function function type to get the parameter types of.
 */
template <typename Function>
using function_parameter_types_t = typename function_parameter_types<Function>::type;

/// \cond
template <typename Function>
struct function_return_type {
	using type = decltype(detail::getFunctionReturnType(std::declval<Function>()));
};
/// \endcond

/**
 * Return type of a given function.
 *
 * \tparam Function function type to get the return type of.
 */
template <typename Function>
using function_return_type_t = typename function_return_type<Function>::type;

/// \cond
template <typename V>
struct variant_alternative_types {
	using type = decltype(detail::getVariantAlternativeTypes(std::declval<V>()));
};
/// \endcond

/**
 * List of all alternative types of a given variant.
 *
 * \tparam V variant type to get the alternative types of.
 */
template <typename V>
using variant_alternative_types_t = typename variant_alternative_types<V>::type;

/// \cond
template <typename T>
struct tuple_element_types {
	using type = decltype(detail::getTupleElementTypes(std::declval<T>()));
};
/// \endcond

/**
 * List of all element types of a given tuple.
 *
 * \tparam T tuple type to get the element types of.
 */
template <typename T>
using tuple_element_types_t = typename tuple_element_types<T>::type;

/// \cond
template <typename V>
struct tuple_of_variant_alternatives {
	using type = decltype(detail::getTupleOfVariantAlternatives(std::declval<V>()));
};
/// \endcond

/**
 * Tuple of all alternative types of a given variant.
 *
 * \tparam V variant type to get the alternative types tuple of.
 */
template <typename V>
using tuple_of_variant_alternatives_t = typename tuple_of_variant_alternatives<V>::type;

} // namespace grem::meta

#endif
