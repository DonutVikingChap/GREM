// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_KEYBOARD_HPP
#define GREM_EVENTS_KEYBOARD_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

namespace grem::events {

/**
 * Number corresponding to an abstract keyboard key that was translated from its
 * physical location based on the current OS-level key mapping.
 *
 * The enumerand values are based on the SDLK_ constants from SDL3, which in
 * turn are based on Unicode code points (when the key corresponds to a
 * character, which can be checked with unicode::isValidCodePoint()).
 *
 * \note The list of enumerands is not exhaustive, i.e. there are many possible
 *       key codes without an explicit name in this enum.
 *
 * \sa Scancode
 */
enum class KeyCode : uint32_t {
	UNKNOWN = 0x00000000,                    ///< Unknown key.
	RETURN = 0x0000000D,                     ///< `'\r'`
	ESCAPE = 0x0000001B,                     ///< `'\x1B'`
	BACKSPACE = 0x00000008,                  ///< `'\b'`
	TAB = 0x00000009,                        ///< `'\t'`
	SPACE = 0x00000020,                      ///< `' '`
	EXCLAMATION_POINT = 0x00000021,          ///< `'!'`
	DOUBLE_QUOTATION_MARK = 0x00000022,      ///< `'"'`
	HASH = 0x00000023,                       ///< `'#'`
	DOLLAR = 0x00000024,                     ///< `'$'`
	PERCENT = 0x00000025,                    ///< `'%'`
	AMPERSAND = 0x00000026,                  ///< `'&'`
	APOSTROPHE = 0x00000027,                 ///< `'\''`
	LEFT_PARENTHESIS = 0x00000028,           ///< `'('`
	RIGHT_PARENTHESIS = 0x00000029,          ///< `')'`
	ASTERISK = 0x0000002A,                   ///< `'*'`
	PLUS = 0x0000002B,                       ///< `'+'`
	COMMA = 0x0000002C,                      ///< `','`
	MINUS = 0x0000002D,                      ///< `'-'`
	PERIOD = 0x0000002E,                     ///< `'.'`
	SLASH = 0x0000002F,                      ///< `'/'`
	ZERO = 0x00000030,                       ///< `'0'`
	ONE = 0x00000031,                        ///< `'1'`
	TWO = 0x00000032,                        ///< `'2'`
	THREE = 0x00000033,                      ///< `'3'`
	FOUR = 0x00000034,                       ///< `'4'`
	FIVE = 0x00000035,                       ///< `'5'`
	SIX = 0x00000036,                        ///< `'6'`
	SEVEN = 0x00000037,                      ///< `'7'`
	EIGHT = 0x00000038,                      ///< `'8'`
	NINE = 0x00000039,                       ///< `'9'`
	COLON = 0x0000003A,                      ///< `':'`
	SEMICOLON = 0x0000003B,                  ///< `';'`
	LESS_THAN = 0x0000003C,                  ///< `'<'`
	EQUALS = 0x0000003D,                     ///< `'='`
	GREATER_THAN = 0x0000003E,               ///< `'>'`
	QUESTION_MARK = 0x0000003F,              ///< `'?'`
	AT = 0x00000040,                         ///< `'@'`
	LEFT_BRACKET = 0x0000005B,               ///< `'['`
	BACKSLASH = 0x0000005C,                  ///< `'\\'`
	RIGHT_BRACKET = 0x0000005D,              ///< `']'`
	CARET = 0x0000005E,                      ///< `'^'`
	UNDERSCORE = 0x0000005F,                 ///< `'_'`
	GRAVE_ACCENT = 0x00000060,               ///< `'`'`
	A = 0x00000061,                          ///< `'a'`
	B = 0x00000062,                          ///< `'b'`
	C = 0x00000063,                          ///< `'c'`
	D = 0x00000064,                          ///< `'d'`
	E = 0x00000065,                          ///< `'e'`
	F = 0x00000066,                          ///< `'f'`
	G = 0x00000067,                          ///< `'g'`
	H = 0x00000068,                          ///< `'h'`
	I = 0x00000069,                          ///< `'i'`
	J = 0x0000006A,                          ///< `'j'`
	K = 0x0000006B,                          ///< `'k'`
	L = 0x0000006C,                          ///< `'l'`
	M = 0x0000006D,                          ///< `'m'`
	N = 0x0000006E,                          ///< `'n'`
	O = 0x0000006F,                          ///< `'o'`
	P = 0x00000070,                          ///< `'p'`
	Q = 0x00000071,                          ///< `'q'`
	R = 0x00000072,                          ///< `'r'`
	S = 0x00000073,                          ///< `'s'`
	T = 0x00000074,                          ///< `'t'`
	U = 0x00000075,                          ///< `'u'`
	V = 0x00000076,                          ///< `'v'`
	W = 0x00000077,                          ///< `'w'`
	X = 0x00000078,                          ///< `'x'`
	Y = 0x00000079,                          ///< `'y'`
	Z = 0x0000007A,                          ///< `'z'`
	LEFT_BRACE = 0x0000007B,                 ///< `'{'`
	PIPE = 0x0000007C,                       ///< `'|'`
	RIGHT_BRACE = 0x0000007D,                ///< `'}'`
	TILDE = 0x0000007E,                      ///< `'~'`
	DEL = 0x0000007F,                        ///< `'\x7F'`
	PLUS_MINUS = 0x000000B1,                 ///< `'\xB1'`
	CAPS_LOCK = 0x40000039,                  ///< Keyboard Caps Lock
	F1 = 0x4000003A,                         ///< Keyboard F1
	F2 = 0x4000003B,                         ///< Keyboard F2
	F3 = 0x4000003C,                         ///< Keyboard F3
	F4 = 0x4000003D,                         ///< Keyboard F4
	F5 = 0x4000003E,                         ///< Keyboard F5
	F6 = 0x4000003F,                         ///< Keyboard F6
	F7 = 0x40000040,                         ///< Keyboard F7
	F8 = 0x40000041,                         ///< Keyboard F8
	F9 = 0x40000042,                         ///< Keyboard F9
	F10 = 0x40000043,                        ///< Keyboard F10
	F11 = 0x40000044,                        ///< Keyboard F11
	F12 = 0x40000045,                        ///< Keyboard F12
	PRINT_SCREEN = 0x40000046,               ///< Keyboard PrintScreen
	SCROLL_LOCK = 0x40000047,                ///< Keyboard Scroll Lock
	PAUSE = 0x40000048,                      ///< Keyboard Pause
	INSERT = 0x40000049,                     ///< Keyboard Insert
	HOME = 0x4000004A,                       ///< Keyboard Home
	PAGE_UP = 0x4000004B,                    ///< Keyboard PageUp
	END = 0x4000004D,                        ///< Keyboard End
	PAGE_DOWN = 0x4000004E,                  ///< Keyboard PageDown
	RIGHT_ARROW = 0x4000004F,                ///< Keyboard RightArrow
	LEFT_ARROW = 0x40000050,                 ///< Keyboard LeftArrow
	DOWN_ARROW = 0x40000051,                 ///< Keyboard DownArrow
	UP_ARROW = 0x40000052,                   ///< Keyboard UpArrow
	NUM_LOCK = 0x40000053,                   ///< Keyboard Num Lock and Clear
	NUMPAD_DIVIDE = 0x40000054,              ///< Keypad /
	NUMPAD_MULTIPLY = 0x40000055,            ///< Keypad *
	NUMPAD_MINUS = 0x40000056,               ///< Keypad -
	NUMPAD_PLUS = 0x40000057,                ///< Keypad +
	NUMPAD_ENTER = 0x40000058,               ///< Keypad ENTER
	NUMPAD_ONE = 0x40000059,                 ///< Keypad 1 and End
	NUMPAD_TWO = 0x4000005A,                 ///< Keypad 2 and Down Arrow
	NUMPAD_THREE = 0x4000005B,               ///< Keypad 3 and PageDn
	NUMPAD_FOUR = 0x4000005C,                ///< Keypad 4 and Left Arrow
	NUMPAD_FIVE = 0x4000005D,                ///< Keypad 5
	NUMPAD_SIX = 0x4000005E,                 ///< Keypad 6 and Right Arrow
	NUMPAD_SEVEN = 0x4000005F,               ///< Keypad 7 and Home
	NUMPAD_EIGHT = 0x40000060,               ///< Keypad 8 and Up Arrow
	NUMPAD_NINE = 0x40000061,                ///< Keypad 9 and PageUp
	NUMPAD_ZERO = 0x40000062,                ///< Keypad 0 and Insert
	NUMPAD_PERIOD = 0x40000063,              ///< Keypad . and Delete
	APPLICATION = 0x40000065,                ///< Keyboard Application
	POWER = 0x40000066,                      ///< Keyboard Power
	NUMPAD_EQUALS = 0x40000067,              ///< Keypad =
	F13 = 0x40000068,                        ///< Keyboard F13
	F14 = 0x40000069,                        ///< Keyboard F14
	F15 = 0x4000006A,                        ///< Keyboard F15
	F16 = 0x4000006B,                        ///< Keyboard F16
	F17 = 0x4000006C,                        ///< Keyboard F17
	F18 = 0x4000006D,                        ///< Keyboard F18
	F19 = 0x4000006E,                        ///< Keyboard F19
	F20 = 0x4000006F,                        ///< Keyboard F20
	F21 = 0x40000070,                        ///< Keyboard F21
	F22 = 0x40000071,                        ///< Keyboard F22
	F23 = 0x40000072,                        ///< Keyboard F23
	F24 = 0x40000073,                        ///< Keyboard F24
	EXECUTE = 0x40000074,                    ///< Keyboard Execute
	HELP = 0x40000075,                       ///< Keyboard Help
	MENU = 0x40000076,                       ///< Keyboard Menu
	SELECT = 0x40000077,                     ///< Keyboard Select
	STOP = 0x40000078,                       ///< Keyboard Stop
	AGAIN = 0x40000079,                      ///< Keyboard Again
	UNDO = 0x4000007A,                       ///< Keyboard Undo
	CUT = 0x4000007B,                        ///< Keyboard Cut
	COPY = 0x4000007C,                       ///< Keyboard Copy
	PASTE = 0x4000007D,                      ///< Keyboard Paste
	FIND = 0x4000007E,                       ///< Keyboard Find
	MUTE = 0x4000007F,                       ///< Keyboard Mute
	VOLUME_UP = 0x40000080,                  ///< Keyboard Volume Up
	VOLUME_DOWN = 0x40000081,                ///< Keyboard Volume Down
	NUMPAD_COMMA = 0x40000085,               ///< Keypad Comma
	NUMPAD_EQUALS_AS400 = 0x40000086,        ///< Keypad Equal Sign
	ALTERNATE_ERASE = 0x40000099,            ///< Keyboard Alternate Erase (Erase-Eaze)
	SYS_REQ = 0x4000009A,                    ///< Keyboard SysReq/Attention
	CANCEL = 0x4000009B,                     ///< Keyboard Cancel
	CLEAR = 0x4000009C,                      ///< Keyboard Clear
	PRIOR = 0x4000009D,                      ///< Keyboard Prior
	RETURN2 = 0x4000009E,                    ///< Keyboard Return
	SEPARATOR = 0x4000009F,                  ///< Keyboard Separator
	OUT_ = 0x400000A0,                       ///< Keyboard Out
	OPER = 0x400000A1,                       ///< Keyboard Oper
	CLEAR_AGAIN = 0x400000A2,                ///< Keyboard Clear/Again
	CR_SEL = 0x400000A3,                     ///< Keyboard CrSel/Props
	EX_SEL = 0x400000A4,                     ///< Keyboard ExSel
	NUMPAD_00 = 0x400000B0,                  ///< Keypad 00
	NUMPAD_000 = 0x400000B1,                 ///< Keypad 000
	THOUSANDS_SEPARATOR = 0x400000B2,        ///< Thousands Separator
	DECIMAL_SEPARATOR = 0x400000B3,          ///< Decimal Separator
	CURRENCY_UNIT = 0x400000B4,              ///< Currency Unit
	CURRENCY_SUB_UNIT = 0x400000B5,          ///< Currency Sub-unit
	NUMPAD_LEFT_PARENTHESIS = 0x400000B6,    ///< Keypad (
	NUMPAD_RIGHT_PARENTHESIS = 0x400000B7,   ///< Keypad )
	NUMPAD_LEFT_BRACE = 0x400000B8,          ///< Keypad {
	NUMPAD_RIGHT_BRACE = 0x400000B9,         ///< Keypad }
	NUMPAD_TAB = 0x400000BA,                 ///< Keypad Tab
	NUMPAD_BACKSPACE = 0x400000BB,           ///< Keypad Backspace
	NUMPAD_A = 0x400000BC,                   ///< Keypad A
	NUMPAD_B = 0x400000BD,                   ///< Keypad B
	NUMPAD_C = 0x400000BE,                   ///< Keypad C
	NUMPAD_D = 0x400000BF,                   ///< Keypad D
	NUMPAD_E = 0x400000C0,                   ///< Keypad E
	NUMPAD_F = 0x400000C1,                   ///< Keypad F
	NUMPAD_XOR = 0x400000C2,                 ///< Keypad XOR
	NUMPAD_POWER = 0x400000C3,               ///< Keypad ^
	NUMPAD_PERCENT = 0x400000C4,             ///< Keypad %
	NUMPAD_LESS_THAN = 0x400000C5,           ///< Keypad <
	NUMPAD_GREATER_THAN = 0x400000C6,        ///< Keypad >
	NUMPAD_AMPERSAND = 0x400000C7,           ///< Keypad &
	NUMPAD_DOUBLE_AMPERSAND = 0x400000C8,    ///< Keypad &&
	NUMPAD_VERTICALBAR = 0x400000C9,         ///< Keypad |
	NUMPAD_DOUBLE_VERTICAL_BAR = 0x400000CA, ///< Keypad ||
	NUMPAD_COLON = 0x400000CB,               ///< Keypad :
	NUMPAD_HASH = 0x400000CC,                ///< Keypad #
	NUMPAD_SPACE = 0x400000CD,               ///< Keypad Space
	NUMPAD_AT = 0x400000CE,                  ///< Keypad @
	NUMPAD_EXCLAMATION_POINT = 0x400000CF,   ///< Keypad !
	NUMPAD_MEMORY_STORE = 0x400000D0,        ///< Keypad Memory Store
	NUMPAD_MEMORY_RECALL = 0x400000D1,       ///< Keypad Memory Recall
	NUMPAD_MEMORY_CLEAR = 0x400000D2,        ///< Keypad Memory Clear
	NUMPAD_MEMORY_ADD = 0x400000D3,          ///< Keypad Memory Add
	NUMPAD_MEMORY_SUBTRACT = 0x400000D4,     ///< Keypad Memory Subtract
	NUMPAD_MEMORY_MULTIPLY = 0x400000D5,     ///< Keypad Memory Multiply
	NUMPAD_MEMORY_DIVIDE = 0x400000D6,       ///< Keypad Memory Divide
	NUMPAD_PLUS_MINUS = 0x400000D7,          ///< Keypad +/-
	NUMPAD_CLEAR = 0x400000D8,               ///< Keypad Clear
	NUMPAD_CLEAR_ENTRY = 0x400000D9,         ///< Keypad Clear Entry
	NUMPAD_BINARY = 0x400000DA,              ///< Keypad Binary
	NUMPAD_OCTAL = 0x400000DB,               ///< Keypad Octal
	NUMPAD_DECIMAL = 0x400000DC,             ///< Keypad Decimal
	NUMPAD_HEXADECIMAL = 0x400000DD,         ///< Keypad Hexadecimal
	LEFT_CONTROL = 0x400000E0,               ///< Keyboard LeftControl
	LEFT_SHIFT = 0x400000E1,                 ///< Keyboard LeftShift
	LEFT_ALT = 0x400000E2,                   ///< Keyboard LeftAlt (Alt, Option)
	LEFT_GUI = 0x400000E3,                   ///< Keyboard Left GUI (Windows, Command, Meta)
	RIGHT_CONTROL = 0x400000E4,              ///< Keyboard RightControl
	RIGHT_SHIFT = 0x400000E5,                ///< Keyboard RightShift
	RIGHT_ALT = 0x400000E6,                  ///< Keyboard RightAlt (Alt Gr, Option)
	RIGHT_GUI = 0x400000E7,                  ///< Keyboard Right GUI (Windows, Command, Meta)
	MODE = 0x40000101,                       ///< Mode
	SLEEP = 0x40000102,                      ///< Sleep
	WAKE = 0x40000103,                       ///< Wake
	CHANNEL_INCREMENT = 0x40000104,          ///< Channel Increment
	CHANNEL_DECREMENT = 0x40000105,          ///< Channel Decrement
	MEDIA_PLAY = 0x40000106,                 ///< Play
	MEDIA_PAUSE = 0x40000107,                ///< Pause
	MEDIA_RECORD = 0x40000108,               ///< Record
	MEDIA_FAST_FORWARD = 0x40000109,         ///< Fast Forward
	MEDIA_REWIND = 0x4000010A,               ///< Rewind
	MEDIA_NEXT_TRACK = 0x4000010B,           ///< Next Track
	MEDIA_PREVIOUS_TRACK = 0x4000010C,       ///< Previous Track
	MEDIA_STOP = 0x4000010D,                 ///< Stop
	MEDIA_EJECT = 0x4000010E,                ///< Eject
	MEDIA_PLAY_PAUSE = 0x4000010F,           ///< Play/Pause
	MEDIA_SELECT = 0x40000110,               ///< Media Select
	AC_NEW = 0x40000111,                     ///< AC New
	AC_OPEN = 0x40000112,                    ///< AC Open
	AC_CLOSE = 0x40000113,                   ///< AC Close
	AC_EXIT = 0x40000114,                    ///< AC Exit
	AC_SAVE = 0x40000115,                    ///< AC Save
	AC_PRINT = 0x40000116,                   ///< AC Print
	AC_PROPERTIES = 0x40000117,              ///< AC Properties
	AC_SEARCH = 0x40000118,                  ///< AC Search
	AC_HOME = 0x40000119,                    ///< AC Home
	AC_BACK = 0x4000011A,                    ///< AC Back
	AC_FORWARD = 0x4000011B,                 ///< AC Forward
	AC_STOP = 0x4000011C,                    ///< AC Stop
	AC_REFRESH = 0x4000011D,                 ///< AC Refresh
	AC_BOOKMARKS = 0x4000011E,               ///< AC Bookmarks
	SOFT_LEFT = 0x4000011F,                  ///< Bottom left multi-function feature key on phones.
	SOFT_RIGHT = 0x40000120,                 ///< Bottom right multi-function feature key on phones.
	CALL = 0x40000121,                       ///< Used for accepting phone calls.
	END_CALL = 0x40000122,                   ///< Used for rejecting phone calls.
	LEFT_TAB = 0x20000001,                   ///< Extended key Left Tab
	LEVEL5_SHIFT = 0x20000002,               ///< Extended key Level 5 Shift
	MULTI_KEY_COMPOSE = 0x20000003,          ///< Extended key Multi-key Compose
	LEFT_META = 0x20000004,                  ///< Extended key Left Meta
	RIGHT_META = 0x20000005,                 ///< Extended key Left Meta
	LEFT_HYPER = 0x20000006,                 ///< Extended key Left Hyper
	RIGHT_HYPER = 0x20000007,                ///< Extended key Left Hyper
};

/**
 * Number corresponding to a physical keyboard key location, ignoring the
 * OS-level key mapping.
 *
 * The enumerand values are based on the SDL_SCANCODE_ constants from SDL3,
 * which in turn are based on the USB usage page standard:
 * https://usb.org/sites/default/files/hut1_5.pdf
 *
 * \note The list of enumerands is not exhaustive, i.e. there are many possible
 *       scancodes without an explicit name in this enum.
 *
 * \sa KeyCode
 */
enum class Scancode : uint16_t {
	UNKNOWN = 0, ///< Unknown key.

