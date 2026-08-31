// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
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
#include "buffer_implementations.hpp"

#include <utility> // std::move, std::exchange

namespace grem::graphics {

namespace detail {

UniformBufferBase::UniformBufferBase(Device&, BufferLayoutReference, size_t bufferSize, size_t)
	: implementation(UniformBufferImplementation::create(bufferSize)) {}

void UniformBufferBase::upload(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures, Span<const ParameterDescription>) {
	detail::ensureExclusiveResourceAccess(
		implementation, [&]() -> SharedPointer<UniformBufferImplementation> { return UniformBufferImplementation::create(newParameterValuesBytes.size_bytes()); },
		[&](UniformBufferImplementation&) -> void {});

	implementation->upload(newParameterValuesBytes, newTextures);
}

void UniformBufferBase::flush() const {}

StorageBufferBase::StorageBufferBase(Device& device, BufferLayoutReference, size_t elementSize)
	: implementation(StorageBufferImplementation::create(device, elementSize)) {}

void StorageBufferBase::upload(StridedSpan<const byte> elementsData, size_t elementSize) {
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<StorageBufferImplementation> { //
			return StorageBufferImplementation::create(implementation->device, elementSize);
		},
		[](StorageBufferImplementation&) -> void {});

	implementation->write(0, elementsData);
}

void StorageBufferBase::write(uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize) {
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<StorageBufferImplementation> { //
			SharedPointer<StorageBufferImplementation> newBuffer = StorageBufferImplementation::create(implementation->device, elementSize);
			newBuffer->assign(*implementation);
			return newBuffer;
		},
		[&](StorageBufferImplementation& oldBuffer) -> void { //
			oldBuffer.assign(*implementation);
		});

	implementation->write(elementOffset, elementsData);
}

void StorageBufferBase::flush() const {}

BufferSetBase::BufferSetBase(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers)
	: implementation(BufferSetImplementation::create(device, bufferSetLayout, std::move(buffers))) {}

void BufferSetBase::setBuffer(size_t bufferIndex, SharedPointer<void> newBuffer) {
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::create(implementation->device, implementation->bufferSetLayout, implementation->buffers);
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			GREM_ASSERT(oldBufferSet.bufferSetLayout == implementation->bufferSetLayout);
			oldBufferSet.buffers = implementation->buffers;
		});

	implementation->buffers[bufferIndex] = std::move(newBuffer);
}

void BufferSetBase::setBuffers(Span<SharedPointer<void>> newBuffers) {
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			return BufferSetImplementation::create(implementation->device, implementation->bufferSetLayout, Allocation<SharedPointer<void>>(newBuffers.size(), nullptr));
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			GREM_ASSERT(oldBufferSet.bufferSetLayout == implementation->bufferSetLayout);
			oldBufferSet.buffers.fill(nullptr);
		});

	for (size_t i = 0; i < newBuffers.size(); ++i) {
		implementation->buffers[i] = std::move(newBuffers[i]);
	}
}

void BufferSetBase::uploadToUniformBuffer(size_t bufferIndex, Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures,
	Span<const ParameterDescription>) {
	const auto setNewBuffers = [&](Allocation<SharedPointer<void>>& newBuffers) -> void {
		const Span<const SharedPointer<void>> buffers = implementation->buffers;
		const Span<const BufferLayoutReference> bufferLayouts = implementation->bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		GREM_ASSERT(buffers.size() == bufferLayouts.size());
		newBuffers.resize(buffers.size());
		for (size_t i = 0; i < newBuffers.size(); ++i) {
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					if (i == bufferIndex) {
						newBuffers[i] = UniformBufferImplementation::create(static_cast<const UniformBufferImplementation*>(implementation->buffers[i].get())->bufferSize);
					} else {
						newBuffers[i] = buffers[i];
					}
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					newBuffers[i] = buffers[i];
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}
	};
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			Allocation<SharedPointer<void>> newBuffers{};
			setNewBuffers(newBuffers);
			return BufferSetImplementation::create(implementation->device, implementation->bufferSetLayout, std::move(newBuffers));
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			GREM_ASSERT(oldBufferSet.bufferSetLayout == implementation->bufferSetLayout);
			setNewBuffers(oldBufferSet.buffers);
		});

	UniformBufferImplementation& uniformBuffer = *static_cast<UniformBufferImplementation*>(implementation->buffers[bufferIndex].get());
	uniformBuffer.upload(newParameterValuesBytes, newTextures);
}

