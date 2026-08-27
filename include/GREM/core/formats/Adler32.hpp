// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_ADLER32_HPP
#define GREM_CORE_FORMATS_ADLER32_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash

namespace grem {

class alignas(4) Adler32 {
public:
	constexpr Adler32() noexcept = default;

	constexpr explicit Adler32(uint32_t value) noexcept
		: s1(static_cast<uint16_t>(value & 0xFFFF))
		, s2(static_cast<uint16_t>(value >> 16)) {}

	constexpr explicit Adler32(Span<const byte> bytes) noexcept {
		append(bytes);
	}

	constexpr explicit Adler32(StringView chars) noexcept {
		append(chars);
	}

	constexpr explicit operator uint32_t() const noexcept {
		return static_cast<uint32_t>((uint32_t{s2} << 16) | uint32_t{s1});
	}

	constexpr bool operator==(const Adler32&) const noexcept = default;

	constexpr void append(Span<const byte> bytes) noexcept {
		for (const byte byte : bytes) {
			s1 = static_cast<uint16_t>((uint32_t{s1} + uint32_t{bit_cast<uint8_t>(byte)}) % 65521);
			s2 = static_cast<uint16_t>((uint32_t{s2} + uint32_t{s1}) % 65521);
		}
	}

	constexpr void append(StringView chars) noexcept {
		for (const char ch : chars) {
			s1 = static_cast<uint16_t>((uint32_t{s1} + uint32_t{bit_cast<uint8_t>(ch)}) % 65521);
			s2 = static_cast<uint16_t>((uint32_t{s2} + uint32_t{s1}) % 65521);
		}
	}

	constexpr Adler32& operator+=(Span<const byte> bytes) noexcept {
		append(bytes);
		return *this;
	}

	constexpr Adler32& operator+=(StringView chars) noexcept {
		append(chars);
		return *this;
	}

	[[nodiscard]] friend constexpr Adler32 operator+(Adler32 a, Span<const byte> bytes) noexcept {
		return a += bytes;
	}

	[[nodiscard]] friend constexpr Adler32 operator+(Adler32 a, StringView chars) noexcept {
		return a += chars;
	}

	template <typename Stream>
	friend constexpr Stream& operator<<(Stream& stream, Adler32 adler32) {
		return stream << static_cast<uint32_t>(adler32);
	}

	template <typename Stream>
	friend constexpr Stream& operator>>(Stream& stream, Adler32& adler32) {
		uint32_t value{};
		stream >> value;
		adler32 = Adler32{value};
		return stream;
	}

private:
	uint16_t s1 = 1;
	uint16_t s2 = 0;
};

} // namespace grem

template <>
struct std::hash<grem::Adler32> {
	[[nodiscard]] std::size_t operator()(const grem::Adler32& adler32) const {
		return hasher(static_cast<uint32_t>(adler32));
	}

private:
	std::hash<uint32_t> hasher;
};

#endif
