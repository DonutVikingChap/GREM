// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_RENDERER_3D_HPP
#define GREM_GRAPHICS_3D_RENDERER_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/RangeAllocator.hpp>
#include <GREM/core/data/Registry.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/Cubemap3D.hpp>
#include <GREM/graphics_3d/Decals3D.hpp>
#include <GREM/graphics_3d/Fog3D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>
#include <GREM/resource/Image.hpp>
#include <GREM/resource/Model.hpp>

#include <new>     // std::bad_alloc
#include <utility> // std::move

namespace grem::graphics {

class Device;       // Forward declaration, to avoid including Device.hpp.
class Bootstrapper; // Forward declaration.

/**
 * Configuration options for a Renderer3D.
 */
struct Renderer3DOptions {
	/**
	 * Width or height of each screen tile, in pixels.
	 *
	 * Must be positive.
	 */
	uint32_t tileSize = 64;

	/**
	 * Number of depth bins to use for culling items on the screen along the Z
	 * axis.
	 *
	 * Must be positive.
	 */
	uint32_t depthBinCount = 1024;

	/**
	 * The width, in texels, of the generated bidirectional reflectance
	 * distribution function (BRDF) integration map for split-sum approximation
	 * of ambient specular reflections.
	 *
	 * Must be a power of 2.
	 */
	uint32_t specularSplitSumBRDFIntegrationMapResolution = 512;

	/**
	 * Number of samples to use when generating the bidirectional reflectance
	 * distribution function (BRDF) integration map for split-sum approximation
	 * of ambient specular reflections.
	 *
	 * Must be positive.
	 */
	uint32_t specularSplitSumBRDFIntegrationMapSampleCount = 2048;

	/**
	 * Slope-scaled depth bias factor to use in the shadow map shader.
	 */
	float shadowMapDepthBiasSlopeFactor = 0.0f;

	/**
	 * Constant depth bias factor to use in the shadow map shader.
	 */
	float shadowMapDepthBiasConstantFactor = 1.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Renderer3DOptions& other) const noexcept = default;
};

/**
 * Persistent system that provides 3D rendering capabilities.
 */
class Renderer3D {
public:
	/** Default vertex shader for drawing 2D models with a 3D transformation. */
	using Model2DTransformed3DVertexShader =
		graphics::VertexShader<Model2D::Mesh, Model2D::VertexShaderConstants, Model2D::VertexShaderOutputs, Camera3D::ParameterBuffer, Model2D::Transformation3DBuffer>;

	/** Default fragment shader for drawing 2D models with a 3D transformation. */
	using Model2DTransformed3DFragmentShader = graphics::FragmentShader<Model2D::Mesh, Model2D::VertexShaderOutputs, Model2D::FragmentShaderConstants,
		Model2D::FragmentShaderOutputs, Camera3D::ParameterBuffer, Model2D::Transformation3DBuffer, Model2D::TextureBuffer>;

	/** Struct of shader parameters for PBR rendering. */
	struct PBRParameters {
		/** Sampler for the bidirectional reflectance distribution function (BRDF) integration map for split-sum approximation of ambient specular reflections. */
		sampler2D pbrSpecularSplitSumBRDFIntegrationMap;
	};

	/** Shader buffer for PBR rendering parameters. */
	using PBRParameterBuffer = UniformBuffer<PBRParameters, "Renderer3DPBRParameters">;

	/** Struct of shader fields representing an on-screen decal. */
	struct ScreenDecalFields {
		/** Combined view-projection matrix of the decal. */
		mat4 decalMatrix;

		/** Direction vector (XYZ) and range (W) of the decal. */
		vec4 decalDirectionAndRange;

		/** Texture offset (XY) and scale (ZW) of the base color map in the decal atlas texture. */
		vec4 decalBaseColorTextureOffsetAndScale;

		/** Texture offset (XY) and scale (ZW) of the normal map in the decal atlas texture. */
		vec4 decalNormalTextureOffsetAndScale;

		/** Texture offset (XY) and scale (ZW) of the occlusion-roughness-metallic map in the decal atlas texture. */
		vec4 decalOcclusionRoughnessMetallicTextureOffsetAndScale;

		/** Texture offset (XY) and scale (ZW) of the emissive map in the decal atlas texture. */
		vec4 decalEmissiveTextureOffsetAndScale;

		/** Base color factor. */
		vec4 decalBaseColorFactor;

		/** Occlusion strength (X), roughness factor (Y), metallic factor (Z) and normal scale (W). */
		vec4 decalOcclusionRoughnessMetallicFactorAndNormalScale;

		/** Emissive factor. */
		vec3 decalEmissiveFactor;

		/** Model identifier that the decal applies to, or the maximum value to apply to all models. */
		uint32_t decalModelInstanceIdentifier;
	};

	/** Shader buffer for on-screen decals. */
	using ScreenDecalBuffer = StorageBuffer<ScreenDecalFields, "Renderer3DScreenDecals">;

	/** Struct of shader fields representing an on-screen light source. */
	struct ScreenLightFields {
		/** Direction (XYZ) vector and maximum range (W) of the light. */
		vec4 lightDirectionAndRange;

		/** Color (XYZ) and intensity factor (W) of the light. */
		vec4 lightColorAndIntensity;

		/** Position (XYZ) and type index (W) of the light. */
		vec4 lightPositionAndType;

		/** Cosine of the inner (X) and outer (Y) cone angles, as well as the shadow map index (Z) and shadow matrix index (W) of the light. */
		vec4 lightConeCosinesAndShadowMapIndexAndShadowMatrixIndex;

		/** Distances to the near (X) and far (Y) planes of the light shadow, as well as the constant (Z) and slope-scaled (W) normal offset bias factors to use when sampling the shadow map of the light. */
		vec4 lightShadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors;
	};

	/** Shader buffer for on-screen lights. */
	using ScreenLightBuffer = StorageBuffer<ScreenLightFields, "Renderer3DScreenLights">;

	/** Struct of shader parameters representing the tiles of the screen. */
	struct ScreenTileParameters {
		/**
		 * Maximum number of on-screen tiles.
		 */
		static constexpr size_t MAX_TILE_COUNT = 1024;
		static_assert((MAX_TILE_COUNT * 2) % 4 == 0);

		/**
		 * Offsets and number of each item belonging to each tile in the item
		 * buffer, grouped into 4D vectors to improve packing in the std140
		 * layout. The data is packed in the following layout per tile:
		 * - 32 bits: Item offset
		 * - 8 bits: Decal count
		 * - 8 bits: Light count
		 * - 8 bits: Light probe count
		 * - 8 bits: Reflection probe count
		 */
		Array<u32vec4, (MAX_TILE_COUNT * 2) / 4> tileItemOffsetsAndCountsBy4s;
	};
	static_assert(sizeof(ScreenTileParameters) <= 16384);

	/** Shader buffer for screen tiles. */
	using ScreenTileBuffer = UniformBuffer<ScreenTileParameters, "Renderer3DScreenTiles">;

	/** Struct of shader parameters representing the depth bins of the screen. */
	struct ScreenDepthBinParameters {
		/**
		 * Maximum number of depth bins.
		 */
		static constexpr size_t MAX_DEPTH_BIN_COUNT = 1024;

