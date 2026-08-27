// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_PARAMETER_DESCRIPTION_HPP
#define GREM_GRAPHICS_PARAMETER_DESCRIPTION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>

#include <type_traits> // std::remove_cvref_t, std::false_type, std::true_type, std::bool_constant
#include <utility>     // std::move, std::declval

namespace grem::graphics {

namespace detail {

template <TextureType Type, bool IsShadow>
class SamplerBase {
public:
	/**
	 * Construct a null-initialized sampler reference.
	 */
	SamplerBase() noexcept = default;

	/**
	 * Construct a sampler reference to a given texture.
	 *
	 * \param texture texture to reference.
	 *
	 * \throws graphics::Error if the given texture is not a sampled texture of
	 *         the correct type.
	 */
	SamplerBase(const Texture& texture) {
		if (texture) {
			if (texture.getType() != Type) {
				throw graphics::Error{"Invalid texture type provided to sampler."};
			}
			const Optional<TextureSamplerOptions> samplerOptions = texture.getSamplerOptions();
			if (!samplerOptions) {
				throw graphics::Error{"Unsampled texture provided to sampler."};
			}
			if constexpr (IsShadow) {
				if (!samplerOptions->depthComparisonMode) {
					throw graphics::Error{"Texture without depth comparison mode provided to shadow sampler."};
				}
			} else {
				if (samplerOptions->depthComparisonMode) {
					throw graphics::Error{"Texture with depth comparison mode provided to non-shadow sampler."};
				}
			}
		}
		implementation = texture.lock();
	}

	/**
	 * Construct a sampler reference to a given texture.
	 *
	 * \param handle shared handle to the texture to reference. Must be either
	 *        null or a valid sampled texture of the correct type.
	 */
	SamplerBase(SharedPointer<TextureImplementation> handle) noexcept
		: implementation(std::move(handle)) {}

