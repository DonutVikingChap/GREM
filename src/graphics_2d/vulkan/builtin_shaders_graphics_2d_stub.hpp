// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_VULKAN_BUILTIN_SHADERS_GRAPHICS_2D_STUB_HPP
#define GREM_GRAPHICS_2D_VULKAN_BUILTIN_SHADERS_GRAPHICS_2D_STUB_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

namespace grem::graphics {

namespace detail {

inline constexpr uint32_t RENDERER_2D_DEFAULT_MODEL_2D_VERTEX_SHADER_CODE[1]{};
inline constexpr uint32_t RENDERER_2D_PLAIN_MODEL_2D_FRAGMENT_SHADER_CODE[1]{};
inline constexpr uint32_t RENDERER_2D_TEXT_MODEL_2D_FRAGMENT_SHADER_CODE[1]{};
inline constexpr uint32_t RENDERER_2D_TONEMAPPING_MODEL_2D_FRAGMENT_SHADER_CODE[1]{};

} // namespace detail

} // namespace grem::graphics

#endif