		/**
		 * Depth bins, each containing the following data:
		 * - 32 bits: depthBinDecalsBegin
		 * - 32 bits: depthBinDecalsEnd
		 * - 32 bits: depthBinLightsBegin
		 * - 32 bits: depthBinLightsEnd
		 */
		Array<u32vec4, MAX_DEPTH_BIN_COUNT> depthBins;
	};
	static_assert(sizeof(ScreenDepthBinParameters) <= 16384);

	/** Shader buffer for screen depth bins. */
	using ScreenDepthBinBuffer = UniformBuffer<ScreenDepthBinParameters, "Renderer3DScreenDepthBins">;

	/** Struct of shader fields representing an item belonging to a screen tile. */
	struct ScreenItemFields {
		/** Index of the item in its corresponding buffer. */
		uint32_t itemIndex;
	};

	/** Shader buffer for screen items. */
	using ScreenItemBuffer = StorageBuffer<ScreenItemFields, "Renderer3DScreenItems">;

	/** Struct of shader parameters representing the current screen. */
	struct ScreenParameters {
		/** Viewport offset in pixels. */
		vec2 screenViewportOffset;

		/** Framebuffer height in pixels. */
		float screenFramebufferHeight;

		/** Inverse of tile size. */
		float screenInverseTileSize;

		/** Tile counts along the X/Y axes. */
		u32vec2 screenTileCounts;

		/** Depth bin count along the Z axis. */
		uint32_t screenDepthBinCount;

		/** Sampler for the depth texture array representing the cascaded 2D shadow maps of sun/directional lights. */
		sampler2DArrayShadow screenCascadedShadowMaps;

		/** Sampler for the depth texture array representing the cube shadow maps of point lights. */
		samplerCubeArrayShadow screenPointLightShadowMaps;

		/** Sampler for the depth texture array representing the 2D shadow maps of spot lights. */
		sampler2DArrayShadow screenSpotLightShadowMaps;

		/** Number of global lights at the beginning of the light buffer. */
		uint32_t screenGlobalLightCount;
	};

	/** Shader buffer for screen parameters. */
	using ScreenParameterBuffer = UniformBuffer<ScreenParameters, "Renderer3DScreenParameters">;

	/** Set of shader buffers related to the environment. */
	using EnvironmentBuffers = BufferSet<PBRParameterBuffer, Fog3D::ParameterBuffer, Sky3D::ParameterBuffer, Decals3D::ParameterBuffer, Lights3D::ParameterBuffer,
		Lights3D::ShadowMatrixBuffer, LightProbeVolumes3D::AtlasBuffer, LightProbeVolumes3D::VolumeBuffer, ReflectionProbes3D::AtlasBuffer, ReflectionProbes3D::ProbeBuffer>;

	/** Set of shader buffers related to the screen. */
	using ScreenBuffers = BufferSet<ScreenTileBuffer, ScreenDepthBinBuffer, ScreenItemBuffer, ScreenDecalBuffer, ScreenLightBuffer, ScreenParameterBuffer>;

	/** Default vertex shader type for drawing 3D models. */
	using DefaultModel3DVertexShader = Model3D::VertexShader;

	/** Default fragment shader type for drawing 3D models at full brightness without any lighting. */
	using UnlitModel3DFragmentShader = Model3D::FragmentShader;

	/** Default shader pipeline set for drawing 3D models at full brightness without any lighting. */
	using UnlitModel3DShaderPipelineSet = Model3D::ShaderPipelineSet;

	/** Default fragment shader type for drawing 3D models using PBR. */
	using PBRModel3DFragmentShader =
		Model3D::FragmentShaderBase<Model3D::VertexShaderOutputs, Model3D::FragmentShaderConstants, Model3D::FragmentShaderOutputs, EnvironmentBuffers, ScreenBuffers>;

	/** Default shader pipeline set for drawing 3D models using PBR. */
	using PBRModel3DShaderPipelineSet = Model3D::ShaderPipelineSetBase<Model3D::VertexShaderConstants, Model3D::VertexShaderOutputs, meta::TypeList<>,
		Model3D::FragmentShaderConstants, Model3D::FragmentShaderOutputs, meta::TypeList<EnvironmentBuffers, ScreenBuffers>>;

	/** Default vertex shader for drawing skyboxes. */
	using DefaultSky3DVertexShader = VertexShader<Sky3D::Mesh, Sky3D::VertexShaderConstants, Sky3D::VertexShaderOutputs, Camera3D::ParameterBuffer>;

	/** Default fragment shader for drawing skyboxes using PBR. */
	using PBRSky3DFragmentShader =
		FragmentShader<Sky3D::Mesh, Sky3D::VertexShaderOutputs, Sky3D::FragmentShaderConstants, Sky3D::FragmentShaderOutputs, Camera3D::ParameterBuffer, EnvironmentBuffers>;

	/** Struct of fields output by the shadow map fragment shader. */
	struct ShadowMapFragmentShaderOutputs {};

	/** Default fragment shader for drawing 3D models to shadow maps. */
	using ShadowMapModel3DFragmentShader = Model3D::FragmentShaderBase<Model3D::VertexShaderOutputs, Model3D::FragmentShaderConstants, ShadowMapFragmentShaderOutputs>;

	/** Shader pipeline set for drawing 3D models to shadow maps. */
	using ShadowMapModel3DShaderPipelineSet = Model3D::ShaderPipelineSetBase<Model3D::VertexShaderConstants, Model3D::VertexShaderOutputs, meta::TypeList<>,
		Model3D::FragmentShaderConstants, ShadowMapFragmentShaderOutputs, meta::TypeList<>>;

	/** Default fragment shader for drawing the distance of 3D models from the camera. */
	using DistanceModel3DFragmentShader = Model3D::FragmentShader;

	/** Shader pipeline set for drawing the distance of 3D models from the camera. */
	using DistanceModel3DShaderPipelineSet = Model3D::ShaderPipelineSet;

	/**
	 * Construct a 3D renderer.
	 *
	 * \param device device to create the renderer for. Must outlive the
	 *        renderer.
	 * \param renderer2D system providing 2D rendering capabilities. Must
	 *        outlive the renderer.
	 * \param options initial configuration of the renderer, see
	 *        Renderer3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) Renderer3D(Device& device, Renderer2D& renderer2D, const Renderer3DOptions& options = {});

	/** Destructor. */
	GREM_API(graphics_3d) ~Renderer3D();

	/** Copying a renderer is not allowed. */
	Renderer3D(const Renderer3D&) = delete;

	/** Moving a renderer is not allowed. */
	Renderer3D(Renderer3D&&) = delete;

	/** Copying a renderer is not allowed. */
	Renderer3D& operator=(const Renderer3D&) = delete;

