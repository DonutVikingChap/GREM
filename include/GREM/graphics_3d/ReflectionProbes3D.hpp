// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_REFLECTION_PROBES_3D_HPP
#define GREM_GRAPHICS_3D_REFLECTION_PROBES_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>

namespace grem::graphics {

class Device;       // Forward declaration, to avoid including Device.hpp.
class RenderPass;   // Forward declaration, to avoid including RenderPass.hpp.
class Renderer3D;   // Forward declaration, to avoid a circular include of Renderer3D.hpp.
class LightBaker3D; // Forward declaration, to avoid a circular include of LightBaker3D.hpp.

/**
 * Configuration options for a reflection probe in a ReflectionProbes3D set.
 */
struct ReflectionProbeOptions3D {
	/**
	 * Center position of the reflection probe box in world space.
	 */
	vec3 center;

	/**
	 * Orientation of the reflection probe box in world space.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Size of the reflection probe box along each local axis.
	 *
	 * Each component must be positive.
	 */
	vec3 size;

	/**
	 * Offset of the center of the local region of the reflection probe box
	 * where the reflection will be visible.
	 */
	vec3 localAffectedRegionOffset{};

	/**
	 * Size of the local region of the reflection probe box where the reflection
	 * will be visible.
	 *
	 * Each component must be positive.
	 */
	vec3 localAffectedRegionSize;

	/**
	 * Size of the blend padding along each local axis on the local negative
	 * sides of the affected region.
	 *
	 * Each component must be non-negative.
	 */
	vec3 blendWidthsOnNegativeSides{};

	/**
	 * Size of the blend padding along each local axis on the local positive
	 * sides of the affected region.
	 *
	 * Each component must be non-negative.
	 */
	vec3 blendWidthsOnPositiveSides{};

	/**
	 * Offset of the reflection probe's capture position from the center of the
	 * box in world space.
	 */
	vec3 captureOffset{};
};

/**
 * Configuration options for a ReflectionProbes3D set.
 */
struct ReflectionProbes3DOptions {
	/**
	 * Width, in texels, of the reflection maps of reflection probes.
	 *
	 * Must be a power of 2.
	 */
	uint32_t reflectionMapResolution = 256;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ReflectionProbes3DOptions& other) const noexcept = default;
};

/**
 * Set of reflection probes in 3D space.
 */
class ReflectionProbes3D {
public:
	/**
	 * Struct of shader parameters representing the reflection probe atlases.
	 */
	struct AtlasParameters {
		/** Sampler for the reflection maps of the reflection probes. */
		samplerCubeArray reflectionProbesReflectionMaps;

		/** Scale of the reflection probes reflection detail level. */
		float reflectionProbesReflectionMapDetailLevelScale;

		/** Maximum mip level in the reflection maps of the reflection probes. */
		float reflectionProbesReflectionMapDetailLevelMax;
	};

	/**
	 * Shader buffer for reflection probe atlases.
	 */
	using AtlasBuffer = UniformBuffer<AtlasParameters, "ReflectionProbes3DAtlases">;

	/**
	 * Struct of shader fields representing a reflection probe.
	 */
	struct ProbeFields {
		/** Center position of the reflection probe box in world space. */
		vec3 reflectionProbeCenter;

		/** Components of the quaternion representing the orientation of the reflection probe box in world space. */
		vec4 reflectionProbeOrientation;

		/** Size of the reflection probe box along each local axis. */
		vec3 reflectionProbeSize;

		/** Offset of the center of the local region of the reflection probe box where the reflection will be visible. */
		vec3 reflectionProbeLocalAffectedRegionOffset;

		/** Size of the local region of the reflection probe box where the reflection will be visible. */
		vec3 reflectionProbeLocalAffectedRegionSize;

		/** Size of the reflection probe box's blend padding along each local axis on the local negative sides of the affected region. */
		vec3 reflectionProbeBlendWidthsOnNegativeSides;

		/** Size of the reflection probe box's blend padding along each local axis on the local positive sides of the affected region. */
		vec3 reflectionProbeBlendWidthsOnPositiveSides;

		/** Offset of the reflection probe's capture position from the center of the box in world space. */
		vec3 reflectionProbeCaptureOffset;
	};

	/**
	 * Shader buffer for reflection probes.
	 */
	using ProbeBuffer = StorageBuffer<ProbeFields, "LightProbeVolumes3DProbes">;

