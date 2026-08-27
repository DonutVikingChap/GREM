// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/events/Error.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/EventPump.hpp>
#include <GREM/events/Input.hpp>
#include <GREM/events/controller.hpp>
#include <GREM/events/keyboard.hpp>
#include <GREM/events/mouse.hpp>

#include <SDL3/SDL.h> // SDL..., Uint64, Sint64
#include <utility>    // std::move

namespace grem::events {

namespace {

constexpr Array<ControllerButton, SDL_GAMEPAD_BUTTON_COUNT> CONTROLLER_BUTTON_MAP = [] {
	Array<ControllerButton, SDL_GAMEPAD_BUTTON_COUNT> result{};
	result[SDL_GAMEPAD_BUTTON_SOUTH] = ControllerButton::SOUTH;
	result[SDL_GAMEPAD_BUTTON_EAST] = ControllerButton::EAST;
	result[SDL_GAMEPAD_BUTTON_WEST] = ControllerButton::WEST;
	result[SDL_GAMEPAD_BUTTON_NORTH] = ControllerButton::NORTH;
	result[SDL_GAMEPAD_BUTTON_BACK] = ControllerButton::SELECT;
	result[SDL_GAMEPAD_BUTTON_GUIDE] = ControllerButton::GUIDE;
	result[SDL_GAMEPAD_BUTTON_START] = ControllerButton::START;
	result[SDL_GAMEPAD_BUTTON_LEFT_STICK] = ControllerButton::LEFT_STICK;
	result[SDL_GAMEPAD_BUTTON_RIGHT_STICK] = ControllerButton::RIGHT_STICK;
	result[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = ControllerButton::LEFT_SHOULDER;
	result[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = ControllerButton::RIGHT_SHOULDER;
	result[SDL_GAMEPAD_BUTTON_DPAD_UP] = ControllerButton::DPAD_UP;
	result[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = ControllerButton::DPAD_DOWN;
	result[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = ControllerButton::DPAD_LEFT;
	result[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = ControllerButton::DPAD_RIGHT;
	result[SDL_GAMEPAD_BUTTON_MISC1] = ControllerButton::MISC1;
	result[SDL_GAMEPAD_BUTTON_LEFT_PADDLE1] = ControllerButton::LEFT_PADDLE1;
	result[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1] = ControllerButton::RIGHT_PADDLE1;
	result[SDL_GAMEPAD_BUTTON_LEFT_PADDLE2] = ControllerButton::LEFT_PADDLE2;
	result[SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2] = ControllerButton::RIGHT_PADDLE2;
	result[SDL_GAMEPAD_BUTTON_TOUCHPAD] = ControllerButton::TOUCHPAD;
	return result;
}();

[[nodiscard]] TimePoint getTimestamp(Uint64 baseTickNanoseconds, TimePoint baseTime, Uint64 tickNanoseconds) {
	return baseTime + duration_cast<Duration>(Nanoseconds{static_cast<Nanoseconds::rep>(static_cast<Sint64>(tickNanoseconds - baseTickNanoseconds))});
}

[[nodiscard]] ControllerType translateControllerType(SDL_GamepadType gamepadType) {
	switch (gamepadType) {
		case SDL_GAMEPAD_TYPE_STANDARD: return ControllerType::STANDARD;
		case SDL_GAMEPAD_TYPE_XBOX360: return ControllerType::XBOX_360;
		case SDL_GAMEPAD_TYPE_XBOXONE: return ControllerType::XBOX_ONE;
		case SDL_GAMEPAD_TYPE_PS3: return ControllerType::PS3;
		case SDL_GAMEPAD_TYPE_PS4: return ControllerType::PS4;
		case SDL_GAMEPAD_TYPE_PS5: return ControllerType::PS5;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return ControllerType::SWITCH_PRO;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return ControllerType::SWITCH_JOYCON_LEFT;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return ControllerType::SWITCH_JOYCON_RIGHT;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return ControllerType::SWITCH_JOYCON_PAIR;
		case SDL_GAMEPAD_TYPE_GAMECUBE: return ControllerType::GAMECUBE;
		default: break;
	}
	return ControllerType::UNKNOWN;
}

[[nodiscard]] KeyModifiers translateKeyModifiers(SDL_Keymod keymod) {
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_LSHIFT}) == KeyModifiers{KeyModifier::LEFT_SHIFT});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_RSHIFT}) == KeyModifiers{KeyModifier::RIGHT_SHIFT});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_LCTRL}) == KeyModifiers{KeyModifier::LEFT_CONTROL});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_RCTRL}) == KeyModifiers{KeyModifier::RIGHT_CONTROL});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_LALT}) == KeyModifiers{KeyModifier::LEFT_ALT});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_RALT}) == KeyModifiers{KeyModifier::RIGHT_ALT});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_LGUI}) == KeyModifiers{KeyModifier::LEFT_GUI});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_RGUI}) == KeyModifiers{KeyModifier::RIGHT_GUI});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_NUM}) == KeyModifiers{KeyModifier::NUM_LOCK});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_CAPS}) == KeyModifiers{KeyModifier::CAPS_LOCK});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_MODE}) == KeyModifiers{KeyModifier::MODE});
	static_assert(bit_cast<KeyModifiers>(uint16_t{SDL_KMOD_SCROLL}) == KeyModifiers{KeyModifier::SCROLL_LOCK});
	return bit_cast<KeyModifiers>(uint16_t{keymod});
}

