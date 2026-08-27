// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_LAYERS_WORLD_LAYER_HPP
#define GREM_EXAMPLES_TILES_LAYERS_WORLD_LAYER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

#include "../Coordinate.hpp"
#include "../Graphics.hpp"
#include "../Layer.hpp"
#include "../Schema.hpp"
#include "../World.hpp"

class WorldLayer final : public Layer {
public:
	WorldLayer(Filesystem& filesystem, Schema& schema)
		: world(filesystem, schema)
		, numberGenerator(schema.worldSeed) {}

	~WorldLayer() override = default;

	Continuation update(const Graphics&, Duration) override {
		GREM_PROFILE_FUNCTION();

		const Schema& schema = world.resources.getResource<Schema>();

		for (size_t updateSubStructureIndex = 0; updateSubStructureIndex < STRUCTURES_PER_UPDATE; ++updateSubStructureIndex) {
			if (structureIndex >= STRUCTURE_COUNT) {
				return StartPlaying{.world = &world};
			}

			const StructureType structureType = static_cast<StructureType>(structureTypeDistribution(numberGenerator));
			const float structurePositionX = structurePositionDistribution(numberGenerator);
			const float structurePositionY = structurePositionDistribution(numberGenerator);
			const vec2 structurePosition{structurePositionX, structurePositionY};

			switch (structureType) {
				case StructureType::PATCH_OF_GRASS: placePatch(0, structurePosition, schema.tiles.at("GRASS"), 1.5f); break;
				case StructureType::PATCH_OF_WATER: placePatch(0, structurePosition, schema.tiles.at("WATER"), 0.8f); break;
				case StructureType::PATCH_OF_DIRT: placePatch(0, structurePosition, schema.tiles.at("DIRT"), 1.0f); break;
				case StructureType::PATCH_OF_ICE: placePatch(0, structurePosition, schema.tiles.at("ICE"), 0.5f); break;
				case StructureType::SPLOTCH_OF_TILLED_DIRT_WITH_RED_FRUIT: placeSplotch(0, structurePosition, schema.tiles.at("TILLED_DIRT_WITH_RED_FRUIT"), 100000); break;
				case StructureType::SPLOTCH_OF_TILLED_DIRT_WITH_YELLOW_FRUIT: placeSplotch(0, structurePosition, schema.tiles.at("TILLED_DIRT_WITH_YELLOW_FRUIT"), 100000); break;
				case StructureType::SPLOTCH_OF_FOUNTAINS: placeSplotchOfBrushes(1, structurePosition, schema.brushIDs.at("FOUNTAIN"), 3000); break;
				case StructureType::SPLOTCH_OF_FLAG_ENTITIES: placeSplotchOfFlagEntities(structurePosition, 100); break;
				case StructureType::COUNT: break;
			}
			++structureIndex;
		}
		return ContinueDownLayerStack{};
	}

	void draw(gfx::Device&, Graphics& graphics, gfx::RenderPass& renderPass, float, size_t) override {
		GREM_PROFILE_FUNCTION();

		const size_t progressSteps = structureIndex / STRUCTURES_PER_UPDATE;
		const float progress = static_cast<float>(progressSteps) / ceil(static_cast<float>(STRUCTURE_COUNT) / static_cast<float>(STRUCTURES_PER_UPDATE));

		String messageString = "   Generating world";
		const int progressInPercent = static_cast<int>(100.0f * progress);
		const int dotCount = progressInPercent % 4;
		for (int i = 0; i < dotCount; ++i) {
			messageString.push_back('.');
		}
		for (int i = dotCount; i < 3; ++i) {
			messageString.push_back(' ');
		}
		graphics.instances2D.clear();
		const vec2 screenCenter = vec2{graphics.renderSize / 2};
		graphics.put2DText(screenCenter - vec2{0.0f, 4.0f}, Color::ORANGE, messageString, 2.0f, gfx::TextAlign::CENTER_HORIZONTALLY);
		graphics.put2DText(screenCenter - vec2{0.0f, 32.0f}, Color::ORANGE, formatString("{} %", progressInPercent), 2.0f, gfx::TextAlign::CENTER_HORIZONTALLY);
		graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.camera2D);
	}

