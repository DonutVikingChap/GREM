// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_MOUSE_HPP
#define GREM_EVENTS_MOUSE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

namespace grem::events {

/**
 * Physical mouse button identifier.
 *
 * The enumerand values are based on the SDL_BUTTON_ constants from SDL3.
 */
enum class MouseButton : uint8_t {
	LEFT = 1,    ///< Left click.
	MIDDLE = 2,  ///< Middle click.
	RIGHT = 3,   ///< Right click.
	BACK = 4,    ///< Back button.
	FORWARD = 5, ///< Forward button.
};

} // namespace grem::events

#endif
