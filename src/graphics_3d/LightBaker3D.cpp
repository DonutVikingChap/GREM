// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/LightBaker3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>

#include "builtin_shaders_graphics_3d.hpp"

#include <utility> // std::move

namespace grem::graphics {

LightBaker3D::LightBaker3D(Device& device, Renderer3D& renderer3D, const LightBaker3DOptions& options)
	: device(device)
	, renderer3D(renderer3D)
	, options(options) {
	GREM_ASSERT(options.skyIrradianceSampleCount > 0);
	GREM_ASSERT(options.skyReflectionSampleCount > 0);
	GREM_ASSERT(isPowerOf2(options.lightProbeRenderResolution));
	GREM_ASSERT(options.lightProbeNearZ < options.lightProbeFarZ);
	GREM_ASSERT(options.lightProbeIrradianceSampleCount > 0);
	GREM_ASSERT(options.lightProbeDistanceSampleCount > 0);
	GREM_ASSERT(isPowerOf2(options.reflectionProbeRenderResolution));
	GREM_ASSERT(options.reflectionProbeNearZ < options.reflectionProbeFarZ);
	GREM_ASSERT(options.reflectionProbeReflectionSampleCount > 0);
}

void LightBaker3D::bakeSkybox(Sky3D& sky, Texture newSkyTexture, const Sky3DOptions& newSkyOptions) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(newSkyOptions.radianceMapResolution == 0 || isPowerOf2(newSkyOptions.radianceMapResolution));

	sky.options = newSkyOptions;
	sky.radianceMap = {};
	sky.irradianceMap = {};
	sky.reflectionMap = {};
	sky.parameterBufferDirty = true;

	if (newSkyTexture) {
		try {
			static constexpr TextureSamplerOptions SAMPLER_OPTIONS{
				.minificationFilter = TextureFilter::LINEAR,
				.magnificationFilter = TextureFilter::LINEAR,
				.mipmapMode = TextureMipmapMode::LINEAR,
				.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
				.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			};

			if (newSkyTexture.getType() == TextureType::TEXTURE_2D) {
				newSkyTexture = generateCubemapFromEquirectangularMap(newSkyTexture.getInternalFormat(),
					(newSkyOptions.radianceMapResolution == 0) ? newSkyTexture.getHeight() : newSkyOptions.radianceMapResolution, newSkyTexture, SAMPLER_OPTIONS);
			} else {
				if (newSkyTexture.getType() != TextureType::TEXTURE_CUBE) {
					throw graphics::Error{"Invalid sky map texture type."};
				}

				if (const Optional<TextureSamplerOptions> samplerOptions = newSkyTexture.getSamplerOptions(); !samplerOptions || *samplerOptions != SAMPLER_OPTIONS) {
					newSkyTexture = newSkyTexture.copyWithSamplerOptions(SAMPLER_OPTIONS);
				}
			}

			if (sky.options.irradianceMapResolution > 0 || sky.options.reflectionMapResolution > 0) {
				CubemapBaker& cubemapBaker = getCubemapBaker();
				SkyBaker& skyBaker = getSkyBaker();

				if (sky.options.irradianceMapResolution > 0) {
					// Render irradiance map.
					sky.irradianceMap = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::B10G11R11_UFLOAT_PACK32,
						Extent3D{.width = sky.options.irradianceMapResolution, .depth = 6}, 1, ClearValues{},
						TextureSamplerOptions{
							.minificationFilter = TextureFilter::LINEAR,
							.magnificationFilter = TextureFilter::LINEAR,
							.mipmapMode = TextureMipmapMode::NONE,
							.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
							.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
							.maxAnisotropy = 1.0f,
						});

					skyBaker.cubemapIrradianceFragmentParameterBuffer.upload(CubemapIrradianceFragmentParameters{
						.radianceCubemapTexture = newSkyTexture,
					});
					for (uint32_t side = 0; side < 6; ++side) {
						RenderPass renderPass{device, sky.irradianceMap.getSubresource({.layer = side}), ClearValues{}};
						renderPass.draw(skyBaker.cubemapIrradianceDrawCommandBuffer, cubemapBaker.sideCubemapPerspectiveBuffers[side],
							skyBaker.cubemapIrradianceFragmentParameterBuffer);
						device.render(renderPass);
					}
				}

				if (sky.options.reflectionMapResolution > 0) {
					// Render reflection map.
					const uint32_t maxMipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{sky.options.reflectionMapResolution});
					const uint32_t mipLevelCount = maxMipLevelCount - min(maxMipLevelCount - 1, size_t{2}); // Limit to 4x4 pixels as minimum mip resolution.
					sky.reflectionMap = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R16G16B16A16_FLOAT,
						Extent3D{.width = sky.options.reflectionMapResolution, .depth = 6}, mipLevelCount, ClearValues{},
						TextureSamplerOptions{
							.minificationFilter = TextureFilter::LINEAR,
							.magnificationFilter = TextureFilter::LINEAR,
							.mipmapMode = TextureMipmapMode::LINEAR,
							.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
							.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
							.maxAnisotropy = 1.0f,
						});

					for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
						skyBaker.cubemapReflectionFragmentParameterBuffer.upload(CubemapReflectionFragmentParameters{
							.radianceCubemapTexture = newSkyTexture,
							.depthCubemapTexture = renderer3D.getDefaultDepthTextureCube(),
							.reflectionResolution = static_cast<float>(newSkyTexture.getWidth()),
							.reflectionRoughness = (mipLevelCount < 2) ? 0.0f : static_cast<float>(mipLevel) / static_cast<float>(mipLevelCount - 1),
						});
						for (uint32_t side = 0; side < 6; ++side) {
							RenderPass renderPass{device, sky.reflectionMap.getSubresource({.layer = side, .mipLevel = mipLevel}), UndefinedClearValues{}};
							renderPass.draw(skyBaker.cubemapReflectionDrawCommandBuffer, cubemapBaker.sideCubemapPerspectiveBuffers[side],
								skyBaker.cubemapReflectionFragmentParameterBuffer);
							device.render(renderPass);
						}
					}
				}
			}
		} catch (...) {
			sky.irradianceMap = {};
			sky.reflectionMap = {};
			throw;
		}
	}

	sky.radianceMap = std::move(newSkyTexture);
}

