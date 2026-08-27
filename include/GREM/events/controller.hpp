// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_CONTROLLER_HPP
#define GREM_EVENTS_CONTROLLER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

namespace grem::events {

/**
 * Common controller type categories.
 *
 * Useful for deciding which button prompts to show, or as a hint for which
 * inputs are available, etc.
 */
enum class ControllerType : uint8_t {
	UNKNOWN,             ///< Unknown controller type.
	STANDARD,            ///< Standard controller type.
	XBOX_360,            ///< Microsoft Xbox 360-like controller.
	XBOX_ONE,            ///< Microsoft Xbox One-like controller.
	PS3,                 ///< Sony Playstation 3-like controller.
	PS4,                 ///< Sony Playstation 4-like controller.
	PS5,                 ///< Sony Playstation 5-like controller.
	SWITCH_PRO,          ///< Nintendo Switch Pro-like controller.
	SWITCH_JOYCON_LEFT,  ///< Nintendo Switch Left Joycon-like controller.
	SWITCH_JOYCON_RIGHT, ///< Nintendo Switch Right Joycon-like controller.
	SWITCH_JOYCON_PAIR,  ///< Nintendo Switch Joycon Pair-like controller.
	GAMECUBE,            ///< Nintendo Gamecube-like controller.
};

/**
 * Physical controller button identifier.
 */
enum class ControllerButton : uint8_t {
	SOUTH,          ///< South face button (A/Cross).
	EAST,           ///< East face button (B/Circle).
	WEST,           ///< West face button (X/Square).
	NORTH,          ///< North face button (Y/Triangle).
	SELECT,         ///< Select button (Select/View/Share).
	GUIDE,          ///< Guide button.
	START,          ///< Start button (Start/Menu/Options).
	LEFT_STICK,     ///< Left analog stick button (L Stick/L3).
	RIGHT_STICK,    ///< Right analog stick button (R Stick/R3).
	LEFT_SHOULDER,  ///< Left shoulder button (L Bumper/L1).
	RIGHT_SHOULDER, ///< Right shoulder button (R Bumper/R1).
	DPAD_UP,        ///< D-pad up.
	DPAD_DOWN,      ///< D-pad down.
	DPAD_LEFT,      ///< D-pad left.
	DPAD_RIGHT,     ///< D-pad right.
	MISC1,          ///< Misc1 button.
	LEFT_PADDLE1,   ///< Left paddle 1.
	RIGHT_PADDLE1,  ///< Right paddle 1.
	LEFT_PADDLE2,   ///< Left paddle 2.
	RIGHT_PADDLE2,  ///< Right paddle 2.
	TOUCHPAD,       ///< Touchpad button.
};

/**
 * Physical controller axis identifier.
 */
enum class ControllerAxis : uint8_t {
	LEFT_STICK_X = 0,  ///< Horizontal movement of the left analog stick.
	LEFT_STICK_Y = 1,  ///< Vertical movement of the left analog stick.
	RIGHT_STICK_X = 2, ///< Horizontal movement of the right analog stick.
	RIGHT_STICK_Y = 3, ///< Vertical movement of the right analog stick.
	LEFT_TRIGGER = 4,  ///< Movement of the left trigger.
	RIGHT_TRIGGER = 5, ///< Movement of the right trigger.
	INVALID = 255,     ///< Invalid axis.
};

} // namespace grem::events

#endif
