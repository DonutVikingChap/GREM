// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/shaders.hpp>

#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/graphics/Error.hpp>

#include <cstring> // std::strstr
#include <utility> // std::move
#else
#include <GREM/core/assertions.hpp>
#endif

namespace grem::graphics {

namespace detail {

#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)

namespace {

constexpr CStringView QUATERNION_HEADER_SOURCE_CODE = R"GLSL(
vec4 GREM_getInverseOrientation(vec4 orientation) {
	return vec4(-orientation.xyz, orientation.w);
}

vec3 GREM_rotateVector(vec4 orientation, vec3 v) {
	return v + 2.0 * cross(orientation.xyz, cross(orientation.xyz, v) + orientation.w * v);
}
)GLSL";

constexpr CStringView NUMBERS_HEADER_SOURCE_CODE = R"GLSL(
const float GREM_PI = 3.14159265358979323846;
)GLSL";

constexpr CStringView TONEMAPPING_HEADER_SOURCE_CODE = R"GLSL(
/**
 * Tonemap an HDR color to a normalized range.
 *
 * \param color linear HDR color to convert.
 *
 * \return the tonemapped normalized linear color.
 */
vec3 GREM_tonemap(vec3 color) {
	// PBR Neutral tonemapping. Reference: https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral

	const float F90 = 0.04;
	const float COMPRESSION_THRESHOLD = 0.8 - F90;
	const float DESATURATION_SPEED = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = (x <= 2.0 * F90) ? x - x * x * (1.0 / (4.0 * F90)) : F90;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < COMPRESSION_THRESHOLD) {
		return color;
	}

	const float d = 1.0 - COMPRESSION_THRESHOLD;
	float newPeak = 1.0 - d * d / (peak + d - COMPRESSION_THRESHOLD);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (DESATURATION_SPEED * (peak - newPeak) + 1.0);
	return mix(color, newPeak * vec3(1.0), g);
}
)GLSL";

constexpr CStringView GAMMA_CORRECTION_HEADER_SOURCE_CODE = R"GLSL(
/**
 * Convert a color component in linear space to the sRGB color space.
 *
 * \param x normalized linear color component to convert.
 *
 * \return the converted normalized sRGB color component.
 */
float GREM_convertLinearToSRGB(float x) {
	return (x <= 0.0031308) ? x * 12.92 : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

/**
 * Convert a color in linear space to the sRGB color space.
 *
 * \param rgb normalized linear color to convert.
 *
 * \return the converted normalized sRGB color.
 */
vec3 GREM_convertLinearToSRGB(vec3 rgb) {
	return vec3(
		GREM_convertLinearToSRGB(rgb.r),
		GREM_convertLinearToSRGB(rgb.g),
		GREM_convertLinearToSRGB(rgb.b));
}

/**
 * Convert a color in linear space to the sRGB color space.
 *
 * \param rgba normalized linear color to convert.
 *
 * \return the converted normalized sRGB color.
 */
vec4 GREM_convertLinearToSRGB(vec4 rgba) {
	return vec4(
		GREM_convertLinearToSRGB(rgba.r),
		GREM_convertLinearToSRGB(rgba.g),
		GREM_convertLinearToSRGB(rgba.b),
		rgba.a);
}

/**
 * Convert a color component in the sRGB color space to linear space.
 *
 * \param x normalized sRGB color component to convert.
 *
 * \return the converted normalized linear color component.
 */
float GREM_convertSRGBToLinear(float x) {
	return (x <= 0.04045) ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

/**
 * Convert a color in the sRGB color space to linear space.
 *
 * \param rgb normalized sRGB color to convert.
 *
 * \return the converted normalized linear color.
 */
vec3 GREM_convertSRGBToLinear(vec3 rgb) {
	return vec3(
		GREM_convertSRGBToLinear(rgb.r),
		GREM_convertSRGBToLinear(rgb.g),
		GREM_convertSRGBToLinear(rgb.b));
}

/**
 * Convert a color in the sRGB color space to linear space.
 *
 * \param rgba normalized sRGB color to convert.
 *
 * \return the converted normalized linear color.
 */
vec4 GREM_convertSRGBToLinear(vec4 rgba) {
	return vec4(
		GREM_convertSRGBToLinear(rgba.r),
		GREM_convertSRGBToLinear(rgba.g),
		GREM_convertSRGBToLinear(rgba.b),
		rgba.a);
}
)GLSL";

constexpr CStringView BLENDING_HEADER_SOURCE_CODE = R"GLSL(
/**
 * Convert a color in linear space with straight alpha to pre-multiplied alpha.
 *
 * \param rgba normalized linear color with straight alpha to convert.
 *
 * \return the converted normalized linear color with pre-multiplied alpha.
 */
vec4 GREM_convertStraightToPremultipliedAlpha(vec4 rgba) {
	return vec4(rgba.rgb * rgba.a, rgba.a);
}

/**
 * Convert a color in linear space with pre-multiplied alpha to straight alpha.
 *
 * \param rgba normalized linear color with pre-multiplied alpha to convert.
 *
 * \return the converted normalized linear color with straight alpha.
 */
vec4 GREM_convertPremultipliedToStraightAlpha(vec4 rgba) {
	return vec4((rgba.a > 0.0001) ? rgba.rgb / rgba.a : vec3(0.0), rgba.a);
}

/**
 * Composite two linear colors with straight alpha using the "over" operator.
 *
 * \param a first color to paint.
 * \param b second color to paint over.
 *
 * \return the blended color.
 */
vec4 GREM_blendAOverB(vec4 a, vec4 b) {
	float alpha = a.a + b.a * (1.0 - a.a);
	return vec4((abs(alpha) < 0.0001) ? b.rgb : (a.rgb * a.a + b.rgb * b.a * (1.0 - a.a)) / alpha, alpha);
}
)GLSL";

constexpr CStringView IRRADIANCE_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/numbers.glsl>

vec3 GREM_encodeIrradiance(vec3 irradiance) {
	return pow(irradiance / (2.0 * GREM_PI), vec3(1.0 / 5.0));
}

vec3 GREM_decodeIrradiance(vec3 encodedIrradiance) {
	return (2.0 * GREM_PI) * pow(encodedIrradiance, vec3(5.0));
}
)GLSL";

constexpr CStringView SAMPLING_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/numbers.glsl>

/**
 * Reverse the bits in a 32-bit unsigned integer.
 *
 * \param x unsigned integer value to reverse the bits of.
 *
 * \return the given value with its bits reversed.
 */
uint GREM_getBitReversed(uint x) {
	x = ((x & 0x55555555u) << 1u) | ((x & 0xAAAAAAAAu) >> 1u);
	x = ((x & 0x33333333u) << 2u) | ((x & 0xCCCCCCCCu) >> 2u);
	x = ((x & 0x0F0F0F0Fu) << 4u) | ((x & 0xF0F0F0F0u) >> 4u);
	x = ((x & 0x00FF00FFu) << 8u) | ((x & 0xFF00FF00u) >> 8u);
	x = ((x & 0x0000FFFFu) << 16u) | ((x & 0xFFFF0000u) >> 16u);
	return x;
}

/**
 * Get the i:th value in the Van der Corput radical inverse sequence.
 *
 * \param i index of the value to get.
 *
 * \return the given value with its binary representation mirrored at the radix
 *         point.
 */
float GREM_getVanDerCorputRadicalInverse(uint i) {
	return float(GREM_getBitReversed(i)) * 2.3283064365386963e-10; // Divide by 2^32.
}

/**
 * Get the i:th point in the Hammersley set for a given total number of points,
 * distributed on the unit square.
 *
 * \param i index of the point to get.
 * \param pointCount total number of points in the set.
 *
 * \return the i:th point in the Hammersley set.
 */
vec2 GREM_getHammersleyPoint(uint i, uint pointCount) {
	// Reference: Holger Dammertz: "Hammersley Points on the Hemisphere": http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	return vec2(float(i) / float(pointCount), GREM_getVanDerCorputRadicalInverse(i));
}

/**
 * Convert a 2D sample point on the unit square to a 3D sample direction on the
 * hemisphere, distributed uniformly.
 *
 * \param point uniformly distributed sample point on the unit square to
 *        convert.
 *
 * \return the sample direction in tangent space, where the Z axis is the normal
 *         direction of the hemisphere.
 */
vec3 GREM_getUniformHemisphereSample(vec2 point) {
	float yaw = (2.0 * GREM_PI) * point.x;
	float cosPitch = 1.0 - point.y;
	float sinPitch = sqrt(1.0 - cosPitch * cosPitch);
	return normalize(vec3(cos(yaw) * sinPitch, sin(yaw) * sinPitch, cosPitch));
}

/**
 * Convert a 2D sample point on the unit square to a 3D sample direction on the
 * hemisphere, weighted by the cosine of the angle from the hemisphere's center.
 *
 * \param point uniformly distributed sample point on the unit square to
 *        convert.
 *
 * \return the sample direction in tangent space, where the Z axis is the normal
 *         direction of the hemisphere.
 */
vec3 GREM_getCosineWeightedHemisphereImportanceSample(vec2 point) {
	float yaw = (2.0 * GREM_PI) * point.x;
	float cosPitch = sqrt(1.0 - point.y);
	float sinPitch = sqrt(1.0 - cosPitch * cosPitch);
	return normalize(vec3(cos(yaw) * sinPitch, sin(yaw) * sinPitch, cosPitch));
}

/**
 * Convert a 2D sample point on the unit square to a 3D sample direction on the
 * cosine-weighted hemisphere, weighted towards the center of the hemisphere
 * given the surface's GGX roughness.
 *
 * \param point uniformly distributed sample point on the unit square to
 *        convert.
 * \param roughness surface roughness.
 *
 * \return the sample direction in tangent space, where the Z axis is the normal
 *         direction of the hemisphere.
 */
vec3 GREM_getCosineWeightedHemisphereImportanceSampleGGX(vec2 point, float roughness) {
	float alphaRoughness = roughness * roughness;
	float alphaRoughnessSquared = alphaRoughness * alphaRoughness;
	float yaw = (2.0 * GREM_PI) * point.x;
	float cosPitch = sqrt((1.0 - point.y) / (1.0 + (alphaRoughnessSquared - 1.0) * point.y));
	float sinPitch = sqrt(1.0 - cosPitch * cosPitch);
	return normalize(vec3(cos(yaw) * sinPitch, sin(yaw) * sinPitch, cosPitch));
}

