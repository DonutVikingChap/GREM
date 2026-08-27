// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_ERROR_HPP
#define GREM_EXECUTION_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>

namespace grem::execution {

/**
 * Exception type for domain-specific errors originating from the
 * grem::execution module.
 */
struct Error : grem::Error {
	using grem::Error::Error;
};

} // namespace grem::execution

#endif
