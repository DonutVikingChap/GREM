// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/resource/Image.hpp>

#include "DeviceImplementation.hpp"
#include "StatePreserver.hpp"
#include "TextureImplementation.hpp"
#include "objects.hpp"
#include "opengl.hpp"

#include <new>     // std::launder
#include <utility> // std::move

namespace grem::graphics {

namespace {

void copyBC1BlockFlippedVertically(uint8_t* output, const uint8_t* input) {
	output[0] = input[0];
	output[1] = input[1];
	output[2] = input[2];
	output[3] = input[3];
	output[4] = input[7];
	output[5] = input[6];
	output[6] = input[5];
	output[7] = input[4];
}

void copyBC3BlockFlippedVertically(uint8_t* output, const uint8_t* input) {
	const uint32_t line01 = static_cast<uint8_t>(input[2]) + 256 * (static_cast<uint8_t>(input[3]) + 256 * static_cast<uint8_t>(input[4]));
	const uint32_t line23 = static_cast<uint8_t>(input[5]) + 256 * (static_cast<uint8_t>(input[6]) + 256 * static_cast<uint8_t>(input[7]));
	const uint32_t line10 = ((line01 & 0x00000FFF) << 12) | ((line01 & 0x00FFF000) >> 12);
	const uint32_t line32 = ((line23 & 0x00000FFF) << 12) | ((line23 & 0x00FFF000) >> 12);
	output[0] = input[0];
	output[1] = input[1];
	output[2] = static_cast<uint8_t>(line32 & 0x000000FF);
	output[3] = static_cast<uint8_t>((line32 & 0x0000FF00) >> 8);
	output[4] = static_cast<uint8_t>((line32 & 0x00FF0000) >> 16);
	output[5] = static_cast<uint8_t>(line10 & 0x000000FF);
	output[6] = static_cast<uint8_t>((line10 & 0x0000FF00) >> 8);
	output[7] = static_cast<uint8_t>((line10 & 0x00FF0000) >> 16);
	output[8] = input[8];
	output[9] = input[9];
	output[10] = input[10];
	output[11] = input[11];
	output[12] = input[15];
	output[13] = input[14];
	output[14] = input[13];
	output[15] = input[12];
}

void copyImageFlippedVertically(byte* output, const resource::ImageView& image) {
	GREM_PROFILE_FUNCTION();

	if (image.getWidth() > 0 && image.getHeight() > 0 && image.getDepth() > 0) {
		const size_t blockStride = resource::Image::getBlockStride(image.getFormat());
		if (blockStride == 0) {
			throw std::invalid_argument{"Invalid image format."};
		}
		const Extent2D blockSize = resource::Image::getBlockSize2D(image.getFormat());
		GREM_ASSERT(blockSize.width != 0);
		GREM_ASSERT(blockSize.height != 0);
		GREM_ASSERT(image.getWidth() % blockSize.width == 0);
		GREM_ASSERT(image.getHeight() % blockSize.height == 0);
		const byte* input = image.data();
		for (uint32_t mipLevel = 0; mipLevel < image.getMipLevelCount(); ++mipLevel) {
			const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(image.getSize2D(), mipLevel);
			const size_t blockCountX = static_cast<size_t>((mipLevelSize.width + blockSize.width - 1) / blockSize.width);
			const size_t blockCountY = static_cast<size_t>((mipLevelSize.height + blockSize.height - 1) / blockSize.height);
			const size_t rowStride = blockStride * static_cast<size_t>(blockCountX);
			const size_t layerStride = rowStride * static_cast<size_t>(blockCountY);
			if (image.getFormat() == resource::ImageFormat::BC1_RGB_UINT_BLOCK) {
				for (uint32_t z = 0; z < image.getDepth(); ++z) {
					const byte* layerInput = input + layerStride * (static_cast<size_t>(z) + 1);
					for (uint32_t blockY = 0; blockY < blockCountY; ++blockY) {
						layerInput -= rowStride;
						const byte* rowInput = layerInput;
						for (uint32_t blockX = 0; blockX < blockCountX; ++blockX) {
							copyBC1BlockFlippedVertically(std::launder(reinterpret_cast<uint8_t*>(output)), std::launder(reinterpret_cast<const uint8_t*>(rowInput)));
							output += blockStride;
							rowInput += blockStride;
						}
					}
				}
			} else if (image.getFormat() == resource::ImageFormat::BC3_RGBA_UINT_BLOCK) {
				for (uint32_t z = 0; z < image.getDepth(); ++z) {
					const byte* layerInput = input + layerStride * (static_cast<size_t>(z) + 1);
					for (uint32_t blockY = 0; blockY < blockCountY; ++blockY) {
						layerInput -= rowStride;
						const byte* rowInput = layerInput;
						for (uint32_t blockX = 0; blockX < blockCountX; ++blockX) {
							copyBC3BlockFlippedVertically(std::launder(reinterpret_cast<uint8_t*>(output)), std::launder(reinterpret_cast<const uint8_t*>(rowInput)));
							output += blockStride;
							rowInput += blockStride;
						}
					}
				}
			} else {
				if (resource::Image::getPixelStride(image.getFormat()) == 0) {
					throw std::invalid_argument{"Invalid image format."};
				}
				for (uint32_t z = 0; z < image.getDepth(); ++z) {
					const byte* layerInput = input + layerStride * (static_cast<size_t>(z) + 1);
					for (size_t blockY = 0; blockY < blockCountY; ++blockY) {
						layerInput -= rowStride;
						memcpy(output, layerInput, rowStride);
						output += rowStride;
					}
				}
			}
			input += layerStride * static_cast<size_t>(image.getDepth());
		}
	}
}

[[nodiscard]] resource::Image getImageFlippedVertically(const resource::ImageView& image) {
	resource::Image result{image.getType(), image.getFormat(), image.getSize3D(), image.getMipLevelCount()};
	copyImageFlippedVertically(result.data(), image);
	return result;
}

[[nodiscard]] SharedPointer<TextureImplementation> createRenderbufferTextureImplementation(Device& device, TextureFormat internalFormat, Extent3D size,
	uint32_t maxMultisampleCount) {
	GREM_PROFILE_FUNCTION();

	detail::RenderbufferObject renderbufferObject = detail::createRenderbufferObject();

	const detail::RenderbufferBindingPreserver renderbufferBindingPreserver{};
	glBindRenderbuffer(GL_RENDERBUFFER, renderbufferObject.get());

	maxMultisampleCount = clamp(maxMultisampleCount, uint32_t{1}, device.getSupportedFeatures().maxSupportedMultisampleCount);
	if (maxMultisampleCount <= 1) {
		glRenderbufferStorage(GL_RENDERBUFFER, static_cast<GLenum>(internalFormat), static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));
	} else {
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(maxMultisampleCount), static_cast<GLenum>(internalFormat), static_cast<GLsizei>(size.width),
			static_cast<GLsizei>(size.height));
	}
	return TextureImplementation::create(device, std::move(renderbufferObject), TextureType::RENDERBUFFER, internalFormat, size, 1, maxMultisampleCount, {});
}

