// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_EXTENTS_HPP
#define GREM_CORE_EXTENTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem {

/**
 * Offset in a data region.
 *
 * \tparam N number of data dimensions. Must be 2 or 3.
 */
template <size_t N>
struct Offset;
using Offset2D = Offset<2>; ///< Offset in a 2D data region.
using Offset3D = Offset<3>; ///< Offset in a 3D data region.

/**
 * Size of a data region.
 *
 * \tparam N number of data dimensions. Must be 2 or 3.
 */
template <size_t N>
struct Extent;
using Extent2D = Extent<2>; ///< Size of a 2D data region.
using Extent3D = Extent<3>; ///< Size of a 3D data region.

template <>
struct Offset<3> {
	/**
	 * Create an offset from a vector of integers.
	 *
	 * \param v vector to convert.
	 *
	 * \return the corresponding offset.
	 */
	[[nodiscard]] static constexpr Offset<3> from(i32vec3 v) noexcept {
		return {.x = v.x, .y = v.y, .z = v.z};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded to
	 * their nearest integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the rounded offset.
	 */
	[[nodiscard]] static Offset<3> round(vec3 v) {
		return {
			.x = static_cast<int32_t>(grem::round(v.x)),
			.y = static_cast<int32_t>(grem::round(v.y)),
			.z = static_cast<int32_t>(grem::round(v.z)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded down to
	 * their floor integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the floor-rounded offset.
	 */
	[[nodiscard]] static Offset<3> floor(vec3 v) {
		return {
			.x = static_cast<int32_t>(grem::floor(v.x)),
			.y = static_cast<int32_t>(grem::floor(v.y)),
			.z = static_cast<int32_t>(grem::floor(v.z)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded up to
	 * their ceiling integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the ceiling-rounded offset.
	 */
	[[nodiscard]] static Offset<3> ceil(vec3 v) {
		return {
			.x = static_cast<int32_t>(grem::ceil(v.x)),
			.y = static_cast<int32_t>(grem::ceil(v.y)),
			.z = static_cast<int32_t>(grem::ceil(v.z)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded to
	 * their truncated integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the truncated offset.
	 */
	[[nodiscard]] static Offset<3> trunc(vec3 v) {
		return {
			.x = static_cast<int32_t>(grem::trunc(v.x)),
			.y = static_cast<int32_t>(grem::trunc(v.y)),
			.z = static_cast<int32_t>(grem::trunc(v.z)),
		};
	}

	int32_t x; ///< Horizontal offset.
	int32_t y; ///< Vertical offset.
	int32_t z; ///< Depth offset.

	/**
	 * Convert this offset to a floating-point 3D vector.
	 *
	 * \return a 3D vector representing the offset.
	 */
	constexpr operator vec3() const noexcept {
		return vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
	}

	/**
	 * Convert this offset to an integer 3D vector.
	 *
	 * \return a 3D vector representing the offset.
	 */
	constexpr operator ivec3() const noexcept {
		return ivec3{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)};
	}

	/**
	 * Convert this offset to an unsigned integer 3D vector.
	 *
	 * \return a 3D vector representing the offset.
	 */
	constexpr operator u32vec3() const noexcept {
		return u32vec3{static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z)};
	}

	/**
	 * Compare this offset to another for equality.
	 *
	 * \param other the offset to compare this one to.
	 *
	 * \return true if the offsets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Offset<3>& other) const = default;
};

template <>
struct Extent<3> {
	/**
	 * Create an extent from a vector of integers.
	 *
	 * \param v vector to convert.
	 *
	 * \return the corresponding extent.
	 */
	[[nodiscard]] static constexpr Extent<3> from(u32vec3 v) noexcept {
		return {.width = v.x, .height = v.y, .depth = v.z};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded to
	 * their nearest integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the rounded extent.
	 */
	[[nodiscard]] static Extent<3> round(vec3 v) {
		return {
			.width = static_cast<uint32_t>(grem::round(v.x)),
			.height = static_cast<uint32_t>(grem::round(v.y)),
			.depth = static_cast<uint32_t>(grem::round(v.z)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded down to
	 * their floor integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the floor-rounded extent.
	 */
	[[nodiscard]] static Extent<3> floor(vec3 v) {
		return {
			.width = static_cast<uint32_t>(grem::floor(v.x)),
			.height = static_cast<uint32_t>(grem::floor(v.y)),
			.depth = static_cast<uint32_t>(grem::floor(v.z)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded up to
	 * their ceiling integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the ceiling-rounded extent.
	 */
	[[nodiscard]] static Extent<3> ceil(vec3 v) {
		return {
			.width = static_cast<uint32_t>(grem::ceil(v.x)),
			.height = static_cast<uint32_t>(grem::ceil(v.y)),
			.depth = static_cast<uint32_t>(grem::ceil(v.z)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded to
	 * their truncated integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the truncated extent.
	 */
	[[nodiscard]] static Extent<3> trunc(vec3 v) {
		return {
			.width = static_cast<uint32_t>(grem::trunc(v.x)),
			.height = static_cast<uint32_t>(grem::trunc(v.y)),
			.depth = static_cast<uint32_t>(grem::trunc(v.z)),
		};
	}

	uint32_t width;          ///< Horizontal extent.
	uint32_t height = width; ///< Vertical extent.
	uint32_t depth = 1;      ///< Number of layers.

	/**
	 * Convert this extent to an offset.
	 *
	 * \return an offset with the width, height and depth of the extent as its
	 *         x, y and z components, respectively.
	 */
	constexpr explicit operator Offset<3>() const noexcept {
		return Offset<3>{.x = static_cast<int32_t>(width), .y = static_cast<int32_t>(height), .z = static_cast<int32_t>(depth)};
	}

	/**
	 * Convert this extent to a floating-point 3D vector.
	 *
	 * \return a 3D vector holding the width, height and depth of the extent in
	 *         its x, y and z components, respectively.
	 */
	constexpr operator vec3() const noexcept {
		return vec3{static_cast<float>(width), static_cast<float>(height), static_cast<float>(depth)};
	}

	/**
	 * Convert this extent to an integer 3D vector.
	 *
	 * \return a 3D vector holding the width, height and depth of the extent in
	 *         its x, y and z components, respectively.
	 */
	constexpr operator ivec3() const noexcept {
		return ivec3{static_cast<int>(width), static_cast<int>(height), static_cast<int>(depth)};
	}

	/**
	 * Convert this extent to an unsigned integer 3D vector.
	 *
	 * \return a 3D vector holding the width, height and depth of the extent in
	 *         its x, y and z components, respectively.
	 */
	constexpr operator u32vec3() const noexcept {
		return u32vec3{width, height, depth};
	}

	/**
	 * Compare this extent to another for equality.
	 *
	 * \param other the extent to compare this one to.
	 *
	 * \return true if the extents are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Extent<3>& other) const = default;

	/**
	 * Fit the extents into a target size at the largest positive integer scale
	 * of its original size that still fits within the target.
	 *
	 * \param targetSize target size to fit the extents into.
	 *
	 * \return the integer scale that was chosen.
	 *
	 * \note If the original size cannot fit within the target, then the chosen
	 *       scale will be 1.
	 */
	uint32_t fitIntegerScaled(Extent targetSize) {
		uint32_t scale = 1;
		const uint32_t originalWidth = width;
		const uint32_t originalHeight = height;
		const uint32_t originalDepth = depth;
		while (true) {
			const uint32_t nextWidth = originalWidth * (scale + 1);
			const uint32_t nextHeight = originalHeight * (scale + 1);
			const uint32_t nextDepth = originalDepth * (scale + 1);
			if (nextWidth > targetSize.width || nextHeight > targetSize.height || nextDepth > targetSize.depth) {
				break;
			}
			width = nextWidth;
			height = nextHeight;
			depth = nextDepth;
			++scale;
		}
		return scale;
	}
};

template <>
struct Offset<2> {
	/**
	 * Create an offset from a vector of integers.
	 *
	 * \param v vector to convert.
	 *
	 * \return the corresponding offset.
	 */
	[[nodiscard]] static constexpr Offset<2> from(i32vec2 v) noexcept {
		return {.x = v.x, .y = v.y};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded to
	 * their nearest integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the rounded offset.
	 */
	[[nodiscard]] static Offset<2> round(vec2 v) {
		return {
			.x = static_cast<int32_t>(grem::round(v.x)),
			.y = static_cast<int32_t>(grem::round(v.y)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded down to
	 * their floor integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the floor-rounded offset.
	 */
	[[nodiscard]] static Offset<2> floor(vec2 v) {
		return {
			.x = static_cast<int32_t>(grem::floor(v.x)),
			.y = static_cast<int32_t>(grem::floor(v.y)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded up to
	 * their ceiling integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the ceiling-rounded offset.
	 */
	[[nodiscard]] static Offset<2> ceil(vec2 v) {
		return {
			.x = static_cast<int32_t>(grem::ceil(v.x)),
			.y = static_cast<int32_t>(grem::ceil(v.y)),
		};
	}

	/**
	 * Create an offset from a vector of floating-point numbers, rounded to
	 * their truncated integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the truncated offset.
	 */
	[[nodiscard]] static Offset<2> trunc(vec2 v) {
		return {
			.x = static_cast<int32_t>(grem::trunc(v.x)),
			.y = static_cast<int32_t>(grem::trunc(v.y)),
		};
	}

	int32_t x; ///< Horizontal offset.
	int32_t y; ///< Vertical offset.

	/**
	 * Convert this offset to a 3D offset.
	 *
	 * \return the equivalent 3D offset, with a depth offset of 0.
	 */
	constexpr operator Offset<3>() const noexcept {
		return Offset<3>{.x = x, .y = y, .z = 0};
	}

	/**
	 * Convert this offset to a floating-point 2D vector.
	 *
	 * \return a 2D vector representing the offset.
	 */
	constexpr operator vec2() const noexcept {
		return vec2{static_cast<float>(x), static_cast<float>(y)};
	}

	/**
	 * Convert this offset to an integer 2D vector.
	 *
	 * \return a 2D vector representing the offset.
	 */
	constexpr operator ivec2() const noexcept {
		return ivec2{static_cast<int>(x), static_cast<int>(y)};
	}

	/**
	 * Convert this offset to an unsigned integer 2D vector.
	 *
	 * \return a 2D vector representing the offset.
	 */
	constexpr operator u32vec2() const noexcept {
		return u32vec2{static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
	}

	/**
	 * Compare this offset to another for equality.
	 *
	 * \param other the offset to compare this one to.
	 *
	 * \return true if the offsets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Offset<2>& other) const = default;
};

template <>
struct Extent<2> {
	/**
	 * Create an extent from a vector of integers.
	 *
	 * \param v vector to convert.
	 *
	 * \return the corresponding extent.
	 */
	[[nodiscard]] static constexpr Extent<2> from(u32vec2 v) noexcept {
		return {.width = v.x, .height = v.y};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded to
	 * their nearest integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the rounded extent.
	 */
	[[nodiscard]] static Extent<2> round(vec2 v) {
		return {
			.width = static_cast<uint32_t>(grem::round(v.x)),
			.height = static_cast<uint32_t>(grem::round(v.y)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded down to
	 * their floor integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the floor-rounded extent.
	 */
	[[nodiscard]] static Extent<2> floor(vec2 v) {
		return {
			.width = static_cast<uint32_t>(grem::floor(v.x)),
			.height = static_cast<uint32_t>(grem::floor(v.y)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded up to
	 * their ceiling integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the ceiling-rounded extent.
	 */
	[[nodiscard]] static Extent<2> ceil(vec2 v) {
		return {
			.width = static_cast<uint32_t>(grem::ceil(v.x)),
			.height = static_cast<uint32_t>(grem::ceil(v.y)),
		};
	}

	/**
	 * Create an extent from a vector of floating-point numbers, rounded to
	 * their truncated integer values before conversion.
	 *
	 * \param v vector to convert.
	 *
	 * \return the truncated extent.
	 */
	[[nodiscard]] static Extent<2> trunc(vec2 v) {
		return {
			.width = static_cast<uint32_t>(grem::trunc(v.x)),
			.height = static_cast<uint32_t>(grem::trunc(v.y)),
		};
	}

	uint32_t width;          ///< Horizontal extent of the region.
	uint32_t height = width; ///< Vertical extent of the region.

	/**
	 * Convert this extent to an offset.
	 *
	 * \return an offset with the width and height of the extent as its x and y
	 *         components, respectively.
	 */
	constexpr explicit operator Offset<2>() const noexcept {
		return Offset<2>{.x = static_cast<int32_t>(width), .y = static_cast<int32_t>(height)};
	}

	/**
	 * Convert this extent to a floating-point 2D vector.
	 *
	 * \return a 2D vector holding the width and height of the extent in its x
	 *         and y components, respectively.
	 */
	constexpr operator vec2() const noexcept {
		return vec2{static_cast<float>(width), static_cast<float>(height)};
	}

	/**
	 * Convert this extent to an integer 2D vector.
	 *
	 * \return a 2D vector holding the width and height of the extent in its x
	 *         and y components, respectively.
	 */
	constexpr operator ivec2() const noexcept {
		return ivec2{static_cast<int>(width), static_cast<int>(height)};
	}

	/**
	 * Convert this extent to an unsigned integer 2D vector.
	 *
	 * \return a 2D vector holding the width and height of the extent in its x
	 *         and y components, respectively.
	 */
	constexpr operator u32vec2() const noexcept {
		return u32vec2{width, height};
	}

	/**
	 * Convert this extent to a 3D extent.
	 *
	 * \return the equivalent 3D extent, with a depth of 1.
	 */
	constexpr operator Extent<3>() const noexcept {
		return Extent<3>{.width = width, .height = height, .depth = 1};
	}

	/**
	 * Compare this extent to another for equality.
	 *
	 * \param other the extent to compare this one to.
	 *
	 * \return true if the extents are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Extent2D& other) const = default;

	/**
	 * Get the aspect ratio of the extent (width / height).
	 *
	 * \return `width / height` as a float.
	 */
	[[nodiscard]] float getAspectRatio() const {
		return static_cast<float>(width) / static_cast<float>(height);
	}

	/**
	 * Fit the extents into a target size at the largest positive integer scale
	 * of its original size that still fits within the target.
	 *
	 * \param targetSize target size to fit the extents into.
	 *
	 * \return the integer scale that was chosen.
	 *
	 * \note If the original size cannot fit within the target, then the chosen
	 *       scale will be 1.
	 */
	uint32_t fitIntegerScaled(Extent targetSize) {
		uint32_t scale = 1;
		const uint32_t originalWidth = width;
		const uint32_t originalHeight = height;
		while (true) {
			const uint32_t nextWidth = originalWidth * (scale + 1);
			const uint32_t nextHeight = originalHeight * (scale + 1);
			if (nextWidth > targetSize.width || nextHeight > targetSize.height) {
				break;
			}
			width = nextWidth;
			height = nextHeight;
			++scale;
		}
		return scale;
	}
};

constexpr Offset2D& operator+=(Offset2D& a, Offset2D b) noexcept {
	a.x += b.x;
	a.y += b.y;
	return a;
}

constexpr Offset2D& operator+=(Offset2D& a, Extent2D b) noexcept {
	a.x += static_cast<int32_t>(b.width);
	a.y += static_cast<int32_t>(b.height);
	return a;
}

constexpr Offset2D& operator-=(Offset2D& a, Extent2D b) noexcept {
	a.x -= static_cast<int32_t>(b.width);
	a.y -= static_cast<int32_t>(b.height);
	return a;
}

[[nodiscard]] constexpr Offset2D operator-(Offset2D a) noexcept {
	return {.x = -a.x, .y = -a.y};
}

[[nodiscard]] constexpr Offset2D operator+(Offset2D a, Offset2D b) noexcept {
	return {.x = a.x + b.x, .y = a.y + b.y};
}

[[nodiscard]] constexpr Offset2D operator+(Offset2D a, Extent2D b) noexcept {
	return {.x = a.x + static_cast<int32_t>(b.width), .y = a.y + static_cast<int32_t>(b.height)};
}

[[nodiscard]] constexpr Offset2D operator+(Extent2D a, Offset2D b) noexcept {
	return {.x = static_cast<int32_t>(a.width) + b.x, .y = static_cast<int32_t>(a.height) + b.y};
}

[[nodiscard]] constexpr Offset2D operator-(Offset2D a, Offset2D b) noexcept {
	return {.x = a.x - b.x, .y = a.y - b.y};
}

[[nodiscard]] constexpr Offset2D operator-(Offset2D a, Extent2D b) noexcept {
	return {.x = a.x - static_cast<int32_t>(b.width), .y = a.y - static_cast<int32_t>(b.height)};
}

[[nodiscard]] constexpr Offset2D operator-(Extent2D a, Offset2D b) noexcept {
	return {.x = static_cast<int32_t>(a.width) - b.x, .y = static_cast<int32_t>(a.height) - b.y};
}

[[nodiscard]] constexpr Offset2D operator*(Offset2D a, int32_t b) noexcept {
	return {.x = a.x * b, .y = a.y * b};
}

[[nodiscard]] constexpr Offset2D operator*(int32_t a, Offset2D b) noexcept {
	return {.x = a * b.x, .y = a * b.y};
}

[[nodiscard]] constexpr Offset2D operator/(Offset2D a, int32_t b) noexcept {
	return {.x = a.x / b, .y = a.y / b};
}

[[nodiscard]] constexpr Offset2D operator/(int32_t a, Offset2D b) noexcept {
	return {.x = a / b.x, .y = a / b.y};
}

[[nodiscard]] constexpr Offset2D operator*(Offset2D a, uint32_t b) noexcept {
	return a * static_cast<int32_t>(b);
}

[[nodiscard]] constexpr Offset2D operator*(uint32_t a, Offset2D b) noexcept {
	return static_cast<int32_t>(a) * b;
}

[[nodiscard]] constexpr Offset2D operator/(Offset2D a, uint32_t b) noexcept {
	return a / static_cast<int32_t>(b);
}

[[nodiscard]] constexpr Offset2D operator/(uint32_t a, Offset2D b) noexcept {
	return static_cast<int32_t>(a) / b;
}

constexpr Extent2D& operator+=(Extent2D& a, Extent2D b) noexcept {
	a.width += b.width;
	a.height += b.height;
	return a;
}

constexpr Extent2D& operator-=(Extent2D& a, Extent2D b) noexcept {
	a.width -= b.width;
	a.height -= b.height;
	return a;
}

constexpr Extent2D& operator*=(Extent2D& a, uint32_t b) noexcept {
	a.width *= b;
	a.height *= b;
	return a;
}

constexpr Extent2D& operator/=(Extent2D& a, uint32_t b) noexcept {
	a.width /= b;
	a.height /= b;
	return a;
}

[[nodiscard]] constexpr Extent2D operator+(Extent2D a, Extent2D b) noexcept {
	return {.width = a.width + b.width, .height = a.height + b.height};
}

[[nodiscard]] constexpr Extent2D operator-(Extent2D a, Extent2D b) noexcept {
	return {.width = a.width - b.width, .height = a.height - b.height};
}

[[nodiscard]] constexpr Extent2D operator*(Extent2D a, uint32_t b) noexcept {
	return {.width = a.width * b, .height = a.height * b};
}

[[nodiscard]] constexpr Extent2D operator*(uint32_t a, Extent2D b) noexcept {
	return {.width = a * b.width, .height = a * b.height};
}

[[nodiscard]] constexpr Extent2D operator/(Extent2D a, uint32_t b) noexcept {
	return {.width = a.width / b, .height = a.height / b};
}

[[nodiscard]] constexpr Extent2D operator/(uint32_t a, Extent2D b) noexcept {
	return {.width = a / b.width, .height = a / b.height};
}

/**
 * Rectangular region of data.
 *
 * \tparam N number of data dimensions. Must be 2 or 3.
 */
template <size_t N>
struct Region {
	Offset<N> offset{}; ///< Offset of the region.
	Extent<N> size;     ///< Size of the region.

	/**
	 * Compare this region's offset and extents to another for equality.
	 *
	 * \param other the region to compare this one to.
	 *
	 * \return true if the regions are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Region<N>& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this region.
	 *
	 * \param point point to check.
	 *
	 * \return true if the region contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(Offset<N> point) const noexcept {
		return point.x >= offset.x && point.y >= offset.y && static_cast<uint32_t>(point.x - offset.x) < size.width && static_cast<uint32_t>(point.y - offset.y) < size.height;
	}

	/**
	 * Fit the region into the middle of a target region at the largest positive
	 * integer scale of its original size that still fits within the target.
	 *
	 * \param targetRegion target region to fit the region into.
	 *
	 * \return the integer scale that was chosen.
	 *
	 * \note If the original size cannot fit within the target, then the chosen
	 *       scale will be 1.
	 */
	uint32_t fitCenteredIntegerScaled(Region targetRegion) {
		const uint32_t scale = size.fitIntegerScaled(targetRegion.size);
		offset = targetRegion.offset + (static_cast<Offset<N>>(targetRegion.size) - size) / 2;
		return scale;
	}
};
using Region2D = Region<2>; ///< Rectangular region of 2D data.
using Region3D = Region<3>; ///< Rectangular region of 3D data.

} // namespace grem

#endif
