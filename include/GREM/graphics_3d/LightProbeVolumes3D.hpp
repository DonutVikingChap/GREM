// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_LIGHT_PROBE_VOLUMES_3D_HPP
#define GREM_GRAPHICS_3D_LIGHT_PROBE_VOLUMES_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>

namespace grem::graphics {

class Device;       // Forward declaration, to avoid including Device.hpp.
class RenderPass;   // Forward declaration, to avoid including RenderPass.hpp.
class Renderer3D;   // Forward declaration, to avoid a circular include of Renderer3D.hpp.
class LightBaker3D; // Forward declaration, to avoid a circular include of LightBaker3D.hpp.

/**
 * Configuration options for a light probe volume in a LightProbeVolumes3D set.
 */
struct LightProbeVolumeOptions3D {
	/**
	 * Center position of the light probe volume in world space.
	 */
	vec3 center;

	/**
	 * Orientation of the light probe volume in world space.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Local spacing between the light probes.
	 *
	 * Each component must be positive.
	 */
	vec3 probeSpacing;

	/**
	 * Number of light probes along each axis of the light probe volume.
	 *
	 * Each count must be positive.
	 */
	u32vec3 probeCounts;

	/**
	 * Width, in texels, of each light probe's irradiance map in the irradiance
	 * atlas, including a 1 pixel border of padding.
	 *
	 * Must be a power of 2.
	 * Must be greater than or equal to 4.
	 */
	uint32_t irradianceMapResolution = 8;

	/**
	 * Width, in texels, of each light probe's distance map in the distance
	 * atlas, including a 1 pixel border of padding.
	 *
	 * Must be a power of 2.
	 * Must be greater than or equal to 4.
	 */
	uint32_t distanceMapResolution = 16;
};

/**
 * Configuration options for a LightProbeVolumes3D set.
 */
struct LightProbeVolumes3DOptions {
	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const LightProbeVolumes3DOptions& other) const noexcept = default;
};

/**
 * Set of light probe volumes in 3D space.
 */
class LightProbeVolumes3D {
public:
	/**
	 * Struct of shader parameters representing the light probe volume atlases.
	 */
	struct AtlasParameters {
		/** Sampler for the atlas of irradiance values for the light probe volumes. */
		sampler2DArray lightProbeVolumesIrradianceAtlasTexture;

		/** Sampler for the atlas of distance values for the light probe volumes. */
		sampler2DArray lightProbeVolumesDistanceAtlasTexture;
	};

	/**
	 * Shader buffer for light probe volume atlases.
	 */
	using AtlasBuffer = UniformBuffer<AtlasParameters, "LightProbeVolumes3DAtlases">;

	/**
	 * Struct of shader fields representing a light probe volume.
	 */
	struct VolumeFields {
		/** Center position of the light probe volume in world space. */
		vec3 lightProbeVolumeCenter;

		/** Components of the quaternion representing the orientation of the light probe volume in world space. */
		vec4 lightProbeVolumeOrientation;

		/** Local spacing between the light probes. */
		vec3 lightProbeVolumeProbeSpacing;

		/** Number of probes along each coordinate axis in the light probe volume. */
		vec3 lightProbeVolumeProbeCounts;

		/** Texture coordinate offset of the light probe volume in the atlas of irradiance values. */
		vec3 lightProbeVolumeIrradianceAtlasOffset;

		/** Texture coordinate offset of the light probe volume in the atlas of distance values. */
		vec3 lightProbeVolumeDistanceAtlasOffset;

		/** Padded size of the probes in the irradiance map atlas (X), and the size of the texels (Y) in texture coordinates. */
		vec2 lightProbeVolumeIrradianceAtlasPaddedProbeSizeAndTexelSize;

		/** Padded size of the probes in the distance map atlas (X), and the size of the texels (Y) in texture coordinates. */
		vec2 lightProbeVolumeDistanceAtlasPaddedProbeSizeAndTexelSize;
	};

	/**
	 * Shader buffer for light probe volumes.
	 */
	using VolumeBuffer = StorageBuffer<VolumeFields, "LightProbeVolumes3DVolumes">;

	/**
	 * Sampler options to use for the irradiance atlas of light probe volumes.
	 */
	static constexpr TextureSamplerOptions IRRADIANCE_ATLAS_SAMPLER_OPTIONS{
		.minificationFilter = TextureFilter::LINEAR,
		.magnificationFilter = TextureFilter::LINEAR,
		.mipmapMode = TextureMipmapMode::NONE,
		.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
	};

	/**
	 * Sampler options to use for the distance atlas of light probe volumes.
	 */
	static constexpr TextureSamplerOptions DISTANCE_ATLAS_SAMPLER_OPTIONS{
		.minificationFilter = TextureFilter::LINEAR,
		.magnificationFilter = TextureFilter::LINEAR,
		.mipmapMode = TextureMipmapMode::NONE,
		.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
	};

