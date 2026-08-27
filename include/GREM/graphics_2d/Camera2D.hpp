// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_2D_CAMERA_2D_HPP
#define GREM_GRAPHICS_2D_CAMERA_2D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Variant.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/buffers.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

/**
 * Configuration of an orthographic projection in 2D.
 */
struct OrthographicProjection2D {
	/**
	 * Bottom left corner of the orthographic projection, in render target
	 * coordinates.
	 */
	vec2 offset{0.0f, 0.0f};

	/**
	 * Size of the orthographic projection, in render target coordinates.
	 */
	vec2 size{1.0f, 1.0f};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const OrthographicProjection2D& other) const = default;
};

/**
 * Projection matrix or configuration of a projection in 2D.
 */
struct Projection2D : Variant<mat3, OrthographicProjection2D> {
	using Variant::Variant;
};

/**
 * Configuration of a world-space view in 2D.
 */
struct WorldView2D {
	/**
	 * World-space position of the view origin.
	 *
	 * \note As the view moves, its viewed contents will appear to move in the
	 *       opposite direction relative to the view. In other words, moving the
	 *       view to the left results in shifting what's seen in the view to the
	 *       right.
	 */
	vec2 position{0.0f, 0.0f};

	/**
	 * World-space orientation of the view around its origin, in radians.
	 *
	 * \note As the view rotates, its viewed contents will appear to rotate in
	 *       the opposite direction relative to the view. In other words,
	 *       rotating the view counter-clockwise (positive angle) results in
	 *       rotating what's seen in the view clockwise.
	 */
	float angle = 0.0f;

	/**
	 * World-space scale of the view.
	 *
	 * \note As the view is scaled, its viewed contents will appear to scale in
	 *       the opposite direction relative to the view. In other words,
	 *       lowering the scale below 1 results in zooming in, while increasing
	 *       the scale above 1 results in zooming out.
	 */
	vec2 scale{1.0f, 1.0f};
};

/**
 * View matrix or configuration of a view in 2D.
 */
struct View2D : Variant<mat3, WorldView2D> {
	using Variant::Variant;
};

/**
 * Configuration options for a Camera2D.
 */
struct Camera2DOptions {
	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Camera2DOptions& other) const noexcept = default;
};

/**
 * Combined view-projection matrix, defining a 2D perspective to render from.
 */
class Camera2D {
public:
	/**
	 * Struct of shader parameters representing the state of a Camera2D.
	 */
	struct Parameters {
		mat3 cameraProjectionMatrix;     ///< Current projection matrix of the camera.
		mat3 cameraViewMatrix;           ///< Current view matrix of the camera.
		mat3 cameraViewProjectionMatrix; ///< Current combined view-projection matrix of the camera.
		vec2 cameraPosition;             ///< Current position of the camera in world space.
		mat2 cameraBasis;                ///< Current right and up direction vectors (local X and Y axes) of the camera.
	};

	/**
	 * Shader buffer for 2D camera parameters.
	 */
	using ParameterBuffer = UniformBuffer<Parameters, "Camera2DParameters">;

