// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/resource/Image.hpp>

#include "DeviceImplementation.hpp"
#include "MeshImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "ShaderImplementation.hpp"
#include "TextureImplementation.hpp"
#include "buffer_implementations.hpp"
#include "opengl.hpp"

#include <SDL3/SDL.h> // SDL_...

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
#include <cstdio> // stderr, std::fprintf
#endif

namespace grem::graphics {

namespace {

[[nodiscard]] GLenum translateMeshIndexType(MeshIndexType indexType) noexcept {
	switch (indexType) {
		case MeshIndexType::U16: return GL_UNSIGNED_SHORT;
		case MeshIndexType::U32: return GL_UNSIGNED_INT;
	}
	return {};
}

[[nodiscard]] GLenum translateDepthTestPredicate(DepthTestPredicate predicate) noexcept {
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

[[nodiscard]] GLenum translateStencilTestPredicate(StencilTestPredicate predicate) noexcept {
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

[[nodiscard]] GLenum translateStencilBufferOperation(StencilBufferOperation operation) noexcept {
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

[[nodiscard]] GLenum translatePrimitiveType(PrimitiveType primitiveType) noexcept {
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
[[nodiscard]] GLenum translatePolygonMode(PolygonMode mode) noexcept {
	switch (mode) {
		case PolygonMode::POINT: return GL_POINT;
		case PolygonMode::LINE: return GL_LINE;
		case PolygonMode::FILL: return GL_FILL;
	}
	return {};
}
#endif

[[nodiscard]] GLenum translateFrontFace(FrontFace face) noexcept {
	switch (face) {
		case FrontFace::CLOCKWISE: return GL_CW;
		case FrontFace::COUNTERCLOCKWISE: return GL_CCW;
	}
	return {};
}

[[nodiscard]] GLenum translateBlendFactor(BlendFactor blendFactor) noexcept {
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

[[nodiscard]] GLenum translateBlendOperation(BlendOperation blendOperation) noexcept {
	switch (blendOperation) {
		case BlendOperation::ADD: return GL_FUNC_ADD;
		case BlendOperation::SUBTRACT: return GL_FUNC_SUBTRACT;
		case BlendOperation::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
		case BlendOperation::MIN: return GL_MIN;
		case BlendOperation::MAX: return GL_MAX;
	}
	return {};
}

[[nodiscard]] SharedPointer<RenderPassImplementation> acquireRenderPass(Device& device, TextureSubresourceReference resolveTarget,
	Span<const TextureSubresourceReference> renderTargets) {
	GREM_PROFILE_FUNCTION();

	if (renderTargets.empty()) {
		throw graphics::Error{"Render pass has no render targets."};
	}

	if (resolveTarget.texture) {
		if (!Texture::isFramebufferCompatibleFormat(resolveTarget.texture->getInternalFormat()) && resolveTarget.texture->getType() != TextureType::SWAPCHAIN) {
			throw graphics::Error{"Cannot resolve to a non-framebuffer-compatible texture."};
		}
		if (resolveTarget.texture->get()->maxMultisampleCount > 1) {
			throw graphics::Error{"Cannot resolve to a multisampled texture."};
		}
		if (!Texture::getFormatAspects(resolveTarget.texture->getInternalFormat()).contains(TextureAspect::COLOR) ||
			!resolveTarget.subresource.aspects.contains(TextureAspect::COLOR)) {
			throw graphics::Error{"Cannot resolve to a non-color texture."};
		}
	}

	if (anyOf(renderTargets, [](const TextureSubresourceReference& renderTarget) -> bool {
			GREM_ASSERT(renderTarget.texture);
			return !Texture::isFramebufferCompatibleFormat(renderTarget.texture->getInternalFormat()) && renderTarget.texture->getType() != TextureType::SWAPCHAIN;
		})) {
		throw graphics::Error{"Cannot render to a non-framebuffer-compatible texture."};
	}

	const Extent2D maxFramebufferSize = device.getSupportedFeatures().maxFramebufferSize;
	if (anyOf(renderTargets, [&](const TextureSubresourceReference& renderTarget) -> bool {
			return renderTarget.texture->getWidth() > maxFramebufferSize.width && renderTarget.texture->getHeight() > maxFramebufferSize.height;
		})) {
		throw graphics::Error{"Maximum framebuffer size exceeded."};
	}

	DoubleEndedQueue<SharedPointer<RenderPassImplementation>>& renderPassesForReuse = device.get()->renderPassesForReuse;
	for (auto it = renderPassesForReuse.begin(); it != renderPassesForReuse.end(); ++it) {
		if (it->use_count() == 1) {
			SharedPointer<RenderPassImplementation> result = std::move(*it);
			renderPassesForReuse.erase(it);
			result->reset();
			return result;
		}
	}
	return SharedPointer<RenderPassImplementation>::create(device);
}

void ensureExclusiveRenderPassAccess(SharedPointer<RenderPassImplementation>& implementation) {
	GREM_ASSERT(implementation);
	if (implementation.use_count() == 1) {
		[[likely]];
		return;
	}

	GREM_PROFILE_FUNCTION();

	DoubleEndedQueue<SharedPointer<RenderPassImplementation>>& renderPassesForReuse = implementation->device.get()->renderPassesForReuse;
	for (auto it = renderPassesForReuse.begin(); it != renderPassesForReuse.end(); ++it) {
		if (it->use_count() == 1) {
			SharedPointer<RenderPassImplementation> newRenderPass = std::move(*it);
			renderPassesForReuse.erase(it);
			*newRenderPass = *implementation;
			renderPassesForReuse.push_back(std::move(implementation));
			implementation = std::move(newRenderPass);
			return;
		}
	}

	SharedPointer<RenderPassImplementation> newRenderPass = SharedPointer<RenderPassImplementation>::create(*implementation);
	renderPassesForReuse.push_back(std::move(implementation));
	implementation = std::move(newRenderPass);
}

void getRenderTargets(RenderPassImplementation::RenderTargets& output, Span<const TextureSubresourceReference> renderTargets) {
	for (const TextureSubresourceReference& renderTarget : renderTargets) {
		GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
		const TextureImplementation& targetTexture = *renderTarget.texture->get();
		if (targetTexture.type == TextureType::SWAPCHAIN) {
			GREM_ASSERT(!output.colorTarget);
			GREM_ASSERT(!output.depthStencilTarget);
			output.colorTarget = renderTarget;
			output.depthStencilTarget = renderTarget;
		} else {
			const TextureAspects aspects = renderTarget.subresource.aspects & Texture::getFormatAspects(targetTexture.internalFormat);
			if (aspects.contains(TextureAspect::COLOR)) {
				if (output.colorTarget) {
					throw graphics::Error{"Cannot render to multiple color targets."};
				}
				if (output.resolveTarget && targetTexture.maxMultisampleCount <= 1) {
					throw graphics::Error{"Cannot resolve from a non-multisampled texture."};
				}
				output.colorTarget = renderTarget;
			}
			if (aspects.containsAnyOf(TextureAspects::DEPTH_STENCIL)) {
				if (output.depthStencilTarget) {
					throw graphics::Error{"Cannot render to multiple depth/stencil targets."};
				}
				if (output.resolveTarget && targetTexture.maxMultisampleCount <= 1) {
					throw graphics::Error{"Cannot resolve from a non-multisampled texture."};
				}
				output.depthStencilTarget = renderTarget;
			}
		}
	}
}

[[nodiscard]] bool areBufferHandlesCurrent(const RenderPassImplementation& implementation, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
	if (bufferHandles.size() != implementation.currentBufferHandles.size()) {
		return false;
	}
	for (size_t i = 0; i < bufferHandles.size(); ++i) {
		if (bufferHandles[i].second.get() != implementation.currentBufferHandles[i]) {
			return false;
		}
	}
	return true;
}

void setupTextureBindings(RenderPassImplementation& implementation, GLint& textureUnit, Span<const ParameterDescription> parameterDescriptions,
	Span<const SharedPointer<TextureImplementation>> textures) {
	size_t textureIndex = 0;
	for (const ParameterDescription& parameterDescription : parameterDescriptions) {
		if (isTextureParameter(parameterDescription.type)) {
			const GLuint textureObjectHandle = textures[textureIndex]->object.as<detail::TextureObject>().get();
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
				case ParameterType::MAT4: break;
				case ParameterType::SAMPLER_2D: [[fallthrough]];
				case ParameterType::SAMPLER_2D_SHADOW:
					implementation.commands->push_back(RenderPassImplementation::CommandUseTexture2D{
						.textureUnit = textureUnit,
						.textureObjectHandle = textureObjectHandle,
					});
					break;
				case ParameterType::SAMPLER_2D_ARRAY: [[fallthrough]];
				case ParameterType::SAMPLER_2D_ARRAY_SHADOW:
					implementation.commands->push_back(RenderPassImplementation::CommandUseTexture2DArray{
						.textureUnit = textureUnit,
						.textureObjectHandle = textureObjectHandle,
					});
					break;
				case ParameterType::SAMPLER_CUBE: [[fallthrough]];
				case ParameterType::SAMPLER_CUBE_SHADOW:
					implementation.commands->push_back(RenderPassImplementation::CommandUseTextureCube{
						.textureUnit = textureUnit,
						.textureObjectHandle = textureObjectHandle,
					});
					break;
				case ParameterType::SAMPLER_CUBE_ARRAY: [[fallthrough]];
				case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW:
					implementation.commands->push_back(RenderPassImplementation::CommandUseTextureCubeArray{
						.textureUnit = textureUnit,
						.textureObjectHandle = textureObjectHandle,
					});
					break;
			}
			++textureIndex;
			++textureUnit;
		}
	}
}

void setupInstanceContext(RenderPassImplementation& implementation, bool& boundNewBufferHandles, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles,
	SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle) {
	bool meshUniformBufferRebindRequired = false;
	bool bufferRebindRequired = !boundNewBufferHandles;

	if (shaderPipelineHandle->programObject.get() != implementation.currentShaderProgramHandle) {
		implementation.commitInstances();
		implementation.currentShaderProgramHandle = shaderPipelineHandle->programObject.get();
		implementation.currentShaderMeshTypeIndex = shaderPipelineHandle->meshTypeIndex;
		implementation.currentInstanceStride = shaderPipelineHandle->instanceStride;
		implementation.commands->push_back(RenderPassImplementation::CommandUseProgram{
			.storageBufferBindingsUniformLocations = implementation.commands->append(Span{shaderPipelineHandle->storageBufferBindingsUniformLocations}),
			.storageBufferTextureUnit = shaderPipelineHandle->storageBufferTextureUnit,
			.shaderProgramObjectHandle = shaderPipelineHandle->programObject.get(),
			.srgbCorrectionModeUniformLocation = shaderPipelineHandle->srgbCorrectionModeUniformLocation,
			.framebufferHeightUniformLocation = shaderPipelineHandle->framebufferHeightUniformLocation,
			.hasColorOutput = shaderPipelineHandle->hasColorOutput,
		});
		meshUniformBufferRebindRequired = true;
		bufferRebindRequired = true;
	}

	if (shaderPipelineHandle->shaderPipelineOptions != implementation.currentShaderPipelineOptions) {
		implementation.commitInstances();
		implementation.currentShaderPipelineOptions = shaderPipelineHandle->shaderPipelineOptions;
		implementation.commands->push_back(RenderPassImplementation::CommandUseShaderPipelineOptions{
			.shaderPipelineOptions = shaderPipelineHandle->shaderPipelineOptions,
		});
	}

	if (meshHandle.get() != implementation.currentMeshHandle) {
		implementation.commitInstances();
		implementation.currentMeshHandle = meshHandle.get();
		implementation.commands->push_back(RenderPassImplementation::CommandUseVertexArray{
			.vertexArrayObjectHandle = meshHandle->vertexArrayObject.get(),
		});
		if (!shaderPipelineHandle->parameterDescriptions.empty()) {
			meshUniformBufferRebindRequired = true;
		}
	}

	GLuint uniformBlockBinding = 0;
	GLuint storageBufferBinding = 0;
	GLint textureUnit = 0;

	if (!shaderPipelineHandle->parameterDescriptions.empty()) {
		if (meshUniformBufferRebindRequired) {
			if (anyOf(shaderPipelineHandle->parameterDescriptions,
					[](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
				implementation.commands->push_back(RenderPassImplementation::CommandUseUniformBuffer{
					.uniformBlockBinding = uniformBlockBinding,
					.uniformBufferObjectHandle = meshHandle->uniformBufferObject->object.get(),
				});
				++uniformBlockBinding;
			}
			setupTextureBindings(implementation, textureUnit, shaderPipelineHandle->parameterDescriptions, meshHandle->textures);
		} else {
			if (anyOf(shaderPipelineHandle->parameterDescriptions,
					[](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
				++uniformBlockBinding;
			}
			for (const ParameterDescription& parameterDescription : shaderPipelineHandle->parameterDescriptions) {
				if (isTextureParameter(parameterDescription.type)) {
					++textureUnit;
				}
			}
		}
	}

	if (bufferRebindRequired) {
		if (!boundNewBufferHandles) {
			boundNewBufferHandles = true;
			implementation.commitInstances();
			implementation.currentBufferHandles.clear();
			for (const Pair<BufferLayoutReference, SharedPointer<void>>& bufferHandle : bufferHandles) {
				SharedPointer<void> buffer = bufferHandle.second;
				GREM_ASSERT(buffer);
				implementation.currentBufferHandles.push_back(buffer.get());
				implementation.usedResources.push_back(std::move(buffer));
			}
		}

		const auto setupBufferImplementation = [&](const auto& self, const BufferLayoutReference& bufferLayout, void* buffer) -> void {
			GREM_MATCH(bufferLayout) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					const UniformBufferImplementation* const uniformBuffer = static_cast<const UniformBufferImplementation*>(buffer);
					if (anyOf(uniformBufferLayout.parameterDescriptions,
							[](const ParameterDescription& parameterDescription) -> bool { return !isTextureParameter(parameterDescription.type); })) {
						implementation.commands->push_back(RenderPassImplementation::CommandUseUniformBuffer{
							.uniformBlockBinding = uniformBlockBinding,
							.uniformBufferObjectHandle = uniformBuffer->uniformBufferObject.get(),
						});
						++uniformBlockBinding;
					}
					setupTextureBindings(implementation, textureUnit, uniformBufferLayout.parameterDescriptions, uniformBuffer->textures);
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					const StorageBufferImplementation* const storageBuffer = static_cast<const StorageBufferImplementation*>(buffer);
					implementation.commands->push_back(RenderPassImplementation::CommandUseStorageBuffer{
						.storageBuffer = storageBuffer,
						.storageBufferBinding = storageBufferBinding,
					});
					++storageBufferBinding;
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					const BufferSetImplementation* const bufferSet = static_cast<const BufferSetImplementation*>(buffer);
					GREM_ASSERT(bufferSetLayout.bufferLayouts.size() == bufferSet->buffers.size());
					for (size_t i = 0; i < bufferSetLayout.bufferLayouts.size(); ++i) {
						self(self, bufferSetLayout.bufferLayouts[i], bufferSet->buffers[i].get());
					}
					break;
				}
			}
		};

		const auto setupBuffer = [&](const BufferLayoutReference& bufferLayout) -> void {
			const auto it = lowerBound(bufferHandles, bufferLayout,
				[](const Pair<BufferLayoutReference, SharedPointer<void>>& a, const BufferLayoutReference& b) -> bool { return a.first < b; });
			if (it == bufferHandles.end() || it->first != bufferLayout) {
				GREM_MATCH(bufferLayout) {
					GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
						throw graphics::Error{formatString("RenderPass cannot enqueue draw commands because shader buffer \"{}\" was not provided.", uniformBufferLayout.name)};
					}
					GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
						throw graphics::Error{formatString("RenderPass cannot enqueue draw commands because shader buffer \"{}\" was not provided.", storageBufferLayout.name)};
					}
					GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
						throw graphics::Error{formatString("RenderPass cannot enqueue draw commands because shader buffer set \"{}\" was not provided.", bufferSetLayout.name)};
					}
				}
			}

			void* const buffer = it->second.get();
			setupBufferImplementation(setupBufferImplementation, bufferLayout, buffer);
		};

		for (const BufferLayoutReference& bufferLayout : shaderPipelineHandle->vertexShaderBufferLayouts) {
			setupBuffer(bufferLayout);
		}
		if (!shaderPipelineHandle->fragmentShaderBufferLayouts.empty()) {
			for (const BufferLayoutReference& bufferLayout : shaderPipelineHandle->fragmentShaderBufferLayouts.subspan(shaderPipelineHandle->vertexShaderBufferLayouts.size())) {
				setupBuffer(bufferLayout);
			}
		}

		GREM_ASSERT(shaderPipelineHandle->storageBufferTextureUnit == -1 || textureUnit == shaderPipelineHandle->storageBufferTextureUnit);
	}
}

void uploadImageToBoundTexture2D(Offset2D offset, Extent2D size, GLenum format, GLenum type, size_t pixelStride, const byte* data) {
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

[[nodiscard]] GLuint flushStorageBufferTexture(Device& device) {
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

void uploadInstancesToBoundInstanceBuffer(Span<const byte> data) {
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

void applyShaderPipelineOptions(const ShaderPipelineOptions& configuration) {
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

void enqueueInstanceRange(RenderPassImplementation& implementation, const MeshImplementation& mesh, const InstanceBufferImplementation* instanceBuffer, uint32_t instanceOffset,
	uint32_t instanceCount) {
	if (instanceCount == 0) {
		[[unlikely]];
		return;
	}

	if (instanceBuffer) {
		const size_t byteOffset = implementation.currentInstanceData.size();
		const size_t instancesByteOffset = static_cast<size_t>(instanceOffset) * static_cast<size_t>(implementation.currentInstanceStride);
		const size_t instancesSizeInBytes = static_cast<size_t>(instanceCount) * static_cast<size_t>(implementation.currentInstanceStride);
		GREM_ASSERT(instancesByteOffset + instancesSizeInBytes <= instanceBuffer->instanceData.size());
		implementation.currentInstanceData.resize(byteOffset + instancesSizeInBytes);
		memcpy(implementation.currentInstanceData.data() + byteOffset, instanceBuffer->instanceData.data() + instancesByteOffset, instancesSizeInBytes);
	}
	GREM_ASSERT(instanceBuffer || instanceOffset == 0);
	implementation.currentInstanceCount += instanceCount;

	const bool isIndexed = mesh.indexType.has_value();
	const uint32_t vertexCount = mesh.vertexCount;
	const uint32_t indexCount = (isIndexed) ? mesh.indexCount : vertexCount;
	implementation.statistics.totalDrawnVertexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(vertexCount);
	implementation.statistics.totalDrawnIndexCount += static_cast<size_t>(instanceCount) * static_cast<size_t>(indexCount);
	implementation.statistics.totalDrawnInstanceCount += static_cast<size_t>(instanceCount);
}

} // namespace

void RenderPassImplementation::invalidateContentsOfOldBuffersAvailableForReuse() noexcept {
	for (const UniformBufferImplementation* const uniformBuffer : usedUniformBuffers) {
		for (const UniformBufferImplementation* handle = uniformBuffer; handle->oldResource; handle = handle->oldResource.get()) {
			if (handle->oldResource.use_count() == 1) {
				handle->oldResource->invalidateContents();
			}
		}
	}
	for (const BufferSetImplementation* const bufferSet : usedBufferSets) {
		for (const BufferSetImplementation* handle = bufferSet; handle->oldResource; handle = handle->oldResource.get()) {
			if (handle->oldResource.use_count() == 1) {
				handle->oldResource->invalidateContents();
			}
		}
	}
}

void RenderPassImplementation::commitInstances() {
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

void RenderPassImplementation::render() {
	commitInstances();

	const bool isDefaultFramebufferTarget = renderTargets.colorTarget && renderTargets.colorTarget->texture->get()->type == TextureType::SWAPCHAIN;
	GLuint framebufferObjectHandle = 0;
	if (isDefaultFramebufferTarget) {
		Window& window = *renderTargets.colorTarget->texture->get()->object.get<Window*>();
		SDL_GL_MakeCurrent(static_cast<SDL_Window*>(const_cast<void*>(window.get())), static_cast<SDL_GLContext>(const_cast<void*>(window.getSurface())));
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
		// not. To work around this, we assume that the default framebuffer
		// is always sRGB, since it's what we requested on context creation.
		// Note that the ES profile doesn't seem to need this workaround for
		// some reason.
		//
		// Other references to the same driver bug found in the wild:
		// - 2014: https://stackoverflow.com/questions/25842211/opengl-srgb-framebuffer-oddity
		// - 2014, bumped in 2017 and 2018: https://forums.developer.nvidia.com/t/gl-framebuffer-srgb-functions-incorrectly/34889
		// - 2022: https://forums.developer.nvidia.com/t/glgetframebufferattachmentparameteriv-with-gl-framebuffer-attachment-color-encoding-returns-wrong-value/205092
		//
		// AMD's proprietary driver on Windows seems to have a similar issue
		// as well (the AMDgpu driver on Linux does not).
		if (isDefaultFramebufferTarget) {
			colorEncoding = GL_SRGB;
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
			glScissor(static_cast<GLint>(command.targetRegion.offset.x), static_cast<GLint>(command.targetRegion.offset.y), static_cast<GLsizei>(command.targetRegion.size.width),
				static_cast<GLsizei>(command.targetRegion.size.height));
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
					storageBufferTextureHandle = flushStorageBufferTexture(device);
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
			GREM_ASSERT((command.shaderPipelineOptions.depthBufferMode == DepthBufferMode::NONE || hasDepthAttachment) && "Cannot use depth test without a depth render target.");
			GREM_ASSERT(
				(command.shaderPipelineOptions.stencilBufferMode == StencilBufferMode::NONE || hasStencilAttachment) && "Cannot use stencil test without a stencil render target.");
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
				uploadInstancesToBoundInstanceBuffer(
					Span<const byte>{command.instanceData, static_cast<size_t>(command.instanceCount) * static_cast<size_t>(command.instanceStride)});
			}
			GREM_ASSERT(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			glDrawArraysInstanced(command.primitiveType, 0, static_cast<GLsizei>(command.vertexCount), static_cast<GLsizei>(command.instanceCount));
		},
		[&](const RenderPassImplementation::CommandDrawElementsInstanced& command) -> void { //
			if (command.instanceBufferObjectHandle != 0) {
				glBindBuffer(GL_ARRAY_BUFFER, command.instanceBufferObjectHandle);
				uploadInstancesToBoundInstanceBuffer(
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

RenderPass::RenderPass(Device& device, Span<const TextureSubresourceReference> renderTargets, const ClearMode& clearMode, Optional<Viewport> viewport)
	: implementation(acquireRenderPass(device, {}, renderTargets)) {
	implementation->renderTargets.clearMode = clearMode;
	implementation->renderTargets.resolveMode = DiscardIntermediateValues{};
	getRenderTargets(implementation->renderTargets, renderTargets);
	if (viewport) {
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = *viewport,
		});
		implementation->currentViewport = *viewport;
	}
}

RenderPass::RenderPass(Device& device, TextureSubresourceReference resolveTarget, const ResolveMode& resolveMode, Span<const TextureSubresourceReference> renderTargets,
	const ClearMode& clearMode, Optional<Viewport> viewport)
	: implementation(acquireRenderPass(device, resolveTarget, renderTargets)) {
	GREM_ASSERT(resolveTarget.texture && *resolveTarget.texture);
	implementation->renderTargets.resolveTarget = resolveTarget;
	implementation->renderTargets.clearMode = clearMode;
	implementation->renderTargets.resolveMode = resolveMode;
	getRenderTargets(implementation->renderTargets, renderTargets);
	if (viewport) {
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = *viewport,
		});
		implementation->currentViewport = *viewport;
	}
}

RenderPass::~RenderPass() {
	if (implementation.use_count() == 1) {
		implementation->reset();
	}
	try {
		implementation->device.get()->renderPassesForReuse.push_back(std::move(implementation));
	} catch (...) {
	}
}

RenderPass& RenderPass::setViewport(const Viewport& viewport) {
	if (viewport != implementation->currentViewport) {
		ensureExclusiveRenderPassAccess(implementation);

		implementation->commitInstances();
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = viewport,
		});
		implementation->currentViewport = viewport;
	}
	return *this;
}

RenderPass& RenderPass::setViewportScissor(const Optional<Region2D>& scissor) {
	if (!implementation->currentViewport) {
		setViewport(Viewport{.region{.size = getFramebufferSize()}});
	}

	if (scissor != implementation->currentViewport->scissor) {
		ensureExclusiveRenderPassAccess(implementation);

		implementation->commitInstances();
		implementation->commands->push_back(RenderPassImplementation::CommandSetScissor{
			.scissor = scissor,
		});
	}
	return *this;
}

const Viewport& RenderPass::getViewport() {
	if (!implementation->currentViewport) {
		setViewport(Viewport{.region{.size = getFramebufferSize()}});
	}
	return *implementation->currentViewport;
}

Extent2D RenderPass::getFramebufferSize() {
	if (implementation->renderTargets.colorTarget) {
		GREM_ASSERT(implementation->renderTargets.colorTarget->texture);
		return resource::Image::getMipLevelSize2D(implementation->renderTargets.colorTarget->texture->getSize2D(), implementation->renderTargets.colorTarget->subresource.mipLevel);
	}
	if (implementation->renderTargets.depthStencilTarget) {
		GREM_ASSERT(implementation->renderTargets.depthStencilTarget->texture);
		return resource::Image::getMipLevelSize2D(implementation->renderTargets.depthStencilTarget->texture->getSize2D(),
			implementation->renderTargets.depthStencilTarget->subresource.mipLevel);
	}
	throw graphics::Error{"RenderPass is missing a render targets."};
}

RenderPass& RenderPass::fill(const Region2D& targetRegion, const ClearValues& values) {
	ensureExclusiveRenderPassAccess(implementation);

	implementation->commitInstances();
	implementation->commands->push_back(RenderPassImplementation::CommandFill{
		.targetRegion = targetRegion,
		.aspectMask = TextureImplementation::getAspectBits(values.aspects),
		.color = values.color.toLinearRGBA(),
		.depth = values.depth,
		.stencil = values.stencil,
	});
	return *this;
}

RenderPass& RenderPass::drawShaded(SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle, SharedPointer<DrawCommandBufferImplementation> drawCommandBufferHandle,
	SharedPointer<InstanceBufferImplementation> instanceBufferHandle, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
	if (!implementation->currentViewport) {
		setViewport(Viewport{.region{.size = getFramebufferSize()}});
	}

	if (drawCommandBufferHandle->instanceRanges.empty()) {
		return *this;
	}

	ensureExclusiveRenderPassAccess(implementation);

	bool boundNewBufferHandles = areBufferHandlesCurrent(*implementation, bufferHandles);
	for (const DrawCommandBufferImplementation::InstanceRange& instanceRange : drawCommandBufferHandle->instanceRanges) {
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = (shaderPipelineOverrideHandle) ? shaderPipelineOverrideHandle : instanceRange.shaderPipelineHandle;
		setupInstanceContext(*implementation, boundNewBufferHandles, bufferHandles, std::move(shaderPipelineHandle), instanceRange.meshHandle);
		enqueueInstanceRange(*implementation, *instanceRange.meshHandle, instanceBufferHandle.get(), instanceRange.offset, instanceRange.count);
	}

	implementation->usedResources.push_back(std::move(drawCommandBufferHandle));
	if (instanceBufferHandle) {
		implementation->usedResources.push_back(std::move(instanceBufferHandle));
	}
	for (const Pair<BufferLayoutReference, SharedPointer<void>>& bufferHandle : bufferHandles) {
		implementation->usedResources.push_back(bufferHandle.second);
		GREM_MATCH(bufferHandle.first) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				implementation->usedUniformBuffers.push_back(static_cast<UniformBufferImplementation*>(bufferHandle.second.get()));
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) break;
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				implementation->usedBufferSets.push_back(static_cast<BufferSetImplementation*>(bufferHandle.second.get()));
				break;
			}
		}
	}
	return *this;
}

RenderPass& RenderPass::drawShadedUnordered(SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle,
	SharedPointer<UnorderedDrawCommandBufferImplementation> drawCommandBufferHandle, SharedPointer<InstanceBufferImplementation> instanceBufferHandle,
	Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
	if (!implementation->currentViewport) {
		setViewport(Viewport{.region{.size = getFramebufferSize()}});
	}

	if (drawCommandBufferHandle->instanceRanges.empty()) {
		return *this;
	}

	ensureExclusiveRenderPassAccess(implementation);

	bool boundNewBufferHandles = areBufferHandlesCurrent(*implementation, bufferHandles);
	for (const auto& [key, instanceRanges] : drawCommandBufferHandle->instanceRanges) {
		if (instanceRanges.empty()) {
			continue;
		}
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = (shaderPipelineOverrideHandle) ? shaderPipelineOverrideHandle : key.shaderPipelineHandle;
		setupInstanceContext(*implementation, boundNewBufferHandles, bufferHandles, std::move(shaderPipelineHandle), key.meshHandle);
		for (const UnorderedDrawCommandBufferImplementation::InstanceRange& instanceRange : instanceRanges) {
			enqueueInstanceRange(*implementation, *key.meshHandle, instanceBufferHandle.get(), instanceRange.offset, instanceRange.count);
		}
	}

	implementation->usedResources.push_back(std::move(drawCommandBufferHandle));
	if (instanceBufferHandle) {
		implementation->usedResources.push_back(std::move(instanceBufferHandle));
	}
	for (const Pair<BufferLayoutReference, SharedPointer<void>>& bufferHandle : bufferHandles) {
		implementation->usedResources.push_back(bufferHandle.second);
		GREM_MATCH(bufferHandle.first) {
			GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
				implementation->usedUniformBuffers.push_back(static_cast<UniformBufferImplementation*>(bufferHandle.second.get()));
				break;
			}
			GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) break;
			GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
				implementation->usedBufferSets.push_back(static_cast<BufferSetImplementation*>(bufferHandle.second.get()));
				break;
			}
		}
	}
	return *this;
}

RenderPass::Statistics RenderPass::getStatistics() const {
	RenderPass::Statistics result = implementation->statistics;
	if (implementation->currentInstanceCount > 0) {
		++result.totalDrawCallCount;
	}
	return result;
}

} // namespace grem::graphics
