// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_MATH_HPP
#define GREM_CORE_MATH_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cmath>       // std::...
#include <numbers>     // std::numbers::...
#include <type_traits> // std::conditional_t, std::remove_cvref_t, std::is_constant_evaluated

namespace grem {

template <typename T>
struct Formatter;

namespace numbers {

using std::numbers::e_v;
using std::numbers::egamma_v;
using std::numbers::inv_pi_v;
using std::numbers::inv_sqrt3_v;
using std::numbers::inv_sqrtpi_v;
using std::numbers::ln10_v;
using std::numbers::ln2_v;
using std::numbers::log10e_v;
using std::numbers::log2e_v;
using std::numbers::phi_v;
using std::numbers::pi_v;
using std::numbers::sqrt2_v;
using std::numbers::sqrt3_v;

inline constexpr float E = e_v<float>;
inline constexpr float LOG2E = log2e_v<float>;
inline constexpr float LOG10E = log10e_v<float>;
inline constexpr float PI = pi_v<float>;
inline constexpr float INV_PI = inv_pi_v<float>;
inline constexpr float INV_SQRTPI = inv_sqrtpi_v<float>;
inline constexpr float LN2 = ln2_v<float>;
inline constexpr float LN10 = ln10_v<float>;
inline constexpr float SQRT2 = sqrt2_v<float>;
inline constexpr float SQRT3 = sqrt3_v<float>;
inline constexpr float INV_SQRT3 = inv_sqrt3_v<float>;
inline constexpr float EGAMMA = egamma_v<float>;
inline constexpr float PHI = phi_v<float>;

} // namespace numbers

template <size_t N, typename T>
struct vec;

template <typename T>
struct vec<1, T> {
	using Component = T;
	static constexpr size_t RANK = 1;

	T x{};

	GREM_ALWAYS_INLINE constexpr vec() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit vec(vec<1, U> v) noexcept
		: x(static_cast<T>(v.x)) {}

	GREM_ALWAYS_INLINE constexpr vec(T value) noexcept
		: x(value) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const vec&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				default: unreachable();
			}
		} else {
			return x;
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				default: unreachable();
			}
		} else {
			return x;
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 1;
	}
};

template <typename T>
struct vec<2, T> {
	using Component = T;
	static constexpr size_t RANK = 2;

	T x{};
	T y{};

	GREM_ALWAYS_INLINE constexpr vec() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit vec(vec<2, U> v) noexcept
		: x(static_cast<T>(v.x))
		, y(static_cast<T>(v.y)) {}

	GREM_ALWAYS_INLINE constexpr explicit vec(T value) noexcept
		: x(value)
		, y(value) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, T y) noexcept
		: x(x)
		, y(y) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<1, U>() const noexcept {
		return vec<1, U>{static_cast<U>(x)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const vec&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<const Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 2;
	}
};

template <typename T>
struct vec<3, T> {
	using Component = T;
	static constexpr size_t RANK = 3;

	T x{};
	T y{};
	T z{};

	GREM_ALWAYS_INLINE constexpr vec() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit vec(vec<3, U> v) noexcept
		: x(static_cast<T>(v.x))
		, y(static_cast<T>(v.y))
		, z(static_cast<T>(v.z)) {}

