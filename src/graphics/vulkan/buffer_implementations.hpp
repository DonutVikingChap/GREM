// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_BUFFER_IMPLEMENTATIONS_HPP
#define GREM_GRAPHICS_VULKAN_BUFFER_IMPLEMENTATIONS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "DeviceImplementation.hpp"
#include "MeshImplementation.hpp"
#include "ShaderBuffer.hpp"
#include "TextureImplementation.hpp"
#include "TextureResources.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <memory>  // std::allocator_traits
#include <utility> // std::move, std::exchange

namespace grem::graphics {

struct DescriptorSetUpdate {
	Arena<4096> arena;
	Buffer<VkWriteDescriptorSet, ArenaAllocator<VkWriteDescriptorSet>> descriptorWrites{&arena};

	void reserve(size_t newCapacity) {
		descriptorWrites.reserve(newCapacity);
	}

	template <typename T>
	T* insert(const T& value) {
		static_assert(trivially_destructible<T>);
		ArenaAllocator<T> allocator{&arena};
		T* const result = allocator.allocate(1);
		std::allocator_traits<ArenaAllocator<T>>::construct(allocator, result, value);
		return result;
	}

	template <typename T>
	T* insertArray(Span<const T> values) {
		static_assert(trivially_destructible<T>);
		if (values.empty()) {
			[[unlikely]];
			return nullptr;
		}
		ArenaAllocator<T> allocator{&arena};
		T* const result = allocator.allocate(values.size());
		for (size_t i = 0; i < values.size(); ++i) {
			std::allocator_traits<ArenaAllocator<T>>::construct(allocator, result + i, values[i]);
		}
		return result;
	}

	template <typename T>
	T* createArray(size_t size) {
		static_assert(trivially_destructible<T>);
		if (size == 0) {
			[[unlikely]];
			return nullptr;
		}
		ArenaAllocator<T> allocator{&arena};
		T* const result = allocator.allocate(size);
		for (size_t i = 0; i < size; ++i) {
			std::allocator_traits<ArenaAllocator<T>>::construct(allocator, result + i);
		}
		return result;
	}

	void push(const VkWriteDescriptorSet& descriptorWrite) {
		descriptorWrites.push_back(descriptorWrite);
	}

	void apply(VkDevice device) const {
		if (!descriptorWrites.empty()) {
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}
};

struct UniformBufferImplementation : detail::ReusableCopyOnWriteResourceBase<UniformBufferImplementation> {
	struct UninitializedTag {};

	[[nodiscard]] static SharedPointer<UniformBufferImplementation> create(Device& device, BufferLayoutReference bufferLayout, size_t bufferSize, size_t textureParameterCount) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<UniformBufferImplementation>::create(device, bufferLayout, bufferSize, textureParameterCount);
	}

	[[nodiscard]] static SharedPointer<UniformBufferImplementation> cloneUninitialized(const UniformBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<UniformBufferImplementation>::create(implementation, UninitializedTag{});
	}

	UniformBufferImplementation(Device& device, BufferLayoutReference bufferLayout, size_t bufferSize, size_t textureParameterCount)
		: device(device)
		, bufferLayout(bufferLayout)
		, uniformBuffer(device.get()->allocator.get(), bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
		, textures(textureParameterCount, nullptr) {
		uniformBuffer.resize(bufferSize, []() -> void {});
	}

	UniformBufferImplementation(const UniformBufferImplementation& other, UninitializedTag)
		: UniformBufferImplementation(other.device, other.bufferLayout, other.uniformBuffer.size(), other.textures.size()) {}

	void invalidateContents() noexcept {
		invalidateContentsInBufferSet(descriptorSetUpToDate);
	}

	void invalidateContentsInBufferSet(bool& destinationDescriptorSetBindingUpToDate) noexcept {
		if (anyOf(textures, [](const SharedPointer<TextureImplementation>& texture) -> bool { return static_cast<bool>(texture); })) {
			for (SharedPointer<TextureImplementation>& texture : textures) {
				texture = {};
			}
			destinationDescriptorSetBindingUpToDate = false;
		}
	}

	void upload(Span<const byte> newParameterValuesBytes, Span<const SharedPointer<TextureImplementation>> newTextures) {
		uploadInBufferSet(newParameterValuesBytes, newTextures, descriptorSetUpToDate);
	}

	void uploadInBufferSet(Span<const byte> newParameterValuesBytes, Span<const SharedPointer<TextureImplementation>> newTextures, bool& destinationDescriptorSetBindingUpToDate) {
		GREM_ASSERT(newTextures.size() == textures.size());
		uniformBuffer.write(0, newParameterValuesBytes);
		if (!equal(textures, newTextures)) {
			textures.assign_range(newTextures);
			destinationDescriptorSetBindingUpToDate = false;
		}
	}

	void flush() {
		flushUniformBuffer();

		if (!descriptorSet) {
			allocateDescriptorSet();
		}

		if (!descriptorSetUpToDate) {
			DescriptorSetUpdate descriptorSetUpdate{};
			getDescriptorSetUpdate(descriptorSetUpdate, descriptorSet, 0);
			descriptorSetUpdate.apply(device.get()->logicalDevice.get());
			descriptorSetUpToDate = true;
		}
	}

	void flushInBufferSet(DescriptorSetUpdate& descriptorSetUpdate, VkDescriptorSet destinationDescriptorSet, uint32_t destinationBindingOffset,
		bool& destinationDescriptorSetBindingUpToDate) {
		GREM_ASSERT(destinationDescriptorSet);

		flushUniformBuffer();

		if (!destinationDescriptorSetBindingUpToDate) {
			getDescriptorSetUpdate(descriptorSetUpdate, destinationDescriptorSet, destinationBindingOffset);
			destinationDescriptorSetBindingUpToDate = true;
		}
	}

	[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
		return descriptorSet;
	}

private:
	void flushUniformBuffer() {
		uniformBuffer.flush(device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT);
	}

	void allocateDescriptorSet() {
		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		detail::VulkanDescriptorSetLayout& descriptorSetLayout = device.get()->bufferDescriptorSetLayoutMap[bufferLayout.nameCRC32];
		if (!descriptorSetLayout) {
			SmallBuffer<VkDescriptorSetLayoutBinding, 16> bindings{};
			if (uniformBuffer.capacity() > 0) {
				bindings.push_back(VkDescriptorSetLayoutBinding{
					.binding = static_cast<uint32_t>(bindings.size()),
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = nullptr,
				});
			}
			for (const ParameterDescription& parameterDescription : bufferLayout.as<UniformBufferLayoutReference>().parameterDescriptions) {
				if (isTextureParameter(parameterDescription.type)) {
					bindings.push_back(VkDescriptorSetLayoutBinding{
						.binding = static_cast<uint32_t>(bindings.size()),
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = static_cast<uint32_t>(max(parameterDescription.arrayElementCount, size_t{1})),
						.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						.pImmutableSamplers = nullptr,
					});
				}
			}
			const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkDescriptorSetLayoutCreateFlags{},
				.bindingCount = static_cast<uint32_t>(bindings.size()),
				.pBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.data()),
			};
			VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
			if (const VkResult result = vkCreateDescriptorSetLayout(deviceHandle, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutHandle); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateDescriptorSetLayout", result};
			}
			descriptorSetLayout = detail::VulkanDescriptorSetLayout{descriptorSetLayoutHandle, detail::VulkanDescriptorSetLayoutDeleter{deviceHandle}};
		}

