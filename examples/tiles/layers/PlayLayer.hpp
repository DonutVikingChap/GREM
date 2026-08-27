// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_LAYERS_PLAY_LAYER_HPP
#define GREM_EXAMPLES_TILES_LAYERS_PLAY_LAYER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>

#include "../Brushes.hpp"
#include "../Graphics.hpp"
#include "../Layer.hpp"
#include "../Map.hpp"
#include "../Schema.hpp"
#include "../World.hpp"
#include "../WorldRenderer.hpp"

class PlayLayer final : public Layer {
public:
	enum class Action : uint8_t {
		PAUSE,
		MOVE_DOWN,
		MOVE_UP,
		MOVE_LEFT,
		MOVE_RIGHT,
		AIM_DOWN,
		AIM_UP,
		AIM_LEFT,
		AIM_RIGHT,
		SPRINT,
		ATTACK,
	};

	PlayLayer(const Filesystem& filesystem, gfx::Device& device, World& world)
		: world(world)
		, worldRenderer(filesystem, device) {
		const Map& map = world.resources.getResource<Map>();
		const exec::EntityID playerEntityID = world.createPlayerEntity(map.getSpawnpoint());
		players.emplace_back(filesystem, "configuration/player1.json", playerEntityID);
	}

	~PlayLayer() override = default;

	void reloadShaders(const Filesystem& filesystem, gfx::Device& device) override {
		worldRenderer.reloadShaders(filesystem, device);
	}

	void prepareForEvents() override {
		GREM_PROFILE_FUNCTION();

		for (Player& player : players) {
			player.inputManager.update();
		}
	}

	bool handleEvent(const Graphics& graphics, const evt::Event& event) override {
		GREM_PROFILE_FUNCTION();

		const Schema& schema = world.resources.getResource<Schema>();

		for (Player& player : players) {
			player.inputManager.handleEvent(event);
		}

		Optional<vec2> newMouseCoordinates{};
		Optional<vec2> oldMouseCoordinates{};
		GREM_MATCH(event) {
			GREM_CASE(const evt::MouseMovedEvent& moved) {
				if (paintingMouseCoordinates) {
					newMouseCoordinates = moved.mousePosition;
					oldMouseCoordinates = *paintingMouseCoordinates;
					paintingMouseCoordinates = newMouseCoordinates;
				}
				break;
			}
			GREM_CASE(const evt::MouseButtonPressedEvent& pressed) {
				if (pressed.mouseButton == evt::MouseButton::LEFT) {
					newMouseCoordinates = pressed.mousePosition;
					oldMouseCoordinates = newMouseCoordinates;
					paintingMouseCoordinates = newMouseCoordinates;
				} else if (pressed.mouseButton == evt::MouseButton::RIGHT) {
					if (!schema.paintableBrushes.empty()) {
						selectedPaintableBrushIndex = (selectedPaintableBrushIndex + 1) % schema.paintableBrushes.size();
					}
				}
				break;
			}
			GREM_CASE(const evt::MouseButtonReleasedEvent& released) {
				if (released.mouseButton == evt::MouseButton::LEFT) {
					paintingMouseCoordinates.reset();
				}
				break;
			}
			GREM_CASE(const evt::MouseWheelScrolledEvent& scrolled) {
				constexpr float MIN_ZOOM_AMOUNT = -8.0f;
				constexpr float MAX_ZOOM_AMOUNT = 8.0f;
				constexpr float ZOOM_AMOUNT_SNAP_STEP = 1.0f;
				const float relativeZoomAmount = scrolled.scrollAmount.y * 0.125f;
				float newZoomAmount = clamp(zoomAmount + relativeZoomAmount, MIN_ZOOM_AMOUNT, MAX_ZOOM_AMOUNT);
				if (newZoomAmount != zoomAmount) {
					for (float snapZoomAmount = MIN_ZOOM_AMOUNT; snapZoomAmount <= MAX_ZOOM_AMOUNT; snapZoomAmount += ZOOM_AMOUNT_SNAP_STEP) {
						if ((newZoomAmount < snapZoomAmount && zoomAmount > snapZoomAmount) || (newZoomAmount > snapZoomAmount && zoomAmount < snapZoomAmount)) {
							newZoomAmount = snapZoomAmount;
						}
					}
					if (paintingMouseCoordinates) {
						const Map::VisibleRegion startVisibleRegion = getVisibleRegion(graphics, 1.0f);
						zoomAmount = newZoomAmount;
						const Map::VisibleRegion endVisibleRegion = getVisibleRegion(graphics, 1.0f);
						const Offset2D startPosition = convertScreenToTileCoordinates(graphics, startVisibleRegion, *paintingMouseCoordinates);
						const Offset2D endPosition = convertScreenToTileCoordinates(graphics, endVisibleRegion, *paintingMouseCoordinates);
						paintLine(startPosition, endPosition);
					} else {
						zoomAmount = newZoomAmount;
					}
				}
				break;
			}
			GREM_CASE_DEFAULT(const auto& other) break;
		}

		if (newMouseCoordinates && oldMouseCoordinates) {
			const Map::VisibleRegion visibleRegion = getVisibleRegion(graphics, 1.0f);
			const Offset2D startPosition = convertScreenToTileCoordinates(graphics, visibleRegion, *oldMouseCoordinates);
			const Offset2D endPosition = convertScreenToTileCoordinates(graphics, visibleRegion, *newMouseCoordinates);
			paintLine(startPosition, endPosition);
		}
		return true;
	}