	// From USB Usage Page 0x07 (Keyboard/Keypad Page):
	A = 0x04,                          ///< Keyboard a and A
	B = 0x05,                          ///< Keyboard b and B
	C = 0x06,                          ///< Keyboard c and C
	D = 0x07,                          ///< Keyboard d and D
	E = 0x08,                          ///< Keyboard e and E
	F = 0x09,                          ///< Keyboard f and F
	G = 0x0A,                          ///< Keyboard g and G
	H = 0x0B,                          ///< Keyboard h and H
	I = 0x0C,                          ///< Keyboard i and I
	J = 0x0D,                          ///< Keyboard j and J
	K = 0x0E,                          ///< Keyboard k and K
	L = 0x0F,                          ///< Keyboard l and L
	M = 0x10,                          ///< Keyboard m and M
	N = 0x11,                          ///< Keyboard n and N
	O = 0x12,                          ///< Keyboard o and O
	P = 0x13,                          ///< Keyboard p and P
	Q = 0x14,                          ///< Keyboard q and Q
	R = 0x15,                          ///< Keyboard r and R
	S = 0x16,                          ///< Keyboard s and S
	T = 0x17,                          ///< Keyboard t and T
	U = 0x18,                          ///< Keyboard u and U
	V = 0x19,                          ///< Keyboard v and V
	W = 0x1A,                          ///< Keyboard w and W
	X = 0x1B,                          ///< Keyboard x and X
	Y = 0x1C,                          ///< Keyboard y and Y
	Z = 0x1D,                          ///< Keyboard z and Z
	ONE = 0x1E,                        ///< Keyboard 1 and !
	TWO = 0x1F,                        ///< Keyboard 2 and @
	THREE = 0x20,                      ///< Keyboard 3 and #
	FOUR = 0x21,                       ///< Keyboard 4 and $
	FIVE = 0x22,                       ///< Keyboard 5 and %
	SIX = 0x23,                        ///< Keyboard 6 and ^
	SEVEN = 0x24,                      ///< Keyboard 7 and &
	EIGHT = 0x25,                      ///< Keyboard 8 and *
	NINE = 0x26,                       ///< Keyboard 9 and (
	ZERO = 0x27,                       ///< Keyboard 0 and )
	RETURN = 0x28,                     ///< Keyboard Return (ENTER)
	ESCAPE = 0x29,                     ///< Keyboard ESCAPE
	BACKSPACE = 0x2A,                  ///< Keyboard DELETE (Backspace)
	TAB = 0x2B,                        ///< Keyboard Tab
	SPACE = 0x2C,                      ///< Keyboard Spacebar
	MINUS = 0x2D,                      ///< Keyboard - and (underscore)
	EQUALS = 0x2E,                     ///< Keyboard = and +
	LEFT_BRACKET = 0x2F,               ///< Keyboard [ and {
	RIGHT_BRACKET = 0x30,              ///< Keyboard ] and }
	BACKSLASH = 0x31,                  ///< Keyboard \ and |
	NON_US_HASH = 0x32,                ///< Keyboard Non-US # and ~
	SEMICOLON = 0x33,                  ///< Keyboard ; and :
	APOSTROPHE = 0x34,                 ///< Keyboard ' and "
	GRAVE_ACCENT = 0x35,               ///< Keyboard Grave Accent and Tilde
	COMMA = 0x36,                      ///< Keyboard , and <
	PERIOD = 0x37,                     ///< Keyboard . and >
	SLASH = 0x38,                      ///< Keyboard / and ?
	CAPS_LOCK = 0x39,                  ///< Keyboard Caps Lock
	F1 = 0x3A,                         ///< Keyboard F1
	F2 = 0x3B,                         ///< Keyboard F2
	F3 = 0x3C,                         ///< Keyboard F3
	F4 = 0x3D,                         ///< Keyboard F4
	F5 = 0x3E,                         ///< Keyboard F5
	F6 = 0x3F,                         ///< Keyboard F6
	F7 = 0x40,                         ///< Keyboard F7
	F8 = 0x41,                         ///< Keyboard F8
	F9 = 0x42,                         ///< Keyboard F9
	F10 = 0x43,                        ///< Keyboard F10
	F11 = 0x44,                        ///< Keyboard F11
	F12 = 0x45,                        ///< Keyboard F12
	PRINT_SCREEN = 0x46,               ///< Keyboard PrintScreen
	SCROLL_LOCK = 0x47,                ///< Keyboard Scroll Lock
	PAUSE = 0x48,                      ///< Keyboard Pause
	INSERT = 0x49,                     ///< Keyboard Insert
	HOME = 0x4A,                       ///< Keyboard Home
	PAGE_UP = 0x4B,                    ///< Keyboard PageUp
	DEL = 0x4C,                        ///< Keyboard Delete Forward
	END = 0x4D,                        ///< Keyboard End
	PAGE_DOWN = 0x4E,                  ///< Keyboard PageDown
	RIGHT_ARROW = 0x4F,                ///< Keyboard RightArrow
	LEFT_ARROW = 0x50,                 ///< Keyboard LeftArrow
	DOWN_ARROW = 0x51,                 ///< Keyboard DownArrow
	UP_ARROW = 0x52,                   ///< Keyboard UpArrow
	NUM_LOCK = 0x53,                   ///< Keypad Num Lock and Clear
	NUMPAD_DIVIDE = 0x54,              ///< Keypad /
	NUMPAD_MULTIPLY = 0x55,            ///< Keypad *
	NUMPAD_MINUS = 0x56,               ///< Keypad -
	NUMPAD_PLUS = 0x57,                ///< Keypad +
	NUMPAD_ENTER = 0x58,               ///< Keypad ENTER
	NUMPAD_ONE = 0x59,                 ///< Keypad 1 and End
	NUMPAD_TWO = 0x5A,                 ///< Keypad 2 and Down Arrow
	NUMPAD_THREE = 0x5B,               ///< Keypad 3 and PageDn
	NUMPAD_FOUR = 0x5C,                ///< Keypad 4 and Left Arrow
	NUMPAD_FIVE = 0x5D,                ///< Keypad 5
	NUMPAD_SIX = 0x5E,                 ///< Keypad 6 and Right Arrow
	NUMPAD_SEVEN = 0x5F,               ///< Keypad 7 and Home
	NUMPAD_EIGHT = 0x60,               ///< Keypad 8 and Up Arrow
	NUMPAD_NINE = 0x61,                ///< Keypad 9 and PageUp
	NUMPAD_ZERO = 0x62,                ///< Keypad 0 and Insert
	NUMPAD_PERIOD = 0x63,              ///< Keypad . and Delete
	NON_US_BACKSLASH = 0x64,           ///< Keyboard Non-US \ and |
	APPLICATION = 0x65,                ///< Keyboard Application
	POWER = 0x66,                      ///< Keyboard Power
	NUMPAD_EQUALS = 0x67,              ///< Keypad =
	F13 = 0x68,                        ///< Keyboard F13
	F14 = 0x69,                        ///< Keyboard F14
	F15 = 0x6A,                        ///< Keyboard F15
	F16 = 0x6B,                        ///< Keyboard F16
	F17 = 0x6C,                        ///< Keyboard F17
	F18 = 0x6D,                        ///< Keyboard F18
	F19 = 0x6E,                        ///< Keyboard F19
	F20 = 0x6F,                        ///< Keyboard F20
	F21 = 0x70,                        ///< Keyboard F21
	F22 = 0x71,                        ///< Keyboard F22
	F23 = 0x72,                        ///< Keyboard F23
	F24 = 0x73,                        ///< Keyboard F24
	EXECUTE = 0x74,                    ///< Keyboard Execute
	HELP = 0x75,                       ///< Keyboard Help
	MENU = 0x76,                       ///< Keyboard Menu
	SELECT = 0x77,                     ///< Keyboard Select
	STOP = 0x78,                       ///< Keyboard Stop
	AGAIN = 0x79,                      ///< Keyboard Again
	UNDO = 0x7A,                       ///< Keyboard Undo
	CUT = 0x7B,                        ///< Keyboard Cut
	COPY = 0x7C,                       ///< Keyboard Copy
	PASTE = 0x7D,                      ///< Keyboard Paste
	FIND = 0x7E,                       ///< Keyboard Find
	MUTE = 0x7F,                       ///< Keyboard Mute
	VOLUME_UP = 0x80,                  ///< Keyboard Volume Up
	VOLUME_DOWN = 0x81,                ///< Keyboard Volume Down
	NUMPAD_COMMA = 0x85,               ///< Keypad Comma
	NUMPAD_EQUALS_AS400 = 0x86,        ///< Keypad Equal Sign
	INTERNATIONAL1 = 0x87,             ///< Keyboard International1 (Used on Asian keyboards, see footnotes in USB doc)
	INTERNATIONAL2 = 0x88,             ///< Keyboard International2
	INTERNATIONAL3 = 0x89,             ///< Keyboard International3 (Yen)
	INTERNATIONAL4 = 0x8A,             ///< Keyboard International4
	INTERNATIONAL5 = 0x8B,             ///< Keyboard International5
	INTERNATIONAL6 = 0x8C,             ///< Keyboard International6
	INTERNATIONAL7 = 0x8D,             ///< Keyboard International7
	INTERNATIONAL8 = 0x8E,             ///< Keyboard International8
	INTERNATIONAL9 = 0x8F,             ///< Keyboard International9
	LANG1 = 0x90,                      ///< Keyboard LANG1 (Hangul/English toggle)
	LANG2 = 0x91,                      ///< Keyboard LANG2 (Hanja conversion)
	LANG3 = 0x92,                      ///< Keyboard LANG3 (Katakana)
	LANG4 = 0x93,                      ///< Keyboard LANG4 (Hiragana)
	LANG5 = 0x94,                      ///< Keyboard LANG5 (Zenkaku/Hankaku)
	LANG6 = 0x95,                      ///< Keyboard LANG6
	LANG7 = 0x96,                      ///< Keyboard LANG7
	LANG8 = 0x97,                      ///< Keyboard LANG8
	LANG9 = 0x98,                      ///< Keyboard LANG9
	ALTERNATE_ERASE = 0x99,            ///< Keyboard Alternate Erase (Erase-Eaze)
	SYS_REQ = 0x9A,                    ///< Keyboard SysReq/Attention
	CANCEL = 0x9B,                     ///< Keyboard Cancel
	CLEAR = 0x9C,                      ///< Keyboard Clear
	PRIOR = 0x9D,                      ///< Keyboard Prior
	RETURN2 = 0x9E,                    ///< Keyboard Return
	SEPARATOR = 0x9F,                  ///< Keyboard Separator
	OUT_ = 0xA0,                       ///< Keyboard Out
	OPER = 0xA1,                       ///< Keyboard Oper
	CLEAR_AGAIN = 0xA2,                ///< Keyboard Clear/Again
	CR_SEL = 0xA3,                     ///< Keyboard CrSel/Props
	EX_SEL = 0xA4,                     ///< Keyboard ExSel
	NUMPAD_00 = 0xB0,                  ///< Keypad 00
	NUMPAD_000 = 0xB1,                 ///< Keypad 000
	THOUSANDS_SEPARATOR = 0xB2,        ///< Thousands Separator
	DECIMAL_SEPARATOR = 0xB3,          ///< Decimal Separator
	CURRENCY_UNIT = 0xB4,              ///< Currency Unit
	CURRENCY_SUB_UNIT = 0xB5,          ///< Currency Sub-unit
	NUMPAD_LEFT_PARENTHESIS = 0xB6,    ///< Keypad (
	NUMPAD_RIGHT_PARENTHESIS = 0xB7,   ///< Keypad )
	NUMPAD_LEFT_BRACE = 0xB8,          ///< Keypad {
	NUMPAD_RIGHT_BRACE = 0xB9,         ///< Keypad }
	NUMPAD_TAB = 0xBA,                 ///< Keypad Tab
	NUMPAD_BACKSPACE = 0xBB,           ///< Keypad Backspace
	NUMPAD_A = 0xBC,                   ///< Keypad A
	NUMPAD_B = 0xBD,                   ///< Keypad B
	NUMPAD_C = 0xBE,                   ///< Keypad C
	NUMPAD_D = 0xBF,                   ///< Keypad D
	NUMPAD_E = 0xC0,                   ///< Keypad E
	NUMPAD_F = 0xC1,                   ///< Keypad F
	NUMPAD_XOR = 0xC2,                 ///< Keypad XOR
	NUMPAD_POWER = 0xC3,               ///< Keypad ^
	NUMPAD_PERCENT = 0xC4,             ///< Keypad %
	NUMPAD_LESS_THAN = 0xC5,           ///< Keypad <
	NUMPAD_GREATER_THAN = 0xC6,        ///< Keypad >
	NUMPAD_AMPERSAND = 0xC7,           ///< Keypad &
	NUMPAD_DOUBLE_AMPERSAND = 0xC8,    ///< Keypad &&
	NUMPAD_VERTICAL_BAR = 0xC9,        ///< Keypad |
	NUMPAD_DOUBLE_VERTICAL_BAR = 0xCA, ///< Keypad ||
	NUMPAD_COLON = 0xCB,               ///< Keypad :
	NUMPAD_HASH = 0xCC,                ///< Keypad #
	NUMPAD_SPACE = 0xCD,               ///< Keypad Space
	NUMPAD_AT = 0xCE,                  ///< Keypad @
	NUMPAD_EXCLAMATION_POINT = 0xCF,   ///< Keypad !
	NUMPAD_MEMORY_STORE = 0xD0,        ///< Keypad Memory Store
	NUMPAD_MEMORY_RECALL = 0xD1,       ///< Keypad Memory Recall
	NUMPAD_MEMORY_CLEAR = 0xD2,        ///< Keypad Memory Clear
	NUMPAD_MEMORY_ADD = 0xD3,          ///< Keypad Memory Add
	NUMPAD_MEMORY_SUBTRACT = 0xD4,     ///< Keypad Memory Subtract
	NUMPAD_MEMORY_MULTIPLY = 0xD5,     ///< Keypad Memory Multiply
	NUMPAD_MEMORY_DIVIDE = 0xD6,       ///< Keypad Memory Divide
	NUMPAD_PLUS_MINUS = 0xD7,          ///< Keypad +/-
	NUMPAD_CLEAR = 0xD8,               ///< Keypad Clear
	NUMPAD_CLEAR_ENTRY = 0xD9,         ///< Keypad Clear Entry
	NUMPAD_BINARY = 0xDA,              ///< Keypad Binary
	NUMPAD_OCTAL = 0xDB,               ///< Keypad Octal
	NUMPAD_DECIMAL = 0xDC,             ///< Keypad Decimal
	NUMPAD_HEXADECIMAL = 0xDD,         ///< Keypad Hexadecimal
	LEFT_CONTROL = 0xE0,               ///< Keyboard LeftControl
	LEFT_SHIFT = 0xE1,                 ///< Keyboard LeftShift
	LEFT_ALT = 0xE2,                   ///< Keyboard LeftAlt (Alt, Option)
	LEFT_GUI = 0xE3,                   ///< Keyboard Left GUI (Windows, Command, Meta)
	RIGHT_CONTROL = 0xE4,              ///< Keyboard RightControl
	RIGHT_SHIFT = 0xE5,                ///< Keyboard RightShift
	RIGHT_ALT = 0xE6,                  ///< Keyboard RightAlt (Alt Gr, Option)
	RIGHT_GUI = 0xE7,                  ///< Keyboard Right GUI (Windows, Command, Meta)

