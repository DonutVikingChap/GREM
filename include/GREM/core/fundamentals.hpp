// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FUNDAMENTALS_HPP
#define GREM_CORE_FUNDAMENTALS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Pair.hpp>

#include <bit>         // std::endian, std::bit_..., std::countl_zero, std::countl_one, std::countr_zero, std::countr_one, std::popcount, std::has_single_bit
#include <climits>     // CHAR_BIT
#include <compare>     // std::partial_ordering, std::strong_ordering
#include <cstddef>     // std::size_t, std::ptrdiff_t, std::max_align_t, std::byte
#include <cstdint>     // std::int..._t, std::uint..._t
#include <cstring>     // std::memcpy, std::memmove, std::memset, std::memcmp
#include <limits>      // std::numeric_limits
#include <type_traits> // std::make_signed_t, std::make_unsigned_t, std::common_type_t
#include <utility>     // std::forward

namespace grem {

template <typename T>
struct Limits {
	static constexpr T MIN = std::numeric_limits<T>::lowest();
	static constexpr T MAX = std::numeric_limits<T>::max();
};

template <floating_point T>
struct Limits<T> {
	static constexpr bool IS_IEC60559 = std::numeric_limits<T>::is_iec559;
	static constexpr T MIN = std::numeric_limits<T>::lowest();
	static constexpr T MAX = std::numeric_limits<T>::max();
	static constexpr T SMALLEST_NORMAL = std::numeric_limits<T>::min();
	static constexpr T SMALLEST_SUBNORMAL = std::numeric_limits<T>::denorm_min();
	static constexpr T MACHINE_EPSILON = std::numeric_limits<T>::epsilon();
	static constexpr T INF = std::numeric_limits<T>::infinity();
	static constexpr T QUIET_NAN = std::numeric_limits<T>::quiet_NaN();
};

using std::byte;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
using std::intmax_t;
using std::intptr_t;
using std::max_align_t;
using std::ptrdiff_t;
using std::size_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;
using std::uintmax_t;
using std::uintptr_t;
using ssize_t = std::make_signed_t<size_t>;
using std::bit_cast;
using std::memcmp;
using std::memcpy;
using std::memmove;
using std::memset;

inline constexpr size_t BYTE_BITS = CHAR_BIT;

template <typename T>
struct ValueRepresentation {
	alignas(T) byte bytes[sizeof(T)];

	[[nodiscard]] constexpr bool operator==(const ValueRepresentation&) const noexcept = default;
};

using bool8_t = bool;
enum : bool8_t { false8 = false, true8 = true };
static_assert(sizeof(bool8_t) == 1);

template <>
struct Limits<bool8_t> {
	static constexpr bool8_t MIN = false8;
	static constexpr bool8_t MAX = true8;
};

class bool16_t {
public:
	GREM_ALWAYS_INLINE bool16_t() noexcept = default;

	GREM_ALWAYS_INLINE constexpr bool16_t(bool value) noexcept
		: value(static_cast<uint16_t>(value)) {}

	GREM_ALWAYS_INLINE constexpr operator bool() const noexcept {
		return static_cast<bool>(value);
	}

private:
	uint16_t value;
};
inline constexpr bool16_t false16{false};
inline constexpr bool16_t true16{true};

template <>
struct Limits<bool16_t> {
	static constexpr bool16_t MIN = false16;
	static constexpr bool16_t MAX = true16;
};

class bool32_t {
public:
	GREM_ALWAYS_INLINE bool32_t() noexcept = default;

	GREM_ALWAYS_INLINE constexpr bool32_t(bool value) noexcept
		: value(static_cast<uint16_t>(value)) {}

	GREM_ALWAYS_INLINE constexpr operator bool() const noexcept {
		return static_cast<bool>(value);
	}

private:
	uint32_t value;
};
inline constexpr bool32_t false32{false};
inline constexpr bool32_t true32{true};

template <>
struct Limits<bool32_t> {
	static constexpr bool32_t MIN = false32;
	static constexpr bool32_t MAX = true32;
};

class bool64_t {
public:
	GREM_ALWAYS_INLINE bool64_t() noexcept = default;

	GREM_ALWAYS_INLINE constexpr bool64_t(bool value) noexcept
		: value(static_cast<uint16_t>(value)) {}

