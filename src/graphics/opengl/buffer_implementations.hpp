// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_BUFFER_IMPLEMENTATIONS_HPP
#define GREM_GRAPHICS_OPENGL_BUFFER_IMPLEMENTATIONS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/SquareAllocator.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "DeviceImplementation.hpp"
#include "StatePreserver.hpp"
#include "objects.hpp"
#include "opengl.hpp"

#include <stdexcept> // std::length_error
#include <utility>   // std::move, std::exchange

namespace grem::graphics {

struct UniformBufferImplementation : detail::ReusableCopyOnWriteResourceBase<UniformBufferImplementation> {
	[[nodiscard]] static SharedPointer<UniformBufferImplementation> create(size_t bufferSize) {
		return SharedPointer<UniformBufferImplementation>::create(bufferSize);
	}

	size_t bufferSize;
	detail::BufferObject uniformBufferObject = detail::createBufferObject();
	SmallArrayList<SharedPointer<TextureImplementation>, 8> textures{};

	explicit UniformBufferImplementation(size_t bufferSize)
		: bufferSize(bufferSize) {
		const detail::UniformBufferBindingPreserver uniformBufferBindingPreserver{};
		glBindBuffer(GL_UNIFORM_BUFFER, uniformBufferObject.get());
		glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(bufferSize), nullptr, GL_STREAM_DRAW);
	}

	void invalidateContents() noexcept {
		for (SharedPointer<TextureImplementation>& texture : textures) {
			texture = {};
		}
	}

	void upload(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures) {
		const detail::UniformBufferBindingPreserver uniformBufferBindingPreserver{};
		glBindBuffer(GL_UNIFORM_BUFFER, uniformBufferObject.get());

		glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(newParameterValuesBytes.size_bytes()), newParameterValuesBytes.data());

		textures.clear();
		for (SharedPointer<TextureImplementation>& handle : newTextures) {
			textures.push_back(std::move(handle));
		}
	}
};

struct StorageBufferImplementation : detail::ReusableCopyOnWriteResourceBase<StorageBufferImplementation> {
	[[nodiscard]] static SharedPointer<StorageBufferImplementation> create(Device& device, size_t elementSize) {
		return SharedPointer<StorageBufferImplementation>::create(device, elementSize);
	}

	Device& device;
	SquareAllocation<uint32_t> squareAllocation{};
	size_t elementSize;
	Allocation<byte> stagingMemory{};
	uint32_t stagingMemoryWidth = 0;
	bool dirty = false;

	StorageBufferImplementation(Device& device, size_t elementSize)
		: device(device)
		, elementSize(elementSize) {
		GREM_ASSERT(elementSize > 0);
		GREM_ASSERT(elementSize % sizeof(float) == 0);
		[[maybe_unused]] const bool inserted = device.get()->storageBuffers.insert(this).second;
		GREM_ASSERT(inserted);
	}

	~StorageBufferImplementation() {
		device.get()->storageBufferSquareAllocator.deallocateSquare(squareAllocation);
		[[maybe_unused]] const size_t erased = device.get()->storageBuffers.erase(this);
		GREM_ASSERT(erased == 1);
	}

	StorageBufferImplementation(const StorageBufferImplementation&) = delete;
	StorageBufferImplementation(StorageBufferImplementation&&) = delete;
	StorageBufferImplementation& operator=(const StorageBufferImplementation&) = delete;
	StorageBufferImplementation& operator=(StorageBufferImplementation&&) = delete;

	void assign(const StorageBufferImplementation& other) {
		const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));

		GREM_ASSERT(elementStrideInVec4s < Limits<size_t>::MAX / (sizeof(float) * 4));
		const size_t elementStrideInBytes = elementStrideInVec4s * (sizeof(float) * 4);

		write(0, StridedSpan{other.stagingMemory.data(), other.stagingMemory.size() / elementStrideInBytes, elementStrideInBytes});
	}

	void write(uint32_t elementOffset, StridedSpan<const byte> elementsData) {
		if (elementsData.empty()) {
			[[unlikely]];
			return;
		}

		const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
		GREM_ASSERT(elementStrideInVec4s <= size_t{Limits<uint32_t>::MAX});

		GREM_ASSERT(elementStrideInVec4s < Limits<size_t>::MAX / (sizeof(float) * 4));
		const size_t elementStrideInBytes = elementStrideInVec4s * (sizeof(float) * 4);

		if (elementOffset > Limits<uint32_t>::MAX / static_cast<uint32_t>(elementStrideInVec4s)) {
			throw std::length_error{"Maximum shader storage buffer size exceeded."};
		}
		const size_t elementOffsetInVec4s = static_cast<size_t>(elementOffset) * elementStrideInVec4s;

		GREM_ASSERT(elementOffsetInVec4s < Limits<size_t>::MAX / (sizeof(float) * 4));
		const size_t elementOffsetInBytes = elementOffsetInVec4s * (sizeof(float) * 4);

		if (elementsData.size() > size_t{Limits<uint32_t>::MAX} / elementStrideInVec4s) {
			throw std::length_error{"Maximum shader storage buffer size exceeded."};
		}
		const size_t addedElementsSizeInVec4s = elementsData.size() * elementStrideInVec4s;

		if (elementOffsetInVec4s > size_t{Limits<uint32_t>::MAX} - addedElementsSizeInVec4s) {
			throw std::length_error{"Maximum shader storage buffer size exceeded."};
		}
		const size_t requiredSizeInVec4s = elementOffsetInVec4s + addedElementsSizeInVec4s;

		if (requiredSizeInVec4s > stagingMemoryWidth * stagingMemoryWidth) {
			uint32_t newWidth = max(stagingMemoryWidth, uint32_t{1});
			do {
				newWidth *= 2;
				if (newWidth > Limits<uint32_t>::MAX / newWidth) {
					throw std::length_error{"Maximum shader storage buffer size exceeded."};
				}
			} while (newWidth * newWidth < requiredSizeInVec4s);

			GREM_PROFILE_BLOCK("Expand shader storage buffer staging memory");
			const uint32_t newSizeInVec4s = newWidth * newWidth;
			const size_t requiredSizeInBytes = newSizeInVec4s * (sizeof(float) * 4);
			stagingMemory.resize(requiredSizeInBytes, byte{});
			stagingMemoryWidth = newWidth;
		}

		byte* output = stagingMemory.data() + elementOffsetInBytes;
		if (elementSize == elementStrideInBytes && elementSize == elementsData.stride()) {
			memcpy(output, elementsData.base(), elementsData.size() * elementSize);
		} else {
			const auto end = elementsData.end();
			for (auto it = elementsData.begin(); it != end; ++it) {
				memcpy(output, it.base(), elementSize);
				output += elementStrideInBytes;
			}
		}
		dirty = true;
	}
};

