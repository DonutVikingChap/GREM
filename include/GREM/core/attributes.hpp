// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_ATTRIBUTES_HPP
#define GREM_CORE_ATTRIBUTES_HPP

#include <GREM/build_config.hpp>

#ifndef GREM_ALWAYS_INLINE
#define GREM_ALWAYS_INLINE inline
#elif defined(__clang__) || defined(__GNUC__)
#define GREM_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define GREM_ALWAYS_INLINE __forceinline
#else
#define GREM_ALWAYS_INLINE inline
#endif

#ifndef GREM_NOINLINE
#if defined(__clang__) || defined(__GNUC__)
#define GREM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define GREM_NOINLINE __declspec(noinline)
#else
#define GREM_NOINLINE
#endif
#endif

#ifndef GREM_FLATTEN
#if defined(__clang__) || defined(__GNUC__)
#define GREM_FLATTEN __attribute__((flatten))
#else
#define GREM_FLATTEN
#endif
#endif

#ifndef GREM_EXPORT
#if defined(_WIN32)
#define GREM_EXPORT __declspec(dllexport)
#elif defined(__clang__) || defined(__GNUC__)
#define GREM_EXPORT __attribute__((visibility("default")))
#else
#define GREM_EXPORT
#endif
#endif

#ifndef GREM_VECTORCALL
#if defined(_MSC_VER) || (defined(__clang__) && !defined(__EMSCRIPTEN__))
#define GREM_VECTORCALL __vectorcall
#else
#define GREM_VECTORCALL
#endif
#endif

#ifndef GREM_THREAD_LOCAL
#ifdef GREM_USE_MULTITHREADING
#define GREM_THREAD_LOCAL thread_local
#else
#define GREM_THREAD_LOCAL static
#endif
#endif

#endif
