// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_ACTIONS_HPP
#define GREM_EXAMPLES_FPS_GAME_ACTIONS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/events/Input.hpp>

// clang-format off
//	X(name,                             identifier,                         defaultInputs...)
#define GREM_EXAMPLES_FPS_ENUM_ACTIONS(X) \
	X(CONFIRM,                          "confirm",                          Input::KEY_RETURN, Input::KEY_SPACE, Input::MOUSE_BUTTON_LEFT, Input::CONTROLLER_BUTTON_SOUTH, Input::CONTROLLER_AXIS_RIGHT_TRIGGER) \
	X(CANCEL,                           "cancel",                           Input::KEY_ESCAPE, Input::MOUSE_BUTTON_RIGHT, Input::CONTROLLER_BUTTON_EAST, Input::CONTROLLER_AXIS_LEFT_TRIGGER) \
	X(NAVIGATE_MENU_UP,                 "navigate_menu_up",                 Input::KEY_ARROW_UP, Input::CONTROLLER_BUTTON_DPAD_UP, Input::CONTROLLER_AXIS_LEFT_STICK_UP) \
	X(NAVIGATE_MENU_DOWN,               "navigate_menu_down",               Input::KEY_ARROW_DOWN, Input::CONTROLLER_BUTTON_DPAD_DOWN, Input::CONTROLLER_AXIS_LEFT_STICK_DOWN) \
	X(NAVIGATE_MENU_LEFT,               "navigate_menu_left",               Input::KEY_ARROW_LEFT, Input::CONTROLLER_BUTTON_DPAD_LEFT, Input::CONTROLLER_AXIS_LEFT_STICK_LEFT) \
	X(NAVIGATE_MENU_RIGHT,              "navigate_menu_right",              Input::KEY_ARROW_RIGHT, Input::CONTROLLER_BUTTON_DPAD_RIGHT, Input::CONTROLLER_AXIS_LEFT_STICK_RIGHT) \
	X(MOVE_FORWARD,                     "move_forward",                     Input::KEY_W, Input::CONTROLLER_AXIS_LEFT_STICK_UP) \
	X(MOVE_BACKWARD,                    "move_backward",                    Input::KEY_S, Input::CONTROLLER_AXIS_LEFT_STICK_DOWN) \
	X(MOVE_LEFT,                        "move_left",                        Input::KEY_A, Input::CONTROLLER_AXIS_LEFT_STICK_LEFT) \
	X(MOVE_RIGHT,                       "move_right",                       Input::KEY_D, Input::CONTROLLER_AXIS_LEFT_STICK_RIGHT) \
	X(TURN_UP,                          "turn_up",                          Input::KEY_ARROW_UP, Input::CONTROLLER_AXIS_RIGHT_STICK_UP) \
	X(TURN_DOWN,                        "turn_down",                        Input::KEY_ARROW_DOWN, Input::CONTROLLER_AXIS_RIGHT_STICK_DOWN) \
	X(TURN_LEFT,                        "turn_left",                        Input::KEY_ARROW_LEFT, Input::CONTROLLER_AXIS_RIGHT_STICK_LEFT) \
	X(TURN_RIGHT,                       "turn_right",                       Input::KEY_ARROW_RIGHT, Input::CONTROLLER_AXIS_RIGHT_STICK_RIGHT) \
	X(AIM_DOWN,                         "aim_down",                         Input::MOUSE_MOTION_DOWN) \
	X(AIM_UP,                           "aim_up",                           Input::MOUSE_MOTION_UP) \
	X(AIM_LEFT,                         "aim_left",                         Input::MOUSE_MOTION_LEFT) \
	X(AIM_RIGHT,                        "aim_right",                        Input::MOUSE_MOTION_RIGHT) \
	X(INTERACT,                         "interact",                         Input::KEY_E, Input::CONTROLLER_BUTTON_WEST) \
	X(CROUCH,                           "crouch",                           Input::KEY_LEFT_CONTROL, Input::CONTROLLER_BUTTON_LEFT_STICK, Input::CONTROLLER_BUTTON_LEFT_SHOULDER, Input::CONTROLLER_BUTTON_LEFT_PADDLE1, Input::CONTROLLER_BUTTON_LEFT_PADDLE2) \
	X(SPRINT,                           "sprint",                           Input::KEY_LEFT_SHIFT, Input::CONTROLLER_BUTTON_RIGHT_STICK, Input::CONTROLLER_BUTTON_RIGHT_SHOULDER, Input::CONTROLLER_BUTTON_RIGHT_PADDLE1, Input::CONTROLLER_BUTTON_RIGHT_PADDLE2) \
	X(JUMP,                             "jump",                             Input::KEY_SPACE, Input::CONTROLLER_BUTTON_SOUTH) \
	X(ATTACK,                           "attack",                           Input::MOUSE_BUTTON_LEFT, Input::CONTROLLER_AXIS_RIGHT_TRIGGER) \
	X(RELOAD,                           "reload",                           Input::KEY_R, Input::CONTROLLER_BUTTON_EAST) \
	X(AIM_DOWN_SIGHTS,                  "aim_down_sights",                  Input::MOUSE_BUTTON_RIGHT, Input::CONTROLLER_AXIS_LEFT_TRIGGER) \
	X(CHANGE_FIRE_MODE_LEFT,            "change_fire_mode_left",            Input::MOUSE_BUTTON_FORWARD, Input::CONTROLLER_BUTTON_DPAD_LEFT) \
	X(CHANGE_FIRE_MODE_RIGHT,           "change_fire_mode_right",           Input::MOUSE_BUTTON_BACK, Input::CONTROLLER_BUTTON_DPAD_RIGHT) \
	X(CYCLE_FIRE_MODE,                  "cycle_fire_mode",                  Input::KEY_X) \
	X(TOGGLE_FLASHLIGHT,                "toggle_flashlight",                Input::KEY_F, Input::CONTROLLER_BUTTON_NORTH) \
	X(SCROLL_UP,                        "scroll_up",                        Input::MOUSE_SCROLL_UP, Input::CONTROLLER_BUTTON_DPAD_UP) \
	X(SCROLL_DOWN,                      "scroll_down",                      Input::MOUSE_SCROLL_DOWN, Input::CONTROLLER_BUTTON_DPAD_DOWN) \
	X(TOGGLE_FLYING,                    "toggle_flying",                    Input::KEY_V) \
	X(TOGGLE_SIMULATION_PAUSED,         "toggle_simulation_paused",         Input::KEY_P) \
	X(SINGLE_STEP_PAUSED_SIMULATION,    "single_step_paused_simulation",    Input::KEY_COMMA) \
	X(SLOWLY_ADVANCE_PAUSED_SIMULATION, "slowly_advance_paused_simulation", Input::KEY_PERIOD) \
	X(SPAWN_CARROT_CAKE,                "spawn_carrot_cake",                Input::KEY_E) \
	X(SPAWN_TELEVISION,                 "spawn_television",                 Input::KEY_H) \
	X(SPAM_CRATES,                      "spam_crates",                      Input::KEY_G) \
	X(REMOVE_PROP,                      "remove_prop",                      Input::KEY_DELETE) \
	X(RAIN_BOXES,                       "rain_boxes",                       Input::KEY_9) \
	X(PLACE_DECAL,                      "place_decal",                      Input::KEY_T) \
	X(OPEN_CHAT,                        "open_chat",                        Input::KEY_Y)
