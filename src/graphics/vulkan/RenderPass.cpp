// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
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

#include "DeviceImplementation.hpp"
#include "MeshImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "ShaderImplementation.hpp"
#include "TextureImplementation.hpp"
#include "buffer_implementations.hpp"

namespace grem::graphics {

namespace {

[[nodiscard]] VkViewport translateViewport(const Viewport& viewport, Extent2D framebufferSize) {
	return VkViewport{
		.x = static_cast<float>(viewport.region.offset.x),
		.y = static_cast<float>(static_cast<int32_t>(framebufferSize.height) - viewport.region.offset.y),
		.width = static_cast<float>(viewport.region.size.width),
		.height = -static_cast<float>(viewport.region.size.height),
		.minDepth = viewport.minDepth,
		.maxDepth = viewport.maxDepth,
	};
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
		TextureImplementation& targetTexture = *renderTarget.texture->get();
		GREM_ASSERT(output.sampleCount == VK_SAMPLE_COUNT_1_BIT || output.sampleCount == targetTexture.sampleCount);
		output.sampleCount = targetTexture.sampleCount;
		if (targetTexture.type == TextureType::SWAPCHAIN) {
			GREM_ASSERT(!output.colorTarget);
			GREM_ASSERT(!output.depthStencilTarget);
			output.colorTarget = renderTarget;
			output.depthStencilTarget = renderTarget;
			TextureImplementation::SwapchainImplementation& swapchainImplementation = targetTexture.object.get<TextureImplementation::SwapchainImplementation>();
			if (!output.resolveTarget && swapchainImplementation.multisampledColorBuffer) {
				output.sampleCount = swapchainImplementation.multisampledColorBuffer.get()->sampleCount;
				output.colorTarget = swapchainImplementation.multisampledColorBuffer;
				output.resolveTarget = renderTarget;
			}
		} else {
			const VkImageAspectFlags aspectMask =
				TextureImplementation::translateTextureAspects(renderTarget.subresource.aspects) & TextureImplementation::getAspectMask(targetTexture.format);
			if ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
				if (output.colorTarget) {
					throw graphics::Error{"Cannot render to multiple color targets."};
				}
				if (output.resolveTarget && targetTexture.maxMultisampleCount <= 1) {
					throw graphics::Error{"Cannot resolve from a non-multisampled texture."};
				}
				output.colorTarget = renderTarget;
			}
			if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
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

void setupInstanceContext(RenderPassImplementation& implementation, bool& bufferRebindRequired, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles,
	SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, VertexAttributeMask activeVertexAttributes, VkDescriptorSet drawCommandBufferDescriptorSet,
	VkDescriptorSet instanceBufferDescriptorSet) {
	const ShaderPipelineImplementation& shaderPipeline = *shaderPipelineHandle;
	const std::type_index meshTypeIndex = shaderPipeline.meshTypeIndex;
	const VertexShaderImplementation& vertexShader = *shaderPipeline.vertexShaderHandle;
	const FragmentShaderImplementation& fragmentShader = *shaderPipeline.fragmentShaderHandle;
	const DeviceImplementation::MeshContext& meshContext = implementation.device.get()->getMeshContext(meshTypeIndex);
	const DeviceImplementation::Pipeline& pipeline = implementation.device.get()->getPipeline(
		DeviceImplementation::PipelineKey{
			.shaderPipelineHandle = std::move(shaderPipelineHandle),
			.renderPassContextKey = implementation.renderTargets.getRenderPassContextKey(),
		},
		meshContext);

	if (&shaderPipeline != implementation.currentShaderPipeline) {
		implementation.commands->push_back(RenderPassImplementation::CommandUsePipeline{
			.pipeline = pipeline.pipeline.get(),
			.pipelineLayout = pipeline.pipelineLayout.get(),
		});
		implementation.currentShaderPipeline = &shaderPipeline;
		bufferRebindRequired = true;
	}

	if (meshTypeIndex != implementation.currentMeshTypeIndex || activeVertexAttributes != implementation.currentActiveVertexAttributes) {
		implementation.commands->push_back(RenderPassImplementation::CommandUseMesh{
			.meshTypeIndex = meshTypeIndex,
			.activeVertexAttributes = activeVertexAttributes,
			.indexType = (meshTypeIndex == implementation.currentMeshTypeIndex) ? Optional<MeshIndexType>{} : vertexShader.indexType,
		});
		implementation.currentMeshTypeIndex = meshTypeIndex;
		implementation.currentActiveVertexAttributes = activeVertexAttributes;
		bufferRebindRequired = true;
	}

	if (bufferRebindRequired || drawCommandBufferDescriptorSet != implementation.currentDrawCommandBufferDescriptorSet ||
		instanceBufferDescriptorSet != implementation.currentInstanceBufferDescriptorSet) {
		SmallBuffer<VkDescriptorSet, 8> descriptorSets{};
		if (!vertexShader.instanceAttributeDescriptions.empty() || !vertexShader.parameterDescriptions.empty()) {
			GREM_ASSERT(drawCommandBufferDescriptorSet);
			descriptorSets.push_back(drawCommandBufferDescriptorSet);
		}

		GREM_ASSERT(static_cast<bool>(instanceBufferDescriptorSet) == !vertexShader.instanceAttributeDescriptions.empty());
		if (!vertexShader.instanceAttributeDescriptions.empty()) {
			GREM_ASSERT(instanceBufferDescriptorSet);
			descriptorSets.push_back(instanceBufferDescriptorSet);
		}

		if (!vertexShader.parameterDescriptions.empty()) {
			descriptorSets.push_back(meshContext.parametersDescriptorSet);
		}

		const auto setupBuffer = [&](const BufferLayoutReference& bufferLayout) -> void {
			const auto it = std::lower_bound(bufferHandles.begin(), bufferHandles.end(), bufferLayout,
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
			GREM_MATCH(bufferLayout) {
				GREM_CASE(const UniformBufferLayoutReference& uniformBufferLayout) {
					descriptorSets.push_back(static_cast<const UniformBufferImplementation*>(buffer)->getDescriptorSet());
					break;
				}
				GREM_CASE(const StorageBufferLayoutReference& storageBufferLayout) {
					descriptorSets.push_back(static_cast<const StorageBufferImplementation*>(buffer)->getDescriptorSet());
					break;
				}
				GREM_CASE(const BufferSetLayoutReference& bufferSetLayout) {
					descriptorSets.push_back(static_cast<const BufferSetImplementation*>(buffer)->getDescriptorSet());
					break;
				}
			}
		};

		for (const BufferLayoutReference& bufferLayout : vertexShader.bufferLayouts) {
			setupBuffer(bufferLayout);
		}
		if (!fragmentShader.bufferLayouts.empty()) {
			for (const BufferLayoutReference& bufferLayout : fragmentShader.bufferLayouts.subspan(vertexShader.bufferLayouts.size())) {
				setupBuffer(bufferLayout);
			}
		}

		implementation.commands->append(Span{descriptorSets});

		implementation.currentDrawCommandBufferDescriptorSet = drawCommandBufferDescriptorSet;
		implementation.currentInstanceBufferDescriptorSet = instanceBufferDescriptorSet;
		bufferRebindRequired = false;
	}
}

} // namespace

void RenderPassImplementation::invalidateContentsOfOldBuffersAvailableForReuse() noexcept {
	for (UniformBufferImplementation* const uniformBuffer : usedUniformBuffers) {
		for (UniformBufferImplementation* handle = uniformBuffer; handle->oldResource; handle = handle->oldResource.get()) {
			if (handle->oldResource.use_count() == 1) {
				handle->oldResource->invalidateContents();
			}
		}
	}
	for (BufferSetImplementation* const bufferSet : usedBufferSets) {
		for (BufferSetImplementation* handle = bufferSet; handle->oldResource; handle = handle->oldResource.get()) {
			if (handle->oldResource.use_count() == 1) {
				handle->oldResource->invalidateContents();
			}
		}
	}
}

RenderPass::RenderPass(Device& device, Span<const TextureSubresourceReference> renderTargets, const ClearMode& clearMode, Optional<Viewport> viewport)
	: implementation(acquireRenderPass(device, {}, renderTargets)) {
	implementation->renderTargets.clearMode = clearMode;
	implementation->renderTargets.resolveMode = DiscardIntermediateValues{};
	getRenderTargets(implementation->renderTargets, renderTargets);
	if (viewport) {
		const Extent2D framebufferSize = getFramebufferSize();
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = translateViewport(*viewport, framebufferSize),
		});
		implementation->commands->push_back(RenderPassImplementation::CommandSetScissor{
			.scissor = TextureImplementation::translateRegion(viewport->scissor.value_or(viewport->region), framebufferSize),
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
	GREM_ASSERT(implementation->renderTargets.colorTarget);
	if (viewport) {
		const Extent2D framebufferSize = getFramebufferSize();
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = translateViewport(*viewport, framebufferSize),
		});
		implementation->commands->push_back(RenderPassImplementation::CommandSetScissor{
			.scissor = TextureImplementation::translateRegion(viewport->scissor.value_or(viewport->region), framebufferSize),
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

		const Extent2D framebufferSize = getFramebufferSize();
		implementation->commands->push_back(RenderPassImplementation::CommandSetViewport{
			.viewport = translateViewport(viewport, framebufferSize),
		});
		implementation->commands->push_back(RenderPassImplementation::CommandSetScissor{
			.scissor = TextureImplementation::translateRegion(viewport.scissor.value_or(viewport.region), framebufferSize),
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

		const Extent2D framebufferSize = getFramebufferSize();
		implementation->commands->push_back(RenderPassImplementation::CommandSetScissor{
			.scissor = TextureImplementation::translateRegion(scissor.value_or(implementation->currentViewport->region), framebufferSize),
		});
		implementation->currentViewport->scissor = scissor;
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
	throw graphics::Error{"Render pass has no render targets."};
}

RenderPass& RenderPass::fill(const Region2D& targetRegion, const ClearValues& values) {
	if (!values.aspects.containsAnyOf(TextureAspects::COLOR_DEPTH_STENCIL)) {
		return *this;
	}

	ensureExclusiveRenderPassAccess(implementation);

	const Extent2D framebufferSize = getFramebufferSize();
	implementation->commands->push_back(RenderPassImplementation::CommandFill{
		.targetRegion = TextureImplementation::translateRegion(targetRegion, framebufferSize),
		.values = values,
	});
	return *this;
}

RenderPass& RenderPass::drawShaded(SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle, SharedPointer<DrawCommandBufferImplementation> drawCommandBufferHandle,
	SharedPointer<InstanceBufferImplementation> instanceBufferHandle, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
	if (!implementation->currentViewport) {
		setViewport(Viewport{.region{.size = getFramebufferSize()}});
	}

	if (drawCommandBufferHandle->getInstanceRanges().empty()) {
		return *this;
	}

	ensureExclusiveRenderPassAccess(implementation);

	bool bufferRebindRequired = true;
	for (const DrawCommandBufferImplementation::InstanceRange& instanceRange : drawCommandBufferHandle->getInstanceRanges()) {
		GREM_ASSERT(instanceRange.count > 0);

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = (shaderPipelineOverrideHandle) ? shaderPipelineOverrideHandle : instanceRange.shaderPipelineHandle;
		const MeshImplementation& mesh = *instanceRange.meshHandle;
		setupInstanceContext(*implementation, bufferRebindRequired, bufferHandles, std::move(shaderPipelineHandle), mesh.activeVertexAttributes,
			drawCommandBufferHandle->getDescriptorSet(), (instanceBufferHandle) ? instanceBufferHandle->getDescriptorSet() : VK_NULL_HANDLE);

		if (instanceRange.meshHandle->indexType) {
			implementation->commands->push_back(RenderPassImplementation::CommandDrawIndexed{
				.indexCount = mesh.indexRange.size(),
				.instanceCount = instanceRange.count,
				.firstIndex = mesh.indexRange.begin,
				.vertexOffset = static_cast<int32_t>(mesh.vertexRange.begin),
				.firstInstance = instanceRange.drawCommandOffset,
			});
		} else {
			implementation->commands->push_back(RenderPassImplementation::CommandDraw{
				.vertexCount = mesh.vertexCount,
				.instanceCount = instanceRange.count,
				.firstVertex = mesh.vertexRange.begin,
				.firstInstance = instanceRange.drawCommandOffset,
			});
		}
	}
	implementation->statistics += drawCommandBufferHandle->getStatistics();

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

	if (drawCommandBufferHandle->getInstanceRanges().empty()) {
		return *this;
	}

	ensureExclusiveRenderPassAccess(implementation);

	for (const auto& [key, instanceRanges] : drawCommandBufferHandle->getInstanceRanges()) {
		const detail::ShaderBuffer& indirectBuffer = instanceRanges.getIndirectBuffer();
		if (indirectBuffer.empty()) {
			continue;
		}

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = (shaderPipelineOverrideHandle) ? shaderPipelineOverrideHandle : key.shaderPipelineHandle;
		const bool isIndexed = shaderPipelineHandle->vertexShaderHandle->indexType.has_value();

		bool bufferRebindRequired = true;
		setupInstanceContext(*implementation, bufferRebindRequired, bufferHandles, std::move(shaderPipelineHandle), key.activeVertexAttributes, instanceRanges.getDescriptorSet(),
			(instanceBufferHandle) ? instanceBufferHandle->getDescriptorSet() : VK_NULL_HANDLE);

		if (isIndexed) {
			implementation->commands->push_back(RenderPassImplementation::CommandDrawIndexedIndirect{
				.buffer = indirectBuffer.get(),
				.drawCount = static_cast<uint32_t>(indirectBuffer.size() / sizeof(VkDrawIndexedIndirectCommand)),
				.stride = sizeof(VkDrawIndexedIndirectCommand),
			});
		} else {
			implementation->commands->push_back(RenderPassImplementation::CommandDrawIndirect{
				.buffer = indirectBuffer.get(),
				.drawCount = static_cast<uint32_t>(indirectBuffer.size() / sizeof(VkDrawIndirectCommand)),
				.stride = sizeof(VkDrawIndirectCommand),
			});
		}
	}
	implementation->statistics += drawCommandBufferHandle->getStatistics();

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
	return implementation->statistics;
}

} // namespace grem::graphics
