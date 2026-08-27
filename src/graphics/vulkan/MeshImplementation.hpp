// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_MESH_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_VULKAN_MESH_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/RangeAllocator.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "DeviceImplementation.hpp"

#include <typeindex> // std::type_index
#include <utility>   // std::move, std::exchange

namespace grem::graphics {

struct MeshImplementation : detail::ReusableCopyOnWriteResourceBase<MeshImplementation> {
	[[nodiscard]] static SharedPointer<MeshImplementation> create(Device& device, Optional<MeshIndexType> indexType, bool isInstanced, std::type_index meshTypeIndex,
		VertexAttributeMask activeVertexAttributes) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<MeshImplementation>::create(device, indexType, isInstanced, meshTypeIndex, activeVertexAttributes);
	}

	[[nodiscard]] static SharedPointer<MeshImplementation> clone(const MeshImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<MeshImplementation>::create(implementation);
	}

	Device& device;
	RangeAllocation<uint32_t> indexRange{};
	RangeAllocation<uint32_t> vertexRange{};
	RangeAllocation<uint32_t> parameterRange{};
	uint32_t vertexCount = 0;
	Optional<MeshIndexType> indexType;
	bool isInstanced;
	std::type_index meshTypeIndex;
	VertexAttributeMask activeVertexAttributes;
	ArrayList<SharedPointer<TextureImplementation>> textures{};

	MeshImplementation(Device& device, Optional<MeshIndexType> indexType, bool isInstanced, std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes)
		: device(device)
		, indexType(indexType)
		, isInstanced(isInstanced)
		, meshTypeIndex(meshTypeIndex)
		, activeVertexAttributes(activeVertexAttributes) {}

	~MeshImplementation() {
		device.get()->releaseIndices(meshTypeIndex, indexRange);
		device.get()->releaseVertices(meshTypeIndex, activeVertexAttributes, vertexRange);
		device.get()->releaseParameters(meshTypeIndex, parameterRange);
	}

	MeshImplementation(const MeshImplementation& other)
		: device(other.device)
		, indexRange(other.indexRange)
		, vertexRange(other.vertexRange)
		, parameterRange(other.parameterRange)
		, vertexCount(other.vertexCount)
		, indexType(other.indexType)
		, isInstanced(other.isInstanced)
		, meshTypeIndex(other.meshTypeIndex)
		, activeVertexAttributes(other.activeVertexAttributes)
		, textures(other.textures) {
		device.get()->reacquireIndices(meshTypeIndex, indexRange);
		device.get()->reacquireVertices(meshTypeIndex, activeVertexAttributes, vertexRange);
		device.get()->reacquireParameters(meshTypeIndex, parameterRange);
	}

	MeshImplementation(MeshImplementation&& other) noexcept
		: device(other.device)
		, indexRange(std::exchange(other.indexRange, {}))
		, vertexRange(std::exchange(other.vertexRange, {}))
		, parameterRange(std::exchange(other.parameterRange, {}))
		, vertexCount(std::exchange(other.vertexCount, uint32_t{0}))
		, indexType(other.indexType)
		, isInstanced(other.isInstanced)
		, meshTypeIndex(other.meshTypeIndex)
		, activeVertexAttributes(other.activeVertexAttributes)
		, textures(std::move(other.textures)) {}

	MeshImplementation& operator=(const MeshImplementation& other) {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return *this;
		}
		device.get()->releaseIndices(meshTypeIndex, indexRange);
		device.get()->releaseVertices(meshTypeIndex, activeVertexAttributes, vertexRange);
		device.get()->releaseParameters(meshTypeIndex, parameterRange);
		indexRange = other.indexRange;
		vertexRange = other.vertexRange;
		parameterRange = other.parameterRange;
		vertexCount = other.vertexCount;
		indexType = other.indexType;
		isInstanced = other.isInstanced;
		meshTypeIndex = other.meshTypeIndex;
		activeVertexAttributes = other.activeVertexAttributes;
		textures = other.textures;
		device.get()->reacquireIndices(other.meshTypeIndex, other.indexRange);
		device.get()->reacquireVertices(other.meshTypeIndex, other.activeVertexAttributes, other.vertexRange);
		device.get()->reacquireParameters(other.meshTypeIndex, other.parameterRange);
		return *this;
	}

	MeshImplementation& operator=(MeshImplementation&& other) noexcept {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return *this;
		}
		device.get()->releaseIndices(meshTypeIndex, indexRange);
		device.get()->releaseVertices(meshTypeIndex, activeVertexAttributes, vertexRange);
		device.get()->releaseParameters(meshTypeIndex, parameterRange);
		indexRange = std::exchange(other.indexRange, {});
		vertexRange = std::exchange(other.vertexRange, {});
		parameterRange = std::exchange(other.parameterRange, {});
		vertexCount = std::exchange(other.vertexCount, uint32_t{0});
		indexType = other.indexType;
		isInstanced = other.isInstanced;
		meshTypeIndex = other.meshTypeIndex;
		activeVertexAttributes = other.activeVertexAttributes;
		textures = std::move(other.textures);
		return *this;
	}
};

} // namespace grem::graphics

#endif
