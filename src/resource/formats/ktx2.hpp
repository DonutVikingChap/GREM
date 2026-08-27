// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_FORMATS_KTX2_HPP
#define GREM_RESOURCE_FORMATS_KTX2_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>

#include "dfd.hpp"

#include <basisu_transcoder.h> // basist::...
#include <stdexcept>           // std::length_error
#include <utility>             // std::move

namespace grem::resource {

inline constexpr Array<char, 12> KTX2_IDENTIFIER{'\xAB', '\x4B', 'T', 'X', ' ', '2', '0', '\xBB', '\r', '\n', '\x1A', '\n'};

struct KTX2Header {
	Array<char, 12> identifier;
	uint32_t vkFormat;
	uint32_t typeSize;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t layerCount;
	uint32_t faceCount;
	uint32_t levelCount;
	uint32_t supercompressionScheme;
	uint32_t dfdByteOffset;
	uint32_t dfdByteLength;
	uint32_t kvdByteOffset;
	uint32_t kvdByteLength;
	uint64_t sgdByteOffset;
	uint64_t sgdByteLength;
};

struct KTX2LevelIndexEntry {
	uint64_t byteOffset;
	uint64_t byteLength;
	uint64_t uncompressedByteLength;
};

inline void ensureBasisUniversalTranscoderInitialized() {
	static const struct BasisUniversalTranscoderInitializer {
		[[nodiscard]] BasisUniversalTranscoderInitializer() {
			basist::basisu_transcoder_init();
		}
	} initializer{};
}

[[nodiscard]] inline Optional<ImageFormat> getKTX2ImageFormat(const basist::ktx2_transcoder& transcoder) noexcept {
	switch (transcoder.get_basis_tex_format()) {
		case basist::basis_tex_format::cETC1S:
			if (transcoder.get_dfd_total_samples() == 1) {
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_ETC1S_RRR) {
					return ImageFormat::KTX2_ETC1S_R_UINT_BLOCK;
				}
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_ETC1S_RGB) {
					return ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK;
				}
			} else if (transcoder.get_dfd_total_samples() == 2) {
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_ETC1S_RRR && transcoder.get_dfd_channel_id1() == basist::KTX2_DF_CHANNEL_ETC1S_GGG) {
					return ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK;
				}
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_ETC1S_RGB && transcoder.get_dfd_channel_id1() == basist::KTX2_DF_CHANNEL_ETC1S_AAA) {
					return ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK;
				}
			}
			break;
		case basist::basis_tex_format::cUASTC_LDR_4x4:
			if (transcoder.get_dfd_total_samples() == 1) {
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_UASTC_RRR) {
					return ImageFormat::KTX2_UASTC_R_UINT_BLOCK;
				}
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_UASTC_RG) {
					return ImageFormat::KTX2_UASTC_RG_UINT_BLOCK;
				}
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_UASTC_RGB) {
					return ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK;
				}
				if (transcoder.get_dfd_channel_id0() == basist::KTX2_DF_CHANNEL_UASTC_RGBA) {
					return ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK;
				}
			}
			break;
		default: break;
	}
	return {};
}

