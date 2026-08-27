// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_SPRITE_ATLAS_HPP
#define GREM_GRAPHICS_SPRITE_ATLAS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/resource/AtlasPacker.hpp>
#include <GREM/resource/Image.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash
#include <stdexcept>  // std::length_error
#include <utility>    // std::move

namespace grem::graphics {

class Device;      // Forward declaration, to avoid including Device.hpp.
class SpriteAtlas; // Forward declaration.

/**
 * Configuration options for a sprite in a SpriteAtlas.
 */
struct SpriteOptions {
	/**
	 * Flags that describe how a sprite is flipped when rendered.
	 */
	using Flip = uint8_t;

	/**
	 * Flag values for Flip that describe how a sprite is flipped when rendered.
	 */
	enum FlipAxis : Flip {
		NO_FLIP = 0,                ///< Do not flip the sprite.
		FLIP_HORIZONTALLY = 1 << 0, ///< Flip the sprite along the X axis.
		FLIP_VERTICALLY = 1 << 1,   ///< Flip the sprite along the Y axis.
	};

	/**
	 * Flags that describe how the sprite should be flipped when rendered.
	 */
	Flip flip = NO_FLIP;

	/**
	 * Convert the inserted sprite image from straight to pre-multiplied alpha.
	 *
	 * \note Only applies to images in raw RGBA formats.
	 */
	bool convertToPremultipliedAlpha = true;

	/**
	 * Compare this set of options to another set for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const SpriteOptions& other) const = default;
};

/**
 * Configuration options for a SpriteAtlas.
 */
struct SpriteAtlasOptions {
	/**
	 * Internal texture format to use for the internal texture atlas.
	 */
	TextureFormat internalFormat = TextureFormat::R8G8B8A8_SRGB;

	/**
	 * Initial width of the internal texture atlas, in pixels.
	 *
	 * \warning Must be positive.
	 */
	uint32_t initialResolution = 128;

	/**
	 * Empty space to reserve between sprite images, in pixels.
	 */
	uint32_t padding = 0;

	/**
	 * Align padded sprites to a multiple of this number of pixels and limit the
	 * number of mip levels in the internal texture atlas such that a mipped
	 * texel never covers more than this width of pixels in the sprite atlas.
	 *
	 * \warning Must be a power of 2.
	 *
	 * \note If this is set to 1, the number of mip levels will always be
	 *       limited to 1 (highest detail level only), which means that the
	 *       TextureSamplerOptions::mipmapMode of #samplerOptions should ideally
	 *       be set to TextureMipmapMode::NONE to match. Likewise, whenever
	 *       mipmapMode is set to TextureMipmapMode::NONE, this option should
	 *       probably be 1 to allow for tighter packing.
	 */
	uint32_t alignment = 16;

	/**
	 * Sampler options to use for the atlas texture.
	 */
	TextureSamplerOptions samplerOptions{
		.minificationFilter = TextureFilter::NEAREST,
		.magnificationFilter = TextureFilter::NEAREST,
		.mipmapMode = TextureMipmapMode::NEAREST,
		.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
	};
};

/**
 * Identifier for a specific image in a SpriteAtlas.
 */
struct SpriteID {
public:
	/**
     * Construct an invalid sprite identifier.
     */
	constexpr SpriteID() noexcept = default;

	/**
	 * Check if this identifier is potentially valid.
	 *
	 * \return true if this identifier is potentially valid, false if it is
	 *         equal to a default-constructed invalid identifier.
	 */
	constexpr explicit operator bool() const noexcept {
		return *this != SpriteID{};
	}

	/**
	 * Compare this identifier to another for equality.
	 *
	 * \param other the identifier to compare this one to.
	 *
	 * \return true if the identifiers are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SpriteID& other) const noexcept = default;

	/**
	 * Compare this identifier to another.
	 *
	 * \param other the identifier to compare this one to.
	 *
	 * \return a strong ordering between the two identifiers.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const SpriteID& other) const noexcept = default;

private:
	friend SpriteAtlas;
	friend std::hash<SpriteID>;

	constexpr explicit SpriteID(uint32_t index) noexcept
		: index(index) {}

	uint32_t index = Limits<uint32_t>::MAX;
};

/**
 * Expandable texture atlas for packing 2D images into a spritesheet to
 * enable batch rendering.
 */
class SpriteAtlas {
public:
	/**
	 * Information about a specific image in the spritesheet.
	 */
	struct Sprite {
		vec2 position{};                                   ///< Position of the image in the texture atlas, in texels.
		vec2 size{};                                       ///< Size of the image in the texture atlas, in texels.
		SpriteOptions::Flip flip = SpriteOptions::NO_FLIP; ///< Flags that describe how the sprite should be flipped when rendered.
	};

