// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_STRIDED_SPAN_HPP
#define GREM_CORE_DATA_STRIDED_SPAN_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>

#include <algorithm>        // std::equal, std::lexicographical_compare
#include <cstddef>          // std::byte, std::size_t, std::ptrdiff_t
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::data, std::size, std::reverse_iterator
#include <type_traits>      // std::remove_..._t, std::is_..._v, std::true_type, std::false_type, std::bool_constant, std::conditional_t

namespace grem {

inline constexpr std::size_t DYNAMIC_STRIDE = static_cast<std::size_t>(-1);

template <typename T, std::size_t Extent = DYNAMIC_EXTENT, std::size_t Stride = DYNAMIC_STRIDE>
class StridedSpan;

namespace detail {

template <typename T>
struct is_strided_span_impl : std::false_type {};

template <typename T, std::size_t Extent>
struct is_strided_span_impl<grem::StridedSpan<T, Extent>> : std::true_type {};

template <typename T>
struct is_strided_span : is_strided_span_impl<std::remove_cv_t<T>> {};

template <typename T>
inline constexpr bool is_strided_span_v = is_strided_span<T>::value;

template <std::size_t From, std::size_t To>
struct is_allowed_stride_conversion : std::bool_constant<From == To || From == grem::DYNAMIC_STRIDE || To == grem::DYNAMIC_STRIDE> {};

template <std::size_t From, std::size_t To>
inline constexpr bool is_allowed_stride_conversion_v = is_allowed_stride_conversion<From, To>::value;

template <typename T, std::size_t Extent, std::size_t Stride>
class StridedSpanBase {
public:
	static_assert(Stride >= sizeof(T), "Stride must be greater than or equal to the stride of a single element.");

	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr StridedSpanBase() noexcept = default;

	constexpr StridedSpanBase(pointer base, [[maybe_unused]] size_type size)
		: elements(base) {
		GREM_ASSERT(size == Extent);
	}

	constexpr StridedSpanBase(pointer base, [[maybe_unused]] size_type size, [[maybe_unused]] size_type stride)
		: elements(base) {
		GREM_ASSERT(size == Extent);
		GREM_ASSERT(stride == Stride);
	}

	template <size_type N, size_type S>
	constexpr StridedSpanBase(StridedSpanBase<T, N, S> other)
		: elements(other.base()) {
		static_assert(N == Extent || N == DYNAMIC_EXTENT);
		static_assert(S == Stride || S == DYNAMIC_STRIDE);
		GREM_ASSERT(other.size() == Extent);
		GREM_ASSERT(other.stride() == Stride);
	}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return Extent;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return Stride;
	}

private:
	pointer elements = nullptr;
};

template <typename T, std::size_t Stride>
class StridedSpanBase<T, DYNAMIC_EXTENT, Stride> {
public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr StridedSpanBase() noexcept = default;

	constexpr StridedSpanBase(pointer data, size_type size)
		: elements(data)
		, elementCount(size) {}

	constexpr StridedSpanBase(pointer data, size_type size, [[maybe_unused]] size_type stride)
		: elements(data)
		, elementCount(size) {
		GREM_ASSERT(stride == Stride);
	}

	template <size_type N, size_type S>
	constexpr explicit StridedSpanBase(StridedSpanBase<T, N, S> other)
		: elements(other.data())
		, elementCount(other.size()) {
		static_assert(S == Stride || S == DYNAMIC_STRIDE);
		GREM_ASSERT(other.stride() == Stride);
	}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elementCount;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return Stride;
	}

private:
	pointer elements = nullptr;
	size_type elementCount = 0;
};

template <typename T, std::size_t Extent>
class StridedSpanBase<T, Extent, DYNAMIC_STRIDE> {
public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr StridedSpanBase() noexcept = default;

	constexpr StridedSpanBase(pointer data, [[maybe_unused]] size_type size)
		: elements(data)
		, elementStride(sizeof(T)) {
		GREM_ASSERT(size == Extent);
	}

