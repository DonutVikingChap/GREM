#include <GREM/Model3D/vertex.glsl>

void main() {
	mat4 modelMatrix = GREM_Model3D_getVertexModelMatrix();
	mat3 normalMatrix = GREM_Model3D_getVertexNormalMatrix(modelMatrix);

	vec3 localPosition = GREM_Model3D_getVertexPosition();
	vec3 localNormal = GREM_Model3D_getVertexNormal();
	vec4 localTangent = GREM_Model3D_getVertexTangent();

	vec4 position = modelMatrix * vec4(localPosition, 1.0);
	vec3 worldSpacePosition = position.xyz / position.w;
	vec3 worldSpaceNormal = normalize(normalMatrix * localNormal);
	vec3 worldSpaceTangent = normalize(mat3(modelMatrix) * localTangent.xyz);
	vec3 worldSpaceBitangent = cross(worldSpaceNormal, worldSpaceTangent) * localTangent.w;

	fragmentPosition = worldSpacePosition;
	fragmentDepth = -(cameraViewMatrix * vec4(worldSpacePosition, 1.0)).z;
	fragmentNormal = worldSpaceNormal;
	fragmentTangent = worldSpaceTangent;
	fragmentBitangent = worldSpaceBitangent;
	fragmentTextureCoordinatesChannel0 = GREM_Model3D_getVertexTextureCoordinatesChannel0();
	fragmentTextureCoordinatesChannel1 = GREM_Model3D_getVertexTextureCoordinatesChannel1();
	fragmentTintColor = GREM_Model3D_getVertexTintColor();
	fragmentEmissiveColor = GREM_Model3D_getVertexEmissiveColor();
	fragmentEmissiveFactor = GREM_Model3D_getVertexEmissiveFactor();
	fragmentInstanceIdentifier = instanceInstanceIdentifier;

	gl_Position = cameraViewProjectionMatrix * position;
}
