// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_IMAGE_HPP
#define GREM_RESOURCE_IMAGE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Filesystem.hpp>

#include <stdexcept> // std::length_error

namespace grem::resource {

class Image; // Forward declaration.

/**
 * File type of an Image.
 */
enum class ImageFileType : uint8_t {
	UNKNOWN, ///< Unknown file type.
	JPEG,    ///< JPEG (.jpg/.jpeg).
	PNG,     ///< PNG (.png).
	HDR,     ///< RGBE/Radiance HDR (.hdr).
	KTX2,    ///< Khronos TeXture, version 2.0 (.ktx2).
};

/**
 * Type of an Image.
 */
enum class ImageType : uint8_t {
	EMPTY,            ///< Empty image without a value.
	IMAGE_2D,         ///< A single 2D image.
	IMAGE_2D_ARRAY,   ///< An array of 2D images.
	IMAGE_CUBE,       ///< A cube of 6 sides of square 2D images.
	IMAGE_CUBE_ARRAY, ///< An array of cubes of 6 sides of square 2D images.
};

/**
 * Description of the format of an Image, including the number of
 * component channels, their layout, their meanings and their data types.
 */
enum class ImageFormat : uint8_t {
	UNKNOWN,                     ///< Unknown image format.
	R8_UINT,                     ///< Raw bitmap where each pixel comprises 1 8-bit unsigned integer component:  red.
	R16_FLOAT,                   ///< Raw bitmap where each pixel comprises 1 16-bit floating-point component:   red.
	R32_FLOAT,                   ///< Raw bitmap where each pixel comprises 1 32-bit floating-point component:   red.
	R8G8_UINT,                   ///< Raw bitmap where each pixel comprises 2 8-bit unsigned integer components: red, green.
	R16G16_FLOAT,                ///< Raw bitmap where each pixel comprises 2 16-bit floating-point components:  red, green.
	R32G32_FLOAT,                ///< Raw bitmap where each pixel comprises 2 32-bit floating-point components:  red, green.
	R8G8B8_UINT,                 ///< Raw bitmap where each pixel comprises 3 8-bit unsigned integer components: red, green, blue.
	R16G16B16_FLOAT,             ///< Raw bitmap where each pixel comprises 3 16-bit floating-point components:  red, green, blue.
	R32G32B32_FLOAT,             ///< Raw bitmap where each pixel comprises 3 32-bit floating-point components:  red, green, blue.
	R8G8B8A8_UINT,               ///< Raw bitmap where each pixel comprises 4 8-bit unsigned integer components: red, green, blue, alpha.
	R16G16B16A16_FLOAT,          ///< Raw bitmap where each pixel comprises 4 16-bit floating-point components:  red, green, blue, alpha.
	R32G32B32A32_FLOAT,          ///< Raw bitmap where each pixel comprises 4 32-bit floating-point components:  red, green, blue, alpha.
	R5G6B5_UINT_PACK16,          ///< Packed bitmap with the following 16-bit unsigned integer RGB pixel format:        RRRRRGGGGGGBBBBB.
	A1R5G5B5_UINT_PACK16,        ///< Packed bitmap with the following 16-bit unsigned integer RGBA pixel format:       ARRRRRGGGGGBBBBB.
	B10G11R11_UFLOAT_PACK32,     ///< Packed bitmap with the following 32-bit unsigned floating-point RGB pixel format: BBBBBBBBBBGGGGGGGGGGGRRRRRRRRRRR.
	A2B10G10R10_UINT_PACK32,     ///< Packed bitmap with the following 32-bit unsigned integer RGBA pixel format:       AABBBBBBBBBBGGGGGGGGGGRRRRRRRRRR.
	ASTC_4x4_RGBA_UINT_BLOCK,    ///< ASTC block-compressed RGBA image using 128 bits per 4x4 block.
	BC1_RGB_UINT_BLOCK,          ///< S3TC BC1 (DXT1) block-compressed RGB image using 64 bits per 4x4 block.
	BC3_RGBA_UINT_BLOCK,         ///< S3TC BC3 (DXT5) block-compressed RGBA image using 128 bits per 4x4 block.
	BC4_R_UINT_BLOCK,            ///< S3TC BC4 (RGTC1) block-compressed single-channel image using 64 bits per 4x4 block.
	BC5_RG_UINT_BLOCK,           ///< S3TC BC5 (RGTC2) block-compressed double-channel image using 128 bits per 4x4 block.
	BC6H_RGB_UFLOAT_BLOCK,       ///< S3TC BC6H (BPTC) block-compressed unsigned floating-point RGB image using 128 bits per 4x4 block.
	BC6H_RGB_FLOAT_BLOCK,        ///< S3TC BC6H (BPTC) block-compressed signed floating-point RGB image using 128 bits per 4x4 block.
	BC7_RGBA_UINT_BLOCK,         ///< S3TC BC7 (BPTC) block-compressed RGBA image using 128 bits per 4x4 block.
	ETC2_R8G8B8_UINT_BLOCK,      ///< ETC2 block-compressed RGB image using 64 bits per 4x4 block.
	ETC2_R8G8B8A8_UINT_BLOCK,    ///< ETC2 block-compressed RGBA image using 128 bits per 4x4 block.
	EAC_R11_UINT_BLOCK,          ///< ETC2 EAC block-compressed single-channel image using 64 bits per 4x4 block.
	EAC_R11G11_UINT_BLOCK,       ///< ETC2 EAC block-compressed double-channel image using 128 bits per 4x4 block.
	PVRTC1_4BPP_RGBA_UINT_BLOCK, ///< PVRTC1 block-compressed RGBA image using 64 bits per 4x4 block.
	KTX2_ETC1S_R_UINT_BLOCK,     ///< KTX2 container of an ETC1-compressed ETC1S-encoded image with 1 DFD channel:  RRR.
	KTX2_ETC1S_RG_UINT_BLOCK,    ///< KTX2 container of an ETC1-compressed ETC1S-encoded image with 2 DFD channels: RRR and GGG.
	KTX2_ETC1S_RGB_UINT_BLOCK,   ///< KTX2 container of an ETC1-compressed ETC1S-encoded image with 1 DFD channel:  RGB.
	KTX2_ETC1S_RGBA_UINT_BLOCK,  ///< KTX2 container of an ETC1-compressed ETC1S-encoded image with 2 DFD channels: RGB and AAA.
	KTX2_UASTC_R_UINT_BLOCK,     ///< KTX2 container of an ASTC 4x4-compressed UASTC LDR-encoded image with 1 DFD channel: RRR.
	KTX2_UASTC_RG_UINT_BLOCK,    ///< KTX2 container of an ASTC 4x4-compressed UASTC LDR-encoded image with 1 DFD channel: RG.
	KTX2_UASTC_RGB_UINT_BLOCK,   ///< KTX2 container of an ASTC 4x4-compressed UASTC LDR-encoded image with 1 DFD channel: RGB.
	KTX2_UASTC_RGBA_UINT_BLOCK,  ///< KTX2 container of an ASTC 4x4-compressed UASTC LDR-encoded image with 1 DFD channel: RGBA.
};

/**
 * Subresource of an image.
 */
struct ImageSubresource {
	uint32_t layer = 0;    ///< Layer index in the image.
	uint32_t mipLevel = 0; ///< Mip level index in the image.
};

/**
 * Read-only non-owning view over a 2D image.
 *
 * \sa ImageReference
 * \sa Image
 */
class ImageView {
public:
	/**
	 * Construct a view that does not reference an image.
	 */
	constexpr ImageView() noexcept = default;

	/**
	 * Construct an image view over arbitrary 2D pixel data.
	 *
	 * \param type image type.
	 * \param format image format.
	 * \param size3D size3D of the image, in pixels and layers. If type is
	 *        ImageType::IMAGE_CUBE or ImageType::IMAGE_CUBE_ARRAY, width must
	 *        be equal to height. If type is ImageType::IMAGE_2D, depth must be
	 *        1. If type is ImageType::IMAGE_CUBE, depth must be 6. If type is
	 *        ImageType::IMAGE_CUBE_ARRAY, depth must be a multiple of 6.
	 * \param mipLevelCount number of mip levels stored for the image. Must be
	 *        less than or equal to the result of Image::getMaxMipLevelCount()
	 *        for the given width and height.
	 * \param contents read-only view over the image data, or an empty span to
	 *        create a view that doesn't reference an image. Must have a valid
	 *        size for the given type and shape of the image, including all
	 *        layers and mip levels.
	 */
	constexpr ImageView(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Span<const byte> contents)
		: contents(contents)
		, type(type)
		, format(format)
		, size3D(size3D)
		, mipLevelCount(mipLevelCount) {
		GREM_ASSERT(type != ImageType::IMAGE_2D || size3D.depth == 1);
		GREM_ASSERT((type != ImageType::IMAGE_CUBE && type != ImageType::IMAGE_CUBE_ARRAY) || size3D.width == size3D.height);
		GREM_ASSERT(type != ImageType::IMAGE_CUBE || size3D.depth == 6);
		GREM_ASSERT(type != ImageType::IMAGE_CUBE_ARRAY || size3D.depth % 6 == 0);
		GREM_ASSERT(mipLevelCount <= static_cast<uint32_t>(getRequiredBitCount(max(size3D.width, size3D.height))));
	}

	/**
	 * Transcode the image data to a specific format.
	 *
	 * \param output non-owning pointer to a block of memory to write the
	 *        transcoded image data to. Must point to a valid block of memory of
	 *        at least the size required for the full image in the given output
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param outputFormat image format to transcode to.
	 *
	 * \throws std::invalid_argument if transcoding failed, or if transcoding
	 *         from the image's current format to the given format is not
	 *         supported.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The following transcodings are supported:
	 *       - Copying any non-containerized format to the same format
	 *       - KTX2 ETC1S/UASTC -> ASTC 4x4 LDR/BC1-5/Unsigned BC6H/BC7/ETC1/ETC2/EAC/PVRTC1
	 *       - KTX2 ETC1S/UASTC -> Raw format
	 *       - Raw 32-bit float format -> Raw 16-bit float format
	 *       - Raw 16-bit float format -> Raw 32-bit float format
	 */
	GREM_API(resource) void transcodeTo(byte* output, ImageFormat outputFormat) const;

	/**
	 * Get the type of the image referenced by this view.
	 *
	 * \return the image type.
	 */
	[[nodiscard]] constexpr ImageType getType() const noexcept {
		return type;
	}

	/**
	 * Get the format of the image referenced by this view.
	 *
	 * \return the image format.
	 */
	[[nodiscard]] constexpr ImageFormat getFormat() const noexcept {
		return format;
	}

	/**
	 * Get the size, in pixels, of the image layers referenced by this view.
	 *
	 * \return the size of a layer of the image, in pixels.
	 *
	 * \note For 2D array images, this function returns the width and height of
	 *       a single image layer in the array.
	 * \note For cube images, this function returns the size of a single side of
	 *       the cubemap.
	 *
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getHeight()
	 */
	[[nodiscard]] constexpr Extent2D getSize2D() const noexcept {
		return Extent2D{.width = size3D.width, .height = size3D.height};
	}

	/**
	 * Get the size, in pixels, and the depth, in layers, of the image
	 * referenced by this view.
	 *
	 * \return the size of the image, in pixels and layers.
	 *
	 * \sa getSize2D()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 */
	[[nodiscard]] constexpr Extent3D getSize3D() const noexcept {
		return size3D;
	}

	/**
	 * Get the width of the image referenced by this view.
	 *
	 * \return the width of the image, in pixels.
	 */
	[[nodiscard]] constexpr uint32_t getWidth() const noexcept {
		return size3D.width;
	}

	/**
	 * Get the height of the image referenced by this view.
	 *
	 * \return the height of the image, in pixels.
	 */
	[[nodiscard]] constexpr uint32_t getHeight() const noexcept {
		return size3D.height;
	}

	/**
	 * Get the depth of the image referenced by this view.
	 *
	 * \return the depth of the image, in layers.
	 */
	[[nodiscard]] constexpr uint32_t getDepth() const noexcept {
		return size3D.depth;
	}

	/**
	 * Get the number of mip levels in the image referenced by this view.
	 *
	 * \return the number of mip levels in the image.
	 */
	[[nodiscard]] constexpr uint32_t getMipLevelCount() const noexcept {
		return mipLevelCount;
	}

	/**
	 * Get the image data referenced by this view.
	 *
	 * \return a read-only view over the image data.
	 *
	 * \sa getType()
	 * \sa getFormat()
	 * \sa getSize3D()
	 * \sa getMipLevelCount()
	 * \sa data()
	 * \sa size()
	 */
	[[nodiscard]] constexpr Span<const byte> getContents() const noexcept {
		return contents;
	}

	/**
	 * Get a pointer to the image data referenced by this view.
	 *
	 * \return a non-owning read-only pointer to the image data.
	 *
	 * \sa size()
	 */
	[[nodiscard]] constexpr const byte* data() const noexcept {
		return contents.data();
	}

