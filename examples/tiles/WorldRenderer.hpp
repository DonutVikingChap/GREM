// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_WORLD_RENDERER_HPP
#define GREM_EXAMPLES_TILES_WORLD_RENDERER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>
#include <GREM/resource.hpp>

#include "Brushes.hpp"
#include "Coordinate.hpp"
#include "Map.hpp"
#include "Schema.hpp"
#include "Tile.hpp"
#include "Tilesets.hpp"
#include "World.hpp"
#include "shaders.hpp"

#include <utility> // std::move

class WorldRenderer {
public:
	WorldRenderer(const Filesystem& filesystem, gfx::Device& device)
		: tileShaderParameterBuffer(device)
		, tilemapShaderParameterBuffer(device)
		, tilemapShaderChunkBuffer(device)
		, tileShaderPipeline(loadTileShaderPipeline(filesystem, device))
		, tilemapShaderPipeline(loadTilemapShaderPipeline(filesystem, device))
		, dummyChunkTextures(gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D_ARRAY, TILEMAP_TEXTURE_FORMAT, Extent3D{1, 1, 1}, 1, gfx::ClearValues{},
			  gfx::TextureSamplerOptions::UNFILTERED))
		, tileMesh(device, TILE_MESH_VERTICES)
		, tilemapMesh(device, TILE_MESH_VERTICES)
		, tileInstanceBuffer(device)
		, tileDrawCommandBuffer(device)
		, tilemapDrawCommandBuffer(device) {
		tilemapDrawCommandBuffer.push(tilemapShaderPipeline, tilemapMesh);
	}

	void drawWorld(gfx::Device& device, gfx::RenderPass& renderPass, World& world, const Map::VisibleRegion& visibleRegion, vec2 zoomScale, float animationTime,
		float interpolationAlpha) {
		GREM_PROFILE_FUNCTION();

		const auto entities = world.registry.getEntities<const World::Position, const World::PreviousPosition, const World::Sprite>();
		const Schema& schema = world.resources.getResource<Schema>();
		Map& map = world.resources.getResource<Map>();

		const uint32_t layerCount = map.getLayerCount();
		const Region3D visibleTileRegion{
			.offset{
				.x = visibleRegion.tileOffset.x,
				.y = visibleRegion.tileOffset.y,
				.z = 0,
			},
			.size{
				.width = 1 + static_cast<uint32_t>(max(int32_t{0}, static_cast<int32_t>(ceil(visibleRegion.size.x)))),
				.height = 1 + static_cast<uint32_t>(max(int32_t{0}, static_cast<int32_t>(ceil(visibleRegion.size.y)))),
				.depth = layerCount,
			},
		};

		tilemapLayers.resize(layerCount);
		for (uint32_t z = 0; z < layerCount; ++z) {
			TilemapLayer& tilemapLayer = tilemapLayers[static_cast<size_t>(z)];
			const Optional<Box<2, int32_t>> newChunkIndicesBounds = map.getLayerChunkIndicesBounds(static_cast<int32_t>(z));
			const Optional<Box<2, int32_t>> oldChunkIndicesBounds = tilemapLayer.chunkIndicesBounds;
			if (newChunkIndicesBounds) {
				if (oldChunkIndicesBounds) {
					if (any(lessThan(newChunkIndicesBounds->min, oldChunkIndicesBounds->min) | greaterThan(newChunkIndicesBounds->max, oldChunkIndicesBounds->max))) {
						const uzvec2 newChunkIndicesExtents{u32vec2{newChunkIndicesBounds->max} - u32vec2{newChunkIndicesBounds->min} + u32vec2{1}};
						const uzvec2 oldChunkIndicesExtents{u32vec2{oldChunkIndicesBounds->max} - u32vec2{oldChunkIndicesBounds->min} + u32vec2{1}};
						const uzvec2 offset{u32vec2{oldChunkIndicesBounds->min} - u32vec2{newChunkIndicesBounds->min}};
						Allocation<TilemapShaderChunkFields> newChunkTextureIndices(newChunkIndicesExtents.x * newChunkIndicesExtents.y,
							TilemapShaderChunkFields{.tilemapChunkTextureIndex = 0xFFFFFFFF});
						for (size_t y = 0; y < oldChunkIndicesExtents.y; ++y) {
							for (size_t x = 0; x < oldChunkIndicesExtents.x; ++x) {
								newChunkTextureIndices[(offset.y + y) * newChunkIndicesExtents.x + (offset.x + x)] =
									tilemapLayer.chunkTextureIndices[y * oldChunkIndicesExtents.x + x];
							}
						}
						tilemapLayer.chunkIndicesBounds = *newChunkIndicesBounds;
						tilemapLayer.chunkTextureIndices = std::move(newChunkTextureIndices);
					}
				} else {
					const uzvec2 newChunkIndicesExtents{u32vec2{newChunkIndicesBounds->max} - u32vec2{newChunkIndicesBounds->min} + u32vec2{1}};
					tilemapLayer.chunkIndicesBounds = *newChunkIndicesBounds;
					tilemapLayer.chunkTextureIndices =
						Allocation<TilemapShaderChunkFields>(newChunkIndicesExtents.x * newChunkIndicesExtents.y, TilemapShaderChunkFields{.tilemapChunkTextureIndex = 0xFFFFFFFF});
				}
			}
		}

		map.flushDirtyTileChunksInRegion(visibleTileRegion, [&](i32vec2 chunkIndices, int32_t z, const Tile* tiles) -> void {
			const size_t layer = static_cast<size_t>(z);
			TilemapLayer& tilemapLayer = tilemapLayers[layer];
			const uzvec2 chunkIndicesExtents{u32vec2{tilemapLayer.chunkIndicesBounds->max} - u32vec2{tilemapLayer.chunkIndicesBounds->min} + u32vec2{1}};
			const uzvec2 relativeChunkIndices{u32vec2{chunkIndices} - u32vec2{tilemapLayer.chunkIndicesBounds->min}};
			uint32_t& chunkTextureIndex = tilemapLayer.chunkTextureIndices[relativeChunkIndices.y * chunkIndicesExtents.x + relativeChunkIndices.x].tilemapChunkTextureIndex;
			if (chunkTextureIndex == uint32_t{0xFFFFFFFF}) {
				if (tilemapLayer.nextChunkTextureIndex >= tilemapLayer.chunkTextures.getDepth()) {
					const uint32_t newCapacity = max(roundUpToPowerOf2(tilemapLayer.nextChunkTextureIndex + 1), uint32_t{16});
					const Tile defaultTile = map.getLayers()[layer].tile;
					gfx::Texture newChunkTextures = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D_ARRAY, TILEMAP_TEXTURE_FORMAT,
						Extent3D{Map::CHUNK_SIZE.width, Map::CHUNK_SIZE.height, newCapacity}, 1,
						gfx::ClearValues{.color = Color::fromSRGB(bit_cast<u8vec4norm>(defaultTile.value))}, gfx::TextureSamplerOptions::UNFILTERED);
					if (tilemapLayer.chunkTextures) {
						newChunkTextures.pasteTexture(tilemapLayer.chunkTextures);
					}
					tilemapLayer.chunkTextures = std::move(newChunkTextures);
				}
				chunkTextureIndex = tilemapLayer.nextChunkTextureIndex++;
			}
			tilemapLayer.chunkTextures.pasteImage(Map::CHUNK_SIZE, tiles, Offset3D{0, 0, static_cast<int32_t>(chunkTextureIndex)});
		});

		Array<u32vec2, Tilesets::MAX_TILESET_COUNT> tilesetTileSizesInPixels{};
		Array<u32vec2, Tilesets::MAX_TILESET_COUNT> tilesetTileGapsInPixels{};
		Array<uint32_t, Tilesets::MAX_TILESET_COUNT> tilesetMipLevels{};
		for (Tilesets::Index tilesetIndex = 0; tilesetIndex < schema.tilesets.size(); ++tilesetIndex) {
			const Tilesets::TilesetInfo& tilesetInfo = schema.tilesets[tilesetIndex];
			uint32_t mipLevel = 0;
			if (tilesetInfo.useMipmap) {
				const uint32_t maxMipLevelCount = min(res::Image::getMaxMipLevelCount(tilesetInfo.tilesetSizeInPixels),
					res::Image::getMaxMipLevelCount(tilesetInfo.tileSizeInPixels + tilesetInfo.tileGapInPixels));
				mipLevel = min(static_cast<uint32_t>(log2(max(1.0f / minComponent(zoomScale), 1.0f))), static_cast<uint32_t>(maxMipLevelCount - 1));
			}
			tilesetTileSizesInPixels[tilesetIndex] = tilesetInfo.tileSizeInPixels;
			tilesetTileGapsInPixels[tilesetIndex] = tilesetInfo.tileGapInPixels;
			tilesetMipLevels[tilesetIndex] = mipLevel;
		}
		for (Tilesets::Index tilesetIndex = schema.tilesets.size(); tilesetIndex < Tilesets::MAX_TILESET_COUNT; ++tilesetIndex) {
			tilesetTileSizesInPixels[tilesetIndex] = Extent2D{1, 1};
			tilesetTileGapsInPixels[tilesetIndex] = Extent2D{0, 0};
			tilesetMipLevels[tilesetIndex] = 0;
		}

		tileShaderParameterBuffer.upload({
			.tilesetArrayTexture = schema.tilesets.getTilesetArrayTexture(),
			.tileTilesetTileSizesInPixels = tilesetTileSizesInPixels,
			.tileTilesetTileGapsInPixels = tilesetTileGapsInPixels,
			.tileTilesetMipLevels = tilesetMipLevels,
			.tileAnimationTime = animationTime,
			.tileBaseOffset = visibleRegion.tileOffset,
			.tileSubTileOffset = visibleRegion.subTileOffset,
			.tileViewSize = visibleRegion.size,
		});

		const Region3D visibleEntityRegion{
			.offset{
				visibleTileRegion.offset.x - int32_t{VISIBLE_ENTITY_REGION_EXPANSION},
				visibleTileRegion.offset.y - int32_t{VISIBLE_ENTITY_REGION_EXPANSION},
				visibleTileRegion.offset.z,
			},
			.size{
				visibleTileRegion.size.width + VISIBLE_ENTITY_REGION_EXPANSION * 2,
				visibleTileRegion.size.height + VISIBLE_ENTITY_REGION_EXPANSION * 2,
				visibleTileRegion.size.depth,
			},
		};

		objectInstances.clear();
		map.forEachEntityInRegion(visibleEntityRegion, [&](exec::EntityID id) -> void {
			if (entities.containsEntity(id)) {
				const auto& [entityID, newPosition, oldPosition, sprite] = entities[id];
				if (const auto itBrush = schema.brushes.find(sprite.brushID); itBrush != schema.brushes.end()) {
					const Coordinates2D coordinates = mix(oldPosition.coordinates, newPosition.coordinates, interpolationAlpha);
					objectInstances.push_back({
						.position{coordinates, newPosition.layer},
						.brushIndex = sprite.brushID.getIndex(),
					});
				}
			}
		});
		sort(objectInstances, ObjectInstance::Compare{});

		size_t objectInstancesOffset = 0;
		const int32_t visibleLayersBegin = visibleTileRegion.offset.z;
		const int32_t visibleLayersEnd = visibleTileRegion.offset.z + static_cast<int32_t>(visibleTileRegion.size.depth);
		for (int32_t z = visibleLayersBegin; z < visibleLayersEnd; ++z) {
			GREM_ASSERT(z >= 0);
			const size_t layer = static_cast<size_t>(z);
			const TilemapLayer& tilemapLayer = tilemapLayers[layer];
			tilemapShaderParameterBuffer.upload({
				.tilemapChunkTextures = (tilemapLayer.nextChunkTextureIndex == 0) ? dummyChunkTextures : tilemapLayer.chunkTextures,
				.tilemapChunkIndicesMin = (tilemapLayer.chunkIndicesBounds) ? tilemapLayer.chunkIndicesBounds->min : i32vec2{0},
				.tilemapChunkIndicesMax = (tilemapLayer.chunkIndicesBounds) ? tilemapLayer.chunkIndicesBounds->max : i32vec2{-1},
				.tilemapDefaultTile = map.getLayers()[layer].tile.value,
			});
			tilemapShaderChunkBuffer.upload(tilemapLayer.chunkTextureIndices);
			renderPass.draw(tilemapDrawCommandBuffer, tileShaderParameterBuffer, tilemapShaderParameterBuffer, tilemapShaderChunkBuffer);

			const auto [objectInstancesBegin, objectInstancesEnd] = equalRange(Span{objectInstances}.subspan(objectInstancesOffset), z, ObjectInstance::Compare{});
			objectInstancesOffset = static_cast<size_t>(objectInstancesEnd - objectInstances.begin());
			if (objectInstancesBegin != objectInstancesEnd) {
				tileInstances.clear();
				for (const ObjectInstance& objectInstance : Subrange{objectInstancesBegin, objectInstancesEnd}) {
					const Offset3D tileCoordinates = objectInstance.position.getTileCoordinates();
					const vec2 subTileCoordinates = objectInstance.position.getSubTileCoordinates();
					const auto& [brushID, brush] = schema.brushes[objectInstance.brushIndex];
					for (const Brush::TilePlacement& tilePlacement : brush.tilePlacements) {
						const auto [tileOffsetX, subTileOffsetX] = Coordinate::canonical(tileCoordinates.x + tilePlacement.offset.x, subTileCoordinates.x - brush.origin.x);
						const auto [tileOffsetY, subTileOffsetY] = Coordinate::canonical(tileCoordinates.y + tilePlacement.offset.y, subTileCoordinates.y - brush.origin.y);
						tileInstances.push_back({
							.instanceTileOffset{tileOffsetX, tileOffsetY},
							.instanceSubTileOffset{subTileOffsetX, subTileOffsetY},
							.instanceTile = tilePlacement.tile.tile.value,
						});
					}
				}
				tileInstanceBuffer.clear();
				tileInstanceBuffer.append(tileInstances);
				tileDrawCommandBuffer.clear();
				tileDrawCommandBuffer.append(tileShaderPipeline, tileMesh, 0, tileInstanceBuffer.size());
				renderPass.draw(tileDrawCommandBuffer, tileInstanceBuffer, tileShaderParameterBuffer);
			}
		}

		// Release tilemapChunkTextures to make it reusable.
		tilemapShaderParameterBuffer.upload({.tilemapChunkTextures{}, .tilemapChunkIndicesMin{}, .tilemapChunkIndicesMax{}, .tilemapDefaultTile = 0});
	}

	void reloadShaders(const Filesystem& filesystem, gfx::Device& device) {
		GREM_PROFILE_FUNCTION();

		try {
			tileShaderPipeline = loadTileShaderPipeline(filesystem, device);
			tilemapShaderPipeline = loadTilemapShaderPipeline(filesystem, device);
		} catch (const gfx::Error& e) {
			eprintln("Failed to reload shaders:\n", e.what());
			return;
		}
		tilemapDrawCommandBuffer.clear();
		tilemapDrawCommandBuffer.push(tilemapShaderPipeline, tilemapMesh);
		eprintln("Shaders reloaded!");
	}

