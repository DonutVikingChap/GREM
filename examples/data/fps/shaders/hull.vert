void main() {
	gl_Position = cameraViewProjectionMatrix * instanceTransformation * vec4(vertexPosition, 1.0);
}
