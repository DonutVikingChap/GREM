// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/graphics/ConstantDescription.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "ShaderImplementation.hpp"
#include "StatePreserver.hpp"
#include "opengl.hpp"

#include <cstring>   // std::strlen
#include <new>       // std::launder
#include <typeindex> // std::type_index
#include <utility>   // std::move
#include <vector>    // std::vector

namespace grem::graphics {

namespace detail {

namespace {

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
constexpr CStringView SHADER_HEADER =
	"#version 300 es\n"
	"precision highp float;\n"
	"precision highp int;\n"
	"precision highp sampler2D;\n"
	"precision highp sampler2DShadow;\n"
	"precision highp sampler2DArray;\n"
	"precision highp sampler2DArrayShadow;\n"
	"precision highp samplerCube;\n"
	"precision highp samplerCubeShadow;\n";
#else
constexpr CStringView SHADER_HEADER = "#version 330 core\n";
#endif

constexpr CStringView SHADER_PROLOGUE = R"GLSL(
#define GREM_GRAPHICS_BACKEND_OPENGL
#define GREM_vertexIndex gl_VertexID
#define GREM_fragmentCoordinates (vec4(gl_FragCoord.x, GREM_private_framebufferHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w))
#define GREM_textureSample2D(name, coordinates) texture(name, coordinates)
#define GREM_textureSample2DShadow(name, coordinates) texture(name, coordinates)
#define GREM_textureSample2DArray(name, coordinates) texture(name, coordinates)
#define GREM_textureSample2DArrayShadow(name, coordinates) texture(name, coordinates)
#define GREM_textureSampleCube(name, coordinates) texture(name, coordinates)
#define GREM_textureSampleCubeShadow(name, coordinates) texture(name, coordinates)
#define GREM_textureSampleCubeArray(name, coordinates) texture(name, coordinates)
#define GREM_textureSampleCubeArrayShadow(name, coordinates, compare) texture(name, vec4(GREM_private_getCubeArrayTextureCoordinates(coordinates), compare))
#define GREM_textureSampleLod2D(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLod2DShadow(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLod2DArray(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLod2DArrayShadow(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLodCube(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLodCubeShadow(name, coordinates, lod) textureLod(name, coordinates, lod)
#define GREM_textureSampleLodCubeArray(name, coordinates, lod) textureLod(name, GREM_private_getCubeArrayTextureCoordinates(coordinates), lod)
#define GREM_textureSampleLodCubeArrayFiltered(name, coordinates, lod) textureLod(name, GREM_private_getCubeArrayTextureCoordinatesFiltered(coordinates, float(lod), float(textureSize(name, 0).x)), lod)
#define GREM_textureSampleGrad2D(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DArray(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGrad2DArrayShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCube(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCubeShadow(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, coordinates, partialDerivativeX, partialDerivativeY)
#define GREM_textureSampleGradCubeArray(name, coordinates, partialDerivativeX, partialDerivativeY) textureGrad(name, GREM_private_getCubeArrayTextureCoordinates(coordinates), partialDerivativeX, partialDerivativeY)
#define GREM_texelFetch2D(name, coordinates, lod) texelFetch(name, coordinates, lod)
#define GREM_texelFetch2DArray(name, coordinates, lod) texelFetch(name, coordinates, lod)
#define main() GREM_private_main()

vec3 GREM_private_getCubeArrayTextureCoordinates(vec4 coordinates) {
	vec3 r = coordinates.xyz;
	float cubeLayer = coordinates.w;

	vec3 m = abs(r);

	float ma;
	vec2 uv;
	float sideIndex;
	if (m.x > m.y && m.x > m.z) {
		ma = m.x;
		if (r.x >= 0.0) {
			uv = vec2(-r.z, -r.y);
			sideIndex = 0.0;
		} else {
			uv = vec2(r.z, -r.y);
			sideIndex = 1.0;
		}
	} else if (m.y > m.z) {
		ma = m.y;
		// Note: Side 2 and 3 are flipped to match the Vulkan convention.
		if (r.y >= 0.0) {
			uv = vec2(r.x, r.z);
			sideIndex = 3.0;
		} else {
			uv = vec2(r.x, -r.z);
			sideIndex = 2.0;
		}
	} else {
		ma = m.z;
		if (r.z >= 0.0) {
			uv = vec2(r.x, -r.y);
			sideIndex = 4.0;
		} else {
			uv = vec2(-r.x, -r.y);
			sideIndex = 5.0;
		}
	}

	return vec3(0.5 * (uv / ma + vec2(1.0)), cubeLayer * 6.0 + sideIndex);
}

vec3 GREM_private_getCubeArrayTextureCoordinatesFiltered(vec4 coordinates, float lod, float size) {
	vec3 r = coordinates.xyz;
	float cubeLayer = coordinates.w;

	vec3 m = abs(r);

	float ma;
	vec2 uv;
	float sideIndex;
	if (m.x > m.y && m.x > m.z) {
		ma = m.x;
		if (r.x >= 0.0) {
			uv = vec2(-r.z, -r.y);
			sideIndex = 0.0;
		} else {
			uv = vec2(r.z, -r.y);
			sideIndex = 1.0;
		}
	} else if (m.y > m.z) {
		ma = m.y;
		// Note: Side 2 and 3 are flipped to match the Vulkan convention.
		if (r.y >= 0.0) {
			uv = vec2(r.x, r.z);
			sideIndex = 3.0;
		} else {
			uv = vec2(r.x, -r.z);
			sideIndex = 2.0;
		}
	} else {
		ma = m.z;
		if (r.z >= 0.0) {
			uv = vec2(r.x, -r.y);
			sideIndex = 4.0;
		} else {
			uv = vec2(-r.x, -r.y);
			sideIndex = 5.0;
		}
	}

	// Scale down the UV coordinates to alleviate seam artifacts when sampling near the cube edges.
	float scale = 1.0 - exp2(lod) / size;
	return vec3(0.5 * (scale * uv / ma + vec2(1.0)), cubeLayer * 6.0 + sideIndex);
}
)GLSL";

constexpr CStringView SHADER_LINE_DIRECTIVE = "\n#line 1\n";

constexpr CStringView VERTEX_SHADER_EPILOGUE = R"GLSL(
#undef main
void main() {
	GREM_private_main();

	// Convert from [-1, 1] depth range to [0, 1].
	gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
)GLSL";

constexpr CStringView FRAGMENT_SHADER_EPILOGUE = R"GLSL(
#undef main
void main() {
	GREM_private_main();

	// Perform gamma correction.
	GREM_PRIVATE_GAMMA_CORRECTION_CODE
}
)GLSL";

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
		case ParameterType::SAMPLER_CUBE_ARRAY:
			return "sampler2DArray"; // Note: samplerCubeArray is emulated by a sampler2DArray due to lack of support in the GL version that we target.
		case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW:
			return "sampler2DArrayShadow"; // Note: samplerCubeArrayShadow is emulated by a sampler2DArrayShadow due to lack of support in the GL version that we target.
	}
	return {};
}

