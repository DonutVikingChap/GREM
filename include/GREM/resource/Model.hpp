// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_MODEL_HPP
#define GREM_RESOURCE_MODEL_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/BitArray.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/TriangleMesh.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/resource/Image.hpp>

#include <stdexcept> // std::out_of_range

namespace grem {
class Filesystem; // Forward declaration, to avoid including Filesystem.hpp.
} // namespace grem

namespace grem::gltf {
struct Asset; // Forward declaration, to avoid including gltf.hpp.
} // namespace grem::gltf

namespace grem::obj {
struct Asset; // Forward declaration, to avoid including obj.hpp.
namespace mtl {
struct Library; // Forward declaration, to avoid including obj.hpp.
} // namespace mtl
} // namespace grem::obj

namespace grem::resource {

/**
 * File type of a Model.
 */
enum class ModelFileType : uint8_t {
	UNKNOWN,     ///< Unknown file type.
	OBJ,         ///< Wavefront OBJ (.obj).
	GLTF,        ///< glTF (.gltf).
	GLTF_BINARY, ///< Binary glTF (.glb).
};

/**
 * Configuration options for a Model.
 */
struct ModelOptions {
	/**
	 * Translation to apply to the root joint of the model.
	 */
	vec3 rootTranslation{0.0f, 0.0f, 0.0f};

	/**
	 * Rotation to apply to the root joint of the model.
	 */
	quat rootRotation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Scale to apply to the root joint of the model.
	 */
	vec3 rootScale{1.0f, 1.0f, 1.0f};

	/**
	 * Exclude any animations from the loaded model.
	 *
	 * Set to true to potentially improve load times in cases where the
	 * animation data isn't needed.
	 */
	bool excludeAnimations = false;

	/**
	 * Exclude any lights from the loaded model.
	 *
	 * Set to true to potentially improve load times in cases where the light
	 * data isn't needed.
	 */
	bool excludeLights = false;

	/**
	 * Exclude any colliders from the loaded model.
	 *
	 * Set to true to potentially improve load times in cases where the
	 * collision data isn't needed.
	 *
	 * \note If #excludePhysics is false, colliders may be included regardless
	 *       of this option.
	 */
	bool excludeColliders = false;

	/**
	 * Exclude any physics information from the loaded model.
	 *
	 * Set to true to potentially improve load times in cases where the physics
	 * data isn't needed.
	 */
	bool excludePhysics = false;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ModelOptions& other) const = default;
};

/**
 * In-memory representation of a 3D model with all of its vertex data directly
 * accessible.
 */
