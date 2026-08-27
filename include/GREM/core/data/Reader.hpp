// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_READER_HPP
#define GREM_CORE_DATA_READER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>

#include <istream>   // std::istream, std::streamsize
#include <stdexcept> // std::length_error
#include <streambuf> // std::streambuf
#include <utility>   // std::exchange

namespace grem {

namespace detail {

template <typename Self>
class ReaderBase {
public:
	[[nodiscard]] GREM_ALWAYS_INLINE Allocation<byte> readBytesIntoAllocation(size_t maxLength = Limits<size_t>::MAX) {
		return readBytesInto<Allocation<byte>>(maxLength);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ArrayList<byte> readBytesIntoArrayList(size_t maxLength = Limits<size_t>::MAX) {
		return readBytesInto<ArrayList<byte>>(maxLength);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Buffer<byte> readBytesIntoBuffer(size_t maxLength = Limits<size_t>::MAX) {
		return readBytesInto<Buffer<byte>>(maxLength);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE String readBytesIntoString(size_t maxLength = Limits<size_t>::MAX) {
		return readBytesInto<String>(maxLength);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<char> peekChar() {
		if (const Optional<byte> result = peekByte()) {
			return bit_cast<char>(*result);
		}
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE char readChar() {
		return bit_cast<char>(readByte());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<char> tryReadChar() {
		if (const Optional<byte> result = tryReadByte()) {
			return bit_cast<char>(*result);
		}
		return {};
	}

	template <size_t N>
	[[nodiscard]] GREM_ALWAYS_INLINE Array<byte, N> readBytes() {
		if constexpr (N == 1) {
			return {readByte()};
		} else {
			Array<byte, N> data{};
			read(data);
			return data;
		}
	}

	template <size_t N>
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Array<byte, N>> tryReadBytes() {
		if constexpr (N == 1) {
			if (const Optional<byte> result = tryReadByte()) {
				return Array<byte, 1>{*result};
			}
			return {};
		} else {
			Array<byte, N> data{};
			if (!tryRead(data)) {
				return {};
			}
			return data;
		}
	}

	template <size_t N>
	[[nodiscard]] GREM_ALWAYS_INLINE Array<char, N> readChars() {
		Array<byte, N> data{};
		read(data);
		return bit_cast<Array<char, N>>(data);
	}

	template <size_t N>
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Array<char, N>> tryReadChars() {
		Array<byte, N> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<Array<char, N>>(data);
	}

	[[nodiscard]] String readLine(size_t maxLength = Limits<size_t>::MAX) {
		String result{};
		while (result.size() < maxLength) {
			const Optional<char> ch = tryReadChar();
			if (!ch || *ch == '\n') {
				break;
			}
			if (*ch == '\r') {
				if (peekChar() == '\n') {
					skipByte();
				}
				break;
			}
			result.push_back(*ch);
		}
		return result;
	}

	[[nodiscard]] String readNullTerminatedString(size_t maxLength = Limits<size_t>::MAX) {
		String result{};
		while (result.size() < maxLength) {
			const Optional<char> ch = tryReadChar();
			if (!ch || *ch == '\0') {
				break;
			}
			result.push_back(*ch);
		}
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int8_t peekInt8() {
		if (const Optional<byte> result = peekByte()) {
			return bit_cast<int8_t>(*result);
		}
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int8_t readInt8() {
		return bit_cast<int8_t>(readByte());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int8_t> tryReadInt8() {
		if (const Optional<byte> result = tryReadByte()) {
			return bit_cast<int8_t>(*result);
		}
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint8_t peekUInt8() {
		if (const Optional<byte> result = peekByte()) {
			return bit_cast<uint8_t>(*result);
		}
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint8_t readUInt8() {
		return bit_cast<uint8_t>(readByte());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint8_t> tryReadUInt8() {
		if (const Optional<byte> result = tryReadByte()) {
			return bit_cast<uint8_t>(*result);
		}
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int16_t readInt16HE() {
		Array<byte, 2> data{};
		read(data);
		return bit_cast<int16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> tryReadInt16HE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<int16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int16_t readInt16LE() {
		Array<byte, 2> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<int16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> tryReadInt16LE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<int16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int16_t readInt16BE() {
		Array<byte, 2> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<int16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> tryReadInt16BE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<int16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint16_t readUInt16HE() {
		Array<byte, 2> data{};
		read(data);
		return bit_cast<uint16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint16_t> tryReadUInt16HE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<uint16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint16_t readUInt16LE() {
		Array<byte, 2> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<uint16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint16_t> tryReadUInt16LE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<uint16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint16_t readUInt16BE() {
		Array<byte, 2> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<uint16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint16_t> tryReadUInt16BE() {
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<uint16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int32_t readInt32HE() {
		Array<byte, 4> data{};
		read(data);
		return bit_cast<int32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int32_t> tryReadInt32HE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<int32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int32_t readInt32LE() {
		Array<byte, 4> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<int32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int32_t> tryReadInt32LE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<int32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int32_t readInt32BE() {
		Array<byte, 4> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<int32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int32_t> tryReadInt32BE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<int32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint32_t readUInt32HE() {
		Array<byte, 4> data{};
		read(data);
		return bit_cast<uint32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint32_t> tryReadUInt32HE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<uint32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint32_t readUInt32LE() {
		Array<byte, 4> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<uint32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint32_t> tryReadUInt32LE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<uint32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint32_t readUInt32BE() {
		Array<byte, 4> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<uint32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint32_t> tryReadUInt32BE() {
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<uint32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int64_t readInt64HE() {
		Array<byte, 8> data{};
		read(data);
		return bit_cast<int64_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int64_t> tryReadInt64HE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<int64_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int64_t readInt64LE() {
		Array<byte, 8> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<int64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int64_t> tryReadInt64LE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<int64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE int64_t readInt64BE() {
		Array<byte, 8> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<int64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int64_t> tryReadInt64BE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<int64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint64_t readUInt64HE() {
		Array<byte, 8> data{};
		read(data);
		return bit_cast<uint64_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint64_t> tryReadUInt64HE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<uint64_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint64_t readUInt64LE() {
		Array<byte, 8> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<uint64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint64_t> tryReadUInt64LE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<uint64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE uint64_t readUInt64BE() {
		Array<byte, 8> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<uint64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<uint64_t> tryReadUInt64BE() {
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<uint64_t>(data));
	}

	[[nodiscard]] int64_t readIntLEB128() {
		uint64_t result = 0;
		int shift = 0;
		uint8_t byte{};
		do {
			byte = readUInt8();
			result |= static_cast<uint64_t>(byte & 0b01111111) << shift;
			shift += 7;
		} while (shift < 64 && (byte & 0b10000000) != 0);
		if (shift < 64 && (byte & 0b01000000) != 0) {
			result |= ~uint64_t{0} << shift;
		}
		return bit_cast<int64_t>(result);
	}

	[[nodiscard]] Optional<int64_t> tryReadIntLEB128() {
		uint64_t result = 0;
		int shift = 0;
		uint8_t byte{};
		do {
			const Optional<uint8_t> nextByte = tryReadUInt8();
			if (!nextByte) {
				return {};
			}
			byte = *nextByte;
			result |= static_cast<uint64_t>(byte & 0b01111111) << shift;
			shift += 7;
		} while (shift < 64 && (byte & 0b10000000) != 0);
		if (shift < 64 && (byte & 0b01000000) != 0) {
			result |= ~uint64_t{0} << shift;
		}
		return bit_cast<int64_t>(result);
	}

	[[nodiscard]] uint64_t readUIntLEB128() {
		uint64_t result = 0;
		int shift = 0;
		uint8_t byte{};
		do {
			byte = readUInt8();
			result |= static_cast<uint64_t>(byte & 0b01111111) << shift;
			shift += 7;
		} while (shift < 64 && (byte & 0b10000000) != 0);
		return result;
	}

	[[nodiscard]] Optional<uint64_t> tryReadUIntLEB128() {
		uint64_t result = 0;
		int shift = 0;
		uint8_t byte{};
		do {
			const Optional<uint8_t> nextByte = tryReadUInt8();
			if (!nextByte) {
				return {};
			}
			byte = *nextByte;
			result |= static_cast<uint64_t>(byte & 0b01111111) << shift;
			shift += 7;
		} while (shift < 64 && (byte & 0b10000000) != 0);
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float16_t readFloat16HE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		read(data);
		return bit_cast<float16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float16_t> tryReadFloat16HE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<float16_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float16_t readFloat16LE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<float16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float16_t> tryReadFloat16LE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<float16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float16_t readFloat16BE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<float16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float16_t> tryReadFloat16BE() {
		static_assert(Limits<float16_t>::IS_IEC60559);
		Array<byte, 2> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<float16_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float32_t readFloat32HE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		read(data);
		return bit_cast<float32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float32_t> tryReadFloat32HE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<float32_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float32_t readFloat32LE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<float32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float32_t> tryReadFloat32LE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<float32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float32_t readFloat32BE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<float32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float32_t> tryReadFloat32BE() {
		static_assert(Limits<float32_t>::IS_IEC60559);
		Array<byte, 4> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<float32_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float64_t> tryReadFloat64HE() {
		static_assert(Limits<float64_t>::IS_IEC60559);
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return bit_cast<float64_t>(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float64_t readFloat64LE() {
		static_assert(Limits<float64_t>::IS_IEC60559);
		Array<byte, 8> data{};
		read(data);
		return convertLittleEndianToHostEndian(bit_cast<float64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float64_t> tryReadFloat64LE() {
		static_assert(Limits<float64_t>::IS_IEC60559);
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertLittleEndianToHostEndian(bit_cast<float64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE float64_t readFloat64BE() {
		static_assert(Limits<float64_t>::IS_IEC60559);
		Array<byte, 8> data{};
		read(data);
		return convertBigEndianToHostEndian(bit_cast<float64_t>(data));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float64_t> tryReadFloat64BE() {
		static_assert(Limits<float64_t>::IS_IEC60559);
		Array<byte, 8> data{};
		if (!tryRead(data)) {
			return {};
		}
		return convertBigEndianToHostEndian(bit_cast<float64_t>(data));
	}

private:
	template <typename Container>
	[[nodiscard]] GREM_ALWAYS_INLINE Container readBytesInto(size_t maxLength) {
		return static_cast<Self*>(this)->template readBytesInto<Container>(maxLength);
	}

	GREM_ALWAYS_INLINE void read(Span<byte> data) {
		static_cast<Self*>(this)->read(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool tryRead(Span<byte> data) {
		return static_cast<Self*>(this)->tryRead(data);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<byte> peekByte() {
		return static_cast<Self*>(this)->peekByte();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE byte readByte() {
		return static_cast<Self*>(this)->readByte();
	}

	GREM_ALWAYS_INLINE void skipByte() {
		static_cast<Self*>(this)->skipByte();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<byte> tryReadByte() {
		return static_cast<Self*>(this)->tryReadByte();
	}
};

} // namespace detail

class Reader : private detail::ReaderBase<Reader> {
public:
	GREM_ALWAYS_INLINE Reader(void* context, size_t (*readSomeImplementation)(void* context, Span<byte> data)) noexcept
		: context(context)
		, readSomeImplementation(readSomeImplementation) {}

	GREM_ALWAYS_INLINE Reader(Span<const byte>& input) noexcept
		: Reader(&input, [](void* context, Span<byte> data) -> size_t {
			Span<const byte>& input = *static_cast<Span<const byte>*>(context);
			const size_t bytesRead = min(input.size(), data.size());
			if (bytesRead > 0) {
				memcpy(data.data(), input.data(), bytesRead);
				input = input.subspan(bytesRead);
			}
			return bytesRead;
		}) {}

	GREM_ALWAYS_INLINE Reader(std::streambuf& input) noexcept
		: Reader(&input, [](void* context, Span<byte> data) -> size_t {
			std::streambuf& input = *static_cast<std::streambuf*>(context);
			size_t totalBytesRead = 0;
			while (!data.empty()) {
				const size_t size = min(data.size(), size_t{Limits<std::streamsize>::MAX});
				const std::streamsize bytesRead = input.sgetn(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
				if (bytesRead <= 0) {
					break;
				}
				totalBytesRead += static_cast<size_t>(bytesRead);
				data = data.subspan(static_cast<size_t>(bytesRead));
			}
			return totalBytesRead;
		}) {}

	GREM_ALWAYS_INLINE Reader(std::istream& input)
		: Reader(*input.rdbuf()) {}

	~Reader() = default;

	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;

	GREM_ALWAYS_INLINE Reader(Reader&& other) noexcept
		: context(std::exchange(other.context, nullptr))
		, readSomeImplementation(std::exchange(other.readSomeImplementation, nullptr))
		, writeOffset(std::exchange(other.writeOffset, size_t{0}))
		, readOffset(std::exchange(other.readOffset, size_t{0})) {
		if (readOffset < writeOffset) {
			memcpy(buffer.data(), other.buffer.data() + readOffset, writeOffset - readOffset);
		}
	}

	GREM_ALWAYS_INLINE Reader& operator=(Reader&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		context = std::exchange(other.context, nullptr);
		readSomeImplementation = std::exchange(other.readSomeImplementation, nullptr);
		writeOffset = std::exchange(other.writeOffset, size_t{0});
		readOffset = std::exchange(other.readOffset, size_t{0});
		if (readOffset < writeOffset) {
			memcpy(buffer.data(), other.buffer.data() + readOffset, writeOffset - readOffset);
		}
		return *this;
	}

	[[nodiscard]] bool eof() {
		if (readOffset < writeOffset) {
			return false;
		}
		writeOffset = readSomeImplementation(context, buffer);
		readOffset = 0;
		return writeOffset != 0;
	}

	[[nodiscard]] size_t readSome(Span<byte> data) {
		if (readOffset == writeOffset) {
			writeOffset = readSomeImplementation(context, buffer);
			readOffset = 0;
		}
		const size_t bytesRead = min(writeOffset - readOffset, data.size());
		if (bytesRead > 0) {
			memcpy(data.data(), buffer.data() + readOffset, bytesRead);
			readOffset += bytesRead;
		}
		return bytesRead;
	}

	[[nodiscard]] size_t readUntilEOF(Span<byte> data) {
		size_t result = 0;
		while (!data.empty()) {
			const size_t bytesRead = readSome(data);
			if (bytesRead == 0) {
				break;
			}
			data = data.subspan(bytesRead);
			result += bytesRead;
		}
		return result;
	}

	[[nodiscard]] size_t skipSome(size_t maxLength) {
		if (readOffset == writeOffset) {
			writeOffset = readSomeImplementation(context, buffer);
			readOffset = 0;
		}
		const size_t bytesSkipped = min(writeOffset - readOffset, maxLength);
		readOffset += bytesSkipped;
		return bytesSkipped;
	}

	size_t skipUntilEOF(size_t maxLength) {
		size_t result = 0;
		while (maxLength > 0) {
			const size_t bytesRead = skipSome(maxLength);
			if (bytesRead == 0) {
				break;
			}
			maxLength -= bytesRead;
			result += bytesRead;
		}
		return result;
	}

	void read(Span<byte> data) {
		if (!tryRead(data)) {
			throw std::length_error{"Unexpected end of data."};
		}
	}

	void skip(size_t n) {
		if (!trySkip(n)) {
			throw std::length_error{"Unexpected end of data."};
		}
	}

	[[nodiscard]] bool tryRead(Span<byte> data) {
		return readUntilEOF(data) == data.size();
	}

	[[nodiscard]] bool trySkip(size_t n) {
		while (n > 0) {
			const size_t bytesSkipped = skipSome(n);
			if (bytesSkipped == 0) {
				return false;
			}
			n -= bytesSkipped;
		}
		return true;
	}

	[[nodiscard]] Optional<byte> peekByte() {
		if (readOffset < writeOffset) {
			return buffer[readOffset];
		}
		writeOffset = readSomeImplementation(context, buffer);
		readOffset = 0;
		if (writeOffset == 0) {
			return {};
		}
		return buffer[0];
	}

	[[nodiscard]] byte readByte() {
		if (const Optional<byte> result = tryReadByte()) {
			return *result;
		}
		throw std::length_error{"Unexpected end of input."};
	}

	void skipByte() {
		if (!trySkipByte()) {
			throw std::length_error{"Unexpected end of input."};
		}
	}

	[[nodiscard]] Optional<byte> tryReadByte() {
		if (const Optional<byte> result = peekByte()) {
			++readOffset;
			return result;
		}
		return {};
	}

	[[nodiscard]] bool trySkipByte() {
		return tryReadByte().has_value();
	}

	using ReaderBase::peekChar;
	using ReaderBase::peekInt8;
	using ReaderBase::peekUInt8;
	using ReaderBase::readBytes;
	using ReaderBase::readBytesIntoAllocation;
	using ReaderBase::readBytesIntoArrayList;
	using ReaderBase::readBytesIntoBuffer;
	using ReaderBase::readBytesIntoString;
	using ReaderBase::readChar;
	using ReaderBase::readChars;
	using ReaderBase::readFloat16BE;
	using ReaderBase::readFloat16HE;
	using ReaderBase::readFloat16LE;
	using ReaderBase::readFloat32BE;
	using ReaderBase::readFloat32HE;
	using ReaderBase::readFloat32LE;
	using ReaderBase::readFloat64BE;
	using ReaderBase::readFloat64LE;
	using ReaderBase::readInt16BE;
	using ReaderBase::readInt16HE;
	using ReaderBase::readInt16LE;
	using ReaderBase::readInt32BE;
	using ReaderBase::readInt32HE;
	using ReaderBase::readInt32LE;
	using ReaderBase::readInt64BE;
	using ReaderBase::readInt64HE;
	using ReaderBase::readInt64LE;
	using ReaderBase::readInt8;
	using ReaderBase::readIntLEB128;
	using ReaderBase::readLine;
	using ReaderBase::readNullTerminatedString;
	using ReaderBase::readUInt16BE;
	using ReaderBase::readUInt16HE;
	using ReaderBase::readUInt16LE;
	using ReaderBase::readUInt32BE;
	using ReaderBase::readUInt32HE;
	using ReaderBase::readUInt32LE;
	using ReaderBase::readUInt64BE;
	using ReaderBase::readUInt64HE;
	using ReaderBase::readUInt64LE;
	using ReaderBase::readUInt8;
	using ReaderBase::readUIntLEB128;
	using ReaderBase::tryReadBytes;
	using ReaderBase::tryReadChar;
	using ReaderBase::tryReadChars;
	using ReaderBase::tryReadFloat16BE;
	using ReaderBase::tryReadFloat16HE;
	using ReaderBase::tryReadFloat16LE;
	using ReaderBase::tryReadFloat32BE;
	using ReaderBase::tryReadFloat32HE;
	using ReaderBase::tryReadFloat32LE;
	using ReaderBase::tryReadFloat64BE;
	using ReaderBase::tryReadFloat64HE;
	using ReaderBase::tryReadFloat64LE;
	using ReaderBase::tryReadInt16BE;
	using ReaderBase::tryReadInt16HE;
	using ReaderBase::tryReadInt16LE;
	using ReaderBase::tryReadInt32BE;
	using ReaderBase::tryReadInt32HE;
	using ReaderBase::tryReadInt32LE;
	using ReaderBase::tryReadInt64BE;
	using ReaderBase::tryReadInt64HE;
	using ReaderBase::tryReadInt64LE;
	using ReaderBase::tryReadInt8;
	using ReaderBase::tryReadIntLEB128;
	using ReaderBase::tryReadUInt16BE;
	using ReaderBase::tryReadUInt16HE;
	using ReaderBase::tryReadUInt16LE;
	using ReaderBase::tryReadUInt32BE;
	using ReaderBase::tryReadUInt32HE;
	using ReaderBase::tryReadUInt32LE;
	using ReaderBase::tryReadUInt64BE;
	using ReaderBase::tryReadUInt64HE;
	using ReaderBase::tryReadUInt64LE;
	using ReaderBase::tryReadUInt8;
	using ReaderBase::tryReadUIntLEB128;

private:
	friend ReaderBase;

	template <typename Container>
	[[nodiscard]] Container readBytesInto(size_t maxLength) {
		static_assert(sizeof(typename Container::value_type) == sizeof(byte));

		Container result{};
		result.resize(min(buffer.size(), maxLength));
		size_t length = readSome(asWritableBytes(Span{result}));
		if (length > 0) {
			while (length < maxLength) {
				result.resize(min(length + buffer.size(), maxLength));
				const size_t bytesRead = readSome(asWritableBytes(Span{result}).subspan(length));
				if (bytesRead == 0) {
					break;
				}
				length += bytesRead;
			}
		}
		result.resize(length);
		return result;
	}

	void* context;
	size_t (*readSomeImplementation)(void* context, Span<byte> data);
	size_t writeOffset = 0;
	size_t readOffset = 0;
	Array<byte, 1024> buffer;
};

class SpanReader : private detail::ReaderBase<SpanReader> {
public:
	GREM_ALWAYS_INLINE SpanReader(Span<const byte>& input) noexcept
		: input(&input) {}

	[[nodiscard]] GREM_ALWAYS_INLINE operator Reader() const {
		return Reader{*input};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool eof() {
		return input->empty();
	}

	[[nodiscard]] size_t readSome(Span<byte> data) {
		return readUntilEOF(data);
	}

	[[nodiscard]] size_t readUntilEOF(Span<byte> data) {
		const size_t bytesRead = min(data.size(), input->size());
		if (bytesRead > 0) {
			memcpy(data.data(), input->data(), bytesRead);
			*input = input->subspan(bytesRead);
		}
		return bytesRead;
	}

	[[nodiscard]] size_t skipSome(size_t maxLength) {
		return skipUntilEOF(maxLength);
	}

	[[nodiscard]] size_t skipUntilEOF(size_t maxLength) {
		const size_t bytesSkipped = min(maxLength, input->size());
		*input = input->subspan(bytesSkipped);
		return bytesSkipped;
	}

	[[nodiscard]] Span<const byte> peek(size_t n) const noexcept {
		return input->first(min(n, input->size()));
	}

	void read(Span<byte> data) {
		if (!tryRead(data)) {
			throw std::length_error{"Unexpected end of data."};
		}
	}

	void skip(size_t n) {
		if (!trySkip(n)) {
			throw std::length_error{"Unexpected end of data."};
		}
	}

	[[nodiscard]] bool tryRead(Span<byte> data) {
		return readUntilEOF(data) == data.size();
	}

	[[nodiscard]] bool trySkip(size_t n) {
		if (n > 0) {
			if (n > input->size()) {
				return false;
			}
			*input = input->subspan(n);
		}
		return true;
	}

	[[nodiscard]] Optional<byte> peekByte() {
		if (input->empty()) {
			return {};
		}
		return input->front();
	}

	[[nodiscard]] byte readByte() {
		if (const Optional<byte> result = tryReadByte()) {
			return *result;
		}
		throw std::length_error{"Unexpected end of input."};
	}

	void skipByte() {
		if (!trySkipByte()) {
			throw std::length_error{"Unexpected end of data."};
		}
	}

	[[nodiscard]] Optional<byte> tryReadByte() {
		if (const Optional<byte> result = peekByte()) {
			*input = input->subspan(1);
			return result;
		}
		return {};
	}

	[[nodiscard]] bool trySkipByte() {
		if (input->empty()) {
			return false;
		}
		*input = input->subspan(1);
		return true;
	}

	using ReaderBase::peekChar;
	using ReaderBase::peekInt8;
	using ReaderBase::peekUInt8;
	using ReaderBase::readBytes;
	using ReaderBase::readBytesIntoAllocation;
	using ReaderBase::readBytesIntoArrayList;
	using ReaderBase::readBytesIntoBuffer;
	using ReaderBase::readBytesIntoString;
	using ReaderBase::readChar;
	using ReaderBase::readChars;
	using ReaderBase::readFloat16BE;
	using ReaderBase::readFloat16HE;
	using ReaderBase::readFloat16LE;
	using ReaderBase::readFloat32BE;
	using ReaderBase::readFloat32HE;
	using ReaderBase::readFloat32LE;
	using ReaderBase::readFloat64BE;
	using ReaderBase::readFloat64LE;
	using ReaderBase::readInt16BE;
	using ReaderBase::readInt16HE;
	using ReaderBase::readInt16LE;
	using ReaderBase::readInt32BE;
	using ReaderBase::readInt32HE;
	using ReaderBase::readInt32LE;
	using ReaderBase::readInt64BE;
	using ReaderBase::readInt64HE;
	using ReaderBase::readInt64LE;
	using ReaderBase::readInt8;
	using ReaderBase::readIntLEB128;
	using ReaderBase::readLine;
	using ReaderBase::readNullTerminatedString;
	using ReaderBase::readUInt16BE;
	using ReaderBase::readUInt16HE;
	using ReaderBase::readUInt16LE;
	using ReaderBase::readUInt32BE;
	using ReaderBase::readUInt32HE;
	using ReaderBase::readUInt32LE;
	using ReaderBase::readUInt64BE;
	using ReaderBase::readUInt64HE;
	using ReaderBase::readUInt64LE;
	using ReaderBase::readUInt8;
	using ReaderBase::readUIntLEB128;
	using ReaderBase::tryReadBytes;
	using ReaderBase::tryReadChar;
	using ReaderBase::tryReadChars;
	using ReaderBase::tryReadFloat16BE;
	using ReaderBase::tryReadFloat16HE;
	using ReaderBase::tryReadFloat16LE;
	using ReaderBase::tryReadFloat32BE;
	using ReaderBase::tryReadFloat32HE;
	using ReaderBase::tryReadFloat32LE;
	using ReaderBase::tryReadFloat64BE;
	using ReaderBase::tryReadFloat64HE;
	using ReaderBase::tryReadFloat64LE;
	using ReaderBase::tryReadInt16BE;
	using ReaderBase::tryReadInt16HE;
	using ReaderBase::tryReadInt16LE;
	using ReaderBase::tryReadInt32BE;
	using ReaderBase::tryReadInt32HE;
	using ReaderBase::tryReadInt32LE;
	using ReaderBase::tryReadInt64BE;
	using ReaderBase::tryReadInt64HE;
	using ReaderBase::tryReadInt64LE;
	using ReaderBase::tryReadInt8;
	using ReaderBase::tryReadIntLEB128;
	using ReaderBase::tryReadUInt16BE;
	using ReaderBase::tryReadUInt16HE;
	using ReaderBase::tryReadUInt16LE;
	using ReaderBase::tryReadUInt32BE;
	using ReaderBase::tryReadUInt32HE;
	using ReaderBase::tryReadUInt32LE;
	using ReaderBase::tryReadUInt64BE;
	using ReaderBase::tryReadUInt64HE;
	using ReaderBase::tryReadUInt64LE;
	using ReaderBase::tryReadUInt8;
	using ReaderBase::tryReadUIntLEB128;

private:
	friend ReaderBase;

	template <typename Container>
	[[nodiscard]] Container readBytesInto(size_t maxLength) {
		static_assert(sizeof(typename Container::value_type) == sizeof(byte));

		Container result{};
		if (!input->empty()) {
			result.resize(min(input->size(), maxLength));
			memcpy(result.data(), input->data(), result.size());
			*input = {};
		}
		return result;
	}

	Span<const byte>* input;
};

} // namespace grem

#endif
