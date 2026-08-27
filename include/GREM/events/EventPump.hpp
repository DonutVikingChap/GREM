// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_EVENT_PUMP_HPP
#define GREM_EVENTS_EVENT_PUMP_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/events/controller.hpp>

namespace grem::events {

struct Event; // Forward declaration, to avoid including Event.hpp.

/**
 * Visual mouse cursor style.
 */
enum class CursorStyle : uint8_t {
	DEFAULT,               ///< Default cursor. Usually an arrow.
	TEXT,                  ///< Text selection cursor. Usually an I-beam.
	WAIT,                  ///< Waiting cursor. Usually an hourglass, watch or spinning ball.
	CROSSHAIR,             ///< Crosshair cursor.
	PROGRESS,              ///< Waiting cursor indicating that the app is still interactive. Usually WAIT with an arrow.
	RESIZE_DIAGONAL_NW_SE, ///< Diagonal double arrow cursor pointing northwest and southeast.
	RESIZE_DIAGONAL_NE_SW, ///< Diagonal double arrow cursor pointing northeast and southwest.
	RESIZE_HORIZONTAL,     ///< Horizontal double arrow cursor pointing east and west.
	RESIZE_VERTICAL,       ///< Vertical double arrow cursor pointing north and south.
	MOVE,                  ///< Four-pointed arrow cursor.
	NOT_ALLOWED,           ///< Not allowed cursor. Usually a slashed circle or crossbones.
	POINTER,               ///< Pointer cursor that indicates a link. Usually a pointing hand.
	RESIZE_NW,             ///< Diagonal double arrow cursor pointing northwest. May be a single or double arrow.
	RESIZE_N,              ///< Diagonal double arrow cursor pointing north. May be a single or double arrow.
	RESIZE_NE,             ///< Diagonal double arrow cursor pointing northeast. May be a single or double arrow.
	RESIZE_E,              ///< Diagonal double arrow cursor pointing east. May be a single or double arrow.
	RESIZE_SW,             ///< Diagonal double arrow cursor pointing southwest. May be a single or double arrow.
	RESIZE_S,              ///< Diagonal double arrow cursor pointing south. May be a single or double arrow.
	RESIZE_SE,             ///< Diagonal double arrow cursor pointing southeast. May be a single or double arrow.
	RESIZE_W,              ///< Diagonal double arrow cursor pointing west. May be a single or double arrow.
};

/**
 * Configuration options for an EventPump.
 */
struct EventPumpOptions {
	/**
	 * Enable the gamepad subsystem to allow for controller events to be
	 * produced.
	 */
	bool enableControllerSupport = true;

	/**
	 * Use raw input for relative mouse motion.
	 *
	 * If set to true, the operating system will be requested to provide as raw
	 * data as possible from the mouse sensor for the relative motion in mouse
	 * input events. If false, the OS is encouraged to perform the same
	 * processing as it would on the desktop mouse cursor, such as potentially
	 * applying acceleration curves.
	 */
	bool useRawMouseInput = true;
};

/**
 * Persistent system for polling Event data and other user input from the host
 * environment on demand.
 *
 * The latest events are stored in a buffer that can be accessed until the next
 * time events are polled. The main intended usage pattern for this is to call
 * pollEvents() once at the start of each Application frame and then access the
 * event buffer immediately or throughout the rest of the frame as necessary.
 */
class EventPump {
public:
	/**
	 * Construct an event pump.
	 *
	 * \param options initial configuration of the event pump, see
	 *        EventPumpOptions.
	 *
	 * \throws events::Error on failure to initialize the required global
	 *         subsystems.
	 */
	GREM_API(events) EventPump(const EventPumpOptions& options = {});

	/** Destructor. */
	GREM_API(events) ~EventPump();

	/** Copying an event pump is not allowed, since it manages global state. */
	EventPump(const EventPump&) = delete;

	/** Moving an event pump is not allowed, since it manages global state. */
	EventPump(EventPump&&) = delete;

	/** Copying an event pump is not allowed, since it manages global state. */
	EventPump& operator=(const EventPump&) = delete;

	/** Moving an event pump is not allowed, since it manages global state. */
	EventPump& operator=(EventPump&&) = delete;

	/**
	 * Poll events from the environment and update the internal event buffer.
	 *
	 * \return a non-owning read-only view over the polled events, stored in the
	 *         internal event buffer, which is valid until the next call to
	 *         pollEvents() or until the event pump is destroyed, whichever
	 *         happens first.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa getLatestPolledEvents()
	 */
	GREM_API(events) Span<const Event> pollEvents();

	/**
	 * Show the mouse cursor.
	 *
	 * \throws events::Error on failure to show the cursor.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa hideCursor()
	 * \sa setCursorStyle()
	 */
	GREM_API(events) void showCursor();

	/**
	 * Hide the mouse cursor.
	 *
	 * \throws events::Error on failure to hide the cursor.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa showCursor()
	 */
	GREM_API(events) void hideCursor();

