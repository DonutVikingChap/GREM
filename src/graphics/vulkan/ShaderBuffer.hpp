// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_SHADER_BUFFER_HPP
#define GREM_GRAPHICS_VULKAN_SHADER_BUFFER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>

#include "VulkanError.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <utility> // std::move

namespace grem::graphics {

namespace detail {

class ShaderBuffer {
public:
	ShaderBuffer(VmaAllocator allocator, size_t capacity, VkBufferUsageFlags usage)
		: buffers(allocator, (capacity == 0) ? 0 : max(capacity, MIN_CAPACITY), usage)
		, bufferUsage(usage) {}

	~ShaderBuffer() = default;

	ShaderBuffer(const ShaderBuffer& other)
		: buffers(other.get_allocator(), (other.bufferSize == 0) ? 0 : max(other.bufferSize, MIN_CAPACITY), other.bufferUsage)
		, bufferUsage(other.bufferUsage) {
		if (other.bufferSize > 0) {
			other.buffers.flushMappedBuffer(0, other.bufferSize);
			memcpy(buffers.mappedData, other.buffers.mappedData, other.bufferSize);
			bufferSize = other.bufferSize;
		}
	}

	ShaderBuffer(ShaderBuffer&&) noexcept = default;

	ShaderBuffer& operator=(const ShaderBuffer&) = delete;
	ShaderBuffer& operator=(ShaderBuffer&&) noexcept = default;

	void clear() noexcept {
		bufferSize = 0;
		flushedRangeBegin = 0;
		flushedRangeEnd = 0;
	}

	[[nodiscard]] size_t size() const noexcept {
		return bufferSize;
	}

	[[nodiscard]] bool empty() const noexcept {
		return bufferSize == 0;
	}

	[[nodiscard]] size_t capacity() const noexcept {
		return buffers.bufferCapacity;
	}

	void reserve(size_t newCapacity, auto&& onReallocated) {
		if (newCapacity <= capacity()) {
			return;
		}

		const VmaAllocator allocator = get_allocator();
		Buffers newBuffers{allocator, max(newCapacity, MIN_CAPACITY), bufferUsage};
		if (bufferSize > 0) {
			buffers.flushMappedBuffer(0, bufferSize);
			memcpy(newBuffers.mappedData, buffers.mappedData, bufferSize);
		}
		buffers = std::move(newBuffers);
		flushedRangeBegin = 0;
		flushedRangeEnd = 0;
		onReallocated();
	}

	void resize(size_t newSize, auto&& onReallocated) {
		if (newSize > capacity()) {
			[[unlikely]];
			reserve(max(newSize, capacity() * 2), onReallocated);
		}
		bufferSize = newSize;
	}

	void assign(const ShaderBuffer& other, auto&& onReallocated) {
		if (this == &other) {
			return;
		}
		if (other.empty()) {
			clear();
		} else if (capacity() < other.bufferSize) {
			*this = ShaderBuffer{other};
			onReallocated();
		} else {
			other.buffers.flushMappedBuffer(0, other.bufferSize);
			memcpy(buffers.mappedData, other.buffers.mappedData, other.bufferSize);
			bufferSize = other.bufferSize;
			flushedRangeBegin = 0;
			flushedRangeEnd = 0;
		}
	}

	void assign(Span<const byte> data, auto&& onReallocated) {
		if (data.empty()) {
			[[unlikely]];
			clear();
			return;
		}

		reserve(data.size_bytes(), onReallocated);
		memcpy(buffers.mappedData, data.data(), data.size_bytes());
		bufferSize = data.size_bytes();
		flushedRangeBegin = 0;
		flushedRangeEnd = 0;
	}

	size_t append(Span<const byte> data, auto&& onReallocated) {
		const size_t oldSize = bufferSize;
		if (data.empty()) {
			[[unlikely]];
			return oldSize;
		}

		const size_t newSize = oldSize + data.size_bytes();
		if (newSize > capacity()) {
			[[unlikely]];
			reserve(max(newSize, capacity() * 2), onReallocated);
		}
		memcpy(static_cast<byte*>(buffers.mappedData) + oldSize, data.data(), data.size_bytes());
		bufferSize = newSize;
		return oldSize;
	}

