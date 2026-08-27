// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_SIMPLE_MESSAGE_BOX_HPP
#define GREM_EVENTS_SIMPLE_MESSAGE_BOX_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>

namespace grem::events {

/**
 * Type of message contained in a message box.
 */
enum class MessageType : uint8_t {
	ERROR_MESSAGE,   ///< Indicates that an error occured.
	WARNING_MESSAGE, ///< Warns the user about a potential error.
	INFO_MESSAGE,    ///< Provides general information.
};

/**
 * Utility class for simple message boxes with an OK button to be shown to the
 * user through the host environment.
 */
class SimpleMessageBox {
public:
	/**
	 * Display a simple message box that blocks execution on the current thread
	 * until the user presses OK.
	 *
	 * \param type type of message shown in the box. This may be reflected in
	 *        the styling of the box.
	 * \param title window title of the message box.
	 * \param message main message to show in the box.
	 *
	 * \throws events::Error on failure to show the message box.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(events) static void show(MessageType type, CStringView title, CStringView message);

	/** Message box objects are not intended to be constructed directly. */
	SimpleMessageBox() = delete;
};

} // namespace grem::events

#endif
