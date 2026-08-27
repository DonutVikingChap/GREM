#include "Tileset.glsl"

void main() {
	vec2 offsetFromViewTileOffset = fragmentViewCoordinates + tileSubTileOffset;
	vec2 tileOffsetFromViewTileOffset = floor(offsetFromViewTileOffset);
	vec2 subTileCoordinates = offsetFromViewTileOffset - tileOffsetFromViewTileOffset;
	ivec2 tileCoordinates = tileBaseOffset + ivec2(tileOffsetFromViewTileOffset);

	ivec2 chunkIndices = ivec2(tileCoordinates.x >> TILEMAP_CHUNK_BIT_SHIFT_X, tileCoordinates.y >> TILEMAP_CHUNK_BIT_SHIFT_Y);

	uint tile;
	if (any(lessThan(chunkIndices, tilemapChunkIndicesMin)) || any(greaterThan(chunkIndices, tilemapChunkIndicesMax))) {
		tile = tilemapDefaultTile;
	} else {
		uvec2 relativeChunkIndices = uvec2(chunkIndices) - uvec2(tilemapChunkIndicesMin);
		uint chunkTextureIndex = tilemapChunkTextureIndex(relativeChunkIndices.y * (uint(tilemapChunkIndicesMax.x) - uint(tilemapChunkIndicesMin.x) + 1u) + relativeChunkIndices.x);
		if (chunkTextureIndex == 0xFFFFFFFFu) {
			tile = tilemapDefaultTile;
		} else {
			ivec2 tileCoordinatesInChunk = ivec2(tileCoordinates.x - chunkIndices.x * TILEMAP_CHUNK_WIDTH, tileCoordinates.y - chunkIndices.y * TILEMAP_CHUNK_HEIGHT);
			uvec4 tileSample = uvec4(GREM_texelFetch2DArray(tilemapChunkTextures, ivec3(tileCoordinatesInChunk.x, TILEMAP_CHUNK_HEIGHT - 1 - tileCoordinatesInChunk.y, chunkTextureIndex), 0) * 255.0 + vec4(0.5));
			tile = (tileSample.a << 24) | (tileSample.b << 16) | (tileSample.g << 8) | tileSample.r;
		}
	}

	outputColor = Tileset_sampleTile(tile, subTileCoordinates);
}