[[nodiscard]] Optional<Event> translateEvent(Uint64 baseTickNanoseconds, TimePoint baseTime, const SDL_Event& event) {
	Optional<Event> result{};
	switch (event.type) {
		case SDL_EVENT_QUIT:
			result = Event{ApplicationQuitRequestedEvent{ApplicationEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.quit.timestamp)}}}};
			break;
		case SDL_EVENT_DISPLAY_ORIENTATION:
			result =
				Event{DisplayOrientationChangedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID},
					static_cast<int32_t>(event.display.data1)}};
			break;
		case SDL_EVENT_DISPLAY_ADDED:
			result = Event{DisplayAddedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_REMOVED:
			result = Event{DisplayRemovedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_MOVED:
			result = Event{DisplayMovedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
			result =
				Event{DisplayDesktopModeChangedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
			result =
				Event{DisplayCurrentModeChangedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
			result =
				Event{DisplayContentScaleChangedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
			result =
				Event{DisplayUsableBoundsChangedEvent{DisplayEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.display.timestamp)}, event.display.displayID}}};
			break;
		case SDL_EVENT_WINDOW_SHOWN:
			result = Event{WindowShownEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_HIDDEN:
			result = Event{WindowHiddenEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_EXPOSED:
			result = Event{WindowExposedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_MOVED:
			result = Event{WindowMovedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID},
				{static_cast<int32_t>(event.window.data1), static_cast<int32_t>(event.window.data2)}}};
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			result = Event{WindowResizedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID},
				{static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2)}}};
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			result = Event{WindowDrawableSizeChangedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID},
				{static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2)}}};
			break;
		case SDL_EVENT_WINDOW_MINIMIZED:
			result = Event{WindowMinimizedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_MAXIMIZED:
			result = Event{WindowMaximizedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_RESTORED:
			result = Event{WindowRestoredEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			result = Event{WindowMouseFocusGainedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			result = Event{WindowMouseFocusLostEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			result = Event{WindowKeyboardFocusGainedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			result = Event{WindowKeyboardFocusLostEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			result = Event{WindowCloseRequestedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			result = Event{WindowDisplayChangedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID},
				static_cast<uint32_t>(event.window.data1)}};
			break;
		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			result = Event{WindowDisplayScaleChangedEvent{WindowEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.window.timestamp)}, event.window.windowID}}};
			break;
		case SDL_EVENT_KEY_DOWN:
			if (event.key.repeat == 0) {
				result = Event{KeyPressedEvent{KeyEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.key.timestamp)}, event.key.windowID},
					static_cast<Scancode>(static_cast<uint16_t>(event.key.scancode)), static_cast<KeyCode>(event.key.key), translateKeyModifiers(event.key.mod)}}};
			} else {
				result = Event{KeyPressRepeatedEvent{KeyEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.key.timestamp)}, event.key.windowID},
					static_cast<Scancode>(static_cast<uint16_t>(event.key.scancode)), static_cast<KeyCode>(event.key.key), translateKeyModifiers(event.key.mod)}}};
			}
			break;
		case SDL_EVENT_KEY_UP:
			result = Event{KeyReleasedEvent{KeyEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.key.timestamp)}, event.key.windowID},
				static_cast<Scancode>(static_cast<uint16_t>(event.key.scancode)), static_cast<KeyCode>(event.key.key), translateKeyModifiers(event.key.mod)}}};
			break;
		case SDL_EVENT_TEXT_EDITING:
			result = Event{TextInputEditedEvent{
				TextInputEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.edit.timestamp)}, event.edit.windowID}, event.edit.text},
				event.edit.start, event.edit.length}};
			break;
		case SDL_EVENT_TEXT_INPUT:
			result = Event{TextInputSubmittedEvent{
				TextInputEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.text.timestamp)}, event.text.windowID}, event.text.text}}};
			break;
		case SDL_EVENT_MOUSE_MOTION:
			result = Event{MouseMovedEvent{MouseEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.motion.timestamp)}, event.motion.windowID},
				event.motion.which, {event.motion.x, event.motion.y}, {event.motion.xrel, event.motion.yrel}}}};
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			result = Event{MouseButtonPressedEvent{
				MouseButtonEventBase{MouseEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.button.timestamp)}, event.button.windowID},
										 event.button.which, {event.button.x, event.button.y}, {0, 0}},
					static_cast<MouseButton>(event.button.button), event.button.clicks}}};
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			result = Event{MouseButtonReleasedEvent{
				MouseButtonEventBase{MouseEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.button.timestamp)}, event.button.windowID},
										 event.button.which, {event.button.x, event.button.y}, {0, 0}},
					static_cast<MouseButton>(event.button.button), event.button.clicks}}};
			break;
		case SDL_EVENT_MOUSE_WHEEL: {
			const float direction = (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f;
			result =
				Event{MouseWheelScrolledEvent{MouseEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.wheel.timestamp)}, event.wheel.windowID},
												  event.wheel.which, {event.wheel.mouse_x, event.wheel.mouse_y}, {0, 0}},
					{event.wheel.x * direction, event.wheel.y * direction}}};
			break;
		}
		case SDL_EVENT_GAMEPAD_ADDED:
			result = Event{
				ControllerAddedEvent{ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gdevice.timestamp)}, 0}, event.gdevice.which}}};
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			result = Event{ControllerRemovedEvent{
				ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gdevice.timestamp)}, 0}, event.gdevice.which}}};
			break;
		case SDL_EVENT_GAMEPAD_REMAPPED:
			result = Event{ControllerRemappedEvent{
				ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gdevice.timestamp)}, 0}, event.gdevice.which}}};
			break;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			result = Event{ControllerAxisMovedEvent{
				ControllerAxisEventBase{ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gaxis.timestamp)}, 0}, event.gaxis.which},
					static_cast<ControllerAxis>(event.gaxis.axis), event.gaxis.value}}};
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			result = Event{ControllerButtonPressedEvent{ControllerButtonEventBase{
				ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gbutton.timestamp)}, 0}, event.gbutton.which},
				CONTROLLER_BUTTON_MAP[event.gbutton.button]}}};
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			result = Event{ControllerButtonReleasedEvent{ControllerButtonEventBase{
				ControllerEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.gbutton.timestamp)}, 0}, event.gbutton.which},
				CONTROLLER_BUTTON_MAP[event.gbutton.button]}}};
			break;
		case SDL_EVENT_FINGER_MOTION:
			result = Event{TouchMovedEvent{TouchEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.tfinger.timestamp)}, event.tfinger.windowID},
				event.tfinger.touchID, event.tfinger.fingerID, {event.tfinger.x, event.tfinger.y}, {event.tfinger.dx, event.tfinger.dy}, event.tfinger.pressure}}};
			break;
		case SDL_EVENT_FINGER_DOWN:
			result = Event{TouchPressedEvent{TouchEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.tfinger.timestamp)}, event.tfinger.windowID},
				event.tfinger.touchID, event.tfinger.fingerID, {event.tfinger.x, event.tfinger.y}, {event.tfinger.dx, event.tfinger.dy}, event.tfinger.pressure}}};
			break;
		case SDL_EVENT_FINGER_UP:
			result =
				Event{TouchReleasedEvent{TouchEventBase{InputEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.tfinger.timestamp)}, event.tfinger.windowID},
					event.tfinger.touchID, event.tfinger.fingerID, {event.tfinger.x, event.tfinger.y}, {event.tfinger.dx, event.tfinger.dy}, event.tfinger.pressure}}};
			break;
		case SDL_EVENT_KEYMAP_CHANGED: result = Event{KeymapChangedEvent{KeymapEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.common.timestamp)}}}}; break;
		case SDL_EVENT_CLIPBOARD_UPDATE:
			result = Event{ClipboardUpdatedEvent{ClipboardEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.common.timestamp)}}}};
			break;
		case SDL_EVENT_DROP_FILE:
			result = Event{DroppedFileEvent{DropEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.drop.timestamp)}, event.drop.windowID,
												{event.drop.x, event.drop.y}, (event.drop.source) ? String{event.drop.source} : String{}},
				event.drop.data}};
			break;
		case SDL_EVENT_DROP_TEXT:
			result = Event{DroppedTextEvent{DropEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.drop.timestamp)}, event.drop.windowID,
												{event.drop.x, event.drop.y}, (event.drop.source) ? String{event.drop.source} : String{}},
				event.drop.data}};
			break;
		case SDL_EVENT_DROP_BEGIN:
			result = Event{DropStartedEvent{DropEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.drop.timestamp)}, event.drop.windowID,
				{event.drop.x, event.drop.y}, (event.drop.source) ? String{event.drop.source} : String{}}}};
			break;
		case SDL_EVENT_DROP_COMPLETE:
			result = Event{DropCompletedEvent{DropEventBase{EventBase{getTimestamp(baseTickNanoseconds, baseTime, event.drop.timestamp)}, event.drop.windowID,
				{event.drop.x, event.drop.y}, (event.drop.source) ? String{event.drop.source} : String{}}}};
			break;
		default: break;
	}
	return result;
}

} // namespace