		SmallBuffer<VkDescriptorPoolSize, 16> poolSizes{};
		if (uniformBuffer.capacity() > 0) {
			poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1});
		}
		for (const ParameterDescription& parameterDescription : bufferLayout.as<UniformBufferLayoutReference>().parameterDescriptions) {
			if (isTextureParameter(parameterDescription.type)) {
				poolSizes.push_back(VkDescriptorPoolSize{
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = static_cast<uint32_t>(max(parameterDescription.arrayElementCount, size_t{1})),
				});
			}
		}
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

		const Array setLayouts{descriptorSetLayout.get()};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}
	}

	void getDescriptorSetUpdate(DescriptorSetUpdate& descriptorSetUpdate, VkDescriptorSet destinationDescriptorSet, uint32_t destinationBindingOffset) const {
		uint32_t binding = destinationBindingOffset;
		if (uniformBuffer.capacity() > 0) {
			descriptorSetUpdate.push(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = destinationDescriptorSet,
				.dstBinding = binding,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = descriptorSetUpdate.insert(VkDescriptorBufferInfo{
					.buffer = uniformBuffer.get(),
					.offset = 0,
					.range = uniformBuffer.capacity(),
				}),
				.pTexelBufferView = nullptr,
			});
			++binding;
		}

		size_t textureIndex = 0;
		for (const ParameterDescription& parameterDescription : bufferLayout.as<UniformBufferLayoutReference>().parameterDescriptions) {
			if (isTextureParameter(parameterDescription.type)) {
				const size_t textureParameterCount = max(parameterDescription.arrayElementCount, size_t{1});
				VkDescriptorImageInfo* const imageInfos = descriptorSetUpdate.createArray<VkDescriptorImageInfo>(textureParameterCount);
				for (size_t textureParameterIndex = 0; textureParameterIndex < textureParameterCount; ++textureParameterIndex) {
					const SharedPointer<TextureImplementation>& texture = textures[textureIndex];
					imageInfos[textureParameterIndex] = {
						.sampler = texture->object.get<detail::TextureResources>().sampler,
						.imageView = texture->object.get<detail::TextureResources>().samplerImageView,
						.imageLayout = texture->imageLayout,
					};
					++textureIndex;
				}
				descriptorSetUpdate.push(VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = destinationDescriptorSet,
					.dstBinding = binding,
					.dstArrayElement = 0,
					.descriptorCount = static_cast<uint32_t>(textureParameterCount),
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = imageInfos,
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr,
				});
				++binding;
			}
		}
	}

	Device& device;
	BufferLayoutReference bufferLayout;
	detail::ShaderBuffer uniformBuffer;
	Allocation<SharedPointer<TextureImplementation>> textures;
	mutable DescriptorSetUpdate descriptorSetUpdate{};
	detail::VulkanDescriptorPool descriptorPool{};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	bool descriptorSetUpToDate = false;
};

struct StorageBufferImplementation : detail::ReusableCopyOnWriteResourceBase<StorageBufferImplementation> {
	struct UninitializedTag {};

	static constexpr size_t MIN_CAPACITY = 512;

