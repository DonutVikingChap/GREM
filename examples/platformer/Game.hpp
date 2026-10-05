// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_GAME_HPP
#define GREM_EXAMPLES_PLATFORMER_GAME_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "Audio.hpp"
#include "Graphics.hpp"
#include "System.hpp"
#include "systems/collectible.hpp"
#include "systems/level.hpp"
#include "systems/object.hpp"
#include "systems/player.hpp"

struct GameOptions {
	app::ApplicationOptions app{
		.tickInterval = 1.0_second / 128,
	};
	gfx::WindowOptions wnd{
		.title = "Example Platformer",
		.size{1280, 720},
		.multisampleCount = 4,
	};
	gfx::SwapchainOptions swap{};
	gfx::Renderer2DOptions r2d{};
	gfx::Renderer3DOptions r3d{};
	aud::SoundStageOptions snd{};
	exec::DynamicExecutorOptions exe{
		.targetParallelism = static_cast<exec::Task::ParallelCount>(clamp(Thread::hardware_concurrency() / 2u, 2u, 16u) - 1),
	};
	GraphicsOptions gfx{};
	String level{};
};

class Game final : public app::Application {
public:
	static constexpr CStringView GAME_CONFIGURATION_FILEPATH = "configuration/game.json";

	enum class Action : uint8_t {
		CONFIRM,
		CANCEL,
		LEFT,
		RIGHT,
		UP,
		DOWN,
		SCROLL_UP,
		SCROLL_DOWN,
	};

