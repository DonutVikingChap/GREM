// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_FORMATS_JPEG_HPP
#define GREM_RESOURCE_FORMATS_JPEG_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>

#include <csetjmp>   // std::jmp_buf, std::longjmp, setjump
#include <jerror.h>  // JERR_..., ERREXIT
#include <jpeglib.h> // JPEG_..., J..., TRUE, jpeg_..., j_...
#include <stdexcept> // std::length_error
#include <utility>   // std::move

namespace grem::resource {

inline constexpr Array<char, 2> JPEG_IDENTIFIER{'\xFF', '\xD8'};

[[nodiscard]] inline Image loadJPEGImage(Span<const byte> fileContents, const ImageOptions& options) {
	GREM_PROFILE_BLOCK("Load JPEG image");

	if (options.requiredType && *options.requiredType != ImageType::IMAGE_2D) {
		throw resource::Error{"Unexpected image type."};
	}

	if (fileContents.size() > static_cast<size_t>(Limits<unsigned long>::MAX)) {
		throw std::length_error{"Maximum JPEG file size exceeded."};
	}

	Image result{};

	struct jpeg_decompress_struct cinfo{};
	struct jpeg_error_mgr jerr{};

	std::jmp_buf onError{};
	if (setjmp(onError) != 0) {
		jpeg_destroy_decompress(&cinfo);
		Array<char, JMSG_LENGTH_MAX> message{};
		jerr.format_message(reinterpret_cast<j_common_ptr>(&cinfo), message.data());
		throw resource::Error{message.data()};
	}

	cinfo.client_data = &onError;
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = [](j_common_ptr cinfo) -> void {
		std::jmp_buf& onError = *static_cast<std::jmp_buf*>(cinfo->client_data);
		std::longjmp(onError, 1);
	};

	jpeg_create_decompress(&cinfo);
	// IMPORTANT:
	// We intentionally do not declare any variables with RAII types from here until jpeg_destroy_decompress!
	// The longjmp in the error handler might skip over destructors otherwise.

	jpeg_mem_src(&cinfo, reinterpret_cast<const unsigned char*>(fileContents.data()), static_cast<unsigned long>(fileContents.size()));

	jpeg_read_header(&cinfo, TRUE);

	cinfo.out_color_space = JCS_RGB;
	jpeg_start_decompress(&cinfo);

	if (cinfo.data_precision != 8) {
		ERREXIT1(&cinfo, JERR_BAD_PRECISION, cinfo.data_precision);
	}
	if (cinfo.out_color_components != 3) {
		ERREXIT(&cinfo, JERR_BAD_J_COLORSPACE);
	}

	const Extent2D size2D{.width = static_cast<uint32_t>(cinfo.output_width), .height = static_cast<uint32_t>(cinfo.output_height)};
	if (size2D.width > options.maxImageDimensions.width) {
		ERREXIT1(&cinfo, JERR_IMAGE_TOO_BIG, static_cast<unsigned>(options.maxImageDimensions.width));
	}
	if (size2D.height > options.maxImageDimensions.height) {
		ERREXIT1(&cinfo, JERR_IMAGE_TOO_BIG, static_cast<unsigned>(options.maxImageDimensions.height));
	}
	if (options.maxImageDimensions.depth == 0) {
		ERREXIT1(&cinfo, JERR_IMAGE_TOO_BIG, 0u);
	}
	size_t sizeInBytes = 0;
	try {
		sizeInBytes = Image::getSizeInBytes(ImageFormat::R8G8B8_UINT, size2D, 1);
	} catch (...) {
	}
	if (sizeInBytes == 0 || sizeInBytes > options.maxImageSizeInBytes) {
		ERREXIT(&cinfo, JERR_NO_BACKING_STORE);
	}
	try {
		result = Image{ImageType::IMAGE_2D, ImageFormat::R8G8B8_UINT, size2D, 1};
	} catch (...) {
	}
	if (result.getSize2D() != size2D) {
		ERREXIT1(&cinfo, JERR_OUT_OF_MEMORY, 0);
	}

	const size_t rowStride = static_cast<size_t>(cinfo.output_width) * Image::getPixelStride(ImageFormat::R8G8B8_UINT);
	while (cinfo.output_scanline < cinfo.output_height) {
		JSAMPROW rows[4];
		const JDIMENSION n = min(JDIMENSION{cinfo.output_height - cinfo.output_scanline}, JDIMENSION{4});
		for (JDIMENSION i = 0; i < n; ++i) {
			rows[i] = reinterpret_cast<JSAMPROW>(result.data() + (cinfo.output_scanline + i) * rowStride);
		}
		jpeg_read_scanlines(&cinfo, rows, n);
	}
	jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	if (options.requiredFormat) {
		if (*options.requiredFormat != ImageFormat::R8G8B8_UINT) {
			Image temporaryImage{ImageType::IMAGE_2D, *options.requiredFormat, size2D, 1};
			switch (*options.requiredFormat) {
				case ImageFormat::R8_UINT: Image::convertPixels<u8norm, 1, u8norm, 3>(size2D, 1, temporaryImage.data(), result.data()); break;
				case ImageFormat::R8G8_UINT: Image::convertPixels<u8norm, 2, u8norm, 3>(size2D, 1, temporaryImage.data(), result.data()); break;
				case ImageFormat::R8G8B8_UINT: unreachable();
				case ImageFormat::R8G8B8A8_UINT: Image::convertPixels<u8norm, 4, u8norm, 3>(size2D, 1, temporaryImage.data(), result.data()); break;
				default: throw resource::Error{"Incompatible required format specified for JPEG image."};
			}
			result = std::move(temporaryImage);
		}
	} else {
		Image temporaryImage{ImageType::IMAGE_2D, ImageFormat::R8G8B8A8_UINT, size2D, 1};
		Image::convertPixels<u8norm, 4, u8norm, 3>(size2D, 1, temporaryImage.data(), result.data());
		result = std::move(temporaryImage);
	}
	return result;
}

[[nodiscard]] inline Allocation<byte> saveJPEGImage(const ImageView& image, const ImageSaveJPEGOptions& options) {
	GREM_PROFILE_BLOCK("Save JPEG image");

	Allocation<byte> result{};

	if (options.subresource.layer >= image.getDepth()) {
		throw resource::Error{formatString("Invalid layer index {} (image has {} layers).", options.subresource.layer, image.getDepth())};
	}
	if (options.subresource.mipLevel >= image.getMipLevelCount()) {
		throw resource::Error{formatString("Invalid mip level index {} (image has {} mip levels).", options.subresource.mipLevel, image.getMipLevelCount())};
	}
	const ImageView layer = image.getLayer(options.subresource.layer, options.subresource.mipLevel);
	if (layer.getWidth() > JPEG_MAX_DIMENSION || layer.getHeight() > JPEG_MAX_DIMENSION) {
		throw std::length_error{"Image size overflow."};
	}

	const byte* pixels = nullptr;
	Allocation<byte> temporaryData{};
	switch (layer.getFormat()) {
		case ImageFormat::R8_UINT:
			temporaryData.resize(Image::getLayerStride(ImageFormat::R8G8B8_UINT, layer.getSize2D()));
			Image::convertPixels<u8norm, 3, u8norm, 1>(layer.getSize2D(), 1, temporaryData.data(), layer.data());
			pixels = temporaryData.data();
			break;
		case ImageFormat::R8G8_UINT:
			temporaryData.resize(Image::getLayerStride(ImageFormat::R8G8B8_UINT, layer.getSize2D()));
			Image::convertPixels<u8norm, 3, u8norm, 2>(layer.getSize2D(), 1, temporaryData.data(), layer.data());
			pixels = temporaryData.data();
			break;
		case ImageFormat::R8G8B8_UINT: pixels = layer.data(); break;
		case ImageFormat::R8G8B8A8_UINT:
			temporaryData.resize(Image::getLayerStride(ImageFormat::R8G8B8_UINT, layer.getSize2D()));
			Image::convertPixels<u8norm, 3, u8norm, 4>(layer.getSize2D(), 1, temporaryData.data(), layer.data());
			pixels = temporaryData.data();
			break;
		default: throw resource::Error{"Cannot save as JPEG since the image is not stored in a raw 8-bit unsigned integer format."};
	}

	struct jpeg_compress_struct cinfo{};
	struct jpeg_error_mgr jerr{};

	std::jmp_buf errorJumpState{};
	if (setjmp(errorJumpState) != 0) {
		jpeg_destroy_compress(&cinfo);
		Array<char, JMSG_LENGTH_MAX> message{};
		jerr.format_message(reinterpret_cast<j_common_ptr>(&cinfo), message.data());
		throw resource::Error{message.data()};
	}

	cinfo.client_data = &errorJumpState;
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = [](j_common_ptr cinfo) -> void {
		std::jmp_buf& onError = *static_cast<std::jmp_buf*>(cinfo->client_data);
		std::longjmp(onError, 1);
	};

	jpeg_create_compress(&cinfo);
	// IMPORTANT:
	// We intentionally do not declare any variables with RAII types from here until jpeg_destroy_compress!
	// The longjmp in the error handler might skip over destructors otherwise.

	cinfo.image_width = static_cast<JDIMENSION>(layer.getWidth());
	cinfo.image_height = static_cast<JDIMENSION>(layer.getHeight());
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	cinfo.data_precision = 8;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, options.quality, TRUE);

	unsigned char* outbuffer = nullptr;
	unsigned long outsize = 0;
	jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

	jpeg_start_compress(&cinfo, TRUE);
	const size_t rowStride = layer.getWidth() * Image::getPixelStride(ImageFormat::R8G8B8_UINT);
	while (cinfo.next_scanline < cinfo.image_height) {
		JSAMPROW row = reinterpret_cast<JSAMPROW>(const_cast<byte*>(pixels + cinfo.next_scanline * rowStride));
		jpeg_write_scanlines(&cinfo, &row, 1);
	}
	jpeg_finish_compress(&cinfo);

	try {
		result.resize(static_cast<size_t>(outsize));
	} catch (...) {
	}
	if (result.size() != static_cast<size_t>(outsize)) {
		ERREXIT1(&cinfo, JERR_OUT_OF_MEMORY, 0);
	}

	memcpy(result.data(), outbuffer, static_cast<size_t>(outsize));

	jpeg_destroy_compress(&cinfo);
	return result;
}

} // namespace grem::resource

#endif