void writeConstants(String& output, Span<const ConstantDescription> constantDescriptions, Span<const byte> constantData) {
	if (!constantDescriptions.empty()) {
		for (const ConstantDescription& constantDescription : constantDescriptions) {
			switch (constantDescription.type) {
				case ConstantType::BOOL:
					output.append(formatString("const bool {} = {};\n", constantDescription.name, *std::launder(reinterpret_cast<const bool32_t*>(constantData.data()))));
					break;
				case ConstantType::INT:
					output.append(formatString("const int {} = {};\n", constantDescription.name, *std::launder(reinterpret_cast<const int32_t*>(constantData.data()))));
					break;
				case ConstantType::UINT:
					output.append(formatString("const uint {} = {}u;\n", constantDescription.name, *std::launder(reinterpret_cast<const uint32_t*>(constantData.data()))));
					break;
				case ConstantType::FLOAT:
					output.append(formatString("const float {} = {:#};\n", constantDescription.name, *std::launder(reinterpret_cast<const float*>(constantData.data()))));
					break;
			}
			constantData = constantData.subspan(sizeof(float));
		}
		output.push_back('\n');
	}
}

void writeIODeclarations(String& output, StringView qualifier, Span<const FieldDescription> fieldDescriptions) {
	if (!fieldDescriptions.empty()) {
		for (const FieldDescription& fieldDescription : fieldDescriptions) {
			switch (fieldDescription.type) {
				case FieldType::INT: output.append(formatString("flat {} int ", qualifier)); break;
				case FieldType::IVEC2: output.append(formatString("flat {} ivec2 ", qualifier)); break;
				case FieldType::IVEC3: output.append(formatString("flat {} ivec3 ", qualifier)); break;
				case FieldType::IVEC4: output.append(formatString("flat {} ivec4 ", qualifier)); break;
				case FieldType::UINT: output.append(formatString("flat {} uint ", qualifier)); break;
				case FieldType::UVEC2: output.append(formatString("flat {} uvec2 ", qualifier)); break;
				case FieldType::UVEC3: output.append(formatString("flat {} uvec3 ", qualifier)); break;
				case FieldType::UVEC4: output.append(formatString("flat {} uvec4 ", qualifier)); break;
				case FieldType::FLOAT: output.append(formatString("{} float ", qualifier)); break;
				case FieldType::VEC2: output.append(formatString("{} vec2 ", qualifier)); break;
				case FieldType::VEC3: output.append(formatString("{} vec3 ", qualifier)); break;
				case FieldType::VEC4: output.append(formatString("{} vec4 ", qualifier)); break;
				case FieldType::MAT2: output.append(formatString("{} mat2 ", qualifier)); break;
				case FieldType::MAT3: output.append(formatString("{} mat3 ", qualifier)); break;
				case FieldType::MAT4: output.append(formatString("{} mat4 ", qualifier)); break;
			}
			output.append(fieldDescription.name);
			if (fieldDescription.arrayElementCount != 0) {
				output.append(formatString("[{}]", fieldDescription.arrayElementCount));
			}
			output.append(";\n");
		}
		output.push_back('\n');
	}
}

