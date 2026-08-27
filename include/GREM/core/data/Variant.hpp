// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_VARIANT_HPP
#define GREM_CORE_DATA_VARIANT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <compare>          // std::strong_ordering, std::common_comparison_category_t, std::compare_three_way_result_t
#include <cstddef>          // std::size_t
#include <cstdint>          // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <exception>        // std::exception
#include <functional>       // std::hash
#include <initializer_list> // std::initializer_list
#include <memory>           // std::construct_at, std::destroy_at
#include <new>              // std::launder
#include <type_traits>      // std::is_..._v, std::..._type, std::integral_constant, std::remove_..._t, std::common_reference_t, std::conditional_t, std::is_constant_evaluated
#include <utility>          // std::move, std::forward, std::swap, std::in_place_..., std::...index_sequence, std::as_const

namespace grem {

/**
 * Tagged union value type that holds a value of one of the given types.
 *
 * Its API mimics that of std::variant, with some added convenience functions
 * such as Variant::is(), Variant::as() and match().
 *
 * \tparam Ts type alternatives that can be held by the variant.
 */
template <typename... Ts>
class Variant;

namespace detail {

template <typename T>
concept bool_convertible = std::is_convertible_v<T, bool>;

template <class T, class U>
[[nodiscard]] constexpr auto&& forwardLike(U&& x) noexcept { // NOLINT(cppcoreguidelines-missing-std-forward)
	constexpr bool IS_CONST = std::is_const_v<std::remove_reference_t<T>>;
	if constexpr (std::is_lvalue_reference_v<T&&>) {
		if constexpr (IS_CONST) {
			return std::as_const(x);
		} else {
			return static_cast<U&>(x);
		}
	} else {
		if constexpr (IS_CONST) {
			return std::move(std::as_const(x));
		} else {
			return std::move(x);
		}
	}
}

template <auto...>
struct ConstantList {};

template <std::size_t N, typename Union, auto... MemberPointers>
struct n_member_pointers {
	using type = ConstantList<MemberPointers..., &Union::head>;
};

template <std::size_t N, typename Union, auto... MemberPointers>
requires(N > 0) struct n_member_pointers<N, Union, MemberPointers...> : n_member_pointers<N - 1, typename Union::Tail, MemberPointers..., &Union::tail> {};

template <std::size_t N, typename Union>
using n_member_pointers_t = typename n_member_pointers<N, Union>::type;

template <auto... MemberPointers, typename U>
[[nodiscard]] constexpr auto& getUnionMemberImplementation(ConstantList<MemberPointers...>, U&& u) noexcept {
	return (std::forward<U>(u).*....*MemberPointers);
}

template <std::size_t Index, typename U>
[[nodiscard]] constexpr auto& getUnionMember(U& u) noexcept {
	return getUnionMemberImplementation(n_member_pointers_t<Index, U>{}, u);
}

template <typename... Ts>
union UnionStorage {
	constexpr UnionStorage() = default;

	template <std::size_t Index, typename... Args>
	UnionStorage(std::in_place_index_t<Index>, Args&&...) = delete; // NOLINT(cppcoreguidelines-missing-std-forward)
};

template <typename First, typename... Rest>
union UnionStorage<First, Rest...> {
	static constexpr std::size_t size = 1 + sizeof...(Rest);

	using Head = First;
	using Tail = UnionStorage<Rest...>;

	constexpr UnionStorage()
		: tail() {}

	template <typename... Args>
	constexpr UnionStorage(std::in_place_index_t<0>, Args&&... args)
		: head(std::forward<Args>(args)...) {}

	template <std::size_t Index, typename... Args>
	constexpr UnionStorage(std::in_place_index_t<Index>, Args&&... args)
		: tail(std::in_place_index<Index - 1>, std::forward<Args>(args)...) {}

	constexpr UnionStorage(const UnionStorage& other, std::size_t otherIndex) {
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
			(void)(((otherIndex == Indices) ? (construct<Indices>(getUnionMember<Indices>(other)), true) : false) || ...);
		}(std::make_index_sequence<size>{});
	}

	constexpr UnionStorage(UnionStorage&& other, std::size_t otherIndex) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
			(void)(((otherIndex == Indices) ? (construct<Indices>(std::move(getUnionMember<Indices>(other))), true) : false) || ...);
		}(std::make_index_sequence<size>{});
	}

	~UnionStorage() = default;
	constexpr ~UnionStorage() requires(!std::is_trivially_destructible_v<Head> || !std::is_trivially_destructible_v<Tail>) {}

	constexpr UnionStorage(const UnionStorage&) = default;
	constexpr UnionStorage(UnionStorage&&) noexcept(
		std::is_nothrow_move_constructible_v<Head> && // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::is_nothrow_move_constructible_v<Tail>) = default;
	constexpr UnionStorage& operator=(const UnionStorage&) = default;
	constexpr UnionStorage& operator=(UnionStorage&&) noexcept(
		std::is_nothrow_move_assignable_v<Head> && // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::is_nothrow_move_assignable_v<Tail>) = default;

	template <std::size_t Index, typename... Args>
	constexpr void construct(Args&&... args) {
		std::construct_at(&getUnionMember<Index>(*this), std::forward<Args>(args)...);
	}

	constexpr void destroy(std::size_t index) noexcept {
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
			(void)(((index == Indices) ? (std::destroy_at(&getUnionMember<Indices>(*this)), true) : false) || ...);
		}(std::make_index_sequence<size>{});
	}

	Head head;
	Tail tail;
};

template <typename T, std::size_t Index, typename... Ts>
struct VariantIndexImpl;

template <typename T, std::size_t Index, typename First, typename... Rest>
struct VariantIndexImpl<T, Index, First, Rest...> : VariantIndexImpl<T, Index + 1, Rest...> {};

template <typename T, std::size_t Index, typename... Rest>
struct VariantIndexImpl<T, Index, T, Rest...> : std::integral_constant<std::size_t, Index> {};

} // namespace detail

/// \cond
template <typename T, typename V>
struct variant_has_alternative;

template <typename T, typename First, typename... Rest>
struct variant_has_alternative<T, Variant<First, Rest...>> : variant_has_alternative<T, Variant<Rest...>> {};

template <typename T>
struct variant_has_alternative<T, Variant<>> : std::false_type {};

template <typename T, typename... Rest>
struct variant_has_alternative<T, Variant<T, Rest...>> : std::true_type {};
/// \endcond

/**
 * Check if a variant type has a given type as one of its possible alternatives.
 *
 * \tparam T alternative type to check for.
 * \tparam V variant type.
 */
template <typename T, typename V>
inline constexpr bool variant_has_alternative_v = variant_has_alternative<T, V>::value;

/// \cond
template <typename T, typename V>
struct variant_index;

template <typename T, typename... Ts>
struct variant_index<T, Variant<Ts...>> : detail::VariantIndexImpl<T, 0, Ts...> {};
/// \endcond

