// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_LIGHTS_3D_HPP
#define GREM_GRAPHICS_3D_LIGHTS_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/data/Registry.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Model3D.hpp>

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer3D; // Forward declaration, to avoid a circular include of Renderer3D.hpp.

/**
 * Opaque handle to a specific light in a Lights3D set.
 */
struct LightID : RegistryElementIDBase<LightID> {
	using RegistryElementIDBase::RegistryElementIDBase;
};

/**
 * Type of a light in a Lights3D set.
 */
enum class LightType : uint32_t { // NOLINT(performance-enum-size)
	AMBIENT_LIGHT = 0,            ///< See AmbientLightOptions3D.
	SUN_LIGHT = 1,                ///< See SunLightOptions3D.
	DIRECTIONAL_LIGHT = 2,        ///< See DirectionalLightOptions3D.
	POINT_LIGHT = 3,              ///< See PointLightOptions3D.
	SPOT_LIGHT = 4,               ///< See SpotLightOptions3D.
};

/**
 * Configuration options for a 3D light source representing the ambient light in a
 * scene, which adds an (unrealistic) constant amount of light to everything.
 */
struct AmbientLightOptions3D {
	/**
	 * Color of the light, where the alpha component controls the light's
	 * intensity.
	 */
	Color color = Color::WHITE;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const AmbientLightOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for a 3D light source representing a directional light
 * that is tied to the skybox and replaces its baked lighting on directly lit
 * surfaces.
 */
struct SunLightOptions3D {
	/**
	 * Vector representing the direction of the light, in world coordinates.
	 */
	vec3 direction{0.0f, -1.0f, 0.0f};

	/**
	 * Color of the light, where the alpha component controls the light's
	 * intensity.
	 */
	Color color = Color::WHITE;

	/**
	 * Constant normal offset bias factor to use when sampling the shadow map of
	 * the light.
	 */
	float shadowMapNormalOffsetBiasConstantFactor = 0.04f;

	/**
	 * Slope-scaled normal offset bias factor to use when sampling the shadow
	 * map of the light.
	 */
	float shadowMapNormalOffsetBiasSlopeFactor = 1.0f;

	/**
	 * Whether the light should use shadow mapping or not.
	 */
	bool shadowMapped = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SunLightOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for a 3D light source representing a purely
 * directional light with infinite range.
 */
struct DirectionalLightOptions3D {
	/**
	 * Vector representing the direction of the light, in world coordinates.
	 */
	vec3 direction{0.0f, -1.0f, 0.0f};

	/**
	 * Color of the light, where the alpha component controls the light's
	 * intensity.
	 */
	Color color = Color::WHITE;

	/**
	 * Constant normal offset bias factor to use when sampling the shadow map of
	 * the light.
	 */
	float shadowMapNormalOffsetBiasConstantFactor = 0.04f;

	/**
	 * Slope-scaled normal offset bias factor to use when sampling the shadow
	 * map of the light.
	 */
	float shadowMapNormalOffsetBiasSlopeFactor = 1.0f;

	/**
	 * Whether the light should use shadow mapping or not.
	 */
	bool shadowMapped = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const DirectionalLightOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for an infinitesimally small point light source,
 * which can be used to approximate e.g. a light bulb.
 */
struct PointLightOptions3D {
	/**
	 * Position of the light, in world coordinates.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Maximum range of the light, in world coordinates, beyond which the
	 * light will have no effect, or a non-positive value for infinite
	 * range.
	 */
	float range = 10.0f;

	/**
	 * Color of the light, where the alpha component controls the light's
	 * intensity.
	 */
	Color color = Color::WHITE;

	/**
	 * Distance to the near plane of the light's shadow map.
	 *
	 * Must be less than #shadowFarZ.
	 */
	float shadowNearZ = 0.01f;

	/**
	 * Distance to the near plane of the light's shadow map.
	 *
	 * Must be greater than #shadowNearZ.
	 */
	float shadowFarZ = 100.0f;

	/**
	 * Constant normal offset bias factor to use when sampling the shadow map of
	 * the light.
	 */
	float shadowMapNormalOffsetBiasConstantFactor = 0.04f;

	/**
	 * Slope-scaled normal offset bias factor to use when sampling the shadow
	 * map of the light.
	 */
	float shadowMapNormalOffsetBiasSlopeFactor = 1.0f;

	/**
	 * Whether the light should use shadow mapping or not.
	 */
	bool shadowMapped = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const PointLightOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for a point light source that is limited to a cone
 * shape, which can be used to approximate e.g. a headlamp on a car.
 */
struct SpotLightOptions3D {
	/**
	 * Position of the light, in world coordinates.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Vector representing the direction of the light, in world coordinates.
	 */
	vec3 direction{0.0f, -1.0f, 0.0f};

