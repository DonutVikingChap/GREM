// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>

#include "formats/hdr.hpp"
#include "formats/jpeg.hpp"
#include "formats/ktx2.hpp"
#include "formats/png.hpp"

#include <basisu_transcoder.h> // basist::...
#include <stdexcept>           // std::invalid_argument, std::length_error

namespace grem::resource {

namespace {

template <size_t N>
[[nodiscard]] bool fileStartsWith(InputFile& file, const Array<char, N>& identifier) {
	GREM_ASSERT(file.tellg() == 0);
	Array<uint8_t, N> buffer{};
	if (!file.tryRead(asWritableBytes(Span{buffer}))) {
		file.seekg(0);
		return false;
	}
	file.seekg(0);
	return memcmp(buffer.data(), identifier.data(), N) == 0;
}

template <size_t N>
[[nodiscard]] bool fileContentsStartWith(Span<const byte> fileContents, const Array<char, N>& identifier) {
	return fileContents.size() >= N && memcmp(fileContents.data(), identifier.data(), N) == 0;
}

} // namespace

void ImageView::transcodeTo(byte* output, ImageFormat outputFormat) const {
	GREM_PROFILE_FUNCTION();
	switch (format) {
		case ImageFormat::UNKNOWN: [[fallthrough]];
		case ImageFormat::R8_UINT: [[fallthrough]];
		case ImageFormat::R8G8_UINT: [[fallthrough]];
		case ImageFormat::R8G8B8_UINT: [[fallthrough]];
		case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
		case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
		case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
		case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
		case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
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
		case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK:
			if (format == outputFormat) {
				if (!contents.empty()) {
					memcpy(output, contents.data(), contents.size());
				}
				return;
			}
			break;
		case ImageFormat::R16_FLOAT: [[fallthrough]];
		case ImageFormat::R32_FLOAT: [[fallthrough]];
		case ImageFormat::R16G16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32_FLOAT: [[fallthrough]];
		case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
		case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32B32A32_FLOAT:
			if (format == outputFormat) {
				if (!contents.empty()) {
					memcpy(output, contents.data(), contents.size());
				}
				return;
			}
			if (format == ImageFormat::R16_FLOAT && outputFormat == ImageFormat::R32_FLOAT) {
				Image::convertPixels<float32_t, 1, float16_t, 1>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R16G16_FLOAT && outputFormat == ImageFormat::R32G32_FLOAT) {
				Image::convertPixels<float32_t, 2, float16_t, 2>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R16G16B16_FLOAT && outputFormat == ImageFormat::R32G32B32_FLOAT) {
				Image::convertPixels<float32_t, 3, float16_t, 3>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R16G16B16A16_FLOAT && outputFormat == ImageFormat::R32G32B32A32_FLOAT) {
				Image::convertPixels<float32_t, 4, float16_t, 4>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R32_FLOAT && outputFormat == ImageFormat::R16_FLOAT) {
				Image::convertPixels<float16_t, 1, float32_t, 1>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R32G32_FLOAT && outputFormat == ImageFormat::R16G16_FLOAT) {
				Image::convertPixels<float16_t, 2, float32_t, 2>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R32G32B32_FLOAT && outputFormat == ImageFormat::R16G16B16_FLOAT) {
				Image::convertPixels<float16_t, 3, float32_t, 3>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			if (format == ImageFormat::R32G32B32A32_FLOAT && outputFormat == ImageFormat::R16G16B16A16_FLOAT) {
				Image::convertPixels<float16_t, 4, float32_t, 4>(size3D, mipLevelCount, output, contents.data());
				return;
			}
			break;
		case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: {
			if (contents.size() > Limits<uint32_t>::MAX) {
				throw std::length_error{"Failed to transcode image: Maximum KTX2 file size exceeded."};
			}

			ensureBasisUniversalTranscoderInitialized();
			basist::ktx2_transcoder transcoder{};
			if (!transcoder.init(contents.data(), static_cast<uint32_t>(contents.size()))) {
				throw std::invalid_argument{"Failed to transcode image: Invalid KTX2 file."};
			}

			if (!transcoder.start_transcoding()) {
				throw std::invalid_argument{"Failed to transcode image: Failed to start KTX2 transcode."};
			}

			Image temporaryRGBA8Image{};
			byte* outputRGBA8 = output;

			basist::transcoder_texture_format fmt{};
			switch (outputFormat) {
				case ImageFormat::UNKNOWN: [[fallthrough]];
				case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
				case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
				case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
				case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
				case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
				case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: throw std::invalid_argument{"Invalid image format transcoding."};
				case ImageFormat::R8_UINT: [[fallthrough]];
				case ImageFormat::R16_FLOAT: [[fallthrough]];
				case ImageFormat::R32_FLOAT: [[fallthrough]];
				case ImageFormat::R8G8_UINT: [[fallthrough]];
				case ImageFormat::R16G16_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32_FLOAT: [[fallthrough]];
				case ImageFormat::R8G8B8_UINT: [[fallthrough]];
				case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
				case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
				case ImageFormat::R32G32B32A32_FLOAT:
					temporaryRGBA8Image = Image{type, ImageFormat::R8G8B8A8_UINT, size3D, mipLevelCount};
					outputRGBA8 = temporaryRGBA8Image.data();
					fmt = basist::transcoder_texture_format::cTFRGBA32;
					break;
				case ImageFormat::R8G8B8A8_UINT: fmt = basist::transcoder_texture_format::cTFRGBA32; break;
				case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFASTC_4x4_RGBA; break;
				case ImageFormat::BC1_RGB_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC1_RGB; break;
				case ImageFormat::BC3_RGBA_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC3_RGBA; break;
				case ImageFormat::BC4_R_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC4_R; break;
				case ImageFormat::BC5_RG_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC5_RG; break;
				case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC6H; break;
				case ImageFormat::BC7_RGBA_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFBC7_RGBA; break;
				case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFETC1_RGB; break; // ETC1 is a subset of ETC2.
				case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFETC2_RGBA; break;
				case ImageFormat::EAC_R11_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFETC2_EAC_R11; break;
				case ImageFormat::EAC_R11G11_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFETC2_EAC_RG11; break;
				case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: fmt = basist::transcoder_texture_format::cTFPVRTC1_4_RGBA; break;
			}

			basist::ktx2_transcoder_state state{};
			state.clear();
			const size_t outputBlockStride = Image::getBlockStride(outputFormat);
			GREM_ASSERT(outputBlockStride > 0);
			const uint32_t layerCount = (type == ImageType::IMAGE_CUBE || type == ImageType::IMAGE_CUBE_ARRAY) ? size3D.depth / 6 : size3D.depth;
			const uint32_t faceCount = (type == ImageType::IMAGE_CUBE || type == ImageType::IMAGE_CUBE_ARRAY) ? 6 : 1;
			for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
				const size_t outputLayerStride = Image::getLayerStride(outputFormat, Image::getMipLevelSize2D(Extent2D{size3D.width, size3D.height}, mipLevel));
				const size_t outputLayerBlockCount = outputLayerStride / outputBlockStride;

				for (uint32_t layer = 0; layer < layerCount; ++layer) {
					for (uint32_t face = 0; face < faceCount; ++face) {
						if (!transcoder.transcode_image_level(mipLevel, layer, face, outputRGBA8, static_cast<uint32_t>(outputLayerBlockCount), fmt,
								basist::cDecodeFlagsHighQuality, 0, 0, -1, -1, &state)) {
							throw std::invalid_argument{"Failed to transcode image: Failed to perform KTX2 transcode."};
						}
						outputRGBA8 += outputLayerStride;
					}
				}
			}

			switch (outputFormat) {
				case ImageFormat::R8_UINT: Image::convertPixels<u8norm, 1, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R16_FLOAT: Image::convertPixels<float16_t, 1, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R32_FLOAT: Image::convertPixels<float32_t, 1, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R8G8_UINT: Image::convertPixels<u8norm, 2, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R16G16_FLOAT: Image::convertPixels<float16_t, 2, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R32G32_FLOAT: Image::convertPixels<float32_t, 2, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R8G8B8_UINT: Image::convertPixels<u8norm, 3, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R16G16B16_FLOAT: Image::convertPixels<float16_t, 3, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R32G32B32_FLOAT: Image::convertPixels<float32_t, 3, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R16G16B16A16_FLOAT: Image::convertPixels<float16_t, 4, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				case ImageFormat::R32G32B32A32_FLOAT: Image::convertPixels<float32_t, 4, u8norm, 4>(size3D, mipLevelCount, output, temporaryRGBA8Image.data()); break;
				default: break;
			}
			return;
		}
	}
	throw std::invalid_argument{"Invalid image format transcoding."};
}

Image ImageView::getPadded(uint32_t paddingLeft, uint32_t paddingRight, uint32_t paddingTop, uint32_t paddingBottom) const {
	const size_t pixelStride = Image::getPixelStride(format);
	if (pixelStride == 0) {
		throw std::invalid_argument{"Invalid image format."};
	}
	if (size3D.depth == 0 || mipLevelCount == 0 || (paddingLeft == 0 && paddingRight == 0 && paddingTop == 0 && paddingBottom == 0)) {
		return Image{*this};
	}
	const uint32_t newWidth = paddingLeft + size3D.width + paddingRight;
	const uint32_t newHeight = paddingTop + size3D.height + paddingBottom;
	Image result{type, format, Extent3D{newWidth, newHeight, size3D.depth}, mipLevelCount};
	if (size3D.width > 0 && size3D.height > 0) {
		const byte* input = contents.data();
		byte* output = result.data();
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent2D mipLevelSize = Image::getMipLevelSize2D(Extent2D{size3D.width, size3D.height}, mipLevel);
			const Extent2D newMipLevelSize = Image::getMipLevelSize2D(Extent2D{newWidth, newHeight}, mipLevel);
			const uint32_t xBegin = paddingLeft >> mipLevel;
			const uint32_t xEnd = xBegin + mipLevelSize.width;
			const uint32_t yBegin = paddingTop >> mipLevel;
			const uint32_t yEnd = yBegin + mipLevelSize.height;
			for (uint32_t z = 0; z < size3D.depth; ++z) {
				uint32_t y = 0;
				for (; y < yBegin; ++y) {
					uint32_t x = 0;
					for (; x < xBegin; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					for (; x < xEnd; ++x) {
						memcpy(output, input, pixelStride);
						input += pixelStride;
						output += pixelStride;
					}
					input -= pixelStride;
					for (; x < newMipLevelSize.width; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					input -= (mipLevelSize.width - 1) * pixelStride;
				}
				for (; y < yEnd; ++y) {
					uint32_t x = 0;
					for (; x < xBegin; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					for (; x < xEnd; ++x) {
						memcpy(output, input, pixelStride);
						input += pixelStride;
						output += pixelStride;
					}
					input -= pixelStride;
					for (; x < newMipLevelSize.width; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					input += pixelStride;
				}
				input -= mipLevelSize.width * pixelStride;
				for (; y < newMipLevelSize.height; ++y) {
					uint32_t x = 0;
					for (; x < xBegin; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					for (; x < xEnd; ++x) {
						memcpy(output, input, pixelStride);
						input += pixelStride;
						output += pixelStride;
					}
					input -= pixelStride;
					for (; x < newMipLevelSize.width; ++x) {
						memcpy(output, input, pixelStride);
						output += pixelStride;
					}
					input -= (mipLevelSize.width - 1) * pixelStride;
				}
				input += mipLevelSize.width * pixelStride;
			}
		}
	} else {
		memset(result.data(), 0, result.size());
	}
	return result;
}

void ImageReference::transformFromStraightToPremultipliedAlpha(Color::TransferFunction transferFunction) {
	switch (format) {
		case ImageFormat::R8G8B8A8_UINT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<u8norm, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<u8norm, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		case ImageFormat::R16G16B16A16_FLOAT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<float16_t, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<float16_t, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		case ImageFormat::R32G32B32A32_FLOAT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<float32_t, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromStraightToPremultipliedAlpha<float32_t, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		default: throw std::invalid_argument{"Invalid image format."};
	}
}

void ImageReference::transformFromPremultipliedToStraightAlpha(Color::TransferFunction transferFunction) {
	switch (format) {
		case ImageFormat::R8G8B8A8_UINT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<u8norm, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<u8norm, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		case ImageFormat::R16G16B16A16_FLOAT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<float16_t, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<float16_t, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		case ImageFormat::R32G32B32A32_FLOAT:
			switch (transferFunction) {
				case Color::TransferFunction::SRGB:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<float32_t, Color::TransferFunction::SRGB>(size3D, mipLevelCount, data());
					break;
				case Color::TransferFunction::LINEAR:
					Image::transformRGBAPixelsFromPremultipliedToStraightAlpha<float32_t, Color::TransferFunction::LINEAR>(size3D, mipLevelCount, data());
					break;
			}
			break;
		default: throw std::invalid_argument{"Invalid image format."};
	}
}

ImageFileType Image::determineFileType(Span<const byte> fileContents) {
	if (fileContentsStartWith(fileContents, KTX2_IDENTIFIER)) {
		return ImageFileType::KTX2;
	}
	if (fileContentsStartWith(fileContents, PNG_IDENTIFIER)) {
		return ImageFileType::PNG;
	}
	if (fileContentsStartWith(fileContents, JPEG_IDENTIFIER)) {
		return ImageFileType::JPEG;
	}
	if (fileContentsStartWith(fileContents, HDR_RADIANCE_IDENTIFIER) || fileContentsStartWith(fileContents, HDR_RGBE_IDENTIFIER)) {
		return ImageFileType::HDR;
	}
	return ImageFileType::UNKNOWN;
}

void Image::savePNG(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSavePNGOptions& options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Save PNG image \"{}\"", filepath));
	filesystem.createParentOutputDirectories(filepath);
	OutputFileHandle file = filesystem.openEmptyOutputFile(filepath);
	try {
		savePNGImage(image, options, file);
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to save image to \"{}\": {}", filepath, e.what())};
	}
}

Allocation<byte> Image::savePNG(const ImageView& image, const ImageSavePNGOptions& options) {
	GREM_PROFILE_FUNCTION();
	Buffer<byte> buffer{};
	savePNGImage(image, options, buffer);
	return Allocation<byte>(buffer.begin(), buffer.end()); // NOLINT(modernize-return-braced-init-list)
}

void Image::saveJPEG(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveJPEGOptions& options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Save JPEG image \"{}\"", filepath));
	filesystem.createParentOutputDirectories(filepath);
	OutputFileHandle file = filesystem.openEmptyOutputFile(filepath);
	try {
		file.write(saveJPEGImage(image, options));
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to save image to \"{}\": {}", filepath, e.what())};
	}
}

