// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_RENDER_PASS_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_RENDER_PASS_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/LinearBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/shaders.hpp>

#include "DeviceImplementation.hpp"
#include "MeshImplementation.hpp"
#include "TextureImplementation.hpp"
#include "buffer_implementations.hpp"
#include "opengl.hpp"

#include <typeindex> // std::type_index

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
#include <cstdio> // stderr, std::fprintf
#endif

namespace grem::graphics {

namespace detail {

inline void uploadImageToBoundTexture2D(Offset2D offset, Extent2D size, GLenum format, GLenum type, size_t pixelStride, const byte* data) {
	GREM_ASSERT(offset.x >= 0 && offset.y >= 0);
	GREM_ASSERT(size.width > 0 && size.height > 0);
	GREM_ASSERT(pixelStride > 0);
	const size_t rowStride = static_cast<size_t>(size.width) * pixelStride;
	const size_t maxRowsPerChunk = max(size_t{1073741824} / rowStride, size_t{1});
	const size_t yBegin = static_cast<size_t>(offset.y);
	const size_t yEnd = yBegin + static_cast<size_t>(size.height);
	for (size_t y = yBegin; y < yEnd; y += maxRowsPerChunk) {
		const size_t chunkRows = min(yEnd - y, maxRowsPerChunk);
		glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(offset.x), static_cast<GLint>(y), static_cast<GLsizei>(size.width), static_cast<GLsizei>(chunkRows), format, type,
			data);
		data += chunkRows * rowStride;
	}
}

[[nodiscard]] inline GLuint flushStorageBufferTexture(Device& device) {
	const detail::TextureBinding2DPreserver textureBinding2DPreserver{};
	glBindTexture(GL_TEXTURE_2D, device.get()->storageBufferTexture.get()->object.get<detail::TextureObject>().get());

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

	for (StorageBufferImplementation* const storageBuffer : device.get()->storageBuffers) {
		if (storageBuffer->stagingMemoryWidth == 0 || !storageBuffer->dirty) {
			continue;
		}

		if (storageBuffer->squareAllocation.allocatedWidth != storageBuffer->stagingMemoryWidth) {
			device.get()->storageBufferSquareAllocator.deallocateSquare(std::exchange(storageBuffer->squareAllocation, {}));

			bool expandedStorageBufferTexture = false;
			while (true) {
				if (const Optional<SquareAllocation<uint32_t>> newSquareAllocation = device.get()->storageBufferSquareAllocator.allocateSquare(storageBuffer->stagingMemoryWidth)) {
					storageBuffer->squareAllocation = *newSquareAllocation;
					break;
				}
				const uint32_t newResolution = max(device.get()->storageBufferSquareAllocator.getFullWidth() * 2, storageBuffer->stagingMemoryWidth);
				if (newResolution > Limits<uint32_t>::MAX / newResolution) {
					throw std::length_error{"Maximum shader storage buffer memory size exceeded."};
				}
				device.get()->storageBufferSquareAllocator.expandTo(newResolution);
				expandedStorageBufferTexture = true;
			}

			if (expandedStorageBufferTexture) {
				GREM_PROFILE_BLOCK("Expand shader storage buffer texture");

				device.get()->storageBufferTexture = {};
				device.get()->storageBufferTexture = Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R32G32B32A32_FLOAT,
					Extent2D{device.get()->storageBufferSquareAllocator.getFullWidth()}, 1, nullptr, TextureSamplerOptions::UNFILTERED);
				glBindTexture(GL_TEXTURE_2D, device.get()->storageBufferTexture.get()->object.get<detail::TextureObject>().get());

				for (StorageBufferImplementation* const flushedStorageBuffer : device.get()->storageBuffers) {
					if (flushedStorageBuffer->stagingMemoryWidth == 0 || flushedStorageBuffer->dirty) {
						continue;
					}

					GREM_ASSERT(flushedStorageBuffer->squareAllocation.allocatedWidth == flushedStorageBuffer->stagingMemoryWidth);
					uploadImageToBoundTexture2D(
						Offset2D{static_cast<int32_t>(flushedStorageBuffer->squareAllocation.x), static_cast<int32_t>(flushedStorageBuffer->squareAllocation.y)},
						Extent2D{flushedStorageBuffer->stagingMemoryWidth}, GL_RGBA, GL_FLOAT, sizeof(vec4), flushedStorageBuffer->stagingMemory.data());
				}
			}
		}

		GREM_ASSERT(storageBuffer->squareAllocation.allocatedWidth == storageBuffer->stagingMemoryWidth);
		uploadImageToBoundTexture2D(Offset2D{static_cast<int32_t>(storageBuffer->squareAllocation.x), static_cast<int32_t>(storageBuffer->squareAllocation.y)},
			Extent2D{storageBuffer->stagingMemoryWidth}, GL_RGBA, GL_FLOAT, sizeof(vec4), storageBuffer->stagingMemory.data());

		storageBuffer->dirty = false;
	}

	return device.get()->storageBufferTexture.get()->object.get<detail::TextureObject>().get();
}

inline void uploadInstancesToBoundInstanceBuffer(Span<const byte> data) {
	if (data.size() <= size_t{1073741824}) {
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size_bytes()), data.data(), GL_STREAM_DRAW);
	} else {
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size_bytes()), nullptr, GL_STREAM_DRAW);
		size_t bufferOffset = 0;
		while (!data.empty()) {
			const size_t chunkSize = min(data.size_bytes(), size_t{1073741824});
			glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(bufferOffset), static_cast<GLsizeiptr>(chunkSize), data.data());
			bufferOffset += chunkSize;
			data = data.subspan(chunkSize);
		}
	}
}

} // namespace detail

