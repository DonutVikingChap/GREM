// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "DeviceImplementation.hpp"
#include "MeshImplementation.hpp"

#include <utility> // std::move, std::exchange

namespace grem::graphics {

namespace detail {

MeshBase::MeshBase(Device& device, std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	size_t, Optional<MeshIndexType> indexType, size_t indexStride, Span<const ParameterDescription>, size_t parameterStride, size_t textureParameterCount,
	Span<const FieldDescription> instanceAttributeDescriptions, size_t)
	: implementation(MeshImplementation::create(device, indexType, !instanceAttributeDescriptions.empty(), meshTypeIndex, activeVertexAttributes)) {
	implementation->device.get()->declareMeshType(implementation->meshTypeIndex, implementation->activeVertexAttributes, vertexAttributeDescriptions, indexStride, parameterStride,
		textureParameterCount);
}

void MeshBase::uploadVertices(Span<const byte> vertexData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t vertexStride) {
	GREM_PROFILE_FUNCTION();

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);

	implementation->device.get()->releaseVertices(implementation->meshTypeIndex, implementation->activeVertexAttributes, std::exchange(implementation->vertexRange, {}));
	implementation->vertexRange = implementation->device.get()->uploadVertices(implementation->meshTypeIndex, implementation->activeVertexAttributes, vertexData, vertexCount,
		vertexAttributeDescriptions, vertexStride);
	implementation->vertexCount = vertexCount;
}

void MeshBase::uploadVertexAttributes(Span<const Span<const byte>> vertexAttributeData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	size_t) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(vertexAttributeData.size() == vertexAttributeDescriptions.size());

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);

	implementation->device.get()->releaseVertices(implementation->meshTypeIndex, implementation->activeVertexAttributes, std::exchange(implementation->vertexRange, {}));
	implementation->vertexRange = implementation->device.get()->uploadVertexAttributes(implementation->meshTypeIndex, implementation->activeVertexAttributes, vertexAttributeData,
		vertexCount, vertexAttributeDescriptions);
	implementation->vertexCount = vertexCount;
}

void MeshBase::uploadIndices(Span<const byte> indexData, uint32_t indexCount, size_t indexStride) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(implementation->indexType);

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);

	implementation->device.get()->releaseIndices(implementation->meshTypeIndex, std::exchange(implementation->indexRange, {}));
	implementation->indexRange = implementation->device.get()->uploadIndices(implementation->meshTypeIndex, indexData, indexCount, indexStride);
}

void MeshBase::uploadParameters(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures) {
	GREM_PROFILE_FUNCTION();

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);

	implementation->device.get()->releaseParameters(implementation->meshTypeIndex, std::exchange(implementation->parameterRange, {}));
	implementation->parameterRange = implementation->device.get()->uploadParameters(implementation->meshTypeIndex, newParameterValuesBytes, newTextures);

	implementation->textures.clear();
	for (SharedPointer<TextureImplementation>& texture : newTextures) {
		implementation->textures.push_back(std::move(texture));
	}
}

} // namespace detail

} // namespace grem::graphics
