// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_SKY_3D_HPP
#define GREM_GRAPHICS_3D_SKY_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/Cubemap3D.hpp>
#include <GREM/graphics_3d/Fog3D.hpp>

namespace grem::graphics {

class Device;       // Forward declaration, to avoid including Device.hpp.
class Renderer3D;   // Forward declaration, to avoid a circular include of Renderer3D.hpp.
class LightBaker3D; // Forward declaration, to avoid a circular include of LightBaker3D.hpp.

/**
 * Configuration options for a Sky3D.
 */
struct Sky3DOptions {
	/**
	 * Tint color of the skybox.
	 */
	Color color = Color::WHITE;

	/**
	 * Ambient light tint color produced by the skybox.
	 */
	Color ambientColor = Color::WHITE;

	/**
	 * Reflection tint color produced by the skybox.
	 */
	Color reflectionColor = Color::WHITE;

	/**
	 * Desired width, in texels, of the generated radiance cubemap, or 0 to
	 * choose an appropriate resolution automatically based on the size of the
	 * source image or texture.
	 * 
	 * Must be 0 or a power of 2.
	 */
	uint32_t radianceMapResolution = 0;

	/**
	 * Desired width, in texels, of the generated irradiance cubemap, or 0 to
	 * disable diffuse irradiance from the sky.
	 * 
	 * Must be 0 or a power of 2.
	 */
	uint32_t irradianceMapResolution = 16;

	/**
	 * Desired width, in texels, of the generated reflection cubemap, or 0 to
	 * disable specular reflections from the sky.
	 * 
	 * Must be 0 or a power of 2.
	 */
	uint32_t reflectionMapResolution = 256;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Sky3DOptions& other) const noexcept = default;
};

/**
 * Skybox in 3D space.
 */
class Sky3D {
public:
	/** Type of a skybox mesh. */
	using Mesh = Cubemap3D::Mesh;

	/** Struct of default vertex shader constants. */
	using VertexShaderConstants = Cubemap3D::VertexShaderConstants;

	/** Struct of fields output by the default vertex shader. */
	using VertexShaderOutputs = Cubemap3D::VertexShaderOutputs;

	/** Struct of default fragment shader constants. */
	struct FragmentShaderConstants {
		bool32_t SKY_HDR;
	};

	/** Struct of fields output by the default fragment shader. */
	using FragmentShaderOutputs = Cubemap3D::FragmentShaderOutputs;

	/** Struct of shader parameters representing the sky's appearance. */
	struct Parameters {
		/** Sampler for the skybox radiance map. */
		samplerCube skyRadianceMap;

		/** Sampler for the skybox irradiance map. */
		samplerCube skyIrradianceMap;

		/** Sampler for the skybox reflection map. */
		samplerCube skyReflectionMap;

		/** Tint color of the skybox. */
		vec4 skyColor;

		/** Tint color of the ambient light produced by the skybox. */
		vec4 skyAmbientColor;

		/** Tint color of the reflections produced by the skybox. */
		vec4 skyReflectionColor;

		/** Scale of the skybox reflection detail level. */
		float skyReflectionMapDetailLevelScale;

		/** Maximum mip level in the skybox reflection map. */
		float skyReflectionMapDetailLevelMax;
	};

	/** Shader buffer for sky parameters. */
	using ParameterBuffer = UniformBuffer<Parameters, "Sky3DParameters">;

	/** Shader pipeline for drawing skyboxes. */
	using ShaderPipeline = graphics::ShaderPipeline<Mesh>;

	/** Default skybox vertex shader constants. */
	static constexpr VertexShaderConstants DEFAULT_VERTEX_SHADER_CONSTANTS{};

	/** Default skybox fragment shader constants. */
	static constexpr FragmentShaderConstants DEFAULT_FRAGMENT_SHADER_CONSTANTS{
		.SKY_HDR = false,
	};

	/** Default skybox graphics pipeline configuration. */
	static constexpr ShaderPipelineOptions DEFAULT_SHADER_PIPELINE_OPTIONS{
		.depthTestPredicate = DepthTestPredicate::LESS_OR_EQUAL,
		.faceCullingMode = FaceCullingMode::NONE,
	};