/**
 * Build a tangent space basis matrix from a given normal direction.
 *
 * \param normal direction vector of the tangent space normal. Must be a unit
 *        vector.
 *
 * \return the tangent space basis matrix with its columns in TBN order
 *         (tangent, bitangent, normal).
 */
mat3 GREM_getTangentSpaceBasis(vec3 normal) {
	vec3 up = (abs(normal.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, normal));
	vec3 bitangent = cross(normal, tangent);
	return mat3(tangent, bitangent, normal);
}

/**
 * Convert a direction vector to a point in an octahedral map on the unit
 * square.
 *
 * \param normal direction vector to convert. Must be a unit vector.
 *
 * \return the octahedral map coordinate on the unit square corresponding to the
 *         given vector.
 */
vec2 GREM_getOctahedralMapCoordinatesFromNormal(vec3 normal) {
	float normL1 = abs(normal.x) + abs(normal.y) + abs(normal.z);
	vec2 uv = normal.xy * (1.0 / normL1);
	if (normal.z < 0.0) {
		uv = (vec2(1.0) - abs(uv.yx)) * vec2((uv.x >= 0.0) ? 1.0 : -1.0, (uv.y >= 0.0) ? 1.0 : -1.0);
	}
	return vec2(0.5) + 0.5 * uv;
}

/**
 * Convert a point in an octahedral map on the unit square to a direction
 * vector.
 *
 * \param uv point on the unit square to convert. Must be in the [0, 1] range on
 *        each axis.
 *
 * \return the direction unit vector corresponding to the given point.
 */
vec3 GREM_getNormalFromOctahedralMapCoordinates(vec2 uv) {
	uv = uv * 2.0 - vec2(1.0);
	vec3 v = vec3(uv, 1.0 - abs(uv.x) - abs(uv.y));
	if (v.z < 0.0) {
		v.xy = (vec2(1.0) - abs(v.yx)) * vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
	}
	return normalize(v);
}

/**
 * Get the length of a vector along its major axis.
 *
 * \param v vector to get the major axis length of.
 *
 * \return the largest absolute value of the components of the given vector.
 */
float GREM_getMajorAxisLength(vec3 v) {
	vec3 m = abs(v);
	return max(m.x, max(m.y, m.z));
}

/**
 * Convert a depth value from world space to the clip space of the 90-degree
 * projection of a side of a cubemap.
 *
 * \param worldSpaceDepth world-space depth value to convert.
 * \param nearZ near clip plane depth of the cube side projection.
 * \param farZ far clip plane depth of the cube side projection.
 *
 * \return the converted depth.
 */
float GREM_getCubeSideDepth(float worldSpaceDepth, float nearZ, float farZ) {
	float c1 = farZ / (farZ - nearZ);
	float c0 = -nearZ * c1;
	return (c0 + c1 * worldSpaceDepth) / worldSpaceDepth;
}
)GLSL";

constexpr CStringView MATERIAL_HEADER_SOURCE_CODE = R"GLSL(
struct GREM_Material {
	vec3 albedo;
	float alpha;
	vec3 tangentSpaceNormal;
	float occlusion;
	float roughness;
	float metallic;
	vec3 emissive;
	float alphaCutoff;
	float coverage;
	float indexOfRefraction;
	float dielectricF0;
};
)GLSL";

constexpr CStringView LIGHT_HEADER_SOURCE_CODE = R"GLSL(
#define GREM_AMBIENT_LIGHT 0.0
#define GREM_SUN_LIGHT 1.0
#define GREM_DIRECTIONAL_LIGHT 2.0
#define GREM_POINT_LIGHT 3.0
#define GREM_SPOT_LIGHT 4.0

struct GREM_Light {
	vec3 intensity;
	float type;
	vec3 direction;
	float visibility;
};

GREM_Light GREM_createLight(float type) {
	return GREM_Light(vec3(0.0), type, vec3(0.0), 0.0);
}
)GLSL";

constexpr CStringView PBR_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/light.glsl>
#include <GREM/material.glsl>
#include <GREM/numbers.glsl>

float GREM_getMicrofacetDistribution(float nDotH, float roughness) {
	// Throwbridge-Reitz (GGX) distribution.
	float alphaRoughness = roughness * roughness;
	float alphaRoughnessSquared = alphaRoughness * alphaRoughness;
	float f = nDotH * nDotH * (alphaRoughnessSquared - 1.0) + 1.0;
	return alphaRoughnessSquared / (GREM_PI * f * f);
}

float GREM_getGeometryVisibility(float nDotL, float nDotV, float roughness) {
	// Height-correlated Smith-GGX approximation.
	float alphaRoughness = roughness * roughness;
	float alphaRoughnessSquared = alphaRoughness * alphaRoughness;
    float GGXL = nDotV * sqrt((-nDotL * alphaRoughnessSquared + nDotL) * nDotL + alphaRoughnessSquared);
    float GGXV = nDotL * sqrt((-nDotV * alphaRoughnessSquared + nDotV) * nDotV + alphaRoughnessSquared);
    return 0.5 / max(GGXV + GGXL, 0.00001);
}

float GREM_getGeometryShadowingIBL(float nDotL, float nDotV, float roughness) {
	// Height-correlated Smith-GGX approximation.
	float alphaRoughness = roughness * roughness;
	float alphaRoughnessSquared = alphaRoughness * alphaRoughness;
    float GGXL = nDotV * sqrt((-nDotL * alphaRoughnessSquared + nDotL) * nDotL + alphaRoughnessSquared);
    float GGXV = nDotL * sqrt((-nDotV * alphaRoughnessSquared + nDotV) * nDotV + alphaRoughnessSquared);
    return (2.0 * nDotL) / max(GGXV + GGXL, 0.00001);
}

float GREM_getFresnelReflectance(float f0, float f90, float vDotH) {
	// Schlick's approximation.
	float x = max(1.0 - vDotH, 0.0);
	return f0 + (f90 - f0) * (x * x * x * x * x);
}

vec3 GREM_getFresnelReflectance(vec3 f0, vec3 f90, float vDotH) {
	// Schlick's approximation.
	float x = max(1.0 - vDotH, 0.0);
	return f0 + (f90 - f0) * (x * x * x * x * x);
}

vec3 GREM_getRoughnessDependentFresnelReflectance(vec3 f0, float roughness, float nDotV, vec2 splitSumBRDFIntegration) {
	vec3 f90 = max(vec3(1.0 - roughness), f0);
	vec3 f = GREM_getFresnelReflectance(f0, f90, nDotV);
	vec3 singleScatterSpecular = f * splitSumBRDFIntegration.x + splitSumBRDFIntegration.y;

	float multiScatterEnergyCompensation = 1.0 - splitSumBRDFIntegration.x - splitSumBRDFIntegration.y;
	vec3 fAverage = f0 + (1.0 / 21.0) * (vec3(1.0) - f0);
	vec3 multiScatterSpecular = multiScatterEnergyCompensation * singleScatterSpecular * fAverage / (vec3(1.0) - fAverage * multiScatterEnergyCompensation);

	return singleScatterSpecular + multiScatterSpecular;
}

float GREM_getDiffuseBRDF(float roughness, float nDotV, float nDotL, float lDotH) {
	// Burley diffuse BRDF.
	float alphaRoughness = roughness * roughness;
	float f90 = 0.5 + 2.0 * alphaRoughness * lDotH * lDotH;
	float lightScatter = GREM_getFresnelReflectance(1.0, f90, nDotL);
	float viewScatter = GREM_getFresnelReflectance(1.0, f90, nDotV);
   	return (1.0 / GREM_PI) * lightScatter * viewScatter;
}

float GREM_getSpecularBRDF(float roughness, float nDotL, float nDotV, float nDotH) {
	// Cook-Torrance BRDF.
	float d = GREM_getMicrofacetDistribution(nDotH, roughness);
	float v = GREM_getGeometryVisibility(nDotL, nDotV, roughness);
	return d * v;
}

vec3 GREM_getDirectLightContribution(GREM_Light light, GREM_Material material, vec3 normal, vec3 viewDirection, float nDotV) {
	if (light.type == GREM_AMBIENT_LIGHT) {
		return light.visibility * material.occlusion * material.coverage * material.albedo * light.intensity;
	}

	vec3 halfwayDirection = normalize(light.direction + viewDirection);

	float nDotL = max(dot(normal, light.direction), 0.0);
	float nDotH = max(dot(normal, halfwayDirection), 0.0);
	float lDotH = max(dot(light.direction, halfwayDirection), 0.0);
	float vDotH = max(dot(viewDirection, halfwayDirection), 0.0);

    vec3 diffuse = material.albedo * GREM_getDiffuseBRDF(material.roughness, nDotV, nDotL, lDotH);
	float specular = GREM_getSpecularBRDF(material.roughness, nDotL, nDotV, nDotH);

	vec3 f0 = mix(vec3(material.dielectricF0), material.albedo, material.metallic);
	vec3 f = GREM_getFresnelReflectance(f0, vec3(1.0), vDotH);
	return nDotL * light.visibility * material.coverage * light.intensity * (diffuse * (vec3(1.0) - f) * (1.0 - material.metallic) + specular * f);
}

