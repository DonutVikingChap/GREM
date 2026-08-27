// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SUBRANGE_HPP
#define GREM_CORE_DATA_SUBRANGE_HPP

#include <GREM/build_config.hpp>

#include <cstdint>  // std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <iterator> // std::input_or_output_iterator, std::contiguous_iterator, std::sentinel_for, std::sized_sentinel_for, std::begin, std::end, std::size, std::advance, std::prev, std::iterator_traits, std::..._iterator_tag
#include <memory>   // std::to_address
#include <type_traits> // std::is_..._v, std::remove_..._t, std::make_unsigned_t, std::decay_t, std::contitional_t
#include <utility>     // std::move, std::declval

namespace grem {

enum class SubrangeKind : bool {
	UNSIZED,
	SIZED,
};

namespace detail {

template <typename R>
concept borrowed_range = requires(R r) {
	std::begin(r);
	std::end(r);
};

template <typename R>
concept sized_range = requires(R r) { std::size(r); };

template <typename T>
concept default_initializable = std::is_nothrow_destructible_v<T> && std::is_constructible_v<T> && requires {
	T{};
	::new T;
};

template <typename From, typename To>
concept convertible_to = std::is_convertible_v<From, To> && requires { static_cast<To>(std::declval<From>()); };

template <typename T, typename U>
concept different_from = !std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

template <typename T>
concept copyable = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

template <typename From, typename To>
concept uses_nonqualification_pointer_conversion =
	std::is_pointer_v<From> && std::is_pointer_v<To> && !convertible_to<std::remove_pointer_t<From> (*)[], std::remove_pointer_t<To> (*)[]>;

template <typename From, typename To>
concept convertible_to_non_slicing = convertible_to<From, To> && !uses_nonqualification_pointer_conversion<std::decay_t<From>, std::decay_t<To>>;

template <typename T>
using iter_difference_t = typename std::iterator_traits<std::remove_cvref_t<T>>::difference_type;

// clang-format off
template <typename T>
using make_unsigned_like_t =
	std::conditional_t<std::is_integral_v<T>, std::make_unsigned_t<T>,
	std::conditional_t<sizeof(T) == sizeof(std::uint8_t), std::uint8_t,
	std::conditional_t<sizeof(T) == sizeof(std::uint16_t), std::uint16_t,
	std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t,
	std::uint64_t>>>>;
// clang-format on

template <typename I, typename S, SubrangeKind K>
struct SubrangeBase {
	static constexpr bool STORE_SIZE = false;

	I i{};
	S s{};
};

template <typename I, typename S, SubrangeKind K>
requires(K == SubrangeKind::SIZED && !std::sized_sentinel_for<S, I>) struct SubrangeBase<I, S, K> {
	static constexpr bool STORE_SIZE = true;

	I i{};
	S s{};
	make_unsigned_like_t<iter_difference_t<I>> n = 0;
};

} // namespace detail

template <typename I, typename S = I, SubrangeKind K = (std::sized_sentinel_for<S, I>) ? SubrangeKind::SIZED : SubrangeKind::UNSIZED>
class Subrange : private detail::SubrangeBase<I, S, K> {
private:
	using detail::SubrangeBase<I, S, K>::STORE_SIZE;
	using detail::SubrangeBase<I, S, K>::i;
	using detail::SubrangeBase<I, S, K>::s;

public:
	static_assert(std::input_or_output_iterator<I>);
	static_assert(std::sentinel_for<S, I>);
	static_assert(K == SubrangeKind::SIZED || !std::sized_sentinel_for<S, I>);

	Subrange() requires(detail::default_initializable<I>) = default;

	constexpr Subrange(detail::convertible_to_non_slicing<I> auto i, S s) requires(!STORE_SIZE)
		: detail::SubrangeBase<I, S, K>{.i = std::move(i), .s = s} {}

	constexpr Subrange(detail::convertible_to_non_slicing<I> auto i, S s, detail::make_unsigned_like_t<detail::iter_difference_t<I>> n) requires(K == SubrangeKind::SIZED)
		: detail::SubrangeBase<I, S, K>{.i = std::move(i), .s = s, .n = n} {}

	template <detail::different_from<Subrange> R>
	requires(detail::borrowed_range<R> && detail::convertible_to_non_slicing<decltype(std::begin(std::declval<R&>())), I> &&
			 detail::convertible_to<decltype(std::end(std::declval<R&>())), S> && (!STORE_SIZE || detail::sized_range<R>))
	constexpr Subrange(R&& r) // NOLINT(cppcoreguidelines-missing-std-forward)
		: detail::SubrangeBase<I, S, K>{
			  .i = std::move(std::begin(r)),
			  .s = std::end(r),
		  } {
		if constexpr (STORE_SIZE) {
			detail::SubrangeBase<I, S, K>::n = static_cast<decltype(detail::SubrangeBase<I, S, K>::n)>(std::size(r));
		}
	}