void validateTextureShape(Device& device, TextureType type, Extent3D size, uint32_t mipLevelCount) {
	switch (type) {
		case TextureType::EMPTY: return;
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
			if (const uint32_t max2DTextureResolution = device.getSupportedFeatures().max2DTextureResolution;
				size.width > max2DTextureResolution || size.height > max2DTextureResolution) {
				throw graphics::Error{"Maximum renderbuffer resolution exceeded."};
			}
			return;
		case TextureType::SWAPCHAIN: throw graphics::Error{"Invalid texture type."};
	}
	if (mipLevelCount < 1 || mipLevelCount > resource::Image::getMaxMipLevelCount(Extent2D{size.width, size.height})) {
		throw graphics::Error{"Invalid texture mip level count."};
	}
}

[[nodiscard]] SharedPointer<TextureImplementation> createImageTextureImplementation(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size,
	uint32_t mipLevelCount, const void* pixels, const TextureImageUploadOptions& options, Optional<TextureSamplerOptions> samplerOptions) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(mipLevelCount > 0);

	detail::TextureObject textureObject = detail::createTextureObject();

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

	const GLenum target = TextureImplementation::getTextureTarget(type);

	const uint32_t maxMipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{size.width, size.height});
	const bool generateMipmap = options.generateMipmap && pixels && Texture::isFramebufferCompatibleFormat(internalFormat) && maxMipLevelCount >= 2;
	if (generateMipmap) {
		mipLevelCount = maxMipLevelCount;
	}

	const detail::TextureBindingPreserver textureBindingPreserver{target};
	glBindTexture(target, textureObject.get());
	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(mipLevelCount - 1));

	switch (type) {
		case TextureType::EMPTY: [[fallthrough]];
		case TextureType::RENDERBUFFER: [[fallthrough]];
		case TextureType::SWAPCHAIN: unreachable();
		case TextureType::TEXTURE_2D: {
			GREM_PROFILE_BLOCK_DYNAMIC((pixels) ? "Upload texture data" : "Allocate texture data");
			if (Texture::isCompressedFormat(internalFormat)) {
				const byte* input = static_cast<const byte*>(pixels);
				for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
					const size_t mipLevelStride = (pixels) ? resource::Image::getMipLevelStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
					glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
						static_cast<GLsizei>(mipLevelSize.height), 0, static_cast<GLsizei>(mipLevelStride), input);
					input += mipLevelStride;
				}
			} else {
				const GLenum imageFormat = TextureImplementation::getBaseImageFormat(internalFormat);
				const GLenum imageType = TextureImplementation::getBaseImageType(internalFormat);
				if (generateMipmap) {
					glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height), 0, imageFormat,
						imageType, pixels);
					glGenerateMipmap(GL_TEXTURE_2D);
				} else {
					const byte* input = static_cast<const byte*>(pixels);
					for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
						const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
						const size_t mipLevelStride = (pixels) ? resource::Image::getMipLevelStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
						glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
							static_cast<GLsizei>(mipLevelSize.height), 0, imageFormat, imageType, input);
						input += mipLevelStride;
					}
				}
			}
			break;
		}
		case TextureType::TEXTURE_CUBE: {
			GREM_PROFILE_BLOCK_DYNAMIC((pixels) ? "Upload texture data" : "Allocate texture data");
			if (Texture::isCompressedFormat(internalFormat)) {
				const byte* input = static_cast<const byte*>(pixels);
				for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
					const size_t layerStride = (pixels) ? resource::Image::getLayerStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
					for (uint32_t side = 0; side < 6; ++side) {
						glCompressedTexImage2D(TextureImplementation::getCubemapTarget(side), static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat),
							static_cast<GLsizei>(mipLevelSize.width), static_cast<GLsizei>(mipLevelSize.height), 0, static_cast<GLsizei>(layerStride), input);
						input += layerStride;
					}
				}
			} else {
				const GLenum imageFormat = TextureImplementation::getBaseImageFormat(internalFormat);
				const GLenum imageType = TextureImplementation::getBaseImageType(internalFormat);
				if (generateMipmap) {
					const byte* input = static_cast<const byte*>(pixels);
					const size_t layerStride = (pixels) ? resource::Image::getLayerStride(Texture::getImageFormat(internalFormat), Extent2D{size.width, size.height}) : size_t{0};
					for (uint32_t side = 0; side < 6; ++side) {
						glTexImage2D(TextureImplementation::getCubemapTarget(side), 0, static_cast<GLint>(internalFormat), static_cast<GLsizei>(size.width),
							static_cast<GLsizei>(size.height), 0, imageFormat, imageType, input);
						input += layerStride;
					}
					glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
				} else {
					const byte* input = static_cast<const byte*>(pixels);
					for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
						const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
						const size_t layerStride = (pixels) ? resource::Image::getLayerStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
						for (uint32_t side = 0; side < 6; ++side) {
							glTexImage2D(TextureImplementation::getCubemapTarget(side), static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat),
								static_cast<GLsizei>(mipLevelSize.width), static_cast<GLsizei>(mipLevelSize.height), 0, imageFormat, imageType, input);
							input += layerStride;
						}
					}
				}
			}
			break;
		}
		case TextureType::TEXTURE_2D_ARRAY: [[fallthrough]];
		case TextureType::TEXTURE_CUBE_ARRAY: {
			GREM_PROFILE_BLOCK_DYNAMIC((pixels) ? "Upload texture data" : "Allocate texture data");
			if (Texture::isCompressedFormat(internalFormat)) {
				const byte* input = static_cast<const byte*>(pixels);
				for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
					const size_t mipLevelStride = (pixels) ? resource::Image::getMipLevelStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
					glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
						static_cast<GLsizei>(mipLevelSize.height), static_cast<GLsizei>(mipLevelSize.depth), 0, static_cast<GLsizei>(mipLevelStride), input);
					input += mipLevelStride;
				}
			} else {
				const GLenum imageFormat = TextureImplementation::getBaseImageFormat(internalFormat);
				const GLenum imageType = TextureImplementation::getBaseImageType(internalFormat);
				if (generateMipmap) {
					glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, static_cast<GLint>(internalFormat), static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height),
						static_cast<GLsizei>(size.depth), 0, imageFormat, imageType, pixels);
					glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
				} else {
					const byte* input = static_cast<const byte*>(pixels);
					for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
						const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
						const size_t mipLevelStride = (pixels) ? resource::Image::getMipLevelStride(Texture::getImageFormat(internalFormat), mipLevelSize) : size_t{0};
						glTexImage3D(GL_TEXTURE_2D_ARRAY, static_cast<GLint>(mipLevel), static_cast<GLint>(internalFormat), static_cast<GLsizei>(mipLevelSize.width),
							static_cast<GLsizei>(mipLevelSize.height), static_cast<GLsizei>(mipLevelSize.depth), 0, imageFormat, imageType, input);
						input += mipLevelStride;
					}
				}
			}
			break;
		}
	}

	SharedPointer<TextureImplementation> newTexture = TextureImplementation::create(device, std::move(textureObject), type, internalFormat, size, mipLevelCount, 1, samplerOptions);
	newTexture->setupSampler();
	return newTexture;
}