	/**
	 * Get the size of the image data.
	 *
	 * \return the size of the image data, in bytes.
	 *
	 * \sa getSize2D()
	 * \sa getSize3D()
	 * \sa data()
	 */
	[[nodiscard]] constexpr size_t size() const noexcept {
		return contents.size();
	}

	/**
	 * Read a specific pixel of the image.
	 *
	 * \tparam Pixel pixel type to read. Must be a default-initializable,
	 *         trivially copyable type that is binary-compatible with the
	 *         current pixel format of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a copy of the specified pixel of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	template <typename Pixel>
	[[nodiscard]] constexpr Pixel readPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific pixel of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a 1x1 read-only view over the specified pixel of the image, with
	 *         an image type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	[[nodiscard]] constexpr ImageView getPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific layer of a specific mip level of the image.
	 *
	 * \param layer index of the layer to get. Must be a valid layer index that
	 *        is less than getDepth().
	 * \param mipLevel index of the mip level to get the layer from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a read-only view over the specified layer of the image, with an
	 *         image type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageView getLayer(uint32_t layer, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over all layers in a specific mip level of the image.
	 *
	 * \param mipLevel index of the mip level to get. Must be a valid mip level
	 *        index that is less than getMipLevelCount().
	 *
	 * \return a read-only view over all layers in the specified mip level of
	 *         the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageView getMipLevel(uint32_t mipLevel) const;

	/**
	 * Get a view over all layers in a specific range of mip levels of the
	 * image.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 * \param mipLevels number of mip levels to get. Must be less than or equal
	 *        to getMipLevelCount() - firstMipLevel.
	 *
	 * \return a read-only view over all layers in the specified mip levels of
	 *         the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageView getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const;

	/**
	 * Get a view over all layers of all mip levels of the image at or above a
	 * specific mip level.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 *
	 * \return a read-only view over all layers in the mip levels at or above
	 *         the specified mip level.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageView getMipLevels(uint32_t firstMipLevel) const;

	/**
	 * Get a padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param paddingLeft padding to add to the left of the image, in pixels.
	 * \param paddingRight padding to add to the right of the image, in pixels.
	 * \param paddingTop padding to add above the image, in pixels.
	 * \param paddingBottom padding to add below the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] GREM_API(resource) Image getPadded(uint32_t paddingLeft, uint32_t paddingRight, uint32_t paddingTop, uint32_t paddingBottom) const;

	/**
	 * Get a uniformly padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param padding padding to add on each side of the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] Image getPadded(uint32_t padding) const;

private:
	Span<const byte> contents{};
	ImageType type = ImageType::EMPTY;
	ImageFormat format = ImageFormat::UNKNOWN;
	Extent3D size3D{.width = 0, .height = 0, .depth = 0};
	uint32_t mipLevelCount = 0;
};

/**
 * Non-owning view over a 2D image.
 *
 * \sa ImageView
 * \sa Image
 */
class ImageReference {
public:
	/**
	 * Construct a view that does not reference an image.
	 */
	constexpr ImageReference() noexcept = default;

	/**
	 * Construct an image view over arbitrary 2D pixel data.
	 *
	 * \param type image type.
	 * \param format image format.
	 * \param size3D size of the image, in pixels and layers. If type is
	 *        ImageType::IMAGE_CUBE or ImageType::IMAGE_CUBE_ARRAY, width must
	 *        be equal to height. If type is ImageType::IMAGE_2D, depth must be
	 *        1. If type is ImageType::IMAGE_CUBE, depth must be 6. If type is
	 *        ImageType::IMAGE_CUBE_ARRAY, depth must be a multiple of 6.
	 * \param mipLevelCount number of mip levels stored for the image. Must be
	 *        less than or equal to the result of Image::getMaxMipLevelCount()
	 *        for the given 2D size.
	 * \param contents view over the image data, or an empty span to create a
	 *        view that doesn't reference an image. Must have a valid size for
	 *        the given type and shape of the image, including all layers and
	 *        mip levels.
	 */
	constexpr ImageReference(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Span<byte> contents)
		: contents(contents)
		, type(type)
		, format(format)
		, size3D(size3D)
		, mipLevelCount(mipLevelCount) {
		GREM_ASSERT(type != ImageType::IMAGE_2D || size3D.depth == 1);
		GREM_ASSERT((type != ImageType::IMAGE_CUBE && type != ImageType::IMAGE_CUBE_ARRAY) || size3D.width == size3D.height);
		GREM_ASSERT(type != ImageType::IMAGE_CUBE || size3D.depth == 6);
		GREM_ASSERT(type != ImageType::IMAGE_CUBE_ARRAY || size3D.depth % 6 == 0);
		GREM_ASSERT(mipLevelCount <= static_cast<uint32_t>(getRequiredBitCount(max(size3D.width, size3D.height))));
	}

	/**
	 * Create a read-only view from this view.
	 *
	 * \return a non-owning read-only view over the image.
	 */
	constexpr operator ImageView() const noexcept {
		return ImageView{getType(), getFormat(), getSize3D(), getMipLevelCount(), getContents()};
	}

	/**
	 * Transcode the image data to a specific format.
	 *
	 * \param output non-owning pointer to a block of memory to write the
	 *        transcoded image data to. Must point to a valid block of memory of
	 *        at least the size required for the full image in the given output
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param outputFormat image format to transcode to.
	 *
	 * \throws std::invalid_argument if transcoding failed, or if transcoding
	 *         from the image's current format to the given format is not
	 *         supported.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The following transcodings are supported:
	 *       - Copying any non-containerized format to the same format
	 *       - KTX2 ETC1S/UASTC -> ASTC 4x4 LDR/BC1-5/Unsigned BC6H/BC7/ETC1/ETC2/EAC/PVRTC1
	 *       - KTX2 ETC1S/UASTC -> Raw format
	 *       - Raw 32-bit float format -> Raw 16-bit float format
	 *       - Raw 16-bit float format -> Raw 32-bit float format
	 */
	void transcodeTo(byte* output, ImageFormat outputFormat) const {
		ImageView{*this}.transcodeTo(output, outputFormat);
	}

	/**
	 * Transform the RGBA image pixel colors from straight to pre-multiplied
	 * alpha.
	 *
	 * \param transferFunction transfer function of the pixels stored in the
	 *        image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw color format
	 *         with an alpha channel.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) void transformFromStraightToPremultipliedAlpha(Color::TransferFunction transferFunction);

	/**
	 * Transform the RGBA image pixel colors from pre-multiplied to straight
	 * alpha.
	 *
	 * \param transferFunction transfer function of the pixels stored in the
	 *        image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw color format
	 *         with an alpha channel.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) void transformFromPremultipliedToStraightAlpha(Color::TransferFunction transferFunction);

	/**
	 * Get the type of the image referenced by this view.
	 *
	 * \return the image type.
	 */
	[[nodiscard]] constexpr ImageType getType() const noexcept {
		return type;
	}

	/**
	 * Get the format of the image referenced by this view.
	 *
	 * \return the image format.
	 */
	[[nodiscard]] constexpr ImageFormat getFormat() const noexcept {
		return format;
	}

	/**
	 * Get the size, in pixels, of the image layers referenced by this view.
	 *
	 * \return the size of a layer of the image, in pixels.
	 *
	 * \note For 2D array images, this function returns the width and height of
	 *       a single image layer in the array.
	 * \note For cube images, this function returns the size of a single side of
	 *       the cubemap.
	 *
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getHeight()
	 */
	[[nodiscard]] constexpr Extent2D getSize2D() const noexcept {
		return Extent2D{.width = size3D.width, .height = size3D.height};
	}

	/**
	 * Get the size, in pixels, and the depth, in layers, of the image
	 * referenced by this view.
	 *
	 * \return the size of the image, in pixels and layers.
	 *
	 * \sa getSize2D()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 */
	[[nodiscard]] constexpr Extent3D getSize3D() const noexcept {
		return size3D;
	}

	/**
	 * Get the width of the image referenced by this view.
	 *
	 * \return the width of the image, in pixels.
	 */
	[[nodiscard]] constexpr uint32_t getWidth() const noexcept {
		return size3D.width;
	}

	/**
	 * Get the height of the image referenced by this view.
	 *
	 * \return the height of the image, in pixels.
	 */
	[[nodiscard]] constexpr uint32_t getHeight() const noexcept {
		return size3D.height;
	}

	/**
	 * Get the depth of the image referenced by this view.
	 *
	 * \return the depth of the image, in layers.
	 */
	[[nodiscard]] constexpr uint32_t getDepth() const noexcept {
		return size3D.depth;
	}

	/**
	 * Get the number of mip levels in the image referenced by this view.
	 *
	 * \return the number of mip levels in the image.
	 */
	[[nodiscard]] constexpr uint32_t getMipLevelCount() const noexcept {
		return mipLevelCount;
	}

	/**
	 * Get the image data referenced by this view.
	 *
	 * \return a view over the image data.
	 *
	 * \sa getType()
	 * \sa getFormat()
	 * \sa getSize3D()
	 * \sa getMipLevelCount()
	 * \sa data()
	 * \sa size()
	 */
	[[nodiscard]] constexpr Span<byte> getContents() const noexcept {
		return contents;
	}

	/**
	 * Get a pointer to the image data referenced by this view.
	 *
	 * \return a non-owning pointer to the image data.
	 *
	 * \sa size()
	 */
	[[nodiscard]] constexpr byte* data() const noexcept {
		return contents.data();
	}

	/**
	 * Get the size of the image data.
	 *
	 * \return the size of the image data, in bytes.
	 *
	 * \sa getSize2D()
	 * \sa getSize3D()
	 * \sa data()
	 */
	[[nodiscard]] constexpr size_t size() const noexcept {
		return contents.size();
	}

	/**
	 * Read a specific pixel of the image.
	 *
	 * \tparam Pixel pixel type to read. Must be a default-initializable,
	 *         trivially copyable type that is binary-compatible with the
	 *         current pixel format of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a copy of the specified pixel of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	template <typename Pixel>
	[[nodiscard]] constexpr Pixel readPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific pixel of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a 1x1 view over the specified pixel of the image, with an image
	 *         type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	[[nodiscard]] constexpr ImageReference getPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific layer of a specific mip level of the image.
	 *
	 * \param layer index of the layer to get. Must be a valid layer index that
	 *        is less than getDepth().
	 * \param mipLevel index of the mip level to get the layer from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a view over the specified layer of the image, with an image type
	 *         of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageReference getLayer(uint32_t layer, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over all layers in a specific mip level of the image.
	 *
	 * \param mipLevel index of the mip level to get. Must be a valid mip level
	 *        index that is less than getMipLevelCount().
	 *
	 * \return a view over all layers in the specified mip level of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageReference getMipLevel(uint32_t mipLevel) const;

	/**
	 * Get a view over all layers in a specific range of mip levels of the
	 * image.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 * \param mipLevels number of mip levels to get. Must be less than or equal
	 *        to getMipLevelCount() - firstMipLevel.
	 *
	 * \return a view over all layers in the specified mip levels of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageReference getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const;

	/**
	 * Get a view over all layers of all mip levels of the image at or above a
	 * specific mip level.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 *
	 * \return a view over all layers in the mip levels at or above the
	 *         specified mip level.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] constexpr ImageReference getMipLevels(uint32_t firstMipLevel) const;

	/**
	 * Get a padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param paddingLeft padding to add to the left of the image, in pixels.
	 * \param paddingRight padding to add to the right of the image, in pixels.
	 * \param paddingTop padding to add above the image, in pixels.
	 * \param paddingBottom padding to add below the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] Image getPadded(uint32_t paddingLeft, uint32_t paddingRight, uint32_t paddingTop, uint32_t paddingBottom) const;

	/**
	 * Get a uniformly padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param padding padding to add on each side of the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] Image getPadded(uint32_t padding) const;

private:
	Span<byte> contents{};
	ImageType type = ImageType::EMPTY;
	ImageFormat format = ImageFormat::UNKNOWN;
	Extent3D size3D{.width = 0, .height = 0, .depth = 0};
	uint32_t mipLevelCount = 0;
};

/**
 * Options for saving an image in PNG format.
 */
struct ImageSavePNGOptions {
	enum class FilterType : uint8_t {
		NONE = 0,
		SUB = 1,
		UP = 2,
		AVERAGE = 3,
		PAETH = 4,
	};

	/**
	 * Subresource of the image to save.
	 */
	ImageSubresource subresource{
		.layer = 0,
		.mipLevel = 0,
	};

	/**
	 * Deflate compression level.
	 *
	 * Higher values yield smaller compressed sizes at the cost of potentially
	 * slower compression/decompression speed and higher memory usage during
	 * compression.
	 *
	 * \note PNG compression is lossless, so the image will retain the same
	 *       visual quality regardless of compression level.
	 */
	size_t compressionLevel = 8;

