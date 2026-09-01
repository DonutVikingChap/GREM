// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Physics Example
 *
 * This example is a simple 2D physics toy game that simulates draggable physics
 * objects in a resizable window that can be moved relative to the objects to
 * shake them around.
 *
 * The game uses the included `examples/data/shared_2d/` and
 * `examples/data/physics/` folders as its resource archives for any asset files
 * loaded at runtime.
 */

#include <GREM/aliases.hpp>
#include <GREM/application.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>
#include <GREM/physics.hpp>
#include <GREM/resource.hpp>

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

namespace {

struct PhysicsGameOptions {
	app::ApplicationOptions app{
		.tickInterval = 1.0_seconds / 120,
	};
	exec::DynamicExecutorOptions exe{};
};

class PhysicsGame final : public app::Application {
public:
	PhysicsGame(const Filesystem& filesystem, const PhysicsGameOptions& options)
		: Application(options.app)
		, executor(options.exe)
		, mainFont(filesystem, "fonts/unscii/unscii-8.ttf")
		, checkeredSquareTexture(device, res::Image{filesystem, "textures/checkered_square.png"})
		, checkeredCircleTexture(device, res::Image{filesystem, "textures/checkered_circle.png"})
		, simulation({
			  .stepInterval = options.app.tickInterval,
			  .targetParallelism = executor.getMaxParallelism(),
			  .contactStiffness = 1_x / options.app.tickInterval,
		  }) {
		window.setIcon(res::Image{filesystem, "textures/icon.png"});

		const Extent2D drawableSize = window.getDrawableSize();
		resize(drawableSize);

		const phys::Position2D position = vec2{drawableSize} * 0.5f * PIXEL_LENGTH_UNIT;
		const phys::Length2D size = vec2{80.0f} * PIXEL_LENGTH_UNIT;
		const ShapeCategory shapeCategory = ShapeCategory::ELLIPSE;
		const Color color = Color::LIME;
		createShapeObject(position, size, shapeCategory, color);
	}

protected:
	void update(app::FrameInfo) override {
		for (const evt::Event& event : eventPump.pollEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
					quit();
					break;
				}
				GREM_CASE(const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
					if (drawableSizeChanged.windowID == window.getID()) {
						resize(drawableSizeChanged.windowDrawableSize);
					}
					break;
				}
				GREM_CASE(const evt::MouseButtonPressedEvent& pressed) {
					if (pressed.windowID == window.getID()) {
						const phys::Position2D clickPosition =
							vec2{pressed.mousePosition.x, static_cast<float>(window.getSize().height) - pressed.mousePosition.y} * PIXEL_LENGTH_UNIT;

						const phys::Broadphase2D& broadphase = simulation.resources.getResource<phys::Broadphase2D>();
						const auto entities =
							simulation.registry
								.getEntities<const phys::Position2D, const phys::Orientation2D, const phys::Scale2D, const phys::Collider2D, const phys::ObjectBounds2D>();

						phys::EntityID clickedObjectID{};
						broadphase.testPoint(clickPosition, phys::CollisionFilter{}, entities, phys::CollisionFilterTest{},
							[&](phys::EntityID objectID, phys::CollisionFilterTestResult) -> bool {
								if (simulation.registry.getComponent<phys::ObjectActivity>(objectID).isCorrectable != 0) {
									clickedObjectID = objectID;
									return true;
								}
								return false;
							});

						if (pressed.mouseButton == evt::MouseButton::LEFT) {
							simulation.registry.destroyEntity(cursorJointID);
							cursorJointID = {};

							if (clickedObjectID) {
								const phys::Position2D position = entities.getComponent<phys::Position2D>(clickedObjectID);
								const phys::Orientation2D orientation = entities.getComponent<phys::Orientation2D>(clickedObjectID);
								cursorJointID =
									simulation
										.createWeld({cursorObjectID, clickedObjectID},
											{
												.attachmentOffsets{{}, inverse(orientation)(clickPosition - position)},
												.attachmentOrientations{{}, inverse(orientation)},
											})
										.build();
							}
						} else if (pressed.mouseButton == evt::MouseButton::RIGHT) {
							if (clickedObjectID) {
								simulation.registry.destroyEntity(clickedObjectID);
							}
						}
					}
					break;
				}
				GREM_CASE(const evt::MouseButtonReleasedEvent& released) {
					if (released.mouseButton == evt::MouseButton::LEFT) {
						simulation.registry.destroyEntity(cursorJointID);
						cursorJointID = {};
					}
					break;
				}
				GREM_CASE(const evt::MouseMovedEvent& moved) {
					const phys::Position2D newPosition = vec2{moved.mousePosition.x, static_cast<float>(window.getSize().height) - moved.mousePosition.y} * PIXEL_LENGTH_UNIT;
					cursorMotion += newPosition - cursorPosition;
					cursorPosition = newPosition;
					break;
				}
				GREM_CASE(const evt::KeyPressedEvent& pressed) {
					if (pressed.keyCode == evt::KeyCode::SPACE) {
						createRandomShapeObject(simulation.registry.getComponent<phys::Position2D>(cursorObjectID));
					} else if (pressed.keyCode == evt::KeyCode::F2) {
						showFPS = !showFPS;
					} else if (pressed.keyCode == evt::KeyCode::F4) {
						showDebugVisualization = !showDebugVisualization;
					} else if (pressed.keyCode == evt::KeyCode::F10) {
						quit();
					} else if (pressed.keyCode == evt::KeyCode::F11 || (pressed.keyCode == evt::KeyCode::RETURN && pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT))) {
						window.setFullscreen(!window.isFullscreen());
					}
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}
		}
	}

	void tick(app::TickInfo) override {
		if (skippedSimulationTime > Duration{}) {
			eprintln("Warning: Skipped {} milliseconds of simulation time.", duration_cast<FloatMilliseconds>(skippedSimulationTime).count());
			skippedSimulationTime = {};
		}

		lastSimulationState = simulation.registry;

		const Offset2D windowPosition = window.getPosition();
		if (windowPosition != lastWindowPosition) {
			const phys::Length2D windowMotion =
				vec2{static_cast<float>(windowPosition.x - lastWindowPosition.x), static_cast<float>(lastWindowPosition.y - windowPosition.y)} * PIXEL_LENGTH_UNIT;
			lastWindowPosition = windowPosition;

			for (auto&& [entityID, position, activity] : simulation.registry.getEntities<phys::Position2D, phys::ObjectActivity>()) {
				if (activity.isCorrectable != 0) {
					position -= windowMotion;
					activity.energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;
				}
			}
		}

		if (cursorMotion != 0) {
			simulation.registry.getComponent<phys::ObjectActivity>(cursorObjectID).energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;
			simulation.registry.getComponent<phys::LinearVelocity2D>(cursorObjectID) = cursorMotion / simulation.resources.getResource<phys::SimulationOptions2D>().stepInterval;
			cursorMotion = {};
		} else {
			simulation.registry.getComponent<phys::LinearVelocity2D>(cursorObjectID) = {};
		}

		simulation.step();

		debugVisualization.clear();
		if (showDebugVisualization) {
			simulation.drawDebugVisualization(debugVisualization);
		}

		simulation.registry.getComponent<phys::Position2D>(cursorObjectID) = cursorPosition;
	}

	void skipTick(Duration tickInterval) override {
		skippedSimulationTime += tickInterval;
	}

	void display(app::FrameInfo frameInfo) override {
		instances2D.clear();

		for (auto&& [entityID, position, orientation, scale, collider, color] :
			simulation.registry.getEntities<const phys::Position2D, const phys::Orientation2D, const phys::Scale2D, const phys::Collider2D, const Color>()) {
			if (!lastSimulationState.containsEntity(entityID)) {
				continue;
			}

			const phys::Position2D oldPosition = lastSimulationState.getComponent<phys::Position2D>(entityID);
			const phys::Orientation2D oldOrientation = lastSimulationState.getComponent<phys::Orientation2D>(entityID);
			const phys::Scale2D oldScale = lastSimulationState.getComponent<phys::Scale2D>(entityID);

			const phys::Position2D displayPosition = mix(oldPosition, position, frameInfo.tickInterpolationAlpha);
			const phys::Orientation2D displayOrientation = mix(oldOrientation, orientation, frameInfo.tickInterpolationAlpha);
			const phys::Scale2D displayScale = mix(oldScale, scale, frameInfo.tickInterpolationAlpha);

			GREM_MATCH(collider.shape) {
				GREM_CASE(const phys::RectangleShape2D& rectangle) {
					instances2D.putRectangleInstance({
						.texture = &checkeredSquareTexture,
						.position = displayPosition.in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (rectangle.halfExtents * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					break;
				}
				GREM_CASE(const phys::SquareShape2D& square) {
					instances2D.putRectangleInstance({
						.texture = &checkeredSquareTexture,
						.position = displayPosition.in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (square.halfExtent * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					break;
				}
				GREM_CASE(const phys::EllipseShape2D& ellipse) {
					instances2D.putRectangleInstance({
						.texture = &checkeredCircleTexture,
						.position = displayPosition.in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (ellipse.radii * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					break;
				}
				GREM_CASE(const phys::CircleShape2D& circle) {
					instances2D.putRectangleInstance({
						.texture = &checkeredCircleTexture,
						.position = displayPosition.in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (circle.radius * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					break;
				}
				GREM_CASE(const phys::CapsuleShape2D& capsule) {
					const phys::Length2D pointOffset = displayOrientation(displayScale.getY() * capsule.halfLength * phys::Y_AXIS_2D);
					instances2D.putRectangleInstance({
						.texture = &checkeredCircleTexture,
						.position = (displayPosition - pointOffset).in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (capsule.radius * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					instances2D.putRectangleInstance({
						.texture = &checkeredCircleTexture,
						.position = (displayPosition + pointOffset).in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (capsule.radius * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.color = color,
					});
					instances2D.putRectangleInstance({
						.texture = &checkeredSquareTexture,
						.position = displayPosition.in(PIXEL_LENGTH_UNIT),
						.angle = displayOrientation.in(phys::RADIANS),
						.size = displayScale * (phys::Length2D{capsule.radius, capsule.halfLength} * 2_x).in(PIXEL_LENGTH_UNIT),
						.origin{0.5f, 0.5f},
						.textureScale{1.0f, capsule.halfLength / capsule.radius},
						.color = color,
					});
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}
		}

		if (showFPS) {
			const size_t fps = getLastSecondFrameCount();
			const Color fpsColor = (fps < 60) ? Color::RED : (fps < 120) ? Color::YELLOW : (fps < 240) ? Color::GRAY : Color::LIME;
			instances2D.putTextStringInstance(mainFont, formatString("FPS: {}", fps),
				{
					.characterSize = 8,
					.position{4.0f, static_cast<float>(viewport.region.offset.y) + static_cast<float>(viewport.region.size.height) - 4.0f},
					.scale{2.0f, 2.0f},
					.alignment = gfx::TextAlign::FIRST_LINE_START_TOP,
					.color = fpsColor,
				});
		}

		if (showDebugVisualization) {
			debugVisualization.putWorldVisualizationInstances(renderer2D, instances2D, phys::Length2D{1.0f * PIXEL_LENGTH_UNIT});
			debugVisualization.putUIVisualizationInstances(renderer2D, instances2D, mainFont);
		}

#ifndef NDEBUG
		instances2D.putTextStringInstance(mainFont, "DEBUG BUILD",
			{
				.characterSize = 8,
				.position{
					static_cast<float>(viewport.region.offset.x) + static_cast<float>(viewport.region.size.width) - 4.0f,
					static_cast<float>(viewport.region.offset.y) + static_cast<float>(viewport.region.size.height) - 4.0f,
				},
				.scale{2.0f, 2.0f},
				.alignment = gfx::TextAlign::FIRST_LINE_END_TOP,
				.color = Color::RED,
			});
#endif

		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::BLACK}, viewport};
		renderer2D.drawFrame(renderPass, {instances2D}, camera2D);
		device.render(renderPass);

		device.present(swapchain);
	}

private:
	static constexpr phys::unit auto PIXEL_LENGTH_UNIT = phys::CENTIMETERS;

	enum class ShapeCategory : uint8_t {
		RECTANGLE,
		ELLIPSE,
		CAPSULE,
	};

	void resize(Extent2D newDrawableSize) {
		camera2D.setProjection(gfx::OrthographicProjection2D{.size = newDrawableSize});
		viewport.region = {.size = newDrawableSize};

		const vec2 bottomLeft{viewport.region.offset};
		const vec2 topRight = bottomLeft + vec2{viewport.region.size};
		const vec2 center = midpoint(bottomLeft, topRight);

		simulation.registry.getComponent<phys::Position2D>(leftWallObjectID) = {
			bottomLeft.x * PIXEL_LENGTH_UNIT,
			center.y * PIXEL_LENGTH_UNIT,
		};
		simulation.registry.getComponent<phys::Position2D>(rightWallObjectID) = {
			topRight.x * PIXEL_LENGTH_UNIT,
			center.y * PIXEL_LENGTH_UNIT,
		};
		simulation.registry.getComponent<phys::Position2D>(floorObjectID) = {
			center.x * PIXEL_LENGTH_UNIT,
			bottomLeft.y * PIXEL_LENGTH_UNIT,
		};
		simulation.registry.getComponent<phys::Position2D>(ceilingObjectID) = {
			center.x * PIXEL_LENGTH_UNIT,
			topRight.y * PIXEL_LENGTH_UNIT,
		};

		simulation.updateObjectBounds(leftWallObjectID);
		simulation.updateObjectBounds(rightWallObjectID);
		simulation.updateObjectBounds(floorObjectID);
		simulation.updateObjectBounds(ceilingObjectID);

		for (auto&& [entityID, activity] : simulation.registry.getEntities<phys::ObjectActivity>()) {
			if (activity.isCorrectable != 0) {
				activity.energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;
			}
		}
	}

	[[nodiscard]] phys::EntityID createWallObject(phys::Orientation2D orientation) {
		return simulation
		    .createObject({
				.orientation = orientation,
				.gravityAcceleration{},
				.mass = phys::Mass::INF,
				.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia2D::INF,
				.collider{.shape = phys::InfiniteHalfSpaceShape2D{}},
			})
		    .build();
	}

	[[nodiscard]] phys::EntityID createCursorObject() {
		return simulation
		    .createObject({
				.gravityAcceleration{},
				.mass = phys::Mass::INF,
				.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia2D::INF,
				.collider{.shape = phys::PointShape2D{}, .filter{.layers{}, .detectionLayers{}, .responseLayers{}}},
			})
		    .build();
	}

	phys::EntityID createShapeObject(phys::Position2D position, phys::Length2D size, ShapeCategory shapeCategory, Color color) {
		phys::Shape2D shape{};
		switch (shapeCategory) {
			default: [[fallthrough]];
			case ShapeCategory::RECTANGLE:
				if (size.getX() == size.getY()) {
					shape = phys::SquareShape2D{.halfExtent = size.getX() * 0.5f};
				} else {
					shape = phys::RectangleShape2D{.halfExtents = size * 0.5f};
				}
				break;
			case ShapeCategory::ELLIPSE:
				if (size.getX() == size.getY()) {
					shape = phys::CircleShape2D{.radius = size.getX() * 0.5f};
				} else {
					shape = phys::EllipseShape2D{.radii = size * 0.5f};
				}
				break;
			case ShapeCategory::CAPSULE:
				size = {size.getX(), ceil(size.getY() / size.getX()) * size.getX()};
				shape = phys::CapsuleShape2D{.radius = size.getX() * 0.5f, .halfLength = size.getY() * 0.5f};
				break;
		}
		phys::EntityBuilder2D entityBuilder = simulation.createObject({
			.position = position,
			.collider{.shape = std::move(shape)},
			.material{.restitution = 0.8f},
		});
		entityBuilder.addComponent<Color>(color);
		return entityBuilder.build();
	}

	phys::EntityID createRandomShapeObject(phys::Position2D position) {
		const int shapeSizeXInSixteensOfPixels = shapeSizeInSixteensOfPixelsDistribution(numberGenerator);
		const int shapeSizeYInSixteensOfPixels = shapeSizeInSixteensOfPixelsDistribution(numberGenerator);
		const size_t shapeCategoryIndex = shapeCategoryIndexDistribution(numberGenerator);
		const float shapeColorR = shapeColorComponentDistribution(numberGenerator);
		const float shapeColorG = shapeColorComponentDistribution(numberGenerator);
		const float shapeColorB = shapeColorComponentDistribution(numberGenerator);
		const phys::Length2D size = vec2{static_cast<float>(shapeSizeXInSixteensOfPixels * 16), static_cast<float>(shapeSizeYInSixteensOfPixels * 16)} * PIXEL_LENGTH_UNIT;
		const ShapeCategory shapeCategory = static_cast<ShapeCategory>(shapeCategoryIndex);
		const Color color = Color::fromLinear(shapeColorR, shapeColorG, shapeColorB);
		return createShapeObject(position, size, shapeCategory, color);
	}

	exec::DynamicExecutor executor;
	evt::EventPump eventPump{};
	gfx::Window window{{.title = "Physics", .size{640, 480}}};
	gfx::Device device{window};
	gfx::Swapchain swapchain{device, window};
	gfx::Renderer2D renderer2D{device};
	gfx::Camera2D camera2D{device};
	gfx::Viewport viewport{};
	gfx::Instances2D instances2D{device, renderer2D};
	gfx::Font2D mainFont;
	gfx::Texture checkeredSquareTexture;
	gfx::Texture checkeredCircleTexture;
	phys::Simulation2D simulation;
	phys::EntityRegistry2D lastSimulationState = simulation.registry;
	phys::DebugVisualization2D debugVisualization{};
	phys::EntityID leftWallObjectID = createWallObject(-90_degrees);
	phys::EntityID rightWallObjectID = createWallObject(90_degrees);
	phys::EntityID floorObjectID = createWallObject(0_degrees);
	phys::EntityID ceilingObjectID = createWallObject(180_degrees);
	Offset2D lastWindowPosition = window.getPosition();
	phys::EntityID cursorObjectID = createCursorObject();
	phys::EntityID cursorJointID{};
	phys::Position2D cursorPosition{};
	phys::Length2D cursorMotion{};
	Duration skippedSimulationTime{};
	rng::DefaultRandomEngine numberGenerator{};
	rng::UniformIntegerDistribution<int> shapeSizeInSixteensOfPixelsDistribution{2, 8};
	rng::UniformIntegerDistribution<size_t> shapeCategoryIndexDistribution{0, 2};
	rng::UniformRealDistribution<float> shapeColorComponentDistribution{0.0f, 1.0f};
	bool showFPS = false;
	bool showDebugVisualization = false;
};

} // namespace

int main(int argc, char* argv[]) {
	try {
		app::VirtualFilesystem filesystem{argv[0]};
		filesystem.mountInputArchive("data/shared_2d");
		filesystem.mountInputArchive("data/physics");
		PhysicsGameOptions options{};
		try {
			cli::parseCommandLineOptions(options, argc, argv);
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}
		PhysicsGame game{filesystem, options};
		game.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
