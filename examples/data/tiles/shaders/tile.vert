void main() {
	fragmentSubTileCoordinates = vertexPosition;
	fragmentTile = instanceTile;

	vec2 instanceOffsetInView = vec2(instanceTileOffset - tileBaseOffset) + (instanceSubTileOffset - tileSubTileOffset);
	gl_Position = vec4(((instanceOffsetInView + vertexPosition) / tileViewSize) * 2.0 - vec2(1.0), 0.0, 1.0);
}
