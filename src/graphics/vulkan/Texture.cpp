// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
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
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/resource/Image.hpp>

#include "DeviceImplementation.hpp"
#include "TextureImplementation.hpp"
#include "TextureResources.hpp"

#include <new>     // std::launder
#include <utility> // std::move

namespace grem::graphics {

namespace {

[[nodiscard]] SharedPointer<TextureImplementation> createTextureImplementation(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size,
	uint32_t mipLevelCount, const VkComponentMapping& components, const void* pixels, const TextureImageUploadOptions& options, Optional<TextureSamplerOptions> samplerOptions) {
	GREM_PROFILE_FUNCTION();

	if (internalFormat == TextureFormat::UNKNOWN) {
		throw graphics::Error{"Invalid internal texture format."};
	}
	switch (type) {
		case TextureType::EMPTY: return {};
		case TextureType::TEXTURE_2D:
			if (size.depth != 1) {
				throw graphics::Error{"Invalid 2D texture depth."};
			}
			if (const uint32_t max2DTextureResolution = device.getSupportedFeatures().max2DTextureResolution;
				size.width > max2DTextureResolution || size.height > max2DTextureResolution) {
				throw graphics::Error{"Maximum 2D texture resolution exceeded."};
			}
			break;
		case TextureType::TEXTURE_2D_ARRAY: break;
		case TextureType::TEXTURE_CUBE:
			if (size.width != size.height) {
				throw graphics::Error{"Invalid cube texture size."};
			}
			if (size.depth != 6) {
				throw graphics::Error{"Invalid cube texture depth."};
			}
			if (size.width > device.getSupportedFeatures().maxCubeTextureResolution) {
				throw graphics::Error{"Maximum cube texture resolution exceeded."};
			}
			break;
		case TextureType::TEXTURE_CUBE_ARRAY:
			if (size.width != size.height) {
				throw graphics::Error{"Invalid cube array texture size."};
			}
			if (size.depth % 6 != 0) {
				throw graphics::Error{"Invalid cube array texture depth."};
			}
			if (size.width > device.getSupportedFeatures().maxCubeTextureResolution) {
				throw graphics::Error{"Maximum cube array texture resolution exceeded."};
			}
			if (size.depth > device.getSupportedFeatures().maxTextureLayerCount) {
				throw graphics::Error{"Maximum cube array texture depth exceeded."};
			}
			break;
		case TextureType::RENDERBUFFER:
			if (size.depth != 1) {
				throw graphics::Error{"Invalid renderbuffer texture depth."};
			}
			if (mipLevelCount != 1) {
				throw graphics::Error{"Invalid renderbuffer mip level count."};
			}
			if (pixels) {
				throw graphics::Error{"Cannot initialize renderbuffer texture to an image."};
			}
			if (samplerOptions) {
				throw graphics::Error{"Renderbuffer texture cannot have an associated sampler."};
			}
			if (!Texture::isFramebufferCompatibleFormat(internalFormat)) {
				throw graphics::Error{"Framebuffer-incompatible internal texture format specified for renderbuffer."};
			}
			if (const uint32_t max2DTextureResolution = device.getSupportedFeatures().max2DTextureResolution;
				size.width > max2DTextureResolution || size.height > max2DTextureResolution) {
				throw graphics::Error{"Maximum renderbuffer resolution exceeded."};
			}
			break;
		case TextureType::SWAPCHAIN: throw graphics::Error{"Invalid texture type."};
	}

	if (samplerOptions) {
		if (samplerOptions->depthComparisonMode) {
			if (!Texture::getFormatAspects(internalFormat).contains(TextureAspect::DEPTH)) {
				throw graphics::Error{"Non-depth textures cannot be sampled with a depth comparison mode."};
			}
		} else {
			if (Texture::getFormatAspects(internalFormat).contains(TextureAspect::DEPTH)) {
				throw graphics::Error{"Depth textures can only be sampled with a depth comparison mode."};
			}
		}
	}

	const uint32_t maxMipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{size.width, size.height});
	if (mipLevelCount < 1 || mipLevelCount > maxMipLevelCount) {
		throw graphics::Error{"Invalid texture mip level count."};
	}

	const bool generateMipmap = options.generateMipmap && pixels && Texture::isFramebufferCompatibleFormat(internalFormat) && maxMipLevelCount >= 2;
	const uint32_t storedMipLevelCount = (generateMipmap) ? maxMipLevelCount : mipLevelCount;

