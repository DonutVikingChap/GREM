// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_TEXTURE_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_TEXTURE_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/resource/Image.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "StatePreserver.hpp"
#include "objects.hpp"

#include <utility> // std::move

namespace grem::graphics {

class Window; // Forward declaration, to avoid including Window.hpp.

struct TextureImplementation : detail::ReusableCopyOnWriteResourceBase<TextureImplementation> {
	struct UninitializedTag {};

	[[nodiscard]] static GLbitfield getAspectBits(TextureAspects aspects) noexcept {
		return static_cast<GLbitfield>(aspects.bits) & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	[[nodiscard]] static GLenum getTextureTarget(TextureType type) {
		switch (type) {
			case TextureType::EMPTY: [[fallthrough]];
			case TextureType::RENDERBUFFER: [[fallthrough]];
			case TextureType::SWAPCHAIN: break;
			case TextureType::TEXTURE_2D: return GL_TEXTURE_2D;
			case TextureType::TEXTURE_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
			case TextureType::TEXTURE_CUBE: return GL_TEXTURE_CUBE_MAP;
			case TextureType::TEXTURE_CUBE_ARRAY: return GL_TEXTURE_2D_ARRAY;
		}
		unreachable();
	}

	[[nodiscard]] static GLenum getCubemapTarget(uint32_t layer) {
		// Note: Side 2 and 3 are flipped to match the Vulkan convention.
		switch (layer) {
			case 0: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			case 1: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			case 2: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			case 3: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			case 4: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			case 5: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			default: break;
		}
		unreachable();
	}

	[[nodiscard]] static GLenum getFramebufferAttachment(TextureFormat internalFormat) {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: break;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return GL_COLOR_ATTACHMENT0;
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: return GL_DEPTH_ATTACHMENT;
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return GL_DEPTH_STENCIL_ATTACHMENT;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: throw graphics::Error{"Texture format is not framebuffer compatible."};
		}
		unreachable();
	}

	[[nodiscard]] static GLbitfield getFramebufferMask(TextureFormat internalFormat) {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: break;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return GL_COLOR_BUFFER_BIT;
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: return GL_DEPTH_BUFFER_BIT;
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: throw graphics::Error{"Texture format is not framebuffer compatible."};
		}
		unreachable();
	}