Allocation<byte> Image::saveJPEG(const ImageView& image, const ImageSaveJPEGOptions& options) {
	GREM_PROFILE_FUNCTION();
	return saveJPEGImage(image, options);
}

void Image::saveHDR(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveHDROptions& options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Save HDR image \"{}\"", filepath));
	filesystem.createParentOutputDirectories(filepath);
	OutputFileHandle file = filesystem.openEmptyOutputFile(filepath);
	try {
		saveHDRImage(image, options, file);
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to save image to \"{}\": {}", filepath, e.what())};
	}
}

Allocation<byte> Image::saveHDR(const ImageView& image, const ImageSaveHDROptions& options) {
	GREM_PROFILE_FUNCTION();
	Buffer<byte> buffer{};
	saveHDRImage(image, options, buffer);
	return Allocation<byte>(buffer.begin(), buffer.end()); // NOLINT(modernize-return-braced-init-list)
}

void Image::saveKTX2(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveKTX2Options& options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Save KTX2 image \"{}\"", filepath));
	filesystem.createParentOutputDirectories(filepath);
	OutputFileHandle file = filesystem.openEmptyOutputFile(filepath);
	try {
		saveKTX2Image(image, options, file);
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to save image to \"{}\":\n{}", filepath, e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to save image to \"{}\": {}", filepath, e.what())};
	}
}

