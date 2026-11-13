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

namespace {

[[nodiscard]] TextureFormat getSwapchainTextureFormat(Window& window) {
	SDL_GL_MakeCurrent(static_cast<SDL_Window*>(const_cast<void*>(window.get())), static_cast<SDL_GLContext>(const_cast<void*>(window.getSurface())));
	int redSize = 0;
	int greenSize = 0;
	int blueSize = 0;
	int alphaSize = 0;
	if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &redSize) && SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &greenSize) && SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &blueSize) &&
		SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alphaSize)) {
		if (redSize == 10 && greenSize == 10 && blueSize == 10 && alphaSize == 2) {
			return TextureFormat::A2B10G10R10_UNORM_PACK32;
		}
		if (redSize == 16 && greenSize == 16 && blueSize == 16 && alphaSize == 16) {
			return TextureFormat::R16G16B16A16_FLOAT;
		}
	}
	return TextureFormat::R8G8B8A8_SRGB;
}

} // namespace

Swapchain::Swapchain(Device& device, Window& window, const SwapchainOptions& options)
	: Texture(TextureImplementation::create(device, &window, TextureType::SWAPCHAIN, getSwapchainTextureFormat(window), Extent3D{}, 1, window.getMultisampleCount(), {})) {
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
