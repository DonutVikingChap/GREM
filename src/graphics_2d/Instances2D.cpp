// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>

#include <utility> // std::move

namespace grem::graphics {

void Instances2D::putModelInstance(const Model2D& model, const ModelInstance2D& instance) {
	putShadedModelInstance(renderer2D->getPlainModel2DShaderPipeline(), model, instance);
}

void Instances2D::putShadedModelInstance(const Model2D::ShaderPipeline& shaderPipeline, const Model2D& model, const ModelInstance2D& instance) {
	putShadedModelInstanceImplementation(shaderPipeline.lock(), model, instance);
}

void Instances2D::putTriangleInstance(const TriangleInstance2D& instance) {
	putShadedTriangleInstance(renderer2D->getPlainModel2DShaderPipeline(), instance);
}

void Instances2D::putShadedTriangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const TriangleInstance2D& instance) {
	putShadedModelInstanceImplementation(shaderPipeline.lock(), renderer2D->getUnitRightAngledTriangleModel2D(),
		ModelInstance2D{
			.texture = instance.texture,
			.position = instance.pointA,
			.basis{instance.pointB - instance.pointA, instance.pointC - instance.pointA},
			.textureOffset = instance.textureOffset,
			.textureBasis = instance.textureBasis,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
		});
}

void Instances2D::putQuadInstance(const QuadInstance2D& instance) {
	putShadedQuadInstance(renderer2D->getPlainModel2DShaderPipeline(), instance);
}

void Instances2D::putShadedQuadInstance(const Model2D::ShaderPipeline& shaderPipeline, const QuadInstance2D& instance) {
	putShadedModelInstanceImplementation(shaderPipeline.lock(), renderer2D->getUnitSquareModel2D(),
		ModelInstance2D{
			.texture = instance.texture,
			.position = instance.position,
			.basis = instance.basis,
			.textureOffset = instance.textureOffset,
			.textureBasis = instance.textureBasis,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
		});
}

void Instances2D::putRectangleInstance(const RectangleInstance2D& instance) {
	putShadedRectangleInstance(renderer2D->getPlainModel2DShaderPipeline(), instance);
}

void Instances2D::putShadedRectangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const RectangleInstance2D& instance) {
	const mat3 transformation = translateRotateScale(instance.position, instance.angle, instance.size) * translate(-instance.origin);
	putShadedModelInstanceImplementation(shaderPipeline.lock(), renderer2D->getUnitSquareModel2D(),
		ModelInstance2D{
			.texture = instance.texture,
			.position = vec2{transformation[2]},
			.basis = mat2{transformation},
			.textureOffset = instance.textureOffset,
			.textureBasis = mat2{rotateScale(instance.textureAngle, instance.textureScale)},
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
		});
}

void Instances2D::putImageInstance(const Texture& texture, const ImageInstance2D& instance) {
	putShadedImageInstance(renderer2D->getPlainModel2DShaderPipeline(), texture, instance);
}

void Instances2D::putShadedImageInstance(const Model2D::ShaderPipeline& shaderPipeline, const Texture& texture, const ImageInstance2D& instance) {
	putShadedRectangleInstance(shaderPipeline,
		RectangleInstance2D{
			.texture = &texture,
			.position = instance.position,
			.angle = instance.angle,
			.size = vec2{texture.getSize2D()} * instance.scale,
			.origin = instance.origin,
			.textureOffset = instance.textureOffset,
			.textureScale = instance.textureScale,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
		});
}

void Instances2D::putSpriteInstance(const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance2D& instance) {
	putShadedSpriteInstance(renderer2D->getPlainModel2DShaderPipeline(), spriteAtlas, spriteID, instance);
}

void Instances2D::putShadedSpriteInstance(const Model2D::ShaderPipeline& shaderPipeline, const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance2D& instance) {
	const auto [textureOffset, textureScale] = spriteAtlas.getSpriteTextureOffsetAndScale(spriteID);
	putShadedRectangleInstance(shaderPipeline,
		RectangleInstance2D{
			.texture = &spriteAtlas.getAtlasTexture(),
			.position = instance.position,
			.angle = instance.angle,
			.size = spriteAtlas.getSprite(spriteID).size * instance.scale,
			.origin = instance.origin,
			.textureOffset = textureOffset,
			.textureScale = textureScale,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
		});
}

void Instances2D::putTextInstance(const Text2D& text, const TextInstance2D& instance) {
	putShadedTextInstance(renderer2D->getTextModel2DShaderPipeline(), text, instance);
}

void Instances2D::putShadedTextInstance(const Model2D::ShaderPipeline& shaderPipeline, const Text2D& text, const TextInstance2D& instance) {
	putShadedTextInstanceImplementation(shaderPipeline.lock(), text, instance);
}

void Instances2D::putTextStringInstance(Font2D& font, UTF8StringView string, const TextStringInstance2D& instance) {
	putShadedTextStringInstance(renderer2D->getTextModel2DShaderPipeline(), font, string, instance);
}

