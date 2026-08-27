// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_MODEL_3D_HPP
#define GREM_GRAPHICS_3D_MODEL_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Function.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/RangeAllocator.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/resource/Model.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash
#include <utility>    // std::move, std::forward

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer3D; // Forward declaration, to avoid a circular include of Renderer3D.hpp.

/**
 * Configuration options for a Model3D.
 */
struct Model3DOptions {
	/**
	 * Maximum level of anisotropic filtering to use when sampling material
	 * textures.
	 *
	 * Set to 1 or lower to disable anisotropic filtering.
	 */
	float maxTextureSamplerAnisotropy = 16.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Model3DOptions& other) const = default;
};

/**
 * Container for a set of 3D triangle mesh nodes stored on the GPU.
 */
class Model3D {
public:
	/**
	 * Material type that specifies how the material parameters should be
	 * interpreted when shading a mesh instance of a 3D model.
	 */
	enum class MaterialType : uint8_t {
		METALLIC_ROUGHNESS = 0, ///< The shader should interpret the material according to the standard metallic-roughness model.
		UNLIT = 1,              ///< The shader should use only the base color at full brightness.
	};

	/**
	 * Configuration options for shading a mesh instance of a 3D model.
	 */
	struct ShaderConfiguration {
		/**
		 * Hasher for a ShaderConfiguration.
		 */
		class Hash {
		public:
			[[nodiscard]] size_t operator()(const grem::graphics::Model3D::ShaderConfiguration& shaderConfiguration) const {
				static_assert(sizeof(shaderConfiguration.vertexFlags) == 2);
				static_assert(sizeof(shaderConfiguration.fragmentFlags) == 2);
				return hasher(static_cast<uint32_t>(shaderConfiguration.vertexFlags << 16) | shaderConfiguration.fragmentFlags);
			}

		private:
			[[no_unique_address]] std::hash<uint32_t> hasher;
		};

		/**
		 * How to interpret the material parameters when shading.
		 */
		MaterialType materialType = MaterialType::METALLIC_ROUGHNESS;

		/**
		 * How to form primitives out of consecutive mesh vertices.
		 */
		PrimitiveType primitiveType = PrimitiveType::TRIANGLES;

		/**
		 * The winding order of front-facing faces.
		 */
		FrontFace frontFace = FrontFace::COUNTERCLOCKWISE;

		/**
		 * Vertex flags used by the shader, see VertexFlag.
		 */
		resource::Model::VertexFlags vertexFlags{};

		/**
		 * Fragment flags used by the shader, see FragmentFlag.
		 */
		resource::Model::FragmentFlags fragmentFlags{};

		/**
		 * Compare this configuration to another for equality.
		 *
		 * \param other the configuration to compare this one to.
		 *
		 * \return true if the configurations are equal, false otherwise.
		 */
		[[nodiscard]] constexpr bool operator==(const ShaderConfiguration& other) const = default;
	};

	/**
	 * Data layout for the attributes of a single vertex of a 3D model mesh.
	 *
	 * Meets the requirements of the grem::graphics::mesh_vertex concept.
	 */
	struct Vertex {
		/**
		 * Bind-pose position relative to the origin of the model.
		 */
		vec3 vertexPosition;

		/**
		 * Unit vector (XYZ) pointing away from the vertex surface in the bind
		 * pose of the model.
		 */
		iA2B10G10R10vec4norm vertexNormal;

		/**
		 * Unit vector (XYZ) pointing in some direction along the vertex surface
		 * in the bind pose of the model, along with a sign component (W)
		 * indicating the handedness of the tangent basis.
		 */
		iA2B10G10R10vec4norm vertexTangent;

		/**
		 * Texture UV coordinates that map to this vertex on channel 0.
		 */
		vec2 vertexTextureCoordinatesChannel0;

		/**
		 * Texture UV coordinates that map to this vertex on channel 1.
		 */
		vec2 vertexTextureCoordinatesChannel1;

		/**
		 * Vertex tint color.
		 */
		u8vec4norm vertexColor;

		/**
		 * Indices of the 4 joints that this vertex is skinned by.
		 */
		u8vec4 vertexJointIndices;

		/**
		 * Weights of the 4 joints that this vertex is skinned by.
		 */
		u8vec4norm vertexJointWeights;
	};

	/**
	 * Data type used in the index buffer of a 3D model mesh.
	 *
	 * Meets the requirements of the grem::graphics::mesh_index concept.
	 */
	using Index = uint32_t;

