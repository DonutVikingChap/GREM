// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_CONSTANT_DESCRIPTION_HPP
#define GREM_GRAPHICS_CONSTANT_DESCRIPTION_HPP

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
 * Concept that checks if a type is a valid value type for a shader constant.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept constant_value_type = //
	same_as<T, bool32_t> ||   //
	same_as<T, int32_t> ||    //
	same_as<T, uint32_t> ||   //
	same_as<T, float>;

/**
 * Specification of the underlying value type of a shader constant.
 */
enum class ConstantType : uint8_t {
	BOOL,  ///< C++: bool32_t, GLSL: bool
	INT,   ///< C++: int32_t, GLSL: int
	UINT,  ///< C++: uint32_t, GLSL: uint
	FLOAT, ///< C++: float, GLSL: float
};

namespace detail {

template <typename T>
inline constexpr ConstantType CONSTANT_TYPE{};

// clang-format off
template <> inline constexpr ConstantType CONSTANT_TYPE<bool32_t> = ConstantType::BOOL;
template <> inline constexpr ConstantType CONSTANT_TYPE<int32_t> = ConstantType::INT;
template <> inline constexpr ConstantType CONSTANT_TYPE<uint32_t> = ConstantType::UINT;
template <> inline constexpr ConstantType CONSTANT_TYPE<float> = ConstantType::FLOAT;
// clang-format on

template <typename T>
struct is_shader_constant : std::false_type {};

template <constant_value_type T>
struct is_shader_constant<T> : std::true_type {};

template <typename TupleType>
inline constexpr bool is_shader_constant_v = is_shader_constant<TupleType>::value;

template <typename TupleType>
struct is_shader_constants : std::false_type {};

template <typename... Ts>
struct is_shader_constants<Tuple<Ts...>> : std::bool_constant<(is_shader_constant_v<std::remove_cvref_t<Ts>> && ...)> {};

template <typename TupleType>
inline constexpr bool is_shader_constants_v = is_shader_constants<TupleType>::value;

} // namespace detail

/**
 * Concept that checks if a type is a valid shader constant struct type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept shader_constant_struct = //
	aggregate<T> &&              //
	standard_layout<T> &&        //
	trivially_copyable<T> &&     //
	detail::is_shader_constants_v<decltype(meta::getFields(std::declval<T>()))>;

/**
 * Description of a constant in a shader constant struct.
 */
struct ConstantDescription {
	ConstantType type; ///< Constant value type.
	CStringView name;  ///< Identifier that the constant is accessed by in the shader source code.
};

namespace detail {

static_assert(sizeof(float) == sizeof(float32_t));
static_assert(alignof(float) == alignof(float32_t));
static_assert(sizeof(bool32_t) == sizeof(float));
static_assert(sizeof(int32_t) == sizeof(float));
static_assert(sizeof(uint32_t) == sizeof(float));
static_assert(alignof(bool32_t) == alignof(float));
static_assert(alignof(int32_t) == alignof(float));
static_assert(alignof(uint32_t) == alignof(float));

template <typename ConstantStruct>
[[nodiscard]] constexpr auto getConstantDescriptions() {
	Array<ConstantDescription, meta::aggregate_size_v<ConstantStruct>> result{};
	meta::forEachIndexedFieldType<ConstantStruct>([&]<typename T>(auto index, meta::Type<T>) -> void {
		result[index] = {
			.type = detail::CONSTANT_TYPE<T>,
			.name = meta::aggregate_field_name_v<index, ConstantStruct>,
		};
	});
	return result;
}

template <typename ConstantStruct>
inline constexpr auto CONSTANT_DESCRIPTIONS = getConstantDescriptions<ConstantStruct>();

} // namespace detail

} // namespace grem::graphics

#endif