vec3 GREM_getAmbientLightContribution(vec3 irradiance, vec3 reflection, GREM_Light sunLight, float skyReflectionVisibility, GREM_Material material, vec3 normal, vec2 splitSumBRDFIntegration, vec3 reflectionDirection, float nDotV) {
	float sunNDotL = max(dot(normal, sunLight.direction), 0.0);
	vec3 sunIrradiance = sunNDotL * sunLight.intensity;
	float sunReflectionVisibility = skyReflectionVisibility * clamp(1.03 * dot(reflectionDirection, sunLight.direction), 0.0, 1.0); // Scale up slightly and clamp to try and cover the whole sun with 1.
	float diffuseBrightness = (1.0 / GREM_PI) * length(irradiance);
	float reflectionBrightness = max(length(reflection), 1.0);
	float diffuseReflectionBrightnessRatio = min(diffuseBrightness, reflectionBrightness) / reflectionBrightness;
	float sunDirectVisibility = sunNDotL * sunLight.visibility;
	float sunBlocked = 1.0 - sunLight.visibility;
	irradiance = mix(irradiance, sunIrradiance, sunDirectVisibility); // If we're being lit directly by the sun, use its irradiance instead of the sky's.
	reflection *= mix(1.0, diffuseReflectionBrightnessRatio, sunReflectionVisibility * sunBlocked); // If we're reflecting the sun and it's blocked, scale the reflection brightness to match the diffuse brightness at most.

	vec3 diffuse = (1.0 / GREM_PI) * material.albedo * irradiance;
	vec3 specular = reflection;

	vec3 f0 = mix(vec3(material.dielectricF0), material.albedo, material.metallic);
	vec3 f = GREM_getRoughnessDependentFresnelReflectance(f0, material.roughness, nDotV, splitSumBRDFIntegration);
	return material.occlusion * material.coverage * (diffuse * (vec3(1.0) - f) * (1.0 - material.metallic) + specular * f);
}
)GLSL";

constexpr CStringView MODEL_3D_VERTEX_HEADER_SOURCE_CODE = R"GLSL(
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_POSITION_OFFSET = 0u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_NORMAL_OFFSET = GREM_MODEL_3D_MORPH_TARGET_VERTEX_POSITION_OFFSET + uint(VERTEX_MORPHED_POSITION) * 3u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_TANGENT_OFFSET = GREM_MODEL_3D_MORPH_TARGET_VERTEX_NORMAL_OFFSET + uint(VERTEX_MORPHED_NORMAL) * 3u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_0_OFFSET = GREM_MODEL_3D_MORPH_TARGET_VERTEX_TANGENT_OFFSET + uint(VERTEX_MORPHED_TANGENT) * 3u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_1_OFFSET = GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_0_OFFSET + uint(VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) * 2u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET = GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_1_OFFSET + uint(VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) * 2u;
const uint GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE = GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET + uint(VERTEX_MORPHED_COLOR) * 4u;

vec3 GREM_Model3D_getVertexPosition() {
	vec3 position = vertexPosition;
	if (VERTEX_MORPHED_POSITION) {
		uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
		for (uint i = 0u; i < meshMorphTargetCount; ++i) {
			position += vec3(
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_POSITION_OFFSET + 0u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_POSITION_OFFSET + 1u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_POSITION_OFFSET + 2u)
			) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
		}
	}
	return position;
}

vec3 GREM_Model3D_getVertexNormal() {
	vec3 normal = vertexNormal.xyz;
	if (VERTEX_MORPHED_NORMAL) {
		uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
		for (uint i = 0u; i < meshMorphTargetCount; ++i) {
			normal += vec3(
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_NORMAL_OFFSET + 0u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_NORMAL_OFFSET + 1u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_NORMAL_OFFSET + 2u)
			) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
		}
	}
	return normal;
}

vec4 GREM_Model3D_getVertexTangent() {
	vec4 tangent = vertexTangent;
	if (VERTEX_MORPHED_TANGENT) {
		uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
		for (uint i = 0u; i < meshMorphTargetCount; ++i) {
			tangent.xyz += vec3(
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TANGENT_OFFSET + 0u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TANGENT_OFFSET + 1u),
				morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TANGENT_OFFSET + 2u)
			) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
		}
	}
	return tangent;
}

vec2 GREM_Model3D_getVertexTextureCoordinatesChannel0() {
	vec2 textureCoordinatesChannel0 = vec2(0.0);
	if (VERTEX_TEXTURED_ON_CHANNEL_0) {
		textureCoordinatesChannel0 = vertexTextureCoordinatesChannel0;
		if (VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) {
			uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
			for (uint i = 0u; i < meshMorphTargetCount; ++i) {
				textureCoordinatesChannel0 += vec2(
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_0_OFFSET + 0u),
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_0_OFFSET + 1u)
				) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
			}
		}
	}
	return textureCoordinatesChannel0;
}

vec2 GREM_Model3D_getVertexTextureCoordinatesChannel1() {
	vec2 textureCoordinatesChannel1 = vec2(0.0);
	if (VERTEX_TEXTURED_ON_CHANNEL_1) {
		textureCoordinatesChannel1 = vertexTextureCoordinatesChannel1;
		if (VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) {
			uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
			for (uint i = 0u; i < meshMorphTargetCount; ++i) {
				textureCoordinatesChannel1 += vec2(
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_1_OFFSET + 0u),
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_TEXTURE_COORDINATES_CHANNEL_1_OFFSET + 1u)
				) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
			}
		}
	}
	return textureCoordinatesChannel1;
}

vec4 GREM_Model3D_getVertexTintColor() {
	vec4 color = meshBaseColorFactor * instanceTintColor;
	if (VERTEX_COLORED) {
		vec4 morphedColor = vertexColor;
		if (VERTEX_MORPHED_COLOR) {
			uint morphTargetVertexOffset = meshMorphTargetValueOffset + uint(GREM_vertexIndex) * GREM_MODEL_3D_MORPH_TARGET_VERTEX_STRIDE;
			for (uint i = 0u; i < meshMorphTargetCount; ++i) {
				morphedColor += vec4(
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET + 0u),
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET + 1u),
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET + 2u),
					morphTargetValue(morphTargetVertexOffset + i * meshMorphTargetStride + GREM_MODEL_3D_MORPH_TARGET_VERTEX_COLOR_OFFSET + 3u)
				) * morphTargetWeight(instanceMorphTargetWeightOffset + i);
			}
		}
		color *= morphedColor;
	}
	return color;
}

vec3 GREM_Model3D_getVertexEmissiveColor() {
	return instanceEmissiveColor;
}

vec3 GREM_Model3D_getVertexEmissiveFactor() {
	return meshEmissiveFactor * instanceEmissiveFactor;
}

mat4 GREM_Model3D_getVertexModelMatrix() {
	if (VERTEX_SKINNED) {
		return
			jointMatrix(instanceJointOffset + vertexJointIndices[0]) * inverseBindPoseMatrix(instanceInverseBindPoseMatrixOffset + vertexJointIndices[0]) * vertexJointWeights[0] +
			jointMatrix(instanceJointOffset + vertexJointIndices[1]) * inverseBindPoseMatrix(instanceInverseBindPoseMatrixOffset + vertexJointIndices[1]) * vertexJointWeights[1] +
			jointMatrix(instanceJointOffset + vertexJointIndices[2]) * inverseBindPoseMatrix(instanceInverseBindPoseMatrixOffset + vertexJointIndices[2]) * vertexJointWeights[2] +
			jointMatrix(instanceJointOffset + vertexJointIndices[3]) * inverseBindPoseMatrix(instanceInverseBindPoseMatrixOffset + vertexJointIndices[3]) * vertexJointWeights[3];
	}
	return jointMatrix(instanceJointOffset);
}

mat3 GREM_Model3D_getVertexNormalMatrix(mat4 modelMatrix) {
	return mat3(
		vec3(modelMatrix[0]) / dot(vec3(modelMatrix[0]), vec3(modelMatrix[0])),
		vec3(modelMatrix[1]) / dot(vec3(modelMatrix[1]), vec3(modelMatrix[1])),
		vec3(modelMatrix[2]) / dot(vec3(modelMatrix[2]), vec3(modelMatrix[2])));
}
)GLSL";

constexpr CStringView MODEL_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/material.glsl>

vec4 GREM_Model3D_getMaterialBaseColor() {
	vec4 baseColor = fragmentTintColor;
	if (FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0) {
		baseColor *= GREM_textureSample2D(meshBaseColorMap, meshBaseColorMapTextureOffset + meshBaseColorMapTextureBasis * fragmentTextureCoordinatesChannel0);
	}
	if (FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1) {
		baseColor *= GREM_textureSample2D(meshBaseColorMap, meshBaseColorMapTextureOffset + meshBaseColorMapTextureBasis * fragmentTextureCoordinatesChannel1);
	}
	return baseColor;
}

vec3 GREM_Model3D_getMaterialTangentSpaceNormal() {
	vec3 normal = vec3(0.0, 0.0, 1.0);
	if (FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0) {
		vec3 sampledNormal = GREM_textureSample2D(meshNormalMap, meshNormalMapTextureOffset + meshNormalMapTextureBasis * fragmentTextureCoordinatesChannel0).xyz * 2.0 - vec3(1.0);
		normal = vec3(sampledNormal.xy * meshNormalScale, sampledNormal.z);
	} else if (FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1) {
		vec3 sampledNormal = GREM_textureSample2D(meshNormalMap, meshNormalMapTextureOffset + meshNormalMapTextureBasis * fragmentTextureCoordinatesChannel1).xyz * 2.0 - vec3(1.0);
		normal = vec3(sampledNormal.xy * meshNormalScale, sampledNormal.z);
	}
	return normal;
}

vec3 GREM_Model3D_getMaterialOcclusionRoughnessMetallic() {
	vec3 occlusionRoughnessMetallic = meshOcclusionRoughnessMetallicFactor;
	if (FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0 || FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0) {
		vec3 sampledOcclusionRoughnessMetallic0 = GREM_textureSample2D(meshOcclusionRoughnessMetallicMap, meshOcclusionRoughnessMetallicMapTextureOffset + meshOcclusionRoughnessMetallicMapTextureBasis * fragmentTextureCoordinatesChannel0).rgb;
		if (FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0) {
			occlusionRoughnessMetallic.r = 1.0 + occlusionRoughnessMetallic.r * (sampledOcclusionRoughnessMetallic0.r - 1.0);
		}
		if (FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0) {
			occlusionRoughnessMetallic.gb *= sampledOcclusionRoughnessMetallic0.gb;
		}
	}
	if (FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1 || FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1) {
		vec3 sampledOcclusionRoughnessMetallic1 = GREM_textureSample2D(meshOcclusionRoughnessMetallicMap, meshOcclusionRoughnessMetallicMapTextureOffset + meshOcclusionRoughnessMetallicMapTextureBasis * fragmentTextureCoordinatesChannel1).rgb;
		if (FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1) {
			occlusionRoughnessMetallic.r = 1.0 + occlusionRoughnessMetallic.r * (sampledOcclusionRoughnessMetallic1.r - 1.0);
		}
		if (FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1) {
			occlusionRoughnessMetallic.gb *= sampledOcclusionRoughnessMetallic1.gb;
		}
	}
	return clamp(occlusionRoughnessMetallic, vec3(0.0), vec3(1.0));
}

