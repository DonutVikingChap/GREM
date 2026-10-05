// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics/shaders.hpp>

#include "DeviceImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "TextureImplementation.hpp"

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

DeviceImplementation::DeviceImplementation(Window& window, const DeviceOptions& options) {
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

void DeviceImplementation::await() {
	const TimePoint waitStartTime = Clock::now();
	glFinish();
	const TimePoint waitEndTime = Clock::now();
	currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
	cleanupExpiredFramebufferContexts();
}

Device::PresentationSubmission DeviceImplementation::present(Swapchain& swapchain) {
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
	cleanupRenderPassesAvailableForReuse();
	cleanupExpiredFramebufferContexts();
	currentPresentationSubmission.id = static_cast<PresentationSubmissionID>(static_cast<uint64_t>(currentPresentationSubmission.id) + 1);
	return std::exchange(currentPresentationSubmission, Device::PresentationSubmission{.id = currentPresentationSubmission.id});
}

void DeviceImplementation::blit(TextureSubresourceReference renderTarget, const Region2D& targetRegion, TextureRegion2DConstReference renderSource, TextureFilter filter) {
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

void DeviceImplementation::cleanupRenderPassesAvailableForReuse() {
	for (const SharedPointer<RenderPassImplementation>& renderPass : renderPassesForReuse) {
		if (renderPass.use_count() == 1) {
			renderPass->reset();
		}
	}
}

Device::Device(Window& window, const DeviceOptions& options)
	: implementation(UniquePointer<DeviceImplementation>::create(window, options)) {
	constexpr uint32_t INITIAL_STORAGE_BUFFER_RESOLUTION = 64;
	implementation->storageBufferTexture = Texture::create(*this, TextureType::TEXTURE_2D, TextureFormat::R32G32B32A32_FLOAT, Extent2D{INITIAL_STORAGE_BUFFER_RESOLUTION}, 1,
		nullptr, TextureSamplerOptions::UNFILTERED);
	implementation->storageBufferSquareAllocator.expandTo(INITIAL_STORAGE_BUFFER_RESOLUTION);
}

Device::Device(Filesystem&, Window& window, const DeviceOptions& options)
	: Device(window, options) {}

Device::~Device() = default;

void Device::blit(TextureRegion2DReference renderTarget, TextureRegion2DConstReference renderSource, TextureFilter filter) {
	GREM_ASSERT(renderTarget.texture && *renderTarget.texture && renderTarget.texture->get()->maxMultisampleCount <= 1);
	GREM_ASSERT(renderSource.texture && *renderSource.texture && renderSource.texture->get()->maxMultisampleCount <= 1);
	implementation->blit(
		TextureSubresourceReference{
			.texture = renderTarget.texture,
			.subresource{
				.aspects = renderTarget.region.aspects,
				.layer = static_cast<uint32_t>(renderTarget.region.offset.z),
				.mipLevel = renderTarget.region.mipLevel,
			},
		},
		Region2D{.offset{.x = renderTarget.region.offset.x, .y = renderTarget.region.offset.y}, .size = renderTarget.region.size}, renderSource, filter);
}

void Device::blit(TextureSubresourceReference renderTarget, Offset2D targetOffset, TextureRegion2DConstReference renderSource) {
	GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
	GREM_ASSERT(renderSource.texture && *renderSource.texture);
	GREM_ASSERT(renderTarget.texture->get()->maxMultisampleCount <= 1 || renderTarget.texture->get()->maxMultisampleCount == renderSource.texture->get()->maxMultisampleCount);
	implementation->blit(renderTarget, Region2D{.offset = targetOffset, .size = renderSource.region.size}, renderSource, TextureFilter::NEAREST);
}

void Device::render(const RenderPass& renderPass) {
	GREM_ASSERT(&renderPass.get()->device == this);
	implementation->currentPresentationSubmission.totalRenderPassStatistics += renderPass.getStatistics();
	++implementation->currentPresentationSubmission.totalRenderPassCount;
	implementation->cleanupRenderPassesAvailableForReuse();
	implementation->cleanupExpiredFramebufferContexts();
	const_cast<RenderPassImplementation*>(renderPass.get())->render();
}

void Device::await() noexcept {
	implementation->await();
}

bool Device::awaitPresentation(const Swapchain&, PresentationSubmissionID, Duration) noexcept {
	return false;
}

Device::PresentationSubmission Device::present(Swapchain& swapchain) {
	return implementation->present(swapchain);
}

const FeatureSupport& Device::getSupportedFeatures() const noexcept {
	return implementation->supportedFeatures;
}

} // namespace grem::graphics
