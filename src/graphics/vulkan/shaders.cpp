// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "DeviceImplementation.hpp"
#include "ShaderImplementation.hpp"
#include "VulkanError.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <new>       // std::launder
#include <typeindex> // std::type_index
#include <utility>   // std::move

#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
#include <SPIRV/GlslangToSpv.h>        // glslang::SpvOptions, glslang::GlslangToSpv
#include <cstring>                     // std::strlen
#include <glslang/Public/ShaderLang.h> // ESh..., glslang::...
#include <vector>                      // std::vector
#include <zstd.h>                      // ZSTD_...
#endif

namespace grem::graphics {

namespace detail {

namespace {

[[nodiscard]] detail::VulkanShaderModule createShader(VkDevice device, Span<const uint32_t> code) {
	const VkShaderModuleCreateInfo shaderModuleCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkShaderModuleCreateFlags{},
		.codeSize = code.size() * sizeof(uint32_t),
		.pCode = code.data(),
	};
	VkShaderModule shaderModuleHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModuleHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateShaderModule", result};
	}
	return detail::VulkanShaderModule{shaderModuleHandle, detail::VulkanShaderModuleDeleter{device}};
}

#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION

constexpr CStringView SHADER_HEADER = "#version 460\n#extension GL_EXT_nonuniform_qualifier : require\n";

constexpr CStringView SHADER_PROLOGUE = R"GLSL(
#define GREM_GRAPHICS_BACKEND_VULKAN
#define GREM_vertexIndex (gl_VertexIndex - gl_BaseVertex)
#define GREM_fragmentCoordinates (gl_FragCoord)
#define GREM_textureSample2D(name, coordinates) texture(name, GREM_private_translateTextureCoordinates2D(coordinates))
#define GREM_textureSample2DShadow(name, coordinates) texture(name, GREM_private_translateTextureCoordinates2DShadow(coordinates))
#define GREM_textureSample2DArray(name, coordinates) texture(name, GREM_private_translateTextureCoordinates2DArray(coordinates))
#define GREM_textureSample2DArrayShadow(name, coordinates) texture(name, GREM_private_translateTextureCoordinates2DArrayShadow(coordinates))
#define GREM_textureSampleCube(name, coordinates) texture(name, GREM_private_translateTextureCoordinatesCube(coordinates))
#define GREM_textureSampleCubeShadow(name, coordinates) texture(name, GREM_private_translateTextureCoordinatesCubeShadow(coordinates))
#define GREM_textureSampleCubeArray(name, coordinates) texture(name, GREM_private_translateTextureCoordinatesCubeArray(coordinates))
#define GREM_textureSampleCubeArrayShadow(name, coordinates, compare) texture(name, GREM_private_translateTextureCoordinatesCubeArray(coordinates), compare)
#define GREM_textureSampleLod2D(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinates2D(coordinates), lod)
#define GREM_textureSampleLod2DShadow(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinates2DShadow(coordinates), lod)
#define GREM_textureSampleLod2DArray(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinates2DArray(coordinates), lod)
#define GREM_textureSampleLod2DArrayShadow(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinates2DArrayShadow(coordinates), lod)
#define GREM_textureSampleLodCube(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinatesCube(coordinates), lod)
#define GREM_textureSampleLodCubeShadow(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinatesCubeShadow(coordinates), lod)
#define GREM_textureSampleLodCubeArray(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinatesCubeArray(coordinates), lod)
#define GREM_textureSampleLodCubeArrayFiltered(name, coordinates, lod) textureLod(name, GREM_private_translateTextureCoordinatesCubeArray(coordinates), lod)
#define GREM_textureSampleGrad2D(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinates2D(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinates2DShadow(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DArray(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinates2DArray(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DArrayShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinates2DArrayShadow(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCube(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinatesCube(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCubeShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinatesCubeShadow(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCubeArray(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_translateTextureCoordinatesCubeArray(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_texelFetch2D(name, coordinates, lod) texelFetch(name, GREM_private_translateTexelCoordinates2D(coordinates, textureSize(name, lod).y), lod)
#define GREM_texelFetch2DArray(name, coordinates, lod) texelFetch(name, GREM_private_translateTexelCoordinates2DArray(coordinates, textureSize(name, lod).y), lod)
#define main() GREM_private_main()

vec2 GREM_private_translateTextureCoordinates2D(vec2 coordinates) {
	return vec2(coordinates.x, 1.0 - coordinates.y);
}

vec3 GREM_private_translateTextureCoordinates2DShadow(vec3 coordinates) {
	return vec3(coordinates.x, 1.0 - coordinates.y, coordinates.z);
}

vec3 GREM_private_translateTextureCoordinates2DArray(vec3 coordinates) {
	return vec3(coordinates.x, 1.0 - coordinates.y, coordinates.z);
}

vec4 GREM_private_translateTextureCoordinates2DArrayShadow(vec4 coordinates) {
	return vec4(coordinates.x, 1.0 - coordinates.y, coordinates.z, coordinates.w);
}

vec3 GREM_private_translateTextureCoordinatesCube(vec3 coordinates) {
	return vec3(coordinates.x, -coordinates.y, coordinates.z);
}

vec4 GREM_private_translateTextureCoordinatesCubeShadow(vec4 coordinates) {
	return vec4(coordinates.x, -coordinates.y, coordinates.z, coordinates.w);
}

vec4 GREM_private_translateTextureCoordinatesCubeArray(vec4 coordinates) {
	return vec4(coordinates.x, -coordinates.y, coordinates.z, coordinates.w);
}

ivec2 GREM_private_translateTexelCoordinates2D(ivec2 coordinates, int height) {
	return ivec2(coordinates.x, height - 1 - coordinates.y);
}

ivec3 GREM_private_translateTexelCoordinates2DArray(ivec3 coordinates, int height) {
	return ivec3(coordinates.x, height - 1 - coordinates.y, coordinates.z);
}
)GLSL";

constexpr CStringView SHADER_EPILOGUE = R"GLSL(
#undef main
void main() {
	GREM_private_preMain();
	GREM_private_main();
}
)GLSL";

constexpr CStringView SHADER_LINE_DIRECTIVE = "\n#line 1\n";

class ShaderCompiler {
public:
	ShaderCompiler() {
		GREM_PROFILE_FUNCTION();

		if (!glslang::InitializeProcess()) {
			throw graphics::Error{"Failed to initialize shader compiler process."};
		}
	}

	~ShaderCompiler() {
		glslang::FinalizeProcess();
	}

	ShaderCompiler(const ShaderCompiler&) = delete;
	ShaderCompiler(ShaderCompiler&&) = delete;
	ShaderCompiler& operator=(const ShaderCompiler&) = delete;
	ShaderCompiler& operator=(ShaderCompiler&&) = delete;

