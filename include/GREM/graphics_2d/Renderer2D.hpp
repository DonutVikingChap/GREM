// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_RENDERER_2D_HPP
#define GREM_GRAPHICS_2D_RENDERER_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

/**
 * Configuration options for a Renderer2D.
 */
struct Renderer2DOptions {
	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Renderer2DOptions& other) const noexcept = default;
};

/**
 * Persistent system that provides 2D rendering capabilities.
 *
 * \sa Renderer3D
 */
class Renderer2D {
public:
	/** Default vertex shader type for drawing 2D models. */
	using DefaultModel2DVertexShader = Model2D::VertexShader;

	/** Default fragment shader type for drawing plain 2D models. */
	using PlainModel2DFragmentShader = Model2D::FragmentShader;

	/** Default fragment shader type for drawing text. */
	using TextModel2DFragmentShader = Model2D::FragmentShader;

	/** Default fragment shader type for tonemapping. */
	using TonemappingModel2DFragmentShader = Model2D::FragmentShader;

	/**
	 * Construct a 2D renderer.
	 *
	 * \param device device to create the renderer for. Must outlive the
	 *        renderer.
	 * \param options initial configuration of the renderer, see
	 *        Renderer2DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_2d) explicit Renderer2D(Device& device, const Renderer2DOptions& options = {});

	/** Destructor. */
	GREM_API(graphics_2d) ~Renderer2D();

	/** Copying a renderer is not allowed. */
	Renderer2D(const Renderer2D&) = delete;

	/** Moving a renderer is not allowed. */
	Renderer2D(Renderer2D&&) = delete;

	/** Copying a renderer is not allowed. */
	Renderer2D& operator=(const Renderer2D&) = delete;

	/** Moving a renderer is not allowed. */
	Renderer2D& operator=(Renderer2D&&) = delete;

