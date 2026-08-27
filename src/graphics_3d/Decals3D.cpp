// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics_3d/Decals3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/AtlasPacker.hpp>
#include <GREM/resource/Image.hpp>

#include <utility> // std::move

namespace grem::graphics {

namespace {

[[nodiscard]] vec4 insertSprite(Device& device, resource::AtlasPacker& atlasPacker, Texture& atlasTexture, uint32_t padding, TextureFormat internalFormat,
	const resource::ImageView& image, bool convertToPremultipliedAlpha) {
	if (!Texture::isCompatibleFormat(internalFormat, image.getFormat())) {
		throw graphics::Error{"Failed to insert sprite image: Incompatible image format."};
	}

	const auto [offset, resized] = atlasPacker.insertRectangle(image.getSize2D());
	if (!atlasTexture || resized) {
		Texture oldAtlasTexture = std::move(atlasTexture);
		const Extent2D size{atlasPacker.getResolution()};
		atlasTexture = Texture::create(device, TextureType::TEXTURE_2D, internalFormat, size, resource::Image::getMaxMipLevelCount(size), ClearValues{},
			TextureSamplerOptions{
				.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
				.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			});
		if (oldAtlasTexture) {
			atlasTexture.pasteTexture(oldAtlasTexture);
		}
	}

	resource::Image transformedImage{};
	resource::ImageView transformedImageView = image;
	if (convertToPremultipliedAlpha && resource::Image::isRGBAColorFormat(transformedImageView.getFormat()) && resource::Image::isRawFormat(transformedImageView.getFormat())) {
		transformedImage = resource::Image{transformedImageView};
		transformedImage.transformFromStraightToPremultipliedAlpha(Texture::getTransferFunction(internalFormat));
		transformedImageView = transformedImage;
	}
	if (padding > 0) {
		transformedImage = transformedImageView.getPadded(padding);
		transformedImageView = transformedImage;
	}
	atlasTexture.pasteImage(Extent2D{transformedImageView.getWidth(), transformedImageView.getHeight()}, transformedImageView.data(),
		Offset2D{.x = offset.x - static_cast<int32_t>(padding), .y = offset.y - static_cast<int32_t>(padding)});

	const vec2 position{static_cast<float>(offset.x), static_cast<float>(offset.y)};
	const vec2 size{static_cast<float>(image.getWidth()), static_cast<float>(image.getHeight())};
	return vec4{position, size};
}

} // namespace

Decals3D::Decals3D(Device& device, const Decals3DOptions& options)
	: device(&device)
	, options(options) {}

void Decals3D::clearDecalMaterials() noexcept {
	clearDecals();
	decalMaterials.clear();
	baseColorAtlasPacker = resource::AtlasPacker{{.initialResolution = baseColorAtlasPacker.getResolution(), .padding = PADDING, .alignment = ALIGNMENT}};
	normalAtlasPacker = resource::AtlasPacker{{.initialResolution = normalAtlasPacker.getResolution(), .padding = PADDING, .alignment = ALIGNMENT}};
	occlusionRoughnessMetallicAtlasPacker =
		resource::AtlasPacker{{.initialResolution = occlusionRoughnessMetallicAtlasPacker.getResolution(), .padding = PADDING, .alignment = ALIGNMENT}};
	emissiveAtlasPacker = resource::AtlasPacker{{.initialResolution = emissiveAtlasPacker.getResolution(), .padding = PADDING, .alignment = ALIGNMENT}};
	defaultBaseColorMapPosition.reset();
	defaultNormalMapPosition.reset();
	defaultOcclusionRoughnessMetallicMapPosition.reset();
	defaultEmissiveMapPosition.reset();
}

DecalMaterialID Decals3D::createDecalMaterial(const DecalMaterialOptions& options) {
	const vec4 baseColorMapPositionAndSize =
		(options.baseColorMapImage.getType() == resource::ImageType::EMPTY)
			? vec4{getDefaultDecalBaseColorMapPosition(), vec2{1.0f, 1.0f}}
			: insertSprite(*device, baseColorAtlasPacker, baseColorAtlasTexture, PADDING, TextureFormat::R8G8B8A8_SRGB, options.baseColorMapImage,
				  options.convertToPremultipliedAlpha);
	const vec4 normalMapPositionAndSize =
		(options.normalMapImage.getType() == resource::ImageType::EMPTY)
			? vec4{getDefaultDecalNormalMapPosition(), vec2{1.0f, 1.0f}}
			: insertSprite(*device, normalAtlasPacker, normalAtlasTexture, PADDING, TextureFormat::R8G8B8A8_UNORM, options.normalMapImage, options.convertToPremultipliedAlpha);
	const vec4 occlusionRoughnessMetallicMapPositionAndSize =
		(options.occlusionRoughnessMetallicMapImage.getType() == resource::ImageType::EMPTY)
			? vec4{getDefaultDecalOcclusionRoughnessMetallicMapPosition(), vec2{1.0f, 1.0f}}
			: insertSprite(*device, occlusionRoughnessMetallicAtlasPacker, occlusionRoughnessMetallicAtlasTexture, PADDING, TextureFormat::R8G8B8A8_UNORM,
				  options.occlusionRoughnessMetallicMapImage, options.convertToPremultipliedAlpha);
	const vec4 emissiveMapPositionAndSize =
		(options.emissiveMapImage.getType() == resource::ImageType::EMPTY)
			? vec4{getDefaultDecalEmissiveMapPosition(), vec2{1.0f, 1.0f}}
			: insertSprite(*device, emissiveAtlasPacker, emissiveAtlasTexture, PADDING, TextureFormat::R8G8B8A8_SRGB, options.emissiveMapImage,
				  options.convertToPremultipliedAlpha);
	const uint32_t index = static_cast<uint32_t>(decalMaterials.size());
	decalMaterials.push_back(DecalMaterial{
		.baseColorMapPositionAndSize = baseColorMapPositionAndSize,
		.normalMapPositionAndSize = normalMapPositionAndSize,
		.occlusionRoughnessMetallicMapPositionAndSize = occlusionRoughnessMetallicMapPositionAndSize,
		.emissiveMapPositionAndSize = emissiveMapPositionAndSize,
		.baseColorFactor = options.baseColorFactor,
		.occlusionRoughnessMetallicFactor{options.occlusionStrength, options.roughnessFactor, options.metallicFactor},
		.normalScale = options.normalScale,
		.emissiveFactor = options.emissiveFactor,
	});
	parameterBufferDirty = true;
	return DecalMaterialID{index};
}

void Decals3D::clearDecals() noexcept {
	decals.clear();
}

DecalID Decals3D::createDecal(DecalMaterialID materialID, const DecalOptions3D& options) {
	const DecalID newDecalID{lastDecalIDValue + 1};
	const Decal newDecal{
		.materialID = materialID,
		.position = options.position,
		.range = options.range,
		.orientation = options.orientation,
		.size = options.size,
		.origin = options.origin,
		.color = options.color,
		.emissiveFactor = options.emissiveFactor,
		.modelInstanceIdentifier = options.modelInstanceIdentifier,
	};
	const auto [it, inserted] = decals.emplace(newDecalID, newDecal);
	if (!inserted) {
		it->second = newDecal;
	}
	lastDecalIDValue = newDecalID.value;
	return newDecalID;
}

bool Decals3D::containsDecal(DecalID id) const noexcept {
	return decals.contains(id);
}

bool Decals3D::destroyDecal(DecalID id) {
	return decals.erase(id) > 0;
}

void Decals3D::setDecalPosition(DecalID id, vec3 newPosition) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.position = newPosition;
	}
}