struct BufferSetImplementation : detail::ReusableCopyOnWriteResourceBase<BufferSetImplementation> {
	[[nodiscard]] static SharedPointer<BufferSetImplementation> create(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers) {
		return SharedPointer<BufferSetImplementation>::create(device, bufferSetLayout, std::move(buffers));
	}

	Device& device;
	BufferLayoutReference bufferSetLayout;
	Allocation<SharedPointer<void>> buffers;

	BufferSetImplementation(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers)
		: device(device)
		, bufferSetLayout(bufferSetLayout)
		, buffers(std::move(buffers)) {}

	void invalidateContents() noexcept {
		const Span<const BufferLayoutReference> bufferLayouts = bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		GREM_ASSERT(buffers.size() == bufferLayouts.size());

		for (size_t i = 0; i < bufferLayouts.size(); ++i) {
			GREM_ASSERT(buffers[i]);
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					if (buffers[i].use_count() == 1) {
						static_cast<UniformBufferImplementation*>(buffers[i].get())->invalidateContents();
					}
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}
	}
};

struct InstanceBufferImplementation : detail::ReusableCopyOnWriteResourceBase<InstanceBufferImplementation> {
	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> create(size_t instanceSize) {
		return SharedPointer<InstanceBufferImplementation>::create(instanceSize);
	}

	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> cloneUninitialized(const InstanceBufferImplementation& implementation) {
		return SharedPointer<InstanceBufferImplementation>::create(implementation.instanceSize);
	}

	[[nodiscard]] static SharedPointer<InstanceBufferImplementation> clone(const InstanceBufferImplementation& implementation) {
		return SharedPointer<InstanceBufferImplementation>::create(implementation);
	}

	Buffer<byte> instanceData{};
	size_t instanceSize;

	explicit InstanceBufferImplementation(size_t instanceSize)
		: instanceSize(instanceSize) {}
};

struct DrawCommandBufferImplementation : detail::ReusableCopyOnWriteResourceBase<DrawCommandBufferImplementation> {
	struct InstanceRange {
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;
		uint32_t offset;
		uint32_t count;
	};

	[[nodiscard]] static SharedPointer<DrawCommandBufferImplementation> create() {
		return SharedPointer<DrawCommandBufferImplementation>::create();
	}

	[[nodiscard]] static SharedPointer<DrawCommandBufferImplementation> clone(const DrawCommandBufferImplementation& implementation) {
		return SharedPointer<DrawCommandBufferImplementation>::create(implementation);
	}

	ArrayList<InstanceRange> instanceRanges{};
};

struct UnorderedDrawCommandBufferImplementation : detail::ReusableCopyOnWriteResourceBase<UnorderedDrawCommandBufferImplementation> {
	struct Key {
		struct Hash {
			[[nodiscard]] size_t operator()(const Key& key) const {
				return getHash(key.shaderPipelineHandle, key.meshHandle);
			}
		};

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;

		[[nodiscard]] bool operator==(const Key&) const = default;
	};

	struct InstanceRange {
		uint32_t offset;
		uint32_t count;
	};

	[[nodiscard]] static SharedPointer<UnorderedDrawCommandBufferImplementation> create() {
		return SharedPointer<UnorderedDrawCommandBufferImplementation>::create();
	}

	[[nodiscard]] static SharedPointer<UnorderedDrawCommandBufferImplementation> clone(const UnorderedDrawCommandBufferImplementation& implementation) {
		return SharedPointer<UnorderedDrawCommandBufferImplementation>::create(implementation);
	}

	HashMap<Key, Buffer<InstanceRange>, Key::Hash> instanceRanges{};
};

} // namespace grem::graphics

#endif
