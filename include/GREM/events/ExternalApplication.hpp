// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_EXTERNAL_APPLICATION_HPP
#define GREM_EVENTS_EXTERNAL_APPLICATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>

namespace grem::events {

/**
 * Utility class for opening external applications through the host environment.
 */
class ExternalApplication {
public:
	/**
	 * Open a URL/URI in an appropriate external application.
	 *
	 * \param url valid URL/URI to open.
	 *
	 * \throws events::Error on failure to open the URL.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(events) static void openURL(CStringView url);

	/** External application objects are not intended to be constructed directly. */
	ExternalApplication() = delete;
};

} // namespace grem::events

#endif