void LightBaker3D::bakeSkybox(Sky3D& sky, const resource::ImageView& newSkyImage, const Sky3DOptions& newSkyOptions, const TextureImageUploadOptions& newSkyImageUploadOptions) {
	GREM_PROFILE_FUNCTION();

	Texture newSkyTexture{};
	switch (newSkyImage.getType()) {
		case resource::ImageType::EMPTY: break;
		case resource::ImageType::IMAGE_2D: {
			const Texture equirectangularMap{device, newSkyImage, newSkyImageUploadOptions,
				TextureSamplerOptions{
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
				}};
			newSkyTexture = generateCubemapFromEquirectangularMap(equirectangularMap.getInternalFormat(),
				(newSkyOptions.radianceMapResolution == 0) ? equirectangularMap.getHeight() : newSkyOptions.radianceMapResolution, equirectangularMap,
				TextureSamplerOptions{
					.mipmapMode = TextureMipmapMode::LINEAR,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
				});
			break;
		}
		case resource::ImageType::IMAGE_CUBE:
			newSkyTexture = Texture{device, newSkyImage, newSkyImageUploadOptions,
				TextureSamplerOptions{
					.mipmapMode = TextureMipmapMode::LINEAR,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
				}};
			break;
		default: throw graphics::Error{"Invalid sky map image type."};
	}

	bakeSkybox(sky, std::move(newSkyTexture), newSkyOptions);
}

