// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../ClientSettings.hpp"
#include "../../Graphics.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_map.hpp"

class WorldViewGraphicsRenderingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"WorldViewGraphicsRenderingSystem requires graphics."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void prepareToRender3DGraphics(Graphics& graphics, bool isSplitScreen) override {
		if (isSplitScreen) {
			graphics.device.render(gfx::RenderPass{graphics.device, {graphics.renderColorTexture, graphics.renderDepthStencilTexture}, gfx::ClearValues{}});
		}
	}

	void renderLocalPlayer3DGraphics(Graphics& graphics, bool isSplitScreen, const WorldView& worldView, const LocalPlayerID&, const gfx::Viewport& viewport,
		const gfx::Camera3D& camera) override {
		GREM_PROFILE_FUNCTION();

		const Box<3, float> totalShadowCasterBoundingBox = worldView.subtickResources.getResource<MapInfo>().bounds.in(phys::METERS);

		{
			GREM_PROFILE_BLOCK("Render shadow maps");
			graphics.renderer3D.renderAllShadowMaps(graphics.lights, totalShadowCasterBoundingBox, camera, [&](gfx::RenderPass& renderPass, const gfx::Camera3D& camera) -> void {
				graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
					{
						{graphics.shadowCasterInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}},
						{graphics.localPlayerShadowCasterInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}},
					},
					camera);
			});
		}

		{
			gfx::ResolveMode resolveMode = gfx::DiscardIntermediateValues{};
			gfx::ClearMode clearMode = gfx::ClearValues{};
			if (isSplitScreen) {
				resolveMode = gfx::StoreIntermediateValues{};
				clearMode = gfx::RetainValues{};
			}

			GREM_PROFILE_BLOCK("Render world view");
			gfx::RenderPass renderPass =
				(graphics.resolvedColorTexture)
					? gfx::RenderPass{graphics.device, graphics.resolvedColorTexture, resolveMode, {graphics.renderColorTexture, graphics.renderDepthStencilTexture}, clearMode,
						  viewport}
					: gfx::RenderPass{graphics.device, {graphics.renderColorTexture, graphics.renderDepthStencilTexture}, clearMode, viewport};

			{
				GREM_PROFILE_BLOCK("Record depth prepass");
				graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
					{
						{graphics.depthPrepassInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}},
						{graphics.localPlayerDepthPrepassInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}},
					},
					camera);
			}

			{
				GREM_PROFILE_BLOCK("Record world draw");
				graphics.renderer3D.drawHDRPBRFrame(renderPass,
					{
						graphics.visibleInstances3D,
						graphics.localPlayerVisibleInWorldInstances3D,
					},
					camera, graphics.fog, graphics.sky, graphics.decals, graphics.lights, graphics.lightProbeVolumes, graphics.reflectionProbes);
			}

			{
				GREM_PROFILE_BLOCK("Record depth buffer clear");
				renderPass.fill(viewport.region, gfx::ClearValues{.aspects = gfx::TextureAspect::DEPTH, .depth = 1.0f});
			}

			{
				GREM_PROFILE_BLOCK("Record view draw");
				graphics.renderer3D.drawHDRPBRFrame(renderPass,
					{
						graphics.localPlayerVisibleInViewInstances3D,
					},
					camera, graphics.fog, {graphics.sky, {.skipSkyRendering = true}}, graphics.decals, graphics.lights, graphics.lightProbeVolumes, graphics.reflectionProbes);
			}

			{
				GREM_PROFILE_BLOCK("Record hull draw");
				renderPass.draw(graphics.hullDrawCommandBuffer, graphics.hullInstanceBuffer, camera.getParameterBuffer());
			}

			graphics.worldRenderPassStatistics += renderPass.getStatistics();
			graphics.device.render(renderPass);
		}

		graphics.localPlayerDepthPrepassInstances3D.clear();
		graphics.localPlayerVisibleInWorldInstances3D.clear();
		graphics.localPlayerVisibleInViewInstances3D.clear();
		graphics.localPlayerShadowCasterInstances3D.clear();
	}

	void renderGraphics(Graphics& graphics, const WorldView& worldView, gfx::Texture* renderTargetOverride) override {
		GREM_PROFILE_FUNCTION();

		const ClientSettings& settings = worldView.subtickResources.getResource<ClientSettings>();

		gfx::Texture* fullSizeColorTexture = (graphics.resolvedColorTexture) ? &graphics.resolvedColorTexture : &graphics.renderColorTexture;
		if (graphics.fullSizeColorTexture) {
			if (graphics.fullSizeColorTexture.getHeight() < fullSizeColorTexture->getHeight()) {
				GREM_PROFILE_BLOCK("Render downscale");
				graphics.renderDownscale(graphics.fullSizeColorTexture, *fullSizeColorTexture);
			} else {
				GREM_PROFILE_BLOCK("Render upscale");
				graphics.renderUpscale(graphics.fullSizeColorTexture, *fullSizeColorTexture);
			}
			fullSizeColorTexture = &graphics.fullSizeColorTexture;
		}

		if (graphics.verticallyBlurredTexture) {
			{
				GREM_PROFILE_BLOCK("Render vertical blur");
				graphics.renderVerticalBlur(graphics.verticallyBlurredTexture, *fullSizeColorTexture);
			}
			{
				GREM_PROFILE_BLOCK("Render horizontal blur");
				graphics.renderHorizontalBlur(*fullSizeColorTexture, graphics.verticallyBlurredTexture);
			}
		}

		if (graphics.bloomTextureHalfSize) {
			GREM_PROFILE_BLOCK("Render bloom");
			graphics.renderBloomDownsampleFirstLevel(graphics.bloomTextureHalfSize, *fullSizeColorTexture);
			if (graphics.bloomTextureQuarterSize) {
				graphics.renderBloomDownsample(graphics.bloomTextureQuarterSize, graphics.bloomTextureHalfSize);
				if (graphics.bloomTextureOneEighthSize) {
					graphics.renderBloomDownsample(graphics.bloomTextureOneEighthSize, graphics.bloomTextureQuarterSize);
					if (graphics.bloomTextureOneSixteenthSize) {
						graphics.renderBloomDownsample(graphics.bloomTextureOneSixteenthSize, graphics.bloomTextureOneEighthSize);
						graphics.renderBloomUpsample(graphics.bloomTextureOneEighthSize, graphics.bloomTextureOneSixteenthSize);
					}
					graphics.renderBloomUpsample(graphics.bloomTextureQuarterSize, graphics.bloomTextureOneEighthSize);
				}
				graphics.renderBloomUpsample(graphics.bloomTextureHalfSize, graphics.bloomTextureQuarterSize);
			}
		}

		{
			GREM_PROFILE_BLOCK("Render to swapchain");
			gfx::RenderPass renderPass{graphics.device, (renderTargetOverride) ? *renderTargetOverride : graphics.swapchain, gfx::UndefinedClearValues{}, graphics.screenViewport};

			if (graphics.bloomTextureHalfSize) {
				GREM_PROFILE_BLOCK("Record 3D world bloom composition with tonemapping");
				graphics.drawBloomComposeTonemapped(renderPass, *fullSizeColorTexture, graphics.bloomTextureHalfSize);
			} else {
				GREM_PROFILE_BLOCK("Record 3D world with tonemapping");
				graphics.drawTonemapped(renderPass, *fullSizeColorTexture);
			}

			if (settings.graphics.showPerformanceStats) {
				GREM_PROFILE_BLOCK("Record 2D performance stats");
				const gfx::FeatureSupport supportedFeatures = graphics.device.getSupportedFeatures();
				const vec2 position{
					static_cast<float>(graphics.screenViewport.region.offset.x) + 15.0f + 2.0f,
					static_cast<float>(graphics.screenViewport.region.offset.y) + static_cast<float>(graphics.screenViewport.region.size.height) - 15.0f - 120.0f,
				};
				graphics.put2DText(position, Color::WHITE,
					formatSmallString<256>("Graphics API: {} {}\nForward 3D Render Pass Statistics:\n  Vertices: {}\n  Indices: {}\n  Instances: {}\n  Draw Calls: {}",
						supportedFeatures.graphicsBackendAPIName, supportedFeatures.graphicsBackendAPIVersionName, graphics.worldRenderPassStatistics.totalDrawnVertexCount,
						graphics.worldRenderPassStatistics.totalDrawnIndexCount, graphics.worldRenderPassStatistics.totalDrawnInstanceCount,
						graphics.worldRenderPassStatistics.totalDrawCallCount));
			}

			{
				GREM_PROFILE_BLOCK("Record 2D user interface");
				graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.screenCamera);
			}

			graphics.device.render(renderPass);
		}

		graphics.depthPrepassInstances3D.clear();
		graphics.visibleInstances3D.clear();
		graphics.shadowCasterInstances3D.clear();
		graphics.hullDrawCommandBuffer.clear();
		graphics.hullInstanceBuffer.clear();
		graphics.instances2D.clear();
		graphics.worldRenderPassStatistics = {};

		graphics.finishedLoadingAssets = true;
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createWorldViewGraphicsRenderingSystem() { // NOLINT(misc-use-internal-linkage)
	return new WorldViewGraphicsRenderingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