/**
 * Get the index of the variant alternative with a given type in a variant type.
 *
 * \tparam T type of the variant alternative to get the index of.
 * \tparam V variant type.
 */
template <typename T, typename V>
inline constexpr std::size_t variant_index_v = variant_index<T, V>::value;

/// \cond
template <std::size_t Index, typename V>
struct variant_alternative;

template <std::size_t Index, typename First, typename... Rest>
struct variant_alternative<Index, Variant<First, Rest...>> : variant_alternative<Index - 1, Variant<Rest...>> {};

template <typename T, typename... Rest>
struct variant_alternative<0, Variant<T, Rest...>> {
	using type = T;
};
/// \endcond

/**
 * Get the type of the variant alternative with a given index in a variant type.
 *
 * \tparam Index index of the variant alternative type to get.
 * \tparam V variant type.
 */
template <std::size_t Index, typename V>
using variant_alternative_t = typename variant_alternative<Index, V>::type;

/// \cond
template <typename V>
struct variant_size;

template <typename... Ts>
struct variant_size<Variant<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};
/// \endcond

/**
 * Get the number of alternative types of a variant type.
 *
 * \tparam V variant type.
 */
template <typename V>
inline constexpr std::size_t variant_size_v = variant_size<V>::value;

/**
 * Unit type for representing an empty alternative in Variant.
 *
 * A Variant can use Monostate as its first alternative type to make sure that
 * the variant type is default-constructible.
 */
struct Monostate {};

/**
 * Compare two monostates for equality.
 *
 * This function always returns true, since monostates are empty unit values
 * containing no state, meaning they are always equal.
 *
 * \return true.
 */
[[nodiscard]] constexpr bool operator==(Monostate, Monostate) noexcept {
	return true;
}

/**
 * Compare two monostates.
 *
 * This function always returns equal, since monostates are empty unit values
 * containing no state, meaning they are always equal.
 *
 * \return a strong ordering representing equality.
 */
[[nodiscard]] constexpr std::strong_ordering operator<=>(Monostate, Monostate) noexcept {
	return std::strong_ordering::equal;
}

} // namespace grem

/**
 * Specialization of std::hash for grem::Monostate.
 */
template <>
struct std::hash<grem::Monostate> {
	[[nodiscard]] std::size_t operator()(const grem::Monostate&) const {
		return 0;
	}
};

namespace grem {

/**
 * Exception type that is thrown on an attempt to erroneously access an inactive
 * alternative of a Variant when using a safe access function such as
 * Variant::get().
 */
struct BadVariantAccess : std::exception {
	BadVariantAccess() noexcept = default;

	[[nodiscard]] const char* what() const noexcept override {
		return "Bad variant access.";
	}
};

template <typename... Ts>
class Variant {
private:
	static constexpr bool HAS_DEFAULT_CONSTRUCTOR = std::is_default_constructible_v<variant_alternative_t<0, Variant>>;
	static constexpr bool HAS_COPY_CONSTRUCTOR = (std::is_copy_constructible_v<Ts> && ...);
	static constexpr bool HAS_MOVE_CONSTRUCTOR = (std::is_move_constructible_v<Ts> && ...);
	static constexpr bool HAS_COPY_ASSIGNMENT = ((std::is_copy_constructible_v<Ts> && std::is_copy_assignable_v<Ts>) && ...);
	static constexpr bool HAS_MOVE_ASSIGNMENT = ((std::is_move_constructible_v<Ts> && std::is_move_assignable_v<Ts>) && ...);
	static constexpr bool HAS_TRIVIAL_COPY_CONSTRUCTOR = (std::is_trivially_copy_constructible_v<Ts> && ...);
	static constexpr bool HAS_TRIVIAL_MOVE_CONSTRUCTOR = (std::is_trivially_move_constructible_v<Ts> && ...);
	static constexpr bool HAS_TRIVIAL_COPY_ASSIGNMENT =
		((std::is_trivially_copy_constructible_v<Ts> && std::is_trivially_copy_assignable_v<Ts> && std::is_trivially_destructible_v<Ts>) && ...);
	static constexpr bool HAS_TRIVIAL_MOVE_ASSIGNMENT =
		((std::is_trivially_move_constructible_v<Ts> && std::is_trivially_move_assignable_v<Ts> && std::is_trivially_destructible_v<Ts>) && ...);
	static constexpr bool HAS_TRIVIAL_DESTRUCTOR = (std::is_trivially_destructible_v<Ts> && ...);

	template <typename T>
	struct SelectAlternativeTypeOverload {
		template <typename U>
		T operator()(T, U&& u) requires(requires { T(std::forward<U>(u)); });
	};

	struct SelectAlternativeTypeOverloadSet : SelectAlternativeTypeOverload<Ts>... {
		using SelectAlternativeTypeOverload<Ts>::operator()...;
	};

	template <typename U>
	static constexpr auto F(U&& u) -> decltype(SelectAlternativeTypeOverloadSet{}(std::forward<U>(u), std::forward<U>(u)));

public:
	using GREM_private_DerivedFromVariantTag = void;

	// clang-format off
	/**
	 * Index type used to encode the active alternative type.
	 *
	 * This is automatically sized to the smallest possible standard unsigned
	 * integer type that can represent all of the possible variant alternatives,
	 * including the valueless by exception state.
	 */
    using index_type =
        std::conditional_t<sizeof...(Ts) < 255ull, std::uint8_t,
        std::conditional_t<sizeof...(Ts) < 65535ull, std::uint16_t,
        std::conditional_t<sizeof...(Ts) < 4294967295ull, std::uint32_t,
        std::uint64_t>>>;
	// clang-format on

	/**
	 * Invalid alternative index, representing the valueless by exception state.
	 */
	static constexpr index_type npos = sizeof...(Ts);

	/**
	 * Default-construct a variant with the first variant alternative, if it is
	 * default-constructible.
	 *
	 * \throws any exception thrown by the underlying constructor of the first
	 *         alternative type.
	 */
	constexpr Variant() noexcept(std::is_nothrow_default_constructible_v<variant_alternative_t<0, Variant>>) requires(HAS_DEFAULT_CONSTRUCTOR)
		: storage(std::in_place_index<0>)
		, activeTypeIndex(0) {}

	/**
	 * Converting constructor.
	 *
	 * Chooses the variant alternative type to construct based on the
	 * overloading rules specified by std::variant.
	 *
	 * \param value underlying value to construct the variant from. Must be
	 *        convertible to one of the variant's listed alternative types.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 */
	template <typename U>
	constexpr Variant(U&& value) noexcept(std::is_nothrow_constructible_v<decltype(F(std::forward<U>(value))), U>)
		requires(!std::is_same_v<std::remove_cvref_t<U>, Variant> && variant_has_alternative_v<decltype(F(std::forward<U>(value))), Variant> &&
				 std::is_constructible_v<decltype(F(std::forward<U>(value))), U>)
		: Variant(std::in_place_index<variant_index_v<decltype(F(std::forward<U>(value))), Variant>>, std::forward<U>(value)) {}