void Decals3D::setDecalRange(DecalID id, float newRange) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.range = newRange;
	}
}

void Decals3D::setDecalOrientation(DecalID id, quat newOrientation) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.orientation = newOrientation;
	}
}

void Decals3D::setDecalSize(DecalID id, vec2 newSize) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.size = newSize;
	}
}

void Decals3D::setDecalOrigin(DecalID id, vec2 newOrigin) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.origin = newOrigin;
	}
}

void Decals3D::setDecalColor(DecalID id, Color newColor) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.color = newColor;
	}
}

void Decals3D::setDecalEmissiveFactor(DecalID id, vec3 newEmissiveFactor) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.emissiveFactor = newEmissiveFactor;
	}
}

void Decals3D::setDecalModelInstanceIdentifier(DecalID id, uint32_t newModelInstanceIdentifier) {
	if (const auto it = decals.find(id); it != decals.end()) {
		it->second.modelInstanceIdentifier = newModelInstanceIdentifier;
	}
}

vec2 Decals3D::getDefaultDecalBaseColorMapPosition() {
	if (!defaultBaseColorMapPosition) {
		constexpr Array WHITE_PIXEL{uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255}};
		defaultBaseColorMapPosition = vec2{insertSprite(*device, baseColorAtlasPacker, baseColorAtlasTexture, PADDING, TextureFormat::R8G8B8A8_SRGB,
			resource::ImageView{resource::ImageType::IMAGE_2D, resource::ImageFormat::R8G8B8A8_UINT, Extent2D{1, 1}, 1, asBytes(Span{WHITE_PIXEL})}, false)};
	}
	return *defaultBaseColorMapPosition;
}