void BufferSetBase::uploadToStorageBuffer(size_t bufferIndex, StridedSpan<const byte> elementsData, size_t elementSize) {
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	const auto setNewBuffers = [&](Allocation<SharedPointer<void>>& newBuffers) -> void {
		const Span<const SharedPointer<void>> buffers = implementation->buffers;
		const Span<const BufferLayoutReference> bufferLayouts = implementation->bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		GREM_ASSERT(buffers.size() == bufferLayouts.size());
		newBuffers.resize(buffers.size());
		for (size_t i = 0; i < newBuffers.size(); ++i) {
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					newBuffers[i] = buffers[i];
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					if (i == bufferIndex) {
						newBuffers[i] = StorageBufferImplementation::create(implementation->device, elementSize);
					} else {
						newBuffers[i] = buffers[i];
					}
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}
	};
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			Allocation<SharedPointer<void>> newBuffers{};
			setNewBuffers(newBuffers);
			return BufferSetImplementation::create(implementation->device, implementation->bufferSetLayout, std::move(newBuffers));
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			GREM_ASSERT(oldBufferSet.bufferSetLayout == implementation->bufferSetLayout);
			setNewBuffers(oldBufferSet.buffers);
		});

	StorageBufferImplementation& storageBuffer = *static_cast<StorageBufferImplementation*>(implementation->buffers[bufferIndex].get());
	storageBuffer.write(0, elementsData);
}

void BufferSetBase::writeToStorageBuffer(size_t bufferIndex, uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize) {
	if (elementsData.empty()) {
		[[unlikely]];
		return;
	}

	const auto setNewBuffers = [&](Allocation<SharedPointer<void>>& newBuffers) -> void {
		Device& device = implementation->device;
		const Span<const SharedPointer<void>> buffers = implementation->buffers;
		const Span<const BufferLayoutReference> bufferLayouts = implementation->bufferSetLayout.as<BufferSetLayoutReference>().bufferLayouts;
		GREM_ASSERT(buffers.size() == bufferLayouts.size());
		newBuffers.resize(buffers.size());
		for (size_t i = 0; i < newBuffers.size(); ++i) {
			GREM_MATCH(bufferLayouts[i]) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					newBuffers[i] = buffers[i];
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					if (i == bufferIndex) {
						newBuffers[i] = StorageBufferImplementation::create(device, elementSize);
						static_cast<StorageBufferImplementation*>(newBuffers[i].get())->assign(*static_cast<const StorageBufferImplementation*>(buffers[i].get()));
					} else {
						newBuffers[i] = buffers[i];
					}
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					unreachable();
				}
			}
		}
	};
	detail::ensureExclusiveResourceAccess(
		implementation,
		[&]() -> SharedPointer<BufferSetImplementation> { //
			Allocation<SharedPointer<void>> newBuffers{};
			setNewBuffers(newBuffers);
			return BufferSetImplementation::create(implementation->device, implementation->bufferSetLayout, std::move(newBuffers));
		},
		[&](BufferSetImplementation& oldBufferSet) -> void { //
			GREM_ASSERT(oldBufferSet.bufferSetLayout == implementation->bufferSetLayout);
			setNewBuffers(oldBufferSet.buffers);
		});

	StorageBufferImplementation& storageBuffer = *static_cast<StorageBufferImplementation*>(implementation->buffers[bufferIndex].get());
	storageBuffer.write(elementOffset, elementsData);
}

void BufferSetBase::flush() const {}

void InstanceBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(
		implementation, InstanceBufferImplementation::cloneUninitialized, [&](InstanceBufferImplementation&, const InstanceBufferImplementation&) -> void {}, *implementation);

	implementation->instanceData.clear();
}

uint32_t InstanceBufferBase::size() const noexcept {
	GREM_ASSERT(implementation->instanceSize > 0);
	GREM_ASSERT(implementation->instanceData.size() % implementation->instanceSize == 0);
	return static_cast<uint32_t>(implementation->instanceData.size() / implementation->instanceSize);
}

InstanceBufferBase::InstanceBufferBase(Device&, size_t instanceSize)
	: implementation(InstanceBufferImplementation::create(instanceSize)) {}

