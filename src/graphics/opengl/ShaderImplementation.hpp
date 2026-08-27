// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_SHADER_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_SHADER_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/shaders.hpp>

#include "objects.hpp"
#include "opengl.hpp"

#include <typeindex> // std::type_index
#include <utility>   // std::move

namespace grem::graphics {

struct VertexShaderImplementation {
	String sourceCode;
	std::type_index meshTypeIndex;
	Span<const VertexAttributeDescription> vertexAttributeDescriptions;
	Optional<MeshIndexType> indexType;
	uint32_t instanceStride;
	Span<const ParameterDescription> parameterDescriptions;
	Span<const FieldDescription> instanceAttributeDescriptions;
	Span<const FieldDescription> outputFieldDescriptions;
	Span<const BufferLayoutReference> bufferLayouts;

	VertexShaderImplementation(String sourceCode, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions,
		uint32_t instanceStride, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
		: sourceCode(std::move(sourceCode))
		, meshTypeIndex(meshTypeIndex)
		, vertexAttributeDescriptions(vertexAttributeDescriptions)
		, indexType(indexType)
		, instanceStride(instanceStride)
		, parameterDescriptions(parameterDescriptions)
		, instanceAttributeDescriptions(instanceAttributeDescriptions)
		, outputFieldDescriptions(outputFieldDescriptions)
		, bufferLayouts(bufferLayouts) {}
};

struct FragmentShaderImplementation {
	String sourceCode;
	std::type_index meshTypeIndex;
	Span<const FieldDescription> inputFieldDescriptions;
	Span<const FieldDescription> outputFieldDescriptions;
	Span<const BufferLayoutReference> bufferLayouts;

	FragmentShaderImplementation(String sourceCode, std::type_index meshTypeIndex, Span<const FieldDescription> inputFieldDescriptions,
		Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
		: sourceCode(std::move(sourceCode))
		, meshTypeIndex(meshTypeIndex)
		, inputFieldDescriptions(inputFieldDescriptions)
		, outputFieldDescriptions(outputFieldDescriptions)
		, bufferLayouts(bufferLayouts) {}
};

struct ShaderPipelineImplementation {
	detail::ProgramObject programObject = detail::createProgramObject();
	std::type_index meshTypeIndex;
	uint32_t instanceStride;
	Span<const ParameterDescription> parameterDescriptions;
	Span<const BufferLayoutReference> vertexShaderBufferLayouts;
	Span<const BufferLayoutReference> fragmentShaderBufferLayouts;
	ShaderPipelineOptions shaderPipelineOptions;
	GLint srgbCorrectionModeUniformLocation = -1;
	GLint framebufferHeightUniformLocation = -1;
	bool hasColorOutput = false;

	ShaderPipelineImplementation(std::type_index meshTypeIndex, uint32_t instanceStride, Span<const ParameterDescription> parameterDescriptions,
		Span<const BufferLayoutReference> vertexShaderBufferLayouts, Span<const BufferLayoutReference> fragmentShaderBufferLayouts,
		const ShaderPipelineOptions& shaderPipelineOptions)
		: meshTypeIndex(meshTypeIndex)
		, instanceStride(instanceStride)
		, parameterDescriptions(parameterDescriptions)
		, vertexShaderBufferLayouts(vertexShaderBufferLayouts)
		, fragmentShaderBufferLayouts(fragmentShaderBufferLayouts)
		, shaderPipelineOptions(shaderPipelineOptions) {}
};

} // namespace grem::graphics

#endif
