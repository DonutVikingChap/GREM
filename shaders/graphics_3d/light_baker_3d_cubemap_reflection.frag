#include <GREM/numbers.glsl>
#include <GREM/pbr.glsl>
#include <GREM/sampling.glsl>

void main() {
	vec3 normal = normalize(fragmentTextureCoordinates);
	mat3 tbn = GREM_getTangentSpaceBasis(normal);
	vec3 viewDirection = normal;

	float resolution = reflectionResolution;
	float resolutionSquared = resolution * resolution;

	vec3 sampleSum = vec3(0.0);
	float weightSum = 0.0;
	for (uint i = 0u; i < REFLECTION_SAMPLE_COUNT; ++i) {
		vec3 halfwayVector = tbn * GREM_getCosineWeightedHemisphereImportanceSampleGGX(GREM_getHammersleyPoint(i, REFLECTION_SAMPLE_COUNT), reflectionRoughness);
		vec3 lightDirection = reflect(-viewDirection, halfwayVector);
		float nDotL = dot(normal, lightDirection);
		if (nDotL > 0.0) {
			float nDotH = max(dot(normal, halfwayVector), 0.0);
			float vDotH = max(dot(viewDirection, halfwayVector), 0.0);
			float d = GREM_getMicrofacetDistribution(nDotH, reflectionRoughness);
			float pdf = d * nDotH / (4.0 * vDotH);
			float solidAngleSample = 1.0 / (float(REFLECTION_SAMPLE_COUNT) * (pdf + 0.0001));
			float solidAngleTexel = 4.0 * GREM_PI / (6.0 * resolutionSquared);
			float detailLevel = (reflectionRoughness == 0.0) ? 0.0 : 0.5 * log2(solidAngleSample / solidAngleTexel);
			float weight = nDotL;
			sampleSum += GREM_textureSampleLodCube(radianceCubemapTexture, lightDirection, detailLevel).rgb * weight;
			weightSum += weight;
		}
	}
	vec3 reflection = sampleSum / weightSum;
	outputColor = vec4(reflection, GREM_textureSampleGradCubeShadow(depthCubemapTexture, vec4(normal, 1.0), vec3(0.0), vec3(0.0)));
}
