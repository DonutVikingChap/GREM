#include <GREM/quaternion.glsl>
#include <GREM/numbers.glsl>
#include <GREM/tonemapping.glsl>
#include <GREM/irradiance.glsl>
#include <GREM/sampling.glsl>

vec4 getLightProbeVolumeIrradiance(uint lightProbeVolumeIndex, vec3 samplePosition, vec3 rawNormal) {
	vec3 center = lightProbeVolumeCenter(lightProbeVolumeIndex);
	vec4 orientation = lightProbeVolumeOrientation(lightProbeVolumeIndex);
	vec3 probeSpacing = lightProbeVolumeProbeSpacing(lightProbeVolumeIndex);
	vec3 probeCounts = lightProbeVolumeProbeCounts(lightProbeVolumeIndex);
	vec3 irradianceAtlasOffset = lightProbeVolumeIrradianceAtlasOffset(lightProbeVolumeIndex);
	vec2 irradianceAtlasPaddedProbeSizeAndTexelSize = lightProbeVolumeIrradianceAtlasPaddedProbeSizeAndTexelSize(lightProbeVolumeIndex);

	vec3 halfExtents = probeCounts * probeSpacing * 0.5;
	vec3 localSampleOffset = GREM_rotateVector(GREM_getInverseOrientation(orientation), samplePosition - center);
	vec3 gridPosition = (halfExtents + localSampleOffset) / probeSpacing - vec3(0.5);
	vec3 gridPositionBase = floor(gridPosition);
	ivec3 gridIndices = ivec3(gridPositionBase);
	ivec3 gridIndicesMax = ivec3(probeCounts) - ivec3(1);
	vec3 gridPositionOffset = gridPosition - gridPositionBase;

	vec3 irradianceSum = vec3(0.0);
	float weightSum = 0.0;
	for (int i = 0; i < 8; ++i) {
		ivec3 gridOffset = ivec3(i & 1, (i >> 2) & 1, (i >> 1) & 1);
		vec3 probeGridIndices = vec3(clamp(gridIndices + gridOffset, ivec3(0), gridIndicesMax));

		vec2 irradianceMapCoordinates = irradianceAtlasPaddedProbeSizeAndTexelSize.y + GREM_getOctahedralMapCoordinatesFromNormal(rawNormal) * (irradianceAtlasPaddedProbeSizeAndTexelSize.x - irradianceAtlasPaddedProbeSizeAndTexelSize.y * 2.0);
		vec2 irradianceAtlasCoordinates = irradianceAtlasOffset.xy + probeGridIndices.xz * irradianceAtlasPaddedProbeSizeAndTexelSize.x + irradianceMapCoordinates;
		vec4 irradianceSample = GREM_textureSampleGrad2DArray(lightProbeVolumesIrradianceAtlasTexture, vec3(irradianceAtlasCoordinates, irradianceAtlasOffset.z + probeGridIndices.y), vec2(0.0), vec2(0.0));
		vec3 irradiance = GREM_decodeIrradiance(irradianceSample.rgb);

		vec3 trilinearWeights = max(mix(vec3(1.0) - gridPositionOffset, gridPositionOffset, vec3(gridOffset)), vec3(0.001));
		float trilinearWeight = trilinearWeights.x * trilinearWeights.y * trilinearWeights.z;
		float weight = trilinearWeight;
		irradianceSum += irradiance * weight;
		weightSum += weight;
	}

	vec3 irradiance = irradianceSum * (1.0 / weightSum);

	vec3 blendWeights = vec3(1.0) - clamp((abs(localSampleOffset) - halfExtents) / probeSpacing, vec3(0.0), vec3(1.0));
	float blendWeight = min(min(blendWeights.x, blendWeights.y), blendWeights.z);

	return vec4(irradiance, blendWeight);
}

void main() {
	vec2 positionOnScreen = vec2(GREM_fragmentCoordinates.x, screenFramebufferHeight - GREM_fragmentCoordinates.y) - screenViewportOffset;
	uvec2 tileIndices = uvec2(clamp(ivec2(floor(positionOnScreen * screenInverseTileSize)), ivec2(0, 0), ivec2(screenTileCounts) - ivec2(1, 1)));
	uint tileOffset = (tileIndices.y * screenTileCounts.x + tileIndices.x) * 2u;

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

	uint lightProbeVolumeItemsBegin = itemOffset + decalCount + lightCount;
	uint lightProbeVolumeItemsEnd = lightProbeVolumeItemsBegin + lightProbeVolumeCount;

	vec3 rawNormal = normalize(fragmentNormal);

	vec4 ambientIrradiance = vec4(0.0);
	for (uint i = lightProbeVolumeItemsBegin; i < lightProbeVolumeItemsEnd; ++i) {
		uint lightProbeVolumeIndex = itemIndex(i);
		vec4 irradiance = getLightProbeVolumeIrradiance(lightProbeVolumeIndex, fragmentPosition, rawNormal);
		ambientIrradiance.rgb = mix(irradiance.rgb, ambientIrradiance.rgb, ambientIrradiance.a);
		ambientIrradiance.a = max(ambientIrradiance.a, irradiance.a);
	}

	vec3 color = ambientIrradiance.rgb * ambientIrradiance.a;

	color *= cameraExposure;
	if (!FRAGMENT_HDR) {
		color = GREM_tonemap(color);
	}

	outputColor = vec4(color, 1.0);
}