struct Model {
	static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= 4); // Make sure the 4-byte values stored in the data arrays will be properly aligned.

	using ValueOffset = uint32_t;                          ///< Index of a 4-byte value in a data array of the model.
	using IndexCount = uint32_t;                           ///< Number of indices in a mesh of the model.
	using VertexCount = uint32_t;                          ///< Number of vertices in a mesh of the model.
	using MorphTargetCount = uint32_t;                     ///< Number of morph targets in a mesh of the model.
	using KeyframeCount = uint32_t;                        ///< Number of keyframes in an animation channel of the model.
	using MorphTargetWeightIndex = uint32_t;               ///< Offset of a morph target weight in the model's morph target weight array.
	using MorphTargetWeightCount = MorphTargetWeightIndex; ///< Number of morph target weights in a range within the model's morph target weight array.
	using TextureIndex = uint32_t;                         ///< Offset of a texture in the model's texture array.
	using TextureCount = TextureIndex;                     ///< Number of textures in a range within the model's texture array.
	using MaterialIndex = uint32_t;                        ///< Offset of a material in the model's material array.
	using MaterialCount = MaterialIndex;                   ///< Number of materials in a range within the model's material array.
	using MeshIndex = uint32_t;                            ///< Offset of a mesh in the model's mesh array.
	using MeshCount = MeshIndex;                           ///< Number of meshes in a range within the model's mesh array.
	using JointIndex = uint32_t;                           ///< Offset of a joint in the model's joint array.
	using JointCount = JointIndex;                         ///< Number of joints in a range within the model's joint array.
	using AnimationChannelIndex = uint32_t;                ///< Offset of an animation channel in the model's animation channel array.
	using AnimationChannelCount = AnimationChannelIndex;   ///< Number of animation channels in a range within the model's animation channel array.
	using AnimationIndex = uint32_t;                       ///< Offset of an animation in the model's animation array.
	using AnimationCount = AnimationIndex;                 ///< Number of animations in a range within the model's animation array.
	using InstanceIndex = uint32_t;                        ///< Offset of an instance in the model's instance array.
	using InstanceCount = InstanceIndex;                   ///< Number of instances in a range within the model's instance array.
	using LightIndex = uint32_t;                           ///< Offset of a light in the model's light array.
	using LightCount = LightIndex;                         ///< Number of lights in a range within the model's light array.
	using PhysicsObjectIndex = uint32_t;                   ///< Offset of a physics object in the model's physics object array.
	using PhysicsObjectCount = PhysicsObjectIndex;         ///< Number of physics objects in a range within the model's physics object array.
	using PhysicsJointIndex = uint32_t;                    ///< Offset of a physics joint in the model's physics joint array.
	using PhysicsJointCount = PhysicsJointIndex;           ///< Number of physics joints in a range within the model's physics joint array.

	static constexpr size_t MAX_COLLISION_LAYER_COUNT = 32;      ///< Maximum number of collision layers in a model.
	using CollisionLayers = BitArray<MAX_COLLISION_LAYER_COUNT>; ///< Set of collision layers.
	using CollisionLayerIndex = uint8_t;                         ///< Index of a collision layer in a set of collision layers.

	/** Set of VertexFlag bits. */
	using VertexFlags = uint16_t;

	/** Single flag in a set of VertexFlags, specifying vertex attribute usage. */
	enum VertexFlag : VertexFlags {
		VERTEX_TEXTURED_ON_CHANNEL_0 = 1 << 0,                 ///< Texture coordinate channel 0 is used to sample materials.
		VERTEX_TEXTURED_ON_CHANNEL_1 = 1 << 1,                 ///< Texture coordinate channel 1 is used to sample materials.
		VERTEX_COLORED = 1 << 2,                               ///< The vertex color component is used to tint the fragment color.
		VERTEX_SKINNED = 1 << 3,                               ///< The vertex joint components are used to skin the vertex over the model joints.
		VERTEX_MORPHED_POSITION = 1 << 4,                      ///< The vertex position is morphed against the morph targets of the model using provided weights.
		VERTEX_MORPHED_NORMAL = 1 << 5,                        ///< The vertex normal is morphed against the morph targets of the model using provided weights.
		VERTEX_MORPHED_TANGENT = 1 << 6,                       ///< The vertex tangent is morphed against the morph targets of the model using provided weights.
		VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0 = 1 << 7, ///< Texture coordinate channel 0 is morphed against the morph targets of the model using provided weights.
		VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1 = 1 << 8, ///< Texture coordinate channel 1 is morphed against the morph targets of the model using provided weights.
		VERTEX_MORPHED_COLOR = 1 << 9,                         ///< The vertex color is morphed against the morph targets of the model using provided weights.
	};

	/** Set of FragmentFlag bits. */
	using FragmentFlags = uint16_t;

	/** Single flag in a set of FragmentFlags, specifying fragment shader usage. */
	enum FragmentFlag : FragmentFlags {
		FRAGMENT_ALPHA_MASKED = 1 << 0,                           ///< Fragments with alpha less than the alpha cutoff are discarded.
		FRAGMENT_ALPHA_BLENDED = 1 << 1,                          ///< Fragments are alpha blended.
		FRAGMENT_DOUBLE_SIDED = 1 << 2,                           ///< Fragments are double-sided.
		FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0 = 1 << 3,         ///< The base color map is sampled using texture coordinate channel 0.
		FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1 = 1 << 4,         ///< The base color map is sampled using texture coordinate channel 1.
		FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0 = 1 << 5, ///< The metallic/roughness map is sampled using texture coordinate channel 0.
		FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1 = 1 << 6, ///< The metallic/roughness map is sampled using texture coordinate channel 1.
		FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0 = 1 << 7,          ///< The occlusion map is sampled using texture coordinate channel 0.
		FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1 = 1 << 8,          ///< The occlusion map is sampled using texture coordinate channel 1.
		FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0 = 1 << 9,             ///< The normal map is sampled using texture coordinate channel 0.
		FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1 = 1 << 10,            ///< The normal map is sampled using texture coordinate channel 1.
		FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0 = 1 << 11,          ///< The emissive map is sampled using texture coordinate channel 0.
		FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1 = 1 << 12,          ///< The emissive map is sampled using texture coordinate channel 1.
	};

	/** Set of InstanceFlag bits. */
	using InstanceFlags = uint8_t;

	/** Single flag in a set of InstanceFlags, specifying information about a model mesh instance. */
	enum InstanceFlag : InstanceFlags {
		INSTANCE_REVERSE_WINDING_ORDER = 1 << 0, ///< Consider mesh faces with clockwise winding order as front-facing for this instance. \hideinitializer
	};

	/** Graphical primitive type formed by vertices in a model mesh. */
	enum class PrimitiveType : uint8_t {
		POINTS = 0, ///< Individual points. \hideinitializer
		LINES = 1,  ///< Each consecutive pair of points forms an individual line segment. \hideinitializer
		// Note: LINE_LOOP is intentionally omitted since Vulkan does not support it.
		LINE_STRIP = 3,     ///< Each point, except the first, forms a line segment to the previous point. \hideinitializer
		TRIANGLES = 4,      ///< Each consecutive triple of points forms an individual filled triangle. \hideinitializer
		TRIANGLE_STRIP = 5, ///< Each point, except the first two, forms a filled triangle with the previous two points. \hideinitializer
	};

	/** Range of indexed vertices in a model. */
	struct Mesh {
		/** Type of primitives formed by the vertices of the mesh. */
		PrimitiveType primitiveType;

		/** Number of indices of the mesh, or 0 if not indexed. */
		IndexCount indexCount;

		/** Number of vertices in the mesh. */
		VertexCount vertexCount;

		/** Number of morph targets/weights, or 0 if not morphed. */
		MorphTargetCount morphTargetCount;

		/** Stride, in 4-byte values, of the vertices in the morph targets in this mesh, or 0 if not morphed. */
		ValueOffset morphedVertexStride;

		/**
		 * Index of the first 4-byte value in the mesh data in the model.
		 *
		 * After this offset, the mesh data is stored in the following order:
		 * - Indices, `indexCount` values of type `uint32_t`.
		 * - Positions, `vertexCount` values of type `vec3`.
		 * - Normals, `vertexCount` values of type `iA2B10G10R10vec4norm`.
		 * - Tangents, `vertexCount` values of type `iA2B10G10R10vec4norm`.
		 * - Texture coordinates channel 0, `vertexCount` values of type `vec2`, if `vertexFlags` contains `VERTEX_TEXTURED_ON_CHANNEL_0`.
		 * - Texture coordinates channel 1, `vertexCount` values of type `vec2`, if `vertexFlags` contains `VERTEX_TEXTURED_ON_CHANNEL_1`.
		 * - Colors, `vertexCount` values of type `u8vec4norm`, if `vertexFlags` contains `VERTEX_COLORED`.
		 * - Joint indices, `vertexCount` values of type `u8vec4`, if `vertexFlags` contains `VERTEX_SKINNED`.
		 * - Joint weights, `vertexCount` values of type `u8vec4norm`, if `vertexFlags` contains `VERTEX_SKINNED`.
		 */
		ValueOffset meshDataOffset;

		/**
		 * Index of the first 4-byte value in the morph target data in the model.
		 *
		 * After this offset, the morph target data is stored in the following order:
		 * - For each morph target in `morphTargetCount`:
		 *   - For each vertex in `vertexCount`:
		 *     - Morphed position offset, 1 value of type `vec3`, if `vertexFlags` contains `VERTEX_MORPHED_POSITION`.
		 *     - Morphed normal offset, 1 value of type `vec3`, if `vertexFlags` contains `VERTEX_MORPHED_NORMAL`.
		 *     - Morphed tangent offset, 1 value of type `vec3`, if `vertexFlags` contains `VERTEX_MORPHED_TANGENT`.
		 *     - Morphed texture coordinates channel 0 offset, 1 value of type `vec2`, if `vertexFlags` contains `VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0`.
		 *     - Morphed texture coordinates channel 1 offset, 1 value of type `vec2`, if `vertexFlags` contains `VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1`.
		 *     - Morphed color offset, 1 value of type `vec4`, if `vertexFlags` contains `VERTEX_MORPHED_COLOR`.
		 *
		 * \note The `vertexCount` and `vertexFlags` are stored in the mesh that
		 *       this morph target belongs to.
		 */
		ValueOffset morphTargetDataOffset;

		/** Local axis-aligned bounding box of the mesh's vertex positions. */
		Box<3, float> boundingBox;

		/** Maximum distance from the origin of the mesh's vertex positions. */
		float boundingRadius;

		/** Vertex flags of the mesh. */
		VertexFlags vertexFlags;
	};

	/** Image of a texture in a model. */
	using Image = Variant<resource::Image, resource::ImageView>;

	/** Minification filter of a texture in a model. */
	enum class MinificationFilter : uint32_t { // NOLINT(performance-enum-size)
		UNSPECIFIED = 0,                       ///< Unspecified filter.
		NEAREST = 9728,                        ///< Nearest-neighbor filtering without mipmapping.
		LINEAR = 9729,                         ///< Linear filtering without mipmapping.
		NEAREST_MIPMAP_NEAREST = 9984,         ///< Nearest-neighbor filtering from the nearest mip level.
		LINEAR_MIPMAP_NEAREST = 9985,          ///< Linear filtering from the nearest mip level.
		NEAREST_MIPMAP_LINEAR = 9986,          ///< Nearest-neighbor filtering linearly interpolated between mip levels.
		LINEAR_MIPMAP_LINEAR = 9987,           ///< Linear filtering linearly interpoated between mip levels.
	};

	/** Magnification filter of a texture in a model. */
	enum class MagnificationFilter : uint32_t { // NOLINT(performance-enum-size)
		UNSPECIFIED = 0,                        ///< Unspecified filter.
		NEAREST = 9728,                         ///< Nearest-neighbor filtering.
		LINEAR = 9729,                          ///< Linear filtering.
	};

	/** Wrapping mode of a texture in a model. */
	enum class WrappingMode : uint32_t { // NOLINT(performance-enum-size)
		REPEAT = 10497,                  ///< Wrap the texture coordinates.
		MIRRORED_REPEAT = 33648,         ///< Mirror the texture coordinates.
		CLAMP_TO_EDGE = 33071,           ///< Clamp the texture coordinates.
	};

	/** Texture of a material in a model. */
	struct Texture {
		Image image;                             ///< Texture image data.
		MinificationFilter minificationFilter;   ///< Minification filter of the texture sampler.
		MagnificationFilter magnificationFilter; ///< Magnification filter of the texture sampler.
		WrappingMode horizontalWrappingMode;     ///< Horizontal wrapping mode of the texture sampler.
		WrappingMode verticalWrappingMode;       ///< Vertical wrapping mode of the texture sampler.
	};

	/**
	 * Material type that specifies how the material parameters should be
	 * interpreted when shading a mesh instance of a model.
	 */
	enum class MaterialType : uint8_t {
		METALLIC_ROUGHNESS = 0, ///< The shader should interpret the material according to the standard metallic-roughness model.
		UNLIT = 1,              ///< The shader should use only the base color at full brightness.
	};

	/** Material of a mesh instance in a model. */
	struct Material {
		/** Information about how a material uses a texture. */
		struct TextureInfo {
			TextureIndex textureIndex; ///< Index of the texture in the model's texture array, or an out-of-range index for the default texture.
			vec2 textureOffset;        ///< Texture coordinate offset to apply when sampling the texture.
			mat2 textureBasis;         ///< Texture coordinate basis to apply when sampling the texture.
		};

		MaterialType materialType;                 ///< How to interpret the material parameters when shading.
		vec4 baseColorFactor;                      ///< Base color factor of the material.
		float occlusionStrength;                   ///< Occlusion strength of the material.
		float roughnessFactor;                     ///< Roughness factor of the material.
		float metallicFactor;                      ///< Metallic factor of the material.
		float normalScale;                         ///< Normal scale of the material.
		vec3 emissiveFactor;                       ///< Emissive factor of the material.
		float alphaCutoff;                         ///< Alpha cutoff value of the material.
		float indexOfRefraction;                   ///< Index of refraction of the material.
		TextureInfo baseColorMap;                  ///< Base color map of the material (sRGB-encoded), or an out-of-range index for a fully white texture.
		TextureInfo occlusionRoughnessMetallicMap; ///< Occlusion-roughness-metallic map of the material (linearly encoded), or an out-of-range index for a fully white texture.
		TextureInfo normalMap;                     ///< Normal map of the material (linearly encoded), or an out-of-range index for a flat normal texture.
		TextureInfo emissiveMap;                   ///< Emissive map of the material (sRGB-encoded), or an out-of-range index for a fully white texture.
		FragmentFlags fragmentFlags;               ///< Fragment flags of the material.
	};

	/** Mesh instance in a model. */
	struct Instance {
		/** Index of the instance's material, or an out-of-range index if the instance uses the default material. */
		MaterialIndex materialIndex;

		/** Index of the instance's mesh. */
		MeshIndex meshIndex;

		/**
		 * Index of the first 4-byte value in the skin data in the model.
		 *
		 * After this offset, the skin data is stored in the following order:
		 * - Inverse bind-pose matrices, `model.bindPose.localJoints.size()` values of type 'mat4', if the mesh's `vertexFlags` contains `VERTEX_SKINNED`.
		 *
		 * \note The `vertexFlags` are stored in the instance's mesh, which is
		 *       referenced by #meshIndex.
		 */
		ValueOffset skinDataOffset;

		/** Offset of the initial morph target weights of the instance, or 0 if not morphed. */
		MorphTargetWeightIndex morphTargetWeightOffset;

		/** Index of the joint that the instance is attached to. */
		JointIndex jointIndex;

		/** Instance flags of the instance. */
		InstanceFlags instanceFlags;
	};

	/** Parameters describing a directional light. */
	struct DirectionalLight {
		vec3 color;      ///< Color of the light in linear RGB space.
		float intensity; ///< Illuminance of the light, in lux, if it were colored pure white.
	};

	/** Parameters describing a point light. */
	struct PointLight {
		vec3 color;      ///< Color of the light in linear RGB space.
		float intensity; ///< Luminous intensity of the light, in candela, if it were colored pure white.
		float range;     ///< Distance cutoff where the light's intensity reaches zero, or a non-positive value for infinite range.
	};

	/** Parameters describing a spot light. */
	struct SpotLight {
		vec3 color;           ///< Color of the light in linear RGB space.
		float intensity;      ///< Luminous intensity of the light, in candela, if it were colored pure white.
		float range;          ///< Distance cutoff where the light's intensity reaches zero, or a non-positive value for infinite range.
		float innerConeAngle; ///< Angle, in radians, where the light cone reaches maximum brightness.
		float outerConeAngle; ///< Angle, in radians, where the light cone fades to zero.
	};

	/** Light source in a model. */
	struct Light : Variant<DirectionalLight, PointLight, SpotLight> {
		JointIndex jointIndex; ///< Index of the model joint that controls the position and direction of the light.
	};

	/** Parameters describing a plane shape. */
	struct PlaneShape {
		float sizeX;      ///< Size of the plane in the local X axis.
		float sizeZ;      ///< Size of the plane in the local Z axis.
		bool doubleSided; ///< Whether the plane is double-sided or not.
	};

	/** Parameters describing a sphere shape. */
	struct SphereShape {
		float radius; ///< Radius of the sphere.
	};

	/** Parameters describing a box shape. */
	struct BoxShape {
		vec3 size; ///< Extents of the box in each axis in local space.
	};

	/** Parameters describing a cylinder shape. */
	struct CylinderShape {
		float halfLength;   ///< Half of the height of the cylinder, centered along the Y axis.
		float bottomRadius; ///< Radius of the bottom of the cylinder (the disk located along -Y.).
		float topRadius;    ///< Radius of the top of the cylinder (the disk located along +Y.).
	};

	/** Parameters describing a capsule shape. */
	struct CapsuleShape {
		float halfLength;   ///< Half of the distance between the centers of the two capping spheres of capsule.
		float bottomRadius; ///< Radius of the sphere located at the bottom of the capsule (i.e. the sphere at the half-height along -Y).
		float topRadius;    ///< Radius of the sphere located at the top of the capsule (i.e. the sphere at the half-height along +Y).
	};

	/** Parameters describing a convex polytope shape. */
	using ConvexPolytopeShape = SharedPointer<ConvexPolytope3D>;

	/** Parameters describing a triangle mesh shape. */
	using TriangleMeshShape = SharedPointer<TriangleMesh3D>;

	/** Collision shape in a model. */
	using Shape = Variant<PlaneShape, SphereShape, BoxShape, CylinderShape, CapsuleShape, ConvexPolytopeShape, TriangleMeshShape>;

	/** Collider in a model. */
	struct Collider {
		Shape shape;                       ///< Shape of the collider.
		CollisionLayers layers;            ///< Collision layers that the collider belongs to.
		CollisionLayers detectionLayers;   ///< Collision layers that the collider detects collisions with.
		CollisionLayers noDetectionLayers; ///< Collision layers that the collider skips collision detection with.
		CollisionLayers responseLayers;    ///< Collision layers that the collider responds to collisions with.
		CollisionLayers noResponseLayers;  ///< Collision layers that the collider skips collision response with.
	};

	/**
	 * Friction combination algorithm.
	 *
	 * When two objects collide, the friction combination algorithm with the
	 * lowest value of the two should be used.
	 */
	enum class FrictionCombine : uint8_t {
		AVERAGE = 0,  ///< The two values should be averaged.
		MINIMUM = 1,  ///< The smallest of the two values should be used.
		MAXIMUM = 2,  ///< The largest of the two values should be used.
		MULTIPLY = 3, ///< The two values should be multiplied with each other.
	};

	/**
	 * Restitution combination algorithm.
	 *
	 * When two objects collide, the restitution combination algorithm with the
	 * lowest value of the two should be used.
	 */
	enum class RestitutionCombine : uint8_t {
		AVERAGE = 0,  ///< The two values should be averaged.
		MINIMUM = 1,  ///< The smallest of the two values should be used.
		MAXIMUM = 2,  ///< The largest of the two values should be used.
		MULTIPLY = 3, ///< The two values should be multiplied with each other.
	};

	/** Physics object contained in a model. */
	struct PhysicsObject {
		JointIndex jointIndex;                 ///< Index of the model joint that is affected by the object.
		float mass;                            ///< Mass of the object, or 0 to calculate.
		vec3 centerOfMass;                     ///< Local center of mass of the object.
		vec3 principalMomentsOfInertia;        ///< Diagonal of the local moment of inertia tensor of the object, or 0 to calculate.
		quat inertiaOrientation;               ///< Orientation of the local moment of inertia tensor of the object.
		vec3 initialLinearVelocity;            ///< Initial linear velocity of the object in local space.
		vec3 initialAngularVelocity;           ///< Initial angular velocity of the object in local space.
		float gravityFactor;                   ///< Gravity factor of the object.
		float staticFriction;                  ///< Static friction of the object.
		float dynamicFriction;                 ///< Dynamic friction of the object.
		float rollingResistance;               ///< Rolling resistance of the object.
		float restitution;                     ///< Coefficient of restitution of the object.
		FrictionCombine frictionCombine;       ///< How to combine the friction when this object collides with another.
		RestitutionCombine restitutionCombine; ///< How to combine the restitution when this object collides with another.
	};

	/** Physics joint between two physics objects in a model. */
	struct PhysicsJoint {
		Pair<PhysicsObjectIndex> objectIndices; ///< Ordered pair of indices of the connected physics objects, or the maximum value if there is no object.
		Pair<JointIndex> jointIndices;          ///< Ordered pair of indices of the joints serving as the attachment frames.
		bool driveIgnoresMassX : 1;             ///< Whether the value computed from the spring equation is the linear acceleration or the force to apply along the X axis.
		bool driveIgnoresMassY : 1;             ///< Whether the value computed from the spring equation is the linear acceleration or the force to apply along the Y axis.
		bool driveIgnoresMassZ : 1;             ///< Whether the value computed from the spring equation is the linear acceleration or the force to apply along the Z axis.
		bool driveIgnoresMomentOfInertiaX : 1;  ///< Whether the value computed from the spring equation is the angular acceleration or the torque to apply about the X axis.
		bool driveIgnoresMomentOfInertiaY : 1;  ///< Whether the value computed from the spring equation is the angular acceleration or the torque to apply about the Y axis.
		bool driveIgnoresMomentOfInertiaZ : 1;  ///< Whether the value computed from the spring equation is the angular acceleration or the torque to apply about the Z axis.
		bool enableCollision : 1;               ///< Allow the connected objects to collide.
		vec3 minDistances;                      ///< The minimum of the allowed range of relative distance along each axis.
		vec3 maxDistances;                      ///< The maximum of the allowed range of relative distance along each axis.
		vec3 minAngles;                         ///< The minimum of the allowed range of relative angle about each axis.
		vec3 maxAngles;                         ///< The maximum of the allowed range of relative angle about each axis.
		vec3 linearStiffnesses;                 ///< The spring constants used to calculate a restorative force along each axis when the joint is extended beyond the limit.
		vec3 angularStiffnesses;                ///< The spring constants used to calculate a restorative torque about each axis when the joint is extended beyond the limit.
		vec3 linearDamping;                     ///< Damping applied to the linear velocity along each axis when the joint is extended beyond the limit.
		vec3 angularDamping;                    ///< Damping applied to the angular velocity about each axis when the joint is extended beyond the limit.
		vec3 maxForce;                          ///< Maximum force applied along each driven linear axis.
		vec3 maxTorque;                         ///< Maximum torque applied along each driven angular axis.
		vec3 targetPosition;                    ///< Target translation along each axis that the drive attempts to achieve.
		vec3 targetAngles;                      ///< Target angle about each axis that the drive attempts to achieve.
		vec3 targetLinearVelocity;              ///< Target linear velocity along each axis that this drive attempts to achieve.
		vec3 targetAngularVelocity;             ///< Target angular velocity about each axis that this drive attempts to achieve.
		vec3 linearDriveDamping;                ///< The damping of the linear drive, scaling the force based on the target linear velocity.
		vec3 angularDriveDamping;               ///< The damping of the angular drive, scaling the torque based on the target angular velocity.
	};

	/** State of a local joint in a pose of a model. */
	struct Joint {
		vec3 translation; ///< Local position of the joint.
		quat rotation;    ///< Local orientation of the joint.
		vec3 scale;       ///< Local scale of the joint.
		bool visible;     ///< Whether the joint and its children are currently visible or not.
	};

	/** Target type of a model animation channel. */
	enum class AnimationPath : uint8_t {
		JOINT_ROTATION,       ///< The animation targets the rotation of a local joint.
		JOINT_SCALE,          ///< The animation targets the scale of a local joint.
		JOINT_TRANSLATION,    ///< The animation targets the translation of a local joint.
		MORPH_TARGET_WEIGHTS, ///< The animation targets the morph target weights of the model.
	};

	/** Interpolation type of a model animation channel. */
	enum class InterpolationMode : uint8_t {
		LINEAR,       ///< Linear interpolation.
		STEP,         ///< Discrete steps without interpolation.
		CUBIC_SPLINE, ///< Cubic spline interpolation.
	};

	/** Channel of a model animation. */
	struct AnimationChannel {
		float minTimePoint;                                  ///< Start time of keyframes.
		float maxTimePoint;                                  ///< End time of keyframes.
		KeyframeCount keyframeCount;                         ///< Number of keyframes.
		ValueOffset keyframeInputTimePointOffset;            ///< Offset of the keyframe input time points.
		ValueOffset keyframeOutputValueOffset;               ///< Offset of the keyframe output values.
		uint32_t targetOffset;                               ///< Index into the joint array or morph target weight array.
		MorphTargetWeightCount targetMorphTargetWeightCount; ///< Number of affected morph target weights.
		AnimationPath targetPath;                            ///< Target type.
		InterpolationMode interpolationMode;                 ///< Interpolation type.
	};

	/** Animation of a model. */
	struct Animation {
		/** Start time of all animation channels. */
		float minTimePoint;

		/** End time of all animation channels. */
		float maxTimePoint;

		/**
		 * Index of the first animation channel.
		 * The number of channels in the animation is derived from the distance
		 * to the subsequent animation's offset (or the end of the animation
		 * channel array for the last animation).
		 */
		AnimationChannelIndex animationChannelOffset;
	};

	/** Transparent hash function object type for strings. */
	struct TransparentStringHash {
		using is_transparent = void;

		[[nodiscard]] size_t operator()(StringView string) const {
			return getHash(string);
		}
	};

	/** Transparent equality comparison function object type for strings. */
	struct TransparentStringEqual {
		using is_transparent = void;

		[[nodiscard]] bool operator()(StringView a, StringView b) const {
			return a == b;
		}
	};

	/** Map from name strings to joint indices. */
	using JointMap = HashMap<String, JointIndex, TransparentStringHash, TransparentStringEqual>;

	/** Map from name strings to animation indices. */
	using AnimationMap = HashMap<String, AnimationIndex, TransparentStringHash, TransparentStringEqual>;

	/** Map from name strings to texture indices. */
	using TextureMap = HashMap<String, TextureIndex, TransparentStringHash, TransparentStringEqual>;

	/** Map from name strings to material indices. */
	using MaterialMap = HashMap<String, MaterialIndex, TransparentStringHash, TransparentStringEqual>;

	/** Map from name strings to light indices. */
	using LightMap = HashMap<String, LightIndex, TransparentStringHash, TransparentStringEqual>;

	/** Map from name strings to collision layer indices. */
	using CollisionLayerMap = HashMap<String, CollisionLayerIndex, TransparentStringHash, TransparentStringEqual>;

	/**
	 * Non-owning read-only view over a set of animation channels and their
	 * associated data.
	 */
	struct AnimationView {
		/** Start time of all animation channels. */
		float minTimePoint = 0.0f;

		/** End time of all animation channels. */
		float maxTimePoint = 0.0f;

		/** Non-owning read-only view over the animation channels. */
		Span<const AnimationChannel> channels{};

		/**
		 * Non-owning read-only view over the keyframe input time points
		 * indexed by the animation channels.
		 */
		Span<const float> keyframeInputTimePoints{};

		/**
		 * Non-owning read-only view over the keyframe output value data indexed
		 * by the animation channels.
		 */
		Span<const byte> keyframeOutputValueData{};
	};

	/**
	 * State of an animation affecting a pose.
	 */
	struct AnimationState {
		/**
		 * Animation to apply.
		 *
		 * All joint and morph target weight indices referenced by the animation
		 * must be in range of the initial joints and morph target weights of
		 * the pose being animated.
		 */
		AnimationView animation;

		/**
		 * Current animation time from the start of the animation.
		 */
		Duration time{};

		/**
		 * Normalized overall blend weight of this animation's influence on the
		 * pose.
		 */
		float blendWeight = 1.0f;

		/**
		 * Normalized blend weights of each joint's influence on the pose.
		 *
		 * Must either be empty or have a size equal to the number of joints in
		 * the pose being animated.
		 */
		Span<const float> jointBlendWeights{};

		/**
		 * Normalized blend weights of each morph target weight's influence on
		 * the pose.
		 *
		 * Must either be empty or have a size equal to the number of morph
		 * target weights in the pose being animated.
		 */
		Span<const float> morphTargetWeightBlendWeights{};

		/**
		 * Whether the animation time should be wrapped or clamped when out of
		 * range of the animation.
		 */
		bool looping = true;
	};

	/**
	 * Read-only view over a local pose of a model.
	 */
	struct PoseView {
		/**
		 * Non-owning read-only pointer to the current local joints of the pose.
		 */
		const Joint* localJoints;

		/**
		 * Non-owning read-only pointer to the current local morph target
		 * weights of the pose.
		 */
		const float* localMorphTargetWeights;
	};

	/**
	 * Reference to a local pose of a model.
	 */
	struct PoseReference {
		/**
		 * Non-owning pointer to the current local joints of the pose.
		 */
		Joint* localJoints;

		/**
		 * Non-owning pointer to the current local morph target weights of the
		 * pose.
		 */
		float* localMorphTargetWeights;

		/**
		 * Get a read-only view over the pose.
		 *
		 * \return the pose view.
		 */
		constexpr operator PoseView() const noexcept {
			return PoseView{.localJoints = localJoints, .localMorphTargetWeights = localMorphTargetWeights};
		}

		/**
		 * Add the influence of an animation state to the current pose.
		 *
		 * \param animationState animation state to apply to the joints and
		 *        morph target weights.
		 *
		 * \return `*this`, for chaining.
		 */
		GREM_API(resource) PoseReference applyAnimation(const AnimationState& animationState) const; // NOLINT(modernize-use-nodiscard)
	};

	/**
	 * Local pose of a model.
	 */
	struct Pose {
		ArrayList<Joint> localJoints{};             ///< Current local joints of the pose.
		ArrayList<float> localMorphTargetWeights{}; ///< Current local morph target weights of the pose.

		/**
		 * Get a reference to the pose.
		 *
		 * \return the pose reference.
		 */
		constexpr operator PoseReference() noexcept {
			return PoseReference{.localJoints = localJoints.data(), .localMorphTargetWeights = localMorphTargetWeights.data()};
		}

		/**
		 * Get a read-only view over the pose.
		 *
		 * \return the pose view.
		 */
		constexpr operator PoseView() const noexcept {
			return PoseView{.localJoints = localJoints.data(), .localMorphTargetWeights = localMorphTargetWeights.data()};
		}

		/**
		 * Add the influence of an animation state to the current pose.
		 *
		 * \param animationState animation state to apply to the joints and
		 *        morph target weights.
		 *
		 * \return `*this`, for chaining.
		 */
		Pose& applyAnimation(const AnimationState& animationState) {
			static_cast<PoseReference>(*this).applyAnimation(animationState);
			return *this;
		}
	};

	/**
	 * Read-only view over a world-space transformation of a model.
	 */
	struct TransformationView {
		const mat4* jointMatrices;       ///< Non-owning read-only pointer to the world-space transformations of the joints in the model.
		const bool* jointsVisible;       ///< Non-owning read-only pointer to the visibility flags of the joints in the model.
		const float* morphTargetWeights; ///< Non-owning read-only pointer to the morph target weights of the model.
	};

	/**
	 * Reference to a world-space transformation of a model.
	 */
	struct TransformationReference {
		mat4* jointMatrices;       ///< Non-owning pointer to the world-space transformations of the joints in the model.
		bool* jointsVisible;       ///< Non-owning pointer to the visibility flags of the joints in the model.
		float* morphTargetWeights; ///< Non-owning pointer to the morph target weights of the model.

		/**
		 * Get a read-only view over the transformation.
		 *
		 * \return the transformation view.
		 */
		constexpr operator TransformationView() const noexcept {
			return TransformationView{.jointMatrices = jointMatrices, .jointsVisible = jointsVisible, .morphTargetWeights = morphTargetWeights};
		}

		/**
		 * Skin the non-root joints of the transformation according to a local
		 * pose, relative to the current root joint(s) of the transformation,
		 * and add a set of morph target weights to the current morph target
		 * weights of the transformation.
		 *
		 * \param localJoints local joints of the pose. Must have a size equal
		 *        to the number of joint matrices in the transformation.
		 * \param localMorphTargetWeights local morph target weights of the
		 *        pose. Must have a size equal to the number of morph target
		 *        weights in the transformation.
		 * \param modelJointParentIndices indices of each joint's parent joint
		 *        in the joints array. Must have the same size as the joints
		 *        array of the new pose. Each parent index, except the first,
		 *        must be less than its own index. The first parent index must
		 *        be 0.
		 */
		GREM_API(resource) void pose(Span<const Joint> localJoints, Span<const float> localMorphTargetWeights, Span<const JointIndex> modelJointParentIndices) const;
	};

	/**
	 * World-space transformation of a model.
	 */
	struct Transformation {
		ArrayList<mat4> jointMatrices{};       ///< World-space transformations of the joints in the model.
		ArrayList<bool> jointsVisible{};       ///< Whether each joint in the model is currently visible.
		ArrayList<float> morphTargetWeights{}; ///< Morph target weights of the model.

		/**
		 * Get a reference to the transformation.
		 *
		 * \return the transformation reference.
		 */
		constexpr operator TransformationReference() noexcept {
			return TransformationReference{.jointMatrices = jointMatrices.data(), .jointsVisible = jointsVisible.data(), .morphTargetWeights = morphTargetWeights.data()};
		}

		/**
		 * Get a read-only view over the transformation.
		 *
		 * \return the transformation view.
		 */
		constexpr operator TransformationView() const noexcept {
			return TransformationView{.jointMatrices = jointMatrices.data(), .jointsVisible = jointsVisible.data(), .morphTargetWeights = morphTargetWeights.data()};
		}

		/**
		 * Assign new joints and morph target weights to the transformation
		 * according to a local pose, relative to a new root transformation.
		 *
		 * \param rootTransformation world-space transformation of the root
		 *        joint.
		 * \param localJoints local joints of the pose. Must have a size equal
		 *        to the number of joint matrices in the transformation.
		 * \param localMorphTargetWeights morph target weights of the pose. Must
		 *        have a size equal to the number of morph target weights in the
		 *        transformation.
		 * \param modelJointParentIndices indices of each joint's parent joint
		 *        in the joints array. Must have the same size as the joints
		 *        array of the new pose. Each parent index, except the first,
		 *        must be less than its own index. The first parent index must
		 *        be 0.
		 */
		void assign(const mat4& rootTransformation, Span<const Joint> localJoints, Span<const float> localMorphTargetWeights, Span<const JointIndex> modelJointParentIndices) {
			const size_t jointCount = localJoints.size();
			GREM_ASSERT(modelJointParentIndices.size() == jointCount);
			// Make sure resize() doesn't over-allocate, since jointMatrices and jointsVisible are unlikely to grow.
			// Without this, we'd just be wasting memory (and cache) in the typical case.
			if (jointCount > jointMatrices.capacity()) {
				[[unlikely]];
				jointMatrices.reserve(jointCount);
				jointsVisible.reserve(jointCount);
			}
			jointMatrices.resize(jointCount);
			jointMatrices.front() = rootTransformation;
			jointsVisible.resize(jointCount);
			jointsVisible.front() = true;
			morphTargetWeights.assign(localMorphTargetWeights.size(), 0.0f);
			pose(localJoints, localMorphTargetWeights, modelJointParentIndices);
		}

		/**
		 * Skin the non-root joints of the transformation according to a local
		 * pose, relative to the current root joint(s) of the transformation,
		 * and add a set of morph target weights to the current morph target
		 * weights of the transformation.
		 *
		 * \param localJoints local joints of the pose. Must have a size equal
		 *        to the number of joint matrices in the transformation.
		 * \param localMorphTargetWeights local morph target weights of the
		 *        pose. Must have a size equal to the number of morph target
		 *        weights in the transformation.
		 * \param modelJointParentIndices indices of each joint's parent joint
		 *        in the joints array. Must have the same size as the joints
		 *        array of the new pose. Each parent index, except the first,
		 *        must be less than its own index. The first parent index must
		 *        be 0.
		 */
		void pose(Span<const Joint> localJoints, Span<const float> localMorphTargetWeights, Span<const JointIndex> modelJointParentIndices) {
			GREM_ASSERT(localJoints.size() == jointMatrices.size());
			GREM_ASSERT(localMorphTargetWeights.size() == morphTargetWeights.size());
			GREM_ASSERT(modelJointParentIndices.size() == localJoints.size());
			static_cast<TransformationReference>(*this).pose(localJoints, localMorphTargetWeights, modelJointParentIndices);
		}
	};

	/**
	 * Generate normal vectors for each vertex in a set of triangles.
	 *
	 * \param normalData strided view of the byte offsets at which to write the
	 *        generated normal vectors. Must have the same size as `positions`,
	 *        and a stride that is greater than or equal to the size of a
	 *        iA2B10G10R10vec4norm.
	 * \param positions positions of the vertices to generate normals for.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	GREM_API(resource) static void generateTriangleNormals(StridedSpan<byte> normalData, StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices);

	/**
	 * Generate normal vectors for each vertex in a set of triangles.
	 *
	 * \param normalData view over the memory to write the generated normal
	 *        vectors to. Must have the same size as `positions` times the size
	 *        of a iA2B10G10R10vec4norm.
	 * \param positions positions of the vertices to generate normals for.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	static void generateTriangleNormals(Span<byte> normalData, StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices) {
		GREM_ASSERT(normalData.size_bytes() % sizeof(iA2B10G10R10vec4norm) == 0);
		generateTriangleNormals(StridedSpan{normalData.data(), normalData.size_bytes() / sizeof(iA2B10G10R10vec4norm), sizeof(iA2B10G10R10vec4norm)}, positions, indices);
	}

	/**
	 * Generate normal vectors for each vertex in a set of triangles.
	 *
	 * \param normals normal vectors to generate the values of. Must have the
	 *        same size as `positions`.
	 * \param positions positions of the vertices to generate normals for.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	static void generateTriangleNormals(StridedSpan<iA2B10G10R10vec4norm> normals, StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices) {
		generateTriangleNormals(as_strided_writable_bytes(normals), positions, indices);
	}

	/**
	 * Generate tangent vectors for each vertex in a set of triangles.
	 *
	 * \param tangentData strided view of the byte offsets at which to write the
	 *        generated tangent vectors. Must have the same size as `positions`,
	 *        and a stride that is greater than or equal to the size of a
	 *        iA2B10G10R10vec4norm.
	 * \param positions positions of the vertices to generate tangents for.
	 * \param normals normal vectors of the vertices to generate tangents for.
	 *        Must have the same size as `positions`.
	 * \param textureCoordinates optional texture coordinates of the vertices to
	 *        generate tangents for, which help guide the generated tangent
	 *        directions. Must either be empty or have the same size as
	 *        `positions`.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	GREM_API(resource)
	static void generateTriangleTangents(StridedSpan<byte> tangentData, StridedSpan<const vec3> positions, StridedSpan<const iA2B10G10R10vec4norm> normals,
		StridedSpan<const vec2> textureCoordinates, StridedSpan<const uint32_t> indices);

	/**
	 * Generate tangent vectors for each vertex in a set of triangles.
	 *
	 * \param tangentData view over the memory to write the generated tangent
	 *        vectors to. Must have the same size as `positions` times the size
	 *        of a iA2B10G10R10vec4norm.
	 * \param positions positions of the vertices to generate tangents for.
	 * \param normals normal vectors of the vertices to generate tangents for.
	 *        Must have the same size as `positions`.
	 * \param textureCoordinates optional texture coordinates of the vertices to
	 *        generate tangents for, which help guide the generated tangent
	 *        directions. Must either be empty or have the same size as
	 *        `positions`.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	static void generateTriangleTangents(Span<byte> tangentData, StridedSpan<const vec3> positions, StridedSpan<const iA2B10G10R10vec4norm> normals,
		StridedSpan<const vec2> textureCoordinates, StridedSpan<const uint32_t> indices) {
		GREM_ASSERT(tangentData.size_bytes() % sizeof(iA2B10G10R10vec4norm) == 0);
		generateTriangleTangents(StridedSpan{tangentData.data(), tangentData.size_bytes() / sizeof(iA2B10G10R10vec4norm), sizeof(iA2B10G10R10vec4norm)}, positions, normals,
			textureCoordinates, indices);
	}

	/**
	 * Generate tangent vectors for each vertex in a set of triangles.
	 *
	 * \param tangents tangent vectors to generate the values of. Must have the
	 *        same size as `positions`.
	 * \param positions positions of the vertices to generate tangents for.
	 * \param normals normal vectors of the vertices to generate tangents for.
	 *        Must have the same size as `positions`.
	 * \param textureCoordinates optional texture coordinates of the vertices to
	 *        generate tangents for, which help guide the generated tangent
	 *        directions. Must either be empty or have the same size as
	 *        `positions`.
	 * \param indices optional vertex indices.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the number of indices is 0, each 3 consecutive vertices form a
	 *       triangle. Otherwise, each 3 consecutive indices specify 3 vertices
	 *       that form a triangle. The triangles are specified in
	 *       counter-clockwise winding order.
	 *
	 * \warning If the number of indices is 0, the number of vertex positions
	 *          must be divisible by 3. Otherwise, the number of indices must be
	 *          divisible by 3.
	 * \warning Each given index must be in-range, i.e. have a value that is
	 *          less than the number of vertex positions.
	 */
	static void generateTriangleTangents(StridedSpan<iA2B10G10R10vec4norm> tangents, StridedSpan<const vec3> positions, StridedSpan<const iA2B10G10R10vec4norm> normals,
		StridedSpan<const vec2> textureCoordinates, Span<const uint32_t> indices) {
		generateTriangleTangents(as_strided_writable_bytes(tangents), positions, normals, textureCoordinates, indices);
	}

	/**
	 * Determine which model file type a file should be parsed as based on the
	 * contents at the beginning of the file.
	 *
	 * \param fileContents file contents to be parsed.
	 *
	 * \return the determined model file type, or ModelFileType::UNKNOWN if a
	 *         type could not be determined.
	 */
	[[nodiscard]] GREM_API(resource) static ModelFileType determineFileType(Span<const byte> fileContents) noexcept;

	Buffer<byte> meshData{};                                   ///< Mesh data of the model. Indexed by #meshes.
	Buffer<byte> morphTargetData{};                            ///< Morph target data of the model. Indexed by #meshes.
	Buffer<byte> skinData{};                                   ///< Skin data of the model. Indexed by #instances.
	ArrayList<Mesh> meshes{};                                  ///< Layout of mesh data in the model. Indexed by #instances.
	Pose bindPose{};                                           ///< Initial bind pose of the model. Indexed by #instances, #lights, #physicsObjects, #jointMap and #skinMap.
	ArrayList<JointIndex> jointParentIndices{};                ///< Indices of the parent joint of each joint of the model.
	ArrayList<Optional<Collider>> jointColliders{};            ///< Colliders of each joint in the model.
	ArrayList<PhysicsObjectIndex> jointPhysicsObjectIndices{}; ///< Indices of the physics object that each joint in the model belongs to, or the maximum value if none.
	JointCount staticJointCount = 0;                           ///< Number of unskinned joints at the beginning of the joint array.
	ArrayList<Animation> animations{};                         ///< Animations of the model. Indexed by #animationMap.
	ArrayList<AnimationChannel> animationChannels{};           ///< Animation channels of the model. Indexed by #animations.
	ArrayList<float> keyframeInputTimePoints{};                ///< Keyframe input time points of animation channels. Indexed by #animationChannels.
	Buffer<byte> keyframeOutputValueData{};                    ///< Keyframe output value data of animation channels. Indexed by #animationChannels.
	ArrayList<Texture> textures{};                             ///< Textures of the model. Indexed by #materials and #textureMap.
	ArrayList<Material> materials{};                           ///< Materials of the model. Indexed by #instances and #materialMap.
	ArrayList<Instance> instances{};                           ///< Mesh instances of the model.
	ArrayList<Light> lights{};                                 ///< Lights in the model. Indexed by #lightMap.
	ArrayList<PhysicsObject> physicsObjects{};                 ///< Physics objects in the model. Indexed by #physicsJoints and #jointPhysicsObjectIndices.
	ArrayList<PhysicsJoint> physicsJoints{};                   ///< Physics joints in the model.
	Box<3, float> bindPoseBoundingBox{};                       ///< Local axis-aligned bounding box of all meshes' vertex positions in the initial bind pose.
	float bindPoseBoundingRadius = 0.0f;                       ///< Maximum distance from the origin of all meshes' vertex positions in the initial bind pose.
	JointMap jointMap{};                                       ///< Map from joint names to their index in the joint array.
	JointMap skinMap{};                                        ///< Map from skin names to the index of their root joint in the joint array.
	AnimationMap animationMap{};                               ///< Map from animation names to their index in the animation array.
	TextureMap textureMap{};                                   ///< Map from texture names to their index in the texture array.
	MaterialMap materialMap{};                                 ///< Map from material names to their index in the material array.
	LightMap lightMap{};                                       ///< Map from light names to their index in the light array.
	CollisionLayerMap collisionLayerMap{};                     ///< Map from collision layer names to their index in a collision layer set.

	/**
	 * Construct an empty, invalid model that must not be rendered until
	 * manually made valid.
	 *
	 * \warning Only use this constructor if you intend to reassign the model or
	 *          fill out all of its fields manually before use, such as if
	 *          loading from a custom file format.
	 */
	Model() noexcept = default;

	/**
	 * Construct a basic single-mesh model from raw mesh data.
	 *
	 * \param positions vertex positions of the mesh.
	 * \param indices triangle vertex indices of the mesh. Must have a size that
	 *        is divisible by 3. Each index must be less than
	 *        `positions.size()`.
	 * \param normals normals of the vertices. Must either be empty or have the
	 *        same size as `positions`. If empty, normals will be generated
	 *        automatically.
	 * \param tangents tangents of the vertices. Must either be empty or have
	 *        the same size as `positions`. If empty, tangents will be generated
	 *        automatically.
	 * \param textureCoordinatesChannel0 channel 0 texture coordinates of the
	 *        vertices. Must either be empty or have the same size as
	 *        `positions`.
	 * \param textureCoordinatesChannel1 channel 1 texture coordinates of the
	 *        vertices. Must either be empty or have the same size as
	 *        `positions`.
	 * \param colors colors of the vertices. Must either be empty or have the
	 *        same size as `positions`.
	 * \param material material of the mesh, or an empty optional to use the
	 *        default material. If specified, its fragment flags must be valid
	 *        for the given combination of texture coordinate channels.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource)
	Model(StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices, StridedSpan<const iA2B10G10R10vec4norm> normals = {},
		StridedSpan<const iA2B10G10R10vec4norm> tangents = {}, StridedSpan<const vec2> textureCoordinatesChannel0 = {}, StridedSpan<const vec2> textureCoordinatesChannel1 = {},
		StridedSpan<const u8vec4norm> colors = {}, const Optional<Material>& material = {});

	/**
	 * Construct an untextured single-mesh model from a convex polytope.
	 *
	 * \param polytope convex polytope to model.
	 * \param transformation transformation to apply to each of the polytope
	 *        vertices in the new model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) Model(const ConvexPolytope3D& polytope, const mat4& transformation = mat4{1.0f});

	/**
	 * Load a model from a file.
	 *
	 * The supported file formats are:
	 * - glTF 2.0 (.gltf)
	 * - Binary glTF 2.0 (.glb)
	 * - Wavefront OBJ (.obj)
	 *
	 * \param filesystem filepath to load the files from.
	 * \param filepath input filepath of the model file to load.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 * \note Any material libraries, data files and texture image files required
	 *       by the model are also loaded as needed. See the documentation of
	 *       Image for a description of the supported image file formats.
	 *
	 * \remark When authoring models, the glTF format should generally be
	 *         preferred over OBJ, since glTF is more memory efficient, faster
	 *         to load, properly standardized and supports features such as PBR
	 *         materials and animation.
	 */
	GREM_API(resource) Model(const Filesystem& filesystem, CStringView filepath, const ModelOptions& options = {});

	/**
	 * Construct a model from a loaded glTF asset.
	 *
	 * \param asset glTF asset to construct the model from.
	 * \param loadImage function to use to load the images required by the asset
	 *        given their relative filepaths and image options.
	 * \param loadBufferData function to use to load the buffer contents
	 *        required by the asset given their relative filepaths.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the loadImage callback.
	 * \throws any exception thrown by the loadBufferData callback.
	 */
	GREM_API(resource)
	Model(const gltf::Asset& asset, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
		FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData, const ModelOptions& options = {});

	/**
	 * Construct a model from a loaded OBJ asset.
	 *
	 * \param asset OBJ asset to construct the model from.
	 * \param materialLibrary material library to get materials from, which
	 *        should contain the materials loaded from the files specified by
	 *        obj::Asset::materialLibraryFilenames.
	 * \param loadImage function to use to load the images required by the
	 *        materials in the asset given their relative filepaths and image
	 *        options.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the loadImage callback.
	 */
	GREM_API(resource)
	Model(const obj::Asset& asset, const obj::mtl::Library& materialLibrary, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
		const ModelOptions& options = {});

	/**
	 * Reset the model to an empty, invalid state that must not be rendered
	 * until the model is manually made valid.
	 *
	 * \warning Only use this function if you intend to reassign the model or
	 *          fill out all of its fields manually before use, such as if
	 *          loading from a custom file format.
	 */
	void reset() noexcept {
		meshData.clear();
		morphTargetData.clear();
		skinData.clear();
		meshes.clear();
		bindPose.localJoints.clear();
		bindPose.localMorphTargetWeights.clear();
		jointParentIndices.clear();
		jointColliders.clear();
		jointPhysicsObjectIndices.clear();
		staticJointCount = 0;
		animations.clear();
		animationChannels.clear();
		keyframeInputTimePoints.clear();
		keyframeOutputValueData.clear();
		textures.clear();
		materials.clear();
		instances.clear();
		lights.clear();
		physicsObjects.clear();
		physicsJoints.clear();
		bindPoseBoundingBox = {};
		bindPoseBoundingRadius = 0.0f;
		jointMap.clear();
		skinMap.clear();
		animationMap.clear();
		textureMap.clear();
		materialMap.clear();
		lightMap.clear();
		collisionLayerMap.clear();
	}

	/**
	 * Assign a basic single-mesh model from raw mesh data.
	 *
	 * \param positions vertex positions of the mesh.
	 * \param indices triangle vertex indices of the mesh. Must have a size that
	 *        is divisible by 3. Each index must be less than
	 *        `positions.size()`.
	 * \param normals normals of the vertices. Must either be empty or have the
	 *        same size as `positions`. If empty, normals will be generated
	 *        automatically.
	 * \param tangents tangents of the vertices. Must either be empty or have
	 *        the same size as `positions`. If empty, tangents will be generated
	 *        automatically.
	 * \param textureCoordinatesChannel0 channel 0 texture coordinates of the
	 *        vertices. Must either be empty or have the same size as
	 *        `positions`.
	 * \param textureCoordinatesChannel1 channel 1 texture coordinates of the
	 *        vertices. Must either be empty or have the same size as
	 *        `positions`.
	 * \param colors colors of the vertices. Must either be empty or have the
	 *        same size as `positions`.
	 * \param material material of the mesh, or an empty optional to use the
	 *        default material. If specified, its fragment flags must be valid
	 *        for the given combination of texture coordinate channels.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource)
	void assign(StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices, StridedSpan<const iA2B10G10R10vec4norm> normals = {},
		StridedSpan<const iA2B10G10R10vec4norm> tangents = {}, StridedSpan<const vec2> textureCoordinatesChannel0 = {}, StridedSpan<const vec2> textureCoordinatesChannel1 = {},
		StridedSpan<const u8vec4norm> colors = {}, const Optional<Material>& material = {});

	/**
	 * Assign an untextured single-mesh model from a convex polytope.
	 *
	 * \param polytope convex polytope to model.
	 * \param transformation transformation to apply to each of the polytope
	 *        vertices in the new model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) void assign(const ConvexPolytope3D& polytope, const mat4& transformation = mat4{1.0f});

	/**
	 * Load and assign a model from a file.
	 *
	 * The supported file formats are:
	 * - glTF 2.0 (.gltf)
	 * - Binary glTF 2.0 (.glb)
	 * - Wavefront OBJ (.obj)
	 *
	 * \param filesystem filepath to load the files from.
	 * \param filepath input filepath of the model file to load.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 * \note Any material libraries, data files and texture image files required
	 *       by the model are also loaded as needed. See the documentation of
	 *       Image for a description of the supported image file formats.
	 *
	 * \remark When authoring models, the glTF format should generally be
	 *         preferred over OBJ, since glTF is more memory efficient, faster
	 *         to load, properly standardized and supports features such as PBR
	 *         materials and animation.
	 */
	GREM_API(resource) void load(const Filesystem& filesystem, CStringView filepath, const ModelOptions& options = {});

	/**
	 * Assign a model from a loaded glTF asset.
	 *
	 * \param asset glTF asset to assign the model from.
	 * \param loadImage function to use to load the images required by the asset
	 *        given their relative filepaths and image options.
	 * \param loadBufferData function to use to load the buffer contents
	 *        required by the asset given their relative filepaths.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the loadImage callback.
	 * \throws any exception thrown by the loadBufferData callback.
	 */
	GREM_API(resource)
	void load(const gltf::Asset& asset, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
		FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData, const ModelOptions& options = {});

	/**
	 * Assign a model from a loaded OBJ asset.
	 *
	 * \param asset OBJ asset to assign the model from.
	 * \param materialLibrary material library to get materials from, which
	 *        should contain the materials loaded from the files specified by
	 *        obj::Asset::materialLibraryFilenames.
	 * \param loadImage function to use to load the images required by the
	 *        materials in the asset given their relative filepaths and image
	 *        options.
	 * \param options model options, see ModelOptions.
	 *
	 * \throws resource::Error on failure to load a valid model.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the loadImage callback.
	 */
	GREM_API(resource)
	void load(const obj::Asset& asset, const obj::mtl::Library& materialLibrary, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
		const ModelOptions& options = {});

	/**
	 * Find the index of an animation with a specific name.
	 *
	 * \param name name of the animation to search for.
	 *
	 * \return the index of the given animation in the model's animation array,
	 *         or an empty optional if the animation wasn't found.
	 */
	[[nodiscard]] Optional<AnimationIndex> findAnimationIndex(StringView name) const noexcept {
		if (const auto it = animationMap.find(name); it != animationMap.end()) {
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
	[[nodiscard]] AnimationIndex getAnimationIndex(StringView name) const {
		if (const Optional<AnimationIndex> animationIndex = findAnimationIndex(name)) {
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
	[[nodiscard]] AnimationView getAnimationAtIndex(AnimationIndex index) const {
		if (static_cast<size_t>(index) >= animations.size()) {
			throw std::out_of_range{"Invalid animation index."};
		}
		const Animation& animation = animations[index];
		const AnimationChannelIndex animationChannelsBegin = animation.animationChannelOffset;
		const AnimationChannelIndex animationChannelsEnd =
			(index + 1 < animations.size()) ? animations[index + 1].animationChannelOffset : static_cast<AnimationChannelIndex>(animationChannels.size());
		return AnimationView{
			.minTimePoint = animation.minTimePoint,
			.maxTimePoint = animation.maxTimePoint,
			.channels = Span{animationChannels}.subspan(animationChannelsBegin, animationChannelsEnd - animationChannelsBegin),
			.keyframeInputTimePoints = keyframeInputTimePoints,
			.keyframeOutputValueData = keyframeOutputValueData,
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
	[[nodiscard]] Optional<AnimationView> findAnimation(StringView name) const noexcept {
		if (const Optional<AnimationIndex> animationIndex = findAnimationIndex(name)) {
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
	[[nodiscard]] Optional<JointIndex> findJointIndex(StringView name) const noexcept {
		if (const auto it = jointMap.find(name); it != jointMap.end()) {
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
	[[nodiscard]] Optional<JointIndex> findSkinRootJointIndex(StringView name) const noexcept {
		if (const auto it = skinMap.find(name); it != skinMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of a texture with a specific name.
	 *
	 * \param name name of the texture to search for.
	 *
	 * \return the index of the given texture in the model's texture array, or
	 *         an empty optional if the texture wasn't found.
	 */
	[[nodiscard]] Optional<TextureIndex> findTextureIndex(StringView name) const noexcept {
		if (const auto it = textureMap.find(name); it != textureMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of a material with a specific name.
	 *
	 * \param name name of the material to search for.
	 *
	 * \return the index of the given material in the model's material array, or
	 *         an empty optional if the material wasn't found.
	 */
	[[nodiscard]] Optional<MaterialIndex> findMaterialIndex(StringView name) const noexcept {
		if (const auto it = materialMap.find(name); it != materialMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of a light with a specific name.
	 *
	 * \param name name of the light to search for.
	 *
	 * \return the index of the given light in the model's light array, or an
	 *         empty optional if the light wasn't found.
	 */
	[[nodiscard]] Optional<LightIndex> findLightIndex(StringView name) const noexcept {
		if (const auto it = lightMap.find(name); it != lightMap.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Find the index of a collision layer with a specific name.
	 *
	 * \param name name of the collision layer to search for.
	 *
	 * \return the index of the given collision layer, or an empty optional if
	 *         the collision layer wasn't found.
	 */
	[[nodiscard]] Optional<CollisionLayerIndex> findCollisionLayerIndex(StringView name) const noexcept {
		if (const auto it = collisionLayerMap.find(name); it != collisionLayerMap.end()) {
			return it->second;
		}
		return {};
	}
};

} // namespace grem::resource

#endif
