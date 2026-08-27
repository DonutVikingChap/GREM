// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>

#include "builtin_shaders_graphics_2d.hpp"

namespace grem::graphics {

constexpr Array<Model2D::Vertex, 4> Renderer2D::UNIT_SQUARE_MESH_2D_VERTICES{
	Model2D::Vertex{.vertexPosition{0.0f, 0.0f}, .vertexTextureCoordinates{0.0f, 0.0f}},
	Model2D::Vertex{.vertexPosition{1.0f, 0.0f}, .vertexTextureCoordinates{1.0f, 0.0f}},
	Model2D::Vertex{.vertexPosition{0.0f, 1.0f}, .vertexTextureCoordinates{0.0f, 1.0f}},
	Model2D::Vertex{.vertexPosition{1.0f, 1.0f}, .vertexTextureCoordinates{1.0f, 1.0f}},
};

Renderer2D::Renderer2D(Device& device, const Renderer2DOptions& options)
	: device(device) {
	(void)options;
}

Renderer2D::~Renderer2D() = default;

const Renderer2D::DefaultModel2DVertexShader& Renderer2D::getDefaultModel2DVertexShader() {
	if (!defaultModel2DVertexShader) {
		[[unlikely]];
		defaultModel2DVertexShader.emplace(Model2D::VertexShader::create(device, detail::RENDERER_2D_DEFAULT_MODEL_2D_VERTEX_SHADER_CODE));
	}
	return *defaultModel2DVertexShader;
}

const Renderer2D::PlainModel2DFragmentShader& Renderer2D::getPlainModel2DFragmentShader() {
	if (!plainModel2DFragmentShader) {
		[[unlikely]];
		plainModel2DFragmentShader.emplace(Model2D::FragmentShader::create(device, detail::RENDERER_2D_PLAIN_MODEL_2D_FRAGMENT_SHADER_CODE));
	}
	return *plainModel2DFragmentShader;
}

const Renderer2D::TextModel2DFragmentShader& Renderer2D::getTextModel2DFragmentShader() {
	if (!textModel2DFragmentShader) {
		[[unlikely]];
		textModel2DFragmentShader.emplace(Model2D::FragmentShader::create(device, detail::RENDERER_2D_TEXT_MODEL_2D_FRAGMENT_SHADER_CODE));
	}
	return *textModel2DFragmentShader;
}

const Renderer2D::TonemappingModel2DFragmentShader& Renderer2D::getTonemappingModel2DFragmentShader() {
	if (!tonemappingModel2DFragmentShader) {
		[[unlikely]];
		tonemappingModel2DFragmentShader.emplace(Model2D::FragmentShader::create(device, detail::RENDERER_2D_TONEMAPPING_MODEL_2D_FRAGMENT_SHADER_CODE));
	}
	return *tonemappingModel2DFragmentShader;
}

void Renderer2D::drawFrameImplementation(RenderPass& renderPass, StridedSpan<const Instances2DView> instanceBatches, const Camera2D& camera,
	Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles) {
	for (const Instances2DView& instanceBatch : instanceBatches) {
		GREM_ASSERT(instanceBatch.instances);
		const Instances2D& instances = *instanceBatch.instances;
		if (!instances.drawCommands.empty()) {
			drawCommandBuffer2D.clear();
			SharedPointer<TextureImplementation> textureHandle = instances.drawCommands.front().textureHandle;
			for (size_t drawCommandIndex = 0; drawCommandIndex < instances.drawCommands.size(); ++drawCommandIndex) {
				const Instances2D::DrawCommand& drawCommand = instances.drawCommands[drawCommandIndex];
				if (drawCommand.textureHandle != textureHandle) {
					textureBuffer2D.upload(Model2D::TextureParameters{.mainTexture = std::move(textureHandle)});
					setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer(), textureBuffer2D);
					renderPass.drawShaded(instanceBatch.shaderPipelineOverrideHandle, drawCommandBuffer2D.lock(), instances.instanceBuffer.lock(), temporaryCombinedBufferHandles);
					drawCommandBuffer2D.clear();
					textureHandle = drawCommand.textureHandle;
				}
				const uint32_t instancesEnd =
					(drawCommandIndex + 1 < instances.drawCommands.size()) ? instances.drawCommands[drawCommandIndex + 1].instanceOffset : instances.instanceBuffer.size();
				drawCommandBuffer2D.append(drawCommand.shaderPipelineHandle, drawCommand.meshHandle, drawCommand.instanceOffset, instancesEnd - drawCommand.instanceOffset);
			}
			textureBuffer2D.upload(Model2D::TextureParameters{.mainTexture = std::move(textureHandle)});
			setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer(), textureBuffer2D);
			renderPass.drawShaded(instanceBatch.shaderPipelineOverrideHandle, drawCommandBuffer2D.lock(), instances.instanceBuffer.lock(), temporaryCombinedBufferHandles);
			drawCommandBuffer2D.clear();
		}
	}
}

} // namespace grem::graphics