	/**
	 * Maximum range of the light, in world coordinates, beyond which the
	 * light will have no effect, or a non-positive value for infinite
	 * range.
	 */
	float range = 10.0f;

	/**
	 * Angle, from the light's direction axis, of the inner limit where the
	 * light cone is the brightest, in radians. Must be less than or equal
	 * to #outerConeAngle, and between 0 and pi radians (inclusive).
	 */
	float innerConeAngle = convertDegreesToRadians(22.5f);

	/**
	 * Angle, from the light's direction axis, of the outer limit where the
	 * light cone fades to zero, in radians. Must be greater than or equal
	 * to #innerConeAngle, and between 0 and pi radians (inclusive).
	 */
	float outerConeAngle = convertDegreesToRadians(45.0f);

	/**
	 * Color of the light, where the alpha component controls the light's
	 * intensity.
	 */
	Color color = Color::WHITE;

	/**
	 * Distance to the near plane of the light's shadow map.
	 *
	 * Must be less than or equal to #shadowFarZ.
	 */
	float shadowNearZ = 0.01f;

	/**
	 * Distance to the near plane of the light's shadow map.
	 *
	 * Must be greater than or equal to #shadowNearZ.
	 */
	float shadowFarZ = 100.0f;

	/**
	 * Constant normal offset bias factor to use when sampling the shadow map of
	 * the light.
	 */
	float shadowMapNormalOffsetBiasConstantFactor = 0.04f;

	/**
	 * Slope-scaled normal offset bias factor to use when sampling the shadow
	 * map of the light.
	 */
	float shadowMapNormalOffsetBiasSlopeFactor = 1.0f;

	/**
	 * Whether the light should use shadow mapping or not.
	 */
	bool shadowMapped = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SpotLightOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for a Lights3D set.
 */
struct Lights3DOptions {
	/**
	 * Maximum number of cascade levels used for cascaded shadow mapping of
	 * directional lights.
	 */
	static constexpr size_t MAX_SHADOW_CASCADE_COUNT = 8;

	/**
	 * Width, in texels, of each cascade level of the shadow maps of
	 * sun/directional lights. Must be a power of 2.
	 */
	uint32_t cascadedShadowMapResolution = 2048;

	/**
	 * Width, in texels, of the shadow maps of point lights. Must be a power of
	 * 2.
	 */
	uint32_t pointLightShadowMapResolution = 512;

	/**
	 * Width, in texels, of the shadow maps of spot lights. Must be a power of
	 * 2.
	 */
	uint32_t spotLightShadowMapResolution = 512;

	/**
	 * Distances of the far planes of each cascade level's view frustum used for
	 * cascaded shadow mapping of sun/directional lights. Must be non-empty.
	 */
	InplaceBuffer<float, MAX_SHADOW_CASCADE_COUNT> shadowCascadeFrustumFarPlaneDistances{
		5.0f,
		13.0f,
		35.0f,
		100.0f,
		300.0f,
	};

	/**
	 * Length of the blend area between cascade levels for cascaded shadow
	 * mapping of sun/directional lights.
	 *
	 * Must be positive.
	 */
	float shadowCascadeBlendSize = 4.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Lights3DOptions& other) const noexcept = default;
};

/**
 * Set of direct light sources in 3D space.
 */
class Lights3D {
public:
	/** Maximum number of cascade levels used for cascaded shadow mapping of directional lights. */
	static constexpr size_t MAX_SHADOW_CASCADE_COUNT = Lights3DOptions::MAX_SHADOW_CASCADE_COUNT;

	/** Struct of shader parameters for a set of lights. */
	struct Parameters {
		/** Distances to the far planes of each cascade level's view frustum used for cascaded shadow mapping of directional lights. */
		Array<float, Lights3D::MAX_SHADOW_CASCADE_COUNT> lightsShadowCascadeFrustumFarPlaneDistances;

		/** Number of shadow cascade levels for cascaded shadow mapping of directional lights. */
		uint32_t lightsShadowCascadeCount;

		/** Length of the blend area between cascade levels for cascaded shadow mapping of directional lights. */
		float lightsShadowCascadeBlendSize;
	};

	/** Shader buffer for light parameters. */
	using ParameterBuffer = UniformBuffer<Parameters, "Lights3DParameters">;

