// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SPAN_HPP
#define GREM_CORE_DATA_SPAN_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>

#include <algorithm>        // std::equal, std::lexicographical_compare
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::data, std::size, std::reverse_iterator
#include <type_traits>      // std::remove_..._t, std::is_..._v, std::true_type, std::false_type, std::bool_constant

namespace grem {

inline constexpr std::size_t DYNAMIC_EXTENT = static_cast<std::size_t>(-1);

template <typename T, std::size_t Extent = DYNAMIC_EXTENT>
class Span;

namespace detail {

template <typename T>
struct is_span_impl : std::false_type {};

template <typename T, std::size_t Extent>
struct is_span_impl<grem::Span<T, Extent>> : std::true_type {};

template <typename T>
struct is_span : is_span_impl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_span_v = is_span<T>::value;

template <typename T>
struct is_inplace_array_impl : std::false_type {};

template <typename T, std::size_t Extent>
struct is_inplace_array_impl<Array<T, Extent>> : std::true_type {};

template <typename T>
struct is_inplace_array : is_inplace_array_impl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_inplace_array_v = is_inplace_array<T>::value;

template <typename T>
inline constexpr bool is_contiguous_range_v = requires(T t) {
	std::data(t);
	std::size(t);
};

template <std::size_t From, std::size_t To>
struct is_allowed_size_conversion : std::bool_constant<From == To || From == grem::DYNAMIC_EXTENT || To == grem::DYNAMIC_EXTENT> {};

template <std::size_t From, std::size_t To>
inline constexpr bool is_allowed_size_conversion_v = is_allowed_size_conversion<From, To>::value;

template <typename From, typename To>
struct is_allowed_element_type_conversion : std::bool_constant<std::is_convertible_v<From (*)[], To (*)[]>> {};

template <typename From, typename To>
inline constexpr bool is_allowed_element_type_conversion_v = is_allowed_element_type_conversion<From, To>::value;

template <typename T, std::size_t Extent>
class SpanBase {
public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr SpanBase() noexcept = default;

	constexpr SpanBase(pointer data, [[maybe_unused]] size_type size)
		: elements(data) {
		GREM_ASSERT(size == Extent);
	}

	template <size_type N>
	constexpr SpanBase(SpanBase<T, N> other)
		: elements(other.data()) {
		static_assert(N == Extent || N == DYNAMIC_EXTENT);
		GREM_ASSERT(other.size() == Extent);
	}

	[[nodiscard]] constexpr pointer data() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return Extent;
	}

private:
	pointer elements = nullptr;
};

template <typename T>
class SpanBase<T, DYNAMIC_EXTENT> {
public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr SpanBase() noexcept = default;

	constexpr SpanBase(pointer data, size_type size)
		: elements(data)
		, elementCount(size) {}

	template <size_type N>
	constexpr explicit SpanBase(SpanBase<T, N> other)
		: elements(other.data())
		, elementCount(other.size()) {}

	[[nodiscard]] constexpr pointer data() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elementCount;
	}

private:
	pointer elements = nullptr;
	size_type elementCount = 0;
};

template <typename T, std::size_t Extent, std::size_t Offset, std::size_t N>
struct subspan_type {
	using type = grem::Span<T, (N != grem::DYNAMIC_EXTENT) ? N : (Extent != grem::DYNAMIC_EXTENT) ? Extent - Offset : Extent>;
};

template <typename T, std::size_t Extent, std::size_t Offset, std::size_t N>
using subspan_type_t = typename subspan_type<T, Extent, Offset, N>::type;

} // namespace detail

template <typename T, std::size_t Extent>
class Span : public detail::SpanBase<T, Extent> {
private:
	using Base = detail::SpanBase<T, Extent>;

	static constexpr size_t EXTENT = Extent;

public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = std::remove_cv_t<T>;
	using reference = T&;
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr Span() noexcept = default;

	constexpr Span(pointer data, size_type size)
		: Base(data, size) {}

	template <typename U, std::size_t N>
	constexpr Span(const Span<U, N>& other) requires(detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_element_type_conversion_v<U, T>)
		: Span(other.data(), other.size()) {}

	constexpr Span(pointer begin, pointer end)
		: Span(begin, static_cast<size_type>(end - begin)) {}

	template <std::size_t N>
	constexpr Span(T (&arr)[N]) noexcept
		: Span(std::data(arr), N) {}

	template <std::size_t N>
	constexpr Span(Array<value_type, N>& arr) noexcept requires(N != 0)
		: Span(std::data(arr), N) {}

	constexpr Span(Array<value_type, 0>&) noexcept
		: Span() {}

	template <std::size_t N>
	constexpr Span(const Array<value_type, N>& arr) noexcept requires(N != 0)
		: Span(std::data(arr), N) {}

	constexpr Span(const Array<value_type, 0>&) noexcept
		: Span() {}

	constexpr explicit(EXTENT != DYNAMIC_EXTENT) Span(std::initializer_list<value_type> ilist) requires(std::is_const_v<T>)
		: Span(ilist.begin(), ilist.size()) {}