[[nodiscard]] SharedPointer<TextureImplementation> createTextureImplementation(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size,
	uint32_t mipLevelCount, const void* pixels, const TextureImageUploadOptions& options, Optional<TextureSamplerOptions> samplerOptions) {
	GREM_PROFILE_FUNCTION();

	if (internalFormat == TextureFormat::UNKNOWN) {
		throw graphics::Error{"Invalid internal texture format."};
	}
	validateTextureShape(device, type, size, mipLevelCount);
	if (type == TextureType::RENDERBUFFER) {
		if (pixels) {
			throw graphics::Error{"Cannot initialize renderbuffer texture to an image."};
		}
		if (samplerOptions) {
			throw graphics::Error{"Renderbuffer texture cannot have an associated sampler."};
		}
		return createRenderbufferTextureImplementation(device, internalFormat, size, 1);
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
	resource::Image image{};
	if (pixels) {
		if (Texture::isCompressedFormat(internalFormat)) {
			const FeatureSupport features = device.getSupportedFeatures();
			switch (internalFormat) {
				case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
				case TextureFormat::BC3_RGBA_UNORM_BLOCK:
					if (!features.supportsTextureCompressionS3TC) {
						throw graphics::Error{"Unsupported compressed texture format."};
					}
					break;
				case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
				case TextureFormat::BC3_RGBA_SRGB_BLOCK:
					if (!features.supportsTextureCompressionS3TC_SRGB) {
						throw graphics::Error{"Unsupported compressed texture format."};
					}
					break;
				default: throw graphics::Error{"Unsupported compressed texture format."};
			}
		}
		const resource::ImageFormat format = Texture::getImageFormat(internalFormat);
		const size_t sizeInBytes = resource::Image::getSizeInBytes(format, size, mipLevelCount);
		image = getImageFlippedVertically(resource::ImageView{Texture::getImageType(type), format, size, mipLevelCount, Span{static_cast<const byte*>(pixels), sizeInBytes}});
		if (options.convertToPremultipliedAlpha && Texture::isRGBAColorFormat(internalFormat) && Texture::isRawFormat(internalFormat)) {
			image.transformFromStraightToPremultipliedAlpha(Texture::getTransferFunction(internalFormat));
		}
		pixels = image.data();
	}
	return createImageTextureImplementation(device, type, internalFormat, size, mipLevelCount, pixels, options, samplerOptions);
}

} // namespace