	/**
	 * Construct an empty sprite atlas.
	 *
	 * \param device device to create the sprite atlas for. Must outlive the
	 *        sprite atlas.
	 * \param options sprite atlas options, see SpriteAtlasOptions.
	 */
	explicit SpriteAtlas(Device& device, const SpriteAtlasOptions& options = {})
		: device(&device)
		, atlasPacker({.initialResolution = options.initialResolution, .padding = options.padding, .alignment = options.alignment})
		, options(options) {}

	/**
	 * Clear all inserted sprites and images from the spritesheet.
	 */
	void clear() noexcept {
		sprites.clear();
		atlasPacker = resource::AtlasPacker{{.initialResolution = atlasPacker.getResolution(), .padding = options.padding, .alignment = options.alignment}};
	}

	/**
	 * Add a new image to the spritesheet, possibly expanding the texture atlas
	 * in order to make space for it.
	 *
	 * \param image non-owning view over the image to copy into the spritesheet.
	 * \param spriteOptions sprite options, see SpriteOptions.
	 *
	 * \return an identifier for the inserted image.
	 *
	 * \throws graphics::Error on failure to copy the image or expand the
	 *         texture atlas.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The function will fail if the given image is not of a format that
	 *       is compatible with the internal format of the texture atlas.
	 *
	 * \sa createSubSprite()
	 */
	[[nodiscard]] SpriteID insertSprite(const resource::ImageView& image, const SpriteOptions& spriteOptions = {}) {
		if (!Texture::isCompatibleFormat(options.internalFormat, image.getFormat())) {
			throw graphics::Error{"Failed to insert sprite image: Incompatible image format."};
		}

		if (sprites.size() >= size_t{Limits<uint32_t>::MAX}) {
			throw std::length_error{"Maximum sprite count exceeded."};
		}

		const auto [offset, resized] = atlasPacker.insertRectangle(image.getSize2D());
		if (!atlasTexture || resized) {
			Texture oldAtlasTexture = std::move(atlasTexture);
			const Extent2D size{static_cast<uint32_t>(atlasPacker.getResolution())};
			uint32_t mipLevelCount = 1;
			if (options.samplerOptions.mipmapMode != TextureMipmapMode::NONE) {
				mipLevelCount = min(resource::Image::getMaxMipLevelCount(size), resource::Image::getMaxMipLevelCount(Extent2D{options.alignment}));
			}
			atlasTexture = Texture::create(*device, TextureType::TEXTURE_2D, options.internalFormat, size, mipLevelCount, ClearValues{}, options.samplerOptions);
			if (oldAtlasTexture) {
				atlasTexture.pasteTexture(oldAtlasTexture);
			}
		}

		resource::Image transformedImage{};
		resource::ImageView transformedImageView = image;
		if (spriteOptions.convertToPremultipliedAlpha && resource::Image::isRGBAColorFormat(transformedImageView.getFormat()) &&
			resource::Image::isRawFormat(transformedImageView.getFormat())) {
			transformedImage = resource::Image{transformedImageView};
			transformedImage.transformFromStraightToPremultipliedAlpha(Texture::getTransferFunction(options.internalFormat));
			transformedImageView = transformedImage;
		}
		if (options.padding > 0) {
			transformedImage = transformedImageView.getPadded(options.padding);
			transformedImageView = transformedImage;
		}
		atlasTexture.pasteImage(Extent2D{transformedImageView.getWidth(), transformedImageView.getHeight()}, transformedImageView.data(),
			Offset2D{.x = offset.x - static_cast<int32_t>(options.padding), .y = offset.y - static_cast<int32_t>(options.padding)});

		const vec2 position{static_cast<float>(offset.x), static_cast<float>(offset.y)};
		const vec2 size{static_cast<float>(image.getWidth()), static_cast<float>(image.getHeight())};

		const uint32_t index = static_cast<uint32_t>(sprites.size());
		sprites.push_back(Sprite{.position = position, .size = size, .flip = spriteOptions.flip});
		return SpriteID{index};
	}

