// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_FOG_3D_HPP
#define GREM_GRAPHICS_3D_FOG_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer3D; // Forward declaration, to avoid a circular include of Renderer3D.hpp.

/**
 * Configuration options for a Fog3D.
 */
struct Fog3DOptions {
	/**
	 * Distance from the camera's position to where visible fog starts.
	 *
	 * Must be less than #endDistance.
	 */
	float startDistance = 0.0f;

	/**
	 * Distance from the camera's position to where fog reaches its maximum
	 * density.
	 *
	 * Must be greater than #startDistance.
	 */
	float endDistance = 1000.0f;

	/**
	 * Color of the fog, where the alpha component represents the fraction of
	 * the maximum density reached by the fog at its end distance.
	 */
	Color color = Color::INVISIBLE;

	/**
	 * Direction in which the fog gradually fades out to reveal the skybox
	 * according to #skyFadeMinAngle and #skyFadeMaxAngle (typically towards the
	 * sky).
	 */
	vec3 skyFadeDirection{0.0f, 1.0f, 0.0f};

	/**
	 * Angle from #skyFadeDirection, in radians, at which fog starts to appear
	 * in the skybox.
	 * 
	 * \note If this is greater than or equal to #skyFadeMaxAngle, sky fog is
	 *       disabled.
	 */
	float skyFadeMinAngle = 1.4835299f; // 85 degrees.

	/**
	 * Angle from #skyFadeDirection, in radians, at which fog reaches its
	 * maximum density in the skybox.
	 *
	 * \note If this is less than or equal to #skyFadeMinAngle, directional fade
	 *       is disabled.
	 */
	float skyFadeMaxAngle = 1.5707963f; // 90 degrees.

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Fog3DOptions& other) const noexcept = default;
};

/**
 * Distance-based fog in 3D space.
 */
class Fog3D {
public:
	/** Struct of shader parameters representing the fog's appearance. */
	struct Parameters {
		/**
		 * Distance from the camera's position to where visible fog starts (X)
		 * and to where it reaches its maximum density (Y), followed by the
		 * cosines of the angles from #fogSkyFadeDirection at which fog starts
		 * to appear in the skybox (Z), and where it reaches its maximum density
		 * (W).
		 * 
		 * \note If the max angle cosine is greater than or equal to the min
		 *       angle cosine, sky fog is disabled.
		 */
		vec4 fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines;

		/**
		 * Color (XYZ) and fraction of maximum density (W) reached by fog at its
		 * end distance.
		 */
		vec4 fogColorAndMaxDensity;

		/**
		 * Direction in which the fog gradually fades out to reveal the skybox
		 * (typically towards the sky).
		 */
		vec3 fogSkyFadeDirection;
	};

	/** Shader buffer for fog parameters. */
	using ParameterBuffer = UniformBuffer<Parameters, "Fog3DParameters">;

	/**
	 * Construct a fog.
	 *
	 * \param device device to create the fog for. Must outlive the fog.
	 * \param options fog options, see Fog3DOptions.
	 */
	GREM_API(graphics_3d) explicit Fog3D(Device& device, const Fog3DOptions& options = {});

	/**
	 * Set the fog parameters.
	 *
	 * \param newFogOptions fog options, see Fog3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setFog(const Fog3DOptions& newFogOptions);

	/**
	 * Get the configuration options of the fog.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] Fog3DOptions getOptions() const noexcept {
		return options;
	}

private:
	friend Renderer3D;

	GREM_API(graphics_3d) void flush() const;

	Fog3DOptions options;
	mutable ParameterBuffer parameterBuffer;
	mutable bool parameterBufferDirty = true;
};

} // namespace grem::graphics

#endif
