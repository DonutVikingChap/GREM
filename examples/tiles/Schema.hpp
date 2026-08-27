// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_SCHEMA_HPP
#define GREM_EXAMPLES_TILES_SCHEMA_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>

#include "Brushes.hpp"
#include "Tile.hpp"
#include "TileCategories.hpp"
#include "Tilesets.hpp"

#include <sstream> // std::istringstream
#include <utility> // std::move

struct Schema {
	using WorldSeed = rng::Xoroshiro128PlusPlusEngine::result_type;

	struct PaintableBrush {
		String name;
		BrushID brushID;
		Color previewColor;
	};

	WorldSeed worldSeed;
	ArrayList<String> mapFilepaths{};
	size_t mapIndex = 0;
	TileCategories tileCategories{};
	Tilesets tilesets;
	Brushes brushes{};
	HashMap<String, TileCategoryID> tileCategoryIDs{};
	HashMap<String, Tilesets::Index> tilesetIndices{};
	HashMap<String, CategorizedTile> tiles{};
	HashMap<String, BrushID> brushIDs{};
	ArrayList<PaintableBrush> paintableBrushes{};

	Schema(gfx::Device& device, const Filesystem& filesystem, CStringView filepath, Extent2D maxTilesetSizeInPixels, Optional<WorldSeed> worldSeed)
		: worldSeed((worldSeed) ? *worldSeed : static_cast<WorldSeed>(rng::NonDeterministicRandomEngine{}()))
		, tilesets(device, {.maxTilesetSizeInPixels = maxTilesetSizeInPixels}) {
		try {
			Loader loader{mapFilepaths, tileCategories, tilesets, brushes};
			loader.extend(*this, filesystem, filepath);
			if (mapFilepaths.empty()) {
				throw Error{"No maps in schema."};
			}
			loader.resolveResourceReferences(*this);
			loadPaintableBrushes();
		} catch (...) {
			Error::throwWithNested(Error{"Failed to load schema."});
		}
	}

private:
	void loadPaintableBrushes() {
		const res::Image tilesetArrayImage = tilesets.getTilesetArrayTexture().downloadImage();
		GREM_ASSERT(tilesetArrayImage.getFormat() == Tilesets::IMAGE_FORMAT);
		static_assert(Tilesets::IMAGE_FORMAT == res::ImageFormat::R8G8B8A8_UINT);
		const auto getTileCenterPixel = [&](Tile tile) -> Color {
			const u8vec2 tilesetPosition = tile.getTilesetPosition();
			const Tilesets::Index tilesetIndex = tile.getTilesetIndex();
			const Tilesets::TilesetInfo& tilesetInfo = tilesets[tilesetIndex];
			const u32vec2 tilePositionInPixels = u32vec2{tilesetPosition} * u32vec2{tilesetInfo.tileSizeInPixels + tilesetInfo.tileGapInPixels};
			const u32vec2 tileCenterInPixels = tilePositionInPixels + u32vec2{tilesetInfo.tileSizeInPixels / 2};
			const int32_t x = static_cast<int32_t>(tileCenterInPixels.x);
			const int32_t y = static_cast<int32_t>(tilesetArrayImage.getHeight() - 1 - tileCenterInPixels.y);
			const int32_t z = static_cast<int32_t>(tilesetIndex);
			const u8vec4norm pixel = tilesetArrayImage.readPixel<u8vec4norm>({x, y, z});
			const u8vec3norm rgb{pixel};
			const u8vec4norm rgba = (rgb == u8vec3norm{}) ? u8vec4norm{1.0f} : u8vec4norm{rgb, 1.0f};
			return Color::fromSRGB(rgba);
		};

		for (const auto& [tileName, tile] : tiles) {
			paintableBrushes.push_back({
				.name = tileName + " (single)",
				.brushID = brushes.insert(Brush{.origin{0.5f, 0.5f}, .tilePlacements{{.offset{0, 0, 0}, .tile = tile}}})->first,
				.previewColor = getTileCenterPixel(tile.tile),
			});

			paintableBrushes.push_back({
				.name = tileName + " (patch)",
				.brushID = brushes
			        .insert(Brush{
						.origin{0.5f, 0.5f},
						.tilePlacements{
							{.offset{-1, -3, 0}, .tile = tile},
							{.offset{0, -3, 0}, .tile = tile},
							{.offset{1, -3, 0}, .tile = tile},

							{.offset{-2, -2, 0}, .tile = tile},
							{.offset{-1, -2, 0}, .tile = tile},
							{.offset{0, -2, 0}, .tile = tile},
							{.offset{1, -2, 0}, .tile = tile},
							{.offset{2, -2, 0}, .tile = tile},

							{.offset{-3, -1, 0}, .tile = tile},
							{.offset{-2, -1, 0}, .tile = tile},
							{.offset{-1, -1, 0}, .tile = tile},
							{.offset{0, -1, 0}, .tile = tile},
							{.offset{1, -1, 0}, .tile = tile},
							{.offset{2, -1, 0}, .tile = tile},
							{.offset{3, -1, 0}, .tile = tile},

							{.offset{-3, 0, 0}, .tile = tile},
							{.offset{-2, 0, 0}, .tile = tile},
							{.offset{-1, 0, 0}, .tile = tile},
							{.offset{0, 0, 0}, .tile = tile},
							{.offset{1, 0, 0}, .tile = tile},
							{.offset{2, 0, 0}, .tile = tile},
							{.offset{3, 0, 0}, .tile = tile},

							{.offset{-3, 1, 0}, .tile = tile},
							{.offset{-2, 1, 0}, .tile = tile},
							{.offset{-1, 1, 0}, .tile = tile},
							{.offset{0, 1, 0}, .tile = tile},
							{.offset{1, 1, 0}, .tile = tile},
							{.offset{2, 1, 0}, .tile = tile},
							{.offset{3, 1, 0}, .tile = tile},

							{.offset{-2, 2, 0}, .tile = tile},
							{.offset{-1, 2, 0}, .tile = tile},
							{.offset{0, 2, 0}, .tile = tile},
							{.offset{1, 2, 0}, .tile = tile},
							{.offset{2, 2, 0}, .tile = tile},

							{.offset{-1, 3, 0}, .tile = tile},
							{.offset{0, 3, 0}, .tile = tile},
							{.offset{1, 3, 0}, .tile = tile},
						},
					})
			        ->first,
				.previewColor = getTileCenterPixel(tile.tile),
			});
		}

		for (const auto& [brushName, brushID] : brushIDs) {
			if (!brushName.starts_with("PLAYER")) {
				const Brush& brush = brushes[brushID];
				paintableBrushes.push_back({
					.name = brushName,
					.brushID = brushID,
					.previewColor = getTileCenterPixel(brush.tilePlacements[brush.tilePlacements.size() / 2].tile.tile),
				});
			}
		}

		sortByDescending<&PaintableBrush::name>(paintableBrushes);
	}

