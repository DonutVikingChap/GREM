// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_RENDER_PASS_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_VULKAN_RENDER_PASS_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/LinearBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include "DeviceImplementation.hpp"
#include "TextureImplementation.hpp"
#include "TextureResources.hpp"
#include "vulkan.hpp"

#include <typeindex> // std::type_index

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

struct RenderPassImplementation {
	struct RenderTargets {
		Optional<TextureSubresourceReference> resolveTarget{};
		Optional<TextureSubresourceReference> colorTarget{};
		Optional<TextureSubresourceReference> depthStencilTarget{};
		ClearMode clearMode{};
		ResolveMode resolveMode{};
		VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

		[[nodiscard]] DeviceImplementation::RenderPassContextKey getRenderPassContextKey() const {
			DeviceImplementation::RenderPassContextFlags flags{};
			VkFormat colorFormat = VK_FORMAT_UNDEFINED;
			VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;

			if (resolveTarget) {
				GREM_ASSERT(resolveTarget->texture && *resolveTarget->texture);
				const TextureImplementation& resolveTexture = *resolveTarget->texture->get();
				flags |= DeviceImplementation::RENDER_PASS_HAS_RESOLVE_TARGET;
				if (resolveTexture.type == TextureType::SWAPCHAIN) {
					flags |= DeviceImplementation::RENDER_PASS_RESOLVE_TARGET_IS_SWAPCHAIN;
				}
				if (resolveTexture.samplerOptions) {
					flags |= DeviceImplementation::RENDER_PASS_RESOLVE_TARGET_IS_SAMPLED;
				}
			}

			if (colorTarget) {
				GREM_ASSERT(colorTarget->texture && *colorTarget->texture);
				const TextureImplementation& colorTexture = *colorTarget->texture->get();
				if (colorTexture.samplerOptions) {
					flags |= DeviceImplementation::RENDER_PASS_COLOR_TARGET_IS_SAMPLED;
				}
				if (colorTexture.type == TextureType::SWAPCHAIN) {
					flags |= DeviceImplementation::RENDER_PASS_RENDER_TARGET_IS_SWAPCHAIN;
					colorFormat = colorTexture.object.get<TextureImplementation::SwapchainImplementation>().device->get()->physicalDevice.surfaceFormat.format;
				} else {
					colorFormat = colorTexture.format;
				}
				if (const StoreIntermediateValues* const storeIntermediateValues = resolveMode.get_if<StoreIntermediateValues>()) {
					if (storeIntermediateValues->aspects.contains(TextureAspect::COLOR)) {
						flags |= DeviceImplementation::RENDER_PASS_STORE_INTERMEDIATE_COLOR;
					}
				}
			}

			if (depthStencilTarget) {
				GREM_ASSERT(depthStencilTarget->texture && *depthStencilTarget->texture);
				const TextureImplementation& depthStencilTexture = *depthStencilTarget->texture->get();
				if (depthStencilTexture.samplerOptions) {
					flags |= DeviceImplementation::RENDER_PASS_DEPTH_STENCIL_TARGET_IS_SAMPLED;
				}
				if (depthStencilTexture.type == TextureType::SWAPCHAIN) {
					flags |= DeviceImplementation::RENDER_PASS_RENDER_TARGET_IS_SWAPCHAIN;
					depthStencilFormat = depthStencilTexture.object.get<TextureImplementation::SwapchainImplementation>().device->get()->physicalDevice.depthStencilFormat;
				} else {
					depthStencilFormat = depthStencilTexture.format;
				}
				if (const StoreIntermediateValues* const storeIntermediateValues = resolveMode.get_if<StoreIntermediateValues>()) {
					if (storeIntermediateValues->aspects.contains(TextureAspect::DEPTH)) {
						flags |= DeviceImplementation::RENDER_PASS_STORE_INTERMEDIATE_DEPTH;
					}
					if (storeIntermediateValues->aspects.contains(TextureAspect::STENCIL)) {
						flags |= DeviceImplementation::RENDER_PASS_STORE_INTERMEDIATE_STENCIL;
					}
				}
			}

			const TextureAspects targetAspectsToClear = match(clearMode)(                             //
				[](const RetainValues&) -> TextureAspects { return {}; },                             //
				[](const ClearValues& clearValues) -> TextureAspects { return clearValues.aspects; }, //
				[](const UndefinedClearValues& undefinedClearValues) -> TextureAspects { return undefinedClearValues.aspects; });
			if (targetAspectsToClear.contains(TextureAspect::COLOR)) {
				flags |= DeviceImplementation::RENDER_PASS_CLEAR_COLOR;
			}
			if (targetAspectsToClear.contains(TextureAspect::DEPTH)) {
				flags |= DeviceImplementation::RENDER_PASS_CLEAR_DEPTH;
			}
			if (targetAspectsToClear.contains(TextureAspect::STENCIL)) {
				flags |= DeviceImplementation::RENDER_PASS_CLEAR_STENCIL;
			}
			if (clearMode.is<UndefinedClearValues>()) {
				flags |= DeviceImplementation::RENDER_PASS_CLEAR_UNDEFINED_VALUES;
			}

			return DeviceImplementation::RenderPassContextKey{
				.sampleCount = static_cast<uint8_t>(sampleCount),
				.flags = flags,
				.colorFormat = colorFormat,
				.depthStencilFormat = depthStencilFormat,
			};
		}

