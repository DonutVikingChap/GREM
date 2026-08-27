// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_EVENT_HPP
#define GREM_EVENTS_EVENT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/events/Input.hpp>
#include <GREM/events/controller.hpp>
#include <GREM/events/keyboard.hpp>
#include <GREM/events/mouse.hpp>

namespace grem::events {

/** Event base. */
struct EventBase {
	TimePoint timestamp; ///< Timestamp of the event.
};

/** Application Event base. */
struct ApplicationEventBase : EventBase {};

/** Display Event base. */
struct DisplayEventBase : EventBase {
	uint32_t displayID; ///< Unique identifier of the display.
};

/** Window Event base. */
struct WindowEventBase : EventBase {
	uint32_t windowID; ///< Unique identifier of the window.
};

/** Input Event base. */
struct InputEventBase : EventBase {
	uint32_t windowID; ///< Unique identifier of the window that this event belongs to, if any.
};

/** Keyboard key Event base. */
struct KeyEventBase : InputEventBase {
	Scancode scancode;         ///< Scancode of the physical key location.
	KeyCode keyCode;           ///< Virtual key code, based on the user's current OS-level key mapping.
	KeyModifiers keyModifiers; ///< Current key modifiers.

	/**
	 * Get the general identifier of the physical key input.
	 *
	 * \return the input identifier corresponding to the key's scancode.
	 */
	[[nodiscard]] Input getInput() const noexcept;
};

/** Text input Event base. */
struct TextInputEventBase : InputEventBase {
	String inputText; ///< Input text.
};

/** Mouse Event base. */
struct MouseEventBase : InputEventBase {
	uint32_t mouseID;         ///< Unique identifier of the mouse instance, or 0xFFFFFFFF if it was emulated by touch input, or 0xFFFFFFFE if it was emulated by pen input.
	vec2 mousePosition;       ///< Current mouse position in the window.
	vec2 relativeMouseMotion; ///< Position offset relative to the previous position.
};

/** Mouse button Event base. */
struct MouseButtonEventBase : MouseEventBase {
	MouseButton mouseButton; ///< Physical mouse button.
	uint8_t clickCount;      ///< Number of consecutive clicks within a short time interval.

	/**
	 * Get the general identifier of the physical mouse button input.
	 *
	 * \return the input identifier corresponding to the mouse button.
	 */
	[[nodiscard]] Input getInput() const noexcept;
};

/** Controller Event base. */
struct ControllerEventBase : InputEventBase {
	uint32_t controllerID; ///< Unique identifier of the controller instance.
};

/** Controller axis Event base. */
struct ControllerAxisEventBase : ControllerEventBase {
	ControllerAxis axis; ///< Physical controller axis.
	int16_t axisValue;   ///< Current axis value in the range [-32768, 32767].
};

/** Controller button Event base. */
struct ControllerButtonEventBase : ControllerEventBase {
	ControllerButton controllerButton; ///< Physical controller button.