Texture Texture::create(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount, const void* pixels,
	Optional<TextureSamplerOptions> samplerOptions) {
	GREM_PROFILE_FUNCTION();

	return Texture{
		createTextureImplementation(device, type, internalFormat, size, mipLevelCount, pixels, {.convertToPremultipliedAlpha = false, .generateMipmap = false}, samplerOptions)};
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
	return Texture{createRenderbufferTextureImplementation(device, internalFormat, size, maxMultisampleCount)};
}

Texture::Texture(Device& device, const resource::ImageView& image, const TextureImageUploadOptions& options, Optional<TextureSamplerOptions> samplerOptions)
	: Texture() {
	GREM_PROFILE_FUNCTION();

	const TextureType type = getType(image.getType());
	if (type == TextureType::RENDERBUFFER) {
		throw graphics::Error{"Cannot initialize renderbuffer texture to an image."};
	}
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
			validateTextureShape(device, type, image.getSize3D(), image.getMipLevelCount());

			// Since there doesn't seem to be a 100% consistent way to make either OpenGL or Vulkan render framebuffer images upside-down to match the other's behavior,
			// we flip OpenGL images vertically on upload to achieve consistent sampling behavior between the two backends.
			// Unfortunately, this means that in terms of compressed formats, the OpenGL backend is limited to BC1 and BC3, whose compressed blocks can easily be flipped.
			// Upload time also suffers slightly.
			// This compromise favors the Vulkan backend, which should generally be preferred when performance is a high priority anyway.

			const FeatureSupport features = device.getSupportedFeatures();
			TextureFormat internalFormat = TextureFormat::UNKNOWN;
			resource::ImageFormat transcodedImageFormat = resource::ImageFormat::UNKNOWN;
			switch (image.getFormat()) {
				case resource::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
				case resource::ImageFormat::KTX2_UASTC_R_UINT_BLOCK:
					if (features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else {
						internalFormat = TextureFormat::R8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
				case resource::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK:
					if (features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else {
						internalFormat = TextureFormat::R8G8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
				case resource::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK:
					if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC1_RGB_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC1_RGB_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC1_RGB_UINT_BLOCK;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				case resource::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
				case resource::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK:
					if (options.transferFunction == Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC_SRGB) {
						internalFormat = TextureFormat::BC3_RGBA_SRGB_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else if (options.transferFunction != Color::TransferFunction::SRGB && features.supportsTextureCompressionS3TC) {
						internalFormat = TextureFormat::BC3_RGBA_UNORM_BLOCK;
						transcodedImageFormat = resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
					} else {
						internalFormat = (options.transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
						transcodedImageFormat = resource::ImageFormat::R8G8B8A8_UINT;
					}
					break;
				default: unreachable();
			}

			resource::Image transcodedImage{image.getType(), transcodedImageFormat, image.getSize3D(), image.getMipLevelCount()};
			image.transcodeTo(transcodedImage.data(), transcodedImage.getFormat());
			transcodedImage = getImageFlippedVertically(transcodedImage);
			*this = Texture{createImageTextureImplementation(device, type, internalFormat, transcodedImage.getSize3D(), transcodedImage.getMipLevelCount(), transcodedImage.data(),
				options, samplerOptions)};
			break;
		}
	}
}

void Texture::pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset, Region3D sourceRegion) {
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

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot paste onto a non-framebuffer-compatible texture."};
	}

	if (sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0) {
		return;
	}

	const bool uninitialized =
		sourceRegion.size.width == implementation->size.width && sourceRegion.size.height == implementation->size.height && sourceRegion.size.depth == implementation->size.depth;
	GREM_ASSERT(!uninitialized || (destinationOffset.x == 0 && destinationOffset.y == 0 && destinationOffset.z == 0));
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	GREM_ASSERT(implementation->object.is<detail::TextureObject>());
	GREM_ASSERT(destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(implementation->size.width));
	GREM_ASSERT(destinationOffset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(implementation->size.height));
	GREM_ASSERT(destinationOffset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(implementation->size.depth));

	const detail::UnpackRowLengthPreserver unpackRowLengthPreserver{};
	glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(imageSize.width));
	const detail::UnpackImageHeightPreserver unpackImageHeightPreserver{};
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, static_cast<GLint>(imageSize.height));
	const detail::UnpackSkipPixelsPreserver unpackSkipPixelsPreserver{};
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, static_cast<GLint>(sourceRegion.offset.x));
	const detail::UnpackSkipRowsPreserver unpackSkipRowsPreserver{};
	glPixelStorei(GL_UNPACK_SKIP_ROWS, static_cast<GLint>(sourceRegion.offset.y));
	const detail::UnpackSkipImagesPreserver unpackSkipImagesPreserver{};
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, static_cast<GLint>(sourceRegion.offset.z));
	const detail::UnpackAlignmentPreserver unpackAlignmentPreserver{};
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	resource::Image image{};
	if (pixels) {
		const resource::ImageFormat format = getImageFormat(implementation->internalFormat);
		const size_t mipLevelStride = resource::Image::getMipLevelStride(format, imageSize);
		image = getImageFlippedVertically(resource::ImageView{getImageType(implementation->type), format, imageSize, 1, Span{static_cast<const byte*>(pixels), mipLevelStride}});
		pixels = image.data();
	}

	const GLenum target = TextureImplementation::getTextureTarget(implementation->type);
	const GLenum imageFormat = TextureImplementation::getBaseImageFormat(implementation->internalFormat);
	const GLenum imageType = TextureImplementation::getBaseImageType(implementation->internalFormat);

	const detail::TextureBindingPreserver textureBindingPreserver{target};
	glBindTexture(target, static_cast<GLuint>(implementation->object.as<detail::TextureObject>().get()));

	switch (implementation->type) {
		case TextureType::EMPTY: [[fallthrough]];
		case TextureType::TEXTURE_CUBE: [[fallthrough]];
		case TextureType::TEXTURE_CUBE_ARRAY: [[fallthrough]];
		case TextureType::RENDERBUFFER: [[fallthrough]];
		case TextureType::SWAPCHAIN: unreachable();
		case TextureType::TEXTURE_2D:
			GREM_ASSERT(sourceRegion.size.depth == 1);
			GREM_ASSERT(destinationOffset.z == 0);
			glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(destinationOffset.x), static_cast<GLint>(destinationOffset.y), static_cast<GLsizei>(sourceRegion.size.width),
				static_cast<GLsizei>(sourceRegion.size.height), imageFormat, imageType, pixels);
			break;
		case TextureType::TEXTURE_2D_ARRAY:
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, static_cast<GLint>(destinationOffset.x), static_cast<GLint>(destinationOffset.y), static_cast<GLint>(destinationOffset.z),
				static_cast<GLsizei>(sourceRegion.size.width), static_cast<GLsizei>(sourceRegion.size.height), static_cast<GLsizei>(sourceRegion.size.depth), imageFormat,
				imageType, pixels);
			break;
	}

	if (implementation->mipLevelCount > 1) {
		glGenerateMipmap(target);
	}
}

