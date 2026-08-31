// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/resource/Image.hpp>

#include "MeshImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "ShaderImplementation.hpp"
#include "TextureImplementation.hpp"
#include "buffer_implementations.hpp"

namespace grem::graphics {

namespace {

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
	Device& device = implementation->device;
	try {
		device.get()->renderPassesForReuse.push_back(std::move(implementation));
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