[[nodiscard]] inline Optional<ImageFormat> getKTX2ImageFormat(uint32_t vkFormat) {
	switch (vkFormat) {
		case 9: return ImageFormat::R8_UINT;                              // VK_FORMAT_R8_UNORM
		case 76: return ImageFormat::R16_FLOAT;                           // VK_FORMAT_R16_SFLOAT
		case 100: return ImageFormat::R32_FLOAT;                          // VK_FORMAT_R32_SFLOAT
		case 16: return ImageFormat::R8G8_UINT;                           // VK_FORMAT_R8G8_UNORM
		case 83: return ImageFormat::R16G16_FLOAT;                        // VK_FORMAT_R16G16_SFLOAT
		case 103: return ImageFormat::R32G32_FLOAT;                       // VK_FORMAT_R32G32_SFLOAT
		case 29: return ImageFormat::R8G8B8_UINT;                         // VK_FORMAT_R8G8B8_SRGB
		case 90: return ImageFormat::R16G16B16_FLOAT;                     // VK_FORMAT_R16G16B16_SFLOAT
		case 106: return ImageFormat::R32G32B32_FLOAT;                    // VK_FORMAT_R32G32B32_SFLOAT
		case 43: return ImageFormat::R8G8B8A8_UINT;                       // VK_FORMAT_R8G8B8A8_SRGB
		case 97: return ImageFormat::R16G16B16A16_FLOAT;                  // VK_FORMAT_R16G16B16A16_SFLOAT
		case 109: return ImageFormat::R32G32B32A32_FLOAT;                 // VK_FORMAT_R32G32B32A32_SFLOAT
		case 4: return ImageFormat::R5G6B5_UINT_PACK16;                   // VK_FORMAT_R5G6B5_UNORM_PACK16;
		case 8: return ImageFormat::A1R5G5B5_UINT_PACK16;                 // VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		case 122: return ImageFormat::B10G11R11_UFLOAT_PACK32;            // VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case 64: return ImageFormat::A2B10G10R10_UINT_PACK32;             // VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case 158: return ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;           // VK_FORMAT_ASTC_4x4_SRGB_BLOCK
		case 132: return ImageFormat::BC1_RGB_UINT_BLOCK;                 // VK_FORMAT_BC1_RGB_SRGB_BLOCK
		case 138: return ImageFormat::BC3_RGBA_UINT_BLOCK;                // VK_FORMAT_BC3_SRGB_BLOCK
		case 139: return ImageFormat::BC4_R_UINT_BLOCK;                   // VK_FORMAT_BC4_UNORM_BLOCK
		case 141: return ImageFormat::BC5_RG_UINT_BLOCK;                  // VK_FORMAT_BC5_UNORM_BLOCK
		case 143: return ImageFormat::BC6H_RGB_UFLOAT_BLOCK;              // VK_FORMAT_BC6H_UFLOAT_BLOCK
		case 144: return ImageFormat::BC6H_RGB_FLOAT_BLOCK;               // VK_FORMAT_BC6H_SFLOAT_BLOCK
		case 146: return ImageFormat::BC7_RGBA_UINT_BLOCK;                // VK_FORMAT_BC7_SRGB_BLOCK
		case 148: return ImageFormat::ETC2_R8G8B8_UINT_BLOCK;             // VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
		case 152: return ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;           // VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
		case 153: return ImageFormat::EAC_R11_UINT_BLOCK;                 // VK_FORMAT_EAC_R11_UNORM_BLOCK
		case 155: return ImageFormat::EAC_R11G11_UINT_BLOCK;              // VK_FORMAT_EAC_R11G11_UNORM_BLOCK
		case 1000054005: return ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK; // VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG
		default: break;
	}
	return {};
}