	/**
	 * Construct a variant alternative in-place given its type.
	 *
	 * \param type std::in_place_type<T>, where T is the alternative type to
	 *        construct, which must be one of the variant's listed alternative
	 *        types.
	 * \param args arguments to pass to the underlying value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 */
	template <typename T, typename... Args>
	constexpr explicit Variant(std::in_place_type_t<T> type, Args&&... args) requires(variant_has_alternative_v<T, Variant> && std::is_constructible_v<T, Args...>)
		: Variant(std::in_place_index<variant_index_v<T, Variant>>, std::forward<Args>(args)...) {
		(void)type;
	}

	/**
	 * Construct a variant alternative in-place given its type, with an
	 * initializer list as the first constructor argument.
	 *
	 * \param type std::in_place_type<T>, where T is the alternative type to
	 *        construct, which must be one of the variant's listed alternative
	 *        types.
	 * \param ilist first argument to pass to the underlying value's
	 *        constructor.
	 * \param args subsequent arguments to pass to the underlying value's
	 *        constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 */
	template <typename T, typename U, typename... Args>
	constexpr explicit Variant(std::in_place_type_t<T> type, std::initializer_list<U> ilist, Args&&... args)
		requires(variant_has_alternative_v<T, Variant> && std::is_constructible_v<T, std::initializer_list<U>&, Args...>)
		: Variant(std::in_place_index<variant_index_v<T, Variant>>, ilist, std::forward<Args>(args)...) {
		(void)type;
	}

	/**
	 * Construct a variant alternative in-place given its index.
	 *
	 * \param index std::in_place_index<Index>, where Index is the alternative index
	 *        to construct, which must be within the range of the variant's list
	 *        of alternative types.
	 * \param args arguments to pass to the underlying value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 */
	template <std::size_t Index, typename... Args>
	constexpr explicit Variant(std::in_place_index_t<Index> index, Args&&... args)
		requires(Index < sizeof...(Ts) && std::is_constructible_v<variant_alternative_t<Index, Variant>, Args...>)
		: storage(index, std::forward<Args>(args)...)
		, activeTypeIndex(Index) {}

	/**
	 * Construct a variant alternative in-place given its index, with an
	 * initializer list as the first constructor argument.
	 *
	 * \param index std::in_place_index<Index>, where Index is the alternative index
	 *        to construct, which must be within the range of the variant's list
	 *        of alternative types.
	 * \param ilist first argument to pass to the underlying value's
	 *        constructor.
	 * \param args subsequent arguments to pass to the underlying value's
	 *        constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 */
	template <std::size_t Index, typename U, typename... Args>
	constexpr explicit Variant(std::in_place_index_t<Index> index, std::initializer_list<U> ilist, Args&&... args)
		requires(Index < sizeof...(Ts) && std::is_constructible_v<variant_alternative_t<Index, Variant>, std::initializer_list<U>&, Args...>)
		: storage(index, ilist, std::forward<Args>(args)...)
		, activeTypeIndex(Index) {}

	/** Destructor. */
	constexpr ~Variant() {
		destroy();
	}

	/** Trivial destructor. */
	constexpr ~Variant() requires(HAS_TRIVIAL_DESTRUCTOR) = default;

	/** Deleted copy constructor. */
	constexpr Variant(const Variant& other) requires(!HAS_COPY_CONSTRUCTOR) = delete;

	/** Trivial copy constructor. */
	constexpr Variant(const Variant& other) requires(HAS_COPY_CONSTRUCTOR && HAS_TRIVIAL_COPY_CONSTRUCTOR) = default;

	/** Copy constructor. */
	constexpr Variant(const Variant& other) requires(HAS_COPY_CONSTRUCTOR && !HAS_TRIVIAL_COPY_CONSTRUCTOR)
		: storage(other.storage, other.activeTypeIndex)
		, activeTypeIndex(other.activeTypeIndex) {}

	/** Trivial move constructor. */
	constexpr Variant(Variant&& other) noexcept requires(HAS_MOVE_CONSTRUCTOR && HAS_TRIVIAL_MOVE_CONSTRUCTOR) = default;

	/** Move constructor. */
	constexpr Variant(Variant&& other) noexcept(
		(std::is_nothrow_move_constructible_v<Ts> && ...)) // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
		requires(HAS_MOVE_CONSTRUCTOR && !HAS_TRIVIAL_MOVE_CONSTRUCTOR)
		: storage(std::move(other.storage), other.activeTypeIndex)
		, activeTypeIndex(other.activeTypeIndex) {}

	/** Deleted copy assignment. */
	constexpr Variant& operator=(const Variant& other) requires(!HAS_COPY_ASSIGNMENT) = delete;

	/** Trivial copy assignment. */
	constexpr Variant& operator=(const Variant& other) requires(HAS_COPY_ASSIGNMENT && HAS_TRIVIAL_COPY_ASSIGNMENT) = default;

	/**
	 * Copy assignment.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	constexpr Variant& operator=(const Variant& other) requires(HAS_COPY_ASSIGNMENT && !HAS_TRIVIAL_COPY_ASSIGNMENT) {
		if (this == &other) {
			return *this;
		}
		const auto copyAssign = [&]<size_t OtherIndex, typename Other>(std::in_place_index_t<OtherIndex>, const Other& other) -> void {
			if (activeTypeIndex == OtherIndex) {
				as<OtherIndex>() = other;
			} else if constexpr (std::is_nothrow_copy_constructible_v<Other> || !std::is_nothrow_move_constructible_v<Other>) {
				emplace<OtherIndex>(other);
			} else {
				*this = Variant{other};
			}
		};
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
			if (!(((other.activeTypeIndex == Indices) ? (copyAssign(std::in_place_index<Indices>, other.template access<Ts, Indices>()), true) : false) || ...)) {
				destroy();
			}
		}(std::make_index_sequence<sizeof...(Ts)>{});
		return *this;
	}

	/** Trivial move assignment. */
	constexpr Variant& operator=(Variant&& other) noexcept requires(HAS_MOVE_ASSIGNMENT && HAS_TRIVIAL_MOVE_ASSIGNMENT) = default;

