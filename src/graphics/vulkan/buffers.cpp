// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "MeshImplementation.hpp"
#include "buffer_implementations.hpp"

#include <utility> // std::move

namespace grem::graphics {

namespace detail {

UniformBufferBase::UniformBufferBase(Device& device, BufferLayoutReference bufferLayout, size_t bufferSize, size_t textureParameterCount)
	: implementation(UniformBufferImplementation::create(device, bufferLayout, bufferSize, textureParameterCount)) {}

void UniformBufferBase::upload(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures, Span<const ParameterDescription>) {
	detail::ensureExclusiveResourceAccess(
		implementation, UniformBufferImplementation::cloneUninitialized, [&](UniformBufferImplementation&, const UniformBufferImplementation&) -> void {}, *implementation);

	implementation->upload(newParameterValuesBytes, newTextures);
}

void UniformBufferBase::flush() const {
	implementation->flush();
}

StorageBufferBase::StorageBufferBase(Device& device, BufferLayoutReference bufferLayout, size_t elementSize)
	: implementation(StorageBufferImplementation::create(device, bufferLayout, elementSize)) {
	GREM_ASSERT(elementSize > 0);
	GREM_ASSERT(elementSize % sizeof(float) == 0);
}

void StorageBufferBase::upload(StridedSpan<const byte> elementsData, size_t elementSize) {
	GREM_ASSERT(elementSize > 0);
	GREM_ASSERT(elementSize % sizeof(float) == 0);

	const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
	const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
	const size_t requiredSizeInBytes = elementsData.size() * elementStrideInBytes;

	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<StorageBufferImplementation> {
			return StorageBufferImplementation::cloneUninitialized(*implementation, max(requiredSizeInBytes, implementation->size()));
		},
		[&](StorageBufferImplementation&) -> void {});

	implementation->resize(requiredSizeInBytes);
	if (elementSize == elementStrideInBytes && elementSize == elementsData.stride()) {
		implementation->write(0, Span{elementsData.base(), elementsData.size() * elementSize});
	} else {
		size_t outputByteOffset = 0;
		const auto end = elementsData.end();
		for (auto it = elementsData.begin(); it != end; ++it) {
			implementation->write(outputByteOffset, Span{it.base(), elementSize});
			outputByteOffset += elementStrideInBytes;
		}
	}
}

void StorageBufferBase::write(uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize) {
	GREM_ASSERT(elementSize > 0);
	GREM_ASSERT(elementSize % sizeof(float) == 0);
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
	const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
	const size_t elementOffsetInBytes = static_cast<size_t>(elementOffset) * elementStrideInBytes;
	const size_t addedElementsSizeInBytes = elementsData.size() * elementStrideInBytes;
	const size_t writtenRangeEnd = elementOffsetInBytes + addedElementsSizeInBytes;

	if (elementOffsetInBytes == 0 && writtenRangeEnd <= implementation->size()) {
		detail::ensureExclusiveResourceAccess(
			implementation,
			[&]() -> SharedPointer<StorageBufferImplementation> { return StorageBufferImplementation::cloneUninitialized(*implementation, implementation->size()); },
			[&](StorageBufferImplementation&) -> void {});
	} else {
		detail::ensureExclusiveResourceAccess(
			implementation,
			[&]() -> SharedPointer<StorageBufferImplementation> { return StorageBufferImplementation::clone(*implementation, max(writtenRangeEnd, implementation->size())); },
			[&](StorageBufferImplementation& oldBuffer) -> void {
				oldBuffer.clear();
				oldBuffer.reserve(max(writtenRangeEnd, implementation->size()));
				oldBuffer.assign(*implementation);
			});
	}

	if (writtenRangeEnd > implementation->size()) {
		implementation->resize(writtenRangeEnd);
	}

	if (elementSize == elementStrideInBytes && elementSize == elementsData.stride()) {
		implementation->write(elementOffsetInBytes, Span{elementsData.base(), elementsData.size() * elementSize});
	} else {
		size_t outputByteOffset = elementOffsetInBytes;
		const auto end = elementsData.end();
		for (auto it = elementsData.begin(); it != end; ++it) {
			implementation->write(outputByteOffset, Span{it.base(), elementSize});
			outputByteOffset += elementStrideInBytes;
		}
	}
}

void StorageBufferBase::flush() const {
	implementation->flush();
}

BufferSetBase::BufferSetBase(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers)
	: implementation(BufferSetImplementation::create(device, bufferSetLayout, std::move(buffers))) {}

void BufferSetBase::setBuffer(size_t bufferIndex, SharedPointer<void> newBuffer) {
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::clone(*implementation);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			oldBufferSet.assign(*implementation);
		});

	implementation->setBuffer(bufferIndex, newBuffer);
}

void BufferSetBase::setBuffers(Span<SharedPointer<void>> newBuffers) {
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::clone(*implementation);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			oldBufferSet.assign(*implementation);
		});

	implementation->setBuffers(newBuffers);
}

void BufferSetBase::uploadToUniformBuffer(size_t bufferIndex, Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures,
	Span<const ParameterDescription>) {
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::clone(*implementation);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			oldBufferSet.assign(*implementation);
		});

	implementation->uploadBytesToUniformBuffer(bufferIndex, newParameterValuesBytes, newTextures);
}