[[nodiscard]] inline Image loadKTX2Image(Allocation<byte> fileContents, const ImageOptions& options) {
	GREM_PROFILE_BLOCK("Load KTX2 image");

	if (fileContents.size() <= sizeof(KTX2Header)) {
		throw std::length_error{"Invalid KTX2 file size."};
	}

	KTX2Header header;
	memcpy(&header, fileContents.data(), sizeof(KTX2Header));

	if (header.identifier != KTX2_IDENTIFIER) {
		throw resource::Error{"Invalid KTX2 identifier."};
	}

	const uint32_t vkFormat = convertLittleEndianToHostEndian(header.vkFormat);
	const uint32_t supercompressionScheme = convertLittleEndianToHostEndian(header.supercompressionScheme);
	if (vkFormat == 0 || vkFormat == 1000066000 || supercompressionScheme != 0) {
		if (fileContents.size() > Limits<uint32_t>::MAX) {
			throw std::length_error{"Maximum KTX2 file size exceeded."};
		}

		ensureBasisUniversalTranscoderInitialized();
		basist::ktx2_transcoder transcoder{};
		if (!transcoder.init(fileContents.data(), static_cast<uint32_t>(fileContents.size()))) {
			throw resource::Error{"Invalid KTX2 file."};
		}

		const uint32_t layerCount = transcoder.get_layers();
		const uint32_t faceCount = transcoder.get_faces();
		Extent3D size3D{
			.width = transcoder.get_width(),
			.height = transcoder.get_height(),
			.depth = max(layerCount, uint32_t{1}),
		};
		ImageType type{};
		if (faceCount == 6) {
			type = (layerCount == 0) ? ImageType::IMAGE_CUBE : ImageType::IMAGE_CUBE_ARRAY;
			if (Limits<uint32_t>::MAX / size3D.depth < 6) {
				throw std::length_error{"Image size overflow."};
			}
			size3D.depth *= 6;
		} else if (faceCount != 1) {
			throw resource::Error{"Invalid KTX2 face count."};
		} else {
			type = (layerCount == 0) ? ImageType::IMAGE_2D : ImageType::IMAGE_2D_ARRAY;
		}
		const uint32_t mipLevelCount = min(transcoder.get_levels(), Image::getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));
		const Optional<ImageFormat> format = getKTX2ImageFormat(transcoder);
		if (!format) {
			throw resource::Error{"Unsupported KTX2 image format."};
		}
		if (options.requiredType && *options.requiredType != type) {
			throw resource::Error{"Unexpected image type."};
		}
		if (options.requiredFormat && *options.requiredFormat != *format) {
			throw resource::Error{"Unexpected image format."};
		}
		if (size3D.width > options.maxImageDimensions.width || size3D.height > options.maxImageDimensions.height || size3D.depth > options.maxImageDimensions.depth) {
			throw std::length_error{"Maximum image dimensions exceeded."};
		}
		if (Image::getSizeInBytes(*format, size3D, mipLevelCount) > options.maxImageSizeInBytes) {
			throw std::length_error{"Maximum image size exceeded."};
		}
		return Image{type, *format, size3D, mipLevelCount, std::move(fileContents)};
	}

	if (convertLittleEndianToHostEndian(header.pixelDepth) != 0) {
		throw resource::Error{"Unsupported KTX2 pixel depth."};
	}

	const uint32_t layerCount = convertLittleEndianToHostEndian(header.layerCount);
	const uint32_t faceCount = convertLittleEndianToHostEndian(header.faceCount);
	Extent3D size3D{
		.width = convertLittleEndianToHostEndian(header.pixelWidth),
		.height = convertLittleEndianToHostEndian(header.pixelHeight),
		.depth = max(layerCount, uint32_t{1}),
	};
	ImageType type{};
	if (faceCount == 6) {
		type = (layerCount == 0) ? ImageType::IMAGE_CUBE : ImageType::IMAGE_CUBE_ARRAY;
		if (Limits<uint32_t>::MAX / size3D.depth < 6) {
			throw std::length_error{"Image size overflow."};
		}
		size3D.depth *= 6;
	} else if (faceCount != 1) {
		throw resource::Error{"Invalid KTX2 face count."};
	} else {
		type = (layerCount == 0) ? ImageType::IMAGE_2D : ImageType::IMAGE_2D_ARRAY;
	}
	const uint32_t mipLevelCount = min(convertLittleEndianToHostEndian(header.levelCount), Image::getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));
	const Optional<ImageFormat> format = getKTX2ImageFormat(vkFormat);
	if (!format) {
		throw resource::Error{"Unsupported KTX2 image format."};
	}
	if (fileContents.size() - sizeof(KTX2Header) < static_cast<size_t>(max(uint32_t{1}, mipLevelCount)) * sizeof(KTX2LevelIndexEntry)) {
		throw std::length_error{"Invalid KTX2 file size."};
	}
	if (options.requiredType && *options.requiredType != type) {
		throw resource::Error{"Unexpected image type."};
	}
	if (options.requiredFormat && *options.requiredFormat != *format) {
		throw resource::Error{"Unexpected image format."};
	}

	const uint64_t fileSize = static_cast<uint64_t>(fileContents.size());
	Image result{type, *format, size3D, mipLevelCount};
	byte* output = result.data();
	for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
		KTX2LevelIndexEntry level;
		memcpy(&level, fileContents.data() + sizeof(KTX2Header) + mipLevel * sizeof(KTX2LevelIndexEntry), sizeof(KTX2LevelIndexEntry));

		const uint64_t byteOffset = convertLittleEndianToHostEndian(level.byteOffset);
		const uint64_t byteLength = convertLittleEndianToHostEndian(level.byteLength);
		const uint64_t uncompressedByteLength = convertLittleEndianToHostEndian(level.uncompressedByteLength);
		if (uncompressedByteLength != byteLength) {
			throw resource::Error{"Unsupported KTX2 image layout."};
		}

		const size_t mipLevelStride = Image::getMipLevelStride(*format, Image::getMipLevelSize3D(size3D, mipLevel));
		if (static_cast<size_t>(byteLength) != mipLevelStride) {
			throw std::length_error{"Invalid KTX2 mip level size."};
		}

		if (byteOffset > fileSize || byteLength > fileSize - byteOffset) {
			throw std::length_error{"Invalid KTX2 file size."};
		}

		memcpy(output, fileContents.data() + byteOffset, mipLevelStride);
		output += mipLevelStride;
	}
	return result;
}