	struct Loader {
		struct ReferenceToTileCategoryInTile {
			String tile;
			String tileCategory;
		};

		struct ReferenceToTilesetInTile {
			String tile;
			String tileset;
		};

		struct ReferenceToTileCategoryInBrushTilePlacement {
			String brush;
			size_t tilePlacementIndex;
			String tileCategory;
		};

		struct ReferenceToTileInBrushTilePlacement {
			String brush;
			size_t tilePlacementIndex;
			String tile;
		};

		struct ReferenceToTilesetInBrushTilePlacement {
			String brush;
			size_t tilePlacementIndex;
			String tileset;
		};

		ArrayList<String>& mapFilepaths;
		TileCategories& tileCategories;
		Tilesets& tilesets;
		Brushes& brushes;
		HashSet<CStringView> visitedFilepaths{};
		ArrayList<ReferenceToTileCategoryInTile> unresolvedReferencesToTileCategoriesInTiles{};
		ArrayList<ReferenceToTilesetInTile> unresolvedReferencesToTilesetsInTiles{};
		ArrayList<ReferenceToTileCategoryInBrushTilePlacement> unresolvedReferencesToTileCategoriesInBrushTilePlacements{};
		ArrayList<ReferenceToTileInBrushTilePlacement> unresolvedReferencesToTilesInBrushTilePlacements{};
		ArrayList<ReferenceToTilesetInBrushTilePlacement> unresolvedReferencesToTilesetsInBrushTilePlacements{};

