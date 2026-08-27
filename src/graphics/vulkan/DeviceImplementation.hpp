// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_DEVICE_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_VULKAN_DEVICE_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/BitArray.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/RangeAllocator.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/shaders.hpp>

#include "ResourceBuffer.hpp"
#include "StagingBuffer.hpp"
#include "TextureResources.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <typeindex> // std::type_index

namespace grem::graphics {

class Window; // Forward declaration, to avoid including Window.hpp.

struct DeviceImplementation {
	using GraphicsQueueSubmissionGenerationIndex = uint64_t;

	static constexpr GraphicsQueueSubmissionGenerationIndex NOT_IN_USE = Limits<GraphicsQueueSubmissionGenerationIndex>::MAX;

	struct PhysicalDevice {
		VkPhysicalDevice handle;
		Array<uint8_t, 16> uuid;
		FeatureSupport supportedFeatures;
		uint32_t graphicsQueueFamilyIndex;
		uint32_t presentQueueFamilyIndex;
		VkSurfaceFormatKHR surfaceFormat;
		VkFormat depthStencilFormat;
	};

	enum class ShaderType : uint32_t { // NOLINT(performance-enum-size)
		SPIRV_VERTEX = 0,
		SPIRV_FRAGMENT = 1,
	};

	enum class ShaderCompressionMode : uint8_t {
		UNCOMPRESSED = 0,
		ZSTANDARD = 1,
	};

