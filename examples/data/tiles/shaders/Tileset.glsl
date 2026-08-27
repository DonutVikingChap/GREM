#pragma once

#include "TileOptions.glsl"

vec4 Tileset_sampleTile(uint tile, vec2 subTileCoordinates) {
	TileOptions options = TileOptions_decode(tile);

	uint frameOffset = uint(tileAnimationTime * float(options.animationFrameRate)) % (options.animationFrameCountX * options.animationFrameCountY);
	uint frameOffsetX = (frameOffset % options.animationFrameCountX) * options.animationFrameStep;
	uint frameOffsetY = (frameOffset / options.animationFrameCountX) * options.animationFrameStep;

	uvec2 tilesetTileSizeInPixels = tileTilesetTileSizesInPixels[options.tilesetIndex];
	uvec2 tilesetTileGapInPixels = tileTilesetTileGapsInPixels[options.tilesetIndex];
	uvec2 tilesetTilePositionInPixels = uvec2(options.tilesetPosition.x + frameOffsetX, options.tilesetPosition.y - frameOffsetY) * (tilesetTileSizeInPixels + tilesetTileGapInPixels);

	ivec2 flip = ivec2(int(options.flipX), int(options.flipY));
	ivec2 flipBias = (ivec2(tilesetTileSizeInPixels) - ivec2(1)) * flip;
	ivec2 flipScale = ivec2(1) - 2 * flip;
	ivec2 subTilePixelCoordinates = clamp(ivec2(subTileCoordinates * vec2(tilesetTileSizeInPixels)) * flipScale + flipBias, ivec2(0), ivec2(tilesetTileSizeInPixels) - ivec2(1));

	ivec2 tilesetPixelCoordinates = ivec2(tilesetTilePositionInPixels) + subTilePixelCoordinates;
	int tilesetMipLevel = int(tileTilesetMipLevels[options.tilesetIndex]);
	return GREM_texelFetch2DArray(tilesetArrayTexture, ivec3(tilesetPixelCoordinates >> ivec2(tilesetMipLevel), int(options.tilesetIndex)), tilesetMipLevel);
}