private:
	static constexpr uint32_t VISIBLE_ENTITY_REGION_EXPANSION = 64; // Should cover the maximum entity radius plus the maximum relative entity motion per tick.
	static constexpr res::ImageFormat TILEMAP_IMAGE_FORMAT = res::ImageFormat::R8G8B8A8_UINT;
	static constexpr gfx::TextureFormat TILEMAP_TEXTURE_FORMAT = gfx::Texture::getInternalFormat(TILEMAP_IMAGE_FORMAT, Color::TransferFunction::LINEAR);
	static_assert(sizeof(Tile) == res::Image::getPixelStride(TILEMAP_IMAGE_FORMAT));
	static_assert(alignof(Tile) >= alignof(float));

	static constexpr Array TILE_MESH_VERTICES{
		TileVertex{.vertexPosition{0.0f, 0.0f}},
		TileVertex{.vertexPosition{1.0f, 0.0f}},
		TileVertex{.vertexPosition{0.0f, 1.0f}},
		TileVertex{.vertexPosition{1.0f, 1.0f}},
	};

	static constexpr gfx::ShaderPipelineOptions TILE_SHADER_PIPELINE_OPTIONS{
		.depthBufferMode = gfx::DepthBufferMode::NONE,
		.primitiveType = gfx::PrimitiveType::TRIANGLE_STRIP,
		.faceCullingMode = gfx::FaceCullingMode::NONE,
		.blendState = gfx::BlendState::ALPHA_BLENDING_PREMULTIPLIED,
	};

	[[nodiscard]] static TileShaderPipeline loadTileShaderPipeline(const Filesystem& filesystem, gfx::Device& device) {
		return TileShaderPipeline{
			device,
			TileVertexShader{device, filesystem, "shaders/tile.vert"},
			TileVertexShaderConstants{},
			TileFragmentShader{device, filesystem, "shaders/tile.frag"},
			TileFragmentShaderConstants{},
			TILE_SHADER_PIPELINE_OPTIONS,
		};
	}

	[[nodiscard]] static TilemapShaderPipeline loadTilemapShaderPipeline(const Filesystem& filesystem, gfx::Device& device) {
		return TilemapShaderPipeline{
			device,
			TilemapVertexShader{device, filesystem, "shaders/tilemap.vert"},
			TileVertexShaderConstants{},
			TilemapFragmentShader{device, filesystem, "shaders/tilemap.frag"},
			TilemapFragmentShaderConstants{
				.TILEMAP_CHUNK_WIDTH = static_cast<int32_t>(Map::CHUNK_SIZE.width),
				.TILEMAP_CHUNK_HEIGHT = static_cast<int32_t>(Map::CHUNK_SIZE.height),
				.TILEMAP_CHUNK_BIT_SHIFT_X = static_cast<int32_t>(Map::CHUNK_BIT_SHIFT.x),
				.TILEMAP_CHUNK_BIT_SHIFT_Y = static_cast<int32_t>(Map::CHUNK_BIT_SHIFT.y),
			},
			TILE_SHADER_PIPELINE_OPTIONS,
		};
	}

	struct ObjectInstance {
		struct Compare {
			[[nodiscard]] bool operator()(const ObjectInstance& a, const ObjectInstance& b) const {
				return Pair{a.position.layer, -a.position.coordinates.y} < Pair{b.position.layer, -b.position.coordinates.y};
			}

			[[nodiscard]] bool operator()(const ObjectInstance& a, int32_t z) const {
				return a.position.layer < z;
			}

			[[nodiscard]] bool operator()(int32_t z, const ObjectInstance& b) const {
				return z < b.position.layer;
			}
		};

		World::Position position;
		BrushID::Index brushIndex;
	};

	struct TilemapLayer {
		gfx::Texture chunkTextures{};
		Optional<Box<2, int32_t>> chunkIndicesBounds{};
		Allocation<TilemapShaderChunkFields> chunkTextureIndices{
			TilemapShaderChunkFields{.tilemapChunkTextureIndex = 0xFFFFFFFF},
		};
		uint32_t nextChunkTextureIndex = 0;
	};

	TileShaderParameterBuffer tileShaderParameterBuffer;
	TilemapShaderParameterBuffer tilemapShaderParameterBuffer;
	TilemapShaderChunkBuffer tilemapShaderChunkBuffer;
	TileShaderPipeline tileShaderPipeline;
	TilemapShaderPipeline tilemapShaderPipeline;
	ArrayList<TilemapLayer> tilemapLayers{};
	gfx::Texture dummyChunkTextures;
	TileMesh tileMesh;
	TilemapMesh tilemapMesh;
	ArrayList<ObjectInstance> objectInstances{};
	ArrayList<TileInstance> tileInstances{};
	gfx::InstanceBuffer<TileInstance> tileInstanceBuffer;
	gfx::DrawCommandBuffer<TileMesh> tileDrawCommandBuffer;
	gfx::DrawCommandBuffer<TilemapMesh> tilemapDrawCommandBuffer;
};

#endif