private:
	enum class StructureType : uint8_t {
		PATCH_OF_GRASS,
		PATCH_OF_WATER,
		PATCH_OF_DIRT,
		PATCH_OF_ICE,
		SPLOTCH_OF_TILLED_DIRT_WITH_RED_FRUIT,
		SPLOTCH_OF_TILLED_DIRT_WITH_YELLOW_FRUIT,
		SPLOTCH_OF_FOUNTAINS,
		SPLOTCH_OF_FLAG_ENTITIES,
		COUNT,
	};

	static constexpr size_t STRUCTURE_COUNT = 200;
	static constexpr size_t STRUCTURES_PER_UPDATE = 2;

	void placePatch(int32_t layer, vec2 structurePosition, CategorizedTile tile, float radiusScale) {
		Map& map = world.resources.getResource<Map>();
		const i32vec2 center = i32vec2{structurePosition};
		const float patchRadius = patchRadiusDistribution(numberGenerator) * radiusScale;
		const int32_t patchRadiusSquared = static_cast<int32_t>(length2(patchRadius));
		const Offset2D patchMin = Offset2D::floor(structurePosition - vec2{patchRadius});
		const Offset2D patchMax = Offset2D::floor(structurePosition + vec2{patchRadius});
		for (int32_t y = patchMin.y; y < patchMax.y; ++y) {
			for (int32_t x = patchMin.x; x < patchMax.x; ++x) {
				const Offset3D position{x, y, layer};
				if (distance2(i32vec2{x, y}, center) < patchRadiusSquared) {
					map.setTile(position, tile);
				}
			}
		}
	}

	void placeSplotch(int32_t layer, vec2 structurePosition, CategorizedTile tile, size_t sampleCount) {
		Map& map = world.resources.getResource<Map>();
		for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
			const float angle = splotchAngleDistribution(numberGenerator);
			const float scale = abs(splotchScaleDistribution(numberGenerator));
			const Offset2D offset = Offset2D::floor(structurePosition + scale * angledVector(angle));
			const Offset3D position{offset.x, offset.y, layer};
			map.setTile(position, tile);
		}
	}

	void placeSplotchOfBrushes(int32_t layer, vec2 structurePosition, BrushID brushID, size_t sampleCount) {
		Map& map = world.resources.getResource<Map>();
		const Schema& schema = world.resources.getResource<Schema>();
		const Brush& brush = schema.brushes[brushID];
		const i32vec2 origin{floor(brush.origin)};
		for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
			const float angle = splotchAngleDistribution(numberGenerator);
			const float scale = abs(splotchScaleDistribution(numberGenerator));
			const Offset2D baseOffset = Offset2D::floor(structurePosition + scale * angledVector(angle));
			for (const Brush::TilePlacement& tilePlacement : brush.tilePlacements) {
				const Offset3D position{
					baseOffset.x + tilePlacement.offset.x - origin.x,
					baseOffset.y + tilePlacement.offset.y - origin.y,
					layer + tilePlacement.offset.z,
				};
				map.setTile(position, tilePlacement.tile);
			}
		}
	}

	void placeSplotchOfFlagEntities(vec2 structurePosition, size_t sampleCount) {
		const Map& map = world.resources.getResource<Map>();
		const int32_t layer = map.getSpawnpoint().layer;
		for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
			const float angle = splotchAngleDistribution(numberGenerator);
			const float scale = abs(splotchScaleDistribution(numberGenerator));
			const vec2 position = structurePosition + scale * angledVector(angle);
			world.createFlagEntity({Coordinates2D{position}, layer});
		}
	}

	World world;
	rng::Xoroshiro128PlusPlusEngine numberGenerator;
	rng::UniformIntegerDistribution<unsigned> structureTypeDistribution{0u, static_cast<unsigned>(StructureType::COUNT) - 1u};
	rng::NormalDistribution<float> structurePositionDistribution{0.0f, 2000.0f};
	rng::UniformRealDistribution<float> splotchAngleDistribution{0.0f, 2.0f * numbers::PI};
	rng::NormalDistribution<float> splotchScaleDistribution{0.0f, 500.0f};
	rng::NormalDistribution<float> patchRadiusDistribution{0.0f, 300.0f};
	size_t structureIndex = 0;
};

#endif
