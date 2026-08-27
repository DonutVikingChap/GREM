// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/Input.hpp>

#include <SDL3/SDL.h> // SDL...

namespace grem::events {

constexpr Array<Input, SDL_SCANCODE_COUNT> SCANCODE_INPUT_MAP = [] {
	Array<Input, SDL_SCANCODE_COUNT> result{};
	result[SDL_SCANCODE_A] = Input::KEY_A;
	result[SDL_SCANCODE_B] = Input::KEY_B;
	result[SDL_SCANCODE_C] = Input::KEY_C;
	result[SDL_SCANCODE_D] = Input::KEY_D;
	result[SDL_SCANCODE_E] = Input::KEY_E;
	result[SDL_SCANCODE_F] = Input::KEY_F;
	result[SDL_SCANCODE_G] = Input::KEY_G;
	result[SDL_SCANCODE_H] = Input::KEY_H;
	result[SDL_SCANCODE_I] = Input::KEY_I;
	result[SDL_SCANCODE_J] = Input::KEY_J;
	result[SDL_SCANCODE_K] = Input::KEY_K;
	result[SDL_SCANCODE_L] = Input::KEY_L;
	result[SDL_SCANCODE_M] = Input::KEY_M;
	result[SDL_SCANCODE_N] = Input::KEY_N;
	result[SDL_SCANCODE_O] = Input::KEY_O;
	result[SDL_SCANCODE_P] = Input::KEY_P;
	result[SDL_SCANCODE_Q] = Input::KEY_Q;
	result[SDL_SCANCODE_R] = Input::KEY_R;
	result[SDL_SCANCODE_S] = Input::KEY_S;
	result[SDL_SCANCODE_T] = Input::KEY_T;
	result[SDL_SCANCODE_U] = Input::KEY_U;
	result[SDL_SCANCODE_V] = Input::KEY_V;
	result[SDL_SCANCODE_W] = Input::KEY_W;
	result[SDL_SCANCODE_X] = Input::KEY_X;
	result[SDL_SCANCODE_Y] = Input::KEY_Y;
	result[SDL_SCANCODE_Z] = Input::KEY_Z;
	result[SDL_SCANCODE_1] = Input::KEY_1;
	result[SDL_SCANCODE_2] = Input::KEY_2;
	result[SDL_SCANCODE_3] = Input::KEY_3;
	result[SDL_SCANCODE_4] = Input::KEY_4;
	result[SDL_SCANCODE_5] = Input::KEY_5;
	result[SDL_SCANCODE_6] = Input::KEY_6;
	result[SDL_SCANCODE_7] = Input::KEY_7;
	result[SDL_SCANCODE_8] = Input::KEY_8;
	result[SDL_SCANCODE_9] = Input::KEY_9;
	result[SDL_SCANCODE_0] = Input::KEY_0;
	result[SDL_SCANCODE_ESCAPE] = Input::KEY_ESCAPE;
	result[SDL_SCANCODE_LCTRL] = Input::KEY_LEFT_CONTROL;
	result[SDL_SCANCODE_RCTRL] = Input::KEY_RIGHT_CONTROL;
	result[SDL_SCANCODE_LSHIFT] = Input::KEY_LEFT_SHIFT;
	result[SDL_SCANCODE_RSHIFT] = Input::KEY_RIGHT_SHIFT;
	result[SDL_SCANCODE_LALT] = Input::KEY_LEFT_ALT;
	result[SDL_SCANCODE_RALT] = Input::KEY_RIGHT_ALT;
	result[SDL_SCANCODE_MENU] = Input::KEY_MENU;
	result[SDL_SCANCODE_LEFTBRACKET] = Input::KEY_LEFT_BRACKET;
	result[SDL_SCANCODE_RIGHTBRACKET] = Input::KEY_RIGHT_BRACKET;
	result[SDL_SCANCODE_SEMICOLON] = Input::KEY_SEMICOLON;
	result[SDL_SCANCODE_COMMA] = Input::KEY_COMMA;
	result[SDL_SCANCODE_PERIOD] = Input::KEY_PERIOD;
	result[SDL_SCANCODE_APOSTROPHE] = Input::KEY_APOSTROPHE;
	result[SDL_SCANCODE_SLASH] = Input::KEY_SLASH;
	result[SDL_SCANCODE_BACKSLASH] = Input::KEY_BACKSLASH;
	result[SDL_SCANCODE_GRAVE] = Input::KEY_GRAVE_ACCENT;
	result[SDL_SCANCODE_NONUSBACKSLASH] = Input::KEY_NON_US_BACKSLASH;
	result[SDL_SCANCODE_EQUALS] = Input::KEY_EQUALS;
	result[SDL_SCANCODE_MINUS] = Input::KEY_MINUS;
	result[SDL_SCANCODE_SPACE] = Input::KEY_SPACE;
	result[SDL_SCANCODE_RETURN] = Input::KEY_RETURN;
	result[SDL_SCANCODE_BACKSPACE] = Input::KEY_BACKSPACE;
	result[SDL_SCANCODE_TAB] = Input::KEY_TAB;
	result[SDL_SCANCODE_PAGEUP] = Input::KEY_PAGE_UP;
	result[SDL_SCANCODE_PAGEDOWN] = Input::KEY_PAGE_DOWN;
	result[SDL_SCANCODE_END] = Input::KEY_END;
	result[SDL_SCANCODE_HOME] = Input::KEY_HOME;
	result[SDL_SCANCODE_INSERT] = Input::KEY_INSERT;
	result[SDL_SCANCODE_DELETE] = Input::KEY_DELETE;
	result[SDL_SCANCODE_UP] = Input::KEY_ARROW_UP;
	result[SDL_SCANCODE_DOWN] = Input::KEY_ARROW_DOWN;
	result[SDL_SCANCODE_LEFT] = Input::KEY_ARROW_LEFT;
	result[SDL_SCANCODE_RIGHT] = Input::KEY_ARROW_RIGHT;
	result[SDL_SCANCODE_KP_PLUS] = Input::KEY_NUMPAD_PLUS;
	result[SDL_SCANCODE_KP_MINUS] = Input::KEY_NUMPAD_MINUS;
	result[SDL_SCANCODE_KP_MULTIPLY] = Input::KEY_NUMPAD_MULTIPLY;
	result[SDL_SCANCODE_KP_DIVIDE] = Input::KEY_NUMPAD_DIVIDE;
	result[SDL_SCANCODE_KP_1] = Input::KEY_NUMPAD_1;
	result[SDL_SCANCODE_KP_2] = Input::KEY_NUMPAD_2;
	result[SDL_SCANCODE_KP_3] = Input::KEY_NUMPAD_3;
	result[SDL_SCANCODE_KP_4] = Input::KEY_NUMPAD_4;
	result[SDL_SCANCODE_KP_5] = Input::KEY_NUMPAD_5;
	result[SDL_SCANCODE_KP_6] = Input::KEY_NUMPAD_6;
	result[SDL_SCANCODE_KP_7] = Input::KEY_NUMPAD_7;
	result[SDL_SCANCODE_KP_8] = Input::KEY_NUMPAD_8;
	result[SDL_SCANCODE_KP_9] = Input::KEY_NUMPAD_9;
	result[SDL_SCANCODE_KP_0] = Input::KEY_NUMPAD_0;
	result[SDL_SCANCODE_F1] = Input::KEY_F1;
	result[SDL_SCANCODE_F2] = Input::KEY_F2;
	result[SDL_SCANCODE_F3] = Input::KEY_F3;
	result[SDL_SCANCODE_F4] = Input::KEY_F4;
	result[SDL_SCANCODE_F5] = Input::KEY_F5;
	result[SDL_SCANCODE_F6] = Input::KEY_F6;
	result[SDL_SCANCODE_F7] = Input::KEY_F7;
	result[SDL_SCANCODE_F8] = Input::KEY_F8;
	result[SDL_SCANCODE_F9] = Input::KEY_F9;
	result[SDL_SCANCODE_F10] = Input::KEY_F10;
	result[SDL_SCANCODE_F11] = Input::KEY_F11;
	result[SDL_SCANCODE_F12] = Input::KEY_F12;
	result[SDL_SCANCODE_F13] = Input::KEY_F13;
	result[SDL_SCANCODE_F14] = Input::KEY_F14;
	result[SDL_SCANCODE_F15] = Input::KEY_F15;
	result[SDL_SCANCODE_PRINTSCREEN] = Input::KEY_PRINT_SCREEN;
	result[SDL_SCANCODE_SCROLLLOCK] = Input::KEY_SCROLL_LOCK;
	result[SDL_SCANCODE_PAUSE] = Input::KEY_PAUSE;
	return result;
}();

constexpr Array<Input, 8> MOUSE_BUTTON_INPUT_MAP = [] {
	Array<Input, 8> result{};
	result[SDL_BUTTON_LEFT] = Input::MOUSE_BUTTON_LEFT;
	result[SDL_BUTTON_RIGHT] = Input::MOUSE_BUTTON_RIGHT;
	result[SDL_BUTTON_MIDDLE] = Input::MOUSE_BUTTON_MIDDLE;
	result[SDL_BUTTON_X1] = Input::MOUSE_BUTTON_BACK;
	result[SDL_BUTTON_X2] = Input::MOUSE_BUTTON_FORWARD;
	return result;
}();

constexpr Array<Input, 21> CONTROLLER_BUTTON_INPUT_MAP = [] {
	Array<Input, 21> result{};
	result[static_cast<size_t>(ControllerButton::SOUTH)] = Input::CONTROLLER_BUTTON_SOUTH;
	result[static_cast<size_t>(ControllerButton::EAST)] = Input::CONTROLLER_BUTTON_EAST;
	result[static_cast<size_t>(ControllerButton::WEST)] = Input::CONTROLLER_BUTTON_WEST;
	result[static_cast<size_t>(ControllerButton::NORTH)] = Input::CONTROLLER_BUTTON_NORTH;
	result[static_cast<size_t>(ControllerButton::SELECT)] = Input::CONTROLLER_BUTTON_SELECT;
	result[static_cast<size_t>(ControllerButton::GUIDE)] = Input::CONTROLLER_BUTTON_GUIDE;
	result[static_cast<size_t>(ControllerButton::START)] = Input::CONTROLLER_BUTTON_START;
	result[static_cast<size_t>(ControllerButton::LEFT_STICK)] = Input::CONTROLLER_BUTTON_LEFT_STICK;
	result[static_cast<size_t>(ControllerButton::RIGHT_STICK)] = Input::CONTROLLER_BUTTON_RIGHT_STICK;
	result[static_cast<size_t>(ControllerButton::LEFT_SHOULDER)] = Input::CONTROLLER_BUTTON_LEFT_SHOULDER;
	result[static_cast<size_t>(ControllerButton::RIGHT_SHOULDER)] = Input::CONTROLLER_BUTTON_RIGHT_SHOULDER;
	result[static_cast<size_t>(ControllerButton::DPAD_UP)] = Input::CONTROLLER_BUTTON_DPAD_UP;
	result[static_cast<size_t>(ControllerButton::DPAD_DOWN)] = Input::CONTROLLER_BUTTON_DPAD_DOWN;
	result[static_cast<size_t>(ControllerButton::DPAD_LEFT)] = Input::CONTROLLER_BUTTON_DPAD_LEFT;
	result[static_cast<size_t>(ControllerButton::DPAD_RIGHT)] = Input::CONTROLLER_BUTTON_DPAD_RIGHT;
	result[static_cast<size_t>(ControllerButton::MISC1)] = Input::CONTROLLER_BUTTON_MISC1;
	result[static_cast<size_t>(ControllerButton::LEFT_PADDLE1)] = Input::CONTROLLER_BUTTON_LEFT_PADDLE1;
	result[static_cast<size_t>(ControllerButton::RIGHT_PADDLE1)] = Input::CONTROLLER_BUTTON_RIGHT_PADDLE1;
	result[static_cast<size_t>(ControllerButton::LEFT_PADDLE2)] = Input::CONTROLLER_BUTTON_LEFT_PADDLE2;
	result[static_cast<size_t>(ControllerButton::RIGHT_PADDLE2)] = Input::CONTROLLER_BUTTON_RIGHT_PADDLE2;
	result[static_cast<size_t>(ControllerButton::TOUCHPAD)] = Input::CONTROLLER_BUTTON_TOUCHPAD;
	return result;
}();

[[nodiscard]] Input KeyEventBase::getInput() const noexcept {
	if (static_cast<uint32_t>(scancode) >= SCANCODE_INPUT_MAP.size()) {
		return Input::UNKNOWN;
	}
	return SCANCODE_INPUT_MAP[static_cast<uint32_t>(scancode)];
}

[[nodiscard]] Input MouseButtonEventBase::getInput() const noexcept {
	if (static_cast<uint8_t>(mouseButton) >= MOUSE_BUTTON_INPUT_MAP.size()) {
		return Input::UNKNOWN;
	}
	return MOUSE_BUTTON_INPUT_MAP[static_cast<uint8_t>(mouseButton)];
}

[[nodiscard]] Input ControllerButtonEventBase::getInput() const noexcept {
	if (static_cast<uint8_t>(controllerButton) >= CONTROLLER_BUTTON_INPUT_MAP.size()) {
		return Input::UNKNOWN;
	}
	return CONTROLLER_BUTTON_INPUT_MAP[static_cast<uint8_t>(controllerButton)];
}

} // namespace grem::events
