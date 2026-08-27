// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_WRITER_HPP
#define GREM_CORE_DATA_WRITER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstdio>    // std::FILE
#include <iosfwd>    // std::streambuf, std::ostream
#include <stdexcept> // std::out_of_range
#include <streambuf> // std::streambuf

namespace grem {

namespace detail {

template <typename Self>
class WriterBase {
public:
	GREM_ALWAYS_INLINE void writeChars(StringView data) {
		write(asBytes(Span{data}));
	}

	GREM_ALWAYS_INLINE void writeLine(StringView data, StringView newlineString = "\r\n") {
		write(asBytes(Span{data}));
		write(asBytes(Span{newlineString}));
	}

	GREM_ALWAYS_INLINE void writeNullTerminatedString(StringView string) {
		write(asBytes(Span{string}));
		writeChar('\0');
	}

	GREM_ALWAYS_INLINE void writeByte(byte value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeChar(char value) {
		writeRawValue(bit_cast<byte>(value));
	}

	GREM_ALWAYS_INLINE void writeInt8(int8_t value) {
		writeRawValue(bit_cast<byte>(value));
	}

	GREM_ALWAYS_INLINE void writeUInt8(uint8_t value) {
		writeRawValue(bit_cast<byte>(value));
	}

	GREM_ALWAYS_INLINE void writeInt16HE(int16_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeInt16LE(int16_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeInt16BE(int16_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt16HE(uint16_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeUInt16LE(uint16_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt16BE(uint16_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeInt32HE(int32_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeInt32LE(int32_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeInt32BE(int32_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt32HE(uint32_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeUInt32LE(uint32_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt32BE(uint32_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeInt64HE(int64_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeInt64LE(int64_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeInt64BE(int64_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt64HE(uint64_t value) {
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeUInt64LE(uint64_t value) {
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeUInt64BE(uint64_t value) {
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	void writeIntLEB128(int64_t value) {
		bool done = false;
		uint64_t valueBits = bit_cast<uint64_t>(value);
		const bool signExtend = value < 0;
		do {
			uint8_t byte = static_cast<uint8_t>(valueBits & uint64_t{0b01111111});
			valueBits >>= 7;
			if (signExtend) {
				valueBits |= ~uint64_t{0} << 57;
			}
			const bool sign = (byte & 0b01000000) != 0;
			if ((sign && valueBits == ~uint64_t{0}) || (!sign && valueBits == 0)) {
				done = true;
			} else {
				byte |= uint8_t{0b10000000};
			}
			writeUInt8(byte);
		} while (!done);
	}

	void writeUIntLEB128(uint64_t value) {
		do {
			uint8_t byte = static_cast<uint8_t>(value & uint64_t{0b01111111});
			value >>= 7;
			if (value != 0) {
				byte |= uint8_t{0b10000000};
			}
			writeUInt8(byte);
		} while (value != 0);
	}

	GREM_ALWAYS_INLINE void writeFloat16HE(float16_t value) {
		static_assert(Limits<float16_t>::IS_IEC60559);
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeFloat16LE(float16_t value) {
		static_assert(Limits<float16_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeFloat16BE(float16_t value) {
		static_assert(Limits<float16_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeFloat32HE(float32_t value) {
		static_assert(Limits<float32_t>::IS_IEC60559);
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeFloat32LE(float32_t value) {
		static_assert(Limits<float32_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeFloat32BE(float32_t value) {
		static_assert(Limits<float32_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToBigEndian(value));
	}

	GREM_ALWAYS_INLINE void writeFloat64HE(float64_t value) {
		static_assert(Limits<float64_t>::IS_IEC60559);
		writeRawValue(value);
	}

	GREM_ALWAYS_INLINE void writeFloat64LE(float64_t value) {
		static_assert(Limits<float64_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToLittleEndian(value));
	}

	GREM_ALWAYS_INLINE void writeFloat64BE(float64_t value) {
		static_assert(Limits<float64_t>::IS_IEC60559);
		writeRawValue(convertHostEndianToBigEndian(value));
	}

private:
	template <typename T>
	GREM_ALWAYS_INLINE void writeRawValue(const T& value) {
		write(asBytes(Span{&value, 1}));
	}

	GREM_ALWAYS_INLINE void write(Span<const byte> data, bool thenFlush = false) {
		static_cast<Self*>(this)->write(data, thenFlush);
	}
};

} // namespace detail

class Writer : private detail::WriterBase<Writer> {
public:
	GREM_ALWAYS_INLINE Writer(void* context, size_t (*writeSomeImplementation)(void* context, Span<const byte> data, bool thenFlush)) noexcept
		: context(context)
		, writeSomeImplementation(writeSomeImplementation) {}

	template <typename Container>
	GREM_ALWAYS_INLINE Writer(Container& output) noexcept requires(sizeof(typename Container::value_type) == 1)
		: Writer(&output, [](void* context, Span<const byte> data, bool) -> size_t {
			if (!data.empty()) {
				Container& output = *static_cast<Container*>(context);
				const size_t offset = output.size();
				output.resize(offset + data.size());
				memcpy(output.data() + offset, data.data(), data.size());
			}
			return data.size();
		}) {}

	GREM_API(core) Writer(std::FILE* output);
	GREM_API(core) Writer(std::streambuf* output);
	GREM_API(core) Writer(std::ostream& output);

	[[nodiscard]] GREM_ALWAYS_INLINE size_t writeSome(Span<const byte> data, bool thenFlush = false) {
		return writeSomeImplementation(context, data, thenFlush);
	}

	GREM_ALWAYS_INLINE void write(Span<const byte> data, bool thenFlush = false) {
		while (!data.empty()) {
			data = data.subspan(writeSome(data, thenFlush));
		}
	}

	GREM_ALWAYS_INLINE void flush() {
		writeSomeImplementation(context, {}, true);
	}

	using WriterBase::writeByte;
	using WriterBase::writeChar;
	using WriterBase::writeChars;
	using WriterBase::writeFloat16BE;
	using WriterBase::writeFloat16HE;
	using WriterBase::writeFloat16LE;
	using WriterBase::writeFloat32BE;
	using WriterBase::writeFloat32HE;
	using WriterBase::writeFloat32LE;
	using WriterBase::writeFloat64BE;
	using WriterBase::writeFloat64HE;
	using WriterBase::writeFloat64LE;
	using WriterBase::writeInt16BE;
	using WriterBase::writeInt16HE;
	using WriterBase::writeInt16LE;
	using WriterBase::writeInt32BE;
	using WriterBase::writeInt32HE;
	using WriterBase::writeInt32LE;
	using WriterBase::writeInt64BE;
	using WriterBase::writeInt64HE;
	using WriterBase::writeInt64LE;
	using WriterBase::writeInt8;
	using WriterBase::writeIntLEB128;
	using WriterBase::writeLine;
	using WriterBase::writeNullTerminatedString;
	using WriterBase::writeUInt16BE;
	using WriterBase::writeUInt16HE;
	using WriterBase::writeUInt16LE;
	using WriterBase::writeUInt32BE;
	using WriterBase::writeUInt32HE;
	using WriterBase::writeUInt32LE;
	using WriterBase::writeUInt64BE;
	using WriterBase::writeUInt64HE;
	using WriterBase::writeUInt64LE;
	using WriterBase::writeUInt8;
	using WriterBase::writeUIntLEB128;

private:
	friend WriterBase;

	void* context = nullptr;
	size_t (*writeSomeImplementation)(void* context, Span<const byte> data, bool thenFlush) = nullptr;
};

class SpanWriter : private detail::WriterBase<SpanWriter> {
public:
	GREM_ALWAYS_INLINE SpanWriter(Span<byte>& output) noexcept
		: output(&output) {}

	[[nodiscard]] GREM_ALWAYS_INLINE operator Writer() const {
		return Writer{output, [](void* context, Span<const byte> data, bool thenFlush) -> size_t {
						  Span<byte>& output = *static_cast<Span<byte>*>(context);
						  return SpanWriter{output}.writeSome(data, thenFlush);
					  }};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t writeSome(Span<const byte> data, bool thenFlush = false) {
		(void)thenFlush;
		if (data.empty()) {
			return 0;
		}
		if (output->empty()) {
			throw std::out_of_range{"Buffer capacity exceeded."};
		}
		const size_t bytesWritten = min(output->size(), data.size());
		memcpy(output->data(), data.data(), bytesWritten);
		*output = output->subspan(bytesWritten);
		return bytesWritten;
	}

	GREM_ALWAYS_INLINE void write(Span<const byte> data, bool thenFlush = false) {
		(void)thenFlush;
		if (data.empty()) {
			return;
		}
		if (output->size() < data.size()) {
			throw std::out_of_range{"Buffer capacity exceeded."};
		}
		memcpy(output->data(), data.data(), data.size());
		*output = output->subspan(data.size());
	}

	GREM_ALWAYS_INLINE void flush() {}

	using WriterBase::writeByte;
	using WriterBase::writeChar;
	using WriterBase::writeChars;
	using WriterBase::writeFloat16BE;
	using WriterBase::writeFloat16HE;
	using WriterBase::writeFloat16LE;
	using WriterBase::writeFloat32BE;
	using WriterBase::writeFloat32HE;
	using WriterBase::writeFloat32LE;
	using WriterBase::writeFloat64BE;
	using WriterBase::writeFloat64HE;
	using WriterBase::writeFloat64LE;
	using WriterBase::writeInt16BE;
	using WriterBase::writeInt16HE;
	using WriterBase::writeInt16LE;
	using WriterBase::writeInt32BE;
	using WriterBase::writeInt32HE;
	using WriterBase::writeInt32LE;
	using WriterBase::writeInt64BE;
	using WriterBase::writeInt64HE;
	using WriterBase::writeInt64LE;
	using WriterBase::writeInt8;
	using WriterBase::writeIntLEB128;
	using WriterBase::writeLine;
	using WriterBase::writeNullTerminatedString;
	using WriterBase::writeUInt16BE;
	using WriterBase::writeUInt16HE;
	using WriterBase::writeUInt16LE;
	using WriterBase::writeUInt32BE;
	using WriterBase::writeUInt32HE;
	using WriterBase::writeUInt32LE;
	using WriterBase::writeUInt64BE;
	using WriterBase::writeUInt64HE;
	using WriterBase::writeUInt64LE;
	using WriterBase::writeUInt8;
	using WriterBase::writeUIntLEB128;

private:
	friend WriterBase;

	Span<byte>* output;
};

class SizeCountingWriter : private detail::WriterBase<SizeCountingWriter> {
public:
	GREM_ALWAYS_INLINE SizeCountingWriter(size_t& output) noexcept
		: output(&output) {}

	[[nodiscard]] GREM_ALWAYS_INLINE operator Writer() const {
		return Writer{output, [](void* context, Span<const byte> data, bool thenFlush) -> size_t {
						  size_t& output = *static_cast<size_t*>(context);
						  return SizeCountingWriter{output}.writeSome(data, thenFlush);
					  }};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t writeSome(Span<const byte> data, bool thenFlush = false) {
		(void)thenFlush;
		*output += data.size();
		return data.size();
	}

	GREM_ALWAYS_INLINE void write(Span<const byte> data, bool thenFlush = false) {
		(void)thenFlush;
		*output += data.size();
	}

	GREM_ALWAYS_INLINE void flush() {}

	using WriterBase::writeByte;
	using WriterBase::writeChar;
	using WriterBase::writeChars;
	using WriterBase::writeFloat16BE;
	using WriterBase::writeFloat16HE;
	using WriterBase::writeFloat16LE;
	using WriterBase::writeFloat32BE;
	using WriterBase::writeFloat32HE;
	using WriterBase::writeFloat32LE;
	using WriterBase::writeFloat64BE;
	using WriterBase::writeFloat64HE;
	using WriterBase::writeFloat64LE;
	using WriterBase::writeInt16BE;
	using WriterBase::writeInt16HE;
	using WriterBase::writeInt16LE;
	using WriterBase::writeInt32BE;
	using WriterBase::writeInt32HE;
	using WriterBase::writeInt32LE;
	using WriterBase::writeInt64BE;
	using WriterBase::writeInt64HE;
	using WriterBase::writeInt64LE;
	using WriterBase::writeInt8;
	using WriterBase::writeIntLEB128;
	using WriterBase::writeLine;
	using WriterBase::writeNullTerminatedString;
	using WriterBase::writeUInt16BE;
	using WriterBase::writeUInt16HE;
	using WriterBase::writeUInt16LE;
	using WriterBase::writeUInt32BE;
	using WriterBase::writeUInt32HE;
	using WriterBase::writeUInt32LE;
	using WriterBase::writeUInt64BE;
	using WriterBase::writeUInt64HE;
	using WriterBase::writeUInt64LE;
	using WriterBase::writeUInt8;
	using WriterBase::writeUIntLEB128;

private:
	friend WriterBase;

	size_t* output;
};

} // namespace grem

#endif
