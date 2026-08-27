// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_INSTANCES_3D_HPP
#define GREM_GRAPHICS_3D_INSTANCES_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/SpriteAtlas.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/resource/Model.hpp>

#include <utility> // std::forward

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class Renderer3D; // Forward declaration, to avoid a circular include of Renderer3D.hpp.

/**
 * Configuration of a Model3D instance to draw.
 */
struct ModelInstance3D {
	/**
	 * Tint color to use in the shader.
	 *
	 * \note In the default shaders, the base color is multiplied by this
	 *       value, meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1)
	 *       in linear color, represents no modification to the original base
	 *       color map color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shaders, the base color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;

	/**
	 * Emissive factor to use in the shader.
	 *
	 * \note In the default PBR shader, the emissive color is multiplied by
	 *       this value, meaning that a value of (1, 1, 1) represents no
	 *       modification to the original emissive map color.
	 */
	vec3 emissiveFactor{1.0f, 1.0f, 1.0f};

	/**
	 * Instance identifier that can be used to identify this instance in
	 * shaders. This may, for example, affect which decals are applied by the
	 * PBR shader.
	 */
	uint32_t instanceIdentifier = 0;

	/**
	 * Bias to subtract from the model's distance from the camera, which is used
	 * for distance ordering when drawing transparent meshes. One possible
	 * strategy for reducing artifacts caused by incorrect ordering is to match
	 * this roughly to the average distance of the model's vertices from its
	 * origin.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of a flat Model2D instance to draw in 3D.
 */
struct FlatModelInstance3D {
	/**
	 * Non-owning pointer to a texture to apply to the model, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	/**
	 * Transformation of the model instance.
	 */
	mat4 transformation{1.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture
	 *       coordinates by the Model2DInstance3D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the Model2DInstance3D::textureOffset.
	 */
	mat2 textureBasis{1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this effectively controls the base
	 * color of the model.
	 *
	 * \note In the default shader, the base color is multiplied by this value,
	 *       meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shaders, the base color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;

	/**
	 * Bias to subtract from the model's distance from the camera, which is used
	 * for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of an arbitrarily transformed 3D triangle instance, optionally
 * textured, to draw.
 */
struct TriangleInstance3D {
	/**
	 * Non-owning pointer to a texture to apply to the triangle, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	vec3 pointA{0.0f, 0.0f, 0.0f}; ///< Position, in world coordinates, of the first vertex of the triangle to draw.
	vec3 pointB{1.0f, 0.0f, 0.0f}; ///< Position, in world coordinates, of the second vertex of the triangle to draw.
	vec3 pointC{0.0f, 1.0f, 0.0f}; ///< Position, in world coordinates, of the third vertex of the triangle to draw.

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture coordinates
	 *       by the TriangleInstance3D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the TriangleInstance3D::textureOffset.
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

	/**
	 * Bias to subtract from the triangle's distance from the camera, which is
	 * used for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of a 3D quadrilateral instance, optionally textured, to draw.
 */
struct QuadInstance3D {
	/**
	 * Non-owning pointer to a texture to apply to the quad, or nullptr to
	 * use a fully white texture.
	 *
	 * \warning If not nullptr, the pointed-to texture must be a valid 2D
	 *          texture.
	 */
	const Texture* texture = nullptr;

	/**
	 * Position, in world coordinates, to draw the quad at.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Rotation of the quad around its QuadInstance3D::origin.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Size of the quad, in world coordinates.
	 */
	vec2 size{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the quad. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the quad.
	 */
	vec2 origin{0.0f, 0.0f};

	/**
	 * Offset, in texture coordinates, to apply to the texture coordinates
	 * before sampling the texture.
	 *
	 * \note This offset is applied after transforming the texture
	 *       coordinates by the QuadInstance3D::textureBasis.
	 */
	vec2 textureOffset{0.0f, 0.0f};

	/**
	 * Texture coordinate basis to scale/rotate/shear the texture coordinates by
	 * before sampling the texture.
	 *
	 * \note The texture coordinates are transformed by this matrix before
	 *       applying the QuadInstance3D::textureOffset.
	 */
	mat2 textureBasis{1.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * When no texture is specified, this effectively controls the base
	 * color of the quad.
	 *
	 * \note In the default shader, the base color is multiplied by this value,
	 *       meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shaders, the base color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;

	/**
	 * Bias to subtract from the quad's distance from the camera, which is used
	 * for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of a 3D sprite instance from a SpriteAtlas to draw.
 */
struct SpriteInstance3D {
	/**
	 * Position, in world coordinates, to draw the sprite at.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Rotation of the sprite around its SpriteInstance3D::origin.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * Size of the sprite, in world coordinates.
	 */
	vec2 size{1.0f, 1.0f};

	/**
	 * Offset, in local vertex coordinates, specifying the origin relative to
	 * the bottom left of the sprite. For example, a value of (0.5, 0.5)
	 * corresponds to the middle of the sprite.
	 */
	vec2 origin{0.0f, 0.0f};

	/**
	 * Tint color to use in the shader.
	 *
	 * \note In the default shaders, the base color is multiplied by this value,
	 *       meaning that a value of Color::WHITE, i.e. RGBA(1, 1, 1, 1) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color color = Color::WHITE;

	/**
	 * Emissive color to use in the shader.
	 *
	 * \note In the default shaders, the base color has this value added to it,
	 *       meaning that a value of Color::INVISIBLE, i.e. RGBA(0, 0, 0, 0) in
	 *       linear color, represents no modification to the original texture
	 *       color.
	 */
	Color emissiveColor = Color::INVISIBLE;

	/**
	 * Bias to subtract from the sprite's distance from the camera, which is
	 * used for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of a 3D instance of Text2D shaped from a Font2D to draw.
 */
struct TextInstance3D {
	/**
	 * Starting position, in world coordinates, to draw the text at. This will
	 * be the first position on the baseline of the first line of text.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Rotation of the text around its TextInstance3D::origin.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

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

	/**
	 * Bias to subtract from the text's distance from the camera, which is used
	 * for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Configuration of a 3D instance of a string of text to draw using a Font2D.
 */
struct TextStringInstance3D {
	/**
	 * Character size to shape the glyphs at.
	 */
	uint32_t characterSize = 16;

	/**
	 * Starting position, in world coordinates, to draw the text at. This will
	 * be the first position on the baseline of the first line of text.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * Rotation of the text around its TextStringInstance3D::origin.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

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

	/**
	 * Bias to subtract from the text's distance from the camera, which is used
	 * for distance ordering when drawing transparent meshes.
	 */
	float distanceOrderingBias = 0.0f;
};

/**
 * Batch of instances to be drawn in a Renderer3D frame.
 */
class Instances3D {
public:
	/**
	 * Construct a batch of instances.
	 *
	 * \param device device to create the batch for. Must outlive the batch.
	 * \param renderer3D renderer to create the batch for. Must be associated
	 *        with the given device. Must outlive the batch.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Instances3D(Device& device, Renderer3D& renderer3D)
		: device(&device)
		, renderer3D(&renderer3D) {}

	/**
	 * Clear the batch of all instances.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void clear() {
		jointsDirty = !joints.empty();
		morphTargetWeightsDirty = !morphTargetWeights.empty();
		model3DInstanceBuffer.clear();
		model2DInstanceBuffer.clear();
		opaqueModel3DDrawCommandBuffer.clear();
		transparent3DDrawCommands.clear();
		transparent2DDrawCommands.clear();
		joints.clear();
		morphTargetWeights.clear();
	}

	/**
	 * Add a 3D model instance to be rendered using the default PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance. Must contain
	 *        at least as many joints and morph target weights as the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putPBRModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in its default bind pose using the
	 * default PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putPBRModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in a specific local pose using the
	 * default PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param pose local pose to draw the model in. The number of joints in the
	 *        pose must match the number of joints in the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putPBRModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const bool> instancesVisible);

	/**
	 * Add a 3D model instance to be rendered using the default HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance. Must contain
	 *        at least as many joints and morph target weights as the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRPBRModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in its default bind pose using the
	 * default HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRPBRModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in a specific local pose using the
	 * default HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param pose local pose to draw the model in. The number of joints in the
	 *        pose must match the number of joints in the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRPBRModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR PBR shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const bool> instancesVisible);

	/**
	 * Add a 3D model instance to be rendered using the default unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance. Must contain
	 *        at least as many joints and morph target weights as the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putUnlitModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in its default bind pose using the
	 * default unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putUnlitModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in a specific local pose using the
	 * default unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param pose local pose to draw the model in. The number of joints in the
	 *        pose must match the number of joints in the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putUnlitModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance = {});

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the instances to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the instances to draw, see
	 *        ModelInstance3D.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const bool> instancesVisible);

	/**
	 * Add a 3D model instance to be rendered using the default HDR unlit
	 * shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance. Must contain
	 *        at least as many joints and morph target weights as the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRUnlitModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in its default bind pose using the
	 * default HDR unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRUnlitModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance = {});

	/**
	 * Add a 3D model instance to be rendered in a specific local pose using the
	 * default HDR unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param pose local pose to draw the model in. The number of joints in the
	 *        pose must match the number of joints in the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRUnlitModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance = {});

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d) void putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
		StridedSpan<const bool> instancesVisible);

	/**
	 * Add a list of instances of a 3D model to be rendered using the default
	 * HDR unlit shader if they are in a visible set.
	 *
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics_3d)
	void putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const bool> instancesVisible);

	/**
	 * Add a 3D model instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformation transformation of the model instance. Must contain
	 *        at least as many joints and morph target weights as the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstance(auto&& shaderPipelineSelector, const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance = {}) {
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		putShadedModelInstanceImplementation(shaderPipelineSelectorAdapter, model, transformation, instance);
	}

	/**
	 * Add a 3D model instance to be rendered in its default bind pose using a
	 * specific shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its mesh
	 *        configuration.
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstance(auto&& shaderPipelineSelector, const Model3D& model, const mat4& transformation, const ModelInstance3D& instance = {}) {
		const resource::Model::Pose& pose = model.getBindPose();
		resource::Model::Transformation& temporaryModelTransformation = getTemporaryModelTransformation();
		temporaryModelTransformation.assign(transformation, pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
		putShadedModelInstance(std::forward<decltype(shaderPipelineSelector)>(shaderPipelineSelector), model, temporaryModelTransformation, instance);
	}

	/**
	 * Add a 3D model instance to be rendered in a specific local pose using a
	 * specific shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its mesh
	 *        configuration.
	 * \param model model to draw.
	 * \param transformation transformation of the model instance.
	 * \param pose local pose to draw the model in. The number of joints in the
	 *        pose must match the number of joints in the model.
	 * \param instance configuration of the model instance to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstance(auto&& shaderPipelineSelector, const Model3D& model, const mat4& transformation, resource::Model::PoseView pose,
		const ModelInstance3D& instance = {}) {
		resource::Model::Transformation& temporaryModelTransformation = getTemporaryModelTransformation();
		temporaryModelTransformation.assign(transformation, Span{pose.localJoints, model.getJointCount()}, Span{pose.localMorphTargetWeights, model.getMorphTargetWeightCount()},
			model.getJointParentIndices());
		putShadedModelInstance(std::forward<decltype(shaderPipelineSelector)>(shaderPipelineSelector), model, temporaryModelTransformation, instance);
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		StridedSpan<const ModelInstance3D> instances) {
		GREM_ASSERT(transformations.size() == instances.size());
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		putShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, instances);
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the instances to draw, see
	 *        ModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		const ModelInstance3D& instance) {
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		putShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, StridedSpan{&instance, 1});
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations) {
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		const ModelInstance3D defaultInstance{};
		putShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, StridedSpan{&defaultInstance, 1});
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline if they are in a visible set.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instances configuration of each model instance to draw, see
	 *        ModelInstance3D. Must have the same size as `transformations`.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putVisibleShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
		GREM_ASSERT(transformations.size() == instances.size());
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		putVisibleShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, instances, instancesVisible);
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline if they are in a visible set.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instance configuration of the model instances to draw, see
	 *        ModelInstance3D.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putVisibleShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		const ModelInstance3D& instance, StridedSpan<const bool> instancesVisible) {
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		putVisibleShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, StridedSpan{&instance, 1}, instancesVisible);
	}

	/**
	 * Add a list of instances of a 3D model to be rendered using a specific
	 * shader pipeline if they are in a visible set.
	 *
	 * \param shaderPipelineSelector function that returns the shader pipeline
	 *        to render each mesh of the model with, given its shader
	 *        configuration.
	 * \param model model to draw.
	 * \param transformations transformation of each model instance. Each must
	 *        contain at least as many joints and morph target weights as the
	 *        model.
	 * \param instancesVisible visibility of each instance. Must have the same
	 *        size as `transformations`.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putVisibleShadedModelInstances(auto&& shaderPipelineSelector, const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
		StridedSpan<const bool> instancesVisible) {
		const auto shaderPipelineSelectorAdapter = [&](const Model3D::ShaderConfiguration& shaderConfiguration) -> SharedPointer<ShaderPipelineImplementation> {
			return shaderPipelineSelector(shaderConfiguration).lock();
		};
		const ModelInstance3D defaultInstance{};
		putVisibleShadedModelInstancesImplementation(shaderPipelineSelectorAdapter, model, transformations, StridedSpan{&defaultInstance, 1}, instancesVisible);
	}

	/**
	 * Add a flat 2D model instance to be rendered using a plain 2D model
	 * shader.
	 *
	 * \param model model to draw.
	 * \param instance configuration of the flat model instance to draw, see
	 *        FlatModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 * \sa putSpriteInstance()
	 */
	GREM_API(graphics_3d) void putFlatModelInstance(const Model2D& model, const FlatModelInstance3D& instance);

	/**
	 * Add a flat 2D model instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the model with.
	 * \param model model to draw.
	 * \param instance configuration of the flat model instance to draw, see
	 *        FlatModelInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 * \sa putShadedSpriteInstance()
	 */
	GREM_API(graphics_3d) void putShadedFlatModelInstance(const Model2D::ShaderPipeline& shaderPipeline, const Model2D& model, const FlatModelInstance3D& instance);

	/**
	 * Add a triangle instance to be rendered using a plain 2D model shader.
	 *
	 * \param instance configuration of the triangle instance to draw, see
	 *        TriangleInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putQuadInstance()
	 */
	GREM_API(graphics_3d) void putTriangleInstance(const TriangleInstance3D& instance);

	/**
	 * Add a triangle instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the triangle with.
	 * \param instance configuration of the triangle instance to draw, see
	 *        TriangleInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedQuadInstance()
	 */
	GREM_API(graphics_3d) void putShadedTriangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const TriangleInstance3D& instance);

	/**
	 * Add a quad instance to be rendered using a plain 2D model shader.
	 *
	 * \param instance configuration of the quad instance to draw, see
	 *        QuadInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putFlatModelInstance()
	 * \sa putSpriteInstance()
	 */
	GREM_API(graphics_3d) void putQuadInstance(const QuadInstance3D& instance);

	/**
	 * Add a quad instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the quad with.
	 * \param instance configuration of the quad instance to draw, see
	 *        QuadInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedFlatModelInstance()
	 * \sa putShadedSpriteInstance()
	 */
	GREM_API(graphics_3d) void putShadedQuadInstance(const Model2D::ShaderPipeline& shaderPipeline, const QuadInstance3D& instance);

	/**
	 * Add a sprite instance to be rendered using a plain 2D model shader.
	 *
	 * \param spriteAtlas sprite atlas to fetch the sprite image from.
	 * \param spriteID identifier of the sprite in the atlas to draw. Must be a
	 *        valid sprite identifier obtained from the given atlas.
	 * \param instance configuration of the sprite instance to draw, see
	 *        SpriteInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putFlatModelInstance()
	 * \sa putQuadInstance()
	 */
	GREM_API(graphics_3d) void putSpriteInstance(const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance3D& instance);

	/**
	 * Add a sprite instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the sprite with.
	 * \param spriteAtlas sprite atlas to fetch the sprite image from.
	 * \param spriteID identifier of the sprite in the atlas to draw. Must be a
	 *        valid sprite identifier obtained from the given atlas.
	 * \param instance configuration of the sprite instance to draw, see
	 *        SpriteInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedFlatModelInstance()
	 * \sa putShadedQuadInstance()
	 */
	GREM_API(graphics_3d)
	void putShadedSpriteInstance(const Model2D::ShaderPipeline& shaderPipeline, const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance3D& instance);

	/**
	 * Add a 3D text instance to be rendered using a plain 3D text shader.
	 *
	 * \param text shaped text to draw.
	 * \param instance configuration of the text instance to draw, see
	 *        TextInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putTextStringInstance()
	 */
	GREM_API(graphics_3d) void putTextInstance(const Text2D& text, const TextInstance3D& instance);

	/**
	 * Add a 3D text instance to be rendered using a specific shader pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param text shaped text to draw.
	 * \param instance configuration of the text instance to draw, see
	 *        TextInstance3D.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa putShadedTextStringInstance()
	 */
	GREM_API(graphics_3d) void putShadedTextInstance(const Model2D::ShaderPipeline& shaderPipeline, const Text2D& text, const TextInstance3D& instance);

	/**
	 * Add a 3D text string instance to be rendered using a plain 3D text
	 * shader.
	 *
	 * \param font font from which to shape the text.
	 * \param string UTF-8-encoded string to shape the text from.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance3D.
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
	GREM_API(graphics_3d) void putTextStringInstance(Font2D& font, UTF8StringView string, const TextStringInstance3D& instance);

	/**
	 * Add a 3D text string instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param font font from which to shape the text.
	 * \param string UTF-8-encoded string to shape the text from.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance3D.
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
	GREM_API(graphics_3d)
	void putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, UTF8StringView string, const TextStringInstance3D& instance);

	/**
	 * Add a 3D text string instance to be rendered using a plain 3D text
	 * shader.
	 *
	 * \param font font from which to shape the text.
	 * \param string string to shape the text from, which will be interpreted as
	 *        being UTF-8-encoded.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance3D.
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
	GREM_API(graphics_3d) void putTextStringInstance(Font2D& font, StringView string, const TextStringInstance3D& instance);

	/**
	 * Add a 3D text string instance to be rendered using a specific shader
	 * pipeline.
	 *
	 * \param shaderPipeline shader pipeline to render the text with.
	 * \param font font from which to shape the text.
	 * \param string string to shape the text from, which will be interpreted as
	 *        being UTF-8-encoded.
	 * \param instance configuration of the text string instance to draw, see
	 *        TextStringInstance3D.
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
	GREM_API(graphics_3d) void putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, StringView string, const TextStringInstance3D& instance);

private:
	friend Renderer3D;

	struct Transparent3DDrawCommand {
		vec3 position;
		uint32_t instanceIndex;
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;
		float boundingRadius;
		float distanceOrderingBias;
	};

	struct Transparent2DDrawCommand {
		mat4 transformation;
		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle;
		SharedPointer<MeshImplementation> meshHandle;
		SharedPointer<TextureImplementation> textureHandle;
		uint32_t instanceOffset;
		float boundingRadius;
		float distanceOrderingBias;
	};

	[[nodiscard]] GREM_API(graphics_3d) resource::Model::Transformation& getTemporaryModelTransformation();

	GREM_API(graphics_3d)
	void putShadedModelInstanceImplementation(
		FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
		resource::Model::TransformationView transformation, const ModelInstance3D& instance);

	GREM_API(graphics_3d)
	void putShadedModelInstancesImplementation(
		FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
		StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances);

	GREM_API(graphics_3d)
	void putVisibleShadedModelInstancesImplementation(
		FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
		StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible);

	GREM_API(graphics_3d)
	void putShadedFlatModelInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Model2D& model, const FlatModelInstance3D& instance);

	GREM_API(graphics_3d)
	void putShadedTextInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Text2D& text, const TextInstance3D& instance);

	Device* device;
	Renderer3D* renderer3D;
	mutable Model3D::InstanceBuffers buffers{*device};
	mutable bool jointsDirty = false;
	mutable bool morphTargetWeightsDirty = false;
	InstanceBuffer<Model3D::Instance> model3DInstanceBuffer{*device};
	InstanceBuffer<Model2D::Instance> model2DInstanceBuffer{*device};
	UnorderedDrawCommandBuffer<Model3D::Mesh> opaqueModel3DDrawCommandBuffer{*device};
	ArrayList<Transparent3DDrawCommand> transparent3DDrawCommands{};
	ArrayList<Transparent2DDrawCommand> transparent2DDrawCommands{};
	Buffer<Model3D::JointFields> joints{};
	Buffer<Model3D::MorphTargetWeightFields> morphTargetWeights{};
};

/**
 * Filter parameters that determine which instances in a 3D batch to draw.
 */
struct Instances3DFilter {
	/** Function that determines whether a specific transparent instance should be drawn or not. */
	FunctionView<bool(vec3 position, float boundingRadius, float distanceOrderingBias)> transparentInstanceFilter = [](vec3, float, float) -> bool {
		return true;
	};

	/** If set, alpha-blended model mesh instances will not be drawn. */
	bool skipAlphaBlendedModelMeshInstances = false;

	/** If set, non-alpha-blended model mesh instances will not be drawn. */
	bool skipOpaqueModelMeshInstances = false;

	/** If set, 3D instances will not be drawn. */
	bool skipAll3DInstances = false;

	/** If set, 2D instances will not be drawn. */
	bool skipAll2DInstances = false;
};

/**
 * View over a batch of instances to be drawn in a Renderer3D frame.
 */
struct Instances3DView {
	/** Non-owning read-only pointer to the batch of instances. Must not be nullptr. */
	const Instances3D* instances;

	/** Shader pipeline override to use for 2D mesh instances, or nullptr to not use a shader override. */
	SharedPointer<ShaderPipelineImplementation> shaderPipelineOverride2DHandle{};

	/** Shader pipeline override to use for 3D mesh instances, or nullptr to not use a shader override. */
	SharedPointer<ShaderPipelineImplementation> shaderPipelineOverride3DHandle{};

	/** Filter parameters that determine which instances to draw. */
	Instances3DFilter filter;

	/**
	 * Construct an instance batch view.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param filter filter parameters, see Instances3DFilter.
	 */
	Instances3DView(const Instances3D& instances, const Instances3DFilter& filter = {})
		: instances(&instances)
		, filter(filter) {}

	/**
	 * Construct an instance batch view with a 2D shader override.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param shaderPipelineOverride2D shader pipeline override to use for 2D
	 *        mesh instances.
	 * \param filter filter parameters, see Instances3DFilter.
	 */
	Instances3DView(const Instances3D& instances, const Model2D::ShaderPipeline& shaderPipelineOverride2D, const Instances3DFilter& filter = {})
		: instances(&instances)
		, shaderPipelineOverride2DHandle(shaderPipelineOverride2D.lock())
		, filter(filter) {}

	/**
	 * Construct an instance batch view with a 3D shader override.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param shaderPipelineOverride3D shader pipeline override to use for 3D
	 *        mesh instances.
	 * \param filter filter parameters, see Instances3DFilter.
	 */
	Instances3DView(const Instances3D& instances, const Model3D::ShaderPipeline& shaderPipelineOverride3D, const Instances3DFilter& filter = {})
		: instances(&instances)
		, shaderPipelineOverride3DHandle(shaderPipelineOverride3D.lock())
		, filter(filter) {}

	/**
	 * Construct an instance batch view with both 2D and 3D shader overrides.
	 *
	 * \param instances instances to reference. Must outlive the view.
	 * \param shaderPipelineOverride2D shader pipeline override to use for 2D
	 *        mesh instances.
	 * \param shaderPipelineOverride3D shader pipeline override to use for 3D
	 *        mesh instances.
	 * \param filter filter parameters, see Instances3DFilter.
	 */
	Instances3DView(const Instances3D& instances, const Model2D::ShaderPipeline& shaderPipelineOverride2D, const Model3D::ShaderPipeline& shaderPipelineOverride3D,
		const Instances3DFilter& filter = {})
		: instances(&instances)
		, shaderPipelineOverride2DHandle(shaderPipelineOverride2D.lock())
		, shaderPipelineOverride3DHandle(shaderPipelineOverride3D.lock())
		, filter(filter) {}
};

} // namespace grem::graphics

#endif
