// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_SHADERS_HPP
#define GREM_EXAMPLES_TILES_SHADERS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

#include "Tilesets.hpp"

struct TileVertex {
	vec2 vertexPosition;
};

struct TileInstance {
	i32vec2 instanceTileOffset;
	vec2 instanceSubTileOffset;
	uint32_t instanceTile;
};

using TileMesh = gfx::Mesh<TileVertex, gfx::NoIndex, gfx::NoParameters, TileInstance>;
using TilemapMesh = gfx::Mesh<TileVertex>;

struct TileVertexShaderConstants {};

struct TileShaderParameters {
	gfx::sampler2DArray tilesetArrayTexture;
	Array<u32vec2, Tilesets::MAX_TILESET_COUNT> tileTilesetTileSizesInPixels;
	Array<u32vec2, Tilesets::MAX_TILESET_COUNT> tileTilesetTileGapsInPixels;
	Array<uint32_t, Tilesets::MAX_TILESET_COUNT> tileTilesetMipLevels;
	float tileAnimationTime;
	i32vec2 tileBaseOffset;
	vec2 tileSubTileOffset;
	vec2 tileViewSize;
};

using TileShaderParameterBuffer = gfx::UniformBuffer<TileShaderParameters, "TileShaderParameters">;

struct TileVertexShaderOutputs {
	vec2 fragmentSubTileCoordinates;
	uint32_t fragmentTile;
};

struct TilemapVertexShaderOutputs {
	vec2 fragmentViewCoordinates;
};

using TileVertexShader = gfx::VertexShader<TileMesh, TileVertexShaderConstants, TileVertexShaderOutputs, TileShaderParameterBuffer>;
using TilemapVertexShader = gfx::VertexShader<TilemapMesh, TileVertexShaderConstants, TilemapVertexShaderOutputs, TileShaderParameterBuffer>;

struct TileFragmentShaderConstants {};

struct TileFragmentShaderOutputs {
	vec4 outputColor;
};

struct TilemapFragmentShaderConstants {
	int32_t TILEMAP_CHUNK_WIDTH;
	int32_t TILEMAP_CHUNK_HEIGHT;
	int32_t TILEMAP_CHUNK_BIT_SHIFT_X;
	int32_t TILEMAP_CHUNK_BIT_SHIFT_Y;
};

struct TilemapShaderParameters {
	gfx::sampler2DArray tilemapChunkTextures;
	i32vec2 tilemapChunkIndicesMin;
	i32vec2 tilemapChunkIndicesMax;
	uint32_t tilemapDefaultTile;
};
using TilemapShaderParameterBuffer = gfx::UniformBuffer<TilemapShaderParameters, "TilemapShaderParameters">;

struct TilemapShaderChunkFields {
	uint32_t tilemapChunkTextureIndex;
};
using TilemapShaderChunkBuffer = gfx::StorageBuffer<TilemapShaderChunkFields, "TilemapShaderChunks">;

using TileFragmentShader = gfx::FragmentShader<TileMesh, TileVertexShaderOutputs, TileFragmentShaderConstants, TileFragmentShaderOutputs, TileShaderParameterBuffer>;
using TilemapFragmentShader = gfx::FragmentShader<TilemapMesh, TilemapVertexShaderOutputs, TilemapFragmentShaderConstants, TileFragmentShaderOutputs, TileShaderParameterBuffer,
	TilemapShaderParameterBuffer, TilemapShaderChunkBuffer>;

using TileShaderPipeline = gfx::ShaderPipeline<TileMesh>;
using TilemapShaderPipeline = gfx::ShaderPipeline<TilemapMesh>;

#endif
