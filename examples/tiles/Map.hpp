// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_MAP_HPP
#define GREM_EXAMPLES_TILES_MAP_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/execution.hpp>

#include "Brushes.hpp"
#include "Coordinate.hpp"
#include "Schema.hpp"
#include "Tile.hpp"
#include "TileCategories.hpp"

#include <sstream> // std::istringstream

struct MapPosition {
	Coordinates2D coordinates{};
	int32_t layer = 0;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Offset3D getTileCoordinates() const noexcept {
		return {coordinates.x.getTileCoordinate(), coordinates.y.getTileCoordinate(), layer};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr vec2 getSubTileCoordinates() const noexcept {
		return {coordinates.x.getSubTileCoordinate(), coordinates.y.getSubTileCoordinate()};
	}
};

struct MapOptions {
	Extent2D tileSizeInPixels;
	ArrayList<CategorizedTile> layers;
	MapPosition spawnpoint;
};

class Map {
public:
	static constexpr Extent2D CHUNK_SIZE{512};
	static_assert(isPowerOf2(CHUNK_SIZE.width));
	static_assert(isPowerOf2(CHUNK_SIZE.height));

	// Bit-shift right by this amount to effectively floor-divide a coordinate by CHUNK_SIZE.
	// Also works correctly for negative numbers since C++20 (was implementation-defined before).
	static constexpr u32vec2 CHUNK_BIT_SHIFT{
		countTrailingZeroBits(CHUNK_SIZE.width),
		countTrailingZeroBits(CHUNK_SIZE.height),
	};

	struct VisibleRegion {
		Offset2D tileOffset;
		vec2 subTileOffset;
		vec2 size;
	};

	Map(const MapOptions& options)
		: options(options)
		, layers(options.layers.size()) {
		if (options.layers.empty()) {
			throw Error{"Missing layers."};
		}
	}