	template <typename R>
	constexpr explicit(EXTENT != DYNAMIC_EXTENT) Span(R&& range) // NOLINT(cppcoreguidelines-missing-std-forward)
		requires(detail::is_contiguous_range_v<R> && !detail::is_span_v<std::remove_cvref_t<R>> && !detail::is_inplace_array_v<std::remove_cvref_t<R>> &&
				 !std::is_array_v<std::remove_cvref_t<R>> && std::is_convertible_v<decltype(std::data(range)), pointer>)
		: Span(std::data(range), std::size(range)) {}

	[[nodiscard]] constexpr pointer data() const noexcept {
		return Base::data();
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return Base::size();
	}

	[[nodiscard]] constexpr iterator begin() const noexcept {
		return data();
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return data();
	}

	[[nodiscard]] constexpr iterator end() const noexcept {
		return data() + size();
	}

	[[nodiscard]] constexpr const_iterator cend() const noexcept {
		return data() + size();
	}

	[[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
		return reverse_iterator{end()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
		return const_reverse_iterator{cend()};
	}

	[[nodiscard]] constexpr reverse_iterator rend() const noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
		return const_reverse_iterator{cbegin()};
	}

	[[nodiscard]] constexpr reference operator[](size_type i) const noexcept {
		GREM_ASSERT(i < size());
		return data()[i];
	}

	[[nodiscard]] constexpr reference operator()(size_type i) const noexcept {
		GREM_ASSERT(i < size());
		return data()[i];
	}

	[[nodiscard]] constexpr size_type size_bytes() const noexcept {
		return size() * sizeof(T);
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] constexpr reference front() const noexcept {
		GREM_ASSERT(!empty());
		return data()[0];
	}

	[[nodiscard]] constexpr reference back() const noexcept {
		GREM_ASSERT(!empty());
		return data()[size() - 1];
	}

	template <std::size_t N>
	[[nodiscard]] constexpr Span<T, N> first() const {
		static_assert(N != DYNAMIC_EXTENT && N <= EXTENT);
		return {data(), N};
	}

	template <std::size_t N>
	[[nodiscard]] constexpr Span<T, N> last() const {
		static_assert(N != DYNAMIC_EXTENT && N <= EXTENT);
		return {data() + (EXTENT - N), N};
	}

	template <std::size_t Offset, std::size_t N = DYNAMIC_EXTENT>
	[[nodiscard]] constexpr detail::subspan_type_t<T, EXTENT, Offset, N> subspan() const {
		static_assert(Offset <= EXTENT);
		return {data() + Offset, (N == DYNAMIC_EXTENT) ? size() - Offset : N};
	}

	[[nodiscard]] constexpr Span<T, DYNAMIC_EXTENT> first(size_type n) const {
		GREM_ASSERT(n <= size());
		return {data(), n};
	}

	[[nodiscard]] constexpr Span<T, DYNAMIC_EXTENT> last(size_type n) const {
		return subspan(size() - n);
	}

	[[nodiscard]] constexpr Span<T, DYNAMIC_EXTENT> subspan(size_type offset, size_type n = DYNAMIC_EXTENT) const {
		if constexpr (EXTENT == DYNAMIC_EXTENT) {
			GREM_ASSERT(offset <= size());
			if (n == DYNAMIC_EXTENT) {
				return {data() + offset, size() - offset};
			}
			GREM_ASSERT(n <= size());
			GREM_ASSERT(offset + n <= size());
			return {data() + offset, n};
		} else {
			return Span<T, DYNAMIC_EXTENT>{*this}.subspan(offset, n);
		}
	}
};

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator==(Span<T, N> a, Span<T, M> b) {
	return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator!=(Span<T, N> a, Span<T, M> b) {
	return !(a == b);
}

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator<(Span<T, N> a, Span<T, M> b) {
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator<=(Span<T, N> a, Span<T, M> b) {
	return !(a > b);
}

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator>(Span<T, N> a, Span<T, M> b) {
	return b < a;
}

template <typename T, std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator>=(Span<T, N> a, Span<T, M> b) {
	return !(a < b);
}

template <typename R>
Span(R&& range) -> Span<std::remove_reference_t<decltype(*std::data(range))>>;

template <typename T, std::size_t N>
Span(T (&)[N]) -> Span<T, N>;

template <typename T, std::size_t N>
Span(Array<T, N>&) -> Span<T, N>;

template <typename T, std::size_t N>
Span(const Array<T, N>&) -> Span<const T, N>;

template <typename Pointer, typename PointerOrSize>
Span(Pointer data, PointerOrSize&&) -> Span<std::remove_reference_t<decltype(data[0])>>;

template <typename T, std::size_t N>
auto asBytes(Span<T, N> s) noexcept {
	if constexpr (N == DYNAMIC_EXTENT) {
		return Span<const std::byte, DYNAMIC_EXTENT>{reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
	} else {
		return Span<const std::byte, sizeof(T) * N>{reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
	}
}

template <typename T, std::size_t N>
auto asWritableBytes(Span<T, N> s) noexcept {
	if constexpr (N == DYNAMIC_EXTENT) {
		return Span<std::byte, DYNAMIC_EXTENT>{reinterpret_cast<std::byte*>(s.data()), s.size_bytes()};
	} else {
		return Span<std::byte, sizeof(T) * N>{reinterpret_cast<std::byte*>(s.data()), s.size_bytes()};
	}
}

} // namespace grem

#endif
