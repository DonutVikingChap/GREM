// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_FUNCTION_VIEW_HPP
#define GREM_CORE_DATA_FUNCTION_VIEW_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>

#include <functional>  // std::invoke
#include <memory>      // std::addressof
#include <type_traits> // std::is_invocable_r_v, std::is_same_v, std::remove_cvref_t
#include <utility>     // std::forward

namespace grem {

namespace detail {

template <typename T, typename Ret, typename... Args>
concept invocable_r = std::is_invocable_r_v<Ret, T, Args...>;

template <typename T, typename Ret, typename... Args>
concept function_pointer_convertible = requires(T f) { static_cast<Ret (*)(Args...)>(f); };

} // namespace detail

template <typename Signature>
class FunctionView;

template <typename Ret, typename... Args>
class FunctionView<Ret(Args...)> {
public:
	template <typename FreeFunction>
	GREM_ALWAYS_INLINE constexpr FunctionView(FreeFunction&& function) noexcept // NOLINT(cppcoreguidelines-missing-std-forward)
		requires(!std::is_same_v<std::remove_cvref_t<FreeFunction>, FunctionView> && detail::invocable_r<FreeFunction, Ret, Args...> &&
					detail::function_pointer_convertible<FreeFunction, Ret, Args...>)
		: dispatcher([](Data data, Args... args) -> Ret { return data.function(std::forward<Args>(args)...); })
		, data(static_cast<Ret (*)(Args...)>(function)) {}

	template <typename Functor>
	GREM_ALWAYS_INLINE constexpr FunctionView(Functor&& functor) noexcept // NOLINT(cppcoreguidelines-missing-std-forward)
		requires(!std::is_same_v<std::remove_cvref_t<Functor>, FunctionView> && detail::invocable_r<Functor, Ret, Args...> &&
					!detail::function_pointer_convertible<Functor, Ret, Args...>)
		: dispatcher([](Data data, Args... args) -> Ret { return std::invoke(*static_cast<std::add_pointer_t<Functor>>(data.data), std::forward<Args>(args)...); })
		, data(const_cast<void*>(static_cast<const void*>(std::addressof(functor)))) {}

	constexpr ~FunctionView() = default;
	constexpr FunctionView(const FunctionView&) = default;
	constexpr FunctionView(FunctionView&&) noexcept = default;
	constexpr FunctionView& operator=(const FunctionView&) = default;
	constexpr FunctionView& operator=(FunctionView&&) noexcept = default;

	GREM_ALWAYS_INLINE constexpr decltype(auto) operator()(Args... args) const noexcept(noexcept(dispatcher(data, std::forward<Args>(args)...))) {
		return dispatcher(data, std::forward<Args>(args)...);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const FunctionView& other) const {
		return dispatcher == other.dispatcher;
	}

private:
	union Data {
		constexpr explicit Data(void* data) noexcept
			: data(data) {}

		constexpr explicit Data(Ret (*function)(Args...)) noexcept
			: function(function) {}

		void* data;
		Ret (*function)(Args...);
	};

	Ret (*dispatcher)(Data, Args...);
	Data data;
};

} // namespace grem

#endif
