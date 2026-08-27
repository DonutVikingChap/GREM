// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_FIELD_DESCRIPTION_HPP
#define GREM_GRAPHICS_FIELD_DESCRIPTION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>

#include <type_traits> // std::remove_cvref_t, std::false_type, std::true_type, std::bool_constant
#include <utility>     // std::declval

namespace grem::graphics {

/**
 * Concept that checks if a type is a valid value type for a field of a
 * shader-read buffer.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept field_value_type =  //
	same_as<T, int32_t> ||  //
	same_as<T, i32vec2> ||  //
	same_as<T, i32vec3> ||  //
	same_as<T, i32vec4> ||  //
	same_as<T, uint32_t> || //
	same_as<T, u32vec2> ||  //
	same_as<T, u32vec3> ||  //
	same_as<T, u32vec4> ||  //
	same_as<T, float> ||    //
	same_as<T, vec2> ||     //
	same_as<T, vec3> ||     //
	same_as<T, vec4> ||     //
	same_as<T, mat2> ||     //
	same_as<T, mat3> ||     //
	same_as<T, mat4>;

/**
 * Specification of the underlying value type of a field of a shader-read
 * buffer.
 */
enum class FieldType : uint8_t {
	INT,   ///< C++: int32_t, GLSL: int
	IVEC2, ///< C++: i32vec2, GLSL: ivec2
	IVEC3, ///< C++: i32vec3, GLSL: ivec3
	IVEC4, ///< C++: i32vec4, GLSL: ivec4
	UINT,  ///< C++: uint32_t, GLSL: uint
	UVEC2, ///< C++: u32vec2, GLSL: uvec2
	UVEC3, ///< C++: u32vec3, GLSL: uvec3
	UVEC4, ///< C++: u32vec4, GLSL: uvec4
	FLOAT, ///< C++: float, GLSL: float
	VEC2,  ///< C++: vec2, GLSL: vec2
	VEC3,  ///< C++: vec3, GLSL: vec3
	VEC4,  ///< C++: vec4, GLSL: vec4
	MAT2,  ///< C++: mat2, GLSL: mat2
	MAT3,  ///< C++: mat3, GLSL: mat3
	MAT4,  ///< C++: mat4, GLSL: mat4
};

namespace detail {

template <typename T>
inline constexpr FieldType FIELD_TYPE{};

// clang-format off
template <> inline constexpr FieldType FIELD_TYPE<int32_t> = FieldType::INT;
template <> inline constexpr FieldType FIELD_TYPE<i32vec2> = FieldType::IVEC2;
template <> inline constexpr FieldType FIELD_TYPE<i32vec3> = FieldType::IVEC3;
template <> inline constexpr FieldType FIELD_TYPE<i32vec4> = FieldType::IVEC4;
template <> inline constexpr FieldType FIELD_TYPE<uint32_t> = FieldType::UINT;
template <> inline constexpr FieldType FIELD_TYPE<u32vec2> = FieldType::UVEC2;
template <> inline constexpr FieldType FIELD_TYPE<u32vec3> = FieldType::UVEC3;
template <> inline constexpr FieldType FIELD_TYPE<u32vec4> = FieldType::UVEC4;
template <> inline constexpr FieldType FIELD_TYPE<float> = FieldType::FLOAT;
template <> inline constexpr FieldType FIELD_TYPE<vec2> = FieldType::VEC2;
template <> inline constexpr FieldType FIELD_TYPE<vec3> = FieldType::VEC3;
template <> inline constexpr FieldType FIELD_TYPE<vec4> = FieldType::VEC4;
template <> inline constexpr FieldType FIELD_TYPE<mat2> = FieldType::MAT2;
template <> inline constexpr FieldType FIELD_TYPE<mat3> = FieldType::MAT3;
template <> inline constexpr FieldType FIELD_TYPE<mat4> = FieldType::MAT4;
// clang-format on

template <typename T>
struct is_field : std::false_type {};

template <field_value_type T>
struct is_field<T> : std::true_type {};

template <field_value_type T, size_t N>
struct is_field<Array<T, N>> : std::true_type {};

template <typename TupleType>
inline constexpr bool is_field_v = is_field<TupleType>::value;

template <typename TupleType>
struct is_fields : std::false_type {};

template <typename... Ts>
struct is_fields<Tuple<Ts...>> : std::bool_constant<(is_field_v<std::remove_cvref_t<Ts>> && ...)> {};

template <typename TupleType>
inline constexpr bool is_fields_v = is_fields<TupleType>::value;

} // namespace detail

/**
 * Concept that checks if a type is a valid field struct type for a shader-read
 * buffer.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept field_struct =       //
	aggregate<T> &&          //
	standard_layout<T> &&    //
	trivially_copyable<T> && //
	detail::is_fields_v<decltype(meta::getFields(std::declval<T>()))>;

/**
 * Description of a field in a field struct for a shader-read buffer.
 */
struct FieldDescription {
	FieldType type;           ///< Field value type.
	CStringView name;         ///< Identifier that the field or array is accessed by in the shader source code.
	size_t arrayElementCount; ///< Number of elements in the array, or 0 if not an array.
};

namespace detail {

static_assert(sizeof(float) == sizeof(float32_t));
static_assert(alignof(float) == alignof(float32_t));
static_assert(sizeof(int32_t) == sizeof(float));
static_assert(sizeof(uint32_t) == sizeof(float));
static_assert(sizeof(vec2) == sizeof(float) * 2);
static_assert(sizeof(vec3) == sizeof(float) * 3);
static_assert(sizeof(vec4) == sizeof(float) * 4);
static_assert(sizeof(mat2) == sizeof(float) * 2 * 2);
static_assert(sizeof(mat3) == sizeof(float) * 3 * 3);
static_assert(sizeof(mat4) == sizeof(float) * 4 * 4);
static_assert(alignof(int32_t) == alignof(float));
static_assert(alignof(uint32_t) == alignof(float));
static_assert(alignof(vec2) == alignof(float));
static_assert(alignof(vec3) == alignof(float));
static_assert(alignof(vec4) == alignof(float));
static_assert(alignof(mat2) == alignof(float));
static_assert(alignof(mat3) == alignof(float));
static_assert(alignof(mat4) == alignof(float));

[[nodiscard]] constexpr size_t convertFloatCountToVec4Count(size_t floatCount) noexcept {
	return (floatCount + 3) / 4;
}

[[nodiscard]] constexpr size_t getFieldSizeInFloats(FieldType type) noexcept {
	switch (type) {
		case FieldType::INT: return 1;
		case FieldType::IVEC2: return 2;
		case FieldType::IVEC3: return 3;
		case FieldType::IVEC4: return 4;
		case FieldType::UINT: return 1;
		case FieldType::UVEC2: return 2;
		case FieldType::UVEC3: return 3;
		case FieldType::UVEC4: return 4;
		case FieldType::FLOAT: return 1;
		case FieldType::VEC2: return 2;
		case FieldType::VEC3: return 3;
		case FieldType::VEC4: return 4;
		case FieldType::MAT2: return 2 * 2;
		case FieldType::MAT3: return 3 * 3;
		case FieldType::MAT4: return 4 * 4;
	}
	unreachable();
}

[[nodiscard]] constexpr size_t getFieldSizeInFloats(const FieldDescription& fieldDescription) noexcept {
	return getFieldSizeInFloats(fieldDescription.type) * max(fieldDescription.arrayElementCount, size_t{1});
}

[[nodiscard]] constexpr size_t calculateElementSizeInFloats(Span<const FieldDescription> fieldDescriptions) noexcept {
	size_t result = 0;
	for (const FieldDescription& fieldDescription : fieldDescriptions) {
		result += getFieldSizeInFloats(fieldDescription);
	}
	return result;
}

[[nodiscard]] constexpr size_t calculateElementStrideInVec4s(Span<const FieldDescription> fieldDescriptions) noexcept {
	return convertFloatCountToVec4Count(calculateElementSizeInFloats(fieldDescriptions));
}

template <field_value_type T>
[[nodiscard]] constexpr FieldDescription getFieldDescription(meta::Type<T>, CStringView name) {
	return {
		.type = detail::FIELD_TYPE<T>,
		.name = name,
		.arrayElementCount = 0,
	};
}

template <field_value_type T, size_t N>
[[nodiscard]] constexpr FieldDescription getFieldDescription(meta::Type<Array<T, N>>, CStringView name) {
	return {
		.type = detail::FIELD_TYPE<T>,
		.name = name,
		.arrayElementCount = N,
	};
}

template <typename FieldStruct>
[[nodiscard]] constexpr auto getFieldDescriptions() {
	Array<FieldDescription, meta::aggregate_size_v<FieldStruct>> result{};
	meta::forEachIndexedFieldType<FieldStruct>([&](auto index, auto type) -> void { //
		result[index] = getFieldDescription(type, meta::aggregate_field_name_v<index, FieldStruct>);
	});
	return result;
}

template <typename FieldStruct>
inline constexpr auto FIELD_DESCRIPTIONS = getFieldDescriptions<FieldStruct>();

} // namespace detail

} // namespace grem::graphics

#endif
