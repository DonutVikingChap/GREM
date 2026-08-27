// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_MODEL_2D_HPP
#define GREM_GRAPHICS_2D_MODEL_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Camera2D.hpp>

#include <utility> // std::move

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer2D; // Forward declaration, to avoid a circular include of Renderer2D.hpp.

/**
 * Container for a set of 2D triangle strip meshes stored on the GPU.
 */
class Model2D {
public:
	/**
	 * Data layout for the attributes of a single vertex of a 2D model mesh.
	 *
	 * Meets the requirements of the grem::graphics::mesh_vertex concept.
	 */
	struct Vertex {
		vec2 vertexPosition;           ///< Position relative to the model origin.
		vec2 vertexTextureCoordinates; ///< Texture UV coordinates that map to this vertex.
	};

	/**
	 * Data layout for the attributes of a single instance of a 2D model mesh.
	 *
	 * Meets the requirements of the grem::graphics::mesh_instance concept.
	 */
	struct Instance {
		vec2 instancePosition;      ///< Offset to apply to the vertex positions.
		mat2 instanceBasis;         ///< Basis transformation to apply to the vertex positions.
		vec2 instanceTextureOffset; ///< Offset to apply to the texture coordinates before sampling the texture.
		mat2 instanceTextureBasis;  ///< Basis transformation to apply to the texture coordinates before sampling the texture.
		vec4 instanceTintColor;     ///< Multiplicative tint color to use when rendering.
		vec3 instanceEmissiveColor; ///< Additive emissive color to use when rendering.
	};

	/**
	 * Type of a single mesh of a 2D model.
	 */
	using Mesh = graphics::Mesh<Vertex, NoIndex, NoParameters, Instance>;

	/**
	 * Struct of default vertex shader constants.
	 */
	struct VertexShaderConstants {};

	/**
	 * Struct of fields output by the default vertex shader.
	 */
	struct VertexShaderOutputs {
		vec2 fragmentTextureCoordinates; ///< Texture coordinates of the vertex.
		vec4 fragmentTintColor;          ///< Tint color of the vertex.
		vec3 fragmentEmissiveColor;      ///< Emissive color of the vertex.
	};

	/**
	 * Struct of default fragment shader constants.
	 */
	struct FragmentShaderConstants {};

	/**
	 * Struct of fields output by the default fragment shader.
	 */
	struct FragmentShaderOutputs {
		vec4 outputColor; ///< Final fragment color.
	};

	/**
	 * Struct of shader parameters representing the texture of a 2D model.
	 */
	struct TextureParameters {
		sampler2D mainTexture; ///< Texture to use when rendering the model.
	};

	/**
	 * Shader buffer for 2D model texture parameters.
	 */
	using TextureBuffer = UniformBuffer<TextureParameters, "Model2DTexture">;

	/**
	 * Struct of shader fields representing the 3D transformation of a 2D model.
	 */
	struct Transformation3DParameters {
		mat4 transformation3DTransformation; ///< Transformation of the model instance.
	};

	/**
	 * Shader buffer for 2D model 3D transformation parameters.
	 */
	using Transformation3DBuffer = UniformBuffer<Transformation3DParameters, "Model2DTransformation3D">;

	/**
	 * Vertex shader for drawing 2D models.
	 *
	 * \tparam Constants user-defined aggregate type of external constants that
	 *         are declared in the shader.
	 * \tparam Outputs user-defined aggregate type of fields that are declared
	 *         in the shader and passed to the next pipeline stage.
	 * \tparam ExtraBuffers list of buffers that the shader reads from in
	 *         addition to the required 2D model buffers.
	 */
	template <typename Constants, typename Outputs, typename... ExtraBuffers>
	using VertexShaderBase = graphics::VertexShader<Mesh, Constants, Outputs, Camera2D::ParameterBuffer, ExtraBuffers...>;

	/**
	 * Default vertex shader for drawing 2D models.
	 */
	using VertexShader = VertexShaderBase<VertexShaderConstants, VertexShaderOutputs>;

	/**
	 * Fragment shader for drawing 2D models.
	 *
	 * \tparam Inputs user-defined aggregate type of fields that are passed into
	 *         the shader from the previous pipeline stage.
	 * \tparam Constants user-defined aggregate type of external constants that
	 *         are declared in the shader.
	 * \tparam Outputs user-defined aggregate type of fields that are declared
	 *         in the shader and passed to the next pipeline stage.
	 * \tparam ExtraBuffers list of buffers that the shader reads from in
	 *         addition to the required 2D model buffers.
	 */
	template <typename Inputs, typename Constants, typename Outputs, typename... ExtraBuffers>
	using FragmentShaderBase = graphics::FragmentShader<Mesh, Inputs, Constants, Outputs, Camera2D::ParameterBuffer, ExtraBuffers..., TextureBuffer>;