	// Others:
	MODE = 257,                 ///< Mode
	SLEEP = 258,                ///< Sleep
	WAKE = 259,                 ///< Wake
	CHANNEL_INCREMENT = 260,    ///< Channel Increment
	CHANNEL_DECREMENT = 261,    ///< Channel Decrement
	MEDIA_PLAY = 262,           ///< Play
	MEDIA_PAUSE = 263,          ///< Pause
	MEDIA_RECORD = 264,         ///< Record
	MEDIA_FAST_FORWARD = 265,   ///< Fast Forward
	MEDIA_REWIND = 266,         ///< Rewind
	MEDIA_NEXT_TRACK = 267,     ///< Next Track
	MEDIA_PREVIOUS_TRACK = 268, ///< Previous Track
	MEDIA_STOP = 269,           ///< Stop
	MEDIA_EJECT = 270,          ///< Eject
	MEDIA_PLAY_PAUSE = 271,     ///< Play/Pause
	MEDIA_SELECT = 272,         ///< Media Select
	AC_NEW = 273,               ///< AC New
	AC_OPEN = 274,              ///< AC Open
	AC_CLOSE = 275,             ///< AC Close
	AC_EXIT = 276,              ///< AC Exit
	AC_SAVE = 277,              ///< AC Save
	AC_PRINT = 278,             ///< AC Print
	AC_PROPERTIES = 279,        ///< AC Properties
	AC_SEARCH = 280,            ///< AC Search
	AC_HOME = 281,              ///< AC Home
	AC_BACK = 282,              ///< AC Back
	AC_FORWARD = 283,           ///< AC Forward
	AC_STOP = 284,              ///< AC Stop
	AC_REFRESH = 285,           ///< AC Refresh
	AC_BOOKMARKS = 286,         ///< AC Bookmarks
	SOFT_LEFT = 287,            ///< Bottom left multi-function feature key on phones.
	SOFT_RIGHT = 288,           ///< Bottom right multi-function feature key on phones.
	CALL = 289,                 ///< Used for accepting phone calls.
	END_CALL = 290,             ///< Used for rejecting phone calls.
};

/**
 * Key modifier.
 *
 * \sa KeyModifiers
 */
enum class KeyModifier : uint16_t {
	LEFT_SHIFT = 1 << 0,    ///< Left shift.
	RIGHT_SHIFT = 1 << 1,   ///< Right shift.
	LEFT_CONTROL = 1 << 6,  ///< Left control.
	RIGHT_CONTROL = 1 << 7, ///< Right control.
	LEFT_ALT = 1 << 8,      ///< Left alt.
	RIGHT_ALT = 1 << 9,     ///< Right alt.
	LEFT_GUI = 1 << 10,     ///< Left GUI.
	RIGHT_GUI = 1 << 11,    ///< Right GUI.
	NUM_LOCK = 1 << 12,     ///< Number lock.
	CAPS_LOCK = 1 << 13,    ///< Capital lock.
	MODE = 1 << 14,         ///< Mode.
	SCROLL_LOCK = 1 << 15,  ///< Scroll lock.
};

/**
 * Set of key modifiers.
 *
 * \sa KeyModifier
 */
class KeyModifiers {
public:
	static const KeyModifiers ALL;     ///< Set containing all possible key modifiers.
	static const KeyModifiers CONTROL; ///< Left or right control.
	static const KeyModifiers SHIFT;   ///< Left or right shift.
	static const KeyModifiers ALT;     ///< Left or right alt.
	static const KeyModifiers GUI;     ///< Left or right GUI.