void Texture::pasteTexture(const Texture& texture, Offset3D destinationOffset, Region3D sourceRegion) {
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

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot paste onto a non-framebuffer-compatible texture."};
	}

	if (sourceRegion.size.width == 0 || sourceRegion.size.height == 0 || sourceRegion.size.depth == 0) {
		return;
	}

	if (texture.get()->type != TextureType::TEXTURE_2D && texture.get()->type != TextureType::TEXTURE_2D_ARRAY) {
		throw graphics::Error{"Invalid source texture type."};
	}

	if (implementation->internalFormat != texture.get()->internalFormat) {
		throw graphics::Error{"Mismatched internal texture formats."};
	}

	const GLenum attachment = TextureImplementation::getFramebufferAttachment(implementation->internalFormat);
	const GLenum target = TextureImplementation::getTextureTarget(implementation->type);
	const GLbitfield mask = TextureImplementation::getFramebufferMask(implementation->internalFormat);

	const bool uninitialized =
		sourceRegion.size.width == implementation->size.width && sourceRegion.size.height == implementation->size.height && sourceRegion.size.depth == implementation->size.depth;
	GREM_ASSERT(!uninitialized || (destinationOffset.x == 0 && destinationOffset.y == 0 && destinationOffset.z == 0));
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	GREM_ASSERT(implementation->object.is<detail::TextureObject>());
	GREM_ASSERT(destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width) <= static_cast<int32_t>(implementation->size.width));
	GREM_ASSERT(destinationOffset.y + static_cast<int32_t>(sourceRegion.size.height) <= static_cast<int32_t>(implementation->size.height));
	GREM_ASSERT(destinationOffset.z + static_cast<int32_t>(sourceRegion.size.depth) <= static_cast<int32_t>(implementation->size.depth));

	detail::FramebufferObject readFramebufferObject = detail::createFramebufferObject();
	detail::FramebufferObject drawFramebufferObject = detail::createFramebufferObject();

	const detail::ReadFramebufferBindingPreserver readFramebufferBindingPreserver{};
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferObject.get());

	const detail::DrawFramebufferBindingPreserver drawFramebufferBindingPreserver{};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferObject.get());

	const detail::ScissorTestPreserver scissorTestPreserver{};
	glDisable(GL_SCISSOR_TEST);

	const GLint srcX0 = static_cast<GLint>(sourceRegion.offset.x);
	const GLint srcY0 = static_cast<GLint>(sourceRegion.offset.y);
	const GLint srcX1 = static_cast<GLint>(sourceRegion.offset.x + static_cast<int32_t>(sourceRegion.size.width));
	const GLint srcY1 = static_cast<GLint>(sourceRegion.offset.y + static_cast<int32_t>(sourceRegion.size.height));
	const GLint dstX0 = static_cast<GLint>(destinationOffset.x);
	const GLint dstY0 = static_cast<GLint>(destinationOffset.y);
	const GLint dstX1 = static_cast<GLint>(destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width));
	const GLint dstY1 = static_cast<GLint>(destinationOffset.y + static_cast<int32_t>(sourceRegion.size.height));
	switch (implementation->type) {
		case TextureType::EMPTY: [[fallthrough]];
		case TextureType::TEXTURE_CUBE: [[fallthrough]];
		case TextureType::TEXTURE_CUBE_ARRAY: [[fallthrough]];
		case TextureType::RENDERBUFFER: [[fallthrough]];
		case TextureType::SWAPCHAIN: unreachable();
		case TextureType::TEXTURE_2D:
			GREM_ASSERT(sourceRegion.size.depth == 1);
			GREM_ASSERT(destinationOffset.z == 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GL_TEXTURE_2D, implementation->object.as<detail::TextureObject>().get(), 0);
			switch (texture.get()->type) {
				case TextureType::EMPTY: [[fallthrough]];
				case TextureType::TEXTURE_CUBE: [[fallthrough]];
				case TextureType::TEXTURE_CUBE_ARRAY: [[fallthrough]];
				case TextureType::SWAPCHAIN: unreachable();
				case TextureType::TEXTURE_2D:
					GREM_ASSERT(sourceRegion.offset.z == 0);
					glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture.get()->object.as<detail::TextureObject>().get(), 0);
					break;
				case TextureType::TEXTURE_2D_ARRAY:
					glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, attachment, texture.get()->object.as<detail::TextureObject>().get(), 0,
						static_cast<GLint>(sourceRegion.offset.z));
					break;
				case TextureType::RENDERBUFFER:
					glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, attachment, GL_RENDERBUFFER, texture.get()->object.as<detail::RenderbufferObject>().get());
					break;
			}
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, GL_NEAREST);
			break;
		case TextureType::TEXTURE_2D_ARRAY:
			switch (texture.get()->type) {
				case TextureType::EMPTY: [[fallthrough]];
				case TextureType::TEXTURE_CUBE: [[fallthrough]];
				case TextureType::TEXTURE_CUBE_ARRAY: [[fallthrough]];
				case TextureType::SWAPCHAIN: unreachable();
				case TextureType::TEXTURE_2D:
					GREM_ASSERT(sourceRegion.size.depth == 1);
					GREM_ASSERT(sourceRegion.offset.z == 0);
					glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, implementation->object.as<detail::TextureObject>().get(), 0,
						static_cast<GLint>(destinationOffset.z));
					glFramebufferTexture2D(GL_READ_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture.get()->object.as<detail::TextureObject>().get(), 0);
					glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, GL_NEAREST);
					break;
				case TextureType::TEXTURE_2D_ARRAY:
					for (uint32_t z = 0; z < sourceRegion.size.depth; ++z) {
						glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, implementation->object.as<detail::TextureObject>().get(), 0,
							static_cast<GLint>(destinationOffset.z + static_cast<int32_t>(z)));
						glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, attachment, texture.get()->object.as<detail::TextureObject>().get(), 0,
							static_cast<GLint>(sourceRegion.offset.z + static_cast<int32_t>(z)));
						glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, GL_NEAREST);
					}
					break;
				case TextureType::RENDERBUFFER:
					glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, implementation->object.as<detail::TextureObject>().get(), 0,
						static_cast<GLint>(destinationOffset.z));
					glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, attachment, GL_RENDERBUFFER, texture.get()->object.as<detail::RenderbufferObject>().get());
					glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, GL_NEAREST);
					break;
			}
			break;
	}

	if (implementation->mipLevelCount > 1) {
		const detail::TextureBindingPreserver textureBindingPreserver{target};
		glBindTexture(target, static_cast<GLuint>(implementation->object.as<detail::TextureObject>().get()));
		glGenerateMipmap(target);
	}
}

