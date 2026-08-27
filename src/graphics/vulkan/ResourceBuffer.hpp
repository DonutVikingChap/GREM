// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_RESOURCE_BUFFER_HPP
#define GREM_GRAPHICS_VULKAN_RESOURCE_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>

#include "VulkanError.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <utility> // std::move

namespace grem::graphics {

namespace detail {

class ResourceBuffer {
public:
	ResourceBuffer(VmaAllocator allocator, size_t capacity, VkBufferUsageFlags usage)
		: buffer(allocator, capacity, usage)
		, bufferUsage(usage) {}

	// Returns true if the buffer was resized.
	bool upload(VkCommandBuffer commandBuffer, size_t oldBufferRangeBegin, size_t oldBufferRangeEnd, size_t newRequiredBufferSizeBytes, size_t byteOffset, size_t sizeBytes,
		VkBuffer stagingBuffer, VkPipelineStageFlags destinationPipelineStages, VkAccessFlags destinationAccessMask, FunctionView<VkCommandBuffer()> submitAndAwaitCommands) {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(newRequiredBufferSizeBytes >= byteOffset + sizeBytes);
		bool resizedBuffer = false;
		if (newRequiredBufferSizeBytes > capacity()) {
			Buffer newBuffer{buffer.deviceLocalBuffer.get_deleter().allocator, max(newRequiredBufferSizeBytes, capacity() * 2), bufferUsage};
			if (oldBufferRangeBegin < oldBufferRangeEnd) {
				const Array bufferMemoryBarriers{VkBufferMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = buffer.deviceLocalBuffer.get(),
					.offset = static_cast<VkDeviceSize>(oldBufferRangeBegin),
					.size = static_cast<VkDeviceSize>(oldBufferRangeEnd - oldBufferRangeBegin),
				}};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr,
					static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), 0, nullptr);
				const Array copyRegions{VkBufferCopy{
					.srcOffset = static_cast<VkDeviceSize>(oldBufferRangeBegin),
					.dstOffset = static_cast<VkDeviceSize>(oldBufferRangeBegin),
					.size = static_cast<VkDeviceSize>(oldBufferRangeEnd - oldBufferRangeBegin),
				}};
				vkCmdCopyBuffer(commandBuffer, buffer.deviceLocalBuffer.get(), newBuffer.deviceLocalBuffer.get(), static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
			}
			commandBuffer = submitAndAwaitCommands();
			buffer = std::move(newBuffer);
			resizedBuffer = true;
		} else {
			const Array bufferMemoryBarriers{VkBufferMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = buffer.deviceLocalBuffer.get(),
				.offset = static_cast<VkDeviceSize>(byteOffset),
				.size = static_cast<VkDeviceSize>(sizeBytes),
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr,
				static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), 0, nullptr);
		}

		const Array copyRegions{VkBufferCopy{
			.srcOffset = 0,
			.dstOffset = static_cast<VkDeviceSize>(byteOffset),
			.size = static_cast<VkDeviceSize>(sizeBytes),
		}};
		vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer.deviceLocalBuffer.get(), static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
		if (destinationPipelineStages != VkPipelineStageFlags{}) {
			const Array bufferMemoryBarriers{VkBufferMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = destinationAccessMask,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = buffer.deviceLocalBuffer.get(),
				.offset = static_cast<VkDeviceSize>(byteOffset),
				.size = static_cast<VkDeviceSize>(sizeBytes),
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destinationPipelineStages, VkDependencyFlags{}, 0, nullptr,
				static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), 0, nullptr);
		}
		return resizedBuffer;
	}

	[[nodiscard]] size_t capacity() const noexcept {
		return buffer.bufferCapacity;
	}

	[[nodiscard]] VkBuffer get() const noexcept {
		return buffer.deviceLocalBuffer.get();
	}

private:
	struct Buffer {
		detail::VulkanBuffer deviceLocalBuffer{};
		size_t bufferCapacity = 0;

		Buffer(VmaAllocator allocator, size_t capacity, VkBufferUsageFlags usage)
			: bufferCapacity(capacity) {
			GREM_PROFILE_FUNCTION();

			if (capacity > 0) {
				const VkBufferCreateInfo bufferCreateInfo{
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.pNext = nullptr,
					.flags = VkBufferCreateFlags{},
					.size = static_cast<VkDeviceSize>(capacity),
					.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
				};
				const VmaAllocationCreateInfo allocationCreateInfo{
					.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
					.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
					.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					.preferredFlags = VkMemoryPropertyFlags{},
					.memoryTypeBits = 0,
					.pool = VK_NULL_HANDLE,
					.pUserData = nullptr,
					.priority = 0.0f,
				};
				VkBuffer bufferHandle = VK_NULL_HANDLE;
				VmaAllocation allocationHandle = VK_NULL_HANDLE;
				if (const VkResult result = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &bufferHandle, &allocationHandle, nullptr); result != VK_SUCCESS) {
					throw detail::VulkanError{"vmaCreateBuffer", result};
				}
#ifndef NDEBUG
				vmaSetAllocationName(allocator, allocationHandle, "ResourceBuffer");
#endif
				deviceLocalBuffer = detail::VulkanBuffer{bufferHandle, detail::VulkanBufferDeleter{allocator, allocationHandle}};
			}
		}
	};

	Buffer buffer;
	VkBufferUsageFlags bufferUsage;
};

} // namespace detail

} // namespace grem::graphics

#endif
