// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_TEXTURE_HPP
#define GREM_GRAPHICS_TEXTURE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/resource/Image.hpp>

#include <utility> // std::move

namespace grem::graphics {

class Texture;               // Forward declaration.
class Swapchain;             // Forward declaration, to avoid a circular include of Swapchain.hpp.
class Device;                // Forward declaration, to avoid a circular include of Device.hpp.
struct DeviceImplementation; // Forward declaration, to avoid a circular include of Device.hpp.

struct TextureImplementation; ///< Backend-specific implementation of UniformBuffer.

/**
 * Type of a Texture.
 */
enum class TextureType : uint8_t {
	EMPTY,              ///< Empty texture without a value.
	TEXTURE_2D,         ///< Texture set up to store a single 2D image.
	TEXTURE_2D_ARRAY,   ///< Texture set up to store an array of 2D images.
	TEXTURE_CUBE,       ///< Texture set up to store a cube of 6 sides of square 2D images.
	TEXTURE_CUBE_ARRAY, ///< Texture set up to store an array of cubes of 6 sides of square 2D images.
	RENDERBUFFER,       ///< Unsampled texture set up to store a potentially multisampled 2D image.
	SWAPCHAIN,          ///< Unsampled texture representing a presentation swapchain.
};

/**
 * Aspect of a texture format.
 */
enum class TextureAspect : uint32_t { // NOLINT(performance-enum-size)
	COLOR = 0x00004000,               ///< Color buffer. \hideinitializer
	DEPTH = 0x00000100,               ///< Depth buffer. \hideinitializer
	STENCIL = 0x00000400,             ///< Stencil buffer. \hideinitializer
};

/**
 * Set of aspects of a texture format.
 */
class TextureAspects {
public:
	/**
	 * Set containing all possible texture aspects.
	 */
	static const TextureAspects ALL;

	/**
	 * Set containing both the depth and stencil texture aspects.
	 */
	static const TextureAspects DEPTH_STENCIL;

	/**
	 * Set containing the color, depth and stencil texture aspects.
	 */
	static const TextureAspects COLOR_DEPTH_STENCIL;

	/**
	 * Construct an empty aspect set.
	 */
	constexpr TextureAspects() noexcept = default;

	/**
	 * Construct an aspect set containing only one specific aspect.
	 *
	 * \param aspect texture aspect to include.
	 *
	 * \note Aspect sets can be combined using
	 *       operator|(TextureAspects, TextureAspects).
	 */
	constexpr TextureAspects(TextureAspect aspect)
		: bits(static_cast<uint32_t>(aspect)) {}

	/**
	 * Compare this aspect set to another for equality.
	 *
	 * \param other the aspect set to compare this one to.
	 *
	 * \return true if the aspect sets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureAspects& other) const noexcept = default;

	/**
	 * Check if the aspect set is empty.
	 *
	 * \return true if the set contains no aspects, false otherwise.
	 */
	[[nodiscard]] constexpr bool empty() const noexcept {
		return bits == 0;
	}

	/**
	 * Check if the aspect set contains the given aspect.
	 *
	 * \param aspect aspect identifier to check for.
	 *
	 * \return true if the set contains the given aspect, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(TextureAspect aspect) const noexcept {
		return (bits & TextureAspects{aspect}.bits) != 0;
	}

	/**
	 * Check if the aspect set contains at least one of the given aspects.
	 *
	 * \param aspects aspect set to check for.
	 *
	 * \return true if the set contains at least one of the given aspects, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAnyOf(TextureAspects aspects) const noexcept {
		return (bits & aspects.bits) != 0;
	}

	/**
	 * Check if the aspect set contains all of the given aspects.
	 *
	 * \param aspects aspect set to check for.
	 *
	 * \return true if the set contains all of the given aspects, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAllOf(TextureAspects aspects) const noexcept {
		return (bits & aspects.bits) == aspects.bits;
	}

	/**
	 * Get the complement of an aspect set.
	 *
	 * \param a the set to invert.
	 *
	 * \return a set containing all possible aspects except those in the given
	 *         set.
	 */
	[[nodiscard]] friend constexpr TextureAspects operator~(TextureAspects a) noexcept {
		return TextureAspects{~a.bits};
	}

	/**
	 * Get the intersection of two aspect sets.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a set containing all aspects contained in both a and b.
	 */
	[[nodiscard]] friend constexpr TextureAspects operator&(TextureAspects a, TextureAspects b) noexcept {
		return TextureAspects{a.bits & b.bits};
	}

	/**
	 * Get the union of two aspect sets.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a set containing all aspects contained in a or b or both.
	 */
	[[nodiscard]] friend constexpr TextureAspects operator|(TextureAspects a, TextureAspects b) noexcept {
		return TextureAspects{a.bits | b.bits};
	}

	/**
	 * Get the symmetric difference of two aspect sets.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a set containing all aspects contained in either a or b, but not
	 *         both.
	 */
	[[nodiscard]] friend constexpr TextureAspects operator^(TextureAspects a, TextureAspects b) noexcept {
		return TextureAspects{a.bits ^ b.bits};
	}