	Continuation update(const Graphics&, Duration deltaTime) override {
		GREM_PROFILE_FUNCTION();

		for (Player& player : players) {
			if (player.inputManager.justPressed(Action::PAUSE)) {
				player.inputManager.resetAllStates();
				return PauseGame{};
			}

			if (World::Movement* const movement = world.registry.findComponent<World::Movement>(player.entityID)) {
				constexpr float BASE_SPEED = 12.0f;
				constexpr float SPRINT_SPEED = 1000.0f;
				const vec2 desiredDirectionScale = player.inputManager.getCurrentState2D(Action::MOVE_LEFT, Action::MOVE_RIGHT, Action::MOVE_DOWN, Action::MOVE_UP).value;
				const float desiredSpeed = mix(BASE_SPEED, SPRINT_SPEED, clamp(player.inputManager.getCurrentState(Action::SPRINT).value, 0.0f, 1.0f));
				movement->desiredVelocity = desiredSpeed * clampLength(desiredDirectionScale, 1.0f);
			}
		}
		animationTime += deltaTime;
		return BreakFromLayerStack{};
	}

	void tick(exec::Executor& executor, const Graphics& graphics, Duration tickInterval) override {
		GREM_PROFILE_FUNCTION();

		const Schema& schema = world.resources.getResource<Schema>();

		world.tick(executor, tickInterval);

		if (paintingMouseCoordinates && selectedPaintableBrushIndex < schema.paintableBrushes.size()) {
			const Map::VisibleRegion startVisibleRegion = getVisibleRegion(graphics, 0.0f);
			const Map::VisibleRegion endVisibleRegion = getVisibleRegion(graphics, 1.0f);
			if (startVisibleRegion.tileOffset != endVisibleRegion.tileOffset || startVisibleRegion.subTileOffset != endVisibleRegion.subTileOffset) {
				const Offset2D startPosition = convertScreenToTileCoordinates(graphics, startVisibleRegion, *paintingMouseCoordinates);
				const Offset2D endPosition = convertScreenToTileCoordinates(graphics, endVisibleRegion, *paintingMouseCoordinates);
				paintLine(startPosition, endPosition);
			}
		}
	}

	void draw(gfx::Device& device, Graphics& graphics, gfx::RenderPass& renderPass, float tickInterpolationAlpha, size_t fps) override {
		GREM_PROFILE_FUNCTION();

		{
			GREM_PROFILE_BLOCK("Prepare world for display");
			world.prepareForDisplay();
		}

		const vec2 zoomScale = getZoomScale();
		const Map::VisibleRegion visibleRegion = getVisibleRegion(graphics, tickInterpolationAlpha);

		{
			GREM_PROFILE_BLOCK("Draw background");
			graphics.instances2D.clear();
			graphics.instances2D.putRectangleInstance(
				{.position = graphics.worldViewportRenderRegion.offset, .size = graphics.worldViewportRenderRegion.size, .color = Color::BLACK});
			renderPass.setViewport(graphics.viewport);
			graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.camera2D);
		}

		{
			GREM_PROFILE_BLOCK("Draw world");
			const gfx::Viewport worldViewport{.region{
				.offset = graphics.viewport.region.offset + graphics.worldViewportRenderRegion.offset * graphics.viewportScale,
				.size = graphics.worldViewportRenderRegion.size * graphics.viewportScale,
			}};
			renderPass.setViewport(worldViewport);
			worldRenderer.drawWorld(device, renderPass, world, visibleRegion, zoomScale, duration_cast<FloatSeconds>(animationTime).count(), tickInterpolationAlpha);
		}

