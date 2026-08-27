// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_CONCEPTS_HPP
#define GREM_CORE_CONCEPTS_HPP

#include <GREM/build_config.hpp>

#include <iterator>    // std::begin, std::cbegin, std::end, std::cend, std::size, std::data, std::...
#include <type_traits> // std::is_..._v, std::common_reference_t, std::common_type_t, std::add_lvalue_reference_t, std::remove_reference_t, std::false_type, std::true_type
#include <utility>     // std::declval, std::forward

namespace grem {

namespace detail {

template <typename T, template <typename...> typename Template>
struct is_template_specialization : std::false_type {};

template <template <typename...> typename Template, typename... Args>
struct is_template_specialization<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> typename Template>
inline constexpr bool is_template_specialization_v = is_template_specialization<T, Template>::value;

template <template <typename...> typename Template, typename... TemplateArgs>
consteval void derivedFromTemplateSpecializationTest(const Template<TemplateArgs...>&);

template <template <template <typename...> typename...> typename TemplateTemplate, template <typename...> typename... TemplateTemplateArgs>
consteval void derivedFromTemplateTemplateSpecializationTest(const TemplateTemplate<TemplateTemplateArgs...>&);

} // namespace detail

template <typename T, typename U>
concept same_as = std::is_same_v<T, U>;

template <typename T, typename Base>
concept derived_from = std::is_base_of_v<Base, T> && std::is_convertible_v<const volatile T*, const volatile Base*>;

template <typename T, template <typename...> typename Template>
concept template_specialization_of = detail::is_template_specialization_v<T, Template>;

template <typename T, template <typename...> typename Template>
concept derived_from_template_specialization_of = requires(const T t) { detail::derivedFromTemplateSpecializationTest<Template>(t); };

template <typename T, typename U>
concept convertible_to = std::is_convertible_v<T, U> && requires { static_cast<U>(std::declval<T>()); };

template <typename T, typename U>
concept common_reference_with =
	same_as<std::common_reference_t<T, U>, std::common_reference_t<U, T>> && //
	convertible_to<T, std::common_reference_t<T, U>> &&                      //
	convertible_to<U, std::common_reference_t<T, U>>;

template <typename T, typename U>
concept common_with =
	same_as<std::common_type_t<T, U>, std::common_type_t<U, T>> && //
	requires {
		static_cast<std::common_type_t<T, U>>(std::declval<T>());
		static_cast<std::common_type_t<T, U>>(std::declval<U>());
	} &&                                                                                                 //
	common_reference_with<std::add_lvalue_reference_t<const T>, std::add_lvalue_reference_t<const U>> && //
	common_reference_with<std::add_lvalue_reference_t<std::common_type_t<T, U>>,
		std::common_reference_t<std::add_lvalue_reference_t<const T>, std::add_lvalue_reference_t<const U>>>;

template <typename T>
concept integral = std::is_integral_v<T>;

template <typename T>
concept strict_integral = integral<T> && !same_as<T, bool> && !same_as<T, char> && !same_as<T, char8_t> && !same_as<T, char16_t> && !same_as<T, char32_t> && !same_as<T, wchar_t>;

template <typename T>
concept signed_integral = integral<T> && std::is_signed_v<T>;

template <typename T>
concept strict_signed_integral = strict_integral<T> && std::is_signed_v<T>;

template <typename T>
concept unsigned_integral = integral<T> && !signed_integral<T>;

template <typename T>
concept strict_unsigned_integral = strict_integral<T> && !signed_integral<T>;

template <typename T>
concept floating_point = std::is_floating_point_v<T>;

template <typename T>
concept arithmetic = integral<T> || floating_point<T>;

template <typename T>
concept strict_arithmetic = strict_integral<T> || floating_point<T>;

template <typename T>
concept enumeration = std::is_enum_v<T>;

template <typename T>
concept scoped_enumeration = enumeration<T> && !convertible_to<T, int>;

template <typename T>
concept aggregate = std::is_aggregate_v<T>;

template <typename T>
concept scalar = std::is_scalar_v<T>;

template <typename T>
concept pointer = std::is_pointer_v<T>;

template <typename T>
concept standard_layout = std::is_standard_layout_v<T>;

template <typename T, typename U>
concept assignable_from =
	std::is_lvalue_reference_v<T> &&                                                               //
	common_reference_with<const std::remove_reference_t<T>&, const std::remove_reference_t<U>&> && //
	requires(T t, U&& u) {
		{ t = std::forward<U>(u) } -> same_as<T>;
	};

template <typename T>
concept swappable = []() -> bool {
	using std::swap;
	return requires(T& a, T& b) { swap(a, b); };
}();

template <typename T, typename U>
concept swappable_with = common_reference_with<T, U> && //
                         []() -> bool {
	using std::swap;
	return requires(T&& t, U&& u) {
		swap(std::forward<T>(t), std::forward<T>(t));
		swap(std::forward<U>(u), std::forward<U>(u));
		swap(std::forward<T>(t), std::forward<U>(u));
		swap(std::forward<U>(u), std::forward<T>(t));
	};
}();

template <typename T>
concept destructible = std::is_nothrow_destructible_v<T>;

template <typename T, typename... Args>
concept constructible_from = destructible<T> && std::is_constructible_v<T, Args...>;

template <typename T>
concept default_initializable = constructible_from<T> && requires {
	T{};
	::new T;
};

template <typename T>
concept move_constructible = constructible_from<T, T> && convertible_to<T, T>;

template <typename T>
concept copy_constructible =
	move_constructible<T> &&                                          //
	constructible_from<T, T&> && convertible_to<T&, T> &&             //
	constructible_from<T, const T&> && convertible_to<const T&, T> && //
	constructible_from<T, const T> && convertible_to<const T, T>;

template <typename A, typename B = A>
concept equality_comparable = requires(const A a, const B b) { a == b; };

template <typename A, typename B = A>
concept three_way_comparable = requires(const A a, const B b) { a <=> b; };

template <typename T>
concept movable = std::is_object_v<T> && move_constructible<T> && assignable_from<T&, T> && swappable<T>;

template <typename T>
concept nothrow_movable = movable<T> && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> && std::is_nothrow_swappable_v<T>;

template <typename T>
concept copyable = copy_constructible<T> && movable<T> && assignable_from<T&, T&> && assignable_from<T&, const T&> && assignable_from<T&, const T>;

template <typename T>
concept nothrow_copyable = copyable<T> && nothrow_movable<T> && std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>;

template <typename T>
concept semiregular = copyable<T> && default_initializable<T>;

template <typename T>
concept regular = semiregular<T> && equality_comparable<T>;

template <typename T>
concept trivially_destructible = std::is_trivially_destructible_v<T>;

template <typename T>
concept trivially_copyable = trivially_destructible<T> && std::is_trivially_copyable_v<T>;

using std::bidirectional_iterator;
using std::contiguous_iterator;
using std::forward_iterator;
using std::incrementable;
using std::indirectly_readable;
using std::indirectly_writable;
using std::input_iterator;
using std::input_or_output_iterator;
using std::output_iterator;
using std::random_access_iterator;
using std::sentinel_for;
using std::sized_sentinel_for;
using std::weakly_incrementable;

using std::iter_common_reference_t;
using std::iter_difference_t;
using std::iter_reference_t;
using std::iter_rvalue_reference_t;
using std::iter_value_t;

template <typename T>
using iterator_t = decltype(std::begin(std::declval<T&>()));

template <typename T>
using const_iterator_t = decltype(std::cbegin(std::declval<T&>()));

template <typename T>
using sentinel_t = decltype(std::end(std::declval<T&>()));

template <typename T>
using const_sentinel_t = decltype(std::cend(std::declval<T&>()));

template <typename T>
using range_value_t = iter_value_t<iterator_t<T>>;

template <typename T>
using range_reference_t = iter_reference_t<iterator_t<T>>;

template <typename T>
using range_reference_t = iter_reference_t<iterator_t<T>>;

template <typename T>
using range_difference_t = iter_difference_t<iterator_t<T>>;

template <typename T>
using range_rvalue_reference_t = iter_rvalue_reference_t<iterator_t<T>>;

template <typename T>
using range_common_reference_t = iter_common_reference_t<iterator_t<T>>;

template <typename T>
using range_size_t = decltype(std::size(std::declval<T&>()));

template <typename T>
concept range = requires(T t) {
	std::begin(t);
	std::end(t);
};

template <typename T>
concept input_range = range<T> && input_iterator<iterator_t<T>>;

template <typename T, typename V>
concept output_range = range<T> && output_iterator<iterator_t<T>, V>;

template <typename T>
concept forward_range = input_range<T> && forward_iterator<iterator_t<T>>;

template <typename T>
concept bidirectional_range = forward_range<T> && bidirectional_iterator<iterator_t<T>>;

template <typename T>
concept random_access_range = bidirectional_range<T> && random_access_iterator<iterator_t<T>>;

template <typename T>
concept contiguous_range = random_access_range<T> && contiguous_iterator<iterator_t<T>> && requires(T t) { std::data(t); };

template <typename T>
concept common_range = range<T> && same_as<iterator_t<T>, sentinel_t<T>>;

template <typename T>
concept sized_range = range<T> && requires(T t) { std::size(t); };

} // namespace grem

#endif