EventPump::EventPump(const EventPumpOptions& options)
	: controllerSupportEnabled(options.enableControllerSupport)
	, events() // NOLINT(readability-redundant-member-init)
{
	GREM_PROFILE_FUNCTION();

	SDL_InitFlags initFlags = SDL_INIT_EVENTS | SDL_INIT_VIDEO; // We need SDL_INIT_VIDEO for SDL_CreateSystemCursor to work.
	if (options.enableControllerSupport) {
		initFlags |= SDL_INIT_GAMEPAD;
	}
	{
		GREM_PROFILE_BLOCK("Initialize SDL (events subsystem)");
		if (!SDL_InitSubSystem(initFlags)) {
			throw events::Error{String{"Failed to initialize SDL events subsystem:\n"} + SDL_GetError()};
		}
	}
	cursorRegular = SDL_GetDefaultCursor();
	cursorDefault.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));
	cursorText.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT));
	cursorWait.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT));
	cursorCrosshair.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));
	cursorProgress.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_PROGRESS));
	cursorResizeDiagonalNwSe.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE));
	cursorResizeDiagonalNeSw.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE));
	cursorResizeHorizontal.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE));
	cursorResizeVertical.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE));
	cursorMove.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE));
	cursorNotAllowed.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED));
	cursorPointer.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER));
	cursorResizeNw.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NW_RESIZE));
	cursorResizeN.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_N_RESIZE));
	cursorResizeNe.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NE_RESIZE));
	cursorResizeE.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_E_RESIZE));
	cursorResizeSw.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SW_RESIZE));
	cursorResizeS.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_S_RESIZE));
	cursorResizeSe.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SE_RESIZE));
	cursorResizeW.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_W_RESIZE));

	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
	SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
	SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "0");
	setRawMouseInputEnabled(options.useRawMouseInput);
}