void writeUniformBufferDeclarations(String& output, CStringView blockName, Span<const ParameterDescription> parameterDescriptions) {
	if (parameterDescriptions.empty()) {
		return;
	}

	if (anyOf(parameterDescriptions, [](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
		output.append(formatString("layout(std140) uniform {} {{\n", blockName));
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
				output.append(formatString("uniform {} {};\n", getParameterTypeName(parameterDescription.type), parameterDescription.name));
			} else {
				output.append(
					formatString("uniform {0} {1}Array[{2}];\n"
								 "#define {1}(GREM_private_arrayIndex) {1}Array[GREM_private_arrayIndex]\n",
						getParameterTypeName(parameterDescription.type), parameterDescription.name, parameterDescription.arrayElementCount));
			}
		}
	}
	output.push_back('\n');
}

void writeStorageBufferDeclarations(String& output, CStringView bufferName, Span<const FieldDescription> fieldDescriptions) {
	if (fieldDescriptions.empty()) {
		return;
	}

	output.append(formatString("uniform uvec4 {}Binding;\n", bufferName));
	writeVec4BufferGetters(output, {}, fieldDescriptions, [&](String& output, StringView nameString, StringView indexString) -> void {
		output.append(
			formatString("vec4 {0} = texelFetch(GREM_private_storageBuffer, ivec2(int({1}Binding.x + (({2}) & {1}Binding.z)), int({1}Binding.y + (({2}) >> {1}Binding.w))), 0);",
				nameString, bufferName, indexString));
	});
}

void writeBufferDeclarations(String& output, bool& hasStorageBuffer, Span<const BufferLayoutReference> bufferLayouts) {
	for (const BufferLayoutReference& bufferLayout : bufferLayouts) {
		GREM_MATCH(bufferLayout) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				if (!isValidName(uniformBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", uniformBufferLayout.name)};
				}
				writeUniformBufferDeclarations(output, formatString("GREM_private_{}Block", uniformBufferLayout.name), uniformBufferLayout.parameterDescriptions);
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
				if (!isValidName(storageBufferLayout.name)) {
					throw graphics::Error{formatString("Invalid buffer name \"{}\".", storageBufferLayout.name)};
				}
				if (!hasStorageBuffer) {
					output.append("uniform sampler2D GREM_private_storageBuffer;\n\n");
					hasStorageBuffer = true;
				}
				writeStorageBufferDeclarations(output, formatString("GREM_private_{}", storageBufferLayout.name), storageBufferLayout.fieldDescriptions);
				break;
			}
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				writeBufferDeclarations(output, hasStorageBuffer, bufferSetLayout.bufferLayouts);
				break;
			}
		}
	}
}

