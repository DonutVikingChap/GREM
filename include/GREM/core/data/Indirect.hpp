// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_INDIRECT_HPP
#define GREM_CORE_DATA_INDIRECT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>

#include <compare>         // std::strong_ordering, std::compare_three_way_result_t
#include <functional>      // std::hash
#include <memory>          // std::allocator, std::allocator_traits, std::to_address, std::allocator_arg...
#include <memory_resource> // std::pmr::polymorphic_allocator
#include <type_traits>     // std::is_..._v, std::remove_..._t, std::decay_t
#include <utility>         // std::move, std::forward

namespace grem {

template <typename T, typename Allocator = std::allocator<T>>
class Indirect {
public:
	using value_type = T;
	using allocator_type = Allocator;
	using reference = T&;
	using const_reference = const T&;
	using pointer = typename std::allocator_traits<Allocator>::pointer;
	using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

	constexpr Indirect(std::allocator_arg_t, const Allocator& allocator = Allocator()) requires(std::is_default_constructible_v<T>)
		: allocator(allocator) {
		emplace();
	}

	template <typename Arg, typename... Args>
	explicit(!std::is_convertible_v<Arg&&, T>) constexpr Indirect(std::allocator_arg_t, const Allocator& allocator, Arg&& arg, Args&&... args)
		requires(std::is_constructible_v<T, Arg &&, Args && ...>)
		: allocator(allocator) {
		emplace(std::forward<Arg>(arg), std::forward<Args>(args)...);
	}

	template <typename U, typename... Args>
	constexpr Indirect(std::allocator_arg_t, const Allocator& allocator, std::initializer_list<U> ilist, Args&&... args)
		requires(std::is_constructible_v<T, std::initializer_list<U>&, Args && ...>)
		: allocator(allocator) {
		emplace(ilist, std::forward<Args>(args)...);
	}

	template <typename Arg, typename... Args>
	explicit(!std::is_convertible_v<Arg&&, T>) constexpr Indirect(Arg&& arg, Args&&... args)
		requires(!std::is_same_v<std::remove_cvref_t<Arg>, Indirect> && !std::is_convertible_v<Arg &&, std::allocator_arg_t> && std::is_constructible_v<T, Arg &&, Args && ...>)
		: Indirect(std::allocator_arg, Allocator(), std::forward<Arg>(arg), std::forward<Args>(args)...) {}

	template <typename U, typename... Args>
	explicit constexpr Indirect(std::initializer_list<U> ilist, Args&&... args) requires(std::is_constructible_v<T, std::initializer_list<U>&, Args && ...>)
		: Indirect(std::allocator_arg, Allocator(), ilist, std::forward<Args>(args)...) {}

	constexpr Indirect(const Indirect& other)
		: Indirect(std::allocator_arg, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator()), other) {}

	constexpr Indirect(std::allocator_arg_t, const Allocator& allocator, const Indirect& other)
		: allocator(allocator) {
		if (other.object) {
			emplace(*other);
		}
	}

	constexpr Indirect(Indirect&& other) noexcept
		: Indirect(std::allocator_arg, other.get_allocator(), std::move(other)) {}

	constexpr Indirect(std::allocator_arg_t, const Allocator& allocator, Indirect&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: object(std::exchange(other.object, nullptr))
		, allocator(allocator) {}

	~Indirect() {
		reset();
	}

	constexpr Indirect& operator=(const Indirect& other) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					reset();
				}
			}
			allocator = other.get_allocator();
		}
		if (other.object) {
			if (object) {
				**this = *other;
			} else {
				*this = Indirect(std::allocator_arg, allocator, other);
			}
		} else {
			reset();
		}
		return *this;
	}

	constexpr Indirect& operator=(Indirect&& other) noexcept(
		std::allocator_traits<
			Allocator>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<Allocator>::is_always_equal::value) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<Allocator>::is_always_equal::value) {
			reset();
			object = std::exchange(other.object, nullptr);
			if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
				allocator = other.allocator;
			}
		} else {
			if (allocator == other.allocator) {
				reset();
				object = std::exchange(other.object, nullptr);
			} else {
				GREM_ASSERT(other.object);
				*this = Indirect(std::allocator_arg, allocator, other);
			}
		}
		return *this;
	}

	template <typename U = T>
	constexpr Indirect& operator=(U&& value)
		requires(!std::is_same_v<std::remove_cvref_t<U>, Indirect> && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U> &&
				 (!std::is_scalar_v<T> || !std::is_same_v<T, std::decay_t<U>>)) {
		if (object) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
			**this = std::forward<U>(value);
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
		} else {
			emplace(std::forward<U>(value));
		}
		return *this;
	}

	template <typename U>
	constexpr Indirect& operator=(const Indirect<U>& other)
		requires(std::is_constructible_v<T, const U&> && std::is_assignable_v<T&, const U&> && !std::is_constructible_v<T, Indirect<U>&> &&
				 !std::is_constructible_v<T, const Indirect<U>&> && !std::is_constructible_v<T, Indirect<U> &&> && !std::is_constructible_v<T, const Indirect<U> &&> &&
				 !std::is_convertible_v<Indirect<U>&, T> && !std::is_convertible_v<const Indirect<U>&, T> && !std::is_convertible_v<Indirect<U> &&, T> &&
				 !std::is_convertible_v<const Indirect<U> &&, T> && !std::is_assignable_v<T&, Indirect<U>&> && !std::is_assignable_v<T&, const Indirect<U>&> &&
				 !std::is_assignable_v<T&, Indirect<U> &&> && !std::is_assignable_v<T&, const Indirect<U> &&>) {
		if (object) {
			if (other.object) {
				**this = **other;
			} else {
				reset();
			}
		} else if (other.object) {
			emplace(*other);
		}
		return *this;
	}

	template <typename U>
	constexpr Indirect& operator=(Indirect<U>&& other) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(std::is_constructible_v<T, U> && std::is_assignable_v<T&, U> && !std::is_constructible_v<T, Indirect<U>&> && !std::is_constructible_v<T, const Indirect<U>&> &&
				 !std::is_constructible_v<T, Indirect<U> &&> && !std::is_constructible_v<T, const Indirect<U> &&> && !std::is_convertible_v<Indirect<U>&, T> &&
				 !std::is_convertible_v<const Indirect<U>&, T> && !std::is_convertible_v<Indirect<U> &&, T> && !std::is_convertible_v<const Indirect<U> &&, T> &&
				 !std::is_assignable_v<T&, Indirect<U>&> && !std::is_assignable_v<T&, const Indirect<U>&> && !std::is_assignable_v<T&, Indirect<U> &&> &&
				 !std::is_assignable_v<T&, const Indirect<U> &&>) {
		if (object) {
			if (other.object) {
				**this = std::move(**other);
			} else {
				reset();
			}
		} else if (other.object) {
			emplace(std::move(*other));
		}
		return *this;
	}

	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] constexpr pointer get() const noexcept {
		return object;
	}

	[[nodiscard]] constexpr pointer operator->() const noexcept {
		GREM_ASSERT(object);
		return object;
	}

	[[nodiscard]] constexpr reference operator*() const noexcept {
		GREM_ASSERT(object);
		return *object;
	}