EventPump::~EventPump() {
	SDL_InitFlags initFlags = SDL_INIT_EVENTS | SDL_INIT_VIDEO;
	if (controllerSupportEnabled) {
		initFlags |= SDL_INIT_GAMEPAD;
	}
	SDL_QuitSubSystem(initFlags);
}

Span<const Event> EventPump::pollEvents() {
	GREM_PROFILE_FUNCTION();

	const Uint64 baseTickNanoseconds = SDL_GetTicksNS();
	const TimePoint baseTime = Clock::now();
	events.clear();
	for (SDL_Event event{}; SDL_PollEvent(&event);) {
		if (Optional<Event> translated = translateEvent(baseTickNanoseconds, baseTime, event)) {
			if (controllerSupportEnabled) {
				if (const ControllerAddedEvent* const controllerAdded = translated->get_if<ControllerAddedEvent>()) {
					Controller controller{SDL_OpenGamepad(controllerAdded->controllerID)};
					if (controller) {
						controllers.push_back(std::move(controller));
						try {
							controllerIDs.push_back(controllerAdded->controllerID);
						} catch (...) {
							controllers.pop_back();
							throw;
						}
					}
				} else if (const ControllerRemovedEvent* const controllerRemoved = translated->get_if<ControllerRemovedEvent>()) {
					auto it = controllerIDs.begin();
					while (true) {
						it = find(Subrange{it, controllerIDs.end()}, controllerRemoved->controllerID);
						if (it == controllerIDs.end()) {
							break;
						}
						const size_t index = static_cast<size_t>(it - controllerIDs.begin());
						controllers.erase(controllers.begin() + static_cast<ptrdiff_t>(index));
						it = controllerIDs.erase(it);
					}
				}
			}
			events.push_back(std::move(*translated));
		}
	}
	return events;
}

