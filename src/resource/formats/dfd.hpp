// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_FORMATS_DFD_HPP
#define GREM_RESOURCE_FORMATS_DFD_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/resource/Image.hpp>

#include <initializer_list> // std::initializer_list

namespace grem::resource {

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_KHR_DESCRIPTORTYPE_BASICFORMAT = 0,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_VENDORID_KHRONOS = 0,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_MODEL_RGBSDA = 1,
	KHR_DF_MODEL_ASTC = 162,
	KHR_DF_MODEL_BC1A = 128,
	KHR_DF_MODEL_BC3 = 130,
	KHR_DF_MODEL_BC4 = 131,
	KHR_DF_MODEL_BC5 = 132,
	KHR_DF_MODEL_BC6H = 133,
	KHR_DF_MODEL_BC7 = 134,
	KHR_DF_MODEL_ETC2 = 161,
	KHR_DF_MODEL_PVRTC = 164,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_CHANNEL_ASTC_DATA = 0,
	KHR_DF_CHANNEL_BC1A_COLOR = 0,
	KHR_DF_CHANNEL_BC3_ALPHA = 15,
	KHR_DF_CHANNEL_BC3_COLOR = 0,
	KHR_DF_CHANNEL_BC4_DATA = 0,
	KHR_DF_CHANNEL_BC5_RED = 0,
	KHR_DF_CHANNEL_BC5_GREEN = 1,
	KHR_DF_CHANNEL_BC6H_COLOR = 0,
	KHR_DF_CHANNEL_BC7_COLOR = 0,
	KHR_DF_CHANNEL_ETC2_COLOR = 2,
	KHR_DF_CHANNEL_ETC2_ALPHA = 15,
	KHR_DF_CHANNEL_ETC2_RED = 0,
	KHR_DF_CHANNEL_ETC2_GREEN = 1,
	KHR_DF_CHANNEL_PVRTC_COLOR = 0,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_PRIMARIES_BT709 = 1,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_TRANSFER_LINEAR = 1,
	KHR_DF_TRANSFER_SRGB = 2,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_FLAG_ALPHA_STRAIGHT = 0,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_CHANNEL_RGBSDA_RED = 0,
	KHR_DF_CHANNEL_RGBSDA_GREEN = 1,
	KHR_DF_CHANNEL_RGBSDA_BLUE = 2,
	KHR_DF_CHANNEL_RGBSDA_ALPHA = 15,
};

enum : uint32_t { // NOLINT(performance-enum-size)
	KHR_DF_SAMPLE_DATATYPE_LINEAR = 1 << 4,
	KHR_DF_SAMPLE_DATATYPE_SIGNED = 1 << 6,
	KHR_DF_SAMPLE_DATATYPE_FLOAT = 1 << 7,
};

struct UncompressedComponentInfo {
	uint32_t bitOffset;
	uint32_t bitLength;
};

struct UncompressedDataFormatInfo {
	bool srgb;
	bool isSigned;
	bool isFloat;
};