	[[nodiscard]] static SharedPointer<StorageBufferImplementation> create(Device& device, BufferLayoutReference bufferLayout, size_t capacity) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<StorageBufferImplementation>::create(device, bufferLayout, capacity);
	}

	[[nodiscard]] static SharedPointer<StorageBufferImplementation> clone(const StorageBufferImplementation& implementation, size_t capacity) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<StorageBufferImplementation>::create(implementation, capacity);
	}

	[[nodiscard]] static SharedPointer<StorageBufferImplementation> cloneUninitialized(const StorageBufferImplementation& implementation, size_t capacity) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<StorageBufferImplementation>::create(implementation, capacity, UninitializedTag{});
	}

	StorageBufferImplementation(Device& device, BufferLayoutReference bufferLayout, size_t capacity)
		: device(device)
		, bufferLayout(bufferLayout)
		, storageBuffer(device.get()->allocator.get(), max(capacity, MIN_CAPACITY), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {}

	StorageBufferImplementation(const StorageBufferImplementation& other, size_t capacity)
		: StorageBufferImplementation(other, capacity, UninitializedTag{}) {
		storageBuffer.assign(other.storageBuffer, []() -> void {});
	}

	StorageBufferImplementation(const StorageBufferImplementation& other, size_t capacity, UninitializedTag)
		: StorageBufferImplementation(other.device, other.bufferLayout, capacity) {}

	void clear() noexcept {
		storageBuffer.clear();
	}

	void reserve(size_t newCapacity) {
		reserveInBufferSet(newCapacity, descriptorSetUpToDate);
	}

	void reserveInBufferSet(size_t newCapacity, bool& destinationDescriptorSetBindingUpToDate) {
		storageBuffer.reserve(newCapacity, [&]() -> void { destinationDescriptorSetBindingUpToDate = false; });
	}

	void resize(size_t newSize) {
		resizeInBufferSet(newSize, descriptorSetUpToDate);
	}

	void resizeInBufferSet(size_t newSize, bool& destinationDescriptorSetBindingUpToDate) {
		storageBuffer.resize(newSize, [&]() -> void { destinationDescriptorSetBindingUpToDate = false; });
	}

	void assign(const StorageBufferImplementation& other) {
		assignInBufferSet(other, descriptorSetUpToDate);
	}

	void assignInBufferSet(const StorageBufferImplementation& other, bool& destinationDescriptorSetBindingUpToDate) {
		storageBuffer.assign(other.storageBuffer, [&]() -> void { destinationDescriptorSetBindingUpToDate = false; });
	}

	void write(size_t byteOffset, Span<const byte> data) {
		storageBuffer.write(byteOffset, data);
	}

	void flush() {
		flushStorageBuffer();

		if (!descriptorSet) {
			allocateDescriptorSet();
		}

		if (!descriptorSetUpToDate) {
			DescriptorSetUpdate descriptorSetUpdate{};
			getDescriptorSetUpdate(descriptorSetUpdate, descriptorSet, 0);
			descriptorSetUpdate.apply(device.get()->logicalDevice.get());
			descriptorSetUpToDate = true;
		}
	}

	void flushInBufferSet(DescriptorSetUpdate& descriptorSetUpdate, VkDescriptorSet destinationDescriptorSet, uint32_t destinationBindingOffset,
		bool& destinationDescriptorSetBindingUpToDate) {
		GREM_ASSERT(destinationDescriptorSet);

		flushStorageBuffer();

		if (!destinationDescriptorSetBindingUpToDate) {
			getDescriptorSetUpdate(descriptorSetUpdate, destinationDescriptorSet, destinationBindingOffset);
			destinationDescriptorSetBindingUpToDate = true;
		}
	}

	[[nodiscard]] size_t size() const noexcept {
		return storageBuffer.size();
	}

	[[nodiscard]] size_t capacity() const noexcept {
		return storageBuffer.capacity();
	}

	[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
		return descriptorSet;
	}

private:
	void flushStorageBuffer() {
		storageBuffer.flush(device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
	}

	void allocateDescriptorSet() {
		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		detail::VulkanDescriptorSetLayout& descriptorSetLayout = device.get()->bufferDescriptorSetLayoutMap[bufferLayout.nameCRC32];
		if (!descriptorSetLayout) {
			const Array bindings{VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}};
			const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkDescriptorSetLayoutCreateFlags{},
				.bindingCount = static_cast<uint32_t>(bindings.size()),
				.pBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.data()),
			};
			VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
			if (const VkResult result = vkCreateDescriptorSetLayout(deviceHandle, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutHandle); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateDescriptorSetLayout", result};
			}
			descriptorSetLayout = detail::VulkanDescriptorSetLayout{descriptorSetLayoutHandle, detail::VulkanDescriptorSetLayoutDeleter{deviceHandle}};
		}

		const Array poolSizes{VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1}};
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

		const Array setLayouts{descriptorSetLayout.get()};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}
	}

	void getDescriptorSetUpdate(DescriptorSetUpdate& descriptorSetUpdate, VkDescriptorSet destinationDescriptorSet, uint32_t destinationBindingOffset) {
		GREM_PROFILE_FUNCTION();

		descriptorSetUpdate.push(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = destinationDescriptorSet,
			.dstBinding = destinationBindingOffset,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = descriptorSetUpdate.insert(VkDescriptorBufferInfo{
				.buffer = storageBuffer.get(),
				.offset = 0,
				.range = storageBuffer.capacity(),
			}),
			.pTexelBufferView = nullptr,
		});
	}

	Device& device;
	BufferLayoutReference bufferLayout;
	detail::ShaderBuffer storageBuffer;
	detail::VulkanDescriptorPool descriptorPool{};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	bool descriptorSetUpToDate = false;
};

struct BufferSetImplementation : detail::ReusableCopyOnWriteResourceBase<BufferSetImplementation> {
	[[nodiscard]] static SharedPointer<BufferSetImplementation> create(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<BufferSetImplementation>::create(device, bufferSetLayout, std::move(buffers));
	}

	[[nodiscard]] static SharedPointer<BufferSetImplementation> clone(const BufferSetImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<BufferSetImplementation>::create(implementation);
	}

	BufferSetImplementation(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers)
		: BufferSetImplementation(device, bufferSetLayout) {
		this->buffers = std::move(buffers); // NOLINT(cppcoreguidelines-prefer-member-initializer)
	}

	BufferSetImplementation(const BufferSetImplementation& other)
		: BufferSetImplementation(other.device, other.bufferSetLayout, other.buffers) {}

	void assign(const BufferSetImplementation& other) {
		GREM_ASSERT(bufferSetLayout == other.bufferSetLayout);
		GREM_ASSERT(other.buffers.size() == bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts.size());

		buffers = other.buffers;
		descriptorSetBindingsUpToDate.fill(false);
	}

	void setBuffer(size_t bufferIndex, SharedPointer<void> newBuffer) {
		buffers[bufferIndex] = std::move(newBuffer);
		descriptorSetBindingsUpToDate[bufferIndex] = false;
	}

	void setBuffers(Span<SharedPointer<void>> newBuffers) {
		GREM_ASSERT(newBuffers.size() == bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts.size());

		for (size_t i = 0; i < newBuffers.size(); ++i) {
			buffers[i] = std::move(newBuffers[i]);
		}
		descriptorSetBindingsUpToDate.fill(false);
	}

	void uploadBytesToUniformBuffer(size_t bufferIndex, Span<const byte> newParameterValuesBytes, Span<const SharedPointer<TextureImplementation>> newTextures) {
		GREM_ASSERT(bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts[bufferIndex].is<UniformBufferLayoutReference>());

		SharedPointer<UniformBufferImplementation> uniformBufferHandle = static_pointer_cast<UniformBufferImplementation>(std::move(buffers[bufferIndex]));
		try {
			detail::ensureExclusiveResourceAccess(
				uniformBufferHandle, [&]() -> SharedPointer<UniformBufferImplementation> { return UniformBufferImplementation::cloneUninitialized(*uniformBufferHandle); },
				[&](UniformBufferImplementation&) -> void {});

			uniformBufferHandle->uploadInBufferSet(newParameterValuesBytes, newTextures, descriptorSetBindingsUpToDate[bufferIndex]);
		} catch (...) {
			buffers[bufferIndex] = std::move(uniformBufferHandle);
			throw;
		}
		buffers[bufferIndex] = std::move(uniformBufferHandle);
	}