	/**
	 * Move assignment.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	constexpr Variant& operator=(Variant&& other) noexcept(
		((std::is_nothrow_move_constructible_v<Ts> && // NOLINT(performance-noexcept-move-constructor, cppcoreguidelines-noexcept-move-operations)
			 std::is_nothrow_move_assignable_v<Ts>) &&
			...)) requires(HAS_MOVE_ASSIGNMENT && !HAS_TRIVIAL_MOVE_ASSIGNMENT) {
		if (this == &other) {
			return *this;
		}
		const auto moveAssign = [&]<size_t OtherIndex, typename Other>(std::in_place_index_t<OtherIndex>, Other& other) -> void {
			if (activeTypeIndex == OtherIndex) {
				as<OtherIndex>() = std::move(other);
			} else {
				emplace<OtherIndex>(std::move(other));
			}
		};
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
			if (!(((other.activeTypeIndex == Indices) ? (moveAssign(std::in_place_index<Indices>, other.template access<Ts, Indices>()), true) : false) || ...)) {
				destroy();
			}
		}(std::make_index_sequence<sizeof...(Ts)>{});
		return *this;
	}

	/**
	 * Converting assignment.
	 *
	 * Chooses the variant alternative type to construct or assign based on the
	 * overloading rules specified by std::variant.
	 *
	 * \param value underlying value to construct the new value from. Must be
	 *        convertible to one of the variant's listed alternative types.
	 *
	 * \throws any exception thrown by the underlying constructor or assignment
	 *         of the relevant alternative type.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	template <typename U>
	constexpr Variant& operator=(U&& value) noexcept(
		std::is_nothrow_assignable_v<decltype(F(std::forward<U>(value)))&, U> && std::is_nothrow_constructible_v<decltype(F(std::forward<U>(value))), U>)
		requires(!std::is_same_v<std::remove_cvref_t<U>, Variant> && variant_has_alternative_v<decltype(F(std::forward<U>(value))), Variant> &&
				 std::is_assignable_v<decltype(F(std::forward<U>(value))), U>) {
		using T = decltype(F(std::forward<U>(value)));
		if (is<T>()) {
			as<T>() = std::forward<U>(value);
		} else {
			if constexpr (std::is_nothrow_constructible_v<T, U> || !std::is_nothrow_move_constructible_v<T>) {
				emplace<T>(std::forward<U>(value));
			} else {
				emplace<T>(T(std::forward<U>(value)));
			}
		}
		return *this;
	}

	/**
	 * Construct an alternative given its type, destroying the old value.
	 *
	 * \tparam T the alternative type to construct. Must be one of the variant's
	 *         listed alternative types.
	 *
	 * \param args arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	template <typename T, typename... Args>
	constexpr T& emplace(Args&&... args) requires(variant_has_alternative_v<T, Variant> && std::is_constructible_v<T, Args...>) {
		return emplace<variant_index_v<T, Variant>>(std::forward<Args>(args)...);
	}

	/**
	 * Construct an alternative given its type, with an initializer list as the
	 * first constructor argument, destroying the old value.
	 *
	 * \tparam T the alternative type to construct. Must be one of the variant's
	 *         listed alternative types.
	 *
	 * \param ilist first argument to pass to the new value's constructor.
	 * \param args subsequent arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	template <typename T, typename U, typename... Args>
	constexpr T& emplace(std::initializer_list<U> ilist, Args&&... args)
		requires(variant_has_alternative_v<T, Variant> && std::is_constructible_v<T, std::initializer_list<U>&, Args...>) {
		return emplace<variant_index_v<T, Variant>>(ilist, std::forward<Args>(args)...);
	}

	/**
	 * Construct an alternative given its index, destroying the old value.
	 *
	 * \tparam Index the alternative index to construct. Must be within the
	 *         range of the variant's list of alternative types.
	 *
	 * \param args arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	template <std::size_t Index, typename... Args>
	constexpr auto& emplace(Args&&... args) requires(std::is_constructible_v<variant_alternative_t<Index, Variant>, Args...>) {
		static_assert(Index < sizeof...(Ts));
		destroy();
		storage.template construct<Index>(std::forward<Args>(args)...);
		activeTypeIndex = Index;
		return as<Index>();
	}

	/**
	 * Construct an alternative given its index, with an initializer list as the
	 * first constructor argument, destroying the old value.
	 *
	 * \tparam Index the alternative index to construct. Must be within the
	 *         range of the variant's list of alternative types.
	 *
	 * \param ilist first argument to pass to the new value's constructor.
	 * \param args subsequent arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the
	 *         relevant alternative type.
	 *
	 * \note If an exception is thrown after the old value has been destroyed,
	 *       the variant ends up in the valueless by exception state.
	 */
	template <std::size_t Index, typename U, typename... Args>
	constexpr auto& emplace(std::initializer_list<U> ilist, Args&&... args)
		requires(std::is_constructible_v<variant_alternative_t<Index, Variant>, std::initializer_list<U>&, Args...>) {
		static_assert(Index < sizeof...(Ts));
		destroy();
		storage.template construct<Index>(ilist, std::forward<Args>(args)...);
		activeTypeIndex = Index;
		return as<Index>();
	}

	/**
	 * Swap this variant's value with that of another.
	 *
	 * \param other variant to swap with.
	 *
	 * \throws any exception thrown by the underlying constructor, assignment or
	 *         swap implementation of the relevant alternative types.
	 *
	 * \note If an exception is thrown after any value has been destroyed,
	 *       and before it has been replaced with a new value, the associated
	 *       variant ends up in the valueless by exception state.
	 */
	constexpr void swap(Variant& other) noexcept(
		((std::is_nothrow_move_constructible_v<Ts> && std::is_nothrow_swappable_v<Ts>) && ...)) { // NOLINT(performance-noexcept-swap, cppcoreguidelines-noexcept-swap)
		if (activeTypeIndex == other.activeTypeIndex) {
			using std::swap;
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				(void)(((activeTypeIndex == Indices) ? ((swap(access<Ts, Indices>(), other.template access<Ts, Indices>())), true) : false) || ...);
			}(std::make_index_sequence<sizeof...(Ts)>{});
		} else {
			Variant old = std::move(*this);
			*this = std::move(other);
			other = std::move(old);
		}
	}

	/**
	 * Swap the values of two variants.
	 *
	 * \param a first variant.
	 * \param b second variant.
	 *
	 * \throws any exception thrown by the underlying constructor, assignment or
	 *         swap implementation of the relevant alternative types.
	 *
	 * \note If an exception is thrown after any value has been destroyed,
	 *       and before it has been replaced with a new value, the associated
	 *       variant ends up in the valueless by exception state.
	 */
	friend constexpr void swap(Variant& a, Variant& b) noexcept(noexcept(a.swap(b))) { // NOLINT(performance-noexcept-swap, cppcoreguidelines-noexcept-swap)
		a.swap(b);
	}

	/**
	 * Get the alternative index of the currently active value held by the
	 * variant.
	 *
	 * \return the active alternative index, or #npos if the variant is in the
	 *         valueless by exception state.
	 */
	[[nodiscard]] constexpr index_type index() const noexcept {
		return activeTypeIndex;
	}

	/**
	 * Check if the variant is in the valueless by exception state.
	 *
	 * \return true if the variant is in the valueless by exception state, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool valueless_by_exception() const noexcept {
		return activeTypeIndex == npos;
	}

	/**
	 * Check if the variant currently holds the alternative with the given type.
	 *
	 * \tparam T alternative type to check for. Must be one of the variant's
	 *         listed alternative types.
	 *
	 * \return true if the variant holds a value of the given type, false
	 *         otherwise.
	 */
	template <typename T>
	[[nodiscard]] constexpr bool is() const noexcept requires(variant_has_alternative_v<T, Variant>) {
		return activeTypeIndex == variant_index_v<T, Variant>;
	}