	[[nodiscard]] std::vector<uint32_t> compileShaderStageGLSLToSPIRV(EShLanguage stage, Span<const char* const> sourceCodeStrings,
		const ShaderCompilationOptions& compilationOptions) {
		GREM_PROFILE_FUNCTION();

		constexpr int defaultVersion = 100;
		constexpr bool forwardCompatible = false;
		constexpr EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
		constexpr TBuiltInResource resource{
			.maxLights = 32,
			.maxClipPlanes = 6,
			.maxTextureUnits = 32,
			.maxTextureCoords = 32,
			.maxVertexAttribs = 64,
			.maxVertexUniformComponents = 4096,
			.maxVaryingFloats = 64,
			.maxVertexTextureImageUnits = 32,
			.maxCombinedTextureImageUnits = 80,
			.maxTextureImageUnits = 32,
			.maxFragmentUniformComponents = 4096,
			.maxDrawBuffers = 32,
			.maxVertexUniformVectors = 128,
			.maxVaryingVectors = 8,
			.maxFragmentUniformVectors = 16,
			.maxVertexOutputVectors = 16,
			.maxFragmentInputVectors = 15,
			.minProgramTexelOffset = -8,
			.maxProgramTexelOffset = 7,
			.maxClipDistances = 8,
			.maxComputeWorkGroupCountX = 65535,
			.maxComputeWorkGroupCountY = 65535,
			.maxComputeWorkGroupCountZ = 65535,
			.maxComputeWorkGroupSizeX = 1024,
			.maxComputeWorkGroupSizeY = 1024,
			.maxComputeWorkGroupSizeZ = 64,
			.maxComputeUniformComponents = 1024,
			.maxComputeTextureImageUnits = 16,
			.maxComputeImageUniforms = 8,
			.maxComputeAtomicCounters = 8,
			.maxComputeAtomicCounterBuffers = 1,
			.maxVaryingComponents = 60,
			.maxVertexOutputComponents = 64,
			.maxGeometryInputComponents = 64,
			.maxGeometryOutputComponents = 128,
			.maxFragmentInputComponents = 128,
			.maxImageUnits = 8,
			.maxCombinedImageUnitsAndFragmentOutputs = 8,
			.maxCombinedShaderOutputResources = 8,
			.maxImageSamples = 0,
			.maxVertexImageUniforms = 0,
			.maxTessControlImageUniforms = 0,
			.maxTessEvaluationImageUniforms = 0,
			.maxGeometryImageUniforms = 0,
			.maxFragmentImageUniforms = 8,
			.maxCombinedImageUniforms = 8,
			.maxGeometryTextureImageUnits = 16,
			.maxGeometryOutputVertices = 256,
			.maxGeometryTotalOutputComponents = 1024,
			.maxGeometryUniformComponents = 1024,
			.maxGeometryVaryingComponents = 64,
			.maxTessControlInputComponents = 128,
			.maxTessControlOutputComponents = 128,
			.maxTessControlTextureImageUnits = 16,
			.maxTessControlUniformComponents = 1024,
			.maxTessControlTotalOutputComponents = 4096,
			.maxTessEvaluationInputComponents = 128,
			.maxTessEvaluationOutputComponents = 128,
			.maxTessEvaluationTextureImageUnits = 16,
			.maxTessEvaluationUniformComponents = 1024,
			.maxTessPatchComponents = 120,
			.maxPatchVertices = 32,
			.maxTessGenLevel = 64,
			.maxViewports = 16,
			.maxVertexAtomicCounters = 0,
			.maxTessControlAtomicCounters = 0,
			.maxTessEvaluationAtomicCounters = 0,
			.maxGeometryAtomicCounters = 0,
			.maxFragmentAtomicCounters = 8,
			.maxCombinedAtomicCounters = 8,
			.maxAtomicCounterBindings = 1,
			.maxVertexAtomicCounterBuffers = 0,
			.maxTessControlAtomicCounterBuffers = 0,
			.maxTessEvaluationAtomicCounterBuffers = 0,
			.maxGeometryAtomicCounterBuffers = 0,
			.maxFragmentAtomicCounterBuffers = 1,
			.maxCombinedAtomicCounterBuffers = 1,
			.maxAtomicCounterBufferSize = 16384,
			.maxTransformFeedbackBuffers = 4,
			.maxTransformFeedbackInterleavedComponents = 64,
			.maxCullDistances = 8,
			.maxCombinedClipAndCullDistances = 8,
			.maxSamples = 4,
			.maxMeshOutputVerticesNV = 256,
			.maxMeshOutputPrimitivesNV = 512,
			.maxMeshWorkGroupSizeX_NV = 32,
			.maxMeshWorkGroupSizeY_NV = 1,
			.maxMeshWorkGroupSizeZ_NV = 1,
			.maxTaskWorkGroupSizeX_NV = 32,
			.maxTaskWorkGroupSizeY_NV = 1,
			.maxTaskWorkGroupSizeZ_NV = 1,
			.maxMeshViewCountNV = 4,
			.maxMeshOutputVerticesEXT = 0,
			.maxMeshOutputPrimitivesEXT = 0,
			.maxMeshWorkGroupSizeX_EXT = 0,
			.maxMeshWorkGroupSizeY_EXT = 0,
			.maxMeshWorkGroupSizeZ_EXT = 0,
			.maxTaskWorkGroupSizeX_EXT = 0,
			.maxTaskWorkGroupSizeY_EXT = 0,
			.maxTaskWorkGroupSizeZ_EXT = 0,
			.maxMeshViewCountEXT = 0,
			.maxDualSourceDrawBuffersEXT = 0,
			.limits =
				TLimits{
					.nonInductiveForLoops = true,
					.whileLoops = true,
					.doWhileLoops = true,
					.generalUniformIndexing = true,
					.generalAttributeMatrixVectorIndexing = true,
					.generalVaryingIndexing = true,
					.generalSamplerIndexing = true,
					.generalVariableIndexing = true,
					.generalConstantMatrixVectorIndexing = true,
				},
		};

		glslang::TShader shader{stage};
		shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, defaultVersion);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_2);
		shader.setStrings(sourceCodeStrings.data(), static_cast<int>(sourceCodeStrings.size()));

		if (!shader.parse(&resource, defaultVersion, forwardCompatible, messages)) {
			const char* const info_log = shader.getInfoLog();
			throw graphics::Error{info_log};
		}

		glslang::TProgram program{};
		program.addShader(&shader);

		if (!program.link(messages)) {
			const char* const info_log = shader.getInfoLog();
			throw graphics::Error{info_log};
		}

		std::vector<uint32_t> result{};
		glslang::SpvOptions options{
			.generateDebugInfo = !compilationOptions.optimize,
			.stripDebugInfo = compilationOptions.optimize,
			.disableOptimizer = !compilationOptions.optimize,
			.optimizeSize = compilationOptions.optimize,
			.optimizePerformance = compilationOptions.optimize,
			.disassemble = false,
			.validate = true,
			.emitNonSemanticShaderDebugInfo = false,
			.emitNonSemanticShaderDebugSource = false,
			.compileOnly = false,
			.optimizerAllowExpandedIDBound = false,
		};
		glslang::GlslangToSpv(*program.getIntermediate(stage), result, &options);
		return result;
	}
};