	Map(const Filesystem& filesystem, const Schema& schema)
		: options{.tileSizeInPixels{0, 0}, .layers{}, .spawnpoint{}}
		, layers{} {
		try {
			const CStringView filepath = schema.mapFilepaths[schema.mapIndex];
			String fileContents = filesystem.readInputFileString(filepath);
			try {
				std::istringstream stream{std::move(fileContents)};
				json::Reader reader{stream};
				reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
					if (key == "tileSize") {
						options.tileSizeInPixels = Extent2D::from(reader.deserialize<u32vec2>());
					} else if (key == "layers") {
						reader.readCustomArray([&](const json::SourceLocation& source) -> void {
							const json::String tileName = reader.readString();
							if (const auto it = schema.tiles.find(tileName); it != schema.tiles.end()) {
								options.layers.push_back(it->second);
							} else {
								throw json::Error{formatString("Unknown tile type \"{}\".", tileName), source};
							}
						});
					} else if (key == "spawnpoint") {
						const dvec3 spawnpoint = reader.deserialize<dvec3>();
						options.spawnpoint.coordinates.x = spawnpoint.x;
						options.spawnpoint.coordinates.y = spawnpoint.y;
						options.spawnpoint.layer = static_cast<int32_t>(floor(spawnpoint.z));
					}
				});
				if (options.tileSizeInPixels.width == 0 || options.tileSizeInPixels.height == 0) {
					throw Error{"Invalid tile size."};
				}
				if (options.layers.empty()) {
					throw Error{"Missing layers."};
				}
				layers.resize(options.layers.size());
			} catch (...) {
				Error::throwWithNestedFilepath(filepath);
			}
		} catch (...) {
			Error::throwWithNested(Error{"Failed to load map."});
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE CategorizedTile getTile(Offset3D position) const noexcept {
		position.z = clamp(position.z, int32_t{0}, static_cast<int32_t>(layers.size() - 1));
		const Layer& layer = layers[static_cast<size_t>(position.z)];
		return layer.getTile(Offset2D{position.x, position.y}, options.layers[static_cast<size_t>(position.z)]);
	}

	GREM_ALWAYS_INLINE void updateTile(Offset3D position, auto callback) {
		if (position.z < 0 || position.z >= static_cast<int32_t>(layers.size())) {
			[[unlikely]];
			return;
		}
		Layer& layer = layers[static_cast<size_t>(position.z)];
		layer.updateTile(Offset2D{position.x, position.y}, options.layers[static_cast<size_t>(position.z)], callback);
	}

	GREM_ALWAYS_INLINE void setTile(Offset3D position, CategorizedTile newTile) {
		updateTile(position, [newTile](Tile& tile, TileCategoryID& categoryID) -> void {
			tile = newTile.tile;
			categoryID = newTile.categoryID;
		});
	}

	void paint(Offset3D position, const Brush& brush) {
		const i32vec2 origin{floor(brush.origin)};
		for (const Brush::TilePlacement& tilePlacement : brush.tilePlacements) {
			setTile(
				Offset3D{
					position.x + tilePlacement.offset.x - origin.x,
					position.y + tilePlacement.offset.y - origin.y,
					position.z + tilePlacement.offset.z,
				},
				tilePlacement.tile);
		}
	}

	void paintLine(int32_t z, Offset2D startPosition, Offset2D endPosition, const Brush& brush) {
		const i32vec2 start{startPosition};
		const i32vec2 end{endPosition};
		const i32vec2 step = select(greaterThan(end, start), i32vec2{1, 1}, i32vec2{-1, -1});
		const i32vec2 size = abs(end - start);
		i32vec2 p = start;
		int32_t error = size.x - size.y;
		while (true) {
			paint({p.x, p.y, z}, brush);

			const int32_t doubleError = error * 2;
			if (doubleError >= -size.y) {
				if (p.x == end.x) {
					break;
				}
				error -= size.y;
				p.x += step.x;
			}
			if (doubleError <= size.x) {
				if (p.y == end.y) {
					break;
				}
				error += size.x;
				p.y += step.y;
			}
		}
	}

	void markAllTileChunksDirty() {
		GREM_PROFILE_FUNCTION();

		for (Layer& layer : layers) {
			layer.markAllChunksDirty();
		}
	}

	void flushDirtyTileChunksInRegion(const Region3D& region, FunctionView<void(i32vec2 chunkIndices, int32_t z, const Tile* tiles)> callback) {
		GREM_PROFILE_FUNCTION();

		const ChunkKey chunkMin = getChunkKey(Offset2D{region.offset.x, region.offset.y});
		const ChunkKey chunkMax =
			getChunkKey(Offset2D{region.offset.x + static_cast<int32_t>(region.size.width) - 1, region.offset.y + static_cast<int32_t>(region.size.height) - 1});
		const int32_t layersEnd = region.offset.z + static_cast<int32_t>(region.size.depth);
		for (int32_t z = 0; z < layersEnd; ++z) {
			layers[static_cast<size_t>(z)].flushDirtyTiles(chunkMin, chunkMax, z, callback);
		}
	}

	void forEachEntityInRegion(const Region3D& region, FunctionView<void(exec::EntityID id)> callback) const {
		GREM_PROFILE_FUNCTION();

		const ChunkKey chunkMin = getChunkKey(Offset2D{region.offset.x, region.offset.y});
		const ChunkKey chunkMax =
			getChunkKey(Offset2D{region.offset.x + static_cast<int32_t>(region.size.width) - 1, region.offset.y + static_cast<int32_t>(region.size.height) - 1});
		const size_t visibleChunkCount = product(uzvec2{i32vec2{chunkMax - chunkMin}});
		const int32_t layersEnd = region.offset.z + static_cast<int32_t>(region.size.depth);
		for (int32_t z = 0; z < layersEnd; ++z) {
			const Layer& layer = layers[static_cast<size_t>(z)];
			if (visibleChunkCount <= layer.chunksWithEntities.size()) {
				// Iterate all visible chunks, since we're zoomed in enough that the number of visible chunks is smaller than the total number of chunks with entities.
				for (int64_t chunkY = chunkMin.y; chunkY <= chunkMax.y; ++chunkY) {
					for (int64_t chunkX = chunkMin.x; chunkX <= chunkMax.x; ++chunkX) {
						const ChunkKey chunkKey{static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkY)};
						if (const Chunk* const chunk = layer.findChunk(chunkKey)) {
							for (const exec::EntityID id : chunk->entities) {
								callback(id);
							}
						}
					}
				}
			} else {
				// Iterate and check visibility of all chunks with entities, since there were fewer of those in total than the number of visible chunks.
				// This usually happens when zooming out far enough, since the number of visible chunks grows quadratically with decreasing zoom.
				for (const ChunkKey chunkKey : layer.chunksWithEntities) {
					if (chunkKey.x >= chunkMin.x && chunkKey.y >= chunkMin.y && chunkKey.x <= chunkMax.x && chunkKey.y <= chunkMax.y) {
						if (const Chunk* const chunk = layer.findChunk(chunkKey)) {
							for (const exec::EntityID id : chunk->entities) {
								callback(id);
							}
						}
					}
				}
			}
		}
	}

