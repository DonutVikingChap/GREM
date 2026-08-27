void main() {
	fragmentTextureCoordinates = vertexPosition;
	gl_Position = vec4(vertexPosition * 2.0 - vec2(1.0), 0.0, 1.0);
}