[[nodiscard]] StringView getParameterTypeName(ParameterType parameterType) noexcept {
	switch (parameterType) {
		case ParameterType::INT: return "int";
		case ParameterType::IVEC2: return "ivec2";
		case ParameterType::IVEC3: return "ivec3";
		case ParameterType::IVEC4: return "ivec4";
		case ParameterType::UINT: return "uint";
		case ParameterType::UVEC2: return "uvec2";
		case ParameterType::UVEC3: return "uvec3";
		case ParameterType::UVEC4: return "uvec4";
		case ParameterType::FLOAT: return "float";
		case ParameterType::VEC2: return "vec2";
		case ParameterType::VEC3: return "vec3";
		case ParameterType::VEC4: return "vec4";
		case ParameterType::MAT2: return "mat2";
		case ParameterType::MAT3: return "mat3";
		case ParameterType::MAT4: return "mat4";
		case ParameterType::SAMPLER_2D: return "sampler2D";
		case ParameterType::SAMPLER_2D_SHADOW: return "sampler2DShadow";
		case ParameterType::SAMPLER_2D_ARRAY: return "sampler2DArray";
		case ParameterType::SAMPLER_2D_ARRAY_SHADOW: return "sampler2DArrayShadow";
		case ParameterType::SAMPLER_CUBE: return "samplerCube";
		case ParameterType::SAMPLER_CUBE_SHADOW: return "samplerCubeShadow";
		case ParameterType::SAMPLER_CUBE_ARRAY: return "samplerCubeArray";
		case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW: return "samplerCubeArrayShadow";
	}
	return {};
}

void writeSpecializationConstantDeclarations(String& output, Span<const ConstantDescription> constantDescriptions) {
	if (!constantDescriptions.empty()) {
		uint32_t constantID = 0;
		for (const ConstantDescription& constantDescription : constantDescriptions) {
			switch (constantDescription.type) {
				case ConstantType::BOOL: output.append(formatString("layout(constant_id = {}) const bool {} = false;\n", constantID, constantDescription.name)); break;
				case ConstantType::INT: output.append(formatString("layout(constant_id = {}) const int {} = 0;\n", constantID, constantDescription.name)); break;
				case ConstantType::UINT: output.append(formatString("layout(constant_id = {}) const uint {} = 0u;\n", constantID, constantDescription.name)); break;
				case ConstantType::FLOAT: output.append(formatString("layout(constant_id = {}) const float {} = 0.;\n", constantID, constantDescription.name)); break;
			}
			++constantID;
		}
		output.push_back('\n');
	}
}