	GREM_ALWAYS_INLINE constexpr operator bool() const noexcept {
		return static_cast<bool>(value);
	}

private:
	uint64_t value;
};
inline constexpr bool64_t false64{false};
inline constexpr bool64_t true64{true};

template <>
struct Limits<bool64_t> {
	static constexpr bool64_t MIN = false64;
	static constexpr bool64_t MAX = true64;
};

using float32_t = float;
static_assert(sizeof(float32_t) == 4);
using float64_t = double;
static_assert(sizeof(float64_t) == 8);

struct float16_t {
	uint16_t _private_value{};

	GREM_ALWAYS_INLINE float16_t() noexcept = default;

	GREM_ALWAYS_INLINE constexpr float16_t(float32_t value) noexcept
		: _private_value(getFloat16IntegerRepresentationFromFloat32(value)) {}

	constexpr operator float() const noexcept {
		static_assert(std::numeric_limits<float32_t>::is_iec559);

		constexpr uint32_t F32_EXPONENT_BIAS = 127;
		constexpr uint32_t F16_EXPONENT_BIAS = 15;

		const uint16_t integerRepresentation = _private_value;
		// clang-format off
		const uint32_t signMask          = static_cast<uint32_t>(integerRepresentation & 0b1'00000'0000000000) << 16;
			  uint32_t f16BiasedExponent = static_cast<uint32_t>(integerRepresentation & 0b0'11111'0000000000) >> 10;
			  uint32_t mantissa          = static_cast<uint32_t>(integerRepresentation & 0b0'00000'1111111111);
		// clang-format on

		if (f16BiasedExponent == 31) {
			if (mantissa == 0) {
				// Infinite. Make infinity with the same sign.
				return bit_cast<float32_t>(static_cast<uint32_t>(signMask | 0b0'11111111'00000000000000000000000));
			}
			// Not a number. Make NaN with the same sign.
			return bit_cast<float32_t>(static_cast<uint32_t>(signMask | 0b0'11111111'00000000000000000000000 | (mantissa << 13)));
		}

		if (f16BiasedExponent == 0) {
			if (mantissa == 0) {
				return bit_cast<float32_t>(static_cast<uint32_t>(signMask));
			}

			// Subnormal. Renormalize.
			while (!(mantissa & 0b0'00001'0000000000)) {
				mantissa <<= 1;
				--f16BiasedExponent;
			}
			++f16BiasedExponent;
			mantissa &= 0b1111111111111111'1'11110'1111111111;
		}

		// Make a normal float.
		const uint32_t f32BiasedExponent = f16BiasedExponent + (F32_EXPONENT_BIAS - F16_EXPONENT_BIAS);
		return bit_cast<float32_t>(static_cast<uint32_t>(signMask | (f32BiasedExponent << 23) | (mantissa << 13)));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const float16_t& other) const noexcept {
		return static_cast<float>(*this) == static_cast<float>(other);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(float other) const noexcept {
		return static_cast<float>(*this) == other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr std::partial_ordering operator<=>(const float16_t& other) const noexcept {
		return static_cast<float>(*this) <=> static_cast<float>(other);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr std::partial_ordering operator<=>(float other) const noexcept {
		return static_cast<float>(*this) <=> other;
	}

private:
	friend Limits<float16_t>;

	[[nodiscard]] static constexpr uint16_t getFloat16IntegerRepresentationFromFloat32(float32_t value) noexcept {
		static_assert(std::numeric_limits<float32_t>::is_iec559);

		constexpr uint32_t F32_EXPONENT_BIAS = 127;
		constexpr uint32_t F16_EXPONENT_BIAS = 15;
		constexpr uint32_t F32_BIASED_EXPONENT_MAX = 255;

		const uint32_t integerRepresentation = bit_cast<uint32_t>(value);
		// clang-format off
		const uint16_t signMask          = static_cast<uint16_t>((integerRepresentation & 0b1'00000000'00000000000000000000000) >> 16);
		const uint32_t f32BiasedExponent = static_cast<uint32_t>((integerRepresentation & 0b0'11111111'00000000000000000000000) >> 23);
			   int32_t mantissa          = static_cast< int32_t>((integerRepresentation & 0b0'00000000'11111111111111111111111)      );
		// clang-format on

		if (f32BiasedExponent == F32_BIASED_EXPONENT_MAX) {
			if (mantissa == 0) {
				// Infinite. Make infinity with the same sign.
				return static_cast<uint16_t>(signMask | 0b0'11111'0000000000);
			}
			// Not a number. Make NaN with the same sign.
			mantissa >>= 13;
			return static_cast<uint16_t>(signMask | 0b0'11111'0000000000 | mantissa | (mantissa == 0));
		}

		int32_t f16BiasedExponent = static_cast<int32_t>(f32BiasedExponent) - static_cast<int32_t>(F32_EXPONENT_BIAS - F16_EXPONENT_BIAS);
		if (f16BiasedExponent <= 0) {
			if (f16BiasedExponent < -10) {
				// Magnitude is too small for a half float. Round to 0 with the same sign.
				return static_cast<uint16_t>(signMask);
			}

			// Magnitude is too small for a normal half float. Round to nearest subnormal half float (ties to even).
			mantissa |= 0b0'00000001'00000000000000000000000;
			const int32_t shift = 1 - f16BiasedExponent + 13;
			mantissa = (mantissa + ((1 << (shift - 1)) - 1) + ((mantissa >> shift) & 1)) >> shift;
			return static_cast<uint16_t>(signMask | mantissa);
		}

		// Round to nearest normal half float (ties to even).
		mantissa += 0b0'00011'1111111111 + ((mantissa >> 13) & 1);

		if ((mantissa & 0b0'00000001'00000000000000000000000) != 0) {
			// Mantissa overflowed. Adjust exponent.
			mantissa = 0;
			++f16BiasedExponent;
		}

		if (f16BiasedExponent > 30) {
			// Exponent overflowed. Make infinity with the same sign.
			return static_cast<uint16_t>(signMask | 0b0'11111'0000000000);
		}

		// Make a normal half float.
		return static_cast<uint16_t>(signMask | (f16BiasedExponent << 10) | (mantissa >> 13));
	}

	GREM_ALWAYS_INLINE constexpr float16_t(uint16_t value, int)
		: _private_value(value) {}
};

static_assert(static_cast<float32_t>(static_cast<float16_t>(0.0f)) == 0.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(-0.0f)) == -0.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(1.0f)) == 1.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(-1.0f)) == -1.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(512.0f)) == 512.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(-512.0f)) == -512.0f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(0.125f)) == 0.125f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(-0.125f)) == -0.125f);
static_assert(static_cast<float32_t>(static_cast<float16_t>(std::numeric_limits<float32_t>::infinity())) == std::numeric_limits<float32_t>::infinity());
static_assert(static_cast<float32_t>(static_cast<float16_t>(-std::numeric_limits<float32_t>::infinity())) == -std::numeric_limits<float32_t>::infinity());
static_assert(static_cast<float32_t>(static_cast<float16_t>(std::numeric_limits<float32_t>::quiet_NaN())) !=
			  static_cast<float32_t>(static_cast<float16_t>(std::numeric_limits<float32_t>::quiet_NaN())));