[[nodiscard]] inline Buffer<uint32_t> buildUncompressedDataFormatDescriptor(uint32_t pixelStride, std::initializer_list<UncompressedComponentInfo> pixelComponents,
	const UncompressedDataFormatInfo& info) {
	GREM_ASSERT(!info.isSigned || info.isFloat); // Only float may be signed.
	GREM_ASSERT(!info.srgb || !info.isFloat);    // Only non-float may be sRGB.

	Buffer<uint32_t> result{};

	const uint32_t channelCount = static_cast<uint32_t>(pixelComponents.size());

	const uint32_t totalSize = static_cast<uint32_t>(sizeof(uint32_t)) * (7 + channelCount * 4);

	const uint32_t descriptorType = KHR_DF_KHR_DESCRIPTORTYPE_BASICFORMAT;
	const uint32_t vendorId = KHR_DF_VENDORID_KHRONOS;
	const uint32_t descriptorBlockSize = static_cast<uint32_t>(sizeof(uint32_t)) * (6 + channelCount * 4);
	const uint32_t versionNumber = 2;

	const uint32_t flags = KHR_DF_FLAG_ALPHA_STRAIGHT;
	const uint32_t transferFunction = (info.srgb) ? KHR_DF_TRANSFER_SRGB : KHR_DF_TRANSFER_LINEAR;
	const uint32_t colorPrimaries = KHR_DF_PRIMARIES_BT709;
	const uint32_t colorModel = KHR_DF_MODEL_RGBSDA;

	const uint32_t texelBlockDimension0to3 = 0;
	const uint32_t bytesPlane0to3 = pixelStride;
	const uint32_t bytesPlane4to7 = 0;

	result.push_back(convertHostEndianToLittleEndian(totalSize));
	result.push_back(convertHostEndianToLittleEndian((descriptorType << 17) | vendorId));
	result.push_back(convertHostEndianToLittleEndian((descriptorBlockSize << 16) | versionNumber));
	result.push_back(convertHostEndianToLittleEndian((flags << 24) | (transferFunction << 16) | (colorPrimaries << 8) | colorModel));
	result.push_back(convertHostEndianToLittleEndian(texelBlockDimension0to3));
	result.push_back(convertHostEndianToLittleEndian(bytesPlane0to3));
	result.push_back(convertHostEndianToLittleEndian(bytesPlane4to7));

	for (uint32_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
		const UncompressedComponentInfo component = pixelComponents.begin()[channelIndex];

		uint32_t channelType = 0;
		switch (channelIndex) {
			case 0: channelType = KHR_DF_CHANNEL_RGBSDA_RED; break;
			case 1: channelType = KHR_DF_CHANNEL_RGBSDA_GREEN; break;
			case 2: channelType = KHR_DF_CHANNEL_RGBSDA_BLUE; break;
			case 3:
				channelType = KHR_DF_CHANNEL_RGBSDA_ALPHA;
				if (info.srgb) {
					channelType |= KHR_DF_SAMPLE_DATATYPE_LINEAR;
				}
				break;
			default: unreachable();
		}
		if (info.isSigned) {
			channelType |= KHR_DF_SAMPLE_DATATYPE_SIGNED;
		}
		if (info.isFloat) {
			channelType |= KHR_DF_SAMPLE_DATATYPE_FLOAT;
		}
		const uint32_t bitLength = component.bitLength - 1;
		const uint32_t samplePosition0to3 = 0;
		const uint32_t sampleLower = (info.isFloat) ? ((info.isSigned) ? bit_cast<uint32_t>(float32_t{-1.0f}) : bit_cast<uint32_t>(float32_t{0.0f})) : uint32_t{0};
		const uint32_t sampleUpper = (info.isFloat) ? bit_cast<uint32_t>(float32_t{1.0f}) : getMaxValueForBits<uint32_t>(component.bitLength);
		result.push_back(convertHostEndianToLittleEndian((channelType << 24) | (bitLength << 16) | component.bitOffset));
		result.push_back(convertHostEndianToLittleEndian(samplePosition0to3));
		result.push_back(convertHostEndianToLittleEndian(sampleLower));
		result.push_back(convertHostEndianToLittleEndian(sampleUpper));
	}

	GREM_ASSERT(result.size() * sizeof(uint32_t) == totalSize);
	return result;
}

struct CompressedDataFormatInfo {
	bool srgb;
	bool isSigned;
	bool isFloat;
};

