// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/formats/deflate.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>

#include <stdexcept> // std::invalid_argument, std::length_error

namespace grem::deflate {

namespace {

[[nodiscard]] constexpr uint16_t getBitsReversed(uint16_t bits, uint32_t n) {
	GREM_ASSERT(n <= 16);
	return getBitReversed(bits) >> (16 - n);
}

class BitSpanReader {
public:
	BitSpanReader(SpanReader reader)
		: reader(reader) {}

	[[nodiscard]] uint16_t readBits(uint32_t n) {
		GREM_ASSERT(n <= 16);
		while (unreadBitCount < n) {
			unreadBits |= uint32_t{reader.readUInt8()} << unreadBitCount;
			unreadBitCount += 8;
		}
		const uint16_t result = static_cast<uint16_t>(unreadBits & ((1 << n) - 1));
		unreadBits >>= n;
		unreadBitCount -= n;
		return result;
	}

	void skipBits(uint32_t n) {
		GREM_ASSERT(n <= 16);
		while (unreadBitCount < n) {
			unreadBits |= uint32_t{reader.readUInt8()} << unreadBitCount;
			unreadBitCount += 8;
		}
		unreadBits >>= n;
		unreadBitCount -= n;
	}

	[[nodiscard]] uint16_t peekBits(uint32_t n) const {
		GREM_ASSERT(n <= 16);
		uint32_t bits = unreadBits;
		uint32_t bitCount = unreadBitCount;
		size_t offset = 0;
		while (bitCount < n) {
			if (const Span<const byte> upcomingBytes = reader.peek(offset + 1); offset < upcomingBytes.size()) {
				bits |= uint32_t{bit_cast<uint8_t>(upcomingBytes[offset])} << bitCount;
				bitCount += 8;
				++offset;
			} else {
				break;
			}
		}
		return static_cast<uint16_t>(bits & ((1 << n) - 1));
	}

	void readAlignedBytes(Span<byte> data) {
		// Skip remaining bits in partially processed byte.
		const size_t unreadBitsInByte = unreadBitCount % 8;
		while (unreadBitCount < unreadBitsInByte) {
			unreadBits |= uint32_t{reader.readUInt8()} << unreadBitCount;
			unreadBitCount += 8;
		}
		unreadBits >>= unreadBitsInByte;
		unreadBitCount -= unreadBitsInByte;

		// Read from unread bits.
		while (unreadBitCount != 0) {
			if (data.empty()) {
				return;
			}
			data.front() = bit_cast<byte>(static_cast<uint8_t>(unreadBits & 0xFF));
			data = data.subspan(1);
			unreadBits >>= 8;
			unreadBitCount -= 8;
		}

		// Unread bits exhausted. Read the rest directly from the reader.
		reader.read(data);
	}

private:
	SpanReader reader;
	uint32_t unreadBits = 0;
	uint32_t unreadBitCount = 0;
};

class BitWriter {
public:
	BitWriter(Writer writer)
		: writer(writer) {}

	void writeBits(uint16_t bits, uint32_t n) {
		GREM_ASSERT(n <= 16);
		outgoingBits |= uint32_t{bits} << outgoingBitCount;
		outgoingBitCount += n;
		while (outgoingBitCount >= 8) {
			writer.writeByte(bit_cast<byte>(static_cast<uint8_t>(outgoingBits & 0xFF)));
			outgoingBits >>= 8;
			outgoingBitCount -= 8;
		}
	}

	void writeAlignedBytes(Span<const byte> data) {
		GREM_ASSERT(outgoingBitCount < 8);

		// Write zero bits up to byte boundary.
		if (outgoingBitCount != 0) {
			writeBits(0, 8 - outgoingBitCount);
		}

		// Write bytes directly to the writer.
		writer.write(data);
	}

private:
	Writer writer;
	uint32_t outgoingBits = 0;
	uint32_t outgoingBitCount = 0;
};

struct BlockHeader {
	uint8_t blockFinal : 1;
	uint8_t blockType : 2;
};

struct NonCompressedBlockHeader {
	uint16_t length;
	uint16_t lengthComplement;
};

struct HuffmanDecoder {
	struct DecodeResult {
		uint16_t codeLength : 5;
		uint16_t value : 9;
	};

