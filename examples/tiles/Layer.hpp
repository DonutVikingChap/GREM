// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_LAYER_HPP
#define GREM_EXAMPLES_TILES_LAYER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>
#include <GREM/graphics.hpp>

struct Graphics;
struct World;

class Layer {
public:
	struct ContinueDownLayerStack {};
	struct BreakFromLayerStack {};
	struct QuitGame {};
	struct GoToMainMenu {};
	struct PauseGame {};
	struct ResumeGame {};
	struct SelectNextMap {};
	struct RandomizeWorldSeed {};
	struct LoadSelectedMap {};
	struct StartPlaying {
		World* world;
	};
	using Continuation = Variant< //
		ContinueDownLayerStack,   //
		BreakFromLayerStack,      //
		QuitGame,                 //
		GoToMainMenu,             //
		PauseGame,                //
		ResumeGame,               //
		SelectNextMap,            //
		RandomizeWorldSeed,       //
		LoadSelectedMap,          //
		StartPlaying>;

	virtual ~Layer() = default;

	virtual void reloadShaders(const Filesystem& filesystem, gfx::Device& device) {
		(void)filesystem;
		(void)device;
	}

	virtual void prepareForEvents() {}

	[[nodiscard]] virtual bool handleEvent(const Graphics& graphics, const evt::Event& event) {
		(void)graphics;
		(void)event;
		return false;
	}

	[[nodiscard]] virtual Continuation update(const Graphics& graphics, Duration deltaTime) {
		(void)graphics;
		(void)deltaTime;
		return ContinueDownLayerStack{};
	}

	virtual void tick(exec::Executor& executor, const Graphics& graphics, Duration tickInterval) {
		(void)executor;
		(void)graphics;
		(void)tickInterval;
	}

	virtual void draw(gfx::Device& device, Graphics& graphics, gfx::RenderPass& renderPass, float tickInterpolationAlpha, size_t fps) {
		(void)device;
		(void)graphics;
		(void)renderPass;
		(void)tickInterpolationAlpha;
		(void)fps;
	}
};

#endif