	/**
	 * Construct an empty key modifier set.
	 */
	constexpr KeyModifiers() noexcept = default;

	/**
	 * Construct a key modifier set containing only one specific key modifier.
	 *
	 * \param keyModifier key modifier identifier to include.
	 *
	 * \note Key modifier sets can be combined using
	 *       operator|(KeyModifiers, KeyModifiers).
	 */
	constexpr KeyModifiers(KeyModifier keyModifier) noexcept
		: bits(static_cast<uint16_t>(keyModifier)) {}

	/**
	 * Compare this key modifier set to another for equality.
	 *
	 * \param other the key modifier set to compare this one to.
	 *
	 * \return true if the key modifier sets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const KeyModifiers& other) const noexcept = default;

	/**
	 * Check if the key modifier set is empty.
	 *
	 * \return true if the set contains no key modifiers, false otherwise.
	 */
	[[nodiscard]] constexpr bool empty() const noexcept {
		return bits == 0;
	}

	/**
	 * Check if the key modifier set contains the given key modifier.
	 *
	 * \param keyModifier key modifier identifier to check for.
	 *
	 * \return true if the set contains the given key modifier, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(KeyModifier keyModifier) const noexcept {
		return (bits & KeyModifiers{keyModifier}.bits) != 0;
	}

	/**
	 * Check if the key modifier set contains at least one of the given key
	 * modifiers.
	 *
	 * \param keyModifiers key modifier set to check for.
	 *
	 * \return true if the set contains at least one of the given key modifiers,
	 *         false otherwise.
	 */
	[[nodiscard]] constexpr bool containsAnyOf(KeyModifiers keyModifiers) const noexcept {
		return (bits & keyModifiers.bits) != 0;
	}