	GREM_ALWAYS_INLINE constexpr explicit vec(T value) noexcept
		: x(value)
		, y(value)
		, z(value) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, T y, T z) noexcept
		: x(x)
		, y(y)
		, z(z) {}

	GREM_ALWAYS_INLINE constexpr vec(vec<2, T> xy, T z) noexcept
		: x(xy.x)
		, y(xy.y)
		, z(z) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, vec<2, T> yz) noexcept
		: x(x)
		, y(yz.x)
		, z(yz.y) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<1, U>() const noexcept {
		return vec<1, U>{static_cast<U>(x)};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<2, U>() const noexcept {
		return vec<2, U>{static_cast<U>(x), static_cast<U>(y)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const vec&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<const Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 3;
	}
};

template <typename T>
struct vec<4, T> {
	using Component = T;
	static constexpr size_t RANK = 4;

	T x{};
	T y{};
	T z{};
	T w{};

	GREM_ALWAYS_INLINE constexpr vec() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit vec(vec<4, U> v) noexcept
		: x(static_cast<T>(v.x))
		, y(static_cast<T>(v.y))
		, z(static_cast<T>(v.z))
		, w(static_cast<T>(v.w)) {}

	GREM_ALWAYS_INLINE constexpr explicit vec(T value) noexcept
		: x(value)
		, y(value)
		, z(value)
		, w(value) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, T y, T z, T w) noexcept
		: x(x)
		, y(y)
		, z(z)
		, w(w) {}

	GREM_ALWAYS_INLINE constexpr vec(vec<3, T> xyz, T w) noexcept
		: x(xyz.x)
		, y(xyz.y)
		, z(xyz.z)
		, w(w) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, vec<3, T> yzw) noexcept
		: x(x)
		, y(yzw.x)
		, z(yzw.y)
		, w(yzw.z) {}

	GREM_ALWAYS_INLINE constexpr vec(vec<2, T> xy, vec<2, T> zw) noexcept
		: x(xy.x)
		, y(xy.y)
		, z(zw.x)
		, w(zw.y) {}

	GREM_ALWAYS_INLINE constexpr vec(vec<2, T> xy, T z, T w) noexcept
		: x(xy.x)
		, y(xy.y)
		, z(z)
		, w(w) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, vec<2, T> yz, T w) noexcept
		: x(x)
		, y(yz.x)
		, z(yz.y)
		, w(w) {}

	GREM_ALWAYS_INLINE constexpr vec(T x, T y, vec<2, T> zw) noexcept
		: x(x)
		, y(y)
		, z(zw.x)
		, w(zw.y) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<1, U>() const noexcept {
		return vec<1, U>{static_cast<U>(x)};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<2, U>() const noexcept {
		return vec<2, U>{static_cast<U>(x), static_cast<U>(y)};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<3, U>() const noexcept {
		return vec<3, U>{static_cast<U>(x), static_cast<U>(y), static_cast<U>(z)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const vec&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				case 3: return w;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				case 3: return w;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<const Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 4;
	}
};

using bvec1 = vec<1, bool>;
using bvec2 = vec<2, bool>;
using bvec3 = vec<3, bool>;
using bvec4 = vec<4, bool>;
using b8vec1 = vec<1, bool8_t>;
using b8vec2 = vec<2, bool8_t>;
using b8vec3 = vec<3, bool8_t>;
using b8vec4 = vec<4, bool8_t>;
using b16vec1 = vec<1, bool16_t>;
using b16vec2 = vec<2, bool16_t>;
using b16vec3 = vec<3, bool16_t>;
using b16vec4 = vec<4, bool16_t>;
using b32vec1 = vec<1, bool32_t>;
using b32vec2 = vec<2, bool32_t>;
using b32vec3 = vec<3, bool32_t>;
using b32vec4 = vec<4, bool32_t>;
using b64vec1 = vec<1, bool64_t>;
using b64vec2 = vec<2, bool64_t>;
using b64vec3 = vec<3, bool64_t>;
using b64vec4 = vec<4, bool64_t>;
using ivec1 = vec<1, int>;
using ivec2 = vec<2, int>;
using ivec3 = vec<3, int>;
using ivec4 = vec<4, int>;
using uvec1 = vec<1, unsigned>;
using uvec2 = vec<2, unsigned>;
using uvec3 = vec<3, unsigned>;
using uvec4 = vec<4, unsigned>;
using i8vec1 = vec<1, int8_t>;
using i8vec2 = vec<2, int8_t>;
using i8vec3 = vec<3, int8_t>;
using i8vec4 = vec<4, int8_t>;
using u8vec1 = vec<1, uint8_t>;
using u8vec2 = vec<2, uint8_t>;
using u8vec3 = vec<3, uint8_t>;
using u8vec4 = vec<4, uint8_t>;
using i16vec1 = vec<1, int16_t>;
using i16vec2 = vec<2, int16_t>;
using i16vec3 = vec<3, int16_t>;
using i16vec4 = vec<4, int16_t>;
using u16vec1 = vec<1, uint16_t>;
using u16vec2 = vec<2, uint16_t>;
using u16vec3 = vec<3, uint16_t>;
using u16vec4 = vec<4, uint16_t>;
using i32vec1 = vec<1, int32_t>;
using i32vec2 = vec<2, int32_t>;
using i32vec3 = vec<3, int32_t>;
using i32vec4 = vec<4, int32_t>;
using u32vec1 = vec<1, uint32_t>;
using u32vec2 = vec<2, uint32_t>;
using u32vec3 = vec<3, uint32_t>;
using u32vec4 = vec<4, uint32_t>;
using i64vec1 = vec<1, int64_t>;
using i64vec2 = vec<2, int64_t>;
using i64vec3 = vec<3, int64_t>;
using i64vec4 = vec<4, int64_t>;
using u64vec1 = vec<1, uint64_t>;
using u64vec2 = vec<2, uint64_t>;
using u64vec3 = vec<3, uint64_t>;
using u64vec4 = vec<4, uint64_t>;
using izvec1 = vec<1, ssize_t>;
using izvec2 = vec<2, ssize_t>;
using izvec3 = vec<3, ssize_t>;
using izvec4 = vec<4, ssize_t>;
using uzvec1 = vec<1, size_t>;
using uzvec2 = vec<2, size_t>;
using uzvec3 = vec<3, size_t>;
using uzvec4 = vec<4, size_t>;
using vec1 = vec<1, float>;
using vec2 = vec<2, float>;
using vec3 = vec<3, float>;
using vec4 = vec<4, float>;
using dvec1 = vec<1, double>;
using dvec2 = vec<2, double>;
using dvec3 = vec<3, double>;
using dvec4 = vec<4, double>;
using f16vec1 = vec<1, float16_t>;
using f16vec2 = vec<2, float16_t>;
using f16vec3 = vec<3, float16_t>;
using f16vec4 = vec<4, float16_t>;
using f32vec1 = vec<1, float32_t>;
using f32vec2 = vec<2, float32_t>;
using f32vec3 = vec<3, float32_t>;
using f32vec4 = vec<4, float32_t>;
using f64vec1 = vec<1, float64_t>;
using f64vec2 = vec<2, float64_t>;
using f64vec3 = vec<3, float64_t>;
using f64vec4 = vec<4, float64_t>;
using i8vec1norm = vec<1, i8norm>;
using i8vec2norm = vec<2, i8norm>;
using i8vec3norm = vec<3, i8norm>;
using i8vec4norm = vec<4, i8norm>;
using u8vec1norm = vec<1, u8norm>;
using u8vec2norm = vec<2, u8norm>;
using u8vec3norm = vec<3, u8norm>;
using u8vec4norm = vec<4, u8norm>;
using i16vec1norm = vec<1, i16norm>;
using i16vec2norm = vec<2, i16norm>;
using i16vec3norm = vec<3, i16norm>;
using i16vec4norm = vec<4, i16norm>;
using u16vec1norm = vec<1, u16norm>;
using u16vec2norm = vec<2, u16norm>;
using u16vec3norm = vec<3, u16norm>;
using u16vec4norm = vec<4, u16norm>;

struct iA2B10G10R10vec4norm {
	static constexpr size_t RANK = 4;

	uint32_t _private_value{};

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm() noexcept = default;

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(vec4 value) noexcept
		: _private_value(encode(value)) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit iA2B10G10R10vec4norm(vec<4, U> v) noexcept
		: iA2B10G10R10vec4norm(vec4{v}) {}

	GREM_ALWAYS_INLINE constexpr explicit iA2B10G10R10vec4norm(float value) noexcept
		: iA2B10G10R10vec4norm(vec4{value}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(float x, float y, float z, float w) noexcept
		: iA2B10G10R10vec4norm(vec4{x, y, z, w}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(vec3 xyz, float w) noexcept
		: iA2B10G10R10vec4norm(vec4{xyz, w}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(float x, vec3 yzw) noexcept
		: iA2B10G10R10vec4norm(vec4{x, yzw}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(vec2 xy, vec2 zw) noexcept
		: iA2B10G10R10vec4norm(vec4{xy, zw}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(vec2 xy, float z, float w) noexcept
		: iA2B10G10R10vec4norm(vec4{xy, z, w}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(float x, vec2 yz, float w) noexcept
		: iA2B10G10R10vec4norm(vec4{x, yz, w}) {}

	GREM_ALWAYS_INLINE constexpr iA2B10G10R10vec4norm(float x, float y, vec2 zw) noexcept
		: iA2B10G10R10vec4norm(vec4{x, y, zw}) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<1, U>() const noexcept {
		return vec<1, U>{vec4{*this}};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<2, U>() const noexcept {
		return vec<2, U>{vec4{*this}};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<3, U>() const noexcept {
		return vec<3, U>{vec4{*this}};
	}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit operator vec<4, U>() const noexcept requires(!same_as<U, float>) {
		return vec<4, U>{vec4{*this}};
	}

	GREM_ALWAYS_INLINE constexpr operator vec4() const noexcept {
		return decode(_private_value);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const iA2B10G10R10vec4norm&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr float operator[](size_t i) const {
		return decode(_private_value)[i];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 4;
	}

private:
	[[nodiscard]] static constexpr uint32_t encode(vec4 value) noexcept {
		float x = clamp(value.x, -1.0f, 1.0f) * 511.0f;
		float y = clamp(value.y, -1.0f, 1.0f) * 511.0f;
		float z = clamp(value.z, -1.0f, 1.0f) * 511.0f;
		float w = clamp(value.w, -1.0f, 1.0f);
		x += (value.x < 0.0f) ? -0.5f : 0.5f;
		y += (value.y < 0.0f) ? -0.5f : 0.5f;
		z += (value.z < 0.0f) ? -0.5f : 0.5f;
		w += (value.w < 0.0f) ? -0.5f : 0.5f;
		return (bit_cast<uint32_t>(static_cast<int32_t>(x)) & 0b1111111111) |         //
		       ((bit_cast<uint32_t>(static_cast<int32_t>(y)) & 0b1111111111) << 10) | //
		       ((bit_cast<uint32_t>(static_cast<int32_t>(z)) & 0b1111111111) << 20) | //
		       ((bit_cast<uint32_t>(static_cast<int32_t>(w)) & 0b11) << 30);
	}

	[[nodiscard]] static constexpr vec4 decode(uint32_t value) noexcept {
		uint32_t x = static_cast<uint32_t>(value & 0b1111111111);
		uint32_t y = static_cast<uint32_t>((value >> 10) & 0b1111111111);
		uint32_t z = static_cast<uint32_t>((value >> 20) & 0b1111111111);
		uint32_t w = static_cast<uint32_t>((value >> 30) & 0b11);
		x |= uint32_t{0} - (x & 0b1000000000);
		y |= uint32_t{0} - (y & 0b1000000000);
		z |= uint32_t{0} - (z & 0b1000000000);
		w |= uint32_t{0} - (w & 0b10);
		return {
			static_cast<float>(bit_cast<int32_t>(x)) / 511.0f,
			static_cast<float>(bit_cast<int32_t>(y)) / 511.0f,
			static_cast<float>(bit_cast<int32_t>(z)) / 511.0f,
			static_cast<float>(bit_cast<int32_t>(w)),
		};
	}
};

struct uA2B10G10R10vec4norm {
	static constexpr size_t RANK = 4;

	uint32_t _private_value{};

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm() noexcept = default;

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(vec4 value) noexcept
		: _private_value(encode(value)) {}

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit uA2B10G10R10vec4norm(vec<4, U> v) noexcept
		: uA2B10G10R10vec4norm(vec4{v}) {}

	GREM_ALWAYS_INLINE constexpr explicit uA2B10G10R10vec4norm(float value) noexcept
		: uA2B10G10R10vec4norm(vec4{value}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(float x, float y, float z, float w) noexcept
		: uA2B10G10R10vec4norm(vec4{x, y, z, w}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(vec3 xyz, float w) noexcept
		: uA2B10G10R10vec4norm(vec4{xyz, w}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(float x, vec3 yzw) noexcept
		: uA2B10G10R10vec4norm(vec4{x, yzw}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(vec2 xy, vec2 zw) noexcept
		: uA2B10G10R10vec4norm(vec4{xy, zw}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(vec2 xy, float z, float w) noexcept
		: uA2B10G10R10vec4norm(vec4{xy, z, w}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(float x, vec2 yz, float w) noexcept
		: uA2B10G10R10vec4norm(vec4{x, yz, w}) {}

	GREM_ALWAYS_INLINE constexpr uA2B10G10R10vec4norm(float x, float y, vec2 zw) noexcept
		: uA2B10G10R10vec4norm(vec4{x, y, zw}) {}

	GREM_ALWAYS_INLINE constexpr explicit operator vec1() const noexcept {
		return vec1{vec4{*this}};
	}

	GREM_ALWAYS_INLINE constexpr explicit operator vec2() const noexcept {
		return vec2{vec4{*this}};
	}

	GREM_ALWAYS_INLINE constexpr explicit operator vec3() const noexcept {
		return vec3{vec4{*this}};
	}

	GREM_ALWAYS_INLINE constexpr operator vec4() const noexcept {
		return decode(_private_value);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const uA2B10G10R10vec4norm&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr float operator[](size_t i) const {
		return decode(_private_value)[i];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 4;
	}

private:
	[[nodiscard]] static constexpr uint32_t encode(vec4 value) noexcept {
		return (static_cast<uint32_t>(clamp(value.x, 0.0f, 1.0f) * 1023.0f)) |       //
		       (static_cast<uint32_t>(clamp(value.y, 0.0f, 1.0f) * 1023.0f) << 10) | //
		       (static_cast<uint32_t>(clamp(value.z, 0.0f, 1.0f) * 1023.0f) << 20) | //
		       (static_cast<uint32_t>(clamp(value.w, 0.0f, 1.0f) * 3.0f) << 30);
	}

	[[nodiscard]] static constexpr vec4 decode(uint32_t value) noexcept {
		return {
			static_cast<float>(value & 0b1111111111) / 1023.0f,
			static_cast<float>((value >> 10) & 0b1111111111) / 1023.0f,
			static_cast<float>((value >> 20) & 0b1111111111) / 1023.0f,
			static_cast<float>((value >> 30) & 0b11) / 3.0f,
		};
	}
};

template <size_t C, size_t R, typename T>
struct mat {
	using Component = vec<R, T>;
	static constexpr size_t RANK = C;

	vec<R, T> _private_value[C]{};

	GREM_ALWAYS_INLINE constexpr mat() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit mat(const mat<C, R, U>& m) noexcept {
		for (size_t i = 0; i < C; ++i) {
			_private_value[i] = vec<R, T>{m[i]};
		}
	}

	GREM_ALWAYS_INLINE constexpr explicit mat(T value) noexcept requires(C == R) {
		for (size_t i = 0; i < C; ++i) {
			_private_value[i][i] = value;
		}
	}

	GREM_ALWAYS_INLINE constexpr mat(T ix, T iy, T jx, T jy) noexcept requires(C == 2 && R == 2)
		: _private_value{{ix, iy}, {jx, jy}} {}

	GREM_ALWAYS_INLINE constexpr mat(T ix, T iy, T iz, T jx, T jy, T jz, T kx, T ky, T kz) noexcept requires(C == 3 && R == 3)
		: _private_value{{ix, iy, iz}, {jx, jy, jz}, {kx, ky, kz}} {}

	GREM_ALWAYS_INLINE constexpr mat(T ix, T iy, T iz, T iw, T jx, T jy, T jz, T jw, T kx, T ky, T kz, T kw, T lx, T ly, T lz, T lw) noexcept requires(C == 4 && R == 4)
		: _private_value{{ix, iy, iz, iw}, {jx, jy, jz, jw}, {kx, ky, kz, kw}, {lx, ly, lz, lw}} {}

	GREM_ALWAYS_INLINE constexpr explicit mat(vec<1, T> i) noexcept requires(C == 1 && R == 1)
		: _private_value{i} {}

	GREM_ALWAYS_INLINE constexpr mat(vec<2, T> i, vec<2, T> j) noexcept requires(C == 2 && R == 2)
		: _private_value{i, j} {}

	GREM_ALWAYS_INLINE constexpr mat(vec<3, T> i, vec<3, T> j, vec<3, T> k) noexcept requires(C == 3 && R == 3)
		: _private_value{i, j, k} {}

	GREM_ALWAYS_INLINE constexpr mat(vec<4, T> i, vec<4, T> j, vec<4, T> k, vec<4, T> l) noexcept requires(C == 4 && R == 4)
		: _private_value{i, j, k, l} {}

	template <size_t N>
	GREM_ALWAYS_INLINE constexpr mat(const mat<N, N, T>& m) noexcept requires(C == R && C > N) {
		for (size_t y = 0; y < N; ++y) {
			for (size_t x = 0; x < N; ++x) {
				_private_value[y][x] = m[y][x];
			}
		}
		for (size_t i = N; i < C; ++i) {
			_private_value[i][i] = T{1};
		}
	}

	GREM_ALWAYS_INLINE constexpr explicit operator mat<1, 1, T>() const noexcept requires(C == R && C > 1) {
		return mat<1, 1, T>{
			_private_value[0][0],
		};
	}

	GREM_ALWAYS_INLINE constexpr explicit operator mat<2, 2, T>() const noexcept requires(C == R && C > 2) {
		return mat<2, 2, T>{
			_private_value[0][0],
			_private_value[0][1],
			_private_value[1][0],
			_private_value[1][1],
		};
	}

	GREM_ALWAYS_INLINE constexpr explicit operator mat<3, 3, T>() const noexcept requires(C == R && C > 3) {
		return mat<3, 3, T>{
			_private_value[0][0],
			_private_value[0][1],
			_private_value[0][2],
			_private_value[1][0],
			_private_value[1][1],
			_private_value[1][2],
			_private_value[2][0],
			_private_value[2][1],
			_private_value[2][2],
		};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const mat&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		return _private_value[i];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		return _private_value[i];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return C;
	}
};

using imat1 = mat<1, 1, int>;
using imat2 = mat<2, 2, int>;
using imat3 = mat<3, 3, int>;
using imat4 = mat<4, 4, int>;
using umat1 = mat<1, 1, unsigned>;
using umat2 = mat<2, 2, unsigned>;
using umat3 = mat<3, 3, unsigned>;
using umat4 = mat<4, 4, unsigned>;
using i8mat1 = mat<1, 1, int8_t>;
using i8mat2 = mat<2, 2, int8_t>;
using i8mat3 = mat<3, 3, int8_t>;
using i8mat4 = mat<4, 4, int8_t>;
using u8mat1 = mat<1, 1, uint8_t>;
using u8mat2 = mat<2, 2, uint8_t>;
using u8mat3 = mat<3, 3, uint8_t>;
using u8mat4 = mat<4, 4, uint8_t>;
using i16mat1 = mat<1, 1, int16_t>;
using i16mat2 = mat<2, 2, int16_t>;
using i16mat3 = mat<3, 3, int16_t>;
using i16mat4 = mat<4, 4, int16_t>;
using u16mat1 = mat<1, 1, uint16_t>;
using u16mat2 = mat<2, 2, uint16_t>;
using u16mat3 = mat<3, 3, uint16_t>;
using u16mat4 = mat<4, 4, uint16_t>;
using i32mat1 = mat<1, 1, int32_t>;
using i32mat2 = mat<2, 2, int32_t>;
using i32mat3 = mat<3, 3, int32_t>;
using i32mat4 = mat<4, 4, int32_t>;
using u32mat1 = mat<1, 1, uint32_t>;
using u32mat2 = mat<2, 2, uint32_t>;
using u32mat3 = mat<3, 3, uint32_t>;
using u32mat4 = mat<4, 4, uint32_t>;
using i64mat1 = mat<1, 1, int64_t>;
using i64mat2 = mat<2, 2, int64_t>;
using i64mat3 = mat<3, 3, int64_t>;
using i64mat4 = mat<4, 4, int64_t>;
using u64mat1 = mat<1, 1, uint64_t>;
using u64mat2 = mat<2, 2, uint64_t>;
using u64mat3 = mat<3, 3, uint64_t>;
using u64mat4 = mat<4, 4, uint64_t>;
using mat1 = mat<1, 1, float>;
using mat2 = mat<2, 2, float>;
using mat3 = mat<3, 3, float>;
using mat4 = mat<4, 4, float>;
using dmat1 = mat<1, 1, double>;
using dmat2 = mat<2, 2, double>;
using dmat3 = mat<3, 3, double>;
using dmat4 = mat<4, 4, double>;
using f16mat1 = mat<1, 1, float16_t>;
using f16mat2 = mat<2, 2, float16_t>;
using f16mat3 = mat<3, 3, float16_t>;
using f16mat4 = mat<4, 4, float16_t>;
using f32mat1 = mat<1, 1, float32_t>;
using f32mat2 = mat<2, 2, float32_t>;
using f32mat3 = mat<3, 3, float32_t>;
using f32mat4 = mat<4, 4, float32_t>;
using f64mat1 = mat<1, 1, float64_t>;
using f64mat2 = mat<2, 2, float64_t>;
using f64mat3 = mat<3, 3, float64_t>;
using f64mat4 = mat<4, 4, float64_t>;

template <typename T>
struct qua {
	using Component = T;
	static constexpr size_t RANK = 4;

	T x{};
	T y{};
	T z{};
	T w{};

	GREM_ALWAYS_INLINE constexpr qua() noexcept = default;

	template <typename U>
	GREM_ALWAYS_INLINE constexpr explicit qua(qua<U> q) noexcept
		: x(static_cast<T>(q.x))
		, y(static_cast<T>(q.y))
		, z(static_cast<T>(q.z))
		, w(static_cast<T>(q.w)) {}

	GREM_ALWAYS_INLINE constexpr qua(T x, T y, T z, T w) noexcept
		: x(x)
		, y(y)
		, z(z)
		, w(w) {}

	GREM_ALWAYS_INLINE constexpr qua(vec<3, T> xyz, T w) noexcept
		: x(xyz.x)
		, y(xyz.y)
		, z(xyz.z)
		, w(w) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const qua&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component& operator[](size_t i) {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				case 3: return w;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const Component& operator[](size_t i) const {
		GREM_ASSERT(i < RANK);
		if (std::is_constant_evaluated()) {
			switch (i) {
				case 0: return x;
				case 1: return y;
				case 2: return z;
				case 3: return w;
				default: unreachable();
			}
		} else {
			return reinterpret_cast<const Component*>(this)[i];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 4;
	}
};

using quat = qua<float>;
using dquat = qua<float>;
using f16quat = qua<float16_t>;
using f32quat = qua<float32_t>;
using f64quat = qua<float64_t>;

// Vector overloads of functions from fundamentals:

/**
 * Get the midpoint between two vectors.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return half the sum of `a` and `b`, rounded towards `a` for integer types.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> midpoint(vec<N, T> a, vec<N, T> b) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = midpoint(a[i], b[i]);
	}
	return result;
}

/**
 * Choose between two values based on a condition.
 *
 * \param condition condition to check.
 * \param ifTrue value to return if the condition is true.
 * \param ifFalse value to return if the condition is false.
 *
 * \return `ifTrue` if `condition` is true, `ifFalse` otherwise.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T select(bool condition, T ifTrue, T ifFalse) {
	// clang-format off
	using Mask =
		std::conditional_t<sizeof(T) == sizeof(uint8_t), uint8_t,
		std::conditional_t<sizeof(T) == sizeof(uint16_t), uint16_t,
		std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t,
		uint64_t>>>;
	// clang-format on
	if constexpr (sizeof(Mask) == sizeof(T)) {
		const Mask mask = static_cast<Mask>(condition) - Mask{1};
		const Mask ifTrueMask = bit_cast<Mask>(ifTrue);
		const Mask ifFalseMask = bit_cast<Mask>(ifFalse);
		return bit_cast<T>((mask & ifFalseMask) | (~mask & ifTrueMask));
	} else {
		return (condition) ? ifTrue : ifFalse;
	}
}

/**
 * Choose between values from two vectors based on a vector of conditions.
 *
 * \param conditions conditions to check.
 * \param ifTrue values to return if the corresponding condition is true.
 * \param ifFalse values to return if the corresponding condition is false.
 *
 * \return a vector where each component is the result of calling select() on
 *         the corresponding components of `conditions`, `ifTrue` and `ifFalse`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> select(vec<N, bool> conditions, vec<N, T> ifTrue, vec<N, T> ifFalse) {
	// clang-format off
	using Mask =
		std::conditional_t<sizeof(T) == sizeof(uint8_t), uint8_t,
		std::conditional_t<sizeof(T) == sizeof(uint16_t), uint16_t,
		std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t,
		uint64_t>>>;
	// clang-format on
	if constexpr (sizeof(Mask) == sizeof(T)) {
		const vec<N, Mask> mask = vec<N, Mask>{conditions} - vec<N, Mask>{Mask{1}};
		const vec<N, Mask> ifTrueMask = bit_cast<vec<N, Mask>>(ifTrue);
		const vec<N, Mask> ifFalseMask = bit_cast<vec<N, Mask>>(ifFalse);
		return bit_cast<vec<N, T>>((mask & ifFalseMask) | (~mask & ifTrueMask));
	} else {
		vec<N, T> result;
		for (size_t i = 0; i < N; ++i) {
			result[i] = select(conditions[i], ifTrue[i], ifFalse[i]);
		}
		return result;
	}
}

/**
 * Get the smaller of each component of two vectors.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return a vector where each component is the result of calling min() on the
 *         corresponding components of `a` and `b`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> min(vec<N, T> a, vec<N, T> b) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = min(a[i], b[i]);
	}
	return result;
}

/**
 * Get the greater of each component of two vectors.
 *
 * \param a first value.
 * \param b second value.
 *
 * \return a vector where each component is the result of calling max() on the
 *         corresponding components of `a` and `b`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> max(vec<N, T> a, vec<N, T> b) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = max(a[i], b[i]);
	}
	return result;
}

/**
 * Clamp each component of two vectors to specific intervals.
 *
 * \param values values to clamp.
 * \param minValues low boundaries to clamp each value to. Each component must
 *        be less than or equal to the corresponding component of maxValues.
 * \param maxValues high boundaries to clamp each value to. Each component must
 *        be greater than or equal to the corresponding component of minValues.
 *
 * \return a vector where each component is the result of calling clamp() on the
 *         corresponding component of `values`, `minValues` and `maxValues`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> clamp(vec<N, T> values, vec<N, T> minValues, vec<N, T> maxValues) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = clamp(values[i], minValues[i], maxValues[i]);
	}
	return result;
}

/**
 * Clamp each component of two vectors to a specific interval.
 *
 * \param values values to clamp.
 * \param minValue low boundary to clamp each value to. Must be less than or
 *        equal to maxValue.
 * \param maxValue high boundary to clamp each value to. Must be greater than or
 *        equal to minValue.
 *
 * \return a vector where each component is the result of calling clamp() on the
 *         corresponding component of `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> clamp(vec<N, T> values, T minValue, T maxValue) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = clamp(values[i], minValue, maxValue);
	}
	return result;
}

// Math functions:

using std::abs;
using std::acos;
using std::acosh;
using std::asin;
using std::asinh;
using std::atan;
using std::atan2;
using std::atanh;
using std::cbrt;
using std::ceil;
using std::cos;
using std::cosh;
using std::erf;
using std::erfc;
using std::exp;
using std::exp2;
using std::expm1;
using std::fdim;
using std::floor;
using std::fma;
using std::frexp;
using std::hypot;
using std::ilogb;
using std::isfinite;
using std::isinf;
using std::isnan;
using std::isnormal;
using std::isunordered;
using std::ldexp;
using std::lgamma;
using std::log;
using std::log10;
using std::log1p;
using std::log2;
using std::logb;
using std::lround;
using std::modf;
using std::nextafter;
using std::nexttoward;
using std::pow;
using std::remainder;
using std::remquo;
using std::round;
using std::signbit;
using std::sin;
using std::sinh;
using std::sqrt;
using std::tan;
using std::tanh;
using std::tgamma;
using std::trunc;

/**
 * Get the reciprocal of the square root of a value.
 *
 * \param a value to get the reciprocal square root of. Must be positive.
 *
 * \return `1 / sqrt(a)`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL inversesqrt(T a) requires(floating_point<T>) {
	GREM_ASSERT(a > T{0});
	return T{1} / sqrt(a);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL fract(T a) requires(floating_point<T>) {
	return a - floor(a);
}

/**
 * Calculate the floating-point remainder of a scalar division.
 *
 * \param a numerator.
 * \param b denominator. Must not be 0.
 *
 * \return `a - q * b`, where `q` is `a / b` with its fractional part truncated.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL fmod(T a, T b) noexcept requires(floating_point<T>) {
	return std::fmod(a, b);
}

/**
 * Get the sign multiplier of a scalar.
 *
 * \param a value to get the sign of.
 *
 * \return `-1` if `a` is negative, `0` if `a` is zero, `1` if `a` is positive.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL sign(T a) requires(floating_point<T> || strict_signed_integral<T>) {
	return static_cast<T>(T{0} < a) - static_cast<T>(a < T{0});
}

/**
 * Wrap a scalar value around a range of non-negative values.
 *
 * \param value value to wrap.
 * \param limit upper bound (exclusive) of the range to wrap around. Must be
 *        positive.
 *
 * \return `value` mod `limit`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL wrap(T value, T limit) noexcept requires(strict_arithmetic<T>) {
	GREM_ASSERT(limit > T{0});
	if constexpr (floating_point<T>) {
		return value - floor(value / limit) * limit;
	} else if constexpr (unsigned_integral<T>) {
		return static_cast<T>(value % limit);
	} else {
		return static_cast<T>((value % limit + limit) % limit);
	}
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL equal(T a, T b) {
	return a == b;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL notEqual(T a, T b) {
	return a != b;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL lessThan(T a, T b) {
	return a < b;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL lessThanEqual(T a, T b) {
	return a <= b;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL greaterThan(T a, T b) {
	return a > b;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL greaterThanEqual(T a, T b) {
	return a >= b;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL not_(bool a) {
	return !a;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, bool> GREM_VECTORCALL not_(vec<N, bool> a) {
	vec<N, bool> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = not_(a[i]);
	}
	return result;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL any(bool a) {
	return a;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL any(vec<N, bool> a) {
	bool result = false;
	for (size_t i = 0; i < N; ++i) {
		result |= a[i];
	}
	return result;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL all(bool a) {
	return a;
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool GREM_VECTORCALL all(vec<N, bool> a) {
	bool result = true;
	for (size_t i = 0; i < N; ++i) {
		result &= a[i];
	}
	return result;
}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a) requires(requires(const vec<N, T> x, const size_t i) { function(x[i]); }) \
	{ \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i]); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, vec<N, T> b) \
		requires(requires(const vec<N, T> x, const vec<N, T> y, const size_t i) { function(x[i], y[i]); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b[i]); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, T b) \
		requires(requires(const vec<N, T> x, const T y, const size_t i) { function(x[i], y); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_SCALAR_X_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(T a, vec<N, T> b) \
		requires(requires(const T x, const vec<N, T> y, const size_t i) { function(x, y[i]); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a, b[i]); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, vec<N, T> b, vec<N, T> c) \
		requires(requires(const vec<N, T> x, const vec<N, T> y, const vec<N, T> z, const size_t i) { function(x[i], y[i], z[i]); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b[i], c[i]); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_VECTOR_X_SCALAR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, vec<N, T> b, T c) \
		requires(requires(const vec<N, T> x, const vec<N, T> y, const T z, const size_t i) { function(x[i], y[i], z); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b[i], c); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_X_VECTOR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, T b, vec<N, T> c) \
		requires(requires(const vec<N, T> x, const T y, const vec<N, T> z, const size_t i) { function(x[i], y, z[i]); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b, c[i]); \
		} \
		return result; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_X_SCALAR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(ResultType, function) \
	template <size_t N, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, ResultType> GREM_VECTORCALL function(vec<N, T> a, T b, T c) \
		requires(requires(const vec<N, T> x, const T y, const T z, const size_t i) { function(x[i], y, z); }) { \
		vec<N, ResultType> result; \
		for (size_t i = 0; i < N; ++i) { \
			result[i] = function(a[i], b, c); \
		} \
		return result; \
	}

GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, abs)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, sqrt)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, inversesqrt)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, cos)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, cosh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, acos)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, acosh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, sin)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, sinh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, asin)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, asinh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, tan)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, tanh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, atan)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, atanh)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, cbrt)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, erf)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, erfc)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, lgamma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, tgamma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, pow)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, pow)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_SCALAR_X_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, pow)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, exp)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, exp2)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, expm1)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, log)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, log10)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, log1p)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, log2)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, logb)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(int, ilogb)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, round)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(long, lround)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, trunc)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, floor)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, ceil)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, fract)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION(T, sign)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fmod)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fmod)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_SCALAR_X_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fmod)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, wrap)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, wrap)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_SCALAR_X_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, wrap)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(T, fma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_VECTOR_X_SCALAR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(T, fma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_X_VECTOR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(T, fma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_X_SCALAR_OVERLOAD_OF_TERNARY_MATH_FUNCTION(T, fma)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fdim)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fdim)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_SCALAR_X_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(T, fdim)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, equal)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, notEqual)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, lessThan)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, lessThanEqual)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, greaterThan)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION(bool, greaterThanEqual)

#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_UNARY_MATH_FUNCTION
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_OVERLOAD_OF_BINARY_MATH_FUNCTION
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_VECTOR_X_SCALAR_OVERLOAD_OF_BINARY_MATH_FUNCTION

// Basic operators:

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_VECTOR_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<1, T> GREM_VECTORCALL operator op(vec<1, T> a) { \
		return {static_cast<T>(op a.x)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> GREM_VECTORCALL operator op(vec<2, T> a) { \
		return {static_cast<T>(op a.x), static_cast<T>(op a.y)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator op(vec<3, T> a) { \
		return {static_cast<T>(op a.x), static_cast<T>(op a.y), static_cast<T>(op a.z)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<4, T> GREM_VECTORCALL operator op(vec<4, T> a) { \
		return {static_cast<T>(op a.x), static_cast<T>(op a.y), static_cast<T>(op a.z), static_cast<T>(op a.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<1, T> GREM_VECTORCALL operator op(vec<1, T> a, vec<1, T> b) { \
		return {static_cast<T>(a.x op b.x)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> GREM_VECTORCALL operator op(vec<2, T> a, vec<2, T> b) { \
		return {static_cast<T>(a.x op b.x), static_cast<T>(a.y op b.y)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator op(vec<3, T> a, vec<3, T> b) { \
		return {static_cast<T>(a.x op b.x), static_cast<T>(a.y op b.y), static_cast<T>(a.z op b.z)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<4, T> GREM_VECTORCALL operator op(vec<4, T> a, vec<4, T> b) { \
		return {static_cast<T>(a.x op b.x), static_cast<T>(a.y op b.y), static_cast<T>(a.z op b.z), static_cast<T>(a.w op b.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<1, T> GREM_VECTORCALL operator op(vec<1, T> a, T b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a.x op b)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> GREM_VECTORCALL operator op(vec<2, T> a, T b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a.x op b), static_cast<T>(a.y op b)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator op(vec<3, T> a, T b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a.x op b), static_cast<T>(a.y op b), static_cast<T>(a.z op b)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<4, T> GREM_VECTORCALL operator op(vec<4, T> a, T b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a.x op b), static_cast<T>(a.y op b), static_cast<T>(a.z op b), static_cast<T>(a.w op b)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<1, T> GREM_VECTORCALL operator op(T a, vec<1, T> b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a op b.x)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> GREM_VECTORCALL operator op(T a, vec<2, T> b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a op b.x), static_cast<T>(a op b.y)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator op(T a, vec<3, T> b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a op b.x), static_cast<T>(a op b.y), static_cast<T>(a op b.z)}; \
	} \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<4, T> GREM_VECTORCALL operator op(T a, vec<4, T> b) requires(strict_arithmetic<T>) { \
		return {static_cast<T>(a op b.x), static_cast<T>(a op b.y), static_cast<T>(a op b.z), static_cast<T>(a op b.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS(op) \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<1, T>& GREM_VECTORCALL operator op(vec<1, T>& a, vec<1, T> b) { \
		a.x op b.x; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<2, T>& GREM_VECTORCALL operator op(vec<2, T>& a, vec<2, T> b) { \
		a.x op b.x; \
		a.y op b.y; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<3, T>& GREM_VECTORCALL operator op(vec<3, T>& a, vec<3, T> b) { \
		a.x op b.x; \
		a.y op b.y; \
		a.z op b.z; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<4, T>& GREM_VECTORCALL operator op(vec<4, T>& a, vec<4, T> b) { \
		a.x op b.x; \
		a.y op b.y; \
		a.z op b.z; \
		a.w op b.w; \
		return a; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS(op) \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<1, T>& GREM_VECTORCALL operator op(vec<1, T>& a, T b) requires(strict_arithmetic<T>) { \
		a.x op b; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<2, T>& GREM_VECTORCALL operator op(vec<2, T>& a, T b) requires(strict_arithmetic<T>) { \
		a.x op b; \
		a.y op b; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<3, T>& GREM_VECTORCALL operator op(vec<3, T>& a, T b) requires(strict_arithmetic<T>) { \
		a.x op b; \
		a.y op b; \
		a.z op b; \
		return a; \
	} \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr vec<4, T>& GREM_VECTORCALL operator op(vec<4, T>& a, T b) requires(strict_arithmetic<T>) { \
		a.x op b; \
		a.y op b; \
		a.z op b; \
		a.w op b; \
		return a; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_MATRIX_OPERATORS(op) \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, R, T> GREM_VECTORCALL operator op(const mat<1, R, T>& a) { \
		return {op a[0]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, R, T> GREM_VECTORCALL operator op(const mat<2, R, T>& a) { \
		return {op a[0], op a[1]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, R, T> GREM_VECTORCALL operator op(const mat<3, R, T>& a) { \
		return {op a[0], op a[1], op a[2]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, R, T> GREM_VECTORCALL operator op(const mat<4, R, T>& a) { \
		return {op a[0], op a[1], op a[2], op a[3]}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_OPERATORS(op) \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, R, T> GREM_VECTORCALL operator op(const mat<1, R, T>& a, const mat<1, R, T>& b) { \
		return {a[0] op b[0]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, R, T> GREM_VECTORCALL operator op(const mat<2, R, T>& a, const mat<2, R, T>& b) { \
		return {a[0] op b[0], a[1] op b[1]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, R, T> GREM_VECTORCALL operator op(const mat<3, R, T>& a, const mat<3, R, T>& b) { \
		return {a[0] op b[0], a[1] op b[1], a[2] op b[2]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, R, T> GREM_VECTORCALL operator op(const mat<4, R, T>& a, const mat<4, R, T>& b) { \
		return {a[0] op b[0], a[1] op b[1], a[2] op b[2], a[3] op b[3]}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS(op) \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, R, T> GREM_VECTORCALL operator op(const mat<1, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		return {a[0] op b}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, R, T> GREM_VECTORCALL operator op(const mat<2, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		return {a[0] op b, a[1] op b}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, R, T> GREM_VECTORCALL operator op(const mat<3, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		return {a[0] op b, a[1] op b, a[2] op b}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, R, T> GREM_VECTORCALL operator op(const mat<4, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		return {a[0] op b, a[1] op b, a[2] op b, a[3] op b}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS(op) \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, R, T> GREM_VECTORCALL operator op(T a, const mat<1, R, T>& b) requires(strict_arithmetic<T>) { \
		return {a op b[0]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, R, T> GREM_VECTORCALL operator op(T a, const mat<2, R, T>& b) requires(strict_arithmetic<T>) { \
		return {a op b[0], a op b[1]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, R, T> GREM_VECTORCALL operator op(T a, const mat<3, R, T>& b) requires(strict_arithmetic<T>) { \
		return {a op b[0], a op b[1], a op b[2]}; \
	} \
	template <size_t R, typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, R, T> GREM_VECTORCALL operator op(T a, const mat<4, R, T>& b) requires(strict_arithmetic<T>) { \
		return {a op b[0], a op b[1], a op b[2], a op b[3]}; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_OPERATORS(op) \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<1, R, T>& GREM_VECTORCALL operator op(mat<1, R, T>& a, const mat<1, R, T>& b) { \
		a[0] op b[0]; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<2, R, T>& GREM_VECTORCALL operator op(mat<2, R, T>& a, const mat<2, R, T>& b) { \
		a[0] op b[0]; \
		a[1] op b[1]; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<3, R, T>& GREM_VECTORCALL operator op(mat<3, R, T>& a, const mat<3, R, T>& b) { \
		a[0] op b[0]; \
		a[1] op b[1]; \
		a[2] op b[2]; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<4, R, T>& GREM_VECTORCALL operator op(mat<4, R, T>& a, const mat<4, R, T>& b) { \
		a[0] op b[0]; \
		a[1] op b[1]; \
		a[2] op b[2]; \
		a[3] op b[3]; \
		return a; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS(op) \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<1, R, T>& GREM_VECTORCALL operator op(mat<1, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		a[0] op b; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<2, R, T>& GREM_VECTORCALL operator op(mat<2, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		a[0] op b; \
		a[1] op b; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<3, R, T>& GREM_VECTORCALL operator op(mat<3, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		a[0] op b; \
		a[1] op b; \
		a[2] op b; \
		return a; \
	} \
	template <size_t R, typename T> \
	GREM_ALWAYS_INLINE constexpr mat<4, R, T>& GREM_VECTORCALL operator op(mat<4, R, T>& a, T b) requires(strict_arithmetic<T>) { \
		a[0] op b; \
		a[1] op b; \
		a[2] op b; \
		a[3] op b; \
		return a; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_QUATERNION_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL operator op(qua<T> a) { \
		return {static_cast<T>(op a.x), static_cast<T>(op a.y), static_cast<T>(op a.z), static_cast<T>(op a.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL operator op(qua<T> a, qua<T> b) { \
		return {static_cast<T>(a.x op b.x), static_cast<T>(a.y op b.y), static_cast<T>(a.z op b.z), static_cast<T>(a.w op b.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL operator op(qua<T> a, T b) requires(floating_point<T>) { \
		return {static_cast<T>(a.x op b), static_cast<T>(a.y op b), static_cast<T>(a.z op b), static_cast<T>(a.w op b)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS(op) \
	template <typename T> \
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL operator op(T a, qua<T> b) requires(floating_point<T>) { \
		return {static_cast<T>(a op b.x), static_cast<T>(a op b.y), static_cast<T>(a op b.z), static_cast<T>(a op b.w)}; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_OPERATORS(op) \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr qua<T>& GREM_VECTORCALL operator op(qua<T>& a, qua<T> b) { \
		a.x op b.x; \
		a.y op b.y; \
		a.z op b.z; \
		a.w op b.w; \
		return a; \
	}

#define GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS(op) \
	template <typename T> \
	GREM_ALWAYS_INLINE constexpr qua<T>& GREM_VECTORCALL operator op(qua<T>& a, T b) requires(floating_point<T>) { \
		a.x op b; \
		a.y op b; \
		a.z op b; \
		a.w op b; \
		return a; \
	}

GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_VECTOR_OPERATORS(~)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(|)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(&)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(^)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_VECTOR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_MATRIX_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_QUATERNION_OPERATORS(+)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_VECTOR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_MATRIX_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_QUATERNION_OPERATORS(-)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_OPERATORS(+)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS(+)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS(+)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_OPERATORS(-)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS(-)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS(-)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(*)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS(*)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS(*)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS(*)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS(*)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS(*)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS(*)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS(/)

GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS(/)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS(/)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS(/)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS(/)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS(/)
GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS(/)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS(+=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_OPERATORS(+=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_OPERATORS(+=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS(+=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS(+=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS(+=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS(-=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_OPERATORS(-=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_OPERATORS(-=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS(-=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS(-=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS(-=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS(*=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS(*=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS(*=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS(*=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS(/=)

GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS(/=)
GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS(/=)
GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS(/=)

#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_VECTOR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_VECTOR_X_SCALAR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_VECTOR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_VECTOR_X_SCALAR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_MATRIX_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_MATRIX_X_SCALAR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_MATRIX_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_MATRIX_X_SCALAR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_UNARY_QUATERNION_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_QUATERNION_X_SCALAR_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPONENT_WISE_BINARY_SCALAR_X_QUATERNION_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_OPERATORS
#undef GREM_PRIVATE_DEFINE_COMPOUND_QUATERNION_X_SCALAR_OPERATORS

// Sum of components:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL sum(vec<1, T> a) {
	return a.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL sum(vec<2, T> a) {
	return a.x + a.y;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL sum(vec<3, T> a) {
	return a.x + a.y + a.z;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL sum(vec<4, T> a) {
	return a.x + a.y + a.z + a.w;
}

// Product of components:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL product(vec<1, T> a) {
	return a.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL product(vec<2, T> a) {
	return a.x * a.y;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL product(vec<3, T> a) {
	return a.x * a.y * a.z;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL product(vec<4, T> a) {
	return a.x * a.y * a.z * a.w;
}

// Minimum component:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL minComponent(vec<1, T> a) {
	return a.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL minComponent(vec<2, T> a) {
	return min(a.x, a.y);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL minComponent(vec<3, T> a) {
	return min(min(a.x, a.y), a.z);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL minComponent(vec<4, T> a) {
	return min(min(min(a.x, a.y), a.z), a.w);
}

// Maximum component:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL maxComponent(vec<1, T> a) {
	return a.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL maxComponent(vec<2, T> a) {
	return max(a.x, a.y);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL maxComponent(vec<3, T> a) {
	return max(max(a.x, a.y), a.z);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL maxComponent(vec<4, T> a) {
	return max(max(max(a.x, a.y), a.z), a.w);
}

// Dot product:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL dot(vec<1, T> a, vec<1, T> b) {
	return a.x * b.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL dot(vec<2, T> a, vec<2, T> b) {
	return a.x * b.x + a.y * b.y;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL dot(vec<3, T> a, vec<3, T> b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL dot(vec<4, T> a, vec<4, T> b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL dot(qua<T> a, qua<T> b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// Squared magnitude:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(T a) requires(strict_arithmetic<T>) {
	return a * a;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(vec<1, T> a) requires(strict_arithmetic<T>) {
	return a.x * a.x;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(vec<2, T> a) requires(strict_arithmetic<T>) {
	return a.x * a.x + a.y * a.y;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(vec<3, T> a) requires(strict_arithmetic<T>) {
	return a.x * a.x + a.y * a.y + a.z * a.z;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(vec<4, T> a) requires(strict_arithmetic<T>) {
	return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length2(qua<T> a) requires(strict_arithmetic<T>) {
	return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
}

// Magnitude:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length(T a) requires(floating_point<T>) {
	if constexpr (unsigned_integral<T>) {
		return a;
	} else {
		return abs(a);
	}
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length(vec<N, T> a) requires(floating_point<T>) {
	if constexpr (N == 1) {
		return length(a.x);
	} else {
		return sqrt(length2(a));
	}
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL length(qua<T> a) requires(floating_point<T>) {
	return sqrt(length2(a));
}

// Squared distance:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL distance2(T a, T b) requires(strict_arithmetic<T>) {
	return length2(a - b);
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL distance2(vec<N, T> a, vec<N, T> b) requires(strict_arithmetic<T>) {
	return length2(a - b);
}

// Distance:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL distance(T a, T b) requires(floating_point<T>) {
	return length(a - b);
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL distance(vec<N, T> a, vec<N, T> b) requires(floating_point<T>) {
	return length(a - b);
}

// Normalization:

/**
 * Convert a vector to a unit vector of length 1 in the same direction.
 *
 * \param a vector to normalize. Must not be 0.
 *
 * \return a unit vector in the same direction as the given vector.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL normalize(vec<N, T> a) requires(floating_point<T>) {
	return a / length(a);
}

/**
 * Convert a quaternion to a unit quaternion of length 1.
 *
 * \param a quaternion to normalize. Must not be 0.
 *
 * \return a unit quaternion of the given quaternion.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL normalize(qua<T> a) requires(floating_point<T>) {
	return a / length(a);
}

/**
 * Try to convert a vector to a unit vector of length 1 in the same direction.
 *
 * \param a vector to normalize.
 *
 * \return a unit vector in the same direction as the given vector, or an empty
 *         optional if the given vector is very close to 0.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Optional<vec<N, T>> GREM_VECTORCALL tryNormalize(vec<N, T> a) requires(floating_point<T>) {
	const T lengthSquared = length2(a);
	if (lengthSquared > length2(Limits<T>::MACHINE_EPSILON)) {
		return a / sqrt(lengthSquared);
	}
	return {};
}

/**
 * Try to divide a floating-point value by a scalar and produce a defined
 * result.
 *
 * \param a numerator.
 * \param b denominator.
 *
 * \return the quotient of a and b, or an empty optional if the denominator is
 *         very close to 0.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Optional<T> GREM_VECTORCALL tryDivide(T a, T b) requires(floating_point<T>) {
	if (abs(b) > Limits<T>::MACHINE_EPSILON) {
		return a / b;
	}
	return {};
}

/**
 * Try to divide a floating-point vector by a scalar and produce a defined
 * result.
 *
 * \param a numerator.
 * \param b denominator.
 *
 * \return the quotient of a and b, or an empty optional if the denominator is
 *         very close to 0.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Optional<vec<N, T>> GREM_VECTORCALL tryDivide(vec<N, T> a, T b) requires(floating_point<T>) {
	if (abs(b) > Limits<T>::MACHINE_EPSILON) {
		return a / b;
	}
	return {};
}

// Vector angle:

/**
 * Get the absolute angle of a 2-dimensional vector, in radians.
 *
 * \param a vector to get the angle of.
 *
 * \return `atan2(a.y, a.x)`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL getAngle(vec<2, T> a) requires(floating_point<T>) {
	return atan2(a.y, a.x);
}

/**
 * Try to get the absolute angle of a 2-dimensional vector, in radians.
 *
 * \param a vector to get the angle of.
 *
 * \return `atan2(a.y, a.x)`, or an empty optional if the given vector is very
 *         close to 0.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Optional<T> GREM_VECTORCALL tryGetAngle(vec<2, T> a) requires(floating_point<T>) {
	if (length2(a) > length2(Limits<T>::MACHINE_EPSILON)) {
		return atan2(a.y, a.x);
	}
	return {};
}

// Vector constructors:

/**
 * Create a 2-dimensional direction unit vector from an angle.
 *
 * \param angle absolute angle of the vector to create, in radians.
 *
 * \return `(cos(angle), sin(angle))`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> GREM_VECTORCALL angledVector(T angle) requires(floating_point<T>) {
	return vec<2, T>{cos(angle), sin(angle)};
}

// Matrix determinant:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL determinant(const mat<1, 1, T>& a) {
	return a[0][0];
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL determinant(const mat<2, 2, T>& a) {
	return a[0][0] * a[1][1] - a[1][0] * a[0][1];
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL determinant(const mat<3, 3, T>& a) {
	return a[0][0] * (a[1][1] * a[2][2] - a[2][1] * a[1][2]) - //
	       a[1][0] * (a[0][1] * a[2][2] - a[2][1] * a[0][2]) + //
	       a[2][0] * (a[0][1] * a[1][2] - a[1][1] * a[0][2]);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL determinant(const mat<4, 4, T>& a) {
	const T aa0 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
	const T aa1 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
	const T aa2 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
	const T aa3 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
	const T aa4 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
	const T aa5 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
	return a[0][0] * (a[1][1] * aa0 - a[1][2] * aa1 + a[1][3] * aa2) - //
	       a[0][1] * (a[1][0] * aa0 - a[1][2] * aa3 + a[1][3] * aa4) + //
	       a[0][2] * (a[1][0] * aa1 - a[1][1] * aa3 + a[1][3] * aa5) - //
	       a[0][3] * (a[1][0] * aa2 - a[1][1] * aa4 + a[1][2] * aa5);
}

// Matrix transpose:

template <size_t C, size_t R, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<R, C, T> transpose(const mat<C, R, T>& a) {
	mat<R, C, T> result;
	for (size_t x = 0; x < R; ++x) {
		for (size_t y = 0; y < C; ++y) {
			result[x][y] = a[y][x];
		}
	}
	return result;
}

// Quaternion conjugate:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL conjugate(qua<T> a) {
	return {-a.x, -a.y, -a.z, a.w};
}

// Matrix inverse:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, 1, T> GREM_VECTORCALL inverse(const mat<1, 1, T>& a) requires(floating_point<T>) {
	return {T{1} / a[0][0]};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, 2, T> GREM_VECTORCALL inverse(const mat<2, 2, T>& a) requires(floating_point<T>) {
	const mat<2, 2, T> adjugate{
		+a[1][1],
		-a[0][1],
		-a[1][0],
		+a[0][0],
	};
	return (T{1} / determinant(a)) * adjugate;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL inverse(const mat<3, 3, T>& a) requires(floating_point<T>) {
	const mat<3, 3, T> adjugate{
		+(a[1][1] * a[2][2] - a[2][1] * a[1][2]),
		-(a[0][1] * a[2][2] - a[2][1] * a[0][2]),
		+(a[0][1] * a[1][2] - a[1][1] * a[0][2]),
		-(a[1][0] * a[2][2] - a[2][0] * a[1][2]),
		+(a[0][0] * a[2][2] - a[2][0] * a[0][2]),
		-(a[0][0] * a[1][2] - a[1][0] * a[0][2]),
		+(a[1][0] * a[2][1] - a[2][0] * a[1][1]),
		-(a[0][0] * a[2][1] - a[2][0] * a[0][1]),
		+(a[0][0] * a[1][1] - a[1][0] * a[0][1]),
	};
	return (T{1} / determinant(a)) * adjugate;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL inverse(const mat<4, 4, T>& a) requires(floating_point<T>) {
	const T aa00 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
	const T aa01 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
	const T aa02 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
	const T aa03 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
	const T aa04 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
	const T aa05 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
	const T aa06 = a[1][2] * a[3][3] - a[3][2] * a[1][3];
	const T aa07 = a[1][1] * a[3][3] - a[3][1] * a[1][3];
	const T aa08 = a[1][1] * a[3][2] - a[3][1] * a[1][2];
	const T aa09 = a[1][0] * a[3][3] - a[3][0] * a[1][3];
	const T aa10 = a[1][0] * a[3][2] - a[3][0] * a[1][2];
	const T aa11 = a[1][0] * a[3][1] - a[3][0] * a[1][1];
	const T aa12 = a[1][2] * a[2][3] - a[2][2] * a[1][3];
	const T aa13 = a[1][1] * a[2][3] - a[2][1] * a[1][3];
	const T aa14 = a[1][1] * a[2][2] - a[2][1] * a[1][2];
	const T aa15 = a[1][0] * a[2][3] - a[2][0] * a[1][3];
	const T aa16 = a[1][0] * a[2][2] - a[2][0] * a[1][2];
	const T aa17 = a[1][0] * a[2][1] - a[2][0] * a[1][1];
	const mat<4, 4, T> adjugate{
		+(a[1][1] * aa00 - a[1][2] * aa01 + a[1][3] * aa02),
		-(a[0][1] * aa00 - a[0][2] * aa01 + a[0][3] * aa02),
		+(a[0][1] * aa06 - a[0][2] * aa07 + a[0][3] * aa08),
		-(a[0][1] * aa12 - a[0][2] * aa13 + a[0][3] * aa14),
		-(a[1][0] * aa00 - a[1][2] * aa03 + a[1][3] * aa04),
		+(a[0][0] * aa00 - a[0][2] * aa03 + a[0][3] * aa04),
		-(a[0][0] * aa06 - a[0][2] * aa09 + a[0][3] * aa10),
		+(a[0][0] * aa12 - a[0][2] * aa15 + a[0][3] * aa16),
		+(a[1][0] * aa01 - a[1][1] * aa03 + a[1][3] * aa05),
		-(a[0][0] * aa01 - a[0][1] * aa03 + a[0][3] * aa05),
		+(a[0][0] * aa07 - a[0][1] * aa09 + a[0][3] * aa11),
		-(a[0][0] * aa13 - a[0][1] * aa15 + a[0][3] * aa17),
		-(a[1][0] * aa02 - a[1][1] * aa04 + a[1][2] * aa05),
		+(a[0][0] * aa02 - a[0][1] * aa04 + a[0][2] * aa05),
		-(a[0][0] * aa08 - a[0][1] * aa10 + a[0][2] * aa11),
		+(a[0][0] * aa14 - a[0][1] * aa16 + a[0][2] * aa17),
	};
	const T determinant = a[0][0] * adjugate[0][0] + a[0][1] * adjugate[1][0] + a[0][2] * adjugate[2][0] + a[0][3] * adjugate[3][0];
	return (T{1} / determinant) * adjugate;
}

// Matrix inverse transpose:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<1, 1, T> GREM_VECTORCALL inverseTranspose(const mat<1, 1, T>& a) requires(floating_point<T>) {
	return {T{1} / a[0][0]};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<2, 2, T> GREM_VECTORCALL inverseTranspose(const mat<2, 2, T>& a) requires(floating_point<T>) {
	const mat<2, 2, T> adjugate{
		+a[0][0],
		-a[0][1],
		-a[1][0],
		+a[1][1],
	};
	return (T{1} / determinant(a)) * adjugate;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL inverseTranspose(const mat<3, 3, T>& a) requires(floating_point<T>) {
	const mat<3, 3, T> adjugate{
		+(a[1][1] * a[2][2] - a[2][1] * a[1][2]),
		-(a[1][0] * a[2][2] - a[2][0] * a[1][2]),
		+(a[1][0] * a[2][1] - a[2][0] * a[1][1]),
		-(a[0][1] * a[2][2] - a[2][1] * a[0][2]),
		+(a[0][0] * a[2][2] - a[2][0] * a[0][2]),
		-(a[0][0] * a[2][1] - a[2][0] * a[0][1]),
		+(a[0][1] * a[1][2] - a[1][1] * a[0][2]),
		-(a[0][0] * a[1][2] - a[1][0] * a[0][2]),
		+(a[0][0] * a[1][1] - a[1][0] * a[0][1]),
	};
	return (T{1} / determinant(a)) * adjugate;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL inverseTranspose(const mat<4, 4, T>& a) requires(floating_point<T>) {
	const T aa00 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
	const T aa01 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
	const T aa02 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
	const T aa03 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
	const T aa04 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
	const T aa05 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
	const T aa06 = a[1][2] * a[3][3] - a[3][2] * a[1][3];
	const T aa07 = a[1][1] * a[3][3] - a[3][1] * a[1][3];
	const T aa08 = a[1][1] * a[3][2] - a[3][1] * a[1][2];
	const T aa09 = a[1][0] * a[3][3] - a[3][0] * a[1][3];
	const T aa10 = a[1][0] * a[3][2] - a[3][0] * a[1][2];
	const T aa11 = a[1][0] * a[3][1] - a[3][0] * a[1][1];
	const T aa12 = a[1][2] * a[2][3] - a[2][2] * a[1][3];
	const T aa13 = a[1][1] * a[2][3] - a[2][1] * a[1][3];
	const T aa14 = a[1][1] * a[2][2] - a[2][1] * a[1][2];
	const T aa15 = a[1][0] * a[2][3] - a[2][0] * a[1][3];
	const T aa16 = a[1][0] * a[2][2] - a[2][0] * a[1][2];
	const T aa17 = a[1][0] * a[2][1] - a[2][0] * a[1][1];
	const mat<4, 4, T> adjugate{
		+(a[1][1] * aa00 - a[1][2] * aa01 + a[1][3] * aa02),
		-(a[1][0] * aa00 - a[1][2] * aa03 + a[1][3] * aa04),
		+(a[1][0] * aa01 - a[1][1] * aa03 + a[1][3] * aa05),
		-(a[1][0] * aa02 - a[1][1] * aa04 + a[1][2] * aa05),
		-(a[0][1] * aa00 - a[0][2] * aa01 + a[0][3] * aa02),
		+(a[0][0] * aa00 - a[0][2] * aa03 + a[0][3] * aa04),
		-(a[0][0] * aa01 - a[0][1] * aa03 + a[0][3] * aa05),
		+(a[0][0] * aa02 - a[0][1] * aa04 + a[0][2] * aa05),
		+(a[0][1] * aa06 - a[0][2] * aa07 + a[0][3] * aa08),
		-(a[0][0] * aa06 - a[0][2] * aa09 + a[0][3] * aa10),
		+(a[0][0] * aa07 - a[0][1] * aa09 + a[0][3] * aa11),
		-(a[0][0] * aa08 - a[0][1] * aa10 + a[0][2] * aa11),
		-(a[0][1] * aa12 - a[0][2] * aa13 + a[0][3] * aa14),
		+(a[0][0] * aa12 - a[0][2] * aa15 + a[0][3] * aa16),
		-(a[0][0] * aa13 - a[0][1] * aa15 + a[0][3] * aa17),
		+(a[0][0] * aa14 - a[0][1] * aa16 + a[0][2] * aa17),
	};
	const T determinant = a[0][0] * adjugate[0][0] + a[0][1] * adjugate[0][1] + a[0][2] * adjugate[0][2] + a[0][3] * adjugate[0][3];
	return (T{1} / determinant) * adjugate;
}

// Quaternion inverse:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL inverse(qua<T> a) requires(floating_point<T>) {
	return conjugate(a) / length2(a);
}

// Cross product:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL cross(vec<2, T> a, vec<2, T> b) {
	return a.x * b.y - b.x * a.y;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL cross(vec<3, T> a, vec<3, T> b) {
	return {
		a.y * b.z - b.y * a.z,
		a.z * b.x - b.z * a.x,
		a.x * b.y - b.x * a.y,
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL cross(qua<T> a, qua<T> b) {
	return {
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
		a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL cross(vec<3, T> a, qua<T> b) {
	return inverse(b) * a;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL cross(qua<T> a, vec<3, T> b) {
	return a * b;
}

// Multiplication of matrices:

template <size_t N, size_t C2, size_t R1, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<C2, R1, T> GREM_VECTORCALL operator*(const mat<N, R1, T>& a, const mat<C2, N, T>& b) {
	mat<C2, R1, T> result{};
	for (size_t y = 0; y < C2; ++y) {
		for (size_t x = 0; x < R1; ++x) {
			for (size_t i = 0; i < N; ++i) {
				result[y][x] += a[i][x] * b[y][i];
			}
		}
	}
	return result;
}

// Multiplication of matrix and vector:

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL operator*(const mat<N, N, T>& a, vec<N, T> b) {
	vec<N, T> result{};
	for (size_t x = 0; x < N; ++x) {
		for (size_t i = 0; i < N; ++i) {
			result[x] += a[i][x] * b[i];
		}
	}
	return result;
}

// Multiplication of quaternions:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> GREM_VECTORCALL operator*(qua<T> a, qua<T> b) {
	return {
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
		a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
	};
}

// Multiplication of quaternion and vector:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator*(qua<T> a, vec<3, T> b) {
	const vec<3, T> xyz{a.x, a.y, a.z};
	const vec<3, T> xyzb = cross(xyz, b);
	return b + (xyzb * a.w + cross(xyz, xyzb)) * T{2};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<3, T> GREM_VECTORCALL operator*(vec<3, T> a, qua<T> b) {
	return inverse(b) * a;
}

// Division of matrices:

template <size_t N, size_t C2, size_t R1, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<C2, R1, T> GREM_VECTORCALL operator/(const mat<N, R1, T>& a, const mat<C2, N, T>& b) {
	return a * inverse(b);
}

// Division of matrix and vector:

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL operator/(const mat<N, N, T>& a, vec<N, T> b) {
	return inverse(a) * b;
}

// Quaternion to matrix:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<3, 3, T> GREM_VECTORCALL convertQuaternionTo3x3Matrix(qua<T> a) {
	const T xx = a.x * a.x;
	const T yy = a.y * a.y;
	const T zz = a.z * a.z;
	const T xz = a.x * a.z;
	const T xy = a.x * a.y;
	const T yz = a.y * a.z;
	const T wx = a.w * a.x;
	const T wy = a.w * a.y;
	const T wz = a.w * a.z;
	return {
		vec<3, T>{T{1} - T{2} * (yy + zz), T{2} * (xy + wz), T{2} * (xz - wy)},
		vec<3, T>{T{2} * (xy - wz), T{1} - T{2} * (xx + zz), T{2} * (yz + wx)},
		vec<3, T>{T{2} * (xz + wy), T{2} * (yz - wx), T{1} - T{2} * (xx + yy)},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL convertQuaternionTo4x4Matrix(qua<T> a) {
	const T xx = a.x * a.x;
	const T yy = a.y * a.y;
	const T zz = a.z * a.z;
	const T xz = a.x * a.z;
	const T xy = a.x * a.y;
	const T yz = a.y * a.z;
	const T wx = a.w * a.x;
	const T wy = a.w * a.y;
	const T wz = a.w * a.z;
	return {
		vec<4, T>{T{1} - T{2} * (yy + zz), T{2} * (xy + wz), T{2} * (xz - wy), T{0}},
		vec<4, T>{T{2} * (xy - wz), T{1} - T{2} * (xx + zz), T{2} * (yz + wx), T{0}},
		vec<4, T>{T{2} * (xz + wy), T{2} * (yz - wx), T{1} - T{2} * (xx + yy), T{0}},
		vec<4, T>{T{0}, T{0}, T{0}, T{1}},
	};
}

// Matrix to quaternion:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL convert3x3MatrixToQuaternion(const mat<3, 3, T>& a) requires(floating_point<T>) {
	const T rSquaredTimes4Minus1[4]{
		a[0][0] - a[1][1] - a[2][2],
		a[1][1] - a[0][0] - a[2][2],
		a[2][2] - a[0][0] - a[1][1],
		a[0][0] + a[1][1] + a[2][2],
	};

	size_t maxIndex = 0;
	if (rSquaredTimes4Minus1[1] > rSquaredTimes4Minus1[maxIndex]) {
		maxIndex = 1;
	}
	if (rSquaredTimes4Minus1[2] > rSquaredTimes4Minus1[maxIndex]) {
		maxIndex = 2;
	}
	if (rSquaredTimes4Minus1[3] > rSquaredTimes4Minus1[maxIndex]) {
		maxIndex = 3;
	}

	const T r = T{0.5} * sqrt(T{1} + rSquaredTimes4Minus1[maxIndex]);
	const T s = T{0.25} / r;
	switch (maxIndex) {
		case 0: return {r, (a[0][1] + a[1][0]) * s, (a[2][0] + a[0][2]) * s, (a[1][2] - a[2][1]) * s};
		case 1: return {(a[0][1] + a[1][0]) * s, r, (a[1][2] + a[2][1]) * s, (a[2][0] - a[0][2]) * s};
		case 2: return {(a[2][0] + a[0][2]) * s, (a[1][2] + a[2][1]) * s, r, (a[0][1] - a[1][0]) * s};
		case 3: return {(a[1][2] - a[2][1]) * s, (a[2][0] - a[0][2]) * s, (a[0][1] - a[1][0]) * s, r};
		default: unreachable();
	}
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL convert4x4MatrixToQuaternion(const mat<4, 4, T>& a) requires(floating_point<T>) {
	return convert3x3MatrixToQuaternion(mat<3, 3, T>{a});
}

// Matrix constructors:

/**
 * Create a matrix from its diagonal components.
 *
 * \param components vector of diagonal components of the matrix.
 *
 * \return a matrix with the given components along its diagonal, and zeros
 *         everywhere else.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<N, N, T> GREM_VECTORCALL diagonal(vec<N, T> components) {
	mat<N, N, T> result{};
	for (size_t i = 0; i < N; ++i) {
		result[i][i] = components[i];
	}
	return result;
}

/**
 * Create a translation matrix describing a 2D translation.
 *
 * \param translation translation vector.
 *
 * \return the translation matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL translate(vec<2, T> translation) {
	return {
		vec<3, T>{T{1}, T{0}, T{0}},
		vec<3, T>{T{0}, T{1}, T{0}},
		vec<3, T>{translation.x, translation.y, T{1}},
	};
}

/**
 * Create a rotation matrix describing a 2D rotation.
 *
 * \param rotation rotation angle, in radians.
 *
 * \return the rotation matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<3, 3, T> GREM_VECTORCALL rotate(T rotation) requires(floating_point<T>) {
	const T cosine = cos(rotation);
	const T sine = sin(rotation);
	return {
		vec<3, T>{cosine, sine, T{0}},
		vec<3, T>{-sine, cosine, T{0}},
		vec<3, T>{T{0}, T{0}, T{1}},
	};
}

/**
 * Create a scale matrix describing a 2D scale.
 *
 * \param scale scale vector.
 *
 * \return the scale matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL scale(vec<2, T> scale) {
	return {
		vec<3, T>{scale.x, T{0}, T{0}},
		vec<3, T>{T{0}, scale.y, T{0}},
		vec<3, T>{T{0}, T{0}, T{1}},
	};
}

/**
 * Create a shear matrix describing a 2D shear.
 *
 * \param shear shear vector.
 *
 * \return the shear matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL shear(vec<2, T> shear) {
	return {
		vec<3, T>{T{1} + shear.x * shear.y, shear.y, T{0}},
		vec<3, T>{shear.x, T{1}, T{0}},
		vec<3, T>{T{0}, T{0}, T{1}},
	};
}

/**
 * Create a combined translation, rotation and scale (TRS) matrix describing a
 * 2D transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::rotate(rotation) * grem::scale(scale)`.
 *
 * \param translation translation vector.
 * \param rotation rotation angle, in radians.
 * \param scale scale vector.
 *
 * \return the combined TRS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<3, 3, T> GREM_VECTORCALL translateRotateScale(vec<2, T> translation, T rotation, vec<2, T> scale) requires(floating_point<T>) {
	const T cosine = cos(rotation);
	const T sine = sin(rotation);
	return {
		vec<3, T>{cosine * scale.x, sine * scale.x, T{0}},
		vec<3, T>{-sine * scale.y, cosine * scale.y, T{0}},
		vec<3, T>{translation.x, translation.y, T{1}},
	};
}

/**
 * Create a combined translation and rotation matrix describing a 2D
 * transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::rotate(rotation)`.
 *
 * \param translation translation vector.
 * \param rotation rotation angle, in radians.
 *
 * \return the combined TR matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<3, 3, T> GREM_VECTORCALL translateRotate(vec<2, T> translation, T rotation) requires(floating_point<T>) {
	const T cosine = cos(rotation);
	const T sine = sin(rotation);
	return {
		vec<3, T>{cosine, sine, T{0}},
		vec<3, T>{-sine, cosine, T{0}},
		vec<3, T>{translation.x, translation.y, T{1}},
	};
}

/**
 * Create a combined translation and scale matrix describing a 2D
 * transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::scale(scale)`.
 *
 * \param translation translation vector.
 * \param scale scale vector.
 *
 * \return the combined TS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<3, 3, T> GREM_VECTORCALL translateScale(vec<2, T> translation, vec<2, T> scale) {
	return {
		vec<3, T>{scale.x, T{0}, T{0}},
		vec<3, T>{T{0}, scale.y, T{0}},
		vec<3, T>{translation.x, translation.y, T{1}},
	};
}

/**
 * Create a combined rotation and scale matrix describing a 2D transformation.
 *
 * Equivalent to `grem::rotate(rotation) * grem::scale(scale)`.
 *
 * \param rotation rotation angle, in radians.
 * \param scale scale vector.
 *
 * \return the combined RS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<3, 3, T> GREM_VECTORCALL rotateScale(T rotation, vec<2, T> scale) requires(floating_point<T>) {
	const T cosine = cos(rotation);
	const T sine = sin(rotation);
	return {
		vec<3, T>{cosine * scale.x, sine * scale.x, T{0}},
		vec<3, T>{-sine * scale.y, cosine * scale.y, T{0}},
		vec<3, T>{T{0}, T{0}, T{1}},
	};
}

/**
 * Create a translation matrix describing a 3D translation.
 *
 * \param translation translation vector.
 *
 * \return the translation matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL translate(vec<3, T> translation) {
	return {
		vec<4, T>{T{1}, T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, T{1}, T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, T{1}, T{0}},
		vec<4, T>{translation.x, translation.y, translation.z, T{1}},
	};
}

/**
 * Create a rotation matrix describing a 3D rotation.
 *
 * \param rotation rotation quaternion.
 *
 * \return the rotation matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL rotate(qua<T> rotation) requires(floating_point<T>) {
	return convertQuaternionTo4x4Matrix(rotation);
}

/**
 * Create a rotation matrix describing a 3D rotation from rotation angles.
 *
 * \param pitchYawRollAngles rotation angles (pitch, yaw, roll).
 *
 * \return the rotation matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL rotate(vec<3, T> pitchYawRollAngles) requires(floating_point<T>) {
	const T cosPitch = cos(pitchYawRollAngles.x);
	const T sinPitch = sin(pitchYawRollAngles.x);
	const T cosYaw = cos(pitchYawRollAngles.y);
	const T sinYaw = sin(pitchYawRollAngles.y);
	const T cosRoll = cos(pitchYawRollAngles.z);
	const T sinRoll = sin(pitchYawRollAngles.z);
	return {
		vec<4, T>{
			cosYaw * cosRoll + sinYaw * sinPitch * sinRoll,
			sinRoll * cosPitch,
			-sinYaw * cosRoll + cosYaw * sinPitch * sinRoll,
			T{0},
		},
		vec<4, T>{
			-cosYaw * sinRoll + sinYaw * sinPitch * cosRoll,
			cosRoll * cosPitch,
			sinRoll * sinYaw + cosYaw * sinPitch * cosRoll,
			T{0},
		},
		vec<4, T>{
			sinYaw * cosPitch,
			-sinPitch,
			cosYaw * cosPitch,
			T{0},
		},
		vec<4, T>{T{0}, T{0}, T{0}, T{1}},
	};
}

/**
 * Create a scale matrix describing a 3D scale.
 *
 * \param scale scale vector.
 *
 * \return the scale matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL scale(vec<3, T> scale) {
	return {
		vec<4, T>{scale.x, T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, scale.y, T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, scale.z, T{0}},
		vec<4, T>{T{0}, T{0}, T{0}, T{1}},
	};
}

/**
 * Create a combined translation, rotation and scale (TRS) matrix describing a
 * 3D transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::rotate(rotation) * grem::scale(scale)`.
 *
 * \param translation translation vector.
 * \param rotation rotation quaternion.
 * \param scale scale vector.
 *
 * \return the combined TRS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL translateRotateScale(vec<3, T> translation, qua<T> rotation, vec<3, T> scale) requires(floating_point<T>) {
	return {
		vec<4, T>{
			(T{1} - T{2} * (rotation.y * rotation.y + rotation.z * rotation.z)) * scale.x,
			(rotation.x * rotation.y + rotation.z * rotation.w) * scale.x * T{2},
			(rotation.x * rotation.z - rotation.y * rotation.w) * scale.x * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.y - rotation.z * rotation.w) * scale.y * T{2},
			(T{1} - T{2} * (rotation.x * rotation.x + rotation.z * rotation.z)) * scale.y,
			(rotation.y * rotation.z + rotation.x * rotation.w) * scale.y * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.z + rotation.y * rotation.w) * scale.z * T{2},
			(rotation.y * rotation.z - rotation.x * rotation.w) * scale.z * T{2},
			(T{1} - T{2} * (rotation.x * rotation.x + rotation.y * rotation.y)) * scale.z,
			T{0},
		},
		vec<4, T>{translation.x, translation.y, translation.z, T{1}},
	};
}

/**
 * Create a combined translation and rotation matrix describing a 3D
 * transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::rotate(rotation)`.
 *
 * \param translation translation vector.
 * \param rotation rotation quaternion.
 *
 * \return the combined TR matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL translateRotate(vec<3, T> translation, qua<T> rotation) requires(floating_point<T>) {
	return {
		vec<4, T>{
			T{1} - T{2} * (rotation.y * rotation.y + rotation.z * rotation.z),
			(rotation.x * rotation.y + rotation.z * rotation.w) * T{2},
			(rotation.x * rotation.z - rotation.y * rotation.w) * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.y - rotation.z * rotation.w) * T{2},
			T{1} - T{2} * (rotation.x * rotation.x + rotation.z * rotation.z),
			(rotation.y * rotation.z + rotation.x * rotation.w) * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.z + rotation.y * rotation.w) * T{2},
			(rotation.y * rotation.z - rotation.x * rotation.w) * T{2},
			T{1} - T{2} * (rotation.x * rotation.x + rotation.y * rotation.y),
			T{0},
		},
		vec<4, T>{translation.x, translation.y, translation.z, T{1}},
	};
}

/**
 * Create a combined translation and scale matrix describing a 3D
 * transformation.
 *
 * Equivalent to `grem::translate(translation) * grem::scale(scale)`.
 *
 * \param translation translation vector.
 * \param scale scale vector.
 *
 * \return the combined TS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL translateScale(vec<3, T> translation, vec<3, T> scale) {
	return {
		vec<4, T>{scale.x, T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, scale.y, T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, scale.z, T{0}},
		vec<4, T>{translation.x, translation.y, translation.z, T{1}},
	};
}

/**
 * Create a combined rotation and scale matrix describing a 3D transformation.
 *
 * Equivalent to `grem::rotate(rotation) * grem::scale(scale)`.
 *
 * \param rotation rotation quaternion.
 * \param scale scale vector.
 *
 * \return the combined RS matrix.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr mat<4, 4, T> GREM_VECTORCALL rotateScale(qua<T> rotation, vec<3, T> scale) requires(floating_point<T>) {
	return {
		vec<4, T>{
			(T{1} - T{2} * (rotation.y * rotation.y + rotation.z * rotation.z)) * scale.x,
			(rotation.x * rotation.y + rotation.z * rotation.w) * scale.x * T{2},
			(rotation.x * rotation.z - rotation.y * rotation.w) * scale.x * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.y - rotation.z * rotation.w) * scale.y * T{2},
			(T{1} - T{2} * (rotation.x * rotation.x + rotation.z * rotation.z)) * scale.y,
			(rotation.y * rotation.z + rotation.x * rotation.w) * scale.y * T{2},
			T{0},
		},
		vec<4, T>{
			(rotation.x * rotation.z + rotation.y * rotation.w) * scale.z * T{2},
			(rotation.y * rotation.z - rotation.x * rotation.w) * scale.z * T{2},
			(T{1} - T{2} * (rotation.x * rotation.x + rotation.y * rotation.y)) * scale.z,
			T{0},
		},
		vec<4, T>{T{0}, T{0}, T{0}, T{1}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL lookAt(vec<3, T> eye, vec<3, T> target, vec<3, T> up) requires(floating_point<T>) {
	const vec<3, T> viewBackward = normalize(eye - target);
	const vec<3, T> viewRight = normalize(cross(up, viewBackward));
	const vec<3, T> viewUp = cross(viewBackward, viewRight);
	const T viewLocalX = dot(eye, viewRight);
	const T viewLocalY = dot(eye, viewUp);
	const T viewLocalZ = dot(eye, viewBackward);
	return {
		vec<4, T>{viewRight.x, viewUp.x, viewBackward.x, T{0}},
		vec<4, T>{viewRight.y, viewUp.y, viewBackward.y, T{0}},
		vec<4, T>{viewRight.z, viewUp.z, viewBackward.z, T{0}},
		vec<4, T>{-viewLocalX, -viewLocalY, -viewLocalZ, T{1}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL ortho(T left, T right, T bottom, T top) requires(floating_point<T>) {
	return {
		vec<4, T>{T{2} / (right - left), T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, T{2} / (top - bottom), T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, T{-1}, T{0}},
		vec<4, T>{(left + right) / (left - right), (bottom + top) / (bottom - top), T{0}, T{1}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL ortho(T left, T right, T bottom, T top, T nearZ, T farZ) requires(floating_point<T>) {
	return {
		vec<4, T>{T{2} / (right - left), T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, T{2} / (top - bottom), T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, T{1} / (nearZ - farZ), T{0}},
		vec<4, T>{(left + right) / (left - right), (bottom + top) / (bottom - top), nearZ / (nearZ - farZ), T{1}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL frustum(T left, T right, T bottom, T top, T nearZ, T farZ) requires(floating_point<T>) {
	return {
		vec<4, T>{(T{2} * nearZ) / (right - left), T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, (T{2} * nearZ) / (top - bottom), T{0}, T{0}},
		vec<4, T>{(left + right) / (right - left), (bottom + top) / (top - bottom), farZ / (nearZ - farZ), T{-1}},
		vec<4, T>{T{0}, T{0}, (nearZ * farZ) / (nearZ - farZ), T{0}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL perspective(T verticalFieldOfView, T aspectRatio, T nearZ, T farZ) requires(floating_point<T>) {
	const T tanHalfVerticalFOV = tan(T{0.5} * verticalFieldOfView);
	return {
		vec<4, T>{T{1} / (tanHalfVerticalFOV * aspectRatio), T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, T{1} / tanHalfVerticalFOV, T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, farZ / (nearZ - farZ), T{-1}},
		vec<4, T>{T{0}, T{0}, (nearZ * farZ) / (nearZ - farZ), T{0}},
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE mat<4, 4, T> GREM_VECTORCALL infinitePerspective(T verticalFieldOfView, T aspectRatio, T nearZ) requires(floating_point<T>) {
	const T tanHalfVerticalFOV = tan(T{0.5} * verticalFieldOfView);
	return {
		vec<4, T>{T{1} / (tanHalfVerticalFOV * aspectRatio), T{0}, T{0}, T{0}},
		vec<4, T>{T{0}, T{1} / tanHalfVerticalFOV, T{0}, T{0}},
		vec<4, T>{T{0}, T{0}, T{-1}, T{-1}},
		vec<4, T>{T{0}, T{0}, -nearZ, T{0}},
	};
}

// Matrix decomposition:

/**
 * Result of the decomposeTranslationRotationScale() function.
 */
template <size_t N, typename T>
struct DecomposeTranslationRotationScaleResult;

template <typename T>
struct DecomposeTranslationRotationScaleResult<2, T> {
	vec<2, T> translation; ///< Extracted translation vector.
	T rotation;            ///< Extracted rotation angle.
	vec<2, T> scale;       ///< Extracted scale vector.
};

template <typename T>
struct DecomposeTranslationRotationScaleResult<3, T> {
	vec<3, T> translation; ///< Extracted translation vector.
	qua<T> rotation;       ///< Extracted rotation quaternion.
	vec<3, T> scale;       ///< Extracted scale vector.
};

/**
 * Decompose a combined translation, rotation and scale (TRS) matrix describing
 * a 2D transformation into its components.
 *
 * \param a transformation matrix to decompose.
 *
 * \return the decomposed components.
 *
 * \warning If the given matrix is not decomposible, the results are
 *          unspecified.
 */
template <typename T>
[[nodiscard]] inline DecomposeTranslationRotationScaleResult<2, T> decomposeTranslationRotationScale(const mat<3, 3, T>& a) requires(floating_point<T>) {
	DecomposeTranslationRotationScaleResult<2, T> result;
	vec<2, T> x{a[0]};
	vec<2, T> y{a[1]};
	result.translation = vec<2, T>{a[2]};
	result.scale = vec<2, T>{length(x), length(y)};
	if (result.scale.x >= Limits<T>::MACHINE_EPSILON) {
		result.rotation = atan2(x.y, x.x);
	} else if (result.scale.y >= Limits<T>::MACHINE_EPSILON) {
		result.rotation = atan2(y.x, -y.y);
	} else {
		result.rotation = T{0};
	}
	return result;
}

/**
 * Decompose a combined translation, rotation and scale (TRS) matrix describing
 * a 3D transformation into its components.
 *
 * \param a transformation matrix to decompose.
 *
 * \return the decomposed components.
 *
 * \warning If the given matrix is not decomposible, the results are
 *          unspecified.
 */
template <typename T>
[[nodiscard]] inline DecomposeTranslationRotationScaleResult<3, T> decomposeTranslationRotationScale(const mat<4, 4, T>& a) requires(floating_point<T>) {
	DecomposeTranslationRotationScaleResult<3, T> result;
	vec<3, T> x{a[0]};
	vec<3, T> y{a[1]};
	vec<3, T> z{a[2]};
	result.translation = vec<3, T>{a[3]};
	result.scale = vec<3, T>{length(x), length(y), length(z)};
	if (any(lessThan(abs(result.scale), vec<3, T>{Limits<T>::MACHINE_EPSILON}))) {
		result.rotation = qua<T>{T{0}, T{0}, T{0}, T{1}};
	} else {
		x /= result.scale.x;
		y /= result.scale.y;
		z /= result.scale.z;
		if (dot(x, cross(y, z)) < T{0}) {
			result.scale = -result.scale;
			x = -x;
			y = -y;
			z = -z;
		}
		if (x.x + y.y + z.z > T{0}) {
			const T s = T{2} * sqrt(T{1} + x.x + y.y + z.z);
			result.rotation.x = (y.z - z.y) / s;
			result.rotation.y = (z.x - x.z) / s;
			result.rotation.z = (x.y - y.x) / s;
			result.rotation.w = T{0.25} * s;
		} else if (x.x >= y.y && x.x >= z.z) {
			const T s = T{2} * sqrt(T{1} + x.x - y.y - z.z);
			result.rotation.x = T{0.25} * s;
			result.rotation.y = (x.y + y.x) / s;
			result.rotation.z = (x.z + z.x) / s;
			result.rotation.w = (y.z - z.y) / s;
		} else if (y.y >= z.z) {
			const T s = T{2} * sqrt(T{1} + y.y - x.x - z.z);
			result.rotation.x = (y.x + x.y) / s;
			result.rotation.y = T{0.25} * s;
			result.rotation.z = (y.z + z.y) / s;
			result.rotation.w = (z.x - x.z) / s;
		} else {
			const T s = T{2} * sqrt(T{1} + z.z - x.x - y.y);
			result.rotation.x = (z.x + x.z) / s;
			result.rotation.y = (z.y + y.z) / s;
			result.rotation.z = T{0.25} * s;
			result.rotation.w = (x.y - y.x) / s;
		}
	}
	return result;
}

// Quaternion decomposition:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL getAngle(qua<T> orientation) requires(floating_point<T>) {
	constexpr T COS_ONE_HALF = static_cast<T>(0.87758256189037271612);
	if (abs(orientation.w) <= COS_ONE_HALF) {
		return acos(orientation.w) * T{2};
	}
	const T result = asin(length(vec<3, T>{orientation.x, orientation.y, orientation.z})) * T{2};
	if (orientation.w < T{0}) {
		return T{2} * numbers::pi_v<T> - result;
	}
	return result;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, T> GREM_VECTORCALL getAxis(qua<T> orientation) requires(floating_point<T>) {
	if (const T wSquared = length2(orientation.w); wSquared < T{1}) {
		return vec<3, T>{orientation.x, orientation.y, orientation.z} * (T{1} / sqrt(T{1} - wSquared));
	}
	return vec<3, T>{T{0}, T{0}, T{1}};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL getPitch(qua<T> orientation) requires(floating_point<T>) {
	const T x = length2(orientation.w) - length2(orientation.x) - length2(orientation.y) + length2(orientation.z);
	const T y = T{2} * (orientation.y * orientation.z + orientation.x * orientation.w);
	if (abs(x) < Limits<T>::MACHINE_EPSILON && abs(y) < Limits<T>::MACHINE_EPSILON) {
		return T{2} * atan2(orientation.x, orientation.w);
	}
	return atan2(y, x);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL getYaw(qua<T> orientation) requires(floating_point<T>) {
	return asin(clamp(T{-2} * (orientation.x * orientation.z - orientation.y * orientation.w), T{-1}, T{1}));
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL getRoll(qua<T> orientation) requires(floating_point<T>) {
	const T x = length2(orientation.w) + length2(orientation.x) - length2(orientation.y) - length2(orientation.z);
	const T y = T{2} * (orientation.x * orientation.y + orientation.z * orientation.w);
	if (abs(x) < Limits<T>::MACHINE_EPSILON && abs(y) < Limits<T>::MACHINE_EPSILON) {
		return T{0};
	}
	return atan2(y, x);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, T> GREM_VECTORCALL getPitchYawRoll(qua<T> orientation) requires(floating_point<T>) {
	return {getPitch(orientation), getYaw(orientation), getRoll(orientation)};
}

// Quaternion constructors:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL rotation(vec<3, T> from, vec<3, T> to) {
	const T cosine = dot(from, to);
	if (cosine >= T{1} - Limits<T>::MACHINE_EPSILON) {
		return {T{0}, T{0}, T{0}, T{1}};
	}
	if (cosine < T{-1} + Limits<T>::MACHINE_EPSILON) {
		if (const Optional<vec<3, T>> axis = tryNormalize(cross(vec<3, T>{T{0}, T{0}, T{1}}, from))) {
			return angleAxis(numbers::pi_v<T>, *axis);
		}
		return angleAxis(numbers::pi_v<T>, normalize(cross(vec<3, T>{T{1}, T{0}, T{0}}, from)));
	}
	const T s = sqrt(T{2} * (T{1} + cosine));
	return {(T{1} / s) * cross(from, to), T{0.5} * s};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL quatLookAt(vec<3, T> direction, vec<3, T> up) requires(floating_point<T>) {
	vec<3, T> right = cross(direction, up);
	if (length2(right) < length2(Limits<T>::MACHINE_EPSILON)) {
		right = cross(direction, vec<3, T>{T{0}, T{1}, T{0}});
		if (length2(right) < length2(Limits<T>::MACHINE_EPSILON)) {
			right = cross(direction, vec<3, T>{T{1}, T{0}, T{0}});
		}
	}
	const vec<3, T> viewBackward = -direction;
	const vec<3, T> viewRight = normalize(right);
	const vec<3, T> viewUp = cross(viewRight, direction);
	return convert3x3MatrixToQuaternion(mat<3, 3, T>{viewRight, viewUp, viewBackward});
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL angleAxis(T angle, vec<3, T> axis) requires(floating_point<T>) {
	const T halfAngle = T{0.5} * angle;
	return {axis * sin(halfAngle), cos(halfAngle)};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL pitch(T angle) requires(floating_point<T>) {
	const T halfAngle = T{0.5} * angle;
	return {sin(halfAngle), 0.0f, 0.0f, cos(halfAngle)};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL yaw(T angle) requires(floating_point<T>) {
	const T halfAngle = T{0.5} * angle;
	return {0.0f, sin(halfAngle), 0.0f, cos(halfAngle)};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL roll(T angle) requires(floating_point<T>) {
	const T halfAngle = T{0.5} * angle;
	return {0.0f, 0.0f, sin(halfAngle), cos(halfAngle)};
}

// Conversions:

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL convertAnglesToQuaternion(T pitchAngle, T yawAngle, T rollAngle) requires(floating_point<T>) {
	const T halfPitch = T{0.5} * pitchAngle;
	const T halfYaw = T{0.5} * yawAngle;
	const T halfRoll = T{0.5} * rollAngle;
	const T cosHalfPitch = cos(halfPitch);
	const T cosHalfYaw = cos(halfYaw);
	const T cosHalfRoll = cos(halfRoll);
	const T sinHalfPitch = sin(halfPitch);
	const T sinHalfYaw = sin(halfYaw);
	const T sinHalfRoll = sin(halfRoll);
	return {
		sinHalfPitch * cosHalfYaw * cosHalfRoll - cosHalfPitch * sinHalfYaw * sinHalfRoll,
		cosHalfPitch * sinHalfYaw * cosHalfRoll + sinHalfPitch * cosHalfYaw * sinHalfRoll,
		cosHalfPitch * cosHalfYaw * sinHalfRoll - sinHalfPitch * sinHalfYaw * cosHalfRoll,
		cosHalfPitch * cosHalfYaw * cosHalfRoll + sinHalfPitch * sinHalfYaw * sinHalfRoll,
	};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE qua<T> GREM_VECTORCALL convertAnglesToQuaternion(vec<3, T> pitchYawRollAngles) requires(floating_point<T>) {
	return convertAnglesToQuaternion(pitchYawRollAngles.x, pitchYawRollAngles.y, pitchYawRollAngles.z);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL convertXDirectionToRollAngle(vec<2, T> x) requires(floating_point<T>) {
	GREM_ASSERT(length2(x) >= length2(0.9999f) && length2(x) <= length2(1.0001f));
	return acos(x.x);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T GREM_VECTORCALL convertYDirectionToRollAngle(vec<2, T> y) requires(floating_point<T>) {
	GREM_ASSERT(length2(y) >= length2(0.9999f) && length2(y) <= length2(1.0001f));
	return acos(y.y);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, T> GREM_VECTORCALL convertZDirectionToPitchYawAngles(vec<3, T> z) requires(floating_point<T>) {
	GREM_ASSERT(length2(z) >= length2(0.9999f) && length2(z) <= length2(1.0001f));
	return {asin(z.y), atan2(z.z, z.x)};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, T> GREM_VECTORCALL convertRollAngleToXDirection(T rollAngle) requires(floating_point<T>) {
	const T cosRoll = cos(rollAngle);
	const T sinRoll = sin(rollAngle);
	return {cosRoll, sinRoll};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, T> GREM_VECTORCALL convertRollAngleToYDirection(T rollAngle) requires(floating_point<T>) {
	const T cosRoll = cos(rollAngle);
	const T sinRoll = sin(rollAngle);
	return {-sinRoll, cosRoll};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, T> GREM_VECTORCALL convertPitchYawAnglesToZDirection(T pitchAngle, T yawAngle) requires(floating_point<T>) {
	const T cosYaw = cos(yawAngle);
	const T sinYaw = sin(yawAngle);
	const T cosPitch = cos(pitchAngle);
	const T sinPitch = sin(pitchAngle);
	return {sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch};
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, T> GREM_VECTORCALL convertAnglesToForwardDirection(T rollAngle) requires(floating_point<T>) {
	return convertRollAngleToXDirection(rollAngle);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, T> GREM_VECTORCALL convertAnglesToForwardDirection(vec<2, T> pitchYawAngles) requires(floating_point<T>) {
	return -convertPitchYawAnglesToZDirection(pitchYawAngles.x, pitchYawAngles.y);
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL convertDegreesToRadians(T angleInDegrees) requires(floating_point<T>) {
	return angleInDegrees * static_cast<T>(0.01745329251994329577);
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL convertDegreesToRadians(vec<N, T> angleInDegrees) requires(floating_point<T>) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = convertDegreesToRadians(angleInDegrees[i]);
	}
	return result;
}

template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL convertRadiansToDegrees(T angleInRadians) requires(floating_point<T>) {
	return angleInRadians * static_cast<T>(57.2957795130823208768);
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL convertRadiansToDegrees(vec<N, T> angleInRadians) requires(floating_point<T>) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = convertRadiansToDegrees(angleInRadians[i]);
	}
	return result;
}

// Classification:

/**
 * Check if each component of a vector is finite.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isfinite() on
 *         the corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<1, bool> GREM_VECTORCALL isfinite(const vec<1, T>& values) requires(floating_point<T>) {
	return vec<1, bool>{isfinite(values.x)};
}

/**
 * Check if each component of a vector is finite.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isfinite() on
 *         the corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, bool> GREM_VECTORCALL isfinite(const vec<2, T>& values) requires(floating_point<T>) {
	return vec<2, bool>{isfinite(values.x), isfinite(values.y)};
}

/**
 * Check if each component of a vector is finite.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isfinite() on
 *         the corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, bool> GREM_VECTORCALL isfinite(const vec<3, T>& values) requires(floating_point<T>) {
	return vec<3, bool>{isfinite(values.x), isfinite(values.y), isfinite(values.z)};
}

/**
 * Check if each component of a vector is finite.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isfinite() on
 *         the corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<4, bool> GREM_VECTORCALL isfinite(const vec<4, T>& values) requires(floating_point<T>) {
	return vec<4, bool>{isfinite(values.x), isfinite(values.y), isfinite(values.z), isfinite(values.w)};
}

/**
 * Check if each component of a vector is infinity.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isinf() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<1, bool> GREM_VECTORCALL isinf(const vec<1, T>& values) requires(floating_point<T>) {
	return vec<1, bool>{isinf(values.x)};
}

/**
 * Check if each component of a vector is infinity.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isinf() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, bool> GREM_VECTORCALL isinf(const vec<2, T>& values) requires(floating_point<T>) {
	return vec<2, bool>{isinf(values.x), isinf(values.y)};
}

/**
 * Check if each component of a vector is infinity.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isinf() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, bool> GREM_VECTORCALL isinf(const vec<3, T>& values) requires(floating_point<T>) {
	return vec<3, bool>{isinf(values.x), isinf(values.y), isinf(values.z)};
}

/**
 * Check if each component of a vector is infinity.
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isinf() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<4, bool> GREM_VECTORCALL isinf(const vec<4, T>& values) requires(floating_point<T>) {
	return vec<4, bool>{isinf(values.x), isinf(values.y), isinf(values.z), isinf(values.w)};
}

/**
 * Check if each component of a vector is NaN (not a number).
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isnan() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<1, bool> GREM_VECTORCALL isnan(const vec<1, T>& values) requires(floating_point<T>) {
	return vec<1, bool>{isnan(values.x)};
}

/**
 * Check if each component of a vector is NaN (not a number).
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isnan() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<2, bool> GREM_VECTORCALL isnan(const vec<2, T>& values) requires(floating_point<T>) {
	return vec<2, bool>{isnan(values.x), isnan(values.y)};
}

/**
 * Check if each component of a vector is NaN (not a number).
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isnan() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<3, bool> GREM_VECTORCALL isnan(const vec<3, T>& values) requires(floating_point<T>) {
	return vec<3, bool>{isnan(values.x), isnan(values.y), isnan(values.z)};
}

/**
 * Check if each component of a vector is NaN (not a number).
 *
 * \param values values to check.
 *
 * \return a vector where each component is the result of calling isnan() on the
 *         corresponding component of `values`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<4, bool> GREM_VECTORCALL isnan(const vec<4, T>& values) requires(floating_point<T>) {
	return vec<4, bool>{isnan(values.x), isnan(values.y), isnan(values.z), isnan(values.w)};
}

/**
 * Get the sign bits of the components of a vector.
 *
 * \param values values to get the sign bits of.
 *
 * \return a vector where each component is the result of calling signbit() on
 *         the corresponding component of `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<N, bool> GREM_VECTORCALL signbit(vec<N, T> values) requires(floating_point<T>) {
	vec<N, bool> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = signbit(values[i]);
	}
	return result;
}

/**
 * Get a value with the sign of a different value.
 *
 * \param magnitude value with the magnitude of the resulting value.
 * \param sign value with the sign of the resulting value.
 *
 * \return a value with the magnitude of `magnitude` and the sign of `sign`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE T copysign(T magnitude, T sign) requires(floating_point<T> || strict_signed_integral<T>) {
	if constexpr (same_as<T, float>) {
		return std::copysignf(magnitude, sign);
	} else if constexpr (same_as<T, double>) {
		return std::copysign(magnitude, sign);
	} else {
		magnitude = abs(magnitude);
		return (sign < 0) ? -magnitude : magnitude;
	}
}

/**
 * Get a vector with the signs of a different vector.
 *
 * \param magnitudes values with the magnitude of each component of the
 *        resulting vector.
 * \param signs values with the sign of each component of the resulting vector.
 *
 * \return a vector where each component is the result of calling copysign() on
 *         the corresponding components of `magnitudes` and `signs`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE vec<N, T> copysign(vec<N, T> magnitudes, vec<N, T> signs) requires(requires(const size_t i) { copysign(magnitudes[i], signs[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = copysign(magnitudes[i], signs[i]);
	}
	return result;
}

// Blending:

/**
 * Evaluate a step function with a given edge value.
 *
 * \param edge location of the edge of the step function.
 * \param value value at which to evaluate the step function.
 *
 * \return `0` if `value < edge`, `1` otherwise.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL step(T edge, T value) requires(floating_point<T> || strict_signed_integral<T>) {
	return mix(T{1}, T{0}, value < edge);
}

/**
 * Evaluate component-wise step functions with a given vector of edge values.
 *
 * \param edges locations of the edges of the step functions.
 * \param values values at which to evaluate the step functions.
 *
 * \return a vector where each component is the result of calling step() on the
 *         corresponding components of `edges` and `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL step(vec<N, T> edges, vec<N, T> values)
	requires(requires(const vec<N, T> x, const vec<N, T> y, const size_t i) { step(x[i], y[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = step(edges[i], values[i]);
	}
	return result;
}

/**
 * Evaluate a component-wise step function with a given edge value.
 *
 * \param edge location of the edge of the step function.
 * \param values values at which to evaluate the step function.
 *
 * \return a vector where each component is the result of calling step() on the
 *         corresponding component of `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL step(T edge, vec<N, T> values)
	requires(requires(const T x, const vec<N, T> y, const size_t i) { step(x, y[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = step(edge, values[i]);
	}
	return result;
}

/**
 * Perform Hermite interpolation between two edge values.
 *
 * \param edge0 location of the lower edge of the smoothstep function.
 * \param edge1 location of the upper edge of the smoothstep function.
 * \param value value at which to evaluate the smoothstep function.
 *
 * \return `t * t * (3 - 2 * t)`, where
 *         `t = clamp((value - edge0) / (edge1 - edge0), 0, 1)`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL smoothstep(T edge0, T edge1, T value) requires(floating_point<T>) {
	const T t = clamp((value - edge0) / (edge1 - edge0), T{0}, T{1});
	return t * t * (T{3} - T{2} * t);
}

/**
 * Perform component-wise Hermite interpolation between two vectors of edge
 * values.
 *
 * \param edges0 locations of the lower edges of the smoothstep functions.
 * \param edges1 locations of the upper edges of the smoothstep functions.
 * \param values values at which to evaluate the smoothstep functions.
 *
 * \return a vector where each component is the result of calling smoothstep()
 *         on the corresponding components of `edges0`, `edges1` and `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL smoothstep(vec<N, T> edges0, vec<N, T> edges1, vec<N, T> values)
	requires(requires(const vec<N, T> x, const vec<N, T> y, const vec<N, T> z, const size_t i) { smoothstep(x[i], y[i], z[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = smoothstep(edges0[i], edges1[i], values[i]);
	}
	return result;
}

/**
 * Perform component-wise Hermite interpolation between two edge values.
 *
 * \param edge0 location of the lower edge of the smoothstep function.
 * \param edge1 location of the upper edge of the smoothstep function.
 * \param values values at which to evaluate the smoothstep function.
 *
 * \return a vector where each component is the result of calling smoothstep()
 *         on the corresponding component of `values`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL smoothstep(T edge0, T edge1, vec<N, T> values)
	requires(requires(const T x, const T y, const vec<N, T> z, const size_t i) { smoothstep(x, y, z[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = smoothstep(edge0, edge1, values[i]);
	}
	return result;
}

/**
 * Perform linear interpolation (lerp) between two values.
 *
 * \param a value to blend from.
 * \param b value to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return `a * (1 - alpha) + b * alpha`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL lerp(T a, T b, T alpha) requires(floating_point<T>) {
	return a * (T{1} - alpha) + b * alpha;
}

/**
 * Perform linear interpolation (lerp) between two vectors.
 *
 * \param a values to blend from.
 * \param b values to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return a vector where each component is the result of calling lerp() on the
 *         corresponding components of `a`, `b` and `alpha`.
 */
template <size_t N, typename T, typename Alpha>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL lerp(vec<N, T> a, vec<N, T> b, Alpha alpha)
	requires(requires(const vec<N, T> x, const vec<N, T> y, const Alpha z, const size_t i) { lerp(x[i], y[i], z); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = lerp(a[i], b[i], alpha);
	}
	return result;
}

/**
 * Perform linear interpolation (lerp) between two quaternions.
 *
 * \param a value to blend from.
 * \param b value to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return `a * (1 - alpha) + b * alpha`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> lerp(qua<T> a, qua<T> b, T alpha) requires(floating_point<T>) {
	return a * (1 - alpha) + b * alpha;
}

/**
 * Perform spherical linear interpolation (slerp) between two quaternions.
 *
 * \param a value to blend from.
 * \param b value to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return `a` blended by `alpha` towards `b` using spherical linear
 *         interpolation.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> slerp(qua<T> a, qua<T> b, T alpha) requires(floating_point<T>) {
	T cosine = dot(a, b);
	if (cosine < T{0}) {
		cosine = -cosine;
		b = -b;
	}
	if (cosine > T{1} - Limits<T>::MACHINE_EPSILON) {
		return lerp(a, b, alpha);
	}
	const T angle = acos(cosine);
	return (a * sin((T{1} - alpha) * angle) + b * sin(alpha * angle)) / sin(angle);
}

/**
 * Blend between two values based on an alpha value.
 *
 * \param a value to blend from.
 * \param b value to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return `a * (1 - alpha) + b * alpha`.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL mix(T a, T b, T alpha) requires(floating_point<T>) {
	return lerp(a, b, alpha);
}

/**
 * Blend between values from two vectors based on an alpha value.
 *
 * \param a values to blend from.
 * \param b values to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return a vector where each component is the result of calling mix() on the
 *         corresponding components of `a`, `b` and `alpha`.
 */
template <size_t N, typename T, typename Alpha>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL mix(vec<N, T> a, vec<N, T> b, Alpha alpha)
	requires(requires(const vec<N, T> x, const vec<N, T> y, const Alpha z, const size_t i) { mix(x[i], y[i], z); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = mix(a[i], b[i], alpha);
	}
	return result;
}

/**
 * Blend between values from two vectors based on a vector of alpha values.
 *
 * \param a values to blend from.
 * \param b values to blend towards.
 * \param alphas amount to blend each value.
 *
 * \return a vector where each component is the result of calling mix() on the
 *         corresponding components of `a`, `b` and `alpha`.
 */
template <size_t N, typename T, typename Alpha>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL mix(vec<N, T> a, vec<N, T> b, vec<N, Alpha> alphas)
	requires(requires(const vec<N, T> x, const vec<N, T> y, const Alpha z, const size_t i) { mix(x[i], y[i], z[i]); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = mix(a[i], b[i], alphas[i]);
	}
	return result;
}

/**
 * Blend between two quaternions based on an alpha value.
 *
 * \param a value to blend from.
 * \param b value to blend towards.
 * \param alpha amount to blend each value.
 *
 * \return `a` blended by `alpha` towards `b` using spherical linear
 *         interpolation.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr qua<T> mix(qua<T> a, qua<T> b, T alpha) requires(floating_point<T>) {
	return slerp(a, b, alpha);
}

// Utilities:

/**
 * Get a 2D vector rotated 90 degrees counterclockwise.
 *
 * \param a vector to rotate.
 *
 * \return the rotated vector.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> rotate90DegreesCounterclockwise(vec<2, T> a) {
	return {-a.y, a.x};
}

/**
 * Get a 2D vector rotated 90 degrees clockwise.
 *
 * \param a vector to rotate.
 *
 * \return the rotated vector.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<2, T> rotate90DegreesClockwise(vec<2, T> a) {
	return {a.y, -a.x};
}

/**
 * Get a vector reflected against a plane with a given normal vector.
 *
 * \param a vector to reflect.
 * \param normal normal of the plane to reflect against.
 *
 * \return `a - 2 * dot(normal, a) * normal`.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> reflect(vec<N, T> a, vec<N, T> normal) {
	return a - T{2} * dot(normal, a) * normal;
}

/**
 * Get a value with its sign flipped if a boolean is true.
 *
 * \param value value to potentially flip.
 * \param flip whether to flip the value or not.
 *
 * \return `-value` if `flip` is true, `value` otherwise.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T GREM_VECTORCALL flipSignIf(T value, bool flip) requires(floating_point<T> || strict_signed_integral<T>) {
	if constexpr (same_as<T, float32_t> && Limits<float32_t>::IS_IEC60559) {
		return bit_cast<float32_t>((static_cast<uint32_t>(flip) << 31) ^ bit_cast<uint32_t>(value));
	} else if constexpr (same_as<T, float64_t> && Limits<float64_t>::IS_IEC60559) {
		return bit_cast<float64_t>((static_cast<uint64_t>(flip) << 63) ^ bit_cast<uint64_t>(value));
	} else {
		return (flip) ? -value : value;
	}
}

/**
 * Get a vector with its sign flipped if a boolean is true.
 *
 * \param value vector to potentially flip.
 * \param flip whether to flip the vector or not.
 *
 * \return `-value` if `flip` is true, `value` otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> GREM_VECTORCALL flipSignIf(vec<N, T> value, bool flip)
	requires(requires(const vec<N, T> x, const bool y, const size_t i) { flipSignIf(x[i], y); }) {
	vec<N, T> result;
	for (size_t i = 0; i < N; ++i) {
		result[i] = flipSignIf(value[i], flip);
	}
	return result;
}

/**
 * Get a value with the same sign as a given value, but whose magnitude has been
 * clamped to a maximum length.
 *
 * \param value value to clamp.
 * \param maxLength maximum magnitude to clamp to. Must be non-negative.
 *
 * \return a value with the same sign as the given value, and a magnitude that
 *         is approximately equal to `min(abs(value), maxLength)`.
 *
 * \note If the given value is zero, or if `maxLength` is equal to zero, a value
 *       of zero is returned.
 */
template <typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T clampLength(T value, T maxLength) requires(strict_arithmetic<T>) {
	GREM_ASSERT(maxLength >= T{0});
	if constexpr (unsigned_integral<T>) {
		return min(value, maxLength);
	} else {
		if constexpr (floating_point<T>) {
			if (abs(value) > maxLength) {
				value = copysign(maxLength, value);
			}
		} else {
			if (value < -maxLength) {
				value = -maxLength;
			} else if (value > maxLength) {
				value = maxLength;
			}
		}
		return value;
	}
}

/**
 * Get a vector with the same direction as a given vector, but whose magnitude
 * has been clamped to a maximum length.
 *
 * \param vector vector to clamp.
 * \param maxLength maximum magnitude to clamp to. Must be non-negative.
 *
 * \return a vector with the same direction as the given vector, and a magnitude
 *         that is approximately equal to `min(length(vector), maxLength)`.
 *
 * \note If the given vector is the zero vector, or if `maxLength` is equal to
 *       zero, a zero vector is returned.
 */
template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> clampLength(vec<N, T> vector, T maxLength) requires(strict_arithmetic<T>) {
	GREM_ASSERT(maxLength >= T{0});
	if (const T lengthSquared = length2(vector); lengthSquared > length2(maxLength)) {
		vector *= maxLength / sqrt(lengthSquared);
	}
	return vector;
}

/**
 * Exponentially decay a value towards a target value over a given time.
 *
 * \param value value to decay.
 * \param targetValue value to decay towards.
 * \param decayRate exponential decay constant; higher means faster. Useful
 *                  range is approximately [1, 25]. Must be non-negative.
 * \param deltaTime time step to decay over. Must be non-negative.
 *
 * \return the updated value.
 */
template <typename T, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T expDecay(T value, T targetValue, Frequency decayRate, Duration deltaTime) requires(floating_point<T>) {
	GREM_ASSERT(decayRate >= Frequency{});
	GREM_ASSERT(deltaTime >= Duration{});
	value = targetValue + (value - targetValue) * exp(-decayRate * deltaTime);
	if (abs(value - targetValue) <= Limits<T>::MACHINE_EPSILON) {
		value = targetValue;
	}
	return value;
}

/**
 * Exponentially decay a vector towards a target vector over a given time.
 *
 * \param value vector to decay.
 * \param targetValue vector to decay towards.
 * \param decayRate exponential decay constant; higher means faster. Useful
 *                  range is approximately [1, 25]. Must be non-negative.
 * \param deltaTime time step to decay over. Must be non-negative.
 *
 * \return the updated vector.
 */
template <size_t N, typename T, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> expDecay(vec<N, T> value, vec<N, T> targetValue, Frequency decayRate, Duration deltaTime) requires(floating_point<T>) {
	GREM_ASSERT(decayRate >= Frequency{});
	GREM_ASSERT(deltaTime >= Duration{});
	value = targetValue + (value - targetValue) * exp(-decayRate * deltaTime);
	if (distance2(value, targetValue) <= length2(Limits<T>::MACHINE_EPSILON)) {
		value = targetValue;
	}
	return value;
}

/**
 * Exponentially decay a value towards zero over a given time.
 *
 * \param value value to decay.
 * \param decayRate exponential decay constant; higher means faster. Useful
 *                  range is approximately [1, 25]. Must be non-negative.
 * \param deltaTime time step to decay over. Must be non-negative.
 *
 * \return the updated value.
 */
template <typename T, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr T expDecay(T value, Frequency decayRate, Duration deltaTime) requires(floating_point<T>) {
	GREM_ASSERT(decayRate >= Frequency{});
	GREM_ASSERT(deltaTime >= Duration{});
	value *= exp(-decayRate * deltaTime);
	if (abs(value) <= Limits<T>::MACHINE_EPSILON) {
		value = T{0};
	}
	return value;
}

/**
 * Exponentially decay a vector towards zero over a given time.
 *
 * \param value vector to decay.
 * \param decayRate exponential decay constant; higher means faster. Useful
 *                  range is approximately [1, 25]. Must be non-negative.
 * \param deltaTime time step to decay over. Must be non-negative.
 *
 * \return the updated vector.
 */
template <size_t N, typename T, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec<N, T> expDecay(vec<N, T> value, Frequency decayRate, Duration deltaTime) requires(floating_point<T>) {
	GREM_ASSERT(decayRate >= Frequency{});
	GREM_ASSERT(deltaTime >= Duration{});
	value *= exp(-decayRate * deltaTime);
	if (length2(value) <= length2(Limits<T>::MACHINE_EPSILON)) {
		value = {};
	}
	return value;
}

} // namespace grem

template <grem::size_t N, typename T>
struct grem::Formatter<grem::vec<N, T>> : Formatter<T> {
	StringView separator = ", ";
	StringView openingBracket = "(";
	StringView closingBracket = ")";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<T>::parseFormatSpecification(p);
		if (*p == 'n') {
			openingBracket = {};
			closingBracket = {};
			++p;
		}
		if (*p == 's') {
			separator = " ";
			++p;
		}
		return p;
	}

	void formatTo(auto& output, const vec<N, T>& value) const {
		output.append(openingBracket);
		if (N > 0) {
			Formatter<T>::formatTo(output, value[0]);
			for (size_t i = 1; i < N; ++i) {
				output.append(separator);
				Formatter<T>::formatTo(output, value[i]);
			}
		}
		output.append(closingBracket);
	}
};

template <typename V>
requires(grem::same_as<V, grem::iA2B10G10R10vec4norm> || grem::same_as<V, grem::uA2B10G10R10vec4norm>)
struct grem::Formatter<V> : Formatter<std::remove_cvref_t<decltype(V{}[0])>> {
	StringView separator = ", ";
	StringView openingBracket = "(";
	StringView closingBracket = ")";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<std::remove_cvref_t<decltype(V{}[0])>>::parseFormatSpecification(p);
		if (*p == 'n') {
			openingBracket = {};
			closingBracket = {};
			++p;
		}
		if (*p == 's') {
			separator = " ";
			++p;
		}
		return p;
	}

	void formatTo(auto& output, const V& value) const {
		output.append(openingBracket);
		Formatter<std::remove_cvref_t<decltype(V{}[0])>>::formatTo(output, value[0]);
		output.append(separator);
		Formatter<std::remove_cvref_t<decltype(V{}[0])>>::formatTo(output, value[1]);
		output.append(separator);
		Formatter<std::remove_cvref_t<decltype(V{}[0])>>::formatTo(output, value[2]);
		output.append(separator);
		Formatter<std::remove_cvref_t<decltype(V{}[0])>>::formatTo(output, value[3]);
		output.append(closingBracket);
	}
};

template <grem::size_t C, grem::size_t R, typename T>
struct grem::Formatter<grem::mat<C, R, T>> : Formatter<T> {
	StringView separator = ", ";
	StringView openingBracket = "[";
	StringView closingBracket = "]";
	StringView innerSeparator = ", ";
	StringView innerOpeningBracket = "(";
	StringView innerClosingBracket = ")";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<T>::parseFormatSpecification(p);
		if (*p == 'n') {
			openingBracket = {};
			closingBracket = {};
			innerSeparator = " ";
			innerOpeningBracket = {};
			innerClosingBracket = {};
			++p;
		}
		if (*p == 's') {
			innerSeparator = " ";
			++p;
		}
		return p;
	}

	void formatTo(auto& output, const mat<C, R, T>& value) const {
		output.append(openingBracket);
		if (C > 0) {
			output.append(innerOpeningBracket);
			if (R > 0) {
				Formatter<T>::formatTo(output, value[0][0]);
				for (size_t i = 1; i < R; ++i) {
					output.append(innerSeparator);
					Formatter<T>::formatTo(output, value[0][i]);
				}
			}
			output.append(innerClosingBracket);
			for (size_t i = 1; i < C; ++i) {
				output.append(separator);
				output.append(innerOpeningBracket);
				if (R > 0) {
					Formatter<T>::formatTo(output, value[i][0]);
					for (size_t j = 1; j < R; ++j) {
						output.append(innerSeparator);
						Formatter<T>::formatTo(output, value[i][j]);
					}
				}
				output.append(innerClosingBracket);
			}
		}
		output.append(closingBracket);
	}
};

template <typename T>
struct grem::Formatter<grem::qua<T>> {
	Formatter<T> underlying{};

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == ':') {
			++p;
			p = underlying.parseFormatSpecification(p);
		}
		return p;
	}

	void formatTo(auto& output, const qua<T>& value) const {
		underlying.formatTo(output, value.x);
		output.append("i + ");
		underlying.formatTo(output, value.y);
		output.append("j + ");
		underlying.formatTo(output, value.z);
		output.append("k + ");
		underlying.formatTo(output, value.w);
	}
};

#endif
