// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_MESH_HPP
#define GREM_GRAPHICS_MESH_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/BitArray.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>

#include <type_traits> // std::is_empty_v, std::remove_cvref_t
#include <typeindex>   // std::type_index
#include <typeinfo>    // IWYU pragma: keep // typeid
#include <utility>     // std::declval

namespace grem::graphics {

class Device;                 // Forward declaration, to avoid a circular include of Device.hpp.
struct TextureImplementation; // Forward declaration, to avoid including Texture.hpp.

struct MeshImplementation; ///< Backend-specific implementation of Mesh.

/**
 * Concept that checks if a type is a valid vertex type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept mesh_vertex = vertex_attribute_struct<T>;

/**
 * Tag type for specifying that a Mesh does not have an index buffer.
 */
struct NoIndex {};

/**
 * Specification of which type of indices is used in the index buffer of a
 * particular Mesh.
 */
enum class MeshIndexType : uint8_t {
	U16, ///< Unsigned 16-bit integer.
	U32, ///< Unsigned 32-bit integer.
};

namespace detail {

template <typename T>
inline constexpr Optional<MeshIndexType> MESH_INDEX_TYPE{};

// clang-format off
template <> inline constexpr Optional<MeshIndexType> MESH_INDEX_TYPE<NoIndex>{};
template <> inline constexpr Optional<MeshIndexType> MESH_INDEX_TYPE<uint16_t> = MeshIndexType::U16;
template <> inline constexpr Optional<MeshIndexType> MESH_INDEX_TYPE<uint32_t> = MeshIndexType::U32;
// clang-format on

} // namespace detail

/**
 * Concept that checks if a type is a valid index type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept mesh_index =        //
	same_as<T, NoIndex> ||  //
	same_as<T, uint16_t> || //
	same_as<T, uint32_t>;

/**
 * Tag type for specifying that a Mesh does not have any parameters.
 */
struct NoParameters {};

/**
 * Concept that checks if a type is a valid mesh parameter struct type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept mesh_parameters = parameter_struct<T>;

/**
 * Tag type for specifying that a Mesh does not have any instance attributes.
 */
struct NoInstance {};

/**
 * Concept that checks if a type is a valid instance type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept mesh_instance = field_struct<T>;

/**
 * Mask of active vertex attributes in a Mesh.
 */
using VertexAttributeMask = BitArray<32>;

namespace detail {

class MeshBase {
public:
	/** Destructor. */
	~MeshBase() = default;

	/** Copy constructor. */
	MeshBase(const MeshBase&) = default;

	/** Move constructor. */
	MeshBase(MeshBase&&) noexcept = default;

	/** Copy assignment. */
	MeshBase& operator=(const MeshBase&) = default;

	/** Move assignment. */
	MeshBase& operator=(MeshBase&&) noexcept = default;

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<MeshImplementation> lock() const {
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] MeshImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics)
	MeshBase(Device& device, std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		size_t vertexStride, Optional<MeshIndexType> indexType, size_t indexStride, Span<const ParameterDescription> parameterDescriptions, size_t parameterStride,
		size_t textureParameterCount, Span<const FieldDescription> instanceAttributeDescriptions, size_t instanceStride);

	GREM_API(graphics)
	void uploadVertices(Span<const byte> vertexData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t vertexStride);

	GREM_API(graphics)
	void uploadVertexAttributes(Span<const Span<const byte>> vertexAttributeData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		size_t vertexStride);

	GREM_API(graphics)
	void uploadIndices(Span<const byte> indexData, uint32_t indexCount, size_t indexStride);

	GREM_API(graphics)
	void uploadParameters(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures);

private:
	SharedPointer<MeshImplementation> implementation{};
};

} // namespace detail

/**
 * Generic abstraction of a GPU vertex array object and its associated buffers.
 *
 * \tparam Vertex type of vertices stored in the vertex buffer. Must meet the
 *         requirements of the grem::graphics::mesh_vertex concept.
 * \tparam Index type of indices stored in the index buffer, or NoIndex for no
 *         index buffer. Must meet the requirements of the
 *         grem::graphics::mesh_index concept.
 * \tparam Parameters user-defined aggregate type of parameters that the uniform
 *         buffer of the mesh should contain.
 * \tparam Instance type of instances stored in the instance buffer, or
 *         NoInstance for no instance buffer. Must meet the requirements of the
 *         grem::graphics::mesh_instance concept.
 */
template <typename Vertex, typename Index = NoIndex, typename Parameters = NoParameters, typename Instance = NoInstance>
class Mesh : public detail::MeshBase {
public:
	static_assert(mesh_vertex<Vertex>, "Mesh template parameter \"Vertex\" must be a valid vertex type.");
	static_assert(mesh_index<Index>, "Mesh template parameter \"Index\" must be a valid index type.");
	static_assert(mesh_parameters<Parameters>, "Mesh template parameter \"Parameters\" must be a valid parameter struct type.");
	static_assert(mesh_instance<Instance>, "Mesh template parameter \"Instance\" must be a valid instance type.");