	/**
	 * Check if the variant currently holds the alternative with the given
	 * index.
	 *
	 * \tparam Index alternative index to check for. Must be within the range of
	 *         the variant's list of alternative types.
	 *
	 * \return true if the variant holds a value of the given alternative index,
	 *         false otherwise.
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr bool is() const noexcept {
		return activeTypeIndex == Index;
	}

	/**
	 * Access the underlying value with the given type without a safety check.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr T& as() & noexcept requires(variant_has_alternative_v<T, Variant>) {
		GREM_ASSERT(is<T>());
		return as<variant_index_v<T, Variant>>();
	}

	/**
	 * Access the underlying value with the given type without a safety check.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a read-only reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr const T& as() const& noexcept requires(variant_has_alternative_v<T, Variant>) {
		GREM_ASSERT(is<T>());
		return as<variant_index_v<T, Variant>>();
	}

	/**
	 * Access the underlying value with the given type without a safety check.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return an rvalue reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr T&& as() && noexcept requires(variant_has_alternative_v<T, Variant>) {
		GREM_ASSERT(is<T>());
		return std::move(as<variant_index_v<T, Variant>>());
	}

	/**
	 * Access the underlying value with the given type without a safety check.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a read-only rvalue reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr const T&& as() const&& noexcept requires(variant_has_alternative_v<T, Variant>) {
		GREM_ASSERT(is<T>());
		return std::move(as<variant_index_v<T, Variant>>());
	}

	/**
	 * Access the underlying value with the given index without a safety check.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr auto& as() & noexcept {
		GREM_ASSERT(is<Index>());
		return detail::getUnionMember<Index>(storage);
	}

	/**
	 * Access the underlying value with the given index without a safety check.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a read-only reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr const auto& as() const& noexcept {
		GREM_ASSERT(is<Index>());
		return detail::getUnionMember<Index>(storage);
	}

	/**
	 * Access the underlying value with the given index without a safety check.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return an rvalue reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr auto&& as() && noexcept {
		GREM_ASSERT(is<Index>());
		return std::move(detail::getUnionMember<Index>(storage));
	}

	/**
	 * Access the underlying value with the given index without a safety check.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a read-only rvalue reference to the value held by the variant.
	 *
	 * \warning The behavior of accessing the underlying value using the
	 *          incorrect alternative type which is not currently active is
	 *          undefined. The active alternative type must be known in advance
	 *          in order to use this function safely, for example by checking if
	 *          the variant contains the expected alternative using the is()
	 *          function.
	 *
	 * \remark Rather than using this function in conjunction with is(), it is
	 *         usually more appropriate to use get() or to call get_if() and
	 *         make sure that the returned pointer is not nullptr. This function
	 *         is only meant for the cases where it is absolutely certain which
	 *         alternative the variant currently holds.
	 *
	 * \sa get()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr const auto&& as() const&& noexcept {
		GREM_ASSERT(is<Index>());
		return std::move(detail::getUnionMember<Index>(storage));
	}

	/**
	 * Access the underlying value with the given type.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold a value
	 *         of the given type.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr T& get() & requires(variant_has_alternative_v<T, Variant>) {
		if (!is<T>()) {
			throw BadVariantAccess{};
		}
		return as<T>();
	}

	/**
	 * Access the underlying value with the given type.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a read-only reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold a value
	 *         of the given type.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr const T& get() const& requires(variant_has_alternative_v<T, Variant>) {
		if (!is<T>()) {
			throw BadVariantAccess{};
		}
		return as<T>();
	}

	/**
	 * Access the underlying value with the given type.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return an rvalue reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold a value
	 *         of the given type.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr T&& get() && requires(variant_has_alternative_v<T, Variant>) {
		if (!is<T>()) {
			throw BadVariantAccess{};
		}
		return std::move(as<T>());
	}

	/**
	 * Access the underlying value with the given type.
	 *
	 * \tparam T type of the currently active value to get. Must be one of the
	 *         variant's listed alternative types.
	 *
	 * \return a read-only rvalue reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold a value
	 *         of the given type.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] constexpr const T&& get() const&& requires(variant_has_alternative_v<T, Variant>) {
		if (!is<T>()) {
			throw BadVariantAccess{};
		}
		return std::move(as<T>());
	}

	/**
	 * Access the underlying value with the given index.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold the given
	 *         alternative.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr auto& get() & {
		if (!is<Index>()) {
			throw BadVariantAccess{};
		}
		return as<Index>();
	}

	/**
	 * Access the underlying value with the given index.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a read-only reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold the given
	 *         alternative.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr const auto& get() const& {
		if (!is<Index>()) {
			throw BadVariantAccess{};
		}
		return as<Index>();
	}

	/**
	 * Access the underlying value with the given index.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return an rvalue reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold the given
	 *         alternative.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr auto&& get() && {
		if (!is<Index>()) {
			throw BadVariantAccess{};
		}
		return std::move(as<Index>());
	}

	/**
	 * Access the underlying value with the given index.
	 *
	 * \tparam Index alternative index of the currently active value to get.
	 *         Must be within the range of the variant's list of alternative
	 *         types.
	 *
	 * \return a read-only rvalue reference to the value held by the variant.
	 *
	 * \throws BadVariantAccess if the variant does not currently hold the given
	 *         alternative.
	 *
	 * \sa as()
	 * \sa get_if()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr const auto&& get() const&& {
		if (!is<Index>()) {
			throw BadVariantAccess{};
		}
		return std::move(as<Index>());
	}

	/**
	 * Access the underlying value with the given type if it is the currently
	 * active alternative.
	 *
	 * \tparam T type of the value to get. Must be one of the variant's listed
	 *         alternative types.
	 *
	 * \return a non-owning pointer to the value held by the variant, or nullptr
	 *         if the variant does not currently hold a value of the given type.
	 *
	 * \sa as()
	 * \sa get()
	 */
	template <typename T>
	[[nodiscard]] constexpr T* get_if() noexcept requires(variant_has_alternative_v<T, Variant>) {
		return (is<T>()) ? &as<T>() : nullptr;
	}

	/**
	 * Access the underlying value with the given type if it is the currently
	 * active alternative.
	 *
	 * \tparam T type of the value to get. Must be one of the variant's listed
	 *         alternative types.
	 *
	 * \return a non-owning read-only pointer to the value held by the variant,
	 *         or nullptr if the variant does not currently hold a value of the
	 *         given type.
	 *
	 * \sa as()
	 * \sa get()
	 */
	template <typename T>
	[[nodiscard]] constexpr const T* get_if() const noexcept requires(variant_has_alternative_v<T, Variant>) {
		return (is<T>()) ? &as<T>() : nullptr;
	}