	/**
	 * Check if the key modifier set contains all of the given key modifiers.
	 *
	 * \param keyModifiers key modifier set to check for.
	 *
	 * \return true if the set contains all of the given key modifiers, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAllOf(KeyModifiers keyModifiers) const noexcept {
		return (bits & keyModifiers.bits) == keyModifiers.bits;
	}

	/**
	 * Get the complement of a key modifier set.
	 *
	 * \param a the set to invert.
	 *
	 * \return a set containing all possible key modifiers except those in the
	 *         given set.
	 */
	[[nodiscard]] friend constexpr KeyModifiers operator~(KeyModifiers a) noexcept {
		return KeyModifiers{static_cast<uint16_t>(~a.bits)};
	}

	/**
	 * Get the intersection of two key modifier sets.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a set containing all key modifiers contained in both a and b.
	 */
	[[nodiscard]] friend constexpr KeyModifiers operator&(KeyModifiers a, KeyModifiers b) noexcept {
		return KeyModifiers{static_cast<uint16_t>(a.bits & b.bits)};
	}

	/**
	 * Get the union of two key modifier sets.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a set containing all key modifiers contained in a or b or both.
	 */
	[[nodiscard]] friend constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b) noexcept {
		return KeyModifiers{static_cast<uint16_t>(a.bits | b.bits)};
	}

