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

#include <SDL3/SDL.h> // SDL...
#include <utility>    // std::move, std::exchange
#if !defined(NDEBUG) && !defined(GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE)
#include <cstdio> // stderr, std::fprintf
#endif

namespace grem::graphics {

namespace {

#if !defined(NDEBUG) && !defined(GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE)
void GLAPIENTRY debugOutputCallback(GLenum /*source*/, GLenum type, GLuint /*id*/, GLenum severity, GLsizei /*length*/, const GLchar* message, const void* /*userParam*/) {
	constexpr GLenum DEBUG_SEVERITY_NOTIFICATION = 0x826B;
	constexpr GLenum DEBUG_TYPE_ERROR = 0x824C;
	if (severity != DEBUG_SEVERITY_NOTIFICATION) {
		if (type == DEBUG_TYPE_ERROR) {
			std::fprintf(stderr, "OpenGL ERROR: %s\n", message);
		} else {
			std::fprintf(stderr, "OpenGL: %s\n", message);
		}
	}
}
#endif

} // namespace

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
					GREM_CASE(Window * window) break;
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
				const GLbitfield depthStencilAspectMask = TextureImplementation::getAspectBits(depthStencilAspects);
				result.depthStencilAttachmentHandle = depthStencilSource->texture->lock();
				result.depthStencilAttachmentAspectMask = depthStencilAspectMask;
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
				const GLbitfield colorAspectMask = TextureImplementation::getAspectBits(colorAspects);
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
				const GLbitfield depthStencilAspectMask = TextureImplementation::getAspectBits(depthStencilAspects);
				ensureExclusiveUncompressedTextureAccess(*depthStencilTarget->texture, isWholeTexture && (fullAspectMask & uninitializedTargetAspectMask) == fullAspectMask);
				result.depthStencilAttachmentHandle = depthStencilTarget->texture->lock();
				result.depthStencilAttachmentAspectMask = depthStencilAspectMask;
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

	DeviceImplementation(Window& window, const DeviceOptions& options) {
		(void)window;
		(void)options;

		if (const char* const videoDriverName = SDL_GetCurrentVideoDriver()) {
			supportedFeatures.videoDriverName = videoDriverName;
		}

		GLint maxTextureSize{};
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
		supportedFeatures.max2DTextureResolution = static_cast<uint32_t>(maxTextureSize);

		GLint maxCubeMapTextureSize{};
		glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapTextureSize);
		supportedFeatures.maxCubeTextureResolution = static_cast<uint32_t>(maxCubeMapTextureSize);

		GLint maxArrayTextureLayers{};
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
		supportedFeatures.maxTextureLayerCount = static_cast<uint32_t>(maxArrayTextureLayers);

		GLint maxSamples{};
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
		supportedFeatures.maxSupportedMultisampleCount = static_cast<uint32_t>(max(maxSamples, GLint{1}));

		GLint extensionCount = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

		bool foundSRGBExtension = false;
		bool foundS3TCExtension = false;
		for (GLint extensionIndex = 0; extensionIndex < extensionCount; ++extensionIndex) {
			const CStringView extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(extensionIndex)));
			if (extension == "GL_EXT_texture_filter_anisotropic" || extension == "EXT_texture_filter_anisotropic") {
				constexpr GLenum MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
				GLfloat maxMaxAnisotropy{};
				glGetFloatv(MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxMaxAnisotropy);
				supportedFeatures.maxSupportedSamplerAnisotropy = static_cast<float>(max(maxMaxAnisotropy, GLfloat{1.0f}));
			} else if (extension == "GL_EXT_texture_sRGB") {
				foundSRGBExtension = true;
			} else if (extension == "GL_EXT_texture_compression_s3tc" || extension == "WEBGL_compressed_texture_s3tc") {
				supportedFeatures.supportsTextureCompressionS3TC = true;
				foundS3TCExtension = true;
			} else if (extension == "GL_EXT_texture_compression_s3tc_srgb" || extension == "WEBGL_compressed_texture_s3tc_srgb") {
				supportedFeatures.supportsTextureCompressionS3TC_SRGB = true;
			} else {
#if !defined(NDEBUG) && !defined(GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE)
				if (extension == "GL_KHR_debug") {
					if (const SDL_FunctionPointer debugMessageCallback = SDL_GL_GetProcAddress("glDebugMessageCallback")) {
						using DebugOutputCallbackFunctionPointer =
							void(GLAPIENTRY*)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
						using DebugMessageCallbackFunctionPointer = void(GLAPIENTRY*)(DebugOutputCallbackFunctionPointer callback, const void* userParam);
						constexpr GLenum DEBUG_OUTPUT = 0x92E0;
						glEnable(DEBUG_OUTPUT);
						reinterpret_cast<DebugMessageCallbackFunctionPointer>(debugMessageCallback)(debugOutputCallback, nullptr);
					}
				}
#endif
			}
		}

		if (foundSRGBExtension && foundS3TCExtension) {
			supportedFeatures.supportsTextureCompressionS3TC_SRGB = true;
		}