private:
	template <typename OtherT, typename OtherAllocator>
	friend class Indirect;

	void reset() noexcept {
		if (object) {
			std::allocator_traits<Allocator>::destroy(allocator, std::to_address(object));
			std::allocator_traits<Allocator>::deallocate(allocator, object, 1);
			object = nullptr;
		}
	}

	template <typename... Args>
	reference emplace(Args&&... args) {
		reset();
		object = std::allocator_traits<Allocator>::allocate(allocator, 1);
		try {
			std::allocator_traits<Allocator>::construct(allocator, std::to_address(object), std::forward<Args>(args)...);
		} catch (...) {
			std::allocator_traits<Allocator>::deallocate(allocator, object, 1);
			throw;
		}
		return *object;
	}

	pointer object = nullptr;
	[[no_unique_address]] Allocator allocator;
};

template <typename A, typename AllocatorA, typename B, typename AllocatorB>
[[nodiscard]] constexpr bool operator==(const Indirect<A, AllocatorA>& a, const Indirect<B, AllocatorB>& b) requires(requires(const A x, const B y) { x == y; }) {
	if (a.get() && b.get()) {
		return *a == *b;
	}
	return static_cast<bool>(a.get()) == static_cast<bool>(b.get());
}

template <typename A, typename AllocatorA, typename B>
[[nodiscard]] constexpr bool operator==(const Indirect<A, AllocatorA>& a, const B& b)
	requires(!std::is_same_v<std::remove_cvref_t<B>, Indirect<A, AllocatorA>> &&
			 (std::is_same_v<std::remove_cv_t<A>, bool> || !template_specialization_of<std::remove_cvref_t<B>, Indirect>) && equality_comparable<A, B>) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
	return (a.get()) ? *a == b : false;
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

template <typename A, typename AllocatorA, typename B, typename AllocatorB>
[[nodiscard]] constexpr std::compare_three_way_result_t<A, B> operator<=>(const Indirect<A, AllocatorA>& a, const Indirect<B, AllocatorB>& b) requires three_way_comparable<A, B> {
	if (a.get() && b.get()) {
		return *a <=> *b;
	}
	return static_cast<bool>(a.get()) <=> static_cast<bool>(b.get());
}

template <typename A, typename AllocatorA, typename B>
[[nodiscard]] constexpr std::compare_three_way_result_t<A, B> operator<=>(const Indirect<A, AllocatorA>& a, const B& b)
	requires(!std::is_same_v<std::remove_cvref_t<B>, Indirect<A, AllocatorA>> &&
			 (std::is_same_v<std::remove_cv_t<A>, bool> || !template_specialization_of<std::remove_cvref_t<B>, Indirect>) && three_way_comparable<A, B>) {
	return (a.get()) ? *a <=> b : std::strong_ordering::less;
}

} // namespace grem

template <typename T, typename Allocator>
struct std::hash<grem::Indirect<T, Allocator>> {
	[[nodiscard]] std::size_t operator()(const grem::Indirect<T, Allocator>& indirect) const {
		return (indirect.get()) ? hasher(*indirect) : 0;
	}

private:
	[[no_unique_address]] std::hash<T> hasher;
};

namespace grem::pmr {

template <typename T>
using Indirect = grem::Indirect<T, std::pmr::polymorphic_allocator<T>>;

} // namespace grem::pmr

#endif