[[nodiscard]] inline Buffer<uint32_t> buildCompressedDataFormatDescriptor(std::initializer_list<uint32_t> channelTypes, Extent3D blockSize, uint32_t blockStride,
	uint32_t colorModel, const CompressedDataFormatInfo& info) {
	Buffer<uint32_t> result{};

	const uint32_t channelCount = static_cast<uint32_t>(channelTypes.size());
	const uint32_t totalSize = static_cast<uint32_t>(sizeof(uint32_t)) * (7 + channelCount * 4);

	const uint32_t descriptorType = KHR_DF_KHR_DESCRIPTORTYPE_BASICFORMAT;
	const uint32_t vendorId = KHR_DF_VENDORID_KHRONOS;
	const uint32_t descriptorBlockSize = static_cast<uint32_t>(sizeof(uint32_t)) * (6 + channelCount * 4);
	const uint32_t versionNumber = 2;

	const uint32_t flags = KHR_DF_FLAG_ALPHA_STRAIGHT;
	const uint32_t transferFunction = (info.srgb) ? KHR_DF_TRANSFER_SRGB : KHR_DF_TRANSFER_LINEAR;
	const uint32_t colorPrimaries = KHR_DF_PRIMARIES_BT709;

	const uint32_t texelBlockDimension3 = 0;
	const uint32_t texelBlockDimension2 = blockSize.depth - 1;
	const uint32_t texelBlockDimension1 = blockSize.height - 1;
	const uint32_t texelBlockDimension0 = blockSize.width - 1;
	const uint32_t bytesPlane0to3 = blockStride;
	const uint32_t bytesPlane4to7 = 0;

	result.push_back(convertHostEndianToLittleEndian(totalSize));
	result.push_back(convertHostEndianToLittleEndian((descriptorType << 17) | vendorId));
	result.push_back(convertHostEndianToLittleEndian((descriptorBlockSize << 16) | versionNumber));
	result.push_back(convertHostEndianToLittleEndian((flags << 24) | (transferFunction << 16) | (colorPrimaries << 8) | colorModel));
	result.push_back(convertHostEndianToLittleEndian((texelBlockDimension3 << 24) | (texelBlockDimension2 << 16) | (texelBlockDimension1 << 8) | texelBlockDimension0));
	result.push_back(convertHostEndianToLittleEndian(bytesPlane0to3));
	result.push_back(convertHostEndianToLittleEndian(bytesPlane4to7));

	const uint32_t componentBits = (blockStride * 8) / channelCount;
	for (uint32_t channelIndex = 0; channelIndex < channelTypes.size(); ++channelIndex) {
		uint32_t channelType = channelTypes.begin()[channelIndex];
		if (info.srgb && channelType == 15) {
			channelType |= KHR_DF_SAMPLE_DATATYPE_LINEAR;
		}
		if (info.isSigned) {
			channelType |= KHR_DF_SAMPLE_DATATYPE_SIGNED;
		}
		if (info.isFloat) {
			channelType |= KHR_DF_SAMPLE_DATATYPE_FLOAT;
		}
		const uint32_t bitLength = componentBits - 1;
		const uint32_t bitOffset = channelIndex * componentBits;
		const uint32_t samplePosition0to3 = 0;
		const uint32_t sampleLower = (info.isFloat) ? bit_cast<uint32_t>(float32_t{-1.0f}) : uint32_t{0};
		const uint32_t sampleUpper = (info.isFloat) ? bit_cast<uint32_t>(float32_t{1.0f}) : uint32_t{0xFFFFFFFF};
		result.push_back(convertHostEndianToLittleEndian((channelType << 24) | (bitLength << 16) | bitOffset));
		result.push_back(convertHostEndianToLittleEndian(samplePosition0to3));
		result.push_back(convertHostEndianToLittleEndian(sampleLower));
		result.push_back(convertHostEndianToLittleEndian(sampleUpper));
	}
	return result;
}

