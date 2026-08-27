// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_GLTF_HPP
#define GREM_CORE_FORMATS_GLTF_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem::gltf {

/**
 * Extensions supported by the glTF parser.
 */
inline constexpr Array SUPPORTED_EXTENSIONS{
	CStringView{"KHR_texture_transform"},
	CStringView{"KHR_texture_basisu"},
	CStringView{"KHR_materials_emissive_strength"},
	CStringView{"KHR_materials_unlit"},
	CStringView{"KHR_materials_ior"},
	CStringView{"KHR_node_visibility"},
	CStringView{"KHR_lights_punctual"},
	CStringView{"KHR_implicit_shapes"},
	CStringView{"KHR_physics_rigid_bodies"},
};

/**
 * Line and column numbers of a location in a glTF source string.
 */
struct SourceLocation {
	/**
	 * Filepath of the glTF source string, or an empty string if there is no
	 * associated file.
	 */
	CStringView filepath{};

	/**
	 * Line number, starting at 1 for the first line. A value of 0 means no
	 * particular line.
	 */
	size_t lineNumber = 1;

	/**
	 * Column number, starting at 1 for the first column. A value of 0 means no
	 * particular column.
	 */
	size_t columnNumber = 1;

	/**
	 * Compare this source location to another for equality.
	 *
	 * \param other the source location to compare this one to.
	 *
	 * \return true if the source locations are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SourceLocation& other) const = default;
};

/**
 * Exception type for errors originating from the glTF API.
 */
struct Error : grem::Error {
	/**
	 * Filepath of the glTF source string, or an empty string if there is no
	 * associated file.
	 */
	String filepath{};

	/**
	 * Line number, starting at 1 for the first line. A value of 0 means no
	 * particular line.
	 */
	size_t lineNumber;

	/**
	 * Column number, starting at 1 for the first column. A value of 0 means no
	 * particular column.
	 */
	size_t columnNumber;

	Error(const auto& message, const SourceLocation& source)
		: grem::Error(message)
		, filepath(source.filepath)
		, lineNumber(source.lineNumber)
		, columnNumber(source.columnNumber) {}

	void writeMessage(String& output) const override {
		if (!filepath.empty()) {
			output.append(filepath);
			output.push_back(':');
		}
		if (lineNumber != 0) {
			output.append(toString(lineNumber));
			output.push_back(':');
			if (columnNumber != 0) {
				output.append(toString(columnNumber));
				output.push_back(':');
			}
		}
		if (!filepath.empty() || lineNumber != 0) {
			output.push_back(' ');
		}
		output.append(what());
	}