	/**
	 * Specific PNG filter type to use instead of choosing automatically for
	 * each scanline.
	 */
	Optional<FilterType> filterTypeOverride{};
};

/**
 * Options for saving an image in JPEG format.
 */
struct ImageSaveJPEGOptions {
	/**
	 * Subresource of the image to save.
	 */
	ImageSubresource subresource{
		.layer = 0,
		.mipLevel = 0,
	};

	/**
	 * JPEG quality.
	 *
	 * Higher values yield better image quality but results in a larger file
	 * size. The compression is lossy.
	 */
	int quality = 90;
};

/**
 * Options for saving an image in RGBE/Radiance HDR format.
 */
struct ImageSaveHDROptions {
	/**
	 * Subresource of the image to save.
	 */
	ImageSubresource subresource{
		.layer = 0,
		.mipLevel = 0,
	};
};

/**
 * Options for saving an image in Khronos TeXture, version 2.0 format.
 */
struct ImageSaveKTX2Options {};

/**
 * Options for saving an image in any format.
 */
struct ImageSaveOptions {};

/**
 * Options for loading an image.
 */
struct ImageOptions {
	/**
	 * If set, require the loaded image to have this type.
	 */
	Optional<ImageType> requiredType{};

	/**
	 * If set, require the loaded image to have or be converted to this format.
	 */
	Optional<ImageFormat> requiredFormat{};

	/**
	 * Maximum width, height and depth of the loaded image.
	 */
	Extent3D maxImageDimensions{
		.width = 16384,
		.height = 16384,
		.depth = 2048,
	};

	/**
	 * Maximum size in bytes of the loaded image data.
	 */
	size_t maxImageSizeInBytes = (uint64_t{8'589'934'592ull} <= uint64_t{Limits<size_t>::MAX}) ? static_cast<size_t>(uint64_t{8'589'934'592ull}) : size_t{2'147'483'648ull};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ImageOptions& other) const = default;
};

/**
 * Container for a 2D image.
 *
 * \sa ImageView
 */
class Image {
public:
	/**
	 * Get the maximum number of mip levels for a specific 2D image size.
	 *
	 * \param size2D size of the 2D image at mip level 0.
	 *
	 * \return the maximum number of mip levels for the given image size.
	 */
	[[nodiscard]] static constexpr uint32_t getMaxMipLevelCount(Extent2D size2D) noexcept {
		return static_cast<uint32_t>(getRequiredBitCount(max(size2D.width, size2D.height)));
	}

	/**
	 * Get the number of logical pixel component channels in a specific image
	 * format (which doesn't necessarily correspond to the physical pixel size
	 * of the format).
	 *
	 * \param format image format to get the number of channels in.
	 *
	 * \return the number of logical component channels in the given format.
	 */
	[[nodiscard]] static constexpr size_t getLogicalChannelCount(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return 0;
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: return 1;
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: return 2;
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: return 3;
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return 4;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is an RGBA color format with an alpha
	 * channel.
	 *
	 * \param format image format to check.
	 *
	 * \return true if the image format is an RGBA color format with an alpha
	 *         channel, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isRGBAColorFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: return false;
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is a raw format.
	 *
	 * \param format image format to check.
	 *
	 * \return true if the image format is raw, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isRawFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return false;
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is a bit-packed format.
	 *
	 * \param format image format to check.
	 *
	 * \return true if the image format is packed, false otherwise.
	 *
	 * \note Compressed formats without any further bit-packing do not count as
	 *       packed formats.
	 */
	[[nodiscard]] static constexpr bool isPackedFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return false;
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is a (potentially containerized)
	 * compressed format.
	 *
	 * \param format image format to check.
	 *
	 * \return true if the image format is compressed, false otherwise.
	 *
	 * \note Bit-packed formats without any further compression do not count as
	 *       compressed formats.
	 */
	[[nodiscard]] static constexpr bool isCompressedFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: return false;
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Check if a specific image format is a containerized format.
	 *
	 * \param format image format to check.
	 *
	 * \return true if the image format is containerized, false otherwise.
	 */
	[[nodiscard]] static constexpr bool isContainerizedFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: [[fallthrough]];
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return false;
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return true;
		}
		unreachable();
	}

	/**
	 * Get the raw format corresponding to a specific image format.
	 *
	 * \param format image format to get the raw image format of.
	 *
	 * \return the corresponding raw image format.
	 */
	[[nodiscard]] static constexpr ImageFormat getRawFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return ImageFormat::UNKNOWN;
			case ImageFormat::R8_UINT: return ImageFormat::R8_UINT;
			case ImageFormat::R16_FLOAT: return ImageFormat::R16_FLOAT;
			case ImageFormat::R32_FLOAT: return ImageFormat::R32_FLOAT;
			case ImageFormat::R8G8_UINT: return ImageFormat::R8G8_UINT;
			case ImageFormat::R16G16_FLOAT: return ImageFormat::R16G16_FLOAT;
			case ImageFormat::R32G32_FLOAT: return ImageFormat::R32G32_FLOAT;
			case ImageFormat::R8G8B8_UINT: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::R16G16B16_FLOAT: return ImageFormat::R16G16B16_FLOAT;
			case ImageFormat::R32G32B32_FLOAT: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::R8G8B8A8_UINT: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::R16G16B16A16_FLOAT: return ImageFormat::R16G16B16A16_FLOAT;
			case ImageFormat::R32G32B32A32_FLOAT: return ImageFormat::R32G32B32A32_FLOAT;
			case ImageFormat::R5G6B5_UINT_PACK16: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::A1R5G5B5_UINT_PACK16: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::B10G11R11_UFLOAT_PACK32: return ImageFormat::R16G16B16_FLOAT;
			case ImageFormat::A2B10G10R10_UINT_PACK32: return ImageFormat::R32G32B32A32_FLOAT;
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::BC1_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::BC3_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::BC4_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::BC5_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::BC7_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::EAC_R11_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::EAC_R11G11_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
		}
		unreachable();
	}

	/**
	 * Get the uncompressed format corresponding to a specific image format.
	 *
	 * \param format image format to get the uncompressed image format of.
	 *
	 * \return the corresponding uncompressed image format.
	 *
	 * \note The corresponding uncompressed format may still be packed. To get
	 *       the raw format, use getRawFormat() instead.
	 */
	[[nodiscard]] static constexpr ImageFormat getUncompressedFormat(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return ImageFormat::UNKNOWN;
			case ImageFormat::R8_UINT: return ImageFormat::R8_UINT;
			case ImageFormat::R16_FLOAT: return ImageFormat::R16_FLOAT;
			case ImageFormat::R32_FLOAT: return ImageFormat::R32_FLOAT;
			case ImageFormat::R8G8_UINT: return ImageFormat::R8G8_UINT;
			case ImageFormat::R16G16_FLOAT: return ImageFormat::R16G16_FLOAT;
			case ImageFormat::R32G32_FLOAT: return ImageFormat::R32G32_FLOAT;
			case ImageFormat::R8G8B8_UINT: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::R16G16B16_FLOAT: return ImageFormat::R16G16B16_FLOAT;
			case ImageFormat::R32G32B32_FLOAT: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::R8G8B8A8_UINT: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::R16G16B16A16_FLOAT: return ImageFormat::R16G16B16A16_FLOAT;
			case ImageFormat::R32G32B32A32_FLOAT: return ImageFormat::R32G32B32A32_FLOAT;
			case ImageFormat::R5G6B5_UINT_PACK16: return ImageFormat::R5G6B5_UINT_PACK16;
			case ImageFormat::A1R5G5B5_UINT_PACK16: return ImageFormat::A1R5G5B5_UINT_PACK16;
			case ImageFormat::B10G11R11_UFLOAT_PACK32: return ImageFormat::B10G11R11_UFLOAT_PACK32;
			case ImageFormat::A2B10G10R10_UINT_PACK32: return ImageFormat::A2B10G10R10_UINT_PACK32;
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::BC1_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::BC3_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::BC4_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::BC5_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: return ImageFormat::R32G32B32_FLOAT;
			case ImageFormat::BC7_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::EAC_R11_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::EAC_R11G11_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: return ImageFormat::R8_UINT;
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: return ImageFormat::R8G8_UINT;
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: return ImageFormat::R8G8B8_UINT;
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return ImageFormat::R8G8B8A8_UINT;
		}
		unreachable();
	}