	/**
	 * Construct a sky.
	 *
	 * \param device device to create the sky for. Must outlive the sky.
	 * \param options sky options, see Sky3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) explicit Sky3D(Device& device, const Sky3DOptions& options = {});

	/**
	 * Construct a pre-baked sky.
	 *
	 * \param device device to create the sky for. Must outlive the sky.
	 * \param radianceMap pre-baked radiance map for sky color. Must be a valid
	 *        sampled cube texture in an RGB(A) color format, or empty.
	 * \param irradianceMap pre-baked irradiance map for sky lighting. Must be a
	 *        valid sampled cube texture in an RGB(A) color format, or empty.
	 * \param reflectionMap pre-baked reflection map for sky reflections. Must
	 *        be a valid sampled cube texture in an RGB(A) color format, or
	 *        empty.
	 * \param options sky options, see Sky3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) Sky3D(Device& device, Texture radianceMap, Texture irradianceMap, Texture reflectionMap, const Sky3DOptions& options);

	/**
	 * Set the sky to the default invisible texture with new options.
	 *
	 * \param newOptions sky options, see Sky3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setSky(const Sky3DOptions& newOptions);

	/**
	 * Set the sky to a pre-baked sky.
	 *
	 * \param newRadianceMap pre-baked radiance map for sky color. Must be a
	 *        valid sampled cube texture in an RGB(A) color format with straight
	 *        alpha, or empty.
	 * \param newIrradianceMap pre-baked irradiance map for sky lighting. Must
	 *        be a valid sampled cube texture in an RGB(A) color format with
	 *        straight alpha, or empty.
	 * \param newReflectionMap pre-baked reflection map for sky reflections.
	 *        Must be a valid sampled cube texture in an RGB(A) color format
	 *        with straight alpha, or empty.
	 * \param newOptions sky options, see Sky3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void setSky(Texture newRadianceMap, Texture newIrradianceMap, Texture newReflectionMap, const Sky3DOptions& newOptions);

	/**
	 * Get the radiance map of the sky.
	 *
	 * \return a read-only reference to the radiance map cube texture, which
	 *         will be empty if the sky has not been baked since its last
	 *         modification.
	 */
	[[nodiscard]] const Texture& getRadianceMap() const noexcept {
		return radianceMap;
	}

	/**
	 * Get the irradiance map of the sky.
	 *
	 * \return a read-only reference to the irradiance map cube texture, which
	 *         will be empty if the sky has not been baked since its last
	 *         modification.
	 */
	[[nodiscard]] const Texture& getIrradianceMap() const noexcept {
		return irradianceMap;
	}

	/**
	 * Get the reflection map of the sky.
	 *
	 * \return a read-only reference to the reflection map cube texture, which
	 *         will be empty if the sky has not been baked since its last
	 *         modification.
	 */
	[[nodiscard]] const Texture& getReflectionMap() const noexcept {
		return reflectionMap;
	}

	/**
	 * Get the configuration options of the sky.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] Sky3DOptions getOptions() const noexcept {
		return options;
	}

private:
	friend Renderer3D;
	friend LightBaker3D;

	GREM_API(graphics_3d) void flush(Renderer3D& renderer3D) const;

	Sky3DOptions options;
	Texture radianceMap{};
	Texture irradianceMap{};
	Texture reflectionMap{};
	mutable ParameterBuffer parameterBuffer;
	mutable bool parameterBufferDirty = true;
};

/**
 * Filter parameters that determine which parts of a sky are drawn.
 */
struct Sky3DFilter {
	/**
	 * If set, the sky will not be directly rendered, but will still contribute
	 * to the lighting of other rendered objects.
	 */
	bool skipSkyRendering = false;
};

/**
 * View over a sky to be drawn in a Renderer3D frame.
 */
struct Sky3DView {
	/** Non-owning read-only pointer to the sky. Must not be nullptr. */
	const Sky3D* sky;

	/** Shader pipeline override to use, or nullptr to not use a shader override. */
	SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle{};

	/** Filter parameters that determine which parts of the sky to draw. */
	Sky3DFilter filter;

	/**
	 * Construct a sky view.
	 *
	 * \param sky sky to reference. Must outlive the view.
	 * \param filter filter parameters, see Sky3DFilter.
	 */
	Sky3DView(const Sky3D& sky, const Sky3DFilter& filter = {})
		: sky(&sky)
		, filter(filter) {}

	/**
	 * Construct a sky view with a shader override.
	 *
	 * \param sky sky to reference. Must outlive the view.
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param filter filter parameters, see Sky3DFilter.
	 */
	Sky3DView(const Sky3D& sky, const Sky3D::ShaderPipeline& shaderPipelineOverride, const Sky3DFilter& filter = {})
		: sky(&sky)
		, shaderPipelineOverrideHandle(shaderPipelineOverride.lock())
		, filter(filter) {}
};

} // namespace grem::graphics

#endif
