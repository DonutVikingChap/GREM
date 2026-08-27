// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_IMGUI_CONFIG_HPP
#define GREM_IMGUI_CONFIG_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <cstddef> // std::size_t

#define IM_ASSERT(expression) GREM_ASSERT(expression)
#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS

namespace grem::imgui {
namespace detail {
struct FileHandle;
} // namespace detail
} // namespace grem::imgui

using ImFileHandle = grem::imgui::detail::FileHandle*;

ImFileHandle ImFileOpen(const char* filename, const char* mode);
bool ImFileClose(ImFileHandle file);
std::size_t ImFileGetSize(ImFileHandle file);
std::size_t ImFileRead(void* data, std::size_t size, std::size_t count, ImFileHandle file);
std::size_t ImFileWrite(const void* data, std::size_t size, std::size_t count, ImFileHandle file);

#endif