	/**
	 * Access the underlying value with the given index if it is the currently
	 * active alternative.
	 *
	 * \tparam Index alternative index of the value to get. Must be within the
	 *         range of the variant's list of alternative types.
	 *
	 * \return a non-owning pointer to the value held by the variant, or nullptr
	 *         if the variant does not currently hold the given alternative.
	 *
	 * \sa as()
	 * \sa get()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr auto* get_if() noexcept {
		return (is<Index>()) ? &as<Index>() : nullptr;
	}

	/**
	 * Access the underlying value with the given index if it is the currently
	 * active alternative.
	 *
	 * \tparam Index alternative index of the value to get. Must be within the
	 *         range of the variant's list of alternative types.
	 *
	 * \return a non-owning read-only pointer to the value held by the variant,
	 *         or nullptr if the variant does not currently hold the given
	 *         alternative.
	 *
	 * \sa as()
	 * \sa get()
	 */
	template <std::size_t Index>
	[[nodiscard]] constexpr const auto* get_if() const noexcept {
		return (is<Index>()) ? &as<Index>() : nullptr;
	}

	/**
	 * Call a visitor functor with the currently active underlying value of a
	 * variant.
	 *
	 * \param visitor callable object that is overloaded to accept any of the
	 *        variant alternatives as a parameter.
	 * \param variant variant whose underlying value to pass to the visitor.
	 *
	 * \return the result of calling the visitor with the currently active
	 *         variant alternative value.
	 *
	 * \throws BadVariantAccess if the variant is in the valueless by exception
	 *         state.
	 * \throws any exception thrown by the visitor.
	 *
	 * \sa match()
	 */
	template <typename Visitor, typename V>
	static constexpr decltype(auto) visit(Visitor&& visitor, V&& variant) { // NOLINT(cppcoreguidelines-missing-std-forward)
		using R = std::common_reference_t<decltype(visitor(detail::forwardLike<V>(std::declval<Ts>())))...>;
		if constexpr (std::is_void_v<R>) {
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((variant.activeTypeIndex == Indices) ? (visitor(std::forward<V>(variant).template access<Ts, Indices>()), true) : false) || ...)) {
					throw BadVariantAccess{};
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
		} else if constexpr (std::is_lvalue_reference_v<R>) {
			std::remove_reference_t<R>* result = nullptr;
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((variant.activeTypeIndex == Indices) ? ((result = &visitor(std::forward<V>(variant).template access<Ts, Indices>())), true) : false) || ...)) {
					throw BadVariantAccess{};
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
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
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((variant.activeTypeIndex == Indices) ? (std::construct_at(&result.value.actual, visitor(std::forward<V>(variant).template access<Ts, Indices>())), true)
															: false) ||
						...)) {
					throw BadVariantAccess{};
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
			result.hasValue = true;
			return R{std::move(result.value.actual)};
		}
	}

	/**
	 * Call a visitor functor with the type corresponding to a variant index.
	 *
	 * \param index variant alternative index of the visitor overload to call.
	 * \param visitor callable object that is overloaded to accept any of the
	 *        variant alternative types wrapped in std::in_place_type_t as a
	 *        parameter, as well as no arguments in case the given index is
	 *        outside the range of possible alternatives.
	 *
	 * \return the result of calling the visitor with std::in_place_type<T>,
	 *         where T is the variant alternative type corresponding to the
	 *         given index, or with no arguments if the index is out of range.
	 *
	 * \throws any exception thrown by the visitor.
	 */
	template <typename Visitor>
	static constexpr decltype(auto) visitIndex(index_type index, Visitor&& visitor) { // NOLINT(cppcoreguidelines-missing-std-forward)
		using R = std::common_reference_t<decltype(visitor(std::in_place_type<Ts>))..., decltype(visitor())>;
		if constexpr (std::is_void_v<R>) {
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((index == Indices) ? (visitor(std::in_place_type<Ts>), true) : false) || ...)) {
					visitor();
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
		} else if constexpr (std::is_lvalue_reference_v<R>) {
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				std::remove_reference_t<R>* result = nullptr;
				if (!(((index == Indices) ? ((result = &visitor(std::in_place_type<Ts>)), true) : false) || ...)) {
					result = &visitor();
				}
				return *result;
			}(std::make_index_sequence<sizeof...(Ts)>{});
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
			[&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				if (!(((index == Indices) ? (std::construct_at(&result.value.actual, visitor(std::in_place_type<Ts>)), true) : false) || ...)) {
					std::construct_at(&result.value.actual, visitor());
				}
			}(std::make_index_sequence<sizeof...(Ts)>{});
			result.hasValue = true;
			return R{std::move(result.value.actual)};
		}
	}

	/**
	 * Call a functor with all alternative types.
	 *
	 * \param function callable object that accepts all of the variant
	 *        alternative types wrapped in std::in_place_type_t as consecutive
	 *        arguments.
	 *
	 * \return the result of calling the function with the expansion of
	 *         std::in_place_type<Ts>..., where Ts is the pack of variant
	 *         alternative types.
	 *
	 * \throws any exception thrown by the function.
	 */
	template <typename Function>
	static constexpr decltype(auto) apply(Function&& function) {
		return std::forward<Function>(function)(std::in_place_type<Ts>...);
	}

private:
	template <typename... Us>
	friend class Variant;

	void destroy() noexcept {
		if constexpr (!HAS_TRIVIAL_DESTRUCTOR) {
			storage.destroy(activeTypeIndex);
		}
		activeTypeIndex = npos;
	}

	template <typename T, size_t Index>
	[[nodiscard]] constexpr T& access() & noexcept {
		if (std::is_constant_evaluated()) {
			return detail::getUnionMember<Index>(storage);
		}
		return *std::launder(reinterpret_cast<T*>(&storage));
	}

	template <typename T, size_t Index>
	[[nodiscard]] constexpr const T& access() const& noexcept {
		if (std::is_constant_evaluated()) {
			return detail::getUnionMember<Index>(storage);
		}
		return *std::launder(reinterpret_cast<const T*>(&storage));
	}

	template <typename T, size_t Index>
	[[nodiscard]] constexpr T&& access() && noexcept {
		if (std::is_constant_evaluated()) {
			return std::move(detail::getUnionMember<Index>(storage));
		}
		return std::move(*std::launder(reinterpret_cast<T*>(&storage)));
	}

	template <typename T, size_t Index>
	[[nodiscard]] constexpr const T&& access() const&& noexcept {
		if (std::is_constant_evaluated()) {
			return std::move(detail::getUnionMember<Index>(storage));
		}
		return std::move(*std::launder(reinterpret_cast<const T*>(&storage)));
	}

	detail::UnionStorage<Ts..., Monostate> storage;
	index_type activeTypeIndex;
};

/**
 * Call a visitor functor with the currently active underlying value of a
 * variant.
 *
 * \param visitor callable object that is overloaded to accept any of the
 *        variant alternatives as a parameter.
 * \param variant variant whose underlying value to pass to the visitor.
 *
 * \return the result of calling the visitor with the currently active variant
 *         alternative value.
 *
 * \throws BadVariantAccess if the variant is in the valueless by exception
 *         state.
 * \throws any exception thrown by the visitor.
 *
 * \sa match()
 */
