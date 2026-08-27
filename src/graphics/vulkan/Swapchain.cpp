// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>

#include "TextureImplementation.hpp"

namespace grem::graphics {

Swapchain::Swapchain(Device& device, Window& window, const SwapchainOptions& options)
	: Texture(TextureImplementation::createSwapchain(device, window, options, VK_NULL_HANDLE)) {}

void Swapchain::setVerticalSynchronizationEnabled(bool useVerticalSynchronization) {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	TextureImplementation::SwapchainImplementation& swapchainImplementation = get()->object.get<TextureImplementation::SwapchainImplementation>();
	if (useVerticalSynchronization != swapchainImplementation.options.useVerticalSynchronization) {
		swapchainImplementation.options.useVerticalSynchronization = useVerticalSynchronization;
		swapchainImplementation.recreate(get()->size);
	}
}

void Swapchain::setMaxBufferedFrameCount(uint32_t maxBufferedFrameCount) {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	TextureImplementation::SwapchainImplementation& swapchainImplementation = get()->object.get<TextureImplementation::SwapchainImplementation>();
	if (maxBufferedFrameCount != swapchainImplementation.options.maxBufferedFrameCount) {
		swapchainImplementation.options.maxBufferedFrameCount = maxBufferedFrameCount;
		swapchainImplementation.recreate(get()->size);
	}
}

bool Swapchain::isVerticalSynchronizationEnabled() const {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	TextureImplementation::SwapchainImplementation& swapchainImplementation = get()->object.get<TextureImplementation::SwapchainImplementation>();
	return swapchainImplementation.isVerticalSynchronizationEnabled;
}

uint32_t Swapchain::getMaxBufferedFrameCount() const {
	GREM_ASSERT(getType() == TextureType::SWAPCHAIN);
	TextureImplementation::SwapchainImplementation& swapchainImplementation = get()->object.get<TextureImplementation::SwapchainImplementation>();
	return swapchainImplementation.options.maxBufferedFrameCount;
}

} // namespace grem::graphics
