void main() {
	fragmentTextureCoordinates = vertexPosition;
	gl_Position = cubemapViewProjectionMatrix * vec4(vertexPosition, 1.0);
}
