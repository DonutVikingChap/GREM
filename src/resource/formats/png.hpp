// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_FORMATS_PNG_HPP
#define GREM_RESOURCE_FORMATS_PNG_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formats/Adler32.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/deflate.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>

#include <stdexcept> // std::length_error

namespace grem::resource {

inline constexpr Array<char, 8> PNG_IDENTIFIER{'\x89', 'P', 'N', 'G', '\r', '\n', '\x1A', '\n'};

struct ZLIBStreamHeader {
	uint8_t compressionMethodAndFlags;
	uint8_t flags;
};

[[nodiscard]] inline Buffer<byte> decodeZLIBStreamToPNGData(Span<const byte> encodedData, size_t estimatedDecodedSizeInBytes, size_t maxDecodedSizeInBytes) {
	GREM_PROFILE_BLOCK("Decode ZLIB stream to PNG data");

	SpanReader reader{encodedData};
	const ZLIBStreamHeader streamHeader{
		.compressionMethodAndFlags = reader.readUInt8(),
		.flags = reader.readUInt8(),
	};
	if ((streamHeader.compressionMethodAndFlags * 256 + streamHeader.flags) % 31 != 0) {
		throw resource::Error{"Invalid ZLIB header in PNG data."};
	}
	if ((streamHeader.flags & 0b00100000) != 0) {
		throw resource::Error{"Invalid preset dictionary flag in ZLIB header of PNG data."};
	}
	const uint8_t compressionMethod = streamHeader.compressionMethodAndFlags & 0b00001111;
	if (compressionMethod != 8) {
		throw resource::Error{"Invalid compression method in ZLIB header of PNG data (expected deflate)."};
	}

	Buffer<byte> result = deflate::decompress(reader, {.estimatedDecompressedSizeInBytes = estimatedDecodedSizeInBytes, .maxDecompressedSizeInBytes = maxDecodedSizeInBytes});
	const uint32_t adler32 = reader.readUInt32BE();
	if (adler32 != static_cast<uint32_t>(Adler32{result})) {
		throw resource::Error{"Incorrect ZLIB checksum of uncompressed PNG data."};
	}
	return result;
}

[[nodiscard]] inline Buffer<byte> encodeZLIBStreamFromPNGData(Span<const byte> imageData, size_t compressionLevel) {
	GREM_PROFILE_BLOCK("Encode ZLIB stream from PNG data");

	Buffer<byte> result{};
	Writer writer{result};

	const ZLIBStreamHeader streamHeader{
		.compressionMethodAndFlags = 0b01111000,
		.flags = 0b01011110,
	};
	writer.write(asBytes(Span{&streamHeader, 1}));

	deflate::compress(writer, imageData, {.compressionLevel = compressionLevel});

	const Adler32 adler32{imageData};
	writer.writeUInt32BE(static_cast<uint32_t>(adler32));

	return result;
}

struct PNGNoneFilter {
	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		(void)previousOriginalRow;
		memcpy(filteredRow, originalRow.data(), originalRow.size());
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		filterRow(filteredRow, originalRow, nullptr);
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		(void)previousReconstructedRow;
		memcpy(reconstructedRow, filteredRow.data(), filteredRow.size());
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		reconstructRow(firstReconstructedRow, nullptr, filteredRow);
	}
};

