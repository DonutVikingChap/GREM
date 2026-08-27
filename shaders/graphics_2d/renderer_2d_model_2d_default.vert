void main() {
	fragmentTextureCoordinates = instanceTextureOffset + instanceTextureBasis * vertexTextureCoordinates;
	fragmentTintColor = instanceTintColor;
	fragmentEmissiveColor = instanceEmissiveColor;
	gl_Position = vec4((cameraViewProjectionMatrix * vec3(instancePosition + instanceBasis * vertexPosition, 1.0)).xy, 0.0, 1.0);
}
