#include <GREM/light.glsl>
#include <GREM/material.glsl>
#include <GREM/pbr.glsl>
#include <GREM/tonemapping.glsl>
#include <GREM/Model3D/fragment.glsl>
#include <GREM/Fog3D/fragment.glsl>
#include <GREM/Sky3D/fragment.glsl>
#include <GREM/Decals3D/fragment.glsl>
#include <GREM/Lights3D/fragment.glsl>
#include <GREM/LightProbeVolumes3D/fragment.glsl>
#include <GREM/ReflectionProbes3D/fragment.glsl>

#define USE_GEOMETRIC_SPECULAR_ANTI_ALIASING 1

void main() {
	mat3 tbn = mat3(normalize(fragmentTangent), normalize(fragmentBitangent), normalize(fragmentNormal));
#if USE_GEOMETRIC_SPECULAR_ANTI_ALIASING
	// Calculate gradient before any branches.
	vec3 du = dFdx(tbn[2]);
	vec3 dv = dFdy(tbn[2]);
#endif

	vec2 positionOnScreen = vec2(GREM_fragmentCoordinates.x, screenFramebufferHeight - GREM_fragmentCoordinates.y) - screenViewportOffset;
	uvec2 tileIndices = uvec2(clamp(ivec2(floor(positionOnScreen * screenInverseTileSize)), ivec2(0, 0), ivec2(screenTileCounts) - ivec2(1, 1)));
	uint tileOffset = (tileIndices.y * screenTileCounts.x + tileIndices.x) * 2u;
	float depthRangeBegin = cameraNearAndFarPlaneDistances.x;
	float depthRangeLength = cameraNearAndFarPlaneDistances.y - depthRangeBegin;
	uint depthBinIndex = uint(clamp(int((fragmentDepth - depthRangeBegin) * float(screenDepthBinCount) / depthRangeLength), 0, int(screenDepthBinCount - 1u)));

	uint itemOffsetIndex = (tileOffset + 0u) >> 2;
	uint itemOffsetSubIndex = (tileOffset + 0u) & 0x3u;
	uint itemCountsIndex = (tileOffset + 1u) >> 2;
	uint itemCountsSubIndex = (tileOffset + 1u) & 0x3u;
	uvec4 itemSubOffsets = tileItemOffsetsAndCountsBy4s[itemOffsetIndex];
	uvec4 itemSubCounts = tileItemOffsetsAndCountsBy4s[itemCountsIndex];
	uint itemOffset = (itemOffsetSubIndex == 0u) ? itemSubOffsets.x : (itemOffsetSubIndex == 1u) ? itemSubOffsets.y : (itemOffsetSubIndex == 2u) ? itemSubOffsets.z : itemSubOffsets.w;
	uint itemCounts = (itemCountsSubIndex == 0u) ? itemSubCounts.x : (itemCountsSubIndex == 1u) ? itemSubCounts.y : (itemCountsSubIndex == 2u) ? itemSubCounts.z : itemSubCounts.w;
	uint decalCount = (itemCounts >> 24) & 0xFFu;
	uint lightCount = (itemCounts >> 16) & 0xFFu;
	uint lightProbeVolumeCount = (itemCounts >> 8) & 0xFFu;
	uint reflectionProbeCount = (itemCounts >> 0) & 0xFFu;

	uint decalItemsBegin = itemOffset;
	uint decalItemsEnd = decalItemsBegin + decalCount;
	uint lightItemsBegin = decalItemsEnd;
	uint lightItemsEnd = lightItemsBegin + lightCount;
	uint lightProbeVolumeItemsBegin = lightItemsEnd;
	uint lightProbeVolumeItemsEnd = lightProbeVolumeItemsBegin + lightProbeVolumeCount;
	uint reflectionProbeItemsBegin = lightProbeVolumeItemsEnd;
	uint reflectionProbeItemsEnd = reflectionProbeItemsBegin + reflectionProbeCount;

	uint lightsBegin = depthBinLightsBegin(depthBinIndex);
	uint lightsEnd = depthBinLightsEnd(depthBinIndex);
	uint decalsBegin = depthBinDecalsBegin(depthBinIndex);
	uint decalsEnd = depthBinDecalsEnd(depthBinIndex);

	// Sample material parameters.
	GREM_Material material = GREM_Model3D_getMaterial();

	if (FRAGMENT_DOUBLE_SIDED) {
		tbn *= float(gl_FrontFacing) * 2.0 - 1.0;
	}

	vec3 rawNormal = tbn[2];

	// Apply decals.
	for (uint i = decalItemsBegin; i < decalItemsEnd; ++i) {
		uint decalIndex = itemIndex(i);
		if (decalIndex >= decalsBegin && decalIndex < decalsEnd) {
			GREM_Decals3D_applyDecalToMaterial(material, decalIndex, fragmentPosition, rawNormal, fragmentInstanceIdentifier);
		}
	}

#if USE_GEOMETRIC_SPECULAR_ANTI_ALIASING
	{
		const float SCREEN_SPACE_VARIANCE = 0.15915494;
		const float CLAMPING_THRESHOLD = 0.18;
		float variance = SCREEN_SPACE_VARIANCE * (dot(du, du) + dot(dv, dv));
		float kernelRoughnessSquared = min(2.0 * variance, CLAMPING_THRESHOLD);
		float filteredRoughnessSquared = clamp(material.roughness * material.roughness + kernelRoughnessSquared, 0.0, 1.0);
		material.roughness = sqrt(filteredRoughnessSquared);
	}
#endif

	if (FRAGMENT_ALPHA_MASKED) {
		if (material.alpha < material.alphaCutoff) {
			discard;
		}
	}

	// Calculate the world-space normal from the material's tangent-space normal.
	vec3 normal = normalize(tbn * material.tangentSpaceNormal);

	// Calculate view-dependent parameters.
	vec3 viewVector = cameraPosition - fragmentPosition;
	float viewDistance = length(viewVector);
	vec3 viewDirection = viewVector / viewDistance;
	float nDotV = max(dot(normal, viewDirection), 0.0);
	vec3 reflectionDirection = reflect(-viewDirection, normal);
	vec2 splitSumBRDFIntegration = GREM_textureSample2D(pbrSpecularSplitSumBRDFIntegrationMap, vec2(nDotV, material.roughness)).rg;

	vec3 color = vec3(0.0);

	// Apply light emission from material.
	color += material.coverage * material.emissive;

	// Apply direct light and gather sunlight.
	GREM_Light sunLight = GREM_createLight(GREM_SUN_LIGHT);
	for (uint lightIndex = 0u; lightIndex < screenGlobalLightCount; ++lightIndex) {
		GREM_Light light = GREM_Lights3D_getLight(lightIndex, fragmentPosition, fragmentDepth, rawNormal, viewDirection);
		if (light.type == GREM_SUN_LIGHT) {
			sunLight = light;
		} else {
			color += GREM_getDirectLightContribution(light, material, normal, viewDirection, nDotV);
		}
	}
	for (uint i = lightItemsBegin; i < lightItemsEnd; ++i) {
		uint lightIndex = itemIndex(i);
		if (lightIndex >= lightsBegin && lightIndex < lightsEnd) {
			GREM_Light light = GREM_Lights3D_getLight(lightIndex, fragmentPosition, fragmentDepth, rawNormal, viewDirection);
			color += GREM_getDirectLightContribution(light, material, normal, viewDirection, nDotV);
		}
	}

	// Gather irradiance from the sky and light probe volumes.
	vec4 skyIrradiance = GREM_Sky3D_getIrradiance(normal);
	vec4 irradiance = vec4(0.0);
	for (uint i = lightProbeVolumeItemsBegin; i < lightProbeVolumeItemsEnd; ++i) {
		uint lightProbeVolumeIndex = itemIndex(i);
		vec4 lightProbeVolumeIrradiance = GREM_LightProbeVolumes3D_getLightProbeVolumeIrradiance(lightProbeVolumeIndex, fragmentPosition, rawNormal, viewDirection);
		irradiance = GREM_blendAOverB(irradiance, lightProbeVolumeIrradiance);
		if (irradiance.a >= 0.99999) {
			irradiance.a = 1.0;
			break;
		}
	}
	irradiance = GREM_blendAOverB(irradiance, skyIrradiance);

	// Gather reflection from the sky and reflection probes.
	vec4 skyReflection = GREM_Sky3D_getReflection(material.roughness, reflectionDirection);
	vec4 reflection = vec4(0.0);
	for (uint i = reflectionProbeItemsBegin; i < reflectionProbeItemsEnd; ++i) {
		uint reflectionProbeIndex = itemIndex(i);
		vec4 reflectionProbeReflection = GREM_ReflectionProbes3D_getReflectionProbeReflection(reflectionProbeIndex, fragmentPosition, material.roughness, reflectionDirection);
		reflection = GREM_blendAOverB(reflection, reflectionProbeReflection);
		if (reflection.a >= 0.99999) {
			reflection.a = 1.0;
			break;
		}
	}
	float skyReflectionVisibility = 1.0 - reflection.a;
	reflection = GREM_blendAOverB(reflection, skyReflection);

	// Apply global illumination.
	color += GREM_getAmbientLightContribution(irradiance.rgb * irradiance.a, reflection.rgb * reflection.a, sunLight, skyReflectionVisibility, material, normal, splitSumBRDFIntegration, reflectionDirection, nDotV);

	// Mix in fog.
	color = GREM_Fog3D_blend(color, viewDistance);

	// Perform color grading.
	color *= cameraExposure;
	if (!FRAGMENT_HDR) {
		color = GREM_tonemap(color);
	}

	// Write output.
	outputColor = vec4(color, material.coverage);
}
