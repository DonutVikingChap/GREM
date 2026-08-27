// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GRAPHICS_HPP
#define GREM_EXAMPLES_FPS_GRAPHICS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Camera2D.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>
#include <GREM/graphics_3d/Decals3D.hpp>
#include <GREM/graphics_3d/Fog3D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/LightBaker3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>
#include <GREM/physics/DebugVisualization.hpp>

#include "AssetCache.hpp"
#include "shaders.hpp"

struct GraphicsOptions {
	float bloomThreshold = 1.0f;
	float bloomFilterRadius = 1.0f;
	float bloomStrength = 0.02f;
};

struct Graphics {
	static constexpr uint32_t TEXT_CHARACTER_SIZE = 8;

	static constexpr Array FULLSCREEN_MESH_VERTICES{
		FullscreenVertex{.vertexPosition{0.0f, 0.0f}},
		FullscreenVertex{.vertexPosition{1.0f, 0.0f}},
		FullscreenVertex{.vertexPosition{0.0f, 1.0f}},
		FullscreenVertex{.vertexPosition{1.0f, 1.0f}},
	};

	static constexpr FullscreenVertexShaderConstants FULLSCREEN_VERTEX_SHADER_CONSTANTS{};

	static constexpr FullscreenFragmentShaderConstants FULLSCREEN_FRAGMENT_SHADER_CONSTANTS{};

	static constexpr gfx::ShaderPipelineOptions FULLSCREEN_SHADER_PIPELINE_OPTIONS{
		.depthBufferMode = gfx::DepthBufferMode::NONE,
		.primitiveType = gfx::PrimitiveType::TRIANGLE_STRIP,
		.faceCullingMode = gfx::FaceCullingMode::NONE,
	};

	using FullscreenShaderPipeline = gfx::ShaderPipeline<FullscreenMesh>;

	struct Baking {
		enum class State : uint8_t {
			DONE,
			BAKING_LIGHT_PROBES_DISTANCE,
			BAKING_LIGHT_PROBES_IRRADIANCE,
			BAKING_REFLECTION_PROBES,
		};

		gfx::LightBaker3D lightBaker;
		gfx::Sky3D radianceOnlySky;
		gfx::LightProbeVolumes3D previousBounceLightProbeVolumes;
		gfx::ReflectionProbes3D previousBounceReflectionProbes;
		gfx::Instances3D lightProbeOccluderInstances3D;
		size_t bounceIndex = 0;
		size_t bounceCount = 0;
		size_t lightProbeVolumeIndex = 0;
		size_t lightProbeIndex = 0;
		size_t reflectionProbeIndex = 0;
		size_t totalProgressIndex = 0;
		size_t totalProgressCount = 0;
		State state;
	};

	gfx::Device& device;
	gfx::Swapchain& swapchain;
	gfx::Renderer2D& renderer2D;
	gfx::Renderer3D& renderer3D;
	SharedPointer<gfx::Font2D> mainFont;
	gfx::Text2D temporaryText{};
	gfx::Fog3D fog;
	gfx::Sky3D sky;
	gfx::Decals3D decals;
	gfx::Lights3D lights;
	gfx::LightProbeVolumes3D lightProbeVolumes;
	gfx::ReflectionProbes3D reflectionProbes;
	FullscreenMesh fullscreenMesh;
	FullscreenShaderPipeline upscaleShaderPipeline;
	FullscreenShaderPipeline downscaleShaderPipeline;
	FullscreenShaderPipeline blurVerticalShaderPipeline;
	FullscreenShaderPipeline blurHorizontalShaderPipeline;
	FullscreenShaderPipeline bloomDownsampleShaderPipeline;
	FullscreenShaderPipeline bloomDownsampleFirstLevelShaderPipeline;
	FullscreenShaderPipeline bloomUpsampleShaderPipeline;
	FullscreenShaderPipeline bloomComposeShaderPipeline;
	FullscreenShaderPipeline tonemapShaderPipeline;
	FullscreenTextureBuffer fullscreenTextureBuffer;
	BloomComposeParameterBuffer bloomComposeParameterBuffer;
	gfx::DrawCommandBuffer<FullscreenMesh> fullscreenDrawCommandBuffer;
	gfx::Instances3D depthPrepassInstances3D;
	gfx::Instances3D visibleInstances3D;
	gfx::Instances3D shadowCasterInstances3D;
	gfx::DrawCommandBuffer<HullMesh> hullDrawCommandBuffer;
	gfx::InstanceBuffer<HullInstance> hullInstanceBuffer;
	gfx::Instances3D localPlayerDepthPrepassInstances3D;
	gfx::Instances3D localPlayerVisibleInWorldInstances3D;
	gfx::Instances3D localPlayerVisibleInViewInstances3D;
	gfx::Instances3D localPlayerShadowCasterInstances3D;
	gfx::Instances2D instances2D;
	Optional<Baking> baking{};
	gfx::RenderPass::Statistics worldRenderPassStatistics{};
	gfx::Viewport screenViewport{};
	gfx::Camera2D screenCamera;
	gfx::Texture renderColorTexture{};
	gfx::Texture renderDepthStencilTexture{};
	gfx::Texture resolvedColorTexture{};
	gfx::Texture bloomTextureHalfSize{};
	gfx::Texture bloomTextureQuarterSize{};
	gfx::Texture bloomTextureOneEighthSize{};
	gfx::Texture bloomTextureOneSixteenthSize{};
	gfx::Texture verticallyBlurredTexture{};
	gfx::Texture fullSizeColorTexture{};
	const phys::DebugVisualization3D* serverPhysicsDebugVisualization = nullptr;
	const phys::DebugVisualization3D* clientPhysicsDebugVisualization = nullptr;
	bool finishedLoadingAssets = false;