	/**
	 * Get the stride in bytes of the pixels in a specific uncompressed image
	 * format.
	 *
	 * \param format image format.
	 *
	 * \return the number of bytes to advance to get from one pixel to the next,
	 *         or 0 if the given format is not an uncompressed format.
	 */
	[[nodiscard]] static constexpr size_t getPixelStride(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return 0;
			case ImageFormat::R8_UINT: return 1;
			case ImageFormat::R16_FLOAT: return 2;
			case ImageFormat::R32_FLOAT: return 4;
			case ImageFormat::R8G8_UINT: return 2;
			case ImageFormat::R16G16_FLOAT: return 4;
			case ImageFormat::R32G32_FLOAT: return 8;
			case ImageFormat::R8G8B8_UINT: return 3;
			case ImageFormat::R16G16B16_FLOAT: return 6;
			case ImageFormat::R32G32B32_FLOAT: return 12;
			case ImageFormat::R8G8B8A8_UINT: return 4;
			case ImageFormat::R16G16B16A16_FLOAT: return 8;
			case ImageFormat::R32G32B32A32_FLOAT: return 16;
			case ImageFormat::R5G6B5_UINT_PACK16: return 2;
			case ImageFormat::A1R5G5B5_UINT_PACK16: return 2;
			case ImageFormat::B10G11R11_UFLOAT_PACK32: return 4;
			case ImageFormat::A2B10G10R10_UINT_PACK32: return 4;
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return 0;
		}
		unreachable();
	}

	/**
	 * Get the raw uncompressed size of the 2D blocks in a specific
	 * non-containerized image format, in pixels.
	 *
	 * \param format image format.
	 *
	 * \return the raw uncompressed size of each block in the given format, in
	 *         pixels, or (0, 0) if the given format is unknown or
	 *         containerized.
	 *
	 * \note For uncompressed formats, this function always returns (1, 1),
	 *       since each pixel is its own block.
	 */
	[[nodiscard]] static constexpr Extent2D getBlockSize2D(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return Extent2D{};
			case ImageFormat::R8_UINT: [[fallthrough]];
			case ImageFormat::R16_FLOAT: [[fallthrough]];
			case ImageFormat::R32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8_UINT: [[fallthrough]];
			case ImageFormat::R16G16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
			case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
			case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
			case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
			case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
			case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case ImageFormat::A2B10G10R10_UINT_PACK32: return Extent2D{1, 1};
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
			case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return Extent2D{4, 4};
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return Extent2D{};
		}
		unreachable();
	}

	/**
	 * Get the stride in bytes of the blocks in a specific non-containerized
	 * image format.
	 *
	 * \param format image format.
	 *
	 * \return the number of bytes to advance to get from one block to the next,
	 *         or 0 if the given format is unknown or containerized.
	 *
	 * \note For uncompressed formats, this function is equivalent to
	 *       getPixelStride().
	 */
	[[nodiscard]] static constexpr size_t getBlockStride(ImageFormat format) {
		switch (format) {
			case ImageFormat::UNKNOWN: return 0;
			case ImageFormat::R8_UINT: return 1;
			case ImageFormat::R16_FLOAT: return 2;
			case ImageFormat::R32_FLOAT: return 4;
			case ImageFormat::R8G8_UINT: return 2;
			case ImageFormat::R16G16_FLOAT: return 4;
			case ImageFormat::R32G32_FLOAT: return 8;
			case ImageFormat::R8G8B8_UINT: return 3;
			case ImageFormat::R16G16B16_FLOAT: return 6;
			case ImageFormat::R32G32B32_FLOAT: return 12;
			case ImageFormat::R8G8B8A8_UINT: return 4;
			case ImageFormat::R16G16B16A16_FLOAT: return 8;
			case ImageFormat::R32G32B32A32_FLOAT: return 16;
			case ImageFormat::R5G6B5_UINT_PACK16: return 2;
			case ImageFormat::A1R5G5B5_UINT_PACK16: return 2;
			case ImageFormat::B10G11R11_UFLOAT_PACK32: return 4;
			case ImageFormat::A2B10G10R10_UINT_PACK32: return 4;
			case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: return 16;
			case ImageFormat::BC1_RGB_UINT_BLOCK: return 8;
			case ImageFormat::BC3_RGBA_UINT_BLOCK: return 16;
			case ImageFormat::BC4_R_UINT_BLOCK: return 8;
			case ImageFormat::BC5_RG_UINT_BLOCK: return 16;
			case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: return 16;
			case ImageFormat::BC6H_RGB_FLOAT_BLOCK: return 16;
			case ImageFormat::BC7_RGBA_UINT_BLOCK: return 16;
			case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: return 8;
			case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: return 16;
			case ImageFormat::EAC_R11_UINT_BLOCK: return 8;
			case ImageFormat::EAC_R11G11_UINT_BLOCK: return 16;
			case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: return 8;
			case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return 0;
		}
		unreachable();
	}

	/**
	 * Get the 3D size, in pixels, of all layers in a specific mip level of an
	 * image.
	 *
	 * \param size3D size of the full image, in pixels.
	 * \param mipLevel index of the mip level to get the size of. Must be less
	 *        than the result of Image::getMaxMipLevelCount() for the given 2D
	 *        size.
	 *
	 * \return the 3D size, in pixels, of the full mip level, including all
	 *         layers.
	 */
	[[nodiscard]] static constexpr Extent3D getMipLevelSize3D(Extent3D size3D, uint32_t mipLevel) {
		GREM_ASSERT(mipLevel < getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));
		return Extent3D{
			max(size3D.width >> mipLevel, uint32_t{1}),
			max(size3D.height >> mipLevel, uint32_t{1}),
			size3D.depth,
		};
	}

	/**
	 * Get the 2D size, in pixels, of a single layer in a specific mip level of
	 * an image.
	 *
	 * \param size2D size of the full image, in pixels.
	 * \param mipLevel index of the mip level to get the size of. Must be less
	 *        than the result of Image::getMaxMipLevelCount() for the given 2D
	 *        size.
	 *
	 * \return the 2D size, in pixels, of a single layer at the given mip level.
	 */
	[[nodiscard]] static constexpr Extent2D getMipLevelSize2D(Extent2D size2D, uint32_t mipLevel) {
		GREM_ASSERT(mipLevel < getMaxMipLevelCount(size2D));
		return Extent2D{
			max(size2D.width >> mipLevel, uint32_t{1}),
			max(size2D.height >> mipLevel, uint32_t{1}),
		};
	}

	/**
	 * Get the stride in bytes of the rows in a specific uncompressed image
	 * format.
	 *
	 * \param format image format.
	 * \param width width of the image, in pixels.
	 *
	 * \return the number of bytes to advance to get from one row to the next,
	 *         or 0 if the given format is not an uncompressed format.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] static constexpr size_t getRowStride(ImageFormat format, uint32_t width) {
		if (width == 0) {
			return 0;
		}
		const size_t pixelStride = getPixelStride(format);
		if (pixelStride > Limits<size_t>::MAX / static_cast<size_t>(width)) {
			throw std::length_error{"Image size overflow."};
		}
		return pixelStride * static_cast<size_t>(width);
	}

	/**
	 * Get the stride in bytes of the layers in a specific non-containerized
	 * image format.
	 *
	 * \param format image format.
	 * \param size2D size of the full image layer, in pixels.
	 *
	 * \return the number of bytes to advance to get from one layer to the next,
	 *         or 0 if the given format is unknown or containerized.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] static constexpr size_t getLayerStride(ImageFormat format, Extent2D size2D) {
		const Extent2D blockSize = getBlockSize2D(format);
		if (blockSize.width == 0 || blockSize.height == 0) {
			return 0;
		}
		const size_t blockCountX = static_cast<size_t>((size2D.width + blockSize.width - 1) / blockSize.width);
		const size_t blockCountY = static_cast<size_t>((size2D.height + blockSize.height - 1) / blockSize.height);
		const size_t blockStride = getBlockStride(format);
		if (blockStride > Limits<size_t>::MAX / blockCountX) {
			throw std::length_error{"Image size overflow."};
		}
		const size_t rowStride = blockStride * blockCountX;
		if (rowStride > Limits<size_t>::MAX / blockCountY) {
			throw std::length_error{"Image size overflow."};
		}
		return rowStride * blockCountY;
	}

	/**
	 * Get the size in bytes of a single-mip-level image in a specific
	 * non-containerized image format.
	 *
	 * \param format image format.
	 * \param size3D size of the full image mip level, in pixels.
	 *
	 * \return the total size of the full image mip level, including all layers,
	 *         or 0 if the given format is unknown or containerized.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] static constexpr size_t getMipLevelStride(ImageFormat format, Extent3D size3D) {
		if (size3D.depth == 0) {
			return 0;
		}
		const size_t layerStride = getLayerStride(format, Extent2D{.width = size3D.width, .height = size3D.height});
		if (Limits<size_t>::MAX / static_cast<size_t>(size3D.depth) < layerStride) {
			throw std::length_error{"Image size overflow."};
		}
		return layerStride * static_cast<size_t>(size3D.depth);
	}

	/**
	 * Get the size in bytes of an image in a specific non-containerized image
	 * format.
	 *
	 * \param format image format.
	 * \param size3D size of the full image, in pixels.
	 * \param mipLevelCount number of mip levels in the image.
	 *
	 * \return the total size of the full image, including all layers and mip
	 *         levels, or 0 if the given format is unknown or containerized.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] static constexpr size_t getSizeInBytes(ImageFormat format, Extent3D size3D, uint32_t mipLevelCount) {
		size_t result = 0;
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const size_t mipLevelStride = getMipLevelStride(format, getMipLevelSize3D(size3D, mipLevel));
			if (result > Limits<size_t>::MAX - mipLevelStride) {
				throw std::length_error{"Image size overflow."};
			}
			result += mipLevelStride;
		}
		return result;
	}

	/**
	 * Convert a single pixel component of a raw image to another pixel
	 * component type.
	 *
	 * \tparam OutputComponent component type to convert to. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam InputComponent component type to convert from. Must be a valid
	 *         raw pixel component type (u8norm, float16_t or float32_t).
	 *
	 * \param value component value to convert.
	 *
	 * \return the converted component value.
	 */
	template <typename OutputComponent, typename InputComponent>
	[[nodiscard]] static OutputComponent convertPixelComponent(const InputComponent& value) noexcept {
		if constexpr (same_as<OutputComponent, InputComponent>) {
			return value;
		} else {
			return static_cast<OutputComponent>(static_cast<float>(value));
		}
	}

	/**
	 * Convert a single pixel of a raw image to another raw pixel format.
	 *
	 * \tparam OutputComponent component type to convert to. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam OutputComponentCount number of component channels in the output
	 *         image. Must be in the range [1, 4] (inclusive).
	 * \tparam InputComponent component type to convert from. Must be a valid
	 *         raw pixel component type (u8norm, float16_t or float32_t).
	 * \tparam InputComponentCount number of component channels in the input
	 *         image. Must be in the range [1, 4] (inclusive).
	 *
	 * \param output non-owning pointer to a block of memory containing the
	 *        output image. Must point to a valid block of memory of at least
	 *        the size required for a pixel in the specified output pixel
	 *        format, and must not be nullptr.
	 * \param input non-owning read-only pointer to a block of memory containing
	 *        the input image. Must point to a valid block of memory of at least
	 *        the size required for a pixel in the specified input pixel format,
	 *        and must not be nullptr.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	template <typename OutputComponent, size_t OutputComponentCount, typename InputComponent, size_t InputComponentCount>
	static void convertPixel(byte* output, const byte* input) {
		static_assert(OutputComponentCount >= 1 && OutputComponentCount <= 4);
		static_assert(InputComponentCount >= 1 && InputComponentCount <= 4);

		GREM_ASSERT(output);
		GREM_ASSERT(input);

		constexpr size_t COMMON_COMPONENT_COUNT = min(InputComponentCount, OutputComponentCount);
		for (size_t i = 0; i < COMMON_COMPONENT_COUNT; ++i) {
			InputComponent value{};
			memcpy(&value, input, sizeof(InputComponent));
			input += sizeof(InputComponent);
			const OutputComponent convertedValue = convertPixelComponent<OutputComponent>(value);
			memcpy(output, &convertedValue, sizeof(OutputComponent));
			output += sizeof(OutputComponent);
		}
		if constexpr (OutputComponentCount >= 2 && InputComponentCount < 2) {
			memset(output, 0, sizeof(OutputComponent));
			output += sizeof(OutputComponent);
		}
		if constexpr (OutputComponentCount >= 3 && InputComponentCount < 3) {
			memset(output, 0, sizeof(OutputComponent));
			output += sizeof(OutputComponent);
		}
		if constexpr (OutputComponentCount >= 4 && InputComponentCount < 4) {
			if constexpr (same_as<OutputComponent, float16_t> || same_as<OutputComponent, float32_t>) {
				constexpr OutputComponent ONE = 1.0f;
				memcpy(output, &ONE, sizeof(OutputComponent));
			} else {
				constexpr OutputComponent MAX = Limits<OutputComponent>::MAX;
				memcpy(output, &MAX, sizeof(OutputComponent));
			}
			output += sizeof(OutputComponent);
		}
	}

	/**
	 * Convert the pixels in a region of a raw image to another raw pixel
	 * format.
	 *
	 * \tparam OutputComponent component type to convert to. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam OutputComponentCount number of component channels in the output
	 *         image. Must be in the range [1, 4] (inclusive).
	 * \tparam InputComponent component type to convert from. Must be a valid
	 *         raw pixel component type (u8norm, float16_t or float32_t).
	 * \tparam InputComponentCount number of component channels in the input
	 *         image. Must be in the range [1, 4] (inclusive).
	 *
	 * \param outputSize size of the full mip level in the output image, in
	 *        pixels.
	 * \param output non-owning pointer to a block of memory containing a full
	 *        mip level of the output image. Must point to a valid block of
	 *        memory of at least the size required for the full specified mip
	 *        level in the specified output pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param outputOffset offset, in pixels, from the top left corner of the
	 *        first layer of the output image to start pasting at, where the
	 *        top left corner of the first layer of the converted image region
	 *        will begin.
	 * \param inputSize size of the full mip level in the input image, in
	 *        pixels.
	 * \param input non-owning read-only pointer to a block of memory containing
	 *        a full mip level of the input image. Must point to a valid block
	 *        of memory of at least the size required for the full specified mip
	 *        level in the specified input pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param inputRegion region of the input mip level to convert from,
	 *        relative to the top left corner of its first layer.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	template <typename OutputComponent, size_t OutputComponentCount, typename InputComponent, size_t InputComponentCount>
	static void convertPixels(Extent3D outputSize, byte* output, Offset3D outputOffset, Extent3D inputSize, const byte* input, Region3D inputRegion) {
		GREM_ASSERT(output);
		GREM_ASSERT(outputOffset.x >= 0 && static_cast<uint32_t>(outputOffset.x) <= outputSize.width);
		GREM_ASSERT(outputOffset.y >= 0 && static_cast<uint32_t>(outputOffset.y) <= outputSize.height);
		GREM_ASSERT(outputOffset.z >= 0 && static_cast<uint32_t>(outputOffset.z) <= outputSize.depth);
		GREM_ASSERT(inputRegion.size.width <= outputSize.width - static_cast<uint32_t>(outputOffset.x));
		GREM_ASSERT(inputRegion.size.height <= outputSize.height - static_cast<uint32_t>(outputOffset.y));
		GREM_ASSERT(inputRegion.size.depth <= outputSize.depth - static_cast<uint32_t>(outputOffset.z));
		GREM_ASSERT(input);
		GREM_ASSERT(inputRegion.offset.x >= 0 && static_cast<uint32_t>(inputRegion.offset.x) <= inputSize.width);
		GREM_ASSERT(inputRegion.offset.y >= 0 && static_cast<uint32_t>(inputRegion.offset.y) <= inputSize.height);
		GREM_ASSERT(inputRegion.offset.z >= 0 && static_cast<uint32_t>(inputRegion.offset.z) <= inputSize.depth);
		GREM_ASSERT(inputRegion.size.width <= inputSize.width - static_cast<uint32_t>(inputRegion.offset.x));
		GREM_ASSERT(inputRegion.size.height <= inputSize.height - static_cast<uint32_t>(inputRegion.offset.y));
		GREM_ASSERT(inputRegion.size.depth <= inputSize.depth - static_cast<uint32_t>(inputRegion.offset.z));

		const size_t outOffsetX = static_cast<size_t>(outputOffset.x);
		const size_t inOffsetX = static_cast<size_t>(inputRegion.offset.x);
		const size_t outOffsetY = static_cast<size_t>(outputOffset.y);
		const size_t inOffsetY = static_cast<size_t>(inputRegion.offset.y);
		const size_t outOffsetZ = static_cast<size_t>(outputOffset.z);
		const size_t inOffsetZ = static_cast<size_t>(inputRegion.offset.z);
		const size_t countX = static_cast<size_t>(inputRegion.size.width);
		const size_t countY = static_cast<size_t>(inputRegion.size.height);
		const size_t countZ = static_cast<size_t>(inputRegion.size.depth);
		const size_t outStrideX = 1;
		const size_t inStrideX = 1;
		const size_t outStrideY = static_cast<size_t>(outputSize.width);
		const size_t inStrideY = static_cast<size_t>(inputSize.width);
		const size_t outStrideZ = static_cast<size_t>(outputSize.width) * static_cast<size_t>(outputSize.height);
		const size_t inStrideZ = static_cast<size_t>(inputSize.width) * static_cast<size_t>(inputSize.height);
		const size_t outPixelStride = OutputComponentCount * sizeof(OutputComponent);
		const size_t inPixelStride = InputComponentCount * sizeof(InputComponent);

		for (size_t z = 0; z < countZ; ++z) {
			const size_t outZ = outOffsetZ + z;
			const size_t inZ = inOffsetZ + z;
			for (size_t y = 0; y < countY; ++y) {
				const size_t outY = outOffsetY + y;
				const size_t inY = inOffsetY + y;
				for (size_t x = 0; x < countX; ++x) {
					const size_t outX = outOffsetX + x;
					const size_t inX = inOffsetX + x;
					const size_t outIndex = outZ * outStrideZ + outY * outStrideY + outX * outStrideX;
					const size_t inIndex = inZ * inStrideZ + inY * inStrideY + inX * inStrideX;
					byte* const outputPixel = output + outIndex * outPixelStride;
					const byte* const inputPixel = input + inIndex * inPixelStride;
					convertPixel<OutputComponent, OutputComponentCount, InputComponent, InputComponentCount>(outputPixel, inputPixel);
				}
			}
		}
	}

	/**
	 * Convert all pixels of a full raw image to another raw pixel format.
	 *
	 * \tparam OutputComponent component type to convert to. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam OutputComponentCount number of component channels in the output
	 *         image. Must be in the range [1, 4] (inclusive).
	 * \tparam InputComponent component type to convert from. Must be a valid
	 *         raw pixel component type (u8norm, float16_t or float32_t).
	 * \tparam InputComponentCount number of component channels in the input
	 *         image. Must be in the range [1, 4] (inclusive).
	 *
	 * \param size3D size of the full input and output images, in pixels and
	 *        layers.
	 * \param mipLevelCount number of mip levels in the input and output images.
	 * \param output non-owning pointer to a block of memory containing the
	 *        output image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified output pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param input non-owning read-only pointer to a block of memory containing
	 *        the input image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified input pixel
	 *        format, including all layers, and mip levels, and must not be
	 *        nullptr.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	template <typename OutputComponent, size_t OutputComponentCount, typename InputComponent, size_t InputComponentCount>
	static void convertPixels(Extent3D size3D, uint32_t mipLevelCount, byte* output, const byte* input) {
		GREM_ASSERT(output);
		GREM_ASSERT(input);
		GREM_ASSERT(mipLevelCount <= getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));

		const size_t outPixelStride = OutputComponentCount * sizeof(OutputComponent);
		const size_t inPixelStride = InputComponentCount * sizeof(InputComponent);

		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = getMipLevelSize3D(size3D, mipLevel);
			convertPixels<OutputComponent, OutputComponentCount, InputComponent, InputComponentCount>(mipLevelSize, output, Offset3D{}, mipLevelSize, input,
				Region3D{.size = mipLevelSize});
			const size_t mipLevelPixelCount = static_cast<size_t>(mipLevelSize.width) * static_cast<size_t>(mipLevelSize.height) * static_cast<size_t>(mipLevelSize.depth);
			output += outPixelStride * mipLevelPixelCount;
			input += inPixelStride * mipLevelPixelCount;
		}
	}

	/**
	 * Copy the pixels in a region of a raw image to another image region in the
	 * same format.
	 *
	 * \param outputSize size of the full mip level in the output image, in
	 *        pixels.
	 * \param output non-owning pointer to a block of memory containing a full
	 *        mip level of the output image. Must point to a valid block of
	 *        memory of at least the size required for the full specified mip
	 *        level in the specified output pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param outputOffset offset, in pixels, from the top left corner of the
	 *        first layer of the output image to start pasting at, where the
	 *        top left corner of the first layer of the copied image region will
	 *        begin.
	 * \param inputSize size of the full mip level in the input image, in
	 *        pixels.
	 * \param input non-owning read-only pointer to a block of memory containing
	 *        a full mip level of the input image. Must point to a valid block
	 *        of memory of at least the size required for the full specified mip
	 *        level in the specified input pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param inputRegion region of the input mip level to copy from, relative
	 *        to the top left corner of its first layer.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	static void copyPixels(Extent3D outputSize, byte* output, Offset3D outputOffset, Extent3D inputSize, const byte* input, Region3D inputRegion, size_t pixelStride) {
		GREM_ASSERT(output);
		GREM_ASSERT(outputOffset.x >= 0 && static_cast<uint32_t>(outputOffset.x) <= outputSize.width);
		GREM_ASSERT(outputOffset.y >= 0 && static_cast<uint32_t>(outputOffset.y) <= outputSize.height);
		GREM_ASSERT(outputOffset.z >= 0 && static_cast<uint32_t>(outputOffset.z) <= outputSize.depth);
		GREM_ASSERT(inputRegion.size.width <= outputSize.width - static_cast<uint32_t>(outputOffset.x));
		GREM_ASSERT(inputRegion.size.height <= outputSize.height - static_cast<uint32_t>(outputOffset.y));
		GREM_ASSERT(inputRegion.size.depth <= outputSize.depth - static_cast<uint32_t>(outputOffset.z));
		GREM_ASSERT(input);
		GREM_ASSERT(inputRegion.offset.x >= 0 && static_cast<uint32_t>(inputRegion.offset.x) <= inputSize.width);
		GREM_ASSERT(inputRegion.offset.y >= 0 && static_cast<uint32_t>(inputRegion.offset.y) <= inputSize.height);
		GREM_ASSERT(inputRegion.offset.z >= 0 && static_cast<uint32_t>(inputRegion.offset.z) <= inputSize.depth);
		GREM_ASSERT(inputRegion.size.width <= inputSize.width - static_cast<uint32_t>(inputRegion.offset.x));
		GREM_ASSERT(inputRegion.size.height <= inputSize.height - static_cast<uint32_t>(inputRegion.offset.y));
		GREM_ASSERT(inputRegion.size.depth <= inputSize.depth - static_cast<uint32_t>(inputRegion.offset.z));
		GREM_ASSERT(pixelStride > 0);

		const size_t outOffsetX = static_cast<size_t>(outputOffset.x);
		const size_t inOffsetX = static_cast<size_t>(inputRegion.offset.x);
		const size_t outOffsetY = static_cast<size_t>(outputOffset.y);
		const size_t inOffsetY = static_cast<size_t>(inputRegion.offset.y);
		const size_t outOffsetZ = static_cast<size_t>(outputOffset.z);
		const size_t inOffsetZ = static_cast<size_t>(inputRegion.offset.z);
		const size_t countX = static_cast<size_t>(inputRegion.size.width);
		const size_t countY = static_cast<size_t>(inputRegion.size.height);
		const size_t countZ = static_cast<size_t>(inputRegion.size.depth);
		const size_t outStrideX = 1;
		const size_t inStrideX = 1;
		const size_t outStrideY = static_cast<size_t>(outputSize.width);
		const size_t inStrideY = static_cast<size_t>(inputSize.width);
		const size_t outStrideZ = static_cast<size_t>(outputSize.width) * static_cast<size_t>(outputSize.height);
		const size_t inStrideZ = static_cast<size_t>(inputSize.width) * static_cast<size_t>(inputSize.height);

		for (size_t z = 0; z < countZ; ++z) {
			const size_t outZ = outOffsetZ + z;
			const size_t inZ = inOffsetZ + z;
			for (size_t y = 0; y < countY; ++y) {
				const size_t outY = outOffsetY + y;
				const size_t inY = inOffsetY + y;
				for (size_t x = 0; x < countX; ++x) {
					const size_t outX = outOffsetX + x;
					const size_t inX = inOffsetX + x;
					const size_t outIndex = outZ * outStrideZ + outY * outStrideY + outX * outStrideX;
					const size_t inIndex = inZ * inStrideZ + inY * inStrideY + inX * inStrideX;
					byte* const outputPixel = output + outIndex * pixelStride;
					const byte* const inputPixel = input + inIndex * pixelStride;
					memcpy(outputPixel, inputPixel, pixelStride);
				}
			}
		}
	}

	/**
	 * Copy all pixels of a full raw image to another image in the same format.
	 *
	 * \param size3D size of the full input and output images, in pixels and
	 *        layers.
	 * \param mipLevelCount number of mip levels in the input and output images.
	 * \param output non-owning pointer to a block of memory containing the
	 *        output image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified output pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param input non-owning read-only pointer to a block of memory containing
	 *        the input image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified input pixel
	 *        format, including all layers, and mip levels, and must not be
	 *        nullptr.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	static void copyPixels(Extent3D size3D, uint32_t mipLevelCount, byte* output, const byte* input, size_t pixelStride) {
		GREM_ASSERT(output);
		GREM_ASSERT(input);
		GREM_ASSERT(pixelStride > 0);
		GREM_ASSERT(mipLevelCount <= getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));

		size_t sizeInBytes = 0;
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = getMipLevelSize3D(size3D, mipLevel);
			const size_t mipLevelPixelCount = static_cast<size_t>(mipLevelSize.width) * static_cast<size_t>(mipLevelSize.height) * static_cast<size_t>(mipLevelSize.depth);
			sizeInBytes += pixelStride * mipLevelPixelCount;
		}
		memcpy(output, input, sizeInBytes);
	}

	/**
	 * Transform the pixels in a region of a raw image by a given operation.
	 *
	 * \param size size of the full mip level in the image, in pixels.
	 * \param pixels non-owning pointer to a block of memory containing a full
	 *        mip level of the image to transform. Must point to a valid block
	 *        of memory of at least the size required for the full specified mip
	 *        level in the specified pixel format, including all layers, and
	 *        must not be nullptr.
	 * \param region region, in pixels, relative to the top left corner of the
	 *        first layer of the image, to transform.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 * \param op transformation to execute for each pixel in the specified
	 *        region. Should return void and accept a byte pointer to the
	 *        pixel's memory.
	 */
	static void transformPixels(Extent3D size, byte* pixels, Region3D region, size_t pixelStride, auto op) {
		GREM_ASSERT(pixels);
		GREM_ASSERT(pixelStride > 0);

		const size_t offsetX = static_cast<size_t>(region.offset.x);
		const size_t offsetY = static_cast<size_t>(region.offset.y);
		const size_t offsetZ = static_cast<size_t>(region.offset.z);
		const size_t countX = static_cast<size_t>(region.size.width);
		const size_t countY = static_cast<size_t>(region.size.height);
		const size_t countZ = static_cast<size_t>(region.size.depth);
		const size_t strideX = 1;
		const size_t strideY = static_cast<size_t>(size.width);
		const size_t strideZ = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);

		for (size_t z = 0; z < countZ; ++z) {
			const size_t outZ = offsetZ + z;
			for (size_t y = 0; y < countY; ++y) {
				const size_t outY = offsetY + y;
				for (size_t x = 0; x < countX; ++x) {
					const size_t outX = offsetX + x;
					const size_t index = outZ * strideZ + outY * strideY + outX * strideX;
					byte* const pixel = pixels + index * pixelStride;
					op(pixel);
				}
			}
		}
	}

	/**
	 * Transform all pixels of a full raw image by a given operation.
	 *
	 * \param size3D size of the full image, in pixels and layers.
	 * \param mipLevelCount number of mip levels in the image.
	 * \param pixels non-owning pointer to a block of memory containing the
	 *        image to transform. Must point to a valid block of memory of at
	 *        least the size required for the full image in the specified pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 * \param op transformation to execute for each pixel. Should return void
	 *        and accept a byte pointer to the pixel's memory.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	static void transformPixels(Extent3D size3D, uint32_t mipLevelCount, byte* pixels, size_t pixelStride, auto op) {
		GREM_ASSERT(pixels);
		GREM_ASSERT(pixelStride > 0);
		GREM_ASSERT(mipLevelCount <= getMaxMipLevelCount(Extent2D{size3D.width, size3D.height}));

		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = getMipLevelSize3D(size3D, mipLevel);
			const size_t mipLevelPixelCount = static_cast<size_t>(mipLevelSize.width) * static_cast<size_t>(mipLevelSize.height) * static_cast<size_t>(mipLevelSize.depth);
			transformPixels(mipLevelSize, pixels, Region3D{.size = mipLevelSize}, pixelStride, op);
			pixels += pixelStride * mipLevelPixelCount;
		}
	}

	/**
	 * Set the pixels in a region of a raw image to a specific pixel value.
	 *
	 * \param outputSize size of the full mip level in the output image, in
	 *        pixels.
	 * \param output non-owning pointer to a block of memory containing a full
	 *        mip level of the output image. Must point to a valid block of
	 *        memory of at least the size required for the full specified mip
	 *        level in the specified output pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param outputRegion region, in pixels, relative to the top left corner of
	 *        the first layer of the output image, to fill.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 * \param inputPixel non-owning read-only pointer to a block of memory
	 *        containing the input pixel. Must point to a valid block of memory
	 *        of at least the size required for the stride of one pixel in the
	 *        specified input pixel format, and must not be nullptr.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	static void fillPixels(Extent3D outputSize, byte* output, Region3D outputRegion, size_t pixelStride, const byte* inputPixel) {
		transformPixels(outputSize, output, outputRegion, pixelStride, [&](byte* pixel) -> void { memcpy(pixel, inputPixel, pixelStride); });
	}

	/**
	 * Set all pixels of a full raw image to a specific pixel value.
	 *
	 * \param size3D size of the full output image, in pixels and layers.
	 * \param mipLevelCount number of mip levels in the output image.
	 * \param output non-owning pointer to a block of memory containing the
	 *        output image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified output pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param pixelStride size of a single pixel, in bytes. Must be positive.
	 * \param inputPixel non-owning read-only pointer to a block of memory
	 *        containing the input pixel. Must point to a valid block of memory
	 *        of at least the size required for the stride of one pixel in the
	 *        specified input pixel format, and must not be nullptr.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	static void fillPixels(Extent3D size3D, uint32_t mipLevelCount, byte* output, size_t pixelStride, const byte* inputPixel) {
		transformPixels(size3D, mipLevelCount, output, pixelStride, [&](byte* pixel) -> void { memcpy(pixel, inputPixel, pixelStride); });
	}

	/**
	 * Set a component of the pixels in a region of a raw image to a specific
	 * value.
	 *
	 * \tparam OutputComponent component type of the image. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam OutputComponentCount number of component channels in the image.
	 *         Must be in the range [1, 4] (inclusive).
	 * \tparam ComponentIndex index of the component to set the value of. Must
	 *         be less than OutputComponentCount.
	 *
	 * \param outputSize size of the full mip level in the output image, in
	 *        pixels.
	 * \param output non-owning pointer to a block of memory containing a full
	 *        mip level of the output image. Must point to a valid block of
	 *        memory of at least the size required for the full specified mip
	 *        level in the specified output pixel format, including all layers,
	 *        and must not be nullptr.
	 * \param outputRegion region, in pixels, relative to the top left corner of
	 *        the first layer of the output image, to fill.
	 * \param value value to set the given component of each pixel to.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	template <typename OutputComponent, size_t OutputComponentCount, size_t ComponentIndex>
	static void setPixelsComponent(Extent3D outputSize, byte* output, Region3D outputRegion, const OutputComponent& value) {
		static_assert(same_as<OutputComponent, u8norm> || same_as<OutputComponent, float16_t> || same_as<OutputComponent, float32_t>);
		static_assert(OutputComponentCount >= 1 && OutputComponentCount <= 4);
		static_assert(ComponentIndex < OutputComponentCount);

		transformPixels(outputSize, output, outputRegion, sizeof(OutputComponent) * OutputComponentCount,
			[&](byte* pixel) -> void { memcpy(pixel + sizeof(OutputComponent) * ComponentIndex, &value, sizeof(OutputComponent)); });
	}

	/**
	 * Set a component of all pixels of a full raw image to a specific value.
	 *
	 * \tparam OutputComponent component type of the image. Must be a valid raw
	 *         pixel component type (u8norm, float16_t or float32_t).
	 * \tparam OutputComponentCount number of component channels in the image.
	 *         Must be in the range [1, 4] (inclusive).
	 * \tparam ComponentIndex index of the component to set the value of. Must
	 *         be less than OutputComponentCount.
	 *
	 * \param size3D size of the full output image, in pixels and layers.
	 * \param mipLevelCount number of mip levels in the output image.
	 * \param output non-owning pointer to a block of memory containing the
	 *        output image. Must point to a valid block of memory of at least
	 *        the size required for the full image in the specified output pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 * \param value value to set the given component of each pixel to.
	 *
	 * \warning The referenced input and output memory regions must not overlap.
	 */
	template <typename OutputComponent, size_t OutputComponentCount, size_t ComponentIndex>
	static void setPixelsComponent(Extent3D size3D, uint32_t mipLevelCount, byte* output, const OutputComponent& value) {
		static_assert(same_as<OutputComponent, u8norm> || same_as<OutputComponent, float16_t> || same_as<OutputComponent, float32_t>);
		static_assert(OutputComponentCount >= 1 && OutputComponentCount <= 4);
		static_assert(ComponentIndex < OutputComponentCount);

		transformPixels(size3D, mipLevelCount, output, sizeof(OutputComponent) * OutputComponentCount,
			[&](byte* pixel) -> void { memcpy(pixel + sizeof(OutputComponent) * ComponentIndex, &value, sizeof(OutputComponent)); });
	}

	/**
	 * Transform all pixels of a full raw RGBA image from straight to
	 * pre-multiplied alpha.
	 *
	 * \tparam PixelComponent component type of the pixels in the image. Must be
	 *         a valid raw pixel component type (u8norm, float16_t or
	 *         float32_t).
	 * \tparam TransferFunction transfer function of the pixels in the image.
	 *
	 * \param size3D size of the full image, in pixels and layers.
	 * \param mipLevelCount number of mip levels in the image.
	 * \param pixels non-owning pointer to a block of memory containing the
	 *        image to transform. Must point to a valid block of memory of at
	 *        least the size required for the full image in the specified pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 */
	template <typename PixelComponent, Color::TransferFunction TransferFunction>
	static void transformRGBAPixelsFromStraightToPremultipliedAlpha(Extent3D size3D, uint32_t mipLevelCount, byte* pixels) {
		static_assert(same_as<PixelComponent, u8norm> || same_as<PixelComponent, float16_t> || same_as<PixelComponent, float32_t>);

		transformPixels(size3D, mipLevelCount, pixels, sizeof(PixelComponent) * 4, [&](byte* pixel) -> void {
			vec<4, PixelComponent> pixelValue{};
			memcpy(&pixelValue, pixel, sizeof(pixelValue));
			vec4 linearPixel{};
			if constexpr (TransferFunction == Color::TransferFunction::SRGB) {
				linearPixel = Color::convertSRGBToLinear(vec4{pixelValue});
			} else {
				static_assert(TransferFunction == Color::TransferFunction::LINEAR);
				linearPixel = vec4{pixelValue};
			}
			linearPixel = Color::convertStraightToPremultipliedAlpha(linearPixel);
			if constexpr (TransferFunction == Color::TransferFunction::SRGB) {
				pixelValue = vec<4, PixelComponent>{Color::convertLinearToSRGB(linearPixel)};
			} else {
				static_assert(TransferFunction == Color::TransferFunction::LINEAR);
				pixelValue = vec<4, PixelComponent>{linearPixel};
			}
			memcpy(pixel, &pixelValue, sizeof(pixelValue));
		});
	}

	/**
	 * Transform all pixels of a full raw RGBA image from pre-multiplied to
	 * straight alpha.
	 *
	 * \tparam PixelComponent component type of the pixels in the image. Must be
	 *         a valid raw pixel component type (u8norm, float16_t or
	 *         float32_t).
	 * \tparam TransferFunction transfer function of the pixels in the image.
	 *
	 * \param size3D size of the full image, in pixels and layers.
	 * \param mipLevelCount number of mip levels in the image.
	 * \param pixels non-owning pointer to a block of memory containing the
	 *        image to transform. Must point to a valid block of memory of at
	 *        least the size required for the full image in the specified pixel
	 *        format, including all layers and mip levels, and must not be
	 *        nullptr.
	 */
	template <typename PixelComponent, Color::TransferFunction TransferFunction>
	static void transformRGBAPixelsFromPremultipliedToStraightAlpha(Extent3D size3D, uint32_t mipLevelCount, byte* pixels) {
		static_assert(same_as<PixelComponent, u8norm> || same_as<PixelComponent, float16_t> || same_as<PixelComponent, float32_t>);

		transformPixels(size3D, mipLevelCount, pixels, sizeof(PixelComponent) * 4, [&](byte* pixel) -> void {
			vec<4, PixelComponent> pixelValue{};
			memcpy(&pixelValue, pixel, sizeof(pixelValue));
			vec4 linearPixel{};
			if constexpr (TransferFunction == Color::TransferFunction::SRGB) {
				linearPixel = Color::convertSRGBToLinear(vec4{pixelValue});
			} else {
				static_assert(TransferFunction == Color::TransferFunction::LINEAR);
				linearPixel = vec4{pixelValue};
			}
			linearPixel = Color::convertPremultipliedToStraightAlpha(linearPixel);
			if constexpr (TransferFunction == Color::TransferFunction::SRGB) {
				pixelValue = vec<4, PixelComponent>{Color::convertLinearToSRGB(linearPixel)};
			} else {
				static_assert(TransferFunction == Color::TransferFunction::LINEAR);
				pixelValue = vec<4, PixelComponent>{linearPixel};
			}
			memcpy(pixel, &pixelValue, sizeof(pixelValue));
		});
	}

	/**
	 * Flip a layer of a raw image on the Y axis.
	 *
	 * \param size2D size of the full image layer, in pixels.
	 * \param pixels non-owning pointer to a block of memory containing the raw
	 *        image to flip. Must point to a valid block of memory of at least
	 *        the size required for the full image layer in the specified pixel
	 *        format, and must not be nullptr.
	 * \param pixelStride size of a single pixel in the image layer, in bytes.
	 *        Must be positive.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	static void flipVertically(Extent2D size2D, byte* pixels, size_t pixelStride) {
		GREM_ASSERT(pixels);
		GREM_ASSERT(pixelStride > 0);

		if (Limits<size_t>::MAX / static_cast<size_t>(size2D.width) < pixelStride) {
			throw std::length_error{"Image size overflow."};
		}
		const size_t rowStride = pixelStride * static_cast<size_t>(size2D.width);
		if (rowStride > 0) {
			Allocation<byte> temporaryRow(rowStride);
			const size_t height = static_cast<size_t>(size2D.height);
			const size_t halfHeight = height / 2;
			for (size_t y = 0; y < halfHeight; ++y) {
				byte* const rowA = pixels + y * rowStride;
				byte* const rowB = pixels + (height - y - 1) * rowStride;
				memcpy(temporaryRow.data(), rowA, rowStride);
				memcpy(rowA, rowB, rowStride);
				memcpy(rowB, temporaryRow.data(), rowStride);
			}
		}
	}

	/**
	 * Determine which image file type a file should be parsed as based on the
	 * contents at the beginning of the file.
	 *
	 * \param fileContents file contents to be parsed.
	 *
	 * \return the determined image file type, or ImageFileType::UNKNOWN if a
	 *         type could not be determined.
	 */
	[[nodiscard]] GREM_API(resource) static ImageFileType determineFileType(Span<const byte> fileContents);

	/**
	 * Save an 8-bit-per-channel image to a PNG file.
	 *
	 * \param image view over the image to save.
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the image.
	 * \param options saving options, see ImageSavePNGOptions.
	 *
	 * \note This function will fail if the image is not in a raw 8-bit unsigned
	 *       integer format.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws resource::Error on failure to write a valid image to the file.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 */
	GREM_API(resource) static void savePNG(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSavePNGOptions& options = {});

	/**
	 * Save an 8-bit-per-channel image to an in-memory PNG file.
	 *
	 * \param image view over the image to save.
	 * \param options saving options, see ImageSavePNGOptions.
	 *
	 * \return a contiguous container of the PNG file contents.
	 *
	 * \note This function will fail if the image is not in a raw 8-bit unsigned
	 *       integer format.
	 *
	 * \throws resource::Error on failure to write a valid image.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(resource) static Allocation<byte> savePNG(const ImageView& image, const ImageSavePNGOptions& options = {});

	/**
	 * Save an 8-bit-per-channel image to a JPEG file.
	 *
	 * \param image view over the image to save.
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the image.
	 * \param options saving options, see ImageSaveJPEGOptions.
	 *
	 * \note This function will fail if the image is not in a raw 8-bit unsigned
	 *       integer format.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws resource::Error on failure to write a valid image to the file.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 */
	GREM_API(resource) static void saveJPEG(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveJPEGOptions& options = {});

	/**
	 * Save an 8-bit-per-channel image to an in-memory JPEG file.
	 *
	 * \param image view over the image to save.
	 * \param options saving options, see ImageSaveJPEGOptions.
	 *
	 * \return a contiguous container of the JPEG file contents.
	 *
	 * \note This function will fail if the image is not in a raw 8-bit unsigned
	 *       integer format.
	 *
	 * \throws resource::Error on failure to write a valid image.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(resource) static Allocation<byte> saveJPEG(const ImageView& image, const ImageSaveJPEGOptions& options = {});

	/**
	 * Save a floating-point 32-bit-per-channel image to an RGBE/Radiance HDR
	 * file.
	 *
	 * \param image view over the image to save.
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the image.
	 * \param options saving options, see ImageSaveHDROptions.
	 *
	 * \note This function will fail if the image is not in a raw 32-bit
	 *       floating-point format.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws resource::Error on failure to write a valid image to the file.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 */
	GREM_API(resource) static void saveHDR(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveHDROptions& options = {});

	/**
	 * Save a floating-point 32-bit-per-channel image to an in-memory
	 * RGBE/Radiance HDR file.
	 *
	 * \param image view over the image to save.
	 * \param options saving options, see ImageSaveHDROptions.
	 *
	 * \return a contiguous container of the HDR file contents.
	 *
	 * \note This function will fail if the image is not in a raw 32-bit
	 *       floating-point format.
	 *
	 * \throws resource::Error on failure to write a valid image.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(resource) static Allocation<byte> saveHDR(const ImageView& image, const ImageSaveHDROptions& options = {});

	/**
	 * Save an image to a Khronos TeXture, version 2.0 file.
	 *
	 * \param image view over the image to save.
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the image.
	 * \param options saving options, see ImageSaveKTX2Options.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws resource::Error on failure to write a valid image to the file.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 */
	GREM_API(resource) static void saveKTX2(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveKTX2Options& options = {});

	/**
	 * Save an image to an in-memory Khronos Texture, version 2.0 file.
	 *
	 * \param image view over the image to save.
	 * \param options saving options, see ImageSaveKTX2Options.
	 *
	 * \return a contiguous container of the KTX2 file contents.
	 *
	 * \throws resource::Error on failure to write a valid image.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(resource) static Allocation<byte> saveKTX2(const ImageView& image, const ImageSaveKTX2Options& options = {});

	/**
	 * Save an image to a file.
	 *
	 * If the given filepath has a file extension, then the image will be saved
	 * in the file format corresponding to the extension, or throw an error if
	 * the image cannot be saved in that format.
	 *
	 * Otherwise, the file format will be chosen automatically based on the type
	 * and format of the image, and the corresponding file extension will be
	 * added to the filepath.
	 *
	 * The supported file formats are:
	 * - JPEG (.jpg/.jpeg)
	 * - PNG (.png)
	 * - RGBE/Radiance HDR (.hdr)
	 * - Khronos TeXture, version 2.0 (.ktx2)
	 *
	 * \param image view over the image to save.
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the image.
	 * \param options saving options, see ImageSaveOptions.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws resource::Error on failure to write a valid image to the file.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 */
	GREM_API(resource) static void save(const ImageView& image, Filesystem& filesystem, CStringView filepath, const ImageSaveOptions& options = {});

	/**
	 * Construct an empty image without a value.
	 */
	Image() noexcept = default;

	/**
	 * Construct an image.
	 *
	 * \param type image type.
	 * \param format image format.
	 * \param size3D size of the image to allocate, in pixels and layers. If
	 *        type is ImageType::IMAGE_CUBE or ImageType::IMAGE_CUBE_ARRAY,
	 *        width must be equal to height. If type is ImageType::IMAGE_2D,
	 *        depth must be 1. If type is ImageType::IMAGE_CUBE, depth must be
	 *        6. If type is ImageType::IMAGE_CUBE_ARRAY, depth must be a
	 *        multiple of 6.
	 * \param mipLevelCount number of mip levels stored for the image. Must be
	 *        less than or equal to the result of Image::getMaxMipLevelCount()
	 *        for the given 2D size.
	 * \param contents read-only view over the image data to copy, or an empty
	 *        span to leave the data uninitialized. Must either be empty or have
	 *        a valid size for the given shape of the image.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) Image(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Span<const byte> contents = {});

	/**
	 * Construct an image.
	 *
	 * \param type image type.
	 * \param format image format.
	 * \param size3D size of the image to allocate, in pixels and layers. If
	 *        type is ImageType::IMAGE_CUBE or ImageType::IMAGE_CUBE_ARRAY,
	 *        width must be equal to height. If type is ImageType::IMAGE_2D,
	 *        depth must be 1. If type is ImageType::IMAGE_CUBE, depth must be
	 *        6. If type is ImageType::IMAGE_CUBE_ARRAY, depth must be a
	 *        multiple of 6.
	 * \param mipLevelCount number of mip levels stored for the image. Must be
	 *        less than or equal to the result of Image::getMaxMipLevelCount()
	 *        for the given 2D size.
	 * \param contents array containing the image data, or an empty array to
	 *        allocate new data and leave it uninitialized. Must either be empty
	 *        or have a valid size for the given shape of the image.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) Image(ImageType type, ImageFormat format, Extent3D size3D, uint32_t mipLevelCount, Allocation<byte> contents);

	/**
	 * Construct an image copied from an image view.
	 *
	 * \param image read-only view over the image to copy.
	 *
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(resource) explicit Image(const ImageView& image);

	/**
	 * Load an image from a file.
	 *
	 * The supported file formats are:
	 * - JPEG (.jpg/.jpeg)
	 * - PNG (.png)
	 * - RGBE/Radiance HDR (.hdr)
	 * - Khronos TeXture, version 2.0 (.ktx2)
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the image file to load.
	 * \param options image options, see ImageOptions.
	 *
	 * \throws File::Error on failure to open or read from the file.
	 * \throws resource::Error on failure to load a valid image from the file.
	 * \throws std::length_error if the maximum image or file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 * \note The image format is determined by its source format, unless
	 *       ImageOptions::requiredFormat is specified.
	 * \note For JPEG files, 12 bits per component and arithmetic coding are not
	 *       supported.
	 * \note PNG files support 1, 2, 4, 8 and 16 bits per channel.
	 * \note For BMP files, 1 bit per component and run-length encoding are not
	 *       supported.
	 * \note PSD files support 8 and 16 bits per pixel.
	 * \note For PSD files, only composited view is supported, with no extra
	 *       channels.
	 * \note For GIF files, animation is not supported, and the reported number
	 *       of channels is always 4.
	 * \note For PPM and PGM files, only binary format is supported.
	 */
	GREM_API(resource) Image(const Filesystem& filesystem, CStringView filepath, const ImageOptions& options = {});

	/**
	 * Load an image from an in-memory file.
	 *
	 * The supported file formats are:
	 * - JPEG (.jpg/.jpeg)
	 * - PNG (.png)
	 * - RGBE/Radiance HDR (.hdr)
	 * - Khronos TeXture, version 2.0 (.ktx2)
	 *
	 * \param fileContents contents of the image file to load.
	 * \param options image options, see ImageOptions.
	 *
	 * \throws resource::Error on failure to load a valid image from the file.
	 * \throws std::length_error if the maximum image or file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents.
	 * \note The image format is determined by its source format, unless
	 *       ImageOptions::requiredFormat is specified.
	 * \note For JPEG files, 12 bits per component and arithmetic coding are not
	 *       supported.
	 * \note PNG files support 1, 2, 4, 8 and 16 bits per channel.
	 * \note For BMP files, 1 bit per component and run-length encoding are not
	 *       supported.
	 * \note PSD files support 8 and 16 bits per pixel.
	 * \note For PSD files, only composited view is supported, with no extra
	 *       channels.
	 * \note For GIF files, animation is not supported, and the reported number
	 *       of channels is always 4.
	 * \note For PPM and PGM files, only binary format is supported.
	 */
	GREM_API(resource) explicit Image(Span<const byte> fileContents, const ImageOptions& options = {});

	/**
	 * Get a read-only view over this image.
	 *
	 * \return if the image has a value, returns a non-owning read-only view
	 *         over it. Otherwise, returns a view that doesn't reference an
	 *         image.
	 */
	operator ImageView() const noexcept {
		return ImageView{getType(), getFormat(), getSize3D(), getMipLevelCount(), getContents()};
	}

	/**
	 * Get a view over this image.
	 *
	 * \return if the image has a value, returns a non-owning view over it.
	 *         Otherwise, returns a view that doesn't reference an image.
	 */
	operator ImageReference() noexcept {
		return ImageReference{getType(), getFormat(), getSize3D(), getMipLevelCount(), getContents()};
	}

	/**
	 * Remove the value from this image and reset it to an empty image.
	 */
	void reset() noexcept {
		*this = Image{};
	}

	/**
	 * Transcode the image data to a specific format.
	 *
	 * \param output non-owning pointer to a block of memory to write the
	 *        transcoded image data to. Must point to a valid block of memory of
	 *        at least the size required for the full image in the given output
	 *        format, including all layers, and must not be nullptr.
	 * \param outputFormat image format to transcode to.
	 *
	 * \throws std::invalid_argument if transcoding failed, or if transcoding
	 *         from the image's current format to the given format is not
	 *         supported.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The following transcodings are supported:
	 *       - Copying any non-containerized format to the same format
	 *       - KTX2 ETC1S/UASTC -> ASTC 4x4 LDR/BC1-5/Unsigned BC6H/BC7/ETC1/ETC2/EAC/PVRTC1
	 *       - KTX2 ETC1S/UASTC -> Raw format
	 *       - Raw 32-bit float format -> Raw 16-bit float format
	 *       - Raw 16-bit float format -> Raw 32-bit float format
	 */
	void transcodeTo(byte* output, ImageFormat outputFormat) const {
		ImageView{*this}.transcodeTo(output, outputFormat);
	}

	/**
	 * Transform the RGBA image pixel colors from straight to pre-multiplied
	 * alpha.
	 *
	 * \param transferFunction transfer function of the pixels stored in the
	 *        image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw color format
	 *         with an alpha channel.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void transformFromStraightToPremultipliedAlpha(Color::TransferFunction transferFunction) {
		ImageReference{*this}.transformFromStraightToPremultipliedAlpha(transferFunction);
	}

	/**
	 * Transform the RGBA image pixel colors from pre-multiplied to straight
	 * alpha.
	 *
	 * \param transferFunction transfer function of the pixels stored in the
	 *        image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw color format
	 *         with an alpha channel.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void transformFromPremultipliedToStraightAlpha(Color::TransferFunction transferFunction) {
		ImageReference{*this}.transformFromPremultipliedToStraightAlpha(transferFunction);
	}

	/**
	 * Get the type of the image.
	 *
	 * \return the image type.
	 */
	[[nodiscard]] ImageType getType() const noexcept {
		return type;
	}

	/**
	 * Get the format of the image.
	 *
	 * \return the image format.
	 */
	[[nodiscard]] ImageFormat getFormat() const noexcept {
		return format;
	}

	/**
	 * Get the size, in pixels, of the image layers.
	 *
	 * \return the size of a layer of the image, in pixels.
	 *
	 * \note For 2D array images, this function returns the width and height of
	 *       a single image layer in the array.
	 * \note For cube images, this function returns the size of a single side of
	 *       the cubemap.
	 *
	 * \sa getSize3D()
	 * \sa getWidth()
	 * \sa getHeight()
	 */
	[[nodiscard]] Extent2D getSize2D() const noexcept {
		return Extent2D{.width = size3D.width, .height = size3D.height};
	}

	/**
	 * Get the size, in pixels, and the depth, in layers, of the image.
	 *
	 * \return the size of the image, in pixels.
	 *
	 * \sa getSize2D()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 */
	[[nodiscard]] Extent3D getSize3D() const noexcept {
		return size3D;
	}

	/**
	 * Get the width of the image.
	 *
	 * \return the width of the image, in pixels.
	 */
	[[nodiscard]] uint32_t getWidth() const noexcept {
		return size3D.width;
	}

	/**
	 * Get the height of the image.
	 *
	 * \return the height of the image, in pixels.
	 */
	[[nodiscard]] uint32_t getHeight() const noexcept {
		return size3D.height;
	}

	/**
	 * Get the depth of the image.
	 *
	 * \return the depth of the image, in layers.
	 */
	[[nodiscard]] uint32_t getDepth() const noexcept {
		return size3D.depth;
	}

	/**
	 * Get the number of mip levels in the image referenced by this view.
	 *
	 * \return the number of mip levels in the image.
	 */
	[[nodiscard]] uint32_t getMipLevelCount() const noexcept {
		return mipLevelCount;
	}

	/**
	 * Get the data of this image.
	 *
	 * \return a view over the image data.
	 *
	 * \sa getType()
	 * \sa getFormat()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 * \sa data()
	 * \sa size()
	 */
	[[nodiscard]] Span<byte> getContents() noexcept {
		return contents;
	}

	/**
	 * Get the data of this image.
	 *
	 * \return a read-only view over the image data.
	 *
	 * \sa getType()
	 * \sa getFormat()
	 * \sa getWidth()
	 * \sa getHeight()
	 * \sa getDepth()
	 * \sa data()
	 * \sa size()
	 */
	[[nodiscard]] Span<const byte> getContents() const noexcept {
		return contents;
	}

	/**
	 * Get a pointer to the data of this image.
	 *
	 * \return a non-owning pointer to the image data.
	 *
	 * \sa size()
	 */
	[[nodiscard]] byte* data() noexcept {
		return contents.data();
	}

	/**
	 * Get a pointer to the data of this image.
	 *
	 * \return a non-owning read-only pointer to the image data.
	 *
	 * \sa size()
	 */
	[[nodiscard]] const byte* data() const noexcept {
		return contents.data();
	}

	/**
	 * Get the size of the image data.
	 *
	 * \return the size of the image data, in bytes.
	 *
	 * \sa getSize2D()
	 * \sa getSize3D()
	 * \sa data()
	 */
	[[nodiscard]] size_t size() const noexcept {
		return contents.size();
	}

	/**
	 * Read a specific pixel of the image.
	 *
	 * \tparam Pixel pixel type to read. Must be a default-initializable,
	 *         trivially copyable type that is binary-compatible with the
	 *         current pixel format of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a copy of the specified pixel of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	template <typename Pixel>
	[[nodiscard]] constexpr Pixel readPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific pixel of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a 1x1 read-only view over the specified pixel of the image, with
	 *         an image type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	[[nodiscard]] ImageView getPixel(Offset3D position, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific pixel of the image.
	 *
	 * \param position coordinates of the pixel to get. Must be a valid position
	 *        within the specified mip level.
	 * \param mipLevel index of the mip level to get the pixel from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a 1x1 view over the specified pixel of the image, with an image
	 *         type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw uncompressed
	 *         format.
	 */
	[[nodiscard]] ImageReference getPixel(Offset3D position, uint32_t mipLevel = 0);

	/**
	 * Get a view over a specific layer of a specific mip level of the image.
	 *
	 * \param layer index of the layer to get. Must be a valid layer index that
	 *        is less than getDepth().
	 * \param mipLevel index of the mip level to get the layer from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a read-only view over the specified layer of the image, with an
	 *         image type of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageView getLayer(uint32_t layer, uint32_t mipLevel = 0) const;

	/**
	 * Get a view over a specific layer of a specific mip level of the image.
	 *
	 * \param layer index of the layer to get. Must be a valid layer index that
	 *        is less than getDepth().
	 * \param mipLevel index of the mip level to get the layer from. Must be a
	 *        valid mip level index that is less than getMipLevelCount().
	 *
	 * \return a view over the specified layer of the image, with an image type
	 *         of ImageType::IMAGE_2D.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageReference getLayer(uint32_t layer, uint32_t mipLevel = 0);

	/**
	 * Get a view over all layers in a specific mip level of the image.
	 *
	 * \param mipLevel index of the mip level to get. Must be a valid mip level
	 *        index that is less than getMipLevelCount().
	 *
	 * \return a read-only view over all layers in the specified mip level of
	 *         the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageView getMipLevel(uint32_t mipLevel) const;

	/**
	 * Get a view over all layers in a specific mip level of the image.
	 *
	 * \param mipLevel index of the mip level to get. Must be a valid mip level
	 *        index that is less than getMipLevelCount().
	 *
	 * \return a view over all layers in the specified mip level of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageReference getMipLevel(uint32_t mipLevel);

	/**
	 * Get a view over all layers in a specific range of mip levels of the
	 * image.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 * \param mipLevels number of mip levels to get. Must be less than or equal
	 *        to getMipLevelCount() - firstMipLevel.
	 *
	 * \return a read-only view over all layers in the specified mip levels of
	 *         the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageView getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const;

	/**
	 * Get a view over all layers in a specific range of mip levels of the
	 * image.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 * \param mipLevels number of mip levels to get. Must be less than or equal
	 *        to getMipLevelCount() - firstMipLevel.
	 *
	 * \return a view over all layers in the specified mip levels of the image.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageReference getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels);

	/**
	 * Get a view over all layers of all mip levels of the image at or above a
	 * specific mip level.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 *
	 * \return a read-only view over all layers in the mip levels at or above
	 *         the specified mip level.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageView getMipLevels(uint32_t firstMipLevel) const;

	/**
	 * Get a view over all layers of all mip levels of the image at or above a
	 * specific mip level.
	 *
	 * \param firstMipLevel index of the first mip level to get. Must be less
	 *        than or equal to getMipLevelCount().
	 *
	 * \return a view over all layers in the mip levels at or above the
	 *         specified mip level.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 */
	[[nodiscard]] ImageReference getMipLevels(uint32_t firstMipLevel);

	/**
	 * Get a uniformly padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param paddingLeft padding to add on the left side of the image, in
	 *        pixels.
	 * \param paddingRight padding to add on the right side of the image, in
	 *        pixels.
	 * \param paddingBottom padding to add on the bottom of the image, in
	 *        pixels.
	 * \param paddingTop padding to add on the top of the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] Image getPadded(uint32_t paddingLeft, uint32_t paddingRight, uint32_t paddingBottom, uint32_t paddingTop) const {
		return ImageView{*this}.getPadded(paddingLeft, paddingRight, paddingBottom, paddingTop);
	}

	/**
	 * Get a uniformly padded version of the image.
	 *
	 * The image must be in a raw format.
	 *
	 * \param padding padding to add on each side of the image, in pixels.
	 *
	 * \return a copy of this image with its edges padded by the given amount,
	 *         using the color of the pixel closest to each point in the
	 *         original image, or zeros if the original image is empty.
	 *
	 * \throws std::invalid_argument if the image is not in a raw format.
	 * \throws std::length_error if the maximum image size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If there are multiple layers, each layer is padded individually.
	 */
	[[nodiscard]] Image getPadded(uint32_t padding) const {
		return ImageView{*this}.getPadded(padding);
	}

