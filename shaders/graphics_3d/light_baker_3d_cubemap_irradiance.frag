#include <GREM/irradiance.glsl>
#include <GREM/numbers.glsl>
#include <GREM/sampling.glsl>

void main() {
	vec3 normal = normalize(fragmentTextureCoordinates);
	mat3 tbn = GREM_getTangentSpaceBasis(normal);

	vec3 sampleSum = vec3(0.0);
	for (uint i = 0u; i < IRRADIANCE_SAMPLE_COUNT; ++i) {
		vec3 sampleDirection = tbn * GREM_getCosineWeightedHemisphereImportanceSample(GREM_getHammersleyPoint(i, IRRADIANCE_SAMPLE_COUNT));
		sampleSum += GREM_textureSampleCube(radianceCubemapTexture, sampleDirection).rgb;
	}
	vec3 irradiance = sampleSum * (GREM_PI / float(IRRADIANCE_SAMPLE_COUNT));
	outputColor = vec4(GREM_encodeIrradiance(irradiance), 1.0);
}