	constexpr StridedSpanBase(pointer data, [[maybe_unused]] size_type size, size_type stride)
		: elements(data)
		, elementStride(stride) {
		GREM_ASSERT(size == Extent);
		GREM_ASSERT(stride >= sizeof(T));
	}

	template <size_type N, size_type S>
	constexpr explicit StridedSpanBase(StridedSpanBase<T, N, S> other)
		: elements(other.data())
		, elementStride(other.stride()) {
		static_assert(N == Extent || N == DYNAMIC_EXTENT);
		GREM_ASSERT(other.size() == Extent);
	}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return Extent;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return elementStride;
	}

private:
	pointer elements = nullptr;
	size_type elementStride = sizeof(T);
};

template <typename T>
class StridedSpanBase<T, DYNAMIC_EXTENT, DYNAMIC_STRIDE> {
public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	constexpr StridedSpanBase() noexcept = default;

	constexpr StridedSpanBase(pointer data, size_type size)
		: elements(data)
		, elementCount(size)
		, elementStride(sizeof(T)) {}

	constexpr StridedSpanBase(pointer data, size_type size, size_type stride)
		: elements(data)
		, elementCount(size)
		, elementStride(stride) {
		GREM_ASSERT(stride >= sizeof(T));
	}

	template <size_type N, size_type S>
	constexpr explicit StridedSpanBase(StridedSpanBase<T, N, S> other)
		: elements(other.data())
		, elementCount(other.size())
		, elementStride(other.stride()) {}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elementCount;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return elementStride;
	}

private:
	pointer elements = nullptr;
	size_type elementCount = 0;
	size_type elementStride = sizeof(T);
};

template <typename T, std::size_t Extent, std::size_t Stride, std::size_t Offset, std::size_t N>
struct strided_subspan_type {
	using type = grem::StridedSpan<T, (N != grem::DYNAMIC_EXTENT) ? N : (Extent != grem::DYNAMIC_EXTENT) ? Extent - Offset : Extent, Stride>;
};

template <typename T, std::size_t Extent, std::size_t Stride, std::size_t Offset, std::size_t N>
using strided_subspan_type_t = typename strided_subspan_type<T, Extent, Stride, Offset, N>::type;

template <typename T, std::size_t Stride>
class StridedSpanIteratorBase {
public:
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	constexpr StridedSpanIteratorBase() noexcept = default;

	constexpr StridedSpanIteratorBase(pointer base, [[maybe_unused]] size_type stride)
		: element(base) {
		GREM_ASSERT(stride == Stride);
	}

	template <size_type S>
	constexpr StridedSpanIteratorBase(StridedSpanIteratorBase<T, S> other)
		: element(other.base()) {
		static_assert(S == Stride || S == DYNAMIC_STRIDE);
		GREM_ASSERT(other.stride() == Stride);
	}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return element;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return Stride;
	}

protected:
	void advance(difference_type n) {
		GREM_ASSERT(element);
		using BytePointer = std::conditional_t<std::is_const_v<T>, const std::byte*, std::byte*>;
		element = reinterpret_cast<pointer>(reinterpret_cast<BytePointer>(element) + n * static_cast<difference_type>(Stride));
	}

private:
	pointer element;
};

template <typename T>
class StridedSpanIteratorBase<T, DYNAMIC_STRIDE> {
public:
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	constexpr StridedSpanIteratorBase() noexcept = default;

	constexpr StridedSpanIteratorBase(pointer base, size_type stride)
		: element(base)
		, elementStride(stride) {
		GREM_ASSERT(stride >= sizeof(T));
	}

	template <size_type S>
	constexpr StridedSpanIteratorBase(StridedSpanIteratorBase<T, S> other)
		: element(other.base())
		, elementStride(other.stride()) {}

	[[nodiscard]] constexpr pointer base() const noexcept {
		return element;
	}

	[[nodiscard]] constexpr size_type stride() const noexcept {
		return elementStride;
	}

protected:
	void advance(difference_type n) {
		GREM_ASSERT(element || n == 0);
		using BytePointer = std::conditional_t<std::is_const_v<T>, const std::byte*, std::byte*>;
		element = reinterpret_cast<pointer>(reinterpret_cast<BytePointer>(element) + n * static_cast<difference_type>(elementStride));
	}

private:
	pointer element;
	size_t elementStride;
};