private:
	Allocation<byte> contents{};
	ImageType type = ImageType::EMPTY;
	ImageFormat format = ImageFormat::UNKNOWN;
	Extent3D size3D{.width = 0, .height = 0, .depth = 0};
	uint32_t mipLevelCount;
};

template <typename Pixel>
constexpr Pixel ImageView::readPixel(Offset3D position, uint32_t mipLevel) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.readPixel<Pixel>(position, mipLevel);
}

constexpr ImageView ImageView::getPixel(Offset3D position, uint32_t mipLevel) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.getPixel(position, mipLevel);
}

constexpr ImageView ImageView::getLayer(uint32_t layer, uint32_t mipLevel) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.getLayer(layer, mipLevel);
}

constexpr ImageView ImageView::getMipLevel(uint32_t mipLevel) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.getMipLevel(mipLevel);
}

constexpr ImageView ImageView::getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.getMipLevels(firstMipLevel, mipLevels);
}

constexpr ImageView ImageView::getMipLevels(uint32_t firstMipLevel) const {
	return ImageReference{type, format, size3D, mipLevelCount, Span{const_cast<byte*>(contents.data()), contents.size()}}.getMipLevels(firstMipLevel);
}

template <typename Pixel>
constexpr Pixel ImageReference::readPixel(Offset3D position, uint32_t mipLevel) const {
	static_assert(trivially_copyable<Pixel>);
	const ImageView pixel = getPixel(position, mipLevel);
	GREM_ASSERT(pixel.size() == sizeof(Pixel));
	Pixel result;
	memcpy(&result, pixel.data(), sizeof(Pixel));
	return result;
}