		[[nodiscard]] DeviceImplementation::RenderPassContext::FramebufferContextKey acquireFramebufferContextKey() const {
			DeviceImplementation::RenderPassContext::FramebufferContextKey result{};

			if (colorTarget) {
				GREM_ASSERT(colorTarget->texture && *colorTarget->texture);
				if (colorTarget->texture->get()->type == TextureType::SWAPCHAIN) {
					GREM_ASSERT(colorTarget->subresource.layer == 0);
					GREM_ASSERT(colorTarget->subresource.mipLevel == 0);
					TextureImplementation::SwapchainImplementation& swapchainImplementation =
						colorTarget->texture->get()->object.get<TextureImplementation::SwapchainImplementation>();
					result.colorTargetHandle = swapchainImplementation.device->get()->acquireSwapchainImage(*colorTarget->texture->get()).lock();
				} else {
					const bool isWholeTexture = colorTarget->texture->getDepth() == 1 && colorTarget->texture->getMipLevelCount() == 1;
					const VkImageAspectFlags fullAspectMask = TextureImplementation::getAspectMask(colorTarget->texture->get()->format);
					const VkImageAspectFlags uninitializedTargetAspectMask = TextureImplementation::translateTextureAspects( //
						match(clearMode)(                                                                                    //
							[](const RetainValues&) -> TextureAspects { return {}; },                                        //
							[](const ClearValues& clearValues) -> TextureAspects { return clearValues.aspects; },            //
							[](const UndefinedClearValues& undefinedClearValues) -> TextureAspects { return undefinedClearValues.aspects; }));
					DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*colorTarget->texture,
						isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
					result.colorTargetHandle = colorTarget->texture->lock();
					result.colorTargetLayer = colorTarget->subresource.layer;
					result.colorTargetMipLevel = colorTarget->subresource.mipLevel;
				}
			}

			if (depthStencilTarget) {
				GREM_ASSERT(depthStencilTarget->texture && *depthStencilTarget->texture);
				if (depthStencilTarget->texture->get()->type == TextureType::SWAPCHAIN) {
					GREM_ASSERT(depthStencilTarget->subresource.layer == 0);
					GREM_ASSERT(depthStencilTarget->subresource.mipLevel == 0);
					TextureImplementation::SwapchainImplementation& swapchainImplementation =
						depthStencilTarget->texture->get()->object.get<TextureImplementation::SwapchainImplementation>();
					result.depthStencilTargetHandle = swapchainImplementation.depthStencilBuffer.lock();
					result.depthStencilTargetAspectMask =
						TextureImplementation::translateTextureAspects(depthStencilTarget->subresource.aspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
				} else {
					const bool isWholeTexture = depthStencilTarget->texture->getDepth() == 1 && depthStencilTarget->texture->getMipLevelCount() == 1;
					const VkImageAspectFlags fullAspectMask = TextureImplementation::getAspectMask(depthStencilTarget->texture->get()->format);
					const VkImageAspectFlags depthStencilAspectMask = TextureImplementation::translateTextureAspects(depthStencilTarget->subresource.aspects) & fullAspectMask &
					                                                  (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
					const VkImageAspectFlags uninitializedTargetAspectMask = TextureImplementation::translateTextureAspects( //
						match(clearMode)(                                                                                    //
							[](const RetainValues&) -> TextureAspects { return {}; },                                        //
							[](const ClearValues& clearValues) -> TextureAspects { return clearValues.aspects; },
							[](const UndefinedClearValues& undefinedClearValues) -> TextureAspects { return undefinedClearValues.aspects; }));
					DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*depthStencilTarget->texture,
						isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
					result.depthStencilTargetHandle = depthStencilTarget->texture->lock();
					result.depthStencilTargetAspectMask = depthStencilAspectMask;
					result.depthStencilTargetLayer = depthStencilTarget->subresource.layer;
					result.depthStencilTargetMipLevel = depthStencilTarget->subresource.mipLevel;
				}
			}

			if (resolveTarget) {
				GREM_ASSERT(resolveTarget->texture && *resolveTarget->texture);
				if (resolveTarget->texture->get()->type == TextureType::SWAPCHAIN) {
					GREM_ASSERT(resolveTarget->subresource.layer == 0);
					GREM_ASSERT(resolveTarget->subresource.mipLevel == 0);
					TextureImplementation::SwapchainImplementation& swapchainImplementation =
						resolveTarget->texture->get()->object.get<TextureImplementation::SwapchainImplementation>();
					result.resolveTargetHandle = swapchainImplementation.device->get()->acquireSwapchainImage(*resolveTarget->texture->get()).lock();
				} else {
					const bool isWholeTexture = resolveTarget->texture->getDepth() == 1 && resolveTarget->texture->getMipLevelCount() == 1;
					const VkImageAspectFlags fullAspectMask = TextureImplementation::getAspectMask(resolveTarget->texture->get()->format);
					const VkImageAspectFlags uninitializedTargetAspectMask = TextureImplementation::translateTextureAspects( //
						match(clearMode)(                                                                                    //
							[](const RetainValues&) -> TextureAspects { return {}; },                                        //
							[](const ClearValues& clearValues) -> TextureAspects { return clearValues.aspects; },            //
							[](const UndefinedClearValues& undefinedClearValues) -> TextureAspects { return undefinedClearValues.aspects; }));
					DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*resolveTarget->texture,
						isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
					result.resolveTargetHandle = resolveTarget->texture->lock();
					result.resolveTargetLayer = resolveTarget->subresource.layer;
					result.resolveTargetMipLevel = resolveTarget->subresource.mipLevel;
				}
			}

			return result;
		}
	};

	[[nodiscard]] static VkIndexType translateMeshIndexType(MeshIndexType indexType) noexcept {
		switch (indexType) {
			case MeshIndexType::U16: return VK_INDEX_TYPE_UINT16;
			case MeshIndexType::U32: return VK_INDEX_TYPE_UINT32;
		}
		return {};
	}

	struct CommandSetViewport {
		VkViewport viewport;
	};

	struct CommandSetScissor {
		VkRect2D scissor;
	};

	struct CommandFill {
		VkRect2D targetRegion;
		ClearValues values;
	};

	struct CommandUsePipeline {
		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
	};

	struct CommandUseMesh {
		std::type_index meshTypeIndex;
		VertexAttributeMask activeVertexAttributes;
		Optional<MeshIndexType> indexType;
	};

	struct CommandDraw {
		uint32_t vertexCount;
		uint32_t instanceCount;
		uint32_t firstVertex;
		uint32_t firstInstance;
	};

	struct CommandDrawIndexed {
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		int32_t vertexOffset;
		uint32_t firstInstance;
	};

	struct CommandDrawIndirect {
		VkBuffer buffer;
		uint32_t drawCount;
		uint32_t stride;
	};

	struct CommandDrawIndexedIndirect {
		VkBuffer buffer;
		uint32_t drawCount;
		uint32_t stride;
	};

	using Commands = LinearBuffer< //
		CommandSetViewport,        //
		CommandSetScissor,         //
		CommandFill,               //
		CommandUsePipeline,        //
		CommandUseMesh,            //
		VkDescriptorSet[],         //
		CommandDraw,               //
		CommandDrawIndexed,        //
		CommandDrawIndirect,       //
		CommandDrawIndexedIndirect>;

	Device& device;
	RenderTargets renderTargets{};
	RenderPass::Statistics statistics{};
	Optional<Commands> commands{};
	ArrayList<SharedPointer<void>> usedResources{};
	ArrayList<UniformBufferImplementation*> usedUniformBuffers{};
	ArrayList<BufferSetImplementation*> usedBufferSets{};
	std::type_index currentMeshTypeIndex = typeid(void);
	VertexAttributeMask currentActiveVertexAttributes{};
	const ShaderPipelineImplementation* currentShaderPipeline = nullptr;
	VkDescriptorSet currentDrawCommandBufferDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet currentInstanceBufferDescriptorSet = VK_NULL_HANDLE;
	Optional<Viewport> currentViewport{};
	Arena<3056> commandArena{};

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
			[&](const Span<const VkDescriptorSet> descriptorSets) -> void { commands->append(descriptorSets); },
			[&](const auto& command) -> void { commands->push_back(command); },
		});

		usedResources = other.usedResources;
		usedUniformBuffers = other.usedUniformBuffers;
		usedBufferSets = other.usedBufferSets;
		currentMeshTypeIndex = other.currentMeshTypeIndex;
		currentActiveVertexAttributes = other.currentActiveVertexAttributes;
		currentShaderPipeline = other.currentShaderPipeline;
		currentDrawCommandBufferDescriptorSet = other.currentDrawCommandBufferDescriptorSet;
		currentInstanceBufferDescriptorSet = other.currentInstanceBufferDescriptorSet;
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
		currentMeshTypeIndex = typeid(void);
		currentActiveVertexAttributes = {};
		currentShaderPipeline = nullptr;
		currentDrawCommandBufferDescriptorSet = VK_NULL_HANDLE;
		currentInstanceBufferDescriptorSet = VK_NULL_HANDLE;
		currentViewport.reset();
	}
};

} // namespace grem::graphics

#endif