	[[nodiscard]] Extent2D getTileSizeInPixels() const noexcept {
		return options.tileSizeInPixels;
	}

	[[nodiscard]] Span<const CategorizedTile> getLayers() const noexcept {
		return options.layers;
	}

	[[nodiscard]] uint32_t getLayerCount() const noexcept {
		return static_cast<uint32_t>(options.layers.size());
	}

	[[nodiscard]] Optional<Box<2, int32_t>> getLayerChunkIndicesBounds(int32_t z) const {
		return layers[static_cast<size_t>(z)].chunkIndicesBounds;
	}

	[[nodiscard]] MapPosition getSpawnpoint() const noexcept {
		return options.spawnpoint;
	}

	[[nodiscard]] VisibleRegion getVisibleRegion(Coordinates2D centerPosition, vec2 zoomScale, Extent2D viewSizeInPixels) const {
		const Offset2D centerTile{centerPosition.x.getTileCoordinate(), centerPosition.y.getTileCoordinate()};
		const vec2 centerSubTile{centerPosition.x.getSubTileCoordinate(), centerPosition.y.getSubTileCoordinate()};

		const vec2 viewSize = vec2{viewSizeInPixels} / zoomScale / vec2{options.tileSizeInPixels};

		Offset2D viewTileOffset = centerTile;
		vec2 viewSubTileOffset = centerSubTile - viewSize * 0.5f;
		const vec2 viewSubTileOffsetPhase = floor(viewSubTileOffset);
		viewTileOffset += Offset2D{static_cast<int32_t>(viewSubTileOffsetPhase.x), static_cast<int32_t>(viewSubTileOffsetPhase.y)};
		viewSubTileOffset -= viewSubTileOffsetPhase;

		return {.tileOffset = viewTileOffset, .subTileOffset = viewSubTileOffset, .size = viewSize};
	}

	void addEntity(exec::EntityID id, Offset3D position) {
		position.z = clamp(position.z, int32_t{0}, static_cast<int32_t>(layers.size() - 1));

		const ChunkKey chunkKey = getChunkKey(position);
		Layer& layer = layers[static_cast<size_t>(position.z)];
		const auto [itChunk, insertedChunk] = layer.chunks.try_emplace(chunkKey, options.layers[static_cast<size_t>(position.z)]);
		if (itChunk->second.entities.empty()) {
			layer.chunksWithEntities.insert(chunkKey);
		}
		[[maybe_unused]] const auto [itEntity, insertedEntity] = itChunk->second.entities.insert(id);
		GREM_ASSERT(insertedEntity);
	}

	void removeEntity(exec::EntityID id, Offset3D position) {
		position.z = clamp(position.z, int32_t{0}, static_cast<int32_t>(layers.size() - 1));

		const ChunkKey chunkKey = getChunkKey(position);
		Layer& layer = layers[static_cast<size_t>(position.z)];
		Chunk* const chunk = layer.findChunk(chunkKey);
		GREM_ASSERT(chunk);
		[[maybe_unused]] const size_t erasedEntityCount = chunk->entities.erase(id);
		GREM_ASSERT(erasedEntityCount == 1);
		if (chunk->entities.empty()) {
			layer.chunksWithEntities.erase(chunkKey);
		}
	}