void LightBaker3D::bakeDiffuseLighting(LightProbeVolumes3D& lightProbeVolumes, FunctionView<void(RenderPass&, const Camera3D&)> drawRadiance,
	FunctionView<void(RenderPass&, const Camera3D&)> drawDistance) {
	GREM_PROFILE_FUNCTION();

	for (size_t lightProbeVolumeIndex = 0; lightProbeVolumeIndex < lightProbeVolumes.volumes.size(); ++lightProbeVolumeIndex) {
		const u32vec3 probeCounts = lightProbeVolumes.volumeOptions[lightProbeVolumeIndex].probeCounts;
		for (uint32_t y = 0; y < probeCounts.y; ++y) {
			for (uint32_t z = 0; z < probeCounts.z; ++z) {
				for (uint32_t x = 0; x < probeCounts.x; ++x) {
					bakeLightProbeIrradianceMap(lightProbeVolumes, lightProbeVolumeIndex, u32vec3{x, y, z}, drawRadiance);
				}
			}
		}
	}

	for (size_t lightProbeVolumeIndex = 0; lightProbeVolumeIndex < lightProbeVolumes.volumes.size(); ++lightProbeVolumeIndex) {
		const u32vec3 probeCounts = lightProbeVolumes.volumeOptions[lightProbeVolumeIndex].probeCounts;
		for (uint32_t y = 0; y < probeCounts.y; ++y) {
			for (uint32_t z = 0; z < probeCounts.z; ++z) {
				for (uint32_t x = 0; x < probeCounts.x; ++x) {
					bakeLightProbeDistanceMap(lightProbeVolumes, lightProbeVolumeIndex, u32vec3{x, y, z}, drawDistance);
				}
			}
		}
	}
}

void LightBaker3D::bakeLightProbeIrradianceMap(LightProbeVolumes3D& lightProbeVolumes, uint32_t lightProbeVolumeIndex, u32vec3 lightProbeGridIndices,
	FunctionView<void(RenderPass&, const Camera3D&)> drawRadiance) {
	GREM_PROFILE_FUNCTION();

	CubemapBaker& cubemapBaker = getCubemapBaker();
	LightProbeBaker& lightProbeBaker = getLightProbeBaker();

	lightProbeVolumes.flushVolumesAndTextures(device);

	lightProbeVolumes.buffersDirty = true;

	const LightProbeVolumes3D::VolumeFields& volume = lightProbeVolumes.volumes[lightProbeVolumeIndex];
	const LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumes.volumeOptions[lightProbeVolumeIndex];
	const vec3 halfExtents = vec3{volumeOptions.probeCounts} * volumeOptions.probeSpacing * 0.5f;
	const vec3 position = volumeOptions.center + volumeOptions.orientation * ((vec3{lightProbeGridIndices} + vec3{0.5f}) * volumeOptions.probeSpacing - halfExtents);
	const Array<mat4, 6> renderViewMatrices = Cubemap3D::getSideViewMatrices(position);
	for (uint32_t side = 0; side < 6; ++side) {
		Camera3D& camera = cubemapBaker.sideCameras[side];
		camera.setProjectionAndViewAndOptions(lightProbeBaker.renderProjectionMatrix, renderViewMatrices[side], {.exposure = options.lightProbeExposure});

		RenderPass renderPass{device, {lightProbeBaker.colorBuffer.getSubresource({.layer = side}), lightProbeBaker.depthStencilBuffer}, ClearValues{}};
		drawRadiance(renderPass, camera);
		device.render(renderPass);
	}

	const u32vec3 irradianceAtlasOffset{
		static_cast<uint32_t>(volume.lightProbeVolumeIrradianceAtlasOffset.x * static_cast<float>(lightProbeVolumes.irradianceAtlasResolution)),
		static_cast<uint32_t>(volume.lightProbeVolumeIrradianceAtlasOffset.y * static_cast<float>(lightProbeVolumes.irradianceAtlasResolution)),
		static_cast<uint32_t>(volume.lightProbeVolumeIrradianceAtlasOffset.z),
	};
	const u32vec2 irradianceAtlasCoordinates = u32vec2{irradianceAtlasOffset} + u32vec2{lightProbeGridIndices.x, lightProbeGridIndices.z} * volumeOptions.irradianceMapResolution;
	RenderPass renderPass{device, lightProbeVolumes.irradianceAtlasTexture.getSubresource({.layer = irradianceAtlasOffset.z + lightProbeGridIndices.y}), RetainValues{},
		Viewport{.region{
			.offset = Offset2D::from(i32vec2{irradianceAtlasCoordinates}),
			.size{volumeOptions.irradianceMapResolution},
		}}};
	lightProbeBaker.lightProbeAtlasIrradianceFragmentParameterBuffer.upload(LightProbeAtlasIrradianceFragmentParameters{
		.radianceCubemapTexture = lightProbeBaker.colorBuffer,
		.irradianceMapPadding = 1.0f / static_cast<float>(volumeOptions.irradianceMapResolution),
	});
	renderPass.draw(lightProbeBaker.lightProbeAtlasIrradianceDrawCommandBuffer, lightProbeBaker.lightProbeAtlasIrradianceFragmentParameterBuffer);
	device.render(renderPass);
}