	/**
	 * Check if the sampler reference is null.
	 *
	 * \return true if the sampler reference is null, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return static_cast<bool>(implementation);
	}

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<TextureImplementation> lock() const {
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] TextureImplementation* get() const noexcept {
		return implementation.get();
	}

private:
	SharedPointer<TextureImplementation> implementation{};
};

} // namespace detail

/**
 * Reference to a sampler2D Texture that may be used as a parameter value type
 * in a shader-read buffer.
 */
struct sampler2D : detail::SamplerBase<TextureType::TEXTURE_2D, false> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a sampler2DShadow Texture that may be used as a parameter value
 * type in a shader-read buffer.
 */
struct sampler2DShadow : detail::SamplerBase<TextureType::TEXTURE_2D, true> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a sampler2DArray Texture that may be used as a parameter value
 * type in a shader-read buffer.
 */
struct sampler2DArray : detail::SamplerBase<TextureType::TEXTURE_2D_ARRAY, false> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a sampler2DArrayShadow Texture that may be used as a parameter
 * value type in a shader-read buffer.
 */
struct sampler2DArrayShadow : detail::SamplerBase<TextureType::TEXTURE_2D_ARRAY, true> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a samplerCube Texture that may be used as a parameter value
 * type in a shader-read buffer.
 */
struct samplerCube : detail::SamplerBase<TextureType::TEXTURE_CUBE, false> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a samplerCubeShadow Texture that may be used as a parameter
 * value type in a shader-read buffer.
 */
struct samplerCubeShadow : detail::SamplerBase<TextureType::TEXTURE_CUBE, true> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a samplerCubeArray Texture that may be used as a parameter value
 * type in a shader-read buffer.
 */
struct samplerCubeArray : detail::SamplerBase<TextureType::TEXTURE_CUBE_ARRAY, false> {
	using SamplerBase::SamplerBase;
};

/**
 * Reference to a samplerCubeArrayShadow Texture that may be used as a parameter
 * value type in a shader-read buffer.
 */
struct samplerCubeArrayShadow : detail::SamplerBase<TextureType::TEXTURE_CUBE_ARRAY, true> {
	using SamplerBase::SamplerBase;
};

/**
 * Concept that checks if a type is a valid parameter value type for a
 * shader-read buffer.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept parameter_value_type =          //
	same_as<T, int32_t> ||              //
	same_as<T, ivec2> ||                //
	same_as<T, ivec3> ||                //
	same_as<T, ivec4> ||                //
	same_as<T, uint32_t> ||             //
	same_as<T, uvec2> ||                //
	same_as<T, uvec3> ||                //
	same_as<T, uvec4> ||                //
	same_as<T, float> ||                //
	same_as<T, vec2> ||                 //
	same_as<T, vec3> ||                 //
	same_as<T, vec4> ||                 //
	same_as<T, mat2> ||                 //
	same_as<T, mat3> ||                 //
	same_as<T, mat4> ||                 //
	same_as<T, sampler2D> ||            //
	same_as<T, sampler2DShadow> ||      //
	same_as<T, sampler2DArray> ||       //
	same_as<T, sampler2DArrayShadow> || //
	same_as<T, samplerCube> ||          //
	same_as<T, samplerCubeShadow> ||    //
	same_as<T, samplerCubeArray> ||     //
	same_as<T, samplerCubeArrayShadow>;

/**
 * Specification of the underlying value type of a parameter in a shader-read
 * buffer.
 */
enum class ParameterType : uint8_t {
	INT,                       ///< C++: int32_t, GLSL: int
	IVEC2,                     ///< C++: i32vec2, GLSL: ivec2
	IVEC3,                     ///< C++: i32vec3, GLSL: ivec3
	IVEC4,                     ///< C++: i32vec4, GLSL: ivec4
	UINT,                      ///< C++: uint32_t, GLSL: uint
	UVEC2,                     ///< C++: u32vec2, GLSL: uvec2
	UVEC3,                     ///< C++: u32vec3, GLSL: uvec3
	UVEC4,                     ///< C++: u32vec4, GLSL: uvec4
	FLOAT,                     ///< C++: float, GLSL: float
	VEC2,                      ///< C++: vec2, GLSL: vec2
	VEC3,                      ///< C++: vec3, GLSL: vec3
	VEC4,                      ///< C++: vec4, GLSL: vec4
	MAT2,                      ///< C++: mat2, GLSL: mat2
	MAT3,                      ///< C++: mat3, GLSL: mat3
	MAT4,                      ///< C++: mat4, GLSL: mat4
	SAMPLER_2D,                ///< C++: sampler2D, GLSL: sampler2D
	SAMPLER_2D_SHADOW,         ///< C++: sampler2DShadow, GLSL: sampler2DShadow
	SAMPLER_2D_ARRAY,          ///< C++: sampler2DArray, GLSL: sampler2DArray
	SAMPLER_2D_ARRAY_SHADOW,   ///< C++: sampler2DArrayShadow, GLSL: sampler2DArrayShadow
	SAMPLER_CUBE,              ///< C++: samplerCube, GLSL: samplerCube
	SAMPLER_CUBE_SHADOW,       ///< C++: samplerCubeShadow, GLSL: samplerCubeShadow
	SAMPLER_CUBE_ARRAY,        ///< C++: samplerCubeArray, GLSL: samplerCubeArray
	SAMPLER_CUBE_ARRAY_SHADOW, ///< C++: samplerCubeArrayShadow, GLSL: samplerCubeArrayShadow
};

namespace detail {

template <typename T>
inline constexpr ParameterType PARAMETER_TYPE{};

// clang-format off
template <> inline constexpr ParameterType PARAMETER_TYPE<int32_t> = ParameterType::INT;
template <> inline constexpr ParameterType PARAMETER_TYPE<ivec2> = ParameterType::IVEC2;
template <> inline constexpr ParameterType PARAMETER_TYPE<ivec3> = ParameterType::IVEC3;
template <> inline constexpr ParameterType PARAMETER_TYPE<ivec4> = ParameterType::IVEC4;
template <> inline constexpr ParameterType PARAMETER_TYPE<uint32_t> = ParameterType::UINT;
template <> inline constexpr ParameterType PARAMETER_TYPE<uvec2> = ParameterType::UVEC2;
template <> inline constexpr ParameterType PARAMETER_TYPE<uvec3> = ParameterType::UVEC3;
template <> inline constexpr ParameterType PARAMETER_TYPE<uvec4> = ParameterType::UVEC4;
template <> inline constexpr ParameterType PARAMETER_TYPE<float> = ParameterType::FLOAT;
template <> inline constexpr ParameterType PARAMETER_TYPE<vec2> = ParameterType::VEC2;
template <> inline constexpr ParameterType PARAMETER_TYPE<vec3> = ParameterType::VEC3;
template <> inline constexpr ParameterType PARAMETER_TYPE<vec4> = ParameterType::VEC4;
template <> inline constexpr ParameterType PARAMETER_TYPE<mat2> = ParameterType::MAT2;
template <> inline constexpr ParameterType PARAMETER_TYPE<mat3> = ParameterType::MAT3;
template <> inline constexpr ParameterType PARAMETER_TYPE<mat4> = ParameterType::MAT4;
template <> inline constexpr ParameterType PARAMETER_TYPE<sampler2D> = ParameterType::SAMPLER_2D;
template <> inline constexpr ParameterType PARAMETER_TYPE<sampler2DShadow> = ParameterType::SAMPLER_2D_SHADOW;
template <> inline constexpr ParameterType PARAMETER_TYPE<sampler2DArray> = ParameterType::SAMPLER_2D_ARRAY;
template <> inline constexpr ParameterType PARAMETER_TYPE<sampler2DArrayShadow> = ParameterType::SAMPLER_2D_ARRAY_SHADOW;
template <> inline constexpr ParameterType PARAMETER_TYPE<samplerCube> = ParameterType::SAMPLER_CUBE;
template <> inline constexpr ParameterType PARAMETER_TYPE<samplerCubeShadow> = ParameterType::SAMPLER_CUBE_SHADOW;
template <> inline constexpr ParameterType PARAMETER_TYPE<samplerCubeArray> = ParameterType::SAMPLER_CUBE_ARRAY;
template <> inline constexpr ParameterType PARAMETER_TYPE<samplerCubeArrayShadow> = ParameterType::SAMPLER_CUBE_ARRAY_SHADOW;
// clang-format on

} // namespace detail

namespace detail {

template <typename T>
struct is_parameter : std::false_type {};

template <parameter_value_type T>
struct is_parameter<T> : std::true_type {};

template <parameter_value_type T, size_t N>
struct is_parameter<Array<T, N>> : std::true_type {};

template <typename TupleType>
inline constexpr bool is_parameter_v = is_parameter<TupleType>::value;

template <typename TupleType>
struct is_parameters : std::false_type {};

template <typename... Ts>
struct is_parameters<Tuple<Ts...>> : std::bool_constant<(is_parameter_v<std::remove_cvref_t<Ts>> && ...)> {};

template <typename TupleType>
inline constexpr bool is_parameters_v = is_parameters<TupleType>::value;

} // namespace detail

/**
 * Concept that checks if a type is a valid parameter struct type for a
 * shader-read buffer.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept parameter_struct = //
	aggregate<T> &&        //
	standard_layout<T> &&  //
	detail::is_parameters_v<decltype(meta::getFields(std::declval<T>()))>;

/**
 * Description of a parameter in a parameter struct for a shader-read buffer.
 */
struct ParameterDescription {
	ParameterType type;       ///< Parameter value type.
	CStringView name;         ///< Identifier that the parameter or array is accessed by in the shader source code.
	size_t arrayElementCount; ///< Number of elements in the array, or 0 if not an array.
};

/**
 * Check if a parameter type is a texture sampler type.
 *
 * \param type parameter type to check.
 *
 * \return true if the given parameter type is a texture parameter, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool isTextureParameter(ParameterType type) noexcept {
	switch (type) {
		case ParameterType::INT: [[fallthrough]];
		case ParameterType::IVEC2: [[fallthrough]];
		case ParameterType::IVEC3: [[fallthrough]];
		case ParameterType::IVEC4: [[fallthrough]];
		case ParameterType::UINT: [[fallthrough]];
		case ParameterType::UVEC2: [[fallthrough]];
		case ParameterType::UVEC3: [[fallthrough]];
		case ParameterType::UVEC4: [[fallthrough]];
		case ParameterType::FLOAT: [[fallthrough]];
		case ParameterType::VEC2: [[fallthrough]];
		case ParameterType::VEC3: [[fallthrough]];
		case ParameterType::VEC4: [[fallthrough]];
		case ParameterType::MAT2: [[fallthrough]];
		case ParameterType::MAT3: [[fallthrough]];
		case ParameterType::MAT4: return false;
		case ParameterType::SAMPLER_2D: [[fallthrough]];
		case ParameterType::SAMPLER_2D_ARRAY: [[fallthrough]];
		case ParameterType::SAMPLER_2D_SHADOW: [[fallthrough]];
		case ParameterType::SAMPLER_2D_ARRAY_SHADOW: [[fallthrough]];
		case ParameterType::SAMPLER_CUBE: [[fallthrough]];
		case ParameterType::SAMPLER_CUBE_SHADOW: [[fallthrough]];
		case ParameterType::SAMPLER_CUBE_ARRAY: [[fallthrough]];
		case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW: return true;
	}
	return false;
}

namespace detail {

static_assert(sizeof(float) == sizeof(float32_t));
static_assert(alignof(float) == alignof(float32_t));
static_assert(sizeof(int32_t) == sizeof(float));
static_assert(sizeof(ivec2) == sizeof(float) * 2);
static_assert(sizeof(ivec3) == sizeof(float) * 3);
static_assert(sizeof(ivec4) == sizeof(float) * 4);
static_assert(sizeof(uint32_t) == sizeof(float));
static_assert(sizeof(uvec2) == sizeof(float) * 2);
static_assert(sizeof(uvec3) == sizeof(float) * 3);
static_assert(sizeof(uvec4) == sizeof(float) * 4);
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

constexpr void alignParameter(size_t& byteOffset, ParameterType type, size_t arrayElementCount) noexcept {
	if (arrayElementCount == 0) {
		switch (type) {
			case ParameterType::INT: byteOffset = roundUpToMultiple(byteOffset, sizeof(float)); break;
			case ParameterType::IVEC2: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 2); break;
			case ParameterType::IVEC3: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::IVEC4: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::UINT: byteOffset = roundUpToMultiple(byteOffset, sizeof(float)); break;
			case ParameterType::UVEC2: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 2); break;
			case ParameterType::UVEC3: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::UVEC4: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::FLOAT: byteOffset = roundUpToMultiple(byteOffset, sizeof(float)); break;
			case ParameterType::VEC2: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 2); break;
			case ParameterType::VEC3: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::VEC4: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::MAT2: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::MAT3: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::MAT4: byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4); break;
			case ParameterType::SAMPLER_2D: break;
			case ParameterType::SAMPLER_2D_ARRAY: break;
			case ParameterType::SAMPLER_2D_SHADOW: break;
			case ParameterType::SAMPLER_2D_ARRAY_SHADOW: break;
			case ParameterType::SAMPLER_CUBE: break;
			case ParameterType::SAMPLER_CUBE_SHADOW: break;
			case ParameterType::SAMPLER_CUBE_ARRAY: break;
			case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW: break;
		}
	} else if (!isTextureParameter(type)) {
		byteOffset = roundUpToMultiple(byteOffset, sizeof(float) * 4);
	}
}

constexpr void skipParameter(size_t& byteOffset, ParameterType type, size_t arrayElementCount) noexcept {
	if (arrayElementCount == 0) {
		switch (type) {
			case ParameterType::INT: byteOffset += sizeof(float); break;
			case ParameterType::IVEC2: byteOffset += sizeof(float) * 2; break;
			case ParameterType::IVEC3: byteOffset += sizeof(float) * 3; break;
			case ParameterType::IVEC4: byteOffset += sizeof(float) * 4; break;
			case ParameterType::UINT: byteOffset += sizeof(float); break;
			case ParameterType::UVEC2: byteOffset += sizeof(float) * 2; break;
			case ParameterType::UVEC3: byteOffset += sizeof(float) * 3; break;
			case ParameterType::UVEC4: byteOffset += sizeof(float) * 4; break;
			case ParameterType::FLOAT: byteOffset += sizeof(float); break;
			case ParameterType::VEC2: byteOffset += sizeof(float) * 2; break;
			case ParameterType::VEC3: byteOffset += sizeof(float) * 3; break;
			case ParameterType::VEC4: byteOffset += sizeof(float) * 4; break;
			case ParameterType::MAT2: byteOffset += 2 * sizeof(float) * 4; break;
			case ParameterType::MAT3: byteOffset += 3 * sizeof(float) * 4; break;
			case ParameterType::MAT4: byteOffset += 4 * sizeof(float) * 4; break;
			case ParameterType::SAMPLER_2D: break;
			case ParameterType::SAMPLER_2D_ARRAY: break;
			case ParameterType::SAMPLER_2D_SHADOW: break;
			case ParameterType::SAMPLER_2D_ARRAY_SHADOW: break;
			case ParameterType::SAMPLER_CUBE: break;
			case ParameterType::SAMPLER_CUBE_SHADOW: break;
			case ParameterType::SAMPLER_CUBE_ARRAY: break;
			case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW: break;
		}
	} else {
		switch (type) {
			case ParameterType::INT: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::IVEC2: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::IVEC3: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::IVEC4: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::UINT: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::UVEC2: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::UVEC3: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::UVEC4: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::FLOAT: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::VEC2: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::VEC3: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::VEC4: byteOffset += arrayElementCount * sizeof(float) * 4; break;
			case ParameterType::MAT2: byteOffset += arrayElementCount * 2 * sizeof(float) * 4; break;
			case ParameterType::MAT3: byteOffset += arrayElementCount * 3 * sizeof(float) * 4; break;
			case ParameterType::MAT4: byteOffset += arrayElementCount * 4 * sizeof(float) * 4; break;
			case ParameterType::SAMPLER_2D: break;
			case ParameterType::SAMPLER_2D_ARRAY: break;
			case ParameterType::SAMPLER_2D_SHADOW: break;
			case ParameterType::SAMPLER_2D_ARRAY_SHADOW: break;
			case ParameterType::SAMPLER_CUBE: break;
			case ParameterType::SAMPLER_CUBE_SHADOW: break;
			case ParameterType::SAMPLER_CUBE_ARRAY: break;
			case ParameterType::SAMPLER_CUBE_ARRAY_SHADOW: break;
		}
	}
}

template <parameter_value_type T>
inline void writeParameter(byte* bytes, const T& parameter) noexcept {
	constexpr ParameterType TYPE = detail::PARAMETER_TYPE<T>;
	if constexpr (TYPE == ParameterType::INT) {
		memcpy(bytes, &parameter, sizeof(int32_t));
	} else if constexpr (TYPE == ParameterType::IVEC2) {
		memcpy(bytes, &parameter, sizeof(ivec2));
	} else if constexpr (TYPE == ParameterType::IVEC3) {
		memcpy(bytes, &parameter, sizeof(ivec3));
	} else if constexpr (TYPE == ParameterType::IVEC4) {
		memcpy(bytes, &parameter, sizeof(ivec4));
	} else if constexpr (TYPE == ParameterType::UINT) {
		memcpy(bytes, &parameter, sizeof(uint32_t));
	} else if constexpr (TYPE == ParameterType::UVEC2) {
		memcpy(bytes, &parameter, sizeof(uvec2));
	} else if constexpr (TYPE == ParameterType::UVEC3) {
		memcpy(bytes, &parameter, sizeof(uvec3));
	} else if constexpr (TYPE == ParameterType::UVEC4) {
		memcpy(bytes, &parameter, sizeof(uvec4));
	} else if constexpr (TYPE == ParameterType::FLOAT) {
		memcpy(bytes, &parameter, sizeof(float));
	} else if constexpr (TYPE == ParameterType::VEC2) {
		memcpy(bytes, &parameter, sizeof(vec2));
	} else if constexpr (TYPE == ParameterType::VEC3) {
		memcpy(bytes, &parameter, sizeof(vec3));
	} else if constexpr (TYPE == ParameterType::VEC4) {
		memcpy(bytes, &parameter, sizeof(vec4));
	} else if constexpr (TYPE == ParameterType::MAT2) {
		memcpy(bytes, &parameter[0], sizeof(vec2));
		memcpy(bytes + sizeof(float) * 4, &parameter[1], sizeof(vec2));
	} else if constexpr (TYPE == ParameterType::MAT3) {
		memcpy(bytes, &parameter[0], sizeof(vec3));
		memcpy(bytes + sizeof(float) * 4, &parameter[1], sizeof(vec3));
		memcpy(bytes + 2 * sizeof(float) * 4, &parameter[2], sizeof(vec3));
	} else if constexpr (TYPE == ParameterType::MAT4) {
		memcpy(bytes, &parameter, sizeof(mat4));
	} else {
		static_assert(isTextureParameter(TYPE));
	}
}

template <parameter_value_type T, size_t N>
inline void writeParameter(byte* bytes, const Array<T, N>& parameter) noexcept {
	constexpr ParameterType TYPE = detail::PARAMETER_TYPE<T>;
	for (size_t i = 0; i < N; ++i) {
		if constexpr (TYPE == ParameterType::INT) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(int32_t));
		} else if constexpr (TYPE == ParameterType::IVEC2) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(ivec2));
		} else if constexpr (TYPE == ParameterType::IVEC3) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(ivec3));
		} else if constexpr (TYPE == ParameterType::IVEC4) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(ivec4));
		} else if constexpr (TYPE == ParameterType::UINT) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(uint32_t));
		} else if constexpr (TYPE == ParameterType::UVEC2) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(uvec2));
		} else if constexpr (TYPE == ParameterType::UVEC3) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(uvec3));
		} else if constexpr (TYPE == ParameterType::UVEC4) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(uvec4));
		} else if constexpr (TYPE == ParameterType::FLOAT) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(float));
		} else if constexpr (TYPE == ParameterType::VEC2) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(vec2));
		} else if constexpr (TYPE == ParameterType::VEC3) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(vec3));
		} else if constexpr (TYPE == ParameterType::VEC4) {
			memcpy(bytes + i * sizeof(float) * 4, &parameter[i], sizeof(vec4));
		} else if constexpr (TYPE == ParameterType::MAT2) {
			memcpy(bytes + i * 2 * sizeof(float) * 4, &parameter[i][0], sizeof(vec2));
			memcpy(bytes + i * 2 * sizeof(float) * 4 + sizeof(float) * 4, &parameter[i][1], sizeof(vec2));
		} else if constexpr (TYPE == ParameterType::MAT3) {
			memcpy(bytes + i * 3 * sizeof(float) * 4, &parameter[i][0], sizeof(vec3));
			memcpy(bytes + i * 3 * sizeof(float) * 4 + sizeof(float) * 4, &parameter[i][1], sizeof(vec3));
			memcpy(bytes + i * 3 * sizeof(float) * 4 + 2 * sizeof(float) * 4, &parameter[i][2], sizeof(vec3));
		} else if constexpr (TYPE == ParameterType::MAT4) {
			memcpy(bytes + i * 4 * sizeof(float) * 4, &parameter[i], sizeof(mat4));
		} else {
			static_assert(isTextureParameter(TYPE));
		}
	}
}

template <typename Wrapper>
inline constexpr ParameterType PARAMETER_TYPE_OF_PARAMETER_WRAPPER{};

template <parameter_value_type T>
inline constexpr ParameterType PARAMETER_TYPE_OF_PARAMETER_WRAPPER<T> = PARAMETER_TYPE<T>;

template <parameter_value_type T, size_t N>
inline constexpr ParameterType PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Array<T, N>> = PARAMETER_TYPE<T>;

template <typename Wrapper>
inline constexpr size_t ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER{};

template <parameter_value_type T>
inline constexpr size_t ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<T> = 0;

template <parameter_value_type T, size_t N>
inline constexpr size_t ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Array<T, N>> = N;

template <typename Wrapper>
inline constexpr size_t TEXTURE_COUNT_OF_PARAMETER_WRAPPER{};

template <parameter_value_type T>
inline constexpr size_t TEXTURE_COUNT_OF_PARAMETER_WRAPPER<T> = (isTextureParameter(detail::PARAMETER_TYPE<T>)) ? 1 : 0;

template <parameter_value_type T, size_t N>
inline constexpr size_t TEXTURE_COUNT_OF_PARAMETER_WRAPPER<Array<T, N>> = (isTextureParameter(detail::PARAMETER_TYPE<T>)) ? N : 0;

template <parameter_value_type T>
constexpr void writeParameterTextures(SharedPointer<TextureImplementation>* textures, const T& parameter) noexcept {
	if constexpr (isTextureParameter(detail::PARAMETER_TYPE<T>)) {
		*textures = parameter.lock();
	}
}

template <parameter_value_type T, size_t N>
constexpr void writeParameterTextures(SharedPointer<TextureImplementation>* textures, const Array<T, N>& parameter) noexcept {
	if constexpr (isTextureParameter(detail::PARAMETER_TYPE<T>)) {
		for (size_t i = 0; i < N; ++i) {
			textures[i] = parameter[i].lock();
		}
	}
}

template <typename Tuple>
inline constexpr size_t TUPLE_PARAMETER_VALUES_BYTES_SIZE{};

template <typename... ParameterTypes>
inline constexpr size_t TUPLE_PARAMETER_VALUES_BYTES_SIZE<Tuple<ParameterTypes...>> = []() -> size_t {
	size_t result = 0;
	((detail::alignParameter(result, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<std::remove_cvref_t<ParameterTypes>>,
		  detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<std::remove_cvref_t<ParameterTypes>>),
		 detail::skipParameter(result, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<std::remove_cvref_t<ParameterTypes>>,
			 detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<std::remove_cvref_t<ParameterTypes>>)),
		...);
	return roundUpToMultiple(result, sizeof(float) * 4);
}();

template <typename ParameterStruct>
inline constexpr size_t PARAMETER_VALUES_BYTES_SIZE = TUPLE_PARAMETER_VALUES_BYTES_SIZE<decltype(meta::getFields(std::declval<ParameterStruct>()))>;

template <typename Tuple>
inline constexpr size_t TUPLE_PARAMETER_TEXTURES_COUNT{};

template <typename... ParameterTypes>
inline constexpr size_t TUPLE_PARAMETER_TEXTURES_COUNT<Tuple<ParameterTypes...>> = []() -> size_t {
	size_t result = 0;
	((result += detail::TEXTURE_COUNT_OF_PARAMETER_WRAPPER<std::remove_cvref_t<ParameterTypes>>), ...);
	return result;
}();

template <typename ParameterStruct>
inline constexpr size_t PARAMETER_TEXTURES_COUNT = TUPLE_PARAMETER_TEXTURES_COUNT<decltype(meta::getFields(std::declval<ParameterStruct>()))>;

template <parameter_value_type T>
[[nodiscard]] constexpr ParameterDescription getParameterDescription(meta::Type<T>, CStringView name) {
	return {
		.type = detail::PARAMETER_TYPE<T>,
		.name = name,
		.arrayElementCount = 0,
	};
}

template <parameter_value_type T, size_t N>
[[nodiscard]] constexpr ParameterDescription getParameterDescription(meta::Type<Array<T, N>>, CStringView name) {
	return {
		.type = detail::PARAMETER_TYPE<T>,
		.name = name,
		.arrayElementCount = N,
	};
}

template <typename ParameterStruct>
[[nodiscard]] constexpr auto getParameterDescriptions() {
	Array<ParameterDescription, meta::aggregate_size_v<ParameterStruct>> result{};
	meta::forEachIndexedFieldType<ParameterStruct>([&](auto index, auto type) -> void { //
		result[index] = getParameterDescription(type, meta::aggregate_field_name_v<index, ParameterStruct>);
	});
	return result;
}

template <typename ParameterStruct>
inline constexpr auto PARAMETER_DESCRIPTIONS = getParameterDescriptions<ParameterStruct>();

} // namespace detail

} // namespace grem::graphics

#endif
