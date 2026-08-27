// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/core/version.hpp>

namespace grem {

#define GREM_PRIVATE_CORE_STRINGIFY(x) #x
#define GREM_PRIVATE_CORE_MAKE_VERSION_NAME_STRING(major, minor, patch) \
	GREM_PRIVATE_CORE_STRINGIFY(major) "." GREM_PRIVATE_CORE_STRINGIFY(minor) "." GREM_PRIVATE_CORE_STRINGIFY(patch)

const char* getVersionName() noexcept {
	return GREM_PRIVATE_CORE_MAKE_VERSION_NAME_STRING(GREM_PRIVATE_VERSION_MAJOR, GREM_PRIVATE_VERSION_MINOR, GREM_PRIVATE_VERSION_PATCH);
}

uint32_t getMajorVersion() noexcept {
	return GREM_PRIVATE_VERSION_MAJOR;
}

uint32_t getMinorVersion() noexcept {
	return GREM_PRIVATE_VERSION_MINOR;
}

uint32_t getPatchVersion() noexcept {
	return GREM_PRIVATE_VERSION_PATCH;
}

uint32_t getABIVersion() noexcept {
	return GREM_PRIVATE_ABI_VERSION;
}

} // namespace grem