void LightBaker3D::bakeLightProbeDistanceMap(LightProbeVolumes3D& lightProbeVolumes, uint32_t lightProbeVolumeIndex, u32vec3 lightProbeGridIndices,
	FunctionView<void(RenderPass&, const Camera3D&)> drawDistance) {
	GREM_PROFILE_FUNCTION();

	CubemapBaker& cubemapBaker = getCubemapBaker();
	LightProbeBaker& lightProbeBaker = getLightProbeBaker();

	lightProbeVolumes.flushVolumesAndTextures(device);

	lightProbeVolumes.buffersDirty = true;

	const LightProbeVolumes3D::VolumeFields& volume = lightProbeVolumes.volumes[lightProbeVolumeIndex];
	const LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumes.volumeOptions[lightProbeVolumeIndex];
	const vec3 halfExtents = vec3{volumeOptions.probeCounts} * volumeOptions.probeSpacing * 0.5f;
	const float maxDistance = length(volumeOptions.probeSpacing) * 1.5f;

	const vec3 position = volumeOptions.center + volumeOptions.orientation * ((vec3{lightProbeGridIndices} + vec3{0.5f}) * volumeOptions.probeSpacing - halfExtents);
	const Array<mat4, 6> renderViewMatrices = Cubemap3D::getSideViewMatrices(position);
	for (uint32_t side = 0; side < 6; ++side) {
		Camera3D& camera = cubemapBaker.sideCameras[side];
		camera.setProjectionAndViewAndOptions(lightProbeBaker.renderProjectionMatrix, renderViewMatrices[side], {.exposure = options.lightProbeExposure});

		RenderPass renderPass{device, {lightProbeBaker.distanceBuffer.getSubresource({.layer = side}), lightProbeBaker.depthStencilBuffer},
			ClearValues{.color = Color::fromLinear(maxDistance, 0.0f, 0.0f, 1.0f)}};
		drawDistance(renderPass, camera);
		device.render(renderPass);
	}

	const u32vec3 distanceAtlasOffset{
		static_cast<uint32_t>(volume.lightProbeVolumeDistanceAtlasOffset.x * static_cast<float>(lightProbeVolumes.distanceAtlasResolution)),
		static_cast<uint32_t>(volume.lightProbeVolumeDistanceAtlasOffset.y * static_cast<float>(lightProbeVolumes.distanceAtlasResolution)),
		static_cast<uint32_t>(volume.lightProbeVolumeDistanceAtlasOffset.z),
	};
	const u32vec2 distanceAtlasCoordinates = u32vec2{distanceAtlasOffset} + u32vec2{lightProbeGridIndices.x, lightProbeGridIndices.z} * volumeOptions.distanceMapResolution;
	const Viewport viewport{.region{
		.offset = Offset2D::from(i32vec2{distanceAtlasCoordinates}),
		.size{volumeOptions.distanceMapResolution},
	}};
	RenderPass renderPass{device, lightProbeVolumes.distanceAtlasTexture.getSubresource({.layer = distanceAtlasOffset.z + lightProbeGridIndices.y}), RetainValues{}, viewport};
	lightProbeBaker.lightProbeAtlasDistanceFragmentParameterBuffer.upload(LightProbeAtlasDistanceFragmentParameters{
		.distanceCubemapTexture = lightProbeBaker.distanceBuffer,
		.distanceMapPadding = 1.0f / static_cast<float>(volumeOptions.distanceMapResolution),
		.distanceSharpness = options.lightProbeDistanceSharpness,
		.distanceMaxDistance = maxDistance,
	});
	renderPass.draw(lightProbeBaker.lightProbeAtlasDistanceDrawCommandBuffer, lightProbeBaker.lightProbeAtlasDistanceFragmentParameterBuffer);
	device.render(renderPass);
}