		void extend(Schema& schema, const Filesystem& filesystem, CStringView filepath) {
			if (!visitedFilepaths.insert(filepath).second) {
				throw Error{"Cycle detected in schema."};
			}

			String fileContents = filesystem.readInputFileString(filepath);
			try {
				std::istringstream stream{std::move(fileContents)};
				json::Reader reader{stream};
				reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
					if (key == "extends") {
						const StringView filepathPrefix = filepath.substr(0, filepath.find_last_of("/\\") + 1);
						if (reader.nextIsString()) {
							extend(schema, filesystem, String{filepathPrefix} + reader.readString());
						} else {
							reader.readCustomArray([&](const json::SourceLocation&) -> void { extend(schema, filesystem, String{filepathPrefix} + reader.readString()); });
						}
					} else if (key == "maps") {
						reader.readCustomArray([&](const json::SourceLocation&) -> void { mapFilepaths.push_back(reader.readString()); });
					} else if (key == "tileCategories") {
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							const TileCategoryID tileCategoryID = tileCategories.insert(reader.deserialize<TileCategory>())->first;
							schema.tileCategoryIDs[key] = tileCategoryID;
						});
					} else if (key == "tilesets") {
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							String imageFilepath{};
							TilesetOptions options{};
							reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
								if (key == "imageFilepath") {
									imageFilepath = reader.readString();
								} else if (key == "tileSize") {
									options.tileSizeInPixels = Extent2D::from(reader.deserialize<u32vec2>());
								} else if (key == "tileGap") {
									options.tileGapInPixels = Extent2D::from(reader.deserialize<u32vec2>());
								} else if (key == "useMipmap") {
									options.useMipmap = reader.readBoolean();
								}
							});
							const Tilesets::Index tilesetIndex = tilesets.loadTileset(filesystem, imageFilepath, options);
							schema.tilesetIndices[key] = tilesetIndex;
						});
					} else if (key == "tiles") {
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							schema.tiles[key] = readTile(
								reader,
								[&](String tileCategory) -> void {
									unresolvedReferencesToTileCategoriesInTiles.push_back({
										.tile = key,
										.tileCategory = std::move(tileCategory),
									});
								},
								[&](String tileset) -> void {
									unresolvedReferencesToTilesetsInTiles.push_back({
										.tile = key,
										.tileset = std::move(tileset),
									});
								});
						});
					} else if (key == "brushes") {
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							const BrushID brushID = brushes.insert(readBrush(reader, key))->first;
							schema.brushIDs[key] = brushID;
						});
					}
				});
			} catch (...) {
				Error::throwWithNestedFilepath(filepath);
			}

			visitedFilepaths.erase(filepath);
		}

		void resolveResourceReferences(Schema& schema) {
			const auto isValidTilesetPosition = [&](Tilesets::Index tilesetIndex, u8vec2 tilesetPosition) -> bool {
				if (tilesetIndex >= tilesets.size()) {
					return false;
				}
				const Tilesets::TilesetInfo tilesetInfo = tilesets[tilesetIndex];
				const u32vec2 tileCounts = u32vec2{tilesetInfo.tilesetSizeInPixels + tilesetInfo.tileGapInPixels} / //
				                           u32vec2{tilesetInfo.tileSizeInPixels + tilesetInfo.tileGapInPixels};
				return all(lessThan(u32vec2{tilesetPosition}, tileCounts));
			};

			for (const ReferenceToTileCategoryInTile& unresolved : unresolvedReferencesToTileCategoriesInTiles) {
				if (const auto it = schema.tileCategoryIDs.find(unresolved.tileCategory); it != schema.tileCategoryIDs.end()) {
					schema.tiles.at(unresolved.tile).categoryID = it->second;
				} else {
					throw Error{formatString("Unknown tile category \"{}\" of tile \"{}\".", unresolved.tileCategory, unresolved.tile)};
				}
			}
			unresolvedReferencesToTileCategoriesInTiles.clear();

			for (const ReferenceToTilesetInTile& unresolved : unresolvedReferencesToTilesetsInTiles) {
				if (const auto it = schema.tilesetIndices.find(unresolved.tileset); it != schema.tilesetIndices.end()) {
					Tile& tile = schema.tiles.at(unresolved.tile).tile;
					tile.setTilesetIndex(it->second);
					if (!isValidTilesetPosition(it->second, tile.getTilesetPosition())) {
						throw Error{formatString("Tileset position of tile \"{}\" is outside the valid of range of tileset \"{}\".", unresolved.tile, unresolved.tileset)};
					}
				} else {
					throw Error{formatString("Unknown tileset \"{}\" of tile \"{}\".", unresolved.tileset, unresolved.tile)};
				}
			}
			unresolvedReferencesToTilesetsInTiles.clear();

			for (const ReferenceToTileCategoryInBrushTilePlacement& unresolved : unresolvedReferencesToTileCategoriesInBrushTilePlacements) {
				if (const auto it = schema.tileCategoryIDs.find(unresolved.tileCategory); it != schema.tileCategoryIDs.end()) {
					brushes.at(schema.brushIDs.at(unresolved.brush)).tilePlacements.at(unresolved.tilePlacementIndex).tile.categoryID = it->second;
				} else {
					throw Error{formatString("Unknown tile category \"{}\" in tile placement [{}] of brush \"{}\".", unresolved.tileCategory, unresolved.tilePlacementIndex,
						unresolved.brush)};
				}
			}
			unresolvedReferencesToTileCategoriesInBrushTilePlacements.clear();

			for (const ReferenceToTileInBrushTilePlacement& unresolved : unresolvedReferencesToTilesInBrushTilePlacements) {
				if (const auto it = schema.tiles.find(unresolved.tile); it != schema.tiles.end()) {
					brushes.at(schema.brushIDs.at(unresolved.brush)).tilePlacements.at(unresolved.tilePlacementIndex).tile = it->second;
				} else {
					throw Error{formatString("Unknown tile \"{}\" in tile placement [{}] of brush \"{}\".", unresolved.tile, unresolved.tilePlacementIndex, unresolved.brush)};
				}
			}
			unresolvedReferencesToTilesInBrushTilePlacements.clear();

			for (const ReferenceToTilesetInBrushTilePlacement& unresolved : unresolvedReferencesToTilesetsInBrushTilePlacements) {
				if (const auto it = schema.tilesetIndices.find(unresolved.tileset); it != schema.tilesetIndices.end()) {
					Tile& tile = brushes.at(schema.brushIDs.at(unresolved.brush)).tilePlacements.at(unresolved.tilePlacementIndex).tile.tile;
					tile.setTilesetIndex(it->second);
					if (!isValidTilesetPosition(it->second, tile.getTilesetPosition())) {
						throw Error{formatString("Tileset position in tile placement [{}] of brush \"{}\" is outside the valid of range of tileset \"{}\".",
							unresolved.tilePlacementIndex, unresolved.brush, unresolved.tileset)};
					}
				} else {
					throw Error{
						formatString("Unknown tileset \"{}\" in tile placement [{}] of brush \"{}\".", unresolved.tileset, unresolved.tilePlacementIndex, unresolved.brush)};
				}
			}
			unresolvedReferencesToTilesetsInBrushTilePlacements.clear();
		}

	private:
		[[nodiscard]] CategorizedTile readTile(json::Reader& reader, auto addUnresolvedTileCategoryReference, auto addUnresolvedTilesetReference) {
			TileOptions tileOptions{};
			TileCategoryID tileCategoryID{};
			reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
				if (key == "category") {
					if (reader.nextIsString()) {
						addUnresolvedTileCategoryReference(reader.readString());
					} else {
						tileCategoryID = tileCategories.insert(reader.deserialize<TileCategory>())->first;
					}
				} else if (key == "tileset") {
					addUnresolvedTilesetReference(reader.readString());
				} else if (key == "tilesetPosition") {
					reader.deserialize(tileOptions.tilesetPosition);
					if (tileOptions.tilesetPosition.x > 63 || tileOptions.tilesetPosition.y > 63) {
						throw Error{"Tileset position is out of range."};
					}
				} else if (key == "flipX") {
					reader.deserialize(tileOptions.flipX);
				} else if (key == "flipY") {
					reader.deserialize(tileOptions.flipY);
				} else if (key == "animationFrameStep") {
					reader.deserialize(tileOptions.animationFrameStep);
					if (tileOptions.animationFrameStep < 1 || tileOptions.animationFrameStep > 4) {
						throw Error{"Animation frame step is out of range."};
					}
				} else if (key == "animationFrameCount") {
					if (reader.nextIsNumber()) {
						tileOptions.animationFrameCountX = reader.deserialize<uint8_t>();
					} else {
						const u8vec2 frameCounts = reader.deserialize<u8vec2>();
						tileOptions.animationFrameCountX = frameCounts.x;
						tileOptions.animationFrameCountY = frameCounts.y;
					}
					if (tileOptions.animationFrameCountX < 1 || tileOptions.animationFrameCountX > 16) {
						throw Error{"Horizontal animation frame count is out of range."};
					}
					if (tileOptions.animationFrameCountY < 1 || tileOptions.animationFrameCountY > 16) {
						throw Error{"Vertical animation frame count is out of range."};
					}
				} else if (key == "animationFrameRate") {
					reader.deserialize(tileOptions.animationFrameRate);
					if (tileOptions.animationFrameRate < 3 || tileOptions.animationFrameRate > 48) {
						throw Error{"Animation frame rate is out of range."};
					}
					if (tileOptions.animationFrameRate % 3 != 0) {
						throw Error{"Animation frame rate is not divisible by 3."};
					}
				}
			});
			return {Tile::create(tileOptions), tileCategoryID};
		}

		[[nodiscard]] Brush readBrush(json::Reader& reader, const json::String& brushName) {
			Brush brush{};
			reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
				if (key == "origin") {
					reader.deserialize(brush.origin);
				} else if (key == "tilePlacements") {
					reader.readCustomArray([&](const json::SourceLocation&) -> void {
						Brush::TilePlacement tilePlacement{};
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							if (key == "offset") {
								tilePlacement.offset = Offset3D::from(reader.deserialize<i32vec3>());
							} else if (key == "tile") {
								if (reader.nextIsString()) {
									unresolvedReferencesToTilesInBrushTilePlacements.push_back({
										.brush = brushName,
										.tilePlacementIndex = brush.tilePlacements.size(),
										.tile = reader.readString(),
									});
								} else {
									const CategorizedTile tile = readTile(
										reader,
										[&](String tileCategory) -> void {
											unresolvedReferencesToTileCategoriesInBrushTilePlacements.push_back({
												.brush = brushName,
												.tilePlacementIndex = brush.tilePlacements.size(),
												.tileCategory = std::move(tileCategory),
											});
										},
										[&](String tileset) -> void {
											unresolvedReferencesToTilesetsInBrushTilePlacements.push_back({
												.brush = brushName,
												.tilePlacementIndex = brush.tilePlacements.size(),
												.tileset = std::move(tileset),
											});
										});
									tilePlacement.tile = tile;
								}
							}
						});
						brush.tilePlacements.push_back(tilePlacement);
					});
				}
			});
			return brush;
		}
	};
};

#endif
