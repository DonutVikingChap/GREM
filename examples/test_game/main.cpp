// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Test Game Example
 *
 * This example shows a basic game project crammed into a single source file.
 * The main application class, Game, is defined at the top, and the main
 * function is defined at the bottom.
 *
 * This example is mainly used for testing, but can also be used to study how
 * various GREM features are combined to form a working application. Note
 * however that as a real project grows, the code that this example represents
 * would typically be split into multiple files, like the other examples are, to
 * make the main application file less cluttered and the project structure
 * easier to navigate.
 *
 * The game uses the included `examples/data/shared_2d/`,
 * `examples/data/shared_3d/` and `examples/data/test_game/` folders as its
 * resource archives for any asset files loaded at runtime.
 */

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "shaders.hpp"

#include <imgui.h> // Im...

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

namespace {

struct GameOptions {
	app::ApplicationOptions app{
		.tickInterval = 1.0_seconds / 30,
	};
	evt::EventPumpOptions ev{};
	gfx::WindowOptions wnd{
		.title = "Example Game",
		.size{640, 480},
		.multisampleCount = 4,
		.resizable = true,
	};
	gfx::DeviceOptions dev{};
	gfx::SwapchainOptions swap{};
	gfx::Renderer2DOptions r2D{};
	gfx::Renderer3DOptions r3D{};
	imgui::GraphicalUserInterfaceOptions gui{};
	aud::SoundStageOptions snd{};
	evt::InputManagerOptions in{};
	CStringView bgm = "sounds/music/donauwalzer.ogg";
	phys::Quantity<1, phys::Degrees> fov = 90_degrees;
};

class Game final : public app::Application {
public:
	Game(Filesystem& filesystem, const GameOptions& options)
		: app::Application(options.app)
		, eventPump(options.ev)
		, window(options.wnd)
		, device(filesystem, window, options.dev)
		, swapchain(device, window, options.swap)
		, renderer2D(device, options.r2D)
		, renderer3D(device, renderer2D, options.r3D)
		, gui(filesystem, eventPump, window, device, swapchain, renderer2D, options.gui)
		, exampleShaderPipeline{
			device,
			renderer2D.getDefaultModel2DVertexShader(),
			gfx::Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS,
			ExampleFragmentShader{device, filesystem, "shaders/example.frag"},
			gfx::Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
			gfx::Model2D::DEFAULT_SHADER_PIPELINE_OPTIONS,
		}
		, testTexture(device, res::Image{filesystem, "textures/test.png"})
		, circleTexture(device, res::Image{filesystem, "textures/circle.png"}, {}, gfx::TextureSamplerOptions::UNFILTERED)
		, cubeModel(device, renderer3D, res::Model{filesystem, "models/cube.obj"})
		, carrotCakeModel(device, renderer3D, res::Model{filesystem, "models/carrot_cake/carrot_cake_1k.gltf"})
		, testSprite(spriteAtlas.insertSprite(res::Image{filesystem, "textures/test.png"}))
		, testSubSprite(spriteAtlas.createSubSprite(testSprite, Region2D{.offset{200, 200}, .size{100, 100}}, gfx::SpriteOptions::FLIP_HORIZONTALLY))
		, mainFont(filesystem, "fonts/unscii/unscii-8.ttf")
		, inputManager(options.in)
		, verticalFieldOfView(2.0f * atan((3.0f / 4.0f) * tan(options.fov.in(phys::RADIANS) * 0.5f))) {
		window.setIcon(res::Image{filesystem, "textures/icon.png"});

		inputManager.loadConfiguration<Action>(filesystem, "configuration/player1.json");

		initializeSoundStage(options.snd);
		playBackgroundMusic(filesystem, options.bgm);

		loadCircles();
		loadLights();

		resize(window.getDrawableSize());

		const float contentScale = window.getDisplay().getContentScale();

		ImGui::SetCurrentContext(gui.getContext());
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(contentScale);
		style.FontScaleDpi = contentScale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;

		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		inputManager.update();

		handleEvents();

		gui.update(frameInfo.deltaTime);
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
		if (showGUIDemoWindow) {
			ImGui::ShowDemoWindow(&showGUIDemoWindow);
		}

		if (soundStage) {
			soundStage->update(listener);
		}

		const float sprintInput = (inputManager.isPressed(Action::SPRINT)) ? 4.0f : 1.0f;

		const vec2 movementInput = clampLength(inputManager.getCurrentState2D(Action::MOVE_LEFT, Action::MOVE_RIGHT, Action::MOVE_DOWN, Action::MOVE_UP).value, 1.0f);
		const float carrotCakeSpeed = 2.0f * sprintInput;
		carrotCakeVelocity = {movementInput * carrotCakeSpeed, 0.0f};

		if (inputManager.isPressed(Action::CONFIRM)) {
			const vec2 aimInput = inputManager.getRelativeState2D(Action::AIM_LEFT, Action::AIM_RIGHT, Action::AIM_DOWN, Action::AIM_UP).motion;
			carrotCakeScale = clamp(carrotCakeScale + aimInput * 10.0f, 0.25f, 4.0f);
		}

		const float scrollInput = inputManager.getRelativeState1D(Action::SCROLL_DOWN, Action::SCROLL_UP).motion;
		carrotCakeCurrentPosition.z -= scrollInput * 0.25f * sprintInput;

		const bool triggerInput = inputManager.isPressed(Action::CANCEL);
		counterA += countupLoop(timerA, frameInfo.deltaTime, 1_second, triggerInput);
		counterB += countdownLoop(timerB, frameInfo.deltaTime, 1_second, triggerInput);
	}