void LightBaker3D::bakeSpecularReflections(ReflectionProbes3D& reflectionProbes, FunctionView<void(RenderPass&, const Camera3D&)> drawReflection) {
	GREM_PROFILE_FUNCTION();

	for (size_t reflectionProbeIndex = 0; reflectionProbeIndex < reflectionProbes.probes.size(); ++reflectionProbeIndex) {
		bakeReflectionProbeReflectionMap(reflectionProbes, reflectionProbeIndex, drawReflection);
	}
}

void LightBaker3D::bakeReflectionProbeReflectionMap(ReflectionProbes3D& reflectionProbes, uint32_t reflectionProbeIndex,
	FunctionView<void(RenderPass&, const Camera3D&)> drawReflection) {
	GREM_PROFILE_FUNCTION();

	CubemapBaker& cubemapBaker = getCubemapBaker();
	ReflectionProbeBaker& reflectionProbeBaker = getReflectionProbeBaker();

	reflectionProbes.flushProbesAndTextures(device);

	reflectionProbes.buffersDirty = true;

	const ReflectionProbeOptions3D& probeOptions = reflectionProbes.probeOptions[reflectionProbeIndex];
	const vec3 capturePosition = probeOptions.center + probeOptions.captureOffset;
	const Array<mat4, 6> renderViewMatrices = Cubemap3D::getSideViewMatrices(capturePosition);
	for (uint32_t side = 0; side < 6; ++side) {
		Camera3D& camera = cubemapBaker.sideCameras[side];
		camera.setProjectionAndViewAndOptions(reflectionProbeBaker.renderProjectionMatrix, renderViewMatrices[side], {.exposure = options.reflectionProbeExposure});

		RenderPass renderPass{device, {reflectionProbeBaker.colorBuffer.getSubresource({.layer = side}), reflectionProbeBaker.depthStencilBuffer.getSubresource({.layer = side})},
			ClearValues{}};
		drawReflection(renderPass, camera);
		device.render(renderPass);
	}
	reflectionProbeBaker.colorBuffer.generateMipmap();

	const uint32_t mipLevelCount = reflectionProbes.reflectionMaps.getMipLevelCount();
	for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
		reflectionProbeBaker.cubemapReflectionFragmentParameterBuffer.upload(CubemapReflectionFragmentParameters{
			.radianceCubemapTexture = reflectionProbeBaker.colorBuffer,
			.depthCubemapTexture = reflectionProbeBaker.depthStencilBuffer,
			.reflectionResolution = static_cast<float>(options.reflectionProbeRenderResolution),
			.reflectionRoughness = (mipLevelCount < 2) ? 0.0f : static_cast<float>(mipLevel) / static_cast<float>(mipLevelCount - 1),
		});
		for (uint32_t side = 0; side < 6; ++side) {
			RenderPass renderPass{device, reflectionProbes.reflectionMaps.getSubresource({.layer = static_cast<uint32_t>(reflectionProbeIndex) * 6 + side, .mipLevel = mipLevel}),
				UndefinedClearValues{}};
			renderPass.draw(reflectionProbeBaker.reflectionMapsDrawCommandBuffer, cubemapBaker.sideCubemapPerspectiveBuffers[side],
				reflectionProbeBaker.cubemapReflectionFragmentParameterBuffer);
			device.render(renderPass);
		}
	}
}