	void write(size_t byteOffset, Span<const byte> data) { // NOLINT(readability-make-member-function-const)
		const size_t rangeBegin = byteOffset;
		const size_t rangeEnd = byteOffset + data.size_bytes();
		GREM_ASSERT(rangeEnd <= bufferSize);
		if (data.empty()) {
			[[unlikely]];
			return;
		}

		memcpy(static_cast<byte*>(buffers.mappedData) + byteOffset, data.data(), data.size_bytes());
		if (flushedRangeBegin < rangeBegin) {
			flushedRangeEnd = min(flushedRangeEnd, rangeBegin);
		} else if (flushedRangeEnd > rangeEnd) {
			flushedRangeBegin = max(flushedRangeBegin, rangeEnd);
		} else {
			flushedRangeBegin = 0;
			flushedRangeEnd = 0;
		}
	}

	void flush(VkCommandBuffer commandBuffer, VkPipelineStageFlags destinationPipelineStages, VkAccessFlags destinationAccessMask) {
		if (flushedRangeBegin > 0 || flushedRangeEnd < bufferSize) {
			if (flushedRangeBegin > 0 && flushedRangeEnd < bufferSize) {
				buffers.flushMappedBuffer(0, bufferSize);
				flushedRangeBegin = 0;
				flushedRangeEnd = bufferSize;
			} else if (flushedRangeBegin > 0) {
				buffers.flushMappedBuffer(0, flushedRangeBegin);
				flushedRangeBegin = 0;
			} else {
				buffers.flushMappedBuffer(flushedRangeEnd, bufferSize - flushedRangeEnd);
				flushedRangeEnd = bufferSize;
			}

			if (buffers.stagingBuffer) {
				const Array copyRegions{VkBufferCopy{
					.srcOffset = 0,
					.dstOffset = 0,
					.size = static_cast<VkDeviceSize>(bufferSize),
				}};
				vkCmdCopyBuffer(commandBuffer, buffers.stagingBuffer.get(), buffers.deviceLocalBuffer.get(), static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
				if (destinationPipelineStages != VkPipelineStageFlags{}) {
					const Array bufferMemoryBarriers{VkBufferMemoryBarrier{
						.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.dstAccessMask = destinationAccessMask,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.buffer = buffers.deviceLocalBuffer.get(),
						.offset = 0,
						.size = static_cast<VkDeviceSize>(bufferSize),
					}};
					vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destinationPipelineStages, VkDependencyFlags{}, 0, nullptr,
						static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(), 0, nullptr);
				}
			}
		}
	}

	[[nodiscard]] VkBuffer get() const noexcept {
		return buffers.deviceLocalBuffer.get();
	}

	[[nodiscard]] VmaAllocator get_allocator() const noexcept {
		return buffers.deviceLocalBuffer.get_deleter().allocator;
	}

private:
	static constexpr size_t MIN_CAPACITY = 256;

	struct Buffers {
		detail::VulkanBuffer deviceLocalBuffer{}; // Main GPU buffer.
		detail::VulkanBuffer stagingBuffer{};     // CPU buffer, only present if deviceLocalBuffer couldn't be made host accessible.
		void* mappedData = nullptr;               // References deviceLocalBuffer if it's host accessible, otherwise stagingBuffer.
		size_t bufferCapacity = 0;