	/**
	 * Parameters of a 3D model mesh.
	 */
	struct Parameters {
		vec4 meshBaseColorFactor;                            ///< Base color factor.
		vec3 meshOcclusionRoughnessMetallicFactor;           ///< Occlusion strength (X), roughness factor (Y) and metallic factor (Z).
		float meshNormalScale;                               ///< Normal scale.
		vec3 meshEmissiveFactor;                             ///< Emissive factor.
		float meshAlphaCutoff;                               ///< Alpha cutoff value.
		float meshIndexOfRefraction;                         ///< Index of refraction.
		sampler2D meshBaseColorMap;                          ///< Base color texture used for diffuse shading.
		sampler2D meshOcclusionRoughnessMetallicMap;         ///< ORM texture used for specular shading.
		sampler2D meshNormalMap;                             ///< Normal texture used for normal mapping.
		sampler2D meshEmissiveMap;                           ///< Emissive texture used for emissive mapping.
		vec2 meshBaseColorMapTextureOffset;                  ///< Texture coordinate offset to use when sampling the base color map.
		mat2 meshBaseColorMapTextureBasis;                   ///< Texture coordinate basis to use when sampling the base color map.
		vec2 meshOcclusionRoughnessMetallicMapTextureOffset; ///< Texture coordinate offset to use when sampling the ORM map.
		mat2 meshOcclusionRoughnessMetallicMapTextureBasis;  ///< Texture coordinate basis to use when sampling the ORM map.
		vec2 meshNormalMapTextureOffset;                     ///< Texture coordinate offset to use when sampling the normal map.
		mat2 meshNormalMapTextureBasis;                      ///< Texture coordinate basis to use when sampling the normal map.
		vec2 meshEmissiveMapTextureOffset;                   ///< Texture coordinate offset to use when sampling the emissive map.
		mat2 meshEmissiveMapTextureBasis;                    ///< Texture coordinate basis to use when sampling the emissive map.
		uint32_t meshMorphTargetValueOffset;                 ///< Offset of the first morph target in the MorphTargetValueBuffer.
		uint32_t meshMorphTargetCount;                       ///< Number of morph targets.
		uint32_t meshMorphTargetStride;                      ///< Size of each morph target, in number of floats.
	};

	/**
	 * Data layout for the attributes of a single instance of a 3D model mesh.
	 *
	 * Meets the requirements of the grem::graphics::mesh_instance concept.
	 */
	struct Instance {
		vec4 instanceTintColor;                       ///< Multiplicative tint color to use when rendering.
		vec3 instanceEmissiveColor;                   ///< Additive emissive color to use when rendering.
		vec3 instanceEmissiveFactor;                  ///< Emissive factor to use when rendering.
		uint32_t instanceInverseBindPoseMatrixOffset; ///< Offset of the first matrix in the InverseBindPoseMatrixBuffer.
		uint32_t instanceJointOffset;                 ///< Offset of the first joint in the JointBuffer.
		uint32_t instanceMorphTargetWeightOffset;     ///< Offset of the first morph target weight in the MorphTargetWeightBuffer.
		uint32_t instanceInstanceIdentifier;          ///< Instance identifier that may be used in shaders.
	};

	/**
	 * Type of a single mesh of a 3D model.
	 */
	using Mesh = graphics::Mesh<Vertex, Index, Parameters, Instance>;

	/**
	 * Struct of default vertex shader constants.
	 */
	struct VertexShaderConstants {
		bool32_t VERTEX_TEXTURED_ON_CHANNEL_0;
		bool32_t VERTEX_TEXTURED_ON_CHANNEL_1;
		bool32_t VERTEX_COLORED;
		bool32_t VERTEX_SKINNED;
		bool32_t VERTEX_MORPHED_POSITION;
		bool32_t VERTEX_MORPHED_NORMAL;
		bool32_t VERTEX_MORPHED_TANGENT;
		bool32_t VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0;
		bool32_t VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1;
		bool32_t VERTEX_MORPHED_COLOR;
	};

	/**
	 * Struct of fields output by the default vertex shader.
	 */
	struct VertexShaderOutputs {
		vec3 fragmentPosition;                   ///< World-space position of the vertex.
		float fragmentDepth;                     ///< View-space depth of the vertex from the camera.
		vec3 fragmentNormal;                     ///< World-space normal vector of the vertex.
		vec3 fragmentTangent;                    ///< World-space tangent vector of the vertex.
		vec3 fragmentBitangent;                  ///< World-space bitangent vector of the vertex.
		vec2 fragmentTextureCoordinatesChannel0; ///< Texture coordinates of the vertex on channel 0.
		vec2 fragmentTextureCoordinatesChannel1; ///< Texture coordinates of the vertex on channel 1.
		vec4 fragmentTintColor;                  ///< Tint color of the vertex.
		vec3 fragmentEmissiveColor;              ///< Emissive color of the vertex.
		vec3 fragmentEmissiveFactor;             ///< Emissive factor of the vertex.
		uint32_t fragmentInstanceIdentifier;     ///< Instance identifier of the model instance.
	};

	/**
	 * Struct of default fragment shader constants.
	 */
	struct FragmentShaderConstants {
		bool32_t FRAGMENT_HDR;
		bool32_t FRAGMENT_ALPHA_MASKED;
		bool32_t FRAGMENT_ALPHA_BLENDED;
		bool32_t FRAGMENT_DOUBLE_SIDED;
		bool32_t FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
		bool32_t FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1;
		bool32_t FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
		bool32_t FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1;
		bool32_t FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
		bool32_t FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1;
		bool32_t FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
		bool32_t FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1;
		bool32_t FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
		bool32_t FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1;
	};

	/**
	 * Struct of fields output by the default fragment shader.
	 */
	struct FragmentShaderOutputs {
		vec4 outputColor; ///< Final fragment color.
	};

	/**
	 * Struct of shader fields representing the inverse bind matrix of a joint
	 * of a 3D model.
	 */
	struct InverseBindPoseMatrixFields {
		mat4 inverseBindPoseMatrix; ///< Inverse bind-pose matrix of the joint.
	};

	/**
	 * Shader buffer for 3D model inverse bind-pose matrices.
	 */
	using InverseBindPoseMatrixBuffer = StorageBuffer<InverseBindPoseMatrixFields, "Model3DInverseBindPoseMatrices">;