void Texture::fill(const ClearValues& values) {
	if (!implementation) {
		return;
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot fill a non-framebuffer-compatible texture."};
	}

	const TextureType type = implementation->type;
	const TextureFormat internalFormat = implementation->internalFormat;
	const TextureAspects aspects = getFormatAspects(internalFormat) & values.aspects;
	if (aspects.empty()) {
		return;
	}

	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, true);

	GLuint textureObjectHandle{};
	GREM_MATCH(implementation->object) {
		GREM_CASE(const detail::TextureObject& object) {
			textureObjectHandle = object.get();
			break;
		}
		GREM_CASE(const detail::RenderbufferObject& object) {
			textureObjectHandle = object.get();
			break;
		}
		GREM_CASE(Window * window) {
			unreachable();
		}
	}

	detail::FramebufferObject drawFramebufferObject = detail::createFramebufferObject();

	const detail::DrawFramebufferBindingPreserver drawFramebufferBindingPreserver{};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferObject.get());

	const detail::ScissorTestPreserver scissorTestPreserver{};
	glDisable(GL_SCISSOR_TEST);

	const uint32_t depth = implementation->size.depth;
	const uint32_t mipLevelCount = implementation->mipLevelCount;
	for (uint32_t layer = 0; layer < depth; ++layer) {
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			if (aspects.contains(TextureAspect::COLOR)) {
				TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, type, textureObjectHandle, layer, mipLevel);
			}
			if (aspects.contains(TextureAspect::DEPTH)) {
				TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, type, textureObjectHandle, layer, mipLevel);
			}
			if (aspects.contains(TextureAspect::STENCIL)) {
				TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, type, textureObjectHandle, layer, mipLevel);
			}

			TextureImplementation::clearBoundFramebuffer(internalFormat, TextureImplementation::getAspectBits(aspects), values);
		}
	}
}