void Instances2D::putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, UTF8StringView string, const TextStringInstance2D& instance) {
	renderer2D->temporaryText.assign(font, instance.characterSize, string, instance.shapeOrigin, instance.shapeScale);
	putShadedTextInstance(shaderPipeline, renderer2D->temporaryText,
		TextInstance2D{
			.position = instance.position,
			.angle = instance.angle,
			.scale = instance.scale,
			.alignment = instance.alignment,
			.color = instance.color,
		});
}

void Instances2D::putTextStringInstance(Font2D& font, StringView string, const TextStringInstance2D& instance) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	putTextStringInstance(font, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()}, instance);
}

void Instances2D::putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, StringView string, const TextStringInstance2D& instance) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	putShadedTextStringInstance(shaderPipeline, font, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()}, instance);
}

void Instances2D::putShadedModelInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Model2D& model, const ModelInstance2D& instance) {
	const uint32_t instanceIndex = instanceBuffer.push(Model2D::Instance{
		.instancePosition = instance.position,
		.instanceBasis = instance.basis,
		.instanceTextureOffset = instance.textureOffset,
		.instanceTextureBasis = instance.textureBasis,
		.instanceTintColor = instance.color.toLinearRGBA(),
		.instanceEmissiveColor = instance.emissiveColor.toLinearRGB(),
	});
	for (const Model2D::Node& node : model.getNodes()) {
		pushDrawCommand(shaderPipelineHandle, node.mesh.lock(), (instance.texture) ? instance.texture->lock() : renderer2D->getWhiteTexture2D().lock(), instanceIndex);
	}
}

void Instances2D::putShadedTextInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Text2D& text, const TextInstance2D& instance) {
	if (text.getShapedGlyphs().empty()) {
		return;
	}
	for (const Text2D::ShapedGlyph& shapedGlyph : text.getShapedGlyphs()) {
		GREM_ASSERT(shapedGlyph.font);
		shapedGlyph.font->renderGlyph(*device, shapedGlyph.characterSize, shapedGlyph.codePoint);
	}
	const vec2 alignmentOffset = text.getAlignmentOffset(instance.alignment);
	SharedPointer<MeshImplementation> meshHandle = renderer2D->getUnitSquareModel2D().getNodes().front().mesh.lock();
	const Font2D* font = text.getShapedGlyphs().front().font;
	uint32_t instanceOffset = instanceBuffer.size();
	for (const Text2D::ShapedGlyph& shapedGlyph : text.getShapedGlyphs()) {
		if (shapedGlyph.font != font) {
			pushDrawCommand(shaderPipelineHandle, meshHandle, font->getAtlasTexture().lock(), instanceOffset);
			font = shapedGlyph.font;
			instanceOffset = instanceBuffer.size();
		}
		const vec2 textureSize = shapedGlyph.font->getAtlasTexture().getSize2D();
		const Optional<Font2D::RenderedGlyphInfo> renderedGlyphInfo = shapedGlyph.font->findRenderedGlyphInfo(shapedGlyph.characterSize, shapedGlyph.codePoint);
		GREM_ASSERT(renderedGlyphInfo);
		const mat3 glyphTransformation =
			translateRotateScale(instance.position, instance.angle, instance.scale) * translateScale(shapedGlyph.shapedOffset + alignmentOffset, shapedGlyph.shapedSize);
		instanceBuffer.push(Model2D::Instance{
			.instancePosition = vec2{glyphTransformation[2]},
			.instanceBasis = mat2{glyphTransformation},
			.instanceTextureOffset = renderedGlyphInfo->positionInAtlas / textureSize,
			.instanceTextureBasis = mat2{scale(renderedGlyphInfo->sizeInAtlas / textureSize)},
			.instanceTintColor = instance.color.toLinearRGBA(),
			.instanceEmissiveColor{},
		});
	}
	pushDrawCommand(std::move(shaderPipelineHandle), std::move(meshHandle), font->getAtlasTexture().lock(), instanceOffset);
}

void Instances2D::pushDrawCommand(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle,
	SharedPointer<TextureImplementation> textureHandle, uint32_t instanceOffset) {
	GREM_ASSERT(instanceOffset <= instanceBuffer.size());
	if (!drawCommands.empty()) {
		const DrawCommand& lastDrawCommand = drawCommands.back();
		GREM_ASSERT(instanceOffset >= lastDrawCommand.instanceOffset);
		if (lastDrawCommand.shaderPipelineHandle == shaderPipelineHandle && lastDrawCommand.meshHandle == meshHandle && lastDrawCommand.textureHandle == textureHandle) {
			return;
		}
		if (lastDrawCommand.instanceOffset == instanceOffset) {
			drawCommands.pop_back();
		}
	}
	drawCommands.push_back(DrawCommand{
		.shaderPipelineHandle = std::move(shaderPipelineHandle),
		.meshHandle = std::move(meshHandle),
		.textureHandle = std::move(textureHandle),
		.instanceOffset = instanceOffset,
	});
}

} // namespace grem::graphics
