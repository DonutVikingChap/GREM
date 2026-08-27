// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GREM_HPP
#define GREM_GREM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core.hpp> // IWYU pragma: export

#ifdef GREM_ENABLE_APPLICATION_MODULE
#include <GREM/application.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_AUDIO_MODULE
#include <GREM/audio.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_EVENTS_MODULE
#include <GREM/events.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_EXECUTION_MODULE
#include <GREM/execution.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_GRAPHICS_MODULE
#include <GREM/graphics.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_GRAPHICS_2D_MODULE
#include <GREM/graphics_2d.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_GRAPHICS_3D_MODULE
#include <GREM/graphics_3d.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_IMGUI_MODULE
#include <GREM/imgui.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_NETWORKING_MODULE
#include <GREM/networking.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_PHYSICS_MODULE
#include <GREM/physics.hpp> // IWYU pragma: export
#endif

#ifdef GREM_ENABLE_RESOURCE_MODULE
#include <GREM/resource.hpp> // IWYU pragma: export
#endif

#endif
