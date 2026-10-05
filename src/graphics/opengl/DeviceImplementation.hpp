// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_DEVICE_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_DEVICE_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SquareAllocator.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics/buffers.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "StatePreserver.hpp"
#include "TextureImplementation.hpp"
#include "objects.hpp"
#include "opengl.hpp"

namespace grem::graphics {

struct DeviceImplementation {
	struct ReadFramebufferContextKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const ReadFramebufferContextKey& key) const {
				return getHash(key.colorAttachmentHandle, key.depthStencilAttachmentHandle);
			}
		};

		SharedPointer<TextureImplementation> colorAttachmentHandle{};
		SharedPointer<TextureImplementation> depthStencilAttachmentHandle{};
		GLbitfield depthStencilAttachmentAspectMask = 0;
		uint32_t colorAttachmentLayer = 0;
		uint32_t depthStencilAttachmentLayer = 0;
		uint32_t colorAttachmentMipLevel = 0;
		uint32_t depthStencilAttachmentMipLevel = 0;

		[[nodiscard]] bool operator==(const ReadFramebufferContextKey&) const = default;

		[[nodiscard]] bool isExpired() const {
			return colorAttachmentHandle.use_count() <= 1 || depthStencilAttachmentHandle.use_count() <= 1;
		}
	};

	struct DrawFramebufferContextKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const DrawFramebufferContextKey& key) const {
				return getHash(key.colorAttachmentHandle, key.depthStencilAttachmentHandle);
			}
		};

		WeakPointer<TextureImplementation> colorAttachmentHandle{};
		WeakPointer<TextureImplementation> depthStencilAttachmentHandle{};
		GLbitfield depthStencilAttachmentAspectMask = 0;
		uint32_t colorAttachmentLayer = 0;
		uint32_t depthStencilAttachmentLayer = 0;
		uint32_t colorAttachmentMipLevel = 0;
		uint32_t depthStencilAttachmentMipLevel = 0;

		[[nodiscard]] bool operator==(const DrawFramebufferContextKey&) const = default;

		[[nodiscard]] bool isExpired() const {
			return (colorAttachmentHandle && colorAttachmentHandle.expired()) || (depthStencilAttachmentHandle && depthStencilAttachmentHandle.expired());
		}

		[[nodiscard]] bool isReusable() const {
			return (!colorAttachmentHandle || colorAttachmentHandle.use_count() == 1) && (!depthStencilAttachmentHandle || depthStencilAttachmentHandle.use_count() == 1);
		}
	};

	struct FramebufferContext {
		detail::FramebufferObject framebufferObject = detail::createFramebufferObject();

		FramebufferContext(GLenum framebufferTarget, const auto& key) {
			const detail::FramebufferBindingPreserver framebufferBindingPreserver{
				(framebufferTarget == GL_READ_FRAMEBUFFER) ? GLenum{GL_READ_FRAMEBUFFER_BINDING} : GLenum{GL_DRAW_FRAMEBUFFER_BINDING}};
			glBindFramebuffer(framebufferTarget, framebufferObject.get());

			const auto getTextureObjectHandle = [](const TextureImplementation& texture) -> GLuint {
				GREM_MATCH(texture.object) {
					GREM_CASE(const detail::TextureObject& object) {
						return object.get();
					}
					GREM_CASE(const detail::RenderbufferObject& object) {
						return object.get();
					}
					GREM_CASE(Window * window) break; // NOLINT(misc-const-correctness)
				}
				return 0;
			};

			if (key.colorAttachmentHandle) {
				SharedPointer<TextureImplementation> colorAttachmentHandle{};
				if constexpr (requires { key.colorAttachmentHandle.lock(); }) {
					colorAttachmentHandle = key.colorAttachmentHandle.lock();
				} else {
					colorAttachmentHandle = key.colorAttachmentHandle;
				}
				GREM_ASSERT(colorAttachmentHandle);
				TextureImplementation::attachToBoundFramebuffer(framebufferTarget, GL_COLOR_ATTACHMENT0, colorAttachmentHandle->type,
					getTextureObjectHandle(*colorAttachmentHandle), key.colorAttachmentLayer, key.colorAttachmentMipLevel);
			}

			GREM_ASSERT(key.depthStencilAttachmentHandle || key.depthStencilAttachmentAspectMask == 0);
			if (key.depthStencilAttachmentHandle) {
				GREM_ASSERT((key.depthStencilAttachmentAspectMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0);
				GREM_ASSERT((key.depthStencilAttachmentAspectMask & ~GLbitfield{GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT}) == 0);
				SharedPointer<TextureImplementation> depthStencilAttachmentHandle{};
				if constexpr (requires { key.depthStencilAttachmentHandle.lock(); }) {
					depthStencilAttachmentHandle = key.depthStencilAttachmentHandle.lock();
				} else {
					depthStencilAttachmentHandle = key.depthStencilAttachmentHandle;
				}
				GREM_ASSERT(depthStencilAttachmentHandle);
				const GLenum attachment =
					(key.depthStencilAttachmentAspectMask == (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) ? GL_DEPTH_STENCIL_ATTACHMENT
					: (key.depthStencilAttachmentAspectMask == GL_STENCIL_BUFFER_BIT)
				        ? GL_STENCIL_ATTACHMENT
				        : GL_DEPTH_ATTACHMENT;
				TextureImplementation::attachToBoundFramebuffer(framebufferTarget, attachment, depthStencilAttachmentHandle->type,
					getTextureObjectHandle(*depthStencilAttachmentHandle), key.depthStencilAttachmentLayer, key.depthStencilAttachmentMipLevel);
			}
		}
	};

	static void ensureExclusiveUncompressedTextureAccess(Texture& texture, bool uninitialized) {
		detail::ensureExclusiveResourceAccess(
			texture.implementation,
			[&]() -> SharedPointer<TextureImplementation> {
				if (uninitialized) {
					return TextureImplementation::cloneUncompressedUninitialized(*texture.implementation);
				}
				return TextureImplementation::cloneUncompressed(*texture.implementation);
			},
			[&](TextureImplementation& oldTexture) -> void {
				if (uninitialized) {
					oldTexture.allocateUncompressed(*texture.implementation);
				} else {
					oldTexture = *texture.implementation;
				}
			});
	}

	[[nodiscard]] static ReadFramebufferContextKey acquireReadFramebufferContextKey(Optional<TextureSubresourceConstReference> colorSource,
		Optional<TextureSubresourceConstReference> depthStencilSource = {}) {
		ReadFramebufferContextKey result{};

		if (colorSource) {
			GREM_ASSERT(colorSource->texture && *colorSource->texture);
			GREM_ASSERT(colorSource->texture->get()->type != TextureType::SWAPCHAIN);
			const TextureAspects colorAspects = colorSource->subresource.aspects & Texture::getFormatAspects(colorSource->texture->get()->internalFormat) & TextureAspect::COLOR;
			if (!colorAspects.empty()) {
				result.colorAttachmentHandle = colorSource->texture->lock();
				result.colorAttachmentLayer = colorSource->subresource.layer;
				result.colorAttachmentMipLevel = colorSource->subresource.mipLevel;
			}
		}

		if (depthStencilSource) {
			GREM_ASSERT(depthStencilSource->texture && *depthStencilSource->texture);
			GREM_ASSERT(depthStencilSource->texture->get()->type != TextureType::SWAPCHAIN);
			const TextureAspects depthStencilAspects =
				depthStencilSource->subresource.aspects & Texture::getFormatAspects(depthStencilSource->texture->get()->internalFormat) & TextureAspects::DEPTH_STENCIL;
			if (!depthStencilAspects.empty()) {
				result.depthStencilAttachmentHandle = depthStencilSource->texture->lock();
				result.depthStencilAttachmentAspectMask = TextureImplementation::getAspectBits(depthStencilAspects);
				result.depthStencilAttachmentLayer = depthStencilSource->subresource.layer;
				result.depthStencilAttachmentMipLevel = depthStencilSource->subresource.mipLevel;
			}
		}

		return result;
	}

	[[nodiscard]] static DrawFramebufferContextKey acquireDrawFramebufferContextKey(Optional<TextureSubresourceReference> colorTarget,
		Optional<TextureSubresourceReference> depthStencilTarget, GLbitfield uninitializedTargetAspectMask) {
		DrawFramebufferContextKey result{};

		if (colorTarget) {
			GREM_ASSERT(colorTarget->texture && *colorTarget->texture);
			GREM_ASSERT(colorTarget->texture->get()->type != TextureType::SWAPCHAIN);
			const TextureAspects fullAspects = Texture::getFormatAspects(colorTarget->texture->get()->internalFormat);
			const TextureAspects colorAspects = colorTarget->subresource.aspects & fullAspects & TextureAspect::COLOR;
			if (!colorAspects.empty()) {
				const bool isWholeTexture = colorTarget->texture->getDepth() == 1 && colorTarget->texture->getMipLevelCount() == 1;
				const GLbitfield fullAspectMask = TextureImplementation::getAspectBits(fullAspects);
				ensureExclusiveUncompressedTextureAccess(*colorTarget->texture, isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
				result.colorAttachmentHandle = colorTarget->texture->lock();
				result.colorAttachmentLayer = colorTarget->subresource.layer;
				result.colorAttachmentMipLevel = colorTarget->subresource.mipLevel;
			}
		}

		if (depthStencilTarget) {
			GREM_ASSERT(depthStencilTarget->texture && *depthStencilTarget->texture);
			GREM_ASSERT(depthStencilTarget->texture->get()->type != TextureType::SWAPCHAIN);
			const TextureAspects fullAspects = Texture::getFormatAspects(depthStencilTarget->texture->get()->internalFormat);
			const TextureAspects depthStencilAspects = depthStencilTarget->subresource.aspects & fullAspects & TextureAspects::DEPTH_STENCIL;
			if (!depthStencilAspects.empty()) {
				const bool isWholeTexture = depthStencilTarget->texture->getDepth() == 1 && depthStencilTarget->texture->getMipLevelCount() == 1;
				const GLbitfield fullAspectMask = TextureImplementation::getAspectBits(fullAspects);
				ensureExclusiveUncompressedTextureAccess(*depthStencilTarget->texture, isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
				result.depthStencilAttachmentHandle = depthStencilTarget->texture->lock();
				result.depthStencilAttachmentAspectMask = TextureImplementation::getAspectBits(depthStencilAspects);
				result.depthStencilAttachmentLayer = depthStencilTarget->subresource.layer;
				result.depthStencilAttachmentMipLevel = depthStencilTarget->subresource.mipLevel;
			}
		}

		return result;
	}

	GREM_PROFILE_CONSTRUCTOR_BEGIN();
	HashMap<ReadFramebufferContextKey, FramebufferContext, ReadFramebufferContextKey::Hash> readFramebufferContextMap{};
	HashMap<DrawFramebufferContextKey, FramebufferContext, DrawFramebufferContextKey::Hash> drawFramebufferContextMap{};
	DoubleEndedQueue<SharedPointer<RenderPassImplementation>> renderPassesForReuse{};
	FeatureSupport supportedFeatures{
#ifdef __EMSCRIPTEN__
		.graphicsBackendAPIName = "WebGL",
		.graphicsBackendAPIVersionName = "2.0",
#else
		.graphicsBackendAPIName = "OpenGL",
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
		.graphicsBackendAPIVersionName = "ES 3.0",
#else
		.graphicsBackendAPIVersionName = "3.3 Core",
#endif
#endif
		.supportsGLSLShaderCode = true,
		.supportsSPIRVShaderCode = false,
	};
	Device::PresentationSubmission currentPresentationSubmission{};
	Texture storageBufferTexture{};
	SquareAllocator<uint32_t> storageBufferSquareAllocator{};
	HashSet<StorageBufferImplementation*> storageBuffers{};

	DeviceImplementation(Window& window, const DeviceOptions& options);

	void await();

	Device::PresentationSubmission present(Swapchain& swapchain);

	void blit(TextureSubresourceReference renderTarget, const Region2D& targetRegion, TextureRegion2DConstReference renderSource, TextureFilter filter);

	[[nodiscard]] FramebufferContext& getReadFramebufferContext(const ReadFramebufferContextKey& key) {
		return readFramebufferContextMap.try_emplace(key, GL_READ_FRAMEBUFFER, key).first->second;
	}

	[[nodiscard]] FramebufferContext& getDrawFramebufferContext(const DrawFramebufferContextKey& key) {
		if (!key.isReusable()) {
			drawFramebufferContextMap.erase(key);
		}
		return drawFramebufferContextMap.try_emplace(key, GL_DRAW_FRAMEBUFFER, key).first->second;
	}

	void cleanupRenderPassesAvailableForReuse();

	void cleanupExpiredFramebufferContexts() {
		erase_if(readFramebufferContextMap, [](const auto& kv) -> bool { return kv.first.isExpired(); });
		erase_if(drawFramebufferContextMap, [](const auto& kv) -> bool { return kv.first.isExpired(); });
	}
};

} // namespace grem::graphics

#endif
