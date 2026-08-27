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
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "StatePreserver.hpp"
#include "objects.hpp"
#include "opengl.hpp"

#include <utility> // std::move

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
	[[nodiscard]] static SharedPointer<StorageBufferImplementation> create(size_t elementSize) {
		return SharedPointer<StorageBufferImplementation>::create(elementSize);
	}

	detail::TextureObject textureObject = detail::createTextureObject();
	size_t elementSize;
	size_t textureResolution = 0;
	Allocation<byte> stagingMemory{};

	explicit StorageBufferImplementation(size_t elementSize)
		: elementSize(elementSize) {
		GREM_ASSERT(elementSize > 0);
		GREM_ASSERT(elementSize % sizeof(float) == 0);

		const detail::TextureBinding2DPreserver textureBinding2DPreserver{};
		glBindTexture(GL_TEXTURE_2D, textureObject.get());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	void assign(const StorageBufferImplementation& other) {
		const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
		const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
		write(0, StridedSpan{other.stagingMemory.data(), other.stagingMemory.size() / elementStrideInBytes, elementStrideInBytes});
	}

	void write(uint32_t elementOffset, StridedSpan<const byte> elementsData) {
		if (elementsData.empty()) {
			[[unlikely]];
			return;
		}

		const detail::TextureBinding2DPreserver textureBinding2DPreserver{};
		glBindTexture(GL_TEXTURE_2D, textureObject.get());

		const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
		const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
		const size_t elementOffsetInVec4s = static_cast<size_t>(elementOffset) * elementStrideInVec4s;
		const size_t elementOffsetInBytes = elementOffsetInVec4s * sizeof(float) * 4;
		const size_t addedElementsSizeInVec4s = elementsData.size() * elementStrideInVec4s;
		const size_t requiredSizeInVec4s = elementOffsetInVec4s + addedElementsSizeInVec4s;
		if (textureResolution * textureResolution < requiredSizeInVec4s) {
			size_t newResolution = max(textureResolution * 2, size_t{8});
			while (newResolution * newResolution < requiredSizeInVec4s) {
				GREM_ASSERT(newResolution * 2 > newResolution);
				newResolution *= 2;
			}
			stagingMemory.resize(newResolution * newResolution * sizeof(float) * 4);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>(newResolution), static_cast<GLsizei>(newResolution), 0, GL_RGBA, GL_FLOAT, nullptr);
			textureResolution = newResolution;
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

		const detail::UnpackAlignmentPreserver unpackAlignmentPreserver{};
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		const detail::UnpackRowLengthPreserver unpackRowLengthPreserver{};
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		const detail::UnpackSkipPixelsPreserver unpackSkipPixelsPreserver{};
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		const detail::UnpackSkipRowsPreserver unpackSkipRowsPreserver{};
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		const detail::UnpackImageHeightPreserver unpackImageHeightPreserver{};
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
		const detail::UnpackSkipImagesPreserver unpackSkipImagesPreserver{};
		glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(textureResolution), static_cast<GLsizei>(textureResolution), GL_RGBA, GL_FLOAT, stagingMemory.data());
	}
};

struct BufferSetImplementation : detail::ReusableCopyOnWriteResourceBase<BufferSetImplementation> {
	[[nodiscard]] static SharedPointer<BufferSetImplementation> create(BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers) {
		return SharedPointer<BufferSetImplementation>::create(bufferSetLayout, std::move(buffers));
	}

	BufferLayoutReference bufferSetLayout;
	Allocation<SharedPointer<void>> buffers;

	explicit BufferSetImplementation(BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers)
		: bufferSetLayout(bufferSetLayout)
		, buffers(std::move(buffers)) {}
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
