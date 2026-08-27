// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_BUILD_CONFIG_HPP
#define GREM_BUILD_CONFIG_HPP

// IWYU pragma: always_keep

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#ifndef GREM_API
#ifdef GREM_SHARED_LIBRARY
#define GREM_API(moduleName) GREM_PRIVATE_API_##moduleName
#else
#define GREM_API(moduleName)
#endif
#endif

#endif