vec3 GREM_Model3D_getMaterialEmissive() {
	vec3 emissive = fragmentEmissiveFactor;
	if (FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0) {
		emissive *= GREM_textureSample2D(meshEmissiveMap, meshEmissiveMapTextureOffset + meshEmissiveMapTextureBasis * fragmentTextureCoordinatesChannel0).rgb;
	}
	if (FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1) {
		emissive *= GREM_textureSample2D(meshEmissiveMap, meshEmissiveMapTextureOffset + meshEmissiveMapTextureBasis * fragmentTextureCoordinatesChannel1).rgb;
	}
	return (fragmentEmissiveColor + emissive) * fragmentTintColor.a;
}

float GREM_Model3D_getMaterialAlphaCutoff() {
	return meshAlphaCutoff;
}

float GREM_Model3D_getMaterialIndexOfRefraction() {
	return meshIndexOfRefraction;
}

GREM_Material GREM_Model3D_getMaterial() {
	vec4 baseColor = GREM_Model3D_getMaterialBaseColor();
	vec3 tangentSpaceNormal = GREM_Model3D_getMaterialTangentSpaceNormal();
	vec3 occlusionRoughnessMetallic = GREM_Model3D_getMaterialOcclusionRoughnessMetallic();
	vec3 emissive = GREM_Model3D_getMaterialEmissive();
	float alphaCutoff = GREM_Model3D_getMaterialAlphaCutoff();
	float coverage = 1.0;
	if (FRAGMENT_ALPHA_BLENDED) {
		coverage = baseColor.a;
	}
	float indexOfRefraction = GREM_Model3D_getMaterialIndexOfRefraction();
	float sqrtDielectricF0 = (indexOfRefraction - 1.0) / (indexOfRefraction + 1.0);
	float dielectricF0 = sqrtDielectricF0 * sqrtDielectricF0;

	GREM_Material material;
	material.albedo = (coverage < 0.0001) ? vec3(0.0) : baseColor.rgb / coverage;
	material.alpha = baseColor.a;
	material.tangentSpaceNormal = tangentSpaceNormal;
	material.occlusion = occlusionRoughnessMetallic.r;
	material.roughness = occlusionRoughnessMetallic.g;
	material.metallic = occlusionRoughnessMetallic.b;
	material.emissive = emissive;
	material.alphaCutoff = alphaCutoff;
	material.coverage = coverage;
	material.indexOfRefraction = indexOfRefraction;
	material.dielectricF0 = dielectricF0;
	return material;
}
)GLSL";

constexpr CStringView FOG_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
vec3 GREM_Fog3D_blend(vec3 color, float viewDistance) {
	float fogStartDistance = fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines.x;
	float fogEndDistance = fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines.y;
	vec3 fogColor = fogColorAndMaxDensity.xyz;
	float fogMaxDensity = fogColorAndMaxDensity.w;
	float fogDistanceAttenuationFactor = clamp((viewDistance - fogStartDistance) / (fogEndDistance - fogStartDistance), 0.0, 1.0);
	float fogAmount = fogMaxDensity * fogDistanceAttenuationFactor;
	return mix(color, fogColor, fogAmount);
}

vec3 GREM_Fog3D_blendSky(vec3 color, vec3 direction) {
	float fogMinAngleCosine = fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines.z;
	float fogMaxAngleCosine = fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines.w;
	vec3 fogColor = fogColorAndMaxDensity.xyz;
	float fogMaxDensity = fogColorAndMaxDensity.w;
	vec3 fogFadeDirection = fogSkyFadeDirection;
	float fogFadeDirectionAttenuationFactor = (fogMinAngleCosine > fogMaxAngleCosine) ? 1.0 - smoothstep(fogMaxAngleCosine, fogMinAngleCosine, clamp(dot(direction, fogFadeDirection), fogMaxAngleCosine, fogMinAngleCosine)) : 0.0;
	float fogAmount = fogMaxDensity * fogFadeDirectionAttenuationFactor;
	return mix(color, fogColor, fogAmount);
}
)GLSL";

constexpr CStringView SKY_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/numbers.glsl>
#include <GREM/irradiance.glsl>

vec4 GREM_Sky3D_getRadiance(vec3 direction) {
	vec4 skyRadianceSample = GREM_textureSampleCube(skyRadianceMap, direction);
	return skyColor * skyRadianceSample;
}

vec4 GREM_Sky3D_getIrradiance(vec3 normal) {
	vec4 skyIrradianceSample = GREM_textureSampleCube(skyIrradianceMap, normal);
	return skyAmbientColor * skyColor * vec4(GREM_decodeIrradiance(skyIrradianceSample.rgb), 1.0);
}

vec4 GREM_Sky3D_getReflection(float roughness, vec3 reflectionDirection) {
	float skyReflectionDetailLevel = min(roughness * skyReflectionMapDetailLevelScale, skyReflectionMapDetailLevelMax);
	return skyReflectionColor * skyColor * GREM_textureSampleLodCube(skyReflectionMap, reflectionDirection, skyReflectionDetailLevel);
}
)GLSL";

constexpr CStringView DECALS_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/blending.glsl>
#include <GREM/material.glsl>

void GREM_Decals3D_applyDecalToMaterial(inout GREM_Material material, uint decalIndex, vec3 samplePosition, vec3 rawNormal, uint instanceIdentifier) {	
	mat4 matrix = decalMatrix(decalIndex);
	vec3 decalCoordinates = (matrix * vec4(samplePosition, 1.0)).xyz;
	vec3 direction = decalDirectionAndRange(decalIndex).xyz;
	uint modelInstanceIdentifier = decalModelInstanceIdentifier(decalIndex);

	vec4 baseColorTextureOffsetAndScale = decalBaseColorTextureOffsetAndScale(decalIndex);
	vec4 normalTextureOffsetAndScale = decalNormalTextureOffsetAndScale(decalIndex);
	vec4 occlusionRoughnessMetallicTextureOffsetAndScale = decalOcclusionRoughnessMetallicTextureOffsetAndScale(decalIndex);
	vec4 emissiveTextureOffsetAndScale = decalEmissiveTextureOffsetAndScale(decalIndex);

	vec2 baseColorTextureCoordinates = baseColorTextureOffsetAndScale.xy + decalCoordinates.xy * baseColorTextureOffsetAndScale.zw;
	vec2 normalTextureCoordinates = normalTextureOffsetAndScale.xy + decalCoordinates.xy * normalTextureOffsetAndScale.zw;
	vec2 occlusionRoughnessMetallicTextureCoordinates = occlusionRoughnessMetallicTextureOffsetAndScale.xy + decalCoordinates.xy * occlusionRoughnessMetallicTextureOffsetAndScale.zw;
	vec2 emissiveTextureCoordinates = emissiveTextureOffsetAndScale.xy + decalCoordinates.xy * emissiveTextureOffsetAndScale.zw;

	vec4 sampledBaseColor = GREM_textureSample2D(decalsBaseColorAtlasTexture, baseColorTextureCoordinates);
	vec3 sampledNormal = GREM_textureSample2D(decalsNormalAtlasTexture, normalTextureCoordinates).xyz * 2.0 - vec3(1.0);
	vec3 sampledOcclusionRoughnessMetallic = GREM_textureSample2D(decalsOcclusionRoughnessMetallicAtlasTexture, occlusionRoughnessMetallicTextureCoordinates).xyz;
	vec3 sampledEmissive = GREM_textureSample2D(decalsEmissiveAtlasTexture, emissiveTextureCoordinates).rgb;

	vec4 baseColorFactor = decalBaseColorFactor(decalIndex);
	vec4 occlusionRoughnessMetallicFactorAndNormalScale = decalOcclusionRoughnessMetallicFactorAndNormalScale(decalIndex);
	vec3 emissiveFactor = decalEmissiveFactor(decalIndex);

	vec4 decalBaseColor = baseColorFactor * sampledBaseColor;
	vec3 decalTangentSpaceNormal = vec3(sampledNormal.xy * occlusionRoughnessMetallicFactorAndNormalScale.w, sampledNormal.z);
	vec3 decalOcclusionRoughnessMetallic = clamp(occlusionRoughnessMetallicFactorAndNormalScale.xyz * sampledOcclusionRoughnessMetallic, vec3(0.0), vec3(1.0));
	vec3 decalEmissive = emissiveFactor * sampledEmissive;

	float instanceAttenuation = float(int(modelInstanceIdentifier == 0xFFFFFFFFu) | int(modelInstanceIdentifier == instanceIdentifier));
	float rangeAttenuation = float(int(decalCoordinates.x >= 0.0) & int(decalCoordinates.y >= 0.0) & int(decalCoordinates.x < 1.0) & int(decalCoordinates.y < 1.0)) * smoothstep(0.0, 0.1, decalCoordinates.z) * smoothstep(0.0, 0.1, 1.0 - decalCoordinates.z);
	float angleAttenuation = smoothstep(0.4, 0.5, -dot(rawNormal, direction));
	float attenuation = instanceAttenuation * rangeAttenuation * angleAttenuation;

	float decalCoverage = decalBaseColor.a * attenuation;
	if (decalCoverage > 0.0001) {
		float newCoverage = decalCoverage + material.coverage * (1.0 - decalCoverage);
		vec3 blendedAlbedo = (decalBaseColor.rgb * attenuation + material.albedo * material.coverage * (1.0 - decalCoverage)) / newCoverage;
		vec3 blendedTangentSpaceNormal = (decalTangentSpaceNormal * decalCoverage + material.tangentSpaceNormal * material.coverage * (1.0 - decalCoverage)) / newCoverage;
		vec3 blendedOcclusionRoughnessMetallic = (decalOcclusionRoughnessMetallic * decalCoverage + vec3(material.occlusion, material.roughness, material.metallic) * material.coverage * (1.0 - decalCoverage)) / newCoverage;
		vec3 blendedEmissive = (decalEmissive.rgb * decalCoverage + material.emissive * (1.0 - decalCoverage)) / newCoverage;

		material.albedo = blendedAlbedo;
		material.tangentSpaceNormal = blendedTangentSpaceNormal;
		material.occlusion = blendedOcclusionRoughnessMetallic.x;
		material.roughness = blendedOcclusionRoughnessMetallic.y;
		material.metallic = blendedOcclusionRoughnessMetallic.z;
		material.emissive = blendedEmissive;
		material.coverage = newCoverage;
	}
}
)GLSL";

