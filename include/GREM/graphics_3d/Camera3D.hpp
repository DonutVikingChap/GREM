// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_3D_CAMERA_3D_HPP
#define GREM_GRAPHICS_3D_CAMERA_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/buffers.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid including Device.hpp.

/**
 * Configuration of an orthographic projection in 3D.
 */
struct OrthographicProjection3D {
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
	 * Distance to the near plane of the projection, in view coordinates.
	 *
	 * If greater than or equal to #farZ, the depth will be infinite.
	 */
	float nearZ = 0.0f;

	/**
	 * Distance to the far plane of the projection, in view coordinates.
	 *
	 * If less than or equal to #nearZ, the depth will be infinite.
	 */
	float farZ = 0.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const OrthographicProjection3D& other) const = default;
};

/**
 * Configuration of a perspective projection in 3D.
 */
struct PerspectiveProjection3D {
	/**
	 * Vertical field of view of the projection, in radians.
	 *
	 * \note The default value corresponds to 90 degrees horizontally at 4:3, or
	 *       ~106 degrees horizontally at 16:9, which is ~74 degrees vertically.
	 *       This value was the default in many classic FPS games and is still
	 *       common today.
	 */
	float verticalFieldOfView = 1.28700221758656877361f;

	/**
	 * Aspect ratio of the projection, X/Y.
	 *
	 * \note This should typically be set to
	 *       `viewport.region.size.getAspectRatio()`.
	 */
	float aspectRatio = 1.0f;

	/**
	 * Distance to the near plane of the projection, in view coordinates.
	 */
	float nearZ = 0.01f;

	/**
	 * Distance to the far plane of the projection, in view coordinates.
	 *
	 * If less than or equal to #nearZ, the depth will be infinite.
	 */
	float farZ = 1000.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const PerspectiveProjection3D& other) const = default;
};

/**
 * Projection matrix or configuration of a projection in 3D.
 */
struct Projection3D : Variant<mat4, OrthographicProjection3D, PerspectiveProjection3D> {
	using Variant::Variant;
};

/**
 * Configuration of a world-space view in 3D.
 */
struct WorldView3D {
	/**
	 * World-space position of the view origin.
	 *
	 * \note As the view moves, its viewed contents will appear to move in the
	 *       opposite direction relative to the view. In other words, moving the
	 *       view to the left results in shifting what's seen in the view to the
	 *       right.
	 */
	vec3 position{0.0f, 0.0f, 0.0f};

	/**
	 * World-space orientation of the view around its origin.
	 *
	 * \note As the view rotates, its viewed contents will appear to rotate in
	 *       the opposite direction relative to the view. In other words,
	 *       rotating the view counter-clockwise results in rotating what's seen
	 *       in the view clockwise.
	 */
	quat orientation{0.0f, 0.0f, 0.0f, 1.0f};

	/**
	 * World-space scale of the view.
	 *
	 * \note As the view is scaled, its viewed contents will appear to scale in
	 *       the opposite direction relative to the view. In other words,
	 *       lowering the scale below 1 results in zooming in, while increasing
	 *       the scale above 1 results in zooming out.
	 */
	vec3 scale{1.0f, 1.0f, 1.0f};
};

/**
 * View matrix or configuration of a view in 3D.
 */
struct View3D : Variant<mat4, WorldView3D> {
	using Variant::Variant;
};

/**
 * Configuration options for a Camera3D.
 */
struct Camera3DOptions {
	/**
	 * Exposure parameter of the camera.
	 */
	float exposure = 1.0f;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Camera3DOptions& other) const noexcept = default;
};

/**
 * Combined view-projection matrix, defining a 3D perspective to render from.
 */
class Camera3D {
public:
	/**
	 * Struct of shader parameters representing the state of a Camera3D.
	 */
	struct Parameters {
		mat4 cameraProjectionMatrix;         ///< Current projection matrix of the camera.
		mat4 cameraViewMatrix;               ///< Current view matrix of the camera.
		mat4 cameraViewProjectionMatrix;     ///< Current combined view-projection matrix of the camera.
		vec2 cameraNearAndFarPlaneDistances; ///< Current near (X) and far (Y) plane distances of the camera.
		vec3 cameraPosition;                 ///< Current position of the camera in world space.
		mat3 cameraBasis;                    ///< Current right, up and backward direction vectors (local X, Y and Z axes) of the camera in world space.
		float cameraExposure;                ///< Current exposure parameter of the camera.
	};

	/**
	 * Shader buffer for 3D camera parameters.
	 */
	using ParameterBuffer = UniformBuffer<Parameters, "Camera3DParameters">;

