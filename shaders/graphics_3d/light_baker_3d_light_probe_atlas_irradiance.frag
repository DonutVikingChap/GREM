#include <GREM/irradiance.glsl>
#include <GREM/numbers.glsl>
#include <GREM/sampling.glsl>

void main() {
	vec2 uv = (fragmentTextureCoordinates - irradianceMapPadding) / (1.0 - 2.0 * irradianceMapPadding);
    if (uv.x < 0.0) {
        uv.x = -uv.x;
        uv.y = 1.0 - uv.y;
    } else if (uv.x > 1.0) {
        uv.x = 2.0 - uv.x;
        uv.y = 1.0 - uv.y;
    }
    if (uv.y < 0.0) {
        uv.x = 1.0 - uv.x;
        uv.y = -uv.y;
    } else if (uv.y > 1.0) {
        uv.x = 1.0 - uv.x;
        uv.y = 2.0 - uv.y;
    }

	vec3 normal = GREM_getNormalFromOctahedralMapCoordinates(uv);
	mat3 tbn = GREM_getTangentSpaceBasis(normal);

	vec3 sampleSum = vec3(0.0);
	for (uint i = 0u; i < IRRADIANCE_SAMPLE_COUNT; ++i) {
		vec3 sampleDirection = tbn * GREM_getCosineWeightedHemisphereImportanceSample(GREM_getHammersleyPoint(i, IRRADIANCE_SAMPLE_COUNT));
		sampleSum += GREM_textureSampleCube(radianceCubemapTexture, sampleDirection).rgb;
	}
	vec3 irradiance = sampleSum * (GREM_PI / float(IRRADIANCE_SAMPLE_COUNT));
	outputColor = vec4(GREM_encodeIrradiance(irradiance), 1.0);
}