constexpr CStringView LIGHTS_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/light.glsl>
#include <GREM/sampling.glsl>

#define GREM_PCF_FILTER_SAMPLE_COUNT 8u
const vec2 GREM_PCF_FILTER_SAMPLE_OFFSETS[GREM_PCF_FILTER_SAMPLE_COUNT] = vec2[](
	vec2(-0.0279,  0.9739),
	vec2(-0.7331,  0.6575),
	vec2( 0.8022,  0.5688),
	vec2(-0.9742, -0.1174),
	vec2(-0.3939, -0.8965),
	vec2( 0.0047, -0.0463),
	vec2( 0.9500, -0.2157),
	vec2( 0.5000, -0.8457));

float GREM_Lights3D_pcfFilterCascadedShadowMap(float shadowMapIndex, vec3 uvz, float layer, float filterRadius) {
	vec2 texelSize = vec2(1.0) / vec2(textureSize(screenCascadedShadowMaps, 0).xy);
	float shadowVisibility = 0.0;
	for (uint sampleIndex = 0u; sampleIndex < GREM_PCF_FILTER_SAMPLE_COUNT; ++sampleIndex) {
		vec2 sampleOffset = GREM_PCF_FILTER_SAMPLE_OFFSETS[sampleIndex] * texelSize * filterRadius;
		shadowVisibility += GREM_textureSample2DArrayShadow(screenCascadedShadowMaps, vec4(uvz.xy + sampleOffset, shadowMapIndex + layer, uvz.z));
	}
	return shadowVisibility * (1.0 / float(GREM_PCF_FILTER_SAMPLE_COUNT));
}

float GREM_Lights3D_pcfFilterSpotLightShadowMap(float shadowMapIndex, vec3 uvz, float filterRadius) {
	vec2 texelSize = vec2(1.0) / vec2(textureSize(screenSpotLightShadowMaps, 0).xy);
	float shadowVisibility = 0.0;
	for (uint sampleIndex = 0u; sampleIndex < GREM_PCF_FILTER_SAMPLE_COUNT; ++sampleIndex) {
		vec2 sampleOffset = GREM_PCF_FILTER_SAMPLE_OFFSETS[sampleIndex] * texelSize * filterRadius;
		shadowVisibility += GREM_textureSample2DArrayShadow(screenSpotLightShadowMaps, vec4(uvz.xy + sampleOffset, shadowMapIndex, uvz.z));
	}
	return shadowVisibility * (1.0 / float(GREM_PCF_FILTER_SAMPLE_COUNT));
}

vec3 GREM_Lights3D_getFragmentPositionInLightSpaceOrtho(mat4 lightShadowMatrix, vec3 biasedFragmentPosition) {
	return vec3(lightShadowMatrix * vec4(biasedFragmentPosition, 1.0));
}

vec3 GREM_Lights3D_getFragmentPositionInLightSpacePerspective(mat4 lightShadowMatrix, vec3 biasedFragmentPosition) {
	vec4 biasedFragmentPositionInLightSpace = lightShadowMatrix * vec4(biasedFragmentPosition, 1.0);
	return biasedFragmentPositionInLightSpace.xyz / biasedFragmentPositionInLightSpace.w;
}

float GREM_Lights3D_getCascadedShadowVisibility(float shadowMapIndex, float shadowMatrixIndex, vec3 samplePosition, float sampleDepth, vec3 rawNormal, vec3 lightDirection, float normalOffsetBiasConstantFactor, float normalOffsetBiasSlopeFactor) {
	uint cascadeLevelA = 0u;
	uint cascadeLevelMax = lightsShadowCascadeCount - 1u;
	for (uint cascadeLevel = 0u; cascadeLevel < cascadeLevelMax; ++cascadeLevel) {
		cascadeLevelA += uint(sampleDepth > lightsShadowCascadeFrustumFarPlaneDistances[cascadeLevel]);
	}
	uint cascadeLevelB = min(cascadeLevelA + 1u, cascadeLevelMax);
	float cascadeDistanceA = lightsShadowCascadeFrustumFarPlaneDistances[cascadeLevelA];
	float cascadeBlendNear = cascadeDistanceA - lightsShadowCascadeBlendSize;
	float cascadeBlendFar = cascadeDistanceA;
	float cascadeLevelInterpolationAlpha = (sampleDepth - cascadeBlendNear) / (cascadeBlendFar - cascadeBlendNear);
	float layerA = float(cascadeLevelA);
	float layerB = float(cascadeLevelB);
	float filterRadiusA = 2.0 / (1.0 + layerA);
	float filterRadiusB = 2.0 / (1.0 + layerB);
	mat4 lightShadowMatrixA = shadowMatrix(shadowMatrixIndex + layerA);
	mat4 lightShadowMatrixB = shadowMatrix(shadowMatrixIndex + layerB);
	float texelSize = 1.0 / float(textureSize(screenCascadedShadowMaps, 0).x);
	float viewWidthA = 1.0 / abs(lightShadowMatrixA[0].x);
	float viewWidthB = 1.0 / abs(lightShadowMatrixB[0].x);
	float worldSpacePCFFilterKernelSizeA = texelSize * viewWidthA;
	float worldSpacePCFFilterKernelSizeB = texelSize * viewWidthB;
	float slopeScaleBias = 1.0 - max(dot(rawNormal, lightDirection), 0.0);
	vec3 biasedFragmentPositionA = samplePosition + rawNormal * (normalOffsetBiasConstantFactor + worldSpacePCFFilterKernelSizeA * normalOffsetBiasSlopeFactor * slopeScaleBias);
	if (cascadeLevelInterpolationAlpha < 0.0) {
		vec3 projectedCoordinates = GREM_Lights3D_getFragmentPositionInLightSpaceOrtho(lightShadowMatrixA, biasedFragmentPositionA);
		return GREM_Lights3D_pcfFilterCascadedShadowMap(shadowMapIndex, projectedCoordinates, layerA, filterRadiusA);
	}
	if (cascadeLevelInterpolationAlpha > 1.0) {
		return 1.0;
	}
	vec3 biasedFragmentPositionB = samplePosition + rawNormal * (normalOffsetBiasConstantFactor + worldSpacePCFFilterKernelSizeB * normalOffsetBiasSlopeFactor * slopeScaleBias);
	vec3 projectedCoordinatesA = GREM_Lights3D_getFragmentPositionInLightSpaceOrtho(lightShadowMatrixA, biasedFragmentPositionA);
	vec3 projectedCoordinatesB = GREM_Lights3D_getFragmentPositionInLightSpaceOrtho(lightShadowMatrixB, biasedFragmentPositionB);
	float shadowVisibilityA = GREM_Lights3D_pcfFilterCascadedShadowMap(shadowMapIndex, projectedCoordinatesA, layerA, filterRadiusA);
	float shadowVisibilityB = GREM_Lights3D_pcfFilterCascadedShadowMap(shadowMapIndex, projectedCoordinatesB, layerB, filterRadiusB);
	return mix(shadowVisibilityA, shadowVisibilityB, cascadeLevelInterpolationAlpha);
}

float GREM_Lights3D_getPointLightShadowVisibility(float shadowMapIndex, vec3 lightPosition, float lightRange, vec3 samplePosition, vec3 rawNormal, vec3 lightDirection, float nearPlaneDistance, float farPlaneDistance, float normalOffsetBiasConstantFactor, float normalOffsetBiasSlopeFactor) {
	float texelSize = 1.0 / float(textureSize(screenPointLightShadowMaps, 0).x);
	float farZ = (lightRange > 0.0) ? min(farPlaneDistance, lightRange) : farPlaneDistance;
	float nearZ = min(nearPlaneDistance, farZ);
	float sampleDepthInWorldSpace = GREM_getMajorAxisLength(lightPosition - samplePosition);
	float worldSpacePCFFilterKernelSize = 2.0 * texelSize * sampleDepthInWorldSpace;
	float slopeScaleBias = 1.0 - max(dot(rawNormal, lightDirection), 0.0);
	vec3 biasedFragmentPosition = samplePosition + rawNormal * (normalOffsetBiasConstantFactor + worldSpacePCFFilterKernelSize * normalOffsetBiasSlopeFactor * slopeScaleBias);
	vec3 biasedLightVector = lightPosition - biasedFragmentPosition;
	float biasedFragmentDepthInLightSpace = GREM_getCubeSideDepth(GREM_getMajorAxisLength(biasedLightVector), nearZ, farZ);
	return GREM_textureSampleCubeArrayShadow(screenPointLightShadowMaps, vec4(-biasedLightVector, shadowMapIndex), biasedFragmentDepthInLightSpace);
}

float GREM_Lights3D_getSpotLightShadowVisibility(float shadowMapIndex, float shadowMatrixIndex, float lightRange, vec3 samplePosition, vec3 rawNormal, vec3 lightDirection, float lightDistance, float lightOuterConeCosine, float normalOffsetBiasConstantFactor, float normalOffsetBiasSlopeFactor) {
	const float FILTER_RADIUS = 1.0;
	float texelSize = 1.0 / float(textureSize(screenSpotLightShadowMaps, 0).x);
	float lightOuterConeTangent = sqrt(1.0 - lightOuterConeCosine * lightOuterConeCosine) / lightOuterConeCosine;
	float worldSpacePCFFilterKernelSize = 2.0 * FILTER_RADIUS * texelSize * lightDistance * lightOuterConeTangent;
	float slopeScaleBias = 1.0 - max(dot(rawNormal, lightDirection), 0.0);
	vec3 biasedFragmentPosition = samplePosition + rawNormal * (normalOffsetBiasConstantFactor + worldSpacePCFFilterKernelSize * normalOffsetBiasSlopeFactor * slopeScaleBias);
	vec3 projectedCoordinates = GREM_Lights3D_getFragmentPositionInLightSpacePerspective(shadowMatrix(shadowMatrixIndex), biasedFragmentPosition);
	return GREM_Lights3D_pcfFilterSpotLightShadowMap(shadowMapIndex, projectedCoordinates, FILTER_RADIUS);
}