static_assert(static_cast<float16_t>(16.0f) == static_cast<float16_t>(16));
static_assert(static_cast<float16_t>(16.0f) != static_cast<float16_t>(17.0f));
static_assert(static_cast<float16_t>(16.0f) < static_cast<float16_t>(17.0f));

template <>
struct Limits<float16_t> {
	static constexpr bool IS_IEC60559 = true;
	static constexpr float16_t MIN{uint16_t{0b1'11110'1111111111}, 0};
	static constexpr float16_t MAX{uint16_t{0b0'11110'1111111111}, 0};
	static constexpr float16_t SMALLEST_NORMAL{uint16_t{0b0'00001'0000000000}, 0};
	static constexpr float16_t SMALLEST_SUBNORMAL{uint16_t{0b0'00000'0000000001}, 0};
	static constexpr float16_t MACHINE_EPSILON{uint16_t{0b0'00101'0000000000}, 0};
	static constexpr float16_t INF{uint16_t{0b0'11111'0000000000}, 0};
	static constexpr float16_t QUIET_NAN{uint16_t{0b0'11111'1000000000}, 0};
};

namespace detail {

template <typename T>
struct NormalizedInteger {
	T _private_value{};

	GREM_ALWAYS_INLINE NormalizedInteger() noexcept = default;

	GREM_ALWAYS_INLINE constexpr NormalizedInteger(float value) noexcept
		: _private_value(encode(value)) {}

	constexpr operator float() const noexcept {
		return decode(_private_value);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const NormalizedInteger& other) const noexcept {
		return _private_value == other._private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(float other) const noexcept {
		return static_cast<float>(*this) == other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr std::strong_ordering operator<=>(const NormalizedInteger& other) const noexcept {
		return _private_value <=> other._private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr std::partial_ordering operator<=>(float other) const noexcept {
		return static_cast<float>(*this) <=> other;
	}

private:
	friend Limits<NormalizedInteger<T>>;

	[[nodiscard]] static constexpr T encode(float value) noexcept {
		if constexpr (signed_integral<T>) {
			value = (value < -1.0f) ? -1.0f : (1.0f < value) ? 1.0f : value;
			return static_cast<T>((value < 0.0f) ? value * static_cast<float>(Limits<T>::MAX) - 0.5f : value * static_cast<float>(Limits<T>::MAX) + 0.5f);
		} else {
			value = (value < 0.0f) ? 0.0f : (1.0f < value) ? 1.0f : value;
			return static_cast<T>(value * static_cast<float>(Limits<T>::MAX) + 0.5f);
		}
	}

	[[nodiscard]] static constexpr float decode(T value) noexcept {
		if constexpr (signed_integral<T>) {
			const float result = static_cast<float>(value) / static_cast<float>(Limits<T>::MAX);
			return (result < -1.0f) ? -1.0f : result;
		} else {
			return static_cast<float>(value) / static_cast<float>(Limits<T>::MAX);
		}
	}

	GREM_ALWAYS_INLINE constexpr NormalizedInteger(T value, int)
		: _private_value(value) {}
};

} // namespace detail

using i8norm = detail::NormalizedInteger<int8_t>;
using u8norm = detail::NormalizedInteger<uint8_t>;
using i16norm = detail::NormalizedInteger<int16_t>;
using u16norm = detail::NormalizedInteger<uint16_t>;

template <typename T>
struct Limits<detail::NormalizedInteger<T>> {
	static constexpr detail::NormalizedInteger<T> MIN{Limits<T>::MIN, 0};
	static constexpr detail::NormalizedInteger<T> MAX{Limits<T>::MAX, 0};
};

/**
 * Get a copy of an integer with its bit order reversed.
 *
 * \param value value to reverse the bits of.
 *
 * \return a bit-reversed copy of the given value.
 */
template <strict_integral T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T getBitReversed(T value) {
	static_assert(sizeof(T) <= 8);
	if constexpr (sizeof(T) == 1) {
		uint8_t x = bit_cast<uint8_t>(value);
		x = static_cast<uint8_t>(((x & 0x55) << 1) | ((x & 0xAA) >> 1));
		x = static_cast<uint8_t>(((x & 0x33) << 2) | ((x & 0xCC) >> 2));
		x = static_cast<uint8_t>(((x & 0x0F) << 4) | ((x & 0xF0) >> 4));
		return bit_cast<T>(x);
	} else if constexpr (sizeof(T) == 2) {
		uint16_t x = bit_cast<uint16_t>(value);
		x = static_cast<uint16_t>(((x & 0x5555) << 1) | ((x & 0xAAAA) >> 1));
		x = static_cast<uint16_t>(((x & 0x3333) << 2) | ((x & 0xCCCC) >> 2));
		x = static_cast<uint16_t>(((x & 0x0F0F) << 4) | ((x & 0xF0F0) >> 4));
		x = static_cast<uint16_t>(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
		return bit_cast<T>(x);
	} else if constexpr (sizeof(T) == 4) {
		uint32_t x = bit_cast<uint32_t>(value);
		x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
		x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
		x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
		x = ((x & 0x00FF00FF) << 8) | ((x & 0xFF00FF00) >> 8);
		x = ((x & 0x0000FFFF) << 16) | ((x & 0xFFFF0000) >> 16);
		return bit_cast<T>(x);
	} else if constexpr (sizeof(T) == 8) {
		uint64_t x = bit_cast<uint64_t>(value);
		x = ((x & 0x5555555555555555ull) << 1) | ((x & 0xAAAAAAAAAAAAAAAAull) >> 1);
		x = ((x & 0x3333333333333333ull) << 2) | ((x & 0xCCCCCCCCCCCCCCCCull) >> 2);
		x = ((x & 0x0F0F0F0F0F0F0F0Full) << 4) | ((x & 0xF0F0F0F0F0F0F0F0ull) >> 4);
		x = ((x & 0x00FF00FF00FF00FFull) << 8) | ((x & 0xFF00FF00FF00FF00ull) >> 8);
		x = ((x & 0x0000FFFF0000FFFFull) << 16) | ((x & 0xFFFF0000FFFF0000ull) >> 16);
		x = ((x & 0x00000000FFFFFFFFull) << 32) | ((x & 0xFFFFFFFF00000000ull) >> 32);
		return bit_cast<T>(x);
	}
}

/**
 * Get a copy of a value with its byte order reversed.
 *
 * \param value value to reverse the bytes of.
 *
 * \return a byte-reversed copy of the given value.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T getByteReversed(T value) {
	if constexpr (same_as<T, int8_t> || same_as<T, uint8_t>) {
		return value;
	} else if constexpr (same_as<T, int16_t> || same_as<T, uint16_t>) {
		uint16_t x = bit_cast<uint16_t>(value);
		x = static_cast<uint16_t>(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
		return bit_cast<T>(x);
	} else if constexpr (same_as<T, int32_t> || same_as<T, uint32_t>) {
		uint32_t x = bit_cast<uint32_t>(value);
		x = ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);
		return bit_cast<T>(x);
	} else if constexpr (same_as<T, int64_t> || same_as<T, uint64_t>) {
		uint64_t x = bit_cast<uint64_t>(value);
		x = (x & 0x00000000FFFFFFFFull) << 32 | (x & 0xFFFFFFFF00000000ull) >> 32;
		x = (x & 0x0000FFFF0000FFFFull) << 16 | (x & 0xFFFF0000FFFF0000ull) >> 16;
		x = (x & 0x00FF00FF00FF00FFull) << 8 | (x & 0xFF00FF00FF00FF00ull) >> 8;
		return bit_cast<T>(x);
	} else {
		const ValueRepresentation<T> raw = bit_cast<ValueRepresentation<T>>(value);
		ValueRepresentation<T> reversed;
		for (size_t i = 0; i < sizeof(T); ++i) {
			reversed.bytes[i] = raw.bytes[sizeof(T) - 1 - i];
		}
		return bit_cast<T>(reversed);
	}
}

/**
 * Whether the host system's native byte order is little-endian or not.
 */
inline constexpr bool HOST_IS_LITTLE_ENDIAN = std::endian::native == std::endian::little;

/**
 * Whether the host system's native byte order is big-endian or not.
 */
inline constexpr bool HOST_IS_BIG_ENDIAN = std::endian::native == std::endian::big;

/**
 * Convert a value in host-endian byte order to the corresponding value in
 * big-endian byte order.
 *
 * \param value value to convert. Must be represented in the native byte order
 *        of the host system.
 *
 * \return a copy of the given value that is either byte-reversed or unchanged
 *         depending on the host system's native endianness.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T convertHostEndianToBigEndian(T value) {
	if constexpr (HOST_IS_BIG_ENDIAN) {
		return value;
	} else {
		return getByteReversed(value);
	}
}

/**
 * Convert a value in host-endian byte order to the corresponding value in
 * little-endian byte order.
 *
 * \param value value to convert. Must be represented in the native byte order
 *        of the host system.
 *
 * \return a copy of the given value that is either byte-reversed or unchanged
 *         depending on the host system's native endianness.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T convertHostEndianToLittleEndian(T value) {
	if constexpr (HOST_IS_LITTLE_ENDIAN) {
		return value;
	} else {
		return getByteReversed(value);
	}
}

/**
 * Convert a value in big-endian byte order to the corresponding value in
 * host-endian byte order.
 *
 * \param value value to convert. Must be represented in big-endian byte order.
 *
 * \return a copy of the given value that is either byte-reversed or unchanged
 *         depending on the host system's native endianness.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T convertBigEndianToHostEndian(T value) {
	if constexpr (HOST_IS_BIG_ENDIAN) {
		return value;
	} else {
		return getByteReversed(value);
	}
}

/**
 * Convert a value in little-endian byte order to the corresponding value in
 * host-endian byte order.
 *
 * \param value value to convert. Must be represented in little-endian byte
 *        order.
 *
 * \return a copy of the given value that is either byte-reversed or unchanged
 *         depending on the host system's native endianness.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T convertLittleEndianToHostEndian(T value) {
	if constexpr (HOST_IS_LITTLE_ENDIAN) {
		return value;
	} else {
		return getByteReversed(value);
	}
}

/**
 * Get the midpoint between two values.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return half the sum of `a` and `b`, rounded towards `a` for integer types.
 */
template <strict_arithmetic T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T midpoint(T a, T b) {
	if constexpr (floating_point<T>) {
		return (a + b) * T{0.5};
	} else {
		using U = std::make_unsigned_t<T>;
		int sign = 1;
		U minValue = static_cast<U>(a);
		U maxValue = static_cast<U>(b);
		if (a > b) {
			sign = -1;
			minValue = static_cast<U>(b);
			maxValue = static_cast<U>(a);
		}
		return static_cast<T>(a + sign * static_cast<T>(static_cast<U>(maxValue - minValue) / 2));
	}
}

/**
 * Get the smaller of two values, or the first value if they are equivalent.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return `b` if `b` is less than `a`, otherwise `a`.
 */
template <typename A, typename B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr decltype(auto) min(A&& a, B&& b) requires(requires(A x, B y) { x < y; }) {
	return (b < a) ? std::forward<B>(b) : std::forward<A>(a);
}

/**
 * Get the greater of two values, or the first value if they are equivalent.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return `b` if `a` is less than `b`, otherwise `a`.
 */
template <typename A, typename B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr decltype(auto) max(A&& a, B&& b) requires(requires(A x, B y) { x < y; }) {
	return (a < b) ? std::forward<B>(b) : std::forward<A>(a);
}

/**
 * Get the smaller and greater of two values.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return a pair containing (`b`, `a`) if `b` is less than `a`, otherwise
 *         (`a`, `b`).
 */
template <typename A, typename B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto minmax(A&& a, B&& b) requires(requires(A x, B y) { x < y; }) {
	using T = std::common_type_t<A, B>;
	return (b < a) ? Pair<T>(std::forward<B>(b), std::forward<A>(a)) : Pair<T>(std::forward<A>(a), std::forward<B>(b));
}

/**
 * Clamp a value to a specific interval.
 *
 * \param value value to clamp.
 * \param minValue low boundary to clamp the value to. Must be less than or
 *        equal to maxValue.
 * \param maxValue high boundary to clamp the value to. Must be greater than or
 *        equal to minValue.
 *
 * \return `minValue` if `value` is less than `minValue`, `maxValue` if
 *         `maxValue` is less than `value`, `value` otherwise.
 */
template <typename T, typename Low, typename High>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr decltype(auto) clamp(T&& value, Low&& minValue, High&& maxValue)
	requires(requires(T v, Low l) { v < l; } && requires(T v, High h) { h < v; }) {
	GREM_ASSERT(minValue <= maxValue);
	return (value < minValue) ? std::forward<Low>(minValue) : (maxValue < value) ? std::forward<High>(maxValue) : std::forward<T>(value);
}

/**
 * Get the result of bitwise left-rotating a value by a number of bits.
 *
 * \param value unsigned integer value to rotate.
 * \param shift number of bit positions to shift the value to the left by.
 *
 * \return the rotated value.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger rotateBitsLeft(UnsignedInteger value, int shift) noexcept {
	return std::rotl(value, shift);
}

/**
 * Get the result of bitwise right-rotating a value by a number of bits.
 *
 * \param value unsigned integer value to rotate.
 * \param shift number of bit positions to shift the value to the right by.
 *
 * \return the rotated value.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger rotateBitsRight(UnsignedInteger value, int shift) noexcept {
	return std::rotr(value, shift);
}

/**
 * Get the number of 1 bits in a given value.
 *
 * \param value unsigned integer value to get the number of 1 bits of.
 *
 * \return the number of 1 bits in the `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countOneBits(UnsignedInteger value) noexcept {
	return std::popcount(value);
}

/**
 * Get the number of 0 bits in a given value.
 *
 * \param value unsigned integer value to get the number of 0 bits of.
 *
 * \return the number of 0 bits in the `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countZeroBits(UnsignedInteger value) noexcept {
	return static_cast<int>(sizeof(UnsignedInteger) * BYTE_BITS) - countOneBits(value);
}

/**
 * Get the number of consecutive 0 bits starting at the most significant
 * (leftmost) bit of a given value.
 *
 * \param value unsigned integer value to get the number of leading zero bits
 *        of.
 *
 * \return the number of leading 0 bits in `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countLeadingZeroBits(UnsignedInteger value) noexcept {
	return std::countl_zero(value);
}

/**
 * Get the number of consecutive 1 bits starting at the most significant
 * (leftmost) bit of a given value.
 *
 * \param value unsigned integer value to get the number of leading one bits
 *        of.
 *
 * \return the number of leading 1 bits in `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countLeadingOneBits(UnsignedInteger value) noexcept {
	return std::countl_one(value);
}

/**
 * Get the number of consecutive 0 bits starting at the least significant
 * (rightmost) bit of a given value.
 *
 * \param value unsigned integer value to get the number of trailing zero bits
 *        of.
 *
 * \return the number of trailing 0 bits in `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countTrailingZeroBits(UnsignedInteger value) noexcept {
	return std::countr_zero(value);
}

/**
 * Get the number of consecutive 1 bits starting at the least significant
 * (rightmost) bit of a given value.
 *
 * \param value unsigned integer value to get the number of trailing one bits
 *        of.
 *
 * \return the number of trailing 1 bits in `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int countTrailingOneBits(UnsignedInteger value) noexcept {
	return std::countr_one(value);
}

/**
 * Get the minimum number of bits required to represent a given value.
 *
 * \param value unsigned integer value to get the required number of bits of.
 *
 * \return the smallest integer greater than the base-2 logarithm of `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr int getRequiredBitCount(UnsignedInteger value) noexcept {
	return std::bit_width(value);
}

/**
 * Get the maximum value that can be represented in a given unsigned integer
 * type using a given number of bits.
 *
 * \tparam UnsignedInteger unsigned integer type for holding the value.
 *
 * \param bits number of bits to get the maximum value for.
 *
 * \return the maximum value representable with the given number of bits.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger getMaxValueForBits(size_t bits) noexcept {
	return (bits < sizeof(UnsignedInteger) * BYTE_BITS) ? (UnsignedInteger{1} << bits) - 1 : Limits<UnsignedInteger>::MAX;
}

/**
 * Check if a given integer is a power of 2, i.e. if the number appears in the
 * infinite sequence 2^0, 2^1, 2^2, ..., 2^n.
 *
 * For example, the first 15 powers of 2 are: 1, 2, 4, 8, 16, 32, 64, 128, 256,
 * 512, 1024, 2048, 4096, 8192, 16384.
 *
 * \param value unsigned integer value to check.
 *
 * \return true if the given value is a power of 2, false otherwise.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool isPowerOf2(UnsignedInteger value) noexcept {
	return std::has_single_bit(value);
}

/**
 * Get the smallest integral power of 2 that is not less than a given value.
 *
 * \param value unsigned integer value to round.
 *
 * \return the smallest integral power of 2 that is not less than `value`, or 0
 *         if the result cannot be represented in the given integer type.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger roundUpToPowerOf2(UnsignedInteger value) noexcept {
	const int bits = static_cast<int>(sizeof(UnsignedInteger) * BYTE_BITS);
	const int shift = bits - countLeadingZeroBits(static_cast<UnsignedInteger>(value - UnsignedInteger{1}));
	return (shift >= bits) ? 0 : UnsignedInteger{1} << shift;
}

/**
 * Get the largest integral power of 2 that is not greater than a given value.
 *
 * \param value unsigned integer value to round.
 *
 * \return the largest integral power of 2 that is not greater than `value`, or
 *         0 if `value` is 0.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger roundDownToPowerOf2(UnsignedInteger value) noexcept {
	return (value == 0) ? 0 : UnsignedInteger{1} << (getRequiredBitCount(value) - 1);
}

/**
 * Get the smallest multiple of `n` that is not greater than a given value.
 *
 * \param value unsigned integer value to round.
 * \param n divisor to round to. Must be positive.
 *
 * \return the smallest multiple of `n` that is not greater than `value`.
 */
template <strict_unsigned_integral UnsignedInteger>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr UnsignedInteger roundUpToMultiple(UnsignedInteger value, UnsignedInteger n) {
	const UnsignedInteger remainder = value % n;
	const UnsignedInteger correction = (remainder == 0) ? 0 : n - remainder;
	return value + correction;
}

} // namespace grem

#endif