void Texture::fill(TextureSubresource subresource, const ClearValues& values) {
	GREM_ASSERT(implementation);
	GREM_ASSERT(subresource.layer < implementation->size.depth);
	GREM_ASSERT(subresource.mipLevel < implementation->mipLevelCount);

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot fill a non-framebuffer-compatible texture."};
	}

	const TextureType type = implementation->type;
	const TextureFormat internalFormat = implementation->internalFormat;
	const TextureAspects aspects = getFormatAspects(internalFormat) & values.aspects & subresource.aspects;
	if (aspects.empty()) {
		return;
	}

	const bool uninitialized = implementation->size.depth == 1 && implementation->mipLevelCount == 1 && subresource.aspects.containsAllOf(aspects);
	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, uninitialized);

	GLuint textureObjectHandle{};
	GREM_MATCH(implementation->object) {
		GREM_CASE(const detail::TextureObject& object) {
			textureObjectHandle = object.get();
			break;
		}
		GREM_CASE(const detail::RenderbufferObject& object) {
			textureObjectHandle = object.get();
			break;
		}
		GREM_CASE(Window * window) {
			unreachable();
		}
	}

	detail::FramebufferObject drawFramebufferObject = detail::createFramebufferObject();

	const detail::DrawFramebufferBindingPreserver drawFramebufferBindingPreserver{};
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebufferObject.get());

	const detail::ScissorTestPreserver scissorTestPreserver{};
	glDisable(GL_SCISSOR_TEST);

	if (aspects.contains(TextureAspect::COLOR)) {
		TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, type, textureObjectHandle, subresource.layer, subresource.mipLevel);
	}
	if (aspects.contains(TextureAspect::DEPTH)) {
		TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, type, textureObjectHandle, subresource.layer, subresource.mipLevel);
	}
	if (aspects.contains(TextureAspect::STENCIL)) {
		TextureImplementation::attachToBoundFramebuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, type, textureObjectHandle, subresource.layer, subresource.mipLevel);
	}

	TextureImplementation::clearBoundFramebuffer(internalFormat, TextureImplementation::getAspectBits(aspects), values);
}

void Texture::generateMipmap() {
	if (!implementation) {
		return;
	}

	if (implementation->type == TextureType::RENDERBUFFER || implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot generate mipmap for a non-framebuffer-compatible texture."};
	}

	DeviceImplementation::ensureExclusiveUncompressedTextureAccess(*this, false);

	GREM_ASSERT(implementation->object.is<detail::TextureObject>());

	const GLenum target = TextureImplementation::getTextureTarget(implementation->type);

	const detail::TextureBindingPreserver textureBindingPreserver{target};
	glBindTexture(target, static_cast<GLuint>(implementation->object.as<detail::TextureObject>().get()));
	glGenerateMipmap(target);
	implementation->mipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{implementation->size.width, implementation->size.height});
}

Texture Texture::copy() const {
	if (!implementation) {
		return {};
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot copy a non-framebuffer-compatible texture."};
	}

	return Texture{TextureImplementation::cloneUncompressed(*implementation)};
}

Texture Texture::copyWithSamplerOptions(Optional<TextureSamplerOptions> newSamplerOptions) const {
	if (!implementation) {
		return {};
	}

	if (implementation->type == TextureType::SWAPCHAIN) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!Texture::isFramebufferCompatibleFormat(implementation->internalFormat)) {
		throw graphics::Error{"Cannot copy a non-framebuffer-compatible texture."};
	}

	return Texture{TextureImplementation::cloneUncompressedWithSamplerOptions(*implementation, newSamplerOptions)};
}

resource::Image Texture::downloadImage(const TextureImageDownloadOptions& downloadOptions) const {
	if (!implementation) {
		return resource::Image{};
	}

	GREM_ASSERT(implementation->object.is<detail::TextureObject>());
	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->aspects.contains(TextureAspect::COLOR));
	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->layer < implementation->size.depth);
	GREM_ASSERT(!downloadOptions.subresource || downloadOptions.subresource->mipLevel < implementation->mipLevelCount);

	if (implementation->type != TextureType::TEXTURE_2D && implementation->type != TextureType::TEXTURE_2D_ARRAY && implementation->type != TextureType::TEXTURE_CUBE &&
		implementation->type != TextureType::TEXTURE_CUBE_ARRAY) {
		throw graphics::Error{"Invalid texture type."};
	}

	if (!getFormatAspects(implementation->internalFormat).contains(TextureAspect::COLOR)) {
		throw graphics::Error{"Invalid texture format."};
	}

	const detail::PackAlignmentPreserver packAlignmentPreserver{};
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	const detail::PackRowLengthPreserver packRowLengthPreserver{};
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	const detail::PackSkipPixelsPreserver packSkipPixelsPreserver{};
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	const detail::PackSkipRowsPreserver packSkipRowsPreserver{};
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
#ifndef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
	const detail::PackImageHeightPreserver packImageHeightPreserver{};
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
	const detail::PackSkipImagesPreserver packSkipImagesPreserver{};
	glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