		{
			GREM_PROFILE_BLOCK("Draw user interface");
			graphics.instances2D.clear();
			const int32_t x = graphics.worldViewportRenderRegion.offset.x;
			const int32_t y = graphics.worldViewportRenderRegion.offset.y;
			const int32_t width = static_cast<int32_t>(graphics.worldViewportRenderRegion.size.width);
			const int32_t fullWidth = static_cast<int32_t>(graphics.renderSize.width);
			const int32_t fullHeight = static_cast<int32_t>(graphics.renderSize.height);
			const Color fpsColor = (fps < 60) ? Color::RED : (fps < 120) ? Color::YELLOW : (fps < 240) ? Color::GRAY : Color::LIME;
			graphics.put2DText(Offset2D{x + 4, fullHeight - y + 4}, fpsColor, formatString("FPS: {}", fps));
#ifndef NDEBUG
			graphics.put2DText(Offset2D{x + width - 4, fullHeight - y + 4}, Color::RED, "DEBUG BUILD", 1.0f, gfx::TextAlign::RIGHT);
#endif
			graphics.put2DText(Offset2D{x + 4, y + 4 + 14}, (1.0f / zoomScale.y > 16.0f) ? Color::ORANGE : Color::WHITE,
				(zoomScale.y < 1.0f) ? formatString("Zoom: 1/{:.4f} X", 1.0f / zoomScale.y) : formatString("Zoom: {:.4f} X", zoomScale.y));
			if (!players.empty()) {
				if (const Optional<World::Position> playerDisplayPosition = world.getEntityDisplayPosition(players.front().entityID, tickInterpolationAlpha)) {
					graphics.put2DText(Offset2D{x + 4, y + 4}, Color::WHITE, formatString("Pos.: {:>7}", playerDisplayPosition->coordinates));
				}
			}
			const Schema& schema = world.resources.getResource<Schema>();
			if (selectedPaintableBrushIndex < schema.paintableBrushes.size()) {
				const Schema::PaintableBrush& paintableBrush = schema.paintableBrushes[selectedPaintableBrushIndex];
				graphics.put2DText(Offset2D{x + width - 4, y + 4}, paintableBrush.previewColor, formatString("Brush: {}", paintableBrush.name), 1.0f, gfx::TextAlign::RIGHT);
			}
			graphics.put2DText(Offset2D{fullWidth / 2, 4 + 14}, Color::WHITE, "W/A/S/D: Move | Scroll: Zoom | Shift: Sprint | LMB: Paint | RMB: Switch", 1.0f,
				gfx::TextAlign::CENTER_HORIZONTALLY);
			graphics.put2DText(Offset2D{fullWidth / 2, 4}, Color::WHITE, "F4: Uncap FPS | F5: Reload shaders | F10: Quit | F11: Toggle fullscreen", 1.0f,
				gfx::TextAlign::CENTER_HORIZONTALLY);
			renderPass.setViewport(graphics.viewport);
			graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.camera2D);
		}
	}

private:
	struct Player {
		exec::EntityID entityID;
		evt::InputManager inputManager{};

		Player(const Filesystem& filesystem, CStringView configurationFilepath, exec::EntityID entityID)
			: entityID(entityID) {
			inputManager.loadConfiguration<Action>(filesystem, configurationFilepath);
		}
	};

	[[nodiscard]] vec2 getZoomScale() const {
		return vec2{exp2(zoomAmount)};
	}

	[[nodiscard]] Map::VisibleRegion getVisibleRegion(const Graphics& graphics, float tickInterpolationAlpha) const {
		const Map& map = world.resources.getResource<Map>();
		Coordinates2D centerPosition{};
		for (const Player& player : players) {
			if (const Optional<World::Position> playerDisplayPosition = world.getEntityDisplayPosition(player.entityID, tickInterpolationAlpha)) {
				centerPosition += playerDisplayPosition->coordinates / static_cast<Coordinate::value_type>(players.size());
			}
		}
		return map.getVisibleRegion(centerPosition, getZoomScale(), graphics.worldViewportRenderRegion.size);
	}

	[[nodiscard]] Offset2D convertScreenToTileCoordinates(const Graphics& graphics, const Map::VisibleRegion& visibleRegion, vec2 screenCoordinates) const {
		const vec2 screenCoordinatesFromBottomLeft{screenCoordinates.x, static_cast<float>(graphics.windowSize.height) - screenCoordinates.y};
		const Region2D worldViewportRegion = {
			.offset = graphics.viewport.region.offset + graphics.worldViewportRenderRegion.offset * graphics.viewportScale,
			.size = graphics.worldViewportRenderRegion.size * graphics.viewportScale,
		};
		const vec2 normalizedWorldViewportCoordinates = ((screenCoordinatesFromBottomLeft - vec2{worldViewportRegion.offset}) / vec2{worldViewportRegion.size});
		return visibleRegion.tileOffset + Offset2D::floor(visibleRegion.subTileOffset + normalizedWorldViewportCoordinates * visibleRegion.size);
	}

	void paintLine(Offset2D startPosition, Offset2D endPosition) {
		const Schema& schema = world.resources.getResource<Schema>();
		if (selectedPaintableBrushIndex < schema.paintableBrushes.size()) {
			const BrushID brushID = schema.paintableBrushes[selectedPaintableBrushIndex].brushID;
			Map& map = world.resources.getResource<Map>();
			map.paintLine(paintLayer, startPosition, endPosition, schema.brushes[brushID]);
		}
	}

	World& world;
	WorldRenderer worldRenderer;
	ArrayList<Player> players{};
	Duration animationTime{};
	float zoomAmount = 0.0f;
	size_t selectedPaintableBrushIndex = 0;
	Optional<vec2> paintingMouseCoordinates{};
	int32_t paintLayer = 0;
};

#endif