template <typename Visitor, typename V>
constexpr decltype(auto) visit(Visitor&& visitor, V&& variant) requires(requires { typename std::remove_reference_t<V>::GREM_private_DerivedFromVariantTag; }) {
	return std::remove_reference_t<V>::visit(std::forward<Visitor>(visitor), std::forward<V>(variant));
}

/**
 * Check if a variant currently holds the alternative with the given type.
 *
 * \tparam T alternative type to check for. Must be one of the variant's listed
 *         alternative types.
 *
 * \param variant the variant to check.
 *
 * \return true if the variant holds a value of the given type, false
 *         otherwise.
 *
 * \sa Variant::is()
 */
template <typename T, typename... Ts>
[[nodiscard]] constexpr bool holds_alternative(const Variant<Ts...>& variant) noexcept {
	return variant.template is<T>();
}

/**
 * Check if a variant currently holds the alternative with the given index.
 *
 * \tparam Index alternative index to check for. Must be within the range of the
 *         variant's list of alternative types.
 *
 * \param variant the variant to check.
 *
 * \return true if the variant holds a value of the given alternative index,
 *         false otherwise.
 *
 * \sa Variant::is()
 */
template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr bool holds_alternative(const Variant<Ts...>& variant) noexcept {
	return variant.template is<Index>();
}

/**
 * Access the underlying value with the given type of a variant.
 *
 * \tparam T type of the currently active value to get. Must be one of the
 *         variant's listed alternative types.
 *
 * \param variant the variant to get the value of.
 *
 * \return a reference to the value held by the variant.
 *
 * \throws BadVariantAccess if the variant does not currently hold a value
 *         of the given type.
 *
 * \sa Variant::get()
 */
template <typename T, typename... Ts>
[[nodiscard]] constexpr T& get(Variant<Ts...>& variant) {
	return variant.template get<T>();
}

/**
 * Access the underlying value with the given type of a variant.
 *
 * \tparam T type of the currently active value to get. Must be one of the
 *         variant's listed alternative types.
 *
 * \param variant the variant to get the value of.
 *
 * \return a read-only reference to the value held by the variant.
 *
 * \throws BadVariantAccess if the variant does not currently hold a value
 *         of the given type.
 *
 * \sa Variant::get()
 */
template <typename T, typename... Ts>
[[nodiscard]] constexpr const T& get(const Variant<Ts...>& variant) {
	return variant.template get<T>();
}

/**
 * Access the underlying value with the given index of a variant.
 *
 * \tparam Index alternative index of the currently active value to get. Must be
 *         within the range of the variant's list of alternative types.
 *
 * \param variant the variant to get the value of.
 *
 * \return a reference to the value held by the variant.
 *
 * \throws BadVariantAccess if the variant does not currently hold the given
 *         alternative.
 *
 * \sa Variant::get()
 */
template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr auto& get(Variant<Ts...>& variant) {
	return variant.template get<Index>();
}

/**
 * Access the underlying value with the given index of a variant.
 *
 * \tparam Index alternative index of the currently active value to get. Must be
 *         within the range of the variant's list of alternative types.
 *
 * \param variant the variant to get the value of.
 *
 * \return a read-only reference to the value held by the variant.
 *
 * \throws BadVariantAccess if the variant does not currently hold the given
 *         alternative.
 *
 * \sa Variant::get()
 */
template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr const auto& get(const Variant<Ts...>& variant) {
	return variant.template get<Index>();
}

/**
 * Access the underlying value with the given type of a variant if it is the
 * currently active alternative.
 *
 * \tparam T type of the value to get. Must be one of the variant's listed
 *         alternative types.
 *
 * \param variant non-owning pointer to the variant to get the value of. Must
 *        not be nullptr.
 *
 * \return a non-owning pointer to the value held by the variant, or nullptr
 *         if the variant does not currently hold a value of the given type.
 *
 * \sa Variant::get_if()
 */
template <typename T, typename... Ts>
[[nodiscard]] constexpr T* get_if(Variant<Ts...>* variant) noexcept {
	GREM_ASSERT(variant);
	return variant->template get_if<T>();
}

/**
 * Access the underlying value with the given type of a variant if it is the
 * currently active alternative.
 *
 * \tparam T type of the value to get. Must be one of the variant's listed
 *         alternative types.
 *
 * \param variant non-owning read-only pointer to the variant to get the value
 *        of. Must not be nullptr.
 *
 * \return a non-owning read-only pointer to the value held by the variant, or
 *         nullptr if the variant does not currently hold a value of the given
 *         type.
 *
 * \sa Variant::get_if()
 */
template <typename T, typename... Ts>
[[nodiscard]] constexpr const T* get_if(const Variant<Ts...>* variant) noexcept {
	GREM_ASSERT(variant);
	return variant->template get_if<T>();
}

/**
 * Access the underlying value with the given index of a variant if it is the
 * currently active alternative.
 *
 * \tparam Index alternative index of the value to get. Must be within the range
 *         of the variant's list of alternative types.
 *
 * \param variant non-owning pointer to the variant to get the value of. Must
 *        not be nullptr.
 *
 * \return a non-owning pointer to the value held by the variant, or nullptr if
 *         the variant does not currently hold the given alternative.
 *
 * \sa Variant::get_if()
 */
template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr auto* get_if(Variant<Ts...>* variant) noexcept {
	GREM_ASSERT(variant);
	return variant->template get_if<Index>();
}

/**
 * Access the underlying value with the given index of a variant if it is the
 * currently active alternative.
 *
 * \tparam Index alternative index of the value to get. Must be within the range
 *         of the variant's list of alternative types.
 *
 * \param variant non-owning read-only pointer to the variant to get the value
 *        of. Must not be nullptr.
 *
 * \return a non-owning read-only pointer to the value held by the variant, or
 *         nullptr if the variant does not currently hold the given alternative.
 *
 * \sa Variant::get_if()
 */
template <std::size_t Index, typename... Ts>
[[nodiscard]] constexpr const auto* get_if(const Variant<Ts...>* variant) noexcept {
	GREM_ASSERT(variant);
	return variant->template get_if<Index>();
}

