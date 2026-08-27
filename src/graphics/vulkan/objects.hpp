// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_OBJECTS_HPP
#define GREM_GRAPHICS_VULKAN_OBJECTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/UniqueHandle.hpp>

#include "vulkan.hpp"

namespace grem::graphics {

namespace detail {

struct VulkanDeviceDeleter {
	void operator()(VkDevice handle) const noexcept {
		vkDestroyDevice(handle, nullptr);
	}
};

using VulkanDevice = UniqueHandle<VkDevice, VulkanDeviceDeleter, VK_NULL_HANDLE>;

struct VulkanAllocatorDeleter {
	void operator()(VmaAllocator handle) const noexcept {
		vmaDestroyAllocator(handle);
	}
};

using VulkanAllocator = UniqueHandle<VmaAllocator, VulkanAllocatorDeleter, VK_NULL_HANDLE>;

struct VulkanPipelineCacheDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkPipelineCache handle) const noexcept {
		if (device) {
			vkDestroyPipelineCache(device, handle, nullptr);
		}
	}
};

using VulkanPipelineCache = UniqueHandle<VkPipelineCache, VulkanPipelineCacheDeleter, VK_NULL_HANDLE>;

struct VulkanCommandPoolDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkCommandPool handle) const noexcept {
		if (device) {
			vkDestroyCommandPool(device, handle, nullptr);
		}
	}
};

using VulkanCommandPool = UniqueHandle<VkCommandPool, VulkanCommandPoolDeleter, VK_NULL_HANDLE>;

struct VulkanFenceDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkFence handle) const noexcept {
		if (device) {
			vkDestroyFence(device, handle, nullptr);
		}
	}
};

using VulkanFence = UniqueHandle<VkFence, VulkanFenceDeleter, VK_NULL_HANDLE>;

struct VulkanSemaphoreDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkSemaphore handle) const noexcept {
		if (device) {
			vkDestroySemaphore(device, handle, nullptr);
		}
	}
};

using VulkanSemaphore = UniqueHandle<VkSemaphore, VulkanSemaphoreDeleter, VK_NULL_HANDLE>;

struct VulkanBufferDeleter {
	VmaAllocator allocator = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;

	void operator()(VkBuffer handle) const noexcept {
		if (handle) {
			vmaDestroyBuffer(allocator, handle, allocation);
		}
	}
};

using VulkanBuffer = UniqueHandle<VkBuffer, VulkanBufferDeleter, VK_NULL_HANDLE>;

struct VulkanImageDeleter {
	VmaAllocator allocator = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;

	void operator()(VkImage handle) const noexcept {
		if (handle) {
			vmaDestroyImage(allocator, handle, allocation);
		}
	}
};

using VulkanImage = UniqueHandle<VkImage, VulkanImageDeleter, VK_NULL_HANDLE>;

struct VulkanImageViewDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkImageView handle) const noexcept {
		if (device) {
			vkDestroyImageView(device, handle, nullptr);
		}
	}
};

using VulkanImageView = UniqueHandle<VkImageView, VulkanImageViewDeleter, VK_NULL_HANDLE>;

struct VulkanSamplerDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkSampler handle) const noexcept {
		if (device) {
			vkDestroySampler(device, handle, nullptr);
		}
	}
};

using VulkanSampler = UniqueHandle<VkSampler, VulkanSamplerDeleter, VK_NULL_HANDLE>;

struct VulkanRenderPassDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkRenderPass handle) const noexcept {
		if (device) {
			vkDestroyRenderPass(device, handle, nullptr);
		}
	}
};

using VulkanRenderPass = UniqueHandle<VkRenderPass, VulkanRenderPassDeleter, VK_NULL_HANDLE>;

struct VulkanSwapchainDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkSwapchainKHR handle) const noexcept {
		if (device) {
			vkDestroySwapchainKHR(device, handle, nullptr);
		}
	}
};

using VulkanSwapchain = UniqueHandle<VkSwapchainKHR, VulkanSwapchainDeleter, VK_NULL_HANDLE>;

struct VulkanFramebufferDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkFramebuffer handle) const noexcept {
		if (device) {
			vkDestroyFramebuffer(device, handle, nullptr);
		}
	}
};

using VulkanFramebuffer = UniqueHandle<VkFramebuffer, VulkanFramebufferDeleter, VK_NULL_HANDLE>;

struct VulkanShaderModuleDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkShaderModule handle) const noexcept {
		if (device) {
			vkDestroyShaderModule(device, handle, nullptr);
		}
	}
};

using VulkanShaderModule = UniqueHandle<VkShaderModule, VulkanShaderModuleDeleter, VK_NULL_HANDLE>;

struct VulkanDescriptorSetLayoutDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkDescriptorSetLayout handle) const noexcept {
		if (device) {
			vkDestroyDescriptorSetLayout(device, handle, nullptr);
		}
	}
};

using VulkanDescriptorSetLayout = UniqueHandle<VkDescriptorSetLayout, VulkanDescriptorSetLayoutDeleter, VK_NULL_HANDLE>;

struct VulkanDescriptorPoolDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkDescriptorPool handle) const noexcept {
		if (device) {
			vkDestroyDescriptorPool(device, handle, nullptr);
		}
	}
};

using VulkanDescriptorPool = UniqueHandle<VkDescriptorPool, VulkanDescriptorPoolDeleter, VK_NULL_HANDLE>;

struct VulkanPipelineLayoutDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkPipelineLayout handle) const noexcept {
		if (device) {
			vkDestroyPipelineLayout(device, handle, nullptr);
		}
	}
};

using VulkanPipelineLayout = UniqueHandle<VkPipelineLayout, VulkanPipelineLayoutDeleter, VK_NULL_HANDLE>;

struct VulkanPipelineDeleter {
	VkDevice device = VK_NULL_HANDLE;

	void operator()(VkPipeline handle) const noexcept {
		if (device) {
			vkDestroyPipeline(device, handle, nullptr);
		}
	}
};

using VulkanPipeline = UniqueHandle<VkPipeline, VulkanPipelineDeleter, VK_NULL_HANDLE>;

} // namespace detail

} // namespace grem::graphics

#endif
