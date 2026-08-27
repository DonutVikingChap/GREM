// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_OPTIONAL_HPP
#define GREM_CORE_DATA_OPTIONAL_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Variant.hpp>

#include <compare>          // std::strong_ordering, std::compare_three_way_result_t
#include <cstddef>          // std::size_t
#include <exception>        // std::exception
#include <functional>       // std::hash, std::invoke
#include <initializer_list> // std::initializer_list
#include <type_traits>      // std::is_..._v, std::remove_..._t, std::decay_t, std::invoke_result_t
#include <utility>          // std::move, std::forward, std::in_place...

namespace grem {

/**
 * Unit type for representing an empty Optional.
 */
struct Nullopt {
	/**
	 * Constructor ensuring that Nullopt can only be constructed explicitly.
	 */
	constexpr explicit Nullopt(int) {}
};

/**
 * Unit value representing an empty Optional.
 */
inline constexpr Nullopt nullopt{0};

/**
 * Exception type that is thrown on an attempt to erroneously access an empty
 * Optional when using a safe access function such as Optional::value().
 */
struct BadOptionalAccess : std::exception {
	BadOptionalAccess() noexcept = default;

	[[nodiscard]] const char* what() const noexcept override {
		return "Bad optional access.";
	}
};

/**
 * Optional value type that holds either a value or no value.
 *
 * Its API mimics that of std::optional, including some functions added in
 * C++23.
 *
 * \tparam T type of the value that can be held by the optional.
 */
template <typename T>
class Optional {
public:
	/**
	 * Construct an empty optional.
	 */
	constexpr Optional() noexcept {} // NOLINT(modernize-use-equals-default)

	/**
	 * Construct an empty optional.
	 *
	 * \param value unit value representing an empty optional.
	 */
	constexpr Optional(Nullopt value) noexcept {
		(void)value;
	}

	/**
	 * Construct an optional with an active value in-place.
	 *
	 * \param tag std::in_place, marking that this constructor should be used
	 *        instead of the copy/move constructor.
	 * \param args arguments to pass to the underlying value's constructor.
	 *
	 * \throws any exception thrown by the underlying value's constructor.
	 */
	template <typename... Args>
	constexpr explicit Optional(std::in_place_t tag, Args&&... args) requires(std::is_constructible_v<T, Args...>)
		: variant(std::in_place_type<T>, std::forward<Args>(args)...) {
		(void)tag;
	}

	/**
	 * Construct an optional with an active value in-place, with an initializer
	 * list as the first constructor argument.
	 *
	 * \param tag std::in_place, marking that this constructor should be used
	 *        instead of the copy/move constructor.
	 * \param ilist first argument to pass to the underlying value's
	 *        constructor.
	 * \param args subsequent arguments to pass to the underlying value's
	 *        constructor.
	 *
	 * \throws any exception thrown by the underlying value's constructor.
	 */
	template <typename U, typename... Args>
	constexpr Optional(std::in_place_t tag, std::initializer_list<U> ilist, Args&&... args) requires(std::is_constructible_v<T, std::initializer_list<U>&, Args...>)
		: variant(std::in_place_type<T>, ilist, std::forward<Args>(args)...) {
		(void)tag;
	}

	/**
	 * Implicit converting constructor.
	 *
	 * \param value underlying value to construct the optional from. Must be
	 *        convertible to the underlying value type.
	 *
	 * \throws any exception thrown by the underlying value's constructor.
	 */
	template <typename U = std::remove_cv_t<T>>
	constexpr Optional(U&& value)
		requires(!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> && !std::is_same_v<std::remove_cvref_t<U>, Optional> &&
				 (!std::is_same_v<std::remove_cv_t<T>, bool> || !template_specialization_of<std::remove_cvref_t<U>, Optional>) && std::is_constructible_v<T, U &&> &&
				 std::is_convertible_v<U &&, T>)
		: variant(std::in_place_type<T>, std::forward<U>(value)) {}

	/**
	 * Explicit converting constructor.
	 *
	 * \param value underlying value to construct the optional from. Must be
	 *        convertible to the underlying value type.
	 *
	 * \throws any exception thrown by the underlying value's constructor.
	 */
	template <typename U = std::remove_cv_t<T>>
	constexpr explicit Optional(U&& value)
		requires(!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> && !std::is_same_v<std::remove_cvref_t<U>, Optional> &&
				 (!std::is_same_v<std::remove_cv_t<T>, bool> || !template_specialization_of<std::remove_cvref_t<U>, Optional>) && std::is_constructible_v<T, U &&> &&
				 !std::is_convertible_v<U &&, T>)
		: variant(std::in_place_type<T>, std::forward<U>(value)) {}