#ifndef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

		GREM_PROFILE_CONSTRUCTOR_END();
	}

	void await() {
		const TimePoint waitStartTime = Clock::now();
		glFinish();
		const TimePoint waitEndTime = Clock::now();
		currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
		cleanupExpiredFramebufferContexts();
	}

	Device::PresentationSubmission present(Swapchain& swapchain) {
		GREM_ASSERT(swapchain.getType() == TextureType::SWAPCHAIN);
		Window& window = *swapchain.get()->object.get<Window*>();
		SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		const TimePoint waitStartTime = Clock::now();
		if (!SDL_GL_SwapWindow(static_cast<SDL_Window*>(window.get()))) {
			throw graphics::Error{String{"Failed to present device frame to window:\n"} + SDL_GetError()};
		}
		const TimePoint waitEndTime = Clock::now();
		currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
		cleanupExpiredFramebufferContexts();
		currentPresentationSubmission.id = static_cast<PresentationSubmissionID>(static_cast<uint64_t>(currentPresentationSubmission.id) + 1);
		return std::exchange(currentPresentationSubmission, Device::PresentationSubmission{.id = currentPresentationSubmission.id});
	}

	void blit(TextureSubresourceReference renderTarget, const Region2D& targetRegion, TextureRegion2DConstReference renderSource, TextureFilter filter) {
		GREM_ASSERT(renderSource.texture && *renderSource.texture);
		GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
		GREM_ASSERT(targetRegion.offset.x >= 0);
		GREM_ASSERT(targetRegion.offset.y >= 0);
		GREM_ASSERT(renderSource.region.offset.x >= 0);
		GREM_ASSERT(renderSource.region.offset.y >= 0);

		GLbitfield readAspectMask = 0;
		if (renderSource.texture->get()->type == TextureType::SWAPCHAIN) {
			Window& window = *renderSource.texture->get()->object.get<Window*>();
			SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			readAspectMask |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		} else {
			GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.x) + renderSource.region.size.width <= renderSource.texture->get()->size.width);
			GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.y) + renderSource.region.size.height <= renderSource.texture->get()->size.height);

			const TextureSubresourceConstReference renderSourceSubresource{
				.texture = renderSource.texture,
				.subresource{
					.aspects = renderSource.region.aspects,
					.layer = static_cast<uint32_t>(renderSource.region.offset.z),
					.mipLevel = renderSource.region.mipLevel,
				},
			};
			const ReadFramebufferContextKey readFramebufferContextKey = acquireReadFramebufferContextKey(renderSourceSubresource, renderSourceSubresource);
			const GLuint readFramebufferObjectHandle = getReadFramebufferContext(readFramebufferContextKey).framebufferObject.get();
			glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferObjectHandle);
			if (readFramebufferContextKey.colorAttachmentHandle) {
				readAspectMask |= GL_COLOR_BUFFER_BIT;
			}
			readAspectMask |= readFramebufferContextKey.depthStencilAttachmentAspectMask;
		}

		GLbitfield drawAspectMask = 0;
		if (renderTarget.texture->get()->type == TextureType::SWAPCHAIN) {
			Window& window = *renderTarget.texture->get()->object.get<Window*>();
			SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window.get()), static_cast<SDL_GLContext>(window.getSurface()));
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			drawAspectMask |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		} else {
			GREM_ASSERT(static_cast<uint32_t>(targetRegion.offset.x) + targetRegion.size.width <= renderTarget.texture->get()->size.width);
			GREM_ASSERT(static_cast<uint32_t>(targetRegion.offset.y) + targetRegion.size.height <= renderTarget.texture->get()->size.height);

			const GLbitfield uninitializedTargetAspectMask =
				(targetRegion.size == renderTarget.texture->getSize2D() && targetRegion.size == renderSource.region.size) ? readAspectMask : 0;
			const DrawFramebufferContextKey drawFramebufferContextKey = acquireDrawFramebufferContextKey(renderTarget, renderTarget, uninitializedTargetAspectMask);
			const GLuint drawFramebufferObjectHandle = getDrawFramebufferContext(drawFramebufferContextKey).framebufferObject.get();
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferObjectHandle);
			if (drawFramebufferContextKey.colorAttachmentHandle) {
				drawAspectMask |= GL_COLOR_BUFFER_BIT;
			}
			drawAspectMask |= drawFramebufferContextKey.depthStencilAttachmentAspectMask;
		}

		const GLint srcX0 = static_cast<GLint>(renderSource.region.offset.x);
		const GLint srcY0 = static_cast<GLint>(renderSource.region.offset.y);
		const GLint srcX1 = static_cast<GLint>(renderSource.region.offset.x + static_cast<int32_t>(renderSource.region.size.width));
		const GLint srcY1 = static_cast<GLint>(renderSource.region.offset.y + static_cast<int32_t>(renderSource.region.size.height));
		const GLint dstX0 = static_cast<GLint>(targetRegion.offset.x);
		const GLint dstY0 = static_cast<GLint>(targetRegion.offset.y);
		const GLint dstX1 = static_cast<GLint>(targetRegion.offset.x + static_cast<int32_t>(targetRegion.size.width));
		const GLint dstY1 = static_cast<GLint>(targetRegion.offset.y + static_cast<int32_t>(targetRegion.size.height));

		const GLbitfield aspectMask = readAspectMask & drawAspectMask;
		if (aspectMask == 0) {
			return;
		}

		glDisable(GL_SCISSOR_TEST);
		glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, aspectMask, TextureImplementation::getFilter(filter));

		++currentPresentationSubmission.totalBlitCount;
	}

	[[nodiscard]] FramebufferContext& getReadFramebufferContext(const ReadFramebufferContextKey& key) {
		return readFramebufferContextMap.try_emplace(key, GL_READ_FRAMEBUFFER, key).first->second;
	}

	[[nodiscard]] FramebufferContext& getDrawFramebufferContext(const DrawFramebufferContextKey& key) {
		if (!key.isReusable()) {
			drawFramebufferContextMap.erase(key);
		}
		return drawFramebufferContextMap.try_emplace(key, GL_DRAW_FRAMEBUFFER, key).first->second;
	}

	void cleanupExpiredFramebufferContexts() {
		readFramebufferContextMap.clear();
		erase_if(drawFramebufferContextMap, [](const auto& kv) -> bool { return kv.first.isExpired(); });
	}
};

} // namespace grem::graphics

#endif