	/** Struct of shader fields representing the shadow matrix of a light. */
	struct ShadowMatrixFields {
		/** Combined view-projection matrix of the light. */
		mat4 shadowMatrix;
	};

	/** Shader buffer for shadow matrices. */
	using ShadowMatrixBuffer = StorageBuffer<ShadowMatrixFields, "Lights3DShadowMatrices">;

	/**
	 * Construct a set of lights.
	 *
	 * \param device device to create the lights for. Must outlive the lights.
	 * \param options light set options, see Lights3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) explicit Lights3D(Device& device, const Lights3DOptions& options = {});

	/**
	 * Destroy all lights and remove them from the set.
	 */
	GREM_API(graphics_3d) void clearLights() noexcept {
		lights.clear();
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
	}

	/**
	 * Create an ambient light source, which adds an (unrealistic) constant
	 * amount of light to everything.
	 *
	 * \param options light options, see AmbientLightOptions3D.
	 *
	 * \return a handle to the new light that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) LightID createAmbientLight(const AmbientLightOptions3D& options);

	/**
	 * Create a sun light source, or a directional light that is tied to the
	 * skybox and replaces its baked lighting on directly lit surfaces.
	 *
	 * \param options light options, see SunLightOptions3D.
	 *
	 * \return a handle to the new light that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the created light is shadow mapped, any current rendered shadow
	 *       maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) LightID createSunLight(const SunLightOptions3D& options);

	/**
	 * Create a purely directional light with infinite range.
	 *
	 * \param options light options, see DirectionalLightOptions3D.
	 *
	 * \return a handle to the new light that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the created light is shadow mapped, any current rendered shadow
	 *       maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) LightID createDirectionalLight(const DirectionalLightOptions3D& options);

	/**
	 * Create an infinitesimally small point light, which can be used to
	 * approximate e.g. a light bulb.
	 *
	 * \param options light options, see PointLightOptions3D.
	 *
	 * \return a handle to the new light that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the created light is shadow mapped, any current rendered shadow
	 *       maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) LightID createPointLight(const PointLightOptions3D& options);

	/**
	 * Create a point light that is limited to a cone shape, which can be used
	 * to approximate e.g. a headlamp on a car.
	 *
	 * \param options light options, see SpotLightOptions3D.
	 *
	 * \return a handle to the new light that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the created light is shadow mapped, any current rendered shadow
	 *       maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) LightID createSpotLight(const SpotLightOptions3D& options);

	/**
	 * Check if a specific light handle is still valid, meaning that the light
	 * has not yet been destroyed.
	 *
	 * \param id handle to the light.
	 *
	 * \return true if the associated light still exists, false if the light has
	 *         been destroyed.
	 */
	[[nodiscard]] GREM_API(graphics_3d) bool containsLight(LightID id) const noexcept;

	/**
	 * Destroy a light and remove it from the set.
	 *
	 * \param id handle to the light.
	 *
	 * \return true if the specified light was found and destroyed, false
	 *         otherwise.
	 *
	 * \note If the specified light has already been destroyed, this function
	 *       has no effect.
	 * \note If the destroyed light was shadow mapped, any current rendered
	 *       shadow maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) bool destroyLight(LightID id);

	/**
	 * Get the light type of a specific light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the type of the light, or an empty optional if the light
	 *         doesn't exist.
	 */
	GREM_API(graphics_3d) Optional<LightType> getLightType(LightID id) const;

	/**
	 * Get the options of a specific ambient light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the current options of the light, or an empty optional if the
	 *         light doesn't exist or is not an ambient light.
	 */
	GREM_API(graphics_3d) Optional<AmbientLightOptions3D> getAmbientLightOptions(LightID id) const;

	/**
	 * Get the options of a specific sun light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the current options of the light, or an empty optional if the
	 *         light doesn't exist or is not a sun light.
	 */
	GREM_API(graphics_3d) Optional<SunLightOptions3D> getSunLightOptions(LightID id) const;

	/**
	 * Get the options of a specific directional light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the current options of the light, or an empty optional if the
	 *         light doesn't exist or is not a directional light.
	 */
	GREM_API(graphics_3d) Optional<DirectionalLightOptions3D> getDirectionalLightOptions(LightID id) const;

	/**
	 * Get the options of a specific point light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the current options of the light, or an empty optional if the
	 *         light doesn't exist or is not a point light.
	 */
	GREM_API(graphics_3d) Optional<PointLightOptions3D> getPointLightOptions(LightID id) const;