	/** Destructor. */
	constexpr ~Optional() = default;

	/** Copy constructor. */
	constexpr Optional(const Optional& other) = default;

	/** Move constructor. */
	constexpr Optional(Optional&& other) noexcept(
		std::is_nothrow_move_constructible_v<T>) = default; // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)

	/** Implicit converting copy constructor. */
	template <typename U>
	constexpr Optional(const Optional<U>& other)
		requires((std::is_same_v<std::remove_cv_t<T>, bool> ||
					 (!std::is_constructible_v<T, Optional<U>&> && !std::is_constructible_v<T, const Optional<U>&> && !std::is_constructible_v<T, Optional<U> &&> &&
						 !std::is_constructible_v<T, const Optional<U> &&> && !std::is_convertible_v<Optional<U>&, T> && !std::is_convertible_v<const Optional<U>&, T> &&
						 !std::is_convertible_v<Optional<U> &&, T> && !std::is_convertible_v<const Optional<U> &&, T>)) &&
				 std::is_constructible_v<T, const U&> && std::is_convertible_v<const U&, T>)
		: Optional() {
		if (other) {
			emplace(*other);
		}
	}

	/** Explicit converting copy constructor. */
	template <typename U>
	constexpr explicit Optional(const Optional<U>& other)
		requires((std::is_same_v<std::remove_cv_t<T>, bool> ||
					 (!std::is_constructible_v<T, Optional<U>&> && !std::is_constructible_v<T, const Optional<U>&> && !std::is_constructible_v<T, Optional<U> &&> &&
						 !std::is_constructible_v<T, const Optional<U> &&> && !std::is_convertible_v<Optional<U>&, T> && !std::is_convertible_v<const Optional<U>&, T> &&
						 !std::is_convertible_v<Optional<U> &&, T> && !std::is_convertible_v<const Optional<U> &&, T>)) &&
				 std::is_constructible_v<T, const U&> && !std::is_convertible_v<const U&, T>)
		: Optional() {
		if (other) {
			emplace(*other);
		}
	}

	/** Implicit converting move constructor. */
	template <typename U>
	constexpr Optional(Optional<U>&& other) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires((std::is_same_v<std::remove_cv_t<T>, bool> ||
					 (!std::is_constructible_v<T, Optional<U>&> && !std::is_constructible_v<T, const Optional<U>&> && !std::is_constructible_v<T, Optional<U> &&> &&
						 !std::is_constructible_v<T, const Optional<U> &&> && !std::is_convertible_v<Optional<U>&, T> && !std::is_convertible_v<const Optional<U>&, T> &&
						 !std::is_convertible_v<Optional<U> &&, T> && !std::is_convertible_v<const Optional<U> &&, T>)) &&
				 std::is_constructible_v<T, U &&> && std::is_convertible_v<U &&, T>)
		: Optional() {
		if (other) {
			emplace(std::move(*other));
		}
	}

	/** Explicit converting move constructor. */
	template <typename U>
	constexpr explicit Optional(Optional<U>&& other) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires((std::is_same_v<std::remove_cv_t<T>, bool> ||
					 (!std::is_constructible_v<T, Optional<U>&> && !std::is_constructible_v<T, const Optional<U>&> && !std::is_constructible_v<T, Optional<U> &&> &&
						 !std::is_constructible_v<T, const Optional<U> &&> && !std::is_convertible_v<Optional<U>&, T> && !std::is_convertible_v<const Optional<U>&, T> &&
						 !std::is_convertible_v<Optional<U> &&, T> && !std::is_convertible_v<const Optional<U> &&, T>)) &&
				 std::is_constructible_v<T, U &&> && !std::is_convertible_v<U &&, T>)
		: Optional() {
		if (other) {
			emplace(std::move(*other));
		}
	}

	/**
	 * Destroy the underlying value if present and reset the optional to an
	 * empty state.
	 */
	constexpr Optional& operator=(Nullopt) noexcept {
		reset();
		return *this;
	}