#endif

	detail::FramebufferObject readFramebufferObject = detail::createFramebufferObject();

	const detail::ReadFramebufferBindingPreserver readFramebufferBindingPreserver{};
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferObject.get());

	const GLenum target = TextureImplementation::getTextureTarget(implementation->type);
	const GLenum imageFormat = TextureImplementation::getBaseImageFormat(implementation->internalFormat);
	const GLenum imageType = TextureImplementation::getBaseImageType(implementation->internalFormat);

	const detail::TextureBindingPreserver textureBindingPreserver{target};
	glBindTexture(target, static_cast<GLuint>(implementation->object.as<detail::TextureObject>().get()));

	resource::Image result{};
	const resource::ImageType outputType = (downloadOptions.subresource) ? resource::ImageType::IMAGE_2D : getImageType(implementation->type);
	const resource::ImageFormat outputFormat = resource::Image::getUncompressedFormat(getImageFormat(implementation->internalFormat));
	if (downloadOptions.subresource) {
		const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{implementation->size.width, implementation->size.height}, downloadOptions.subresource->mipLevel);
		result = resource::Image{outputType, outputFormat, mipLevelSize, 1};
		switch (implementation->type) {
			case TextureType::EMPTY: [[fallthrough]];
			case TextureType::RENDERBUFFER: [[fallthrough]];
			case TextureType::SWAPCHAIN: unreachable();
			case TextureType::TEXTURE_2D:
				glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, implementation->object.as<detail::TextureObject>().get(),
					static_cast<GLint>(downloadOptions.subresource->mipLevel));
				break;
			case TextureType::TEXTURE_2D_ARRAY:
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, implementation->object.as<detail::TextureObject>().get(),
					static_cast<GLint>(downloadOptions.subresource->mipLevel), static_cast<GLint>(downloadOptions.subresource->layer));
				break;
			case TextureType::TEXTURE_CUBE:
				glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureImplementation::getCubemapTarget(downloadOptions.subresource->layer),
					implementation->object.as<detail::TextureObject>().get(), static_cast<GLint>(downloadOptions.subresource->mipLevel));
				break;
			case TextureType::TEXTURE_CUBE_ARRAY:
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, implementation->object.as<detail::TextureObject>().get(),
					static_cast<GLint>(downloadOptions.subresource->mipLevel), static_cast<GLint>(downloadOptions.subresource->layer));
				break;
		}
		glReadPixels(0, 0, static_cast<GLsizei>(mipLevelSize.width), static_cast<GLsizei>(mipLevelSize.height), imageFormat, imageType, result.data());
	} else {
		result = resource::Image{outputType, outputFormat, implementation->size, implementation->mipLevelCount};
		byte* output = result.data();
		for (uint32_t mipLevel = 0; mipLevel < implementation->mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(implementation->size, mipLevel);
			const size_t outputLayerStride = resource::Image::getLayerStride(outputFormat, Extent2D{mipLevelSize.width, mipLevelSize.height});
			for (uint32_t layer = 0; layer < mipLevelSize.depth; ++layer) {
				switch (implementation->type) {
					case TextureType::EMPTY: [[fallthrough]];
					case TextureType::RENDERBUFFER: [[fallthrough]];
					case TextureType::SWAPCHAIN: unreachable();
					case TextureType::TEXTURE_2D:
						glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, implementation->object.as<detail::TextureObject>().get(),
							static_cast<GLint>(mipLevel));
						break;
					case TextureType::TEXTURE_2D_ARRAY:
						glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, implementation->object.as<detail::TextureObject>().get(), static_cast<GLint>(mipLevel),
							static_cast<GLint>(layer));
						break;
					case TextureType::TEXTURE_CUBE:
						glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureImplementation::getCubemapTarget(layer),
							implementation->object.as<detail::TextureObject>().get(), static_cast<GLint>(mipLevel));
						break;
					case TextureType::TEXTURE_CUBE_ARRAY:
						glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, implementation->object.as<detail::TextureObject>().get(), static_cast<GLint>(mipLevel),
							static_cast<GLint>(layer));
						break;
				}
				glReadPixels(0, 0, static_cast<GLsizei>(mipLevelSize.width), static_cast<GLsizei>(mipLevelSize.height), imageFormat, imageType, output);
				output += outputLayerStride;
			}
		}
	}
	if (downloadOptions.convertFromPremultipliedAlpha && isRGBAColorFormat(implementation->internalFormat) && isRawFormat(implementation->internalFormat)) {
		result.transformFromPremultipliedToStraightAlpha(getTransferFunction(implementation->internalFormat));
	}
	return getImageFlippedVertically(result);
}

TextureType Texture::getType() const noexcept {
	return (implementation) ? implementation->type : TextureType::EMPTY;
}

TextureFormat Texture::getInternalFormat() const noexcept {
	return (implementation) ? implementation->internalFormat : TextureFormat::UNKNOWN;
}

Extent3D Texture::getSize3D() const noexcept {
	if (!implementation) {
		return Extent3D{.width = 0, .height = 0, .depth = 0};
	}
	if (implementation->type == TextureType::SWAPCHAIN) {
		return implementation->object.get<Window*>()->getDrawableSize();
	}
	return implementation->size;
}

uint32_t Texture::getMipLevelCount() const noexcept {
	return (implementation) ? implementation->mipLevelCount : 0;
}

uint32_t Texture::getMaxMultisampleCount() const noexcept {
	if (!implementation) {
		return 0;
	}
	if (implementation->type == TextureType::SWAPCHAIN) {
		return implementation->object.get<Window*>()->getMultisampleCount();
	}
	return implementation->maxMultisampleCount;
}

Optional<TextureSamplerOptions> Texture::getSamplerOptions() const noexcept {
	return (implementation) ? implementation->samplerOptions : Optional<TextureSamplerOptions>{};
}

} // namespace grem::graphics
