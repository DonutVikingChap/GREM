// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_GAME_HPP
#define GREM_EXAMPLES_TILES_GAME_HPP

#include <GREM/aliases.hpp>
#include <GREM/application.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

#include "Graphics.hpp"
#include "Layer.hpp"
#include "layers/MenuLayer.hpp"
#include "layers/PlayLayer.hpp"
#include "layers/WorldLayer.hpp"

#include <utility> // std::move

struct GameOptions {
	app::ApplicationOptions app{
		.tickInterval = 1.0_seconds / 128,
	};
	exec::DynamicExecutorOptions exe{};
	evt::EventPumpOptions ev{};
	gfx::WindowOptions wnd{
		.title = "Example Tiles",
		.size{640, 480},
		.multisampleCount = 0,
		.resizable = true,
	};
	gfx::DeviceOptions dev{};
	gfx::SwapchainOptions swap{};
	Extent2D renderSize{640, 480};
	Region2D worldViewportRenderRegion{.offset{32, 32}, .size{576, 416}};
	CStringView schemaFilepath = "schema.json5";
	Extent2D maxTilesetSizeInPixels{1024};
	Optional<Schema::WorldSeed> worldSeed{};
};

class Game final : public app::Application {
public:
	Game(Filesystem& filesystem, const GameOptions& options)
		: app::Application(options.app)
		, filesystem(filesystem)
		, executor(options.exe)
		, eventPump(options.ev)
		, window(options.wnd)
		, device(filesystem, window, options.dev)
		, swapchain(device, window, options.swap)
		, graphics(filesystem, device, options.renderSize, options.worldViewportRenderRegion)
		, schema(device, filesystem, options.schemaFilepath, options.maxTilesetSizeInPixels, options.worldSeed) {
		window.setIcon(res::Image{filesystem, "textures/icon.png"});

		graphics.resize(window.getSize(), window.getDrawableSize());

		handleContinuation(Layer::GoToMainMenu{});
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		for (size_t layerIndex = layers.size(); layerIndex-- > lowestActiveLayerIndex;) {
			layers[layerIndex]->prepareForEvents();
		}

		for (const evt::Event& event : eventPump.pollEvents()) {
			handleEvent(event);
			for (size_t layerIndex = layers.size(); layerIndex-- > lowestActiveLayerIndex;) {
				if (layers[layerIndex]->handleEvent(graphics, event)) {
					break;
				}
			}
		}

		lowestActiveLayerIndex = layers.size();
		while (lowestActiveLayerIndex > 0) {
			--lowestActiveLayerIndex;
			Layer::Continuation continuation = layers[lowestActiveLayerIndex]->update(graphics, frameInfo.deltaTime);
			if (match(continuation)([&](auto& other) -> bool { return handleContinuation(std::move(other)); })) {
				break;
			}
		}
	}

	void tick(app::TickInfo tickInfo) override {
		for (size_t layerIndex = lowestActiveLayerIndex; layerIndex < layers.size(); ++layerIndex) {
			layers[layerIndex]->tick(executor, graphics, tickInfo.tickInterval);
		}
	}

	void display(app::FrameInfo frameInfo) override {
		const size_t fps = getLastSecondFrameCount();
		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::fromLinear(Color::PURPLE.toLinearRGB() * 0.05f)}, graphics.viewport};
		for (size_t layerIndex = lowestActiveLayerIndex; layerIndex < layers.size(); ++layerIndex) {
			layers[layerIndex]->draw(device, graphics, renderPass, frameInfo.tickInterpolationAlpha, fps);
		}
		device.render(renderPass);
		device.present(swapchain);
	}

