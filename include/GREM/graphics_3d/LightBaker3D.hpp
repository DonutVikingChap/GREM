// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_LIGHT_BAKER_3D_HPP
#define GREM_GRAPHICS_3D_LIGHT_BAKER_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>

namespace grem::graphics {

class Device;              // Forward declaration, to avoid including Device.hpp.
class RenderPass;          // Forward declaration, to avoid including RenderPass.hpp.
class Camera3D;            // Forward declaration, to avoid including Camera3D.hpp.
class LightProbeVolumes3D; // Forward declaration, to avoid a circular include of LightProbeVolumes3D.hpp.
class ReflectionProbes3D;  // Forward declaration, to avoid a circular include of ReflectionProbes3D.hpp.
class Bootstrapper;        // Forward declaration.

/**
 * Configuration options for a LightBaker3D.
 */
struct LightBaker3DOptions {
	/**
	 * Number of samples to use when baking the irradiance cubemap of skyboxes.
	 *
	 * Must be positive.
	 */
	uint32_t skyIrradianceSampleCount = 4096;

	/**
	 * Number of samples to use when baking the reflection cubemap of skyboxes.
	 *
	 * Must be positive.
	 */
	uint32_t skyReflectionSampleCount = 4096;

	/**
	 * Width, in texels, of the source cubemaps used to render the irradiance
	 * and distance maps of light probes.
	 *
	 * Must be a power of 2.
	 */
	uint32_t lightProbeRenderResolution = 256;

	/**
	 * Distance to the near plane of light probes.
	 *
	 * Must be less than #lightProbeFarZ.
	 */
	float lightProbeNearZ = 0.001f;

	/**
	 * Distance to the far plane of light probes.
	 *
	 * Must be greater than #lightProbeNearZ.
	 */
	float lightProbeFarZ = 100.0f;

	/**
	 * Camera exposure to use when baking the irradiance maps of light probes.
	 */
	float lightProbeExposure = 1.0f;

	/**
	 * Number of samples to use when baking the irradiance maps of light probes.
	 *
	 * Must be positive.
	 */
	uint32_t lightProbeIrradianceSampleCount = 1024;

	/**
	 * Number of samples to use when baking the distance maps of light probes.
	 *
	 * Must be positive.
	 */
	uint32_t lightProbeDistanceSampleCount = 2048;

	/**
	 * Exponent of the cosine weight between normal and sample direction to use
	 * when baking the distance maps of light probes. Must be greater than or
	 * equal to 1.
	 */
	float lightProbeDistanceSharpness = 50.0f;

	/**
	 * Width, in texels, of the source cubemaps used to render the reflection
	 * maps of reflection probes.
	 *
	 * Must be a power of 2.
	 */
	uint32_t reflectionProbeRenderResolution = 1024;

	/**
	 * Distance to the near plane of reflection probes.
	 *
	 * Must be less than #reflectionProbeFarZ.
	 */
	float reflectionProbeNearZ = 0.01f;

	/**
	 * Distance to the far plane of reflection probes.
	 *
	 * Must be greater than #reflectionProbeNearZ.
	 */
	float reflectionProbeFarZ = 1000.0f;

	/**
	 * Camera exposure to use when baking the reflection maps of reflection
	 * probes.
	 */
	float reflectionProbeExposure = 1.0f;

