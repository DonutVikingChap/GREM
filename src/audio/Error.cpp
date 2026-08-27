// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/audio/Error.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

#include <soloud.h> // SoLoud::...

namespace grem::audio {

namespace {

constexpr CStringView getErrorCodeMessage(unsigned errorCode) noexcept {
	static_assert(same_as<unsigned, SoLoud::result>);
	switch (errorCode) {
		case SoLoud::SO_NO_ERROR: return "No error.";
		case SoLoud::INVALID_PARAMETER: return "Some parameter is invalid.";
		case SoLoud::FILE_NOT_FOUND: return "File not found.";
		case SoLoud::FILE_LOAD_FAILED: return "File found, but could not be loaded.";
		case SoLoud::DLL_NOT_FOUND: return "DLL not found, or wrong DLL.";
		case SoLoud::OUT_OF_MEMORY: return "Out of memory.";
		case SoLoud::NOT_IMPLEMENTED: return "Feature not implemented.";
		default: break;
	}
	return "Unknown SoLoud error.";
}

} // namespace

Error::Error(StringView message, unsigned errorCode)
	: Error(String{message} + ": " + getErrorCodeMessage(errorCode).c_str()) {}

} // namespace grem::audio
