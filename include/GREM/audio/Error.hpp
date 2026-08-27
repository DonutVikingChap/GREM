// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_ERROR_HPP
#define GREM_AUDIO_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/StringView.hpp>

namespace grem::audio {

/**
 * Exception type for domain-specific errors originating from the
 * grem::audio module.
 */
struct Error : grem::Error {
	using grem::Error::Error;

	GREM_API(audio) Error(StringView message, unsigned errorCode);
};

} // namespace grem::audio

#endif