	SharedPointer<TextureImplementation> result = TextureImplementation::create(device, type, TextureImplementation::translateInternalFormat(internalFormat), size,
		storedMipLevelCount, 1, VK_SAMPLE_COUNT_1_BIT, components, samplerOptions);
	if (pixels) {
		resource::Image image{};
		if (options.convertToPremultipliedAlpha && Texture::isRGBAColorFormat(internalFormat) && Texture::isRawFormat(internalFormat)) {
			const resource::ImageFormat format = Texture::getImageFormat(internalFormat);
			image = resource::Image{Texture::getImageType(type), format, size, mipLevelCount,
				Span{std::launder(reinterpret_cast<const byte*>(pixels)), resource::Image::getSizeInBytes(format, size, mipLevelCount)}};
			image.transformFromStraightToPremultipliedAlpha(Texture::getTransferFunction(internalFormat));
			pixels = image.data();
		}
		GREM_PROFILE_BLOCK("Upload texture data");
		if (generateMipmap) {
			result->uploadFirstMipLevelAndGenerateMipmap(pixels);
		} else {
			result->uploadAllMipLevels(pixels);
		}
	}
	return result;
}

} // namespace

namespace detail {

void TextureResources::reset() noexcept {
	const VkDevice deviceHandle = device.get()->logicalDevice.get();
	const VmaAllocator allocator = device.get()->allocator.get();
	vkDestroySampler(deviceHandle, sampler, nullptr);
	vkDestroyImageView(deviceHandle, samplerImageView, nullptr);
	if (imageAllocation) {
		vmaDestroyImage(allocator, image, imageAllocation);
	}
	image = VK_NULL_HANDLE;
	imageAllocation = VK_NULL_HANDLE;
	samplerImageView = VK_NULL_HANDLE;
	sampler = VK_NULL_HANDLE;
}

} // namespace detail

