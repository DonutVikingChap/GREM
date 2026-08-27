// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_SHADER_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_SHADER_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/shaders.hpp>

#include "objects.hpp"
#include "vulkan.hpp"

#include <typeindex> // std::type_index
#include <utility>   // std::move

namespace grem::graphics {

struct VertexShaderImplementation {
	Variant<detail::VulkanShaderModule, VkShaderModule> shaderModule;
	std::type_index meshTypeIndex;
	Optional<MeshIndexType> indexType;
	uint32_t instanceStride;
	Span<const ParameterDescription> parameterDescriptions;
	Span<const FieldDescription> instanceAttributeDescriptions;
	Span<const BufferLayoutReference> bufferLayouts;

	VertexShaderImplementation(Variant<detail::VulkanShaderModule, VkShaderModule> shaderModule, std::type_index meshTypeIndex, Optional<MeshIndexType> indexType,
		Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
		Span<const BufferLayoutReference> bufferLayouts)
		: shaderModule(std::move(shaderModule))
		, meshTypeIndex(meshTypeIndex)
		, indexType(indexType)
		, instanceStride(instanceStride)
		, parameterDescriptions(parameterDescriptions)
		, instanceAttributeDescriptions(instanceAttributeDescriptions)
		, bufferLayouts(bufferLayouts) {}
};

struct FragmentShaderImplementation {
	Variant<detail::VulkanShaderModule, VkShaderModule> shaderModule;
	std::type_index meshTypeIndex;
	Span<const BufferLayoutReference> bufferLayouts;

	explicit FragmentShaderImplementation(Variant<detail::VulkanShaderModule, VkShaderModule> shaderModule, std::type_index meshTypeIndex,
		Span<const BufferLayoutReference> bufferLayouts)
		: shaderModule(std::move(shaderModule))
		, meshTypeIndex(meshTypeIndex)
		, bufferLayouts(bufferLayouts) {}
};

struct ShaderPipelineImplementation {
	std::type_index meshTypeIndex;
	SharedPointer<VertexShaderImplementation> vertexShaderHandle;
	SharedPointer<FragmentShaderImplementation> fragmentShaderHandle;
	Span<const ConstantDescription> vertexShaderConstantDescriptions;
	Span<const ConstantDescription> fragmentShaderConstantDescriptions;
	Allocation<byte> vertexShaderConstantData;
	Allocation<byte> fragmentShaderConstantData;
	ShaderPipelineOptions shaderPipelineOptions;

	ShaderPipelineImplementation(std::type_index meshTypeIndex, SharedPointer<VertexShaderImplementation> vertexShaderHandle,
		Span<const ConstantDescription> vertexShaderConstantDescriptions, Span<const byte> vertexShaderConstantData,
		SharedPointer<FragmentShaderImplementation> fragmentShaderHandle, Span<const ConstantDescription> fragmentShaderConstantDescriptions,
		Span<const byte> fragmentShaderConstantData, const ShaderPipelineOptions& shaderPipelineOptions)
		: meshTypeIndex(meshTypeIndex)
		, vertexShaderHandle(std::move(vertexShaderHandle))
		, fragmentShaderHandle(std::move(fragmentShaderHandle))
		, vertexShaderConstantDescriptions(vertexShaderConstantDescriptions)
		, fragmentShaderConstantDescriptions(fragmentShaderConstantDescriptions)
		, vertexShaderConstantData(vertexShaderConstantData.begin(), vertexShaderConstantData.end())
		, fragmentShaderConstantData(fragmentShaderConstantData.begin(), fragmentShaderConstantData.end())
		, shaderPipelineOptions(shaderPipelineOptions) {
		if (this->vertexShaderHandle->meshTypeIndex != meshTypeIndex) {
			throw graphics::Error{"Vertex shader's mesh type does not match the shader pipeline."};
		}
		if (this->fragmentShaderHandle->meshTypeIndex != meshTypeIndex) {
			throw graphics::Error{"Fragment shader's mesh type does not match the shader pipeline."};
		}
	}
};

} // namespace grem::graphics

#endif
