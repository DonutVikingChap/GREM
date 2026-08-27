// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_INPUT_HPP
#define GREM_EVENTS_INPUT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/events/controller.hpp>
#include <GREM/events/keyboard.hpp>
#include <GREM/events/mouse.hpp>

namespace grem::events {

// clang-format off
//  X(name,                                 id,                                     str                     )
#define GREM_EVENTS_ENUM_INPUTS(X) \
	X(UNKNOWN,                              "unknown",                              "Unknown"               ) /**< Unknown input. */ \
	X(KEY_A,                                "key_a",                                "A"                     ) /**< Keyboard A key. */ \
	X(KEY_B,                                "key_b",                                "B"                     ) /**< Keyboard B key. */ \
	X(KEY_C,                                "key_c",                                "C"                     ) /**< Keyboard C key. */ \
	X(KEY_D,                                "key_d",                                "D"                     ) /**< Keyboard D key. */ \
	X(KEY_E,                                "key_e",                                "E"                     ) /**< Keyboard E key. */ \
	X(KEY_F,                                "key_f",                                "F"                     ) /**< Keyboard F key. */ \
	X(KEY_G,                                "key_g",                                "G"                     ) /**< Keyboard G key. */ \
	X(KEY_H,                                "key_h",                                "H"                     ) /**< Keyboard H key. */ \
	X(KEY_I,                                "key_i",                                "I"                     ) /**< Keyboard I key. */ \
	X(KEY_J,                                "key_j",                                "J"                     ) /**< Keyboard J key. */ \
	X(KEY_K,                                "key_k",                                "K"                     ) /**< Keyboard K key. */ \
	X(KEY_L,                                "key_l",                                "L"                     ) /**< Keyboard L key. */ \
	X(KEY_M,                                "key_m",                                "M"                     ) /**< Keyboard M key. */ \
	X(KEY_N,                                "key_n",                                "N"                     ) /**< Keyboard N key. */ \
	X(KEY_O,                                "key_o",                                "O"                     ) /**< Keyboard O key. */ \
	X(KEY_P,                                "key_p",                                "P"                     ) /**< Keyboard P key. */ \
	X(KEY_Q,                                "key_q",                                "Q"                     ) /**< Keyboard Q key. */ \
	X(KEY_R,                                "key_r",                                "R"                     ) /**< Keyboard R key. */ \
	X(KEY_S,                                "key_s",                                "S"                     ) /**< Keyboard S key. */ \
	X(KEY_T,                                "key_t",                                "T"                     ) /**< Keyboard T key. */ \
	X(KEY_U,                                "key_u",                                "U"                     ) /**< Keyboard U key. */ \
	X(KEY_V,                                "key_v",                                "V"                     ) /**< Keyboard V key. */ \
	X(KEY_W,                                "key_w",                                "W"                     ) /**< Keyboard W key. */ \
	X(KEY_X,                                "key_x",                                "X"                     ) /**< Keyboard X key. */ \
	X(KEY_Y,                                "key_y",                                "Y"                     ) /**< Keyboard Y key. */ \
	X(KEY_Z,                                "key_z",                                "Z"                     ) /**< Keyboard Z key. */ \
	X(KEY_1,                                "key_1",                                "1"                     ) /**< Keyboard 1 key. */ \
	X(KEY_2,                                "key_2",                                "2"                     ) /**< Keyboard 2 key. */ \
	X(KEY_3,                                "key_3",                                "3"                     ) /**< Keyboard 3 key. */ \
	X(KEY_4,                                "key_4",                                "4"                     ) /**< Keyboard 4 key. */ \
	X(KEY_5,                                "key_5",                                "5"                     ) /**< Keyboard 5 key. */ \
	X(KEY_6,                                "key_6",                                "6"                     ) /**< Keyboard 6 key. */ \
	X(KEY_7,                                "key_7",                                "7"                     ) /**< Keyboard 7 key. */ \
	X(KEY_8,                                "key_8",                                "8"                     ) /**< Keyboard 8 key. */ \
	X(KEY_9,                                "key_9",                                "9"                     ) /**< Keyboard 9 key. */ \
	X(KEY_0,                                "key_0",                                "0"                     ) /**< Keyboard 0 key. */ \
	X(KEY_ESCAPE,                           "key_escape",                           "Esc"                   ) /**< Keyboard escape key. */ \
	X(KEY_LEFT_CONTROL,                     "key_left_control",                     "LCtrl"                 ) /**< Keyboard left control key. */ \
	X(KEY_RIGHT_CONTROL,                    "key_right_control",                    "RCtrl"                 ) /**< Keyboard right control key. */ \
	X(KEY_LEFT_SHIFT,                       "key_left_shift",                       "LShift"                ) /**< Keyboard left shift key. */ \
	X(KEY_RIGHT_SHIFT,                      "key_right_shift",                      "RShift"                ) /**< Keyboard right shift key. */ \
	X(KEY_LEFT_ALT,                         "key_left_alt",                         "LAlt"                  ) /**< Keyboard left alt key. */ \
	X(KEY_RIGHT_ALT,                        "key_right_alt",                        "RAlt"                  ) /**< Keyboard right alt key. */ \
	X(KEY_MENU,                             "key_menu",                             "Menu"                  ) /**< Keyboard menu key. */ \
	X(KEY_LEFT_BRACKET,                     "key_left_bracket",                      "["                    ) /**< Keyboard [ key. */ \
	X(KEY_RIGHT_BRACKET,                    "key_right_bracket",                     "]"                    ) /**< Keyboard ] key. */ \
	X(KEY_SEMICOLON,                        "key_semicolon",                         ";"                    ) /**< Keyboard ; key. */ \
	X(KEY_COMMA,                            "key_comma",                             ","                    ) /**< Keyboard , key. */ \
	X(KEY_PERIOD,                           "key_period",                            "."                    ) /**< Keyboard \. key. */ \
	X(KEY_APOSTROPHE,                       "key_apostrophe",                        "'"                    ) /**< Keyboard ' key. */ \
	X(KEY_SLASH,                            "key_slash",                             "/"                    ) /**< Keyboard / key. */ \
	X(KEY_BACKSLASH,                        "key_backslash",                         "\\"                   ) /**< Keyboard \\ key. */ \
	X(KEY_GRAVE_ACCENT,                     "key_grave_accent",                      "`"                    ) /**< Keyboard \` key. */ \
	X(KEY_NON_US_BACKSLASH,                 "key_non_us_backslash",                  "<"                    ) /**< Keyboard < key (ISO layout only), AKA "non-US backslash". */ \
	X(KEY_EQUALS,                           "key_equals",                            "="                    ) /**< Keyboard = key. */ \
	X(KEY_MINUS,                            "key_minus",                             "-"                    ) /**< Keyboard \- key. */ \
	X(KEY_SPACE,                            "key_space",                            "Space"                 ) /**< Keyboard space key. */ \
	X(KEY_RETURN,                           "key_return",                           "Return"                ) /**< Keyboard return key. */ \
	X(KEY_BACKSPACE,                        "key_backspace",                        "Backspace"             ) /**< Keyboard backspace key. */ \
	X(KEY_TAB,                              "key_tab",                              "Tab"                   ) /**< Keyboard tab key. */ \
	X(KEY_PAGE_UP,                          "key_page_up",                          "Pgup"                  ) /**< Keyboard page up key. */ \
	X(KEY_PAGE_DOWN,                        "key_page_down",                        "Pgdn"                  ) /**< Keyboard page down key. */ \
	X(KEY_END,                              "key_end",                              "End"                   ) /**< Keyboard end key. */ \
	X(KEY_HOME,                             "key_home",                             "Home"                  ) /**< Keyboard home key. */ \
	X(KEY_INSERT,                           "key_insert",                           "Insert"                ) /**< Keyboard insert key. */ \
	X(KEY_DELETE,                           "key_delete",                           "Delete"                ) /**< Keyboard delete key. */ \
	X(KEY_ARROW_UP,                         "key_arrow_up",                         "Up Arrow"              ) /**< Keyboard up arrow key. */ \
	X(KEY_ARROW_DOWN,                       "key_arrow_down",                       "Down Arrow"            ) /**< Keyboard down arrow key. */ \
	X(KEY_ARROW_LEFT,                       "key_arrow_left",                       "Left Arrow"            ) /**< Keyboard left arrow key. */ \
	X(KEY_ARROW_RIGHT,                      "key_arrow_right",                      "Right Arrow"           ) /**< Keyboard right arrow key. */ \
	X(KEY_NUMPAD_PLUS,                      "key_numpad_plus",                      "Numpad +"              ) /**< Keyboard numpad + key. */ \
	X(KEY_NUMPAD_MINUS,                     "key_numpad_minus",                     "Numpad -"              ) /**< Keyboard numpad - key. */ \
	X(KEY_NUMPAD_MULTIPLY,                  "key_numpad_multiply",                  "Numpad *"              ) /**< Keyboard numpad * key. */ \
	X(KEY_NUMPAD_DIVIDE,                    "key_numpad_divide",                    "Numpad /"              ) /**< Keyboard numpad / key. */ \
	X(KEY_NUMPAD_1,                         "key_numpad_1",                         "Numpad 1"              ) /**< Keyboard numpad 1 key. */ \
	X(KEY_NUMPAD_2,                         "key_numpad_2",                         "Numpad 2"              ) /**< Keyboard numpad 2 key. */ \
	X(KEY_NUMPAD_3,                         "key_numpad_3",                         "Numpad 3"              ) /**< Keyboard numpad 3 key. */ \
	X(KEY_NUMPAD_4,                         "key_numpad_4",                         "Numpad 4"              ) /**< Keyboard numpad 4 key. */ \
	X(KEY_NUMPAD_5,                         "key_numpad_5",                         "Numpad 5"              ) /**< Keyboard numpad 5 key. */ \
	X(KEY_NUMPAD_6,                         "key_numpad_6",                         "Numpad 6"              ) /**< Keyboard numpad 6 key. */ \
	X(KEY_NUMPAD_7,                         "key_numpad_7",                         "Numpad 7"              ) /**< Keyboard numpad 7 key. */ \
	X(KEY_NUMPAD_8,                         "key_numpad_8",                         "Numpad 8"              ) /**< Keyboard numpad 8 key. */ \
	X(KEY_NUMPAD_9,                         "key_numpad_9",                         "Numpad 9"              ) /**< Keyboard numpad 9 key. */ \
	X(KEY_NUMPAD_0,                         "key_numpad_0",                         "Numpad 0"              ) /**< Keyboard numpad 0 key. */ \
	X(KEY_F1,                               "key_f1",                               "F1"                    ) /**< Keyboard F1 key. */ \
	X(KEY_F2,                               "key_f2",                               "F2"                    ) /**< Keyboard F2 key. */ \
	X(KEY_F3,                               "key_f3",                               "F3"                    ) /**< Keyboard F3 key. */ \
	X(KEY_F4,                               "key_f4",                               "F4"                    ) /**< Keyboard F4 key. */ \
	X(KEY_F5,                               "key_f5",                               "F5"                    ) /**< Keyboard F5 key. */ \
	X(KEY_F6,                               "key_f6",                               "F6"                    ) /**< Keyboard F6 key. */ \
	X(KEY_F7,                               "key_f7",                               "F7"                    ) /**< Keyboard F7 key. */ \
	X(KEY_F8,                               "key_f8",                               "F8"                    ) /**< Keyboard F8 key. */ \
	X(KEY_F9,                               "key_f9",                               "F9"                    ) /**< Keyboard F9 key. */ \
	X(KEY_F10,                              "key_f10",                              "F10"                   ) /**< Keyboard F10 key. */ \
	X(KEY_F11,                              "key_f11",                              "F11"                   ) /**< Keyboard F11 key. */ \
	X(KEY_F12,                              "key_f12",                              "F12"                   ) /**< Keyboard F12 key. */ \
	X(KEY_F13,                              "key_f13",                              "F13"                   ) /**< Keyboard F13 key. */ \
	X(KEY_F14,                              "key_f14",                              "F14"                   ) /**< Keyboard F14 key. */ \
	X(KEY_F15,                              "key_f15",                              "F15"                   ) /**< Keyboard F15 key. */ \
	X(KEY_PRINT_SCREEN,                     "key_print_screen",                     "PrtSc"                 ) /**< Keyboard PrtSc key. */ \
	X(KEY_SCROLL_LOCK,                      "key_scroll_lock",                      "Scroll Lock"           ) /**< Keyboard Scroll Lock key. */ \
	X(KEY_PAUSE,                            "key_pause",                            "Pause"                 ) /**< Keyboard Pause key. */ \
	X(MOUSE_BUTTON_LEFT,                    "mouse_button_left",                    "Left Click"            ) /**< Mouse left click. */ \
	X(MOUSE_BUTTON_RIGHT,                   "mouse_button_right",                   "Right Click"           ) /**< Mouse right click. */ \
	X(MOUSE_BUTTON_MIDDLE,                  "mouse_button_middle",                  "Middle Click"          ) /**< Mouse middle click. */ \
	X(MOUSE_BUTTON_BACK,                    "mouse_button_back",                    "Back Mouse Button"     ) /**< Mouse back button. */ \
	X(MOUSE_BUTTON_FORWARD,                 "mouse_button_forward",                 "Forward Mouse Button"  ) /**< Mouse forward button. */ \
	X(MOUSE_SCROLL_UP,                      "mouse_scroll_up",                      "Scroll Up"             ) /**< Mouse scroll wheel upward movement. */ \
	X(MOUSE_SCROLL_DOWN,                    "mouse_scroll_down",                    "Scroll Down"           ) /**< Mouse scroll wheel downward movement. */ \
	X(MOUSE_SCROLL_LEFT,                    "mouse_scroll_left",                    "Scroll Left"           ) /**< Mouse scroll wheel leftward movement. */ \
	X(MOUSE_SCROLL_RIGHT,                   "mouse_scroll_right",                   "Scroll Right"          ) /**< Mouse scroll wheel rightward movement. */ \
	X(MOUSE_MOTION_UP,                      "mouse_motion_up",                      "Mouse Up"              ) /**< Mouse up movement. */ \
	X(MOUSE_MOTION_DOWN,                    "mouse_motion_down",                    "Mouse Down"            ) /**< Mouse down movement. */ \
	X(MOUSE_MOTION_LEFT,                    "mouse_motion_left",                    "Mouse Left"            ) /**< Mouse left movement. */ \
	X(MOUSE_MOTION_RIGHT,                   "mouse_motion_right",                   "Mouse Right"           ) /**< Mouse right movement. */ \
	X(CONTROLLER_BUTTON_SOUTH,              "controller_button_south",              "A Button"              ) /**< Controller south face button (A/Cross). */ \
	X(CONTROLLER_BUTTON_EAST,               "controller_button_east",               "B Button"              ) /**< Controller east face button (B/Circle). */ \
	X(CONTROLLER_BUTTON_WEST,               "controller_button_west",               "X Button"              ) /**< Controller west face button (X/Square). */ \
	X(CONTROLLER_BUTTON_NORTH,              "controller_button_north",              "Y Button"              ) /**< Controller north face button (Y/Triangle). */ \
	X(CONTROLLER_BUTTON_SELECT,             "controller_button_select",             "Select Button"         ) /**< Controller select button (Select/View/Share). */ \
	X(CONTROLLER_BUTTON_GUIDE,              "controller_button_guide",              "Guide Button"          ) /**< Controller guide button. */ \
	X(CONTROLLER_BUTTON_START,              "controller_button_start",              "Start Button"          ) /**< Controller start button (Start/Menu/Options). */ \
	X(CONTROLLER_BUTTON_LEFT_STICK,         "controller_button_left_stick",         "Left Stick Button"     ) /**< Controller left analog stick button (L Stick/L3). */ \
	X(CONTROLLER_BUTTON_RIGHT_STICK,        "controller_button_right_stick",        "Right Stick Button"    ) /**< Controller right analog stick button (R Stick/R3). */ \
	X(CONTROLLER_BUTTON_LEFT_SHOULDER,      "controller_button_left_shoulder",      "Left Shoulder Button"  ) /**< Controller left shoulder button (L Bumper/L1). */ \
	X(CONTROLLER_BUTTON_RIGHT_SHOULDER,     "controller_button_right_shoulder",     "Right Shoulder Button" ) /**< Controller right shoulder button (R Bumper/R1). */ \
	X(CONTROLLER_BUTTON_DPAD_UP,            "controller_button_dpad_up",            "D-Pad Up"              ) /**< Controller D-pad up. */ \
	X(CONTROLLER_BUTTON_DPAD_DOWN,          "controller_button_dpad_down",          "D-Pad Down"            ) /**< Controller D-pad down. */ \
	X(CONTROLLER_BUTTON_DPAD_LEFT,          "controller_button_dpad_left",          "D-Pad Left"            ) /**< Controller D-pad left. */ \
	X(CONTROLLER_BUTTON_DPAD_RIGHT,         "controller_button_dpad_right",         "D-Pad Right"           ) /**< Controller D-pad right. */ \
	X(CONTROLLER_BUTTON_MISC1,              "controller_button_misc1",              "Misc1 Button"          ) /**< Controller misc1 button. */ \
	X(CONTROLLER_BUTTON_LEFT_PADDLE1,       "controller_button_left_paddle1",       "Left Paddle 1"         ) /**< Controller left paddle 1. */ \
	X(CONTROLLER_BUTTON_RIGHT_PADDLE1,      "controller_button_right_paddle1",      "Right Paddle 1"        ) /**< Controller right paddle 1. */ \
	X(CONTROLLER_BUTTON_LEFT_PADDLE2,       "controller_button_left_paddle2",       "Left Paddle 2"         ) /**< Controller left paddle 2. */ \
	X(CONTROLLER_BUTTON_RIGHT_PADDLE2,      "controller_button_right_paddle2",      "Right Paddle 2"        ) /**< Controller right paddle 2. */ \
	X(CONTROLLER_BUTTON_TOUCHPAD,           "controller_button_touchpad",           "Touchpad Button"       ) /**< Controller touchpad button. */ \
	X(CONTROLLER_AXIS_LEFT_STICK_UP,        "controller_axis_left_stick_up",        "Left Stick Up"         ) /**< Controller left analog stick upward movement. */ \
	X(CONTROLLER_AXIS_LEFT_STICK_DOWN,      "controller_axis_left_stick_down",      "Left Stick Down"       ) /**< Controller left analog stick downward movement. */ \
	X(CONTROLLER_AXIS_LEFT_STICK_LEFT,      "controller_axis_left_stick_left",      "Left Stick Left"       ) /**< Controller left analog stick leftward movement. */ \
	X(CONTROLLER_AXIS_LEFT_STICK_RIGHT,     "controller_axis_left_stick_right",     "Left Stick Right"      ) /**< Controller left analog stick rightward movement. */ \
	X(CONTROLLER_AXIS_RIGHT_STICK_UP,       "controller_axis_right_stick_up",       "Right Stick Up"        ) /**< Controller right analog stick upward movement. */ \
	X(CONTROLLER_AXIS_RIGHT_STICK_DOWN,     "controller_axis_right_stick_down",     "Right Stick Down"      ) /**< Controller right analog stick downward movement. */ \
	X(CONTROLLER_AXIS_RIGHT_STICK_LEFT,     "controller_axis_right_stick_left",     "Right Stick Left"      ) /**< Controller right analog stick leftward movement. */ \
	X(CONTROLLER_AXIS_RIGHT_STICK_RIGHT,    "controller_axis_right_stick_right",    "Right Stick Right"     ) /**< Controller right analog stick rightward movement. */ \
	X(CONTROLLER_AXIS_LEFT_TRIGGER,         "controller_axis_left_trigger",         "Left Trigger"          ) /**< Controller left trigger (L Trigger/L2). */ \
	X(CONTROLLER_AXIS_RIGHT_TRIGGER,        "controller_axis_right_trigger",        "Right Trigger"         ) /**< Controller right trigger (R Trigger/R2). */ \
	X(TOUCH_FINGER_TAP,                     "touch_finger_tap",                     "Touch Finger Tap"      ) /**< Finger tap. */ \
	X(TOUCH_FINGER_PRESSURE,                "touch_finger_pressure",                "Touch Finger Pressure" ) /**< Finger pressure. */ \
	X(TOUCH_FINGER_MOTION_UP,               "touch_finger_motion_up",               "Touch Finger Up"       ) /**< Finger up movement. */ \
	X(TOUCH_FINGER_MOTION_DOWN,             "touch_finger_motion_down",             "Touch Finger Down"     ) /**< Finger down movement. */ \
	X(TOUCH_FINGER_MOTION_LEFT,             "touch_finger_motion_left",             "Touch Finger Left"     ) /**< Finger left movement. */ \
	X(TOUCH_FINGER_MOTION_RIGHT,            "touch_finger_motion_right",            "Touch Finger Right"    ) /**< Finger right movement. */
// clang-format on

/**
 * General identifier for a specific control on a physical input device, such as
 * a certain keyboard key, mouse button or joystick axis.
 */
enum class Input : uint8_t {
#define GREM_X_MACRO(name, id, str) name,
	GREM_EVENTS_ENUM_INPUTS(GREM_X_MACRO)
#undef GREM_X_MACRO
};

/**
 * Total number of inputs that exist in the #Input enumeration.
 */
inline constexpr size_t INPUT_COUNT /** \cond */ = [] {
	size_t result = 0;
#define GREM_X_MACRO(name, id, str) ++result;
	GREM_EVENTS_ENUM_INPUTS(GREM_X_MACRO)
#undef GREM_X_MACRO
	return result;
}() /** \endcond */;

/**
 * Get the 0-based index of an #Input.
 *
 * \param input valid input value to get the index of.
 *
 * \return an integer between 0 (inclusive) and #INPUT_COUNT
 *         (exclusive) that uniquely identifies the given input.
 */
[[nodiscard]] constexpr size_t getInputIndex(Input input) noexcept {
	return static_cast<size_t>(static_cast<uint8_t>(input));
}

/**
 * Get the identifier string of an #Input, e.g. "key_k", "mouse_button_left" or
 * "controller_button_right_shoulder".
 *
 * \param input valid input value to get the identifier of.
 *
 * \return an ASCII string that uniquely identifies the given input, starts with
 *         a lowercase letter and only contains lowercase letters, decimal
 *         digits and underscores.
 *
 * \sa findInputByIdentifier()
 */
[[nodiscard]] constexpr StringView getInputIdentifier(Input input) noexcept {
	switch (input) {
		/// \cond
#define GREM_X_MACRO(name, id, str) \
	case Input::name: return id;
		GREM_EVENTS_ENUM_INPUTS(GREM_X_MACRO)
#undef GREM_X_MACRO
		/// \endcond
	}
	return "unknown";
}

/**
 * Get a short human-readable string description of an #Input in English, e.g.
 * "K", "Left Click" or "Right Shoulder Button".
 *
 * \param input valid input value to get the string of.
 *
 * \return an ASCII string that describes the given input in English and may
 *         contain both letter and non-letter characters, such as numbers,
 *         symbols and spaces.
 *
 * \remark The strings from this function may be useful as a baseline for
 *         showing input prompts to the user, but since the strings are always
 *         in English, they should generally be passed through a translation
 *         system before being being shown, if your application has one.
 */
[[nodiscard]] constexpr StringView getInputString(Input input) noexcept {
	switch (input) {
		/// \cond
#define GREM_X_MACRO(name, id, str) \
	case Input::name: return str;
		GREM_EVENTS_ENUM_INPUTS(GREM_X_MACRO)
#undef GREM_X_MACRO
		/// \endcond
	}
	return "Unknown";
}

/**
 * Find the #Input corresponding to a given identifier.
 *
 * \param identifier the identifier string to search for.
 *
 * \return if found, returns the input value whose identifier matches the given
 *         string. Otherwise, returns Input::UNKNOWN.
 *
 * \sa getInputIdentifier()
 */
[[nodiscard]] constexpr Input findInputByIdentifier(StringView identifier) noexcept {
	/// \cond
#define GREM_X_MACRO(name, id, str) \
	if (identifier == id) { \
		return Input::name; \
	}
	GREM_EVENTS_ENUM_INPUTS(GREM_X_MACRO)
#undef GREM_X_MACRO
	/// \endcond
	return Input::UNKNOWN;
}

#undef GREM_EVENTS_ENUM_INPUTS

} // namespace grem::events

#endif
