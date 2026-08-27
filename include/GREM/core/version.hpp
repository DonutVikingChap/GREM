// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_VERSION_HPP
#define GREM_CORE_VERSION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

namespace grem {

/**
 * Get the current version string of the library.
 *
 * \return a non-owning read-only pointer to a null-terminated statically
 *         allocated string containing the full current library version name in
 *         the format `<major>.<minor>.<patch>`. Note that the ABI version
 *         number is implied by the library version.
 *
 * \sa getMajorVersion()
 * \sa getMinorVersion()
 * \sa getPatchVersion()
 * \sa getABIVersion()
 */
[[nodiscard]] GREM_API(core) const char* getVersionName() noexcept;

/**
 * Get the current major version number of the library.
 *
 * \return the current major version number.
 */
[[nodiscard]] GREM_API(core) uint32_t getMajorVersion() noexcept;

/**
 * Get the current minor version number of the library.
 *
 * \return the current minor version number.
 */
[[nodiscard]] GREM_API(core) uint32_t getMinorVersion() noexcept;

/**
 * Get the current patch version number of the library.
 *
 * \return the current patch version number.
 */
[[nodiscard]] GREM_API(core) uint32_t getPatchVersion() noexcept;

/**
 * Get the current ABI version number of the library.
 *
 * \return the current ABI version number.
 */
[[nodiscard]] GREM_API(core) uint32_t getABIVersion() noexcept;

} // namespace grem

#endif