GREM_Light GREM_Lights3D_getLight(uint lightIndex, vec3 samplePosition, float sampleDepth, vec3 rawNormal, vec3 viewDirection) {
	vec4 directionAndRange = lightDirectionAndRange(lightIndex);
	vec4 colorAndIntensity = lightColorAndIntensity(lightIndex);
	vec4 positionAndType = lightPositionAndType(lightIndex);
	vec4 coneCosinesAndShadowMapIndexAndShadowMatrixIndex = lightConeCosinesAndShadowMapIndexAndShadowMatrixIndex(lightIndex);
	vec4 shadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors = lightShadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors(lightIndex);

	vec3 lightPosition = positionAndType.xyz;
	float lightType = positionAndType.w;
	vec3 lightVector = (lightType == GREM_AMBIENT_LIGHT) ? viewDirection : (lightType <= GREM_DIRECTIONAL_LIGHT) ? -directionAndRange.xyz : lightPosition - samplePosition;
	float lightDistanceSquared = dot(lightVector, lightVector);
	float lightDistance = sqrt(lightDistanceSquared);
	vec3 lightDirection = lightVector / lightDistance;

	float lightRange = directionAndRange.w;
	float lightInnerConeCosine = coneCosinesAndShadowMapIndexAndShadowMatrixIndex.x;
	float lightOuterConeCosine = coneCosinesAndShadowMapIndexAndShadowMatrixIndex.y;
	float rangeAttenuationFactor = (lightType <= GREM_DIRECTIONAL_LIGHT) ? 1.0 : (lightRange <= 0.0) ? 1.0 / lightDistanceSquared : clamp(1.0 - pow(lightDistance / lightRange, 4.0), 0.0, 1.0) / lightDistanceSquared;
	float coneAttenuationFactor = (lightType == GREM_SPOT_LIGHT) ? smoothstep(lightOuterConeCosine, lightInnerConeCosine, dot(normalize(directionAndRange.xyz), -lightDirection)) : 1.0;
	float shadowNearPlaneDistance = shadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors.x;
	float shadowFarPlaneDistance = shadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors.y;
	float shadowMapNormalOffsetBiasConstantFactor = shadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors.z;
	float shadowMapNormalOffsetBiasSlopeFactor = shadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors.w;

	vec3 lightColor = colorAndIntensity.rgb;
	float lightIntensity = colorAndIntensity.a;

	float lightShadowVisibility = 1.0;
	float lightShadowMapIndex = coneCosinesAndShadowMapIndexAndShadowMatrixIndex.z;
	float lightShadowMatrixIndex = coneCosinesAndShadowMapIndexAndShadowMatrixIndex.w;
	if (lightShadowMapIndex >= 0.0) {
		switch (uint(lightType)) {
			case uint(GREM_AMBIENT_LIGHT): break;
			case uint(GREM_SUN_LIGHT): // Fallthrough.
			case uint(GREM_DIRECTIONAL_LIGHT): lightShadowVisibility = GREM_Lights3D_getCascadedShadowVisibility(lightShadowMapIndex, lightShadowMatrixIndex, samplePosition, sampleDepth, rawNormal, lightDirection, shadowMapNormalOffsetBiasConstantFactor, shadowMapNormalOffsetBiasSlopeFactor); break;
			case uint(GREM_POINT_LIGHT): lightShadowVisibility = GREM_Lights3D_getPointLightShadowVisibility(lightShadowMapIndex, lightPosition, lightRange, samplePosition, rawNormal, lightDirection, shadowNearPlaneDistance, shadowFarPlaneDistance, shadowMapNormalOffsetBiasConstantFactor, shadowMapNormalOffsetBiasSlopeFactor); break;
			case uint(GREM_SPOT_LIGHT): lightShadowVisibility = GREM_Lights3D_getSpotLightShadowVisibility(lightShadowMapIndex, lightShadowMatrixIndex, lightRange, samplePosition, rawNormal, lightDirection, lightDistance, lightOuterConeCosine, shadowMapNormalOffsetBiasConstantFactor, shadowMapNormalOffsetBiasSlopeFactor); break;
		}
	}

	GREM_Light light;
	light.intensity = rangeAttenuationFactor * coneAttenuationFactor * lightColor * lightIntensity;
	light.type = lightType;
	light.direction = lightDirection;
	light.visibility = lightShadowVisibility;
	return light;
}
)GLSL";

constexpr CStringView LIGHT_PROBE_VOLUMES_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/quaternion.glsl>
#include <GREM/numbers.glsl>
#include <GREM/irradiance.glsl>
#include <GREM/sampling.glsl>

vec4 GREM_LightProbeVolumes3D_getLightProbeVolumeIrradiance(uint lightProbeVolumeIndex, vec3 samplePosition, vec3 rawNormal, vec3 viewDirection) {
	vec3 center = lightProbeVolumeCenter(lightProbeVolumeIndex);
	vec4 orientation = lightProbeVolumeOrientation(lightProbeVolumeIndex);
	vec3 probeSpacing = lightProbeVolumeProbeSpacing(lightProbeVolumeIndex);
	vec3 probeCounts = lightProbeVolumeProbeCounts(lightProbeVolumeIndex);
	vec3 irradianceAtlasOffset = lightProbeVolumeIrradianceAtlasOffset(lightProbeVolumeIndex);
	vec3 distanceAtlasOffset = lightProbeVolumeDistanceAtlasOffset(lightProbeVolumeIndex);
	vec2 irradianceAtlasPaddedProbeSizeAndTexelSize = lightProbeVolumeIrradianceAtlasPaddedProbeSizeAndTexelSize(lightProbeVolumeIndex);
	vec2 distanceAtlasPaddedProbeSizeAndTexelSize = lightProbeVolumeDistanceAtlasPaddedProbeSizeAndTexelSize(lightProbeVolumeIndex);

	vec3 probeBias = (rawNormal * 0.2 + viewDirection * 0.8) * min(min(probeSpacing.x, probeSpacing.y), probeSpacing.z) * 0.75 * 0.3;
	vec3 halfExtents = probeCounts * probeSpacing * 0.5;
	vec3 inverseProbeSpacing = 1.0 / probeSpacing;
	vec3 localFragmentOffset = GREM_rotateVector(GREM_getInverseOrientation(orientation), samplePosition - center);
	vec3 gridPosition = (halfExtents + localFragmentOffset) * inverseProbeSpacing - vec3(0.5);
	vec3 gridPositionBase = floor(gridPosition);
	ivec3 gridIndices = ivec3(gridPositionBase);
	ivec3 gridIndicesMax = ivec3(probeCounts) - ivec3(1);
	vec3 gridPositionOffset = gridPosition - gridPositionBase;

	float probeWeights[8];
	for (int i = 0; i < 8; ++i) {
		ivec3 gridOffset = ivec3(i & 1, (i >> 2) & 1, (i >> 1) & 1);
		vec3 probeGridIndices = vec3(clamp(gridIndices + gridOffset, ivec3(0), gridIndicesMax));
		vec3 probePosition = center + GREM_rotateVector(orientation, (probeGridIndices + vec3(0.5)) * probeSpacing - halfExtents);
	
		vec3 sampleVector = samplePosition - probePosition;
		float sampleDistance = length(sampleVector);
		vec3 sampleDirection = (sampleDistance > 0.0001) ? sampleVector / sampleDistance : rawNormal;

		vec3 biasedFragmentPosition = samplePosition + probeBias;
		vec3 biasedFragmentVector = biasedFragmentPosition - probePosition;
		float biasedFragmentDistance = length(biasedFragmentVector);
		vec3 biasedFragmentDirection = (biasedFragmentDistance > 0.0001) ? biasedFragmentVector / biasedFragmentDistance : rawNormal;

		float slopeWeight = 0.5 - 0.5 * dot(sampleDirection, rawNormal);
		slopeWeight = slopeWeight * slopeWeight;

		vec2 distanceMapCoordinates = distanceAtlasPaddedProbeSizeAndTexelSize.y + GREM_getOctahedralMapCoordinatesFromNormal(biasedFragmentDirection) * (distanceAtlasPaddedProbeSizeAndTexelSize.x - distanceAtlasPaddedProbeSizeAndTexelSize.y * 2.0);
		vec2 distanceAtlasCoordinates = distanceAtlasOffset.xy + probeGridIndices.xz * distanceAtlasPaddedProbeSizeAndTexelSize.x + distanceMapCoordinates;
		vec4 distanceSample = GREM_textureSampleGrad2DArray(lightProbeVolumesDistanceAtlasTexture, vec3(distanceAtlasCoordinates, distanceAtlasOffset.z + probeGridIndices.y), vec2(0.0), vec2(0.0));
		float distanceMean = distanceSample.x;
		float distanceVariance = abs(distanceMean * distanceMean - distanceSample.y);

		float occlusionDepth = biasedFragmentDistance - distanceMean;
		float occlusionWeight = (occlusionDepth > 0.0) ? distanceVariance / (distanceVariance + occlusionDepth * occlusionDepth) : 1.0; // One-sided Chebyshev/Cantelli's inequality.
		occlusionWeight = occlusionWeight * occlusionWeight * occlusionWeight;

		vec3 trilinearWeights = mix(vec3(1.0) - gridPositionOffset, gridPositionOffset, vec3(gridOffset));
		float trilinearWeight = trilinearWeights.x * trilinearWeights.y * trilinearWeights.z;

		probeWeights[i] = max(slopeWeight * occlusionWeight, 0.0001) * trilinearWeight;
	}

	vec3 irradianceSum = vec3(0.0);
	float weightSum = 0.0;
	for (int i = 0; i < 8; ++i) {
		ivec3 gridOffset = ivec3(i & 1, (i >> 2) & 1, (i >> 1) & 1);
		vec3 probeGridIndices = vec3(clamp(gridIndices + gridOffset, ivec3(0), gridIndicesMax));
	
		vec2 irradianceMapCoordinates = irradianceAtlasPaddedProbeSizeAndTexelSize.y + GREM_getOctahedralMapCoordinatesFromNormal(rawNormal) * (irradianceAtlasPaddedProbeSizeAndTexelSize.x - irradianceAtlasPaddedProbeSizeAndTexelSize.y * 2.0);
		vec2 irradianceAtlasCoordinates = irradianceAtlasOffset.xy + probeGridIndices.xz * irradianceAtlasPaddedProbeSizeAndTexelSize.x + irradianceMapCoordinates;
		vec4 irradianceSample = GREM_textureSampleGrad2DArray(lightProbeVolumesIrradianceAtlasTexture, vec3(irradianceAtlasCoordinates, irradianceAtlasOffset.z + probeGridIndices.y), vec2(0.0), vec2(0.0));
		vec3 irradiance = GREM_decodeIrradiance(irradianceSample.rgb);

		float weight = probeWeights[i];
		irradianceSum += irradiance * weight;
		weightSum += weight;
	}

	vec3 irradiance = irradianceSum * (1.0 / weightSum);

	vec3 blendWeights = vec3(1.0) - clamp((abs(localFragmentOffset) - halfExtents) * inverseProbeSpacing, vec3(0.0), vec3(1.0));
	float blendWeight = min(min(blendWeights.x, blendWeights.y), blendWeights.z);

	return vec4(irradiance, blendWeight);
}
)GLSL";