	/**
	 * Construct a camera.
	 *
	 * \param device device to create the camera for. Must outlive the camera.
	 * \param projection projection matrix or configuration options for the
	 *        camera projection, see Projection2D.
	 * \param view view matrix or configuration options for the camera view, see
	 *        View2D.
	 * \param options camera options, see Camera3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit Camera2D(Device& device, const Projection2D& projection = mat3{1.0f}, const View2D& view = mat3{1.0f}, const Camera2DOptions& options = {})
		: parameterBuffer(device)
		, options(options) {
		setProjectionAndView(projection, view);
	}

	/**
	 * Set the projection of the camera.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection2D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjection(const Projection2D& newProjection) {
		setProjectionWithoutFlush(newProjection);
		flush();
	}

	/**
	 * Set the view of the camera.
	 *
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View2D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setView(const View2D& newView) {
		setViewWithoutFlush(newView);
		flush();
	}

	/**
	 * Set the configuration options of the camera.
	 *
	 * \param newOptions new configuration options, see Camera2DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setOptions(const Camera2DOptions& newOptions) {
		options = newOptions;
	}

	/**
	 * Set both the projection and view of the camera.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection2D.
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View2D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjectionAndView(const Projection2D& newProjection, const View2D& newView) {
		setProjectionWithoutFlush(newProjection);
		setViewWithoutFlush(newView);
		flush();
	}

	/**
	 * Set both the view and configuration options of the camera.
	 *
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View2D.
	 * \param newOptions new configuration options, see Camera2DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setViewAndOptions(const View2D& newView, const Camera2DOptions& newOptions) {
		setViewWithoutFlush(newView);
		options = newOptions;
		flush();
	}

	/**
	 * Set both the projection and view of the camera, as well as its
	 * configuration options.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection2D.
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View2D.
	 * \param newOptions new configuration options, see Camera2DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjectionAndViewAndOptions(const Projection2D& newProjection, const View2D& newView, const Camera2DOptions& newOptions) {
		setProjectionWithoutFlush(newProjection);
		setViewWithoutFlush(newView);
		options = newOptions;
		flush();
	}

	/**
	 * Get the parameter buffer of the camera.
	 *
	 * \return a read-only reference to the camera's parameter buffer.
	 */
	[[nodiscard]] const ParameterBuffer& getParameterBuffer() const noexcept {
		return parameterBuffer;
	}

	/**
	 * Get the projection matrix of the camera.
	 *
	 * \return a read-only reference to the camera's current projection matrix.
	 */
	[[nodiscard]] const mat3& getProjectionMatrix() const noexcept {
		return projectionMatrix;
	}

	/**
	 * Get the view matrix of the camera.
	 *
	 * \return a read-only reference to the camera's current view matrix.
	 */
	[[nodiscard]] const mat3& getViewMatrix() const noexcept {
		return viewMatrix;
	}

	/**
	 * Get the configuration options of the camera.
	 *
	 * \return the camera's current options.
	 */
	[[nodiscard]] Camera2DOptions getOptions() const noexcept {
		return options;
	}

private:
	void setProjectionWithoutFlush(const Projection2D& newProjection) {
		GREM_MATCH(newProjection) {
			GREM_CASE(const mat3& newProjectionMatrix) {
				projectionMatrix = newProjectionMatrix;
				break;
			}
			GREM_CASE(const OrthographicProjection2D& newOrthographicProjection) {
				projectionMatrix = scale(2.0f / newOrthographicProjection.size) * translate(newOrthographicProjection.size * -0.5f - newOrthographicProjection.offset);
				break;
			}
		}
	}

	void setViewWithoutFlush(const View2D& newView) {
		GREM_MATCH(newView) {
			GREM_CASE(const mat3& newViewMatrix) {
				viewMatrix = newViewMatrix;
				break;
			}
			GREM_CASE(const WorldView2D& newWorldView) {
				const mat2 inverseBasis = transpose(mat2{rotateScale(newWorldView.angle, newWorldView.scale)});
				const vec2 inversePosition = inverseBasis * -newWorldView.position;
				viewMatrix = {
					vec3{inverseBasis[0], 0.0f},
					vec3{inverseBasis[1], 0.0f},
					vec3{inversePosition, 1.0f},
				};
				break;
			}
		}
	}

	void flush() {
		const mat2 basis = transpose(mat2{viewMatrix});
		const vec2 position = basis * -vec2{viewMatrix[2]};
		parameterBuffer.upload(Parameters{
			.cameraProjectionMatrix = projectionMatrix,
			.cameraViewMatrix = viewMatrix,
			.cameraViewProjectionMatrix = projectionMatrix * viewMatrix,
			.cameraPosition = position,
			.cameraBasis = basis,
		});
	}

	ParameterBuffer parameterBuffer;
	mat3 projectionMatrix{1.0f};
	mat3 viewMatrix{1.0f};
	[[no_unique_address]] Camera2DOptions options;
};

} // namespace grem::graphics

#endif