inline void saveKTX2Image(const ImageView& image, const ImageSaveKTX2Options& options, Writer writer) {
	GREM_PROFILE_BLOCK("Save KTX2 image");

	(void)options;

	if (image.getType() == ImageType::EMPTY || image.getWidth() == 0 || image.getHeight() == 0 || image.getDepth() == 0 || image.getMipLevelCount() == 0) {
		throw resource::Error{"Cannot save an empty image."};
	}

	switch (image.getFormat()) {
		case ImageFormat::UNKNOWN: throw resource::Error{"Cannot save an image of unknown format."};
		case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: writer.write(image.getContents()); return;
		default: break;
	}

	if (image.getMipLevelCount() > Image::getMaxMipLevelCount(image.getSize2D())) {
		throw resource::Error{"Invalid mip level count."};
	}

	const Buffer<uint32_t> dataFormatDescriptor = buildDataFormatDescriptor(image.getFormat());

	const KTX2Header header{
		.identifier = KTX2_IDENTIFIER,
		.vkFormat = convertHostEndianToLittleEndian([&]() -> uint32_t {
			switch (image.getFormat()) {
				case ImageFormat::UNKNOWN: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: unreachable();
				case ImageFormat::R8_UINT: return 9;                              // VK_FORMAT_R8_UNORM
				case ImageFormat::R16_FLOAT: return 76;                           // VK_FORMAT_R16_SFLOAT
				case ImageFormat::R32_FLOAT: return 100;                          // VK_FORMAT_R32_SFLOAT
				case ImageFormat::R8G8_UINT: return 16;                           // VK_FORMAT_R8G8_UNORM
				case ImageFormat::R16G16_FLOAT: return 83;                        // VK_FORMAT_R16G16_SFLOAT
				case ImageFormat::R32G32_FLOAT: return 103;                       // VK_FORMAT_R32G32_SFLOAT
				case ImageFormat::R8G8B8_UINT: return 29;                         // VK_FORMAT_R8G8B8_SRGB
				case ImageFormat::R16G16B16_FLOAT: return 90;                     // VK_FORMAT_R16G16B16_SFLOAT
				case ImageFormat::R32G32B32_FLOAT: return 106;                    // VK_FORMAT_R32G32B32_SFLOAT
				case ImageFormat::R8G8B8A8_UINT: return 43;                       // VK_FORMAT_R8G8B8A8_SRGB
				case ImageFormat::R16G16B16A16_FLOAT: return 97;                  // VK_FORMAT_R16G16B16A16_SFLOAT
				case ImageFormat::R32G32B32A32_FLOAT: return 109;                 // VK_FORMAT_R32G32B32A32_SFLOAT
				case ImageFormat::R5G6B5_UINT_PACK16: return 4;                   // VK_FORMAT_R5G6B5_UNORM_PACK16;
				case ImageFormat::A1R5G5B5_UINT_PACK16: return 8;                 // VK_FORMAT_A1R5G5B5_UNORM_PACK16;
				case ImageFormat::B10G11R11_UFLOAT_PACK32: return 122;            // VK_FORMAT_B10G11R11_UFLOAT_PACK32;
				case ImageFormat::A2B10G10R10_UINT_PACK32: return 64;             // VK_FORMAT_A2B10G10R10_UNORM_PACK32;
				case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: return 158;           // VK_FORMAT_ASTC_4x4_SRGB_BLOCK
				case ImageFormat::BC1_RGB_UINT_BLOCK: return 132;                 // VK_FORMAT_BC1_RGB_SRGB_BLOCK
				case ImageFormat::BC3_RGBA_UINT_BLOCK: return 138;                // VK_FORMAT_BC3_SRGB_BLOCK
				case ImageFormat::BC4_R_UINT_BLOCK: return 139;                   // VK_FORMAT_BC4_UNORM_BLOCK
				case ImageFormat::BC5_RG_UINT_BLOCK: return 141;                  // VK_FORMAT_BC5_UNORM_BLOCK
				case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: return 143;              // VK_FORMAT_BC6H_UFLOAT_BLOCK
				case ImageFormat::BC6H_RGB_FLOAT_BLOCK: return 144;               // VK_FORMAT_BC6H_SFLOAT_BLOCK
				case ImageFormat::BC7_RGBA_UINT_BLOCK: return 146;                // VK_FORMAT_BC7_SRGB_BLOCK
				case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: return 148;             // VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
				case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: return 152;           // VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
				case ImageFormat::EAC_R11_UINT_BLOCK: return 153;                 // VK_FORMAT_EAC_R11_UNORM_BLOCK
				case ImageFormat::EAC_R11G11_UINT_BLOCK: return 155;              // VK_FORMAT_EAC_R11G11_UNORM_BLOCK
				case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return 1000054005; // VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG
			}
			unreachable();
		}()),
		.typeSize = convertHostEndianToLittleEndian([&]() -> uint32_t {
			switch (image.getFormat()) {
				case ImageFormat::UNKNOWN: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: unreachable();
				case ImageFormat::R8_UINT: [[fallthrough]];
				case ImageFormat::R8G8_UINT: [[fallthrough]];
				case ImageFormat::R8G8B8_UINT: [[fallthrough]];
				case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
				case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
				case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
				case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return 1;
				case ImageFormat::R16_FLOAT: [[fallthrough]];
				case ImageFormat::R16G16_FLOAT: [[fallthrough]];
				case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
				case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
				case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
				case ImageFormat::A1R5G5B5_UINT_PACK16: return 2;
				case ImageFormat::R32_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
				case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
				case ImageFormat::A2B10G10R10_UINT_PACK32: return 4;
			}
			unreachable();
		}()),
		.pixelWidth = convertHostEndianToLittleEndian(image.getWidth()),
		.pixelHeight = convertHostEndianToLittleEndian(image.getHeight()),
		.pixelDepth = convertHostEndianToLittleEndian(uint32_t{0}),
		.layerCount = convertHostEndianToLittleEndian([&]() -> uint32_t {
			switch (image.getType()) {
				case ImageType::EMPTY: unreachable();
				case ImageType::IMAGE_2D:
					if (image.getDepth() != 1) {
						throw resource::Error{"Invalid image depth."};
					}
					return 0;
				case ImageType::IMAGE_2D_ARRAY:
					if (image.getDepth() == 0) {
						throw resource::Error{"Invalid image depth."};
					}
					return image.getDepth();
				case ImageType::IMAGE_CUBE:
					if (image.getDepth() != 6) {
						throw resource::Error{"Invalid image depth."};
					}
					return 0;
				case ImageType::IMAGE_CUBE_ARRAY:
					if (image.getDepth() % 6 != 0) {
						throw resource::Error{"Invalid image depth."};
					}
					return image.getDepth() / 6;
			}
			unreachable();
		}()),
		.faceCount = convertHostEndianToLittleEndian([&]() -> uint32_t {
			switch (image.getType()) {
				case ImageType::EMPTY: unreachable();
				case ImageType::IMAGE_2D: [[fallthrough]];
				case ImageType::IMAGE_2D_ARRAY: return 1;
				case ImageType::IMAGE_CUBE: [[fallthrough]];
				case ImageType::IMAGE_CUBE_ARRAY: return 6;
			}
			unreachable();
		}()),
		.levelCount = convertHostEndianToLittleEndian(image.getMipLevelCount()),
		.supercompressionScheme = convertHostEndianToLittleEndian(uint32_t{0}),
		.dfdByteOffset = convertHostEndianToLittleEndian(static_cast<uint32_t>(sizeof(KTX2Header) + image.getMipLevelCount() * sizeof(KTX2LevelIndexEntry))),
		.dfdByteLength = dataFormatDescriptor.front(),
		.kvdByteOffset = convertHostEndianToLittleEndian(uint32_t{0}),
		.kvdByteLength = convertHostEndianToLittleEndian(uint32_t{0}),
		.sgdByteOffset = convertHostEndianToLittleEndian(uint32_t{0}),
		.sgdByteLength = convertHostEndianToLittleEndian(uint32_t{0}),
	};
	writer.write(asBytes(Span{&header, 1}));

	const size_t mipAlignment = [&]() -> size_t {
		switch (image.getFormat()) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: unreachable();
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: return 4;
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return 8;
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: return 12;
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: return 16;
		}
		unreachable();
	}();

	const size_t baseMipLevelOffset = sizeof(KTX2Header) + image.getMipLevelCount() * sizeof(KTX2LevelIndexEntry) + dataFormatDescriptor.size() * sizeof(uint32_t);

	Buffer<KTX2LevelIndexEntry> reverseLevelIndex{};

	size_t byteOffset = baseMipLevelOffset;
	for (uint32_t mipLevel = image.getMipLevelCount(); mipLevel-- > 0;) {
		byteOffset = roundUpToMultiple(byteOffset, mipAlignment);
		const size_t mipLevelStride = Image::getMipLevelStride(image.getFormat(), Image::getMipLevelSize3D(image.getSize3D(), mipLevel));
		reverseLevelIndex.push_back(KTX2LevelIndexEntry{
			.byteOffset = convertHostEndianToLittleEndian(static_cast<uint64_t>(byteOffset)),
			.byteLength = convertHostEndianToLittleEndian(static_cast<uint64_t>(mipLevelStride)),
			.uncompressedByteLength = convertHostEndianToLittleEndian(static_cast<uint64_t>(mipLevelStride)),
		});
		byteOffset += mipLevelStride;
	}

	for (size_t i = reverseLevelIndex.size(); i-- > 0;) {
		writer.write(asBytes(Span{&reverseLevelIndex[i], 1}));
	}

	writer.write(asBytes(Span{dataFormatDescriptor}));

	byteOffset = baseMipLevelOffset;
	for (uint32_t mipLevel = image.getMipLevelCount(); mipLevel-- > 0;) {
		const size_t mipLevelOffset = byteOffset;
		byteOffset = roundUpToMultiple(byteOffset, mipAlignment);
		const size_t alignmentBytes = byteOffset - mipLevelOffset;
		for (size_t i = 0; i < alignmentBytes; ++i) {
			const uint8_t padding = 0x00;
			writer.write(asBytes(Span{&padding, 1}));
		}

		const Span<const byte> mipLevelData = image.getMipLevel(mipLevel).getContents();
		writer.write(mipLevelData);

		byteOffset += mipLevelData.size();
	}
}

} // namespace grem::resource

#endif
