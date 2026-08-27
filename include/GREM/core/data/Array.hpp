// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ARRAY_HPP
#define GREM_CORE_DATA_ARRAY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <iterator>    // std::reverse_iterator
#include <stdexcept>   // std::out_of_range
#include <type_traits> // std::is_..._v, std::remove_cv_t, std::integral_constant
#include <utility>     // std::move, std::tuple_size, std::tuple_element, std::...index_sequence

namespace grem {

template <typename T, std::size_t N>
struct Array {
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	T _private_elements[N];

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= N) {
			throw std::out_of_range{"pos >= size()"};
		}
		return _private_elements[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= N) {
			throw std::out_of_range{"pos >= size()"};
		}
		return _private_elements[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < N);
		return _private_elements[pos];
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < N);
		return _private_elements[pos];
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return _private_elements[0];
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return _private_elements[0];
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return _private_elements[N - 1];
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return _private_elements[N - 1];
	}

	[[nodiscard]] constexpr pointer data() noexcept {
		return _private_elements;
	}

	[[nodiscard]] constexpr const_pointer data() const noexcept {
		return _private_elements;
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{data()};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{data()};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{data() + N};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{data() + N};
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

	[[nodiscard]] constexpr bool empty() const noexcept {
		return N == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return N;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return N;
	}

	constexpr void fill(const T& value) {
		for (size_type i = 0; i < N; ++i) {
			_private_elements[i] = value;
		}
	}

	constexpr void swap(Array& other) noexcept(std::is_nothrow_swappable_v<T>) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		using std::swap;
		for (size_type i = 0; i < N; ++i) {
			swap(_private_elements[i], other._private_elements[i]);
		}
	}

	friend constexpr void swap(Array& a, Array& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	[[nodiscard]] bool operator==(const Array& other) const = default;
	[[nodiscard]] auto operator<=>(const Array& other) const = default;

	template <std::size_t Index>
	[[nodiscard]] friend constexpr T& get(Array& a) noexcept {
		return a[Index];
	}

	template <std::size_t Index>
	[[nodiscard]] friend constexpr const T& get(const Array& a) noexcept {
		return a[Index];
	}
};

template <typename T>
struct Array<T, 0> {
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	[[nodiscard]] constexpr reference at(size_type) {
		throw std::out_of_range{"pos >= size()"};
	}

	[[nodiscard]] constexpr const_reference at(size_type) const {
		throw std::out_of_range{"pos >= size()"};
	}

	[[nodiscard]] constexpr reference operator[](size_type) {
		unreachable();
	}

	[[nodiscard]] constexpr const_reference operator[](size_type) const {
		unreachable();
	}

	[[nodiscard]] constexpr reference front() {
		unreachable();
	}

	[[nodiscard]] constexpr const_reference front() const {
		unreachable();
	}

	[[nodiscard]] constexpr reference back() {
		unreachable();
	}

	[[nodiscard]] constexpr const_reference back() const {
		unreachable();
	}

	[[nodiscard]] constexpr pointer data() noexcept {
		return nullptr;
	}

	[[nodiscard]] constexpr const_pointer data() const noexcept {
		return nullptr;
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{data()};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{data()};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return begin();
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return begin();
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

	[[nodiscard]] constexpr bool empty() const noexcept {
		return true;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return 0;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return 0;
	}

	constexpr void fill(const T&) {}

	constexpr void swap(Array&) noexcept(std::is_nothrow_swappable_v<T>) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
	}

	friend constexpr void swap(Array& a, Array& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	[[nodiscard]] constexpr bool operator==(const Array& other) const = default;
	[[nodiscard]] constexpr auto operator<=>(const Array& other) const = default;

	template <std::size_t Index>
	[[nodiscard]] friend constexpr T& get(Array& a) noexcept {
		return a[Index];
	}

	template <std::size_t Index>
	[[nodiscard]] friend constexpr const T& get(const Array& a) noexcept {
		return a[Index];
	}
};

template <typename T, typename... Us>
requires(std::is_same_v<T, Us> && ...) Array(T, Us...) -> Array<T, 1 + sizeof...(Us)>;

template <typename T, std::size_t N>
constexpr Array<std::remove_cv_t<T>, N> toArray(T (&a)[N]) {
	return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> Array<std::remove_cv_t<T>, N> {
		return {{a[Indices]...}};
	}(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
constexpr Array<std::remove_cv_t<T>, N> toArray(T (&&a)[N]) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) -> Array<std::remove_cv_t<T>, N> {
		return {{std::move(a[Indices])...}};
	}(std::make_index_sequence<N>{});
}

} // namespace grem

template <typename T, std::size_t N>
struct std::tuple_size<grem::Array<T, N>> : std::integral_constant<std::size_t, N> {};

template <std::size_t Index, typename T, std::size_t N>
struct std::tuple_element<Index, grem::Array<T, N>> {
	using type = T;
};

#endif