Allocation<byte> Image::saveKTX2(const ImageView& image, const ImageSaveKTX2Options& options) {
	GREM_PROFILE_FUNCTION();
	Buffer<byte> buffer{};
	saveKTX2Image(image, options, buffer);
	return Allocation<byte>(buffer.begin(), buffer.end()); // NOLINT(modernize-return-braced-init-list)
}

void Image::save(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveOptions&) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Save image \"{}\"", filepath));
	if (const size_t dotPosition = filepath.rfind('.'); dotPosition != CStringView::npos) {
		if (const CStringView extension = filepath.substr(dotPosition + 1); !extension.empty()) {
			if (extension == "png") {
				if (image.getDepth() != 1 || image.getMipLevelCount() != 1) {
					throw resource::Error{formatString("Cannot save image to \"{}\": Image is incompatible with the PNG format.", filepath)};
				}
				savePNG(image, filesystem, filepath);
				return;
			}
			if (extension == "jpg" || extension == "jpeg") {
				if (image.getDepth() != 1 || image.getMipLevelCount() != 1) {
					throw resource::Error{formatString("Cannot save image to \"{}\": Image is incompatible with the JPEG format.", filepath)};
				}
				saveJPEG(image, filesystem, filepath);
				return;
			}
			if (extension == "hdr") {
				if (image.getDepth() != 1 || image.getMipLevelCount() != 1) {
					throw resource::Error{formatString("Cannot save image to \"{}\": Image is incompatible with the HDR format.", filepath)};
				}
				saveHDR(image, filesystem, filepath);
				return;
			}
			if (extension == "ktx2") {
				saveKTX2(image, filesystem, filepath);
				return;
			}
			throw resource::Error{formatString("Cannot save image to \"{}\": Unsupported file format extension \".{}\".", filepath, extension)};
		}
	}

	if (image.getType() == ImageType::EMPTY) {
		throw resource::Error{formatString("Cannot save image to \"{}\": Image is empty.", filepath)};
	}

	if (image.getFormat() == ImageFormat::UNKNOWN) {
		throw resource::Error{formatString("Cannot save image to \"{}\": Image has unknown format.", filepath)};
	}

	if (image.getType() == ImageType::IMAGE_2D && image.getMipLevelCount() == 1) {
		switch (image.getFormat()) {
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: savePNG(image, filesystem, formatString("{}.png", filepath)); return;
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: saveHDR(image, filesystem, formatString("{}.hdr", filepath)); return;
			default: break;
		}
	}
	saveKTX2(image, filesystem, formatString("{}.ktx2", filepath));
}

