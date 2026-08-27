#include <GREM/sampling.glsl>
#include <GREM/pbr.glsl>

void main() {
	float nDotV = fragmentTextureCoordinates.x;
	float roughness = fragmentTextureCoordinates.y;
	vec3 viewDirection = vec3(sqrt(1.0 - nDotV * nDotV), 0.0, nDotV);
	vec3 normal = vec3(0.0, 0.0, 1.0);
	mat3 tbn = GREM_getTangentSpaceBasis(normal);
	float a = 0.0;
	float b = 0.0;
	for (uint i = 0u; i < SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_SAMPLE_COUNT; ++i) {
		vec3 halfwayVector = tbn * GREM_getCosineWeightedHemisphereImportanceSampleGGX(GREM_getHammersleyPoint(i, SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_SAMPLE_COUNT), roughness);
		vec3 lightDirection = reflect(-viewDirection, halfwayVector);
		float nDotL = clamp(lightDirection.z, 0.0, 1.0);
		float nDotH = clamp(halfwayVector.z, 0.0, 1.0);
		float vDotH = clamp(dot(viewDirection, halfwayVector), 0.0, 1.0);
		if (nDotL > 0.0) {
			float g = GREM_getGeometryShadowingIBL(nDotL, nDotV, roughness);
			float v = g * vDotH / nDotH;
			float fresnelFactor = pow(max(1.0 - vDotH, 0.0), 5.0);
			a += v * (1.0 - fresnelFactor);
			b += v * fresnelFactor;
		}
	}
	outputColor = vec4(vec2(a, b) * (1.0 / float(SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_SAMPLE_COUNT)), 0.0, 1.0);
}