		Buffers(VmaAllocator allocator, size_t capacity, VkBufferUsageFlags usage)
			: bufferCapacity(capacity) {
			GREM_PROFILE_FUNCTION();

			if (capacity > 0) {
				const VkBufferCreateInfo deviceLocalBufferCreateInfo{
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.pNext = nullptr,
					.flags = VkBufferCreateFlags{},
					.size = static_cast<VkDeviceSize>(capacity),
					.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
				};
				const VmaAllocationCreateInfo deviceLocalBufferAllocationCreateInfo{
					.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				             VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
					.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
					.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					.preferredFlags = VkMemoryPropertyFlags{},
					.memoryTypeBits = 0,
					.pool = VK_NULL_HANDLE,
					.pUserData = nullptr,
					.priority = 0.0f,
				};
				VkBuffer deviceLocalBufferHandle = VK_NULL_HANDLE;
				VmaAllocation deviceLocalBufferAllocationHandle = VK_NULL_HANDLE;
				VmaAllocationInfo deviceLocalBufferAllocationInfo{};
				if (const VkResult result = vmaCreateBuffer(allocator, &deviceLocalBufferCreateInfo, &deviceLocalBufferAllocationCreateInfo, &deviceLocalBufferHandle,
						&deviceLocalBufferAllocationHandle, &deviceLocalBufferAllocationInfo);
					result != VK_SUCCESS) {
					throw detail::VulkanError{"vmaCreateBuffer", result};
				}
#ifndef NDEBUG
				vmaSetAllocationName(allocator, deviceLocalBufferAllocationHandle, "ShaderBuffer.deviceLocalBuffer");
#endif
				deviceLocalBuffer = detail::VulkanBuffer{deviceLocalBufferHandle, detail::VulkanBufferDeleter{allocator, deviceLocalBufferAllocationHandle}};
				VkMemoryPropertyFlags deviceLocalBufferMemoryPropertyFlags{};
				vmaGetAllocationMemoryProperties(allocator, deviceLocalBufferAllocationHandle, &deviceLocalBufferMemoryPropertyFlags);
				if ((deviceLocalBufferMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
					mappedData = deviceLocalBufferAllocationInfo.pMappedData;
				} else {
					const VkBufferCreateInfo stagingBufferCreateInfo{
						.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
						.pNext = nullptr,
						.flags = VkBufferCreateFlags{},
						.size = static_cast<VkDeviceSize>(capacity),
						.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
						.queueFamilyIndexCount = 0,
						.pQueueFamilyIndices = nullptr,
					};
					const VmaAllocationCreateInfo stagingBufferAllocationCreateInfo{
						.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
						.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
						.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
						.preferredFlags = VkMemoryPropertyFlags{},
						.memoryTypeBits = 0,
						.pool = VK_NULL_HANDLE,
						.pUserData = nullptr,
						.priority = 0.0f,
					};
					VkBuffer stagingBufferHandle = VK_NULL_HANDLE;
					VmaAllocation stagingBufferAllocationHandle = VK_NULL_HANDLE;
					VmaAllocationInfo stagingBufferAllocationInfo{};
					if (const VkResult result = vmaCreateBuffer(allocator, &stagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &stagingBufferHandle,
							&stagingBufferAllocationHandle, &stagingBufferAllocationInfo);
						result != VK_SUCCESS) {
						throw detail::VulkanError{"vmaCreateBuffer", result};
					}
#ifndef NDEBUG
					vmaSetAllocationName(allocator, stagingBufferAllocationHandle, "ShaderBuffer.stagingBuffer");
#endif
					stagingBuffer = detail::VulkanBuffer{stagingBufferHandle, detail::VulkanBufferDeleter{allocator, stagingBufferAllocationHandle}};
					mappedData = stagingBufferAllocationInfo.pMappedData;
				}
				GREM_ASSERT(mappedData);
			} else {
				deviceLocalBuffer.get_deleter().allocator = allocator;
			}
		}

		void flushMappedBuffer(size_t byteOffset, size_t sizeBytes) const {
			const VmaAllocator allocator = deviceLocalBuffer.get_deleter().allocator;
			if (stagingBuffer) {
				if (const VkResult result = vmaFlushAllocation(allocator, stagingBuffer.get_deleter().allocation, byteOffset, sizeBytes); result != VK_SUCCESS) {
					throw detail::VulkanError{"vmaFlushAllocation", result};
				}
			} else if (deviceLocalBuffer) {
				if (const VkResult result = vmaFlushAllocation(allocator, deviceLocalBuffer.get_deleter().allocation, byteOffset, sizeBytes); result != VK_SUCCESS) {
					throw detail::VulkanError{"vmaFlushAllocation", result};
				}
			}
		}
	};

	Buffers buffers;
	size_t bufferSize = 0;
	VkBufferUsageFlags bufferUsage;
	size_t flushedRangeBegin = 0;
	size_t flushedRangeEnd = 0;
};

} // namespace detail

} // namespace grem::graphics

#endif