	/**
	 * Get the options of a specific spot light.
	 *
	 * \param id handle to the light.
	 *
	 * \return the current options of the light, or an empty optional if the
	 *         light doesn't exist or is not a spot light.
	 */
	GREM_API(graphics_3d) Optional<SpotLightOptions3D> getSpotLightOptions(LightID id) const;

	/**
	 * Update the options of an ambient light.
	 *
	 * \param id handle to the light.
	 * \param newOptions new light options, see AmbientLightOptions3D.
	 *
	 * \note If the given light handle is invalid, or if the specified light is
	 *       not an ambient light, this function has no effect.
	 */
	GREM_API(graphics_3d) void setAmbientLightOptions(LightID id, const AmbientLightOptions3D& newOptions);

	/**
	 * Update the options of a sun light.
	 *
	 * \param id handle to the light.
	 * \param newOptions new light options, see SunLightOptions3D.
	 *
	 * \note If the given light handle is invalid, or if the specified light is
	 *       not a sun light, this function has no effect.
	 * \note If the shadow mapped state changed for the specified light, any
	 *       current rendered shadow maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) void setSunLightOptions(LightID id, const SunLightOptions3D& newOptions);

	/**
	 * Update the options of a directional light.
	 *
	 * \param id handle to the light.
	 * \param newOptions new light options, see DirectionalLightOptions3D.
	 *
	 * \note If the given light handle is invalid, or if the specified light is
	 *       not a directional light, this function has no effect.
	 * \note If the shadow mapped state changed for the specified light, any
	 *       current rendered shadow maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) void setDirectionalLightOptions(LightID id, const DirectionalLightOptions3D& newOptions);

	/**
	 * Update the options of a point light.
	 *
	 * \param id handle to the light.
	 * \param newOptions new light options, see PointLightOptions3D.
	 *
	 * \note If the given light handle is invalid, or if the specified light is
	 *       not a point light, this function has no effect.
	 * \note If the shadow mapped state changed for the specified light, any
	 *       current rendered shadow maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) void setPointLightOptions(LightID id, const PointLightOptions3D& newOptions);

	/**
	 * Update the options of a spot light.
	 *
	 * \param id handle to the light.
	 * \param newOptions new light options, see SpotLightOptions3D.
	 *
	 * \note If the given light handle is invalid, or if the specified light is
	 *       not a spot light, this function has no effect.
	 * \note If the shadow mapped state changed for the specified light, any
	 *       current rendered shadow maps are invalidated for all lights.
	 */
	GREM_API(graphics_3d) void setSpotLightOptions(LightID id, const SpotLightOptions3D& newOptions);

	/**
	 * Update the position of a light.
	 *
	 * \param id handle to the light.
	 * \param newPosition new position of the light, in world coordinates.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightPosition(LightID id, vec3 newPosition);

	/**
	 * Update the direction of a light.
	 *
	 * \param id handle to the light.
	 * \param newDirection new direction vector of the light, in world
	 *        coordinates.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightDirection(LightID id, vec3 newDirection);

	/**
	 * Update the range of a light.
	 *
	 * \param id handle to the light.
	 * \param newRange new maximum range of the light, in world coordinates,
	 *        beyond which the light will have no effect, or a non-positive
	 *        value for infinite range.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightRange(LightID id, float newRange);

	/**
	 * Update the inner cone angle of a light.
	 *
	 * \param id handle to the light.
	 * \param newInnerConeAngle new angle, in radians. Must be less than or
	 *        equal to the outer cone angle, and between 0 and pi radians
	 *        (inclusive).
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightInnerConeAngle(LightID id, float newInnerConeAngle);

	/**
	 * Update the outer cone angle of a light.
	 *
	 * \param id handle to the light.
	 * \param newOuterConeAngle new angle, in radians. Must be greater than or
	 *        equal to the inner cone angle, and between 0 and pi radians
	 *        (inclusive).
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightOuterConeAngle(LightID id, float newOuterConeAngle);

	/**
	 * Update the color of a light.
	 *
	 * \param id handle to the light.
	 * \param newColor new color of the light, where the alpha component
	 *        controls the light's intensity.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightColor(LightID id, Color newColor);

	/**
	 * Update the shadow near plane distance of a light.
	 *
	 * \param id handle to the light.
	 * \param newShadowNearZ new distance to the near plane of the light's
	 *        shadow map. Must be less than the shadow far plane distance.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightShadowNearZ(LightID id, float newShadowNearZ);

	/**
	 * Update the shadow far plane distance of a light.
	 *
	 * \param id handle to the light.
	 * \param newShadowFarZ new distance to the far plane of the light's
	 *        shadow map. Must be greater than the shadow near plane distance.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightShadowFarZ(LightID id, float newShadowFarZ);

	/**
	 * Update the shadow map constant normal offset bias factor of a light.
	 *
	 * \param id handle to the light.
	 * \param newShadowMapNormalOffsetBiasConstantFactor new constant normal
	 *        offset bias factor to use when sampling the shadow map of the
	 *        light.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightShadowMapNormalOffsetBiasConstantFactor(LightID id, float newShadowMapNormalOffsetBiasConstantFactor);

	/**
	 * Update the shadow map slope-scaled normal offset bias factor of a light.
	 *
	 * \param id handle to the light.
	 * \param newShadowMapNormalOffsetBiasSlopeFactor new slope-scaled normal
	 *        offset bias factor to use when sampling the shadow map of the
	 *        light.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setLightShadowMapNormalOffsetBiasSlopeFactor(LightID id, float newShadowMapNormalOffsetBiasSlopeFactor);

	/**
	 * Update whether a light should use shadow mapping or not.
	 *
	 * \param id handle to the light.
	 * \param newShadowMapped new shadow mapped flag.
	 *
	 * \note If the given light handle is invalid, this function has no effect.
	 * \note If the shadow mapped state successfully changed for the specified
	 *       light, any current rendered shadow maps are invalidated for all
	 *       lights.
	 */
	GREM_API(graphics_3d) void setLightShadowMapped(LightID id, bool newShadowMapped);