	/**
	 * Struct of shader fields representing the value of a morph target of a 3D
	 * model.
	 */
	struct MorphTargetValueFields {
		float morphTargetValue; ///< Value of the morph target.
	};

	/**
	 * Shader buffer for 3D model morph target values.
	 */
	using MorphTargetValueBuffer = StorageBuffer<MorphTargetValueFields, "Model3DMorphTargetValues">;

	/**
	 * Set of shader buffers related to the static data of 3D models.
	 */
	using DataBuffers = BufferSet<InverseBindPoseMatrixBuffer, MorphTargetValueBuffer>;

	/**
	 * Struct of shader fields representing a joint of a 3D model.
	 */
	struct JointFields {
		mat4 jointMatrix; ///< Transformation matrix of the joint.
	};

	/**
	 * Shader buffer for 3D model joints.
	 */
	using JointBuffer = StorageBuffer<JointFields, "Model3DJoints">;

	/**
	 * Struct of shader fields representing the state of a morph target weight
	 * of a 3D model.
	 */
	struct MorphTargetWeightFields {
		float morphTargetWeight; ///< Weight of the morph target.
	};

	/**
	 * Shader buffer for 3D model morph target weights.
	 */
	using MorphTargetWeightBuffer = StorageBuffer<MorphTargetWeightFields, "Model3DMorphTargetWeights">;

	/**
	 * Set of shader buffers related to the dynamic data of 3D model instances.
	 */
	using InstanceBuffers = BufferSet<JointBuffer, MorphTargetWeightBuffer>;

	/**
	 * Vertex shader for drawing 3D models.
	 *
	 * \tparam Constants user-defined aggregate type of external constants that
	 *         are declared in the shader.
	 * \tparam Outputs user-defined aggregate type of fields that are declared
	 *         in the shader and passed to the next pipeline stage.
	 * \tparam ExtraBuffers list of buffers that the shader reads from in
	 *         addition to the required 3D model buffers.
	 */
	template <typename Constants, typename Outputs, typename... ExtraBuffers>
	using VertexShaderBase = graphics::VertexShader<Mesh, Constants, Outputs, Camera3D::ParameterBuffer, DataBuffers, InstanceBuffers, ExtraBuffers...>;

	/**
	 * Default vertex shader for drawing 3D models.
	 */
	using VertexShader = VertexShaderBase<VertexShaderConstants, VertexShaderOutputs>;

	/**
	 * Fragment shader for drawing 3D models.
	 *
	 * \tparam Inputs user-defined aggregate type of fields that are passed into
	 *         the shader from the previous pipeline stage.
	 * \tparam Constants user-defined aggregate type of external constants that
	 *         are declared in the shader.
	 * \tparam Outputs user-defined aggregate type of fields that are declared
	 *         in the shader and passed to the next pipeline stage.
	 * \tparam ExtraBuffers list of buffers that the shader reads from in
	 *         addition to the required 3D model buffers.
	 */
	template <typename Inputs, typename Constants, typename Outputs, typename... ExtraBuffers>
	using FragmentShaderBase = graphics::FragmentShader<Mesh, Inputs, Constants, Outputs, Camera3D::ParameterBuffer, DataBuffers, InstanceBuffers, ExtraBuffers...>;

	/**
	 * Default fragment shader type for drawing 3D models.
	 */
	using FragmentShader = FragmentShaderBase<VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs>;

	/**
	 * Shader pipeline for drawing 3D models.
	 */
	using ShaderPipeline = graphics::ShaderPipeline<Mesh>;

	/**
	 * Adapter for a function that selects which vertex shader to use for a
	 * given shader configuration.
	 */
	template <typename Constants, typename Outputs, typename... ExtraBuffers>
	class VertexShaderSelectorBase {
	public:
		template <typename F>
		GREM_ALWAYS_INLINE VertexShaderSelectorBase(F&& function)
			requires(constructible_from<Function<SharedPointer<VertexShaderImplementation>(const ShaderConfiguration& shaderConfiguration)>, F>)
			: function(std::forward<F>(function)) {}

		GREM_ALWAYS_INLINE VertexShaderSelectorBase(SharedPointer<VertexShaderImplementation> vertexShaderHandle)
			: function(
				  [vertexShaderHandle = std::move(vertexShaderHandle)](const ShaderConfiguration&) -> SharedPointer<VertexShaderImplementation> { return vertexShaderHandle; }) {}

		template <typename VS>
		GREM_ALWAYS_INLINE VertexShaderSelectorBase(const VS& vertexShader) requires(requires {
			{ vertexShader.lock() } -> convertible_to<SharedPointer<VertexShaderImplementation>>;
		})
			: function([vertexShaderHandle = vertexShader.lock()](const ShaderConfiguration&) -> SharedPointer<VertexShaderImplementation> { return vertexShaderHandle; }) {
			static_assert(same_as<typename VS::mesh_type, Mesh>, "Vertex shader's' mesh type does not match the shader pipeline set.");
			static_assert(same_as<typename VS::constants_type, Constants>, "Vertex shader's constants do not match the shader pipeline set.");
			static_assert(same_as<typename VS::outputs_type, Outputs>, "Vertex shader's outputs do not match the shader pipeline set.");
			static_assert(meta::type_list_starts_with_v<typename VertexShaderBase<Constants, Outputs, ExtraBuffers...>::buffer_types, typename VS::buffer_types>,
				"Vertex shader's buffers do not match the shader pipeline set.");
		}