/**
 * Compare two variants for equality.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first and second variant hold the same alternative and
 *         their values compare equal, false otherwise.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator==(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() == b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.index() != b.index()) {
		return false;
	}
	if (a.valueless_by_exception()) {
		return true;
	}
	return visit([&]<typename T>(const T& value) -> bool { return value == b.template as<T>(); }, a);
}

/**
 * Compare two variants for inequality.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first and second variant hold different alternatives or
 *         if their values do not compare equal, false otherwise.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator!=(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() != b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.index() != b.index()) {
		return true;
	}
	if (a.valueless_by_exception()) {
		return false;
	}
	return visit([&]<typename T>(const T& value) -> bool { return value != b.template as<T>(); }, a);
}

/**
 * Check if a variant is less than another variant.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first variant is less than the second variant, based on
 *         their active alternatives and values.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator<(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() < b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.valueless_by_exception() && b.valueless_by_exception()) {
		return false;
	}
	if (a.valueless_by_exception()) {
		return true;
	}
	if (b.valueless_by_exception()) {
		return false;
	}
	if (a.index() != b.index()) {
		return a.index() < b.index();
	}
	return visit([&]<typename T>(const T& value) -> bool { return value < b.template as<T>(); }, a);
}

/**
 * Check if a variant is less than or equal to another variant.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first variant is less than or equal to the second
 *         variant, based on their active alternatives and values.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator<=(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() <= b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.valueless_by_exception() && b.valueless_by_exception()) {
		return true;
	}
	if (a.valueless_by_exception()) {
		return true;
	}
	if (b.valueless_by_exception()) {
		return false;
	}
	if (a.index() != b.index()) {
		return a.index() <= b.index();
	}
	return visit([&]<typename T>(const T& value) -> bool { return value <= b.template as<T>(); }, a);
}

/**
 * Check if a variant is greater than another variant.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first variant is greater than the second variant, based
 *         on their active alternatives and values.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator>(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() > b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.valueless_by_exception() && b.valueless_by_exception()) {
		return false;
	}
	if (a.valueless_by_exception()) {
		return false;
	}
	if (b.valueless_by_exception()) {
		return true;
	}
	if (a.index() != b.index()) {
		return a.index() > b.index();
	}
	return visit([&]<typename T>(const T& value) -> bool { return value > b.template as<T>(); }, a);
}

/**
 * Check if a variant is greater than or equal to another variant.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return true if the first variant is greater than or equal to the second
 *         variant, based on their active alternatives and values.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr bool operator>=(const Variant<Ts...>& a, const Variant<Ts...>& b) requires(requires {
	{ a.template as<Ts>() >= b.template as<Ts>() } -> detail::bool_convertible;
} && ...) {
	if (a.valueless_by_exception() && b.valueless_by_exception()) {
		return true;
	}
	if (a.valueless_by_exception()) {
		return false;
	}
	if (b.valueless_by_exception()) {
		return true;
	}
	if (a.index() != b.index()) {
		return a.index() >= b.index();
	}
	return visit([&]<typename T>(const T& value) -> bool { return value >= b.template as<T>(); }, a);
}

/**
 * Compare two variants.
 *
 * \param a first variant.
 * \param b second variant.
 *
 * \return an ordering of the first and second variant, based on their active
 *         alternatives and values.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         relevant alternative type.
 */
template <typename... Ts>
[[nodiscard]] constexpr std::common_comparison_category_t<std::compare_three_way_result_t<Ts>...> operator<=>(const Variant<Ts...>& a, const Variant<Ts...>& b) {
	if (a.valueless_by_exception() && b.valueless_by_exception()) {
		return std::strong_ordering::equal;
	}
	if (a.valueless_by_exception()) {
		return std::strong_ordering::less;
	}
	if (b.valueless_by_exception()) {
		return std::strong_ordering::greater;
	}
	if (a.index() != b.index()) {
		return a.index() <=> b.index();
	}
	return visit([&]<typename T>(const T& value) -> std::common_comparison_category_t<std::compare_three_way_result_t<Ts>...> { return value <=> b.template as<T>(); }, a);
}

/**
 * Visitor type for combining callable objects, such as lambdas, into an
 * overload set.
 *
 * \tparam Functors callable object types to combine.
 */
template <typename... Functors>
struct Overloaded : Functors... {
	using Functors::operator()...;
};

template <typename... Functors>
Overloaded(Functors...) -> Overloaded<Functors...>;

namespace detail {

template <typename V>
struct Matcher {
	V variant;

	template <typename... Functors>
	constexpr decltype(auto) operator()(Functors&&... functors) const {
		return visit(Overloaded{std::forward<Functors>(functors)...}, variant);
	}
};

} // namespace detail

/**
 * Choose a function overload to execute based on the active alternative of a
 * variant.
 *
 * \param variant forwarding reference to the variant to match on.
 *
 * \return an object containing the forwarded variant with a call operator that
 *         accepts a set of overloads that take each of the possible variant
 *         alternatives as a parameter and calls the overload corresponding to
 *         the currently active variant alternative. This call operator may
 *         throw BadVariantAccess if the variant is in the valueless by
 *         exception state.
 *
 * \throws BadVariantAccess if the variant is in the valueless by exception
 *         state.
 * \throws any exception thrown when forwarding the variant to the returned
 *         object.
 *
 * \sa visit()
 */
template <typename V>
[[nodiscard]] constexpr detail::Matcher<V> match(V&& variant) {
	return detail::Matcher<V>{std::forward<V>(variant)};
}

} // namespace grem

/**
 * Specialization of std::hash for grem::Variant.
 */
template <typename... Ts>
struct std::hash<grem::Variant<Ts...>> {
	[[nodiscard]] std::size_t operator()(const grem::Variant<Ts...>& variant) const {
		return visit(hasher, variant);
	}

private:
	template <typename T>
	class Hash {
	public:
		[[nodiscard]] std::size_t operator()(const T& value) const {
			return hasher(value);
		}

	private:
		[[no_unique_address]] std::hash<T> hasher;
	};

	class HashVisitor : public Hash<Ts>... {
	public:
		using Hash<Ts>::operator()...;
	};

	[[no_unique_address]] HashVisitor hasher;
};

namespace grem {
namespace detail {

template <typename Signature>
struct match_case_type;

template <typename Arg>
struct match_case_type<void(Arg)> {
	using type = Arg;
};

template <typename Arg>
struct match_case_type<void(Arg&)> {
	using type = Arg;
};

template <typename Arg>
struct match_case_type<void(Arg&&)> {
	using type = Arg;
};

template <typename Arg>
struct match_case_type<void(const Arg&)> {
	using type = Arg;
};

template <typename Arg>
struct match_case_type<void(const Arg&&)> {
	using type = Arg;
};

template <typename Signature>
using match_case_type_t = typename match_case_type<Signature>::type;

template <typename... Ts>
consteval Variant<Ts...> getVariantType(const Variant<Ts...>&);

} // namespace detail
} // namespace grem

#define GREM_MATCH(...) switch (auto&& GREM_private_matchVariant = __VA_ARGS__; GREM_private_matchVariant.index())
#define GREM_CASE(...) \
	case grem::variant_index_v<grem::detail::match_case_type_t<void(__VA_ARGS__)>, decltype(grem::detail::getVariantType(GREM_private_matchVariant))>: \
		if ([[maybe_unused]] __VA_ARGS__ = GREM_private_matchVariant.template as<grem::detail::match_case_type_t<void(__VA_ARGS__)>>(); true)
#define GREM_CASE_DEFAULT(...) \
	default: \
		if ([[maybe_unused]] __VA_ARGS__ = GREM_private_matchVariant; true)

#endif