uint32_t InstanceBufferBase::append(StridedSpan<const byte> instancesData, size_t instanceSize) {
	GREM_ASSERT(instanceSize > 0);
	GREM_ASSERT(instanceSize % sizeof(float) == 0);
	if (instancesData.empty()) {
		[[unlikely]];
		return 0;
	}

	detail::ensureExclusiveResourceAccess(
		implementation, InstanceBufferImplementation::clone,
		[&](InstanceBufferImplementation& oldBuffer, const InstanceBufferImplementation& buffer) -> void { oldBuffer = buffer; }, *implementation);

	const size_t byteOffset = implementation->instanceData.size();
	implementation->instanceData.resize(byteOffset + instancesData.size() * instanceSize);

	byte* output = implementation->instanceData.data() + byteOffset;
	if (instancesData.stride() == instanceSize) {
		memcpy(output, instancesData.base(), instancesData.size() * instanceSize);
	} else {
		const auto end = instancesData.end();
		for (auto it = instancesData.begin(); it != end; ++it) {
			memcpy(output, it.base(), instanceSize);
			output += instanceSize;
		}
	}
	return static_cast<uint32_t>(byteOffset / implementation->instanceSize);
}

void InstanceBufferBase::flush() const {}

void DrawCommandBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(implementation, DrawCommandBufferImplementation::create, [&](DrawCommandBufferImplementation&) -> void {});

	implementation->instanceRanges.clear();
}

DrawCommandBufferBase::DrawCommandBufferBase(Device&)
	: implementation(SharedPointer<DrawCommandBufferImplementation>::create()) {}

void DrawCommandBufferBase::append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset,
	uint32_t instanceCount) {
	if (instanceCount == 0) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation, DrawCommandBufferImplementation::clone,
		[&](DrawCommandBufferImplementation& oldBuffer, const DrawCommandBufferImplementation& buffer) -> void { oldBuffer = buffer; }, *implementation);

	ArrayList<DrawCommandBufferImplementation::InstanceRange>& instanceRanges = implementation->instanceRanges;
	if (!instanceRanges.empty()) {
		DrawCommandBufferImplementation::InstanceRange& lastInstanceRange = instanceRanges.back();
		if (shaderPipelineHandle == lastInstanceRange.shaderPipelineHandle && meshHandle == lastInstanceRange.meshHandle &&
			instanceOffset == lastInstanceRange.offset + lastInstanceRange.count) {
			lastInstanceRange.count += instanceCount;
			return;
		}
	}

	instanceRanges.push_back(DrawCommandBufferImplementation::InstanceRange{
		.shaderPipelineHandle = std::move(shaderPipelineHandle),
		.meshHandle = std::move(meshHandle),
		.offset = instanceOffset,
		.count = instanceCount,
	});
}

void DrawCommandBufferBase::flush() const {}

void UnorderedDrawCommandBufferBase::clear() {
	detail::ensureExclusiveResourceAccess(implementation, UnorderedDrawCommandBufferImplementation::create, [&](UnorderedDrawCommandBufferImplementation&) -> void {});

	for (auto it = implementation->instanceRanges.begin(); it != implementation->instanceRanges.end();) {
		const auto& [key, instanceRanges] = *it;
		if (key.meshHandle.use_count() <= 1) {
			it = implementation->instanceRanges.erase(it);
		} else {
			instanceRanges.clear();
			++it;
		}
	}
}

UnorderedDrawCommandBufferBase::UnorderedDrawCommandBufferBase(Device&)
	: implementation(UnorderedDrawCommandBufferImplementation::create()) {}

void UnorderedDrawCommandBufferBase::insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset,
	uint32_t instanceCount) {
	if (instanceCount == 0) {
		[[unlikely]];
		return;
	}

	detail::ensureExclusiveResourceAccess(
		implementation, UnorderedDrawCommandBufferImplementation::clone,
		[&](UnorderedDrawCommandBufferImplementation& oldBuffer, const UnorderedDrawCommandBufferImplementation& buffer) -> void { oldBuffer = buffer; }, *implementation);

	Buffer<UnorderedDrawCommandBufferImplementation::InstanceRange>& instanceRanges = implementation->instanceRanges[UnorderedDrawCommandBufferImplementation::Key{
		.shaderPipelineHandle = std::move(shaderPipelineHandle),
		.meshHandle = std::move(meshHandle),
	}];
	if (!instanceRanges.empty()) {
		UnorderedDrawCommandBufferImplementation::InstanceRange& lastInstanceRange = instanceRanges.back();
		if (instanceOffset == lastInstanceRange.offset + lastInstanceRange.count) {
			lastInstanceRange.count += instanceCount;
			return;
		}
	}

	instanceRanges.push_back(UnorderedDrawCommandBufferImplementation::InstanceRange{
		.offset = instanceOffset,
		.count = instanceCount,
	});
}

void UnorderedDrawCommandBufferBase::flush() const {}

} // namespace detail

} // namespace grem::graphics