	/**
	 * Assign the intersection of two aspect sets to the first set.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr TextureAspects& operator&=(TextureAspects& a, TextureAspects b) noexcept {
		return a = a & b;
	}

	/**
	 * Assign the union of two aspect sets to the first set.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr TextureAspects& operator|=(TextureAspects& a, TextureAspects b) noexcept {
		return a = a | b;
	}

	/**
	 * Assign the symmetric difference of two aspect sets to the first set.
	 *
	 * \param a first aspect set.
	 * \param b second aspect set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr TextureAspects& operator^=(TextureAspects& a, TextureAspects b) noexcept {
		return a = a ^ b;
	}

private:
	friend TextureImplementation;

	constexpr explicit TextureAspects(uint32_t bits) noexcept
		: bits(bits) {}

	uint32_t bits{};
};

/**
 * Get the complement of a texture aspect.
 *
 * \param a the aspect to invert.
 *
 * \return a set containing all possible aspects except the given aspect.
 */
constexpr TextureAspects operator~(TextureAspect a) noexcept {
	return ~TextureAspects{a};
}

/**
 * Get the union of two texture aspects.
 *
 * \param a first aspect.
 * \param b second aspect.
 *
 * \return a set containing both a and b.
 */
constexpr TextureAspects operator|(TextureAspect a, TextureAspect b) noexcept {
	return TextureAspects{a} | TextureAspects{b};
}

inline constexpr TextureAspects TextureAspects::ALL = ~TextureAspects{};
inline constexpr TextureAspects TextureAspects::DEPTH_STENCIL = TextureAspect::DEPTH | TextureAspect::STENCIL;
inline constexpr TextureAspects TextureAspects::COLOR_DEPTH_STENCIL = TextureAspect::COLOR | TextureAspect::DEPTH | TextureAspect::STENCIL;

/**
 * Description of the internal format of a Texture.
 */
enum class TextureFormat : int32_t { // NOLINT(performance-enum-size)
	UNKNOWN = 0,                     ///< Unknown texture format.
	R8_UNORM = 0x8229,               ///< Raw bitmap where each texel comprises 1 normalized 8-bit unsigned integer component: red. \hideinitializer
	R16_FLOAT = 0x822D,              ///< Raw bitmap where each texel comprises 1 16-bit floating-point component: red. \hideinitializer
	R32_FLOAT = 0x822E,              ///< Raw bitmap where each texel comprises 1 32-bit floating-point component: red. \hideinitializer
	R8G8_UNORM = 0x822B,             ///< Raw bitmap where each texel comprises 2 normalized 8-bit unsigned integer components: red, green. \hideinitializer
	R16G16_FLOAT = 0x822F,           ///< Raw bitmap where each texel comprises 2 16-bit floating-point components: red, green. \hideinitializer
	R32G32_FLOAT = 0x8230,           ///< Raw bitmap where each texel comprises 2 32-bit floating-point components: red, green. \hideinitializer
	// Note: Raw 3-channel RGB formats without alpha are intentionally omitted since many GPUs don't support them.
	R8G8B8A8_UNORM = 0x8058, ///< Raw bitmap where each texel comprises 4 normalized 8-bit unsigned integer components: red, green, blue, alpha. \hideinitializer
	R8G8B8A8_SRGB = 0x8C43,  ///< Raw bitmap where each texel comprises 3 sRGB-encoded and 1 normalized 8-bit unsigned integer components: red, green, blue, alpha. \hideinitializer
	R16G16B16A16_FLOAT = 0x881A, ///< Raw bitmap where each texel comprises 4 16-bit floating-point components: red, green, blue, alpha. \hideinitializer
	R32G32B32A32_FLOAT = 0x8814, ///< Raw bitmap where each texel comprises 4 32-bit floating-point components: red, green, blue, alpha. \hideinitializer
	D16_UNORM = 0x81A5,          ///< Raw bitmap where each texel comprises 1 normalized 16-bit unsigned integer component: depth. \hideinitializer
	// Note: Raw 24-bit depth formats without stencil are intentionally omitted since many GPUs don't support them.
	D32_FLOAT = 0x8CAC, ///< Raw bitmap where each texel comprises 1 32-bit floating-point component: depth. \hideinitializer
	D24_UNORM_S8_UINT =
		0x88F0, ///< Raw bitmap where each texel comprises 1 normalized 24-bit unsigned integer depth component and 1 8-bit unsigned integer stencil component. \hideinitializer
	D32_FLOAT_S8_UINT = 0x8CAD, ///< Raw bitmap where each texel comprises 1 32-bit floating-point depth component and 1 8-bit unsigned integer stencil component. \hideinitializer
	R5G6B5_UNORM_PACK16 = 0x8D62,          ///< Packed bitmap with the following 16-bit unsigned integer RGB texel format: RRRRRGGGGGGBBBBB. \hideinitializer
	A1R5G5B5_UNORM_PACK16 = 0x8057,        ///< Packed bitmap with the following 16-bit unsigned integer RGBA texel format: ARRRRRGGGGGBBBBB. \hideinitializer
	B10G11R11_UFLOAT_PACK32 = 0x8C3A,      ///< Packed bitmap with the following 32-bit unsigned floating-point RGB texel format: BBBBBBBBBBGGGGGGGGGGGRRRRRRRRRRR. \hideinitializer
	A2B10G10R10_UNORM_PACK32 = 0x906F,     ///< Packed bitmap with the following 32-bit unsigned integer RGBA texel format: AABBBBBBBBBBGGGGGGGGGGRRRRRRRRRR. \hideinitializer
	ASTC_4x4_RGBA_UNORM_BLOCK = 0x93B0,    ///< ASTC block-compressed RGBA texture using 128 bits per 4x4 block. \hideinitializer
	ASTC_4x4_RGBA_SRGB_BLOCK = 0x93D0,     ///< ASTC block-compressed RGBA texture using 128 bits per 4x4 block. \hideinitializer
	BC1_RGB_UNORM_BLOCK = 0x83F0,          ///< S3TC BC1 (DXT1) block-compressed RGB texture using 64 bits per 4x4 block. \hideinitializer
	BC1_RGB_SRGB_BLOCK = 0x8C4C,           ///< S3TC BC1 (DXT1) block-compressed sRGB texture using 64 bits per 4x4 block. \hideinitializer
	BC3_RGBA_UNORM_BLOCK = 0x83F3,         ///< S3TC BC3 (DXT5) block-compressed RGBA texture using 128 bits per 4x4 block. \hideinitializer
	BC3_RGBA_SRGB_BLOCK = 0x8C4F,          ///< S3TC BC3 (DXT5) block-compressed sRGBA texture using 128 bits per 4x4 block. \hideinitializer
	BC4_R_UNORM_BLOCK = 0x8DBB,            ///< S3TC BC4 (RGTC1) block-compressed single-channel texture using 64 bits per 4x4 block. \hideinitializer
	BC5_RG_UNORM_BLOCK = 0x8DBD,           ///< S3TC BC5 (RGTC2) block-compressed double-channel texture using 128 bits per 4x4 block. \hideinitializer
	BC6H_RGB_UFLOAT_BLOCK = 0x8E8F,        ///< S3TC BC6H (BPTC) block-compressed unsigned floating-point RGB texture using 128 bits per 4x4 block. \hideinitializer
	BC6H_RGB_FLOAT_BLOCK = 0x8E8E,         ///< S3TC BC6H (BPTC) block-compressed signed floating-point RGB texture using 128 bits per 4x4 block. \hideinitializer
	BC7_RGBA_UNORM_BLOCK = 0x8E8C,         ///< S3TC BC7 (BPTC) block-compressed RGBA texture using 128 bits per 4x4 block. \hideinitializer
	BC7_RGBA_SRGB_BLOCK = 0x8E8D,          ///< S3TC BC7 (BPTC) block-compressed sRGBA texture using 128 bits per 4x4 block. \hideinitializer
	ETC2_R8G8B8_UNORM_BLOCK = 0x9274,      ///< ETC2 block-compressed RGB texture using 64 bits per 4x4 block. \hideinitializer
	ETC2_R8G8B8_SRGB_BLOCK = 0x9275,       ///< ETC2 block-compressed sRGB texture using 64 bits per 4x4 block. \hideinitializer
	ETC2_R8G8B8A8_UNORM_BLOCK = 0x9278,    ///< ETC2 block-compressed RGBA texture using 128 bits per 4x4 block. \hideinitializer
	ETC2_R8G8B8A8_SRGB_BLOCK = 0x9279,     ///< ETC2 block-compressed sRGBA texture using 128 bits per 4x4 block. \hideinitializer
	EAC_R11_UNORM_BLOCK = 0x9270,          ///< ETC2 EAC block-compressed single-channel texture using 64 bits per 4x4 block. \hideinitializer
	EAC_R11G11_UNORM_BLOCK = 0x9272,       ///< ETC2 EAC block-compressed double-channel texture using 128 bits per 4x4 block. \hideinitializer
	PVRTC1_4BPP_RGBA_UNORM_BLOCK = 0x8C02, ///< PVRTC1 block-compressed RGBA texture using 64 bits per 4x4 block. \hideinitializer
	PVRTC1_4BPP_RGBA_SRGB_BLOCK = 0x8A57,  ///< PVRTC1 block-compressed sRGBA texture using 64 bits per 4x4 block. \hideinitializer
};

/**
 * Subresource selection of a texture.
 */
struct TextureSubresource {
	TextureAspects aspects = TextureAspects::ALL; ///< Set of texture aspects.
	uint32_t layer = 0;                           ///< Layer index in the texture.
	uint32_t mipLevel = 0;                        ///< Mip level index in the texture.

	/**
	 * Compare this subresource selection to another for equality.
	 *
	 * \param other the selection to compare this one to.
	 *
	 * \return true if the selections are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureSubresource& other) const = default;
};

/**
 * Two-dimensional region of a texture.
 */
struct TextureRegion2D {
	TextureAspects aspects = TextureAspects::ALL; ///< Set of texture aspects.
	Offset3D offset{.x = 0, .y = 0, .z = 0};      ///< Offset from the bottom left of the first layer in the texture, in texels.
	Extent2D size;                                ///< Size of the region, in texels.
	uint32_t mipLevel = 0;                        ///< Mip level index in the texture.

	/**
	 * Compare this region to another for equality.
	 *
	 * \param other the region to compare this one to.
	 *
	 * \return true if the regions are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureRegion2D& other) const = default;
};

/**
 * Three-dimensional region of a texture.
 */
struct TextureRegion3D {
	TextureAspects aspects = TextureAspects::ALL; ///< Set of texture aspects.
	Offset3D offset{.x = 0, .y = 0, .z = 0};      ///< Offset from the bottom left of the first layer in the texture, in texels.
	Extent3D size;                                ///< Size of the region, in texels/layers.
	uint32_t mipLevel = 0;                        ///< Mip level index in the texture.