constexpr CStringView REFLECTION_PROBES_3D_FRAGMENT_HEADER_SOURCE_CODE = R"GLSL(
#include <GREM/quaternion.glsl>

vec4 GREM_ReflectionProbes3D_getReflectionProbeReflection(uint reflectionProbeIndex, vec3 samplePosition, float roughness, vec3 reflectionDirection) {
	vec3 center = reflectionProbeCenter(reflectionProbeIndex);
	vec4 orientation = reflectionProbeOrientation(reflectionProbeIndex);
	vec3 size = reflectionProbeSize(reflectionProbeIndex);
	vec3 localAffectedRegionOffset = reflectionProbeLocalAffectedRegionOffset(reflectionProbeIndex);
	vec3 localAffectedRegionSize = reflectionProbeLocalAffectedRegionSize(reflectionProbeIndex);
	vec3 blendWidthsOnNegativeSides = reflectionProbeBlendWidthsOnNegativeSides(reflectionProbeIndex);
	vec3 blendWidthsOnPositiveSides = reflectionProbeBlendWidthsOnPositiveSides(reflectionProbeIndex);
	vec3 captureOffset = reflectionProbeCaptureOffset(reflectionProbeIndex);

	vec3 halfExtents = size * 0.5;
	vec3 inverseHalfExtents = 1.0 / halfExtents;
	vec3 localFragmentOffset = GREM_rotateVector(GREM_getInverseOrientation(orientation), samplePosition - center);
	vec3 cubeSpaceFragmentPosition = localFragmentOffset * inverseHalfExtents;
	vec3 cubeSpaceReflectionDirection = GREM_rotateVector(GREM_getInverseOrientation(orientation), reflectionDirection) * inverseHalfExtents;
	vec3 inverseCubeSpaceReflectionDirection = 1.0 / cubeSpaceReflectionDirection;
	vec3 planeA = (vec3(-1.0) - cubeSpaceFragmentPosition) * inverseCubeSpaceReflectionDirection;
	vec3 planeB = (vec3(1.0) - cubeSpaceFragmentPosition) * inverseCubeSpaceReflectionDirection;
	vec3 maxPlane = max(planeA, planeB);
	vec3 boxIntersectionPoint = samplePosition + reflectionDirection * min(min(maxPlane.x, maxPlane.y), maxPlane.z);
	vec3 reflectionSampleVector = boxIntersectionPoint - (center + captureOffset);
	float reflectionDetailLevel = min(roughness * reflectionProbesReflectionMapDetailLevelScale, reflectionProbesReflectionMapDetailLevelMax);
	vec4 reflectionColor = GREM_textureSampleLodCubeArrayFiltered(reflectionProbesReflectionMaps, vec4(reflectionSampleVector, float(reflectionProbeIndex)), reflectionDetailLevel);

	vec3 localFragmentOffsetFromAffectedRegionCenter = localFragmentOffset - localAffectedRegionOffset;
	vec3 affectedRegionHalfExtents = localAffectedRegionSize * 0.5;
	vec3 blendWidths = mix(blendWidthsOnNegativeSides, blendWidthsOnPositiveSides, step(vec3(0.0), localFragmentOffsetFromAffectedRegionCenter));
	vec3 blendWeights = vec3(1.0) - clamp((abs(localFragmentOffsetFromAffectedRegionCenter) - (affectedRegionHalfExtents - blendWidths)) / (blendWidths + vec3(0.0001)), vec3(0.0), vec3(1.0));
	float blendWeight = min(min(blendWeights.x, blendWeights.y), blendWeights.z);

	return vec4(reflectionColor.rgb, reflectionColor.a * blendWeight);
}
)GLSL";

[[nodiscard]] CStringView findBuiltinShaderHeaderSourceCode(StringView filepath) {
	constexpr StringView BUILTIN_SHADER_HEADER_PATH_PREFIX = "GREM/";
	if (!filepath.starts_with(BUILTIN_SHADER_HEADER_PATH_PREFIX)) {
		return {};
	}
	filepath.remove_prefix(BUILTIN_SHADER_HEADER_PATH_PREFIX.size());
	static const HashMap<StringView, CStringView> builtinShaderHeaderMap{
		{"quaternion.glsl", QUATERNION_HEADER_SOURCE_CODE},
		{"numbers.glsl", NUMBERS_HEADER_SOURCE_CODE},
		{"tonemapping.glsl", TONEMAPPING_HEADER_SOURCE_CODE},
		{"gamma_correction.glsl", GAMMA_CORRECTION_HEADER_SOURCE_CODE},
		{"blending.glsl", BLENDING_HEADER_SOURCE_CODE},
		{"irradiance.glsl", IRRADIANCE_HEADER_SOURCE_CODE},
		{"sampling.glsl", SAMPLING_HEADER_SOURCE_CODE},
		{"material.glsl", MATERIAL_HEADER_SOURCE_CODE},
		{"light.glsl", LIGHT_HEADER_SOURCE_CODE},
		{"pbr.glsl", PBR_HEADER_SOURCE_CODE},
		{"Model3D/vertex.glsl", MODEL_3D_VERTEX_HEADER_SOURCE_CODE},
		{"Model3D/fragment.glsl", MODEL_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"Fog3D/fragment.glsl", FOG_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"Sky3D/fragment.glsl", SKY_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"Decals3D/fragment.glsl", DECALS_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"Lights3D/fragment.glsl", LIGHTS_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"LightProbeVolumes3D/fragment.glsl", LIGHT_PROBE_VOLUMES_3D_FRAGMENT_HEADER_SOURCE_CODE},
		{"ReflectionProbes3D/fragment.glsl", REFLECTION_PROBES_3D_FRAGMENT_HEADER_SOURCE_CODE},
	};
	if (const auto it = builtinShaderHeaderMap.find(filepath); it != builtinShaderHeaderMap.end()) {
		return it->second;
	}
	return {};
}

struct TransparentStringHash {
	using is_transparent = void;

	[[nodiscard]] size_t operator()(StringView string) const {
		return getHash(string);
	}
};

struct TransparentStringEqual {
	using is_transparent = void;

	[[nodiscard]] bool operator()(StringView a, StringView b) const {
		return a == b;
	}
};

using IncludedFilepathSet = HashSet<String, TransparentStringHash, TransparentStringEqual>;

size_t expandShaderIncludes(ExpandedStringBuffer& output, AllocatedStringBuffer& allocatedStrings, IncludedFilepathSet& includedFilepaths, const Filesystem* filesystem,
	StringView filepath, const char* input) {
	const auto allocateString = [&](StringView string) -> const char* {
		Allocation<char> allocatedString(string.size() + 1);
		char* const result = allocatedString.data();
		memcpy(result, string.data(), string.size());
		result[string.size()] = '\0';
		allocatedStrings.push_back(std::move(allocatedString));
		return result;
	};

	const auto pushOutputString = [&](StringView string) -> void {
		if (!string.empty()) {
			output.push_back(allocateString(string));
		}
	};

	size_t lineNumber = 1;
	bool lineContentsStarted = false;

	const char* p = input;
	const char* begin = p;
	while (true) {
		switch (*p) {
			case '\0': pushOutputString(StringView{begin, p}); return lineNumber;
			case '#': {
				const char* const directiveBegin = p;
				++p;
				if (!lineContentsStarted) {
					lineContentsStarted = true;
					if (CStringView{p}.starts_with("include") && (*(p + 7) == ' ' || *(p + 7) == '\t')) {
						p += 8;
						while (*p == ' ' || *p == '\t') {
							++p;
						}
						if (*p == '<') {
							++p;
							const char* includedFilepathBegin = p;
							while (*p != '\0') {
								if (*p == '>') {
									const char* includedFilepathEnd = p;
									++p;
									const StringView includedFilepath{includedFilepathBegin, includedFilepathEnd};
									const CStringView builtinShaderHeaderSourceCode = findBuiltinShaderHeaderSourceCode(includedFilepath);
									if (!builtinShaderHeaderSourceCode.empty()) {
										if (includedFilepaths.insert(includedFilepath).second) {
											pushOutputString(StringView{begin, directiveBegin});
											if (expandShaderIncludes(output, allocatedStrings, includedFilepaths, filesystem, includedFilepath,
													builtinShaderHeaderSourceCode.c_str()) > 1) {
												pushOutputString(formatString("\n#line {}", lineNumber + 1));
											}
										}
										begin = p;
									}
									break;
								}
								++p;
							}
						} else if (filesystem && *p == '\"') {
							++p;
							const char* includedFilepathBegin = p;
							while (*p != '\0') {
								if (*p == '\"') {
									const char* includedFilepathEnd = p;
									++p;
									pushOutputString(StringView{begin, directiveBegin});
									output.push_back("\n#line 1\n");
									String includedFilepath{};
									const size_t filepathLastSlashPosition = filepath.find_last_of("/\\");
									if (filepathLastSlashPosition == String::npos) {
										includedFilepath = String{includedFilepathBegin, includedFilepathEnd};
									} else {
										includedFilepath = String{filepath.substr(0, filepathLastSlashPosition + 1)};
										includedFilepath.append(StringView{includedFilepathBegin, includedFilepathEnd});
									}
									Allocation<char> allocatedIncludedFileContents = filesystem->readInputFileCString(includedFilepath);
									const char* const includedFileContents = allocatedIncludedFileContents.data();
									allocatedStrings.push_back(std::move(allocatedIncludedFileContents));
									if (expandShaderIncludes(output, allocatedStrings, includedFilepaths, filesystem, includedFilepath, includedFileContents) > 1) {
										pushOutputString(formatString("\n#line {}", lineNumber + 1));
									}
									begin = p;
									break;
								}
								++p;
							}
						}
					} else if (CStringView{p}.starts_with("pragma") && *(p + 6) == ' ' || *(p + 6) == '\t') {
						p += 7;
						while (*p == ' ' || *p == '\t') {
							++p;
						}
						if (CStringView{p}.starts_with("once") && (*(p + 4) == ' ' || *(p + 4) == '\t' || *(p + 4) == '\r' || *(p + 4) == '\n')) {
							pushOutputString(StringView{begin, directiveBegin});
							if (!includedFilepaths.insert(filepath).second) {
								return lineNumber;
							}
							p += 4;
							begin = p;
						}
					}
				}
				break;
			}
			case '/':
				lineContentsStarted = true;
				++p;
				switch (*p) {
					case '/':
						++p;
						while (*p != '\0') {
							if (*p == '\n') {
								++p;
								++lineNumber;
								lineContentsStarted = false;
								break;
							}
							++p;
						}
						break;
					case '*':
						++p;
						while (*p != '\0') {
							if (*p == '*' && *(p + 1) == '/') {
								p += 2;
								break;
							}
							if (*p == '\n') {
								++p;
								++lineNumber;
							} else {
								++p;
							}
						}
						break;
					default: break;
				}
				break;
			case '\n':
				++p;
				++lineNumber;
				lineContentsStarted = false;
				break;
			case ' ': [[fallthrough]];
			case '\t': ++p; break;
			default:
				lineContentsStarted = true;
				++p;
				break;
		}
	}
}

} // namespace