	/**
	 * Set the configuration options of the light set.
	 *
	 * \param newOptions new configuration options, see Lights3DOptions.
	 *
	 * \note This function invalidates any current rendered shadow maps for all
	 *       lights.
	 */
	void setOptions(const Lights3DOptions& newOptions) {
		options = newOptions;
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
		parameterBufferDirty = true;
	}

	/**
	 * Get the configuration options of the light set.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] Lights3DOptions getOptions() const noexcept {
		return options;
	}

	/**
	 * Execute a callback function for each light in the light set.
	 *
	 * \param callback function to execute, which should accept the light ID as
	 *        a parameter. The callback function should return either void or a
	 *        bool that specifies whether to stop the traversal or not. A value
	 *        of true means to stop and return early, while a value of false
	 *        means to continue traversing.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws any exception thrown by the callback function.
	 *
	 * \note The order of traversal is unspecified.
	 *
	 * \warning Lights must not be added or removed from the set during
	 *          traversal, unless the traversal is stopped immediately
	 *          afterwards.
	 */
	auto forEachLight(auto callback) {
		constexpr bool CALLBACK_RETURNS_BOOL = convertible_to<decltype(callback(LightID{})), bool>;
		for (const auto& [lightID, light] : lights) {
			if constexpr (CALLBACK_RETURNS_BOOL) {
				if (callback(lightID)) {
					return true;
				}
			} else {
				callback(lightID);
			}
		}
		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

private:
	friend Renderer3D;

	struct Light {
		vec3 direction{0.0f, 0.0f, 0.0f};
		float range = 0.0f;
		Color color;
		vec3 position{0.0f, 0.0f, 0.0f};
		float type;
		float innerConeCosine = 0.0f;
		float outerConeCosine = 0.0f;
		float shadowMapIndex = -1.0f;
		float shadowMatrixIndex = -1.0f;
		float shadowNearZ = 0.01f;
		float shadowFarZ = 100.0f;
		float shadowMapNormalOffsetBiasConstantFactor = 0.04f;
		float shadowMapNormalOffsetBiasSlopeFactor = 1.0f;
	};

	GREM_API(graphics_3d) void flushIndices(Device& device) const;
	GREM_API(graphics_3d) void flush(Device& device) const;

	Lights3DOptions options;
	mutable Registry<Light, LightID> lights{};
	mutable uint32_t cascadedShadowMapCapacity = 0;
	mutable uint32_t pointLightShadowMapCapacity = 0;
	mutable uint32_t spotLightShadowMapCapacity = 0;
	mutable Texture cascadedShadowMaps{};
	mutable Texture pointLightShadowMaps{};
	mutable Texture spotLightShadowMaps{};
	mutable Buffer<ShadowMatrixFields> shadowMatrices{};
	mutable ParameterBuffer parameterBuffer;
	mutable ShadowMatrixBuffer shadowMatrixBuffer;
	mutable bool shadowMapIndicesDirty = true;
	mutable bool shadowMatrixIndicesDirty = true;
	mutable bool parameterBufferDirty = true;
	mutable bool shadowMatrixBufferDirty = true;
};

} // namespace grem::graphics

#endif
