// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VERTEX_ATTRIBUTE_DESCRIPTION_HPP
#define GREM_GRAPHICS_VERTEX_ATTRIBUTE_DESCRIPTION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>

#include <type_traits> // std::remove_cvref_t, std::false_type, std::bool_constant
#include <utility>     // std::declval

namespace grem::graphics {

/**
 * Concept that checks if a type is a valid value type for a vertex attribute.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept vertex_attribute_value_type =   //
	same_as<T, u8norm> ||               //
	same_as<T, i8norm> ||               //
	same_as<T, uint8_t> ||              //
	same_as<T, int8_t> ||               //
	same_as<T, u8vec2norm> ||           //
	same_as<T, i8vec2norm> ||           //
	same_as<T, u8vec2> ||               //
	same_as<T, i8vec2> ||               //
	same_as<T, u8vec4norm> ||           //
	same_as<T, i8vec4norm> ||           //
	same_as<T, u8vec4> ||               //
	same_as<T, i8vec4> ||               //
	same_as<T, uA2B10G10R10vec4norm> || //
	same_as<T, iA2B10G10R10vec4norm> || //
	same_as<T, u16norm> ||              //
	same_as<T, i16norm> ||              //
	same_as<T, uint16_t> ||             //
	same_as<T, int16_t> ||              //
	same_as<T, float16_t> ||            //
	same_as<T, u16vec2norm> ||          //
	same_as<T, i16vec2norm> ||          //
	same_as<T, u16vec2> ||              //
	same_as<T, i16vec2> ||              //
	same_as<T, f16vec2> ||              //
	same_as<T, u16vec4norm> ||          //
	same_as<T, i16vec4norm> ||          //
	same_as<T, u16vec4> ||              //
	same_as<T, i16vec4> ||              //
	same_as<T, f16vec4> ||              //
	same_as<T, uint32_t> ||             //
	same_as<T, int32_t> ||              //
	same_as<T, float> ||                //
	same_as<T, u32vec2> ||              //
	same_as<T, i32vec2> ||              //
	same_as<T, vec2> ||                 //
	same_as<T, u32vec3> ||              //
	same_as<T, i32vec3> ||              //
	same_as<T, vec3> ||                 //
	same_as<T, u32vec4> ||              //
	same_as<T, i32vec4> ||              //
	same_as<T, vec4>;

/**
 * Specification of the underlying value type of a vertex attribute.
 */
enum class VertexAttributeType : uint8_t {
	U8NORM,               // C++: u8norm, GLSL: float
	I8NORM,               // C++: i8norm, GLSL: float
	U8,                   // C++: uint8_t, GLSL: uint
	I8,                   // C++: int8_t, GLSL: int
	U8VEC2NORM,           // C++: u8vec2norm, GLSL: vec2
	I8VEC2NORM,           // C++: i8vec2norm, GLSL: vec2
	U8VEC2,               // C++: u8vec2, GLSL: uvec2
	I8VEC2,               // C++: i8vec2, GLSL: ivec2
	U8VEC4NORM,           // C++: u8vec4norm, GLSL: vec4
	I8VEC4NORM,           // C++: i8vec4norm, GLSL: vec4
	U8VEC4,               // C++: u8vec4, GLSL: uvec4
	I8VEC4,               // C++: i8vec4, GLSL: ivec4
	UA2B10G10R10VEC4NORM, // C++: uA2B10G10R10vec4norm, GLSL: vec4
	IA2B10G10R10VEC4NORM, // C++: iA2B10G10R10vec4norm, GLSL: vec4
	U16NORM,              // C++: u16norm, GLSL: float
	I16NORM,              // C++: i16norm, GLSL: float
	U16,                  // C++: uint16_t, GLSL: uint
	I16,                  // C++: int16_t, GLSL: int
	F16,                  // C++: float16_t, GLSL: float
	U16VEC2NORM,          // C++: u16vec2norm, GLSL: vec2
	I16VEC2NORM,          // C++: i16vec2norm, GLSL: vec2
	U16VEC2,              // C++: u16vec2, GLSL: uvec2
	I16VEC2,              // C++: i16vec2, GLSL: ivec2
	F16VEC2,              // C++: f16vec2, GLSL: vec2
	U16VEC4NORM,          // C++: u16vec4norm, GLSL: vec4
	I16VEC4NORM,          // C++: i16vec4norm, GLSL: vec4
	U16VEC4,              // C++: u16vec4, GLSL: uvec4
	I16VEC4,              // C++: i16vec4, GLSL: ivec4
	F16VEC4,              // C++: f16vec4, GLSL: vec4
	U32,                  // C++: uint32_t, GLSL: uint
	I32,                  // C++: int32_t, GLSL: int
	F32,                  // C++: float, GLSL: float
	U32VEC2,              // C++: u32vec2, GLSL: uvec2
	I32VEC2,              // C++: i32vec2, GLSL: ivec2
	F32VEC2,              // C++: vec2, GLSL: vec2
	U32VEC3,              // C++: u32vec3, GLSL: uvec3
	I32VEC3,              // C++: i32vec3, GLSL: ivec3
	F32VEC3,              // C++: vec3, GLSL: vec3
	U32VEC4,              // C++: u32vec4, GLSL: uvec4
	I32VEC4,              // C++: i32vec4, GLSL: ivec4
	F32VEC4,              // C++: vec4, GLSL: vec4
};

namespace detail {

template <typename T>
inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE{};

// clang-format off
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u8norm> = VertexAttributeType::U8NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i8norm> = VertexAttributeType::I8NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<uint8_t> = VertexAttributeType::U8;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<int8_t> = VertexAttributeType::I8;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u8vec2norm> = VertexAttributeType::U8VEC2NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i8vec2norm> = VertexAttributeType::I8VEC2NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u8vec2> = VertexAttributeType::U8VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i8vec2> = VertexAttributeType::I8VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u8vec4norm> = VertexAttributeType::U8VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i8vec4norm> = VertexAttributeType::I8VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u8vec4> = VertexAttributeType::U8VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i8vec4> = VertexAttributeType::I8VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<uA2B10G10R10vec4norm> = VertexAttributeType::UA2B10G10R10VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<iA2B10G10R10vec4norm> = VertexAttributeType::IA2B10G10R10VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u16norm> = VertexAttributeType::U16NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i16norm> = VertexAttributeType::I16NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<uint16_t> = VertexAttributeType::U16;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<int16_t> = VertexAttributeType::I16;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<float16_t> = VertexAttributeType::F16;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u16vec2norm> = VertexAttributeType::U16VEC2NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i16vec2norm> = VertexAttributeType::I16VEC2NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u16vec2> = VertexAttributeType::U16VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i16vec2> = VertexAttributeType::I16VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<f16vec2> = VertexAttributeType::F16VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u16vec4norm> = VertexAttributeType::U16VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i16vec4norm> = VertexAttributeType::I16VEC4NORM;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u16vec4> = VertexAttributeType::U16VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i16vec4> = VertexAttributeType::I16VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<f16vec4> = VertexAttributeType::F16VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<uint32_t> = VertexAttributeType::U32;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<int32_t> = VertexAttributeType::I32;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<float> = VertexAttributeType::F32;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u32vec2> = VertexAttributeType::U32VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i32vec2> = VertexAttributeType::I32VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<vec2> = VertexAttributeType::F32VEC2;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u32vec3> = VertexAttributeType::U32VEC3;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i32vec3> = VertexAttributeType::I32VEC3;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<vec3> = VertexAttributeType::F32VEC3;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<u32vec4> = VertexAttributeType::U32VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<i32vec4> = VertexAttributeType::I32VEC4;
template <> inline constexpr VertexAttributeType VERTEX_ATTRIBUTE_TYPE<vec4> = VertexAttributeType::F32VEC4;
// clang-format on

template <typename TupleType>
struct is_vertex_attributes : std::false_type {};

template <typename... Ts>
struct is_vertex_attributes<Tuple<Ts...>> : std::bool_constant<(vertex_attribute_value_type<std::remove_cvref_t<Ts>> && ...)> {};

template <typename TupleType>
inline constexpr bool is_vertex_attributes_v = is_vertex_attributes<TupleType>::value;

} // namespace detail

/**
 * Concept that checks if a type is a valid vertex attribute struct type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept vertex_attribute_struct = //
	aggregate<T> &&               //
	standard_layout<T> &&         //
	trivially_copyable<T> &&      //
	detail::is_vertex_attributes_v<decltype(meta::getFields(std::declval<T>()))>;

/**
 * Description of a vertex attribute in a vertex atttribute struct.
 */
struct VertexAttributeDescription {
	VertexAttributeType type; ///< Vertex attribute value type.
	CStringView name;         ///< Identifier that the vertex attribute is accessed by in the shader source code.
};

namespace detail {

[[nodiscard]] constexpr size_t getVertexAttributeSizeInBytes(VertexAttributeType type) noexcept {
	switch (type) {
		case VertexAttributeType::U8NORM: return sizeof(u8norm);
		case VertexAttributeType::I8NORM: return sizeof(i8norm);
		case VertexAttributeType::U8: return sizeof(uint8_t);
		case VertexAttributeType::I8: return sizeof(int8_t);
		case VertexAttributeType::U8VEC2NORM: return sizeof(u8vec2norm);
		case VertexAttributeType::I8VEC2NORM: return sizeof(i8vec2norm);
		case VertexAttributeType::U8VEC2: return sizeof(u8vec2);
		case VertexAttributeType::I8VEC2: return sizeof(i8vec2);
		case VertexAttributeType::U8VEC4NORM: return sizeof(u8vec4norm);
		case VertexAttributeType::I8VEC4NORM: return sizeof(i8vec4norm);
		case VertexAttributeType::U8VEC4: return sizeof(u8vec4);
		case VertexAttributeType::I8VEC4: return sizeof(i8vec4);
		case VertexAttributeType::UA2B10G10R10VEC4NORM: return sizeof(uA2B10G10R10vec4norm);
		case VertexAttributeType::IA2B10G10R10VEC4NORM: return sizeof(iA2B10G10R10vec4norm);
		case VertexAttributeType::U16NORM: return sizeof(u16norm);
		case VertexAttributeType::I16NORM: return sizeof(i16norm);
		case VertexAttributeType::U16: return sizeof(uint16_t);
		case VertexAttributeType::I16: return sizeof(int16_t);
		case VertexAttributeType::F16: return sizeof(float16_t);
		case VertexAttributeType::U16VEC2NORM: return sizeof(u16vec2norm);
		case VertexAttributeType::I16VEC2NORM: return sizeof(i16vec2norm);
		case VertexAttributeType::U16VEC2: return sizeof(u16vec2);
		case VertexAttributeType::I16VEC2: return sizeof(i16vec2);
		case VertexAttributeType::F16VEC2: return sizeof(f16vec2);
		case VertexAttributeType::U16VEC4NORM: return sizeof(u16vec4norm);
		case VertexAttributeType::I16VEC4NORM: return sizeof(i16vec4norm);
		case VertexAttributeType::U16VEC4: return sizeof(u16vec4);
		case VertexAttributeType::I16VEC4: return sizeof(i16vec4);
		case VertexAttributeType::F16VEC4: return sizeof(f16vec4);
		case VertexAttributeType::U32: return sizeof(uint32_t);
		case VertexAttributeType::I32: return sizeof(int32_t);
		case VertexAttributeType::F32: return sizeof(float);
		case VertexAttributeType::U32VEC2: return sizeof(u32vec2);
		case VertexAttributeType::I32VEC2: return sizeof(i32vec2);
		case VertexAttributeType::F32VEC2: return sizeof(vec2);
		case VertexAttributeType::U32VEC3: return sizeof(u32vec3);
		case VertexAttributeType::I32VEC3: return sizeof(i32vec3);
		case VertexAttributeType::F32VEC3: return sizeof(vec3);
		case VertexAttributeType::U32VEC4: return sizeof(u32vec4);
		case VertexAttributeType::I32VEC4: return sizeof(i32vec4);
		case VertexAttributeType::F32VEC4: return sizeof(vec4);
	}
	unreachable();
}

[[nodiscard]] constexpr size_t getVertexAttributeSizeInBytes(const VertexAttributeDescription& vertexAttributeDescription) noexcept {
	return getVertexAttributeSizeInBytes(vertexAttributeDescription.type);
}

template <vertex_attribute_value_type T>
[[nodiscard]] constexpr VertexAttributeDescription getVertexAttributeDescription(meta::Type<T>, CStringView name) {
	return {
		.type = detail::VERTEX_ATTRIBUTE_TYPE<T>,
		.name = name,
	};
}

template <typename VertexAttributeStruct>
[[nodiscard]] constexpr auto getVertexAttributeDescriptions() {
	Array<VertexAttributeDescription, meta::aggregate_size_v<VertexAttributeStruct>> result{};
	meta::forEachIndexedFieldType<VertexAttributeStruct>([&](auto index, auto type) -> void { //
		result[index] = getVertexAttributeDescription(type, meta::aggregate_field_name_v<index, VertexAttributeStruct>);
	});
	return result;
}

template <typename VertexAttributeStruct>
inline constexpr auto VERTEX_ATTRIBUTE_DESCRIPTIONS = getVertexAttributeDescriptions<VertexAttributeStruct>();

} // namespace detail

} // namespace grem::graphics

#endif