	/**
	 * Construct a camera.
	 *
	 * \param device device to create the camera for. Must outlive the camera.
	 * \param projection projection matrix or configuration options for the
	 *        camera projection, see Projection3D.
	 * \param view view matrix or configuration options for the camera view, see
	 *        View3D.
	 * \param options camera options, see Camera3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit Camera3D(Device& device, const Projection3D& projection = mat4{1.0f}, const View3D& view = mat4{1.0f}, const Camera3DOptions& options = {})
		: parameterBuffer(device) {
		setProjectionAndViewAndOptions(projection, view, options);
	}

	/**
	 * Set the projection of the camera.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection3D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjection(const Projection3D& newProjection) {
		setProjectionWithoutFlush(newProjection);
		flush();
	}

	/**
	 * Set the view of the camera.
	 *
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View3D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setView(const View3D& newView) {
		setViewWithoutFlush(newView);
		flush();
	}

	/**
	 * Set the configuration options of the camera.
	 *
	 * \param newOptions new configuration options, see Camera3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setOptions(const Camera3DOptions& newOptions) {
		setOptionsWithoutFlush(newOptions);
		flush();
	}

	/**
	 * Set both the projection and view of the camera.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection3D.
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View3D.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjectionAndView(const Projection3D& newProjection, const View3D& newView) {
		setProjectionWithoutFlush(newProjection);
		setViewWithoutFlush(newView);
		flush();
	}

	/**
	 * Set both the view and configuration options of the camera.
	 *
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View3D.
	 * \param newOptions new configuration options, see Camera3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setViewAndOptions(const View3D& newView, const Camera3DOptions& newOptions) {
		setViewWithoutFlush(newView);
		setOptionsWithoutFlush(newOptions);
		flush();
	}

	/**
	 * Set both the projection and view of the camera, as well as its
	 * configuration options.
	 *
	 * \param newProjection new projection matrix or configuration options for
	 *        the camera projection, see Projection3D.
	 * \param newView new view matrix or configuration options for the camera
	 *        view, see View3D.
	 * \param newOptions new configuration options, see Camera3DOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setProjectionAndViewAndOptions(const Projection3D& newProjection, const View3D& newView, const Camera3DOptions& newOptions) {
		setProjectionWithoutFlush(newProjection);
		setViewWithoutFlush(newView);
		setOptionsWithoutFlush(newOptions);
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
	[[nodiscard]] const mat4& getProjectionMatrix() const noexcept {
		return projectionMatrix;
	}

	/**
	 * Get the view matrix of the camera.
	 *
	 * \return a read-only reference to the camera's current view matrix.
	 */
	[[nodiscard]] const mat4& getViewMatrix() const noexcept {
		return viewMatrix;
	}

	/**
	 * Get the configuration options of the camera.
	 *
	 * \return the camera's current options.
	 */
	[[nodiscard]] Camera3DOptions getOptions() const noexcept {
		return options;
	}

private:
	void setProjectionWithoutFlush(const Projection3D& newProjection) {
		GREM_MATCH(newProjection) {
			GREM_CASE(const mat4& newProjectionMatrix) {
				projectionMatrix = newProjectionMatrix;
				break;
			}
			GREM_CASE(const OrthographicProjection3D& newOrthographicProjection) {
				if (newOrthographicProjection.nearZ < newOrthographicProjection.farZ) {
					projectionMatrix = ortho(newOrthographicProjection.offset.x, newOrthographicProjection.offset.x + newOrthographicProjection.size.x,
						newOrthographicProjection.offset.y, newOrthographicProjection.offset.y + newOrthographicProjection.size.y, newOrthographicProjection.nearZ,
						newOrthographicProjection.farZ);
				} else {
					projectionMatrix = ortho(newOrthographicProjection.offset.x, newOrthographicProjection.offset.x + newOrthographicProjection.size.x,
						newOrthographicProjection.offset.y, newOrthographicProjection.offset.y + newOrthographicProjection.size.y);
				}
				break;
			}
			GREM_CASE(const PerspectiveProjection3D& newPerspectiveProjection) {
				if (newPerspectiveProjection.nearZ < newPerspectiveProjection.farZ) {
					projectionMatrix = perspective(newPerspectiveProjection.verticalFieldOfView, newPerspectiveProjection.aspectRatio, newPerspectiveProjection.nearZ,
						newPerspectiveProjection.farZ);
				} else {
					projectionMatrix = infinitePerspective(newPerspectiveProjection.verticalFieldOfView, newPerspectiveProjection.aspectRatio, newPerspectiveProjection.nearZ);
				}
				break;
			}
		}
	}

	void setViewWithoutFlush(const View3D& newView) {
		GREM_MATCH(newView) {
			GREM_CASE(const mat4& newViewMatrix) {
				viewMatrix = newViewMatrix;
				break;
			}
			GREM_CASE(const WorldView3D& newWorldView) {
				const mat3 inverseBasis = transpose(mat3{rotateScale(newWorldView.orientation, newWorldView.scale)});
				const vec3 inversePosition = inverseBasis * -newWorldView.position;
				viewMatrix = {
					vec4{inverseBasis[0], 0.0f},
					vec4{inverseBasis[1], 0.0f},
					vec4{inverseBasis[2], 0.0f},
					vec4{inversePosition, 1.0f},
				};
				break;
			}
		}
	}

	void setOptionsWithoutFlush(const Camera3DOptions& newOptions) {
		options = newOptions;
	}

	void flush() {
		const mat3 basis = transpose(mat3{viewMatrix});
		const vec3 position = basis * -vec3{viewMatrix[3]};

		const float nearZ = projectionMatrix[3][2] / projectionMatrix[2][2];
		const float farZ = (projectionMatrix[2][2] == -1.0f) ? Limits<float>::MAX : projectionMatrix[3][2] / (projectionMatrix[2][2] + 1.0f);

		parameterBuffer.upload(Parameters{
			.cameraProjectionMatrix = projectionMatrix,
			.cameraViewMatrix = viewMatrix,
			.cameraViewProjectionMatrix = projectionMatrix * viewMatrix,
			.cameraNearAndFarPlaneDistances{nearZ, farZ},
			.cameraPosition = position,
			.cameraBasis = basis,
			.cameraExposure = options.exposure,
		});
	}

	ParameterBuffer parameterBuffer;
	mat4 projectionMatrix{1.0f};
	mat4 viewMatrix{1.0f};
	Camera3DOptions options{};
};

} // namespace grem::graphics

#endif