struct RenderPassImplementation {
	struct RenderTargets {
		Optional<TextureSubresourceReference> resolveTarget{};
		Optional<TextureSubresourceReference> colorTarget{};
		Optional<TextureSubresourceReference> depthStencilTarget{};
		ClearMode clearMode{};
		ResolveMode resolveMode{};
	};

	struct CommandSetViewport {
		Viewport viewport;
	};

	struct CommandSetScissor {
		Optional<Region2D> scissor;
	};

	struct CommandFill {
		Region2D targetRegion;
		GLbitfield aspectMask;
		vec4 color;
		float depth;
		uint8_t stencil;
	};

	struct CommandUseProgram {
		Span<const GLint> storageBufferBindingsUniformLocations;
		GLint storageBufferTextureUnit;
		GLuint shaderProgramObjectHandle;
		GLint srgbCorrectionModeUniformLocation;
		GLint framebufferHeightUniformLocation;
		bool hasColorOutput;
	};

	struct CommandUseShaderPipelineOptions {
		ShaderPipelineOptions shaderPipelineOptions;
	};

	struct CommandUseUniformBuffer {
		GLuint uniformBlockBinding;
		GLuint uniformBufferObjectHandle;
	};

	struct CommandUseStorageBuffer {
		const StorageBufferImplementation* storageBuffer;
		GLuint storageBufferBinding;
	};

	struct CommandUseTexture2D {
		GLint textureUnit;
		GLuint textureObjectHandle;
	};

	struct CommandUseTexture2DArray {
		GLint textureUnit;
		GLuint textureObjectHandle;
	};

	struct CommandUseTextureCube {
		GLint textureUnit;
		GLuint textureObjectHandle;
	};

	struct CommandUseTextureCubeArray {
		GLint textureUnit;
		GLuint textureObjectHandle;
	};

	struct CommandUseVertexArray {
		GLuint vertexArrayObjectHandle;
	};

	struct CommandDrawArraysInstanced {
		const byte* instanceData;
		uint32_t instanceCount;
		uint32_t instanceStride;
		GLuint instanceBufferObjectHandle;
		GLenum primitiveType;
		uint32_t vertexCount;
	};

	struct CommandDrawElementsInstanced {
		const byte* instanceData;
		uint32_t instanceCount;
		uint32_t instanceStride;
		GLuint instanceBufferObjectHandle;
		GLenum primitiveType;
		GLenum indexType;
		uint32_t indexCount;
	};

	using Commands = LinearBuffer<       //
		CommandSetViewport,              //
		CommandSetScissor,               //
		CommandFill,                     //
		CommandUseProgram,               //
		CommandUseShaderPipelineOptions, //
		CommandUseUniformBuffer,         //
		CommandUseStorageBuffer,         //
		CommandUseTexture2D,             //
		CommandUseTexture2DArray,        //
		CommandUseTextureCube,           //
		CommandUseTextureCubeArray,      //
		CommandUseVertexArray,           //
		CommandDrawArraysInstanced,      //
		CommandDrawElementsInstanced,    //
		GLint[],                         //
		byte[]>;

	[[nodiscard]] static GLenum translateMeshIndexType(MeshIndexType indexType) noexcept {
		switch (indexType) {
			case MeshIndexType::U16: return GL_UNSIGNED_SHORT;
			case MeshIndexType::U32: return GL_UNSIGNED_INT;
		}
		return {};
	}

	[[nodiscard]] static GLenum translateDepthTestPredicate(DepthTestPredicate predicate) noexcept {
		switch (predicate) {
			case DepthTestPredicate::NEVER_PASS: return GL_NEVER;
			case DepthTestPredicate::LESS: return GL_LESS;
			case DepthTestPredicate::LESS_OR_EQUAL: return GL_LEQUAL;
			case DepthTestPredicate::GREATER: return GL_GREATER;
			case DepthTestPredicate::GREATER_OR_EQUAL: return GL_GEQUAL;
			case DepthTestPredicate::EQUAL: return GL_EQUAL;
			case DepthTestPredicate::NOT_EQUAL: return GL_NOTEQUAL;
			case DepthTestPredicate::ALWAYS_PASS: return GL_ALWAYS;
		}
		return {};
	}

	[[nodiscard]] static GLenum translateStencilTestPredicate(StencilTestPredicate predicate) noexcept {
		switch (predicate) {
			case StencilTestPredicate::NEVER_PASS: return GL_NEVER;
			case StencilTestPredicate::LESS: return GL_LESS;
			case StencilTestPredicate::LESS_OR_EQUAL: return GL_LEQUAL;
			case StencilTestPredicate::GREATER: return GL_GREATER;
			case StencilTestPredicate::GREATER_OR_EQUAL: return GL_GEQUAL;
			case StencilTestPredicate::EQUAL: return GL_EQUAL;
			case StencilTestPredicate::NOT_EQUAL: return GL_NOTEQUAL;
			case StencilTestPredicate::ALWAYS_PASS: return GL_ALWAYS;
		}
		return {};
	}

	[[nodiscard]] static GLenum translateStencilBufferOperation(StencilBufferOperation operation) noexcept {
		switch (operation) {
			case StencilBufferOperation::KEEP: return GL_KEEP;
			case StencilBufferOperation::SET_TO_ZERO: return GL_ZERO;
			case StencilBufferOperation::REPLACE: return GL_REPLACE;
			case StencilBufferOperation::INCREMENT_AND_CLAMP: return GL_INCR;
			case StencilBufferOperation::INCREMENT_AND_WRAP: return GL_INCR_WRAP;
			case StencilBufferOperation::DECREMENT_AND_CLAMP: return GL_DECR;
			case StencilBufferOperation::DECREMENT_AND_WRAP: return GL_DECR_WRAP;
			case StencilBufferOperation::BITWISE_INVERT: return GL_INVERT;
		}
		return {};
	}