void BufferSetBase::uploadToStorageBuffer(size_t bufferIndex, StridedSpan<const byte> elementsData, size_t elementSize) {
	GREM_ASSERT(elementSize > 0);
	GREM_ASSERT(elementSize % sizeof(float) == 0);

	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::clone(*implementation);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			oldBufferSet.assign(*implementation);
		});

	const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
	const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
	const size_t requiredSizeInBytes = elementsData.size() * elementStrideInBytes;
	implementation->resizeStorageBuffer(bufferIndex, requiredSizeInBytes, true);
	if (elementSize == elementStrideInBytes && elementSize == elementsData.stride()) {
		implementation->writeBytesToStorageBuffer(bufferIndex, 0, Span{elementsData.base(), elementsData.size() * elementSize}, true);
	} else {
		size_t outputByteOffset = 0;
		const auto end = elementsData.end();
		for (auto it = elementsData.begin(); it != end; ++it) {
			implementation->writeBytesToStorageBuffer(bufferIndex, outputByteOffset, Span{it.base(), elementSize}, true);
			outputByteOffset += elementStrideInBytes;
		}
	}
}

void BufferSetBase::writeToStorageBuffer(size_t bufferIndex, uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize) {
	GREM_ASSERT(elementSize > 0);
	GREM_ASSERT(elementSize % sizeof(float) == 0);
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::clone(*implementation);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			oldBufferSet.assign(*implementation);
		});

	const size_t elementStrideInVec4s = detail::convertFloatCountToVec4Count(elementSize / sizeof(float));
	const size_t elementStrideInBytes = elementStrideInVec4s * sizeof(float) * 4;
	const size_t elementOffsetInBytes = static_cast<size_t>(elementOffset) * elementStrideInBytes;
	if (elementSize == elementStrideInBytes && elementSize == elementsData.stride()) {
		implementation->writeBytesToStorageBuffer(bufferIndex, elementOffsetInBytes, Span{elementsData.base(), elementsData.size() * elementSize}, false);
	} else {
		size_t outputByteOffset = elementOffsetInBytes;
		const auto end = elementsData.end();
		for (auto it = elementsData.begin(); it != end; ++it) {
			implementation->writeBytesToStorageBuffer(bufferIndex, outputByteOffset, Span{it.base(), elementSize}, false);
			outputByteOffset += elementStrideInBytes;
		}
	}
}

void BufferSetBase::flush() const {
	implementation->flush();
}

void InstanceBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(
		implementation, InstanceBufferImplementation::cloneUninitialized, [&](InstanceBufferImplementation&, const InstanceBufferImplementation&) -> void {}, *implementation);

	implementation->clear();
}

uint32_t InstanceBufferBase::size() const noexcept {
	return implementation->size();
}

InstanceBufferBase::InstanceBufferBase(Device& device, size_t instanceSize)
	: implementation(InstanceBufferImplementation::create(device, instanceSize, 1)) {
	GREM_ASSERT(instanceSize > 0);
	GREM_ASSERT(instanceSize % sizeof(float) == 0);
}

uint32_t InstanceBufferBase::append(StridedSpan<const byte> instancesData, size_t) {
	if (instancesData.empty()) {
		[[unlikely]];
		return 0;
	}

	const uint32_t instanceOffset = implementation->size();
	const uint32_t requiredInstanceCount = instanceOffset + static_cast<uint32_t>(instancesData.size());

	detail::ensureExclusiveResourceAccess(
		implementation, InstanceBufferImplementation::clone,
		[&](InstanceBufferImplementation& oldBuffer, const InstanceBufferImplementation& buffer) -> void { oldBuffer.assign(buffer); }, *implementation);

	implementation->resize(requiredInstanceCount);
	implementation->write(instanceOffset, instancesData);
	return instanceOffset;
}

void InstanceBufferBase::flush() const {
	implementation->flush();
}

void DrawCommandBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(
		implementation, DrawCommandBufferImplementation::cloneUninitialized, [&](DrawCommandBufferImplementation&, const DrawCommandBufferImplementation&) -> void {},
		*implementation);

	implementation->clear();
}

DrawCommandBufferBase::DrawCommandBufferBase(Device& device)
	: implementation(SharedPointer<DrawCommandBufferImplementation>::create(device)) {}

void DrawCommandBufferBase::append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset,
	uint32_t instanceCount) {
	if (instanceCount == 0) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation, DrawCommandBufferImplementation::clone,
		[&](DrawCommandBufferImplementation& oldBuffer, const DrawCommandBufferImplementation& buffer) -> void { oldBuffer.assign(buffer); }, *implementation);

	implementation->append(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
}

void DrawCommandBufferBase::flush() const {
	implementation->flush();
}

void UnorderedDrawCommandBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(
		implementation, UnorderedDrawCommandBufferImplementation::cloneUninitialized,
		[&](UnorderedDrawCommandBufferImplementation&, const UnorderedDrawCommandBufferImplementation&) -> void {}, *implementation);

	implementation->clear();
}

UnorderedDrawCommandBufferBase::UnorderedDrawCommandBufferBase(Device& device)
	: implementation(UnorderedDrawCommandBufferImplementation::create(device)) {}

void UnorderedDrawCommandBufferBase::insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset,
	uint32_t instanceCount) {
	if (instanceCount == 0) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation, UnorderedDrawCommandBufferImplementation::clone,
		[&](UnorderedDrawCommandBufferImplementation& oldBuffer, const UnorderedDrawCommandBufferImplementation& buffer) -> void { oldBuffer.assign(buffer); }, *implementation);

	implementation->insert(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
}

void UnorderedDrawCommandBufferBase::flush() const {
	implementation->flush();
}

} // namespace detail

} // namespace grem::graphics
