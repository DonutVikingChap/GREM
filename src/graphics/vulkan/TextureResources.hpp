// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_TEXTURE_RESOURCES_HPP
#define GREM_GRAPHICS_VULKAN_TEXTURE_RESOURCES_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include "vulkan.hpp"

#include <utility> // std::exchange

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

namespace detail {

struct TextureResources {
	Device& device;
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation imageAllocation = VK_NULL_HANDLE;
	VkImageView samplerImageView = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;

	explicit TextureResources(Device& device) noexcept
		: device(device) {}

	~TextureResources() {
		reset();
	}

	TextureResources(const TextureResources&) = delete;

	TextureResources(TextureResources&& other) noexcept
		: device(other.device)
		, image(std::exchange(other.image, VK_NULL_HANDLE))
		, imageAllocation(std::exchange(other.imageAllocation, VK_NULL_HANDLE))
		, samplerImageView(std::exchange(other.samplerImageView, VK_NULL_HANDLE))
		, sampler(std::exchange(other.sampler, VK_NULL_HANDLE)) {}

	TextureResources& operator=(const TextureResources&) = delete;

	TextureResources& operator=(TextureResources&& other) noexcept {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return *this;
		}
		reset();
		image = std::exchange(other.image, VK_NULL_HANDLE);
		imageAllocation = std::exchange(other.imageAllocation, VK_NULL_HANDLE);
		samplerImageView = std::exchange(other.samplerImageView, VK_NULL_HANDLE);
		sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
		return *this;
	}

	void reset() noexcept;
};

} // namespace detail

} // namespace grem::graphics

#endif