	/**
	 * Default fragment shader type for drawing 2D models.
	 */
	using FragmentShader = FragmentShaderBase<VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs>;

	/**
	 * Shader pipeline for drawing 2D models.
	 */
	using ShaderPipeline = graphics::ShaderPipeline<Mesh>;

	/**
	 * Node of a 2D model.
	 */
	struct Node {
		/** Mesh primitive of the node. */
		Mesh mesh;

		/** Local bounding box of the node. */
		Box<2, float> boundingBox;

		/** Local bounding radius of the node. */
		float boundingRadius;
	};

	/**
	 * Default 2D model vertex shader constants.
	 */
	static constexpr VertexShaderConstants DEFAULT_VERTEX_SHADER_CONSTANTS{};

	/**
	 * Default 2D model fragment shader constants.
	 */
	static constexpr FragmentShaderConstants DEFAULT_FRAGMENT_SHADER_CONSTANTS{};

	/**
	 * Default 2D model graphics pipeline configuration.
	 */
	static constexpr ShaderPipelineOptions DEFAULT_SHADER_PIPELINE_OPTIONS{
		.depthBufferMode = DepthBufferMode::NONE,
		.primitiveType = PrimitiveType::TRIANGLE_STRIP,
		.faceCullingMode = FaceCullingMode::NONE,
		.blendState = BlendState::ALPHA_BLENDING_PREMULTIPLIED,
	};

	/**
	 * Construct an invalid model that must be reassigned before use.
	 */
	Model2D() noexcept = default;

	/**
	 * Construct a model from a list of nodes.
	 *
	 * \param device device to create the model for. Must outlive the model.
	 * \param renderer2D renderer to create the model for. Must be associated with
	 *        the given device. Must outlive the model.
	 * \param nodes nodes that define the model. Must not be empty.
	 */
	Model2D(Device& device, Renderer2D& renderer2D, ArrayList<Node> nodes) noexcept
		: nodes(std::move(nodes)) {
		GREM_ASSERT(!this->nodes.empty());

		(void)device;
		(void)renderer2D;

		boundingBox = this->nodes.front().boundingBox;
		boundingRadius = this->nodes.front().boundingRadius;
		for (const Node& node : Span{nodes}.subspan(1)) {
			boundingBox.min = min(boundingBox.min, node.boundingBox.min);
			boundingBox.max = max(boundingBox.max, node.boundingBox.max);
			boundingRadius = max(boundingRadius, node.boundingRadius);
		}
	}

	/**
	 * Construct a model with a single mesh from a list of vertices.
	 *
	 * \param device device to create the model for. Must outlive the model.
	 * \param renderer2D renderer to create the model for. Must be associated with
	 *        the given device. Must outlive the model.
	 * \param vertices vertices that define the mesh of the model.
	 *
	 * \throws graphics::Error on failure to create the model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Model2D(Device& device, Renderer2D& renderer2D, Span<const Vertex> vertices) {
		(void)renderer2D;

		Box<2, float> meshBoundingBox{};
		float meshBoundingRadius = 0.0f;
		if (!vertices.empty()) {
			meshBoundingBox = {.min = vertices.front().vertexPosition, .max = vertices.front().vertexPosition};
			float meshBoundingRadiusSquared = length2(vertices.front().vertexPosition);
			for (const Vertex& vertex : vertices.subspan(1)) {
				meshBoundingBox.min = min(meshBoundingBox.min, vertex.vertexPosition);
				meshBoundingBox.max = max(meshBoundingBox.max, vertex.vertexPosition);
				meshBoundingRadiusSquared = max(meshBoundingRadiusSquared, length2(vertex.vertexPosition));
			}
			meshBoundingRadius = sqrt(meshBoundingRadiusSquared);
		}

		nodes.push_back(Node{.mesh = Mesh{device, vertices}, .boundingBox = meshBoundingBox, .boundingRadius = meshBoundingRadius});
		boundingBox = meshBoundingBox;
		boundingRadius = meshBoundingRadius;
	}

	/**
	 * Get the nodes of the model.
	 *
	 * \return a read-only view over the nodes of the model.
	 */
	[[nodiscard]] Span<const Node> getNodes() const noexcept {
		return nodes;
	}

	/**
	 * Get the local bounding box of the model.
	 *
	 * \return the axis-aligned bounding box of the model, relative to its
	 *         origin.
	 */
	[[nodiscard]] Box<2, float> getBoundingBox() const noexcept {
		return boundingBox;
	}

	/**
	 * Get the local bounding radius of the model.
	 *
	 * \return the maximum vertex position distance from the model origin.
	 */
	[[nodiscard]] float getBoundingRadius() const noexcept {
		return boundingRadius;
	}

private:
	ArrayList<Node> nodes{};
	Box<2, float> boundingBox{};
	float boundingRadius = 0.0f;
};

} // namespace grem::graphics

#endif