	[[nodiscard]] static GLenum translatePrimitiveType(PrimitiveType primitiveType) noexcept {
		switch (primitiveType) {
			case PrimitiveType::POINTS: return GL_POINTS;
			case PrimitiveType::LINES: return GL_LINES;
			case PrimitiveType::LINE_STRIP: return GL_LINE_STRIP;
			case PrimitiveType::TRIANGLES: return GL_TRIANGLES;
			case PrimitiveType::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
		}
		return {};
	}

#ifndef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
	[[nodiscard]] static GLenum translatePolygonMode(PolygonMode mode) noexcept {
		switch (mode) {
			case PolygonMode::POINT: return GL_POINT;
			case PolygonMode::LINE: return GL_LINE;
			case PolygonMode::FILL: return GL_FILL;
		}
		return {};
	}
#endif

	[[nodiscard]] static GLenum translateFrontFace(FrontFace face) noexcept {
		switch (face) {
			case FrontFace::CLOCKWISE: return GL_CW;
			case FrontFace::COUNTERCLOCKWISE: return GL_CCW;
		}
		return {};
	}

	[[nodiscard]] static GLenum translateBlendFactor(BlendFactor blendFactor) noexcept {
		switch (blendFactor) {
			case BlendFactor::ZERO: return GL_ZERO;
			case BlendFactor::ONE: return GL_ONE;
			case BlendFactor::SOURCE_COLOR: return GL_SRC_COLOR;
			case BlendFactor::ONE_MINUS_SOURCE_COLOR: return GL_ONE_MINUS_SRC_COLOR;
			case BlendFactor::DESTINATION_COLOR: return GL_DST_COLOR;
			case BlendFactor::ONE_MINUS_DESTINATION_COLOR: return GL_ONE_MINUS_DST_COLOR;
			case BlendFactor::SOURCE_ALPHA: return GL_SRC_ALPHA;
			case BlendFactor::ONE_MINUS_SOURCE_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
			case BlendFactor::DESTINATION_ALPHA: return GL_DST_ALPHA;
			case BlendFactor::ONE_MINUS_DESTINATION__ALPHA: return GL_ONE_MINUS_DST_ALPHA;
			case BlendFactor::CONSTANT_COLOR: return GL_CONSTANT_COLOR;
			case BlendFactor::ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
			case BlendFactor::CONSTANT_ALPHA: return GL_CONSTANT_ALPHA;
			case BlendFactor::ONE_MINUS_CONSTANT_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
			case BlendFactor::SOURCE_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
		}
		return {};
	}

	[[nodiscard]] static GLenum translateBlendOperation(BlendOperation blendOperation) noexcept {
		switch (blendOperation) {
			case BlendOperation::ADD: return GL_FUNC_ADD;
			case BlendOperation::SUBTRACT: return GL_FUNC_SUBTRACT;
			case BlendOperation::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
			case BlendOperation::MIN: return GL_MIN;
			case BlendOperation::MAX: return GL_MAX;
		}
		return {};
	}

