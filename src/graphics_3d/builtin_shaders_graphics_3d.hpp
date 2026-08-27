// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_BUILTIN_SHADERS_GRAPHICS_3D_HPP
#define GREM_GRAPHICS_3D_BUILTIN_SHADERS_GRAPHICS_3D_HPP

#include <GREM/build_config.hpp>

#ifdef GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN
#include "vulkan/builtin_shaders_graphics_3d_generated.hpp" // IWYU pragma: export
//#include "vulkan/builtin_shaders_graphics_3d_stub.hpp" // IWYU pragma: export
#else
#include "opengl/builtin_shaders_graphics_3d_generated.hpp" // IWYU pragma: export
//#include "opengl/builtin_shaders_graphics_3d_stub.hpp" // IWYU pragma: export
#endif

#endif