	void resizeStorageBuffer(size_t bufferIndex, size_t newSize, bool ignorePreviousContents) {
		GREM_ASSERT(bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts[bufferIndex].is<StorageBufferLayoutReference>());

		if (newSize <= static_cast<StorageBufferImplementation*>(buffers[bufferIndex].get())->size()) {
			return;
		}

		SharedPointer<StorageBufferImplementation> storageBufferHandle = static_pointer_cast<StorageBufferImplementation>(std::move(buffers[bufferIndex]));
		try {
			detail::ensureExclusiveResourceAccess(
				storageBufferHandle,
				[&]() -> SharedPointer<StorageBufferImplementation> {
					if (!ignorePreviousContents && newSize != 0) {
						return StorageBufferImplementation::clone(*storageBufferHandle, max(newSize, storageBufferHandle->size()));
					}
					return StorageBufferImplementation::cloneUninitialized(*storageBufferHandle, max(newSize, storageBufferHandle->size()));
				},
				[&](StorageBufferImplementation& oldBuffer) -> void {
					oldBuffer.clear();
					oldBuffer.reserve(max(newSize, storageBufferHandle->size()));
					if (!ignorePreviousContents && newSize != 0) {
						oldBuffer.assign(*storageBufferHandle);
					}
				});

			storageBufferHandle->resizeInBufferSet(newSize, descriptorSetBindingsUpToDate[bufferIndex]);
		} catch (...) {
			buffers[bufferIndex] = std::move(storageBufferHandle);
			throw;
		}
		buffers[bufferIndex] = std::move(storageBufferHandle);
	}

	void writeBytesToStorageBuffer(size_t bufferIndex, size_t byteOffset, Span<const byte> data, bool ignorePreviousContents) {
		GREM_ASSERT(bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts[bufferIndex].is<StorageBufferLayoutReference>());

		SharedPointer<StorageBufferImplementation> storageBufferHandle = static_pointer_cast<StorageBufferImplementation>(std::move(buffers[bufferIndex]));
		try {
			const size_t writtenRangeEnd = byteOffset + data.size_bytes();
			detail::ensureExclusiveResourceAccess(
				storageBufferHandle,
				[&]() -> SharedPointer<StorageBufferImplementation> {
					if (!ignorePreviousContents && (byteOffset > 0 || writtenRangeEnd < storageBufferHandle->size())) {
						return StorageBufferImplementation::clone(*storageBufferHandle, max(writtenRangeEnd, storageBufferHandle->size()));
					}
					return StorageBufferImplementation::cloneUninitialized(*storageBufferHandle, max(writtenRangeEnd, storageBufferHandle->size()));
				},
				[&](StorageBufferImplementation& oldBuffer) -> void {
					oldBuffer.clear();
					oldBuffer.reserve(max(writtenRangeEnd, storageBufferHandle->size()));
					if (!ignorePreviousContents && (byteOffset > 0 || writtenRangeEnd < storageBufferHandle->size())) {
						oldBuffer.assign(*storageBufferHandle);
					}
				});

			if (writtenRangeEnd > storageBufferHandle->size()) {
				storageBufferHandle->resizeInBufferSet(writtenRangeEnd, descriptorSetBindingsUpToDate[bufferIndex]);
			}
			storageBufferHandle->write(byteOffset, data);
		} catch (...) {
			buffers[bufferIndex] = std::move(storageBufferHandle);
			throw;
		}
		buffers[bufferIndex] = std::move(storageBufferHandle);
	}

	void flush() {
		const Span<const BufferLayoutReference> bufferLayouts = bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		GREM_ASSERT(buffers.size() == bufferLayouts.size());

		DescriptorSetUpdate descriptorSetUpdate{};

		for (size_t i = 0; i < bufferLayouts.size(); ++i) {
			GREM_ASSERT(buffers[i]);
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					static_cast<UniformBufferImplementation*>(buffers[i].get())
						->flushInBufferSet(descriptorSetUpdate, descriptorSet, bindingOffsets[i], descriptorSetBindingsUpToDate[i]);
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					static_cast<StorageBufferImplementation*>(buffers[i].get())
						->flushInBufferSet(descriptorSetUpdate, descriptorSet, bindingOffsets[i], descriptorSetBindingsUpToDate[i]);
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}

		descriptorSetUpdate.apply(device.get()->logicalDevice.get());
	}