	/**
	 * Push the draw commands of a frame of 2D instance batches to a render
	 * pass.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param instanceBatches list of read-only views over the instance batches
	 *        to draw.
	 * \param camera perspective to render the instances from.
	 * \param extraBuffers extra buffers required by the shaders used in the
	 *        batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All extra buffers required by the shaders used in the batch
	 *          must be provided through the parameter pack.
	 */
	template <typename... ExtraBuffers>
	void drawFrame(RenderPass& renderPass, StridedSpan<const Instances2DView> instanceBatches, const Camera2D& camera, const ExtraBuffers&... extraBuffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(ExtraBuffers)> extraBufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{ExtraBuffers::LAYOUT_REFERENCE, extraBuffers.lock()}...,
		};
		drawFrameImplementation(renderPass, instanceBatches, camera, extraBufferHandles);
	}

	/**
	 * Get the default invisible 2D texture.
	 *
	 * \return a read-only reference to the invisible 2D texture.
	 */
	[[nodiscard]] const Texture& getInvisibleTexture2D() {
		return invisibleTexture2D;
	}

	/**
	 * Get the default invisible 2D array texture.
	 *
	 * \return a read-only reference to the invisible 2D array texture.
	 */
	[[nodiscard]] const Texture& getInvisibleTexture2DArray() {
		return invisibleTexture2DArray;
	}

	/**
	 * Get the default invisible cube texture.
	 *
	 * \return a read-only reference to the invisible cube texture.
	 */
	[[nodiscard]] const Texture& getInvisibleTextureCube() {
		return invisibleTextureCube;
	}

	/**
	 * Get the default invisible cube array texture.
	 *
	 * \return a read-only reference to the invisible cube array texture.
	 */
	[[nodiscard]] const Texture& getInvisibleTextureCubeArray() {
		return invisibleTextureCubeArray;
	}

	/**
	 * Get the default white 2D texture.
	 *
	 * \return a read-only reference to the white 2D texture.
	 */
	[[nodiscard]] const Texture& getWhiteTexture2D() {
		return whiteTexture2D;
	}

	/**
	 * Get the default white cubemap texture.
	 *
	 * \return a read-only reference to the white cubemap texture.
	 */
	[[nodiscard]] const Texture& getWhiteTextureCube() {
		return whiteTextureCube;
	}

	/**
	 * Get the default flat normal map 2D texture.
	 *
	 * \return a read-only reference to the flat normal map 2D texture.
	 */
	[[nodiscard]] const Texture& getFlatNormalTexture2D() {
		return flatNormalTexture2D;
	}

	/**
	 * Get the default 2D depth texture.
	 *
	 * \return a read-only reference to the default 2D depth texture.
	 */
	[[nodiscard]] const Texture& getDefaultDepthTexture2D() {
		return defaultDepthTexture2D;
	}

	/**
	 * Get the default 2D array depth texture.
	 *
	 * \return a read-only reference to the default 2D array depth texture.
	 */
	[[nodiscard]] const Texture& getDefaultDepthTexture2DArray() {
		return defaultDepthTexture2DArray;
	}

	/**
	 * Get the default cube depth texture.
	 *
	 * \return a read-only reference to the default cube depth texture.
	 */
	[[nodiscard]] const Texture& getDefaultDepthTextureCube() {
		return defaultDepthTextureCube;
	}

	/**
	 * Get the default cube array depth texture.
	 *
	 * \return a read-only reference to the default cube array depth texture.
	 */
	[[nodiscard]] const Texture& getDefaultDepthTextureCubeArray() {
		return defaultDepthTextureCubeArray;
	}

	/**
	 * Get the default 2D model vertex shader.
	 *
	 * \return a read-only reference to the default 2D model vertex shader.
	 */
	[[nodiscard]] GREM_API(graphics_2d) const Model2D::VertexShader& getDefaultModel2DVertexShader();

	/**
	 * Get the default plain 2D model fragment shader.
	 *
	 * \return a read-only reference to the plain 2D model fragment shader.
	 */
	[[nodiscard]] GREM_API(graphics_2d) const PlainModel2DFragmentShader& getPlainModel2DFragmentShader();

	/**
	 * Get the default plain 2D model shader pipeline.
	 *
	 * \return a read-only reference to the plain 2D model shader pipeline.
	 */
	[[nodiscard]] const Model2D::ShaderPipeline& getPlainModel2DShaderPipeline() {
		if (!plainModel2DShaderPipeline) {
			[[unlikely]];
			plainModel2DShaderPipeline.emplace(device, getDefaultModel2DVertexShader(), Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS, getPlainModel2DFragmentShader(),
				Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, Model2D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *plainModel2DShaderPipeline;
	}

	/**
	 * Get the default text fragment shader.
	 *
	 * \return a read-only reference to the text fragment shader.
	 */
	[[nodiscard]] GREM_API(graphics_2d) const TextModel2DFragmentShader& getTextModel2DFragmentShader();

	/**
	 * Get the default text shader pipeline.
	 *
	 * \return a read-only reference to the text shader pipeline.
	 */
	[[nodiscard]] const Model2D::ShaderPipeline& getTextModel2DShaderPipeline() {
		if (!textModel2DShaderPipeline) {
			[[unlikely]];
			textModel2DShaderPipeline.emplace(device, getDefaultModel2DVertexShader(), Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS, getTextModel2DFragmentShader(),
				Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, Model2D::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *textModel2DShaderPipeline;
	}

	/**
	 * Get the default tonemapping fragment shader.
	 *
	 * \return a read-only reference to the tonemapping fragment shader.
	 */
	[[nodiscard]] GREM_API(graphics_2d) const TonemappingModel2DFragmentShader& getTonemappingModel2DFragmentShader();

	/**
	 * Get the default tonemapping shader pipeline.
	 *
	 * \return a read-only reference to the tonemapping shader pipeline.
	 */
	[[nodiscard]] const Model2D::ShaderPipeline& getTonemappingModel2DShaderPipeline() {
		if (!tonemappingModel2DShaderPipeline) {
			[[unlikely]];
			ShaderPipelineOptions shaderPipelineOptions = Model2D::DEFAULT_SHADER_PIPELINE_OPTIONS;
			shaderPipelineOptions.blendState.reset();
			tonemappingModel2DShaderPipeline.emplace(device, getDefaultModel2DVertexShader(), Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS, getTonemappingModel2DFragmentShader(),
				Model2D::DEFAULT_FRAGMENT_SHADER_CONSTANTS, shaderPipelineOptions);
		}
		return *tonemappingModel2DShaderPipeline;
	}

	/**
	 * Get the default 2D unit square model, which holds a single mesh that
	 * forms a square using a triangle strip of 2 triangles, consisting of 4
	 * vertices in total with the following positions (and identical texture
	 * coordinates):
	 * - (0, 0)
	 * - (1, 0)
	 * - (0, 1)
	 * - (1, 1)
	 *
	 * \return a read-only reference to the 2D unit square model.
	 */
	[[nodiscard]] const Model2D& getUnitSquareModel2D() const noexcept {
		return unitSquareModel2D;
	}

	/**
	 * Get the default 2D unit right-angled triangle model, which holds a single
	 * mesh that forms a single triangle, consisting of 3 vertices in total with
	 * the following positions (and identical texture coordinates):
	 * - (0, 0)
	 * - (1, 0)
	 * - (0, 1)
	 *
	 * \return a read-only reference to the 2D right-angled triangle model.
	 */
	[[nodiscard]] const Model2D& getUnitRightAngledTriangleModel2D() const noexcept {
		return unitRightAngledTriangleModel2D;
	}

private:
	friend Instances2D;

	static constexpr TextureSamplerOptions DEFAULT_DEPTH_TEXTURE_SAMPLER_OPTIONS{
		.minificationFilter = TextureFilter::NEAREST,
		.magnificationFilter = TextureFilter::NEAREST,
		.mipmapMode = TextureMipmapMode::NONE,
		.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
		.depthComparisonMode = TextureDepthComparisonMode::LESS_OR_EQUAL,
	};

	GREM_API(graphics_2d) static const Array<Model2D::Vertex, 4> UNIT_SQUARE_MESH_2D_VERTICES;

	template <typename... Buffers>
	void setTemporaryCombinedBufferHandles(Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles, const Buffers&... buffers) {
		temporaryCombinedBufferHandles.clear();
		(temporaryCombinedBufferHandles.push_back(Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}), ...);
		for (const Pair<BufferLayoutReference, SharedPointer<void>>& extraBufferHandle : extraBufferHandles) {
			temporaryCombinedBufferHandles.push_back(extraBufferHandle);
		}
		sortByAscending<&Pair<BufferLayoutReference, SharedPointer<void>>::first>(temporaryCombinedBufferHandles);
	}

	GREM_API(graphics_2d)
	void drawFrameImplementation(RenderPass& renderPass, StridedSpan<const Instances2DView> instanceBatches, const Camera2D& camera,
		Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles);

	Device& device;
	Texture invisibleTexture2D =
		Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R8G8B8A8_UNORM, Extent2D{1}, 1, ClearValues{.color = Color::INVISIBLE}, TextureSamplerOptions::UNFILTERED);
	Texture invisibleTexture2DArray = Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::R8G8B8A8_UNORM, Extent3D{1}, 1, ClearValues{.color = Color::INVISIBLE},
		TextureSamplerOptions::UNFILTERED);
	Texture invisibleTextureCube = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R8G8B8A8_UNORM, Extent3D{1, 1, 6}, 1, ClearValues{.color = Color::INVISIBLE},
		TextureSamplerOptions::UNFILTERED);
	Texture invisibleTextureCubeArray = Texture::create(device, TextureType::TEXTURE_CUBE_ARRAY, TextureFormat::R8G8B8A8_UNORM, Extent3D{1, 1, 6}, 1,
		ClearValues{.color = Color::INVISIBLE}, TextureSamplerOptions::UNFILTERED);
	Texture whiteTexture2D =
		Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R8G8B8A8_UNORM, Extent2D{1}, 1, ClearValues{.color = Color::WHITE}, TextureSamplerOptions::UNFILTERED);
	Texture whiteTextureCube = Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::R8G8B8A8_UNORM, Extent3D{1, 1, 6}, 1, ClearValues{.color = Color::WHITE},
		TextureSamplerOptions::UNFILTERED);
	Texture flatNormalTexture2D = Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R8G8B8A8_UNORM, Extent2D{1}, 1,
		ClearValues{.color = Color::fromLinear(0.5f, 0.5f, 1.0f)}, TextureSamplerOptions::UNFILTERED);
	Texture defaultDepthTexture2D =
		Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::D16_UNORM, Extent2D{1}, 1, ClearValues{.depth = 1.0f}, DEFAULT_DEPTH_TEXTURE_SAMPLER_OPTIONS);
	Texture defaultDepthTexture2DArray =
		Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::D16_UNORM, Extent3D{1}, 1, ClearValues{.depth = 1.0f}, DEFAULT_DEPTH_TEXTURE_SAMPLER_OPTIONS);
	Texture defaultDepthTextureCube =
		Texture::create(device, TextureType::TEXTURE_CUBE, TextureFormat::D16_UNORM, Extent3D{1, 1, 6}, 1, ClearValues{.depth = 1.0f}, DEFAULT_DEPTH_TEXTURE_SAMPLER_OPTIONS);
	Texture defaultDepthTextureCubeArray =
		Texture::create(device, TextureType::TEXTURE_CUBE_ARRAY, TextureFormat::D16_UNORM, Extent3D{1, 1, 6}, 1, ClearValues{.depth = 1.0f}, DEFAULT_DEPTH_TEXTURE_SAMPLER_OPTIONS);
	Optional<Model2D::VertexShader> defaultModel2DVertexShader{};
	Optional<PlainModel2DFragmentShader> plainModel2DFragmentShader{};
	Optional<Model2D::ShaderPipeline> plainModel2DShaderPipeline{};
	Optional<TextModel2DFragmentShader> textModel2DFragmentShader{};
	Optional<Model2D::ShaderPipeline> textModel2DShaderPipeline{};
	Optional<TonemappingModel2DFragmentShader> tonemappingModel2DFragmentShader{};
	Optional<Model2D::ShaderPipeline> tonemappingModel2DShaderPipeline{};
	Model2D unitSquareModel2D{device, *this, UNIT_SQUARE_MESH_2D_VERTICES};
	Model2D unitRightAngledTriangleModel2D{device, *this, Span{UNIT_SQUARE_MESH_2D_VERTICES}.first<3>()};
	Text2D temporaryText{};
	ArrayList<Pair<BufferLayoutReference, SharedPointer<void>>> temporaryCombinedBufferHandles{};
	DrawCommandBuffer<Model2D::Mesh> drawCommandBuffer2D{device};
	Model2D::TextureBuffer textureBuffer2D{device};
};

} // namespace grem::graphics

#endif