	[[nodiscard]] bool messageAttachesToPrecedingFilepath() const noexcept override {
		return filepath.empty() && lineNumber != 0;
	}
};

/** Index into Asset::accessors. */
using AccessorIndex = size_t;

/** Index into Asset::buffers. */
using BufferIndex = size_t;

/** Index into Asset::bufferViews. */
using BufferViewIndex = size_t;

/** Index into Asset::cameras. */
using CameraIndex = size_t;

/** Index into Asset::images. */
using ImageIndex = size_t;

/** Index into Asset::materials. */
using MaterialIndex = size_t;

/** Index into Asset::meshes. */
using MeshIndex = size_t;

/** Index into Asset::nodes. */
using NodeIndex = size_t;

/** Index into Asset::samplers. */
using SamplerIndex = size_t;

/** Index into Asset::scenes. */
using SceneIndex = size_t;

/** Index into Asset::skins. */
using SkinIndex = size_t;

/** Index into Asset::textures. */
using TextureIndex = size_t;

/**
 * Reference to Asset::binChunk, i.e. the data of the BIN chunk in a binary glTF
 * Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct BinChunkData {};

/**
 * Inline data URI in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct InlineData {
	String data;
};

/**
 * Relative path URI in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct RelativePath {
	String path;
};

/**
 * URI in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
using URI = Variant<BinChunkData, InlineData, RelativePath>;

/**
 * Typed view into a buffer view that contains raw binary data in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Accessor {
	struct Extension {};

	enum class ComponentType : uint32_t { // NOLINT(performance-enum-size)
		I8 = 5120,
		U8 = 5121,
		I16 = 5122,
		U16 = 5123,
		U32 = 5125,
		F32 = 5126,
	};

	enum class Type : uint8_t {
		SCALAR,
		VEC2,
		VEC3,
		VEC4,
		MAT2,
		MAT3,
		MAT4,
	};

	struct Sparse {
		struct Extension {};

		struct Indices {
			struct Extension {};

			enum class ComponentType : uint32_t { // NOLINT(performance-enum-size)
				U8 = 5121,
				U16 = 5123,
				U32 = 5125,
			};

			BufferViewIndex bufferView;
			size_t byteOffset;
			ComponentType componentType;
			Extension extensions;
			json::Value extras;
		};

		struct Values {
			struct Extension {};

			BufferViewIndex bufferView;
			size_t byteOffset;
			Extension extensions;
			json::Value extras;
		};

		size_t count;
		Indices indices;
		Values values;
		Extension extensions;
		json::Value extras;
	};

	Optional<BufferViewIndex> bufferView;
	size_t byteOffset;
	ComponentType componentType;
	bool normalized;
	size_t count;
	Type type;
	Optional<Array<float, 16>> max;
	Optional<Array<float, 16>> min;
	Optional<Sparse> sparse;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Keyframe animation in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Animation {
	struct Extension {};

	using SamplerIndex = size_t;

	struct Channel {
		struct Extension {};

		struct Target {
			struct Extension {};

			enum class Path : uint8_t {
				TRANSLATION,
				ROTATION,
				SCALE,
				WEIGHTS,
			};

			Optional<NodeIndex> node;
			Path path;

			Extension extensions;
			json::Value extras;
		};

		SamplerIndex sampler;
		Target target;
		Extension extensions;
		json::Value extras;
	};

	struct Sampler {
		struct Extension {};

		enum class Interpolation : uint8_t {
			LINEAR,
			STEP,
			CUBIC_SPLINE,
		};

		AccessorIndex input;
		Interpolation interpolation;
		AccessorIndex output;
		Extension extensions;
		json::Value extras;
	};

	ArrayList<Channel> channels;
	ArrayList<Sampler> samplers;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Reference to binary geometry, animation, or skins in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Buffer {
	struct Extension {};

	URI uri;
	size_t byteLength;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * View into a buffer, generally representing a subset of the buffer, in a glTF
 * Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct BufferView {
	struct Extension {};

	enum class Target : uint32_t { // NOLINT(performance-enum-size)
		UNSPECIFIED = 0,
		ARRAY_BUFFER = 34962,
		ELEMENT_ARRAY_BUFFER = 34963,
	};

	BufferIndex buffer;
	size_t byteOffset;
	size_t byteLength;
	Optional<size_t> byteStride;
	Target target;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Camera projection in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Camera {
	struct Extension {};

	struct Orthographic {
		struct Extension {};

		vec2 magnification;
		float zFar;
		float zNear;

		Extension extensions;
		json::Value extras;
	};

	struct Perspective {
		struct Extension {};

		Optional<float> aspectRatio;
		float yFov;
		Optional<float> zFar;
		float zNear;

		Extension extensions;
		json::Value extras;
	};

	Variant<Orthographic, Perspective> properties;

	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Image data used to create a texture in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Image {
	struct Extension {};

	Optional<URI> uri;
	String mimeType;
	Optional<BufferViewIndex> bufferView;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Reference to a texture in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct TextureInfo {
	struct Extension {
		struct KHRTextureTransform {
			vec2 offset;
			float rotation;
			vec2 scale;
			Optional<size_t> textureCoordinatesChannel;
		};

		Optional<KHRTextureTransform> KHR_texture_transform;
	};

	TextureIndex index;
	size_t textureCoordinatesChannel;
	Extension extensions;
	json::Value extras;
};

/**
 * Material appearance of a primitive in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Material {
	struct Extension {
		struct KHRMaterialsEmissiveStrength {
			float emissiveStrength;
		};

		struct KHRMaterialsUnlit {};

		struct KHRMaterialsIOR {
			float ior;
		};

		Optional<KHRMaterialsEmissiveStrength> KHR_materials_emissive_strength;
		Optional<KHRMaterialsUnlit> KHR_materials_unlit;
		Optional<KHRMaterialsIOR> KHR_materials_ior;
	};

	struct PBRMetallicRoughness {
		struct Extension {};

		vec4 baseColorFactor;
		Optional<TextureInfo> baseColorTexture;
		float metallicFactor;
		float roughnessFactor;
		Optional<TextureInfo> metallicRoughnessTexture;
		Extension extensions;
		json::Value extras;
	};

	struct NormalTextureInfo : TextureInfo {
		float scale;
	};

	struct OcclusionTextureInfo : TextureInfo {
		float strength;
	};

	enum class AlphaMode : uint8_t {
		ALPHA_OPAQUE,
		ALPHA_MASK,
		ALPHA_BLEND,
	};

	PBRMetallicRoughness pbrMetallicRoughness;
	Optional<NormalTextureInfo> normalTexture;
	Optional<OcclusionTextureInfo> occlusionTexture;
	Optional<TextureInfo> emissiveTexture;
	vec3 emissiveFactor;
	AlphaMode alphaMode;
	float alphaCutoff;
	bool doubleSided;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Set of primitives to be rendered in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Mesh {
	struct Extension {};

	struct Primitive {
		struct Extension {};

		struct Attributes {
			Optional<AccessorIndex> position;
			Optional<AccessorIndex> normal;
			Optional<AccessorIndex> tangent;
			Optional<AccessorIndex> texcoord0;
			Optional<AccessorIndex> texcoord1;
			Optional<AccessorIndex> color0;
			Optional<AccessorIndex> joints0;
			Optional<AccessorIndex> weights0;
			HashMap<String, AccessorIndex> others;
		};

		enum class Mode : uint8_t {
			POINTS = 0,
			LINES = 1,
			LINE_LOOP = 2,
			LINE_STRIP = 3,
			TRIANGLES = 4,
			TRIANGLE_STRIP = 5,
			TRIANGLE_FAN = 6,
		};

		Attributes attributes;
		Optional<AccessorIndex> indices;
		Optional<MaterialIndex> material;
		Mode mode;
		ArrayList<Attributes> targets;
		Extension extensions;
		json::Value extras;
	};

	ArrayList<Primitive> primitives;
	ArrayList<float> weights;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Node in the node hierarchy of a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Node {
	struct Extension {
		struct KHRLightsPunctual {
			using LightIndex = size_t;

			LightIndex light;
		};

		struct KHRNodeVisibility {
			bool visible;
		};

		struct KHRPhysicsRigidBodies {
			using PhysicsMaterialIndex = size_t;
			using CollisionFilterIndex = size_t;
			using ImplicitShapeIndex = size_t;
			using PhysicsJointIndex = size_t;

			struct Geometry {
				Optional<ImplicitShapeIndex> shape;
				Optional<MeshIndex> mesh;
				bool convexHull;
			};

			struct Motion {
				bool isKinematic;
				Optional<float> mass;
				quat inertiaOrientation;
				Optional<vec3> inertiaDiagonal;
				vec3 centerOfMass;
				vec3 linearVelocity;
				vec3 angularVelocity;
				float gravityFactor;
			};

			struct Collider {
				Geometry geometry;
				Optional<PhysicsMaterialIndex> physicsMaterial;
				Optional<CollisionFilterIndex> collisionFilter;
			};

			struct Trigger {
				Optional<Geometry> geometry;
				ArrayList<NodeIndex> nodes;
				Optional<CollisionFilterIndex> collisionFilter;
			};

			struct Joint {
				NodeIndex connectedNode;
				PhysicsJointIndex joint;
				bool enableCollision;
			};

			Optional<Motion> motion;
			Optional<Collider> collider;
			Optional<Trigger> trigger;
			Optional<Joint> joint;
		};

		Optional<KHRLightsPunctual> KHR_lights_punctual;
		Optional<KHRNodeVisibility> KHR_node_visibility;
		Optional<KHRPhysicsRigidBodies> KHR_physics_rigid_bodies;
	};

	Optional<CameraIndex> camera;
	ArrayList<NodeIndex> children;
	Optional<SkinIndex> skin;
	quat rotation;
	vec3 scale;
	vec3 translation;
	Optional<MeshIndex> mesh;
	ArrayList<float> weights;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Texture sampler properties for filtering and wrapping modes in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Sampler {
	struct Extension {};

	enum class MagnificationFilter : uint32_t { // NOLINT(performance-enum-size)
		UNSPECIFIED = 0,
		NEAREST = 9728,
		LINEAR = 9729,
	};

	enum class MinificationFilter : uint32_t { // NOLINT(performance-enum-size)
		UNSPECIFIED = 0,
		NEAREST = 9728,
		LINEAR = 9729,
		NEAREST_MIPMAP_NEAREST = 9984,
		LINEAR_MIPMAP_NEAREST = 9985,
		NEAREST_MIPMAP_LINEAR = 9986,
		LINEAR_MIPMAP_LINEAR = 9987,
	};

	enum class WrappingMode : uint32_t { // NOLINT(performance-enum-size)
		REPEAT = 10497,
		MIRRORED_REPEAT = 33648,
		CLAMP_TO_EDGE = 33071,
	};

	MagnificationFilter magFilter;
	MinificationFilter minFilter;
	WrappingMode wrapS;
	WrappingMode wrapT;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * The root nodes of a scene in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Scene {
	struct Extension {};

	ArrayList<NodeIndex> nodes;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Joints and matrices defining a skin in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Skin {
	struct Extension {};

	Optional<AccessorIndex> inverseBindMatrices;
	Optional<NodeIndex> skeleton;
	ArrayList<NodeIndex> joints;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * Information about a texture and its sampler in a glTF Asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Texture {
	struct Extension {
		struct KHRTextureBasisu {
			ImageIndex source;
		};

		Optional<KHRTextureBasisu> KHR_texture_basisu;
	};

	Optional<SamplerIndex> sampler;
	Optional<ImageIndex> source;
	String name;
	Extension extensions;
	json::Value extras;
};

/**
 * The root object for a glTF asset.
 *
 * See the glTF 2.0 [API Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf)
 * and [specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
 * for more info.
 */
struct Asset {
	struct Extension {
		struct KHRLightsPunctual {
			struct Light {
				enum class Type : uint8_t {
					DIRECTIONAL,
					POINT,
					SPOT,
				};