	Graphics(gfx::Device& device, gfx::Swapchain& swapchain, gfx::Renderer2D& renderer2D, gfx::Renderer3D& renderer3D, AssetCache& assetCache, const GraphicsOptions& options)
		: device(device)
		, swapchain(swapchain)
		, renderer2D(renderer2D)
		, renderer3D(renderer3D)
		, mainFont(assetCache.getFont("fonts/unscii/unscii-8.ttf"))
		, fog(device)
		, sky(device)
		, decals(device)
		, lights(device)
		, lightProbeVolumes(device)
		, reflectionProbes(device)
		, fullscreenMesh(device, FULLSCREEN_MESH_VERTICES)
		, upscaleShaderPipeline(device,                                                                                                  //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS, //
			  assetCache.getFragmentShader<UpscaleFragmentShader>(device, "shaders/upscale.frag"), FULLSCREEN_FRAGMENT_SHADER_CONSTANTS, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, downscaleShaderPipeline(device,                                                                                                    //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,     //
			  assetCache.getFragmentShader<DownscaleFragmentShader>(device, "shaders/downscale.frag"), FULLSCREEN_FRAGMENT_SHADER_CONSTANTS, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, blurVerticalShaderPipeline(device,                                                                                                        //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,            //
			  assetCache.getFragmentShader<BlurFragmentShader>(device, "shaders/blur.frag"), BlurFragmentShaderConstants{.BLUR_HORIZONTAL = false}, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, blurHorizontalShaderPipeline(device,                                                                                                     //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,           //
			  assetCache.getFragmentShader<BlurFragmentShader>(device, "shaders/blur.frag"), BlurFragmentShaderConstants{.BLUR_HORIZONTAL = true}, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, bloomDownsampleShaderPipeline(device, //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,
			  assetCache.getFragmentShader<BloomDownsampleFragmentShader>(device, "shaders/bloom_downsample.frag"),
			  BloomDownsampleFragmentShaderConstants{.BLOOM_THRESHOLD = options.bloomThreshold, .BLOOM_DOWNSAMPLE_FIRST_LEVEL = false}, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, bloomDownsampleFirstLevelShaderPipeline(device, //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,
			  assetCache.getFragmentShader<BloomDownsampleFragmentShader>(device, "shaders/bloom_downsample.frag"),
			  BloomDownsampleFragmentShaderConstants{.BLOOM_THRESHOLD = options.bloomThreshold, .BLOOM_DOWNSAMPLE_FIRST_LEVEL = true}, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, bloomUpsampleShaderPipeline(device, //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,
			  assetCache.getFragmentShader<BloomUpsampleFragmentShader>(device, "shaders/bloom_upsample.frag"),
			  BloomUpsampleFragmentShaderConstants{.BLOOM_FILTER_RADIUS = options.bloomFilterRadius}, //
			  gfx::ShaderPipelineOptions{
				  .depthBufferMode = gfx::DepthBufferMode::NONE,
				  .primitiveType = gfx::PrimitiveType::TRIANGLE_STRIP,
				  .faceCullingMode = gfx::FaceCullingMode::NONE,
				  .blendState =
					  gfx::BlendState{
						  .sourceColorBlendFactor = gfx::BlendFactor::ONE,
						  .destinationColorBlendFactor = gfx::BlendFactor::ONE,
						  .sourceAlphaBlendFactor = gfx::BlendFactor::ONE,
						  .destinationAlphaBlendFactor = gfx::BlendFactor::ONE,
					  },
			  })
		, bloomComposeShaderPipeline(device, //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS,
			  assetCache.getFragmentShader<BloomComposeFragmentShader>(device, "shaders/bloom_compose.frag"),
			  BloomComposeFragmentShaderConstants{.BLOOM_STRENGTH = options.bloomStrength}, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, tonemapShaderPipeline(device,                                                                                                  //
			  assetCache.getVertexShader<FullscreenVertexShader>(device, "shaders/fullscreen.vert"), FULLSCREEN_VERTEX_SHADER_CONSTANTS, //
			  assetCache.getFragmentShader<TonemapFragmentShader>(device, "shaders/tonemap.frag"), FULLSCREEN_FRAGMENT_SHADER_CONSTANTS, //
			  FULLSCREEN_SHADER_PIPELINE_OPTIONS)
		, fullscreenTextureBuffer(device)
		, bloomComposeParameterBuffer(device)
		, fullscreenDrawCommandBuffer(device)
		, depthPrepassInstances3D(device, renderer3D)
		, visibleInstances3D(device, renderer3D)
		, shadowCasterInstances3D(device, renderer3D)
		, hullDrawCommandBuffer(device)
		, hullInstanceBuffer(device)
		, localPlayerDepthPrepassInstances3D(device, renderer3D)
		, localPlayerVisibleInWorldInstances3D(device, renderer3D)
		, localPlayerVisibleInViewInstances3D(device, renderer3D)
		, localPlayerShadowCasterInstances3D(device, renderer3D)
		, instances2D(device, renderer2D)
		, screenCamera(device) {
		fullscreenDrawCommandBuffer.push(nullptr, fullscreenMesh);
	}

	void reset() {
		baking.reset();
		fog.setFog({});
		sky.setSky({});
		decals.clearDecals();
		decals.clearDecalMaterials();
		lights.clearLights();
		lightProbeVolumes.clearLightProbeVolumes();
		reflectionProbes.clearReflectionProbes();
		depthPrepassInstances3D.clear();
		visibleInstances3D.clear();
		shadowCasterInstances3D.clear();
		hullDrawCommandBuffer.clear();
		hullInstanceBuffer.clear();
		localPlayerDepthPrepassInstances3D.clear();
		localPlayerVisibleInWorldInstances3D.clear();
		localPlayerVisibleInViewInstances3D.clear();
		localPlayerShadowCasterInstances3D.clear();
		instances2D.clear();
		worldRenderPassStatistics = {};
	}

	void resize(Region2D screenRegion, Extent2D renderResolution, uint32_t maxMultisampleCount, bool enableBloom, bool enableBlur) {
		static constexpr gfx::TextureSamplerOptions CLAMPED_LINEAR_SAMPLER_OPTIONS{
			.minificationFilter = gfx::TextureFilter::LINEAR,
			.magnificationFilter = gfx::TextureFilter::LINEAR,
			.mipmapMode = gfx::TextureMipmapMode::NONE,
			.horizontalWrappingMode = gfx::TextureWrappingMode::CLAMP_TO_EDGE,
			.verticalWrappingMode = gfx::TextureWrappingMode::CLAMP_TO_EDGE,
			.maxAnisotropy = 1.0f,
		};

		screenViewport.region = screenRegion;
		screenCamera.setProjection(gfx::OrthographicProjection2D{.offset = screenRegion.offset, .size = screenRegion.size});

		if (maxMultisampleCount >= 2) {
			renderColorTexture =
				gfx::Texture::createRenderbuffer(device, gfx::TextureFormat::R16G16B16A16_FLOAT, renderResolution, maxMultisampleCount, gfx::UndefinedClearValues{});
			resolvedColorTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, renderResolution, 1,
				gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
		} else {
			renderColorTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, renderResolution, 1,
				gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
			resolvedColorTexture = {};
		}
		renderDepthStencilTexture =
			gfx::Texture::createRenderbuffer(device, gfx::TextureFormat::D32_FLOAT_S8_UINT, renderResolution, maxMultisampleCount, gfx::UndefinedClearValues{});

		const Extent2D halfSize = screenViewport.region.size / 2;
		if (enableBloom && halfSize.width > 0 && halfSize.height > 0) {
			bloomTextureHalfSize = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, halfSize, 1, gfx::UndefinedClearValues{},
				CLAMPED_LINEAR_SAMPLER_OPTIONS);
			if (const Extent2D quarterSize = screenViewport.region.size / 4; quarterSize.width > 0 && quarterSize.height > 0) {
				bloomTextureQuarterSize = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, quarterSize, 1,
					gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
			} else {
				bloomTextureQuarterSize = {};
			}
			if (const Extent2D oneEighthSize = screenViewport.region.size / 8; oneEighthSize.width > 0 && oneEighthSize.height > 0) {
				bloomTextureOneEighthSize = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, oneEighthSize, 1,
					gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
			} else {
				bloomTextureOneEighthSize = {};
			}
			if (const Extent2D oneSixteenthSize = screenViewport.region.size / 16; oneSixteenthSize.width > 0 && oneSixteenthSize.height > 0) {
				bloomTextureOneSixteenthSize = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, oneSixteenthSize, 1,
					gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
			} else {
				bloomTextureOneSixteenthSize = {};
			}
		} else {
			bloomTextureHalfSize = {};
			bloomTextureQuarterSize = {};
			bloomTextureOneEighthSize = {};
			bloomTextureOneSixteenthSize = {};
		}

		if (enableBlur) {
			verticallyBlurredTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, screenViewport.region.size, 1,
				gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
		} else {
			verticallyBlurredTexture = {};
		}

		if (screenViewport.region.size != renderResolution) {
			fullSizeColorTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R16G16B16A16_FLOAT, screenViewport.region.size, 1,
				gfx::UndefinedClearValues{}, CLAMPED_LINEAR_SAMPLER_OPTIONS);
		} else {
			fullSizeColorTexture = {};
		}
	}

	float put2DText(vec2 position, Color color, StringView string, float scale = 1.0f, gfx::TextAlign alignment = {}) {
		temporaryText.assign(*mainFont, TEXT_CHARACTER_SIZE, string, {0.0f, 0.0f}, vec2{scale});
		instances2D.putTextInstance(temporaryText, {.position = position + vec2{1.0f, -1.0f}, .alignment = alignment, .color = Color::BLACK});
		instances2D.putTextInstance(temporaryText, {.position = position, .alignment = alignment, .color = color});
		return temporaryText.getNextLineOffset().y;
	}

	void renderDownscale(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(downscaleShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderUpscale(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(upscaleShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderVerticalBlur(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(blurVerticalShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderHorizontalBlur(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(blurHorizontalShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderBloomDownsampleFirstLevel(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(bloomDownsampleFirstLevelShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderBloomDownsample(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::UndefinedClearValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(bloomDownsampleShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void renderBloomUpsample(gfx::Texture& output, const gfx::Texture& input) {
		gfx::RenderPass renderPass{device, output, gfx::RetainValues{}};
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(bloomUpsampleShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
		device.render(renderPass);
	}

	void drawBloomComposeTonemapped(gfx::RenderPass& renderPass, const gfx::Texture& mainTexture, const gfx::Texture& bloomTexture) {
		bloomComposeParameterBuffer.upload({.mainTexture = mainTexture, .bloomTexture = bloomTexture});
		renderPass.drawShaded(bloomComposeShaderPipeline, fullscreenDrawCommandBuffer, bloomComposeParameterBuffer);
	}

	void drawTonemapped(gfx::RenderPass& renderPass, const gfx::Texture& input) {
		fullscreenTextureBuffer.upload({.mainTexture = input});
		renderPass.drawShaded(tonemapShaderPipeline, fullscreenDrawCommandBuffer, fullscreenTextureBuffer);
	}
};

#endif