	/**
	 * Compare this region to another for equality.
	 *
	 * \param other the region to compare this one to.
	 *
	 * \return true if the regions are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureRegion3D& other) const = default;
};

/**
 * Reference to a subresource of a texture.
 */
struct TextureSubresourceReference {
	Texture* texture;               ///< Non-null pointer to the texture.
	TextureSubresource subresource; ///< Referenced texture subresource.
};

/**
 * Read-only reference to a subresource of a texture.
 */
struct TextureSubresourceConstReference {
	const Texture* texture;         ///< Read-only non-null pointer to the texture.
	TextureSubresource subresource; ///< Referenced texture subresource.
};

/**
 * Reference to a 2D region of a texture.
 */
struct TextureRegion2DReference {
	Texture* texture;       ///< Non-null pointer to the texture.
	TextureRegion2D region; ///< Referenced texture region.
};

/**
 * Read-only reference to a 2D region of a texture.
 */
struct TextureRegion2DConstReference {
	const Texture* texture; ///< Read-only non-null pointer to the texture.
	TextureRegion2D region; ///< Referenced texture region.
};

/**
 * Reference to a 3D region of a texture.
 */
struct TextureRegion3DReference {
	Texture* texture;       ///< Non-null pointer to the texture.
	TextureRegion3D region; ///< Referenced texture region.
};

/**
 * Read-only reference to a 3D region of a texture.
 */
struct TextureRegion3DConstReference {
	const Texture* texture; ///< Read-only non-null pointer to the texture.
	TextureRegion3D region; ///< Referenced texture region.
};

/**
 * Specification that a texture should leave its values untouched rather than
 * clearing them.
 */
struct RetainValues {};

/**
 * Specification of values that a texture should be cleared to.
 */
struct ClearValues {
	TextureAspects aspects = TextureAspects::ALL; ///< Which texture aspects to clear.
	Color color = Color::INVISIBLE;               ///< Color value to clear the color aspect to.
	float depth = 1.0f;                           ///< Depth value to clear the depth aspect to.
	uint8_t stencil = 0;                          ///< Stencil value to clear the stencil aspect to.
};

/**
 * Specification that a texture may be cleared to undefined values.
 */
struct UndefinedClearValues {
	TextureAspects aspects = TextureAspects::ALL; ///< Which texture aspects to clear.
};

/**
 * Specification of how a texture should either be left untouched, be cleared to
 * specific values or be cleared to undefined values before being used.
 */
struct ClearMode : Variant<RetainValues, ClearValues, UndefinedClearValues> {
	using Variant::Variant;
};

/**
 * Specification that an intermediate render texture should store its
 * intermediate values after being resolved.
 */
struct StoreIntermediateValues {
	TextureAspects aspects = TextureAspects::ALL; ///< Which texture aspects to store.
};

/**
 * Specification that an intermediate render texture may have its intermediate
 * values discarded after being resolved.
 */
struct DiscardIntermediateValues {};

/**
 * Specification of how an intermediate render texture should either store or
 * discard its values after being resolved.
 */
struct ResolveMode : Variant<StoreIntermediateValues, DiscardIntermediateValues> {
	using Variant::Variant;
};

/**
 * Filtering mode for the sampler of a Texture.
 */
enum class TextureFilter : uint8_t {
	NEAREST, ///< Use the nearest neighbor to the sampled texel, which results in a blocky appearance.
	LINEAR,  ///< Interpolate between texels for a smoother appearance.
};

/**
 * Mipmap mode for the sampler of a Texture.
 */
enum class TextureMipmapMode : uint8_t {
	NONE,    ///< Always use the most detailed mip level (level 0).
	NEAREST, ///< Use the mip level nearest to the displayed size.
	LINEAR,  ///< Interpolate between the two nearest mip levels to the displayed size.
};

/**
 * Wrapping mode of the sampler of a Texture.
 */
enum class TextureWrappingMode : uint8_t {
	REPEAT,          ///< Wrap the texture coordinates around the 0-1 range.
	CLAMP_TO_EDGE,   ///< Clamp to the edge of the texture when sampling outside the 0-1 range.
	MIRRORED_REPEAT, ///< Mirror the texture coordinates in the 0-1 range.
};

/**
 * Depth comparison mode for the sampler of a Texture.
 */
enum class TextureDepthComparisonMode : uint8_t {
	/**
	 * The depth test always fails.
	 */
	NEVER_PASS,

	/**
	 * The depth test passes if and only if the new depth value is less than the
	 * reference depth value.
	 */
	LESS,

	/**
	 * The depth test passes if and only if the new depth value is less than or
	 * equal to the reference depth value.
	 */
	LESS_OR_EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is greater than
	 * the reference depth value.
	 */
	GREATER,

	/**
	 * The depth test passes if and only if the new depth value is greater than
	 * or equal to the reference depth value.
	 */
	GREATER_OR_EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is equal to the
	 * reference depth value.
	 */
	EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is not equal to
	 * the reference depth value.
	 */
	NOT_EQUAL,

	/**
	 * The depth test always passes.
	 */
	ALWAYS_PASS,
};

/**
 * Configuration options for a texture image upload.
 */
struct TextureImageUploadOptions {
	/**
	 * Preferred transfer function to use when choosing the internal texture
	 * format.
	 *
	 * \note Only applies to image formats where the corresponding texture
	 *       format has both sRGB and non-sRGB variants.
	 */
	Color::TransferFunction transferFunction = Color::TransferFunction::SRGB;

	/**
	 * Convert the uploaded image from straight to pre-multiplied alpha.
	 *
	 * \note Only applies to images in raw RGBA formats.
	 */
	bool convertToPremultipliedAlpha = true;

	/**
	 * Generate a new mip chain with the maximum number of mip levels for the
	 * texture based on mip level 0 of the uploaded image, ignoring any other
	 * mip levels in the input image.
	 *
	 * \note Only applies to textures with framebuffer-compatible internal
	 *       formats.
	 */
	bool generateMipmap = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureImageUploadOptions& other) const = default;
};

/**
 * Configuration options for a texture image download.
 */
struct TextureImageDownloadOptions {
	/**
	 * Texture subresource to download. Must be a valid subresource of the
	 * texture, or an empty optional to download the whole texture.
	 */
	Optional<TextureSubresource> subresource{};

	/**
	 * Convert the downloaded image from pre-multiplied to straight alpha.
	 *
	 * Only applies to raw RGBA formats.
	 */
	bool convertFromPremultipliedAlpha = true;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureImageDownloadOptions& other) const = default;
};

/**
 * Configuration options for the sampler of a Texture.
 */
struct TextureSamplerOptions {
	/**
	 * A texture sampler configuration that ensures that the texture is sampled
	 * as directly as possible, without filtering.
	 */
	static const TextureSamplerOptions UNFILTERED;

	/**
	 * Filtering mode to use when the texture is rendered smaller than its
	 * original size.
	 */
	TextureFilter minificationFilter = TextureFilter::LINEAR;

	/**
	 * Filtering mode to use when the texture is rendered larger than its
	 * original size.
	 */
	TextureFilter magnificationFilter = TextureFilter::LINEAR;

	/**
	 * Mipmap mode to use when determining which mip detail level to sample
	 * depending on the texture's size on screen.
	 *
	 * Mipmapping results in fewer aliasing artifacts when rendering downscaled
	 * textures, such as those on distant 3D objects. It can also improve
	 * rendering performance slightly in those cases, at the cost of some extra
	 * texture memory.
	 *
	 * Note that the texture must have mipmaps generated or uploaded to it in
	 * order for this option to have an effect.
	 */
	TextureMipmapMode mipmapMode = TextureMipmapMode::LINEAR;

	/**
	 * How to address the texture when the horizontal component of the texture
	 * coordinates is outside the 0-1 range.
	 */
	TextureWrappingMode horizontalWrappingMode = TextureWrappingMode::REPEAT;

	/**
	 * How to address the texture when the vertical component of the texture
	 * coordinates is outside the 0-1 range.
	 */
	TextureWrappingMode verticalWrappingMode = TextureWrappingMode::REPEAT;

	/**
	 * Maximum level of anisotropic filtering to use.
	 *
	 * Set to 1 or lower to disable anisotropic filtering.
	 */
	float maxAnisotropy = 16.0f;

	/**
	 * Depth comparison mode to use when sampling the texture.
	 *
	 * If set, the texture must have a depth component.
	 */
	Optional<TextureDepthComparisonMode> depthComparisonMode{};

	/**
	 * Compare this configuration to another for equality.
	 *
	 * \param other the configuration to compare this one to.
	 *
	 * \return true if the configurations are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const TextureSamplerOptions& other) const = default;
};

inline constexpr TextureSamplerOptions TextureSamplerOptions::UNFILTERED{
	.minificationFilter = TextureFilter::NEAREST,
	.magnificationFilter = TextureFilter::NEAREST,
	.mipmapMode = TextureMipmapMode::NONE,
	.horizontalWrappingMode = TextureWrappingMode::REPEAT,
	.verticalWrappingMode = TextureWrappingMode::REPEAT,
	.maxAnisotropy = 1.0f,
};

/**
 * Storage for multidimensional data, such as 2D images, on the GPU, combined
 * with a sampler configuration that defines how to sample the stored data in
 * shaders while rendering.
 */
class Texture {
public:
	/**
	 * Get the set of texture aspects of an internal texture format.
	 *
	 * \param internalFormat format to get the aspects of.
	 *
	 * \return a set containing all texture aspects of the given format.
	 *
	 * \throws graphics::Error if the format aspects cannot be determined.
	 */
	[[nodiscard]] static constexpr TextureAspects getFormatAspects(TextureFormat internalFormat) {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: throw graphics::Error{"Invalid texture format."};
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return TextureAspect::COLOR;
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: return TextureAspect::DEPTH;
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return TextureAspects::DEPTH_STENCIL;
		}
		unreachable();
	}