				struct Spot {
					float innerConeAngle;
					float outerConeAngle;
				};

				String name;
				vec3 color;
				float intensity;
				Type type;
				Optional<float> range;
				Optional<Spot> spot;
			};

			ArrayList<Light> lights;
		};

		struct KHRImplicitShapes {
			struct Plane {
				bool doubleSided;
				Optional<float> sizeX;
				Optional<float> sizeZ;
			};

			struct Sphere {
				float radius;
			};

			struct Box {
				vec3 size;
			};

			struct Cylinder {
				float height;
				float radiusBottom;
				float radiusTop;
			};

			struct Capsule {
				float height;
				float radiusBottom;
				float radiusTop;
			};

			using Shape = Variant<Plane, Sphere, Box, Cylinder, Capsule>;

			ArrayList<Shape> shapes;
		};

		struct KHRPhysicsRigidBodies {
			struct Material {
				enum class FrictionCombine : uint8_t {
					AVERAGE = 0,
					MINIMUM = 1,
					MAXIMUM = 2,
					MULTIPLY = 3,
				};

				enum class RestitutionCombine : uint8_t {
					AVERAGE = 0,
					MINIMUM = 1,
					MAXIMUM = 2,
					MULTIPLY = 3,
				};

				float staticFriction;
				float dynamicFriction;
				float restitution;
				Optional<FrictionCombine> frictionCombine;
				Optional<RestitutionCombine> restitutionCombine;
			};