	struct ShaderKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const ShaderKey& key) const {
				return getHash(key.type, key.sourceSizeInBytes, key.sourceCRC32);
			}
		};

		ShaderType type;
		uint64_t sourceSizeInBytes;
		CRC32 sourceCRC32;

		[[nodiscard]] bool operator==(const ShaderKey&) const = default;
	};

	struct Shader {
		uint32_t codeUncompressedSizeInU32s = 0;
		ShaderCompressionMode compressionMode = ShaderCompressionMode::UNCOMPRESSED;
		Allocation<byte> compressedCode{};
		detail::VulkanShaderModule shaderModule{};
	};

	using ShaderCache = HashMap<ShaderKey, Shader, ShaderKey::Hash>;

	using RenderPassContextFlags = uint16_t;
	enum RenderPassContextFlag : RenderPassContextFlags {
		RENDER_PASS_HAS_RESOLVE_TARGET = 1 << 0,
		RENDER_PASS_RESOLVE_TARGET_IS_SAMPLED = 1 << 1,
		RENDER_PASS_COLOR_TARGET_IS_SAMPLED = 1 << 2,
		RENDER_PASS_DEPTH_STENCIL_TARGET_IS_SAMPLED = 1 << 3,
		RENDER_PASS_RESOLVE_TARGET_IS_SWAPCHAIN = 1 << 4,
		RENDER_PASS_RENDER_TARGET_IS_SWAPCHAIN = 1 << 5,
		RENDER_PASS_CLEAR_COLOR = 1 << 6,
		RENDER_PASS_CLEAR_DEPTH = 1 << 7,
		RENDER_PASS_CLEAR_STENCIL = 1 << 8,
		RENDER_PASS_CLEAR_UNDEFINED_VALUES = 1 << 9,
		RENDER_PASS_STORE_INTERMEDIATE_COLOR = 1 << 10,
		RENDER_PASS_STORE_INTERMEDIATE_DEPTH = 1 << 11,
		RENDER_PASS_STORE_INTERMEDIATE_STENCIL = 1 << 12,
	};

	struct RenderPassContextKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const RenderPassContextKey& key) const {
				return getHash(key.sampleCount, key.flags, key.colorFormat, key.depthStencilFormat);
			}
		};

		uint8_t sampleCount;
		RenderPassContextFlags flags;
		VkFormat colorFormat;
		VkFormat depthStencilFormat;

		[[nodiscard]] bool operator==(const RenderPassContextKey&) const = default;
	};

	struct RenderPassContext {
		struct FramebufferContextKey {
			struct Hash {
				[[nodiscard]] size_t operator()(const FramebufferContextKey& key) const {
					return getHash(key.colorTargetHandle, key.colorTargetLayer, key.colorTargetMipLevel, key.depthStencilTargetHandle, key.depthStencilTargetLayer,
						key.depthStencilTargetMipLevel, key.resolveTargetHandle, key.resolveTargetLayer, key.resolveTargetMipLevel);
				}
			};

			WeakPointer<TextureImplementation> colorTargetHandle;
			WeakPointer<TextureImplementation> depthStencilTargetHandle;
			WeakPointer<TextureImplementation> resolveTargetHandle;
			VkImageAspectFlags depthStencilTargetAspectMask;
			uint32_t colorTargetLayer;
			uint32_t depthStencilTargetLayer;
			uint32_t resolveTargetLayer;
			uint32_t colorTargetMipLevel;
			uint32_t depthStencilTargetMipLevel;
			uint32_t resolveTargetMipLevel;

			[[nodiscard]] bool operator==(const FramebufferContextKey&) const = default;

			[[nodiscard]] bool isExpired() const {
				return (colorTargetHandle && colorTargetHandle.expired()) || (depthStencilTargetHandle && depthStencilTargetHandle.expired()) ||
				       (resolveTargetHandle && resolveTargetHandle.expired());
			}

			[[nodiscard]] bool isReusable() const {
				return (!colorTargetHandle || colorTargetHandle.use_count() == 1) && (!depthStencilTargetHandle || depthStencilTargetHandle.use_count() == 1) &&
				       (!resolveTargetHandle || resolveTargetHandle.use_count() == 1);
			}
		};

		struct FramebufferContext {
			detail::VulkanImageView resolveTargetImageView{};
			detail::VulkanImageView colorTargetImageView{};
			detail::VulkanImageView depthStencilTargetImageView{};
			detail::VulkanFramebuffer framebuffer{};
			Extent2D size{};
			GraphicsQueueSubmissionGenerationIndex latestGraphicsQueueSubmissionUsingThisResource = NOT_IN_USE;

			FramebufferContext(DeviceImplementation& device, const FramebufferContextKey& key, const RenderPassContext& renderPassContext);
		};

		VkImageLayout initialColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout finalColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout initialDepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout finalDepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout initialResolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout finalResolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		detail::VulkanRenderPass renderPass{};
		HashMap<FramebufferContextKey, FramebufferContext, FramebufferContextKey::Hash> framebufferContextMap{};

		RenderPassContext(DeviceImplementation& device, const RenderPassContextKey& key);
	};

	struct MeshContext {
		struct VertexBuffers {
			SmallArrayList<detail::ResourceBuffer, 8> vertexBuffers{};
			SmallBuffer<VkBuffer, 8> vertexBufferHandles{};
			SmallBuffer<VkDeviceSize, 8> vertexBufferOffsets{};
			size_t firstActiveVertexAttributeIndex = 0;
			size_t largestInactiveVertexAttributeStride = 0;
			RangeAllocator<uint32_t> vertexRangeAllocator{};
			HashMap<uint32_t, size_t> vertexRangeReferenceCounts{};

			VertexBuffers(VmaAllocator allocator, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions);

			void flush(VertexAttributeMask activeVertexAttributes);
		};

		HashMap<VertexAttributeMask, VertexBuffers> vertexBufferMap{};
		detail::ResourceBuffer indexBuffer;
		detail::ResourceBuffer uniformBuffer;
		ArrayList<TextureImplementation*> textures{};
		Buffer<VkDescriptorImageInfo> textureSamplers{};
		size_t textureParameterCount;
		Buffer<VkVertexInputBindingDescription> vertexBindings{};
		Buffer<VkVertexInputAttributeDescription> vertexAttributes{};
		RangeAllocator<uint32_t> indexRangeAllocator{};
		HashMap<uint32_t, size_t> indexRangeReferenceCounts{};
		RangeAllocator<uint32_t> parameterRangeAllocator{};
		HashMap<uint32_t, size_t> parameterRangeReferenceCounts{};
		detail::VulkanDescriptorSetLayout parametersDescriptorSetLayout{};
		detail::VulkanDescriptorPool parametersDescriptorPool{};
		VkDescriptorSet parametersDescriptorSet = VK_NULL_HANDLE;
		bool flushed = false;

		MeshContext(VkDevice device, VmaAllocator allocator, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t indexStride, size_t parameterStride,
			size_t textureParameterCount);

		void flush(DeviceImplementation& device);
	};

	struct PipelineKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const PipelineKey& key) const {
				return getHash(key.shaderPipelineHandle) ^ renderPassContextKeyHasher(key.renderPassContextKey);
			}

		private:
			[[no_unique_address]] RenderPassContextKey::Hash renderPassContextKeyHasher;
		};

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		RenderPassContextKey renderPassContextKey;

		[[nodiscard]] bool operator==(const PipelineKey&) const = default;
	};

	struct Pipeline {
		detail::VulkanPipelineLayout pipelineLayout{};
		detail::VulkanPipeline pipeline{};

		Pipeline(DeviceImplementation& device, const PipelineKey& key, const MeshContext& meshContext);
	};

	struct GraphicsQueueSubmission {
		detail::VulkanFence fence{};
		detail::VulkanCommandPool commandPool{};
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		GraphicsQueueSubmissionGenerationIndex generationIndex = 0;
		ArrayList<SharedPointer<RenderPassImplementation>> usedRenderPasses{};
		ArrayList<detail::StagingBuffer> ownedStagingBuffers{};
		ArrayList<detail::TextureResources> ownedTargetedTextureResources{};
		ArrayList<RenderPassContext::FramebufferContext> ownedTargetedFramebufferContexts{};
	};

	static void ensureExclusiveUncompressedTextureAccess(Texture& texture, bool uninitialized);

	GREM_PROFILE_CONSTRUCTOR_BEGIN();
	Filesystem* filesystem;
	String shaderCacheOutputFilepath;
	Device& device;
	VkInstance instance;
	PhysicalDevice physicalDevice;
	detail::VulkanDevice logicalDevice;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	detail::VulkanAllocator allocator;
	detail::VulkanPipelineCache pipelineCache{};
	ShaderCache shaderCache;
	detail::VulkanDescriptorSetLayout instanceOrDrawCommandBufferDescriptorSetLayout;
	Device::PresentationSubmission currentPresentationSubmission{};
	HashMap<RenderPassContextKey, RenderPassContext, RenderPassContextKey::Hash> renderPassContextMap{};
	HashMap<std::type_index, MeshContext> meshContextMap{};
	HashMap<PipelineKey, Pipeline, PipelineKey::Hash> pipelineMap{};
	HashMap<CRC32, detail::VulkanDescriptorSetLayout> bufferDescriptorSetLayoutMap{};
	DoubleEndedQueue<SharedPointer<RenderPassImplementation>> renderPassesForReuse{};
	size_t totalRenderPassesInFlight = 0;
	RingBuffer<detail::StagingBuffer> stagingBuffersForReuse{};
	size_t totalStagingBuffersInFlight = 0;
	size_t totalStagingBufferBytesInFlight = 0;
	RingBuffer<GraphicsQueueSubmission> graphicsQueueSubmissions{};
	GraphicsQueueSubmissionGenerationIndex nextGraphicsQueueSubmissionGenerationIndex = 0;
	Buffer<VkSemaphore> waitSemaphores{};
	Buffer<VkPipelineStageFlags> waitDestinationPipelineStages{};

	DeviceImplementation(Filesystem* filesystem, Window& window, Device& device, const DeviceOptions& options);
	~DeviceImplementation();

	DeviceImplementation(const DeviceImplementation&) = delete;
	DeviceImplementation(DeviceImplementation&&) = delete;
	DeviceImplementation& operator=(const DeviceImplementation&) = delete;
	DeviceImplementation& operator=(DeviceImplementation&&) = delete;

	[[nodiscard]] VkCommandBuffer getGraphicsCommandBuffer() const {
		return graphicsQueueSubmissions.back().commandBuffer;
	}

	void awaitAllCommands();
	VkResult awaitAllCommandsNoexcept() noexcept;
	void submitGraphicsCommands(VkSemaphore signalSemaphore);
	void submitAndAwaitGraphicsCommands();
	void cleanupExpiredFramebufferContexts();

	void adoptTextureResources(GraphicsQueueSubmissionGenerationIndex latestGraphicsQueueSubmissionUsingThisResource, detail::TextureResources&& resources);
	void adoptFramebufferContext(RenderPassContext::FramebufferContext&& framebufferContext);

	[[nodiscard]] RenderPassContext& getRenderPassContext(const RenderPassContextKey& key) {
		return renderPassContextMap.try_emplace(key, *this, key).first->second;
	}

	[[nodiscard]] decltype(auto) getFramebufferContext(RenderPassContext& renderPassContext, const RenderPassContext::FramebufferContextKey& key) {
		const auto [it, inserted] = renderPassContext.framebufferContextMap.try_emplace(key, *this, key, renderPassContext);
		if (!it->first.isReusable()) {
			if (it->second.latestGraphicsQueueSubmissionUsingThisResource != NOT_IN_USE) {
				RenderPassContext::FramebufferContext newFramebufferContext{*this, key, renderPassContext};
				adoptFramebufferContext(std::move(it->second));
				it->second = std::move(newFramebufferContext);
			}
		}
		return *it;
	}

	[[nodiscard]] const MeshContext& getMeshContext(std::type_index meshTypeIndex) {
		DeviceImplementation::MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		meshContext.flush(*this);
		return meshContext;
	}

	[[nodiscard]] Pipeline& getPipeline(const PipelineKey& key, const MeshContext& meshContext) {
		return pipelineMap.try_emplace(key, *this, key, meshContext).first->second;
	}

	Texture& acquireSwapchainImage(TextureImplementation& swapchainTexture);

	[[nodiscard]] detail::StagingBuffer acquireStagingBuffer(size_t size);
	void submitStagingBuffer(detail::StagingBuffer stagingBuffer);

	void declareMeshType(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		size_t indexStride, size_t parameterStride, size_t textureParameterCount);

	[[nodiscard]] RangeAllocation<uint32_t> uploadIndices(std::type_index meshTypeIndex, Span<const byte> indexData, uint32_t indexCount, size_t indexStride);
	[[nodiscard]] RangeAllocation<uint32_t> uploadVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const byte> vertexData,
		uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t vertexStride);
	[[nodiscard]] RangeAllocation<uint32_t> uploadVertexAttributes(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes,
		Span<const Span<const byte>> vertexAttributeData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions);
	[[nodiscard]] RangeAllocation<uint32_t> uploadParameters(std::type_index meshTypeIndex, Span<const byte> parameterValuesBytes,
		Span<const SharedPointer<TextureImplementation>> textures);

	void reacquireIndices(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept;
	void reacquireVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, RangeAllocation<uint32_t> allocation) noexcept;
	void reacquireParameters(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept;

	void releaseIndices(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept;
	void releaseVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, RangeAllocation<uint32_t> allocation) noexcept;
	void releaseParameters(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept;

private:
	void beginGraphicsQueueSubmission();
	void endGraphicsQueueSubmission();
};

} // namespace grem::graphics

#endif