Texture LightBaker3D::generateCubemapFromEquirectangularMap(TextureFormat internalFormat, uint32_t resolution, const Texture& equirectangularMap,
	Optional<TextureSamplerOptions> samplerOptions) {
	if (equirectangularMap.getType() != TextureType::TEXTURE_2D) {
		throw graphics::Error{"Invalid equirectangular map texture type."};
	}

	CubemapBaker& cubemapBaker = getCubemapBaker();
	SkyBaker& skyBaker = getSkyBaker();

	Texture newEquirectangularMap{};
	const Texture* texture = &equirectangularMap;
	if (const Optional<TextureSamplerOptions> samplerOptions = equirectangularMap.getSamplerOptions();
		!samplerOptions || samplerOptions->horizontalWrappingMode != TextureWrappingMode::CLAMP_TO_EDGE ||
		samplerOptions->verticalWrappingMode != TextureWrappingMode::CLAMP_TO_EDGE) {
		TextureSamplerOptions newSamplerOptions = *samplerOptions;
		newSamplerOptions.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE;
		newSamplerOptions.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE;
		newEquirectangularMap = equirectangularMap.copyWithSamplerOptions(newSamplerOptions);
		texture = &newEquirectangularMap;
	}

	const uint32_t mipLevelCount = (samplerOptions && samplerOptions->mipmapMode != TextureMipmapMode::NONE) ? resource::Image::getMaxMipLevelCount(Extent2D{resolution}) : 1;
	Texture result =
		Texture::create(device, TextureType::TEXTURE_CUBE, internalFormat, Extent3D{.width = resolution, .depth = 6}, mipLevelCount, UndefinedClearValues{}, samplerOptions);

	skyBaker.cubemapFromEquirectangularFragmentParameterBuffer.upload(CubemapFromEquirectangularFragmentParameters{.equirectangularTexture = *texture});
	for (uint32_t side = 0; side < 6; ++side) {
		RenderPass renderPass{device, result.getSubresource({.layer = side}), ClearValues{}};
		renderPass.draw(skyBaker.cubemapFromEquirectangularDrawCommandBuffer, cubemapBaker.sideCubemapPerspectiveBuffers[side],
			skyBaker.cubemapFromEquirectangularFragmentParameterBuffer);
		device.render(renderPass);
	}

	if (samplerOptions && samplerOptions->mipmapMode != TextureMipmapMode::NONE) {
		result.generateMipmap();
	}
	return result;
}

LightBaker3D::CubemapBaker& LightBaker3D::getCubemapBaker() {
	if (!cubemapBakerStorage) {
		[[unlikely]];
		cubemapBakerStorage.emplace(CubemapBaker{
			.sideCubemapPerspectiveBuffers{
				Cubemap3D::PerspectiveBuffer{device},
				Cubemap3D::PerspectiveBuffer{device},
				Cubemap3D::PerspectiveBuffer{device},
				Cubemap3D::PerspectiveBuffer{device},
				Cubemap3D::PerspectiveBuffer{device},
				Cubemap3D::PerspectiveBuffer{device},
			},
			.sideCameras{
				Camera3D{device},
				Camera3D{device},
				Camera3D{device},
				Camera3D{device},
				Camera3D{device},
				Camera3D{device},
			},
		});
		try {
			const Array<mat4, 6> sideViewMatrices = Cubemap3D::getSideViewMatrices(vec3{0.0f, 0.0f, 0.0f});
			for (size_t side = 0; side < 6; ++side) {
				cubemapBakerStorage->sideCubemapPerspectiveBuffers[side].upload(Cubemap3D::PerspectiveParameters{
					.cubemapViewProjectionMatrix = Cubemap3D::SIDE_PROJECTION_MATRIX * sideViewMatrices[side],
				});
			}
		} catch (...) {
			cubemapBakerStorage.reset();
			throw;
		}
	}
	return *cubemapBakerStorage;
}