	[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
		return descriptorSet;
	}

private:
	BufferSetImplementation(Device& device, BufferLayoutReference bufferSetLayout)
		: device(device)
		, bufferSetLayout(bufferSetLayout) {
		const Span<const BufferLayoutReference> bufferLayouts = bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		bindingOffsets.resize(bufferLayouts.size());
		descriptorSetBindingsUpToDate.resize(bufferLayouts.size(), false);

		uint32_t bindingOffset = 0;
		for (size_t i = 0; i < bufferLayouts.size(); ++i) {
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					size_t textureParameterCount = 0;
					bool hasAnyNonTextureParameter = false;
					for (const ParameterDescription& parameterDescription : uniformBufferLayout.parameterDescriptions) {
						if (isTextureParameter(parameterDescription.type)) {
							textureParameterCount += max(parameterDescription.arrayElementCount, size_t{1});
						} else {
							hasAnyNonTextureParameter = true;
						}
					}
					bindingOffsets[i] = bindingOffset;
					bindingOffset += static_cast<uint32_t>(hasAnyNonTextureParameter) + static_cast<uint32_t>(textureParameterCount);
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					bindingOffsets[i] = bindingOffset;
					++bindingOffset;
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}

		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		detail::VulkanDescriptorSetLayout& descriptorSetLayout = device.get()->bufferDescriptorSetLayoutMap[bufferSetLayout.nameCRC32];
		if (!descriptorSetLayout) {
			SmallBuffer<VkDescriptorSetLayoutBinding, 16> bindings{};
			for (const BufferLayoutReference& bufferLayout : bufferLayouts) {
				GREM_MATCH(bufferLayout) {
					GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
						size_t textureParameterCount = 0;
						bool hasAnyNonTextureParameter = false;
						for (const ParameterDescription& parameterDescription : uniformBufferLayout.parameterDescriptions) {
							if (isTextureParameter(parameterDescription.type)) {
								textureParameterCount += max(parameterDescription.arrayElementCount, size_t{1});
							} else {
								hasAnyNonTextureParameter = true;
							}
						}
						if (hasAnyNonTextureParameter) {
							bindings.push_back(VkDescriptorSetLayoutBinding{
								.binding = static_cast<uint32_t>(bindings.size()),
								.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								.descriptorCount = 1,
								.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
								.pImmutableSamplers = nullptr,
							});
						}
						for (size_t i = 0; i < textureParameterCount; ++i) {
							bindings.push_back(VkDescriptorSetLayoutBinding{
								.binding = static_cast<uint32_t>(bindings.size()),
								.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								.descriptorCount = 1,
								.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
								.pImmutableSamplers = nullptr,
							});
						}
						break;
					}
					GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
						bindings.push_back(VkDescriptorSetLayoutBinding{
							.binding = static_cast<uint32_t>(bindings.size()),
							.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							.descriptorCount = 1,
							.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							.pImmutableSamplers = nullptr,
						});
						break;
					}
					GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
						unreachable();
					}
				}
			}
			const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkDescriptorSetLayoutCreateFlags{},
				.bindingCount = static_cast<uint32_t>(bindings.size()),
				.pBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.data()),
			};
			VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
			if (const VkResult result = vkCreateDescriptorSetLayout(deviceHandle, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutHandle); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateDescriptorSetLayout", result};
			}
			descriptorSetLayout = detail::VulkanDescriptorSetLayout{descriptorSetLayoutHandle, detail::VulkanDescriptorSetLayoutDeleter{deviceHandle}};
		}

		SmallBuffer<VkDescriptorPoolSize, 16> poolSizes{};
		for (const BufferLayoutReference& bufferLayout : bufferLayouts) {
			GREM_MATCH(bufferLayout) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					size_t textureParameterCount = 0;
					bool hasAnyNonTextureParameter = false;
					for (const ParameterDescription& parameterDescription : uniformBufferLayout.parameterDescriptions) {
						if (isTextureParameter(parameterDescription.type)) {
							textureParameterCount += max(parameterDescription.arrayElementCount, size_t{1});
						} else {
							hasAnyNonTextureParameter = true;
						}
					}
					if (hasAnyNonTextureParameter) {
						poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1});
					}
					for (size_t i = 0; i < textureParameterCount; ++i) {
						poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1});
					}
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1});
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

		const Array setLayouts{descriptorSetLayout.get()};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}
	}

	Device& device;
	BufferLayoutReference bufferSetLayout;
	Allocation<uint32_t> bindingOffsets{};
	Allocation<bool> descriptorSetBindingsUpToDate{};
	detail::VulkanDescriptorPool descriptorPool{};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	Allocation<SharedPointer<void>> buffers{};
};

struct InstanceBufferImplementation : detail::ReusableCopyOnWriteResourceBase<InstanceBufferImplementation> {
	struct UninitializedTag {};

	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> create(Device& device, size_t instanceSize, uint32_t instanceCapacity) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<InstanceBufferImplementation>::create(device, instanceSize, instanceCapacity);
	}

	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> clone(const InstanceBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<InstanceBufferImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> cloneUninitialized(const InstanceBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<InstanceBufferImplementation>::create(implementation, UninitializedTag{});
	}

	InstanceBufferImplementation(Device& device, size_t instanceSize, uint32_t instanceCapacity)
		: device(device)
		, instanceSize(instanceSize)
		, instanceStride(detail::convertFloatCountToVec4Count(instanceSize / sizeof(float)) * (sizeof(float) * 4))
		, instanceBuffer(device.get()->allocator.get(), static_cast<size_t>(instanceCapacity) * instanceStride, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
		allocateDescriptorSet();
	}

	~InstanceBufferImplementation() = default;

	InstanceBufferImplementation(const InstanceBufferImplementation& other)
		: device(other.device)
		, instanceSize(other.instanceSize)
		, instanceStride(other.instanceStride)
		, instanceBuffer(other.instanceBuffer) {
		allocateDescriptorSet();
	}

	InstanceBufferImplementation(const InstanceBufferImplementation& other, UninitializedTag)
		: InstanceBufferImplementation(other.device, other.instanceSize, other.size()) {}

	void clear() noexcept {
		instanceBuffer.clear();
	}

	void reserve(uint32_t newCapacity) {
		instanceBuffer.reserve(static_cast<size_t>(newCapacity) * instanceStride, [&]() -> void { descriptorSetUpToDate = false; });
	}

	void resize(uint32_t newSize) {
		instanceBuffer.resize(static_cast<size_t>(newSize) * instanceStride, [&]() -> void { descriptorSetUpToDate = false; });
	}

	void assign(const InstanceBufferImplementation& other) {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return;
		}
		instanceBuffer.assign(other.instanceBuffer, [&]() -> void { descriptorSetUpToDate = false; });
		instanceSize = other.instanceSize;
		instanceStride = other.instanceStride;
	}

	void write(uint32_t instanceOffset, StridedSpan<const byte> instancesData) {
		size_t outputByteOffset = static_cast<size_t>(instanceOffset) * instanceStride;
		if (instanceSize == instanceStride && instanceSize == instancesData.stride()) {
			instanceBuffer.write(outputByteOffset, Span{instancesData.base(), instancesData.size() * instanceSize});
		} else {
			const auto end = instancesData.end();
			for (auto it = instancesData.begin(); it != end; ++it) {
				instanceBuffer.write(outputByteOffset, Span{it.base(), instanceSize});
				outputByteOffset += instanceStride;
			}
		}
	}

	void flush() {
		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		const VkCommandBuffer commandBuffer = device.get()->getGraphicsCommandBuffer();
		instanceBuffer.flush(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		if (!descriptorSetUpToDate && instanceBuffer.get()) {
			const VkDescriptorBufferInfo descriptorBufferInfo{
				.buffer = instanceBuffer.get(),
				.offset = 0,
				.range = instanceBuffer.capacity(),
			};
			const Array descriptorWrites{VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = descriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &descriptorBufferInfo,
				.pTexelBufferView = nullptr,
			}};
			vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			descriptorSetUpToDate = true;
		}
	}

	[[nodiscard]] uint32_t size() const noexcept {
		return static_cast<uint32_t>(instanceBuffer.size() / instanceStride);
	}

	[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
		return descriptorSet;
	}