[[nodiscard]] detail::ShaderObject compileShader(Span<const ConstantDescription> constantDescriptions, Span<const byte> constantData, CStringView sourceCode,
	Span<const VertexAttributeDescription> vertexAttributeDescriptions, Span<const ParameterDescription> parameterDescriptions,
	Span<const FieldDescription> instanceAttributeDescriptions, Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions,
	Span<const BufferLayoutReference> bufferLayouts, bool isFragmentShader, bool usesFragmentCoordinates) {
	GREM_PROFILE_BLOCK_DYNAMIC((isFragmentShader) ? "Compile fragment shader" : "Compile vertex shader");

	detail::ShaderObject result = detail::createShaderObject((isFragmentShader) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);

	String prologue{};

	writeConstants(prologue, constantDescriptions, constantData);

	size_t attributeIndex = 0;
	writeInputAttributeDeclarations(prologue, attributeIndex, vertexAttributeDescriptions);
	writeInputAttributeDeclarations(prologue, attributeIndex, instanceAttributeDescriptions);

	writeIODeclarations(prologue, "in", inputFieldDescriptions);
	writeIODeclarations(prologue, "out", outputFieldDescriptions);

	writeUniformBufferDeclarations(prologue, "GREM_private_MeshParameters", parameterDescriptions);

	bool hasStorageBuffer = false;
	writeBufferDeclarations(prologue, hasStorageBuffer, bufferLayouts);

	if (isFragmentShader) {
		const bool usesGamma = anyOf(outputFieldDescriptions, [](const FieldDescription& fieldDescription) -> bool {
			if (fieldDescription.arrayElementCount == 0) {
				switch (fieldDescription.type) {
					case FieldType::FLOAT: [[fallthrough]];
					case FieldType::VEC2: [[fallthrough]];
					case FieldType::VEC3: [[fallthrough]];
					case FieldType::VEC4: return true;
					default: break;
				}
			}
			return false;
		});
		if (usesGamma) {
			prologue.append(
				"#define GREM_PRIVATE_SRGB_TO_LINEAR(x) (((x) <= 0.04045) ? (x) / 12.92 : pow(((x) + 0.055) / 1.055, 2.4))\n"
				"#define GREM_PRIVATE_LINEAR_TO_SRGB(x) (((x) <= 0.0031308) ? (x) * 12.92 : 1.055 * pow((x), 1.0 / 2.4) - 0.055)\n"
				"#define GREM_PRIVATE_GAMMA_CORRECTION_CODE \\\n"
				"\tswitch (GREM_private_srgbCorrectionMode) { \\\n"
				"\t\tcase -1: \\\n");
			for (const FieldDescription& fieldDescription : outputFieldDescriptions) {
				if (fieldDescription.arrayElementCount == 0) {
					switch (fieldDescription.type) {
						case FieldType::FLOAT: prologue.append(formatString("\t\t\t{0} = GREM_PRIVATE_SRGB_TO_LINEAR({0}); \\\n", fieldDescription.name)); break;
						case FieldType::VEC2:
							prologue.append(formatString("\t\t\t{0} = vec2(GREM_PRIVATE_SRGB_TO_LINEAR({0}.r), GREM_PRIVATE_SRGB_TO_LINEAR({0}.g)); \\\n", fieldDescription.name));
							break;
						case FieldType::VEC3:
							prologue.append(
								formatString("\t\t\t{0} = vec3(GREM_PRIVATE_SRGB_TO_LINEAR({0}.r), GREM_PRIVATE_SRGB_TO_LINEAR({0}.g), GREM_PRIVATE_SRGB_TO_LINEAR({0}.b)); "
											 "\\\n",
									fieldDescription.name));
							break;
						case FieldType::VEC4:
							prologue.append(formatString(
								"\t\t\t{0}.rgb *= {0}.a; \\\n"
								"\t\t\t{0}.rgb = vec3(GREM_PRIVATE_SRGB_TO_LINEAR({0}.r), GREM_PRIVATE_SRGB_TO_LINEAR({0}.g), GREM_PRIVATE_SRGB_TO_LINEAR({0}.b)) / {0}.a; "
								"\\\n",
								fieldDescription.name));
							break;
						default: break;
					}
				}
			}
			prologue.append(
				"\t\t\tbreak; \\\n"
				"\t\tcase 1: \\\n");
			for (const FieldDescription& fieldDescription : outputFieldDescriptions) {
				if (fieldDescription.arrayElementCount == 0) {
					switch (fieldDescription.type) {
						case FieldType::FLOAT: prologue.append(formatString("\t\t\t{0} = GREM_PRIVATE_LINEAR_TO_SRGB({0}); \\\n", fieldDescription.name)); break;
						case FieldType::VEC2:
							prologue.append(formatString("\t\t\t{0} = vec2(GREM_PRIVATE_LINEAR_TO_SRGB({0}.r), GREM_PRIVATE_LINEAR_TO_SRGB({0}.g)); \\\n", fieldDescription.name));
							break;
						case FieldType::VEC3:
							prologue.append(
								formatString("\t\t\t{0} = vec3(GREM_PRIVATE_LINEAR_TO_SRGB({0}.r), GREM_PRIVATE_LINEAR_TO_SRGB({0}.g), GREM_PRIVATE_LINEAR_TO_SRGB({0}.b)); "
											 "\\\n",
									fieldDescription.name));
							break;
						case FieldType::VEC4:
							prologue.append(formatString(
								"\t\t\t{0}.rgb *= {0}.a; \\\n"
								"\t\t\t{0}.rgb = vec3(GREM_PRIVATE_LINEAR_TO_SRGB({0}.r), GREM_PRIVATE_LINEAR_TO_SRGB({0}.g), GREM_PRIVATE_LINEAR_TO_SRGB({0}.b)) / {0}.a; "
								"\\\n",
								fieldDescription.name));
							break;
						default: break;
					}
				}
			}
			prologue.append(
				"\t\t\tbreak; \\\n"
				"\t\tdefault: break; \\\n"
				"\t}\n"
				"\n"
				"uniform int GREM_private_srgbCorrectionMode;\n\n");
		} else {
			prologue.append("#define GREM_PRIVATE_GAMMA_CORRECTION_CODE\n\n");
		}
		if (usesFragmentCoordinates) {
			prologue.append("uniform float GREM_private_framebufferHeight;\n\n");
		}
	}

	const Array sourceStrings{
		static_cast<const GLchar*>(SHADER_HEADER.c_str()),
		static_cast<const GLchar*>(SHADER_PROLOGUE.c_str()),
		static_cast<const GLchar*>(prologue.c_str()),
		static_cast<const GLchar*>(SHADER_LINE_DIRECTIVE.c_str()),
		static_cast<const GLchar*>(sourceCode.c_str()),
		static_cast<const GLchar*>((isFragmentShader) ? FRAGMENT_SHADER_EPILOGUE.c_str() : VERTEX_SHADER_EPILOGUE.c_str()),
	};
	glShaderSource(result.get(), sourceStrings.size(), sourceStrings.data(), nullptr);
	glCompileShader(result.get());

	GLint success = GL_FALSE;
	glGetShaderiv(result.get(), GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		GLint infoLogLength = 0;
		glGetShaderiv(result.get(), GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			String infoLog(static_cast<size_t>(infoLogLength), '\0');
			glGetShaderInfoLog(result.get(), infoLogLength, nullptr, infoLog.data());
			try {
				throw graphics::Error{infoLog.c_str()}; // Note: Uses c_str() to automatically get the string up until the first '\0'.
			} catch (...) {
				Error::throwWithNested(Error{"Failed to compile shader."});
			}
		}
		throw graphics::Error{"Failed to compile shader."};
	}
	return result;
}