	void tick(app::TickInfo tickInfo) override {
		carrotCakePreviousPosition = carrotCakeCurrentPosition;
		carrotCakeCurrentPosition += carrotCakeVelocity * duration_cast<FloatSeconds>(tickInfo.tickInterval).count();
	}

	void display(app::FrameInfo frameInfo) override {
		carrotCakeDisplayPosition = mix(carrotCakePreviousPosition, carrotCakeCurrentPosition, frameInfo.tickInterpolationAlpha);

		updateLights(frameInfo);

		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::fromSRGB(40, 75, 50)}};

		instances3D.clear();
		draw3DWorld(frameInfo);
		renderPass.setViewport(worldViewport);
		renderer3D.drawPBRFrame(renderPass, {instances3D}, worldCamera, fog, sky, decals, lights, lightProbeVolumes, reflectionProbes);

		instances2D.clear();
		draw2DWorld(frameInfo);
		renderPass.setViewport(screenViewportWithWorldScissor);
		renderer2D.drawFrame(renderPass, {instances2D}, screenCamera, exampleShaderUniformBuffer);

		instances2D.clear();
		drawUserInterface(frameInfo);
		drawFrameRateCounter();
		renderPass.setViewport(screenViewport);
		renderer2D.drawFrame(renderPass, {instances2D}, screenCamera);

		ImGui::Render();
		gui.drawFrame(renderPass, *ImGui::GetDrawData());

		device.render(renderPass);

		device.present(swapchain);

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

private:
	enum class Action : uint8_t {
		CONFIRM = 0,
		CANCEL = 1,
		MOVE_UP = 2,
		MOVE_DOWN = 3,
		MOVE_LEFT = 4,
		MOVE_RIGHT = 5,
		AIM_UP = 6,
		AIM_DOWN = 7,
		AIM_LEFT = 8,
		AIM_RIGHT = 9,
		SPRINT = 10,
		ATTACK = 11,
		SCROLL_UP = 12,
		SCROLL_DOWN = 13,
	};

	static constexpr size_t LIGHT_COUNT = 4;