void EventPump::showCursor() {
	if (!SDL_ShowCursor()) {
		throw events::Error{String{"Failed to show cursor:\n"} + SDL_GetError()};
	}
}

void EventPump::hideCursor() {
	if (!SDL_HideCursor()) {
		throw events::Error{String{"Failed to hide cursor:\n"} + SDL_GetError()};
	}
}

void EventPump::captureMouse() {
	if (!SDL_CaptureMouse(true)) {
		throw events::Error{String{"Failed to capture mouse:\n"} + SDL_GetError()};
	}
}

void EventPump::uncaptureMouse() {
	if (!SDL_CaptureMouse(false)) {
		throw events::Error{String{"Failed to uncapture mouse:\n"} + SDL_GetError()};
	}
}

void EventPump::enableScreenSaver() {
	if (!SDL_EnableScreenSaver()) {
		throw events::Error{String{"Failed to enable screen saver:\n"} + SDL_GetError()};
	}
}

void EventPump::disableScreenSaver() {
	if (!SDL_DisableScreenSaver()) {
		throw events::Error{String{"Failed to disable screen saver:\n"} + SDL_GetError()};
	}
}

void EventPump::setCursorStyle(CursorStyle newStyle) {
	SDL_Cursor* cursor = nullptr;
	switch (newStyle) {
		case CursorStyle::DEFAULT:
			cursor = static_cast<SDL_Cursor*>(cursorDefault.get());
			if (!cursor) {
				cursor = static_cast<SDL_Cursor*>(cursorRegular);
			}
			break;
		case CursorStyle::TEXT: cursor = static_cast<SDL_Cursor*>(cursorText.get()); break;
		case CursorStyle::WAIT: cursor = static_cast<SDL_Cursor*>(cursorWait.get()); break;
		case CursorStyle::CROSSHAIR: cursor = static_cast<SDL_Cursor*>(cursorCrosshair.get()); break;
		case CursorStyle::PROGRESS: cursor = static_cast<SDL_Cursor*>(cursorProgress.get()); break;
		case CursorStyle::RESIZE_DIAGONAL_NW_SE: cursor = static_cast<SDL_Cursor*>(cursorResizeDiagonalNwSe.get()); break;
		case CursorStyle::RESIZE_DIAGONAL_NE_SW: cursor = static_cast<SDL_Cursor*>(cursorResizeDiagonalNeSw.get()); break;
		case CursorStyle::RESIZE_HORIZONTAL: cursor = static_cast<SDL_Cursor*>(cursorResizeHorizontal.get()); break;
		case CursorStyle::RESIZE_VERTICAL: cursor = static_cast<SDL_Cursor*>(cursorResizeVertical.get()); break;
		case CursorStyle::MOVE: cursor = static_cast<SDL_Cursor*>(cursorMove.get()); break;
		case CursorStyle::NOT_ALLOWED: cursor = static_cast<SDL_Cursor*>(cursorNotAllowed.get()); break;
		case CursorStyle::POINTER: cursor = static_cast<SDL_Cursor*>(cursorPointer.get()); break;
		case CursorStyle::RESIZE_NW: cursor = static_cast<SDL_Cursor*>(cursorResizeNw.get()); break;
		case CursorStyle::RESIZE_N: cursor = static_cast<SDL_Cursor*>(cursorResizeN.get()); break;
		case CursorStyle::RESIZE_NE: cursor = static_cast<SDL_Cursor*>(cursorResizeNe.get()); break;
		case CursorStyle::RESIZE_E: cursor = static_cast<SDL_Cursor*>(cursorResizeE.get()); break;
		case CursorStyle::RESIZE_SW: cursor = static_cast<SDL_Cursor*>(cursorResizeSw.get()); break;
		case CursorStyle::RESIZE_S: cursor = static_cast<SDL_Cursor*>(cursorResizeS.get()); break;
		case CursorStyle::RESIZE_SE: cursor = static_cast<SDL_Cursor*>(cursorResizeSe.get()); break;
		case CursorStyle::RESIZE_W: cursor = static_cast<SDL_Cursor*>(cursorResizeW.get()); break;
	}
	if (!SDL_SetCursor(cursor)) {
		throw events::Error{String{"Failed to set cursor style:\n"} + SDL_GetError()};
	}
}