		[[nodiscard]] GREM_ALWAYS_INLINE SharedPointer<VertexShaderImplementation> operator()(const ShaderConfiguration& shaderConfiguration) const {
			return function(shaderConfiguration);
		}

	private:
		Function<SharedPointer<VertexShaderImplementation>(const ShaderConfiguration& shaderConfiguration)> function;
	};

	/**
	 * Adapter for a function that selects which fragment shader to use for a
	 * given shader configuration.
	 */
	template <typename Inputs, typename Constants, typename Outputs, typename... ExtraBuffers>
	class FragmentShaderSelectorBase {
	public:
		template <typename F>
		GREM_ALWAYS_INLINE FragmentShaderSelectorBase(F&& function)
			requires(constructible_from<Function<SharedPointer<FragmentShaderImplementation>(const ShaderConfiguration& shaderConfiguration)>, F>)
			: function(std::forward<F>(function)) {}

		GREM_ALWAYS_INLINE FragmentShaderSelectorBase(SharedPointer<FragmentShaderImplementation> fragmentShaderHandle)
			: function([fragmentShaderHandle = std::move(fragmentShaderHandle)](const ShaderConfiguration&) -> SharedPointer<FragmentShaderImplementation> {
				return fragmentShaderHandle;
			}) {}

		template <typename FS>
		GREM_ALWAYS_INLINE FragmentShaderSelectorBase(const FS& fragmentShader) requires(requires {
			{ fragmentShader.lock() } -> convertible_to<SharedPointer<FragmentShaderImplementation>>;
		})
			: function([fragmentShaderHandle = fragmentShader.lock()](const ShaderConfiguration&) -> SharedPointer<FragmentShaderImplementation> { return fragmentShaderHandle; }) {
			static_assert(same_as<typename FS::mesh_type, Mesh>, "Fragment shader's' mesh type does not match the shader pipeline set.");
			static_assert(same_as<typename FS::inputs_type, Inputs>, "Fragment shader's outputs do not match the shader pipeline set.");
			static_assert(same_as<typename FS::constants_type, Constants>, "Fragment shader's constants do not match the shader pipeline set.");
			static_assert(same_as<typename FS::outputs_type, Outputs>, "Fragment shader's outputs do not match the shader pipeline set.");
			static_assert(meta::type_list_starts_with_v<typename FragmentShaderBase<Inputs, Constants, Outputs, ExtraBuffers...>::buffer_types, typename FS::buffer_types>,
				"Fragment shader's buffers do not match the shader pipeline set.");
		}

		[[nodiscard]] GREM_ALWAYS_INLINE SharedPointer<FragmentShaderImplementation> operator()(const ShaderConfiguration& shaderConfiguration) const {
			return function(shaderConfiguration);
		}

	private:
		Function<SharedPointer<FragmentShaderImplementation>(const ShaderConfiguration& shaderConfiguration)> function;
	};

	/**
	 * Shader pipeline set for drawing 3D models.
	 *
	 * \tparam VertexShaderConstants user-defined aggregate type of external
	 *         constants that are declared in the vertex shader.
	 * \tparam VertexShaderOutputs user-defined aggregate type of fields that
	 *         are declared in the vertex shader and passed from the vertex
	 *         shader to the fragment shader.
	 * \tparam ExtraVertexShaderBuffers meta::TypeList of buffers that the
	 *         vertex shader reads from in addition to the required 3D model
	 *         buffers.
	 * \tparam FragmentShaderConstants user-defined aggregate type of external
	 *         constants that are declared in the fragment shader.
	 * \tparam FragmentShaderOutputs user-defined aggregate type of fields that
	 *         are declared in the fragment shader and passed to the next
	 *         pipeline stage.
	 * \tparam ExtraFragmentShaderBuffers meta::TypeList of buffers that the
	 *         fragment shader reads from in addition to the required 3D model
	 *         buffers.
	 */
	template <typename VertexShaderConstants, typename VertexShaderOutputs, typename ExtraVertexShaderBuffers, typename FragmentShaderConstants, typename FragmentShaderOutputs,
		typename ExtraFragmentShaderBuffers>
	class ShaderPipelineSetBase;