[[nodiscard]] String ensureNotSPIRV(String&& sourceCode) {
	if (sourceCode.starts_with("\x07\x23\x02\x03") || sourceCode.starts_with("\x03\x02\x23\x07")) {
		throw graphics::Error{"Failed to create shader: SPIR-V code is not supported by this graphics backend."};
	}
	return std::move(sourceCode);
}

[[nodiscard]] String expandSource(CStringView sourceCode, const Filesystem* filesystem, CStringView filepath) {
	detail::AllocatedStringBuffer allocatedStrings{};
	detail::ExpandedStringBuffer sourceStrings{sourceCode.c_str()};
	sourceStrings = detail::expandIncludes(allocatedStrings, sourceStrings, filesystem, filepath);
	SmallArrayList<size_t, 16> sizes{};
	sizes.reserve(sourceStrings.size());
	for (const char* const sourceString : sourceStrings) {
		sizes.push_back(std::strlen(sourceString));
	}
	String result{};
	result.resize(accumulate(sizes, size_t{0}));
	char* output = result.data();
	for (size_t i = 0; i < sourceStrings.size(); ++i) {
		memcpy(output, sourceStrings[i], sizes[i]);
		output += sizes[i];
	}
	return result;
}

} // namespace

std::vector<uint32_t> VertexShaderBase::compileGLSLToVulkanSPIRVImplementation(CStringView, const Filesystem*, CStringView, const ShaderCompilationOptions&,
	Span<const ConstantDescription>, Span<const VertexAttributeDescription>, Span<const ParameterDescription>, Span<const FieldDescription>, Span<const FieldDescription>,
	Span<const BufferLayoutReference>) {
	throw graphics::Error{"Failed to compile shader: GLSL to Vulkan SPIR-V compilation is not supported by this graphics backend."};
}

