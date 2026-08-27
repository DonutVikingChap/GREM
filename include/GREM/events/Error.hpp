// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_ERROR_HPP
#define GREM_EVENTS_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>

namespace grem::events {

/**
 * Exception type for domain-specific errors originating from the grem::events
 * module.
 */
struct Error : grem::Error {
	using grem::Error::Error;
};

} // namespace grem::events

#endif