	void resize(Extent2D newDrawableSize) {
		constexpr Extent2D RENDER_RESOLUTION{.width = 640, .height = 480};
		constexpr Offset2D WORLD_VIEWPORT_POSITION{.x = 15, .y = 15};
		constexpr Extent2D WORLD_VIEWPORT_SIZE{.width = 380, .height = 450};

		screenViewport.region = {.size = RENDER_RESOLUTION};
		const uint32_t scale = screenViewport.region.fitCenteredIntegerScaled({.size = newDrawableSize});
		screenCamera.setProjection(gfx::OrthographicProjection2D{.size = RENDER_RESOLUTION});

		worldViewport.region = {
			.offset = screenViewport.region.offset + WORLD_VIEWPORT_POSITION * scale,
			.size = WORLD_VIEWPORT_SIZE * scale,
		};
		worldCamera.setProjection(gfx::PerspectiveProjection3D{
			.verticalFieldOfView = verticalFieldOfView,
			.aspectRatio = WORLD_VIEWPORT_SIZE.getAspectRatio(),
			.nearZ = 0.1f,
			.farZ = 100.0f,
		});

		screenViewportWithWorldScissor = screenViewport;
		screenViewportWithWorldScissor.scissor = worldViewport.region;
	}

	void initializeSoundStage(const aud::SoundStageOptions& options) {
		try {
			soundStage.emplace(options);
		} catch (const aud::Error& e) {
			// In case the user doesn't have a working sound card,
			// just print a warning message instead of crashing.
			eprintln("Warning: {}", e.what());
		}
	}

	void playBackgroundMusic(const Filesystem& filesystem, CStringView filepath) {
		if (soundStage) {
			if (filesystem.inputFileExists(filepath)) {
				music.emplace(filesystem, filepath,
					aud::SoundOptions{
						.attenuationModel = aud::SoundAttenuationModel::NO_ATTENUATION,
						.volume = 0.1f,
						.looping = true,
					});
				musicID = soundStage->createPausedSoundInBackground(*music);
				soundStage->seekToSoundTime(musicID, 46700_milliseconds);
				soundStage->resumeSound(musicID);
			}
		}
	}

	void loadCircles() {
		constexpr Circle<float> CIRCLE_A{.center{60.0f, 80.0f}, .radius = 20.0f};
		constexpr Circle<float> CIRCLE_B{.center{50.0f, 90.0f}, .radius = 20.0f};
		constexpr Circle<float> CIRCLE_C{.center{60.0f, 120.0f}, .radius = 20.0f};
		constexpr Circle<float> CIRCLE_D{.center{300.0f, 100.0f}, .radius = 10.0f};
		constexpr Circle<float> CIRCLE_E{.center{200.0f, 180.0f}, .radius = 30.0f};
		constexpr Circle<float> CIRCLE_F{.center{140.0f, 440.0f}, .radius = 20.0f};
		quadtree.insert(CIRCLE_A.getBoundingBox(), CIRCLE_A);
		quadtree.insert(CIRCLE_B.getBoundingBox(), CIRCLE_B);
		quadtree.insert(CIRCLE_C.getBoundingBox(), CIRCLE_C);
		quadtree.insert(CIRCLE_D.getBoundingBox(), CIRCLE_D);
		quadtree.insert(CIRCLE_E.getBoundingBox(), CIRCLE_E);
		quadtree.insert(CIRCLE_F.getBoundingBox(), CIRCLE_F);
	}

	void loadLights() {
		for (gfx::LightID& lightID : lightIDs) {
			lightID = lights.createPointLight({
				.range = 50.0f,
				.shadowMapped = false,
			});
		}
	}