private:
	void allocateDescriptorSet() {
		GREM_PROFILE_FUNCTION();

		const VkDevice deviceHandle = device.get()->logicalDevice.get();

		const Array poolSizes{VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1}};
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

		const Array setLayouts{device.get()->instanceOrDrawCommandBufferDescriptorSetLayout.get()};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}
	}

	Device& device;
	size_t instanceSize;
	size_t instanceStride;
	detail::ShaderBuffer instanceBuffer;
	detail::VulkanDescriptorPool descriptorPool{};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	bool descriptorSetUpToDate = false;
};

struct DrawCommandBufferImplementation : detail::ReusableCopyOnWriteResourceBase<DrawCommandBufferImplementation> {
	struct UninitializedTag {};

	struct InstanceRange {
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;
		uint32_t count;
		uint32_t drawCommandOffset;
	};

	[[nodiscard]] static SharedPointer<DrawCommandBufferImplementation> create(Device& device) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<DrawCommandBufferImplementation>::create(device);
	}

	[[nodiscard]] static SharedPointer<DrawCommandBufferImplementation> clone(const DrawCommandBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<DrawCommandBufferImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<DrawCommandBufferImplementation> cloneUninitialized(const DrawCommandBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<DrawCommandBufferImplementation>::create(implementation, UninitializedTag{});
	}

	explicit DrawCommandBufferImplementation(Device& device)
		: device(device)
		, drawCommandBuffer(device.get()->allocator.get(), 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
		allocateDescriptorSet();
	}

	~DrawCommandBufferImplementation() = default;

	DrawCommandBufferImplementation(const DrawCommandBufferImplementation& other)
		: device(other.device)
		, statistics(other.statistics)
		, instanceRanges(other.instanceRanges)
		, drawCommandBuffer(other.drawCommandBuffer) {
		allocateDescriptorSet();
	}

	DrawCommandBufferImplementation(const DrawCommandBufferImplementation& other, UninitializedTag)
		: DrawCommandBufferImplementation(other.device) {}

	void clear() noexcept {
		statistics = {};
		instanceRanges.clear();
		drawCommandBuffer.clear();
	}

	void append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount) {
		if (instanceCount == 0) {
			[[unlikely]];
			return;
		}

		const uint32_t meshParameterIndex = meshHandle->parameterRange.begin;
		const bool isInstanced = meshHandle->isInstanced;
		const bool hasMeshParameters = meshParameterIndex != meshHandle->parameterRange.end;
		const size_t drawCommandInstanceIndexOffset = 0;
		const size_t drawCommandMeshParametersIndexOffset = static_cast<size_t>(isInstanced) * sizeof(uint32_t);
		const size_t drawCommandStride = drawCommandMeshParametersIndexOffset + static_cast<size_t>(hasMeshParameters) * sizeof(uint32_t);
		const size_t drawCommandByteOffset = drawCommandBuffer.size();
		const uint32_t drawCommandOffset = (drawCommandStride == 0) ? 0 : static_cast<uint32_t>(drawCommandByteOffset / drawCommandStride);
		drawCommandBuffer.resize(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(instanceCount), [&]() -> void { descriptorSetUpToDate = false; });
		if (isInstanced) {
			for (uint32_t i = 0; i < instanceCount; ++i) {
				const uint32_t instanceIndex = instanceOffset + i;
				drawCommandBuffer.write(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(i) + drawCommandInstanceIndexOffset, asBytes(Span{&instanceIndex, 1}));
			}
		}
		if (hasMeshParameters) {
			for (uint32_t i = 0; i < instanceCount; ++i) {
				drawCommandBuffer.write(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(i) + drawCommandMeshParametersIndexOffset,
					asBytes(Span{&meshParameterIndex, 1}));
			}
		}

		const bool isIndexed = meshHandle->indexType.has_value();
		const uint32_t vertexCount = meshHandle->vertexCount;
		const uint32_t indexCount = (isIndexed) ? meshHandle->indexRange.size() : vertexCount;
		statistics.totalDrawnVertexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(vertexCount);
		statistics.totalDrawnIndexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(indexCount);
		statistics.totalDrawnInstanceCount += static_cast<size_t>(instanceCount);

		if (!instanceRanges.empty()) {
			DrawCommandBufferImplementation::InstanceRange& lastInstanceRange = instanceRanges.back();
			GREM_ASSERT(drawCommandOffset == lastInstanceRange.drawCommandOffset + lastInstanceRange.count);
			if (shaderPipelineHandle == lastInstanceRange.shaderPipelineHandle && meshHandle == lastInstanceRange.meshHandle) {
				lastInstanceRange.count += instanceCount;
				return;
			}
		}

		++statistics.totalDrawCallCount;
		instanceRanges.push_back(DrawCommandBufferImplementation::InstanceRange{
			.shaderPipelineHandle = std::move(shaderPipelineHandle),
			.meshHandle = std::move(meshHandle),
			.count = instanceCount,
			.drawCommandOffset = drawCommandOffset,
		});
	}

	void assign(const DrawCommandBufferImplementation& other) {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return;
		}
		instanceRanges = other.instanceRanges;
		drawCommandBuffer.assign(other.drawCommandBuffer, [&]() -> void { descriptorSetUpToDate = false; });
		statistics = other.statistics;
	}

	void flush() {
		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		const VkCommandBuffer commandBuffer = device.get()->getGraphicsCommandBuffer();
		drawCommandBuffer.flush(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

		if (drawCommandBuffer.get() && !descriptorSetUpToDate) {
			const VkDescriptorBufferInfo descriptorBufferInfo{
				.buffer = drawCommandBuffer.get(),
				.offset = 0,
				.range = drawCommandBuffer.capacity(),
			};
			const Array descriptorWrites{VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = descriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &descriptorBufferInfo,
				.pTexelBufferView = nullptr,
			}};
			vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			descriptorSetUpToDate = true;
		}
	}

	[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
		return descriptorSet;
	}

	[[nodiscard]] const RenderPass::Statistics& getStatistics() const noexcept {
		return statistics;
	}

	[[nodiscard]] Span<const InstanceRange> getInstanceRanges() const noexcept {
		return instanceRanges;
	}