Texture Texture::create(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount, const void* pixels,
	Optional<TextureSamplerOptions> samplerOptions) {
	GREM_PROFILE_FUNCTION();

	return Texture{createTextureImplementation(device, type, internalFormat, size, mipLevelCount,
		VkComponentMapping{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		pixels, {.convertToPremultipliedAlpha = false, .generateMipmap = false}, samplerOptions)};
}

Texture Texture::createRenderbuffer(Device& device, TextureFormat internalFormat, Extent2D size, uint32_t maxMultisampleCount, const UndefinedClearValues&) {
	GREM_PROFILE_FUNCTION();

	if (internalFormat == TextureFormat::UNKNOWN) {
		throw graphics::Error{"Invalid internal texture format."};
	}
	if (!Texture::isFramebufferCompatibleFormat(internalFormat)) {
		throw graphics::Error{"Framebuffer-incompatible internal texture format specified for renderbuffer."};
	}
	if (const uint32_t max2DTextureResolution = device.getSupportedFeatures().max2DTextureResolution; size.width > max2DTextureResolution || size.height > max2DTextureResolution) {
		throw graphics::Error{"Maximum renderbuffer resolution exceeded."};
	}
	maxMultisampleCount = clamp(maxMultisampleCount, uint32_t{1}, device.getSupportedFeatures().maxSupportedMultisampleCount);
	return Texture{TextureImplementation::create(device, TextureType::RENDERBUFFER, TextureImplementation::translateInternalFormat(internalFormat), size, 1, maxMultisampleCount,
		TextureImplementation::getSampleCount(maxMultisampleCount),
		VkComponentMapping{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		{})};
}

Texture::Texture(Device& device, const resource::ImageView& image, const TextureImageUploadOptions& options, Optional<TextureSamplerOptions> samplerOptions)
	: Texture() {
	GREM_PROFILE_FUNCTION();

	const TextureType type = getType(image.getType());
	switch (image.getFormat()) {
		case resource::ImageFormat::UNKNOWN: break;
		case resource::ImageFormat::R8_UINT: [[fallthrough]];
		case resource::ImageFormat::R16_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R32_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R8G8_UINT: [[fallthrough]];
		case resource::ImageFormat::R16G16_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R32G32_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R8G8B8_UINT: [[fallthrough]];
		case resource::ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
		case resource::ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
		case resource::ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
		case resource::ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
		case resource::ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
		case resource::ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
		case resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK:
			*this = Texture{createTextureImplementation(device, type, getInternalFormat(image.getFormat(), options.transferFunction), image.getSize3D(), image.getMipLevelCount(),
				VkComponentMapping{
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				image.data(), options, samplerOptions)};
			break;
		case resource::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
		case resource::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: {
			const FeatureSupport features = device.getSupportedFeatures();

			TextureFormat internalFormat = TextureFormat::UNKNOWN;
			resource::ImageFormat transcodedImageFormat = resource::ImageFormat::UNKNOWN;
			VkComponentMapping components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			};
			switch (image.getFormat()) {
				case resource::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK:
					if (features.supportsTextureCompressionETC2) {
						internalFormat = TextureFormat::EAC_R11_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::EAC_R11_UINT_BLOCK;
					} else if (features.supportsTextureCompressionRGTC) {
						internalFormat = TextureFormat::BC4_R_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC4_R_UINT_BLOCK;
					} else if (features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
						components.g = VK_COMPONENT_SWIZZLE_ZERO;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.g = VK_COMPONENT_SWIZZLE_ZERO;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else {
						internalFormat = TextureFormat::R8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_UASTC_R_UINT_BLOCK:
					if (features.supportsTextureCompressionASTC_LDR) {
						internalFormat = TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
						components.g = VK_COMPONENT_SWIZZLE_ZERO;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionETC2) {
						internalFormat = TextureFormat::EAC_R11_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::EAC_R11_UINT_BLOCK;
					} else if (features.supportsTextureCompressionRGTC) {
						internalFormat = TextureFormat::BC4_R_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC4_R_UINT_BLOCK;
					} else if (features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
						components.g = VK_COMPONENT_SWIZZLE_ZERO;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.g = VK_COMPONENT_SWIZZLE_ZERO;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else {
						internalFormat = TextureFormat::R8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK:
					if (features.supportsTextureCompressionETC2) {
						internalFormat = TextureFormat::EAC_R11G11_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::EAC_R11G11_UINT_BLOCK;
					} else if (features.supportsTextureCompressionRGTC) {
						internalFormat = TextureFormat::BC5_RG_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC5_RG_UINT_BLOCK;
					} else {
						internalFormat = TextureFormat::R8G8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK:
					if (features.supportsTextureCompressionASTC_LDR) {
						internalFormat = TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
						components.b = VK_COMPONENT_SWIZZLE_ZERO;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionETC2) {
						internalFormat = TextureFormat::EAC_R11G11_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::EAC_R11G11_UINT_BLOCK;
					} else if (features.supportsTextureCompressionRGTC) {
						internalFormat = TextureFormat::BC5_RG_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC5_RG_UINT_BLOCK;
					} else {
						internalFormat = TextureFormat::R8G8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK:
					if (features.supportsTextureCompressionETC2) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK;
					} else if (features.supportsTextureCompressionBPTC) {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC7_RGBA_SRGB_BLOCK : TextureFormat::BC7_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC1_RGB_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC_SRGB &&
							   image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() &&
							   isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK:
					if (features.supportsTextureCompressionASTC_LDR) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK : TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionBPTC) {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC7_RGBA_SRGB_BLOCK : TextureFormat::BC7_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (features.supportsTextureCompressionETC2) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC1_RGB_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC_SRGB &&
							   image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() &&
							   isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
						components.a = VK_COMPONENT_SWIZZLE_ONE;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK:
					if (features.supportsTextureCompressionBPTC) {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC7_RGBA_SRGB_BLOCK : TextureFormat::BC7_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
					} else if (features.supportsTextureCompressionETC2) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC3_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC3_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC_SRGB &&
							   image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() &&
							   isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK:
					if (features.supportsTextureCompressionASTC_LDR) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK : TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
					} else if (features.supportsTextureCompressionBPTC) {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC7_RGBA_SRGB_BLOCK : TextureFormat::BC7_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
					} else if (features.supportsTextureCompressionETC2) {
						internalFormat =
							(options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC3_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC3_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC_SRGB &&
							   image.getWidth() == image.getHeight() && isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionPVRTC && image.getWidth() == image.getHeight() &&
							   isPowerOf2(image.getWidth())) {
						internalFormat = TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				default: unreachable();
			}

			resource::Image transcodedImage{image.getType(), transcodedImageFormat, image.getSize3D(), image.getMipLevelCount()};
			image.transcodeTo(transcodedImage.data(), transcodedImage.getFormat());
			*this = Texture{createTextureImplementation(device, type, internalFormat, transcodedImage.getSize3D(), VK_SAMPLE_COUNT_1_BIT, components, transcodedImage.data(),
				options, samplerOptions)};
			break;
		}
	}
}

void Texture::pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset, Region3D sourceRegion) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(pixels || sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0);
	GREM_ASSERT(sourceRegion.offset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(imageSize.width));
	GREM_ASSERT(sourceRegion.offset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(imageSize.height));
	GREM_ASSERT(sourceRegion.offset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(imageSize.depth));

	if (!implementation) {
		throw graphics::Error{"Cannot paste onto an empty texture."};
	}

	if (implementation->type != TextureType::TEXTURE_2D && implementation->type != TextureType::TEXTURE_2D_ARRAY) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot paste onto a non-framebuffer-compatible texture."};
	}

	if (sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0) {
		return;
	}

	const bool uninitialized =
		sourceRegion.size.width == implementation->size.width && sourceRegion.size.height == implementation->size.height && sourceRegion.size.depth == implementation->size.depth;
	GREM_ASSERT(!uninitialized || (destinationOffset.x == 0 && destinationOffset.y == 0 && destinationOffset.z == 0));
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	GREM_ASSERT(destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(implementation->size.width));
	GREM_ASSERT(destinationOffset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(implementation->size.height));
	GREM_ASSERT(destinationOffset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(implementation->size.depth));
	implementation->pasteImageOntoFirstMipLevelAndGenerateMipmap(imageSize, pixels, destinationOffset, sourceRegion);
}

void Texture::pasteTexture(const Texture& texture, Offset3D destinationOffset, Region3D sourceRegion) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(texture || sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0);
	GREM_ASSERT(sourceRegion.offset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(texture.getWidth()));
	GREM_ASSERT(sourceRegion.offset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(texture.getHeight()));
	GREM_ASSERT(sourceRegion.offset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(texture.getDepth()));

	if (!implementation) {
		throw graphics::Error{"Cannot paste onto an empty texture."};
	}

	if (implementation->type != TextureType::TEXTURE_2D && implementation->type != TextureType::TEXTURE_2D_ARRAY) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot paste onto a non-framebuffer-compatible texture."};
	}

	if (sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0) {
		return;
	}

	if (texture.get()->type != TextureType::TEXTURE_2D && texture.get()->type != TextureType::TEXTURE_2D_ARRAY) {
		throw graphics::Error{"Invalid source texture type."};
	}

	if (implementation->format != texture.get()->format) {
		throw graphics::Error{"Mismatched internal texture formats."};
	}

	const bool uninitialized =
		sourceRegion.size.width == implementation->size.width && sourceRegion.size.height == implementation->size.height && sourceRegion.size.depth == implementation->size.depth;
	GREM_ASSERT(!uninitialized || (destinationOffset.x == 0 && destinationOffset.y == 0 && destinationOffset.z == 0));
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	GREM_ASSERT(destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(implementation->size.width));
	GREM_ASSERT(destinationOffset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(implementation->size.height));
	GREM_ASSERT(destinationOffset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(implementation->size.depth));
	implementation->pasteTextureOntoFirstMipLevelAndGenerateMipmap(*texture.get(), destinationOffset, sourceRegion);
}

void Texture::fill(const ClearValues& values) {
	GREM_PROFILE_FUNCTION();

	if (!implementation) {
		return;
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot fill a non-framebuffer-compatible texture."};
	}

	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, true);

	implementation->fillTexture(values);
}

void Texture::fill(TextureSubresource subresource, const ClearValues& values) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(implementation);
	GREM_ASSERT(subresource.layer < implementation->size.depth);
	GREM_ASSERT(subresource.mipLevel < implementation->mipLevelCount);

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot fill a non-framebuffer-compatible texture."};
	}

	const bool uninitialized = implementation->size.depth == 1 && implementation->mipLevelCount == 1 &&
	                           subresource.aspects.containsAllOf(getFormatAspects(TextureImplementation::getInternalFormat(implementation->format)) & values.aspects);
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	implementation->fillTextureSubresource(subresource, values);
}

void Texture::generateMipmap() {
	GREM_PROFILE_FUNCTION();

	if (!implementation) {
		return;
	}

	if (implementation->type == TextureType::RENDERBUFFER || implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot generate mipmap for a non-framebuffer-compatible texture."};
	}

	const uint32_t maxMipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{implementation->size.width, implementation->size.height});
	if (implementation->mipLevelCount != maxMipLevelCount) {
		SharedPointer<TextureImplementation> newTexture = TextureImplementation::create(implementation->object.get<detail::TextureResources>().device, implementation->type,
			implementation->format, implementation->size, maxMipLevelCount, implementation->maxMultisampleCount, implementation->sampleCount, implementation->components,
			implementation->samplerOptions);
		newTexture->assignFirstMipLevelFromOtherUncompressedTextureOfSameShapeAndGenerateMipmap(*implementation);
		implementation = std::move(newTexture);
	} else if (implementation->mipLevelCount > 1) {
		DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, false);
		implementation->transitionToTransferDestinationLayout();
		implementation->generateMipChainAndTransitionFromTransferDestinationToPreferredLayout();
	}
}