#endif

void writeInputAttributeDeclarations([[maybe_unused]] String& output, [[maybe_unused]] size_t& attributeIndex,
	[[maybe_unused]] Span<const VertexAttributeDescription> vertexAttributeDescriptions) {
#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	if (!vertexAttributeDescriptions.empty()) {
		for (const VertexAttributeDescription& vertexAttributeDescription : vertexAttributeDescriptions) {
			if (!isValidName(vertexAttributeDescription.name)) {
				throw graphics::Error{formatString("Invalid vertex attribute name \"{}\".", vertexAttributeDescription.name)};
			}
			output.append(
				formatString("layout(location = {}) in {} {};\n", attributeIndex, getVertexAttributeTypeName(vertexAttributeDescription.type), vertexAttributeDescription.name));
			++attributeIndex;
		}
		output.push_back('\n');
	}
#else
	unreachable();
#endif
}

void writeInputAttributeDeclarations([[maybe_unused]] String& output, [[maybe_unused]] size_t& attributeIndex, [[maybe_unused]] Span<const FieldDescription> fieldDescriptions) {
#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	if (!fieldDescriptions.empty()) {
		for (const FieldDescription& fieldDescription : fieldDescriptions) {
			if (!isValidName(fieldDescription.name)) {
				throw graphics::Error{formatString("Invalid attribute name \"{}\".", fieldDescription.name)};
			}
			if (fieldDescription.arrayElementCount == 0) {
				output.append(formatString("layout(location = {}) in {} {};\n", attributeIndex, getFieldTypeName(fieldDescription.type), fieldDescription.name));
				attributeIndex += getFieldAttributeCount(fieldDescription.type);
			} else {
				output.append(formatString("layout(location = {}) in {} {}[{}];\n", attributeIndex, getFieldTypeName(fieldDescription.type), fieldDescription.name,
					fieldDescription.arrayElementCount));
				attributeIndex += getFieldAttributeCount(fieldDescription.type) * fieldDescription.arrayElementCount;
			}
		}
		output.push_back('\n');
	}
#else
	unreachable();
#endif
}

void writeVec4BufferGetters([[maybe_unused]] String& output, [[maybe_unused]] StringView getterFunctionNamePrefix, [[maybe_unused]] Span<const FieldDescription> fieldDescriptions,
	[[maybe_unused]] FunctionView<void(String& output, StringView nameString, StringView indexString)> writeFetchDeclaration) {
#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	const size_t elementStrideInVec4s = calculateElementStrideInVec4s(fieldDescriptions);
	size_t fieldOffsetInFloats = 0;
	for (const FieldDescription& fieldDescription : fieldDescriptions) {
		const StringView fieldTypeName = getFieldTypeName(fieldDescription.type);
		String returnTypeName{fieldTypeName};
		if (fieldDescription.arrayElementCount != 0) {
			returnTypeName.append(formatString("[{}]", fieldDescription.arrayElementCount));
		}
		const size_t fieldFloatOffset = fieldOffsetInFloats % 4;
		const size_t fieldSizeInFloats = getFieldSizeInFloats(fieldDescription);
		const size_t requiredVec4FetchCount = convertFloatCountToVec4Count(fieldFloatOffset + fieldSizeInFloats);
		if (getterFunctionNamePrefix.empty()) {
			output.append(formatString("{} GREM_private_bufferFetch{}(uint index) {{\n", returnTypeName, fieldDescription.name));
		} else {
			output.append(formatString("{} {}{}(uint index) {{\n", returnTypeName, getterFunctionNamePrefix, fieldDescription.name));
		}
		output.append(formatString("    uint indexInVec4s = index * {}u + {}u;\n", elementStrideInVec4s, fieldOffsetInFloats / 4));
		for (size_t i = 0; i < requiredVec4FetchCount; ++i) {
			output.append("    ");
			writeFetchDeclaration(output, formatString("fetchedTexel{}", i), formatString("indexInVec4s + {}u", i));
			output.push_back('\n');
		}
		output.append("    return ");
		if (fieldDescription.arrayElementCount != 0) {
			output.append(formatString("{}(", returnTypeName));
		}
		const size_t fieldElementSizeInFloats = getFieldSizeInFloats(fieldDescription.type);
		const size_t fieldElementCount = max(fieldDescription.arrayElementCount, size_t{1});
		for (size_t fieldElementIndex = 0; fieldElementIndex < fieldElementCount; ++fieldElementIndex) {
			if (fieldElementIndex > 0) {
				output.append(", ");
			}
			switch (fieldDescription.type) {
				case FieldType::INT:
					output.append(formatString("floatBitsToInt(fetchedTexel{}.{})", (fieldFloatOffset + fieldElementIndex) / 4,
						getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex) % 4)));
					break;
				case FieldType::IVEC2: [[fallthrough]];
				case FieldType::IVEC3: [[fallthrough]];
				case FieldType::IVEC4:
					output.append(formatString("{}(", fieldTypeName));
					for (size_t i = 0; i < fieldElementSizeInFloats; ++i) {
						if (i > 0) {
							output.append(", ");
						}
						output.append(formatString("floatBitsToInt(fetchedTexel{}.{})", (fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) / 4,
							getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) % 4)));
					}
					output.push_back(')');
					break;
				case FieldType::UINT:
					output.append(formatString("floatBitsToUint(fetchedTexel{}.{})", (fieldFloatOffset + fieldElementIndex) / 4,
						getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex) % 4)));
					break;
				case FieldType::UVEC2: [[fallthrough]];
				case FieldType::UVEC3: [[fallthrough]];
				case FieldType::UVEC4:
					output.append(formatString("{}(", fieldTypeName));
					for (size_t i = 0; i < fieldElementSizeInFloats; ++i) {
						if (i > 0) {
							output.append(", ");
						}
						output.append(formatString("floatBitsToUint(fetchedTexel{}.{})", (fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) / 4,
							getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) % 4)));
					}
					output.push_back(')');
					break;
				case FieldType::FLOAT:
					output.append(
						formatString("fetchedTexel{}.{}", (fieldFloatOffset + fieldElementIndex) / 4, getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex) % 4)));
					break;
				case FieldType::VEC2: [[fallthrough]];
				case FieldType::VEC3: [[fallthrough]];
				case FieldType::VEC4: [[fallthrough]];
				case FieldType::MAT2: [[fallthrough]];
				case FieldType::MAT3: [[fallthrough]];
				case FieldType::MAT4:
					output.append(formatString("{}(", fieldTypeName));
					for (size_t i = 0; i < fieldElementSizeInFloats; ++i) {
						if (i > 0) {
							output.append(", ");
						}
						output.append(formatString("fetchedTexel{}.{}", (fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) / 4,
							getVec4ComponentCharacter((fieldFloatOffset + fieldElementIndex * fieldElementSizeInFloats + i) % 4)));
					}
					output.push_back(')');
					break;
			}
		}
		fieldOffsetInFloats += fieldSizeInFloats;
		if (fieldDescription.arrayElementCount != 0) {
			output.push_back(')');
		}
		output.append(";\n}\n");
		if (getterFunctionNamePrefix.empty()) {
			output.append(formatString("#define {0}(index) GREM_private_bufferFetch{0}(uint(index))\n", fieldDescription.name));
		}
	}
	output.push_back('\n');
#else
	unreachable();
#endif
}

ExpandedStringBuffer expandIncludes([[maybe_unused]] AllocatedStringBuffer& allocatedStrings, [[maybe_unused]] Span<const char* const> sourceStrings,
	[[maybe_unused]] const Filesystem* filesystem, [[maybe_unused]] CStringView filepath) {
#if !defined(GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN) || defined(GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	ExpandedStringBuffer result{};
	result.reserve(sourceStrings.size());
	IncludedFilepathSet includedFilepaths{};
	for (const char* const sourceString : sourceStrings) {
		if (!std::strstr(sourceString, "#include")) {
			result.push_back(sourceString);
			continue;
		}

		expandShaderIncludes(result, allocatedStrings, includedFilepaths, filesystem, filepath, sourceString);
	}
	return result;
#else
	unreachable();
#endif
}

} // namespace detail

} // namespace grem::graphics