VertexShaderBase::VertexShaderBase(Device&, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath, Span<const ConstantDescription>,
	std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType,
	Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
	Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
	: implementation(SharedPointer<VertexShaderImplementation>::create(expandSource(sourceCode, filesystem, filepath), meshTypeIndex, vertexAttributeDescriptions, indexType,
		  parameterDescriptions, instanceAttributeDescriptions, instanceStride, outputFieldDescriptions, bufferLayouts)) {}

VertexShaderBase::VertexShaderBase(Device&, Span<const uint32_t>, Span<const ConstantDescription>, std::type_index, Span<const VertexAttributeDescription>, Optional<MeshIndexType>,
	Span<const ParameterDescription>, Span<const FieldDescription>, uint32_t, Span<const FieldDescription>, Span<const BufferLayoutReference>) {
	throw graphics::Error{"Failed to create shader: SPIR-V code is not supported by this graphics backend."};
}

VertexShaderBase::VertexShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const VertexShaderOptions&,
	Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
	Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
	: VertexShaderBase(device, ensureNotSPIRV(filesystem.readInputFileString(filepath)), &filesystem, filepath, constantDescriptions, meshTypeIndex, vertexAttributeDescriptions,
		  indexType, parameterDescriptions, instanceAttributeDescriptions, instanceStride, outputFieldDescriptions, bufferLayouts) {}

std::vector<uint32_t> FragmentShaderBase::compileGLSLToVulkanSPIRVImplementation(CStringView, const Filesystem*, CStringView, const ShaderCompilationOptions&,
	Span<const ConstantDescription>, Span<const ParameterDescription>, Span<const FieldDescription>, Span<const FieldDescription>, Span<const FieldDescription>,
	Span<const BufferLayoutReference>) {
	throw graphics::Error{"Failed to compile shader: GLSL to Vulkan SPIR-V compilation is not supported by this graphics backend."};
}

FragmentShaderBase::FragmentShaderBase(Device&, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath, Span<const ConstantDescription>,
	std::type_index meshTypeIndex, Span<const VertexAttributeDescription>, Optional<MeshIndexType>, Span<const ParameterDescription>, Span<const FieldDescription>, uint32_t,
	Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
	: implementation(SharedPointer<FragmentShaderImplementation>::create(expandSource(sourceCode, filesystem, filepath), meshTypeIndex, inputFieldDescriptions,
		  outputFieldDescriptions, bufferLayouts)) {}

FragmentShaderBase::FragmentShaderBase(Device&, Span<const uint32_t>, Span<const ConstantDescription>, std::type_index, Span<const VertexAttributeDescription>,
	Optional<MeshIndexType>, Span<const ParameterDescription>, Span<const FieldDescription>, uint32_t, Span<const FieldDescription>, Span<const FieldDescription>,
	Span<const BufferLayoutReference>) {
	throw graphics::Error{"Failed to create shader: SPIR-V code is not supported by this graphics backend."};
}

FragmentShaderBase::FragmentShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const FragmentShaderOptions&,
	Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
	Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
	Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts)
	: FragmentShaderBase(device, ensureNotSPIRV(filesystem.readInputFileString(filepath)), &filesystem, filepath, constantDescriptions, meshTypeIndex, vertexAttributeDescriptions,
		  indexType, parameterDescriptions, instanceAttributeDescriptions, instanceStride, inputFieldDescriptions, outputFieldDescriptions, bufferLayouts) {}

ShaderPipelineBase::ShaderPipelineBase(Device&, std::type_index meshTypeIndex, SharedPointer<VertexShaderImplementation> vertexShaderHandle,
	Span<const ConstantDescription> vertexShaderConstantDescriptions, Span<const byte> vertexShaderConstantData, SharedPointer<FragmentShaderImplementation> fragmentShaderHandle,
	Span<const ConstantDescription> fragmentShaderConstantDescriptions, Span<const byte> fragmentShaderConstantData, const ShaderPipelineOptions& shaderPipelineOptions)
	: implementation(SharedPointer<ShaderPipelineImplementation>::create(meshTypeIndex, vertexShaderHandle->instanceStride, vertexShaderHandle->parameterDescriptions,
		  vertexShaderHandle->bufferLayouts, fragmentShaderHandle->bufferLayouts, shaderPipelineOptions)) {
	GREM_PROFILE_FUNCTION();

	if (vertexShaderHandle->meshTypeIndex != meshTypeIndex) {
		throw graphics::Error{"Vertex shader's mesh type does not match the shader pipeline."};
	}
	if (fragmentShaderHandle->meshTypeIndex != meshTypeIndex) {
		throw graphics::Error{"Fragment shader's mesh type does not match the shader pipeline."};
	}

	const detail::ShaderObject vertexShaderObject = compileShader(vertexShaderConstantDescriptions, vertexShaderConstantData, vertexShaderHandle->sourceCode,
		vertexShaderHandle->vertexAttributeDescriptions, vertexShaderHandle->parameterDescriptions, vertexShaderHandle->instanceAttributeDescriptions, {},
		vertexShaderHandle->outputFieldDescriptions, vertexShaderHandle->bufferLayouts, false, false);

	const bool usesFragmentCoordinates = fragmentShaderHandle->sourceCode.find("GREM_fragmentCoordinates") != String::npos;
	const detail::ShaderObject fragmentShaderObject = compileShader(fragmentShaderConstantDescriptions, fragmentShaderConstantData, fragmentShaderHandle->sourceCode, {},
		vertexShaderHandle->parameterDescriptions, {}, fragmentShaderHandle->inputFieldDescriptions, fragmentShaderHandle->outputFieldDescriptions,
		fragmentShaderHandle->bufferLayouts, true, usesFragmentCoordinates);

	const detail::CurrentProgramPreserver currentProgramPreserver{};

	const GLuint programObjectHandle = implementation->programObject.get();
	glAttachShader(programObjectHandle, vertexShaderObject.get());
	glAttachShader(programObjectHandle, fragmentShaderObject.get());

	glLinkProgram(programObjectHandle);

	GLint success = GL_FALSE;
	glGetProgramiv(programObjectHandle, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		GLint infoLogLength = 0;
		glGetProgramiv(programObjectHandle, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			String infoLog(static_cast<size_t>(infoLogLength), '\0');
			glGetProgramInfoLog(programObjectHandle, infoLogLength, nullptr, infoLog.data());
			try {
				throw graphics::Error{infoLog.c_str()}; // Note: Uses c_str() to automatically get the string up until the first '\0'.
			} catch (...) {
				Error::throwWithNested(Error{"Failed to link shader program."});
			}
		}
		throw graphics::Error{"Failed to link shader program."};
	}

	glUseProgram(programObjectHandle);

	GLuint uniformBlockBinding = 0;
	GLint textureUnit = 0;
	ArrayList<GLint> storageBufferBindingsUniformLocations{};
	if (!vertexShaderHandle->parameterDescriptions.empty()) {
		if (anyOf(vertexShaderHandle->parameterDescriptions,
				[](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
			const GLuint uniformBlockIndex = glGetUniformBlockIndex(programObjectHandle, "GREM_private_MeshParameters");
			if (uniformBlockIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(programObjectHandle, uniformBlockIndex, uniformBlockBinding);
			}
			++uniformBlockBinding;
		}
		for (const ParameterDescription& parameterDescription : vertexShaderHandle->parameterDescriptions) {
			if (isTextureParameter(parameterDescription.type)) {
				const GLint location = glGetUniformLocation(programObjectHandle, parameterDescription.name.c_str());
				if (location != -1) {
					glUniform1i(location, textureUnit);
				}
				++textureUnit;
			}
		}
	}

	const auto setupBufferLayout = [&](const auto& self, const BufferLayoutReference& bufferLayout) -> void {
		GREM_MATCH(bufferLayout) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				if (anyOf(uniformBufferLayout.parameterDescriptions,
						[](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
					const GLuint uniformBlockIndex = glGetUniformBlockIndex(programObjectHandle, formatString("GREM_private_{}Block", uniformBufferLayout.name).c_str());
					if (uniformBlockIndex != GL_INVALID_INDEX) {
						glUniformBlockBinding(programObjectHandle, uniformBlockIndex, uniformBlockBinding);
					}
					++uniformBlockBinding;
				}
				for (const ParameterDescription& parameterDescription : uniformBufferLayout.parameterDescriptions) {
					if (isTextureParameter(parameterDescription.type)) {
						const GLint location = glGetUniformLocation(programObjectHandle, parameterDescription.name.c_str());
						if (location != -1) {
							glUniform1i(location, textureUnit);
						}
						++textureUnit;
					}
				}
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
				const GLint location = glGetUniformLocation(programObjectHandle, formatString("GREM_private_{}Binding", storageBufferLayout.name).c_str());
				storageBufferBindingsUniformLocations.push_back(location);
				break;
			}
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				for (const BufferLayoutReference& bufferLayout : bufferSetLayout.bufferLayouts) {
					self(self, bufferLayout);
				}
				break;
			}
		}
	};

	for (const BufferLayoutReference& bufferLayout : vertexShaderHandle->bufferLayouts) {
		setupBufferLayout(setupBufferLayout, bufferLayout);
	}
	if (!fragmentShaderHandle->bufferLayouts.empty()) {
		if (vertexShaderHandle->bufferLayouts.size() > fragmentShaderHandle->bufferLayouts.size()) {
			throw graphics::Error{
				"Failed to link shader program:\n"
				"Incompatible shaders: The vertex shader uses more buffers than the fragment shader."};
		}
		auto it = fragmentShaderHandle->bufferLayouts.begin();
		for (const BufferLayoutReference& bufferLayout : vertexShaderHandle->bufferLayouts) {
			if (*it++ != bufferLayout) {
				throw graphics::Error{
					"Failed to link shader program:\n"
					"Incompatible shaders: The fragment shader does not use all of the vertex shader's buffers."};
			}
		}
		while (it != fragmentShaderHandle->bufferLayouts.end()) {
			setupBufferLayout(setupBufferLayout, *it);
			++it;
		}
	}

	if (!storageBufferBindingsUniformLocations.empty()) {
		implementation->storageBufferBindingsUniformLocations.assign_range(storageBufferBindingsUniformLocations);
		implementation->storageBufferTextureUnit = textureUnit;
		const GLint location = glGetUniformLocation(programObjectHandle, "GREM_private_storageBuffer");
		if (location != -1) {
			glUniform1i(location, textureUnit);
		}
	}

	for (const FieldDescription& outputFieldDescription : fragmentShaderHandle->outputFieldDescriptions) {
		if (glGetFragDataLocation(programObjectHandle, outputFieldDescription.name.c_str()) != -1) {
			implementation->hasColorOutput = true;
			implementation->srgbCorrectionModeUniformLocation = glGetUniformLocation(programObjectHandle, "GREM_private_srgbCorrectionMode");
			break;
		}
	}
	if (usesFragmentCoordinates) {
		implementation->framebufferHeightUniformLocation = glGetUniformLocation(programObjectHandle, "GREM_private_framebufferHeight");
	}
}

} // namespace detail

} // namespace grem::graphics