Texture Texture::copy() const {
	GREM_PROFILE_FUNCTION();

	if (!implementation) {
		return {};
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot copy a non-framebuffer-compatible texture."};
	}

	return Texture{TextureImplementation::cloneUncompressed(*implementation)};
}

Texture Texture::copyWithSamplerOptions(Optional<TextureSamplerOptions> newSamplerOptions) const {
	GREM_PROFILE_FUNCTION();

	if (!implementation) {
		return {};
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(TextureImplementation::getInternalFormat(implementation->format))) {
		throw graphics::Error{"Cannot copy a non-framebuffer-compatible texture."};
	}

	return Texture{TextureImplementation::cloneUncompressedWithSamplerOptions(*implementation, newSamplerOptions)};
}

resource::Image Texture::downloadImage(const TextureImageDownloadOptions& downloadOptions) const {
	GREM_PROFILE_FUNCTION();

	if (!implementation) {
		return resource::Image{};
	}

	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->aspects.contains(TextureAspect::COLOR));
	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->layer < implementation->size.depth);
	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->mipLevel < implementation->mipLevelCount);

	if (implementation->type != TextureType::TEXTURE_2D && implementation->type != TextureType::TEXTURE_2D_ARRAY && implementation->type != TextureType::TEXTURE_CUBE &&
		implementation->type != TextureType::TEXTURE_CUBE_ARRAY) {
		throw graphics::Error{"Invalid texture type."};
	}

	const TextureFormat internalFormat = TextureImplementation::getInternalFormat(implementation->format);
	if (!getFormatAspects(internalFormat).contains(TextureAspect::COLOR)) {
		throw graphics::Error{"Invalid texture format."};
	}

	resource::Image result{};
	const resource::ImageType outputType = (downloadOptions.subresource) ? resource::ImageType::IMAGE_2D : getImageType(implementation->type);
	const resource::ImageFormat outputFormat = getImageFormat(internalFormat);
	if (downloadOptions.subresource) {
		const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{implementation->size.width, implementation->size.height}, downloadOptions.subresource->mipLevel);
		result = resource::Image{outputType, outputFormat, mipLevelSize, 1};
		implementation->downloadColor(downloadOptions.subresource->layer, downloadOptions.subresource->mipLevel, result.data());
	} else {
		result = resource::Image{outputType, outputFormat, implementation->size, implementation->mipLevelCount};
		byte* output = result.data();
		for (uint32_t mipLevel = 0; mipLevel < implementation->mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(implementation->size, mipLevel);
			const size_t outputLayerStride = resource::Image::getLayerStride(outputFormat, Extent2D{mipLevelSize.width, mipLevelSize.height});
			for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
				implementation->downloadColor(layer, mipLevel, output);
				output += outputLayerStride;
			}
		}
	}
	if (downloadOptions.convertFromPremultipliedAlpha && isRGBAColorFormat(internalFormat) && isRawFormat(internalFormat)) {
		result.transformFromPremultipliedToStraightAlpha(getTransferFunction(internalFormat));
	}
	return result;
}

TextureType Texture::getType() const noexcept {
	return (implementation) ? implementation->type : TextureType::EMPTY;
}

TextureFormat Texture::getInternalFormat() const noexcept {
	return (implementation) ? TextureImplementation::getInternalFormat(implementation->format) : TextureFormat::UNKNOWN;
}

Extent3D Texture::getSize3D() const noexcept {
	return (implementation) ? implementation->size : Extent3D{.width = 0, .height = 0, .depth = 0};
}

uint32_t Texture::getMipLevelCount() const noexcept {
	return (implementation) ? implementation->mipLevelCount : 0;
}

uint32_t Texture::getMaxMultisampleCount() const noexcept {
	return (implementation) ? implementation->maxMultisampleCount : 0;
}

Optional<TextureSamplerOptions> Texture::getSamplerOptions() const noexcept {
	return (implementation) ? implementation->samplerOptions : Optional<TextureSamplerOptions>{};
}

} // namespace grem::graphics