	static void applyShaderPipelineOptions(const ShaderPipelineOptions& configuration) {
		switch (configuration.depthBufferMode) {
			case DepthBufferMode::NONE: glDisable(GL_DEPTH_TEST); break;
			case DepthBufferMode::USE_DEPTH_TEST:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(translateDepthTestPredicate(configuration.depthTestPredicate));
				glDepthMask(GL_TRUE);
				break;
			case DepthBufferMode::USE_DEPTH_TEST_READ_ONLY:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(translateDepthTestPredicate(configuration.depthTestPredicate));
				glDepthMask(GL_FALSE);
				break;
		}

		switch (configuration.stencilBufferMode) {
			case StencilBufferMode::NONE: glDisable(GL_STENCIL_TEST); break;
			case StencilBufferMode::USE_STENCIL_TEST:
				glEnable(GL_STENCIL_TEST);
				glStencilFuncSeparate(GL_FRONT,                                                 //
					translateStencilTestPredicate(configuration.stencilTestFrontFacePredicate), //
					static_cast<GLint>(configuration.stencilTestFrontFaceReferenceValue),       //
					static_cast<GLuint>(configuration.stencilTestFrontFaceMask));
				glStencilOpSeparate(GL_FRONT,                                                                        //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnFrontFaceStencilTestFail), //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnFrontFaceDepthTestFail),   //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnFrontFacePass));
				glStencilFuncSeparate(GL_BACK,                                                 //
					translateStencilTestPredicate(configuration.stencilTestBackFacePredicate), //
					static_cast<GLint>(configuration.stencilTestBackFaceReferenceValue),       //
					static_cast<GLuint>(configuration.stencilTestBackFaceMask));
				glStencilOpSeparate(GL_BACK,                                                                        //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnBackFaceStencilTestFail), //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnBackFaceDepthTestFail),   //
					translateStencilBufferOperation(configuration.stencilBufferOperationOnBackFacePass));
				break;
		}

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
		if (configuration.polygonMode != PolygonMode::FILL) {
			static bool warned = false;
			if (!warned) {
				[[unlikely]];
				warned = true;
#ifdef __EMSCRIPTEN__
				std::fprintf(stderr, "Warning: WebGL does not support glPolygonMode. Graphics pipelines specifying PolygonMode POINT or LINE will use FILL instead.\n");
#else
				std::fprintf(stderr, "Warning: OpenGL ES does not support glPolygonMode. Graphics pipelines specifying PolygonMode POINT or LINE will use FILL instead.\n");
#endif
			}
		}
#else
		glPolygonMode(GL_FRONT_AND_BACK, translatePolygonMode(configuration.polygonMode));
#endif

		switch (configuration.faceCullingMode) {
			case FaceCullingMode::NONE: glDisable(GL_CULL_FACE); break;
			case FaceCullingMode::CULL_BACK_FACES:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);
				glFrontFace(translateFrontFace(configuration.frontFace));
				break;
			case FaceCullingMode::CULL_FRONT_FACES:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);
				glFrontFace(translateFrontFace(configuration.frontFace));
				break;
			case FaceCullingMode::CULL_FRONT_AND_BACK_FACES:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT_AND_BACK);
				glFrontFace(translateFrontFace(configuration.frontFace));
				break;
		}

		if (configuration.blendState) {
			glEnable(GL_BLEND);
			glBlendEquationSeparate(                                                    //
				translateBlendOperation(configuration.blendState->colorBlendOperation), //
				translateBlendOperation(configuration.blendState->alphaBlendOperation));
			glBlendFuncSeparate(                                                             //
				translateBlendFactor(configuration.blendState->sourceColorBlendFactor),      //
				translateBlendFactor(configuration.blendState->destinationColorBlendFactor), //
				translateBlendFactor(configuration.blendState->sourceAlphaBlendFactor),      //
				translateBlendFactor(configuration.blendState->destinationAlphaBlendFactor));
			const vec4 blendConstants = configuration.blendState->blendConstants.toLinearRGBA();
			glBlendColor(blendConstants.x, blendConstants.y, blendConstants.z, blendConstants.w);
		} else {
			glDisable(GL_BLEND);
		}

		if (configuration.depthBiasSlopeFactor == 0.0f && configuration.depthBiasConstantFactor == 0.0f) {
			glDisable(GL_POLYGON_OFFSET_FILL);
		} else {
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(configuration.depthBiasSlopeFactor, configuration.depthBiasConstantFactor);
		}
	}

	Device& device;
	RenderTargets renderTargets{};
	RenderPass::Statistics statistics{};
	Optional<Commands> commands{};
	ArrayList<SharedPointer<void>> usedResources{};
	ArrayList<UniformBufferImplementation*> usedUniformBuffers{};
	ArrayList<BufferSetImplementation*> usedBufferSets{};
	Buffer<void*> currentBufferHandles{};
	std::type_index currentShaderMeshTypeIndex = typeid(void);
	MeshImplementation* currentMeshHandle = nullptr;
	Optional<ShaderPipelineOptions> currentShaderPipelineOptions{};
	Buffer<byte> currentInstanceData{};
	GLuint currentShaderProgramHandle = 0;
	uint32_t currentInstanceCount = 0;
	uint32_t currentInstanceStride = 0;
	Optional<Viewport> currentViewport{};
	Arena<3008> commandArena{};

	explicit RenderPassImplementation(Device& device)
		: device(device) {
		commands.emplace(&commandArena, decltype(commandArena)::INPLACE_SIZE);
	}

	~RenderPassImplementation() {
		commands.reset();
	}

	RenderPassImplementation(const RenderPassImplementation& other)
		: device(other.device) {
		*this = other;
	}

	RenderPassImplementation(RenderPassImplementation&&) = delete;

	RenderPassImplementation& operator=(const RenderPassImplementation& other) {
		GREM_ASSERT(&device == &other.device);
		if (this == &other) {
			return *this;
		}
		invalidateContentsOfOldBuffersAvailableForReuse();
		renderTargets = other.renderTargets;
		statistics = other.statistics;
		commands.reset();
		commandArena.release();
		commands.emplace(&commandArena, decltype(commandArena)::INPLACE_SIZE);
		other.commands->visit(Overloaded{
			[&](const RenderPassImplementation::CommandUseProgram& command) -> void { //
				commands->push_back(RenderPassImplementation::CommandUseProgram{
					.storageBufferBindingsUniformLocations = commands->append(command.storageBufferBindingsUniformLocations),
					.storageBufferTextureUnit = command.storageBufferTextureUnit,
					.shaderProgramObjectHandle = command.shaderProgramObjectHandle,
					.srgbCorrectionModeUniformLocation = command.srgbCorrectionModeUniformLocation,
					.framebufferHeightUniformLocation = command.framebufferHeightUniformLocation,
					.hasColorOutput = command.hasColorOutput,
				});
			},
			[&](const RenderPassImplementation::CommandDrawArraysInstanced& command) -> void { //
				commands->push_back(RenderPassImplementation::CommandDrawArraysInstanced{
					.instanceData = commands->append(Span{command.instanceData, command.instanceCount * command.instanceStride}).data(),
					.instanceCount = command.instanceCount,
					.instanceStride = command.instanceStride,
					.instanceBufferObjectHandle = command.instanceBufferObjectHandle,
					.primitiveType = command.primitiveType,
					.vertexCount = command.vertexCount,
				});
			},
			[&](const RenderPassImplementation::CommandDrawElementsInstanced& command) -> void { //
				commands->push_back(RenderPassImplementation::CommandDrawElementsInstanced{
					.instanceData = commands->append(Span{command.instanceData, command.instanceCount * command.instanceStride}).data(),
					.instanceCount = command.instanceCount,
					.instanceStride = command.instanceStride,
					.instanceBufferObjectHandle = command.instanceBufferObjectHandle,
					.primitiveType = command.primitiveType,
					.indexType = command.indexType,
					.indexCount = command.indexCount,
				});
			},
			[&](const Span<const GLint>) -> void {},
			[&](const Span<const byte>) -> void {},
			[&](const auto& command) -> void { commands->push_back(command); },
		});
		usedResources = other.usedResources;
		usedUniformBuffers = other.usedUniformBuffers;
		usedBufferSets = other.usedBufferSets;
		currentBufferHandles = other.currentBufferHandles;
		currentShaderMeshTypeIndex = other.currentShaderMeshTypeIndex;
		currentMeshHandle = other.currentMeshHandle;
		currentShaderPipelineOptions = other.currentShaderPipelineOptions;
		currentInstanceData = other.currentInstanceData;
		currentShaderProgramHandle = other.currentShaderProgramHandle;
		currentInstanceCount = other.currentInstanceCount;
		currentInstanceStride = other.currentInstanceStride;
		currentViewport = other.currentViewport;
		return *this;
	}

	RenderPassImplementation& operator=(RenderPassImplementation&&) = delete;

	void invalidateContentsOfOldBuffersAvailableForReuse() noexcept;

	void reset() noexcept {
		invalidateContentsOfOldBuffersAvailableForReuse();
		renderTargets = {};
		statistics = {};
		commands.reset();
		commandArena.release();
		commands.emplace(&commandArena, decltype(commandArena)::INPLACE_SIZE);
		usedResources.clear();
		usedUniformBuffers.clear();
		usedBufferSets.clear();
		currentBufferHandles.clear();
		currentShaderMeshTypeIndex = typeid(void);
		currentMeshHandle = nullptr;
		currentShaderPipelineOptions.reset();
		currentInstanceData.clear();
		currentShaderProgramHandle = 0;
		currentInstanceCount = 0;
		currentInstanceStride = 0;
		currentViewport.reset();
	}

	void commitInstances() {
		if (currentInstanceCount == 0) {
			return;
		}
		const Span<const byte> instanceData = commands->append(Span{currentInstanceData});
		GREM_ASSERT(currentMeshHandle->meshTypeIndex == currentShaderMeshTypeIndex && "Mesh does not match the mesh type of its shader.");
		if (currentMeshHandle->indexType) {
			commands->push_back(RenderPassImplementation::CommandDrawElementsInstanced{
				.instanceData = instanceData.data(),
				.instanceCount = currentInstanceCount,
				.instanceStride = currentInstanceStride,
				.instanceBufferObjectHandle = (currentMeshHandle->instanceBufferObject) ? currentMeshHandle->instanceBufferObject->object.get() : 0,
				.primitiveType = translatePrimitiveType(currentShaderPipelineOptions->primitiveType),
				.indexType = translateMeshIndexType(*currentMeshHandle->indexType),
				.indexCount = currentMeshHandle->indexCount,
			});
		} else {
			commands->push_back(RenderPassImplementation::CommandDrawArraysInstanced{
				.instanceData = instanceData.data(),
				.instanceCount = currentInstanceCount,
				.instanceStride = currentInstanceStride,
				.instanceBufferObjectHandle = (currentMeshHandle->instanceBufferObject) ? currentMeshHandle->instanceBufferObject->object.get() : 0,
				.primitiveType = translatePrimitiveType(currentShaderPipelineOptions->primitiveType),
				.vertexCount = currentMeshHandle->vertexCount,
			});
		}
		currentInstanceData.clear();
		currentInstanceCount = 0;
		++statistics.totalDrawCallCount;
	}

	void render() {
		commitInstances();

		const bool isDefaultFramebufferTarget = renderTargets.colorTarget && renderTargets.colorTarget->texture->get()->type == TextureType::SWAPCHAIN;
		GLuint framebufferObjectHandle = 0;
		if (isDefaultFramebufferTarget) {
			Window& window = *renderTargets.colorTarget->texture->get()->object.get<Window*>();
			SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
		} else {
			const TextureAspects uninitializedTargetAspects = match(renderTargets.clearMode)(         //
				[](const RetainValues&) -> TextureAspects { return {}; },                             //
				[](const ClearValues& clearValues) -> TextureAspects { return clearValues.aspects; }, //
				[](const UndefinedClearValues& undefinedClearValues) -> TextureAspects { return undefinedClearValues.aspects; });
			const GLbitfield uninitializedTargetAspectMask = TextureImplementation::getAspectBits(uninitializedTargetAspects);
			framebufferObjectHandle =
				device.get()
					->getDrawFramebufferContext(
						DeviceImplementation::acquireDrawFramebufferContextKey(renderTargets.colorTarget, renderTargets.depthStencilTarget, uninitializedTargetAspectMask))
					.framebufferObject.get();
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferObjectHandle);

		bool hasColorAttachment = false;
		bool hasColorOutput = true;
		GLint srgbCorrectionMode = 0;
		uint32_t actualFramebufferHeight = 0;
		Span<const GLint> storageBufferBindingsUniformLocations{};
		Optional<GLuint> storageBufferTextureHandle{};
		if (renderTargets.colorTarget) {
			const TextureFormat internalFormat = renderTargets.colorTarget->texture->get()->internalFormat;
			const TextureAspects aspects = (isDefaultFramebufferTarget) ? TextureAspects::COLOR_DEPTH_STENCIL : Texture::getFormatAspects(internalFormat);
			const TextureAspects colorAspects = renderTargets.colorTarget->subresource.aspects & aspects;
			if (colorAspects.contains(TextureAspect::COLOR)) {
				hasColorAttachment = true;
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			} else {
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}

			GLint colorEncoding{};
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
			glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, (isDefaultFramebufferTarget) ? GL_BACK : GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
				&colorEncoding);
#else
			// Apparently, NVIDIA's drivers have a longstanding bug (unfixed as
			// of 2026) where glGetFramebufferAttachmentParameteriv() always
			// reports the default framebuffer as being GL_LINEAR even when it's
			// not. To work around this, we assume that for the default
			// framebuffer, querying SDL_GL_FRAMEBUFFER_SRGB_CAPABLE directly
			// from the GL context created by SDL should give a more accurate
			// result. Note that the ES profile doesn't seem to need this
			// workaround for some reason.
			//
			// Other references to the same driver bug found in the wild:
			// - 2014: https://stackoverflow.com/questions/25842211/opengl-srgb-framebuffer-oddity
			// - 2014, bumped in 2017 and 2018: https://forums.developer.nvidia.com/t/gl-framebuffer-srgb-functions-incorrectly/34889
			// - 2022: https://forums.developer.nvidia.com/t/glgetframebufferattachmentparameteriv-with-gl-framebuffer-attachment-color-encoding-returns-wrong-value/205092
			//
			// AMD's proprietary driver on Windows seems to have a similar issue
			// as well (the AMDgpu driver on Linux does not).
			if (isDefaultFramebufferTarget) {
				if (int srgbCapable{}; SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, &srgbCapable)) {
					colorEncoding = (srgbCapable == 1) ? GL_SRGB : GL_LINEAR;
				} else {
					colorEncoding = GL_SRGB; // If the query failed for some reason, assume sRGB, since it's what we asked for at context creation.
				}
			} else {
				glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &colorEncoding);
			}
