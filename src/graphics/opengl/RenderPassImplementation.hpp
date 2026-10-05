// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_RENDER_PASS_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_RENDER_PASS_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/LinearBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/shaders.hpp>

#include "MeshImplementation.hpp"
#include "buffer_implementations.hpp"
#include "opengl.hpp"

#include <typeindex> // std::type_index

namespace grem::graphics {

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

	void commitInstances();

	void render();
};

} // namespace grem::graphics

#endif