	/**
	 * Get the general identifier of the physical controller button input.
	 *
	 * \return the input identifier corresponding to the controller button.
	 */
	[[nodiscard]] Input getInput() const noexcept;
};

/** Touch Event base. */
struct TouchEventBase : InputEventBase {
	uint64_t touchDeviceID;              ///< Touch device identifier.
	uint64_t fingerID;                   ///< Finger identifier, or 0xFFFFFFFF if it was emulated by mouse input.
	vec2 normalizedFingerPosition;       ///< Current finger position, normalized to the range [0, 1].
	vec2 relativeNormalizedFingerMotion; ///< Finger offset relative to the previous position, normalized to the range [-1, 1].
	float normalizedFingerPressure;      ///< Amount of pressure applied, normalized to the range [0, 1].
};

/** Keyboard keymap Event base. */
struct KeymapEventBase : EventBase {};

/** Clipboard Event base. */
struct ClipboardEventBase : EventBase {};

/** Drop Event base. */
struct DropEventBase : EventBase {
	uint32_t windowID; ///< Unique identifier of the window that was dropped onto, if any.
	vec2 position;     ///< Relative position in the window where the drop event took place.
	String source;     ///< Source application that sent this drop event, or empty if unavailable.
};

/** Application was requested to quit by the user. */
struct ApplicationQuitRequestedEvent : ApplicationEventBase {};

/** Display orientation changed. */
struct DisplayOrientationChangedEvent : DisplayEventBase {
	int orientation; ///< New display orientation.
};

/** Display was added. */
struct DisplayAddedEvent : DisplayEventBase {};

/** Display was added. */
struct DisplayRemovedEvent : DisplayEventBase {};

/** Display was moved. */
struct DisplayMovedEvent : DisplayEventBase {};

/** Display changed desktop mode. */
struct DisplayDesktopModeChangedEvent : DisplayEventBase {};

/** Display changed current mode. */
struct DisplayCurrentModeChangedEvent : DisplayEventBase {};

/** Display changed content scale. */
struct DisplayContentScaleChangedEvent : DisplayEventBase {};

/** Display changed usable bounds. */
struct DisplayUsableBoundsChangedEvent : DisplayEventBase {};

/** Window was shown. */
struct WindowShownEvent : WindowEventBase {};

/** Window was hidden. */
struct WindowHiddenEvent : WindowEventBase {};

/** Window was exposed. */
struct WindowExposedEvent : WindowEventBase {};

/** Window was moved. */
struct WindowMovedEvent : WindowEventBase {
	Offset2D windowPosition; ///< New window position.
};

/** Window was resized. */
struct WindowResizedEvent : WindowEventBase {
	Extent2D windowSize; ///< New window size.
};

/** Window drawable size was changed. */
struct WindowDrawableSizeChangedEvent : WindowEventBase {
	Extent2D windowDrawableSize; ///< New drawable window size.
};

/** Window was minimized. */
struct WindowMinimizedEvent : WindowEventBase {};

/** Window was maximized. */
struct WindowMaximizedEvent : WindowEventBase {};

/** Window was restored. */
struct WindowRestoredEvent : WindowEventBase {};

/** Window gained mouse focus. */
struct WindowMouseFocusGainedEvent : WindowEventBase {};

/** Window lost mouse focus. */
struct WindowMouseFocusLostEvent : WindowEventBase {};

/** Window gained keyboard focus. */
struct WindowKeyboardFocusGainedEvent : WindowEventBase {};

/** Window lost keyboard focus. */
struct WindowKeyboardFocusLostEvent : WindowEventBase {};

/** Window was requested to close. */
struct WindowCloseRequestedEvent : WindowEventBase {};

/** Window was moved to a new display. */
struct WindowDisplayChangedEvent : WindowEventBase {
	uint32_t displayID; ///< Unique identifier of the display that the window was moved to.
};

/** Window display scale changed. */
struct WindowDisplayScaleChangedEvent : WindowEventBase {};

/** Keyboard key was pressed. */
struct KeyPressedEvent : KeyEventBase {};

/** Keyboard key was held, causing a repeat press. */
struct KeyPressRepeatedEvent : KeyEventBase {};

/** Keyboard key was released. */
struct KeyReleasedEvent : KeyEventBase {};

/** Text input was edited. */
struct TextInputEditedEvent : TextInputEventBase {
	int32_t textCursorOffset;          ///< The cursor offset of the start of the selected text.
	int32_t textCursorSelectionLength; ///< The length of the current text selection, if any.
};

/** Text input was submitted. */
struct TextInputSubmittedEvent : TextInputEventBase {};

/** Mouse was moved. */
struct MouseMovedEvent : MouseEventBase {};

/** Mouse button was pressed. */
struct MouseButtonPressedEvent : MouseButtonEventBase {};

/** Mouse button was released. */
struct MouseButtonReleasedEvent : MouseButtonEventBase {};

/** Mouse wheel was scrolled. */
struct MouseWheelScrolledEvent : MouseEventBase {
	vec2 scrollAmount; ///< Amount scrolled horizontally/vertically, with floating-point precision.
};

/** Controller was added. */
struct ControllerAddedEvent : ControllerEventBase {};

/** Controller was removed. */
struct ControllerRemovedEvent : ControllerEventBase {};

/** Controller was remapped. */
struct ControllerRemappedEvent : ControllerEventBase {};

/** Controller axis was moved. */
struct ControllerAxisMovedEvent : ControllerAxisEventBase {};

/** Controller button was pressed. */
struct ControllerButtonPressedEvent : ControllerButtonEventBase {};

/** Controller button was released. */
struct ControllerButtonReleasedEvent : ControllerButtonEventBase {};

/** Touch was moved. */
struct TouchMovedEvent : TouchEventBase {};

/** Touch was pressed. */
struct TouchPressedEvent : TouchEventBase {};

/** Touch was released. */
struct TouchReleasedEvent : TouchEventBase {};

/** Keyboard keymap was changed. */
struct KeymapChangedEvent : KeymapEventBase {};

/** Clipboard was updated. */
struct ClipboardUpdatedEvent : ClipboardEventBase {};

/** File was dropped. */
struct DroppedFileEvent : DropEventBase {
	String droppedFilepath; ///< Filepath of the dropped file.
};

/** Text was dropped. */
struct DroppedTextEvent : DropEventBase {
	String droppedText; ///< Dropped text.
};

/** Drop was started. */
struct DropStartedEvent : DropEventBase {};

/** Drop was completed. */
struct DropCompletedEvent : DropEventBase {};

/**
 * Data structure containing information about an event.
 *
 * Instances of this type are generated by an EventPump when certain events
 * occur in the host environment, typically as a result of user input. These
 * events can either be handled manually or be forwarded to subsystems such as
 * an InputManager for processing.
 */
struct Event
	: Variant<                             //
		  ApplicationQuitRequestedEvent,   //
		  DisplayOrientationChangedEvent,  //
		  DisplayAddedEvent,               //
		  DisplayRemovedEvent,             //
		  DisplayMovedEvent,               //
		  DisplayDesktopModeChangedEvent,  //
		  DisplayCurrentModeChangedEvent,  //
		  DisplayContentScaleChangedEvent, //
		  DisplayUsableBoundsChangedEvent, //
		  WindowShownEvent,                //
		  WindowHiddenEvent,               //
		  WindowExposedEvent,              //
		  WindowMovedEvent,                //
		  WindowResizedEvent,              //
		  WindowDrawableSizeChangedEvent,  //
		  WindowMinimizedEvent,            //
		  WindowMaximizedEvent,            //
		  WindowRestoredEvent,             //
		  WindowMouseFocusGainedEvent,     //
		  WindowMouseFocusLostEvent,       //
		  WindowKeyboardFocusGainedEvent,  //
		  WindowKeyboardFocusLostEvent,    //
		  WindowCloseRequestedEvent,       //
		  WindowDisplayChangedEvent,       //
		  WindowDisplayScaleChangedEvent,  //
		  KeyPressedEvent,                 //
		  KeyPressRepeatedEvent,           //
		  KeyReleasedEvent,                //
		  TextInputEditedEvent,            //
		  TextInputSubmittedEvent,         //
		  MouseMovedEvent,                 //
		  MouseButtonPressedEvent,         //
		  MouseButtonReleasedEvent,        //
		  MouseWheelScrolledEvent,         //
		  ControllerAddedEvent,            //
		  ControllerRemovedEvent,          //
		  ControllerRemappedEvent,         //
		  ControllerAxisMovedEvent,        //
		  ControllerButtonPressedEvent,    //
		  ControllerButtonReleasedEvent,   //
		  TouchMovedEvent,                 //
		  TouchPressedEvent,               //
		  TouchReleasedEvent,              //
		  KeymapChangedEvent,              //
		  ClipboardUpdatedEvent,           //
		  DroppedFileEvent,                //
		  DroppedTextEvent,                //
		  DropStartedEvent,                //
		  DropCompletedEvent> {};

} // namespace grem::events

#endif