	/**
	 * Get the image type corresponding to a texture type.
	 *
	 * \param type texture type to get the image type of.
	 *
	 * \return the corresponding image type.
	 *
	 * \throws graphics::Error if a corresponding image type cannot be
	 *         determined.
	 */
	[[nodiscard]] static constexpr resource::ImageType getImageType(TextureType type) {
		switch (type) {
			case TextureType::EMPTY: return resource::ImageType::EMPTY;
			case TextureType::TEXTURE_2D: return resource::ImageType::IMAGE_2D;
			case TextureType::TEXTURE_2D_ARRAY: return resource::ImageType::IMAGE_2D_ARRAY;
			case TextureType::TEXTURE_CUBE: return resource::ImageType::IMAGE_CUBE;
			case TextureType::TEXTURE_CUBE_ARRAY: return resource::ImageType::IMAGE_CUBE_ARRAY;
			case TextureType::RENDERBUFFER: [[fallthrough]];
			case TextureType::SWAPCHAIN: throw graphics::Error{"Invalid texture type."};
		}
		unreachable();
	}

	/**
	 * Get the image format corresponding to an internal texture format.
	 *
	 * \param internalFormat internal texture format to get the image format of.
	 *
	 * \return the corresponding image format.
	 *
	 * \throws graphics::Error if a corresponding image format cannot be
	 *         determined.
	 */
	[[nodiscard]] static constexpr resource::ImageFormat getImageFormat(TextureFormat internalFormat) {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: throw graphics::Error{"Invalid texture format."};
			case TextureFormat::R8_UNORM: return resource::ImageFormat::R8_UINT;
			case TextureFormat::R16_FLOAT: return resource::ImageFormat::R16_FLOAT;
			case TextureFormat::R32_FLOAT: return resource::ImageFormat::R32_FLOAT;
			case TextureFormat::R8G8_UNORM: return resource::ImageFormat::R8G8_UINT;
			case TextureFormat::R16G16_FLOAT: return resource::ImageFormat::R16G16_FLOAT;
			case TextureFormat::R32G32_FLOAT: return resource::ImageFormat::R32G32_FLOAT;
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: return resource::ImageFormat::R8G8B8A8_UINT;
			case TextureFormat::R16G16B16A16_FLOAT: return resource::ImageFormat::R16G16B16A16_FLOAT;
			case TextureFormat::R32G32B32A32_FLOAT: return resource::ImageFormat::R32G32B32A32_FLOAT;
			case TextureFormat::R5G6B5_UNORM_PACK16: return resource::ImageFormat::R5G6B5_UINT_PACK16;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return resource::ImageFormat::A1R5G5B5_UINT_PACK16;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return resource::ImageFormat::B10G11R11_UFLOAT_PACK32;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return resource::ImageFormat::A2B10G10R10_UINT_PACK32;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: return resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: return resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
			case TextureFormat::BC1_RGB_UNORM_BLOCK: return resource::ImageFormat::BC1_RGB_UINT_BLOCK;
			case TextureFormat::BC1_RGB_SRGB_BLOCK: return resource::ImageFormat::BC1_RGB_UINT_BLOCK;
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: return resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: return resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
			case TextureFormat::BC4_R_UNORM_BLOCK: return resource::ImageFormat::BC4_R_UINT_BLOCK;
			case TextureFormat::BC5_RG_UNORM_BLOCK: return resource::ImageFormat::BC5_RG_UINT_BLOCK;
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: return resource::ImageFormat::BC6H_RGB_UFLOAT_BLOCK;
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return resource::ImageFormat::BC6H_RGB_FLOAT_BLOCK;
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: return resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: return resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: return resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;
			case TextureFormat::EAC_R11_UNORM_BLOCK: return resource::ImageFormat::EAC_R11_UINT_BLOCK;
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return resource::ImageFormat::EAC_R11G11_UINT_BLOCK;
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: return resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
		}
		unreachable();
	}

	/**
	 * Get the texture type corresponding to an image type.
	 *
	 * \param type image type to get the texture type of.
	 *
	 * \return the corresponding texture type.
	 */
	[[nodiscard]] static constexpr TextureType getType(resource::ImageType type) noexcept {
		switch (type) {
			case resource::ImageType::EMPTY: return TextureType::EMPTY;
			case resource::ImageType::IMAGE_2D: return TextureType::TEXTURE_2D;
			case resource::ImageType::IMAGE_2D_ARRAY: return TextureType::TEXTURE_2D_ARRAY;
			case resource::ImageType::IMAGE_CUBE: return TextureType::TEXTURE_CUBE;
			case resource::ImageType::IMAGE_CUBE_ARRAY: return TextureType::TEXTURE_CUBE_ARRAY;
		}
		unreachable();
	}