LightBaker3D::SkyBaker& LightBaker3D::getSkyBaker() {
	if (!skyBakerStorage) {
		[[unlikely]];
		const Cubemap3D::VertexShader cubemapVertexShader = Cubemap3D::VertexShader::create(device, detail::CUBEMAP_3D_DEFAULT_VERTEX_SHADER_CODE);
		skyBakerStorage.emplace(SkyBaker{
			.cubemapFromEquirectangularShaderPipeline{
				device,
				cubemapVertexShader,
				Cubemap3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				CubemapFromEquirectangularFragmentShader::create(device, detail::LIGHT_BAKER_3D_CUBEMAP_FROM_EQUIRECTANGULAR_FRAGMENT_SHADER_CODE),
				CubemapFromEquirectangularFragmentShaderConstants{},
				Cubemap3D::DEFAULT_SHADER_PIPELINE_OPTIONS,
			},
			.cubemapIrradianceShaderPipeline{
				device,
				cubemapVertexShader,
				Cubemap3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				CubemapIrradianceFragmentShader::create(device, detail::LIGHT_BAKER_3D_CUBEMAP_IRRADIANCE_FRAGMENT_SHADER_CODE),
				CubemapIrradianceFragmentShaderConstants{.IRRADIANCE_SAMPLE_COUNT = options.skyIrradianceSampleCount},
				Cubemap3D::DEFAULT_SHADER_PIPELINE_OPTIONS,
			},
			.cubemapReflectionShaderPipeline{
				device,
				cubemapVertexShader,
				Cubemap3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				CubemapReflectionFragmentShader::create(device, detail::LIGHT_BAKER_3D_CUBEMAP_REFLECTION_FRAGMENT_SHADER_CODE),
				CubemapReflectionFragmentShaderConstants{.REFLECTION_SAMPLE_COUNT = options.skyReflectionSampleCount},
				Cubemap3D::DEFAULT_SHADER_PIPELINE_OPTIONS,
			},
			.cubemapFromEquirectangularFragmentParameterBuffer{device},
			.cubemapIrradianceFragmentParameterBuffer{device},
			.cubemapReflectionFragmentParameterBuffer{device},
			.cubemapFromEquirectangularDrawCommandBuffer{device},
			.cubemapIrradianceDrawCommandBuffer{device},
			.cubemapReflectionDrawCommandBuffer{device},
		});
		try {
			skyBakerStorage->cubemapFromEquirectangularDrawCommandBuffer.push(skyBakerStorage->cubemapFromEquirectangularShaderPipeline, renderer3D.getCubemap3D().getMesh());
			skyBakerStorage->cubemapIrradianceDrawCommandBuffer.push(skyBakerStorage->cubemapIrradianceShaderPipeline, renderer3D.getCubemap3D().getMesh());
			skyBakerStorage->cubemapReflectionDrawCommandBuffer.push(skyBakerStorage->cubemapReflectionShaderPipeline, renderer3D.getCubemap3D().getMesh());
		} catch (...) {
			skyBakerStorage.reset();
			throw;
		}
	}
	return *skyBakerStorage;
}

LightBaker3D::LightProbeBaker& LightBaker3D::getLightProbeBaker() {
	if (!lightProbeBakerStorage) {
		[[unlikely]];
		static constexpr Array LIGHT_PROBE_ATLAS_MESH_VERTICES{
			LightProbeAtlasVertex{.vertexPosition{0.0f, 0.0f}},
			LightProbeAtlasVertex{.vertexPosition{1.0f, 0.0f}},
			LightProbeAtlasVertex{.vertexPosition{0.0f, 1.0f}},
			LightProbeAtlasVertex{.vertexPosition{1.0f, 1.0f}},
		};

		lightProbeBakerStorage.emplace(LightProbeBaker{
			.colorBuffer = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R16G16B16A16_FLOAT, Extent3D{.width = options.lightProbeRenderResolution, .depth = 6},
				1, UndefinedClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
				}),
			.distanceBuffer = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R16_FLOAT, Extent3D{.width = options.lightProbeRenderResolution, .depth = 6}, 1,
				UndefinedClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
				}),
			.depthStencilBuffer = Texture::createRenderbuffer(device, TextureFormat::D32_FLOAT_S8_UINT, Extent2D{options.lightProbeRenderResolution}, 1, UndefinedClearValues{}),
			.lightProbeAtlasMesh{device, LIGHT_PROBE_ATLAS_MESH_VERTICES},
			.lightProbeAtlasIrradianceShaderPipeline{
				device,
				LightProbeAtlasVertexShader::create(device, detail::LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_DEFAULT_VERTEX_SHADER_CODE),
				LightProbeAtlasVertexShaderConstants{},
				LightProbeAtlasIrradianceFragmentShader::create(device, detail::LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_IRRADIANCE_FRAGMENT_SHADER_CODE),
				LightProbeAtlasIrradianceFragmentShaderConstants{.IRRADIANCE_SAMPLE_COUNT = options.lightProbeIrradianceSampleCount},
				ShaderPipelineOptions{
					.depthBufferMode = DepthBufferMode::NONE,
					.stencilBufferMode = StencilBufferMode::NONE,
					.primitiveType = PrimitiveType::TRIANGLE_STRIP,
					.faceCullingMode = FaceCullingMode::NONE,
					.frontFace = FrontFace::COUNTERCLOCKWISE,
					.blendState{},
				},
			},
			.lightProbeAtlasDistanceShaderPipeline{
				device,
				LightProbeAtlasVertexShader::create(device, detail::LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_DEFAULT_VERTEX_SHADER_CODE),
				LightProbeAtlasVertexShaderConstants{},
				LightProbeAtlasDistanceFragmentShader::create(device, detail::LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_DISTANCE_FRAGMENT_SHADER_CODE),
				LightProbeAtlasDistanceFragmentShaderConstants{.DISTANCE_SAMPLE_COUNT = options.lightProbeDistanceSampleCount},
				ShaderPipelineOptions{
					.depthBufferMode = DepthBufferMode::NONE,
					.stencilBufferMode = StencilBufferMode::NONE,
					.primitiveType = PrimitiveType::TRIANGLE_STRIP,
					.faceCullingMode = FaceCullingMode::NONE,
					.frontFace = FrontFace::COUNTERCLOCKWISE,
					.blendState{},
				},
			},
			.lightProbeAtlasIrradianceFragmentParameterBuffer{device},
			.lightProbeAtlasDistanceFragmentParameterBuffer{device},
			.lightProbeAtlasIrradianceDrawCommandBuffer{device},
			.lightProbeAtlasDistanceDrawCommandBuffer{device},
			.renderProjectionMatrix = perspective(convertDegreesToRadians(90.0f), 1.0f, options.lightProbeNearZ, options.lightProbeFarZ),
		});
		try {
			lightProbeBakerStorage->lightProbeAtlasIrradianceDrawCommandBuffer.push(lightProbeBakerStorage->lightProbeAtlasIrradianceShaderPipeline,
				lightProbeBakerStorage->lightProbeAtlasMesh);
			lightProbeBakerStorage->lightProbeAtlasDistanceDrawCommandBuffer.push(lightProbeBakerStorage->lightProbeAtlasDistanceShaderPipeline,
				lightProbeBakerStorage->lightProbeAtlasMesh);
		} catch (...) {
			lightProbeBakerStorage.reset();
			throw;
		}
	}
	return *lightProbeBakerStorage;
}