	/**
	 * Converting assignment.
	 *
	 * \param value underlying value to construct the new value from. Must be
	 *        convertible to the underlying value type.
	 *
	 * \throws any exception thrown by the underlying constructor or assignment
	 *         of the underlying value.
	 */
	template <typename U = std::remove_cv_t<T>>
	constexpr Optional& operator=(U&& value)
		requires(!std::is_same_v<std::remove_cvref_t<U>, Optional> && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U> &&
				 (!std::is_scalar_v<T> || !std::is_same_v<T, std::decay_t<U>>)) {
		if (*this) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4267)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
			**this = std::forward<U>(value);
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
		} else {
			emplace(std::forward<U>(value));
		}
		return *this;
	}

	/** Copy assignment. */
	constexpr Optional& operator=(const Optional& other) = default;

	/** Move assignment. */
	constexpr Optional& operator=(Optional&& other) noexcept(
		std::is_nothrow_move_constructible_v<T> && // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::is_nothrow_move_assignable_v<T>) = default;

	/** Converting copy assignment. */
	template <typename U>
	constexpr Optional& operator=(const Optional<U>& other)
		requires(std::is_constructible_v<T, const U&> && std::is_assignable_v<T&, const U&> && !std::is_constructible_v<T, Optional<U>&> &&
				 !std::is_constructible_v<T, const Optional<U>&> && !std::is_constructible_v<T, Optional<U> &&> && !std::is_constructible_v<T, const Optional<U> &&> &&
				 !std::is_convertible_v<Optional<U>&, T> && !std::is_convertible_v<const Optional<U>&, T> && !std::is_convertible_v<Optional<U> &&, T> &&
				 !std::is_convertible_v<const Optional<U> &&, T> && !std::is_assignable_v<T&, Optional<U>&> && !std::is_assignable_v<T&, const Optional<U>&> &&
				 !std::is_assignable_v<T&, Optional<U> &&> && !std::is_assignable_v<T&, const Optional<U> &&>) {
		if (*this) {
			if (other) {
				**this = **other;
			} else {
				reset();
			}
		} else if (other) {
			emplace(*other);
		}
		return *this;
	}

	/** Converting move assignment. */
	template <typename U>
	constexpr Optional& operator=(Optional<U>&& other) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(std::is_constructible_v<T, U> && std::is_assignable_v<T&, U> && !std::is_constructible_v<T, Optional<U>&> && !std::is_constructible_v<T, const Optional<U>&> &&
				 !std::is_constructible_v<T, Optional<U> &&> && !std::is_constructible_v<T, const Optional<U> &&> && !std::is_convertible_v<Optional<U>&, T> &&
				 !std::is_convertible_v<const Optional<U>&, T> && !std::is_convertible_v<Optional<U> &&, T> && !std::is_convertible_v<const Optional<U> &&, T> &&
				 !std::is_assignable_v<T&, Optional<U>&> && !std::is_assignable_v<T&, const Optional<U>&> && !std::is_assignable_v<T&, Optional<U> &&> &&
				 !std::is_assignable_v<T&, const Optional<U> &&>) {
		if (*this) {
			if (other) {
				**this = std::move(**other);
			} else {
				reset();
			}
		} else if (other) {
			emplace(std::move(*other));
		}
		return *this;
	}

	/**
	 * Access the underlying value by pointer without a safety check.
	 *
	 * \return a non-owning read-only pointer to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr const T* operator->() const noexcept {
		return &**this;
	}

	/**
	 * Access the underlying value by pointer without a safety check.
	 *
	 * \return a non-owning pointer to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr T* operator->() noexcept {
		return &**this;
	}

	/**
	 * Access the underlying value without a safety check.
	 *
	 * \return a read-only reference to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr const T& operator*() const& noexcept {
		return variant.template as<T>();
	}

	/**
	 * Access the underlying value without a safety check.
	 *
	 * \return a reference to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr T& operator*() & noexcept {
		return variant.template as<T>();
	}

	/**
	 * Access the underlying value without a safety check.
	 *
	 * \return a read-only rvalue reference to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr const T&& operator*() const&& noexcept {
		return std::move(variant.template as<T>());
	}

	/**
	 * Access the underlying value without a safety check.
	 *
	 * \return an rvalue reference to the underlying value.
	 *
	 * \warning If the optional is empty, the behavior is undefined.
	 */
	[[nodiscard]] constexpr T&& operator*() && noexcept {
		return std::move(variant.template as<T>());
	}

	/**
	 * Check if the optional contains a value.
	 *
	 * \return true if the optional contains a value, false if the optional is
	 *         empty.
	 */
	constexpr explicit operator bool() const noexcept {
		return has_value();
	}

	/**
	 * Check if the optional contains a value.
	 *
	 * \return true if the optional contains a value, false if the optional is
	 *         empty.
	 */
	[[nodiscard]] constexpr bool has_value() const noexcept {
		return variant.template is<T>();
	}

	/**
	 * Access the underlying value.
	 *
	 * \return a reference to the underlying value.
	 *
	 * \throws BadOptionalAccess if the optional is empty.
	 */
	[[nodiscard]] constexpr T& value() & {
		if (!*this) {
			throw BadOptionalAccess{};
		}
		return **this;
	}

	/**
	 * Access the underlying value.
	 *
	 * \return a read-only reference to the underlying value.
	 *
	 * \throws BadOptionalAccess if the optional is empty.
	 */
	[[nodiscard]] constexpr const T& value() const& {
		if (!*this) {
			throw BadOptionalAccess{};
		}
		return **this;
	}

	/**
	 * Access the underlying value.
	 *
	 * \return an rvalue reference to the underlying value.
	 *
	 * \throws BadOptionalAccess if the optional is empty.
	 */
	[[nodiscard]] constexpr T&& value() && {
		if (!*this) {
			throw BadOptionalAccess{};
		}
		return std::move(**this);
	}

	/**
	 * Access the underlying value.
	 *
	 * \return a read-only rvalue reference to the underlying value.
	 *
	 * \throws BadOptionalAccess if the optional is empty.
	 */
	[[nodiscard]] constexpr const T&& value() const&& {
		if (!*this) {
			throw BadOptionalAccess{};
		}
		return std::move(**this);
	}

	/**
	 * Get the underlying value, or a fallback value in case the optional is
	 * empty.
	 *
	 * \param defaultValue value to return in case the optional is empty.
	 *
	 * \return a copy of the underlying value, or the given default value if the
	 *         optional is empty.
	 */
	template <typename U>
	[[nodiscard]] constexpr T value_or(U&& defaultValue) const& {
		return (*this) ? **this : static_cast<T>(std::forward<U>(defaultValue));
	}

	/**
	 * Get the underlying value, or a fallback value in case the optional is
	 * empty.
	 *
	 * \param defaultValue value to return in case the optional is empty.
	 *
	 * \return a move of the underlying value, or the given default value if the
	 *         optional is empty.
	 */
	template <typename U>
	[[nodiscard]] constexpr T value_or(U&& defaultValue) && {
		return (*this) ? std::move(**this) : static_cast<T>(std::forward<U>(defaultValue));
	}

	/**
	 * Invoke a function with the underlying value, or return a
	 * default-constructed result in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a reference to
	 *        the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or a default-constructed
	 *         value of the function's return type if the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto and_then(F&& f) & {
		if (*this) {
			return std::invoke(std::forward<F>(f), **this);
		}
		return std::remove_cvref_t<std::invoke_result_t<F, T&>>{};
	}

	/**
	 * Invoke a function with the underlying value, or return a
	 * default-constructed result in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a read-only
	 *        reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or a default-constructed
	 *         value of the function's return type if the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto and_then(F&& f) const& {
		if (*this) {
			return std::invoke(std::forward<F>(f), **this);
		}
		return std::remove_cvref_t<std::invoke_result_t<F, const T&>>{};
	}

	/**
	 * Invoke a function with the underlying value, or return a
	 * default-constructed result in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept an rvalue
	 *        reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or a default-constructed
	 *         value of the function's return type if the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto and_then(F&& f) && {
		if (*this) {
			return std::invoke(std::forward<F>(f), std::move(**this));
		}
		return std::remove_cvref_t<std::invoke_result_t<F, T>>{};
	}

	/**
	 * Invoke a function with the underlying value, or return a
	 * default-constructed result in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a read-only
	 *        rvalue reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or a default-constructed
	 *         value of the function's return type if the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto and_then(F&& f) const&& {
		if (*this) {
			return std::invoke(std::forward<F>(f), std::move(**this));
		}
		return std::remove_cvref_t<std::invoke_result_t<F, const T>>{};
	}

	/**
	 * Invoke a function with the underlying value, or return an empty optional
	 * in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a reference to
	 *        the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or an empty optional if
	 *         the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto transform(F&& f) & {
		using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
		if (*this) {
			return Optional<U>{std::in_place, std::invoke(std::forward<F>(f), **this)};
		}
		return Optional<U>{};
	}

	/**
	 * Invoke a function with the underlying value, or return an empty optional
	 * in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a read-only
	 *        reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or an empty optional if
	 *         the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto transform(F&& f) const& {
		using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
		if (*this) {
			return Optional<U>{std::in_place, std::invoke(std::forward<F>(f), **this)};
		}
		return Optional<U>{};
	}

	/**
	 * Invoke a function with the underlying value, or return an empty optional
	 * in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept an rvalue
	 *        reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or an empty optional if
	 *         the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto transform(F&& f) && {
		using U = std::remove_cv_t<std::invoke_result_t<F, T>>;
		if (*this) {
			return Optional<U>{std::in_place, std::invoke(std::forward<F>(f), std::move(**this))};
		}
		return Optional<U>{};
	}

	/**
	 * Invoke a function with the underlying value, or return an empty optional
	 * in case the optional is empty.
	 *
	 * \param f function to invoke, which must be able to accept a read-only
	 *        rvalue reference to the underlying value as a parameter.
	 *
	 * \return the result of the function invocation, or an empty optional if
	 *         the optional is empty.
	 *
	 * \throws any exception thrown by the given function.
	 */
	template <typename F>
	constexpr auto transform(F&& f) const&& {
		using U = std::remove_cv_t<std::invoke_result_t<F, const T>>;
		if (*this) {
			return Optional<U>{std::in_place, std::invoke(std::forward<F>(f), std::move(**this))};
		}
		return Optional<U>{};
	}

	/**
	 * Get an optional containing the underlying value, or the result of a
	 * function in case the optional is empty.
	 *
	 * \param f function to invoke in case the optional is empty.
	 *
	 * \return a copy of the optional, or the result of the given function if
	 *         the optional is empty.
	 */
	template <typename F>
	constexpr Optional or_else(F&& f) const& {
		return (*this) ? *this : std::invoke(std::forward<F>(f));
	}

	/**
	 * Get an optional containing the underlying value, or the result of a
	 * function in case the optional is empty.
	 *
	 * \param f function to invoke in case the optional is empty.
	 *
	 * \return a move of the optional, or the result of the given function if
	 *         the optional is empty.
	 */
	template <typename F>
	constexpr Optional or_else(F&& f) && {
		return (*this) ? std::move(*this) : std::invoke(std::forward<F>(f));
	}

	/**
	 * Swap this optional's value with that of another.
	 *
	 * \param other optional to swap with.
	 *
	 * \throws any exception thrown by the underlying constructor, assignment or
	 *         swap implementation of the value type.
	 */
	constexpr void swap(Optional& other) noexcept(
		std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		variant.swap(other.variant);
	}

	/**
	 * Swap the values of two optionals.
	 *
	 * \param a first optional.
	 * \param b second optional.
	 *
	 * \throws any exception thrown by the underlying constructor, assignment or
	 *         swap implementation of the value type.
	 */
	friend constexpr void swap(Optional& a, Optional& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	/**
	 * Destroy the underlying value if present and reset the optional to an
	 * empty state.
	 */
	constexpr void reset() noexcept {
		variant.template emplace<0>();
	}

	/**
	 * Construct a new underlying value, destroying the old value if present.
	 *
	 * \param args arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the value
	 *         type.
	 */
	template <typename... Args>
	constexpr T& emplace(Args&&... args) requires(std::is_constructible_v<T, Args...>) {
		return variant.template emplace<1>(std::forward<Args>(args)...);
	}

	/**
	 * Construct a new underlying value, with an initializer list as the first
	 * constructor argument, destroying the old value if present.
	 *
	 * \param ilist first argument to pass to the new value's constructor.
	 * \param args subsequent arguments to pass to the new value's constructor.
	 *
	 * \throws any exception thrown by the underlying constructor of the value
	 *         type.
	 */
	template <typename U, typename... Args>
	constexpr T& emplace(std::initializer_list<U> ilist, Args&&... args) requires(std::is_constructible_v<T, std::initializer_list<U>&, Args...>) {
		return variant.template emplace<1>(ilist, std::forward<Args>(args)...);
	}

private:
	Variant<Monostate, T> variant;
};

/**
 * Compare two optionals for equality.
 *
 * \param a first optional.
 * \param b second optional.
 *
 * \return true if the first and second optional are either both empty or if
 *         their values compare equal, false otherwise.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const Optional<T>& a, const Optional<U>& b) requires equality_comparable<T, U> {
	if (a && b) {
		return *a == *b;
	}
	return static_cast<bool>(a) == static_cast<bool>(b);
}

/**
 * Check if an optional is empty.
 *
 * \param value unit value representing an empty optional.
 * \param opt optional to check.
 *
 * \return true if the optional is empty, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool operator==(Nullopt value, const Optional<T>& opt) {
	(void)value;
	return !static_cast<bool>(opt);
}

/**
 * Check if an optional is empty.
 *
 * \param opt optional to check.
 * \param value unit value representing an empty optional.
 *
 * \return true if the optional is empty, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool operator==(const Optional<T>& opt, Nullopt value) {
	(void)value;
	return !static_cast<bool>(opt);
}

/**
 * Compare an optional to a value for equality.
 *
 * \param opt optional to compare.
 * \param value value to compare against.
 *
 * \return true if the optional contains a value that is equal to the given
 *         value, false otherwise.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const Optional<T>& opt, const U& value) requires(!template_specialization_of<std::remove_cvref_t<U>, Optional> && equality_comparable<T, U>)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4389)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
	return (opt) ? *opt == value : false;
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

/**
 * Compare an optional to a value for equality.
 *
 * \param value value to compare against.
 * \param opt optional to compare.
 *
 * \return true if the optional contains a value that is equal to the given
 *         value, false otherwise.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr bool operator==(const U& value, const Optional<T>& opt) requires(!template_specialization_of<std::remove_cvref_t<U>, Optional> && equality_comparable<U, T>)
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
	return (opt) ? *opt == value : false;
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

/**
 * Compare two optionals.
 *
 * \param a first optional.
 * \param b second optional.
 *
 * \return an ordering of the first and second optional, based on whether
 *         they are empty or not as well as the ordering of their underlying
 *         values if present.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr std::compare_three_way_result_t<T, U> operator<=>(const Optional<T>& a, const Optional<U>& b) requires three_way_comparable<T, U> {
	if (a && b) {
		return *a <=> *b;
	}
	return static_cast<bool>(a) <=> static_cast<bool>(b);
}

/**
 * Compare an optional against an empty optional.
 *
 * \param opt optional to compare.
 * \param value unit value representing an empty optional.
 *
 * \return an ordering of the optional, based on whether it is empty or not.
 */
template <typename T>
[[nodiscard]] constexpr std::strong_ordering operator<=>(const Optional<T>& opt, Nullopt value) {
	(void)value;
	return static_cast<bool>(opt) <=> false;
}

/**
 * Compare an optional to a value.
 *
 * \param opt optional to compare.
 * \param value value to compare against.
 *
 * \return an ordering of the optional and the value, based on whether the
 *         optional is empty or not as well as the ordering of its
 *         underlying value compared to the given value if present.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr std::compare_three_way_result_t<T, U> operator<=>(const Optional<T>& opt, const U& value)
	requires(!template_specialization_of<std::remove_cvref_t<U>, Optional> && three_way_comparable<T, U>) {
	return (opt) ? *opt <=> value : std::strong_ordering::less;
}

/**
 * Compare an optional to a value.
 *
 * \param value value to compare against.
 * \param opt optional to compare.
 *
 * \return an ordering of the optional and the value, based on whether the
 *         optional is empty or not as well as the ordering of its
 *         underlying value compared to the given value if present.
 *
 * \throws any exception thrown by the underlying comparison operator of the
 *         value type.
 */
template <typename T, typename U>
[[nodiscard]] constexpr std::compare_three_way_result_t<U, T> operator<=>(const U& value, const Optional<T>& opt)
	requires(!template_specialization_of<std::remove_cvref_t<U>, Optional> && three_way_comparable<U, T>) {
	return (opt) ? value <=> *opt : std::strong_ordering::greater;
}

} // namespace grem

/**
 * Specialization of std::hash for grem::Optional.
 */
template <typename T>
struct std::hash<grem::Optional<T>> {
	[[nodiscard]] std::size_t operator()(const grem::Optional<T>& opt) const {
		if (opt) {
			return hasher(*opt);
		}
		return static_cast<std::size_t>(0xFFFFFFFFFFFFF2FBull);
	}

private:
	[[no_unique_address]] std::hash<std::remove_const_t<T>> hasher;
};

#endif