constexpr ImageReference ImageReference::getPixel(Offset3D position, uint32_t mipLevel) const {
	GREM_ASSERT(position.x >= 0 && static_cast<uint32_t>(position.x) < getWidth());
	GREM_ASSERT(position.y >= 0 && static_cast<uint32_t>(position.y) < getHeight());
	GREM_ASSERT(position.z >= 0 && static_cast<uint32_t>(position.z) < getDepth());
	const ImageReference layer = getLayer(static_cast<uint32_t>(position.z), mipLevel);
	const size_t pixelStride = Image::getPixelStride(format);
	if (pixelStride == 0) {
		throw std::invalid_argument{"Invalid image format."};
	}
	const size_t pixelIndex = static_cast<size_t>(position.y) * static_cast<size_t>(getWidth()) + static_cast<size_t>(position.x);
	return ImageReference{ImageType::IMAGE_2D, format, Extent2D{1, 1}, 1, layer.getContents().subspan(pixelIndex * pixelStride, pixelStride)};
}

constexpr ImageReference ImageReference::getLayer(uint32_t layer, uint32_t mipLevel) const {
	const ImageReference layers = getMipLevel(mipLevel);
	const size_t layerStride = Image::getLayerStride(format, layers.getSize2D());
	if (layerStride == 0) {
		throw std::invalid_argument{"Invalid image format."};
	}
	return ImageReference{ImageType::IMAGE_2D, format, layers.getSize2D(), 1, layers.getContents().subspan(static_cast<size_t>(layer) * layerStride, layerStride)};
}