template <typename T, std::size_t Stride>
class StridedSpanIterator : public StridedSpanIteratorBase<T, Stride> {
public:
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using iterator_category = std::random_access_iterator_tag;

	StridedSpanIterator() = default;

	constexpr StridedSpanIterator(pointer base, size_type stride) noexcept
		: StridedSpanIteratorBase<T, Stride>(base, stride) {}

	constexpr operator StridedSpanIterator<const T, Stride>() const noexcept requires(!std::is_const_v<T>) {
		return StridedSpanIterator<const T, Stride>{base(), stride()};
	}

	using StridedSpanIteratorBase<T, Stride>::base;
	using StridedSpanIteratorBase<T, Stride>::stride;

	[[nodiscard]] constexpr reference operator*() const {
		return *std::launder(base());
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return std::launder(base());
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	constexpr StridedSpanIterator& operator++() {
		this->advance(1);
		return *this;
	}

	constexpr StridedSpanIterator& operator--() {
		this->advance(-1);
		return *this;
	}

	constexpr StridedSpanIterator operator++(int) {
		StridedSpanIterator old = *this;
		++*this;
		return old;
	}

	constexpr StridedSpanIterator operator--(int) {
		StridedSpanIterator old = *this;
		--*this;
		return old;
	}

	constexpr StridedSpanIterator& operator+=(difference_type n) {
		this->advance(n);
		return *this;
	}

	constexpr StridedSpanIterator& operator-=(difference_type n) {
		this->advance(-n);
		return *this;
	}

	[[nodiscard]] friend constexpr StridedSpanIterator operator+(StridedSpanIterator a, difference_type b) {
		a += b;
		return a;
	}

	[[nodiscard]] friend constexpr StridedSpanIterator operator+(difference_type a, StridedSpanIterator b) {
		b += a;
		return b;
	}

	[[nodiscard]] friend constexpr StridedSpanIterator operator-(StridedSpanIterator a, difference_type b) {
		a -= b;
		return a;
	}

	[[nodiscard]] friend constexpr difference_type operator-(StridedSpanIterator a, StridedSpanIterator b) {
		GREM_ASSERT(a.stride() == b.stride());
		return static_cast<difference_type>(reinterpret_cast<const std::byte*>(a.base()) - reinterpret_cast<const std::byte*>(b.base())) / static_cast<difference_type>(a.stride());
	}

	[[nodiscard]] friend constexpr bool operator==(StridedSpanIterator a, StridedSpanIterator b) {
		GREM_ASSERT(a.stride() == b.stride());
		return a.base() == b.base();
	}

	[[nodiscard]] friend constexpr auto operator<=>(StridedSpanIterator a, StridedSpanIterator b) {
		GREM_ASSERT(a.stride() == b.stride());
		return a.base() <=> b.base();
	}
};

} // namespace detail

template <typename T, std::size_t Extent, std::size_t Stride>
class StridedSpan : public detail::StridedSpanBase<T, Extent, Stride> {
private:
	using Base = detail::StridedSpanBase<T, Extent, Stride>;

	static constexpr size_t EXTENT = Extent;
	static constexpr size_t STRIDE = Stride;

public:
	using element_type = T;
	using pointer = T*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using value_type = std::remove_cv_t<T>;
	using reference = T&;
	using iterator = detail::StridedSpanIterator<T, STRIDE>;
	using const_iterator = detail::StridedSpanIterator<const T, STRIDE>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr StridedSpan() noexcept = default;

	constexpr StridedSpan(pointer base, size_type size)
		: Base(base, size) {}

	constexpr StridedSpan(pointer base, size_type size, size_type stride)
		: Base(base, size, stride) {}