// clang-format on

enum class Action : uint8_t {
#define GREM_EXAMPLES_FPS_X(name, identifier, ...) name,
	GREM_EXAMPLES_FPS_ENUM_ACTIONS(GREM_EXAMPLES_FPS_X)
#undef GREM_EXAMPLES_FPS_X
};

[[nodiscard]] inline Optional<Action> findActionByIdentifier(StringView identifier) {
	static const HashMap<StringView, Action> actionsByIdentifier{
#define GREM_EXAMPLES_FPS_X(name, identifier, ...) {identifier, Action::name},
		GREM_EXAMPLES_FPS_ENUM_ACTIONS(GREM_EXAMPLES_FPS_X)
#undef GREM_EXAMPLES_FPS_X
	};
	if (const auto it = actionsByIdentifier.find(identifier); it != actionsByIdentifier.end()) {
		return it->second;
	}
	return {};
}

[[nodiscard]] inline Span<const Pair<Action, Span<const evt::Input>>> getDefaultActionInputs() {
	using namespace events;

#define GREM_EXAMPLES_FPS_X(name, identifier, ...) static constexpr Array DEFAULT_INPUTS_##name{__VA_ARGS__};
	GREM_EXAMPLES_FPS_ENUM_ACTIONS(GREM_EXAMPLES_FPS_X)
#undef GREM_EXAMPLES_FPS_X

	static constexpr Array DEFAULT_ACTION_INPUTS{
#define GREM_EXAMPLES_FPS_X(name, identifier, ...) Pair{Action::name, Span<const Input>{DEFAULT_INPUTS_##name}},
		GREM_EXAMPLES_FPS_ENUM_ACTIONS(GREM_EXAMPLES_FPS_X)
#undef GREM_EXAMPLES_FPS_X
	};

	return DEFAULT_ACTION_INPUTS;
}

[[nodiscard]] inline StringView getActionIdentifier(Action action) {
	switch (action) {
#define GREM_EXAMPLES_FPS_X(name, identifier, ...) \
	case Action::name: return identifier;
		GREM_EXAMPLES_FPS_ENUM_ACTIONS(GREM_EXAMPLES_FPS_X)
#undef GREM_EXAMPLES_FPS_X
	}
	unreachable();
}

#undef GREM_EXAMPLES_FPS_ENUM_ACTIONS

#endif
