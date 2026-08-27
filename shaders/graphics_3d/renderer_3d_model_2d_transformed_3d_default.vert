void main() {
	fragmentTextureCoordinates = instanceTextureOffset + instanceTextureBasis * vertexTextureCoordinates;
	fragmentTintColor = instanceTintColor;
	fragmentEmissiveColor = instanceEmissiveColor;
	gl_Position = cameraViewProjectionMatrix * transformation3DTransformation * vec4(instancePosition + instanceBasis * vertexPosition, 0.0, 1.0);
}
