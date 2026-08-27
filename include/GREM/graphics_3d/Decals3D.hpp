// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_DECALS_3D_HPP
#define GREM_GRAPHICS_3D_DECALS_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/resource/AtlasPacker.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer3D; // Forward declaration, to avoid a circular include of Renderer3D.hpp.
class Decals3D;   // Forward declaration.

/**
 * Opaque identifier for a specific decal material in a Decals3D set.
 */
struct DecalMaterialID {
	[[nodiscard]] constexpr bool operator==(const DecalMaterialID&) const noexcept = default;

private:
	friend Renderer3D;
	friend Decals3D;
	friend std::hash<DecalMaterialID>;

	constexpr explicit DecalMaterialID(uint32_t index) noexcept
		: index(index) {}

	uint32_t index;
};

/**
 * Opaque handle to a specific decal in a Decals3D set.
 */
struct DecalID {
	/**
     * Construct an invalid decal handle.
     */
	constexpr DecalID() noexcept = default;

	/**
	 * Check if this handle is potentially valid.
	 *
	 * \return true if this handle is potentially valid, false if it is equal to
	 *         a default-constructed invalid handle.
	 */
	constexpr explicit operator bool() const noexcept {
		return *this != DecalID{};
	}

	/**
	 * Compare this handle to another for equality.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return true if the handles are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const DecalID& other) const noexcept = default;

	/**
	 * Compare this handle to another.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return a strong ordering between the two handles.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const DecalID& other) const noexcept = default;

private:
	friend Renderer3D;
	friend Decals3D;
	friend std::hash<DecalID>;

	constexpr explicit DecalID(uint64_t value) noexcept
		: value(value) {}

	uint64_t value = 0;
};

/**
 * Configuration options for a decal material.
 */
struct DecalMaterialOptions {
	/**
	 * Base color map image of the decal material, or an empty image view to use
	 * a fully white texture.
	 *
	 * Must reference a valid sRGB-encoded image whose format can be used as a
	 * source image format for TextureFormat::R8G8B8A8_SRGB if non-empty.
	 */
	resource::ImageView baseColorMapImage{};

	/**
	 * Normal map image of the decal material, or an empty image view to use a
	 * flat normal texture.
	 *
	 * Must reference a valid linearly encoded image whose format can be used as
	 * a source image format for TextureFormat::R8G8B8A8_UNORM if non-empty.
	 */
	resource::ImageView normalMapImage{};

	/**
	 * Occlusion-roughness-metallic map image of the decal material, or an empty
	 * image view to use a fully white texture.
	 *
	 * Must reference a valid linearly encoded image whose format can be used as
	 * a source image format for TextureFormat::R8G8B8A8_UNORM if non-empty.
	 */
	resource::ImageView occlusionRoughnessMetallicMapImage{};

	/**
	 * Emissive map image of the decal material, or an empty image view to use a
	 * fully white texture.
	 *
	 * Must reference a valid sRGB-encoded image whose format can be used as a
	 * source image format for TextureFormat::R8G8B8A8_SRGB if non-empty.
	 */
	resource::ImageView emissiveMapImage{};

	/**
	 * Base color factor of the decal material.
	 */
	vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};

	/**
	 * Occlusion strength of the decal material.
	 */
	float occlusionStrength = 1.0f;

	/**
	 * Roughness factor of the decal material.
	 */
	float roughnessFactor = 1.0f;

	/**
	 * Metallic factor of the decal material.
	 */
	float metallicFactor = 1.0f;

	/**
	 * Normal scale of the decal material.
	 */
	float normalScale = 1.0f;

	/**
	 * Emissive factor of the decal material.
	 */
	vec3 emissiveFactor{0.0f, 0.0f, 0.0f};

	/**
	 * Convert the decal base color map image from straight to pre-multiplied
	 * alpha.
	 *
	 * \note Only applies to images in raw RGBA formats.
	 */
	bool convertToPremultipliedAlpha = true;
};

/**
 * Configuration options for a 3D decal.
 */
struct DecalOptions3D {
	/**
	 * Position, in world coordinates, to render the decal at.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Maximum range from the position, in world coordinates, where the decal
	 * will apply to meshes.
	 */
	float range = 0.5f;

	/**
	 * Rotation of the decal around its DecalOptions3D::origin.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Size of the decal, in world coordinates.
	 */
	vec2 size{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the decal. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the decal.
	 */
	vec2 origin{0.5f, 0.5f};

	/**
	 * Tint color to use in the shader.
	 *
	 * \note In the default shader, the output color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e.
	 *       RGBA(1, 1, 1, 1) in linear color, represents no modification to
	 *       the original texture color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive factor to use in the shader.
	 *
	 * \note In the default PBR shader, the emissive color is multiplied by
	 *       this value, meaning that a value of (1, 1, 1) represents no
	 *       modification to the original emissive map color.
	 */
	vec3 emissiveFactor{1.0f, 1.0f, 1.0f};

	/**
	 * Model instance identifier that the decal applies to, or the maximum value
	 * to apply to all instances.
	 */
	uint32_t modelInstanceIdentifier = Limits<uint32_t>::MAX;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const DecalOptions3D& other) const noexcept = default;
};