	/**
	 * Construct an empty set of light probe volumes.
	 *
	 * \param device device to create the set for. Must outlive the set.
	 * \param options light probe volume set options, see
	 *        LightProbeVolumes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) explicit LightProbeVolumes3D(Device& device, const LightProbeVolumes3DOptions& options = {});

	/**
	 * Construct a pre-baked set of light probe volumes.
	 *
	 * \param device device to create the set for. Must outlive the set.
	 * \param irradianceAtlasTexture pre-baked atlas of irradiance values for
	 *        the light probe volumes. Must be a valid sampled 2D array texture
	 *        in an RGB(A) color format, or empty if there are no volumes. The
	 *        texture's sampler options should be
	 *        LightProbeVolumes3D::IRRADIANCE_ATLAS_SAMPLER_OPTIONS.
	 * \param distanceAtlasTexture pre-baked atlas of distance values for the
	 *        light probe volumes. Must be a valid sampled 2D array texture in a
	 *        single-channel color format, or empty if there are no volumes. The
	 *        texture's sampler options should be
	 *        LightProbeVolumes3D::DISTANCE_ATLAS_SAMPLER_OPTIONS.
	 * \param volumeOptions list of light probe volume options.
	 * \param options light probe volume set options, see
	 *        LightProbeVolumes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	LightProbeVolumes3D(Device& device, Texture irradianceAtlasTexture, Texture distanceAtlasTexture, ArrayList<LightProbeVolumeOptions3D> volumeOptions,
		const LightProbeVolumes3DOptions& options);

	/**
	 * Remove all light probe volumes from the set.
	 */
	GREM_API(graphics_3d) void clearLightProbeVolumes() noexcept;

	/**
	 * Add a light probe volume to the set.
	 *
	 * \param options light probe volume options, see LightProbeVolumeOptions3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void addLightProbeVolume(const LightProbeVolumeOptions3D& options);

	/**
	 * Set the light probe volume set to the default empty set with new options.
	 *
	 * \param newOptions light probe volume set options, see
	 *        LightProbeVolumes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setLightProbeVolumes(const LightProbeVolumes3DOptions& newOptions);

	/**
	 * Assign a pre-baked set of light probe volumes to the light probe volume
	 * set.
	 *
	 * \param newIrradianceAtlasTexture pre-baked atlas of irradiance values for
	 *        the light probe volumes. Must be a valid sampled 2D array texture
	 *        in an RGB(A) color format, or empty if there are no volumes.
	 * \param newDistanceAtlasTexture pre-baked atlas of distance values for the
	 *        light probe volumes. Must be a valid sampled 2D array texture in a
	 *        single-channel color format, or empty if there are no volumes.
	 * \param newVolumeOptions list of light probe volume options.
	 * \param newOptions light probe volume set options, see
	 *        LightProbeVolumes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void setLightProbeVolumes(Texture newIrradianceAtlasTexture, Texture newDistanceAtlasTexture, ArrayList<LightProbeVolumeOptions3D> newVolumeOptions,
		const LightProbeVolumes3DOptions& newOptions);

	/**
	 * Get the atlas of irradiance values for the light probe volumes.
	 *
	 * \return a read-only reference to the irradiance atlas texture, which will
	 *         be empty if the light probe volumes have not been baked since
	 *         their last modification.
	 */
	[[nodiscard]] const Texture& getIrradianceAtlasTexture() const noexcept {
		return irradianceAtlasTexture;
	}

	/**
	 * Get the atlas of distance values for the light probe volumes.
	 *
	 * \return a read-only reference to the distance atlas texture, which will
	 *         be empty if the light probe volumes have not been baked since
	 *         their last modification.
	 */
	[[nodiscard]] const Texture& getDistanceAtlasTexture() const noexcept {
		return distanceAtlasTexture;
	}

	/**
	 * Get the configuration options for the light probe volumes in the set.
	 *
	 * \return a read-only view over the list of light probe volume options,
	 *         valid until the next modification to the light probe volume set.
	 */
	[[nodiscard]] Span<const LightProbeVolumeOptions3D> getVolumeOptions() const noexcept {
		return volumeOptions;
	}

	/**
	 * Get the configuration options of the light probe volume set.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] LightProbeVolumes3DOptions getOptions() const noexcept {
		return options;
	}

private:
	friend Renderer3D;
	friend LightBaker3D;

	GREM_API(graphics_3d) void flushVolumesAndTextures(Device& device) const;
	GREM_API(graphics_3d) void flush(Device& device, Renderer3D& renderer3D) const;

	LightProbeVolumes3DOptions options;
	ArrayList<LightProbeVolumeOptions3D> volumeOptions{};
	mutable Texture irradianceAtlasTexture{};
	mutable Texture distanceAtlasTexture{};
	mutable uint32_t irradianceAtlasResolution = 0;
	mutable uint32_t irradianceAtlasDepth = 0;
	mutable uint32_t distanceAtlasResolution = 0;
	mutable uint32_t distanceAtlasDepth = 0;
	mutable Buffer<VolumeFields> volumes{};
	mutable Buffer<float> worldSpaceVolumes{};
	mutable AtlasBuffer atlasBuffer;
	mutable VolumeBuffer volumeBuffer;
	mutable bool volumesDirty = false;
	mutable bool texturesDirty = false;
	mutable bool buffersDirty = true;
};

} // namespace grem::graphics

#endif
