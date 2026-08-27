// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_INSTANCES_2D_HPP
#define GREM_GRAPHICS_2D_INSTANCES_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/SpriteAtlas.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Camera2D.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer2D; // Forward declaration, to avoid a circular include of Renderer2D.hpp.

/**
 * Configuration of a Model2D instance to draw.
 */
struct ModelInstance2D {
	/**
	 * Non-owning pointer to a texture to apply to the model, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	/**
	 * Offset to apply to every vertex position of the model, in world space.
	 *
	 * \note This offset is applied after transforming the vertex positions by
	 *       the ModelInstance2D::basis.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Basis to scale/rotate/shear the vertex positions of the model by.
	 *
	 * \note The vertex positions are transformed by this matrix before applying
	 *       the ModelInstance2D::offset.
	 */
	mat2 basis{1.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture
	 *       coordinates by the ModelInstance2D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the ModelInstance2D::textureOffset.
	 */
	mat2 textureBasis{1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this effectively controls the base
	 * color of the model.
	 *
	 * \note In the default shader, the output color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1)
	 *       in linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of an arbitrarily transformed 2D triangle instance, optionally
 * textured, to draw.
 */
struct TriangleInstance2D {
	/**
	 * Non-owning pointer to a texture to apply to the triangle, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	vec2 pointA{0.0f, 0.0f}; ///< Position, in world coordinates, of the first vertex of the triangle to draw.
	vec2 pointB{1.0f, 0.0f}; ///< Position, in world coordinates, of the second vertex of the triangle to draw.
	vec2 pointC{0.0f, 1.0f}; ///< Position, in world coordinates, of the third vertex of the triangle to draw.

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture coordinates
	 *       by the TriangleInstance2D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the TriangleInstance2D::textureOffset.
	 */
	mat2 textureBasis{1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this effectively controls the base
	 * color of the triangle.
	 *
	 * \note In the default shader, the output color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1)
	 *       in linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of an arbitrarily transformed 2D quadrilateral instance,
 * optionally textured, to draw.
 */
struct QuadInstance2D {
	/**
	 * Non-owning pointer to a texture to apply to the quad, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	/**
	 * Offset to apply to every vertex position of the quad, in world space.
	 *
	 * \note This offset is applied after transforming the vertex positions by
	 *       the QuadInstance2D::basis.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Basis to scale/rotate/shear the vertex positions of the quad by.
	 *
	 * \note The vertex positions are transformed by this matrix before applying
	 *       the QuadInstance2D::offset.
	 */
	mat2 basis{1.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture coordinates
	 *       by the QuadInstance2D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the QuadInstance2D::textureOffset.
	 */
	mat2 textureBasis{1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this effectively controls the base
	 * color of the quad.
	 *
	 * \note In the default shader, the output color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1)
	 *       in linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of a 2D rectangle instance, optionally textured, to draw.
 */
struct RectangleInstance2D {
	/**
	 * Non-owning pointer to a texture to apply to the rectangle, or nullptr
	 * to use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	/**
	 * Position, in world coordinates, to draw the rectangle at, with
	 * respect to its RectangleInstance2D::origin.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the rectangle counter-clockwise by, around
	 * its RectangleInstance2D::origin.
	 */
	float angle = 0.0f;

	/**
	 * Size of the rectangle, in world coordinates.
	 */
	vec2 size{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the rectangle. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the rectangle.
	 */
	vec2 origin{0.0f, 0.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by RectangleInstance2D::textureScale.
	 *       2. Rotate by RectangleInstance2D::textureAngle.
	 *       3. Translate by RectangleInstance2D::textureOffset.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the texture coordinates by before sampling
	 * the texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by RectangleInstance2D::textureScale.
	 *       2. Rotate by RectangleInstance2D::textureAngle.
	 *       3. Translate by RectangleInstance2D::textureOffset.
	 */
	float textureAngle = 0.0f;

	/**
	 * Coefficients to scale the texture coordinates by before sampling the
	 * texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by RectangleInstance2D::textureScale.
	 *       2. Rotate by RectangleInstance2D::textureAngle.
	 *       3. Translate by RectangleInstance2D::textureOffset.
	 */
	vec2 textureScale{1.0f, 1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this controls the base color of the
	 * rectangle.
	 *
	 * \note In the default shader, the output color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1)
	 *       in linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of a 2D image instance to draw.
 */
struct ImageInstance2D {
	/**
	 * Position, in world coordinates, to draw the image at, with
	 * respect to its ImageInstance2D::origin.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the image counter-clockwise by, around its
	 * ImageInstance2D::origin.
	 */
	float angle = 0.0f;

	/**
	 * Coefficients to scale the size of the image by.
	 *
	 * The resulting textured quad will have the size of the original
	 * texture, multiplied by this value.
	 */
	vec2 scale{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the image. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the image.
	 */
	vec2 origin{0.0f, 0.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by ImageInstance2D::textureScale.
	 *       2. Rotate by ImageInstance2D::textureAngle.
	 *       3. Translate by ImageInstance2D::textureOffset.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the texture coordinates by before sampling
	 * the texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by ImageInstance2D::textureScale.
	 *       2. Rotate by ImageInstance2D::textureAngle.
	 *       3. Translate by ImageInstance2D::textureOffset.
	 */
	float textureAngle = 0.0f;

	/**
	 * Coefficients to scale the texture coordinates by before sampling the
	 * texture.
	 *
	 * \note The texture coordinates are transformed in the following order:
	 *       1. Scale by ImageInstance2D::textureScale.
	 *       2. Rotate by ImageInstance2D::textureAngle.
	 *       3. Translate by ImageInstance2D::textureOffset.
	 */
	vec2 textureScale{1.0f, 1.0f};

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
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of a 2D sprite instance from a SpriteAtlas to draw.
 */
struct SpriteInstance2D {
	/**
	 * Position, in world coordinates, to draw the sprite at, with respect to
	 * its SpriteInstance2D::origin.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the sprite counter-clockwise by, around its
	 * SpriteInstance2D::origin.
	 */
	float angle = 0.0f;

	/**
	 * Coefficients to scale the size of the sprite by.
	 *
	 * The resulting textured quad will have the size of the original
	 * sprite, multiplied by this value.
	 */
	vec2 scale{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the sprite. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the sprite.
	 */
	vec2 origin{0.0f, 0.0f};

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
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shader, the output color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;
};

/**
 * Configuration of a 2D instance of Text2D shaped from a Font2D to draw.
 */
struct TextInstance2D {
	/**
	 * Starting position, in world coordinates, to draw the text at. This will
	 * be the first position on the baseline of the first line of text.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the text counter-clockwise by, around its
	 * TextInstance2D::origin.
	 */
	float angle = 0.0f;

	/**
	 * Coefficients to scale the size of the text by. The result is affected
	 * by Font2DOptions::useLinearFiltering.
	 *
	 * \remark The best visual results are usually achieved when the text is
	 *         shaped at an appropriate character size to begin with, rather
	 *         than relying on this scale parameter. As such, the scale should
	 *         generally be kept at (1, 1) unless many different character sizes
	 *         are used with this font and there is a strict requirement on the
	 *         maximum size of the texture atlas.
	 */
	vec2 scale{1.0f, 1.0f};

	/**
	 * Alignment mode of the text.
	 */
	graphics::TextAlign alignment{};

	/**
	 * Base text color.
	 */
	Color color = Color::WHITE;
};

/**
 * Configuration of a 2D instance of a string of text to draw using a Font2D.
 */
struct TextStringInstance2D {
	/**
	 * Character size to shape the glyphs at.
	 */
	uint32_t characterSize = 16;

	/**
	 * Starting position, in world coordinates, to draw the text at. This will
	 * be the first position on the baseline of the first line of text.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * Scaling to apply to the size of the shaped glyphs. The result is affected
	 * by Font2DOptions::useLinearFiltering.
	 *
	 * \remark The best visual results are usually achieved when the text is
	 *         shaped at an appropriate character size to begin with, rather
	 *         than relying on this scale parameter. As such, the scale should
	 *         generally be kept at (1, 1) unless many different character sizes
	 *         are used with this font and there is a strict requirement on the
	 *         maximum size of the texture atlas.
	 */
	vec2 shapeScale{1.0f, 1.0f};

	/**
	 * Offset, in normalized coordinates, specifying the origin relative to the
	 * baseline of the first line of text. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the first line of text.
	 *
	 * \remark A value of (0.5, 0.0) can be used to center text on the X axis.
	 */
	vec2 shapeOrigin{0.0f, 0.0f};

	/**
	 * Angle, in radians, to rotate the text counter-clockwise by, around its
	 * TextStringInstance2D::origin.
	 */
	float angle = 0.0f;

	/**
	 * Coefficients to scale the size of the text by. The result is affected
	 * by Font2DOptions::useLinearFiltering.
	 *
	 * \remark The best visual results are usually achieved when the text is
	 *         shaped at an appropriate character size to begin with, rather
	 *         than relying on this scale parameter. As such, the scale should
	 *         generally be kept at (1, 1) unless many different character sizes
	 *         are used with this font and there is a strict requirement on the
	 *         maximum size of the texture atlas.
	 */
	vec2 scale{1.0f, 1.0f};

	/**
	 * Alignment mode of the text.
	 */
	graphics::TextAlign alignment{};

	/**
	 * Base text color.
	 */
	Color color = Color::WHITE;
};

/**
 * Batch of instances to be drawn in a Renderer2D frame.
 */
class Instances2D {
public:
	/**
	 * Construct a batch of instances.
	 *
	 * \param device device to create the batch for. Must outlive the batch.
	 * \param renderer2D renderer to create the batch for. Must be associated
	 *        with the given device. Must outlive the batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Instances2D(Device& device, Renderer2D& renderer2D)
		: device(&device)
		, renderer2D(&renderer2D) {}

	/**
	 * Clear the batch of all instances.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void clear() {
		instanceBuffer.clear();
		drawCommands.clear();
	}

	/**
	 * Add a 2D model instance to be rendered using a plain 2D model shader.
	 *
	 * \param model model to draw.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_2d) void putModelInstance(const Model2D& model, const ModelInstance2D& instance);

	/**
	 * Add a 2D model instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the model with.
	 * \param model model to draw.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_2d) void putShadedModelInstance(const Model2D::ShaderPipeline& shaderPipeline, const Model2D& model, const ModelInstance2D& instance);

	/**
	 * Add a triangle instance to be rendered using a plain 2D model shader.
	 *
	 * \param instance configuration of the triangle instance to draw, see
	 *        TriangleInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 */
	GREM_API(graphics_2d) void putTriangleInstance(const TriangleInstance2D& instance);

	/**
	 * Add a triangle instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the triangle with.
	 * \param instance configuration of the triangle instance to draw, see
	 *        TriangleInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 */
	GREM_API(graphics_2d) void putShadedTriangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const TriangleInstance2D& instance);

	/**
	 * Add a quad instance to be rendered using a plain 2D model shader.
	 *
	 * \param instance configuration of the quad instance to draw, see
	 *        QuadInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putImageInstance()
	 * \sa putRectangleInstance()
	 * \sa putSpriteInstance()
	 */
	GREM_API(graphics_2d) void putQuadInstance(const QuadInstance2D& instance);

	/**
	 * Add a quad instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the quad with.
	 * \param instance configuration of the quad instance to draw, see
	 *        QuadInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedImageInstance()
	 * \sa putShadedRectangleInstance()
	 * \sa putShadedSpriteInstance()
	 */
	GREM_API(graphics_2d) void putShadedQuadInstance(const Model2D::ShaderPipeline& shaderPipeline, const QuadInstance2D& instance);

	/**
	 * Add a rectangle instance to be rendered using a plain 2D model shader.
	 *
	 * \param instance configuration of the rectangle instance to draw, see
	 *        RectangleInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 * \sa putImageInstance()
	 * \sa putSpriteInstance()
	 */
	GREM_API(graphics_2d) void putRectangleInstance(const RectangleInstance2D& instance);

	/**
	 * Add a rectangle instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the rectangle with.
	 * \param instance configuration of the rectangle instance to draw, see
	 *        RectangleInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 * \sa putShadedImageInstance()
	 * \sa putShadedSpriteInstance()
	 */
	GREM_API(graphics_2d) void putShadedRectangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const RectangleInstance2D& instance);

	/**
	 * Add an image instance to be rendered using a plain 2D model shader.
	 *
	 * \param texture texture of the image to draw. Must be a valid 2D texture.
	 * \param instance configuration of the image instance to draw, see
	 *        ImageInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 * \sa putRectangleInstance()
	 * \sa putSpriteInstance()
	 */
	GREM_API(graphics_2d) void putImageInstance(const Texture& texture, const ImageInstance2D& instance = {});

	/**
	 * Add an image instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the texture with.
	 * \param texture texture of the image to draw. Must be a valid 2D texture.
	 * \param instance configuration of the image instance to draw, see
	 *        ImageInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 * \sa putShadedRectangleInstance()
	 * \sa putShadedSpriteInstance()
	 */
	GREM_API(graphics_2d) void putShadedImageInstance(const Model2D::ShaderPipeline& shaderPipeline, const Texture& texture, const ImageInstance2D& instance = {});

	/**
	 * Add a sprite instance to be rendered using a plain 2D model shader.
	 *
	 * \param spriteAtlas sprite atlas to fetch the sprite image from.
	 * \param spriteID identifier of the sprite in the atlas to draw. Must be a
	 *        valid sprite identifier obtained from the given atlas.
	 * \param instance configuration of the sprite instance to draw, see
	 *        SpriteInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 * \sa putImageInstance()
	 * \sa putRectangleInstance()
	 */
	GREM_API(graphics_2d) void putSpriteInstance(const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance2D& instance);

	/**
	 * Add a sprite instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the sprite with.
	 * \param spriteAtlas sprite atlas to fetch the sprite image from.
	 * \param spriteID identifier of the sprite in the atlas to draw. Must be a
	 *        valid sprite identifier obtained from the given atlas.
	 * \param instance configuration of the sprite instance to draw, see
	 *        SpriteInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 * \sa putShadedImageInstance()
	 * \sa putShadedRectangleInstance()
	 */
	GREM_API(graphics_2d)
	void putShadedSpriteInstance(const Model2D::ShaderPipeline& shaderPipeline, const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance2D& instance);

	/**
	 * Add a 2D text instance to be rendered using a plain 2D text shader.
	 *
	 * \param text shaped text to draw.
	 * \param instance configuration of the text instance to draw, see
	 *        TextInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putTextStringInstance()
	 */
	GREM_API(graphics_2d) void putTextInstance(const Text2D& text, const TextInstance2D& instance);

	/**
	 * Add a text instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param text shaped text to draw.
	 * \param instance configuration of the text instance to draw, see
	 *        TextInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedTextStringInstance()
	 */
	GREM_API(graphics_2d) void putShadedTextInstance(const Model2D::ShaderPipeline& shaderPipeline, const Text2D& text, const TextInstance2D& instance);

	/**
	 * Add a 2D text string instance to be rendered using a plain 2D text
	 * shader.
	 *
	 * \param font font from which to shape the text.
	 * \param string UTF-8-encoded string to shape the text from.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be rendered
	 *       incorrectly. Only one Unicode code point is rendered at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \sa putTextInstance()
	 */
	GREM_API(graphics_2d) void putTextStringInstance(Font2D& font, UTF8StringView string, const TextStringInstance2D& instance);

	/**
	 * Add a 2D text string instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param font font from which to shape the text.
	 * \param string UTF-8-encoded string to shape the text from.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be rendered
	 *       incorrectly. Only one Unicode code point is rendered at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \sa putShadedTextInstance()
	 */
	GREM_API(graphics_2d)
	void putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, UTF8StringView string, const TextStringInstance2D& instance);

	/**
	 * Add a 2D text string instance to be rendered using a plain 2D text
	 * shader.
	 *
	 * \param font font from which to shape the text.
	 * \param string string to shape the text from, which will be interpreted as
	 *        being UTF-8-encoded.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be rendered
	 *       incorrectly. Only one Unicode code point is rendered at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \sa putTextInstance()
	 */
	GREM_API(graphics_2d) void putTextStringInstance(Font2D& font, StringView string, const TextStringInstance2D& instance);

	/**
	 * Add a 2D text string instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param font font from which to shape the text.
	 * \param string string to shape the text from, which will be interpreted as
	 *        being UTF-8-encoded.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance2D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Right-to-left text shaping is currently not supported.
	 * \note Grapheme clusters are currently not supported, and may be rendered
	 *       incorrectly. Only one Unicode code point is rendered at a time.
	 *
	 * \warning If the string contains invalid UTF-8, the invalid code points
	 *          will generate unspecified glyphs that may have any appearance.
	 *
	 * \sa putShadedTextInstance()
	 */
	GREM_API(graphics_2d) void putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, StringView string, const TextStringInstance2D& instance);

private:
	friend Renderer2D;

	struct DrawCommand {
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;
		SharedPointer<TextureImplementation> textureHandle;
		uint32_t instanceOffset;
	};

	GREM_API(graphics_2d)
	void putShadedModelInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Model2D& model, const ModelInstance2D& instance);

	GREM_API(graphics_2d)
	void putShadedTextInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Text2D& text, const TextInstance2D& instance);

	GREM_API(graphics_2d)
	void pushDrawCommand(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle,
		SharedPointer<TextureImplementation> textureHandle, uint32_t instanceOffset);

	Device* device;
	Renderer2D* renderer2D;
	InstanceBuffer<Model2D::Instance> instanceBuffer{*device};
	ArrayList<DrawCommand> drawCommands{};
};

/**
 * Filter parameters that determine which instances in a 2D batch to draw.
 */
struct Instances2DFilter {};

/**
 * View over a batch of instances to be drawn in a Renderer2D frame.
 */
struct Instances2DView {
	/** Non-owning read-only pointer to the batch of instances. Must not be nullptr. */
	const Instances2D* instances;

	/** Shader pipeline override to use, or nullptr to not use a shader override. */
	SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle{};

	/** Filter parameters that determine which instances to draw. */
	Instances2DFilter filter;

	/**
	 * Construct an instance batch view.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param filter filter parameters, see Instances2DFilter.
	 */
	Instances2DView(const Instances2D& instances, const Instances2DFilter& filter = {})
		: instances(&instances)
		, filter(filter) {}

	/**
	 * Construct an instance batch view with a shader override.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param filter filter parameters, see Instances2DFilter.
	 */
	Instances2DView(const Instances2D& instances, const Model2D::ShaderPipeline& shaderPipelineOverride, const Instances2DFilter& filter = {})
		: instances(&instances)
		, shaderPipelineOverrideHandle(shaderPipelineOverride.lock())
		, filter(filter) {}
};

} // namespace grem::graphics

#endif