	/**
	 * Add a new sprite that is defined as a sub-region of an existing sprite.
	 *
	 * \param baseSpriteID identifier for the existing sprite to create a
	 *        sub-region of. Must have been obtained from a previous call to
	 *        insert() or createSubSprite() on the same SpriteAtlas object as
	 *        the one that this function is called on.
	 * \param region region, in pixels, relative to the bottom left corner of
	 *        the original sprite, where the new sprite will begin. Must fit
	 *        within the original sprite image.
	 * \param flip flags that describe how the sprite should be flipped when
	 *        rendered.
	 *
	 * \return an identifier for the new sub-sprite.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function does not grow the texture atlas.
	 *
	 * \sa insert()
	 */
	[[nodiscard]] SpriteID createSubSprite(SpriteID baseSpriteID, Region2D region, SpriteOptions::Flip flip = SpriteOptions::NO_FLIP) {
		if (sprites.size() >= size_t{Limits<uint32_t>::MAX}) {
			throw std::length_error{"Maximum sprite count exceeded."};
		}

		const Sprite& baseSprite = getSprite(baseSpriteID);
		GREM_ASSERT(static_cast<float>(region.offset.x) <= baseSprite.size.x);
		GREM_ASSERT(static_cast<float>(region.offset.y) <= baseSprite.size.y);
		GREM_ASSERT(static_cast<float>(region.size.width) <= baseSprite.size.x - static_cast<float>(region.offset.x));
		GREM_ASSERT(static_cast<float>(region.size.height) <= baseSprite.size.y - static_cast<float>(region.offset.y));

		const vec2 position = baseSprite.position + vec2{static_cast<float>(region.offset.x), static_cast<float>(region.offset.y)};
		const vec2 size{static_cast<float>(region.size.width), static_cast<float>(region.size.height)};

		const uint32_t index = static_cast<uint32_t>(sprites.size());
		sprites.push_back(Sprite{.position = position, .size = size, .flip = flip});
		return SpriteID{index};
	}

	/**
	 * Get information about a specific image in the spritesheet.
	 *
	 * \param id identifier for the image to get the information of. Must have
	 *        been obtained from a previous call to insert() or
	 *        createSubSprite() on the same SpriteAtlas object as the one that
	 *        this function is called on.
	 *
	 * \return a read-only reference to the sprite information that is valid
	 *         until the next call to insert(), or until the SpriteAtlas is
	 *         destroyed, whichever happens first.
	 */
	[[nodiscard]] const Sprite& getSprite(SpriteID id) const {
		GREM_ASSERT(id.index < sprites.size());
		return sprites[id.index];
	}

	/**
	 * Get the offset and scale of a specific sprite in the texture atlas.
	 *
	 * \param id identifier for the image to get the information of. Must have
	 *        been obtained from a previous call to insert() or
	 *        createSubSprite() on the same SpriteAtlas object as the one that
	 *        this function is called on.
	 *
	 * \return a pair of:
	 *         - the texture offset of the sprite, in normalized texture
	 *           coordinates, and
	 *         - the texture scale of the sprite, in normalized texture
	 *           coordinates.
	 */
	[[nodiscard]] Pair<vec2> getSpriteTextureOffsetAndScale(SpriteID id) const noexcept {
		const Sprite& sprite = getSprite(id);
		vec2 unnormalizedTextureOffset{};
		vec2 unnormalizedTextureScale{};
		if ((sprite.flip & SpriteOptions::FLIP_HORIZONTALLY) != 0) {
			unnormalizedTextureOffset.x = sprite.position.x + sprite.size.x;
			unnormalizedTextureScale.x = -sprite.size.x;
		} else {
			unnormalizedTextureOffset.x = sprite.position.x;
			unnormalizedTextureScale.x = sprite.size.x;
		}
		if ((sprite.flip & SpriteOptions::FLIP_VERTICALLY) != 0) {
			unnormalizedTextureOffset.y = sprite.position.y + sprite.size.y;
			unnormalizedTextureScale.y = -sprite.size.y;
		} else {
			unnormalizedTextureOffset.y = sprite.position.y;
			unnormalizedTextureScale.y = sprite.size.y;
		}
		const vec2 textureSize = atlasTexture.getSize2D();
		const vec2 textureOffset = unnormalizedTextureOffset / textureSize;
		const vec2 textureScale = unnormalizedTextureScale / textureSize;
		return {textureOffset, textureScale};
	}

	/**
	 * Get a reference to the internal texture atlas.
	 *
	 * \return a read-only reference to the texture atlas containing the
	 *         sprite image data that is valid until the next call to insert(),
	 *         or until the SpriteAtlas is destroyed, whichever happens first.
	 */
	[[nodiscard]] const Texture& getAtlasTexture() const noexcept {
		return atlasTexture;
	}

	/**
	 * Get the configuration options of this spritesheet.
	 *
	 * \return a read-only reference to the current options.
	 */
	[[nodiscard]] const SpriteAtlasOptions& getOptions() const noexcept {
		return options;
	}

private:
	Device* device;
	resource::AtlasPacker atlasPacker;
	Texture atlasTexture{};
	Buffer<Sprite> sprites{};
	SpriteAtlasOptions options{};
};

} // namespace grem::graphics

template <>
struct std::hash<grem::graphics::SpriteID> {
	[[nodiscard]] std::size_t operator()(const grem::graphics::SpriteID& spriteID) const {
		return hasher(spriteID.index);
	}

private:
	[[no_unique_address]] std::hash<grem::uint32_t> hasher;
};

#endif
