#include <GREM/Model3D/fragment.glsl>
#include <GREM/tonemapping.glsl>

struct Light {
	float type;
	vec3 direction;
	vec3 intensity;
};

Light getLight(uint lightIndex) {
	vec4 typeAndRangeAndConeCosines = lightTypeAndRangeAndConeCosines(lightIndex);
	vec3 position = lightPosition(lightIndex).xyz;
	vec3 direction = lightDirection(lightIndex).xyz;
	vec3 intensity = lightIntensity(lightIndex).xyz;

	float type = typeAndRangeAndConeCosines.x;
	float range = typeAndRangeAndConeCosines.y;
	vec2 coneCosines = typeAndRangeAndConeCosines.zw;

	Light light;
	light.type = type;

	vec3 lightVector = (type == LIGHT_TYPE_DIRECTIONAL) ? -direction : position - fragmentPosition;
	float lightDistanceSquared = dot(lightVector, lightVector);
	float lightDistance = sqrt(lightDistanceSquared);
	light.direction = lightVector / lightDistance;

	float rangeAttenuationFactor = (type == LIGHT_TYPE_DIRECTIONAL) ? 1.0 : clamp(1.0 - pow(lightDistance / range, 4.0), 0.0, 1.0) / lightDistanceSquared;
	float coneAttenuationFactor = (type == LIGHT_TYPE_SPOT) ? smoothstep(coneCosines.y, coneCosines.x, dot(normalize(direction), -light.direction)) : 1.0;
	light.intensity = rangeAttenuationFactor * coneAttenuationFactor * intensity;

	return light;
}

float halfLambertDiffuse(vec3 normal, vec3 lightDirection) {
	float factor = 0.5 + 0.5 * dot(normal, lightDirection);
	return factor * factor;
}

float blinnPhongSpecular(vec3 normal, vec3 lightDirection, vec3 viewDirection, float specularExponent) {
	vec3 halfwayDirection = normalize(lightDirection + viewDirection);
	return pow(max(dot(normal, halfwayDirection), 0.0), specularExponent);
}

vec3 getDirectLightContribution(Light light, vec3 normal, vec3 viewDirection, vec3 materialDiffuse, vec3 materialSpecular) {
	float diffuseFactor = halfLambertDiffuse(normal, light.direction);

	float specularExponentSqrt = length(materialSpecular) * 5.0;
	float specularExponent = specularExponentSqrt * specularExponentSqrt;
	float specularFactor = blinnPhongSpecular(normal, light.direction, viewDirection, specularExponent);

	vec3 diffuse = diffuseFactor * materialDiffuse;
	vec3 specular = specularFactor * materialSpecular;

	return light.intensity * (diffuse + specular);
}

float getBlobShadow(uint blobShadowIndex) {
	vec3 shadowPosition = blobShadowPosition(blobShadowIndex);
	float shadowRadius = blobShadowRadius(blobShadowIndex);
	uint instanceIdentifier = blobShadowInstanceIdentifier(blobShadowIndex);

	float verticalDistance = fragmentPosition.y - shadowPosition.y;
	float verticalShadow = (verticalDistance > 0.0) ? 1.0 - min(verticalDistance / (shadowRadius * 2.0), 1.0) : 0.5 / pow(1.0 - verticalDistance, 0.2) + 0.5 / pow(1.0 - verticalDistance, 1.5);

	float horizontalMinDistance = shadowRadius * 0.3 * (0.2 + 0.8 * verticalShadow);
	float horizontalMaxDistance = shadowRadius * 1.1 * (0.7 + 0.3 * verticalShadow);
	float horizontalDistance = length(fragmentPosition.xz - shadowPosition.xz);
	float horizontalShadow = 1.0 - clamp((horizontalDistance - horizontalMinDistance) / (horizontalMaxDistance - horizontalMinDistance), 0.0, 1.0);

	float instanceAttenuationFactor = (fragmentInstanceIdentifier == instanceIdentifier) ? 0.0 : 1.0;

	return instanceAttenuationFactor * smoothstep(0.0, 1.0, horizontalShadow * verticalShadow);
}

void main() {
	vec2 positionOnScreen = vec2(GREM_fragmentCoordinates.x, modelShaderFramebufferHeight - GREM_fragmentCoordinates.y) - modelShaderViewportOffset;
	uvec2 tileIndices = uvec2(clamp(ivec2(floor(positionOnScreen * modelShaderInverseTileSize)), ivec2(0, 0), ivec2(modelShaderTileCounts) - ivec2(1, 1)));
	uint tileIndex = tileIndices.y * modelShaderTileCounts.x + tileIndices.x;

	uint tileItemOffset = itemOffset(tileIndex);
	uint tileItemCounts = itemCounts(tileIndex);
	uint lightCount = (tileItemCounts >> 24) & 0xFFu;
	uint blobShadowCount = (tileItemCounts >> 16) & 0xFFu;

	uint lightItemsBegin = tileItemOffset;
	uint lightItemsEnd = lightItemsBegin + lightCount;
	uint blobShadowItemsBegin = lightItemsEnd;
	uint blobShadowItemsEnd = blobShadowItemsBegin + blobShadowCount;

	mat3 tbn = mat3(normalize(fragmentTangent), normalize(fragmentBitangent), normalize(fragmentNormal));

	GREM_Material material = GREM_Model3D_getMaterial();
	vec3 emissive = material.coverage * material.emissive;
	vec3 diffuse = material.coverage * material.albedo;
	vec3 specular = material.coverage * (1.0 - material.roughness * material.roughness) * mix(vec3(1.0), material.albedo, material.metallic);
	
	if (FRAGMENT_ALPHA_MASKED) {
		if (material.alpha < material.alphaCutoff) {
			discard;
		}
	}

	if (FRAGMENT_DOUBLE_SIDED) {
		tbn *= float(gl_FrontFacing) * 2.0 - 1.0;
	}

	vec3 normal = normalize(tbn * material.tangentSpaceNormal);

	vec3 viewDirection = normalize(cameraPosition - fragmentPosition);

	vec3 directLight = vec3(0.0);
	for (uint i = lightItemsBegin; i < lightItemsEnd; ++i) {
		uint lightIndex = itemIndex(i);
		directLight += getDirectLightContribution(getLight(lightIndex), normal, viewDirection, diffuse, specular);
	}

	float blobShadow = 0.0;
	for (uint i = blobShadowItemsBegin; i < blobShadowItemsEnd; ++i) {
		uint blobShadowIndex = itemIndex(i);
		blobShadow += getBlobShadow(blobShadowIndex);
	}

	directLight *= mix(1.0, 0.4, min(blobShadow, 1.0) * max(normal.y, 0.0));

	vec3 color = skyAmbientColor.rgb * skyAmbientColor.a * material.occlusion * diffuse + emissive + directLight;

	color *= cameraExposure;
	if (!FRAGMENT_HDR) {
		color = GREM_tonemap(color);
	}

	outputColor = vec4(color, material.coverage);
}