private:
	void handleEvent(const evt::Event& event) {
		GREM_MATCH(event) {
			GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
				handleContinuation(Layer::QuitGame{});
				break;
			}
			GREM_CASE(const evt::WindowResizedEvent& resized) {
				if (resized.windowID == window.getID()) {
					graphics.resize(resized.windowSize, graphics.windowDrawableSize);
				}
				break;
			}
			GREM_CASE(const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
				if (drawableSizeChanged.windowID == window.getID()) {
					graphics.resize(graphics.windowSize, drawableSizeChanged.windowDrawableSize);
				}
				break;
			}
			GREM_CASE(const evt::KeyPressedEvent& pressed) {
				if (pressed.keyCode == evt::KeyCode::F4) {
					setMinFrameTime((getMinFrameTime() == Duration{}) ? 1.0_seconds / 480 : Duration{});
				} else if (pressed.keyCode == evt::KeyCode::F5) {
					for (const UniquePointer<Layer>& layer : layers) {
						layer->reloadShaders(filesystem, device);
					}
				} else if (pressed.keyCode == evt::KeyCode::F8) {
					GREM_PROFILER_SAVE_NEXT_N_FRAMES(16, "tiles_profiler_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
				} else if (pressed.keyCode == evt::KeyCode::F10) {
					handleContinuation(Layer::QuitGame{});
				} else if (pressed.keyCode == evt::KeyCode::F11 || (pressed.keyCode == evt::KeyCode::RETURN && pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT))) {
					window.setFullscreen(!window.isFullscreen());
				}
				break;
			}
			GREM_CASE_DEFAULT(const auto& other) break;
		}
	}

	bool setLayer(UniquePointer<Layer> newLayer) {
		layers.clear();
		layers.push_back(std::move(newLayer));
		lowestActiveLayerIndex = 1;
		return false;
	}

	bool pushLayer(UniquePointer<Layer> addedLayer) {
		layers.push_back(std::move(addedLayer));
		lowestActiveLayerIndex = layers.size();
		return false;
	}

	bool popLayer() {
		if (lowestActiveLayerIndex == layers.size()) {
			--lowestActiveLayerIndex;
		}
		layers.pop_back();
		return false;
	}

	bool clearLayers() {
		layers.clear();
		lowestActiveLayerIndex = 0;
		return true;
	}

	bool handleContinuation(const Layer::ContinueDownLayerStack&) {
		return false;
	}

	bool handleContinuation(const Layer::BreakFromLayerStack&) {
		return true;
	}

	bool handleContinuation(const Layer::QuitGame&) {
		quit();
		return clearLayers();
	}

	bool handleContinuation(const Layer::GoToMainMenu&) {
		return setLayer(UniquePointer<MenuLayer>::create(filesystem, "Tiles", nullopt,
			MenuLayer::Items{
				{.label = "Start Game", .onSelect = Layer::LoadSelectedMap{}},
				{.label = "Map", .onSelect = Layer::SelectNextMap{}, .getValueString = [this] { return schema.mapFilepaths[schema.mapIndex]; }},
				{.label = "World Seed", .onSelect = Layer::RandomizeWorldSeed{}, .getValueString = [this] { return toString(schema.worldSeed); }},
				{.label = "Quit", .onSelect = Layer::QuitGame{}},
			}));
	}

	bool handleContinuation(const Layer::PauseGame&) {
		return pushLayer(UniquePointer<MenuLayer>::create(filesystem, "Paused", Layer::ResumeGame{},
			MenuLayer::Items{
				{.label = "Resume Game", .onSelect = Layer::ResumeGame{}},
				{.label = "Exit to Main Menu", .onSelect = Layer::GoToMainMenu{}},
				{.label = "Quit", .onSelect = Layer::QuitGame{}},
			}));
	}

	bool handleContinuation(const Layer::ResumeGame&) {
		return popLayer();
	}

	bool handleContinuation(const Layer::SelectNextMap&) {
		schema.mapIndex = (schema.mapIndex + 1) % schema.mapFilepaths.size();
		return true;
	}

	bool handleContinuation(const Layer::RandomizeWorldSeed&) {
		schema.worldSeed = static_cast<Schema::WorldSeed>(rng::NonDeterministicRandomEngine{}());
		return true;
	}

	bool handleContinuation(const Layer::LoadSelectedMap&) {
		return setLayer(UniquePointer<WorldLayer>::create(filesystem, schema));
	}

	bool handleContinuation(const Layer::StartPlaying& startPlaying) {
		return pushLayer(UniquePointer<PlayLayer>::create(filesystem, device, *startPlaying.world));
	}

	Filesystem& filesystem;
	exec::DynamicExecutor executor;
	evt::EventPump eventPump;
	gfx::Window window;
	gfx::Device device;
	gfx::Swapchain swapchain;
	Graphics graphics;
	Schema schema;
	ArrayList<UniquePointer<Layer>> layers{};
	size_t lowestActiveLayerIndex = 0;
};

#endif