	template <typename U, typename Outer>
	constexpr StridedSpan(Outer* data, size_type size, U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((size == 0) ? nullptr : &(data->*member), size, sizeof(Outer)) {}

	template <typename U, std::size_t N, std::size_t S>
	constexpr StridedSpan(const StridedSpan<U, N, S>& other)
		requires(detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<S, STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(other.base(), other.size(), other.stride()) {}

	template <typename U, std::size_t N, std::size_t S, typename Outer>
	constexpr StridedSpan(const StridedSpan<Outer, N, S>& outer, U Outer::* member)
		requires(detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<S, STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((outer.empty()) ? nullptr : &(outer.base()->*member), outer.size(), outer.stride()) {}

	template <typename U, std::size_t N, std::size_t S, typename Outer>
	constexpr StridedSpan(const StridedSpan<const Outer, N, S>& outer, U Outer::* member)
		requires(detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<S, STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((outer.empty()) ? nullptr : &(outer.base()->*member), outer.size(), outer.stride()) {}

	template <typename U, std::size_t N>
	constexpr StridedSpan(const Span<U, N>& other)
		requires(detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<sizeof(T), STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(other.data(), other.size()) {}

	template <typename U, std::size_t N, typename Outer>
	constexpr StridedSpan(const Span<Outer, N>& outer, U Outer::* member) requires(
		detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<sizeof(Outer), STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((outer.empty()) ? nullptr : &(outer.data()->*member), outer.size(), sizeof(Outer)) {}

	template <typename U, std::size_t N, typename Outer>
	constexpr StridedSpan(const Span<const Outer, N>& outer, U Outer::* member) requires(
		detail::is_allowed_size_conversion_v<N, EXTENT> && detail::is_allowed_stride_conversion_v<sizeof(Outer), STRIDE> && detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((outer.empty()) ? nullptr : &(outer.data()->*member), outer.size(), sizeof(Outer)) {}

	constexpr StridedSpan(pointer begin, pointer end)
		: StridedSpan(begin, static_cast<size_type>(end - begin)) {}

	constexpr StridedSpan(pointer begin, pointer end, size_type stride)
		: StridedSpan(begin, static_cast<size_type>(end - begin) / stride, stride) {
		GREM_ASSERT(static_cast<size_type>(end - begin) % stride == 0);
	}

	template <typename U, typename Outer>
	constexpr StridedSpan(Outer* begin, Outer* end, U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((begin == end) ? nullptr : &(begin->*member), static_cast<size_type>(end - begin), sizeof(Outer)) {}

	template <typename U, typename Outer>
	constexpr StridedSpan(const Outer* begin, const Outer* end, U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan((begin == end) ? nullptr : &(begin->*member), static_cast<size_type>(end - begin), sizeof(Outer)) {}

	template <std::size_t N>
	constexpr StridedSpan(T (&arr)[N]) noexcept
		: StridedSpan(std::data(arr), N) {}

	template <typename U, typename Outer, std::size_t N>
	constexpr StridedSpan(Outer (&arr)[N], U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(&arr[0].*member, N, sizeof(Outer)) {}

	template <typename U, typename Outer, std::size_t N>
	constexpr StridedSpan(const Outer (&arr)[N], U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(&arr[0].*member, N, sizeof(Outer)) {}

	template <std::size_t N>
	constexpr StridedSpan(Array<value_type, N>& arr) noexcept requires(N != 0)
		: StridedSpan(std::data(arr), N) {}

	template <typename U, typename Outer, std::size_t N>
	constexpr StridedSpan(Array<Outer, N>& arr, U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(&(arr.data()->*member), N, sizeof(Outer)) {}

	constexpr StridedSpan(Array<value_type, 0>&) noexcept
		: StridedSpan() {}

	template <typename U, typename Outer>
	constexpr StridedSpan(Array<Outer, 0>&, U Outer::*) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan() {}

	template <std::size_t N>
	constexpr StridedSpan(const Array<value_type, N>& arr) noexcept requires(N != 0)
		: StridedSpan(std::data(arr), N) {}

	template <typename U, typename Outer, std::size_t N>
	constexpr StridedSpan(const Array<Outer, N>& arr, U Outer::* member) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan(&(arr.data()->*member), N, sizeof(Outer)) {}

	constexpr StridedSpan(const Array<value_type, 0>&) noexcept
		: StridedSpan() {}

	template <typename U, typename Outer>
	constexpr StridedSpan(const Array<Outer, 0>&, U Outer::*) requires(detail::is_allowed_element_type_conversion_v<U, T>)
		: StridedSpan() {}

	constexpr explicit(EXTENT != DYNAMIC_EXTENT) StridedSpan(std::initializer_list<value_type> ilist) requires(std::is_const_v<T>)
		: StridedSpan(ilist.begin(), ilist.size()) {}

	template <typename R>
	constexpr explicit(EXTENT != DYNAMIC_EXTENT) StridedSpan(R&& range) // NOLINT(cppcoreguidelines-missing-std-forward)
		requires(detail::is_contiguous_range_v<R> && !detail::is_strided_span_v<std::remove_cvref_t<R>> && !detail::is_inplace_array_v<std::remove_cvref_t<R>> &&
				 !std::is_array_v<std::remove_cvref_t<R>> && std::is_convertible_v<decltype(std::data(range)), pointer>)
		: StridedSpan(std::data(range), std::size(range)) {}

	template <typename OuterR, typename U, typename Outer>
	constexpr StridedSpan(OuterR&& range, U Outer::* member) // NOLINT(cppcoreguidelines-missing-std-forward)
		requires(detail::is_contiguous_range_v<OuterR> && !detail::is_strided_span_v<std::remove_cvref_t<OuterR>> && !detail::is_inplace_array_v<std::remove_cvref_t<OuterR>> &&
				 !std::is_array_v<std::remove_cvref_t<OuterR>> && detail::is_allowed_element_type_conversion_v<U, T> && std::is_convertible_v<decltype(std::data(range)), Outer*>)
		: StridedSpan(std::data(range), std::size(range), member) {}

	using Base::base;
	using Base::size;
	using Base::stride;

	[[nodiscard]] constexpr iterator begin() const noexcept {
		return iterator{base(), stride()};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return const_iterator{base(), stride()};
	}

	[[nodiscard]] constexpr iterator end() const noexcept {
		return begin() + static_cast<difference_type>(size());
	}

	[[nodiscard]] constexpr const_iterator cend() const noexcept {
		return begin() + static_cast<difference_type>(size());
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
		return begin()[static_cast<difference_type>(i)];
	}

	[[nodiscard]] constexpr reference operator()(size_type i) const noexcept {
		GREM_ASSERT(i < size());
		return begin()[static_cast<difference_type>(i)];
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] constexpr reference front() const noexcept {
		GREM_ASSERT(!empty());
		return *base();
	}

	[[nodiscard]] constexpr reference back() const noexcept {
		GREM_ASSERT(!empty());
		return begin()[size() - 1];
	}

	template <std::size_t N>
	[[nodiscard]] constexpr StridedSpan<T, N, STRIDE> first() const {
		static_assert(N != DYNAMIC_EXTENT && N <= EXTENT);
		return {base(), N, stride()};
	}

	template <std::size_t N>
	[[nodiscard]] constexpr StridedSpan<T, N, STRIDE> last() const {
		static_assert(N != DYNAMIC_EXTENT && N <= EXTENT);
		return {(begin() + static_cast<difference_type>(EXTENT - N)).base(), N, stride()};
	}

	template <std::size_t Offset, std::size_t N = DYNAMIC_EXTENT>
	[[nodiscard]] constexpr detail::strided_subspan_type_t<T, EXTENT, STRIDE, Offset, N> subspan() const {
		static_assert(Offset <= EXTENT);
		return {(begin() + static_cast<difference_type>(Offset)).base(), (N == DYNAMIC_EXTENT) ? size() - Offset : N, stride()};
	}

	[[nodiscard]] constexpr StridedSpan<T, DYNAMIC_EXTENT, STRIDE> first(size_type n) const {
		GREM_ASSERT(n <= size());
		return {base(), n, stride()};
	}

	[[nodiscard]] constexpr StridedSpan<T, DYNAMIC_EXTENT, STRIDE> last(size_type n) const {
		return subspan(size() - n);
	}

	[[nodiscard]] constexpr StridedSpan<T, DYNAMIC_EXTENT, STRIDE> subspan(size_type offset, size_type n = DYNAMIC_EXTENT) const {
		if constexpr (EXTENT == DYNAMIC_EXTENT) {
			GREM_ASSERT(offset <= size());
			if (n == DYNAMIC_EXTENT) {
				return {(begin() + static_cast<difference_type>(offset)).base(), size() - offset, stride()};
			}
			GREM_ASSERT(n <= size());
			GREM_ASSERT(offset + n <= size());
			return {(begin() + static_cast<difference_type>(offset)).base(), n, stride()};
		} else {
			return StridedSpan<T, DYNAMIC_EXTENT, STRIDE>{*this}.subspan(offset, n);
		}
	}
};

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator==(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator!=(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return !(a == b);
}

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator<(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator<=(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return !(a > b);
}

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator>(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return b < a;
}

template <typename T, std::size_t N, std::size_t M, std::size_t S, std::size_t R>
[[nodiscard]] constexpr bool operator>=(StridedSpan<T, N, S> a, StridedSpan<T, M, R> b) {
	return !(a < b);
}

template <typename R>
StridedSpan(R&& range) -> StridedSpan<std::remove_reference_t<decltype(*std::data(range))>, DYNAMIC_EXTENT, sizeof(std::remove_reference_t<decltype(*std::data(range))>)>;

template <typename OuterR, typename T, typename Outer>
StridedSpan(OuterR&& range, T Outer::*)
	-> StridedSpan<std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(*std::data(range))>>, const T, T>, DYNAMIC_EXTENT, sizeof(Outer)>;

template <typename T, std::size_t N>
StridedSpan(T (&)[N]) -> StridedSpan<T, N, sizeof(T)>;

template <typename T, std::size_t N, typename Outer>
StridedSpan(Outer (&)[N], T Outer::*) -> StridedSpan<T, N, sizeof(Outer)>;

template <typename T, std::size_t N, typename Outer>
StridedSpan(const Outer (&)[N], T Outer::*) -> StridedSpan<const T, N, sizeof(Outer)>;

template <typename T, std::size_t N>
StridedSpan(Array<T, N>&) -> StridedSpan<T, N, sizeof(T)>;

template <typename T, std::size_t N, typename Outer>
StridedSpan(Array<Outer, N>&, T Outer::*) -> StridedSpan<T, N, sizeof(Outer)>;

template <typename T, std::size_t N>
StridedSpan(const Array<T, N>&) -> StridedSpan<const T, N, sizeof(T)>;

template <typename T, std::size_t N, typename Outer>
StridedSpan(const Array<Outer, N>&, T Outer::*) -> StridedSpan<const T, N, sizeof(Outer)>;

template <typename Pointer, typename PointerOrSize>
StridedSpan(Pointer base, PointerOrSize&&) -> StridedSpan<std::remove_reference_t<decltype(base[0])>>;

template <typename T, std::size_t N, std::size_t S>
auto as_strided_bytes(StridedSpan<T, N, S> s) noexcept {
	if constexpr (N == DYNAMIC_EXTENT) {
		return StridedSpan<const std::byte, DYNAMIC_EXTENT, S>{reinterpret_cast<const std::byte*>(s.base()), s.size(), s.stride()};
	} else {
		return StridedSpan<const std::byte, sizeof(T) * N, S>{reinterpret_cast<const std::byte*>(s.base()), s.size(), s.stride()};
	}
}

template <typename T, std::size_t N, std::size_t S>
auto as_strided_writable_bytes(StridedSpan<T, N, S> s) noexcept {
	if constexpr (N == DYNAMIC_EXTENT) {
		return StridedSpan<std::byte, DYNAMIC_EXTENT, S>{reinterpret_cast<std::byte*>(s.base()), s.size(), s.stride()};
	} else {
		return StridedSpan<std::byte, sizeof(T) * N, S>{reinterpret_cast<std::byte*>(s.base()), s.size(), s.stride()};
	}
}

} // namespace grem

#endif