	static constexpr Array<uint8_t, 288> FIXED_LENGTH_CODE_LENGTHS = [] {
		Array<uint8_t, 288> result{};
		for (size_t i = 0; i <= 143; ++i) {
			result[i] = 8;
		}
		for (size_t i = 144; i <= 255; ++i) {
			result[i] = 9;
		}
		for (size_t i = 256; i <= 279; ++i) {
			result[i] = 7;
		}
		for (size_t i = 280; i <= 287; ++i) {
			result[i] = 8;
		}
		return result;
	}();

	static constexpr Array<uint8_t, 32> FIXED_DISTANCE_CODE_LENGTHS = [] {
		Array<uint8_t, 32> result{};
		result.fill(5);
		return result;
	}();

	static constexpr Array<uint16_t, 29> LENGTH_CODE_LENGTHS{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
	static constexpr Array<uint8_t, 29> LENGTH_CODE_EXTRA_BIT_COUNTS{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

	static constexpr Array<uint16_t, 30> DISTANCE_CODE_DISTANCES{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
		8193, 12289, 16385, 24577};
	static constexpr Array<uint8_t, 30> DISTANCE_CODE_EXTRA_BIT_COUNTS{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

	static constexpr size_t LOOKUP_TABLE_BIT_COUNT = 9;

	Array<uint16_t, 16> firstReverseCodes{};
	Array<uint16_t, 16> firstSymbols{};
	Array<uint32_t, 17> maxReverseCodes{};
	Array<DecodeResult, 288> values{};
	Array<DecodeResult, 1 << LOOKUP_TABLE_BIT_COUNT> lookupTable{};

	explicit HuffmanDecoder(Span<const uint8_t> codeLengths) {
		GREM_PROFILE_BLOCK("Construct Huffman decoder");

		GREM_ASSERT(codeLengths.size() <= 288 + 32);

		Array<uint16_t, 16> counts{};
		for (const uint8_t codeLength : codeLengths) {
			GREM_ASSERT(codeLength < 16);
			++counts[codeLength];
		}
		for (uint8_t codeLength = 1; codeLength < 16; ++codeLength) {
			if (counts[codeLength] > (1 << codeLength)) {
				throw std::invalid_argument{"Invalid Huffman decoder sizes in deflate-compressed data."};
			}
		}

		counts.front() = 0;

		Array<uint32_t, 16> nextReverseCode{};
		{
			uint32_t reverseCode = 0;
			uint32_t symbol = 0;
			for (uint8_t codeLength = 1; codeLength < 16; ++codeLength) {
				const size_t count = counts[codeLength];
				nextReverseCode[codeLength] = reverseCode;
				firstReverseCodes[codeLength] = static_cast<uint16_t>(reverseCode);
				firstSymbols[codeLength] = static_cast<uint16_t>(symbol);
				reverseCode += count;
				if (count != 0 && reverseCode - 1 >= (1 << codeLength)) {
					throw std::invalid_argument{"Invalid Huffman decoder sizes in deflate-compressed data."};
				}
				maxReverseCodes[codeLength] = reverseCode << (16 - codeLength);
				reverseCode <<= 1;
				symbol += count;
			}
			maxReverseCodes[16] = Limits<uint32_t>::MAX;
		}

		for (uint16_t value = 0; value < codeLengths.size(); ++value) {
			if (const uint8_t codeLength = codeLengths[value]; codeLength != 0) {
				const uint16_t symbol = firstSymbols[codeLength] + nextReverseCode[codeLength] - firstReverseCodes[codeLength];
				if (codeLength <= LOOKUP_TABLE_BIT_COUNT) {
					uint16_t code = getBitsReversed(static_cast<uint16_t>(nextReverseCode[codeLength]), codeLength);
					while (code < (1 << LOOKUP_TABLE_BIT_COUNT)) {
						lookupTable[code] = {.codeLength = codeLength, .value = value};
						code += 1 << codeLength;
					}
				}
				values[symbol] = {.codeLength = codeLength, .value = value};
				++nextReverseCode[codeLength];
			}
		}
	}

	[[nodiscard]] DecodeResult decode(uint16_t code) const {
		if (const DecodeResult result = lookupTable[code & ((1 << LOOKUP_TABLE_BIT_COUNT) - 1)]; result.codeLength != 0) {
			return result;
		}
		const uint16_t reverseCode = getBitReversed(code);
		uint8_t codeLength = LOOKUP_TABLE_BIT_COUNT + 1;
		while (uint32_t{reverseCode} >= maxReverseCodes[codeLength]) {
			++codeLength;
		}
		if (codeLength >= 16) {
			throw std::invalid_argument{"Invalid Huffman code in deflate-compressed data."};
		}
		const uint32_t symbol = firstSymbols[codeLength] + (reverseCode >> (16 - codeLength)) - firstReverseCodes[codeLength];
		if (symbol >= 288) {
			throw std::invalid_argument{"Invalid Huffman code in deflate-compressed data."};
		}
		const DecodeResult result = values[symbol];
		if (result.codeLength != codeLength) {
			throw std::invalid_argument{"Invalid Huffman code in deflate-compressed data."};
		}
		return result;
	}
};

struct BlockHuffmanDecoders {
	HuffmanDecoder lengthDecoder;
	HuffmanDecoder distanceDecoder;
};

[[nodiscard]] BlockHuffmanDecoders readDynamicBlockHuffmanDecoders(BitSpanReader& bitReader) {
	GREM_PROFILE_BLOCK("Read dynamic block Huffman decoders");

	const uint16_t lengthCodeCount = bitReader.readBits(5) + 257;
	const uint16_t distanceCodeCount = bitReader.readBits(5) + 1;
	const uint16_t codeLengthCodeCount = bitReader.readBits(4) + 4;
	const size_t totalCodeCount = lengthCodeCount + distanceCodeCount;

	constexpr Array<uint8_t, 19> CODE_LENGTH_TABLE{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	Array<uint8_t, 19> codeLengthCodeLengths{};
	for (size_t i = 0; i < codeLengthCodeCount; ++i) {
		codeLengthCodeLengths[CODE_LENGTH_TABLE[i]] = static_cast<uint8_t>(bitReader.readBits(3));
	}
	const HuffmanDecoder codeLengthDecoder{codeLengthCodeLengths};

	InplaceArrayList<uint8_t, 288 + 32> codeLengths{};
	while (codeLengths.size() < totalCodeCount) {
		size_t count{};
		uint8_t codeLength{};
		const HuffmanDecoder::DecodeResult decoded = codeLengthDecoder.decode(bitReader.peekBits(16));
		bitReader.skipBits(decoded.codeLength);
		if (decoded.value < 16) {
			count = 1;
			codeLength = static_cast<uint8_t>(decoded.value);
		} else if (decoded.value == 16) {
			if (codeLengths.empty()) {
				throw std::invalid_argument{"Invalid Huffman code length in deflate-compressed data."};
			}
			count = bitReader.readBits(2) + 3;
			codeLength = codeLengths.back();
		} else if (decoded.value == 17) {
			count = bitReader.readBits(3) + 3;
			codeLength = 0;
		} else if (decoded.value == 18) {
			count = bitReader.readBits(7) + 11;
			codeLength = 0;
		} else {
			throw std::invalid_argument{"Invalid Huffman code length in deflate-compressed data."};
		}
		if (count > totalCodeCount - codeLengths.size()) {
			throw std::invalid_argument{"Invalid Huffman code length in deflate-compressed data."};
		}
		codeLengths.insert(codeLengths.end(), count, codeLength);
	}
	return {
		.lengthDecoder{Span{codeLengths}.first(lengthCodeCount)},
		.distanceDecoder{Span{codeLengths}.subspan(lengthCodeCount, distanceCodeCount)},
	};
}

void readNonCompressedBlock(Buffer<byte>& output, BitSpanReader& bitReader, size_t maxOutputSize) {
	GREM_PROFILE_BLOCK("Read non-compressed deflate block");

	NonCompressedBlockHeader header{};
	bitReader.readAlignedBytes(asWritableBytes(Span{&header, 1}));
	if (header.lengthComplement != ~header.length) {
		throw std::invalid_argument{"Invalid non-compressed deflate block header in deflate-compressed data."};
	}
	const size_t dataLength{convertLittleEndianToHostEndian(header.length)};
	if (dataLength > maxOutputSize - output.size()) {
		throw std::length_error{"Maximum decoded deflate-compressed data size exceeded."};
	}
	output.resize(output.size() + dataLength);
	bitReader.readAlignedBytes(Span{output}.last(dataLength));
}

void readCompressedBlock(Buffer<byte>& output, BitSpanReader& bitReader, size_t maxOutputSize, const BlockHuffmanDecoders& huffmanDecoders) {
	GREM_PROFILE_BLOCK("Read compressed deflate block");

	while (true) {
		const HuffmanDecoder::DecodeResult decodedLengthCode = huffmanDecoders.lengthDecoder.decode(bitReader.peekBits(16));
		bitReader.skipBits(decodedLengthCode.codeLength);
		if (decodedLengthCode.value < 256) {
			if (output.size() >= maxOutputSize) {
				throw std::length_error{"Maximum decoded deflate-compressed data size exceeded."};
			}
			output.push_back(bit_cast<byte>(static_cast<uint8_t>(decodedLengthCode.value)));
		} else if (decodedLengthCode.value == 256) {
			return;
		} else if (decodedLengthCode.value <= 285) {
			const size_t lengthCodeIndex = decodedLengthCode.value - 257;
			size_t length{HuffmanDecoder::LENGTH_CODE_LENGTHS[lengthCodeIndex]};
			if (const uint8_t extraLengthBitCount = HuffmanDecoder::LENGTH_CODE_EXTRA_BIT_COUNTS[lengthCodeIndex]; extraLengthBitCount != 0) {
				length += size_t{bitReader.readBits(extraLengthBitCount)};
			}
			if (length > maxOutputSize - output.size()) {
				throw std::length_error{"Maximum decoded deflate-compressed data size exceeded."};
			}
			const HuffmanDecoder::DecodeResult decodedDistanceCode = huffmanDecoders.distanceDecoder.decode(bitReader.peekBits(16));
			bitReader.skipBits(decodedDistanceCode.codeLength);
			if (decodedDistanceCode.value >= 30) {
				throw std::invalid_argument{"Invalid distance Huffman code in deflate-compressed data."};
			}
			size_t distance{HuffmanDecoder::DISTANCE_CODE_DISTANCES[decodedDistanceCode.value]};
			if (const uint8_t extraDistanceBitCount = HuffmanDecoder::DISTANCE_CODE_EXTRA_BIT_COUNTS[decodedDistanceCode.value]; extraDistanceBitCount != 0) {
				distance += size_t{bitReader.readBits(extraDistanceBitCount)};
			}
			if (distance > output.size()) {
				throw std::invalid_argument{"Invalid Huffman distance in deflate-compressed data."};
			}
			const size_t outputOffset = output.size();
			output.resize(outputOffset + length);
			GREM_ASSERT(distance > 0);
			if (distance == 1) {
				memset(output.data() + outputOffset, static_cast<int>(bit_cast<uint8_t>(output[outputOffset - 1])), length);
			} else {
				byte* out = output.data() + outputOffset;
				const byte* in = out - distance;
				while (length-- > 0) {
					*out++ = *in++;
				}
			}
		} else {
			throw std::invalid_argument{"Invalid length Huffman code in deflate-compressed data."};
		}
	}
}

void writeHuffmanCode(BitWriter& bitWriter, uint16_t value) {
	if (value <= 143) {
		bitWriter.writeBits(getBitsReversed(value + 48, 8), 8);
	} else if (value <= 255) {
		bitWriter.writeBits(getBitsReversed(value + 256, 9), 9);
	} else if (value <= 279) {
		bitWriter.writeBits(getBitsReversed(value - 256, 7), 7);
	} else {
		bitWriter.writeBits(getBitsReversed(value - 88, 8), 8);
	}
}

[[nodiscard]] uint32_t hashFunction(Span<const byte, 3> data) {
	uint32_t hash = 0;
	hash += bit_cast<uint8_t>(data[0]);
	hash += bit_cast<uint8_t>(data[1]) << 8;
	hash += bit_cast<uint8_t>(data[2]) << 16;
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	return hash;
}

[[nodiscard]] size_t countMatchingBytes(const byte* a, const byte* b, size_t length) {
	size_t result = 0;
	while (result < length && a[result] == b[result]) {
		++result;
	}
	return result;
}

constexpr size_t MAX_BLOCK_LENGTH = 32767;

} // namespace

void compress(Writer writer, Span<const byte> data, const CompressionOptions& options) {
	GREM_PROFILE_FUNCTION();

	BitWriter bitWriter{writer};

	if (options.compressionLevel == 0) {
		size_t dataOffset = 0;
		while (dataOffset < data.size()) {
			const size_t remainingLength = data.size() - dataOffset;
			const size_t blockLength = min(remainingLength, MAX_BLOCK_LENGTH);

			const BlockHeader blockHeader{
				.blockFinal = (blockLength == remainingLength) ? uint8_t{1} : uint8_t{0},
				.blockType = 0b00, // No compression.
			};
			bitWriter.writeBits(blockHeader.blockFinal, 1);
			bitWriter.writeBits(blockHeader.blockType, 2);

			const uint16_t bigEndianBlockLength = convertHostEndianToBigEndian(static_cast<uint16_t>(blockLength));
			const NonCompressedBlockHeader nonCompressedBlockHeader{
				.length = bigEndianBlockLength,
				.lengthComplement = static_cast<uint16_t>(~bigEndianBlockLength),
			};
			bitWriter.writeAlignedBytes(asBytes(Span{&nonCompressedBlockHeader, 1}));
			bitWriter.writeAlignedBytes(data.subspan(dataOffset, blockLength));
			dataOffset += blockLength;
		}
		return;
	}

	const BlockHeader blockHeader{
		.blockFinal = 1,
		.blockType = 0b01, // Compressed with fixed Huffman codes.
	};
	bitWriter.writeBits(blockHeader.blockFinal, 1);
	bitWriter.writeBits(blockHeader.blockType, 2);

	size_t dataOffset = 0;
	if (data.size() >= 4) {
		constexpr size_t MAX_LENGTH = 258;
		constexpr size_t MAX_DISTANCE = 32767;
		constexpr size_t HASH_TABLE_SIZE = 16384;

		Allocation<ArrayList<const byte*>> hashTable(HASH_TABLE_SIZE);
		const size_t maxHashTableEntryLength = max(options.compressionLevel, size_t{5}) * 2;

		do {
			const byte* longestMatchingBytesBegin = nullptr;
			size_t longestMatchingByteCount = 3;

			const Span<const byte> remainingData = data.subspan(dataOffset);
			ArrayList<const byte*>& hashTableEntry = hashTable[hashFunction(remainingData.first<3>()) % HASH_TABLE_SIZE];
			for (const byte* const begin : hashTableEntry) {
				if (remainingData.data() - begin <= MAX_DISTANCE) {
					const size_t matchingByteCount = countMatchingBytes(begin, remainingData.data(), min(remainingData.size(), MAX_LENGTH));
					if (matchingByteCount >= longestMatchingByteCount) {
						longestMatchingByteCount = matchingByteCount;
						longestMatchingBytesBegin = begin;
					}
				}
			}

			if (hashTableEntry.size() == maxHashTableEntryLength) {
				hashTableEntry.erase(hashTableEntry.begin(), hashTableEntry.begin() + maxHashTableEntryLength / 2);
			}
			hashTableEntry.push_back(remainingData.data());

			if (longestMatchingBytesBegin) {
				// Check if the next byte has a longer matching byte sequence.
				// If it does, write this byte as a literal.
				const Span<const byte> nextRemainingData = data.subspan(dataOffset + 1);
				ArrayList<const byte*>& nextHashTableEntry = hashTable[hashFunction(nextRemainingData.first<3>()) % HASH_TABLE_SIZE];
				for (const byte* const nextBegin : nextHashTableEntry) {
					if (nextRemainingData.data() - nextBegin <= MAX_DISTANCE) {
						const size_t matchingByteCount = countMatchingBytes(nextBegin, nextRemainingData.data(), min(nextRemainingData.size(), MAX_LENGTH));
						if (matchingByteCount > longestMatchingByteCount) {
							longestMatchingBytesBegin = nullptr;
							break;
						}
					}
				}
			}

			if (longestMatchingBytesBegin) {
				const size_t length = longestMatchingByteCount;
				GREM_ASSERT(length <= MAX_LENGTH);
				size_t lengthCodeIndex = 0;
				while (lengthCodeIndex + 1 < HuffmanDecoder::LENGTH_CODE_LENGTHS.size() && length > HuffmanDecoder::LENGTH_CODE_LENGTHS[lengthCodeIndex + 1] - 1) {
					++lengthCodeIndex;
				}
				writeHuffmanCode(bitWriter, lengthCodeIndex + 257);
				if (const uint8_t extraLengthBitCount = HuffmanDecoder::LENGTH_CODE_EXTRA_BIT_COUNTS[lengthCodeIndex]; extraLengthBitCount != 0) {
					bitWriter.writeBits(static_cast<uint16_t>(length - HuffmanDecoder::LENGTH_CODE_LENGTHS[lengthCodeIndex]), extraLengthBitCount);
				}

				const size_t distance = static_cast<size_t>(remainingData.data() - longestMatchingBytesBegin);
				GREM_ASSERT(distance <= MAX_DISTANCE);
				size_t distanceCodeIndex = 0;
				while (distanceCodeIndex + 1 < HuffmanDecoder::DISTANCE_CODE_DISTANCES.size() && distance > HuffmanDecoder::DISTANCE_CODE_DISTANCES[distanceCodeIndex + 1] - 1) {
					++distanceCodeIndex;
				}
				bitWriter.writeBits(getBitsReversed(distanceCodeIndex, 5), 5);
				if (const uint8_t extraDistanceBitCount = HuffmanDecoder::DISTANCE_CODE_EXTRA_BIT_COUNTS[distanceCodeIndex]; extraDistanceBitCount != 0) {
					bitWriter.writeBits(static_cast<uint16_t>(distance - HuffmanDecoder::DISTANCE_CODE_DISTANCES[distanceCodeIndex]), extraDistanceBitCount);
				}

				dataOffset += length;
			} else {
				writeHuffmanCode(bitWriter, bit_cast<uint8_t>(data[dataOffset]));
				++dataOffset;
			}
		} while (dataOffset <= data.size() - 4);
	}
	while (dataOffset < data.size()) {
		writeHuffmanCode(bitWriter, bit_cast<uint8_t>(data[dataOffset]));
		++dataOffset;
	}
	writeHuffmanCode(bitWriter, 256);
	bitWriter.writeAlignedBytes({}); // Pad partially written byte with zero bits and ensure it is written to the result.
}

Buffer<byte> compress(Span<const byte> data, const CompressionOptions& options) {
	Buffer<byte> result{};
	compress(result, data, options);
	if (options.compressionLevel != 0) {
		const size_t requiredNonCompressedBlockCount = (data.size() + MAX_BLOCK_LENGTH - 1) / MAX_BLOCK_LENGTH;
		const size_t requiredNonCompressedSize = data.size() + requiredNonCompressedBlockCount * (1 + sizeof(NonCompressedBlockHeader));
		if (result.size() > requiredNonCompressedSize) {
			// Compression resulted in a larger size than just storing the data non-compressed would.
			// Undo everything and rewrite the data as non-compressed blocks to save some space and decompression time.
			result.clear();
			CompressionOptions newOptions = options;
			newOptions.compressionLevel = 0;
			compress(result, data, newOptions);
		}
	}
	return result;
}

void decompress(Buffer<byte>& output, SpanReader reader, const DecompressionOptions& options) {
	GREM_PROFILE_FUNCTION();

	const size_t maxOutputCapacity = max(output.max_size(), output.capacity());
	const size_t desiredOutputCapacity = output.size() + min(options.estimatedDecompressedSizeInBytes, maxOutputCapacity - output.capacity());
	if (desiredOutputCapacity > output.capacity()) {
		output.reserve(desiredOutputCapacity);
	}
	const size_t maxOutputSize = output.size() + min(options.maxDecompressedSizeInBytes, output.max_size() - output.size());
	BitSpanReader bitReader{reader};
	while (true) {
		const BlockHeader blockHeader{
			.blockFinal = static_cast<uint8_t>(bitReader.readBits(1) & 0b1),
			.blockType = static_cast<uint8_t>(bitReader.readBits(2) & 0b11),
		};
		switch (blockHeader.blockType) {
			case 0b00: // No compression.
				readNonCompressedBlock(output, bitReader, maxOutputSize);
				break;
			case 0b01: { // Compressed with fixed Huffman codes.
				const BlockHuffmanDecoders fixedHuffmanDecoders{
					.lengthDecoder{HuffmanDecoder::FIXED_LENGTH_CODE_LENGTHS},
					.distanceDecoder{HuffmanDecoder::FIXED_DISTANCE_CODE_LENGTHS},
				};
				readCompressedBlock(output, bitReader, maxOutputSize, fixedHuffmanDecoders);
				break;
			}
			case 0b10: { // Compressed with dynamic Huffman codes.
				const BlockHuffmanDecoders dynamicHuffmanDecoders = readDynamicBlockHuffmanDecoders(bitReader);
				readCompressedBlock(output, bitReader, maxOutputSize, dynamicHuffmanDecoders);
				break;
			}
			case 0b11: // Reserved (error).
				throw std::invalid_argument{"Invalid deflate block type in deflate-compressed data."};
			default: unreachable();
		}
		if (blockHeader.blockFinal != 0) {
			break;
		}
	}
}

} // namespace grem::deflate