	/**
	 * Check if an image format has a corresponding internal texture format that
	 * uses the sRGB transfer function.
	 *
	 * \param format image format to check.
	 *
	 * \return true if there exists an sRGB texture format that the given image
	 *         format corresponds to, false otherwise.
	 *
	 * \note This function does not guarantee that the sRGB texture format in
	 *       question is supported by the current graphics backend. Use
	 *       Device::getSupportedFeatures() to query this information.
	 */
	[[nodiscard]] static constexpr bool hasInternalSRGBFormat(resource::ImageFormat format) noexcept {
		switch (format) {
			case resource::ImageFormat::UNKNOWN: [[fallthrough]];
			case resource::ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case resource::ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::R8_UINT: [[fallthrough]];
			case resource::ImageFormat::R16_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R32_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R8G8_UINT: [[fallthrough]];
			case resource::ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case resource::ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case resource::ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case resource::ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case resource::ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::EAC_R11G11_UINT_BLOCK: return false;
			case resource::ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Get the transfer function of an internal texture format.
	 *
	 * \param internalFormat internal texture format to get the transfer
	 *        function of.
	 *
	 * \return the transfer function of the internal format if it is known,
	 *         Color::TransferFunction::LINEAR otherwise.
	 */
	[[nodiscard]] static constexpr Color::TransferFunction getTransferFunction(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: return Color::TransferFunction::LINEAR;
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return Color::TransferFunction::SRGB;
		}
		unreachable();
	}

	/**
	 * Get the internal texture format corresponding to an image format.
	 *
	 * \param format image format to get the internal texture format of.
	 * \param transferFunction preferred transfer function to use. Only applies
	 *        to image formats where the corresponding texture format has both
	 *        sRGB and non-sRGB variants.
	 *
	 * \return the corresponding internal texture format, or
	 *         TextureFormat::UNKNOWN if a corresponding internal texture format
	 *         cannot be determined.
	 *
	 * \sa hasInternalSRGBFormat()
	 */
	[[nodiscard]] static constexpr TextureFormat getInternalFormat(resource::ImageFormat format, Color::TransferFunction transferFunction) noexcept {
		switch (format) {
			case resource::ImageFormat::UNKNOWN: [[fallthrough]];
			case resource::ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case resource::ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case resource::ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case resource::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return TextureFormat::UNKNOWN;
			case resource::ImageFormat::R8_UINT: return TextureFormat::R8_UNORM;
			case resource::ImageFormat::R16_FLOAT: return TextureFormat::R16_FLOAT;
			case resource::ImageFormat::R32_FLOAT: return TextureFormat::R32_FLOAT;
			case resource::ImageFormat::R8G8_UINT: return TextureFormat::R8G8_UNORM;
			case resource::ImageFormat::R16G16_FLOAT: return TextureFormat::R16G16_FLOAT;
			case resource::ImageFormat::R32G32_FLOAT: return TextureFormat::R32G32_FLOAT;
			case resource::ImageFormat::R8G8B8A8_UINT: return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::R8G8B8A8_SRGB : TextureFormat::R8G8B8A8_UNORM;
			case resource::ImageFormat::R16G16B16A16_FLOAT: return TextureFormat::R16G16B16A16_FLOAT;
			case resource::ImageFormat::R32G32B32A32_FLOAT: return TextureFormat::R32G32B32A32_FLOAT;
			case resource::ImageFormat::R5G6B5_UINT_PACK16: return TextureFormat::R5G6B5_UNORM_PACK16;
			case resource::ImageFormat::A1R5G5B5_UINT_PACK16: return TextureFormat::A1R5G5B5_UNORM_PACK16;
			case resource::ImageFormat::B10G11R11_UFLOAT_PACK32: return TextureFormat::B10G11R11_UFLOAT_PACK32;
			case resource::ImageFormat::A2B10G10R10_UINT_PACK32: return TextureFormat::A2B10G10R10_UNORM_PACK32;
			case resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK : TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
			case resource::ImageFormat::BC1_RGB_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC1_RGB_SRGB_BLOCK : TextureFormat::BC1_RGB_UNORM_BLOCK;
			case resource::ImageFormat::BC3_RGBA_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC3_RGBA_SRGB_BLOCK : TextureFormat::BC3_RGBA_UNORM_BLOCK;
			case resource::ImageFormat::BC4_R_UINT_BLOCK: return TextureFormat::BC4_R_UNORM_BLOCK;
			case resource::ImageFormat::BC5_RG_UINT_BLOCK: return TextureFormat::BC5_RG_UNORM_BLOCK;
			case resource::ImageFormat::BC6H_RGB_UFLOAT_BLOCK: return TextureFormat::BC6H_RGB_UFLOAT_BLOCK;
			case resource::ImageFormat::BC6H_RGB_FLOAT_BLOCK: return TextureFormat::BC6H_RGB_FLOAT_BLOCK;
			case resource::ImageFormat::BC7_RGBA_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::BC7_RGBA_SRGB_BLOCK : TextureFormat::BC7_RGBA_UNORM_BLOCK;
			case resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8_UNORM_BLOCK;
			case resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK : TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK;
			case resource::ImageFormat::EAC_R11_UINT_BLOCK: return TextureFormat::EAC_R11_UNORM_BLOCK;
			case resource::ImageFormat::EAC_R11G11_UINT_BLOCK: return TextureFormat::EAC_R11G11_UNORM_BLOCK;
			case resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK:
				return (transferFunction == Color::TransferFunction::SRGB) ? TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK : TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
		}
		unreachable();
	}

	/**
	 * Get the raw format corresponding to a specific internal texture format.
	 *
	 * \param internalFormat internal texture format to get the raw internal
	 *        texture format of.
	 *
	 * \return the corresponding raw internal texture format.
	 */
	[[nodiscard]] static constexpr TextureFormat getRawFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: return TextureFormat::UNKNOWN;
			case TextureFormat::R8_UNORM: return TextureFormat::R8_UNORM;
			case TextureFormat::R16_FLOAT: return TextureFormat::R16_FLOAT;
			case TextureFormat::R32_FLOAT: return TextureFormat::R32_FLOAT;
			case TextureFormat::R8G8_UNORM: return TextureFormat::R8G8_UNORM;
			case TextureFormat::R16G16_FLOAT: return TextureFormat::R16G16_FLOAT;
			case TextureFormat::R32G32_FLOAT: return TextureFormat::R32G32_FLOAT;
			case TextureFormat::R8G8B8A8_UNORM: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::R8G8B8A8_SRGB: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::R16G16B16A16_FLOAT: return TextureFormat::R16G16B16A16_FLOAT;
			case TextureFormat::R32G32B32A32_FLOAT: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::D16_UNORM: return TextureFormat::D16_UNORM;
			case TextureFormat::D32_FLOAT: return TextureFormat::D32_FLOAT;
			case TextureFormat::D24_UNORM_S8_UINT: return TextureFormat::D24_UNORM_S8_UINT;
			case TextureFormat::D32_FLOAT_S8_UINT: return TextureFormat::D32_FLOAT_S8_UINT;
			case TextureFormat::R5G6B5_UNORM_PACK16: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return TextureFormat::R16G16B16A16_FLOAT;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC1_RGB_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC1_RGB_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC4_R_UNORM_BLOCK: return TextureFormat::R8_UNORM;
			case TextureFormat::BC5_RG_UNORM_BLOCK: return TextureFormat::R8G8_UNORM;
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::EAC_R11_UNORM_BLOCK: return TextureFormat::R8_UNORM;
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return TextureFormat::R8G8_UNORM;
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
		}
		unreachable();
	}

