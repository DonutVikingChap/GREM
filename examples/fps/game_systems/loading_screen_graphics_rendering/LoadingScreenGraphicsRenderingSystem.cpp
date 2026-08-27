// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Texture.hpp>

#include "../../ClientState.hpp"
#include "../../Graphics.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"

class LoadingScreenGraphicsRenderingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"LoadingScreenGraphicsRenderingSystem requires graphics."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void renderGraphics(Graphics& graphics, const WorldView& worldView, gfx::Texture* renderTargetOverride) override {
		GREM_PROFILE_FUNCTION();

		String loadingMessageA{};
		String loadingMessageB{};
		String loadingMessageC{};

		switch (worldView.subtickResources.getResource<ClientState>()) {
			case ClientState::IDLE: break;
			case ClientState::CONNECTING: loadingMessageA = "   Connecting..."; break;
			case ClientState::LOADING_MAP: loadingMessageA = "   Loading map..."; break;
			case ClientState::LIGHT_BAKING: {
				switch (graphics.baking->state) {
					case Graphics::Baking::State::BAKING_LIGHT_PROBES_DISTANCE: {
						const Span<const gfx::LightProbeVolumeOptions3D> lightProbeVolumesVolumeOptions = graphics.lightProbeVolumes.getVolumeOptions();
						const gfx::LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumesVolumeOptions[graphics.baking->lightProbeVolumeIndex];
						const size_t lightProbeCount = static_cast<size_t>(volumeOptions.probeCounts.x * volumeOptions.probeCounts.y * volumeOptions.probeCounts.z);
						const size_t dotCount = (graphics.baking->lightProbeIndex / 256) % 4;
						loadingMessageA = "   Baking light probe distances";
						loadingMessageA.append(dotCount, '.');
						loadingMessageA.append(3 - dotCount, ' ');
						loadingMessageB = formatString("(volume {}/{}, probe {}/{})", graphics.baking->lightProbeVolumeIndex + 1, lightProbeVolumesVolumeOptions.size(),
							graphics.baking->lightProbeIndex + 1, lightProbeCount);
						loadingMessageC = formatString("({}% total)", (100 * graphics.baking->totalProgressIndex) / graphics.baking->totalProgressCount);
						break;
					}
					case Graphics::Baking::State::BAKING_LIGHT_PROBES_IRRADIANCE: {
						const Span<const gfx::LightProbeVolumeOptions3D> lightProbeVolumesVolumeOptions = graphics.lightProbeVolumes.getVolumeOptions();
						const gfx::LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumesVolumeOptions[graphics.baking->lightProbeVolumeIndex];
						const size_t lightProbeCount = static_cast<size_t>(volumeOptions.probeCounts.x * volumeOptions.probeCounts.y * volumeOptions.probeCounts.z);
						const size_t dotCount = (graphics.baking->lightProbeIndex / 256) % 4;
						loadingMessageA = "   Baking light probe irradiance";
						loadingMessageA.append(dotCount, '.');
						loadingMessageA.append(3 - dotCount, ' ');
						loadingMessageB = formatString("(bounce {}/{}, volume {}/{}, probe {}/{})", graphics.baking->bounceIndex + 1, graphics.baking->bounceCount,
							graphics.baking->lightProbeVolumeIndex + 1, lightProbeVolumesVolumeOptions.size(), graphics.baking->lightProbeIndex + 1, lightProbeCount);
						loadingMessageC = formatString("({}% total)", (100 * graphics.baking->totalProgressIndex) / graphics.baking->totalProgressCount);
						break;
					}
					case Graphics::Baking::State::BAKING_REFLECTION_PROBES: {
						const Span<const gfx::ReflectionProbeOptions3D> reflectionProbesProbeOptions = graphics.reflectionProbes.getProbeOptions();
						const size_t dotCount = (graphics.baking->reflectionProbeIndex / 4) % 4;
						loadingMessageA = "   Baking reflection probe reflections";
						loadingMessageA.append(dotCount, '.');
						loadingMessageA.append(3 - dotCount, ' ');
						loadingMessageB = formatString("(bounce {}/{}, probe {}/{})", graphics.baking->bounceIndex + 1, graphics.baking->bounceCount,
							graphics.baking->reflectionProbeIndex + 1, reflectionProbesProbeOptions.size());
						loadingMessageC = formatString("({}% total)", (100 * graphics.baking->totalProgressIndex) / graphics.baking->totalProgressCount);
						break;
					}
					case Graphics::Baking::State::DONE: break;
				}
				break;
			}
			case ClientState::JOINING_GAME: loadingMessageA = "   Joining game..."; break;
			case ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT: loadingMessageA = "   Receiving game state..."; break;
			case ClientState::PLAYING_GAME: break;
		}

		GREM_PROFILE_BLOCK("Render to swapchain");
		gfx::RenderPass renderPass{graphics.device, (renderTargetOverride) ? *renderTargetOverride : graphics.swapchain,
			gfx::ClearValues{.color = Color::fromLinear(Color::PURPLE.toLinearRGB() * 0.25f)}, graphics.screenViewport};

		{
			GREM_PROFILE_BLOCK("Draw loading screen");
			vec2 position = graphics.screenViewport.region.offset + graphics.screenViewport.region.size / 2;
			graphics.put2DText(position, Color::LIGHT_GRAY, loadingMessageA, 2.0f, gfx::TextAlign::CENTER);
			position.y -= 26.0f;
			graphics.put2DText(position, Color::LIGHT_GRAY, loadingMessageB, 2.0f, gfx::TextAlign::CENTER);
			position.y -= 26.0f;
			graphics.put2DText(position, Color::LIGHT_GRAY, loadingMessageC, 2.0f, gfx::TextAlign::CENTER);
			graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.screenCamera);
		}

		graphics.device.render(renderPass);

		graphics.instances2D.clear();
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createLoadingScreenGraphicsRenderingSystem() { // NOLINT(misc-use-internal-linkage)
	return new LoadingScreenGraphicsRenderingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