[[nodiscard]] inline Buffer<uint32_t> buildDataFormatDescriptor(ImageFormat format) {
	switch (format) {
		case ImageFormat::UNKNOWN: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: unreachable();
		case ImageFormat::R8_UINT: return buildUncompressedDataFormatDescriptor(1, {{0, 8}}, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::R16_FLOAT: return buildUncompressedDataFormatDescriptor(2, {{0, 16}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R32_FLOAT: return buildUncompressedDataFormatDescriptor(4, {{0, 32}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R8G8_UINT: return buildUncompressedDataFormatDescriptor(2, {{0, 8}, {8, 8}}, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::R16G16_FLOAT: return buildUncompressedDataFormatDescriptor(4, {{0, 16}, {16, 16}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R32G32_FLOAT: return buildUncompressedDataFormatDescriptor(8, {{0, 32}, {32, 32}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R8G8B8_UINT: return buildUncompressedDataFormatDescriptor(3, {{0, 8}, {8, 8}, {16, 8}}, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::R16G16B16_FLOAT: return buildUncompressedDataFormatDescriptor(6, {{0, 16}, {16, 16}, {32, 16}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R32G32B32_FLOAT: return buildUncompressedDataFormatDescriptor(12, {{0, 32}, {32, 32}, {64, 32}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R8G8B8A8_UINT: return buildUncompressedDataFormatDescriptor(4, {{0, 8}, {8, 8}, {16, 8}, {24, 8}}, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::R16G16B16A16_FLOAT:
			return buildUncompressedDataFormatDescriptor(8, {{0, 16}, {16, 16}, {32, 16}, {48, 16}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R32G32B32A32_FLOAT:
			return buildUncompressedDataFormatDescriptor(16, {{0, 32}, {32, 32}, {64, 32}, {96, 32}}, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::R5G6B5_UINT_PACK16: return buildUncompressedDataFormatDescriptor(2, {{0, 5}, {5, 6}, {11, 5}}, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::A1R5G5B5_UINT_PACK16:
			return buildUncompressedDataFormatDescriptor(2, {{1, 5}, {6, 5}, {11, 5}, {0, 1}}, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::B10G11R11_UFLOAT_PACK32:
			return buildUncompressedDataFormatDescriptor(4, {{0, 11}, {11, 11}, {22, 10}}, {.srgb = false, .isSigned = false, .isFloat = true});
		case ImageFormat::A2B10G10R10_UINT_PACK32:
			return buildUncompressedDataFormatDescriptor(4, {{0, 10}, {10, 10}, {20, 10}, {30, 2}}, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_ASTC_DATA}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_ASTC, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::BC1_RGB_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC1A_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_BC1A, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::BC3_RGBA_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC3_ALPHA, KHR_DF_CHANNEL_BC3_COLOR}, Image::getBlockSize2D(format),
				static_cast<uint32_t>(Image::getBlockStride(format)), KHR_DF_MODEL_BC3, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::BC4_R_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC4_DATA}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_BC4, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::BC5_RG_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC5_RED, KHR_DF_CHANNEL_BC5_GREEN}, Image::getBlockSize2D(format),
				static_cast<uint32_t>(Image::getBlockStride(format)), KHR_DF_MODEL_BC5, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::BC6H_RGB_UFLOAT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC6H_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_BC6H, {.srgb = false, .isSigned = false, .isFloat = true});
		case ImageFormat::BC6H_RGB_FLOAT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC6H_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_BC6H, {.srgb = false, .isSigned = true, .isFloat = true});
		case ImageFormat::BC7_RGBA_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_BC7_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_BC7, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::ETC2_R8G8B8_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_ETC2_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_ETC2, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_ETC2_ALPHA, KHR_DF_CHANNEL_ETC2_COLOR}, Image::getBlockSize2D(format),
				static_cast<uint32_t>(Image::getBlockStride(format)), KHR_DF_MODEL_ETC2, {.srgb = true, .isSigned = false, .isFloat = false});
		case ImageFormat::EAC_R11_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_ETC2_RED}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_ETC2, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::EAC_R11G11_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_ETC2_RED, KHR_DF_CHANNEL_ETC2_GREEN}, Image::getBlockSize2D(format),
				static_cast<uint32_t>(Image::getBlockStride(format)), KHR_DF_MODEL_ETC2, {.srgb = false, .isSigned = false, .isFloat = false});
		case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK:
			return buildCompressedDataFormatDescriptor({KHR_DF_CHANNEL_PVRTC_COLOR}, Image::getBlockSize2D(format), static_cast<uint32_t>(Image::getBlockStride(format)),
				KHR_DF_MODEL_PVRTC, {.srgb = true, .isSigned = false, .isFloat = false});
	}
	unreachable();
}

} // namespace grem::resource

#endif