	[[nodiscard]] static GLenum getBaseImageFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: return 0;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: return GL_RED;
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return GL_RG;
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return GL_RGB;
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return GL_RGBA;
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: return GL_DEPTH_COMPONENT;
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return GL_DEPTH_STENCIL;
		}
		unreachable();
	}

	[[nodiscard]] static GLenum getBaseImageType(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: return 0;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return GL_UNSIGNED_BYTE;
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: return GL_HALF_FLOAT;
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return GL_FLOAT;
			case TextureFormat::D16_UNORM: return GL_UNSIGNED_SHORT;
			case TextureFormat::D24_UNORM_S8_UINT: return GL_UNSIGNED_INT_24_8;
			case TextureFormat::D32_FLOAT_S8_UINT: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
			case TextureFormat::R5G6B5_UNORM_PACK16: return GL_UNSIGNED_SHORT_5_6_5;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return GL_UNSIGNED_SHORT_5_5_5_1;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return GL_UNSIGNED_INT_10F_11F_11F_REV;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return GL_UNSIGNED_INT_2_10_10_10_REV;
		}
		unreachable();
	}

	[[nodiscard]] static GLint getWrappingMode(TextureWrappingMode wrappingMode) noexcept {
		switch (wrappingMode) {
			case TextureWrappingMode::REPEAT: return GL_REPEAT;
			case TextureWrappingMode::MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
			case TextureWrappingMode::CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
		}
		unreachable();
	}

	[[nodiscard]] static GLenum getFilter(TextureFilter filter) noexcept {
		switch (filter) {
			case TextureFilter::NEAREST: return GL_NEAREST;
			case TextureFilter::LINEAR: return GL_LINEAR;
		}
		unreachable();
	}

	[[nodiscard]] static GLint getMipmapFilter(TextureFilter filter, TextureMipmapMode mipmapMode) noexcept {
		switch (filter) {
			case TextureFilter::NEAREST:
				switch (mipmapMode) {
					case TextureMipmapMode::NONE: return GL_NEAREST;
					case TextureMipmapMode::NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
					case TextureMipmapMode::LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
				}
				break;
			case TextureFilter::LINEAR:
				switch (mipmapMode) {
					case TextureMipmapMode::NONE: return GL_LINEAR;
					case TextureMipmapMode::NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
					case TextureMipmapMode::LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
				}
				break;
		}
		unreachable();
	}

	static void attachToBoundFramebuffer(GLenum target, GLenum attachment, TextureType type, GLuint textureObjectHandle, uint32_t layer, uint32_t mipLevel) {
		switch (type) {
			case TextureType::EMPTY: unreachable();
			case TextureType::SWAPCHAIN: GREM_ASSERT(textureObjectHandle == 0 && layer == 0 && mipLevel == 0); break;
			case TextureType::TEXTURE_2D:
				GREM_ASSERT(layer == 0);
				glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, textureObjectHandle, static_cast<GLint>(mipLevel));
				break;
			case TextureType::TEXTURE_CUBE:
				glFramebufferTexture2D(target, attachment, TextureImplementation::getCubemapTarget(layer), textureObjectHandle, static_cast<GLint>(mipLevel));
				break;
			case TextureType::TEXTURE_2D_ARRAY: [[fallthrough]];
			case TextureType::TEXTURE_CUBE_ARRAY:
				glFramebufferTextureLayer(target, attachment, textureObjectHandle, static_cast<GLint>(mipLevel), static_cast<GLint>(layer));
				break;
			case TextureType::RENDERBUFFER:
				GREM_ASSERT(layer == 0 && mipLevel == 0);
				glFramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, textureObjectHandle);
				break;
		}
	}

	static void clearBoundFramebuffer(TextureFormat internalFormat, GLbitfield aspectMask, const ClearValues& clearValues) {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: unreachable();
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32:
				if ((aspectMask & GL_COLOR_BUFFER_BIT) != 0) {
					const vec4 clearColor = clearValues.color.toLinearRGBA();
					const Array<GLfloat, 4> color{clearColor.x, clearColor.y, clearColor.z, clearColor.w};
					glClearBufferfv(GL_COLOR, 0, color.data());
				}
				break;
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT:
				if ((aspectMask & GL_DEPTH_BUFFER_BIT) != 0) {
					glClearBufferfv(GL_DEPTH, 0, &clearValues.depth);
				}
				break;
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT:
				if ((aspectMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) == (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
					glClearBufferfi(GL_DEPTH_STENCIL, 0, clearValues.depth, static_cast<GLint>(clearValues.stencil));
				} else if ((aspectMask & GL_DEPTH_BUFFER_BIT) != 0) {
					glClearBufferfv(GL_DEPTH, 0, &clearValues.depth);
				} else if ((aspectMask & GL_STENCIL_BUFFER_BIT) != 0) {
					const GLint stencil = static_cast<GLint>(clearValues.stencil);
					glClearBufferiv(GL_STENCIL, 0, &stencil);
				}
				break;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: throw graphics::Error{"Texture format is not framebuffer compatible."};
		}
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> create(Device& device, Variant<detail::TextureObject, detail::RenderbufferObject, Window*> object, TextureType type,
		TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount, uint32_t maxMultisampleCount, Optional<TextureSamplerOptions> samplerOptions) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(device, std::move(object), type, internalFormat, size, mipLevelCount, maxMultisampleCount, samplerOptions);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressed(const TextureImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressedWithSamplerOptions(const TextureImplementation& implementation,
		Optional<TextureSamplerOptions> newSamplerOptions) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation, newSamplerOptions);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressedUninitialized(const TextureImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation, UninitializedTag{});
	}

	Device& device;
	Variant<detail::TextureObject, detail::RenderbufferObject, Window*> object{};
	TextureType type{};
	TextureFormat internalFormat{};
	Extent3D size{};
	uint32_t mipLevelCount{};
	uint32_t maxMultisampleCount{};
	Optional<TextureSamplerOptions> samplerOptions{};

	TextureImplementation(Device& device, Variant<detail::TextureObject, detail::RenderbufferObject, Window*> object, TextureType type, TextureFormat internalFormat, Extent3D size,
		uint32_t mipLevelCount, uint32_t maxMultisampleCount, Optional<TextureSamplerOptions> samplerOptions)
		: device(device)
		, object(std::move(object))
		, type(type)
		, internalFormat(internalFormat)
		, size(size)
		, mipLevelCount(mipLevelCount)
		, maxMultisampleCount(maxMultisampleCount)
		, samplerOptions(samplerOptions) {}

	TextureImplementation(const TextureImplementation& other)
		: device(other.device) {
		assign(other);
	}

	TextureImplementation(const TextureImplementation& other, Optional<TextureSamplerOptions> newSamplerOptions)
		: device(other.device) {
		assign(other, newSamplerOptions);
	}

	TextureImplementation(const TextureImplementation& other, UninitializedTag)
		: device(other.device) {
		allocateUncompressed(other);
	}

	TextureImplementation& operator=(const TextureImplementation& other) {
		if (this == &other) {
			return *this;
		}

		assign(other);
		return *this;
	}

	TextureImplementation(TextureImplementation&&) = delete;
	TextureImplementation& operator=(TextureImplementation&&) = delete;

	void setupSampler() const {
		if (!samplerOptions) {
			return;
		}
		const GLenum target = getTextureTarget(type);
		glTexParameteri(target, GL_TEXTURE_WRAP_S, getWrappingMode(samplerOptions->horizontalWrappingMode));
		glTexParameteri(target, GL_TEXTURE_WRAP_T, getWrappingMode(samplerOptions->verticalWrappingMode));
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, getMipmapFilter(samplerOptions->minificationFilter, samplerOptions->mipmapMode));
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(getFilter(samplerOptions->magnificationFilter)));
		if (Texture::getFormatAspects(internalFormat).contains(TextureAspect::DEPTH) && samplerOptions->depthComparisonMode) {
			GLint op{};
			switch (*samplerOptions->depthComparisonMode) {
				case TextureDepthComparisonMode::NEVER_PASS: op = GL_NEVER; break;
				case TextureDepthComparisonMode::LESS: op = GL_LESS; break;
				case TextureDepthComparisonMode::LESS_OR_EQUAL: op = GL_LEQUAL; break;
				case TextureDepthComparisonMode::GREATER: op = GL_GREATER; break;
				case TextureDepthComparisonMode::GREATER_OR_EQUAL: op = GL_GEQUAL; break;
				case TextureDepthComparisonMode::EQUAL: op = GL_EQUAL; break;
				case TextureDepthComparisonMode::NOT_EQUAL: op = GL_NOTEQUAL; break;
				case TextureDepthComparisonMode::ALWAYS_PASS: op = GL_ALWAYS; break;
			}
			glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, op);
		}
		const float maxSupportedSamplerAnisotropy = device.getSupportedFeatures().maxSupportedSamplerAnisotropy;
		if (maxSupportedSamplerAnisotropy > 1.0f && samplerOptions->maxAnisotropy > 1.0f) {
			constexpr GLenum TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
			glTexParameterf(target, TEXTURE_MAX_ANISOTROPY_EXT, min(samplerOptions->maxAnisotropy, maxSupportedSamplerAnisotropy));
		}
	}

	void assign(const TextureImplementation& other) {
		assign(other, other.samplerOptions);
	}

	void assign(const TextureImplementation& other, Optional<TextureSamplerOptions> newSamplerOptions) {
		GREM_ASSERT(&device == &other.device);

		allocateUncompressed(other, newSamplerOptions);

		const GLenum attachment = getFramebufferAttachment(internalFormat);
		const GLbitfield mask = getFramebufferMask(internalFormat);

		detail::FramebufferObject readFramebufferObject = detail::createFramebufferObject();
		detail::FramebufferObject drawFramebufferObject = detail::createFramebufferObject();

		const detail::ReadFramebufferBindingPreserver readFramebufferBindingPreserver{};
		glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferObject.get());

		const detail::DrawFramebufferBindingPreserver drawFramebufferBindingPreserver{};
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferObject.get());

		const detail::ScissorTestPreserver scissorTestPreserver{};
		glDisable(GL_SCISSOR_TEST);

		GREM_MATCH(other.object) {
			GREM_CASE(const detail::TextureObject& otherTextureObject) {
				detail::TextureObject& textureObject = object.as<detail::TextureObject>();

				const GLenum target = getTextureTarget(type);

				const detail::TextureBindingPreserver textureBindingPreserver{target};
				glBindTexture(target, static_cast<GLuint>(textureObject.get()));

				switch (type) {
					case TextureType::EMPTY: [[fallthrough]];
					case TextureType::RENDERBUFFER: [[fallthrough]];
					case TextureType::SWAPCHAIN: unreachable();
					case TextureType::TEXTURE_2D:
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
							glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textureObject.get(), static_cast<GLint>(mipLevel));
							glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, GL_TEXTURE_2D, otherTextureObject.get(), static_cast<GLint>(mipLevel));
							glBlitFramebuffer(0, 0, static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), 0, 0, static_cast<GLint>(mipLevelSize.width),
								static_cast<GLint>(mipLevelSize.height), mask, GL_NEAREST);
						}
						break;
					case TextureType::TEXTURE_2D_ARRAY:
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
								glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, textureObject.get(), static_cast<GLint>(mipLevel), static_cast<GLint>(layer));
								glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, attachment, otherTextureObject.get(), static_cast<GLint>(mipLevel), static_cast<GLint>(layer));
								glBlitFramebuffer(0, 0, static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), 0, 0,
									static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), mask, GL_NEAREST);
							}
						}
						break;
					case TextureType::TEXTURE_CUBE:
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
								glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, TextureImplementation::getCubemapTarget(layer), textureObject.get(),
									static_cast<GLint>(mipLevel));
								glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, TextureImplementation::getCubemapTarget(layer), otherTextureObject.get(),
									static_cast<GLint>(mipLevel));
								glBlitFramebuffer(0, 0, static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), 0, 0,
									static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), mask, GL_NEAREST);
							}
						}
						break;
					case TextureType::TEXTURE_CUBE_ARRAY:
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
								glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, textureObject.get(), static_cast<GLint>(mipLevel), static_cast<GLint>(layer));
								glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, attachment, otherTextureObject.get(), static_cast<GLint>(mipLevel), static_cast<GLint>(layer));
								glBlitFramebuffer(0, 0, static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), 0, 0,
									static_cast<GLint>(mipLevelSize.width), static_cast<GLint>(mipLevelSize.height), mask, GL_NEAREST);
							}
						}
						break;
				}
				break;
			}
			GREM_CASE(const detail::RenderbufferObject& otherRenderbufferObject) {
				detail::RenderbufferObject& renderbufferObject = object.as<detail::RenderbufferObject>();

				const detail::RenderbufferBindingPreserver renderbufferBindingPreserver{};
				glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbufferObject.get()));

				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbufferObject.get());
				glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, attachment, GL_RENDERBUFFER, otherRenderbufferObject.get());
				glBlitFramebuffer(0, 0, static_cast<GLint>(size.width), static_cast<GLint>(size.height), 0, 0, static_cast<GLint>(size.width), static_cast<GLint>(size.height),
					mask, GL_NEAREST);
				break;
			}
			GREM_CASE(Window * window) {
				unreachable();
			}
		}
	}

	void allocateUncompressed(const TextureImplementation& other, Optional<TextureSamplerOptions> newSamplerOptions) {
		GREM_ASSERT(&device == &other.device);
		if (newSamplerOptions && other.type == TextureType::RENDERBUFFER) {
			throw graphics::Error{"Cannot create a sampled renderbuffer."};
		}

		const TextureFormat newInternalFormat = Texture::getUncompressedFormat(other.internalFormat);
		if (type == other.type && internalFormat == newInternalFormat && size == other.size && mipLevelCount == other.mipLevelCount &&
			maxMultisampleCount == other.maxMultisampleCount) {
			if (samplerOptions != newSamplerOptions) {
				samplerOptions = newSamplerOptions;
				setupSampler();
			}
			return;
		}

		type = other.type;
		internalFormat = newInternalFormat;
		size = other.size;
		mipLevelCount = other.mipLevelCount;
		maxMultisampleCount = other.maxMultisampleCount;
		samplerOptions = newSamplerOptions;

		GREM_MATCH(other.object) {
			GREM_CASE(const detail::TextureObject& otherTextureObject) {
				detail::TextureObject textureObject = detail::createTextureObject();

				const GLenum target = getTextureTarget(type);
				const GLenum imageFormat = getBaseImageFormat(internalFormat);
				const GLenum imageType = getBaseImageType(internalFormat);

				const detail::TextureBindingPreserver textureBindingPreserver{target};
				glBindTexture(target, textureObject.get());
				glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(mipLevelCount - 1));

				switch (type) {
					case TextureType::EMPTY: [[fallthrough]];
					case TextureType::RENDERBUFFER: [[fallthrough]];
					case TextureType::SWAPCHAIN: unreachable();
					case TextureType::TEXTURE_2D:
						GREM_ASSERT(size.depth == 1);
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
							glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
								static_cast<GLsizei>(mipLevelSize.height), 0, imageFormat, imageType, nullptr);
						}
						break;
					case TextureType::TEXTURE_2D_ARRAY:
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
								static_cast<GLsizei>(mipLevelSize.height), static_cast<GLsizei>(mipLevelSize.depth), 0, imageFormat, imageType, nullptr);
						}
						break;
					case TextureType::TEXTURE_CUBE:
						GREM_ASSERT(size.depth == 6);
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
								glTexImage2D(TextureImplementation::getCubemapTarget(layer), static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat),
									static_cast<GLsizei>(mipLevelSize.width), static_cast<GLsizei>(mipLevelSize.height), 0, imageFormat, imageType, nullptr);
							}
						}
						break;
					case TextureType::TEXTURE_CUBE_ARRAY:
						GREM_ASSERT(size.depth % 6 == 0);
						for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
							const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
							glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
								static_cast<GLsizei>(mipLevelSize.height), static_cast<GLsizei>(mipLevelSize.depth), 0, imageFormat, imageType, nullptr);
						}
						break;
				}

				object = std::move(textureObject);
				setupSampler();
				break;
			}
			GREM_CASE(const detail::RenderbufferObject& otherRenderbufferObject) {
				detail::RenderbufferObject renderbufferObject = detail::createRenderbufferObject();

				const detail::RenderbufferBindingPreserver renderbufferBindingPreserver{};
				glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbufferObject.get()));

				if (maxMultisampleCount <= 1) {
					glRenderbufferStorage(GL_RENDERBUFFER, static_cast<GLenum>(internalFormat), static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
				} else {
					glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(maxMultisampleCount), static_cast<GLenum>(internalFormat),
						static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
				}

				object = std::move(renderbufferObject);
				break;
			}
			GREM_CASE(Window * otherWindow) {
				unreachable();
			}
		}
	}

	void allocateUncompressed(const TextureImplementation& other) {
		allocateUncompressed(other, other.samplerOptions);
	}
};

} // namespace grem::graphics

#endif