	Game(Filesystem& filesystem, const GameOptions& options)
		: app::Application(options.app)
		, filesystem(filesystem)
		, window(options.wnd)
		, graphics(filesystem, window, options.r2d, options.r3d, options.gfx)
		, audio(options.snd)
		, executor(options.exe)
		, simulation(phys::SimulationOptions3D{.stepInterval = options.app.tickInterval, .targetParallelism = executor.getMaxParallelism()},
			  [&](phys::EntityRegistry3D&, phys::ResourceRegistry3D& resources, const phys::SimulationOptions3D&) -> phys::Schedule3D {
				  resources.addExternalResource(&audio);

				  for (System* const* const system : SYSTEMS) {
					  (*system)->addRequiredResources(resources, graphics, audio, filesystem);
				  }

				  phys::Scheduler3D scheduler{};
				  for (System* const* const system : SYSTEMS) {
					  (*system)->scheduleTick(scheduler, resources, executor.getMaxParallelism());
				  }
				  return scheduler.buildSchedule("Tick");
			  })
		, levelFilepath(options.level) {
		{
			phys::Scheduler3D scheduler{};
			for (System* const* const system : SYSTEMS) {
				(*system)->scheduleUpdate(scheduler, simulation.resources, executor.getMaxParallelism());
			}
			updateSchedule = scheduler.buildSchedule("Update");
		}
		{
			phys::Scheduler3D scheduler{};
			for (System* const* const system : SYSTEMS) {
				(*system)->schedulePose(scheduler, simulation.resources, executor.getMaxParallelism());
			}
			poseSchedule = scheduler.buildSchedule("Pose");
		}

		window.setIcon(res::Image{filesystem, "textures/icon.png"});

		inputManager.loadConfiguration<Action>(filesystem, "configuration/input.json");

		loadLevel(simulation.registry, simulation.resources, graphics.device, graphics.renderer3D, filesystem, levelFilepath);

		{
			const auto& [playerSpawnPosition, playerSpawnAngles] = getLevelPlayerSpawnPositionAndAngles(simulation.resources);
			phys::EntityBuilder3D entity = simulation.registry.createEntity();
			createPlayer(entity, simulation.resources, filesystem, "configuration/player1.json", playerSpawnPosition, playerSpawnAngles, {});
			playerEntityID = entity.build();
		}

		graphics.resize(window.getDrawableSize());

		setPlayerHasControl(true);
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		inputManager.update();
		for (const evt::Event& event : eventPump.pollEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
					quit();
					break;
				}
				GREM_CASE(const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
					if (drawableSizeChanged.windowID == window.getID()) {
						graphics.resize(drawableSizeChanged.windowDrawableSize);
					}
					break;
				}
				GREM_CASE(const evt::WindowKeyboardFocusLostEvent& keyboardFocusLost) {
					if (keyboardFocusLost.windowID == window.getID()) {
						setPlayerHasControl(false);
					}
					break;
				}
				GREM_CASE(const evt::WindowMouseFocusLostEvent& mouseFocusLost) {
					if (mouseFocusLost.windowID == window.getID()) {
						setPlayerHasControl(false);
					}
					break;
				}
				GREM_CASE(const evt::MouseButtonPressedEvent& pressed) {
					if (pressed.windowID == window.getID() && pressed.mouseButton == evt::MouseButton::LEFT) {
						setPlayerHasControl(true);
					}
					break;
				}
				GREM_CASE(const evt::KeyPressedEvent& pressed) {
					if (pressed.keyCode == evt::KeyCode::ESCAPE) {
						if (pressed.windowID == window.getID()) {
							setPlayerHasControl(!playerHasControl);
						}
					} else if (pressed.keyCode == evt::KeyCode::F10) {
						quit();
					} else if (pressed.keyCode == evt::KeyCode::F11 || (pressed.keyCode == evt::KeyCode::RETURN && pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT))) {
						window.setFullscreen(!window.isFullscreen());
					} else if (pressed.keyCode == evt::KeyCode::F2) {
						if (getMinFrameTime() == 1.0_second / 30) {
							setMinFrameTime(1.0_second / 60);
						} else if (getMinFrameTime() == 1.0_second / 60) {
							setMinFrameTime(1.0_second / 120);
						} else if (getMinFrameTime() == 1.0_second / 120) {
							setMinFrameTime(1.0_second / 240);
						} else if (getMinFrameTime() == 1.0_second / 240) {
							setMinFrameTime(1.0_second / 480);
						} else if (getMinFrameTime() == 1.0_second / 480) {
							setMinFrameTime({});
						} else if (getMinFrameTime() == Duration{}) {
							setMinFrameTime(1.0_second / 30);
						}
					} else if (pressed.keyCode == evt::KeyCode::F4) {
						if (debugVisualization) {
							debugVisualization.reset();
						} else {
							debugVisualization.emplace();
						}
					} else if (pressed.keyCode == evt::KeyCode::F5) {
						graphics.reloadShaders(filesystem);
						loadLevel(simulation.registry, simulation.resources, graphics.device, graphics.renderer3D, filesystem, levelFilepath);
					} else if (pressed.keyCode == evt::KeyCode::F8) {
						GREM_PROFILER_SAVE_NEXT_N_FRAMES(8, "platformer_profiler_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
					}
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}
			inputManager.handleEvent(event);
			handlePlayerEvent(simulation.registry, event, playerHasControl);
		}