			struct CollisionFilter {
				ArrayList<String> collisionSystems;
				ArrayList<String> collideWithSystems;
				ArrayList<String> notCollideWithSystems;
			};

			struct Joint {
				struct Limit {
					struct LinearAxes {
						bool x;
						bool y;
						bool z;
					};

					struct AngularAxes {
						bool x;
						bool y;
						bool z;
					};

					Optional<float> min;
					Optional<float> max;
					Optional<float> stiffness;
					float damping;
					Variant<LinearAxes, AngularAxes> axes;
				};

				struct Drive {
					enum class Type : uint8_t {
						LINEAR,
						ANGULAR,
					};

					enum class Mode : uint8_t {
						FORCE,
						ACCELERATION,
					};

					Type type;
					Mode mode;
					uint8_t axis;
					Optional<float> maxForce;
					Optional<float> positionTarget;
					Optional<float> velocityTarget;
					float damping;
				};

				ArrayList<Limit> limits;
				ArrayList<Drive> drives;
			};

			ArrayList<Material> physicsMaterials;
			ArrayList<CollisionFilter> collisionFilters;
			ArrayList<Joint> physicsJoints;
		};

		Optional<KHRLightsPunctual> KHR_lights_punctual;
		Optional<KHRImplicitShapes> KHR_implicit_shapes;
		Optional<KHRPhysicsRigidBodies> KHR_physics_rigid_bodies;
	};