constexpr ImageReference ImageReference::getMipLevel(uint32_t mipLevel) const {
	const Extent2D blockSize = Image::getBlockSize2D(format);
	if (blockSize.width == 0 || blockSize.height == 0) {
		throw std::invalid_argument{"Invalid image format."};
	}
	byte* pointer = data();
	for (uint32_t level = 0; level < mipLevel; ++level) {
		const Extent3D mipLevelSize3D = Image::getMipLevelSize3D(size3D, level);
		const size_t mipLevelStride = Image::getMipLevelStride(format, mipLevelSize3D);
		pointer += mipLevelStride;
	}
	const Extent3D mipLevelSize3D = Image::getMipLevelSize3D(size3D, mipLevel);
	const size_t mipLevelStride = Image::getMipLevelStride(format, mipLevelSize3D);
	return ImageReference{type, format, mipLevelSize3D, 1, Span{pointer, mipLevelStride}};
}

constexpr ImageReference ImageReference::getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const {
	GREM_ASSERT(firstMipLevel <= getMipLevelCount());
	GREM_ASSERT(mipLevels <= getMipLevelCount() - firstMipLevel);
	const Extent2D blockSize = Image::getBlockSize2D(format);
	if (blockSize.width == 0 || blockSize.height == 0) {
		throw std::invalid_argument{"Invalid image format."};
	}
	byte* pointer = data();
	for (uint32_t level = 0; level < firstMipLevel; ++level) {
		const Extent3D mipLevelSize3D = Image::getMipLevelSize3D(size3D, level);
		const size_t mipLevelStride = Image::getMipLevelStride(format, mipLevelSize3D);
		pointer += mipLevelStride;
	}
	size_t size = 0;
	const uint32_t mipLevelsEnd = firstMipLevel + mipLevels;
	for (uint32_t level = firstMipLevel; level < mipLevelsEnd; ++level) {
		const Extent3D mipLevelSize3D = Image::getMipLevelSize3D(size3D, level);
		const size_t mipLevelStride = Image::getMipLevelStride(format, mipLevelSize3D);
		size += mipLevelStride;
	}
	return ImageReference{type, format, Image::getMipLevelSize3D(size3D, firstMipLevel), mipLevels, Span{pointer, size}};
}

