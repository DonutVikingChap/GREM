void main() {
	fragmentTextureCoordinates = vertexPosition;
	gl_Position = (cameraProjectionMatrix * mat4(mat3(cameraViewMatrix)) * vec4(vertexPosition, 1.0)).xyww;
}