	/**
	 * Get the uncompressed format corresponding to a specific internal texture
	 * format.
	 *
	 * \param internalFormat internal texture format to get the uncompressed
	 *        internal texture format of.
	 *
	 * \return the corresponding uncompressed internal texture format.
	 *
	 * \note The corresponding uncompressed format may still be packed. To get
	 *       the raw format, use getRawFormat() instead.
	 */
	[[nodiscard]] static constexpr TextureFormat getUncompressedFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: return TextureFormat::UNKNOWN;
			case TextureFormat::R8_UNORM: return TextureFormat::R8_UNORM;
			case TextureFormat::R16_FLOAT: return TextureFormat::R16_FLOAT;
			case TextureFormat::R32_FLOAT: return TextureFormat::R32_FLOAT;
			case TextureFormat::R8G8_UNORM: return TextureFormat::R8G8_UNORM;
			case TextureFormat::R16G16_FLOAT: return TextureFormat::R16G16_FLOAT;
			case TextureFormat::R32G32_FLOAT: return TextureFormat::R32G32_FLOAT;
			case TextureFormat::R8G8B8A8_UNORM: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::R8G8B8A8_SRGB: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::R16G16B16A16_FLOAT: return TextureFormat::R16G16B16A16_FLOAT;
			case TextureFormat::R32G32B32A32_FLOAT: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::D16_UNORM: return TextureFormat::D16_UNORM;
			case TextureFormat::D32_FLOAT: return TextureFormat::D32_FLOAT;
			case TextureFormat::D24_UNORM_S8_UINT: return TextureFormat::D24_UNORM_S8_UINT;
			case TextureFormat::D32_FLOAT_S8_UINT: return TextureFormat::D32_FLOAT_S8_UINT;
			case TextureFormat::R5G6B5_UNORM_PACK16: return TextureFormat::R5G6B5_UNORM_PACK16;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return TextureFormat::A1R5G5B5_UNORM_PACK16;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return TextureFormat::B10G11R11_UFLOAT_PACK32;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return TextureFormat::A2B10G10R10_UNORM_PACK32;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC1_RGB_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC1_RGB_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::BC4_R_UNORM_BLOCK: return TextureFormat::R8_UNORM;
			case TextureFormat::BC5_RG_UNORM_BLOCK: return TextureFormat::R8G8_UNORM;
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return TextureFormat::R32G32B32A32_FLOAT;
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
			case TextureFormat::EAC_R11_UNORM_BLOCK: return TextureFormat::R8_UNORM;
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return TextureFormat::R8G8_UNORM;
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: return TextureFormat::R8G8B8A8_UNORM;
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return TextureFormat::R8G8B8A8_SRGB;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is directly compatible with an internal
	 * texture format.
	 *
	 * \param internalFormat internal texture format to check.
	 * \param format image format to check.
	 *
	 * \return true if the image format is definitely directly compatible with
	 *         the internal texture format, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isCompatibleFormat(TextureFormat internalFormat, resource::ImageFormat format) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return false;
			case TextureFormat::R8_UNORM: return format == resource::ImageFormat::R8_UINT;
			case TextureFormat::R16_FLOAT: return format == resource::ImageFormat::R16_FLOAT;
			case TextureFormat::R32_FLOAT: return format == resource::ImageFormat::R32_FLOAT;
			case TextureFormat::R8G8_UNORM: return format == resource::ImageFormat::R8G8_UINT;
			case TextureFormat::R16G16_FLOAT: return format == resource::ImageFormat::R16G16_FLOAT;
			case TextureFormat::R32G32_FLOAT: return format == resource::ImageFormat::R32G32_FLOAT;
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: return format == resource::ImageFormat::R8G8B8A8_UINT;
			case TextureFormat::R16G16B16A16_FLOAT: return format == resource::ImageFormat::R16G16B16A16_FLOAT;
			case TextureFormat::R32G32B32A32_FLOAT: return format == resource::ImageFormat::R32G32B32A32_FLOAT;
			case TextureFormat::R5G6B5_UNORM_PACK16: return format == resource::ImageFormat::R5G6B5_UINT_PACK16;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return format == resource::ImageFormat::A1R5G5B5_UINT_PACK16;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return format == resource::ImageFormat::B10G11R11_UFLOAT_PACK32;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return format == resource::ImageFormat::A2B10G10R10_UINT_PACK32;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: return format == resource::ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK;
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: return format == resource::ImageFormat::BC1_RGB_UINT_BLOCK;
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: return format == resource::ImageFormat::BC3_RGBA_UINT_BLOCK;
			case TextureFormat::BC4_R_UNORM_BLOCK: return format == resource::ImageFormat::BC4_R_UINT_BLOCK;
			case TextureFormat::BC5_RG_UNORM_BLOCK: return format == resource::ImageFormat::BC5_RG_UINT_BLOCK;
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: return format == resource::ImageFormat::BC6H_RGB_UFLOAT_BLOCK;
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return format == resource::ImageFormat::BC6H_RGB_FLOAT_BLOCK;
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: return format == resource::ImageFormat::BC7_RGBA_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return format == resource::ImageFormat::ETC2_R8G8B8_UINT_BLOCK;
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return format == resource::ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK;
			case TextureFormat::EAC_R11_UNORM_BLOCK: return format == resource::ImageFormat::EAC_R11_UINT_BLOCK;
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return format == resource::ImageFormat::EAC_R11G11_UINT_BLOCK;
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return format == resource::ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK;
		}
		unreachable();
	}

	/**
	 * Check if a specific internal texture format is an RGBA color format with
	 * an alpha channel.
	 *
	 * \param internalFormat internal texture format to check.
	 *
	 * \return true if the texture format is an RGBA color format with an alpha
	 *         channel, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isRGBAColorFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return false;
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific internal texture format is a raw format.
	 *
	 * \param internalFormat internal texture format to check.
	 *
	 * \return true if the internal texture format is raw, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isRawFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return false;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific internal texture format is a bit-packed format.
	 *
	 * \param internalFormat internal texture format to check.
	 *
	 * \return true if the internal texture format is packed, false otherwise.
	 *
	 * \note Compressed formats without any further bit-packing do not count as
	 *       packed formats.
	 */
	[[nodiscard]] static constexpr bool isPackedFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return false;
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific internal texture format is a compressed format.
	 *
	 * \param internalFormat internal texture format to check.
	 *
	 * \return true if the internal texture format is compressed, false
	 *         otherwise.
	 *
	 * \note Bit-packed formats without any further compression do not count as
	 *       compressed formats.
	 */
	[[nodiscard]] static constexpr bool isCompressedFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return false;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific internal texture format is framebuffer-compatible.
	 *
	 * \param internalFormat internal texture format to check.
	 *
	 * \return true if the internal texture format is framebuffer-compatible,
	 *         false otherwise.
	 *
	 * \note Even if this function returns true, the specified format may not be
	 *       supported on all users' graphics drivers.
	 */
	[[nodiscard]] static constexpr bool isFramebufferCompatibleFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::BC4_R_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC5_RG_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: [[fallthrough]];
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return false;
			case TextureFormat::R8_UNORM: [[fallthrough]];
			case TextureFormat::R16_FLOAT: [[fallthrough]];
			case TextureFormat::R32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8_UNORM: [[fallthrough]];
			case TextureFormat::R16G16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32_FLOAT: [[fallthrough]];
			case TextureFormat::R8G8B8A8_UNORM: [[fallthrough]];
			case TextureFormat::R8G8B8A8_SRGB: [[fallthrough]];
			case TextureFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case TextureFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case TextureFormat::D16_UNORM: [[fallthrough]];
			case TextureFormat::D32_FLOAT: [[fallthrough]];
			case TextureFormat::D24_UNORM_S8_UINT: [[fallthrough]];
			case TextureFormat::D32_FLOAT_S8_UINT: [[fallthrough]];
			case TextureFormat::R5G6B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case TextureFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return true;
		}
		unreachable();
	}

	/**
	 * Create a new texture object and allocate GPU memory for storing image
	 * data.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param type type of texture object to create.
	 * \param internalFormat internal texture format of the new texture. Must
	 *        not be TextureFormat::UNKNOWN.
	 * \param size size of the image data to allocate, in texels. Must fit in
	 *        the maximum texture size specified in
	 *        `device.getSupportedFeatures()` corresponding to the given texture
	 *        type.
	 * \param mipLevelCount number of mip levels to allocate.
	 * \param pixels non-owning read-only pointer to the pixel data of the input
	 *        image to copy into the new texture data storage, or nullptr to
	 *        leave the data uninitialized.
	 * \param samplerOptions sampler options, see TextureSamplerOptions. Set to
	 *        an empty optional to create an unsampled texture.
	 *
	 * \return the newly created texture.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 *
	 * \warning If not nullptr, the pixel data pointed to by the pixels
	 *          parameter must be of the exact shape and format described by the
	 *          size and internalFormat parameters.
	 */
	[[nodiscard]] GREM_API(graphics) static Texture create(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount,
		const void* pixels, Optional<TextureSamplerOptions> samplerOptions = TextureSamplerOptions{});

	/**
	 * Create a new texture object and allocate uninitialized GPU memory for
	 * storing image data.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param type type of texture object to create.
	 * \param internalFormat internal texture format of the new texture. Must
	 *        not be TextureFormat::UNKNOWN.
	 * \param size size of the image data to allocate, in texels. Must fit in
	 *        the maximum texture size specified in
	 *        `device.getSupportedFeatures()` corresponding to the given texture
	 *        type.
	 * \param mipLevelCount number of mip levels to allocate.
	 * \param values undefined clear value tag.
	 * \param samplerOptions sampler options, see TextureSamplerOptions. Set to
	 *        an empty optional to create an unsampled texture.
	 *
	 * \return the newly created texture.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] static Texture create(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount, const UndefinedClearValues& values,
		Optional<TextureSamplerOptions> samplerOptions = TextureSamplerOptions{}) {
		(void)values;
		return create(device, type, internalFormat, size, mipLevelCount, nullptr, samplerOptions);
	}

	/**
	 * Create a new texture object and allocate GPU memory for storing image
	 * data, initialized to specific values.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param type type of texture object to create.
	 * \param internalFormat internal texture format of the new texture. Must
	 *        not be TextureFormat::UNKNOWN.
	 * \param size size of the image data to allocate, in texels. Must fit in
	 *        the maximum texture size specified in
	 *        `device.getSupportedFeatures()` corresponding to the given texture
	 *        type.
	 * \param mipLevelCount number of mip levels to allocate.
	 * \param values initial values to fill the texture with.
	 * \param samplerOptions sampler options, see TextureSamplerOptions. Set to
	 *        an empty optional to create an unsampled texture.
	 *
	 * \return the newly created texture.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] static Texture create(Device& device, TextureType type, TextureFormat internalFormat, Extent3D size, uint32_t mipLevelCount, const ClearValues& values,
		Optional<TextureSamplerOptions> samplerOptions = TextureSamplerOptions{}) {
		Texture result = create(device, type, internalFormat, size, mipLevelCount, UndefinedClearValues{}, samplerOptions);
		result.fill(values);
		return result;
	}

	/**
	 * Create a new unsampled texture object and allocate uninitialized GPU
	 * memory for storing potentially multisampled 2D image data.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param internalFormat internal texture format of the new texture. Must
	 *        not be TextureFormat::UNKNOWN.
	 * \param size size of the 2D image data to allocate, in texels. Must fit in
	 *        the maximum 2D texture size specified in
	 *        `device.getSupportedFeatures()`.
	 * \param maxMultisampleCount maximum number of samples to use for the
	 *        texels. The actual sample count may be lower than requested due to
	 *        implementation-specific limitations. Set to 0 or 1 to disable
	 *        multisampling.
	 * \param values undefined clear value tag.
	 *
	 * \return the newly created texture.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] GREM_API(graphics) static Texture
		createRenderbuffer(Device& device, TextureFormat internalFormat, Extent2D size, uint32_t maxMultisampleCount, const UndefinedClearValues& values);

	/**
	 * Create a new unsampled texture object and allocate GPU memory for storing
	 * potentially multisampled 2D image data, initialized to specific values.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param internalFormat internal texture format of the new texture. Must
	 *        not be TextureFormat::UNKNOWN.
	 * \param size size of the 2D image data to allocate, in texels. Must fit in
	 *        the maximum 2D texture size specified in
	 *        `device.getSupportedFeatures()`.
	 * \param maxMultisampleCount maximum number of samples to use for the
	 *        texels. The actual sample count may be lower than requested due to
	 *        implementation-specific limitations. Set to 0 or 1 to disable
	 *        multisampling.
	 * \param values initial values to fill the texture with.
	 *
	 * \return the newly created texture.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] static Texture createRenderbuffer(Device& device, TextureFormat internalFormat, Extent2D size, uint32_t maxMultisampleCount, const ClearValues& values) {
		Texture result = createRenderbuffer(device, internalFormat, size, maxMultisampleCount, UndefinedClearValues{});
		result.fill(values);
		return result;
	}

	/**
	 * Construct an empty texture without a value.
	 */
	Texture() noexcept = default;

	/**
	 * Construct a new texture and allocate GPU memory for storing a copy of an
	 * image.
	 *
	 * \param device device to create the texture for. Must outlive the texture.
	 * \param image non-owning read-only view over the image to copy into the
	 *        new texture data storage. The allocated storage will be sized to
	 *        fit the image.
	 * \param options image upload options, see TextureImageUploadOptions.
	 * \param samplerOptions sampler options, see TextureSamplerOptions. Set to
	 *        an empty optional to create an unsampled texture.
	 *
	 * \throws graphics::Error if resource creation failed, or on failure to
	 *         choose an appropriate internal texture format for the given
	 *         image.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 *
	 * \note A suitable texture type and internal texture format is chosen
	 *       automatically based on the type and format of the image. To choose
	 *       the type and internal format manually, use one of the static
	 *       texture creation functions instead.
	 */
	GREM_API(graphics)
	Texture(Device& device, const resource::ImageView& image, const TextureImageUploadOptions& options = {},
		Optional<TextureSamplerOptions> samplerOptions = TextureSamplerOptions{});

	/**
	 * Check if the texture has a value.
	 *
	 * \return true if the texture has a value, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return static_cast<bool>(implementation);
	}

	/**
	 * Copy all or part of an array of layers of 2D image data into the texture
	 * at a specific position.
	 *
	 * \param imageSize full size of the input image, in pixels.
	 * \param pixels non-owning read-only pointer to the pixel data of the input
	 *        image array to copy into the existing texture data storage. Must
	 *        not be nullptr.
	 * \param destinationOffset offset, in texels, from the bottom left corner
	 *        of the first layer of the destination texture to start pasting at,
	 *        where the bottom left corner of the first layer of the pasted
	 *        image region will begin.
	 * \param sourceRegion region, in texels, relative to the top left corner of
	 *        the first layer of the source image, to copy.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to
	 *         paste the given image data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function must only be called on 2D or 2D array textures.
	 * \warning The pixel data pointed to by the pixels parameter must have the
	 *          exact shape described by the imageSize parameter, and the same
	 *          format as the internal format of the texture.
	 * \warning Enough space must be allocated in the texture for the copied
	 *          image region to fit at the given position.
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	GREM_API(graphics) void pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset, Region3D sourceRegion);

	/**
	 * Copy all of an array of layers of 2D image data into the texture at a
	 * specific position.
	 *
	 * \param imageSize full size of the input image to copy, in pixels.
	 * \param pixels non-owning read-only pointer to the pixel data of the input
	 *        image array to copy into the existing texture data storage. Must
	 *        not be nullptr.
	 * \param destinationOffset offset, in texels, from the bottom left corner
	 *        of the first layer of the destination texture to start pasting at,
	 *        where the bottom left corner of the first layer of the pasted
	 *        image region will begin.
	 * \param sourceOffset offset, in texels, from the top left corner of the
	 *        first layer of the source image to start copying from.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to
	 *         paste the given image data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function must only be called on 2D or 2D array textures.
	 * \warning The pixel data pointed to by the pixels parameter must have the
	 *          exact shape described by the imageSize parameter, and the same
	 *          format as the internal format of the texture.
	 * \warning Enough space must be allocated in the texture for the copied
	 *          image region to fit at the given position.
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	void pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset = {.x = 0, .y = 0, .z = 0}, Offset3D sourceOffset = {.x = 0, .y = 0, .z = 0}) {
		pasteImage(imageSize, pixels, destinationOffset, Region3D{.offset = sourceOffset, .size = imageSize});
	}

	/**
	 * Copy all or part of the image data of another 2D or 2D array texture into
	 * the texture at a specific position.
	 *
	 * \param texture other texture to copy into the existing texture data
	 *        storage of this texture. Must be a 2D or 2D array texture.
	 * \param destinationOffset offset, in texels, from the bottom left corner
	 *        of the first layer of the destination texture to start pasting at,
	 *        where the bottom left corner of the first layer of the pasted
	 *        image region will begin.
	 * \param sourceRegion region, in texels, relative to the bottom left corner
	 *        of the first layer of the source texture, to copy.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to
	 *         paste the given image data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function must only be called on 2D or 2D array textures.
	 * \warning Enough space must be allocated in the texture for the copied
	 *          image region to fit at the given position.
	 * \warning Both textures must have the same internal texture format.
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	GREM_API(graphics) void pasteTexture(const Texture& texture, Offset3D destinationOffset, Region3D sourceRegion);

	/**
	 * Copy all of the image data of another 2D or 2D array texture into the
	 * texture at a specific position.
	 *
	 * \param texture other texture to copy into the existing texture data
	 *        storage of this texture. Must be a 2D or 2D array texture.
	 * \param destinationOffset offset, in texels, from the bottom left corner
	 *        of the first layer of the destination texture to start pasting at,
	 *        where the bottom left corner of the first layer of the pasted
	 *        image region will begin.
	 * \param sourceOffset offset, in texels, from the bottom left corner of the
	 *        first layer of the source texture to start copying from.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to
	 *         paste the given image data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function must only be called on 2D or 2D array textures.
	 * \warning Enough space must be allocated in the texture for the copied
	 *          image region to fit at the given position.
	 * \warning Both textures must have the same internal texture format.
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	void pasteTexture(const Texture& texture, Offset3D destinationOffset = {.x = 0, .y = 0, .z = 0}, Offset3D sourceOffset = {.x = 0, .y = 0, .z = 0}) {
		pasteTexture(texture, destinationOffset, Region3D{.offset = sourceOffset, .size = texture.getSize3D()});
	}

	/**
	 * Fill all allocated texture data with specific values.
	 *
	 * \param values values to fill the texture with.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to fill
	 *         the texture.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	GREM_API(graphics) void fill(const ClearValues& values = {});

	/**
	 * Fill a subresource of the texture with specific values.
	 *
	 * \param subresource texture subresource to fill. Must be a valid
	 *        subresource of this texture.
	 * \param values values to fill the subresource with.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to fill
	 *         the texture.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The internal texture format must be framebuffer-compatible.
	 */
	GREM_API(graphics) void fill(TextureSubresource subresource, const ClearValues& values = {});

	/**
	 * Generate a mipmap, with the maximum possible number of mip levels, for
	 * the texture based on the highest quality mip level.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to
	 *         regenerate the mipmap.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Mipmaps are automatically regenerated without having to call this
	 *       function if the texture was modified using one of its member
	 *       functions, but not if the texture was rendered to.
	 */
	GREM_API(graphics) void generateMipmap();

	/**
	 * Create a new texture object, allocate GPU memory and copy the image data
	 * of this texture onto it.
	 *
	 * \return the new copy of this texture, or an empty texture if this texture
	 *         is empty.
	 *
	 * \warning The internal texture format must be framebuffer-compatible.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to copy
	 *         the texture.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] GREM_API(graphics) Texture copy() const;

	/**
	 * Create a new texture object with new sampler options, allocate GPU memory
	 * and copy the image data of this texture onto it.
	 *
	 * \param newSamplerOptions sampler options of the new texture.
	 *
	 * \return the new copy of this texture, with the given sampler options, or
	 *         an empty texture if this texture is empty.
	 *
	 * \warning The internal texture format must be framebuffer-compatible.
	 *
	 * \throws graphics::Error if resource creation failed or on failure to copy
	 *         the texture.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure. Note: this pertains only to
	 *         CPU memory allocations. Failure to allocate GPU memory for the
	 *         texture data might not be reported directly.
	 */
	[[nodiscard]] GREM_API(graphics) Texture copyWithSamplerOptions(Optional<TextureSamplerOptions> newSamplerOptions) const;

	/**
	 * Download a copy of all or part of the image stored in the texture.
	 *
	 * \param downloadOptions texture download options, see
	 *        TextureImageDownloadOptions.
	 *
	 * \return an image containing a copy of the specified pixels stored in the
	 *         texture.
	 *
	 * \throws graphics::Error on failure to download the image data.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function must only be called on 2D, 2D array, cubemap or
	 *          cubemap array textures.
	 * \warning Downloading data from the GPU tends to be very slow since the
	 *          CPU needs to wait to make sure that the transfer has completed
	 *          before resuming control. As such, this function should only be
	 *          used sparingly, such as for saving screenshots.
	 */
	[[nodiscard]] GREM_API(graphics) resource::Image downloadImage(const TextureImageDownloadOptions& downloadOptions = {}) const;

	/**
	 * Get a reference to the first subresource of the texture.
	 *
	 * \return a reference to the first subresource of the texture.
	 */
	operator TextureSubresourceReference() noexcept {
		return {.texture = this, .subresource{}};
	}

	/**
	 * Get a read-only reference to the first subresource of the texture.
	 *
	 * \return a read-only reference to the first subresource of the texture.
	 */
	operator TextureSubresourceConstReference() const noexcept {
		return {.texture = this, .subresource{}};
	}

	/**
	 * Get a reference to a subresource of the texture.
	 *
	 * \param subresource subresource to get a reference to. Must be a valid
	 *        subresource of this texture.
	 *
	 * \return a reference to the given subresource.
	 */
	[[nodiscard]] TextureSubresourceReference getSubresource(TextureSubresource subresource) noexcept {
		return {.texture = this, .subresource = subresource};
	}

	/**
	 * Get a read-only reference to a subresource of the texture.
	 *
	 * \param subresource subresource to get a reference to. Must be a valid
	 *        subresource of this texture.
	 *
	 * \return a read-only reference to the given subresource.
	 */
	[[nodiscard]] TextureSubresourceConstReference getSubresource(TextureSubresource subresource) const noexcept {
		return {.texture = this, .subresource = subresource};
	}

	/**
	 * Get a reference to the first 2D layer region of the texture.
	 *
	 * \return a reference to the first 2D layer region of the texture.
	 */
	operator TextureRegion2DReference() noexcept {
		return {.texture = this, .region{.size = getSize2D()}};
	}

	/**
	 * Get a read-only reference to the first 2D layer region of the texture.
	 *
	 * \return a read-only reference to the first 2D layer region of the
	 *         texture.
	 */
	operator TextureRegion2DConstReference() const noexcept {
		return {.texture = this, .region{.size = getSize2D()}};
	}

	/**
	 * Get a reference to a 2D region of the texture.
	 *
	 * \param region region to get a reference to. Must be a valid region of
	 *        this texture.
	 *
	 * \return a reference to the given region.
	 */
	[[nodiscard]] TextureRegion2DReference getRegion2D(TextureRegion2D region) noexcept {
		return {.texture = this, .region = region};
	}

	/**
	 * Get a read-only reference to a 2D region of the texture.
	 *
	 * \param region region to get a reference to. Must be a valid region of
	 *        this texture.
	 *
	 * \return a read-only reference to the given region.
	 */
	[[nodiscard]] TextureRegion2DConstReference getRegion2D(TextureRegion2D region) const noexcept {
		return {.texture = this, .region = region};
	}

	/**
	 * Get a reference to a 3D region of the texture.
	 *
	 * \param region region to get a reference to. Must be a valid region of
	 *        this texture.
	 *
	 * \return a reference to the given region.
	 */
	[[nodiscard]] TextureRegion3DReference getRegion3D(TextureRegion3D region) noexcept {
		return {.texture = this, .region = region};
	}

	/**
	 * Get a read-only reference to a 3D region of the texture.
	 *
	 * \param region region to get a reference to. Must be a valid region of
	 *        this texture.
	 *
	 * \return a read-only reference to the given region.
	 */
	[[nodiscard]] TextureRegion3DConstReference getRegion3D(TextureRegion3D region) const noexcept {
		return {.texture = this, .region = region};
	}

	/**
	 * Get the type of the texture.
	 *
	 * \return the texture type.
	 */
	[[nodiscard]] GREM_API(graphics) TextureType getType() const noexcept;

	/**
	 * Get the internal texture format of this texture.
	 *
	 * \return the internal texture format, or TextureFormat::UNKNOWN if the
	 *         texture does not have a value.
	 */
	[[nodiscard]] GREM_API(graphics) TextureFormat getInternalFormat() const noexcept;

	/**
	 * Get the size, in texels, of the 2D image data stored in this texture.
	 *
	 * \return the size of the texture, in texels, or (0, 0) if the texture does
	 *         not have a value.
	 *
	 * \note For 2D array textures, this function returns the width and height
	 *       of a single image layer in the array.
	 * \note For cube textures, this function returns the size of a single side
	 *       of the cubemap.
	 *
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getHeight()
	 */
	[[nodiscard]] Extent2D getSize2D() const noexcept {
		const Extent3D size = getSize3D();
		return Extent2D{.width = size.width, .height = size.height};
	}

	/**
	 * Get the size, in texels, and the depth, in layers, of the 3D image data
	 * stored in this texture.
	 *
	 * \return the size of the texture, in texels, or (0, 0, 0) if the texture
	 *         does not have a value.
	 *
	 * \sa getSize2D()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 */
	[[nodiscard]] GREM_API(graphics) Extent3D getSize3D() const noexcept;

	/**
	 * Get the width, in texels, of the image data stored in this texture.
	 *
	 * \return the width of the texture, in texels, or 0 if the texture does not
	 *         have a value.
	 *
	 * \note For 2D array textures, this function returns the width of a single
	 *       image layer in the array.
	 * \note For cube textures, this function returns the width of a single side
	 *       of the cubemap.
	 *
	 * \sa getSize2D()
	 * \sa getSize3D()
	 * \sa getHeight()
	 * \sa getDepth()
	 */
	[[nodiscard]] uint32_t getWidth() const noexcept {
		return getSize3D().width;
	}

	/**
	 * Get the height, in texels, of the image data stored in this texture.
	 *
	 * \return the height of the texture, in texels, or 0 if the texture does
	 *         not have a value.
	 *
	 * \note For 2D array textures, this function returns the height of a single
	 *       image layer in the array.
	 * \note For cube textures, this function returns the height of a single side
	 *       of the cubemap.
	 *
	 * \sa getSize2D()
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getDepth()
	 */
	[[nodiscard]] uint32_t getHeight() const noexcept {
		return getSize3D().height;
	}

	/**
	 * Get the depth, in layers, of the image data stored in this texture.
	 *
	 * \return the depth of the texture, in layers, or 0 if the texture does
	 *         not have a value.
	 *
	 * \note For 2D textures, this function always returns 1. For 2D array
	 *       textures, this function returns the number of image layers in the
	 *       array. For cube textures, this function always returns 6. For cube
	 *       array textures, this function returns the number of cube layers in
	 *       the array times 6.
	 *
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getHeight()
	 */
	[[nodiscard]] uint32_t getDepth() const noexcept {
		return getSize3D().depth;
	}

	/**
	 * Get the number of mip levels of the texture.
	 *
	 * \return the number of mip levels of each image layer in the texture, or 0
	 *         if the texture does not have a value.
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t getMipLevelCount() const noexcept;

	/**
	 * Get the maximum multisample count of the texture.
	 *
	 * \return the maximum number of samples requested to be used for the texels
	 *         (1 if the texture is not multisampled), or 0 if the texture does
	 *         not have a value.
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t getMaxMultisampleCount() const noexcept;

	/**
	 * Get the configuration options of this texture's associated sampler.
	 *
	 * \return the current sampler options of the texture, or an empty optional
	 *         if the texture does not have a value or is an unsampled texture.
	 */
	[[nodiscard]] GREM_API(graphics) Optional<TextureSamplerOptions> getSamplerOptions() const noexcept;

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<TextureImplementation> lock() const noexcept {
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
	[[nodiscard]] TextureImplementation* get() const noexcept {
		return implementation.get();
	}

private:
	friend Device;
	friend DeviceImplementation;
	friend Swapchain;
	friend TextureImplementation;

	explicit Texture(SharedPointer<TextureImplementation> handle) noexcept
		: implementation(std::move(handle)) {}

	SharedPointer<TextureImplementation> implementation{};
};

} // namespace grem::graphics

#endif
