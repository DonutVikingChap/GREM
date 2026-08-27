#include <GREM/sampling.glsl>

void main() {
	vec2 uv = (fragmentTextureCoordinates - distanceMapPadding) / (1.0 - 2.0 * distanceMapPadding);
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

	vec3 direction = GREM_getNormalFromOctahedralMapCoordinates(uv);
	mat3 tbn = GREM_getTangentSpaceBasis(direction);

	float distanceSum = 0.0;
	float distanceSquaredSum = 0.0;
	float weightSum = 0.0;
	for (uint i = 0u; i < DISTANCE_SAMPLE_COUNT; ++i) {
		vec3 sampleDirection = tbn * GREM_getUniformHemisphereSample(GREM_getHammersleyPoint(i, DISTANCE_SAMPLE_COUNT));
		float sampleDistance = GREM_textureSampleCube(distanceCubemapTexture, sampleDirection).r;
		float distance = min(sampleDistance, distanceMaxDistance);
		float weight = pow(dot(direction, sampleDirection), distanceSharpness);
		distanceSum += distance * weight;
		distanceSquaredSum += distance * distance * weight;
		weightSum += weight;
	}
	outputColor = vec4(distanceSum / weightSum, distanceSquaredSum / weightSum, 0.0, 1.0);
}