	void moveEntity(exec::EntityID id, Offset3D oldPosition, Offset3D newPosition) {
		oldPosition.z = clamp(oldPosition.z, int32_t{0}, static_cast<int32_t>(layers.size() - 1));
		newPosition.z = clamp(newPosition.z, int32_t{0}, static_cast<int32_t>(layers.size() - 1));

		const ChunkKey oldChunkKey = getChunkKey(oldPosition);
		const ChunkKey newChunkKey = getChunkKey(newPosition);
		if (newChunkKey != oldChunkKey || newPosition.z != oldPosition.z) {
			Layer& oldLayer = layers[static_cast<size_t>(oldPosition.z)];
			Chunk* const oldChunk = oldLayer.findChunk(oldChunkKey);
			GREM_ASSERT(oldChunk);
			[[maybe_unused]] const size_t erasedEntityCount = oldChunk->entities.erase(id);
			GREM_ASSERT(erasedEntityCount == 1);
			if (oldChunk->entities.empty()) {
				oldLayer.chunksWithEntities.erase(oldChunkKey);
			}
			Layer& newLayer = layers[static_cast<size_t>(newPosition.z)];
			const auto [itNewChunk, insertedChunk] = newLayer.chunks.try_emplace(newChunkKey, options.layers[static_cast<size_t>(newPosition.z)]);
			if (itNewChunk->second.entities.empty()) {
				newLayer.chunksWithEntities.insert(newChunkKey);
			}
			[[maybe_unused]] const auto [itNewEntity, insertedNewEntity] = itNewChunk->second.entities.insert(id);
			GREM_ASSERT(insertedNewEntity);
		}
	}

private:
	struct ChunkKey : Offset2D {
		struct Hash {
			[[nodiscard]] size_t operator()(const ChunkKey& v) const {
				return getHash(v.x, v.y);
			}
		};

		[[nodiscard]] bool operator==(const ChunkKey&) const = default;
	};

	struct Chunk {
		struct Data {
			using Tiles = Array<Tile, CHUNK_SIZE.width * CHUNK_SIZE.height>;
			using TileCategoryIDs = Array<TileCategoryID, CHUNK_SIZE.width * CHUNK_SIZE.height>;

			Tiles tiles;
			TileCategoryIDs tileCategoryIDs;
		};

		UniquePointer<Data> data = UniquePointer<Data>::create();
		HashSet<exec::EntityID> entities{};
		bool tilesDirty = false;

		explicit Chunk(CategorizedTile defaultTile) {
			data->tiles.fill(defaultTile.tile);
			data->tileCategoryIDs.fill(defaultTile.categoryID);
		}

		[[nodiscard]] GREM_ALWAYS_INLINE CategorizedTile getTileUnsafe(Offset2D positionInChunk) const {
			const uzvec2 indices{i32vec2{positionInChunk}};
			const size_t i = indices.y * size_t{CHUNK_SIZE.width} + indices.x;
			return {data->tiles[i], data->tileCategoryIDs[i]};
		}

		GREM_ALWAYS_INLINE void updateTileUnsafe(Offset2D positionInChunk, auto callback) {
			tilesDirty = true;
			const uzvec2 indices{i32vec2{positionInChunk}};
			const size_t i = indices.y * size_t{CHUNK_SIZE.width} + indices.x;
			callback(data->tiles[i], data->tileCategoryIDs[i]);
		}

		void flushTiles(ChunkKey chunkKey, int32_t z, FunctionView<void(i32vec2 chunkIndices, int32_t z, const Tile* tiles)> callback) {
			if (!tilesDirty) {
				return;
			}

			GREM_PROFILE_FUNCTION();

			callback(i32vec2{chunkKey.x, chunkKey.y}, z, data->tiles.data());

			tilesDirty = false;
		}
	};

	struct Layer {
		Optional<Box<2, int32_t>> chunkIndicesBounds{};
		HashMap<ChunkKey, Chunk, ChunkKey::Hash> chunks{};
		HashSet<ChunkKey, ChunkKey::Hash> chunksWithDirtyTiles{};
		HashSet<ChunkKey, ChunkKey::Hash> chunksWithEntities{};