Image::Image(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Span<const byte> contents) {
	GREM_ASSERT(type != ImageType::IMAGE_2D || size3D.depth == 1);
	GREM_ASSERT(type != ImageType::IMAGE_CUBE || size3D.depth == 6);
	GREM_ASSERT(type != ImageType::IMAGE_CUBE_ARRAY || size3D.depth % 6 == 0);
	const size_t specifiedSize = getSizeInBytes(format, size3D, mipLevelCount);
	GREM_ASSERT(specifiedSize == 0 || contents.empty() || contents.size() == specifiedSize);
	const size_t actualSize = (contents.empty()) ? specifiedSize : contents.size();
	if (actualSize > 0) {
		this->contents.resize(actualSize);
		if (!contents.empty()) {
			memcpy(this->contents.data(), contents.data(), contents.size());
		}
	}
	this->size3D = size3D;
	this->mipLevelCount = mipLevelCount;
	this->type = type;
	this->format = format;
}

Image::Image(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Allocation<byte> contents) {
	GREM_ASSERT(type != ImageType::IMAGE_2D || size3D.depth == 1);
	GREM_ASSERT(type != ImageType::IMAGE_CUBE || size3D.depth == 6);
	GREM_ASSERT(type != ImageType::IMAGE_CUBE_ARRAY || size3D.depth % 6 == 0);
	const size_t specifiedSize = getSizeInBytes(format, size3D, mipLevelCount);
	GREM_ASSERT(specifiedSize == 0 || contents.empty() || contents.size() == specifiedSize);
	if (contents.empty()) {
		this->contents.resize(specifiedSize);
	} else {
		this->contents = std::move(contents);
	}
	this->size3D = size3D;
	this->mipLevelCount = mipLevelCount;
	this->type = type;
	this->format = format;
}

