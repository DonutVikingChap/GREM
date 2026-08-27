// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>

#include "TextureImplementation.hpp"

#include <SDL3/SDL.h> // SDL...

namespace grem::graphics {

Swapchain::Swapchain(Device& device, Window& window, const SwapchainOptions& options)
	: Texture(TextureImplementation::create(device, &window, TextureType::SWAPCHAIN, TextureFormat::UNKNOWN, Extent3D{}, 1, window.getMultisampleCount(), {})) {
	setVerticalSynchronizationEnabled(options.useVerticalSynchronization);
}

void Swapchain::setVerticalSynchronizationEnabled(bool useVerticalSynchronization) {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	Window& window = *get()->object.get<Window*>();
	SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
	if (useVerticalSynchronization) {
		if (!SDL_GL_SetSwapInterval(-1) && !SDL_GL_SetSwapInterval(1)) {
			throw graphics::Error{String{"Failed to enable VSync:\n"} + SDL_GetError()};
		}
	} else {
		if (!SDL_GL_SetSwapInterval(0)) {
			throw graphics::Error{String{"Failed to disable VSync:\n"} + SDL_GetError()};
		}
	}
}

void Swapchain::setMaxBufferedFrameCount(uint32_t maxBufferedFrameCount) {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	(void)maxBufferedFrameCount;
}

bool Swapchain::isVerticalSynchronizationEnabled() const {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	Window& window = *get()->object.get<Window*>();
	SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
	int interval{};
	SDL_GL_GetSwapInterval(&interval);
	return interval != 0;
}

uint32_t Swapchain::getMaxBufferedFrameCount() const {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	return 0;
}

} // namespace grem::graphics