void writeIODeclarations(String& output, size_t& location, StringView qualifier, Span<const FieldDescription> fieldDescriptions) {
	GREM_PROFILE_FUNCTION();

	if (!fieldDescriptions.empty()) {
		for (const FieldDescription& fieldDescription : fieldDescriptions) {
			size_t locationsPerElement = 0;
			switch (fieldDescription.type) {
				case FieldType::INT:
					output.append(formatString("layout(location = {}) flat {} int ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::IVEC2:
					output.append(formatString("layout(location = {}) flat {} ivec2 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::IVEC3:
					output.append(formatString("layout(location = {}) flat {} ivec3 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::IVEC4:
					output.append(formatString("layout(location = {}) flat {} ivec4 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::UINT:
					output.append(formatString("layout(location = {}) flat {} uint ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::UVEC2:
					output.append(formatString("layout(location = {}) flat {} uvec2 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::UVEC3:
					output.append(formatString("layout(location = {}) flat {} uvec3 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::UVEC4:
					output.append(formatString("layout(location = {}) flat {} uvec4 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::FLOAT:
					output.append(formatString("layout(location = {}) {} float ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::VEC2:
					output.append(formatString("layout(location = {}) {} vec2 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::VEC3:
					output.append(formatString("layout(location = {}) {} vec3 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::VEC4:
					output.append(formatString("layout(location = {}) {} vec4 ", location, qualifier));
					locationsPerElement = 1;
					break;
				case FieldType::MAT2:
					output.append(formatString("layout(location = {}) {} mat2 ", location, qualifier));
					locationsPerElement = 2;
					break;
				case FieldType::MAT3:
					output.append(formatString("layout(location = {}) {} mat3 ", location, qualifier));
					locationsPerElement = 3;
					break;
				case FieldType::MAT4:
					output.append(formatString("layout(location = {}) {} mat4 ", location, qualifier));
					locationsPerElement = 4;
					break;
			}
			output.append(fieldDescription.name);
			if (fieldDescription.arrayElementCount == 0) {
				location += locationsPerElement;
			} else {
				output.append(formatString("[{}]", fieldDescription.arrayElementCount));
				location += locationsPerElement * fieldDescription.arrayElementCount;
			}
			output.append(";\n");
		}
		output.push_back('\n');
	}
}

void writeMeshParametersDeclarations(String& output, size_t& descriptorSetIndex, Span<const ParameterDescription> parameterDescriptions) {
	GREM_PROFILE_FUNCTION();

	bool hasAnyTextureParameter = false;
	bool hasAnyNonTextureParameter = false;
	for (const ParameterDescription& parameterDescription : parameterDescriptions) {
		if (!isValidName(parameterDescription.name)) {
			throw graphics::Error{formatString("Invalid mesh parameter name \"{}\".", parameterDescription.name)};
		}
		if (isTextureParameter(parameterDescription.type)) {
			hasAnyTextureParameter = true;
		} else {
			hasAnyNonTextureParameter = true;
		}
	}

	size_t descriptorBindingIndex = 0;

	if (hasAnyNonTextureParameter) {
		output.append("struct GREM_private_MeshParametersStruct {\n");
		for (const ParameterDescription& parameterDescription : parameterDescriptions) {
			if (!isTextureParameter(parameterDescription.type)) {
				if (parameterDescription.arrayElementCount == 0) {
					output.append(formatString("    {} {};\n", getParameterTypeName(parameterDescription.type), parameterDescription.name));
				} else {
					output.append(
						formatString("    {} {}[{}];\n", getParameterTypeName(parameterDescription.type), parameterDescription.name, parameterDescription.arrayElementCount));
				}
			}
		}
		output.append(formatString(
			"}};\n"
			"layout(std140, set = {}, binding = {}) readonly buffer GREM_private_MeshParameters {{ GREM_private_MeshParametersStruct GREM_private_meshParameters[]; }};\n",
			descriptorSetIndex, descriptorBindingIndex));
		++descriptorBindingIndex;

		for (const ParameterDescription& parameterDescription : parameterDescriptions) {
			if (!isTextureParameter(parameterDescription.type)) {
				output.append(formatString("#define {0} (GREM_private_meshParameters[GREM_private_meshParametersIndex].{0})\n", parameterDescription.name));
			}
		}
	}

	if (hasAnyTextureParameter) {
		output.append(formatString(
			"layout(set = {0}, binding = {1}) uniform sampler2D GREM_private_meshTextureSamplers2D[];\n"
			"layout(set = {0}, binding = {1}) uniform sampler2DShadow GREM_private_meshTextureSamplers2DShadow[];\n"
			"layout(set = {0}, binding = {1}) uniform sampler2DArray GREM_private_meshTextureSamplers2DArray[];\n"
			"layout(set = {0}, binding = {1}) uniform sampler2DArrayShadow GREM_private_meshTextureSamplers2DArrayShadow[];\n"
			"layout(set = {0}, binding = {1}) uniform samplerCube GREM_private_meshTextureSamplersCube[];\n"
			"layout(set = {0}, binding = {1}) uniform samplerCubeShadow GREM_private_meshTextureSamplersCubeShadow[];\n"
			"layout(set = {0}, binding = {1}) uniform samplerCubeArray GREM_private_meshTextureSamplersCubeArray[];\n"
			"layout(set = {0}, binding = {1}) uniform samplerCubeArrayShadow GREM_private_meshTextureSamplersCubeArrayShadow[];\n"
			"uint GREM_private_meshParametersTextureOffset;\n",
			descriptorSetIndex, descriptorBindingIndex));
		++descriptorBindingIndex;

		size_t textureParameterOffset = 0;
		for (const ParameterDescription& parameterDescription : parameterDescriptions) {
			if (!isTextureParameter(parameterDescription.type)) {
				continue;
			}

			if (parameterDescription.arrayElementCount == 0) {
				switch (parameterDescription.type) {
					case ParameterType::INT: [[fallthrough]];
					case ParameterType::IVEC2: [[fallthrough]];
					case ParameterType::IVEC3: [[fallthrough]];
					case ParameterType::IVEC4: [[fallthrough]];
					case ParameterType::UINT: [[fallthrough]];
					case ParameterType::UVEC2: [[fallthrough]];
					case ParameterType::UVEC3: [[fallthrough]];
					case ParameterType::UVEC4: [[fallthrough]];
					case ParameterType::FLOAT: [[fallthrough]];
					case ParameterType::VEC2: [[fallthrough]];
					case ParameterType::VEC3: [[fallthrough]];
					case ParameterType::VEC4: [[fallthrough]];
					case ParameterType::MAT2: [[fallthrough]];
					case ParameterType::MAT3: [[fallthrough]];
					case ParameterType::MAT4: unreachable();
					case ParameterType::SAMPLER_2D:
						output.append(formatString("#define {} GREM_private_meshTextureSamplers2D[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_SHADOW:
						output.append(formatString("#define {} GREM_private_meshTextureSamplers2DShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_ARRAY:
						output.append(formatString("#define {} GREM_private_meshTextureSamplers2DArray[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_ARRAY_SHADOW:
						output.append(formatString("#define {} GREM_private_meshTextureSamplers2DArrayShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE:
						output.append(formatString("#define {} GREM_private_meshTextureSamplersCube[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_SHADOW:
						output.append(formatString("#define {} GREM_private_meshTextureSamplersCubeShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_ARRAY:
						output.append(formatString("#define {} GREM_private_meshTextureSamplersCubeArray[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW:
						output.append(formatString("#define {} GREM_private_meshTextureSamplersCubeArrayShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u)]\n",
							parameterDescription.name, textureParameterOffset));
						break;
				}
				++textureParameterOffset;
			} else {
				switch (parameterDescription.type) {
					case ParameterType::INT: [[fallthrough]];
					case ParameterType::IVEC2: [[fallthrough]];
					case ParameterType::IVEC3: [[fallthrough]];
					case ParameterType::IVEC4: [[fallthrough]];
					case ParameterType::UINT: [[fallthrough]];
					case ParameterType::UVEC2: [[fallthrough]];
					case ParameterType::UVEC3: [[fallthrough]];
					case ParameterType::UVEC4: [[fallthrough]];
					case ParameterType::FLOAT: [[fallthrough]];
					case ParameterType::VEC2: [[fallthrough]];
					case ParameterType::VEC3: [[fallthrough]];
					case ParameterType::VEC4: [[fallthrough]];
					case ParameterType::MAT2: [[fallthrough]];
					case ParameterType::MAT3: [[fallthrough]];
					case ParameterType::MAT4: unreachable();
					case ParameterType::SAMPLER_2D:
						output.append(
							formatString("#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplers2D[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
										 "uint(GREM_private_arrayIndex))]\n",
								parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_SHADOW:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplers2DShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_ARRAY:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplers2DArray[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_2D_ARRAY_SHADOW:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplers2DArrayShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE:
						output.append(
							formatString("#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplersCube[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
										 "uint(GREM_private_arrayIndex))]\n",
								parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_SHADOW:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplersCubeShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_ARRAY:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplersCubeArray[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
					case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW:
						output.append(formatString(
							"#define {}(GREM_private_arrayIndex) GREM_private_meshTextureSamplersCubeArrayShadow[nonuniformEXT(GREM_private_meshParametersTextureOffset + {}u + "
							"uint(GREM_private_arrayIndex))]\n",
							parameterDescription.name, textureParameterOffset));
						break;
				}
				textureParameterOffset += parameterDescription.arrayElementCount;
			}
		}
	}

	++descriptorSetIndex;
}

void writeMeshDeclarations(String& output, size_t& descriptorSetIndex, Span<const ParameterDescription> parameterDescriptions,
	Span<const FieldDescription> instanceAttributeDescriptions) {
	GREM_PROFILE_FUNCTION();

	if (!instanceAttributeDescriptions.empty() || !parameterDescriptions.empty()) {
		output.append("struct GREM_private_DrawCommandStruct {\n");
		if (!instanceAttributeDescriptions.empty()) {
			output.append("    uint instanceIndex;\n");
		}
		if (!parameterDescriptions.empty()) {
			output.append("    uint meshParametersIndex;\n");
		}
		output.append(
			formatString("}};\n"
						 "layout(std430, set = {}, binding = 0) readonly buffer GREM_private_DrawCommands {{ GREM_private_DrawCommandStruct GREM_private_drawCommands[]; }};\n",
				descriptorSetIndex));
		++descriptorSetIndex;
		output.push_back('\n');
	}

	if (!instanceAttributeDescriptions.empty()) {
		for (const FieldDescription& fieldDescription : instanceAttributeDescriptions) {
			if (fieldDescription.arrayElementCount == 0) {
				output.append(formatString("{} {};\n", getFieldTypeName(fieldDescription.type), fieldDescription.name));
			} else {
				output.append(formatString("{} {}[{}];\n", getFieldTypeName(fieldDescription.type), fieldDescription.name, fieldDescription.arrayElementCount));
			}
		}
		output.append(
			formatString("layout(std140, set = {}, binding = 0) readonly buffer GREM_private_Instances {{ vec4 GREM_private_instanceValues[]; }};\n", descriptorSetIndex));
		++descriptorSetIndex;
		writeVec4BufferGetters(output, "GREM_private_getInstanceAttribute", instanceAttributeDescriptions,
			[](String& output, StringView nameString, StringView indexString) -> void {
				output.append(formatString("vec4 {} = GREM_private_instanceValues[{}];", nameString, indexString));
			});
	}

	if (!parameterDescriptions.empty()) {
		writeMeshParametersDeclarations(output, descriptorSetIndex, parameterDescriptions);
		output.push_back('\n');
	}
}

void writeUniformBufferDeclarations(String& output, size_t descriptorSetIndex, size_t& descriptorBindingIndex, CStringView blockName,
	Span<const ParameterDescription> parameterDescriptions) {
	GREM_PROFILE_FUNCTION();

	if (anyOf(parameterDescriptions, [](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
		output.append(formatString("layout(std140, set = {}, binding = {}) uniform {} {{\n", descriptorSetIndex, descriptorBindingIndex, blockName));
		++descriptorBindingIndex;
		for (const ParameterDescription& parameterDescription : parameterDescriptions) {
			if (!isTextureParameter(parameterDescription.type)) {
				if (!isValidName(parameterDescription.name)) {
					throw graphics::Error{formatString("Invalid parameter name \"{}\".", parameterDescription.name)};
				}
				if (parameterDescription.arrayElementCount == 0) {
					output.append(formatString("    {} {};\n", getParameterTypeName(parameterDescription.type), parameterDescription.name));
				} else {
					output.append(
						formatString("    {} {}[{}];\n", getParameterTypeName(parameterDescription.type), parameterDescription.name, parameterDescription.arrayElementCount));
				}
			}
		}
		output.append("};\n");
	}
	for (const ParameterDescription& parameterDescription : parameterDescriptions) {
		if (isTextureParameter(parameterDescription.type)) {
			if (!isValidName(parameterDescription.name)) {
				throw graphics::Error{formatString("Invalid parameter name \"{}\".", parameterDescription.name)};
			}
			if (parameterDescription.arrayElementCount == 0) {
				output.append(formatString("layout(set = {}, binding = {}) uniform {} {};\n", descriptorSetIndex, descriptorBindingIndex,
					getParameterTypeName(parameterDescription.type), parameterDescription.name));
			} else {
				output.append(formatString(
					"layout(set = {0}, binding = {1}) uniform {2} {3}Array[{4}];\n"
					"#define {3}(GREM_private_arrayIndex) {3}Array[GREM_private_arrayIndex]\n",
					descriptorSetIndex, descriptorBindingIndex, getParameterTypeName(parameterDescription.type), parameterDescription.name,
					parameterDescription.arrayElementCount));
			}
			++descriptorBindingIndex;
		}
	}
	output.push_back('\n');
}

void writeStorageBufferDeclarations(String& output, size_t descriptorSetIndex, size_t& descriptorBindingIndex, CStringView bufferName,
	Span<const FieldDescription> fieldDescriptions) {
	GREM_PROFILE_FUNCTION();

	output.append(
		formatString("layout(std140, set = {0}, binding = {1}) readonly buffer {2}Block {{ vec4 {2}Values[]; }};\n", descriptorSetIndex, descriptorBindingIndex, bufferName));
	++descriptorBindingIndex;

	writeVec4BufferGetters(output, {}, fieldDescriptions, [&](String& output, StringView nameString, StringView indexString) -> void {
		output.append(formatString("vec4 {} = {}Values[{}];", nameString, bufferName, indexString));
	});
}

void writeBufferSetDeclarations(String& output, size_t descriptorSetIndex, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_FUNCTION();

	size_t descriptorBindingIndex = 0;
	for (const BufferLayoutReference& bufferLayout : bufferLayouts) {
		GREM_MATCH(bufferLayout) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				if (!isValidName(uniformBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", uniformBufferLayout.name)};
				}
				if (!uniformBufferLayout.parameterDescriptions.empty()) {
					writeUniformBufferDeclarations(output, descriptorSetIndex, descriptorBindingIndex, formatString("GREM_private_{}Block", uniformBufferLayout.name),
						uniformBufferLayout.parameterDescriptions);
				}
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
				if (!isValidName(storageBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", storageBufferLayout.name)};
				}
				if (!storageBufferLayout.fieldDescriptions.empty()) {
					writeStorageBufferDeclarations(output, descriptorSetIndex, descriptorBindingIndex, formatString("GREM_private_{}", storageBufferLayout.name),
						storageBufferLayout.fieldDescriptions);
				}
				break;
			}
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				unreachable();
			}
		}
	}
}

void writeBufferDeclarations(String& output, size_t& descriptorSetIndex, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_FUNCTION();

	for (const BufferLayoutReference& bufferLayout : bufferLayouts) {
		GREM_MATCH(bufferLayout) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				if (!isValidName(uniformBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", uniformBufferLayout.name)};
				}
				if (!uniformBufferLayout.parameterDescriptions.empty()) {
					size_t descriptorBindingIndex = 0;
					writeUniformBufferDeclarations(output, descriptorSetIndex, descriptorBindingIndex, formatString("GREM_private_{}Block", uniformBufferLayout.name),
						uniformBufferLayout.parameterDescriptions);
					++descriptorSetIndex;
				}
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
				if (!isValidName(storageBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", storageBufferLayout.name)};
				}
				if (!storageBufferLayout.fieldDescriptions.empty()) {
					size_t descriptorBindingIndex = 0;
					writeStorageBufferDeclarations(output, descriptorSetIndex, descriptorBindingIndex, formatString("GREM_private_{}", storageBufferLayout.name),
						storageBufferLayout.fieldDescriptions);
					++descriptorSetIndex;
				}
				break;
			}
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				writeBufferSetDeclarations(output, descriptorSetIndex, bufferSetLayout.bufferLayouts);
				++descriptorSetIndex;
				break;
			}
		}
	}
}

[[nodiscard]] String generateVertexShaderPrologue(Span<const ConstantDescription> constantDescriptions, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, Span<const FieldDescription> outputFieldDescriptions,
	Span<const BufferLayoutReference> bufferLayouts) {
	String prologue{};

	writeSpecializationConstantDeclarations(prologue, constantDescriptions);

	size_t attributeIndex = 0;
	writeInputAttributeDeclarations(prologue, attributeIndex, vertexAttributeDescriptions);

	size_t outputLocation = 0;
	writeIODeclarations(prologue, outputLocation, "out", outputFieldDescriptions);
	if (!parameterDescriptions.empty()) {
		prologue.append(formatString("layout(location = {}) flat out uint GREM_private_meshParametersIndex;\n\n", outputLocation));
	}

	size_t descriptorSetIndex = 0;
	writeMeshDeclarations(prologue, descriptorSetIndex, parameterDescriptions, instanceAttributeDescriptions);
	writeBufferDeclarations(prologue, descriptorSetIndex, bufferLayouts);

	prologue.append("void GREM_private_preMain() {\n");
	if (!instanceAttributeDescriptions.empty()) {
		prologue.append("    uint GREM_private_instanceIndex = GREM_private_drawCommands[gl_InstanceIndex].instanceIndex;\n");
		for (const FieldDescription& attributeDescription : instanceAttributeDescriptions) {
			if (attributeDescription.arrayElementCount == 0) {
				prologue.append(formatString("    {0} = GREM_private_getInstanceAttribute{0}(GREM_private_instanceIndex);\n", attributeDescription.name));
			} else {
				for (size_t i = 0; i < attributeDescription.arrayElementCount; ++i) {
					prologue.append(formatString("    {0}[{1}] = GREM_private_getInstanceAttribute{0}(GREM_private_instanceIndex)[{1}];\n", attributeDescription.name, i));
				}
			}
		}
	}
	if (!parameterDescriptions.empty()) {
		prologue.append("    GREM_private_meshParametersIndex = GREM_private_drawCommands[gl_InstanceIndex].meshParametersIndex;\n");
		size_t textureParameterCount = 0;
		for (const ParameterDescription& parameterDescription : parameterDescriptions) {
			if (isTextureParameter(parameterDescription.type)) {
				textureParameterCount += max(parameterDescription.arrayElementCount, size_t{1});
			}
		}
		if (textureParameterCount > 0) {
			prologue.append(formatString("    GREM_private_meshParametersTextureOffset = GREM_private_meshParametersIndex * {}u;\n", textureParameterCount));
		}
	}
	prologue.append("}\n");

	return prologue;
}

[[nodiscard]] String generateFragmentShaderPrologue(Span<const ConstantDescription> constantDescriptions, Span<const ParameterDescription> parameterDescriptions,
	Span<const FieldDescription> instanceAttributeDescriptions, Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions,
	Span<const BufferLayoutReference> bufferLayouts) {
	String prologue{};

	writeSpecializationConstantDeclarations(prologue, constantDescriptions);

	size_t inputLocation = 0;
	writeIODeclarations(prologue, inputLocation, "in", inputFieldDescriptions);
	size_t outputLocation = 0;
	writeIODeclarations(prologue, outputLocation, "out", outputFieldDescriptions);
	if (!parameterDescriptions.empty()) {
		prologue.append(formatString("layout(location = {}) flat in uint GREM_private_meshParametersIndex;\n\n", inputLocation));
	}

	size_t descriptorSetIndex = 0;
	if (!instanceAttributeDescriptions.empty() || !parameterDescriptions.empty()) {
		++descriptorSetIndex;
	}
	if (!instanceAttributeDescriptions.empty()) {
		++descriptorSetIndex;
	}
	if (!parameterDescriptions.empty()) {
		writeMeshParametersDeclarations(prologue, descriptorSetIndex, parameterDescriptions);
	}
	writeBufferDeclarations(prologue, descriptorSetIndex, bufferLayouts);

	prologue.append("void GREM_private_preMain() {\n");
	size_t textureParameterCount = 0;
	for (const ParameterDescription& parameterDescription : parameterDescriptions) {
		if (isTextureParameter(parameterDescription.type)) {
			textureParameterCount += max(parameterDescription.arrayElementCount, size_t{1});
		}
	}
	if (textureParameterCount > 0) {
		prologue.append(formatString("    GREM_private_meshParametersTextureOffset = GREM_private_meshParametersIndex * {}u;\n", textureParameterCount));
	}
	prologue.append("}\n");

	return prologue;
}

[[nodiscard]] VkShaderModule compileShader(DeviceImplementation& device, DeviceImplementation::ShaderType type, EShLanguage stage, CStringView prologue, CStringView sourceCode,
	const Filesystem* filesystem, CStringView filepath) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Compile shader {}", filepath));

	detail::AllocatedStringBuffer allocatedStrings{};
	detail::ExpandedStringBuffer sourceStrings{
		SHADER_HEADER.c_str(),
		SHADER_PROLOGUE.c_str(),
		prologue.c_str(),
		SHADER_LINE_DIRECTIVE.c_str(),
		sourceCode.c_str(),
		SHADER_EPILOGUE.c_str(),
	};
	sourceStrings = detail::expandIncludes(allocatedStrings, sourceStrings, filesystem, filepath);

	uint64_t sourceSizeInBytes = 0;
	CRC32 sourceCRC32{};
	for (const char* const sourceString : sourceStrings) {
		const size_t length = std::strlen(sourceString);
		sourceSizeInBytes += static_cast<uint64_t>(length);
		sourceCRC32.append(Span<const byte>{std::launder(reinterpret_cast<const byte*>(sourceString)), length});
	}

	const DeviceImplementation::ShaderKey key{
		.type = type,
		.sourceSizeInBytes = sourceSizeInBytes,
		.sourceCRC32 = sourceCRC32,
	};
	const auto [it, inserted] = device.shaderCache.try_emplace(key);
	if (inserted) {
		try {
			const ShaderCompilationOptions compilationOptions{
#ifdef NDEBUG
				.optimize = true,
#else
				.optimize = false,
#endif
			};
			const std::vector<uint32_t> code = ShaderCompiler{}.compileShaderStageGLSLToSPIRV(stage, sourceStrings, compilationOptions);

			const size_t uncompressedCodeSizeInBytes = code.size() * sizeof(uint32_t);
			const DeviceImplementation::ShaderCompressionMode compressionMode =
				(uncompressedCodeSizeInBytes == 0) ? DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED : DeviceImplementation::ShaderCompressionMode::ZSTANDARD;

			Allocation<byte> compressedCode{};
			if (device.filesystem && !device.shaderCacheOutputFilepath.empty()) {
				switch (compressionMode) {
					case DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED: break;
					case DeviceImplementation::ShaderCompressionMode::ZSTANDARD: {
						compressedCode.resize(ZSTD_compressBound(uncompressedCodeSizeInBytes));
						const size_t codeCompressedSizeInBytes =
							ZSTD_compress(compressedCode.data(), compressedCode.size(), code.data(), uncompressedCodeSizeInBytes, ZSTD_CLEVEL_DEFAULT);
						if (ZSTD_isError(codeCompressedSizeInBytes)) {
							throw graphics::Error{formatString("Failed to compress shader cache shader code: {}.", ZSTD_getErrorName(codeCompressedSizeInBytes))};
						}
						compressedCode.resize(codeCompressedSizeInBytes);
						break;
					}
				}
			}

			it->second = DeviceImplementation::Shader{
				.codeUncompressedSizeInU32s = static_cast<uint32_t>(code.size()),
				.compressionMode = compressionMode,
				.compressedCode = std::move(compressedCode),
				.shaderModule = createShader(device.logicalDevice.get(), code),
			};
		} catch (...) {
			device.shaderCache.erase(it);
			Error::throwWithNested(Error{"Failed to compile shader."});
		}
	}
	return it->second.shaderModule.get();
}

#endif

[[nodiscard]] String readShaderFile(const Filesystem& filesystem, CStringView filepath, bool useCorrespondingCompiledFileIfAvailable, CStringView compiledFileDirectory) {
	InputFileHandle file{};
	if (useCorrespondingCompiledFileIfAvailable) {
		String compiledFilepath{};
		if (compiledFileDirectory.empty()) {
			compiledFilepath = formatString("{}.spv", filepath);
		} else {
			compiledFilepath = formatString("{}/{}.spv", compiledFileDirectory, filepath.substr(filepath.find_last_of("/\\") + 1));
		}
		file = filesystem.tryOpenInputFile(compiledFilepath);
		if (!file) {
			file = filesystem.openInputFile(filepath);
		}
	} else {
		file = filesystem.openInputFile(filepath);
	}
	return file.readBytesIntoString();
}

} // namespace

std::vector<uint32_t> VertexShaderBase::compileGLSLToVulkanSPIRVImplementation([[maybe_unused]] CStringView sourceCode, [[maybe_unused]] const Filesystem* filesystem,
	[[maybe_unused]] CStringView filepath, [[maybe_unused]] const ShaderCompilationOptions& compilationOptions,
	[[maybe_unused]] Span<const ConstantDescription> constantDescriptions, [[maybe_unused]] Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	[[maybe_unused]] Span<const ParameterDescription> parameterDescriptions, [[maybe_unused]] Span<const FieldDescription> instanceAttributeDescriptions,
	[[maybe_unused]] Span<const FieldDescription> outputFieldDescriptions, [[maybe_unused]] Span<const BufferLayoutReference> bufferLayouts) {
#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
	const String prologue = generateVertexShaderPrologue(constantDescriptions, vertexAttributeDescriptions, parameterDescriptions, instanceAttributeDescriptions,
		outputFieldDescriptions, bufferLayouts);
	detail::AllocatedStringBuffer allocatedStrings{};
	detail::ExpandedStringBuffer sourceStrings{
		SHADER_HEADER.c_str(),
		SHADER_PROLOGUE.c_str(),
		prologue.c_str(),
		SHADER_LINE_DIRECTIVE.c_str(),
		sourceCode.c_str(),
		SHADER_EPILOGUE.c_str(),
	};
	sourceStrings = detail::expandIncludes(allocatedStrings, sourceStrings, filesystem, filepath);
	try {
		return ShaderCompiler{}.compileShaderStageGLSLToSPIRV(EShLangVertex, sourceStrings, compilationOptions);
	} catch (...) {
		Error::throwWithNested(Error{"Failed to compile shader."});
	}
#else
	throw graphics::Error{"Failed to compile shader: GLSL to Vulkan SPIR-V compilation is not enabled."};
#endif
}

VertexShaderBase::VertexShaderBase([[maybe_unused]] Device& device, [[maybe_unused]] CStringView sourceCode, [[maybe_unused]] const Filesystem* filesystem,
	[[maybe_unused]] CStringView filepath, [[maybe_unused]] Span<const ConstantDescription> constantDescriptions, [[maybe_unused]] std::type_index meshTypeIndex,
	[[maybe_unused]] Span<const VertexAttributeDescription> vertexAttributeDescriptions, [[maybe_unused]] Optional<MeshIndexType> indexType,
	[[maybe_unused]] Span<const ParameterDescription> parameterDescriptions, [[maybe_unused]] Span<const FieldDescription> instanceAttributeDescriptions,
	[[maybe_unused]] uint32_t instanceStride, [[maybe_unused]] Span<const FieldDescription> outputFieldDescriptions,
	[[maybe_unused]] Span<const BufferLayoutReference> bufferLayouts) {
#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
	GREM_PROFILE_FUNCTION();
	const String prologue = generateVertexShaderPrologue(constantDescriptions, vertexAttributeDescriptions, parameterDescriptions, instanceAttributeDescriptions,
		outputFieldDescriptions, bufferLayouts);
	implementation = SharedPointer<VertexShaderImplementation>::create(
		compileShader(*device.get(), DeviceImplementation::ShaderType::SPIRV_VERTEX, EShLangVertex, prologue, sourceCode, filesystem, filepath), meshTypeIndex, indexType,
		parameterDescriptions, instanceAttributeDescriptions, instanceStride, bufferLayouts);
#else
	throw graphics::Error{"Failed to create shader: GLSL compilation is not enabled."};
#endif
}

VertexShaderBase::VertexShaderBase(Device& device, Span<const uint32_t> code, Span<const ConstantDescription>, std::type_index meshTypeIndex,
	Span<const VertexAttributeDescription>, Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions,
	Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride, Span<const FieldDescription>, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_BLOCK("Load vertex shader from SPIR-V");

	if (code.empty() || (code.front() != 0x07230203 && code.front() != 0x03022307)) {
		// Note: This is just a sanity check. Do not rely on this exception being thrown for invalid input! The caller must ensure that the provided SPIR-V is always valid.
		throw graphics::Error{"Failed to create shader: Invalid SPIR-V module."};
	}
	implementation = SharedPointer<VertexShaderImplementation>::create(createShader(device.get()->logicalDevice.get(), code), meshTypeIndex, indexType, parameterDescriptions,
		instanceAttributeDescriptions, instanceStride, bufferLayouts);
}

VertexShaderBase::VertexShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const VertexShaderOptions& options,
	Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
	Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load vertex shader {}", filepath));

	const String fileContents = readShaderFile(filesystem, filepath, options.useCorrespondingCompiledFileIfAvailable, options.compiledFileDirectory);
	if (fileContents.starts_with("\x07\x23\x02\x03") || fileContents.starts_with("\x03\x02\x23\x07")) {
		if (fileContents.size() % sizeof(uint32_t) != 0) {
			throw graphics::Error{"Failed to create shader: Invalid SPIR-V module length."};
		}
		*this = VertexShaderBase(device, Span{std::launder(reinterpret_cast<const uint32_t*>(fileContents.data())), fileContents.size() / sizeof(uint32_t)}, constantDescriptions,
			meshTypeIndex, vertexAttributeDescriptions, indexType, parameterDescriptions, instanceAttributeDescriptions, instanceStride, outputFieldDescriptions, bufferLayouts);
	} else {
		*this = VertexShaderBase(device, fileContents, &filesystem, filepath, constantDescriptions, meshTypeIndex, vertexAttributeDescriptions, indexType, parameterDescriptions,
			instanceAttributeDescriptions, instanceStride, outputFieldDescriptions, bufferLayouts);
	}
}

std::vector<uint32_t> FragmentShaderBase::compileGLSLToVulkanSPIRVImplementation([[maybe_unused]] CStringView sourceCode, [[maybe_unused]] const Filesystem* filesystem,
	[[maybe_unused]] CStringView filepath, [[maybe_unused]] const ShaderCompilationOptions& compilationOptions,
	[[maybe_unused]] Span<const ConstantDescription> constantDescriptions, [[maybe_unused]] Span<const ParameterDescription> parameterDescriptions,
	[[maybe_unused]] Span<const FieldDescription> instanceAttributeDescriptions, [[maybe_unused]] Span<const FieldDescription> inputFieldDescriptions,
	[[maybe_unused]] Span<const FieldDescription> outputFieldDescriptions, [[maybe_unused]] Span<const BufferLayoutReference> bufferLayouts) {
#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
	const String prologue =
		generateFragmentShaderPrologue(constantDescriptions, parameterDescriptions, instanceAttributeDescriptions, inputFieldDescriptions, outputFieldDescriptions, bufferLayouts);
	detail::AllocatedStringBuffer allocatedStrings{};
	detail::ExpandedStringBuffer sourceStrings{
		SHADER_HEADER.c_str(),
		SHADER_PROLOGUE.c_str(),
		prologue.c_str(),
		SHADER_LINE_DIRECTIVE.c_str(),
		sourceCode.c_str(),
		SHADER_EPILOGUE.c_str(),
	};
	sourceStrings = detail::expandIncludes(allocatedStrings, sourceStrings, filesystem, filepath);
	try {
		return ShaderCompiler{}.compileShaderStageGLSLToSPIRV(EShLangFragment, sourceStrings, compilationOptions);
	} catch (...) {
		Error::throwWithNested(Error{"Failed to compile shader."});
	}
#else
	throw graphics::Error{"Failed to compile shader: GLSL to Vulkan SPIR-V compilation is not enabled."};
#endif
}

FragmentShaderBase::FragmentShaderBase([[maybe_unused]] Device& device, [[maybe_unused]] CStringView sourceCode, [[maybe_unused]] const Filesystem* filesystem,
	[[maybe_unused]] CStringView filepath, [[maybe_unused]] Span<const ConstantDescription> constantDescriptions, [[maybe_unused]] std::type_index meshTypeIndex,
	Span<const VertexAttributeDescription>, Optional<MeshIndexType>, [[maybe_unused]] Span<const ParameterDescription> parameterDescriptions,
	[[maybe_unused]] Span<const FieldDescription> instanceAttributeDescriptions, uint32_t, [[maybe_unused]] Span<const FieldDescription> inputFieldDescriptions,
	[[maybe_unused]] Span<const FieldDescription> outputFieldDescriptions, [[maybe_unused]] Span<const BufferLayoutReference> bufferLayouts) {
#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
	GREM_PROFILE_FUNCTION();
	const String prologue =
		generateFragmentShaderPrologue(constantDescriptions, parameterDescriptions, instanceAttributeDescriptions, inputFieldDescriptions, outputFieldDescriptions, bufferLayouts);
	implementation = SharedPointer<FragmentShaderImplementation>::create(
		compileShader(*device.get(), DeviceImplementation::ShaderType::SPIRV_FRAGMENT, EShLangFragment, prologue, sourceCode, filesystem, filepath), meshTypeIndex, bufferLayouts);
#else
	throw graphics::Error{"Failed to create shader: GLSL compilation is not enabled."};
#endif
}

FragmentShaderBase::FragmentShaderBase(Device& device, Span<const uint32_t> code, Span<const ConstantDescription>, std::type_index meshTypeIndex,
	Span<const VertexAttributeDescription>, Optional<MeshIndexType>, Span<const ParameterDescription>, Span<const FieldDescription>, uint32_t, Span<const FieldDescription>,
	Span<const FieldDescription>, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_BLOCK("Load fragment shader from SPIR-V");

	if (code.empty() || (code.front() != 0x07230203 && code.front() != 0x03022307)) {
		// Note: This is just a sanity check. Do not rely on this exception being thrown for invalid input! The caller must ensure that the provided SPIR-V is always valid.
		throw graphics::Error{"Failed to create shader: Invalid SPIR-V module."};
	}
	implementation = SharedPointer<FragmentShaderImplementation>::create(createShader(device.get()->logicalDevice.get(), code), meshTypeIndex, bufferLayouts);
}

FragmentShaderBase::FragmentShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const FragmentShaderOptions& options,
	Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
	Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load fragment shader {}", filepath));

	const String fileContents = readShaderFile(filesystem, filepath, options.useCorrespondingCompiledFileIfAvailable, options.compiledFileDirectory);
	if (fileContents.starts_with("\x07\x23\x02\x03") || fileContents.starts_with("\x03\x02\x23\x07")) {
		if (fileContents.size() % sizeof(uint32_t) != 0) {
			throw graphics::Error{"Failed to create shader: Invalid SPIR-V module length."};
		}
		*this = FragmentShaderBase(device, Span{std::launder(reinterpret_cast<const uint32_t*>(fileContents.data())), fileContents.size() / sizeof(uint32_t)}, constantDescriptions,
			meshTypeIndex, vertexAttributeDescriptions, indexType, parameterDescriptions, instanceAttributeDescriptions, instanceStride, inputFieldDescriptions,
			outputFieldDescriptions, bufferLayouts);
	} else {
		*this = FragmentShaderBase(device, fileContents, &filesystem, filepath, constantDescriptions, meshTypeIndex, vertexAttributeDescriptions, indexType, parameterDescriptions,
			instanceAttributeDescriptions, instanceStride, inputFieldDescriptions, outputFieldDescriptions, bufferLayouts);
	}
}

ShaderPipelineBase::ShaderPipelineBase(Device&, std::type_index meshTypeIndex, SharedPointer<VertexShaderImplementation> vertexShaderHandle,
	Span<const ConstantDescription> vertexShaderConstantDescriptions, Span<const byte> vertexShaderConstantData, SharedPointer<FragmentShaderImplementation> fragmentShaderHandle,
	Span<const ConstantDescription> fragmentShaderConstantDescriptions, Span<const byte> fragmentShaderConstantData, const ShaderPipelineOptions& shaderPipelineOptions)
	: implementation(SharedPointer<ShaderPipelineImplementation>::create(meshTypeIndex, std::move(vertexShaderHandle), vertexShaderConstantDescriptions, vertexShaderConstantData,
		  std::move(fragmentShaderHandle), fragmentShaderConstantDescriptions, fragmentShaderConstantData, shaderPipelineOptions)) {}

} // namespace detail

} // namespace grem::graphics