constexpr ImageReference ImageReference::getMipLevels(uint32_t firstMipLevel) const {
	GREM_ASSERT(firstMipLevel <= getMipLevelCount());
	return getMipLevels(firstMipLevel, getMipLevelCount() - firstMipLevel);
}

inline Image ImageView::getPadded(uint32_t padding) const {
	return getPadded(padding, padding, padding, padding);
}

inline Image ImageReference::getPadded(uint32_t paddingLeft, uint32_t paddingRight, uint32_t paddingTop, uint32_t paddingBottom) const {
	return ImageView{*this}.getPadded(paddingLeft, paddingRight, paddingTop, paddingBottom);
}

inline Image ImageReference::getPadded(uint32_t padding) const {
	return ImageView{*this}.getPadded(padding);
}

template <typename Pixel>
constexpr Pixel Image::readPixel(Offset3D position, uint32_t mipLevel) const {
	return ImageView{*this}.readPixel<Pixel>(position, mipLevel);
}

inline ImageView Image::getPixel(Offset3D position, uint32_t mipLevel) const {
	return ImageView{*this}.getPixel(position, mipLevel);
}

inline ImageReference Image::getPixel(Offset3D position, uint32_t mipLevel) {
	return ImageReference{*this}.getPixel(position, mipLevel);
}

inline ImageView Image::getLayer(uint32_t layer, uint32_t mipLevel) const {
	return ImageView{*this}.getLayer(layer, mipLevel);
}

inline ImageReference Image::getLayer(uint32_t layer, uint32_t mipLevel) {
	return ImageReference{*this}.getLayer(layer, mipLevel);
}

inline ImageView Image::getMipLevel(uint32_t mipLevel) const {
	return ImageView{*this}.getMipLevel(mipLevel);
}

inline ImageReference Image::getMipLevel(uint32_t mipLevel) {
	return ImageReference{*this}.getMipLevel(mipLevel);
}

inline ImageView Image::getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) const {
	return ImageView{*this}.getMipLevels(firstMipLevel, mipLevels);
}

inline ImageReference Image::getMipLevels(uint32_t firstMipLevel, uint32_t mipLevels) {
	return ImageReference{*this}.getMipLevels(firstMipLevel, mipLevels);
}

inline ImageView Image::getMipLevels(uint32_t firstMipLevel) const {
	return ImageView{*this}.getMipLevels(firstMipLevel);
}

inline ImageReference Image::getMipLevels(uint32_t firstMipLevel) {
	return ImageReference{*this}.getMipLevels(firstMipLevel);
}

} // namespace grem::resource

#endif