	struct Metadata {
		struct Extension {};

		struct Version {
			uint32_t major;
			uint32_t minor;
		};

		String copyright;
		String generator;
		Version version;
		Optional<Version> minVersion;
		Extension extensions;
		json::Value extras;
	};

	/**
	 * Parse a glTF asset from a JSON string.
	 *
	 * \param glTFString input JSON string to parse.
	 * \param source initial source location corresponding to the start of the
	 *        given input string, used when reporting errors.
	 * \param binChunk BIN chunk data of the surrounding binary glTF file, if
	 *        any. Must outlive its use in the asset if not empty. See the
	 *        warning on Asset::binChunk for more info.
	 *
	 * \return the parsed glTF asset.
	 *
	 * \throws gltf::Error on failure to parse the glTF structure.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The parser only checks that the values it reads are the correct
	 *          JSON types; it does NOT validate that any of the values are
	 *          in-bounds, consistent or otherwise valid according to the glTF
	 *          specification. Always validate any indices, accessors, etc. of
	 *          the parsed asset before using them!
	 */
	[[nodiscard]] GREM_API(core) static Asset parse(StringView glTFString, const SourceLocation& source = {}, Span<const byte> binChunk = {});

	/**
	 * Parse a binary glTF asset from a span of bytes.
	 *
	 * \param glbData input binary data to parse. Must outlive its use in the
	 *        asset. See the warning on Asset::binChunk for more info.
	 * \param sourceFilepath source filepath to use when reporting errors.
	 *
	 * \return the parsed glTF asset.
	 *
	 * \throws gltf::Error on failure to parse the binary glTF or its nested
	 *         glTF structure.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The parser only checks that the values it reads are the correct
	 *          JSON types; it does NOT validate that any of the values are
	 *          in-bounds, consistent or otherwise valid according to the glTF
	 *          specification. Always validate any indices, accessors, etc. of
	 *          the parsed asset before using them!
	 */
	[[nodiscard]] GREM_API(core) static Asset parseBinary(Span<const byte> glbData, CStringView sourceFilepath = {});

	ArrayList<String> extensionsUsed;
	ArrayList<String> extensionsRequired;
	ArrayList<Accessor> accessors;
	ArrayList<Animation> animations;
	Metadata asset;
	ArrayList<Buffer> buffers;
	ArrayList<BufferView> bufferViews;
	ArrayList<Camera> cameras;
	ArrayList<Image> images;
	ArrayList<Material> materials;
	ArrayList<Mesh> meshes;
	ArrayList<Node> nodes;
	ArrayList<Sampler> samplers;
	Optional<SceneIndex> scene;
	ArrayList<Scene> scenes;
	ArrayList<Skin> skins;
	ArrayList<Texture> textures;
	Extension extensions;
	json::Value extras;

	/**
	 * View into the glbData parameter passed to parseBinary(), or the binChunk
	 * parameter passed to parse().
	 *
	 * \warning When accessing this view, make sure the container for the binary
	 *          data that was passed to the parser is still valid! For example,
	 *          this is correct:
	 *          ```cpp
	 * 			const Allocation<byte> data = filesystem.readInputFile(filepath);
	 * 			const gltf::Asset asset = gltf::Asset::parse(data, filepath);
	 *          doSomething(asset); // Good, data is still in scope.
	 *          doSomething(gltf::Asset::parse(filesystem.readInputFile(filepath), filepath)); // This is also ok.
	 *          ```
	 *          but this is incorrect, and a potential security vulnerability:
	 *          ```cpp
	 * 			const gltf::Asset asset = gltf::Asset::parse(filesystem.readInputFile(filepath), filepath);
	 *          doSomething(asset); // Bad! The temporary allocation was deleted at the end of the statement above, so the binChunk view is invalid.
	 *          ```
	 *          This unfortunate footgun is necessary to make binChunk available
	 *          to consumers of Asset without unnecessarily copying the entire
	 *          file contents, or constraining its source container type.
	 */
	Span<const byte> binChunk;
};

} // namespace grem::gltf

#endif