void EventPump::setClipboardText(CStringView newString) {
	if (!SDL_SetClipboardText(newString.c_str())) {
		throw events::Error{String{"Failed to set clipboard text:\n"} + SDL_GetError()};
	}
}

void EventPump::setRawMouseInputEnabled(bool useRawMouseInput) {
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, (useRawMouseInput) ? "0" : "1");
}

Optional<String> EventPump::getClipboardText() const {
	if (SDL_HasClipboardText()) {
		Optional<String> result{};
		if (char* const text = SDL_GetClipboardText()) {
			try {
				result.emplace(text);
				SDL_free(text);
			} catch (...) {
				SDL_free(text);
				throw;
			}
		}
		return result;
	}
	return {};
}

bool EventPump::isScreenSaverEnabled() const noexcept {
	return SDL_ScreenSaverEnabled();
}

ControllerType EventPump::getControllerType(uint32_t controllerID) const noexcept {
	return translateControllerType(SDL_GetGamepadTypeForID(controllerID));
}

void EventPump::CursorDeleter::operator()(void* handle) const noexcept {
	SDL_DestroyCursor(static_cast<SDL_Cursor*>(handle));
}

void EventPump::ControllerDeleter::operator()(void* handle) const noexcept {
	SDL_CloseGamepad(static_cast<SDL_Gamepad*>(handle));
}

} // namespace grem::events