/**
 * Configuration options for a Decals3D set.
 */
struct Decals3DOptions {
	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Decals3DOptions& other) const noexcept = default;
};

/**
 * Set of decals in 3D space.
 */
class Decals3D {
public:
	/** Struct of shader parameters representing the decals. */
	struct Parameters {
		/** Sampler for the decal base color atlas texture. */
		sampler2D decalsBaseColorAtlasTexture;

		/** Sampler for the decal normal atlas texture. */
		sampler2D decalsNormalAtlasTexture;

		/** Sampler for the decal occlusion-roughness-metallic atlas texture. */
		sampler2D decalsOcclusionRoughnessMetallicAtlasTexture;

		/** Sampler for the decal emissive atlas texture. */
		sampler2D decalsEmissiveAtlasTexture;
	};

	/** Shader buffer for decal parameters. */
	using ParameterBuffer = UniformBuffer<Parameters, "Decals3DParameters">;

	/**
	 * Construct a set of decals.
	 *
	 * \param device device to create the decals for. Must outlive the decals.
	 * \param options decal set options, see Decals3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) explicit Decals3D(Device& device, const Decals3DOptions& options = {});

	/**
	 * Remove all decal materials and images from the set, and consequently all
	 * decals as well.
	 */
	GREM_API(graphics_3d) void clearDecalMaterials() noexcept;

	/**
	 * Create a decal material.
	 *
	 * \param options decal material options, see DecalMaterialOptions.
	 *
	 * \return an identifier for the inserted decal material.
	 *
	 * \throws graphics::Error on failure to copy an image or expand a texture
	 *         atlas.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics_3d) DecalMaterialID createDecalMaterial(const DecalMaterialOptions& options);

	/**
	 * Remove all decals from the set.
	 */
	GREM_API(graphics_3d) void clearDecals() noexcept;

	/**
	 * Create a decal.
	 *
	 * \param materialID identifier for the decal material. Must be a valid
	 *        identifier acquired using createDecalMaterial().
	 * \param options decal options, see DecalOptions3D.
	 *
	 * \return a handle to the new decal that can be used to refer back to it
	 *         later in order to make changes to it.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) DecalID createDecal(DecalMaterialID materialID, const DecalOptions3D& options);

	/**
	 * Check if a specific decal handle is still valid, meaning that the decal
	 * has not yet been destroyed.
	 *
	 * \param id handle to the decal.
	 *
	 * \return true if the associated decal still exists, false if the decal has
	 *         been destroyed.
	 */
	[[nodiscard]] GREM_API(graphics_3d) bool containsDecal(DecalID id) const noexcept;

	/**
	 * Destroy a decal and remove it from the set.
	 *
	 * \param id handle to the decal.
	 *
	 * \return true if the specified decal was found and destroyed, false
	 *         otherwise.
	 *
	 * \note If the specified decal has already been destroyed, this function
	 *       has no effect.
	 */
	GREM_API(graphics_3d) bool destroyDecal(DecalID id);

	/**
	 * Update the position of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newPosition new position of the decal, in world coordinates.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalPosition(DecalID id, vec3 newPosition);

	/**
	 * Update the range of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newRange new maximum range from the decal position, in world
	 *        coordinates, where the decal will apply to meshes.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalRange(DecalID id, float newRange);

	/**
	 * Update the rotation of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newOrientation new rotation of the decal around its origin.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalOrientation(DecalID id, quat newOrientation);

	/**
	 * Update the size of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newSize new size of the decal, in world coordinates.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalSize(DecalID id, vec2 newSize);

	/**
	 * Update the origin of a decal.
	 * 
	 * \param id handle to the decal.
	 * \param newOrigin new offset, in texture coordinates, specifying the
	 *        origin relative to the bottom left of the decal. For example, a
	 *        value of (0.5, 0.5) corresponds to the middle of the decal.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalOrigin(DecalID id, vec2 newOrigin);

	/**
	 * Update the tint color of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newColor new tint color to use in the shader.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalColor(DecalID id, Color newColor);

	/**
	 * Update the emissive factor of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newEmissiveFactor new emissive factor to use in the shader.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalEmissiveFactor(DecalID id, vec3 newEmissiveFactor);

	/**
	 * Update the model instance identifier of a decal.
	 *
	 * \param id handle to the decal.
	 * \param newModelInstanceIdentifier new model instance identifier that the
	 *        decal applies to, or the maximum value to apply to all instances.
	 *
	 * \note If the given decal handle is invalid, this function has no effect.
	 */
	GREM_API(graphics_3d) void setDecalModelInstanceIdentifier(DecalID id, uint32_t newModelInstanceIdentifier);