struct PNGSubFilter {
	size_t filterBytes;

	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		(void)previousOriginalRow;
		GREM_ASSERT(filterBytes <= originalRow.size());
		memcpy(filteredRow, originalRow.data(), filterBytes);
		for (size_t x = filterBytes; x < originalRow.size(); ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origA = bit_cast<uint8_t>(originalRow[x - filterBytes]);
			const uint8_t filtX = static_cast<uint8_t>((origX - origA) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		filterRow(filteredRow, originalRow, nullptr);
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		(void)previousReconstructedRow;
		GREM_ASSERT(filterBytes <= filteredRow.size());
		memcpy(reconstructedRow, filteredRow.data(), filterBytes);
		for (size_t x = filterBytes; x < filteredRow.size(); ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconA = bit_cast<uint8_t>(reconstructedRow[x - filterBytes]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + reconA) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		reconstructRow(firstReconstructedRow, nullptr, filteredRow);
	}
};

struct PNGUpFilter {
	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		for (size_t x = 0; x < originalRow.size(); ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origB = bit_cast<uint8_t>(previousOriginalRow[x]);
			const uint8_t filtX = static_cast<uint8_t>((origX - origB) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		PNGNoneFilter{}.filterFirstRow(filteredRow, originalRow);
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		for (size_t x = 0; x < filteredRow.size(); ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconB = bit_cast<uint8_t>(previousReconstructedRow[x]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + reconB) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		PNGNoneFilter{}.reconstructFirstRow(firstReconstructedRow, filteredRow);
	}
};

struct PNGAverageFilter {
	size_t filterBytes;

	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		GREM_ASSERT(filterBytes <= originalRow.size());
		for (size_t x = 0; x < filterBytes; ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origB = bit_cast<uint8_t>(previousOriginalRow[x]);
			const uint8_t filtX = static_cast<uint8_t>((origX - origB / 2) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
		for (size_t x = filterBytes; x < originalRow.size(); ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origA = bit_cast<uint8_t>(originalRow[x - filterBytes]);
			const uint8_t origB = bit_cast<uint8_t>(previousOriginalRow[x]);
			const uint8_t filtX = static_cast<uint8_t>((origX - (origA + origB) / 2) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		GREM_ASSERT(filterBytes <= originalRow.size());
		memcpy(filteredRow, originalRow.data(), filterBytes);
		for (size_t x = filterBytes; x < originalRow.size(); ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origA = bit_cast<uint8_t>(originalRow[x - filterBytes]);
			const uint8_t filtX = static_cast<uint8_t>((origX - origA / 2) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		GREM_ASSERT(filterBytes <= filteredRow.size());
		for (size_t x = 0; x < filterBytes; ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconB = bit_cast<uint8_t>(previousReconstructedRow[x]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + reconB / 2) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
		for (size_t x = filterBytes; x < filteredRow.size(); ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconA = bit_cast<uint8_t>(reconstructedRow[x - filterBytes]);
			const uint8_t reconB = bit_cast<uint8_t>(previousReconstructedRow[x]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + (reconA + reconB) / 2) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		GREM_ASSERT(filterBytes <= filteredRow.size());
		memcpy(firstReconstructedRow, filteredRow.data(), filterBytes);
		for (size_t x = filterBytes; x < filteredRow.size(); ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconA = bit_cast<uint8_t>(firstReconstructedRow[x - filterBytes]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + reconA / 2) & 0xFF);
			firstReconstructedRow[x] = bit_cast<byte>(reconX);
		}
	}
};

struct PNGPaethFilter {
	[[nodiscard]] static constexpr uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
		const int threshold = int{c} * 3 - (int{a} + int{b});
		const int minValue{min(a, b)};
		const int maxValue{max(a, b)};
		int Pr = (maxValue <= threshold) ? minValue : int{c};
		Pr = (threshold <= minValue) ? maxValue : Pr;
		return static_cast<uint8_t>(Pr);
	}

	size_t filterBytes;

	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		GREM_ASSERT(filterBytes <= originalRow.size());
		for (size_t x = 0; x < filterBytes; ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origB = bit_cast<uint8_t>(previousOriginalRow[x]);
			const uint8_t filtX = static_cast<uint8_t>((origX - paethPredictor(0, origB, 0)) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
		for (size_t x = filterBytes; x < originalRow.size(); ++x) {
			const uint8_t origX = bit_cast<uint8_t>(originalRow[x]);
			const uint8_t origA = bit_cast<uint8_t>(originalRow[x - filterBytes]);
			const uint8_t origB = bit_cast<uint8_t>(previousOriginalRow[x]);
			const uint8_t origC = bit_cast<uint8_t>(previousOriginalRow[x - filterBytes]);
			const uint8_t filtX = static_cast<uint8_t>((origX - paethPredictor(origA, origB, origC)) & 0xFF);
			filteredRow[x] = bit_cast<byte>(filtX);
		}
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		PNGSubFilter{filterBytes}.filterFirstRow(filteredRow, originalRow);
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		GREM_ASSERT(filterBytes <= filteredRow.size());
		for (size_t x = 0; x < filterBytes; ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconB = bit_cast<uint8_t>(previousReconstructedRow[x]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + paethPredictor(0, reconB, 0)) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
		for (size_t x = filterBytes; x < filteredRow.size(); ++x) {
			const uint8_t filtX = bit_cast<uint8_t>(filteredRow[x]);
			const uint8_t reconA = bit_cast<uint8_t>(reconstructedRow[x - filterBytes]);
			const uint8_t reconB = bit_cast<uint8_t>(previousReconstructedRow[x]);
			const uint8_t reconC = bit_cast<uint8_t>(previousReconstructedRow[x - filterBytes]);
			const uint8_t reconX = static_cast<uint8_t>((filtX + paethPredictor(reconA, reconB, reconC)) & 0xFF);
			reconstructedRow[x] = bit_cast<byte>(reconX);
		}
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		PNGSubFilter{filterBytes}.reconstructFirstRow(firstReconstructedRow, filteredRow);
	}
};

struct PNGFilter : Variant<PNGNoneFilter, PNGSubFilter, PNGUpFilter, PNGAverageFilter, PNGPaethFilter> {
	PNGFilter(byte filterType, size_t filterBytes) {
		switch (filterType) {
			case byte{0}: emplace<PNGNoneFilter>(); break;
			case byte{1}: emplace<PNGSubFilter>(PNGSubFilter{filterBytes}); break;
			case byte{2}: emplace<PNGUpFilter>(); break;
			case byte{3}: emplace<PNGAverageFilter>(PNGAverageFilter{filterBytes}); break;
			case byte{4}: emplace<PNGPaethFilter>(PNGPaethFilter{filterBytes}); break;
			default: throw resource::Error{"Invalid PNG filter type."};
		}
	}

	void filterRow(byte* filteredRow, Span<const byte> originalRow, const byte* previousOriginalRow) const {
		match (*this)([&](const auto& filter) -> void { filter.filterRow(filteredRow, originalRow, previousOriginalRow); });
	}

	void filterFirstRow(byte* filteredRow, Span<const byte> originalRow) const {
		match (*this)([&](const auto& filter) -> void { filter.filterFirstRow(filteredRow, originalRow); });
	}

	void reconstructRow(byte* reconstructedRow, const byte* previousReconstructedRow, Span<const byte> filteredRow) const {
		match (*this)([&](const auto& filter) -> void { filter.reconstructRow(reconstructedRow, previousReconstructedRow, filteredRow); });
	}

	void reconstructFirstRow(byte* firstReconstructedRow, Span<const byte> filteredRow) const {
		match (*this)([&](const auto& filter) -> void { filter.reconstructFirstRow(firstReconstructedRow, filteredRow); });
	}
};

template <uint8_t BitDepth>
inline void writeLowBitDepthReconstructedRowToOutput(byte* output, const byte* reconstructedRow, size_t width, size_t channelCount, uint8_t componentScale) {
	static_assert(BitDepth > 0 && BitDepth < 8 && isPowerOf2(BitDepth));

	const size_t componentCount = width * channelCount;
	uint8_t inputByte = 0;
	for (uint32_t x = 0; x < componentCount; ++x) {
		if (x % (8 / BitDepth) == 0) {
			inputByte = bit_cast<uint8_t>(*reconstructedRow++);
		}
		*output++ = bit_cast<byte>(static_cast<uint8_t>((inputByte >> (8 - BitDepth)) * componentScale));
		inputByte <<= BitDepth;
	}
}

inline void writeReconstructedRowToOutput(byte* output, const byte* reconstructedRow, size_t width, size_t channelCount, size_t bitDepth, bool greyscale) {
	GREM_ASSERT(bitDepth > 0 && bitDepth <= 16 && isPowerOf2(bitDepth));
	switch (bitDepth) {
		case 1: writeLowBitDepthReconstructedRowToOutput<1>(output, reconstructedRow, width, channelCount, (greyscale) ? 0b11111111 : 1); break;
		case 2: writeLowBitDepthReconstructedRowToOutput<2>(output, reconstructedRow, width, channelCount, (greyscale) ? 0b01010101 : 1); break;
		case 4: writeLowBitDepthReconstructedRowToOutput<4>(output, reconstructedRow, width, channelCount, (greyscale) ? 0b00010001 : 1); break;
		case 8: memcpy(output, reconstructedRow, width * channelCount * sizeof(uint8_t)); break;
		case 16: memcpy(output, reconstructedRow, width * channelCount * sizeof(uint16_t)); break;
		default: unreachable();
	}
}

[[nodiscard]] inline Allocation<byte> buildImageFromRawPNGData(Extent2D size2D, size_t channelCount, size_t bitDepth, bool greyscale, Span<const byte> imageData) {
	GREM_PROFILE_BLOCK("Build image from raw PNG data");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(channelCount > 0 && channelCount <= 4);
	GREM_ASSERT(bitDepth > 0 && bitDepth <= 16 && isPowerOf2(bitDepth));

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	if (bitDepth > Limits<size_t>::MAX / imageSizeInPixels.x) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t bitsPerRowChannel = bitDepth * imageSizeInPixels.x;
	if (bitsPerRowChannel > Limits<size_t>::MAX / channelCount) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t bitsPerRow = bitsPerRowChannel * channelCount;
	if (bitsPerRow > Limits<size_t>::MAX - 7) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t bytesPerRow = (bitsPerRow + 7) / 8;
	if (bytesPerRow > Limits<size_t>::MAX / max(imageSizeInPixels.y, size_t{2})) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t imageSizeInBytes = bytesPerRow * imageSizeInPixels.y;
	if (imageSizeInBytes > Limits<size_t>::MAX - imageSizeInPixels.y) {
		throw std::length_error{"Image size overflow."};
	}
	if (imageData.size() < imageSizeInBytes + imageSizeInPixels.y) {
		throw resource::Error{"Invalid decoded PNG image data size."};
	}

	const size_t totalPixelCount = imageSizeInPixels.x * imageSizeInPixels.y;
	const size_t outputComponentSize = (bitDepth == 16) ? sizeof(uint16_t) : sizeof(uint8_t);
	const size_t outputPixelSize = channelCount * outputComponentSize;
	if (outputPixelSize > Limits<size_t>::MAX / totalPixelCount) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t outputRowStride = imageSizeInPixels.x * outputPixelSize;
	GREM_ASSERT(outputPixelSize > 0);
	const size_t filteredWidth = (bitDepth < 8) ? bytesPerRow : imageSizeInPixels.x;
	GREM_ASSERT(filteredWidth > 0);
	const size_t filteredRowSize = filteredWidth * outputPixelSize;
	GREM_ASSERT(filteredRowSize > 0);

	Allocation<byte> filterScratchBuffer(filteredRowSize * 2);
	Allocation<byte> result(totalPixelCount * outputPixelSize);
	byte* output = result.data();

	byte* const firstReconstructedRow = filterScratchBuffer.data();

	const byte firstFilterType = imageData.front();
	imageData = imageData.subspan(1);

	// The first scanline needs special handling in order to treat the non-existent previous reconstructed row as all 0s.
	PNGFilter{firstFilterType, outputPixelSize}.reconstructFirstRow(firstReconstructedRow, imageData.first(filteredRowSize));
	imageData = imageData.subspan(filteredRowSize);

	writeReconstructedRowToOutput(output, firstReconstructedRow, size2D.width, channelCount, bitDepth, greyscale);
	output += outputRowStride;
	for (size_t y = 1; y < size2D.height; ++y) {
		const size_t reconstructedRowIndex = y & 1;
		const size_t previousReconstructedRowIndex = ~y & 1;
		byte* const reconstructedRow = filterScratchBuffer.data() + reconstructedRowIndex * filteredRowSize;
		const byte* const previousReconstructedRow = filterScratchBuffer.data() + previousReconstructedRowIndex * filteredRowSize;

		const byte filterType = imageData.front();
		imageData = imageData.subspan(1);

		PNGFilter{filterType, outputPixelSize}.reconstructRow(reconstructedRow, previousReconstructedRow, imageData.first(filteredRowSize));
		imageData = imageData.subspan(filteredRowSize);

		writeReconstructedRowToOutput(output, reconstructedRow, size2D.width, channelCount, bitDepth, greyscale);
		output += outputRowStride;
	}

	return result;
}

[[nodiscard]] inline Allocation<byte> buildImageFromInterlacedPNGData(Extent2D size2D, size_t channelCount, size_t bitDepth, bool greyscale, Span<const byte> imageData) {
	GREM_PROFILE_BLOCK("Build image from interlaced PNG data");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(channelCount > 0 && channelCount <= 4);
	GREM_ASSERT(bitDepth > 0 && bitDepth <= 16 && isPowerOf2(bitDepth));

	constexpr Array<uzvec2, 7> DESTINATION_OFFSETS{{{0, 0}, {4, 0}, {0, 4}, {2, 0}, {0, 2}, {1, 0}, {0, 1}}};
	constexpr Array<uzvec2, 7> DESTINATION_SPACING{{{8, 8}, {8, 8}, {4, 8}, {4, 4}, {2, 4}, {2, 2}, {1, 2}}};

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	const size_t totalPixelCount = imageSizeInPixels.x * imageSizeInPixels.y;
	const size_t outputComponentSize = (bitDepth == 16) ? sizeof(uint16_t) : sizeof(uint8_t);
	const size_t outputPixelSize = channelCount * outputComponentSize;
	if (outputPixelSize > Limits<size_t>::MAX / totalPixelCount) {
		throw std::length_error{"Image size overflow."};
	}

	Allocation<byte> result(totalPixelCount * outputPixelSize);
	for (size_t i = 0; i < 7; ++i) {
		const uzvec2 destinationOffset = DESTINATION_OFFSETS[i];
		const uzvec2 destinationSpacing = DESTINATION_SPACING[i];
		const uzvec2 sourceSizeInPixels = (imageSizeInPixels - destinationOffset + destinationSpacing - uzvec2{1, 1}) / destinationSpacing;
		if (sourceSizeInPixels.x > 0 && sourceSizeInPixels.y > 0) {
			const size_t sourceSizeInBytes = (1 + ((sourceSizeInPixels.x * channelCount * bitDepth) + 7) / 8) * sourceSizeInPixels.y;
			if (imageData.size() < sourceSizeInBytes) {
				throw resource::Error{"Invalid decoded PNG image data size."};
			}
			const Extent2D sourceSize2D{.width = static_cast<uint32_t>(sourceSizeInPixels.x), .height = static_cast<uint32_t>(sourceSizeInPixels.y)};
			const Allocation<byte> sourceContents = buildImageFromRawPNGData(sourceSize2D, channelCount, bitDepth, greyscale, imageData.first(sourceSizeInBytes));
			for (size_t y = 0; y < sourceSizeInPixels.y; ++y) {
				for (size_t x = 0; x < sourceSizeInPixels.x; ++x) {
					const uzvec2 sourcePixel{x, y};
					const uzvec2 destinationPixel = destinationOffset + sourcePixel * destinationSpacing;
					const size_t sourceIndex = sourcePixel.y * sourceSizeInPixels.x + sourcePixel.x;
					const size_t destinationIndex = destinationPixel.y * imageSizeInPixels.x + destinationPixel.x;
					memcpy(result.data() + destinationIndex * outputPixelSize, sourceContents.data() + sourceIndex * outputPixelSize, outputPixelSize);
				}
			}
			imageData = imageData.subspan(sourceSizeInBytes);
		}
	}
	return result;
}

[[nodiscard]] inline Allocation<byte> buildDepalettizedImageFromPalettizedImage(Extent2D size2D, size_t newChannelCount, Span<const byte> palettizedImageContents,
	Span<const u8vec4, 256> palette) {
	GREM_PROFILE_BLOCK("Build depalettized image from palettized image");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(newChannelCount > 0 && newChannelCount <= 4);

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	const size_t totalPixelCount = imageSizeInPixels.x * imageSizeInPixels.y;
	GREM_ASSERT(palettizedImageContents.size() == totalPixelCount);
	if (newChannelCount > Limits<size_t>::MAX / totalPixelCount) {
		throw std::length_error{"Image size overflow."};
	}

	Allocation<byte> result(totalPixelCount * newChannelCount);
	for (size_t i = 0; i < totalPixelCount; ++i) {
		const uint8_t paletteIndex = bit_cast<uint8_t>(palettizedImageContents[i]);
		const u8vec4 pixel = palette[paletteIndex];
		memcpy(result.data() + i * newChannelCount, &pixel, newChannelCount);
	}
	return result;
}

[[nodiscard]] inline Allocation<byte> buildTransparentImageFromOpaqueImage(Extent2D size2D, size_t newChannelCount, size_t bitDepth, Span<const byte> opaqueImageContents,
	Span<const uint16_t> transparentSampleColor) {
	GREM_PROFILE_BLOCK("Build transparent image from opaque image");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(bitDepth > 0 && bitDepth <= 16 && isPowerOf2(bitDepth));

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	const size_t componentSize = (bitDepth == 16) ? sizeof(uint16_t) : sizeof(uint8_t);
	const size_t channelCount = transparentSampleColor.size();
	const size_t pixelSize = channelCount * componentSize;
	const size_t outputPixelSize = newChannelCount * componentSize;
	const size_t totalPixelCount = imageSizeInPixels.x * imageSizeInPixels.y;
	GREM_ASSERT((newChannelCount == 2 && channelCount == 1) || (newChannelCount == 4 && channelCount == 3));
	GREM_ASSERT(opaqueImageContents.size() == totalPixelCount * pixelSize);
	if (outputPixelSize > Limits<size_t>::MAX / totalPixelCount) {
		throw std::length_error{"Image size overflow."};
	}

	Array<byte, 6> transparentSamplePixel{};
	uint8_t componentScale{};
	switch (bitDepth) {
		case 1: componentScale = 0b11111111; break;
		case 2: componentScale = 0b01010101; break;
		case 4: componentScale = 0b00010001; break;
		case 8: componentScale = 0b00000001; break;
		case 16: memcpy(transparentSamplePixel.data(), transparentSampleColor.data(), channelCount * sizeof(uint16_t)); break;
		default: unreachable();
	}
	if (bitDepth != 16) {
		for (size_t i = 0; i < channelCount; ++i) {
			uint8_t componentValue{};
			memcpy(&componentValue, transparentSampleColor.data() + i, sizeof(uint8_t));
			transparentSamplePixel[i] = bit_cast<byte>(static_cast<uint8_t>(componentValue * componentScale));
		}
	}

	Allocation<byte> result(totalPixelCount * outputPixelSize);
	byte* output = result.data();
	const byte* input = opaqueImageContents.data();

	for (size_t i = 0; i < totalPixelCount; ++i) {
		const bool transparent = memcmp(input, transparentSamplePixel.data(), pixelSize) == 0;
		memcpy(output, input, pixelSize);
		memset(output + pixelSize, (transparent) ? 0x00 : 0xFF, componentSize);
		output += outputPixelSize;
		input += pixelSize;
	}
	return result;
}

[[nodiscard]] inline Image convertImageTo8BitFormat(ImageFormat format, Extent2D size2D, size_t channelCount, size_t bitDepth, Allocation<byte> imageContents) {
	GREM_PROFILE_BLOCK("Convert image to 8-bit format");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(channelCount > 0 && channelCount <= 4);
	GREM_ASSERT(bitDepth > 0 && bitDepth <= 16 && isPowerOf2(bitDepth));

	const size_t outputChannelCount = Image::getLogicalChannelCount(format);
	GREM_ASSERT(Image::getPixelStride(format) == outputChannelCount);
	if (outputChannelCount == channelCount && bitDepth != 16) {
		return Image{ImageType::IMAGE_2D, format, size2D, 1, std::move(imageContents)};
	}

	const size_t componentSize = (bitDepth == 16) ? sizeof(uint16_t) : sizeof(uint8_t);
	const size_t commonChannelCount = min(outputChannelCount, channelCount);
	GREM_ASSERT(commonChannelCount > 0 && commonChannelCount <= 4);
	const size_t extraOutputChannelCount = (outputChannelCount > channelCount) ? outputChannelCount - channelCount : 0;
	const size_t extraInputChannelCount = (outputChannelCount > channelCount) ? 0 : channelCount - outputChannelCount;
	GREM_ASSERT(extraOutputChannelCount <= 3);
	GREM_ASSERT(extraInputChannelCount <= 3);
	GREM_ASSERT(commonChannelCount + extraOutputChannelCount == outputChannelCount);
	GREM_ASSERT(commonChannelCount + extraInputChannelCount == channelCount);
	const size_t extraInputSize = extraInputChannelCount * componentSize;

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	const size_t totalPixelCount = imageSizeInPixels.x * imageSizeInPixels.y;

	Image result{ImageType::IMAGE_2D, format, size2D, 1};
	byte* output = result.data();
	const byte* input = imageContents.data();

	for (size_t i = 0; i < totalPixelCount; ++i) {
		for (size_t channelIndex = 0; channelIndex < commonChannelCount; ++channelIndex) {
			memcpy(output, input, sizeof(uint8_t));
			++output;
			input += componentSize;
		}
		output += extraOutputChannelCount;
		input += extraInputSize;
	}

	if (extraOutputChannelCount > 0) {
		constexpr Array<byte, 4> DEFAULT_PIXEL{byte{0}, byte{0}, byte{0}, byte{255}};
		output = result.data();
		for (size_t i = 0; i < totalPixelCount; ++i) {
			memcpy(output + commonChannelCount, DEFAULT_PIXEL.data() + commonChannelCount, extraOutputChannelCount);
			output += outputChannelCount;
		}
	}

	return result;
}

struct PNGChunkHeader {
	uint32_t length;
	Array<char, 4> chunkType;
};

[[nodiscard]] inline Image loadPNGImage(Reader reader, const ImageOptions& options) {
	GREM_PROFILE_BLOCK("Load PNG image");

	if (options.requiredType && *options.requiredType != ImageType::IMAGE_2D) {
		throw resource::Error{"Unexpected image type."};
	}

	if (reader.readChars<PNG_IDENTIFIER.size()>() != PNG_IDENTIFIER) {
		throw resource::Error{"Invalid PNG identifier."};
	}

	const PNGChunkHeader firstChunkHeader{
		.length = reader.readUInt32BE(),
		.chunkType = reader.readChars<4>(),
	};
	if (firstChunkHeader.chunkType != Array{'I', 'H', 'D', 'R'}) {
		throw resource::Error{"Invalid PNG chunk type of first chunk (expected IHDR)."};
	}
	if (firstChunkHeader.length != 13) {
		throw resource::Error{"Invalid PNG IHDR chunk length."};
	}

	const Extent2D size2D{
		.width = reader.readUInt32BE(),
		.height = reader.readUInt32BE(),
	};
	if (size2D.width == 0 || size2D.height == 0) {
		throw resource::Error{"Invalid PNG image dimensions."};
	}
	if (size2D.width > options.maxImageDimensions.width || size2D.height > options.maxImageDimensions.height || options.maxImageDimensions.depth == 0) {
		throw std::length_error{"Maximum image dimensions exceeded."};
	}

	const uint8_t bitDepth = reader.readUInt8();
	switch (bitDepth) {
		case 1: [[fallthrough]];
		case 2: [[fallthrough]];
		case 4: [[fallthrough]];
		case 8: [[fallthrough]];
		case 16: break;
		default: throw resource::Error{"Invalid PNG bit depth."};
	}

	const uint8_t colorType = reader.readUInt8();
	ImageFormat format{};
	size_t imageChannelCount{};
	switch (colorType) {
		case 0b000: // Greyscale (0):
			format = ImageFormat::R8_UINT;
			imageChannelCount = 1;
			break;
		case 0b010: // Truecolor (2):
			format = ImageFormat::R8G8B8_UINT;
			imageChannelCount = 3;
			if (bitDepth != 8 && bitDepth != 16) {
				throw resource::Error{"Invalid PNG bit depth and color type combination."};
			}
			break;
		case 0b011: // Indexed-color (3):
			format = ImageFormat::R8G8B8_UINT;
			imageChannelCount = 1;
			if (bitDepth == 16) {
				throw resource::Error{"Invalid PNG bit depth and color type combination."};
			}
			break;
		case 0b100: // Greyscale with alpha (4):
			format = ImageFormat::R8G8_UINT;
			imageChannelCount = 2;
			if (bitDepth != 8 && bitDepth != 16) {
				throw resource::Error{"Invalid PNG bit depth and color type combination."};
			}
			break;
		case 0b110: // Truecolor with alpha (6):
			format = ImageFormat::R8G8B8A8_UINT;
			imageChannelCount = 4;
			if (bitDepth != 8 && bitDepth != 16) {
				throw resource::Error{"Invalid PNG bit depth and color type combination."};
			}
			break;
		default: throw resource::Error{"Invalid PNG color type."};
	}
	const bool greyscale = colorType == 0b000;
	const bool alphaUsed = (colorType & 0b100) != 0;
	const bool truecolorUsed = (colorType & 0b010) != 0;
	const bool paletteUsed = (colorType & 0b001) != 0;

	const uint8_t compressionMethod = reader.readUInt8();
	if (compressionMethod != 0) {
		throw resource::Error{"Invalid PNG compression method."};
	}

	const uint8_t filterMethod = reader.readUInt8();
	if (filterMethod != 0) {
		throw resource::Error{"Invalid PNG filter method."};
	}

	const uint8_t interlaceMethod = reader.readUInt8();
	if (interlaceMethod != 0 && interlaceMethod != 1) {
		throw resource::Error{"Invalid PNG interlace method."};
	}
	const bool interlaced = interlaceMethod != 0;

	reader.skip(4); // Skip CRC of image header chunk.

	InplaceArrayList<u8vec4, 256> palette{};
	Array<uint16_t, 3> transparentSampleColor{};
	Buffer<byte> imageDataBuffer{};
	bool foundTransparencyChunk = false;
	bool foundImageDataChunk = false;
	while (true) {
		const PNGChunkHeader chunkHeader{
			.length = reader.readUInt32BE(),
			.chunkType = reader.readChars<4>(),
		};

		if (chunkHeader.chunkType == Array{'P', 'L', 'T', 'E'}) {
			// Palette chunk.
			if (foundImageDataChunk) {
				throw resource::Error{"Invalid PNG chunk order (PLTE after IDAT)."};
			}
			if (chunkHeader.length == 0 || chunkHeader.length > 256 * 3 || chunkHeader.length % 3 != 0) {
				throw resource::Error{"Invalid PNG PLTE chunk length."};
			}
			palette.resize(chunkHeader.length / 3);
			for (u8vec4& color : palette) {
				color = {reader.readUInt8(), reader.readUInt8(), reader.readUInt8(), 255};
			}
		} else if (chunkHeader.chunkType == Array{'t', 'R', 'N', 'S'}) {
			// Transparency chunk.
			if (foundTransparencyChunk) {
				throw resource::Error{"Multiple tRNS chunks in PNG."};
			}
			if (foundImageDataChunk) {
				throw resource::Error{"Invalid PNG chunk order (tRNS after IDAT)."};
			}
			if (paletteUsed) {
				if (palette.empty()) {
					throw resource::Error{"Missing PNG PLTE chunk before tRNS."};
				}
				if (chunkHeader.length > palette.size()) {
					throw resource::Error{"Invalid PNG tRNS chunk length."};
				}
				for (uint32_t i = 0; i < chunkHeader.length; ++i) {
					palette[i][3] = reader.readUInt8();
				}
				GREM_ASSERT(format == ImageFormat::R8G8B8_UINT);
				format = ImageFormat::R8G8B8A8_UINT;
			} else {
				if (alphaUsed) {
					throw resource::Error{"Unexpected tRNS chunk in PNG with alpha color format."};
				}
				if (chunkHeader.length != imageChannelCount * sizeof(uint16_t)) {
					throw resource::Error{"Invalid PNG tRNS chunk length."};
				}
				for (size_t i = 0; i < imageChannelCount; ++i) {
					transparentSampleColor[i] = reader.readUInt16BE();
				}
				switch (imageChannelCount) {
					case 1: format = ImageFormat::R8G8_UINT; break;
					case 3: format = ImageFormat::R8G8B8A8_UINT; break;
					default: unreachable();
				}
			}
			foundTransparencyChunk = true;
		} else if (chunkHeader.chunkType == Array{'I', 'D', 'A', 'T'}) {
			// Image data chunk.
			if (paletteUsed && palette.empty()) {
				throw resource::Error{"Missing PNG PLTE chunk before IDAT."};
			}
			if (!foundImageDataChunk) {
				if (options.requiredFormat) {
					switch (*options.requiredFormat) {
						case ImageFormat::R8_UINT: [[fallthrough]];
						case ImageFormat::R8G8_UINT: [[fallthrough]];
						case ImageFormat::R8G8B8_UINT: [[fallthrough]];
						case ImageFormat::R8G8B8A8_UINT: format = *options.requiredFormat; break;
						default: throw resource::Error{"Incompatible desired format specified for PNG image."};
					}
				} else if (format == ImageFormat::R8G8B8_UINT) {
					format = ImageFormat::R8G8B8A8_UINT;
				}
				if (Image::getSizeInBytes(format, size2D, 1) > options.maxImageSizeInBytes) {
					throw std::length_error{"Maximum image size exceeded."};
				}
				foundImageDataChunk = true;
			}
			const size_t imageDataBufferOffset = imageDataBuffer.size();
			if (size_t{chunkHeader.length} > options.maxImageSizeInBytes - imageDataBufferOffset) {
				throw std::length_error{"Maximum image size exceeded."};
			}
			imageDataBuffer.resize(imageDataBufferOffset + chunkHeader.length);
			reader.read(Span{imageDataBuffer}.subspan(imageDataBufferOffset));
		} else if (chunkHeader.chunkType == Array{'I', 'E', 'N', 'D'}) {
			// Image trailer chunk.
			if (!foundImageDataChunk) {
				throw resource::Error{"Missing PNG IDAT chunk before IEND."};
			}
			if (chunkHeader.length != 0) {
				throw resource::Error{"Invalid PNG IEND chunk length."};
			}
			reader.skip(4); // Skip CRC of chunk.
			const uzvec2 imageSizeInPixels{u32vec2{size2D}};
			const size_t bitsPerDecodedRow = imageSizeInPixels.x * size_t{bitDepth} * imageChannelCount;
			const size_t bytesPerDecodedRow = (bitsPerDecodedRow + 7) / 8;
			const size_t estimatedDecodedSizeInBytes = (1 + bytesPerDecodedRow) * imageSizeInPixels.y;
			imageDataBuffer = decodeZLIBStreamToPNGData(imageDataBuffer, estimatedDecodedSizeInBytes, options.maxImageSizeInBytes);
			Allocation<byte> imageContents = (interlaced) ? buildImageFromInterlacedPNGData(size2D, imageChannelCount, bitDepth, greyscale, imageDataBuffer)
			                                              : buildImageFromRawPNGData(size2D, imageChannelCount, bitDepth, greyscale, imageDataBuffer);
			if (paletteUsed) {
				GREM_ASSERT(imageChannelCount == 1);
				GREM_ASSERT(!palette.empty());
				GREM_ASSERT(bitDepth != 16);
				const size_t newChannelCount = (foundTransparencyChunk) ? 4 : 3;
				palette.resize(256, u8vec4{}); // Fill unspecified entries with 0s in case the image contains out-of-bounds palette indices.
				imageContents = buildDepalettizedImageFromPalettizedImage(size2D, newChannelCount, imageContents, Span<const u8vec4, 256>{palette});
				imageChannelCount = newChannelCount;
			} else if (foundTransparencyChunk) {
				GREM_ASSERT(imageChannelCount == 1 || imageChannelCount == 3);
				const size_t newChannelCount = (imageChannelCount == 1) ? 2 : 4;
				imageContents = buildTransparentImageFromOpaqueImage(size2D, newChannelCount, bitDepth, imageContents, Span{transparentSampleColor}.first(imageChannelCount));
				imageChannelCount = newChannelCount;
			}
			return convertImageTo8BitFormat(format, size2D, imageChannelCount, bitDepth, std::move(imageContents));
		} else if (chunkHeader.chunkType == Array{'I', 'H', 'D', 'R'}) {
			// Image header chunk (already read at the start).
			throw resource::Error{"Multiple IHDR chunks in PNG."};
		} else {
			// Unknown/ignored chunk.
			const bool ancillaryChunk = (chunkHeader.chunkType[0] & 0b00100000) != 0;
			const bool criticalChunk = !ancillaryChunk;
			if (criticalChunk) {
				throw resource::Error{formatString("Unknown critical PNG chunk type \"{}\".", StringView{chunkHeader.chunkType.data(), chunkHeader.chunkType.size()})};
			}
			reader.skip(chunkHeader.length);
		}
		reader.skip(4); // Skip CRC of chunk.
	}
	unreachable();
}

[[nodiscard]] inline int64_t getEstimatedEntropy(Span<const byte> data) {
	int64_t result = 0;
	for (const byte byte : data) {
		result += abs(bit_cast<int8_t>(byte));
	}
	return result;
}

[[nodiscard]] inline Allocation<byte> buildPNGDataFrom8BitImage(Extent2D size2D, size_t channelCount, Span<const byte> imageContents, byte filterTypeOverride) {
	GREM_PROFILE_BLOCK("Build PNG data from 8-bit image");

	GREM_ASSERT(size2D.width > 0 && size2D.height > 0);
	GREM_ASSERT(size_t{size2D.width} <= Limits<size_t>::MAX / size_t{size2D.height});
	GREM_ASSERT(channelCount > 0 && channelCount <= 4);

	const uzvec2 imageSizeInPixels{u32vec2{size2D}};
	const size_t imageRowStride = imageSizeInPixels.x * channelCount;
	const size_t outputRowStride = 1 + imageRowStride;

	Allocation<byte> result(outputRowStride * imageSizeInPixels.y);
	byte* output = result.data();
	const byte* input = imageContents.data();
	if (bit_cast<uint8_t>(filterTypeOverride) <= 4) {
		const PNGFilter filter{filterTypeOverride, channelCount};

		*output = filterTypeOverride;
		filter.filterFirstRow(output + 1, Span{input, imageRowStride});
		output += outputRowStride;
		input += imageRowStride;

		for (size_t y = 1; y < imageSizeInPixels.y; ++y) {
			*output = filterTypeOverride;
			filter.filterRow(output + 1, Span{input, imageRowStride}, input - imageRowStride);
			output += outputRowStride;
			input += imageRowStride;
		}
	} else {
		byte lowestEstimatedEntropyFilterType{};
		int64_t lowestEstimatedEntropy = Limits<int64_t>::MAX;
		for (uint8_t filterTypeIndex = 0; filterTypeIndex < 5; ++filterTypeIndex) {
			const byte filterType = bit_cast<byte>(filterTypeIndex);
			PNGFilter{filterType, channelCount}.filterFirstRow(output + 1, Span{input, imageRowStride});
			const int64_t estimatedEntropy = getEstimatedEntropy(Span{output + 1, imageRowStride});
			if (estimatedEntropy < lowestEstimatedEntropy) {
				lowestEstimatedEntropy = estimatedEntropy;
				lowestEstimatedEntropyFilterType = filterType;
			}
		}
		if (lowestEstimatedEntropyFilterType != byte{4}) {
			PNGFilter{lowestEstimatedEntropyFilterType, channelCount}.filterFirstRow(output + 1, Span{input, imageRowStride});
		}
		*output = lowestEstimatedEntropyFilterType;
		output += outputRowStride;
		input += imageRowStride;

		for (size_t y = 1; y < imageSizeInPixels.y; ++y) {
			lowestEstimatedEntropy = Limits<int64_t>::MAX;
			for (uint8_t filterTypeIndex = 0; filterTypeIndex < 5; ++filterTypeIndex) {
				const byte filterType = bit_cast<byte>(filterTypeIndex);
				PNGFilter{filterType, channelCount}.filterRow(output + 1, Span{input, imageRowStride}, input - imageRowStride);
				const int64_t estimatedEntropy = getEstimatedEntropy(Span{output + 1, imageRowStride});
				if (estimatedEntropy < lowestEstimatedEntropy) {
					lowestEstimatedEntropy = estimatedEntropy;
					lowestEstimatedEntropyFilterType = filterType;
				}
			}
			if (lowestEstimatedEntropyFilterType != byte{4}) {
				PNGFilter{lowestEstimatedEntropyFilterType, channelCount}.filterRow(output + 1, Span{input, imageRowStride}, input - imageRowStride);
			}
			*output = lowestEstimatedEntropyFilterType;
			output += outputRowStride;
			input += imageRowStride;
		}
	}
	return result;
}

inline void savePNGImage(const ImageView& image, const ImageSavePNGOptions& options, Writer writer) {
	GREM_PROFILE_BLOCK("Save PNG image");

	if (options.subresource.layer >= image.getDepth()) {
		throw resource::Error{formatString("Invalid layer index {} (image has {} layers).", options.subresource.layer, image.getDepth())};
	}
	if (options.subresource.mipLevel >= image.getMipLevelCount()) {
		throw resource::Error{formatString("Invalid mip level index {} (image has {} mip levels).", options.subresource.mipLevel, image.getMipLevelCount())};
	}
	const ImageView layer = image.getLayer(options.subresource.layer, options.subresource.mipLevel);
	if (layer.getWidth() == 0 || layer.getHeight() == 0) {
		throw resource::Error{"Cannot save an empty image as PNG."};
	}

	size_t channelCount{};
	uint8_t colorType{};
	switch (layer.getFormat()) {
		case ImageFormat::R8_UINT:
			channelCount = 1;
			colorType = 0b000; // Greyscale (0).
			break;
		case ImageFormat::R8G8_UINT:
			channelCount = 2;
			colorType = 0b100; // Greyscale with alpha (4).
			break;
		case ImageFormat::R8G8B8_UINT:
			channelCount = 3;
			colorType = 0b010; // Truecolor (2).
			break;
		case ImageFormat::R8G8B8A8_UINT:
			channelCount = 4;
			colorType = 0b110; // Truecolor with alpha (6).
			break;
		default: throw resource::Error{"Cannot save as PNG since the image is not stored in a raw 8-bit unsigned integer format."};
	}

	writer.write(asBytes(Span{PNG_IDENTIFIER}));

	struct IHDRChunk {
		uint32_t width;
		uint32_t height;
		uint8_t bitDepth;
		uint8_t colorType;
		uint8_t compressionMethod;
		uint8_t filterMethod;
		uint8_t interlaceMethod;
	};
	const IHDRChunk imageHeaderChunk{
		.width = convertHostEndianToBigEndian(layer.getWidth()),
		.height = convertHostEndianToBigEndian(layer.getHeight()),
		.bitDepth = 8,
		.colorType = colorType,
		.compressionMethod = 0,
		.filterMethod = 0,
		.interlaceMethod = 0,
	};
	const Span<const byte, 13> imageHeaderChunkData = asBytes(Span{&imageHeaderChunk, 1}).first<13>();

	writer.writeUInt32BE(static_cast<uint32_t>(imageHeaderChunkData.size()));
	writer.writeChars("IHDR");
	writer.write(imageHeaderChunkData);
	writer.writeUInt32BE(static_cast<uint32_t>(CRC32{"IHDR"} + imageHeaderChunkData));

	const byte filterTypeOverride = bit_cast<byte>(options.filterTypeOverride.value_or(ImageSavePNGOptions::FilterType{5}));
	const Allocation<byte> imageData = buildPNGDataFrom8BitImage(layer.getSize2D(), channelCount, layer.getContents(), filterTypeOverride);
	const Buffer<byte> imageDataChunkData = encodeZLIBStreamFromPNGData(imageData, options.compressionLevel);
	writer.writeUInt32BE(imageDataChunkData.size());
	writer.writeChars("IDAT");
	writer.write(imageDataChunkData);
	writer.writeUInt32BE(static_cast<uint32_t>(CRC32{"IDAT"} + imageDataChunkData));

	writer.writeUInt32BE(0);
	writer.writeChars("IEND");
	writer.writeUInt32BE(static_cast<uint32_t>(CRC32{"IEND"}));
}

} // namespace grem::resource

#endif