		simulation.resources.addExternalResource(&frameInfo);
		executor.executeSchedule(updateSchedule, simulation.registry, simulation.resources);
		simulation.resources.removeResource<app::FrameInfo>();
	}

	void tick(app::TickInfo) override {
		simulation.step(executor);

		if (debugVisualization) {
			debugVisualization->clear();
			simulation.drawDebugVisualization(*debugVisualization);
		}
	}

	void display(app::FrameInfo frameInfo) override {
		const phys::Box3D levelBounds = getLevelBounds(simulation.resources);
		const gfx::Sky3D& levelSky = getLevelSky(simulation.resources);

		const PlayerCameraView playerCameraView = getPlayerCameraView(simulation.registry, playerEntityID, frameInfo.tickInterpolationAlpha);
		const phys::OrthonormalBasis3D cameraBasis = rotate(playerCameraView.angles);

		graphics.camera3D.setView(gfx::WorldView3D{
			.position = playerCameraView.position.in(phys::METERS),
			.orientation = phys::Orientation3D::lookAt(-cameraBasis[phys::Z], cameraBasis[phys::Y]),
		});

		audio.soundStage.update(aud::Listener{
			.position = playerCameraView.position.in(phys::METERS),
			.velocity = playerCameraView.linearVelocity.in(phys::METERS_PER_SECOND),
			.forward = -cameraBasis[phys::Z],
			.up = cameraBasis[phys::Y],
		});

		WorldView worldView{
			.frustum = Frustum<float>::fromViewProjectionMatrix(graphics.camera3D.getProjectionMatrix() * graphics.camera3D.getViewMatrix()),
			.visibleBounds = calculateBoundingBox<3, float>(worldView.frustum.corners),
		};
		worldView.visibleBounds.min = max(worldView.visibleBounds.min, vec3{levelBounds.min.in(phys::METERS)});
		worldView.visibleBounds.max = min(worldView.visibleBounds.max, vec3{levelBounds.max.in(phys::METERS)});

		simulation.resources.addExternalResource(&frameInfo);
		simulation.resources.addExternalResource(&worldView);

		executor.executeSchedule(poseSchedule, simulation.registry, simulation.resources);

		{
			gfx::RenderPass renderPass{graphics.device, graphics.swapchain, gfx::ClearValues{.color = Color::BLACK}, graphics.viewport};

			graphics.clearInstances();

			for (System* const* const system : SYSTEMS) {
				(*system)->putGraphics(graphics, simulation);
			}

			if (debugVisualization) {
				debugVisualization->putWorldVisualizationInstances(graphics.renderer3D, graphics.instances3D);
			}

			if (debugVisualization) {
				debugVisualization->putUIVisualizationInstances(graphics.renderer2D, graphics.instances2D, graphics.mainFont);
			}

			{
				const size_t fps = getLastSecondFrameCount();
				const Offset2D fpsPosition{6, static_cast<int32_t>(graphics.viewport.region.size.height) - 6};
				const Color fpsColor = (fps < 60) ? Color::RED : (fps < 120) ? Color::YELLOW : (fps < 240) ? Color::GRAY : Color::LIME;
				graphics.put2DText(fpsPosition, fpsColor, formatString("FPS: {}", fps), 2.0f, gfx::TextAlign::FIRST_LINE_START_TOP);
			}

#ifndef NDEBUG
			graphics.put2DText(graphics.viewport.region.offset + graphics.viewport.region.size - Offset2D{6, 6}, Color::RED, "DEBUG BUILD", 2.0f,
				gfx::TextAlign::FIRST_LINE_END_TOP);
#endif

			graphics.flushModelShaderBuffers(executor, renderPass.getFramebufferSize(), worldView);

			graphics.renderer3D.drawUnlitFrameWithSky(renderPass, {graphics.instances3D}, graphics.camera3D, levelSky, graphics.modelShaderBuffers);
			graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.camera2D);

			graphics.device.render(renderPass);
		}

		graphics.device.present(graphics.swapchain);

		simulation.resources.removeResource<WorldView>();
		simulation.resources.removeResource<app::FrameInfo>();
	}

private:
	void setPlayerHasControl(bool newHasControl) {
		if (newHasControl != playerHasControl) {
			try {
				window.setRelativeMouseMode(newHasControl);
			} catch (...) {
			}
			if (!newHasControl) {
				inputManager.releaseAll(Clock::now());
			}
			playerHasControl = newHasControl;
		}
	}

	static constexpr Array SYSTEMS{
		&playerSystem,
		&playerControlSystem,
		&objectSystem,
		&objectPhysicsSystem,
		&levelSystem,
		&collectibleSystem,
		&playerGroundingSystem,
	};

	Filesystem& filesystem;
	evt::EventPump eventPump{};
	gfx::Window window;
	Graphics graphics;
	Audio audio;
	evt::InputManager inputManager{};
	exec::DynamicExecutor executor;
	phys::Simulation3D simulation;
	phys::Schedule3D updateSchedule{};
	phys::Schedule3D poseSchedule{};
	Optional<phys::DebugVisualization3D> debugVisualization{};
	CStringView levelFilepath;
	phys::EntityID playerEntityID{};
	bool playerHasControl = false;
};

#endif
