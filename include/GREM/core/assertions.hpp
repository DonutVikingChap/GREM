// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_ASSERTIONS_HPP
#define GREM_CORE_ASSERTIONS_HPP

//==============================================================================
// Includes
//==============================================================================

#include <GREM/build_config.hpp>

#if (!defined(GREM_ASSERT) || !defined(GREM_UNREACHABLE))
#ifdef GREM_USE_RELEASE_ASSERTIONS
#ifdef GREM_RELEASE_ASSERTIONS_FAST_FAIL
#if !defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)

#include <cstdlib> // IWYU pragma: keep // std::abort

#endif
#else

#include <cstdio>          // IWYU pragma: keep // stderr, std::fprintf
#include <cstdlib>         // IWYU pragma: keep // std::abort
#include <source_location> // IWYU pragma: keep // std::source_location

#endif
#elif !defined(NDEBUG) || !defined(GREM_ASSUME_UNCHECKED_ASSERTIONS_NEVER_FAIL)
#if !defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)

#include <cstdlib> // IWYU pragma: keep // std::abort

#endif
#endif
#endif

//==============================================================================
// GREM_ASSERT
//==============================================================================

#ifndef GREM_ASSERT
#ifdef GREM_USE_RELEASE_ASSERTIONS
#ifdef GREM_RELEASE_ASSERTIONS_FAST_FAIL
#ifdef _MSC_VER
#define GREM_ASSERT(expression) ((static_cast<bool>(expression)) ? (void)0 : __debugbreak())
#elif defined(__clang__) || defined(__GNUC__)
#define GREM_ASSERT(expression) ((static_cast<bool>(expression)) ? (void)0 : __builtin_trap())
#else
#define GREM_ASSERT(expression) ((static_cast<bool>(expression)) ? (void)0 : std::abort())
#endif
#else
namespace grem {
namespace detail {
[[noreturn]] inline void assertionViolation(const char* expressionString, const std::source_location& source) {
	std::fprintf(stderr, "Assertion failed: %s\n  Source: %s:%d:%d\n  Function: %s\n", expressionString, source.file_name(), static_cast<int>(source.line()),
		static_cast<int>(source.column()), source.function_name());
	std::abort();
}
} // namespace detail
} // namespace grem
#define GREM_ASSERT(expression) ((static_cast<bool>(expression)) ? (void)0 : grem::detail::assertionViolation(#expression, std::source_location::current()))
#endif
#elif defined(NDEBUG)
#ifdef GREM_ASSUME_UNCHECKED_ASSERTIONS_NEVER_FAIL
#if defined(__clang__)
#define GREM_ASSERT(expression) __builtin_assume(expression)
#elif defined(_MSC_VER)
#define GREM_ASSERT(expression) __assume(expression)
#else
#define GREM_ASSERT(expression) ((void)0)
#endif
#else
#define GREM_ASSERT(expression) ((void)0)
#endif
#else
#include <cassert> // IWYU pragma: keep // assert
#define GREM_ASSERT(expression) assert(expression)
#endif
#endif

//==============================================================================
// grem::unreachable()
//==============================================================================

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-noreturn"
#endif

#if !defined(GREM_UNREACHABLE) && defined(GREM_USE_RELEASE_ASSERTIONS) && !defined(GREM_RELEASE_ASSERTIONS_FAST_FAIL)
namespace grem {
namespace detail {
[[noreturn]] inline void unreachableViolation(const std::source_location& source) {
	std::fprintf(stderr, "Unreachable violation.\n  Source: %s:%d:%d\n  Function: %s\n", source.file_name(), static_cast<int>(source.line()), static_cast<int>(source.column()),
		source.function_name());
	std::abort();
}
} // namespace detail
} // namespace grem
#endif

namespace grem {

#ifdef GREM_UNREACHABLE
[[noreturn]] inline void unreachable() {
	GREM_UNREACHABLE;
}
#elif defined(GREM_USE_RELEASE_ASSERTIONS)
#ifdef GREM_RELEASE_ASSERTIONS_FAST_FAIL
[[noreturn]] inline void unreachable() {
#ifdef _MSC_VER
	__debugbreak();
#elif defined(__clang__) || defined(__GNUC__)
	__builtin_trap();
#else
	std::abort();
#endif
}
#else
[[noreturn]] inline void unreachable(const std::source_location& source = std::source_location::current()) {
	detail::unreachableViolation(source);
}
#endif
#else
[[noreturn]] inline void unreachable() {
#if defined(NDEBUG) && defined(GREM_ASSUME_UNCHECKED_ASSERTIONS_NEVER_FAIL)
#if defined(__clang__) || defined(__GNUC__)
	__builtin_unreachable();
#elif defined(_MSC_VER)
	__assume(false);
#endif
#else
#ifdef _MSC_VER
	__debugbreak();
#elif defined(__clang__) || defined(__GNUC__)
	__builtin_trap();
#else
	std::abort();
#endif
#endif
}
#endif

} // namespace grem

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif
