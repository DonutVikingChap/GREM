// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_COORDINATES_HPP
#define GREM_EXAMPLES_TILES_COORDINATES_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash

/**
 * Fixed-point 32-bit world coordinate for tilemaps.
 *
 * Consists of a signed 24-bit tile coordinate and an unsigned 8-bit sub-tile
 * coordinate, yielding uniform 256-step sub-tile precision across the whole
 * map from tile -8388608 to +8388607.
 */
struct Coordinate {
	using value_type = int32_t;
	static_assert(signed_integral<value_type>);
	static constexpr value_type SUB_TILE_BITS = 8;
	static constexpr value_type SUB_TILE_MASK = (value_type{1} << SUB_TILE_BITS) - 1;
	static constexpr value_type SUB_TILE_STEPS = SUB_TILE_MASK + 1;
	static constexpr value_type TILE_BITS = sizeof(value_type) * BYTE_BITS - SUB_TILE_BITS;
	static constexpr value_type TILE_MASK = (value_type{1} << TILE_BITS) - 1;
	static constexpr value_type TILE_MIN = -(value_type{1} << (TILE_BITS - 1));
	static constexpr value_type TILE_MAX = (value_type{1} << (TILE_BITS - 1)) - 1;

	value_type value = 0;

	[[nodiscard]] static GREM_ALWAYS_INLINE constexpr Pair<value_type, float> canonical(int32_t tileCoordinate, float subTileCoordinate) {
		subTileCoordinate = clamp(subTileCoordinate, float{TILE_MIN}, float{TILE_MAX});
		const value_type subTileIntegerCoordinate = static_cast<value_type>(subTileCoordinate);
		const value_type subTilePhase = (subTileCoordinate < static_cast<float>(subTileIntegerCoordinate)) ? subTileIntegerCoordinate - 1 : subTileIntegerCoordinate;
		tileCoordinate += static_cast<int32_t>(subTilePhase);
		subTileCoordinate -= static_cast<float>(subTilePhase);
		GREM_ASSERT(subTileCoordinate >= 0.0f && subTileCoordinate < 1.0f);
		return {tileCoordinate, subTileCoordinate};
	}

	[[nodiscard]] static GREM_ALWAYS_INLINE constexpr Coordinate floor(int32_t tileCoordinate, float subTileCoordinate) {
		const auto [canonicalTileCoordinate, canonicalSubTileCoordinate] = canonical(tileCoordinate, subTileCoordinate);
		Coordinate result{};
		result.value = ((static_cast<value_type>(canonicalTileCoordinate) & TILE_MASK) << SUB_TILE_BITS) |
		               static_cast<value_type>(grem::floor(canonicalSubTileCoordinate * float{SUB_TILE_STEPS}));
		return result;
	}

	[[nodiscard]] static GREM_ALWAYS_INLINE constexpr Coordinate round(int32_t tileCoordinate, float subTileCoordinate) {
		const auto [canonicalTileCoordinate, canonicalSubTileCoordinate] = canonical(tileCoordinate, subTileCoordinate);
		const float subTileStep = grem::round(canonicalSubTileCoordinate * float{SUB_TILE_STEPS});
		Coordinate result{};
		result.value = (subTileStep < float{SUB_TILE_STEPS})
		                   ? (((static_cast<value_type>(canonicalTileCoordinate) & TILE_MASK) << SUB_TILE_BITS) | static_cast<value_type>(subTileStep))
		                   : ((static_cast<value_type>(canonicalTileCoordinate + 1) & TILE_MASK) << SUB_TILE_BITS);
		return result;
	}

	GREM_ALWAYS_INLINE constexpr Coordinate() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Coordinate(int integerTileCoordinate)
		: value(static_cast<value_type>(integerTileCoordinate) << SUB_TILE_BITS) {}

	GREM_ALWAYS_INLINE constexpr Coordinate(double floatingPointTileCoordinate) {
		floatingPointTileCoordinate = clamp(floatingPointTileCoordinate, double{TILE_MIN}, double{TILE_MAX});
		const value_type integerCoordinate = static_cast<value_type>(floatingPointTileCoordinate);
		const value_type tileCoordinate = (floatingPointTileCoordinate < static_cast<double>(integerCoordinate)) ? integerCoordinate - 1 : integerCoordinate;
		const float subTileCoordinate = static_cast<float>(floatingPointTileCoordinate - static_cast<double>(tileCoordinate));
		*this = Coordinate::floor(static_cast<int32_t>(tileCoordinate), subTileCoordinate);
	}

	GREM_ALWAYS_INLINE constexpr explicit operator float() const noexcept {
		return static_cast<float>(getTileCoordinate()) + getSubTileCoordinate();
	}

	[[nodiscard]] constexpr bool operator==(const Coordinate&) const noexcept = default;
	[[nodiscard]] constexpr auto operator<=>(const Coordinate&) const noexcept = default;

	GREM_ALWAYS_INLINE constexpr Coordinate& operator+=(Coordinate other) {
		value += other.value;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr Coordinate& operator-=(Coordinate other) {
		value -= other.value;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr Coordinate& operator*=(value_type scalar) {
		value *= scalar;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr Coordinate& operator/=(value_type scalar) {
		value /= scalar;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr Coordinate& operator+=(float scalar) {
		return *this = Coordinate::round(getTileCoordinate(), getSubTileCoordinate() + scalar);
	}

	GREM_ALWAYS_INLINE constexpr Coordinate& operator-=(float scalar) {
		return *this = Coordinate::round(getTileCoordinate(), getSubTileCoordinate() - scalar);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr int32_t getTileCoordinate() const noexcept {
		return static_cast<int32_t>(value >> SUB_TILE_BITS);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr float getSubTileCoordinate() const noexcept {
		return static_cast<float>(value & SUB_TILE_MASK) / float{SUB_TILE_STEPS};
	}
};

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator-(Coordinate a) {
	Coordinate result{};
	result.value = -a.value;
	return result;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator+(Coordinate a, Coordinate b) {
	return a += b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator+(Coordinate a, float b) {
	return a += b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator+(float a, Coordinate b) {
	return b += a;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator-(Coordinate a, Coordinate b) {
	return a -= b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator-(Coordinate a, float b) {
	return a -= b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator-(float a, Coordinate b) {
	return b -= a;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator*(Coordinate a, Coordinate::value_type b) {
	return a *= b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator*(Coordinate::value_type a, Coordinate b) {
	return b *= a;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate operator/(Coordinate a, Coordinate::value_type b) {
	return a /= b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate abs(Coordinate a) {
	return (a < Coordinate{}) ? -a : a;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate mix(Coordinate a, Coordinate b, float alpha) {
	return Coordinate::floor(a.getTileCoordinate(), a.getSubTileCoordinate() + static_cast<float>(b - a) * alpha);
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinate midpoint(Coordinate a, Coordinate b) {
	Coordinate result;
	result.value = midpoint(a.value, b.value);
	return result;
}

template <size_t N>
using Coordinates = vec<N, Coordinate>;
using Coordinates2D = Coordinates<2>;
using Coordinates3D = Coordinates<3>;

template <size_t N>
GREM_ALWAYS_INLINE constexpr Coordinates<N>& operator+=(Coordinates<N>& a, vec<N, float> b) {
	for (size_t i = 0; i < N; ++i) {
		a[i] += b[i];
	}
	return a;
}

template <size_t N>
GREM_ALWAYS_INLINE constexpr Coordinates<N>& operator-=(Coordinates<N>& a, vec<N, float> b) {
	for (size_t i = 0; i < N; ++i) {
		a[i] -= b[i];
	}
	return a;
}

template <size_t N>
GREM_ALWAYS_INLINE constexpr Coordinates<N>& operator*=(Coordinates<N>& a, Coordinate::value_type b) {
	for (size_t i = 0; i < N; ++i) {
		a[i] *= b;
	}
	return a;
}

template <size_t N>
GREM_ALWAYS_INLINE constexpr Coordinates<N>& operator/=(Coordinates<N>& a, Coordinate::value_type b) {
	for (size_t i = 0; i < N; ++i) {
		a[i] /= b;
	}
	return a;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator+(Coordinates<N> a, vec<N, float> b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] + b[i];
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator+(vec<N, float> a, Coordinates<N> b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] + b[i];
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator-(Coordinates<N> a, vec<N, float> b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] - b[i];
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator-(vec<N, float> a, Coordinates<N> b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] - b[i];
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator*(Coordinates<N> a, Coordinate::value_type b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] * b;
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator*(Coordinate::value_type a, Coordinates<N> b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a * b[i];
	}
	return result;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Coordinates<N> operator/(Coordinates<N> a, Coordinate::value_type b) {
	Coordinates<N> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = a[i] / b;
	}
	return result;
}

template <>
struct grem::Formatter<Coordinate> : Formatter<StringView> {
	void formatTo(FormatOutput& output, const Coordinate& value) const {
		constexpr int SUB_TILE_PRECISION = 100;

		Coordinate::value_type tile = value.getTileCoordinate();
		int subTile = static_cast<int>(round(value.getSubTileCoordinate() * static_cast<float>(SUB_TILE_PRECISION)));
		bool zeroWithNegativeDecimal = false;
		if (subTile == SUB_TILE_PRECISION) {
			++tile;
			subTile = 0;
		}
		if (tile < 0) {
			if (subTile > 0) {
				++tile;
				subTile = SUB_TILE_PRECISION - subTile;
			}
			zeroWithNegativeDecimal = tile == 0;
		}
		Formatter<StringView>::formatTo(output, formatString("{}{}.{:02}", (zeroWithNegativeDecimal) ? "-" : "", tile, subTile));
	}
};

template <>
struct std::hash<Coordinate> {
	[[nodiscard]] std::size_t operator()(Coordinate coordinate) const {
		return valueHasher(coordinate.value);
	}

private:
	[[no_unique_address]] std::hash<Coordinate::value_type> valueHasher;
};

#endif