	void handleEvents() {
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
				GREM_CASE(const evt::KeyPressedEvent& pressed) {
					if (pressed.keyCode == evt::KeyCode::F1) {
						showGUIDemoWindow = !showGUIDemoWindow;
					} else if (pressed.keyCode == evt::KeyCode::F2) {
						if (soundStage) {
							soundStage->stopSound(musicID);
						}
					} else if (pressed.keyCode == evt::KeyCode::F4) {
						setMinFrameTime((getMinFrameTime() == Duration{}) ? 1.0_seconds / 480 : Duration{});
					} else if (pressed.keyCode == evt::KeyCode::F8) {
						GREM_PROFILER_SAVE_NEXT_N_FRAMES(16, "example_test_game_profiler_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
					} else if (pressed.keyCode == evt::KeyCode::F10) {
						quit();
					} else if (pressed.keyCode == evt::KeyCode::F11 || (pressed.keyCode == evt::KeyCode::RETURN && pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT))) {
						window.setFullscreen(!window.isFullscreen());
					}
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}

			if (gui.handleEvent(event)) {
				continue;
			}

			inputManager.handleEvent(event);
		}
	}

	void updateLights(const app::FrameInfo& frameInfo) {
		const float elapsedSeconds = duration_cast<FloatSeconds>(frameInfo.totalElapsedTime).count();

		constexpr Array<vec3, LIGHT_COUNT> LIGHT_OFFSETS{
			vec3{-2.0f, 0.0f, 0.0f},
			vec3{0.0f, -2.0f, 0.0f},
			vec3{0.0f, 2.0f, 0.0f},
			vec3{0.0f, 0.0f, 2.0f},
		};
		for (size_t i = 0; i < LIGHT_COUNT; ++i) {
			lights.setLightPosition(lightIDs[i], carrotCakeDisplayPosition + LIGHT_OFFSETS[i]);
			lights.setLightColor(lightIDs[i], Color::fromLinear(0.5f + 0.5f * sin(elapsedSeconds), 0.8f, 0.8f, 10.0f));
		}
	}

	void draw3DWorld(const app::FrameInfo& frameInfo) {
		const float elapsedSeconds = duration_cast<FloatSeconds>(frameInfo.totalElapsedTime).count();

		// Background.
		constexpr vec3 BACKGROUND_POSITION{0.0f, 3.5f, -10.0f};
		constexpr vec2 BACKGROUND_SIZE{18.0f, 18.0f};
		constexpr float BACKGROUND_ANGLE = -30.0f;
		constexpr float BACKGROUND_SPEED = 2.0f;
		instances3D.putQuadInstance({
			.texture = &testTexture,
			.position = BACKGROUND_POSITION,
			.orientation = angleAxis(convertDegreesToRadians(BACKGROUND_ANGLE), vec3{1.0f, 0.0f, 0.0f}),
			.size = BACKGROUND_SIZE,
			.origin{0.5f, 0.5f},
			.textureOffset{0.0f, elapsedSeconds * BACKGROUND_SPEED},
			.textureBasis = mat2{scale(1000.0f * BACKGROUND_SIZE / vec2{testTexture.getSize2D()})},
			.distanceOrderingBias = -10.0f,
		});

		// Cube.
		const mat4 cubeTransformation =           //
			translate(vec3{1.0f, -1.0f, -5.0f}) * //
			scale(vec3{0.5f, 0.5, 0.5f}) *        //
			rotate(vec3{elapsedSeconds * 2.0f, 0.0f, elapsedSeconds * 1.5f});
		instances3D.putPBRModelInstance(cubeModel, cubeTransformation, {.color = Color::RED});

		// Carrot cakes.
		const mat4 firstCarrotCakeTransformation =                             //
			translate(vec3{-0.6f, 0.2f, -3.0f}) *                              //
			scale(vec3{5.0f, 5.0f, 5.0f}) *                                    //
			rotate(vec3{0.0f, elapsedSeconds * 2.0f, elapsedSeconds * 1.5f}) * //
			translate(vec3{0.0f, -0.05f, 0.0f});
		instances3D.putPBRModelInstance(carrotCakeModel, firstCarrotCakeTransformation);

		const mat4 secondCarrotCakeTransformation =                            //
			translate(vec3{-0.5f, -2.0f, -5.0f}) *                             //
			scale(vec3{5.0f, 5.0f, 5.0f}) *                                    //
			rotate(vec3{elapsedSeconds * 2.0f, 0.0f, elapsedSeconds * 1.5f}) * //
			translate(vec3{0.0f, -0.05f, 0.0f});
		instances3D.putPBRModelInstance(carrotCakeModel, secondCarrotCakeTransformation);

		// Player carrot cake.
		const mat4 playerCarrotCakeTransformation =                                 //
			translate(carrotCakeDisplayPosition) *                                  //
			scale(vec3{5.0f * carrotCakeScale.x, 5.0f * carrotCakeScale.y, 5.0f}) * //
			rotate(vec3{0.0f, elapsedSeconds * 2.0f, elapsedSeconds * 1.5f}) *      //
			translate(vec3{0.0f, -0.05f, 0.0f});
		instances3D.putPBRModelInstance(carrotCakeModel, playerCarrotCakeTransformation);

		// Player name text.
		instances3D.putTextStringInstance(mainFont, "Carrot Cake",
			{
				.characterSize = 8,
				.position = carrotCakeDisplayPosition + vec3{0.0f, 0.7f, 0.0f},
				.scale{0.02f, 0.02f},
				.alignment = gfx::TextAlign::CENTER_HORIZONTALLY_BOTTOM,
				.color = Color::LIME,
			});
	}

	void draw2DWorld(const app::FrameInfo& frameInfo) {
		const float elapsedSeconds = duration_cast<FloatSeconds>(frameInfo.totalElapsedTime).count();

		// Left shaded transparent spinning test texture.
		instances2D.putShadedRectangleInstance(exampleShaderPipeline,
			{
				.texture = &testTexture,
				.position{40.0f, 370.0f},
				.angle = elapsedSeconds,
				.size{180.0f, 70.0f},
				.origin{0.5f, 0.5f},
				.color = Color::fromAlpha(0.2f),
			});

		// Middle plain spinning test texture.
		instances2D.putRectangleInstance({
			.texture = &testTexture,
			.position{100.0f, 380.0f},
			.angle = elapsedSeconds,
			.size{180.0f, 70.0f},
			.origin{0.5f, 0.5f},
		});

		// Right shaded transparent spinning test texture.
		instances2D.putShadedRectangleInstance(exampleShaderPipeline,
			{
				.texture = &testTexture,
				.position{160.0f, 390.0f},
				.angle = elapsedSeconds,
				.size{180.0f, 70.0f},
				.origin{0.5f, 0.5f},
				.color = Color::fromAlpha(0.2f),
			});
	}

	void drawUserInterface(const app::FrameInfo& frameInfo) {
		const float elapsedSeconds = duration_cast<FloatSeconds>(frameInfo.totalElapsedTime).count();

		instances2D.putImageInstance(testTexture,
			{
				.position{450.0f + cos(elapsedSeconds) * 25.0f, 100.0f + sin(elapsedSeconds) * 25.0f},
				.scale{0.1f + sin(elapsedSeconds) * 0.03f, 0.1f + cos(elapsedSeconds) * 0.05f},
				.origin{0.5f, 0.5f},
			});

		instances2D.putSpriteInstance(spriteAtlas, testSprite,
			{
				.position{570.0f + cos(elapsedSeconds) * 25.0f, 100.0f + sin(elapsedSeconds) * 25.0f},
				.scale{0.1f + sin(elapsedSeconds) * 0.03f, 0.1f + cos(elapsedSeconds) * 0.05f},
				.origin{0.5f, 0.5f},
			});

		instances2D.putSpriteInstance(spriteAtlas, testSubSprite,
			{
				.position{570.0f + cos(elapsedSeconds) * 20.0f, 180.0f + sin(elapsedSeconds) * 20.0f},
				.angle = sin(elapsedSeconds * 2.0f),
				.scale{0.2f + sin(elapsedSeconds) * 0.1f, 0.2f + cos(elapsedSeconds) * 0.1f},
				.origin{0.5f, 0.5f},
			});

		instances2D.putTextInstance(longTestText, {.position{410.0f, 416.0f}, .color = Color::LIME});

		temporaryText.assign(mainFont, 8,
			formatString("Position:\n({:.2f}, {:.2f}, {:.2f})\n\nScale:\n({:.2f}, {:.2f})", carrotCakeDisplayPosition.x, carrotCakeDisplayPosition.y, carrotCakeDisplayPosition.z,
				carrotCakeScale.x, carrotCakeScale.y));

		instances2D.putTextInstance(temporaryText, {.position{410.0f, 310.0f}});

		if (inputManager.isPressed(Action::MOVE_UP) || inputManager.justPressed(Action::MOVE_UP)) {
			instances2D.putTextStringInstance(mainFont, "^", {.characterSize = 8, .position{590.0f, 320.0f}});
		}
		if (inputManager.isPressed(Action::MOVE_DOWN) || inputManager.justPressed(Action::MOVE_DOWN)) {
			instances2D.putTextStringInstance(mainFont, "v", {.characterSize = 8, .position{590.0f, 300.0f}});
		}
		if (inputManager.isPressed(Action::MOVE_LEFT) || inputManager.justPressed(Action::MOVE_LEFT)) {
			instances2D.putTextStringInstance(mainFont, "<", {.characterSize = 8, .position{580.0f, 310.0f}});
		}
		if (inputManager.isPressed(Action::MOVE_RIGHT) || inputManager.justPressed(Action::MOVE_RIGHT)) {
			instances2D.putTextStringInstance(mainFont, ">", {.characterSize = 8, .position{600.0f, 310.0f}});
		}

		if (inputManager.isPressed(Action::AIM_UP) || inputManager.justPressed(Action::AIM_UP)) {
			instances2D.putTextStringInstance(mainFont, "^", {.characterSize = 8, .position{590.0f, 280.0f}});
		}
		if (inputManager.isPressed(Action::AIM_DOWN) || inputManager.justPressed(Action::AIM_DOWN)) {
			instances2D.putTextStringInstance(mainFont, "v", {.characterSize = 8, .position{590.0f, 260.0f}});
		}
		if (inputManager.isPressed(Action::AIM_LEFT) || inputManager.justPressed(Action::AIM_LEFT)) {
			instances2D.putTextStringInstance(mainFont, "<", {.characterSize = 8, .position{580.0f, 270.0f}});
		}
		if (inputManager.isPressed(Action::AIM_RIGHT) || inputManager.justPressed(Action::AIM_RIGHT)) {
			instances2D.putTextStringInstance(mainFont, ">", {.characterSize = 8, .position{600.0f, 270.0f}});
		}

		instances2D.putTextStringInstance(mainFont,
			formatString("Timer   A: {:.2f}\nCounter A: {}\n\nTimer   B: {:.2f}\nCounter B: {}", duration_cast<FloatSeconds>(timerA).count(), counterA,
				duration_cast<FloatSeconds>(timerB).count(), counterB),
			{
				.characterSize = 8,
				.position{410.0f, 240.0f},
			});

		if (inputManager.isPressed(evt::Input::KEY_SPACE)) {
			constexpr Capsule<2, float> STATIC_CAPSULE{.centerLine{.pointA{80.0f, 200.0f}, .pointB{300.0f, 80.0f}}, .radius = 50.0f};
			constexpr vec2 STATIC_CAPSULE_VECTOR = STATIC_CAPSULE.centerLine.pointB - STATIC_CAPSULE.centerLine.pointA;
			const grem::Rectangle<float> movingRectangle{.position = vec2{200.0f, 50.0f} + vec2{carrotCakeDisplayPosition} * 50.0f, .size{80.0f, 40.f}};
			const Color movingRectangleColor = (intersects(movingRectangle, STATIC_CAPSULE)) ? Color::RED : Color::YELLOW;

			instances2D.putRectangleInstance({
				.texture = &circleTexture,
				.position = STATIC_CAPSULE.centerLine.pointA,
				.size{STATIC_CAPSULE.radius * 2.0f, STATIC_CAPSULE.radius * 2.0f},
				.origin{0.5f, 0.5f},
				.color = Color::GREEN,
			});
			instances2D.putRectangleInstance({
				.texture = &circleTexture,
				.position = STATIC_CAPSULE.centerLine.pointB,
				.size{STATIC_CAPSULE.radius * 2.0f, STATIC_CAPSULE.radius * 2.0f},
				.origin{0.5f, 0.5f},
				.color = Color::GREEN,
			});
			instances2D.putRectangleInstance({
				.position = STATIC_CAPSULE.centerLine.pointA,
				.angle = getAngle(STATIC_CAPSULE_VECTOR),
				.size{length(STATIC_CAPSULE_VECTOR), STATIC_CAPSULE.radius * 2.0f},
				.origin{0.0f, 0.5f},
				.color = Color::GREEN,
			});

			instances2D.putRectangleInstance({
				.position = movingRectangle.position,
				.size = movingRectangle.size,
				.color = movingRectangleColor,
			});

			const auto drawBorder = [&](const Box<2, float>& box, float lineThickness, Color color) -> void {
				const vec2 extents = box.max - box.min;
				instances2D.putRectangleInstance({.position = box.min, .size{extents.x, lineThickness}, .origin{0.0f, 0.0f}, .color = color});
				instances2D.putRectangleInstance({.position{box.min.x, box.max.y}, .size{extents.x, lineThickness}, .origin{0.0f, 1.0f}, .color = color});
				instances2D.putRectangleInstance({.position = box.min, .size{lineThickness, extents.y}, .origin{0.0f, 0.0f}, .color = color});
				instances2D.putRectangleInstance({.position{box.max.x, box.min.y}, .size{lineThickness, extents.y}, .origin{1.0f, 0.0f}, .color = color});
			};

			quadtree.traverseNodes([&](const Box<2, float>& looseBounds, auto first, auto last) -> void {
				drawBorder(looseBounds, 2.0f, Color::BLANCHED_ALMOND);
				for (auto it = first; it != last; ++it) {
					const Circle<float>& circle = *it;
					instances2D.putRectangleInstance({
						.texture = &circleTexture,
						.position = circle.center,
						.size{circle.radius * 2.0f, circle.radius * 2.0f},
						.origin{0.5f, 0.5f},
						.color = Color::BLUE,
					});
				}
			});
			size_t aabbTestCount = 0;
			size_t circleTestCount = 0;
			quadtree.traverseNodes(
				[&](const Box<2, float>& looseBounds, auto first, auto last) -> void {
					drawBorder(looseBounds, 2.0f, Color::DARK_BLUE);
					for (auto it = first; it != last; ++it) {
						const Circle<float>& circle = *it;
						++circleTestCount;
						if (intersects(circle, movingRectangle)) {
							instances2D.putRectangleInstance({
								.texture = &circleTexture,
								.position = circle.center,
								.size{circle.radius * 2.0f, circle.radius * 2.0f},
								.origin{0.5f, 0.5f},
								.color = Color::DARK_GOLDEN_ROD,
							});
						}
					}
				},
				[&](const Box<2, float>& looseBounds) -> bool {
					++aabbTestCount;
					return intersects(movingRectangle, looseBounds);
				});

			instances2D.putTextStringInstance(mainFont, formatString("Broad tests: {}\nNarrow tests: {}", aabbTestCount, circleTestCount),
				{
					.characterSize = 8,
					.position{410.0f, 450.0f},
					.color = Color::BURLY_WOOD,
				});
		}

		if (inputManager.justReleased(evt::Input::KEY_SPACE)) {
			inputManager.releaseAll(Clock::now());
		}
	}

	void drawFrameRateCounter() {
		const size_t fps = getLastSecondFrameCount();
		temporaryText.assign(mainFont, 8, formatString("FPS: {}", fps), {0.0f, 0.0f}, {2.0f, 2.0f});
		const vec2 fpsPosition{15.0f + 2.0f, 480.0f - 15.0f - 20.0f};
		const Color fpsColor = (fps < 60) ? Color::RED : (fps < 120) ? Color::YELLOW : (fps < 240) ? Color::GRAY : Color::LIME;
		instances2D.putTextInstance(temporaryText, {.position = fpsPosition + vec2{1.0f, -1.0f}, .color = Color::BLACK});
		instances2D.putTextInstance(temporaryText, {.position = fpsPosition, .color = fpsColor});
	}

	evt::EventPump eventPump;
	gfx::Window window;
	gfx::Device device;
	gfx::Swapchain swapchain;
	gfx::Renderer2D renderer2D;
	gfx::Renderer3D renderer3D;
	imgui::GraphicalUserInterface gui;
	gfx::Camera3D worldCamera{device};
	gfx::Camera2D screenCamera{device};
	gfx::Viewport screenViewport{};
	gfx::Viewport screenViewportWithWorldScissor{};
	gfx::Viewport worldViewport{};
	ExampleShaderUniformBuffer exampleShaderUniformBuffer{device};
	ExampleShaderPipeline exampleShaderPipeline;
	gfx::Fog3D fog{device};
	gfx::Sky3D sky{device};
	gfx::Decals3D decals{device};
	gfx::Lights3D lights{device};
	gfx::LightProbeVolumes3D lightProbeVolumes{device};
	gfx::ReflectionProbes3D reflectionProbes{device};
	gfx::Instances2D instances2D{device, renderer2D};
	gfx::Instances3D instances3D{device, renderer3D};
	aud::Listener listener{};
	gfx::SpriteAtlas spriteAtlas{device};
	gfx::Texture testTexture;
	gfx::Texture circleTexture;
	gfx::Model3D cubeModel;
	gfx::Model3D carrotCakeModel;
	gfx::SpriteID testSprite;
	gfx::SpriteID testSubSprite;
	gfx::Font2D mainFont;
	gfx::Text2D longTestText{mainFont, 8,
		"The quick brown fox\n"
		"jumps over the lazy dog\n"
		"\n"
		"FLYGANDE BÄCKASINER SÖKA\n"
		"HWILA PÅ MJUKA TUVOR QXZ\n"
		"0123456789\n"
		"\n"
		"+!\"#%&/()=?`@${[]}\\\n"
		"~\'<>|,.-;:_"};
	gfx::Text2D temporaryText{};
	evt::InputManager inputManager;
	Optional<aud::SoundStage> soundStage{};
	Optional<aud::Sound> music{};
	aud::SoundInstanceID musicID{};
	float verticalFieldOfView;
	vec3 carrotCakeCurrentPosition{0.6f, 0.7f, -3.0f};
	vec3 carrotCakePreviousPosition = carrotCakeCurrentPosition;
	vec3 carrotCakeDisplayPosition = carrotCakeCurrentPosition;
	vec2 carrotCakeScale{1.0f, 1.0f};
	vec3 carrotCakeVelocity{0.0f, 0.0f, 0.0f};
	Duration timerA{};
	Duration timerB{};
	size_t counterA = 0;
	size_t counterB = 0;
	LooseQuadtree<Circle<float>> quadtree{Box<2, float>{.min{15.0f, 15.0f}, .max{15.0f + 380.0f, 15.0f + 450.0f}}, 32.0f};
	Array<gfx::LightID, LIGHT_COUNT> lightIDs{};
	bool showGUIDemoWindow = false;
};

} // namespace

int main(int argc, char* argv[]) {
	try {
		app::VirtualFilesystem filesystem{argv[0]};
		filesystem.setOutputDirectory(filesystem.createStandardOutputDirectory({
			.organizationName = "GREM",
			.applicationName = "ExampleTestGame",
		}));
		filesystem.mountInputArchive("data");
		filesystem.mountInputArchive("data/shared_2d");
		filesystem.mountInputArchive("data/shared_3d");
		filesystem.mountInputArchive("data/test_game");
		filesystem.mountInputArchivesInMountedDirectory("custom", "zip");
		filesystem.mountInputArchive(filesystem.getOutputDirectory());

		GameOptions gameOptions = json::deserializeFromString<GameOptions>(filesystem.readInputFileString("configuration/game.json"));
		try {
			cli::parseCommandLineOptions(gameOptions, argc, argv, {.longOptionPrefix = "-"});
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}

		Game game{filesystem, gameOptions};
		game.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