	/**
	 * Get the symmetric difference of two key modifier sets.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a set containing all key modifiers contained in either a or b,
	 *         but not both.
	 */
	[[nodiscard]] friend constexpr KeyModifiers operator^(KeyModifiers a, KeyModifiers b) noexcept {
		return KeyModifiers{static_cast<uint16_t>(a.bits ^ b.bits)};
	}

	/**
	 * Assign the intersection of two key modifier sets to the first set.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr KeyModifiers& operator&=(KeyModifiers& a, KeyModifiers b) noexcept {
		return a = a & b;
	}

	/**
	 * Assign the union of two key modifier sets to the first set.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr KeyModifiers& operator|=(KeyModifiers& a, KeyModifiers b) noexcept {
		return a = a | b;
	}

	/**
	 * Assign the symmetric difference of two key modifier sets to the first set.
	 *
	 * \param a first key modifier set.
	 * \param b second key modifier set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr KeyModifiers& operator^=(KeyModifiers& a, KeyModifiers b) noexcept {
		return a = a ^ b;
	}

private:
	constexpr explicit KeyModifiers(uint16_t bits) noexcept
		: bits(bits) {}

	uint16_t bits = 0;
};

/**
 * Get the complement of a key modifier.
 *
 * \param a the key modifier to invert.
 *
 * \return a set containing all possible key modifiers except the given key
 *         modifier.
 */
constexpr KeyModifiers operator~(KeyModifier a) noexcept {
	return ~KeyModifiers{a};
}

/**
 * Get the union of two key modifiers.
 *
 * \param a first key modifier.
 * \param b second key modifier.
 *
 * \return a set containing both a and b.
 */
constexpr KeyModifiers operator|(KeyModifier a, KeyModifier b) noexcept {
	return KeyModifiers{a} | KeyModifiers{b};
}

inline constexpr KeyModifiers KeyModifiers::ALL = ~KeyModifiers{};
inline constexpr KeyModifiers KeyModifiers::CONTROL = KeyModifier::LEFT_CONTROL | KeyModifier::RIGHT_CONTROL;
inline constexpr KeyModifiers KeyModifiers::SHIFT = KeyModifier::LEFT_SHIFT | KeyModifier::RIGHT_SHIFT;
inline constexpr KeyModifiers KeyModifiers::ALT = KeyModifier::LEFT_ALT | KeyModifier::RIGHT_ALT;
inline constexpr KeyModifiers KeyModifiers::GUI = KeyModifier::LEFT_GUI | KeyModifier::RIGHT_GUI;

} // namespace grem::events

#endif