		[[nodiscard]] GREM_ALWAYS_INLINE Chunk* findChunk(ChunkKey chunkKey) noexcept {
			if (const auto it = chunks.find(chunkKey); it != chunks.end()) {
				return &it->second;
			}
			return nullptr;
		}

		[[nodiscard]] GREM_ALWAYS_INLINE const Chunk* findChunk(ChunkKey chunkKey) const noexcept {
			if (const auto it = chunks.find(chunkKey); it != chunks.end()) {
				return &it->second;
			}
			return nullptr;
		}

		[[nodiscard]] GREM_ALWAYS_INLINE CategorizedTile getTile(Offset2D position, const CategorizedTile& defaultTile) const noexcept {
			const ChunkKey chunkKey = getChunkKey(position);
			if (const Chunk* const chunk = findChunk(chunkKey)) {
				return chunk->getTileUnsafe(getPositionInChunk(chunkKey, position));
			}
			return defaultTile;
		}

		GREM_ALWAYS_INLINE void updateTile(Offset2D position, const CategorizedTile& defaultTile, auto callback) {
			const ChunkKey chunkKey = getChunkKey(position);
			if (chunkIndicesBounds) {
				chunkIndicesBounds->min = min(chunkIndicesBounds->min, i32vec2{chunkKey});
				chunkIndicesBounds->max = max(chunkIndicesBounds->max, i32vec2{chunkKey});
			} else {
				chunkIndicesBounds.emplace(Box<2, int32_t>{.min{chunkKey}, .max{chunkKey}});
			}
			const auto [it, inserted] = chunks.try_emplace(chunkKey, defaultTile);
			if (!it->second.tilesDirty) {
				chunksWithDirtyTiles.insert(chunkKey);
			}
			it->second.updateTileUnsafe(getPositionInChunk(chunkKey, position), callback);
		}

		void markAllChunksDirty() {
			GREM_PROFILE_FUNCTION();

			for (auto&& [chunkKey, chunk] : chunks) {
				if (!chunk.tilesDirty) {
					chunksWithDirtyTiles.insert(chunkKey);
					chunk.tilesDirty = true;
				}
			}
		}

		void flushDirtyTiles(ChunkKey chunkMin, ChunkKey chunkMax, int32_t z, FunctionView<void(i32vec2 chunkIndices, int32_t z, const Tile* tiles)> callback) {
			GREM_PROFILE_FUNCTION();

			const size_t visibleChunkCount = product(uzvec2{i32vec2{chunkMax - chunkMin}});
			if (visibleChunkCount < chunksWithDirtyTiles.size()) {
				for (int64_t chunkY = chunkMin.y; chunkY <= chunkMax.y; ++chunkY) {
					for (int64_t chunkX = chunkMin.x; chunkX <= chunkMax.x; ++chunkX) {
						const ChunkKey chunkKey{static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkY)};
						if (Chunk* const chunk = findChunk(chunkKey)) {
							chunk->flushTiles(chunkKey, z, callback);
							chunksWithDirtyTiles.erase(chunkKey);
						}
					}
				}
			} else {
				for (auto it = chunksWithDirtyTiles.begin(); it != chunksWithDirtyTiles.end();) {
					const ChunkKey chunkKey = *it;
					if (chunkKey.x >= chunkMin.x && chunkKey.y >= chunkMin.y && chunkKey.x <= chunkMax.x && chunkKey.y <= chunkMax.y) {
						if (Chunk* const chunk = findChunk(chunkKey)) {
							chunk->flushTiles(chunkKey, z, callback);
							it = chunksWithDirtyTiles.erase(it);
							continue;
						}
					}
					++it;
				}
			}
		}
	};

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr ChunkKey getChunkKey(Offset3D position) {
		return {
			position.x >> CHUNK_BIT_SHIFT.x,
			position.y >> CHUNK_BIT_SHIFT.y,
		};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Offset2D getPositionInChunk(ChunkKey chunkKey, Offset3D position) {
		return {
			position.x - chunkKey.x * static_cast<int32_t>(CHUNK_SIZE.width),
			position.y - chunkKey.y * static_cast<int32_t>(CHUNK_SIZE.height),
		};
	}

	MapOptions options;
	ArrayList<Layer> layers;
};

#endif