	/**
	 * Set the configuration options of the decal set.
	 *
	 * \param newOptions new configuration options, see Decals3DOptions.
	 */
	void setOptions(const Decals3DOptions& newOptions) {
		options = newOptions;
	}

	/**
	 * Get the configuration options of the decal set.
	 *
	 * \return the current configuration options.
	 */
	[[nodiscard]] Decals3DOptions getOptions() const noexcept {
		return options;
	}

	/**
	 * Execute a callback function for each decal in the decal set.
	 *
	 * \param callback function to execute, which should accept the decal ID as
	 *        a parameter. The callback function should return either void or a
	 *        bool that specifies whether to stop the traversal or not. A value
	 *        of true means to stop and return early, while a value of false
	 *        means to continue traversing.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws any exception thrown by the callback function.
	 *
	 * \note The order of traversal is unspecified.
	 *
	 * \warning Decals must not be added or removed from the set during
	 *          traversal, unless the traversal is stopped immediately
	 *          afterwards.
	 */
	auto forEachDecal(auto callback) {
		constexpr bool CALLBACK_RETURNS_BOOL = convertible_to<decltype(callback(DecalID{})), bool>;
		for (const auto& [decalID, decal] : decals) {
			if constexpr (CALLBACK_RETURNS_BOOL) {
				if (callback(decalID)) {
					return true;
				}
			} else {
				callback(decalID);
			}
		}
		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

private:
	friend Renderer3D;

	static constexpr uint32_t INITIAL_RESOLUTION = 1024;
	static constexpr uint32_t PADDING = 8;
	static constexpr uint32_t ALIGNMENT = 16;

	struct DecalMaterial {
		vec4 baseColorMapPositionAndSize;
		vec4 normalMapPositionAndSize;
		vec4 occlusionRoughnessMetallicMapPositionAndSize;
		vec4 emissiveMapPositionAndSize;
		vec4 baseColorFactor;
		vec3 occlusionRoughnessMetallicFactor;
		float normalScale;
		vec3 emissiveFactor;
	};

	struct Decal {
		DecalMaterialID materialID;
		vec3 position;
		float range;
		quat orientation;
		vec2 size;
		vec2 origin;
		Color color;
		vec3 emissiveFactor;
		uint32_t modelInstanceIdentifier;
	};

	[[nodiscard]] GREM_API(graphics_3d) vec2 getDefaultDecalBaseColorMapPosition();
	[[nodiscard]] GREM_API(graphics_3d) vec2 getDefaultDecalNormalMapPosition();
	[[nodiscard]] GREM_API(graphics_3d) vec2 getDefaultDecalOcclusionRoughnessMetallicMapPosition();
	[[nodiscard]] GREM_API(graphics_3d) vec2 getDefaultDecalEmissiveMapPosition();

	GREM_API(graphics_3d) void flush(Renderer3D& renderer3D) const;

	Device* device;
	[[no_unique_address]] Decals3DOptions options;
	OrderedMap<DecalID, Decal> decals{};
	uint64_t lastDecalIDValue = 0;
	resource::AtlasPacker baseColorAtlasPacker{{.initialResolution = INITIAL_RESOLUTION, .padding = PADDING, .alignment = ALIGNMENT}};
	resource::AtlasPacker normalAtlasPacker{{.initialResolution = INITIAL_RESOLUTION, .padding = PADDING, .alignment = ALIGNMENT}};
	resource::AtlasPacker occlusionRoughnessMetallicAtlasPacker{{.initialResolution = INITIAL_RESOLUTION, .padding = PADDING, .alignment = ALIGNMENT}};
	resource::AtlasPacker emissiveAtlasPacker{{.initialResolution = INITIAL_RESOLUTION, .padding = PADDING, .alignment = ALIGNMENT}};
	Texture baseColorAtlasTexture{};
	Texture normalAtlasTexture{};
	Texture occlusionRoughnessMetallicAtlasTexture{};
	Texture emissiveAtlasTexture{};
	Optional<vec2> defaultBaseColorMapPosition{};
	Optional<vec2> defaultNormalMapPosition{};
	Optional<vec2> defaultOcclusionRoughnessMetallicMapPosition{};
	Optional<vec2> defaultEmissiveMapPosition{};
	Buffer<DecalMaterial> decalMaterials{};
	mutable ParameterBuffer parameterBuffer{*device};
	mutable bool parameterBufferDirty = true;
};

} // namespace grem::graphics

template <>
struct std::hash<grem::graphics::DecalMaterialID> {
	[[nodiscard]] std::size_t operator()(const grem::graphics::DecalMaterialID& decalMaterialID) const {
		return hasher(decalMaterialID.index);
	}

private:
	[[no_unique_address]] std::hash<grem::uint32_t> hasher;
};

template <>
struct std::hash<grem::graphics::DecalID> {
	[[nodiscard]] std::size_t operator()(const grem::graphics::DecalID& decalID) const {
		return hasher(decalID.value);
	}

private:
	[[no_unique_address]] std::hash<grem::uint64_t> hasher;
};

#endif
