// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/events/Error.hpp>
#include <GREM/events/SimpleMessageBox.hpp>

#include <SDL3/SDL.h> // SDL..., Uint32

namespace grem::events {

void SimpleMessageBox::show(MessageType type, CStringView title, CStringView message) {
	Uint32 flags = 0;
	switch (type) {
		case MessageType::ERROR_MESSAGE: flags = SDL_MESSAGEBOX_ERROR; break;
		case MessageType::WARNING_MESSAGE: flags = SDL_MESSAGEBOX_WARNING; break;
		case MessageType::INFO_MESSAGE: flags = SDL_MESSAGEBOX_INFORMATION; break;
	}
	if (!SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), nullptr)) {
		throw events::Error{String{"Failed to show simple message box:\n"} + SDL_GetError()};
	}
}

} // namespace grem::events
