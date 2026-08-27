// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/events/Error.hpp>
#include <GREM/events/ExternalApplication.hpp>

#include <SDL3/SDL.h> // SDL...

namespace grem::events {

void ExternalApplication::openURL(CStringView url) {
	if (!SDL_OpenURL(url.c_str())) {
		throw events::Error{String{"Failed to open external application:\n"} + SDL_GetError()};
	}
}

} // namespace grem::events