	/**
	 * Number of samples to use when baking the reflection maps of reflection
	 * probes.
	 *
	 * Must be positive.
	 */
	uint32_t reflectionProbeReflectionSampleCount = 4096;
};

/**
 * System for pre-baking global illumination.
 */
class LightBaker3D {
public:
	/**
	 * Construct a light baker.
	 *
	 * \param device device to create the baker for. Must outlive the baker.
	 * \param renderer3D system providing 3D rendering capabilities. Must
	 *        outlive the baker.
	 * \param options baking options, see LightBaker3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) LightBaker3D(Device& device, Renderer3D& renderer3D, const LightBaker3DOptions& options = {});

	/**
	 * Render a sky map texture onto a sky.
	 *
	 * \param sky sky to render onto.
	 * \param newSkyTexture new sky texture to set. Must be a cube texture, an
	 *        equirectangular 2D texture or an empty texture without a value.
	 * \param newSkyOptions sky options, see SkyOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void bakeSkybox(Sky3D& sky, Texture newSkyTexture, const Sky3DOptions& newSkyOptions);

	/**
	 * Render a sky map image onto a sky.
	 *
	 * \param sky sky to render onto.
	 * \param newSkyImage new sky map image to set. Must be a cube image, an
	 *        equirectangular 2D image or an empty image without a value.
	 * \param newSkyOptions sky options, see SkyOptions.
	 * \param newSkyImageUploadOptions options for the new sky map image to set,
	 *        see TextureImageUploadOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void bakeSkybox(Sky3D& sky, const resource::ImageView& newSkyImage, const Sky3DOptions& newSkyOptions,
		const TextureImageUploadOptions& newSkyImageUploadOptions = {.convertToPremultipliedAlpha = false, .generateMipmap = false});

	/**
	 * Render the irradiance and distance maps of a set of light probe volumes.
	 *
	 * \param lightProbeVolumes set of light probe volumes to bake.
	 * \param drawRadiance function that records the draw commands
	 *        for the radiance of the surroundings of the light probes. Should
	 *        draw an image from the given camera's perspective in HDR color
	 *        without tonemapping onto the given render pass. The alpha channel
	 *        of the drawn image is ignored.
	 * \param drawDistance function that records the draw commands for the
	 *        distance of the surroundings of the light probe. Should draw an
	 *        image from the given camera's perspective with distance in the red
	 *        channel onto the given render pass, with backface culling
	 *        disabled.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void bakeDiffuseLighting(LightProbeVolumes3D& lightProbeVolumes, FunctionView<void(RenderPass&, const Camera3D&)> drawRadiance,
		FunctionView<void(RenderPass&, const Camera3D&)> drawDistance);

	/**
	 * Render the irradiance map of a single light probe of a specific volume in
	 * a set of light probe volumes.
	 *
	 * \param lightProbeVolumes set of light probe volumes containing the light
	 *        probe to bake.
	 * \param lightProbeVolumeIndex index of the light probe volume in the set
	 *        of light probe volumes. Must be less than
	 *        `lightProbeVolumes.getVolumeOptions().size()`.
	 * \param lightProbeGridIndices local grid coordinates of the light probe in
	 *        the specified light probe volume. Must be in range of the
	 *        specified volume's LightProbeVolumeOptions3D::probeCounts.
	 * \param drawRadiance function that records the draw commands for the
	 *        radiance of the surroundings of the light probe. Should draw an
	 *        image from the given camera's perspective in HDR color without
	 *        tonemapping onto the given render pass. The alpha channel of the
	 *        drawn image is ignored.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void bakeLightProbeIrradianceMap(LightProbeVolumes3D& lightProbeVolumes, uint32_t lightProbeVolumeIndex, u32vec3 lightProbeGridIndices,
		FunctionView<void(RenderPass&, const Camera3D&)> drawRadiance);

	/**
	 * Render the distance map of a single light probe of a specific volume in a
	 * set of light probe volumes.
	 *
	 * \param lightProbeVolumes set of light probe volumes containing the light
	 *        probe to bake.
	 * \param lightProbeVolumeIndex index of the light probe volume in the set
	 *        of light probe volumes. Must be less than
	 *        `lightProbeVolumes.getVolumeOptions().size()`.
	 * \param lightProbeGridIndices local grid coordinates of the light probe in
	 *        the specified light probe volume. Must be in range of the
	 *        specified volume's LightProbeVolumeOptions3D::probeCounts.
	 * \param drawDistance function that records the draw commands for the
	 *        distance of the surroundings of the light probe. Should draw an
	 *        image from the given camera's perspective with distance in the red
	 *        channel onto the given render pass, with backface culling
	 *        disabled.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void bakeLightProbeDistanceMap(LightProbeVolumes3D& lightProbeVolumes, uint32_t lightProbeVolumeIndex, u32vec3 lightProbeGridIndices,
		FunctionView<void(RenderPass&, const Camera3D&)> drawDistance);

	/**
	 * Render the reflection maps of a set of reflection probes.
	 *
	 * \param reflectionProbes set of reflection probes to bake.
	 * \param drawReflection function that records the draw commands for the
	 *        surroundings of the reflection probes. Should draw an image from
	 *        the given camera's perspective in HDR color without tonemapping
	 *        onto the given render pass. The depth buffer of the drawn image is
	 *        used to determine which pixels are part of the reflection probe's
	 *        purview by comparing them to the maximum depth value.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void bakeSpecularReflections(ReflectionProbes3D& reflectionProbes, FunctionView<void(RenderPass&, const Camera3D&)> drawReflection);

	/**
	 * Render the reflection map of a single reflection probe in a set of
	 * reflection probes.
	 *
	 * \param reflectionProbes set of reflection probes containing the
	 *        reflection probe to bake.
	 * \param reflectionProbeIndex index of the reflection probe in the set of
	 *        reflection probes. Must be less than
	 *        `reflectionProbes.getProbeOptions().size()`.
	 * \param drawReflection function that records the draw commands for the
	 *        surroundings of the reflection probe. Should draw an image from
	 *        the given camera's perspective in HDR color without tonemapping
	 *        onto the given render pass. The depth buffer of the drawn image is
	 *        used to determine which pixels are part of the reflection probe's
	 *        purview by comparing them to the maximum depth value.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void bakeReflectionProbeReflectionMap(ReflectionProbes3D& reflectionProbes, uint32_t reflectionProbeIndex, FunctionView<void(RenderPass&, const Camera3D&)> drawReflection);

	/**
	 * Generate a cubemap from a 2D equirectangular map texture.
	 *
	 * \param internalFormat internal format of the generated cubemap texture.
	 *        Must be a color format.
	 * \param resolution width of each side of the generated cubemap texture.
	 * \param equirectangularMap 2D equirectangular map to generate the cubemap
	 *        from.
	 * \param samplerOptions sampler options of the generated cubemap.
	 *
	 * \return the generated cubemap.
	 *
	 * \throws graphics::Error if resource creation failed, or on failure to
	 *         render the cubemap.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) Texture generateCubemapFromEquirectangularMap(TextureFormat internalFormat, uint32_t resolution, const Texture& equirectangularMap,
		Optional<TextureSamplerOptions> samplerOptions = TextureSamplerOptions{
			.minificationFilter = TextureFilter::LINEAR,
			.magnificationFilter = TextureFilter::LINEAR,
			.mipmapMode = TextureMipmapMode::NONE,
			.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.maxAnisotropy = 1.0f,
		});

private:
	friend Bootstrapper;

	//==========================================================================

	struct CubemapIrradianceFragmentShaderConstants {
		uint32_t IRRADIANCE_SAMPLE_COUNT;
	};

	struct CubemapIrradianceFragmentParameters {
		samplerCube radianceCubemapTexture;
	};

	using CubemapIrradianceFragmentParameterBuffer = UniformBuffer<CubemapIrradianceFragmentParameters, "LightBaker3DCubemapIrradianceFragmentParameters">;

	using CubemapIrradianceFragmentShader = graphics::FragmentShader<Cubemap3D::Mesh, Cubemap3D::VertexShaderOutputs, CubemapIrradianceFragmentShaderConstants,
		Cubemap3D::FragmentShaderOutputs, Cubemap3D::PerspectiveBuffer, CubemapIrradianceFragmentParameterBuffer>;

	//==========================================================================

	struct CubemapReflectionFragmentShaderConstants {
		uint32_t REFLECTION_SAMPLE_COUNT;
	};

	struct CubemapReflectionFragmentParameters {
		samplerCube radianceCubemapTexture;
		samplerCubeShadow depthCubemapTexture;
		float reflectionResolution;
		float reflectionRoughness;
	};

	using CubemapReflectionFragmentParameterBuffer = UniformBuffer<CubemapReflectionFragmentParameters, "LightBaker3DCubemapReflectionFragmentParameters">;

	using CubemapReflectionFragmentShader = graphics::FragmentShader<Cubemap3D::Mesh, Cubemap3D::VertexShaderOutputs, CubemapReflectionFragmentShaderConstants,
		Cubemap3D::FragmentShaderOutputs, Cubemap3D::PerspectiveBuffer, CubemapReflectionFragmentParameterBuffer>;

	//==========================================================================

	struct CubemapFromEquirectangularFragmentShaderConstants {};

	struct CubemapFromEquirectangularFragmentParameters {
		sampler2D equirectangularTexture;
	};

	using CubemapFromEquirectangularFragmentParameterBuffer =
		UniformBuffer<CubemapFromEquirectangularFragmentParameters, "LightBaker3DCubemapFromEquirectangularFragmentParameters">;

	using CubemapFromEquirectangularFragmentShader = graphics::FragmentShader<Cubemap3D::Mesh, Cubemap3D::VertexShaderOutputs, CubemapFromEquirectangularFragmentShaderConstants,
		Cubemap3D::FragmentShaderOutputs, Cubemap3D::PerspectiveBuffer, CubemapFromEquirectangularFragmentParameterBuffer>;

	//==========================================================================

	struct LightProbeAtlasVertex {
		vec2 vertexPosition;
	};

	using LightProbeAtlasMesh = Mesh<LightProbeAtlasVertex>;

	struct LightProbeAtlasVertexShaderConstants {};

	struct LightProbeAtlasVertexShaderOutputs {
		vec2 fragmentTextureCoordinates;
	};

	using LightProbeAtlasVertexShader = VertexShader<LightProbeAtlasMesh, LightProbeAtlasVertexShaderConstants, LightProbeAtlasVertexShaderOutputs>;

	struct LightProbeAtlasIrradianceFragmentShaderConstants {
		uint32_t IRRADIANCE_SAMPLE_COUNT;
	};

	struct LightProbeAtlasDistanceFragmentShaderConstants {
		uint32_t DISTANCE_SAMPLE_COUNT;
	};

	struct LightProbeAtlasFragmentShaderOutputs {
		vec4 outputColor;
	};

	struct LightProbeAtlasIrradianceFragmentParameters {
		samplerCube radianceCubemapTexture;
		float irradianceMapPadding;
	};

	using LightProbeAtlasIrradianceFragmentParameterBuffer = UniformBuffer<LightProbeAtlasIrradianceFragmentParameters, "LightBaker3DLightProbeAtlasIrradianceFragmentParameters">;

	using LightProbeAtlasIrradianceFragmentShader = FragmentShader<LightProbeAtlasMesh, LightProbeAtlasVertexShaderOutputs, LightProbeAtlasIrradianceFragmentShaderConstants,
		LightProbeAtlasFragmentShaderOutputs, LightProbeAtlasIrradianceFragmentParameterBuffer>;

	struct LightProbeAtlasDistanceFragmentParameters {
		samplerCube distanceCubemapTexture;
		float distanceMapPadding;
		float distanceSharpness;
		float distanceMaxDistance;
	};

	using LightProbeAtlasDistanceFragmentParameterBuffer = UniformBuffer<LightProbeAtlasDistanceFragmentParameters, "LightBaker3DLightProbeAtlasDistanceFragmentParameters">;

	using LightProbeAtlasDistanceFragmentShader = FragmentShader<LightProbeAtlasMesh, LightProbeAtlasVertexShaderOutputs, LightProbeAtlasDistanceFragmentShaderConstants,
		LightProbeAtlasFragmentShaderOutputs, LightProbeAtlasDistanceFragmentParameterBuffer>;

	using LightProbeAtlasShaderPipeline = ShaderPipeline<LightProbeAtlasMesh>;

	//==========================================================================

	struct CubemapBaker {
		Array<Cubemap3D::PerspectiveBuffer, 6> sideCubemapPerspectiveBuffers;
		Array<Camera3D, 6> sideCameras;
	};

	struct SkyBaker {
		Cubemap3D::ShaderPipeline cubemapFromEquirectangularShaderPipeline;
		Cubemap3D::ShaderPipeline cubemapIrradianceShaderPipeline;
		Cubemap3D::ShaderPipeline cubemapReflectionShaderPipeline;
		CubemapFromEquirectangularFragmentParameterBuffer cubemapFromEquirectangularFragmentParameterBuffer;
		CubemapIrradianceFragmentParameterBuffer cubemapIrradianceFragmentParameterBuffer;
		CubemapReflectionFragmentParameterBuffer cubemapReflectionFragmentParameterBuffer;
		DrawCommandBuffer<Cubemap3D::Mesh> cubemapFromEquirectangularDrawCommandBuffer;
		DrawCommandBuffer<Cubemap3D::Mesh> cubemapIrradianceDrawCommandBuffer;
		DrawCommandBuffer<Cubemap3D::Mesh> cubemapReflectionDrawCommandBuffer;
	};

	struct LightProbeBaker {
		Texture colorBuffer;
		Texture distanceBuffer;
		Texture depthStencilBuffer;
		LightProbeAtlasMesh lightProbeAtlasMesh;
		LightProbeAtlasShaderPipeline lightProbeAtlasIrradianceShaderPipeline;
		LightProbeAtlasShaderPipeline lightProbeAtlasDistanceShaderPipeline;
		LightProbeAtlasIrradianceFragmentParameterBuffer lightProbeAtlasIrradianceFragmentParameterBuffer;
		LightProbeAtlasDistanceFragmentParameterBuffer lightProbeAtlasDistanceFragmentParameterBuffer;
		DrawCommandBuffer<LightProbeAtlasMesh> lightProbeAtlasIrradianceDrawCommandBuffer;
		DrawCommandBuffer<LightProbeAtlasMesh> lightProbeAtlasDistanceDrawCommandBuffer;
		mat4 renderProjectionMatrix;
	};

	struct ReflectionProbeBaker {
		Texture colorBuffer;
		Texture depthStencilBuffer;
		Cubemap3D::ShaderPipeline cubemapReflectionShaderPipeline;
		CubemapReflectionFragmentParameterBuffer cubemapReflectionFragmentParameterBuffer;
		DrawCommandBuffer<Cubemap3D::Mesh> reflectionMapsDrawCommandBuffer;
		mat4 renderProjectionMatrix;
	};

	[[nodiscard]] GREM_API(graphics_3d) CubemapBaker& getCubemapBaker();
	[[nodiscard]] GREM_API(graphics_3d) SkyBaker& getSkyBaker();
	[[nodiscard]] GREM_API(graphics_3d) LightProbeBaker& getLightProbeBaker();
	[[nodiscard]] GREM_API(graphics_3d) ReflectionProbeBaker& getReflectionProbeBaker();

	Device& device;
	Renderer3D& renderer3D;
	LightBaker3DOptions options;
	Optional<CubemapBaker> cubemapBakerStorage{};
	Optional<SkyBaker> skyBakerStorage{};
	Optional<LightProbeBaker> lightProbeBakerStorage{};
	Optional<ReflectionProbeBaker> reflectionProbeBakerStorage{};
};

} // namespace grem::graphics

#endif
