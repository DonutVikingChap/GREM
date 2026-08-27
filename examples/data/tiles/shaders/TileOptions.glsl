#pragma once

struct TileOptions {
	uvec2 tilesetPosition;
	uint tilesetIndex;
	bool flipX;
	bool flipY;
	uint animationFrameStep;
	uint animationFrameCountX;
	uint animationFrameCountY;
	uint animationFrameRate;
};

TileOptions TileOptions_decode(uint value) {
	uint tilesetX = value & 0x3Fu;
	uint tilesetY = (value >> 6) & 0x3Fu;
	uint tilesetZ = (value >> 12) & 0xFu;
	uint flipX = (value >> 16) & 0x1u;
	uint flipY = (value >> 17) & 0x1u;
	uint animationFrameStepMinusOne = (value >> 18) & 0x3u;
	uint animationFrameCountXMinusOne = (value >> 20) & 0xFu;
	uint animationFrameCountYMinusOne = (value >> 24) & 0xFu;
	uint animationFrameRateDividedBy3MinusOne = (value >> 28) & 0xFu;

	TileOptions result;
	result.tilesetPosition = uvec2(tilesetX, tilesetY);
	result.tilesetIndex = tilesetZ;
	result.flipX = flipX != 0u;
	result.flipY = flipY != 0u;
	result.animationFrameStep = animationFrameStepMinusOne + 1u;
	result.animationFrameCountX = animationFrameCountXMinusOne + 1u;
	result.animationFrameCountY = animationFrameCountYMinusOne + 1u;
	result.animationFrameRate = (animationFrameRateDividedBy3MinusOne + 1u) * 3u;
	return result;
}