Image::Image(const ImageView& image)
	: Image(image.getType(), image.getFormat(), image.getSize3D(), image.getMipLevelCount(), image.getContents()) {}

Image::Image(const Filesystem& filesystem, CStringView filepath, const ImageOptions& options) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load image {}", filepath));
	InputFileHandle file = filesystem.openInputFile(filepath);
	try {
		if (fileStartsWith(file, KTX2_IDENTIFIER)) {
			*this = loadKTX2Image(file.readBytesIntoAllocation(), options);
		} else if (fileStartsWith(file, PNG_IDENTIFIER)) {
			*this = loadPNGImage(file, options);
		} else if (fileStartsWith(file, JPEG_IDENTIFIER)) {
			*this = loadJPEGImage(file.readBytesIntoAllocation(), options);
		} else if (fileStartsWith(file, HDR_RADIANCE_IDENTIFIER) || fileStartsWith(file, HDR_RGBE_IDENTIFIER)) {
			*this = loadHDRImage(file, options);
		} else {
			throw resource::Error{"Unknown file type."};
		}
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to load image \"{}\":\n{}", filepath, e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to load image \"{}\":\n{}", filepath, e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to load image \"{}\": {}", filepath, e.what())};
	}
}

Image::Image(Span<const byte> fileContents, const ImageOptions& options) {
	GREM_PROFILE_FUNCTION();
	try {
		switch (determineFileType(fileContents)) {
			case ImageFileType::UNKNOWN: throw resource::Error{"Unknown file type."};
			case ImageFileType::JPEG: *this = loadJPEGImage(fileContents, options); break;
			case ImageFileType::PNG: *this = loadPNGImage(fileContents, options); break;
			case ImageFileType::HDR: *this = loadHDRImage(fileContents, options); break;
			case ImageFileType::KTX2: *this = loadKTX2Image(Allocation<byte>(fileContents.begin(), fileContents.end()), options); break;
		}
	} catch (const File::Error& e) {
		throw File::Error{formatString("Failed to load image:\n{}", e.what())};
	} catch (const Error& e) {
		throw resource::Error{formatString("Failed to load image:\n{}", e.what())};
	} catch (const std::length_error& e) {
		throw std::length_error{formatString("Failed to load image: {}", e.what())};
	}
}

} // namespace grem::resource