	/**
	 * Sampler options to use for the reflection maps of reflection probes.
	 */
	static constexpr TextureSamplerOptions REFLECTION_MAPS_SAMPLER_OPTIONS{
		.minificationFilter = TextureFilter::LINEAR,
		.magnificationFilter = TextureFilter::LINEAR,
		.mipmapMode = TextureMipmapMode::LINEAR,
		.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
	};

	/**
	 * Construct an empty set of reflection probes.
	 *
	 * \param device device to create the set for. Must outlive the set.
	 * \param options reflection probe set options, see
	 *        ReflectionProbes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) explicit ReflectionProbes3D(Device& device, const ReflectionProbes3DOptions& options = {});

	/**
	 * Construct a pre-baked set of reflection probes.
	 *
	 * \param device device to create the set for. Must outlive the set.
	 * \param reflectionMaps pre-baked reflection maps for the reflection
	 *        probes. Must be a valid sampled cube array texture in an RGB(A)
	 *        color format, or empty if there are no probes. The texture's
	 *        sampler options should be
	 *        ReflectionProbes3D::REFLECTION_MAPS_SAMPLER_OPTIONS.
	 * \param probeOptions list of reflection probe options.
	 * \param options reflection probe set options, see
	 *        ReflectionProbes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) ReflectionProbes3D(Device& device, Texture reflectionMaps, ArrayList<ReflectionProbeOptions3D> probeOptions, const ReflectionProbes3DOptions& options);

	/**
	 * Remove all reflection probes from the set.
	 */
	GREM_API(graphics_3d) void clearReflectionProbes() noexcept;

	/**
	 * Add a reflection probe to the set.
	 *
	 * \param options reflection probe options, see ReflectionProbeOptions3D.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 */
	GREM_API(graphics_3d) void addReflectionProbe(const ReflectionProbeOptions3D& options);

	/**
	 * Set the reflection probe set to the default empty set with new options.
	 *
	 * \param newOptions reflection probe set options, see
	 *        ReflectionProbes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setReflectionProbes(const ReflectionProbes3DOptions& newOptions);

	/**
	 * Assign a pre-baked set of reflection probes to the reflection probe set.
	 *
	 * \param newReflectionMaps pre-baked reflection maps for the reflection
	 *        probes. Must be a valid sampled cube array texture in an RGB(A)
	 *        color format, or empty if there are no probes.
	 * \param newProbeOptions list of reflection probe options.
	 * \param newOptions reflection probe set options, see
	 *        ReflectionProbes3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setReflectionProbes(Texture newReflectionMaps, ArrayList<ReflectionProbeOptions3D> newProbeOptions, const ReflectionProbes3DOptions& newOptions);

	/**
	 * Get the reflection maps of the reflection probes.
	 *
	 * \return a read-only reference to the reflection maps cube array texture,
	 *         which will be empty if the reflection probes have not been baked
	 *         since their last modification.
	 */
	[[nodiscard]] const Texture& getReflectionMaps() const noexcept {
		return reflectionMaps;
	}

	/**
	 * Get the configuration options for the reflection probes in the set.
	 *
	 * \return a read-only view over the list of reflection probe options, valid
	 *         until the next modification to the reflection probe set.
	 */
	[[nodiscard]] Span<const ReflectionProbeOptions3D> getProbeOptions() const noexcept {
		return probeOptions;
	}

	/**
	 * Get the configuration options of the reflection probe set.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] ReflectionProbes3DOptions getOptions() const noexcept {
		return options;
	}

private:
	friend Renderer3D;
	friend LightBaker3D;

	GREM_API(graphics_3d) void flushProbesAndTextures(Device& device) const;
	GREM_API(graphics_3d) void flush(Device& device, Renderer3D& renderer3D) const;

	ReflectionProbes3DOptions options;
	ArrayList<ReflectionProbeOptions3D> probeOptions{};
	mutable Texture reflectionMaps{};
	mutable Buffer<ProbeFields> probes{};
	mutable Buffer<float> worldSpaceBoxVolumes{};
	mutable AtlasBuffer atlasBuffer;
	mutable ProbeBuffer probeBuffer;
	mutable bool probesDirty = false;
	mutable bool texturesDirty = false;
	mutable bool buffersDirty = true;
};

} // namespace grem::graphics

#endif