private:
	void allocateDescriptorSet() {
		GREM_PROFILE_FUNCTION();

		const VkDevice deviceHandle = device.get()->logicalDevice.get();

		const Array poolSizes{VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1}};
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

		const Array setLayouts{device.get()->instanceOrDrawCommandBufferDescriptorSetLayout.get()};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}
	}

	Device& device;
	RenderPass::Statistics statistics{};
	ArrayList<InstanceRange> instanceRanges{};
	detail::ShaderBuffer drawCommandBuffer;
	detail::VulkanDescriptorPool descriptorPool{};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	bool descriptorSetUpToDate = false;
};

struct UnorderedDrawCommandBufferImplementation : detail::ReusableCopyOnWriteResourceBase<UnorderedDrawCommandBufferImplementation> {
	struct UninitializedTag {};

	struct Key {
		struct Hash {
			[[nodiscard]] size_t operator()(const Key& key) const {
				return getHash(key.shaderPipelineHandle);
			}
		};

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		VertexAttributeMask activeVertexAttributes;

		[[nodiscard]] bool operator==(const Key&) const = default;
	};

	struct InstanceRanges {
		explicit InstanceRanges(Device& device)
			: device(device)
			, indirectBuffer(device.get()->allocator.get(), 1024, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
			, drawCommandBuffer(device.get()->allocator.get(), 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
			allocateDescriptorSet();
		}

		~InstanceRanges() = default;

		InstanceRanges(const InstanceRanges& other)
			: device(other.device)
			, indirectBuffer(other.indirectBuffer)
			, drawCommandBuffer(other.drawCommandBuffer)
			, usedMeshes(other.usedMeshes)
			, unflushedRangeInstanceCount(other.unflushedRangeInstanceCount)
			, unflushedRangeDrawCommandOffset(other.unflushedRangeDrawCommandOffset) {
			allocateDescriptorSet();
		}

		void clear() noexcept {
			indirectBuffer.clear();
			drawCommandBuffer.clear();
			usedMeshes.clear();
			unflushedRangeInstanceCount = 0;
			unflushedRangeDrawCommandOffset = 0;
		}

		void assign(const InstanceRanges& other) {
			GREM_ASSERT(&device == &other.device);
			if (this == &other) {
				return;
			}
			indirectBuffer.assign(other.indirectBuffer, []() -> void {});
			drawCommandBuffer.assign(other.drawCommandBuffer, [&]() -> void { descriptorSetUpToDate = false; });
			usedMeshes = other.usedMeshes;
			unflushedRangeInstanceCount = other.unflushedRangeInstanceCount;
			unflushedRangeDrawCommandOffset = other.unflushedRangeDrawCommandOffset;
		}

		void append(SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount) {
			if (instanceCount == 0) {
				[[unlikely]];
				return;
			}

			const uint32_t meshParameterIndex = meshHandle->parameterRange.begin;
			const bool isInstanced = meshHandle->isInstanced;
			const bool hasMeshParameters = meshParameterIndex != meshHandle->parameterRange.end;
			const size_t drawCommandInstanceIndexOffset = 0;
			const size_t drawCommandMeshParametersIndexOffset = static_cast<size_t>(isInstanced) * sizeof(uint32_t);
			const size_t drawCommandStride = drawCommandMeshParametersIndexOffset + static_cast<size_t>(hasMeshParameters) * sizeof(uint32_t);
			const size_t drawCommandByteOffset = drawCommandBuffer.size();
			const uint32_t drawCommandOffset = (drawCommandStride == 0) ? 0 : static_cast<uint32_t>(drawCommandByteOffset / drawCommandStride);
			drawCommandBuffer.resize(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(instanceCount), [&]() -> void { descriptorSetUpToDate = false; });
			if (isInstanced) {
				for (uint32_t i = 0; i < instanceCount; ++i) {
					const uint32_t instanceIndex = instanceOffset + i;
					drawCommandBuffer.write(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(i) + drawCommandInstanceIndexOffset, asBytes(Span{&instanceIndex, 1}));
				}
			}
			if (hasMeshParameters) {
				for (uint32_t i = 0; i < instanceCount; ++i) {
					drawCommandBuffer.write(drawCommandByteOffset + drawCommandStride * static_cast<size_t>(i) + drawCommandMeshParametersIndexOffset,
						asBytes(Span{&meshParameterIndex, 1}));
				}
			}

			if (unflushedRangeInstanceCount > 0) {
				GREM_ASSERT(!usedMeshes.empty());
				GREM_ASSERT(drawCommandOffset == unflushedRangeDrawCommandOffset + unflushedRangeInstanceCount);
				if (usedMeshes.back() == meshHandle) {
					unflushedRangeInstanceCount += instanceCount;
					return;
				}
				const MeshImplementation& unflushedMesh = *usedMeshes.back();
				if (unflushedMesh.indexType) {
					const VkDrawIndexedIndirectCommand drawIndexedIndirectCommand{
						.indexCount = unflushedMesh.indexRange.size(),
						.instanceCount = unflushedRangeInstanceCount,
						.firstIndex = unflushedMesh.indexRange.begin,
						.vertexOffset = static_cast<int32_t>(unflushedMesh.vertexRange.begin),
						.firstInstance = unflushedRangeDrawCommandOffset,
					};
					indirectBuffer.append(asBytes(Span{&drawIndexedIndirectCommand, 1}), []() -> void {});
				} else {
					const VkDrawIndirectCommand drawIndirectCommand{
						.vertexCount = unflushedMesh.vertexCount,
						.instanceCount = unflushedRangeInstanceCount,
						.firstVertex = unflushedMesh.vertexRange.begin,
						.firstInstance = unflushedRangeDrawCommandOffset,
					};
					indirectBuffer.append(asBytes(Span{&drawIndirectCommand, 1}), []() -> void {});
				}
			}
			if (usedMeshes.empty() || usedMeshes.back() != meshHandle) {
				usedMeshes.push_back(std::move(meshHandle));
			}
			unflushedRangeInstanceCount = instanceCount;
			unflushedRangeDrawCommandOffset = drawCommandOffset;
		}

		void flush() {
			if (unflushedRangeInstanceCount > 0) {
				GREM_ASSERT(!usedMeshes.empty());
				const MeshImplementation& unflushedMesh = *usedMeshes.back();
				if (unflushedMesh.indexType) {
					const VkDrawIndexedIndirectCommand drawIndexedIndirectCommand{
						.indexCount = unflushedMesh.indexRange.size(),
						.instanceCount = unflushedRangeInstanceCount,
						.firstIndex = unflushedMesh.indexRange.begin,
						.vertexOffset = static_cast<int32_t>(unflushedMesh.vertexRange.begin),
						.firstInstance = unflushedRangeDrawCommandOffset,
					};
					indirectBuffer.append(asBytes(Span{&drawIndexedIndirectCommand, 1}), [&]() -> void {});
				} else {
					const VkDrawIndirectCommand drawIndirectCommand{
						.vertexCount = unflushedMesh.vertexCount,
						.instanceCount = unflushedRangeInstanceCount,
						.firstVertex = unflushedMesh.vertexRange.begin,
						.firstInstance = unflushedRangeDrawCommandOffset,
					};
					indirectBuffer.append(asBytes(Span{&drawIndirectCommand, 1}), []() -> void {});
				}
				unflushedRangeInstanceCount = 0;
			}

			const VkDevice deviceHandle = device.get()->logicalDevice.get();
			const VkCommandBuffer commandBuffer = device.get()->getGraphicsCommandBuffer();
			indirectBuffer.flush(commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
			drawCommandBuffer.flush(commandBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

			if (drawCommandBuffer.get() && !descriptorSetUpToDate) {
				const VkDescriptorBufferInfo descriptorBufferInfo{
					.buffer = drawCommandBuffer.get(),
					.offset = 0,
					.range = drawCommandBuffer.capacity(),
				};
				const Array descriptorWrites{VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = descriptorSet,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.pImageInfo = nullptr,
					.pBufferInfo = &descriptorBufferInfo,
					.pTexelBufferView = nullptr,
				}};
				vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
				descriptorSetUpToDate = true;
			}
		}

		[[nodiscard]] bool empty() const noexcept {
			return drawCommandBuffer.empty();
		}

		[[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept {
			return descriptorSet;
		}

		[[nodiscard]] const detail::ShaderBuffer& getIndirectBuffer() const noexcept {
			return indirectBuffer;
		}

	private:
		void allocateDescriptorSet() {
			GREM_PROFILE_FUNCTION();

			const VkDevice deviceHandle = device.get()->logicalDevice.get();

			const Array poolSizes{VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1}};
			const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkDescriptorPoolCreateFlags{},
				.maxSets = 1,
				.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
				.pPoolSizes = poolSizes.data(),
			};
			VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
			if (const VkResult result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateDescriptorPool", result};
			}
			descriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{deviceHandle}};

			const Array setLayouts{device.get()->instanceOrDrawCommandBufferDescriptorSetLayout.get()};
			const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = descriptorPoolHandle,
				.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
				.pSetLayouts = setLayouts.data(),
			};
			if (const VkResult result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo, &descriptorSet); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkAllocateDescriptorSets", result};
			}
		}

