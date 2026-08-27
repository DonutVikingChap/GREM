// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_CRC32_HPP
#define GREM_CORE_FORMATS_CRC32_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash

namespace grem {

class CRC32 {
private:
	static constexpr Array<uint32_t, 256> CRC_TABLE = [] {
		Array<uint32_t, 256> result{};
		uint32_t crc32 = 1;
		for (size_t i = 128; i != 0; i >>= 1) {
			crc32 = (crc32 >> 1) ^ ((crc32 & 1) ? 0xEDB88320 : 0);
			for (size_t j = 0; j < 256; j += i * 2) {
				result[i + j] = crc32 ^ result[j];
			}
		}
		return result;
	}();

public:
	constexpr CRC32() noexcept = default;

	constexpr explicit CRC32(uint32_t value) noexcept
		: value(value) {}

	constexpr explicit CRC32(Span<const byte> bytes) noexcept {
		append(bytes);
	}

	constexpr explicit CRC32(StringView chars) noexcept {
		append(chars);
	}

	constexpr explicit operator uint32_t() const noexcept {
		return value;
	}

	constexpr bool operator==(const CRC32&) const noexcept = default;

	constexpr void append(Span<const byte> bytes) noexcept {
		value = ~value;
		for (const byte byte : bytes) {
			value = (value >> 8) ^ CRC_TABLE[(value ^ static_cast<uint32_t>(bit_cast<uint8_t>(byte))) & 0xFF];
		}
		value = ~value;
	}

	constexpr void append(StringView chars) noexcept {
		value = ~value;
		for (const char ch : chars) {
			value = (value >> 8) ^ CRC_TABLE[(value ^ static_cast<uint32_t>(bit_cast<uint8_t>(ch))) & 0xFF];
		}
		value = ~value;
	}

	constexpr CRC32& operator+=(Span<const byte> bytes) noexcept {
		append(bytes);
		return *this;
	}

	constexpr CRC32& operator+=(StringView chars) noexcept {
		append(chars);
		return *this;
	}

	[[nodiscard]] friend constexpr CRC32 operator+(CRC32 a, Span<const byte> bytes) noexcept {
		return a += bytes;
	}

	[[nodiscard]] friend constexpr CRC32 operator+(CRC32 a, StringView chars) noexcept {
		return a += chars;
	}

	template <typename Stream>
	friend constexpr Stream& operator<<(Stream& stream, CRC32 crc32) {
		return stream << crc32.value;
	}

	template <typename Stream>
	friend constexpr Stream& operator>>(Stream& stream, CRC32& crc32) {
		return stream >> crc32.value;
	}

private:
	uint32_t value = 0;
};

} // namespace grem

template <>
struct std::hash<grem::CRC32> {
	[[nodiscard]] std::size_t operator()(const grem::CRC32& crc32) const {
		return hasher(static_cast<uint32_t>(crc32));
	}

private:
	std::hash<uint32_t> hasher;
};

#endif