#endif

			if (isDefaultFramebufferTarget || Texture::getTransferFunction(internalFormat) == Color::TransferFunction::SRGB) {
				if (colorEncoding == GL_LINEAR) {
					srgbCorrectionMode = 1;
				}
			} else {
				if (colorEncoding == GL_SRGB) {
					srgbCorrectionMode = -1;
				}
			}

			actualFramebufferHeight = resource::Image::getMipLevelSize2D(renderTargets.colorTarget->texture->getSize2D(), renderTargets.colorTarget->subresource.mipLevel).height;
		} else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

			if (renderTargets.depthStencilTarget) {
				actualFramebufferHeight =
					resource::Image::getMipLevelSize2D(renderTargets.depthStencilTarget->texture->getSize2D(), renderTargets.depthStencilTarget->subresource.mipLevel).height;
			} else if (renderTargets.resolveTarget) {
				actualFramebufferHeight =
					resource::Image::getMipLevelSize2D(renderTargets.resolveTarget->texture->getSize2D(), renderTargets.resolveTarget->subresource.mipLevel).height;
			}
		}
		const float framebufferHeight = static_cast<float>(actualFramebufferHeight + (actualFramebufferHeight & 1));

		[[maybe_unused]] bool hasDepthAttachment = false;
		[[maybe_unused]] bool hasStencilAttachment = false;
		if (renderTargets.depthStencilTarget) {
			const TextureAspects aspects =
				(isDefaultFramebufferTarget) ? TextureAspects::COLOR_DEPTH_STENCIL : Texture::getFormatAspects(renderTargets.depthStencilTarget->texture->get()->internalFormat);
			const TextureAspects depthStencilAspects = renderTargets.depthStencilTarget->subresource.aspects & aspects;
			if (depthStencilAspects.contains(TextureAspect::DEPTH)) {
				hasDepthAttachment = true;
				glDepthMask(GL_TRUE);
			} else {
				glDepthMask(GL_FALSE);
			}
			if (depthStencilAspects.contains(TextureAspect::STENCIL)) {
				hasStencilAttachment = true;
				glStencilMask(static_cast<GLuint>(~GLuint{0}));
			} else {
				glStencilMask(GLuint{0});
			}
		} else {
			glDepthMask(GL_FALSE);
			glStencilMask(GLuint{0});
		}

		const auto clearBoundFramebuffer = [&](GLbitfield aspectMask, ClearValues clearValues) -> void {
			if (aspectMask == 0) {
				return;
			}
			switch (srgbCorrectionMode) {
				case -1: {
					const vec4 clearColor = clearValues.color.toLinearRGBA();
					clearValues.color = Color::fromLinear(Color::convertSRGBToLinear(vec3{clearColor}), clearColor.w);
					break;
				}
				case 1: {
					const vec4 clearColor = clearValues.color.toLinearRGBA();
					clearValues.color = Color::fromLinear(Color::convertLinearToSRGB(vec3{clearColor}), clearColor.w);
					break;
				}
				default: break;
			}
			if (hasColorAttachment && !hasColorOutput) {
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			if (isDefaultFramebufferTarget) {
				const vec4 clearColor = clearValues.color.toLinearRGBA();
				glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
				glClearDepthf(clearValues.depth);
#else
				glClearDepth(clearValues.depth);
#endif
				glClearStencil(static_cast<GLint>(clearValues.stencil));
				glClear(aspectMask);
			} else {
				if (renderTargets.colorTarget) {
					const TextureFormat colorFormat = renderTargets.colorTarget->texture->get()->internalFormat;
					TextureImplementation::clearBoundFramebuffer(colorFormat, aspectMask, clearValues);
				}
				if (renderTargets.depthStencilTarget) {
					const TextureFormat depthStencilFormat = renderTargets.depthStencilTarget->texture->get()->internalFormat;
					TextureImplementation::clearBoundFramebuffer(depthStencilFormat, aspectMask, clearValues);
				}
			}
			if (hasColorAttachment && !hasColorOutput) {
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
		};

		GREM_MATCH(renderTargets.clearMode) {
			GREM_CASE(const RetainValues& retainValues) break;
			GREM_CASE(const ClearValues& clearValues) {
				glDisable(GL_SCISSOR_TEST);
				clearBoundFramebuffer(TextureImplementation::getAspectBits(clearValues.aspects), clearValues);
				break;
			}
			GREM_CASE(const UndefinedClearValues& undefinedClearValues) {
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
				InplaceBuffer<GLenum, 3> attachments{};
				if (hasColorAttachment && undefinedClearValues.aspects.contains(TextureAspect::COLOR)) {
					attachments.push_back((isDefaultFramebufferTarget) ? GL_COLOR : GL_COLOR_ATTACHMENT0);
				}
				if (hasDepthAttachment && undefinedClearValues.aspects.contains(TextureAspect::DEPTH)) {
					attachments.push_back((isDefaultFramebufferTarget) ? GL_DEPTH : GL_DEPTH_ATTACHMENT);
				}
				if (hasStencilAttachment && undefinedClearValues.aspects.contains(TextureAspect::STENCIL)) {
					attachments.push_back((isDefaultFramebufferTarget) ? GL_STENCIL : GL_STENCIL_ATTACHMENT);
				}
				if (!attachments.empty()) {
					glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLsizei>(attachments.size()), attachments.data());
				}
#endif
				break;
			}
		}

		Optional<Viewport> activeViewport{};
		commands->visit(Overloaded{
			[&](const RenderPassImplementation::CommandSetViewport& command) -> void { //
				const Offset2D offset = command.viewport.region.offset;
				const Extent2D size = command.viewport.region.size;
				glViewport(static_cast<GLint>(offset.x), static_cast<GLint>(offset.y), static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
				glDepthRangef(command.viewport.minDepth, command.viewport.maxDepth);
#else
				glDepthRange(static_cast<GLdouble>(command.viewport.minDepth), static_cast<GLdouble>(command.viewport.maxDepth));
#endif
				if (const Optional<Region2D> scissor = command.viewport.scissor) {
					glEnable(GL_SCISSOR_TEST);
					glScissor(static_cast<GLint>(scissor->offset.x), static_cast<GLint>(scissor->offset.y), static_cast<GLsizei>(scissor->size.width),
						static_cast<GLsizei>(scissor->size.height));
				} else {
					glDisable(GL_SCISSOR_TEST);
				}
				activeViewport = command.viewport;
			},
			[&](const RenderPassImplementation::CommandSetScissor& command) -> void { //
				GREM_ASSERT(activeViewport);
				if (command.scissor) {
					glEnable(GL_SCISSOR_TEST);
					glScissor(static_cast<GLint>(command.scissor->offset.x), static_cast<GLint>(command.scissor->offset.y), static_cast<GLsizei>(command.scissor->size.width),
						static_cast<GLsizei>(command.scissor->size.height));
				} else {
					glDisable(GL_SCISSOR_TEST);
				}
				activeViewport->scissor = command.scissor;
			},
			[&](const RenderPassImplementation::CommandFill& command) -> void { //
				glEnable(GL_SCISSOR_TEST);
				glScissor(static_cast<GLint>(command.targetRegion.offset.x), static_cast<GLint>(command.targetRegion.offset.y),
					static_cast<GLsizei>(command.targetRegion.size.width), static_cast<GLsizei>(command.targetRegion.size.height));
				clearBoundFramebuffer(command.aspectMask, ClearValues{.color = Color::fromLinear(command.color), .depth = command.depth, .stencil = command.stencil});
				if (activeViewport) {
					if (activeViewport->scissor) {
						glScissor(static_cast<GLint>(activeViewport->scissor->offset.x), static_cast<GLint>(activeViewport->scissor->offset.y),
							static_cast<GLsizei>(activeViewport->scissor->size.width), static_cast<GLsizei>(activeViewport->scissor->size.height));
					} else {
						glDisable(GL_SCISSOR_TEST);
					}
				}
			},
			[&](const RenderPassImplementation::CommandUseProgram& command) -> void { //
				GREM_ASSERT(activeViewport);
				storageBufferBindingsUniformLocations = command.storageBufferBindingsUniformLocations;
				if (!storageBufferBindingsUniformLocations.empty()) {
					if (!storageBufferTextureHandle) {
						storageBufferTextureHandle = detail::flushStorageBufferTexture(device);
					}
					glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.storageBufferTextureUnit));
					glBindTexture(GL_TEXTURE_2D, *storageBufferTextureHandle);
				}
				glUseProgram(command.shaderProgramObjectHandle);
				if (command.srgbCorrectionModeUniformLocation != -1) {
					glUniform1i(command.srgbCorrectionModeUniformLocation, srgbCorrectionMode);
				}
				if (command.framebufferHeightUniformLocation != -1) {
					glUniform1f(command.framebufferHeightUniformLocation, framebufferHeight);
				}
				if (command.hasColorOutput != hasColorOutput) {
					if (hasColorAttachment) {
						if (hasColorOutput) {
							glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
						} else {
							glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
						}
					}
					hasColorOutput = command.hasColorOutput;
				}
			},
			[&](const RenderPassImplementation::CommandUseShaderPipelineOptions& command) -> void { //
				GREM_ASSERT(
					(command.shaderPipelineOptions.depthBufferMode == DepthBufferMode::NONE || hasDepthAttachment) && "Cannot use depth test without a depth render target.");
				GREM_ASSERT((command.shaderPipelineOptions.stencilBufferMode == StencilBufferMode::NONE || hasStencilAttachment) &&
							"Cannot use stencil test without a stencil render target.");
				applyShaderPipelineOptions(command.shaderPipelineOptions);
			},
			[&](const RenderPassImplementation::CommandUseUniformBuffer& command) -> void { //
				glBindBufferBase(GL_UNIFORM_BUFFER, command.uniformBlockBinding, command.uniformBufferObjectHandle);
			},
			[&](const RenderPassImplementation::CommandUseStorageBuffer& command) -> void { //
				GREM_ASSERT(storageBufferTextureHandle);
				const GLint location = storageBufferBindingsUniformLocations[command.storageBufferBinding];
				if (location != -1) {
					GREM_ASSERT(command.storageBuffer);
					if (command.storageBuffer->squareAllocation.allocatedWidth == 0) {
						glUniform4ui(location, GLuint{0}, GLuint{0}, GLuint{0}, GLuint{0});
					} else {
						GREM_ASSERT(isPowerOf2(command.storageBuffer->squareAllocation.allocatedWidth));
						const GLuint x = command.storageBuffer->squareAllocation.x;
						const GLuint y = command.storageBuffer->squareAllocation.y;
						const GLuint strideMask = command.storageBuffer->squareAllocation.allocatedWidth - 1;
						const GLuint strideShift = static_cast<uint32_t>(countTrailingZeroBits(command.storageBuffer->squareAllocation.allocatedWidth));
						glUniform4ui(location, x, y, strideMask, strideShift);
					}
				}
			},
			[&](const RenderPassImplementation::CommandUseTexture2D& command) -> void { //
				glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.textureUnit));
				glBindTexture(GL_TEXTURE_2D, command.textureObjectHandle);
			},
			[&](const RenderPassImplementation::CommandUseTexture2DArray& command) -> void { //
				glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.textureUnit));
				glBindTexture(GL_TEXTURE_2D_ARRAY, command.textureObjectHandle);
			},
			[&](const RenderPassImplementation::CommandUseTextureCube& command) -> void { //
				glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.textureUnit));
				glBindTexture(GL_TEXTURE_CUBE_MAP, command.textureObjectHandle);
			},
			[&](const RenderPassImplementation::CommandUseTextureCubeArray& command) -> void { //
				glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.textureUnit));
				glBindTexture(GL_TEXTURE_2D_ARRAY, command.textureObjectHandle); // Note: Cube arrays are emulated using 2D arrays.
			},
			[&](const RenderPassImplementation::CommandUseVertexArray& command) -> void { //
				glBindVertexArray(command.vertexArrayObjectHandle);
			},
			[&](const RenderPassImplementation::CommandDrawArraysInstanced& command) -> void { //
				if (command.instanceBufferObjectHandle != 0) {
					glBindBuffer(GL_ARRAY_BUFFER, command.instanceBufferObjectHandle);
					detail::uploadInstancesToBoundInstanceBuffer(
						Span<const byte>{command.instanceData, static_cast<size_t>(command.instanceCount) * static_cast<size_t>(command.instanceStride)});
				}
				GREM_ASSERT(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
				glDrawArraysInstanced(command.primitiveType, 0, static_cast<GLsizei>(command.vertexCount), static_cast<GLsizei>(command.instanceCount));
			},
			[&](const RenderPassImplementation::CommandDrawElementsInstanced& command) -> void { //
				if (command.instanceBufferObjectHandle != 0) {
					glBindBuffer(GL_ARRAY_BUFFER, command.instanceBufferObjectHandle);
					detail::uploadInstancesToBoundInstanceBuffer(
						Span<const byte>{command.instanceData, static_cast<size_t>(command.instanceCount) * static_cast<size_t>(command.instanceStride)});
				}
				GREM_ASSERT(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
				glDrawElementsInstanced(command.primitiveType, static_cast<GLsizei>(command.indexCount), command.indexType, nullptr, static_cast<GLsizei>(command.instanceCount));
			},
			[](const Span<const GLint>) -> void {},
			[](const Span<const byte>) -> void {},
		});

		if (renderTargets.resolveTarget) {
			GREM_ASSERT(renderTargets.resolveTarget->texture && *renderTargets.resolveTarget->texture);
			GREM_ASSERT(renderTargets.resolveTarget->subresource.aspects.contains(TextureAspect::COLOR));
			GREM_ASSERT(renderTargets.colorTarget && *renderTargets.colorTarget->texture);
			const TextureImplementation& colorTargetTexture = *renderTargets.colorTarget->texture->get();
			const Extent2D colorTargetSize{.width = colorTargetTexture.size.width, .height = colorTargetTexture.size.height};
			device.get()->blit(*renderTargets.resolveTarget, Region2D{.offset{.x = 0, .y = 0}, .size = colorTargetSize},
				TextureRegion2DConstReference{
					.texture = renderTargets.colorTarget->texture,
					.region{
						.aspects = TextureAspect::COLOR,
						.offset{.x = 0, .y = 0, .z = static_cast<int32_t>(renderTargets.colorTarget->subresource.layer)},
						.size = colorTargetSize,
						.mipLevel = renderTargets.colorTarget->subresource.mipLevel,
					},
				},
				TextureFilter::NEAREST);

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
			const TextureAspects intermediateTextureAspectsToStore = match(renderTargets.resolveMode)(                                    //
				[](const StoreIntermediateValues& storeIntermediateValues) -> TextureAspects { return storeIntermediateValues.aspects; }, //
				[](const DiscardIntermediateValues&) -> TextureAspects { return {}; });
			InplaceBuffer<GLenum, 3> attachments{};
			if (renderTargets.colorTarget && !intermediateTextureAspectsToStore.contains(TextureAspect::COLOR)) {
				if (hasColorAttachment) {
					attachments.push_back(GL_COLOR_ATTACHMENT0);
				}
			}
			if (renderTargets.depthStencilTarget) {
				if (hasDepthAttachment && !intermediateTextureAspectsToStore.contains(TextureAspect::DEPTH)) {
					attachments.push_back(GL_DEPTH_ATTACHMENT);
				}
				if (hasStencilAttachment && !intermediateTextureAspectsToStore.contains(TextureAspect::STENCIL)) {
					attachments.push_back(GL_STENCIL_ATTACHMENT);
				}
			}
			if (!attachments.empty()) {
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferObjectHandle);
				glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLsizei>(attachments.size()), attachments.data());
			}
#endif
		}
	}
};

} // namespace grem::graphics

#endif