	/**
	 * Capture the mouse in order to track all mouse input even if the cursor is
	 * outside of the current window.
	 *
	 * \throws events::Error on failure to set the mouse capture state.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Make sure to call uncaptureMouse() as soon as global mouse input is
	 *       no longer needed.
	 *
	 * \sa uncaptureMouse()
	 */
	GREM_API(events) void captureMouse();

	/**
	 * Stop tracking mouse input outside the current window.
	 *
	 * \throws events::Error on failure to set the mouse capture state.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa captureMouse()
	 */
	GREM_API(events) void uncaptureMouse();

	/**
	 * Allow the screen to be blanked by a screen saver.
	 *
	 * \throws events::Error on failure to set the screen saver allow state.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa disableScreenSaver()
	 */
	GREM_API(events) void enableScreenSaver();

	/**
	 * Prevent the screen to be blanked by a screen saver.
	 *
	 * \throws events::Error on failure to set the screen saver allow state.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa enableScreenSaver()
	 */
	GREM_API(events) void disableScreenSaver();

	/**
	 * Set the current style of the mouse cursor.
	 *
	 * \param newStyle new cursor style to set.
	 *
	 * \throws events::Error on failure to set the cursor style.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(events) void setCursorStyle(CursorStyle newStyle);

	/**
	 * Set the current text contained in the clipboard.
	 *
	 * \param newString new text string to set the clipboard to.
	 *
	 * \throws events::Error on failure to set the clipboard text.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(events) void setClipboardText(CStringView newString);

	/**
	 * Set the current state of raw input for relative mouse motion.
	 *
	 * \param useRawMouseInput whether raw input should be enabled or not.
	 *
	 * \sa EventPumpOptions::useRawMouseInput
	 */
	GREM_API(events) void setRawMouseInputEnabled(bool useRawMouseInput);

	/**
	 * Get the latest events in the internal event buffer that were polled using
	 * pollEvents().
	 *
	 * \return a non-owning read-only view over the polled events, stored in the
	 *         internal event buffer, which is valid until the next call to
	 *         pollEvents() or until the event pump is destroyed, whichever
	 *         happens first.
	 *
	 * \sa pollEvents()
	 */
	[[nodiscard]] Span<const Event> getLatestPolledEvents() const noexcept {
		return events;
	}

	/**
	 * Get the current text contained in the clipboard.
	 *
	 * \return the text in the clipboard, or an empty optional if there is no
	 *         text.
	 *
	 * \throws std::length_error if the maximum clipboard size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(events) Optional<String> getClipboardText() const;

	/**
	 * Check if the screen saver is currently enabled.
	 *
	 * \return true if the screen saver state is allowed, false otherwise.
	 */
	[[nodiscard]] GREM_API(events) bool isScreenSaverEnabled() const noexcept;

	/**
	 * Get the currently known connected controller IDs.
	 *
	 * \return a non-owning read-only view over the controller IDs, which is
	 *         valid until the next call to pollEvents() or until the event pump
	 *         is destroyed, whichever happens first.
	 */
	[[nodiscard]] Span<const uint32_t> getConnectedControllerIDs() const noexcept {
		return controllerIDs;
	}

	/**
	 * Get the controller type of a connected controller.
	 *
	 * \param controllerID ID of the connected controller to get the type of.
	 *
	 * \return the controller type, or ControllerType::UNKNOWN if it could not
	 *         be determined.
	 *
	 * \sa getConnectedControllerIDs()
	 */
	[[nodiscard]] GREM_API(events) ControllerType getControllerType(uint32_t controllerID) const noexcept;

private:
	struct CursorDeleter {
		GREM_API(events) void operator()(void* handle) const noexcept;
	};

	using Cursor = UniqueHandle<void*, CursorDeleter>;

	struct ControllerDeleter {
		GREM_API(events) void operator()(void* handle) const noexcept;
	};

	using Controller = UniqueHandle<void*, ControllerDeleter>;

	bool controllerSupportEnabled;
	ArrayList<Event> events;
	ArrayList<Controller> controllers{};
	ArrayList<uint32_t> controllerIDs{};
	void* cursorRegular = nullptr;
	Cursor cursorDefault{};
	Cursor cursorText{};
	Cursor cursorWait{};
	Cursor cursorCrosshair{};
	Cursor cursorProgress{};
	Cursor cursorResizeDiagonalNwSe{};
	Cursor cursorResizeDiagonalNeSw{};
	Cursor cursorResizeHorizontal{};
	Cursor cursorResizeVertical{};
	Cursor cursorMove{};
	Cursor cursorNotAllowed{};
	Cursor cursorPointer{};
	Cursor cursorResizeNw{};
	Cursor cursorResizeN{};
	Cursor cursorResizeNe{};
	Cursor cursorResizeE{};
	Cursor cursorResizeSw{};
	Cursor cursorResizeS{};
	Cursor cursorResizeSe{};
	Cursor cursorResizeW{};
};

} // namespace grem::events

#endif