	template <typename VertexShaderConstants, typename VertexShaderOutputs, typename... ExtraVertexShaderBuffers, typename FragmentShaderConstants, typename FragmentShaderOutputs,
		typename... ExtraFragmentShaderBuffers>
	class ShaderPipelineSetBase<VertexShaderConstants, VertexShaderOutputs, meta::TypeList<ExtraVertexShaderBuffers...>, FragmentShaderConstants, FragmentShaderOutputs,
		meta::TypeList<ExtraFragmentShaderBuffers...>> {
	public:
		/**
		 * Adapter for a function that selects which vertex shader to use for a
		 * given shader configuration.
		 */
		using VertexShaderSelector = VertexShaderSelectorBase<VertexShaderConstants, VertexShaderOutputs, ExtraVertexShaderBuffers...>;

		/**
		 * Adapter for a function that selects which fragment shader to use for
		 * a given shader configuration.
		 */
		using FragmentShaderSelector = FragmentShaderSelectorBase<VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs, ExtraFragmentShaderBuffers...>;

		/**
		 * Construct a shader pipeline set.
		 *
		 * \param device device to create the pipeline set for. Must outlive the
		 *        pipeline set.
		 * \param vertexShaderSelector function that chooses which vertex shader
		 *        to use for each shader configuration.
		 * \param vertexShaderConstantsSelector function that chooses which
		 *        vertex shader constants to use for each shader configuration.
		 * \param fragmentShaderSelector function that chooses which fragment
		 *        shader to use for each shader configuration.
		 * \param fragmentShaderConstantsSelector function that chooses which
		 *        fragment shader constants to use for each shader
		 *        configuration.
		 * \param shaderPipelineOptionsSelector function that chooses which
		 *        pipeline configuration to use for each shader configuration.
		 *
		 * \warning The vertex shader must use the constants specified in the
		 *          VertexShaderConstants template parameter.
		 * \warning The fragment shader must use the constants specified in the
		 *          FragmentShaderConstants template parameter.
		 * \warning The vertex and fragment shaders must use Model3D::Mesh as
		 *          their mesh type.
		 * \warning The fragment shader's input type must match the vertex shader's
		 *          output type.
		 */
		ShaderPipelineSetBase(Device& device, VertexShaderSelector vertexShaderSelector,
			Function<VertexShaderConstants(const ShaderConfiguration& shaderConfiguration)> vertexShaderConstantsSelector, FragmentShaderSelector fragmentShaderSelector,
			Function<FragmentShaderConstants(const ShaderConfiguration& shaderConfiguration)> fragmentShaderConstantsSelector,
			Function<ShaderPipelineOptions(const ShaderConfiguration& shaderConfiguration)> shaderPipelineOptionsSelector)
			: device(&device)
			, vertexShaderSelector(std::move(vertexShaderSelector))
			, vertexShaderConstantsSelector(std::move(vertexShaderConstantsSelector))
			, fragmentShaderSelector(std::move(fragmentShaderSelector))
			, fragmentShaderConstantsSelector(std::move(fragmentShaderConstantsSelector))
			, shaderPipelineOptionsSelector(std::move(shaderPipelineOptionsSelector)) {}

		/**
		 * Get the pipeline specialization corresponding to a specific mesh
		 * configuration.
		 *
		 * The specialization will be compiled if it has not already been
		 * requested, and then cached for future requests.
		 *
		 * \param shaderConfiguration shader configuration to get the shader
		 *        pipeline for.
		 *
		 * \return a read-only reference to the requested shader pipeline, valid
		 *         until the next call to this function, or until the pipeline
		 *         set is destroyed, whichever happens first.
		 *
		 * \throws graphics::Error if resource creation failed, or on failure to
		 *         compile the shader code or link the shader pipeline.
		 * \throws std::length_error if an internal size limit was exceeded.
		 * \throws std::bad_array_new_length if an internal size limit was
		 *         exceeded.
		 * \throws std::bad_alloc on allocation failure.
		 */
		[[nodiscard]] const ShaderPipeline& operator()(const ShaderConfiguration& shaderConfiguration) {
			if (const auto it = specializations.find(shaderConfiguration); it != specializations.end()) {
				[[likely]];
				return it->second;
			}
			return specializations
			    .try_emplace(shaderConfiguration, *device, vertexShaderSelector(shaderConfiguration), vertexShaderConstantsSelector(shaderConfiguration),
					fragmentShaderSelector(shaderConfiguration), fragmentShaderConstantsSelector(shaderConfiguration), shaderPipelineOptionsSelector(shaderConfiguration))
			    .first->second;
		}

	private:
		Device* device;
		VertexShaderSelector vertexShaderSelector;
		Function<VertexShaderConstants(const ShaderConfiguration& shaderConfiguration)> vertexShaderConstantsSelector;
		FragmentShaderSelector fragmentShaderSelector;
		Function<FragmentShaderConstants(const ShaderConfiguration& shaderConfiguration)> fragmentShaderConstantsSelector;
		Function<ShaderPipelineOptions(const ShaderConfiguration& shaderConfiguration)> shaderPipelineOptionsSelector;
		HashMap<ShaderConfiguration, ShaderPipeline, ShaderConfiguration::Hash> specializations{};
	};

	/**
	 * Default shader pipeline set for drawing 3D models.
	 */
	using ShaderPipelineSet = ShaderPipelineSetBase<VertexShaderConstants, VertexShaderOutputs, meta::TypeList<>, FragmentShaderConstants, FragmentShaderOutputs, meta::TypeList<>>;

	/**
	 * Node of a 3D model.
	 */
	struct Node {
		/** Mesh primitive of the node. */
		Mesh mesh;

		/** Shader configuration of the node. */
		ShaderConfiguration shaderConfiguration;

		/** Index of the node's skin's first inverse bind-pose matrix in the model's inverse bind-pose matrix array. */
		uint32_t inverseBindPoseMatrixOffset;

		/** Index of the node's first morph target in the model's morph target weight array. */
		resource::Model::MorphTargetWeightIndex morphTargetWeightOffset;

		/** Index of the model joint that the node is attached to. */
		resource::Model::JointIndex jointIndex;

		/** Local bounding box of the node. */
		Box<3, float> boundingBox;

		/** Local bounding radius of the node. */
		float boundingRadius;
	};

