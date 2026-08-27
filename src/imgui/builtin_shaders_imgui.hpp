// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_IMGUI_BUILTIN_SHADERS_IMGUI_HPP
#define GREM_IMGUI_BUILTIN_SHADERS_IMGUI_HPP

#include <GREM/build_config.hpp>

#ifdef GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN
#include "vulkan/builtin_shaders_imgui_generated.hpp" // IWYU pragma: export
//#include "vulkan/builtin_shaders_imgui_stub.hpp" // IWYU pragma: export
#else
#include "opengl/builtin_shaders_imgui_generated.hpp" // IWYU pragma: export
//#include "opengl/builtin_shaders_imgui_stub.hpp" // IWYU pragma: export
#endif

#endif
