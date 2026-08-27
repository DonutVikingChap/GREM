// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_OBJ_HPP
#define GREM_CORE_FORMATS_OBJ_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem::obj {

/**
 * Exception type for errors originating from the OBJ API.
 */
struct Error : grem::Error {
	/**
	 * Iterator into the source OBJ string where the error originated from.
	 */
	StringView::iterator position;

	/**
	 * Line number, starting at 1 for the first line, where the error occured.
	 */
	size_t lineNumber;

	Error(const auto& message, StringView::iterator position, size_t lineNumber)
		: grem::Error(message)
		, position(position)
		, lineNumber(lineNumber) {}

	void writeMessage(String& output) const override {
		output.append(toString(lineNumber) + ": " + what());
	}

	[[nodiscard]] bool messageAttachesToPrecedingFilepath() const noexcept override {
		return true;
	}
};

/**
 * Single vertex of a polygonal Face element.
 */
struct FaceVertex {
	uint32_t vertexIndex = 0;            ///< Index of the vertex coordinates in the Asset that define the vertex position.
	uint32_t textureCoordinateIndex = 0; ///< Index of the texture coordinates in the Asset that define the texture coordinates of the vertex.
	uint32_t normalIndex = 0;            ///< Index of the normal vector in the Asset that define the vertex normal.
};

/**
 * Face element forming a single polygon of FaceVertex vertices.
 */
struct Face {
	Buffer<FaceVertex> vertices{}; ///< List of vertices that make up the polygon.
};

/**
 * Group of polygonal Face elements within a Node.
 */
struct Mesh {
	String name{};           ///< Name of the group, or empty if no name was specified.
	ArrayList<Face> faces{}; ///< List of faces belonging to this group.
	String materialName{};   ///< Name of the material of this group, which should be found in one of the associated material libraries.
};

/**
 * Object containing a list of Mesh elements within an Asset.
 */
struct Node {
	String name{};            ///< Name of the object, or empty if no name was specified.
	ArrayList<Mesh> meshes{}; ///< List of meshes belonging to this object.
};

/**
 * Collection of Node elements defined by an OBJ file.
 */
struct Asset {
	/**
	 * Parse an asset from an OBJ string.
	 *
	 * \param objString input OBJ string to parse.
	 *
	 * \return the parsed asset.
	 *
	 * \throws obj::Error on failure to parse the asset.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(core) static Asset parse(StringView objString);

	ArrayList<String> materialLibraryFilenames{}; ///< List of relative filepaths of the material libraries associated with this asset.
	Buffer<vec3> vertices{};                      ///< List of vertex positions referenced by the face vertices defined in this asset.
	Buffer<vec2> textureCoordinates{};            ///< List of texture coordinates referenced by the face vertices defined in this asset.
	Buffer<vec3> normals{};                       ///< List of normal vectors referenced by the face vertices defined in this asset.
	ArrayList<Node> nodes{};                      ///< List of object nodes belonging to this asset.
};

namespace mtl {

/**
 * Illumination model to use when rendering a specific Material.
 */
enum class IlluminationModel : uint8_t {
	FLAT = 0,                                 ///< Implementation-defined. \hideinitializer
	LAMBERT = 1,                              ///< Implementation-defined. \hideinitializer
	BLINN_PHONG = 2,                          ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_RAYTRACE = 3,                 ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_RAYTRACE_GLASS = 4,           ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_RAYTRACE_FRESNEL = 5,         ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_RAYTRACE_REFRACT = 6,         ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_RAYTRACE_REFRACT_FRESNEL = 7, ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_REFLECT = 8,                  ///< Implementation-defined. \hideinitializer
	BLINN_PHONG_REFLECT_GLASS = 9,            ///< Implementation-defined. \hideinitializer
	SHADOW = 10,                              ///< Implementation-defined. \hideinitializer
};

/**
 * Material properties of a Node.
 */
struct Material {
	String name{};                                                 ///< Name of the material.
	String ambientMapName{};                                       ///< Relative filepath of the ambient map image, or empty for no ambient map.
	String diffuseMapName{};                                       ///< Relative filepath of the diffuse map image, or empty for no diffuse map.
	String specularMapName{};                                      ///< Relative filepath of the specular map image, or empty for no specular map.
	String emissiveMapName{};                                      ///< Relative filepath of the emissive map image, or empty for no emissive map.
	String specularExponentMapName{};                              ///< Relative filepath of the specular exponent map image, or empty for no specular exponent map.
	String dissolveFactorMapName{};                                ///< Relative filepath of the dissolve factor map image, or empty for no dissolve factor map.
	String bumpMapName{};                                          ///< Relative filepath of the bump map image, or empty for no bump map.
	String normalMapName{};                                        ///< Relative filepath of the normal map image, or empty for no normal map.
	String occlusionRoughnessMetallicMapName{};                    ///< Relative filepath of the occlusion-roughness-metallic map image, or empty for no ORM map.
	vec3 ambientColor{1.0f, 1.0f, 1.0f};                           ///< Ambient color factor to multiply the sampled ambient map value by.
	vec3 diffuseColor{1.0f, 1.0f, 1.0f};                           ///< Diffuse color factor to multiply the sampled diffuse map value by.
	vec3 specularColor{1.0f, 1.0f, 1.0f};                          ///< Specular color factor to multiply the sampled specular map value by.
	vec3 emissiveColor{0.0f, 0.0f, 0.0f};                          ///< Emissive color factor to multiply the sampled emissive map value by.
	float specularExponent = 1.0f;                                 ///< Specular exponent factor to multiply the sampled specular exponent map value by.
	float dissolveFactor = 1.0f;                                   ///< Dissolve factor to multiply the sampled dissolve factor map value by.
	Optional<float> roughnessFactor{};                             ///< Roughness factor to multiply the sampled roughness value by, or an empty optional if unspecified.
	Optional<float> metallicFactor{};                              ///< Metallic factor to multiply the sampled metallic value by, or an empty optional if unspecified.
	IlluminationModel illuminationModel = IlluminationModel::FLAT; ///< Illumination model to use for rendering this material.
};

/**
 * Material library that stores the material properties for objects defined in
 * an Asset.
 */
struct Library {
	/**
	 * Parse a material library from an MTL string.
	 *
	 * \param mtlString read-only view over the MTL string to parse.
	 *
	 * \return the parsed material library.
	 *
	 * \throws obj::Error on failure to parse the material library.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] static Library parse(StringView mtlString) {
		Library result{};
		result.load(mtlString);
		return result;
	}

	/**
	 * Add materials parsed from an MTL string to the material library.
	 *
	 * \param mtlString read-only view over the MTL string to parse.
	 *
	 * \throws obj::Error on failure to parse the material library.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(core) void load(StringView mtlString);

	ArrayList<Material> materials{}; ///< List of materials belonging to this library.
};

} // namespace mtl
} // namespace grem::obj

#endif