vec2 Decals3D::getDefaultDecalNormalMapPosition() {
	if (!defaultNormalMapPosition) {
		constexpr Array FLAT_NORMAL_PIXEL{uint8_t{128}, uint8_t{128}, uint8_t{255}, uint8_t{255}};
		defaultNormalMapPosition = vec2{insertSprite(*device, normalAtlasPacker, normalAtlasTexture, PADDING, TextureFormat::R8G8B8A8_UNORM,
			resource::ImageView{resource::ImageType::IMAGE_2D, resource::ImageFormat::R8G8B8A8_UINT, Extent2D{1, 1}, 1, asBytes(Span{FLAT_NORMAL_PIXEL})}, false)};
	}
	return *defaultNormalMapPosition;
}

vec2 Decals3D::getDefaultDecalOcclusionRoughnessMetallicMapPosition() {
	if (!defaultOcclusionRoughnessMetallicMapPosition) {
		constexpr Array WHITE_PIXEL{uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255}};
		defaultOcclusionRoughnessMetallicMapPosition =
			vec2{insertSprite(*device, occlusionRoughnessMetallicAtlasPacker, occlusionRoughnessMetallicAtlasTexture, PADDING, TextureFormat::R8G8B8A8_UNORM,
				resource::ImageView{resource::ImageType::IMAGE_2D, resource::ImageFormat::R8G8B8A8_UINT, Extent2D{1, 1}, 1, asBytes(Span{WHITE_PIXEL})}, false)};
	}
	return *defaultOcclusionRoughnessMetallicMapPosition;
}

vec2 Decals3D::getDefaultDecalEmissiveMapPosition() {
	if (!defaultEmissiveMapPosition) {
		constexpr Array WHITE_PIXEL{uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255}};
		defaultEmissiveMapPosition = vec2{insertSprite(*device, emissiveAtlasPacker, emissiveAtlasTexture, PADDING, TextureFormat::R8G8B8A8_SRGB,
			resource::ImageView{resource::ImageType::IMAGE_2D, resource::ImageFormat::R8G8B8A8_UINT, Extent2D{1, 1}, 1, asBytes(Span{WHITE_PIXEL})}, false)};
	}
	return *defaultEmissiveMapPosition;
}

void Decals3D::flush(Renderer3D& renderer3D) const {
	if (parameterBufferDirty) {
		parameterBuffer.upload(Parameters{
			.decalsBaseColorAtlasTexture = (baseColorAtlasTexture) ? baseColorAtlasTexture : renderer3D.getWhiteTexture2D(),
			.decalsNormalAtlasTexture = (normalAtlasTexture) ? normalAtlasTexture : renderer3D.getFlatNormalTexture2D(),
			.decalsOcclusionRoughnessMetallicAtlasTexture = (occlusionRoughnessMetallicAtlasTexture) ? occlusionRoughnessMetallicAtlasTexture : renderer3D.getWhiteTexture2D(),
			.decalsEmissiveAtlasTexture = (emissiveAtlasTexture) ? emissiveAtlasTexture : renderer3D.getWhiteTexture2D(),
		});
		parameterBufferDirty = false;
	}
}

} // namespace grem::graphics