		Device& device;
		detail::ShaderBuffer indirectBuffer;
		detail::ShaderBuffer drawCommandBuffer;
		detail::VulkanDescriptorPool descriptorPool{};
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		ArrayList<SharedPointer<MeshImplementation>> usedMeshes{};
		uint32_t unflushedRangeInstanceCount = 0;
		uint32_t unflushedRangeDrawCommandOffset = 0;
		bool descriptorSetUpToDate = false;
	};

	[[nodiscard]] static SharedPointer<UnorderedDrawCommandBufferImplementation> create(Device& device) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<UnorderedDrawCommandBufferImplementation>::create(device);
	}

	[[nodiscard]] static SharedPointer<UnorderedDrawCommandBufferImplementation> clone(const UnorderedDrawCommandBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<UnorderedDrawCommandBufferImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<UnorderedDrawCommandBufferImplementation> cloneUninitialized(const UnorderedDrawCommandBufferImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<UnorderedDrawCommandBufferImplementation>::create(implementation, UninitializedTag{});
	}

	explicit UnorderedDrawCommandBufferImplementation(Device& device)
		: device(device) {}

	UnorderedDrawCommandBufferImplementation(const UnorderedDrawCommandBufferImplementation& other, UninitializedTag)
		: device(other.device)
		, statistics(other.statistics) {}

	void clear() noexcept {
		statistics = {};
		for (auto&& [key, instanceRanges] : instanceRanges) {
			instanceRanges.clear();
		}
	}

	void assign(const UnorderedDrawCommandBufferImplementation& other) {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return;
		}
		instanceRanges = other.instanceRanges;
		statistics = other.statistics;
	}

	void insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount) {
		if (instanceCount == 0) {
			[[unlikely]];
			return;
		}

		const auto [it, inserted] = instanceRanges.try_emplace(
			UnorderedDrawCommandBufferImplementation::Key{
				.shaderPipelineHandle = std::move(shaderPipelineHandle),
				.activeVertexAttributes = meshHandle->activeVertexAttributes,
			},
			device);
		const bool isIndexed = meshHandle->indexType.has_value();
		const uint32_t vertexCount = meshHandle->vertexCount;
		const uint32_t indexCount = (isIndexed) ? meshHandle->indexRange.size() : vertexCount;
		statistics.totalDrawnVertexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(vertexCount);
		statistics.totalDrawnIndexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(indexCount);
		statistics.totalDrawnInstanceCount += static_cast<size_t>(instanceCount);
		statistics.totalDrawCallCount += static_cast<size_t>(it->second.empty());
		it->second.append(std::move(meshHandle), instanceOffset, instanceCount);
	}

	void flush() {
		for (auto&& [key, instanceRanges] : instanceRanges) {
			instanceRanges.flush();
		}
	}

	[[nodiscard]] const RenderPass::Statistics& getStatistics() const noexcept {
		return statistics;
	}

	[[nodiscard]] const HashMap<Key, InstanceRanges, Key::Hash>& getInstanceRanges() const noexcept {
		return instanceRanges;
	}

private:
	Device& device;
	RenderPass::Statistics statistics{};
	HashMap<Key, InstanceRanges, Key::Hash> instanceRanges{};
};

} // namespace grem::graphics

#endif