	template <typename R>
	requires(detail::borrowed_range<R> && detail::convertible_to_non_slicing<decltype(std::begin(std::declval<R&>())), I> &&
			 detail::convertible_to<decltype(std::end(std::declval<R&>())), S> && K == SubrangeKind::SIZED)
	constexpr Subrange(R&& r, detail::make_unsigned_like_t<detail::iter_difference_t<I>> n) // NOLINT(cppcoreguidelines-missing-std-forward)
		: detail::SubrangeBase<I, S, K>{.i = std::move(std::begin(r)), .s = std::end(r), .n = n} {}

	[[nodiscard]] constexpr I begin() const requires(detail::copyable<I>) {
		return i;
	}

	[[nodiscard]] constexpr I cbegin() const requires(detail::copyable<I>) {
		return i;
	}

	[[nodiscard]] constexpr I begin() requires(!detail::copyable<I>) {
		return std::move(i);
	}

	[[nodiscard]] constexpr S end() const {
		return s;
	}

	[[nodiscard]] constexpr S cend() const {
		return s;
	}

	[[nodiscard]] constexpr bool empty() const {
		return i == s;
	}

	[[nodiscard]] constexpr detail::make_unsigned_like_t<detail::iter_difference_t<I>> size() const requires(K == SubrangeKind::SIZED) {
		if constexpr (STORE_SIZE) {
			return detail::SubrangeBase<I, S, K>::n;
		} else {
			return static_cast<detail::make_unsigned_like_t<detail::iter_difference_t<I>>>(s - i);
		}
	}

	constexpr Subrange& advance(detail::iter_difference_t<I> n) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::bidirectional_iterator_tag>) {
			if (n < 0) {
				std::advance(i, n);
				if constexpr (STORE_SIZE) {
					detail::SubrangeBase<I, S, K>::n += static_cast<detail::make_unsigned_like_t<detail::iter_difference_t<I>>>(-n);
				}
				return *this;
			}
		}
		if constexpr (std::sized_sentinel_for<S, I>) {
			constexpr auto abs = [](auto x) {
				return (x < 0) ? -x : x;
			};
			const auto dist = abs(n) - abs(s - i);
			if (dist < 0) {
				std::advance(i, s - i);
				if constexpr (STORE_SIZE) {
					detail::SubrangeBase<I, S, K>::n -= n + dist;
				}
			} else {
				std::advance(i, n);
				if constexpr (STORE_SIZE) {
					detail::SubrangeBase<I, S, K>::n -= n;
				}
			}
		} else {
			while (n > 0 && i != s) {
				--n;
				++i;
				if constexpr (STORE_SIZE) {
					--detail::SubrangeBase<I, S, K>::n;
				}
			}
			if constexpr (std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::bidirectional_iterator_tag>) {
				while (n < 0 && i != s) {
					++n;
					--i;
					if constexpr (STORE_SIZE) {
						++detail::SubrangeBase<I, S, K>::n;
					}
				}
			}
		}
		return *this;
	}

	constexpr Subrange prev(detail::iter_difference_t<I> n = 1) const
		requires(std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::bidirectional_iterator_tag>) {
		Subrange temporary = *this;
		temporary.advance(-n);
		return temporary;
	}

	constexpr Subrange next(detail::iter_difference_t<I> n = 1) const& requires(
		std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::forward_iterator_tag>) {
		Subrange temporary = *this;
		temporary.advance(n);
		return temporary;
	}

	constexpr Subrange next(detail::iter_difference_t<I> n = 1) && {
		advance(n);
		return std::move(*this);
	}

	constexpr auto data() const requires(std::contiguous_iterator<I>) {
		return std::to_address(i);
	}

	constexpr decltype(auto) front() const requires(std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::forward_iterator_tag>) {
		return *begin();
	}

	constexpr decltype(auto) back() const requires(std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::bidirectional_iterator_tag>) {
		return *std::prev(end());
	}

	constexpr decltype(auto) operator[](detail::iter_difference_t<I> n) const
		requires(std::is_convertible_v<typename std::iterator_traits<I>::iterator_category, std::bidirectional_iterator_tag>) {
		return begin()[n];
	}
};

template <std::input_or_output_iterator I, std::sentinel_for<I> S>
Subrange(I, S) -> Subrange<I, S>;

template <std::input_or_output_iterator I, std::sentinel_for<I> S>
Subrange(I, S, detail::make_unsigned_like_t<detail::iter_difference_t<I>>) -> Subrange<I, S, SubrangeKind::SIZED>;

template <detail::borrowed_range R>
Subrange(R&&) -> Subrange<decltype(std::begin(std::declval<R&>())), decltype(std::end(std::declval<R&>())),
	(detail::sized_range<R> || std::sized_sentinel_for<decltype(std::end(std::declval<R&>())), decltype(std::begin(std::declval<R&>()))>) ? SubrangeKind::SIZED
																																		  : SubrangeKind::UNSIZED>;

template <detail::borrowed_range R>
Subrange(R&&, detail::iter_difference_t<decltype(std::begin(std::declval<R&>()))>)
	-> Subrange<decltype(std::begin(std::declval<R&>())), decltype(std::end(std::declval<R&>())), SubrangeKind::SIZED>;

} // namespace grem

#endif
