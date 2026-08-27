// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_STAGING_BUFFER_HPP
#define GREM_GRAPHICS_VULKAN_STAGING_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>

#include "VulkanError.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

namespace grem::graphics {

namespace detail {

class StagingBuffer {
public:
	StagingBuffer() noexcept = default;

	explicit StagingBuffer(VmaAllocator allocator, size_t bufferSize)
		: bufferSize(bufferSize) {
		GREM_PROFILE_FUNCTION();

		const VkBufferCreateInfo bufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkBufferCreateFlags{},
			.size = static_cast<VkDeviceSize>(bufferSize),
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		const VmaAllocationCreateInfo allocationCreateInfo{
			.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
			.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			.preferredFlags = VkMemoryPropertyFlags{},
			.memoryTypeBits = 0,
			.pool = VK_NULL_HANDLE,
			.pUserData = nullptr,
			.priority = 0.0f,
		};
		VkBuffer bufferHandle = VK_NULL_HANDLE;
		VmaAllocation allocationHandle = VK_NULL_HANDLE;
		VmaAllocationInfo allocationInfo{};
		if (const VkResult result = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &bufferHandle, &allocationHandle, &allocationInfo); result != VK_SUCCESS) {
			throw detail::VulkanError{"vmaCreateBuffer", result};
		}
#ifndef NDEBUG
		vmaSetAllocationName(allocator, allocationHandle, "StagingBuffer");
#endif
		buffer = detail::VulkanBuffer{bufferHandle, detail::VulkanBufferDeleter{allocator, allocationHandle}};
		mappedData = allocationInfo.pMappedData;
		GREM_ASSERT(mappedData);
	}

	void flush(size_t byteOffset, size_t sizeBytes) {
		GREM_PROFILE_FUNCTION();

		if (const VkResult result =
				vmaFlushAllocation(buffer.get_deleter().allocator, buffer.get_deleter().allocation, static_cast<VkDeviceSize>(byteOffset), static_cast<VkDeviceSize>(sizeBytes));
			result != VK_SUCCESS) {
			throw detail::VulkanError{"vmaFlushAllocation", result};
		}
	}

	[[nodiscard]] VkBuffer get() const noexcept {
		return buffer.get();
	}

	[[nodiscard]] byte* data() noexcept {
		return static_cast<byte*>(mappedData);
	}

	[[nodiscard]] const byte* data() const noexcept {
		return static_cast<const byte*>(mappedData);
	}

	[[nodiscard]] size_t size() const noexcept {
		return bufferSize;
	}

	[[nodiscard]] bool empty() const noexcept {
		return bufferSize == 0;
	}

private:
	detail::VulkanBuffer buffer{};
	size_t bufferSize = 0;
	void* mappedData = nullptr;
};

} // namespace detail

} // namespace grem::graphics

#endif
