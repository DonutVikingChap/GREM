// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_ALIASES_HPP
#define GREM_ALIASES_HPP

#include <GREM/build_config.hpp>

namespace grem {
namespace ascii {}
namespace base16 {}
namespace base64 {}
namespace cli {}
namespace deflate {}
namespace json {}
namespace meta {}
namespace numbers {}
namespace obj {}
namespace randomness {}
namespace time_literals {};
namespace unicode {}
namespace xml {}
} // namespace grem
using namespace grem::time_literals;

#ifdef GREM_ENABLE_APPLICATION_MODULE
namespace grem::application {}
namespace app = grem::application; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_AUDIO_MODULE
namespace grem::audio {}
namespace aud = grem::audio; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_EVENTS_MODULE
namespace grem::events {}
namespace evt = grem::events; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_EXECUTION_MODULE
namespace grem::execution {}
namespace exec = grem::execution; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_GRAPHICS_MODULE
namespace grem::graphics {}
namespace gfx = grem::graphics; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_IMGUI_MODULE
namespace grem::imgui {}
namespace imgui = grem::imgui; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_NETWORKING_MODULE
namespace grem::networking {}
namespace net = grem::networking; // NOLINT(misc-unused-alias-decls)
#endif

#ifdef GREM_ENABLE_PHYSICS_MODULE
namespace grem::physics {
namespace literals {}
} // namespace grem::physics
namespace grem {
namespace physics_literals = grem::physics::literals; // NOLINT(misc-unused-alias-decls)
} // namespace grem
namespace phys = grem::physics; // NOLINT(misc-unused-alias-decls)
using namespace grem::physics_literals;
#endif

#ifdef GREM_ENABLE_RESOURCE_MODULE
namespace grem::resource {}
namespace res = grem::resource; // NOLINT(misc-unused-alias-decls)
#endif

using namespace grem;

namespace ascii = grem::ascii;     // NOLINT(misc-unused-alias-decls)
namespace base16 = grem::base16;   // NOLINT(misc-unused-alias-decls)
namespace base64 = grem::base64;   // NOLINT(misc-unused-alias-decls)
namespace cli = grem::cli;         // NOLINT(misc-unused-alias-decls)
namespace deflate = grem::deflate; // NOLINT(misc-unused-alias-decls)
namespace json = grem::json;       // NOLINT(misc-unused-alias-decls)
namespace meta = grem::meta;       // NOLINT(misc-unused-alias-decls)
namespace numbers = grem::numbers; // NOLINT(misc-unused-alias-decls)
namespace obj = grem::obj;         // NOLINT(misc-unused-alias-decls)
namespace rng = grem::randomness;  // NOLINT(misc-unused-alias-decls)
namespace unicode = grem::unicode; // NOLINT(misc-unused-alias-decls)
namespace xml = grem::xml;         // NOLINT(misc-unused-alias-decls)

#endif
