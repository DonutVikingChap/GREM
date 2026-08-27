// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_ERROR_HPP
#define GREM_PHYSICS_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>

namespace grem::physics {

/**
 * Exception type for domain-specific errors originating from the
 * grem::physics module.
 */
struct Error : grem::Error {
	using grem::Error::Error;
};

} // namespace grem::physics

#endif