	/**
	 * Default 3D model vertex shader constants.
	 */
	static constexpr auto DEFAULT_VERTEX_SHADER_CONSTANTS = [](const Model3D::ShaderConfiguration& shaderConfiguration) noexcept -> VertexShaderConstants {
		return VertexShaderConstants{
			.VERTEX_TEXTURED_ON_CHANNEL_0 = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0,
			.VERTEX_TEXTURED_ON_CHANNEL_1 = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0,
			.VERTEX_COLORED = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_COLORED) != 0,
			.VERTEX_SKINNED = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_SKINNED) != 0,
			.VERTEX_MORPHED_POSITION = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_POSITION) != 0,
			.VERTEX_MORPHED_NORMAL = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_NORMAL) != 0,
			.VERTEX_MORPHED_TANGENT = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_TANGENT) != 0,
			.VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0 = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) != 0,
			.VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1 = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) != 0,
			.VERTEX_MORPHED_COLOR = (shaderConfiguration.vertexFlags & resource::Model::VERTEX_MORPHED_COLOR) != 0,
		};
	};

	/**
	 * Default 3D model fragment shader constants.
	 */
	static constexpr auto DEFAULT_FRAGMENT_SHADER_CONSTANTS = [](const Model3D::ShaderConfiguration& shaderConfiguration) noexcept -> FragmentShaderConstants {
		return FragmentShaderConstants{
			.FRAGMENT_HDR = false,
			.FRAGMENT_ALPHA_MASKED = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_MASKED) != 0,
			.FRAGMENT_ALPHA_BLENDED = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0,
			.FRAGMENT_DOUBLE_SIDED = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_DOUBLE_SIDED) != 0,
			.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0) != 0,
			.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1) != 0,
			.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0) != 0,
			.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1) != 0,
			.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0) != 0,
			.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1) != 0,
			.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0) != 0,
			.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1) != 0,
			.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0) != 0,
			.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1 = (shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1) != 0,
		};
	};

	/**
	 * Default 3D model graphics pipeline configuration.
	 */
	static constexpr auto DEFAULT_SHADER_PIPELINE_OPTIONS = [](const Model3D::ShaderConfiguration& shaderConfiguration) noexcept -> ShaderPipelineOptions {
		return ShaderPipelineOptions{
			.primitiveType = shaderConfiguration.primitiveType,
			.faceCullingMode = ((shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_DOUBLE_SIDED) != 0) ? FaceCullingMode::NONE : FaceCullingMode::CULL_BACK_FACES,
			.frontFace = shaderConfiguration.frontFace,
			.blendState = ((shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0)
		                      ? Optional<BlendState>{BlendState::ALPHA_BLENDING_PREMULTIPLIED}
		                      : Optional<BlendState>{},
		};
	};

	/**
	 * Construct an invalid model that must be reassigned before use.
	 */
	Model3D() noexcept = default;

	/**
	 * Construct a model from loaded model data.
	 *
	 * \param device device to create the model for. Must outlive the model.
	 * \param renderer3D renderer to create the model for. Must be associated with
	 *        the given device. Must outlive the model.
	 * \param model data to create the model from.
	 * \param options model options, see Model3DOptions.
	 * \param loadTexture function to use to load the textures from the images
	 *        provided by the model. Defaults to creating a new texture.
	 *
	 * \throws graphics::Error on failure to create the model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	Model3D(Device& device, Renderer3D& renderer3D, const resource::Model& model, const Model3DOptions& options = {},
		FunctionView<SharedPointer<TextureImplementation>(Device&, const resource::Model::Image&, const TextureImageUploadOptions&, const TextureSamplerOptions&)> loadTexture =
			loadTextureDefault);

	/**
	 * Construct a model from loaded model data.
	 *
	 * \param device device to create the model for. Must outlive the model.
	 * \param renderer3D renderer to create the model for. Must be associated with
	 *        the given device. Must outlive the model.
	 * \param model data to create the model from.
	 * \param options model options, see Model3DOptions.
	 * \param loadTexture function to use to load the textures from the images
	 *        provided by the model. Defaults to creating a new texture.
	 *
	 * \throws graphics::Error on failure to create the model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	Model3D(Device& device, Renderer3D& renderer3D, resource::Model&& model, const Model3DOptions& options = {},
		FunctionView<SharedPointer<TextureImplementation>(Device&, const resource::Model::Image&, const TextureImageUploadOptions&, const TextureSamplerOptions&)> loadTexture =
			loadTextureDefault);

	/** Destructor. */
	GREM_API(graphics_3d) ~Model3D();

	/** Copy constructor. */
	GREM_API(graphics_3d) Model3D(const Model3D& other);

	/** Move constructor. */
	GREM_API(graphics_3d) Model3D(Model3D&& other) noexcept;

	/** Copy assignment. */
	GREM_API(graphics_3d) Model3D& operator=(const Model3D& other);

	/** Move assignment. */
	GREM_API(graphics_3d) Model3D& operator=(Model3D&& other) noexcept;

	/**
	 * Check if the model has a value.
	 *
	 * \return true if the model has a value, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return m.device != nullptr;
	}

	/**
	 * Set the mesh parameters of a node in the model.
	 *
	 * \param nodeIndex index of the node whose parameters to update. Must be a
	 *        valid node index.
	 * \param newParameters new parameter values to set.
	 *
	 * \throws graphics::Error on failure to create the model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The shape of the new parameters will also affect the mesh
	 *       configuration.
	 *
	 * \warning The given morph target parameters must be valid for the model
	 *          and the current morph configuration of the mesh.
	 */
	void setNodeMeshParameters(size_t nodeIndex, const Parameters& newParameters) {
		Node& node = m.nodes[nodeIndex];
		node.mesh.setParameters(newParameters);

		if (newParameters.meshBaseColorMap) {
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
			}
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1;
			}
		} else {
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1;
		}

		if (newParameters.meshOcclusionRoughnessMetallicMap) {
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
			}
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1;
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1;
			}
		} else {
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1;
		}

		if (newParameters.meshNormalMap) {
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
			}
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1;
			}
		} else {
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1;
		}

		if (newParameters.meshEmissiveMap) {
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
			}
			if ((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
				node.shaderConfiguration.fragmentFlags |= resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1;
			}
		} else {
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
			node.shaderConfiguration.fragmentFlags &= ~resource::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1;
		}
	}

	/**
	 * Get the nodes of the model.
	 *
	 * \return a read-only view over the nodes of the model.
	 */
	[[nodiscard]] Span<const Node> getNodes() const noexcept {
		return m.nodes;
	}

	/**
	 * Get the inverse bind-pose matrix offset of the model.
	 *
	 * \return the index of the first inverse bind-pose matrix of the model in
	 *         the device that the model was created for.
	 */
	[[nodiscard]] uint32_t getInverseBindPoseMatrixOffset() const noexcept {
		return m.inverseBindPoseMatrixRange.begin;
	}

	/**
	 * Get the morph target value offset of the model.
	 *
	 * \return the index of the first floating-point value of the model's morph
	 *         targets in the device that the model was created for.
	 */
	[[nodiscard]] uint32_t getMorphTargetValueOffset() const noexcept {
		return m.morphTargetValueRange.begin;
	}

	/**
	 * Get the local bind pose of the model.
	 *
	 * \return the bind pose of the model.
	 */
	[[nodiscard]] const resource::Model::Pose& getBindPose() const noexcept {
		return m.bindPose;
	}

	/**
	 * Get the number of joints in the model.
	 *
	 * \return the number of joints in the model.
	 */
	[[nodiscard]] resource::Model::JointCount getJointCount() const noexcept {
		return static_cast<resource::Model::JointCount>(m.bindPose.localJoints.size());
	}

	/**
	 * Get the number of static joints in the model.
	 *
	 * \return the number of unskinned joints at the beginning of the model's
	 *         joint array.
	 */
	[[nodiscard]] resource::Model::JointCount getStaticJointCount() const noexcept {
		return m.staticJointCount;
	}

	/**
	 * Get the number of morph target weights of the model.
	 *
	 * \return the number of morph target weights of the model.
	 */
	[[nodiscard]] resource::Model::MorphTargetWeightCount getMorphTargetWeightCount() const noexcept {
		return static_cast<resource::Model::MorphTargetWeightCount>(m.bindPose.localMorphTargetWeights.size());
	}

	/**
	 * Get the indices of the parent joints of each joint of the model.
	 *
	 * \return the parent indices of each joint of the model.
	 */
	[[nodiscard]] Span<const resource::Model::JointIndex> getJointParentIndices() const noexcept {
		return m.jointParentIndices;
	}

	/**
	 * Get the animations of the model.
	 *
	 * \return a read-only view over the array of animations of the model.
	 */
	[[nodiscard]] Span<const resource::Model::Animation> getAnimations() const noexcept {
		return m.animations;
	}

	/**
	 * Get the animation channels of the model.
	 *
	 * \return a read-only view over the array of animation channels of the
	 *         model.
	 */
	[[nodiscard]] Span<const resource::Model::AnimationChannel> getAnimationChannels() const noexcept {
		return m.animationChannels;
	}

	/**
	 * Get the input timepoints of the animation channel keyframes of the model.
	 *
	 * \return a read-only view over the array of input timepoints of the
	 *         animation channel keyframes of the model.
	 */
	[[nodiscard]] Span<const float> getKeyframeInputTimePoints() const noexcept {
		return m.keyframeInputTimePoints;
	}

	/**
	 * Get the output values of the animation channel keyframes of the model.
	 *
	 * \return a read-only view over the bytes of the array of output values of
	 *         the animation channel keyframes of the model.
	 */
	[[nodiscard]] Span<const byte> getKeyframeOutputValueData() const noexcept {
		return m.keyframeOutputValueData;
	}

	/**
	 * Find the index of an animation with a specific name.
	 *
	 * \param name name of the animation to search for.
	 *
	 * \return the index of the given animation in the model's animation array,
	 *         or an empty optional if the animation wasn't found.
	 */
	[[nodiscard]] Optional<resource::Model::AnimationIndex> findAnimationIndex(StringView name) const noexcept {
		if (const auto it = m.animationMap.find(name); it != m.animationMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of an animation with a specific name.
	 *
	 * \param name name of the animation to search for.
	 *
	 * \return the index of the given animation in the model's animation array.
	 *
	 * \throws std::out_of_range if the given animation wasn't found.
	 */
	[[nodiscard]] resource::Model::AnimationIndex getAnimationIndex(StringView name) const {
		if (const Optional<resource::Model::AnimationIndex> animationIndex = findAnimationIndex(name)) {
			return *animationIndex;
		}
		throw std::out_of_range{"Animation \"" + String{name} + "\" not found."};
	}

	/**
	 * Get the animation at a specific index.
	 *
	 * \param index index of the animation to get.
	 *
	 * \return a non-owning read-only view over the given animation.
	 *
	 * \throws std::out_of_range if the given index is out of range.
	 */
	[[nodiscard]] resource::Model::AnimationView getAnimationAtIndex(resource::Model::AnimationIndex index) const {
		if (static_cast<size_t>(index) >= m.animations.size()) {
			throw std::out_of_range{"Invalid animation index."};
		}
		const resource::Model::Animation& animation = m.animations[index];
		const resource::Model::AnimationChannelIndex animationChannelsBegin = animation.animationChannelOffset;
		const resource::Model::AnimationChannelIndex animationChannelsEnd =
			(index + 1 < m.animations.size()) ? m.animations[index + 1].animationChannelOffset : static_cast<resource::Model::AnimationChannelIndex>(m.animationChannels.size());
		return resource::Model::AnimationView{
			.minTimePoint = animation.minTimePoint,
			.maxTimePoint = animation.maxTimePoint,
			.channels = Span{m.animationChannels}.subspan(animationChannelsBegin, animationChannelsEnd - animationChannelsBegin),
			.keyframeInputTimePoints = m.keyframeInputTimePoints,
			.keyframeOutputValueData = m.keyframeOutputValueData,
		};
	}

	/**
	 * Get the animation with a specific name.
	 *
	 * \param name name of the animation to search for.
	 *
	 * \return a non-owning read-only view over the given animation, or an empty
	 *         optional if the animation wasn't found.
	 */
	[[nodiscard]] Optional<resource::Model::AnimationView> findAnimation(StringView name) const noexcept {
		if (const Optional<resource::Model::AnimationIndex> animationIndex = findAnimationIndex(name)) {
			return getAnimationAtIndex(*animationIndex);
		}
		return {};
	}

	/**
	 * Find the index of a joint with a specific name.
	 *
	 * \param name name of the joint to search for.
	 *
	 * \return the index of the given joint in the model's joint array, or an
	 *         empty optional if the joint wasn't found.
	 */
	[[nodiscard]] Optional<resource::Model::JointIndex> findJointIndex(StringView name) const noexcept {
		if (const auto it = m.jointMap.find(name); it != m.jointMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of the root joint of a skin with a specific name.
	 *
	 * \param name name of the skin whose root joint to search for.
	 *
	 * \return the index of the root joint of the given skin in the model's
	 *         joint array, or an empty optional if the skin wasn't found.
	 */
	[[nodiscard]] Optional<resource::Model::JointIndex> findSkinRootJointIndex(StringView name) const noexcept {
		if (const auto it = m.skinMap.find(name); it != m.skinMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Get the local bounding box of the model in its bind pose.
	 *
	 * \return the axis-aligned bounding box of the model, relative to its
	 *         origin.
	 */
	[[nodiscard]] Box<3, float> getBindPoseBoundingBox() const noexcept {
		return m.bindPoseBoundingBox;
	}

	/**
	 * Get the local bounding radius of the model in its bind pose.
	 *
	 * \return the maximum vertex position distance from the model origin.
	 */
	[[nodiscard]] float getBindPoseBoundingRadius() const noexcept {
		return m.bindPoseBoundingRadius;
	}

private:
	[[nodiscard]] static SharedPointer<TextureImplementation> loadTextureDefault(Device& device, const resource::Model::Image& image,
		const TextureImageUploadOptions& textureImageUploadOptions, const TextureSamplerOptions& textureSamplerOptions) {
		const resource::ImageView imageView = match(image)([&](const auto& img) -> resource::ImageView { return img; });
		return Texture{device, imageView, textureImageUploadOptions, textureSamplerOptions}.lock();
	}

	struct {
		Device* device = nullptr;
		Renderer3D* renderer3D = nullptr;
		ArrayList<Node> nodes{};
		RangeAllocation<uint32_t> inverseBindPoseMatrixRange{};
		RangeAllocation<uint32_t> morphTargetValueRange{};
		resource::Model::Pose bindPose{};
		ArrayList<resource::Model::JointIndex> jointParentIndices{};
		ArrayList<resource::Model::Animation> animations{};
		ArrayList<resource::Model::AnimationChannel> animationChannels{};
		ArrayList<float> keyframeInputTimePoints{};
		Buffer<byte> keyframeOutputValueData{};
		resource::Model::JointCount staticJointCount = 0;
		Box<3, float> bindPoseBoundingBox{};
		float bindPoseBoundingRadius = 0.0f;
		resource::Model::JointMap jointMap{};
		resource::Model::JointMap skinMap{};
		resource::Model::AnimationMap animationMap{};
	} m;
};

} // namespace grem::graphics

/**
 * Specialization of std::hash for grem::graphics::Model3D::ShaderConfiguration.
 */
template <>
struct std::hash<grem::graphics::Model3D::ShaderConfiguration> {
	[[nodiscard]] std::size_t operator()(const grem::graphics::Model3D::ShaderConfiguration& configuration) const {
		return hasher(configuration);
	}

private:
	[[no_unique_address]] grem::graphics::Model3D::ShaderConfiguration::Hash hasher;
};

#endif
