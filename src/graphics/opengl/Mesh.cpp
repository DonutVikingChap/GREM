// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Mesh.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "MeshImplementation.hpp"
#include "StatePreserver.hpp"
#include "opengl.hpp"

namespace grem::graphics {

namespace detail {

MeshBase::MeshBase(Device&, std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t,
	Optional<MeshIndexType> indexType, size_t, Span<const ParameterDescription> parameterDescriptions, size_t, size_t, Span<const FieldDescription> instanceAttributeDescriptions,
	size_t instanceStride)
	: implementation(MeshImplementation::create(meshTypeIndex, activeVertexAttributes, vertexAttributeDescriptions, indexType, parameterDescriptions, instanceAttributeDescriptions,
		  instanceStride)) {}

void MeshBase::uploadVertices(Span<const byte> vertexData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t vertexStride) {
	GREM_PROFILE_FUNCTION();

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::cloneWithoutVertices,
		[&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh.allocateWithoutVertices(mesh); }, *implementation);
	detail::ensureExclusiveResourceAccess(implementation->vertexBufferObject, MeshImplementation::BufferImplementation::create,
		[&](MeshImplementation::BufferImplementation&) -> void {});

	implementation->vertexAttributes.clear();
	uintptr_t offset = 0;
	for (const VertexAttributeDescription& vertexAttributeDescription : vertexAttributeDescriptions) {
		MeshImplementation::appendInterleavedVertexAttributes(implementation->vertexAttributes, offset, vertexAttributeDescription.type, static_cast<GLsizei>(vertexStride));
	}

	const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
	glBindVertexArray(implementation->vertexArrayObject.get());

	const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
	glBindBuffer(GL_ARRAY_BUFFER, implementation->vertexBufferObject->object.get());
	MeshImplementation::setupVertexAttributes(implementation->vertexAttributes);

	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexData.size_bytes()), vertexData.data(), GL_STATIC_DRAW);

	implementation->vertexCount = vertexCount;
}

void MeshBase::uploadVertexAttributes(Span<const Span<const byte>> vertexAttributeData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	size_t) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(vertexAttributeData.size() == vertexAttributeDescriptions.size());
	GREM_ASSERT(vertexAttributeData.size() <= implementation->activeVertexAttributes.size());

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::cloneWithoutVertices,
		[&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh.allocateWithoutVertices(mesh); }, *implementation);
	detail::ensureExclusiveResourceAccess(implementation->vertexBufferObject, MeshImplementation::BufferImplementation::create,
		[&](MeshImplementation::BufferImplementation&) -> void {});

	implementation->vertexAttributes.clear();
	uintptr_t baseOffset = 0;
	size_t bufferSize = 0;
	for (size_t i = 0; i < vertexAttributeDescriptions.size(); ++i) {
		const VertexAttributeDescription& vertexAttributeDescription = vertexAttributeDescriptions[i];
		const Span<const byte> data = vertexAttributeData[i];
		if (data.empty()) {
			GREM_ASSERT(!implementation->activeVertexAttributes[i]);
			uintptr_t initialBaseOffset = 0;
			MeshImplementation::appendDeinterleavedVertexAttributes(implementation->vertexAttributes, initialBaseOffset, vertexAttributeDescription.type, vertexCount);
			bufferSize = max(bufferSize, static_cast<size_t>(initialBaseOffset));
		} else {
			MeshImplementation::appendDeinterleavedVertexAttributes(implementation->vertexAttributes, baseOffset, vertexAttributeDescription.type, vertexCount);
		}
	}
	bufferSize = max(bufferSize, static_cast<size_t>(baseOffset));

	const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
	glBindVertexArray(implementation->vertexArrayObject.get());

	const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
	glBindBuffer(GL_ARRAY_BUFFER, implementation->vertexBufferObject->object.get());
	MeshImplementation::setupVertexAttributes(implementation->vertexAttributes);

	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bufferSize), nullptr, GL_STATIC_DRAW);
	size_t bufferOffset = 0;
	for (const Span<const byte> data : vertexAttributeData) {
		if (!data.empty()) {
			glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(bufferOffset), static_cast<GLsizeiptr>(data.size_bytes()), data.data());
			bufferOffset += data.size_bytes();
		}
	}

	implementation->vertexCount = vertexCount;
}

void MeshBase::uploadIndices(Span<const byte> indexData, uint32_t indexCount, size_t) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(implementation->indexType);

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);
	detail::ensureExclusiveResourceAccess(implementation->elementBufferObject, MeshImplementation::BufferImplementation::create,
		[&](MeshImplementation::BufferImplementation&) -> void {});

	const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
	glBindVertexArray(implementation->vertexArrayObject.get());

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, implementation->elementBufferObject->object.get());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexData.size_bytes()), indexData.data(), GL_STATIC_DRAW);

	implementation->indexCount = indexCount;
}

void MeshBase::uploadParameters(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures) {
	GREM_PROFILE_FUNCTION();

	detail::ensureExclusiveResourceAccess(
		implementation, MeshImplementation::clone, [&](MeshImplementation& oldMesh, const MeshImplementation& mesh) -> void { oldMesh = mesh; }, *implementation);
	detail::ensureExclusiveResourceAccess(implementation->uniformBufferObject, MeshImplementation::BufferImplementation::create,
		[&](MeshImplementation::BufferImplementation&) -> void {});

	const detail::UniformBufferBindingPreserver uniformBufferBindingPreserver{};
	glBindBuffer(GL_UNIFORM_BUFFER, implementation->uniformBufferObject->object.get());
	glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(newParameterValuesBytes.size_bytes()), newParameterValuesBytes.data(), GL_STATIC_DRAW);

	implementation->textures.clear();
	for (SharedPointer<TextureImplementation>& texture : newTextures) {
		implementation->textures.push_back(std::move(texture));
	}
}

} // namespace detail

} // namespace grem::graphics