LightBaker3D::ReflectionProbeBaker& LightBaker3D::getReflectionProbeBaker() {
	if (!reflectionProbeBakerStorage) {
		[[unlikely]];
		reflectionProbeBakerStorage.emplace(ReflectionProbeBaker{
			.colorBuffer = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R16G16B16A16_FLOAT,
				Extent3D{.width = options.reflectionProbeRenderResolution, .depth = 6}, resource::Image::getMaxMipLevelCount(Extent2D{options.reflectionProbeRenderResolution}),
				UndefinedClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::LINEAR,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
				}),
			.depthStencilBuffer = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::D32_FLOAT_S8_UINT,
				Extent3D{.width = options.reflectionProbeRenderResolution, .depth = 6}, 1, UndefinedClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::NEAREST,
					.magnificationFilter = TextureFilter::NEAREST,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
					.depthComparisonMode = TextureDepthComparisonMode::NOT_EQUAL,
				}),
			.cubemapReflectionShaderPipeline{
				device,
				Cubemap3D::VertexShader::create(device, detail::CUBEMAP_3D_DEFAULT_VERTEX_SHADER_CODE),
				Cubemap3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				CubemapReflectionFragmentShader::create(device, detail::LIGHT_BAKER_3D_CUBEMAP_REFLECTION_FRAGMENT_SHADER_CODE),
				CubemapReflectionFragmentShaderConstants{.REFLECTION_SAMPLE_COUNT = options.reflectionProbeReflectionSampleCount},
				Cubemap3D::DEFAULT_SHADER_PIPELINE_OPTIONS,
			},
			.cubemapReflectionFragmentParameterBuffer{device},
			.reflectionMapsDrawCommandBuffer{device},
			.renderProjectionMatrix = perspective(convertDegreesToRadians(90.0f), 1.0f, options.reflectionProbeNearZ, options.reflectionProbeFarZ),
		});
		try {
			reflectionProbeBakerStorage->reflectionMapsDrawCommandBuffer.push(reflectionProbeBakerStorage->cubemapReflectionShaderPipeline, renderer3D.getCubemap3D().getMesh());
		} catch (...) {
			reflectionProbeBakerStorage.reset();
			throw;
		}
	}
	return *reflectionProbeBakerStorage;
}

} // namespace grem::graphics