	/** Vertex type of the mesh. */
	using vertex_type = Vertex;

	/** Index type of the mesh. */
	using index_type = Index;

	/** Parameters type of the mesh. */
	using parameters_type = Parameters;

	/** Instance type of the mesh. */
	using instance_type = Instance;

	/**
	 * Construct an empty mesh.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{}) requires(std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), detail::MESH_INDEX_TYPE<Index>,
			  (same_as<Index, NoIndex>) ? 0 : sizeof(Index), {}, 0, 0, detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {}

	/**
	 * Construct an empty mesh.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param parameters initial parameter values to upload to the uniform
	 *        buffer.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, const Parameters& parameters, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{}) requires(!std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), detail::MESH_INDEX_TYPE<Index>,
			  (same_as<Index, NoIndex>) ? 0 : sizeof(Index), detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::PARAMETER_VALUES_BYTES_SIZE<Parameters>,
			  detail::PARAMETER_TEXTURES_COUNT<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {
		setParameters(parameters);
	}

	/**
	 * Constructor for meshes that only have a vertex buffer.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param vertices initial data to upload to the vertex buffer.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, Span<const Vertex> vertices, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{})
		requires(same_as<Index, NoIndex> && std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), {}, 0, {}, 0, 0,
			  detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {
		uploadVertices(asBytes(vertices), static_cast<uint32_t>(vertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
	}

	/**
	 * Constructor for meshes that have a vertex buffer and an index buffer.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param vertices initial data to upload to the vertex buffer.
	 * \param indices initial data to upload to the index buffer.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, Span<const Vertex> vertices, Span<const Index> indices, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{})
		requires(!same_as<Index, NoIndex> && std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), detail::MESH_INDEX_TYPE<Index>,
			  sizeof(Index), {}, 0, 0, detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {
		uploadVertices(asBytes(vertices), static_cast<uint32_t>(vertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
		uploadIndices(asBytes(indices), static_cast<uint32_t>(indices.size()), sizeof(Index));
	}

	/**
	 * Constructor for meshes that have a vertex buffer and a uniform buffer.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param vertices initial data to upload to the vertex buffer.
	 * \param parameters initial parameter values to upload to the uniform
	 *        buffer.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, Span<const Vertex> vertices, const Parameters& parameters, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{})
		requires(same_as<Index, NoIndex> && !std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), {}, 0,
			  detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::PARAMETER_VALUES_BYTES_SIZE<Parameters>, detail::PARAMETER_TEXTURES_COUNT<Parameters>,
			  detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {
		uploadVertices(asBytes(vertices), static_cast<uint32_t>(vertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
		setParameters(parameters);
	}

	/**
	 * Constructor for meshes that have a vertex buffer, an index buffer and a
	 * uniform buffer.
	 *
	 * \param device device to create the mesh for. Must outlive the mesh.
	 * \param vertices initial data to copy into the vertex buffer.
	 * \param indices initial data to copy into the index buffer.
	 * \param parameters initial parameter values to upload to the uniform
	 *        buffer.
	 * \param activeVertexAttributes mask of active vertex attributes for this
	 *        mesh. Inactive vertex attributes must not be read by any shader
	 *        that receives this mesh as input.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Mesh(Device& device, Span<const Vertex> vertices, Span<const Index> indices, const Parameters& parameters, VertexAttributeMask activeVertexAttributes = ~VertexAttributeMask{})
		requires(!same_as<Index, NoIndex> && !std::is_empty_v<Parameters>)
		: detail::MeshBase(device, typeid(Mesh), activeVertexAttributes, detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex), detail::MESH_INDEX_TYPE<Index>,
			  sizeof(Index), detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::PARAMETER_VALUES_BYTES_SIZE<Parameters>, detail::PARAMETER_TEXTURES_COUNT<Parameters>,
			  detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance)) {
		uploadVertices(asBytes(vertices), static_cast<uint32_t>(vertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
		uploadIndices(asBytes(indices), static_cast<uint32_t>(indices.size()), sizeof(Index));
		setParameters(parameters);
	}

	/**
	 * Upload new vertices to the vertex buffer, overriding any previous
	 * contents.
	 *
	 * \param newVertices new data to upload to the vertex buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setVertices(Span<const Vertex> newVertices) noexcept {
		uploadVertices(asBytes(newVertices), static_cast<uint32_t>(newVertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
	}

	/**
	 * Upload new deinterleaved vertex attributes to the vertex buffer,
	 * overriding any previous contents.
	 *
	 * \param vertexAttributes new arrays of attribute values to upload to the
	 *        vertex buffer. Each attribute array must either be empty or have
	 *        the same size as all other non-empty arrays. If a given array is
	 *        empty, its corresponding vertex attribute must have been specified
	 *        as inactive in the mesh constructor.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename... AttributeArrays>
	void setVertexAttributes(const AttributeArrays&... vertexAttributes) noexcept requires(sizeof...(AttributeArrays) == meta::aggregate_size_v<Vertex>) {
		static_assert(convertible_to<Tuple<std::remove_cvref_t<decltype(Span{vertexAttributes}.front())>...>, decltype(meta::getFields(std::declval<const Vertex&>()))>,
			"The given vertex attributes must be convertible to the vertex fields of the mesh.");
		size_t vertexCount = 0;
		((vertexCount = (vertexAttributes.empty()) ? vertexCount : vertexAttributes.size()), ...);
		GREM_ASSERT(((vertexAttributes.empty() || vertexAttributes.size() == vertexCount) && ...));
		uploadVertexAttributes({asBytes(Span{vertexAttributes})...}, static_cast<uint32_t>(vertexCount), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
	}

	/**
	 * Upload new indices to the index buffer, overriding any previous contents.
	 *
	 * \param newIndices new data to upload to the index buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setIndices(Span<const Index> newIndices) noexcept requires(!same_as<Index, NoIndex>) {
		uploadIndices(asBytes(newIndices), static_cast<uint32_t>(newIndices.size()), sizeof(Index));
	}

	/**
	 * Upload new vertices and indices to the vertex and index buffers,
	 * overriding any previous contents.
	 *
	 * \param newVertices new data to upload to the vertex buffer.
	 * \param newIndices new data to upload to the index buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setVerticesAndIndices(Span<const Vertex> newVertices, Span<const Index> newIndices) noexcept requires(!same_as<Index, NoIndex>) {
		uploadVertices(asBytes(newVertices), static_cast<uint32_t>(newVertices.size()), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
		uploadIndices(asBytes(newIndices), static_cast<uint32_t>(newIndices.size()), sizeof(Index));
	}

	/**
	 * Upload new indices and deinterleaved vertex attributes to the index and
	 * vertex buffers, overriding any previous contents.
	 *
	 * \param newIndices new data to upload to the index buffer.
	 * \param vertexAttributes new arrays of attribute values to upload to the
	 *        vertex buffer. Each attribute array must either be empty or have
	 *        the same size as all other non-empty arrays. If a given array is
	 *        empty, its corresponding vertex attribute must have been specified
	 *        as inactive in the mesh constructor.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename... AttributeArrays>
	void setIndicesAndVertexAttributes(Span<const Index> newIndices, const AttributeArrays&... vertexAttributes) noexcept
		requires(!same_as<Index, NoIndex> && sizeof...(AttributeArrays) == meta::aggregate_size_v<Vertex>) {
		static_assert(convertible_to<Tuple<std::remove_cvref_t<decltype(Span{vertexAttributes}.front())>...>, decltype(meta::getFields(std::declval<const Vertex&>()))>,
			"The given vertex attributes must be convertible to the vertex fields of the mesh.");
		size_t vertexCount = 0;
		((vertexCount = (vertexAttributes.empty()) ? vertexCount : vertexAttributes.size()), ...);
		GREM_ASSERT(((vertexAttributes.empty() || vertexAttributes.size() == vertexCount) && ...));
		GREM_ASSERT(std::all_of(newIndices.begin(), newIndices.end(), [&](const Index& index) -> bool { return index < vertexCount; }));
		uploadVertexAttributes({asBytes(Span{vertexAttributes})...}, static_cast<uint32_t>(vertexCount), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, sizeof(Vertex));
		uploadIndices(asBytes(newIndices), static_cast<uint32_t>(newIndices.size()), sizeof(Index));
	}

	/**
	 * Upload new parameter values to the uniform buffer, overriding any
	 * previous contents.
	 *
	 * \param newParameters new parameter values to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setParameters(const Parameters& newParameters) noexcept requires(!std::is_empty_v<Parameters>) {
		alignas(sizeof(float) * 4) Array<byte, detail::PARAMETER_VALUES_BYTES_SIZE<Parameters>> bytes;
		Array<SharedPointer<TextureImplementation>, detail::PARAMETER_TEXTURES_COUNT<Parameters>> textures{};
		size_t byteOffset = 0;
		size_t textureIndex = 0;
		meta::forEachField(newParameters, [&]<typename Wrapper>(const Wrapper& parameter) -> void { //
			detail::alignParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameter(bytes.data() + byteOffset, parameter);
			detail::skipParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameterTextures(textures.data() + textureIndex, parameter);
			textureIndex += detail::TEXTURE_COUNT_OF_PARAMETER_WRAPPER<Wrapper>;
		});
		uploadParameters(bytes, textures);
	}
};

} // namespace grem::graphics

#endif
