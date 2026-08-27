// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_CUBEMAP_3D_HPP
#define GREM_GRAPHICS_3D_CUBEMAP_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

/**
 * Container for a cubemap mesh stored on the GPU.
 */
class Cubemap3D {
public:
	/** Data layout for the attributes of a single vertex of a cubemap mesh. */
	struct Vertex {
		vec3 vertexPosition; ///< Position relative to the origin of the cube.
	};

	/** Data type used in the index buffer of a cubemap mesh. */
	using Index = uint16_t;

	/** Cubemap mesh primitive type. */
	using Mesh = graphics::Mesh<Vertex, Index>;

	/** Struct of default vertex shader constants. */
	struct VertexShaderConstants {};

	/** Struct of fields output by the default vertex shader. */
	struct VertexShaderOutputs {
		vec3 fragmentTextureCoordinates; ///< Cube texture coordinates of the vertex.
	};

	/** Struct of fields output by the default fragment shader. */
	struct FragmentShaderOutputs {
		vec4 outputColor; ///< Final fragment color.
	};

	/** Struct of shader parameters representing the perspective viewing the cubemap. */
	struct PerspectiveParameters {
		mat4 cubemapViewProjectionMatrix; ///< Combined view-projection matrix of the view perspective.
	};

	/** Shader buffer for cubemap perspective parameters. */
	using PerspectiveBuffer = UniformBuffer<PerspectiveParameters, "Cubemap3DPerspective">;

	/** Default vertex shader for drawing cubemaps. */
	using VertexShader = graphics::VertexShader<Mesh, VertexShaderConstants, VertexShaderOutputs, PerspectiveBuffer>;

	/** Shader pipeline for drawing cubemaps. */
	using ShaderPipeline = graphics::ShaderPipeline<Mesh>;

	/** Default cubemap vertex shader constants. */
	static constexpr VertexShaderConstants DEFAULT_VERTEX_SHADER_CONSTANTS{};

	/** Default cubemap graphics pipeline configuration. */
	static constexpr ShaderPipelineOptions DEFAULT_SHADER_PIPELINE_OPTIONS{
		.depthBufferMode = DepthBufferMode::NONE,
		.faceCullingMode = FaceCullingMode::NONE,
	};

	/** Projection matrix for rendering a side of a cubemap from its center. */
	static constexpr mat4 SIDE_PROJECTION_MATRIX{
		// clang-format off
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  1.0f,  0.0f,  0.0f,
		0.0f,  0.0f, -1.0f, -1.0f,
		0.0f,  0.0f,  0.0f,  0.0f,
		// clang-format on
	};

	/** Camera forward directions for rendering each side of an axis-aligned cubemap. */
	static constexpr Array SIDE_FORWARD_DIRECTIONS{
		vec3{1.0f, 0.0f, 0.0f},
		vec3{-1.0f, 0.0f, 0.0f},
		vec3{0.0f, -1.0f, 0.0f},
		vec3{0.0f, 1.0f, 0.0f},
		vec3{0.0f, 0.0f, 1.0f},
		vec3{0.0f, 0.0f, -1.0f},
	};

	/** Camera up vectors for rendering each side of an axis-algined cubemap. */
	static constexpr Array SIDE_UP_DIRECTIONS{
		vec3{0.0f, -1.0f, 0.0f},
		vec3{0.0f, -1.0f, 0.0f},
		vec3{0.0f, 0.0f, -1.0f},
		vec3{0.0f, 0.0f, 1.0f},
		vec3{0.0f, -1.0f, 0.0f},
		vec3{0.0f, -1.0f, 0.0f},
	};

	/**
	 * Get an array of view matrices for rendering each side of an axis-aligned
	 * cubemap from its center at a specific position.
	 *
	 * \param position center of the cubemap.
	 *
	 * \return an array of 6 view matrices, one for each side of the cube,
	 *         corresponding to #SIDE_FORWARD_DIRECTIONS and
	 *         #SIDE_UP_DIRECTIONS.
	 */
	[[nodiscard]] static Array<mat4, 6> getSideViewMatrices(vec3 position) noexcept {
		return {
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[0], SIDE_UP_DIRECTIONS[0]),
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[1], SIDE_UP_DIRECTIONS[1]),
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[2], SIDE_UP_DIRECTIONS[2]),
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[3], SIDE_UP_DIRECTIONS[3]),
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[4], SIDE_UP_DIRECTIONS[4]),
			lookAt(position, position + SIDE_FORWARD_DIRECTIONS[5], SIDE_UP_DIRECTIONS[5]),
		};
	}

	/**
	 * Construct a unit cubemap.
	 *
	 * \param device device to create the cubemap for. Must outlive the cubemap.
	 *
	 * \throws graphics::Error on failure to create the cubemap.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit Cubemap3D(Device& device)
		: mesh(device, MESH_VERTICES, MESH_INDICES) {}

	/**
	 * Get the cubemap mesh.
	 *
	 * \return a read-only reference to the cubemap mesh.
	 */
	[[nodiscard]] const Mesh& getMesh() const noexcept {
		return mesh;
	}

private:
	GREM_API(graphics_3d) static const Array<Vertex, 8> MESH_VERTICES;
	GREM_API(graphics_3d) static const Array<Index, 36> MESH_INDICES;

	Mesh mesh;
};

} // namespace grem::graphics

#endif