	/** Moving a renderer is not allowed. */
	Renderer3D& operator=(Renderer3D&&) = delete;

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass, without any environment buffers provided.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 * \warning This function cannot be used with instance batches that contain
	 *          any instances that use PBR shaders.
	 */
	template <typename... ExtraBuffers>
	void drawUnlitFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const ExtraBuffers&... extraBuffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles, {});
	}

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass, without any environment buffers provided, and without sorting
	 * alpha-blended instances by their distance to the camera.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 * \warning This function cannot be used with instance batches that contain
	 *          any instances that use PBR shaders.
	 */
	template <typename... ExtraBuffers>
	void drawUnlitUnorderedFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const ExtraBuffers&... extraBuffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles, {.unordered = true});
	}

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param fog distance-based fog of the environment.
	 * \param sky sky of the environment.
	 * \param decals decals in the environment.
	 * \param lights direct light sources in the environment.
	 * \param lightProbeVolumes light probe volumes in the environment.
	 * \param reflectionProbes reflection probes in the environment.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 */
	template <typename... ExtraBuffers>
	void drawPBRFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const Fog3D& fog, Sky3DView sky, const Decals3D& decals,
		const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes, const ExtraBuffers&... extraBuffers) {
		GREM_ASSERT(sky.sky);
		flushPBRBuffers(renderPass.getFramebufferSize(), renderPass.getViewport(), camera, fog, *sky.sky, decals, lights, lightProbeVolumes, reflectionProbes);
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, 2 + sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{EnvironmentBuffers::LAYOUT_REFERENCE, environmentBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ScreenBuffers::LAYOUT_REFERENCE, screenBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles,
			{
				.skyShaderPipelineOverrideHandle = std::move(sky.shaderPipelineOverrideHandle),
				.pbr = true,
				.drawSky = !sky.filter.skipSkyRendering,
			});
		environmentBuffers.setBuffers(nullptr);
	}

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass, without sorting alpha-blended instances by their distance to the
	 * camera.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param fog distance-based fog of the environment.
	 * \param sky sky of the environment.
	 * \param decals decals in the environment.
	 * \param lights direct light sources in the environment.
	 * \param lightProbeVolumes light probe volumes in the environment.
	 * \param reflectionProbes reflection probes in the environment.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 */
	template <typename... ExtraBuffers>
	void drawPBRUnorderedFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const Fog3D& fog, Sky3DView sky,
		const Decals3D& decals, const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes,
		const ExtraBuffers&... extraBuffers) {
		GREM_ASSERT(sky.sky);
		flushPBRBuffers(renderPass.getFramebufferSize(), renderPass.getViewport(), camera, fog, *sky.sky, decals, lights, lightProbeVolumes, reflectionProbes);
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, 2 + sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{EnvironmentBuffers::LAYOUT_REFERENCE, environmentBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ScreenBuffers::LAYOUT_REFERENCE, screenBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles,
			{
				.skyShaderPipelineOverrideHandle = std::move(sky.shaderPipelineOverrideHandle),
				.pbr = true,
				.unordered = true,
				.drawSky = !sky.filter.skipSkyRendering,
			});
		environmentBuffers.setBuffers(nullptr);
	}

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass, without tonemapping when drawing the sky.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param fog distance-based fog of the environment.
	 * \param sky sky of the environment.
	 * \param decals decals in the environment.
	 * \param lights direct light sources in the environment.
	 * \param lightProbeVolumes light probe volumes in the environment.
	 * \param reflectionProbes reflection probes in the environment.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 */
	template <typename... ExtraBuffers>
	void drawHDRPBRFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const Fog3D& fog, Sky3DView sky,
		const Decals3D& decals, const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes,
		const ExtraBuffers&... extraBuffers) {
		GREM_ASSERT(sky.sky);
		flushPBRBuffers(renderPass.getFramebufferSize(), renderPass.getViewport(), camera, fog, *sky.sky, decals, lights, lightProbeVolumes, reflectionProbes);
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, 2 + sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{EnvironmentBuffers::LAYOUT_REFERENCE, environmentBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ScreenBuffers::LAYOUT_REFERENCE, screenBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles,
			{
				.skyShaderPipelineOverrideHandle = std::move(sky.shaderPipelineOverrideHandle),
				.hdr = true,
				.pbr = true,
				.drawSky = !sky.filter.skipSkyRendering,
			});
		environmentBuffers.setBuffers(nullptr);
	}

	/**
	 * Push the draw commands of a frame of 3D instance batches to a render
	 * pass, without sorting alpha-blended instances by their distance to the
	 * camera, and without tonemapping when drawing the sky.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches read-only views over the instance batches to draw.
	 * \param camera perspective to render the instances from.
	 * \param fog distance-based fog of the environment.
	 * \param sky sky of the environment.
	 * \param decals decals in the environment.
	 * \param lights direct light sources in the environment.
	 * \param lightProbeVolumes light probe volumes in the environment.
	 * \param reflectionProbes reflection probes in the environment.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batches
	 *          must be provided through the parameter pack.
	 */
	template <typename... ExtraBuffers>
	void drawHDRPBRUnorderedFrame(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera, const Fog3D& fog, Sky3DView sky,
		const Decals3D& decals, const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes,
		const ExtraBuffers&... extraBuffers) {
		GREM_ASSERT(sky.sky);
		flushPBRBuffers(renderPass.getFramebufferSize(), renderPass.getViewport(), camera, fog, *sky.sky, decals, lights, lightProbeVolumes, reflectionProbes);
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, 2 + sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{EnvironmentBuffers::LAYOUT_REFERENCE, environmentBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ScreenBuffers::LAYOUT_REFERENCE, screenBuffers.lock()},
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles,
			{
				.skyShaderPipelineOverrideHandle = std::move(sky.shaderPipelineOverrideHandle),
				.hdr = true,
				.pbr = true,
				.unordered = true,
				.drawSky = !sky.filter.skipSkyRendering,
			});
		environmentBuffers.setBuffers(nullptr);
	}

	/** \copydoc Renderer2D::getInvisibleTexture2D() */
	[[nodiscard]] const Texture& getInvisibleTexture2D() {
		return renderer2D.getInvisibleTexture2D();
	}

	/** \copydoc Renderer2D::getInvisibleTexture2DArray() */
	[[nodiscard]] const Texture& getInvisibleTexture2DArray() {
		return renderer2D.getInvisibleTexture2DArray();
	}

	/** \copydoc Renderer2D::getInvisibleTextureCube() */
	[[nodiscard]] const Texture& getInvisibleTextureCube() {
		return renderer2D.getInvisibleTextureCube();
	}

	/** \copydoc Renderer2D::getInvisibleTextureCubeArray() */
	[[nodiscard]] const Texture& getInvisibleTextureCubeArray() {
		return renderer2D.getInvisibleTextureCubeArray();
	}

	/** \copydoc Renderer2D::getWhiteTexture2D() */
	[[nodiscard]] const Texture& getWhiteTexture2D() {
		return renderer2D.getWhiteTexture2D();
	}

	/** \copydoc Renderer2D::getWhiteTextureCube() */
	[[nodiscard]] const Texture& getWhiteTextureCube() {
		return renderer2D.getWhiteTextureCube();
	}

	/** \copydoc Renderer2D::getFlatNormalTexture2D() */
	[[nodiscard]] const Texture& getFlatNormalTexture2D() {
		return renderer2D.getFlatNormalTexture2D();
	}

	/** \copydoc Renderer2D::getDefaultDepthTexture2D() */
	[[nodiscard]] const Texture& getDefaultDepthTexture2D() {
		return renderer2D.getDefaultDepthTexture2D();
	}

	/** \copydoc Renderer2D::getDefaultDepthTexture2DArray() */
	[[nodiscard]] const Texture& getDefaultDepthTexture2DArray() {
		return renderer2D.getDefaultDepthTexture2DArray();
	}

	/** \copydoc Renderer2D::getDefaultDepthTextureCube() */
	[[nodiscard]] const Texture& getDefaultDepthTextureCube() {
		return renderer2D.getDefaultDepthTextureCube();
	}

	/** \copydoc Renderer2D::getDefaultDepthTextureCubeArray() */
	[[nodiscard]] const Texture& getDefaultDepthTextureCubeArray() {
		return renderer2D.getDefaultDepthTextureCubeArray();
	}

	/** \copydoc Renderer2D::getDefaultModel2DVertexShader() */
	[[nodiscard]] const Renderer2D::DefaultModel2DVertexShader& getDefaultModel2DVertexShader() {
		return renderer2D.getDefaultModel2DVertexShader();
	}

	/** \copydoc Renderer2D::getPlainModel2DFragmentShader() */
	[[nodiscard]] const Renderer2D::PlainModel2DFragmentShader& getPlainModel2DFragmentShader() {
		return renderer2D.getPlainModel2DFragmentShader();
	}

	/** \copydoc Renderer2D::getPlainModel2DShaderPipeline() */
	[[nodiscard]] const Model2D::ShaderPipeline& getPlainModel2DShaderPipeline() {
		return renderer2D.getPlainModel2DShaderPipeline();
	}

	/** \copydoc Renderer2D::getTextModel2DFragmentShader() */
	[[nodiscard]] const Renderer2D::TextModel2DFragmentShader& getTextModel2DFragmentShader() {
		return renderer2D.getTextModel2DFragmentShader();
	}

	/** \copydoc Renderer2D::getTextModel2DShaderPipeline() */
	[[nodiscard]] const Model2D::ShaderPipeline& getTextModel2DShaderPipeline() {
		return renderer2D.getTextModel2DShaderPipeline();
	}

	/** \copydoc Renderer2D::getTonemappingModel2DFragmentShader() */
	[[nodiscard]] const Renderer2D::TonemappingModel2DFragmentShader& getTonemappingModel2DFragmentShader() {
		return renderer2D.getTonemappingModel2DFragmentShader();
	}

	/** \copydoc Renderer2D::getTonemappingModel2DShaderPipeline() */
	[[nodiscard]] const Model2D::ShaderPipeline& getTonemappingModel2DShaderPipeline() {
		return renderer2D.getTonemappingModel2DShaderPipeline();
	}

	/** \copydoc Renderer2D::getUnitSquareModel2D() */
	[[nodiscard]] const Model2D& getUnitSquareModel2D() const noexcept {
		return renderer2D.getUnitSquareModel2D();
	}

	/** \copydoc Renderer2D::getUnitRightAngledTriangleModel2D() */
	[[nodiscard]] const Model2D& getUnitRightAngledTriangleModel2D() const noexcept {
		return renderer2D.getUnitRightAngledTriangleModel2D();
	}

	/**
	 * Get the set of 3D model data buffers.
	 *
	 * \return a read-only reference to the 3D model data buffer set.
	 */
	[[nodiscard]] const Model3D::DataBuffers& getModel3DDataBuffers() {
		return model3DDataBuffers;
	}

	/**
	 * Allocate a range of matrices and upload their values to the 3D model
	 * inverse bind-pose matrix buffer.
	 *
	 * \param inverseBindPoseMatrices inverse bind-pose matrices to upload.
	 *
	 * \return the allocated range, which must be released using
	 *         releaseModel3DInverseBindPoseMatrices().
	 *
	 * \throws graphics::Error on failure to upload data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa releaseModel3DInverseBindPoseMatrices()
	 */
	[[nodiscard]] RangeAllocation<uint32_t> uploadModel3DInverseBindPoseMatrices(Span<const Model3D::InverseBindPoseMatrixFields> inverseBindPoseMatrices) {
		if (inverseBindPoseMatrices.empty()) {
			return {};
		}
		const Optional<RangeAllocation<uint32_t>> allocation = model3DInverseBindPoseMatrixRangeAllocator.allocateRange(static_cast<uint32_t>(inverseBindPoseMatrices.size()));
		if (!allocation) {
			throw std::bad_alloc{};
		}
		model3DDataBuffers.write<Model3D::InverseBindPoseMatrixBuffer>(allocation->begin, inverseBindPoseMatrices);
		[[maybe_unused]] const auto [it, inserted] = model3DInverseBindPoseMatrixRangeReferenceCounts.emplace(allocation->begin, size_t{1});
		GREM_ASSERT(inserted);
		return *allocation;
	}

	/**
	 * Increment the reference count of an allocated range of 3D model inverse
	 * bind-pose matrices.
	 *
	 * \param allocation the previously allocated range. Must be a valid range
	 *        that was previously returned from
	 *        uploadModel3DInverseBindPoseMatrices().
	 *
	 * \warning For each time this function is called,
	 *          releaseModel3DInverseBindPoseMatrices() must be called one
	 *          additional time for the given allocation.
	 *
	 * \sa releaseModel3DInverseBindPoseMatrices()
	 */
	void reacquireModel3DInverseBindPoseMatrices(RangeAllocation<uint32_t> allocation) noexcept {
		const auto it = model3DInverseBindPoseMatrixRangeReferenceCounts.find(allocation.begin);
		GREM_ASSERT(it != model3DInverseBindPoseMatrixRangeReferenceCounts.end());
		++it->second;
	}

	/**
	 * Decrement the reference count of an allocated range of 3D model inverse
	 * bind-pose matrices, and deallocate the range if the count reaches 0.
	 *
	 * \param allocation the previously allocated range. Must be a valid range
	 *        that was previously returned from
	 *        uploadModel3DInverseBindPoseMatrices(), or an empty range.
	 *
	 * \sa uploadModel3DInverseBindPoseMatrices()
	 */
	void releaseModel3DInverseBindPoseMatrices(RangeAllocation<uint32_t> allocation) noexcept {
		if (allocation.begin == allocation.end) {
			return;
		}
		const auto it = model3DInverseBindPoseMatrixRangeReferenceCounts.find(allocation.begin);
		GREM_ASSERT(it != model3DInverseBindPoseMatrixRangeReferenceCounts.end());
		if (it->second-- == 1) {
			model3DInverseBindPoseMatrixRangeAllocator.deallocateRange(allocation);
			model3DInverseBindPoseMatrixRangeReferenceCounts.erase(it);
		}
	}

	/**
	 * Allocate a range of morph targets and upload their values to the 3D model
	 * morph target value buffer.
	 *
	 * \param morphTargetValues morph target values to upload.
	 *
	 * \return the allocated range, which must be released using
	 *         releaseModel3DMorphTargetValues().
	 *
	 * \throws graphics::Error on failure to upload data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa releaseModel3DMorphTargetValues()
	 */
	[[nodiscard]] RangeAllocation<uint32_t> uploadModel3DMorphTargetValues(Span<const Model3D::MorphTargetValueFields> morphTargetValues) {
		if (morphTargetValues.empty()) {
			return {};
		}
		const Optional<RangeAllocation<uint32_t>> allocation = model3DMorphTargetValueRangeAllocator.allocateRange(static_cast<uint32_t>(morphTargetValues.size()));
		if (!allocation) {
			throw std::bad_alloc{};
		}
		model3DDataBuffers.write<Model3D::MorphTargetValueBuffer>(allocation->begin, morphTargetValues);
		[[maybe_unused]] const auto [it, inserted] = model3DMorphTargetValueRangeReferenceCounts.emplace(allocation->begin, size_t{1});
		GREM_ASSERT(inserted);
		return *allocation;
	}

	/**
	 * Increment the reference count of an allocated range of 3D model morph
	 * target values.
	 *
	 * \param allocation the previously allocated range. Must be a valid range
	 *        that was previously returned from
	 *        uploadModel3DMorphTargetValues().
	 *
	 * \warning For each time this function is called,
	 *          releaseModel3DMorphTargetValues() must be called one additional
	 *          time for the given allocation.
	 *
	 * \sa releaseModel3DMorphTargetValues()
	 */
	void reacquireModel3DMorphTargetValues(RangeAllocation<uint32_t> allocation) noexcept {
		const auto it = model3DMorphTargetValueRangeReferenceCounts.find(allocation.begin);
		GREM_ASSERT(it != model3DMorphTargetValueRangeReferenceCounts.end());
		++it->second;
	}

	/**
	 * Decrement the reference count of an allocated range of 3D model morph
	 * target values, and deallocate the range if the count reaches 0.
	 *
	 * \param allocation the previously allocated range. Must be a valid range
	 *        that was previously returned from
	 *        uploadModel3DMorphTargetValues(), or an empty range.
	 *
	 * \sa uploadModel3DMorphTargetValues()
	 */
	void releaseModel3DMorphTargetValues(RangeAllocation<uint32_t> allocation) noexcept {
		if (allocation.begin == allocation.end) {
			return;
		}
		const auto it = model3DMorphTargetValueRangeReferenceCounts.find(allocation.begin);
		GREM_ASSERT(it != model3DMorphTargetValueRangeReferenceCounts.end());
		if (it->second-- == 1) {
			model3DMorphTargetValueRangeAllocator.deallocateRange(allocation);
			model3DMorphTargetValueRangeReferenceCounts.erase(it);
		}
	}

	/**
	 * Get the default 2D-model-in-3D vertex shader.
	 *
	 * \return a read-only reference to the default 2D-model-in-3D vertex
	 *         shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const Model2DTransformed3DVertexShader& getDefault3DTransformedModel2DVertexShader();

	/**
	 * Get the default plain 2D-model-in-3D fragment shader.
	 *
	 * \return a read-only reference to the plain 2D-model-in-3D fragment
	 *         shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const Model2DTransformed3DFragmentShader& getPlain3DTransformedModel2DFragmentShader();

	/**
	 * Get the default plain 2D-model-in-3D shader pipeline.
	 *
	 * \return a read-only reference to the plain 2D-model-in-3D shader
	 *         pipeline.
	 *
	 * \throws graphics::Error on failure to create the pipeline if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] const ShaderPipeline<Model2D::Mesh>& getPlain3DTransformedModel2DShaderPipeline() {
		if (!plain3DTransformedModel2DShaderPipeline) {
			[[unlikely]];
			plain3DTransformedModel2DShaderPipeline.emplace(device, getDefault3DTransformedModel2DVertexShader(), Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				getPlain3DTransformedModel2DFragmentShader(), Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
				ShaderPipelineOptions{
					.primitiveType = PrimitiveType::TRIANGLE_STRIP,
					.faceCullingMode = FaceCullingMode::NONE,
					.blendState = BlendState::ALPHA_BLENDING_PREMULTIPLIED,
				});
		}
		return *plain3DTransformedModel2DShaderPipeline;
	}

	/**
	 * Get the default plain 3D text fragment shader.
	 *
	 * \return a read-only reference to the plain 3D text fragment shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const Model2DTransformed3DFragmentShader& getPlain3DTransformedTextFragmentShader();

	/**
	 * Get the default plain 3D text shader pipeline.
	 *
	 * \return a read-only reference to the plain 3D text shader pipeline.
	 *
	 * \throws graphics::Error on failure to create the pipeline if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] const ShaderPipeline<Model2D::Mesh>& getPlain3DTransformedTextShaderPipeline() {
		if (!plain3DTransformedTextShaderPipeline) {
			[[unlikely]];
			plain3DTransformedTextShaderPipeline.emplace(device, getDefault3DTransformedModel2DVertexShader(), Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				getPlain3DTransformedTextFragmentShader(), Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
				ShaderPipelineOptions{
					.primitiveType = PrimitiveType::TRIANGLE_STRIP,
					.faceCullingMode = FaceCullingMode::NONE,
					.blendState = BlendState::ALPHA_BLENDING_PREMULTIPLIED,
				});
		}
		return *plain3DTransformedTextShaderPipeline;
	}

	/**
	 * Get the default 3D model vertex shader.
	 *
	 * \return a read-only reference to the default 3D model vertex shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const DefaultModel3DVertexShader& getDefaultModel3DVertexShader();

	/**
	 * Get the default unlit 3D model fragment shader, which renders models'
	 * base color at full brightness without any lighting.
	 *
	 * \return a read-only reference to the unlit 3D model fragment shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const UnlitModel3DFragmentShader& getUnlitModel3DFragmentShader();

	/**
	 * Get the default unlit 3D model shader pipeline set, which renders models'
	 * base color at full brightness without any lighting.
	 *
	 * \return a reference to the unlit 3D model shader pipeline set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] UnlitModel3DShaderPipelineSet& getUnlitModel3DShaderPipelineSet() {
		if (!unlitModel3DShaderPipelineSet) {
			[[unlikely]];
			unlitModel3DShaderPipelineSet.emplace(device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getUnlitModel3DFragmentShader(),
				Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *unlitModel3DShaderPipelineSet;
	}

	/**
	 * Get the default non-tonemapped unlit 3D model shader pipeline set,
	 * intended for high-dynamic-range render targets, which renders models'
	 * base color at full brightness without any lighting.
	 *
	 * \return a reference to the non-tonemapped unlit 3D model shader pipeline
	 *         set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] UnlitModel3DShaderPipelineSet& getHDRUnlitModel3DShaderPipelineSet() {
		if (!hdrUnlitModel3DShaderPipelineSet) {
			[[unlikely]];
			hdrUnlitModel3DShaderPipelineSet.emplace(
				device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getUnlitModel3DFragmentShader(),
				[](const Model3D::ShaderConfiguration& shaderConfiguration) -> Model3D::FragmentShaderConstants {
					Model3D::FragmentShaderConstants result = Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS(shaderConfiguration);
					result.FRAGMENT_HDR = true;
					return result;
				},
				Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *hdrUnlitModel3DShaderPipelineSet;
	}

	/**
	 * Get the default wireframe 3D model shader pipeline set, which renders the
	 * wireframe of models in their base color at full brightness without any
	 * lighting.
	 *
	 * \return a reference to the wireframe 3D model shader pipeline set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] UnlitModel3DShaderPipelineSet& getWireframeModel3DShaderPipelineSet() {
		if (!wireframeModel3DShaderPipelineSet) {
			[[unlikely]];
			wireframeModel3DShaderPipelineSet.emplace(device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getUnlitModel3DFragmentShader(),
				Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> ShaderPipelineOptions {
					ShaderPipelineOptions result = Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS(shaderConfiguration);
					result.polygonMode = PolygonMode::LINE;
					result.faceCullingMode = FaceCullingMode::NONE;
					return result;
				});
		}
		return *wireframeModel3DShaderPipelineSet;
	}

	/**
	 * Get the default non-tonemapped wireframe 3D model shader pipeline set,
	 * intended for high-dynamic-range render targets, which renders the
	 * wireframe of models in their base color at full brightness without any
	 * lighting.
	 *
	 * \return a reference to the non-tonemapped wireframe 3D model shader
	 *         pipeline set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] UnlitModel3DShaderPipelineSet& getHDRWireframeModel3DShaderPipelineSet() {
		if (!hdrWireframeModel3DShaderPipelineSet) {
			[[unlikely]];
			hdrWireframeModel3DShaderPipelineSet.emplace(
				device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getUnlitModel3DFragmentShader(),
				[](const Model3D::ShaderConfiguration& shaderConfiguration) -> Model3D::FragmentShaderConstants {
					Model3D::FragmentShaderConstants result = Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS(shaderConfiguration);
					result.FRAGMENT_HDR = true;
					return result;
				},
				[&](const Model3D::ShaderConfiguration& shaderConfiguration) -> ShaderPipelineOptions {
					ShaderPipelineOptions result = Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS(shaderConfiguration);
					result.polygonMode = PolygonMode::LINE;
					result.faceCullingMode = FaceCullingMode::NONE;
					return result;
				});
		}
		return *hdrWireframeModel3DShaderPipelineSet;
	}

	/**
	 * Get the default PBR 3D model fragment shader, which renders models using
	 * physically based rendering principles given the lighting from the
	 * environment.
	 *
	 * \return a read-only reference to the PBR 3D model fragment shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const PBRModel3DFragmentShader& getPBRModel3DFragmentShader();

	/**
	 * Get the default PBR 3D model fragment shader selector, which renders
	 * models using physically based rendering principles given the lighting
	 * from the environment, unless the shader configuration's material type is
	 * specified as unlit.
	 *
	 * \return a read-only reference to the PBR 3D model fragment shader
	 *         selector.
	 */
	[[nodiscard]] PBRModel3DShaderPipelineSet::FragmentShaderSelector getPBRModel3DFragmentShaderSelector() {
		return [this](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<FragmentShaderImplementation> {
			switch (shaderConfiguration.materialType) {
				case Model3D::MaterialType::METALLIC_ROUGHNESS: break;
				case Model3D::MaterialType::UNLIT: return getUnlitModel3DFragmentShader().lock();
			}
			return getPBRModel3DFragmentShader().lock();
		};
	}

	/**
	 * Get the default PBR 3D model shader pipeline set, which renders models
	 * using physically based rendering principles given the lighting from the
	 * environment.
	 *
	 * \return a reference to the PBR 3D model shader pipeline set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] PBRModel3DShaderPipelineSet& getPBRModel3DShaderPipelineSet() {
		if (!pbrModel3DShaderPipelineSet) {
			[[unlikely]];
			pbrModel3DShaderPipelineSet.emplace(device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getPBRModel3DFragmentShaderSelector(),
				Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *pbrModel3DShaderPipelineSet;
	}

	/**
	 * Get the default non-tonemapped PBR 3D model shader pipeline set, intended
	 * for high-dynamic-range render targets, which renders models using
	 * physically based rendering principles given the lighting from the
	 * environment.
	 *
	 * \return a reference to the non-tonemapped PBR 3D model shader pipeline
	 *         set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] PBRModel3DShaderPipelineSet& getHDRPBRModel3DShaderPipelineSet() {
		if (!hdrPBRModel3DShaderPipelineSet) {
			[[unlikely]];
			hdrPBRModel3DShaderPipelineSet.emplace(
				device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getPBRModel3DFragmentShaderSelector(),
				[](const Model3D::ShaderConfiguration& shaderConfiguration) -> Model3D::FragmentShaderConstants {
					Model3D::FragmentShaderConstants result = Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS(shaderConfiguration);
					result.FRAGMENT_HDR = true;
					return result;
				},
				Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *hdrPBRModel3DShaderPipelineSet;
	}

	/**
	 * Get the default shadow map 3D model shader pipeline set, which renders
	 * models to the depth buffer, without outputting any color.
	 *
	 * \return a reference to the shadow map 3D model shader pipeline set.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) ShadowMapModel3DShaderPipelineSet& getShadowMapModel3DShaderPipelineSet();

	/**
	 * Get the default distance 3D model shader pipeline set, which renders
	 * models' distance from the camera to the red channel of the output color,
	 * with backface culling disabled.
	 *
	 * \return a reference to the distance 3D model shader pipeline set.
	 *
	 * \warning The distance fragment shader requires a DistanceParameters
	 *          buffer to be provided when drawn.
	 *
	 * \throws graphics::Error on failure to create the pipeline set if it
	 *         doesn't already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) DistanceModel3DShaderPipelineSet& getDistanceModel3DShaderPipelineSet();

	/**
	 * Get the default skybox vertex shader.
	 *
	 * \return a read-only reference to the skybox vertex shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const DefaultSky3DVertexShader& getDefaultSky3DVertexShader();

	/**
	 * Get the default PBR skybox fragment shader.
	 *
	 * \return a read-only reference to the PBR skybox fragment shader.
	 *
	 * \throws graphics::Error on failure to create the shader if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) const PBRSky3DFragmentShader& getPBRSky3DFragmentShader();

	/**
	 * Get the default PBR skybox shader pipeline.
	 *
	 * \return a reference to the PBR skybox shader pipeline.
	 *
	 * \throws graphics::Error on failure to create the pipeline if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] const Sky3D::ShaderPipeline& getPBRSky3DShaderPipeline() {
		if (!pbrSky3DShaderPipeline) {
			[[unlikely]];
			pbrSky3DShaderPipeline.emplace(device, getDefaultSky3DVertexShader(), Sky3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getPBRSky3DFragmentShader(),
				Sky3D::FragmentShaderConstants{.SKY_HDR = false}, Sky3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *pbrSky3DShaderPipeline;
	}

	/**
	 * Get the default non-tonemapped PBR skybox shader pipeline.
	 *
	 * \return a reference to the non-tonemapped PBR skybox shader pipeline.
	 *
	 * \throws graphics::Error on failure to create the pipeline if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] const Sky3D::ShaderPipeline& getHDRPBRSky3DShaderPipeline() {
		if (!hdrPBRSky3DShaderPipeline) {
			[[unlikely]];
			hdrPBRSky3DShaderPipeline.emplace(device, getDefaultSky3DVertexShader(), Sky3D::DEFAULT_VERTEX_SHADER_CONSTANTS, getPBRSky3DFragmentShader(),
				Sky3D::FragmentShaderConstants{.SKY_HDR = true}, Sky3D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *hdrPBRSky3DShaderPipeline;
	}

	/**
	 * Get the default 3D cube model.
	 *
	 * \return a read-only reference to the 3D cube model.
	 */
	[[nodiscard]] const Model3D& getCubeModel3D() const noexcept {
		return cubeModel3D;
	}

	/**
	 * Get the default 3D cubemap.
	 *
	 * \return a read-only reference to the 3D cubemap.
	 */
	[[nodiscard]] const Cubemap3D& getCubemap3D() const noexcept {
		return cubemap3D;
	}

	/**
	 * Get the default BRDF integration map for split-sum approximation of
	 * ambient specular reflections.
	 *
	 * \return a read-only reference to the specular split-sum BRDF integration
	 *         map.
	 *
	 * \throws graphics::Error on failure to create the texture if it doesn't
	 *         already exist.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] const Texture& getSpecularSplitSumBRDFIntegrationMap() {
		if (!specularSplitSumBRDFIntegrationMap) {
			specularSplitSumBRDFIntegrationMap = generateSpecularSplitSumBRDFIntegrationMap(device, renderer2D, options.specularSplitSumBRDFIntegrationMapResolution,
				options.specularSplitSumBRDFIntegrationMapSampleCount);
		}
		return specularSplitSumBRDFIntegrationMap;
	}

	/**
	 * Render the shadow map of a specific shadow mapped light source in a set
	 * of lights with respect to a future observer.
	 *
	 * \param lights set of lights containing the light to render the shadow map
	 *        of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerProjectionMatrix projection matrix of the camera that will
	 *        later be observing the rendered shadows.
	 * \param observerViewMatrix view matrix of the camera that will later be
	 *        observing the rendered shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by the light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, or not shadow mapped, this
	 *       function has no effect.
	 */
	GREM_API(graphics_3d)
	void renderShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix, const mat4& observerViewMatrix,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Render the shadow map of a specific shadow mapped light source in a set
	 * of lights with respect to a future observer.
	 *
	 * \param lights set of lights containing the light to render the shadow map
	 *        of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerCamera camera that will later be observing the rendered
	 *        shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by the light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, or not shadow mapped, this
	 *       function has no effect.
	 */
	void renderShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const Camera3D& observerCamera,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
		renderShadowMap(lights, lightID, totalShadowCasterBoundingBox, observerCamera.getProjectionMatrix(), observerCamera.getViewMatrix(), drawShadowCasters);
	}

	/**
	 * Render the shadow maps of shadow mapped light sources in a set of lights
	 * with respect to a future observer.
	 *
	 * \param lights set of lights to render the shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerProjectionMatrix projection matrix of the camera that will
	 *        later be observing the rendered shadows.
	 * \param observerViewMatrix view matrix of the camera that will later be
	 *        observing the rendered shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void renderAllShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix, const mat4& observerViewMatrix,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Render the shadow maps of shadow mapped light sources in a set of lights
	 * with respect to a future observer.
	 *
	 * \param lights set of lights to render the shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerCamera camera that will later be observing the rendered
	 *        shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void renderAllShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const Camera3D& observerCamera,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
		renderAllShadowMaps(lights, totalShadowCasterBoundingBox, observerCamera.getProjectionMatrix(), observerCamera.getViewMatrix(), drawShadowCasters);
	}

	/**
	 * Unconditionally render the shadow map of a specific shadow mapped view-
	 * independent light source in a set of lights.
	 *
	 * \param lights set of lights containing the light to render the shadow map
	 *        of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by the light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, not shadow mapped, or has a
	 *       view-dependent shadow map type, this function has no effect.
	 */
	GREM_API(graphics_3d) void renderViewIndependentShadowMap(Lights3D& lights, LightID lightID, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Unconditionally render the shadow maps of all shadow mapped
	 * view-independent light sources in a set of lights.
	 *
	 * \param lights set of lights to render the view-independent shadow maps
	 *        of.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void renderAllViewIndependentShadowMaps(Lights3D& lights, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Unconditionally render the shadow map of a specific shadow mapped view-
	 * dependent light source in a set of lights with respect to a future
	 * observer.
	 *
	 * \param lights set of lights containing the light to render the shadow map
	 *        of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerProjectionMatrix projection matrix of the camera that will
	 *        later be observing the rendered shadows.
	 * \param observerViewMatrix view matrix of the camera that will later be
	 *        observing the rendered shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, not shadow mapped, or has a
	 *       view-independent shadow map type, this function has no effect.
	 */
	GREM_API(graphics_3d)
	void renderViewDependentShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix,
		const mat4& observerViewMatrix, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Unconditionally render the shadow map of a specific shadow mapped view-
	 * dependent light source in a set of lights with respect to a future
	 * observer.
	 *
	 * \param lights set of lights containing the light to render the shadow map
	 *        of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerCamera camera that will later be observing the rendered
	 *        shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, not shadow mapped, or has a
	 *       view-independent shadow map type, this function has no effect.
	 */
	void renderViewDependentShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const Camera3D& observerCamera,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
		renderViewDependentShadowMap(lights, lightID, totalShadowCasterBoundingBox, observerCamera.getProjectionMatrix(), observerCamera.getViewMatrix(), drawShadowCasters);
	}

	/**
	 * Render the shadow maps of all shadow mapped view-dependent light sources
	 * in a set of lights with respect to a future observer.
	 *
	 * \param lights set of lights to render the cascaded shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerProjectionMatrix projection matrix of the camera that will
	 *        later be observing the rendered shadows.
	 * \param observerViewMatrix view matrix of the camera that will later be
	 *        observing the rendered shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void renderAllViewDependentShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix, const mat4& observerViewMatrix,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Render the shadow maps of shadow mapped view-dependent light sources in a
	 * set of lights with respect to a future observer.
	 *
	 * \param lights set of lights to render the view-dependent shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param observerCamera camera that will later be observing the rendered
	 *        shadows.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void renderAllViewDependentShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const Camera3D& observerCamera,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
		renderAllViewDependentShadowMaps(lights, totalShadowCasterBoundingBox, observerCamera.getProjectionMatrix(), observerCamera.getViewMatrix(), drawShadowCasters);
	}

	/**
	 * Unconditionally render the shadow map of a specific shadow mapped view-
	 * dependent light source in a set of lights to cover the entire scene at
	 * the same quality for all cascade levels.
	 *
	 * \param lights set of lights to render the shadow maps of.
	 * \param lightID handle to the light whose shadow map to render.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the given light handle is invalid, not shadow mapped, or has a
	 *       view-independent shadow map type, this function has no effect.
	 */
	GREM_API(graphics_3d)
	void renderViewDependentShadowMapForFullZoomedOutView(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Unconditionally render the shadow maps of all shadow mapped view-
	 * dependent light sources in a set of lights to cover the entire scene at
	 * the same quality for all cascade levels.
	 *
	 * \param lights set of lights to render the shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void renderAllViewDependentShadowMapsForFullZoomedOutView(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

	/**
	 * Unconditionally render the shadow maps of all shadow mapped light sources
	 * in a set of lights, such that any view-dependent shadow maps cover the
	 * entire scene at the same quality for all cascade levels.
	 *
	 * \param lights set of lights to render the shadow maps of.
	 * \param totalShadowCasterBoundingBox axis-aligned bounding box containing
	 *        all shadow casters in the scene.
	 * \param drawShadowCasters function that records the draw commands for the
	 *        shadow casters seen by each light. Should draw only depth from the
	 *        given camera's perspective onto the given render pass.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void renderAllShadowMapsForFullZoomedOutView(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox,
		FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters);

private:
	friend Instances3D;
	friend Bootstrapper;

	struct SpecularSplitSumBRDFIntegrationMapFragmentShaderConstants {
		uint32_t SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_SAMPLE_COUNT;
	};

	using SpecularSplitSumBRDFIntegrationMapFragmentShader =
		Model2D::FragmentShaderBase<Model2D::VertexShaderOutputs, SpecularSplitSumBRDFIntegrationMapFragmentShaderConstants, Model2D::FragmentShaderOutputs>;

	struct FrameOptions {
		SharedPointer<ShaderPipelineImplementation> skyShaderPipelineOverrideHandle{};
		bool hdr = false;
		bool pbr = false;
		bool unordered = false;
		bool drawSky = false;
	};

	struct TransparentDrawCommandReference {
		bool is3D;
		uint16_t instanceBatchIndex;
		uint32_t drawCommandIndex;
	};

	GREM_API(graphics_3d) static const Array<vec3, 8> CUBE_MODEL_3D_VERTEX_POSITIONS;
	GREM_API(graphics_3d) static const Array<uint32_t, 36> CUBE_MODEL_3D_INDICES;

	[[nodiscard]] GREM_API(graphics_3d) static Texture
		generateSpecularSplitSumBRDFIntegrationMap(Device& device, Renderer2D& renderer2D, uint32_t resolution, uint32_t sampleCount);

	template <typename... Buffers>
	void setTemporaryCombinedBufferHandles(Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles, const Buffers&... buffers) {
		temporaryCombinedBufferHandles.clear();
		(temporaryCombinedBufferHandles.push_back(Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}), ...);
		for (const Pair<BufferLayoutReference, SharedPointer<void>>& extraBufferHandle : extraBufferHandles) {
			temporaryCombinedBufferHandles.push_back(extraBufferHandle);
		}
		sortByAscending<&Pair<BufferLayoutReference, SharedPointer<void>>::first>(temporaryCombinedBufferHandles);
	}

	GREM_API(graphics_3d)
	void drawFrameImplementation(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera,
		Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles, FrameOptions frameOptions);

	GREM_API(graphics_3d)
	void flushPBRBuffers(Extent2D framebufferSize, const Viewport& viewport, const Camera3D& camera, const Fog3D& fog, const Sky3D& sky, const Decals3D& decals,
		const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes);

	Device& device;
	Renderer2D& renderer2D;
	Renderer3DOptions options;
	Model3D::DataBuffers model3DDataBuffers{device};
	RangeAllocator<uint32_t> model3DInverseBindPoseMatrixRangeAllocator{};
	HashMap<uint32_t, size_t> model3DInverseBindPoseMatrixRangeReferenceCounts{};
	RangeAllocator<uint32_t> model3DMorphTargetValueRangeAllocator{};
	HashMap<uint32_t, size_t> model3DMorphTargetValueRangeReferenceCounts{};
	Optional<Model2DTransformed3DVertexShader> default3DTransformedModel2DVertexShader{};
	Optional<Model2DTransformed3DFragmentShader> plain3DTransformedModel2DFragmentShader{};
	Optional<ShaderPipeline<Model2D::Mesh>> plain3DTransformedModel2DShaderPipeline{};
	Optional<Model2DTransformed3DFragmentShader> raw3DTransformedModel2DFragmentShader{};
	Optional<ShaderPipeline<Model2D::Mesh>> raw3DTransformedModel2DShaderPipeline{};
	Optional<Model2DTransformed3DFragmentShader> plain3DTransformedTextFragmentShader{};
	Optional<ShaderPipeline<Model2D::Mesh>> plain3DTransformedTextShaderPipeline{};
	Optional<Model2DTransformed3DFragmentShader> raw3DTransformedTextFragmentShader{};
	Optional<ShaderPipeline<Model2D::Mesh>> raw3DTransformedTextShaderPipeline{};
	Optional<DefaultModel3DVertexShader> defaultModel3DVertexShader{};
	Optional<UnlitModel3DFragmentShader> unlitModel3DFragmentShader{};
	Optional<UnlitModel3DShaderPipelineSet> unlitModel3DShaderPipelineSet{};
	Optional<UnlitModel3DShaderPipelineSet> hdrUnlitModel3DShaderPipelineSet{};
	Optional<UnlitModel3DShaderPipelineSet> wireframeModel3DShaderPipelineSet{};
	Optional<UnlitModel3DShaderPipelineSet> hdrWireframeModel3DShaderPipelineSet{};
	Optional<PBRModel3DFragmentShader> pbrModel3DFragmentShader{};
	Optional<PBRModel3DShaderPipelineSet> pbrModel3DShaderPipelineSet{};
	Optional<PBRModel3DShaderPipelineSet> hdrPBRModel3DShaderPipelineSet{};
	Optional<ShadowMapModel3DShaderPipelineSet> shadowMapModel3DShaderPipelineSet{};
	Optional<DistanceModel3DShaderPipelineSet> distanceModel3DShaderPipelineSet{};
	Optional<DefaultSky3DVertexShader> defaultSky3DVertexShader{};
	Optional<PBRSky3DFragmentShader> pbrSky3DFragmentShader{};
	Optional<Sky3D::ShaderPipeline> pbrSky3DShaderPipeline{};
	Optional<Sky3D::ShaderPipeline> hdrPBRSky3DShaderPipeline{};
	Model3D cubeModel3D{device, *this, resource::Model{CUBE_MODEL_3D_VERTEX_POSITIONS, CUBE_MODEL_3D_INDICES}};
	Cubemap3D cubemap3D{device};
	Texture specularSplitSumBRDFIntegrationMap{};
	Text2D temporaryText{};
	resource::Model::Transformation temporaryModelTransformation{};
	Camera3D shadowMapCamera3D{device};
	ArrayList<Pair<BufferLayoutReference, SharedPointer<void>>> temporaryCombinedBufferHandles{};
	Model2D::TextureBuffer textureBuffer2D{device};
	Model2D::Transformation3DBuffer transformation3DBuffer2D{device};
	DrawCommandBuffer<Model3D::Mesh> transparentDrawCommandBuffer3D{device};
	DrawCommandBuffer<Model2D::Mesh> transparentDrawCommandBuffer2D{device};
	Buffer<TransparentDrawCommandReference> combined2DAnd3DTransparentDrawCommands{};
	Optional<DrawCommandBuffer<Cubemap3D::Mesh>> skyDrawCommandBuffer{};
	Optional<DrawCommandBuffer<Cubemap3D::Mesh>> hdrSkyDrawCommandBuffer{};
	PBRParameterBuffer pbrParameterBuffer{device};
	EnvironmentBuffers environmentBuffers{device, nullptr};
	ScreenBuffers screenBuffers{device};
	Arena<0> flushPBRBuffersArena{147456};
	bool pbrParameterBufferDirty = true;
};

} // namespace grem::graphics

#endif
