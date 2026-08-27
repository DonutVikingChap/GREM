// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_DEBUG_VISUZLIATION_HPP
#define GREM_PHYSICS_DEBUG_VISUZLIATION_HPP

#include <GREM/build_config.hpp>

#ifdef GREM_PHYSICS_USE_DEBUG_VISUALIZATION
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/resource/Model.hpp>

#include <new>       // std::launder
#include <stdexcept> // std::length_error
#else
#include <GREM/core/fundamentals.hpp>
#endif

namespace grem::physics {

/**
 * Bundle of draw commands for drawing debugging information about a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class DebugVisualization
#ifdef GREM_PHYSICS_USE_DEBUG_VISUALIZATION
{
public:
	/**
	 * Reset the contents of the debug visualization to an empty state.
	 */
	void clear() noexcept {
		worldDrawCommands.clear();
		uiDrawCommands.clear();
		stringData.clear();
	}

	/**
	 * Draw a solid cube onto the world.
	 *
	 * \param transformation world-space transformation of the cube.
	 * \param color color of the cube.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldCube(const mat4& transformation, Color color) requires(N == 3) {
		worldDrawCommands.emplace_back(DrawWorldCubeCommand{.transformation = transformation, .color = color});
	}

	/**
	 * Draw a wireframe cube onto the world.
	 *
	 * \param transformation world-space transformation of the cube.
	 * \param color color of the cube.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldCubeWireframe(const mat4& transformation, Color color) requires(N == 3) {
		worldDrawCommands.emplace_back(DrawWorldCubeWireframeCommand{.transformation = transformation, .color = color});
	}

	/**
	 * Draw an axis-aligned bounding box onto the world as a wireframe.
	 *
	 * \param aabb axis-aligned bounding box to draw.
	 * \param color color of the box.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldAABBWireframe(const Box<N>& aabb, Color color) {
		worldDrawCommands.emplace_back(DrawWorldAABBWireframeCommand{.aabb = aabb, .color = color});
	}

	/**
	 * Draw an axis-aligned bounding box onto the world as a wireframe.
	 *
	 * \param aabb axis-aligned bounding box to draw.
	 * \param color color of the box.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldAABBWireframe(const grem::Box<N, float>& aabb, Color color) {
		drawWorldAABBWireframe(aabb * Box<N>::UNIT, color);
	}

	/**
	 * Draw a point onto the world.
	 *
	 * \param point point to draw.
	 * \param color color of the point.
	 * \param radiusCoefficient optional multiplier for the size of the point.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldPoint(Position<N> point, Color color, Coefficient radiusCoefficient = 1.0f) {
		worldDrawCommands.emplace_back(DrawWorldPointCommand{.point = point, .color = color, .radiusCoefficient = radiusCoefficient});
	}

	/**
	 * Draw a line segment onto the world.
	 *
	 * \param pointA first point of the line segment.
	 * \param pointB second point of the line segment.
	 * \param color color of the point.
	 * \param thicknessCoefficient optional multiplier for the thickness of the
	 *        line.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldLineSegment(Position<N> pointA, Position<N> pointB, Color color, Coefficient thicknessCoefficient = 1.0f) {
		worldDrawCommands.emplace_back(DrawWorldLineSegmentCommand{.pointA = pointA, .pointB = pointB, .color = color, .thicknessCoefficient = thicknessCoefficient});
	}

	/**
	 * Draw a vector onto the world.
	 *
	 * \param point base point of the vector.
	 * \param directionScale scaled direction of the vector.
	 * \param color color of the vector.
	 * \param lengthCoefficient optional multiplier for the length of the
	 *        vector.
	 * \param thicknessCoefficient optional multiplier for the thickness of the
	 *        vector.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawWorldVector(Position<N> point, Scale<N> directionScale, Color color, Coefficient lengthCoefficient = 1.0f, Coefficient thicknessCoefficient = 1.0f) {
		drawWorldLineSegment(point, point + directionScale * 0.25f * METERS * lengthCoefficient, color, thicknessCoefficient);
	}

	/**
	 * Draw some text onto the UI.
	 *
	 * \param position position of the text.
	 * \param characterSize character size of the text.
	 * \param color color of the text.
	 * \param string string of the text.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawUIText(vec2 position, uint32_t characterSize, Color color, UTF8StringView string) {
		if (string.size() > Limits<uint32_t>::MAX || stringData.size() > Limits<uint32_t>::MAX - string.size()) {
			throw std::length_error{"Maximum internal string buffer size exceeded."};
		}
		const uint32_t stringDataOffset = static_cast<uint32_t>(stringData.size());
		const uint32_t stringDataSize = static_cast<uint32_t>(string.size());
		stringData.append(string);
		uiDrawCommands.emplace_back(
			DrawUITextCommand{.position = position, .characterSize = characterSize, .color = color, .stringDataOffset = stringDataOffset, .stringDataSize = stringDataSize});
	}

	/**
	 * Draw some text onto the UI.
	 *
	 * \param position position of the text.
	 * \param characterSize character size of the text.
	 * \param color color of the text.
	 * \param string string of the text.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void drawUIText(vec2 position, uint32_t characterSize, Color color, StringView string) {
		drawUIText(position, characterSize, color, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()});
	}

	/**
	 * Add the world debug visualization to a batch of instances.
	 *
	 * \param renderer2D renderer of the batch.
	 * \param instances batch of instances to add to.
	 * \param lengthUnit length unit of the world space.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putWorldVisualizationInstances(graphics::Renderer2D& renderer2D, graphics::Instances2D& instances, Length2D lengthUnit = Length2D{1.0f * METERS}) const requires(N == 2) {
		(void)renderer2D;
		const Scale2D lengthScale = 1.0f * METERS / lengthUnit;
		for (const WorldDrawCommand& worldDrawCommand : worldDrawCommands) {
			GREM_MATCH(worldDrawCommand) {
				GREM_CASE(const DrawWorldCubeCommand& command) break;
				GREM_CASE(const DrawWorldCubeWireframeCommand& command) break;
				GREM_CASE(const DrawWorldAABBWireframeCommand& command) {
					const vec2 aabbMin = lengthScale * command.aabb.min.in(Position2D::UNIT);
					const vec2 aabbMax = lengthScale * command.aabb.max.in(Position2D::UNIT);
					const vec2 aabbSize = aabbMax - aabbMin;
					instances.putRectangleInstance(graphics::RectangleInstance2D{.position{aabbMin.x, aabbMin.y}, .size{aabbSize.x, 1.0f}, .color = command.color});
					instances.putRectangleInstance(graphics::RectangleInstance2D{.position{aabbMin.x, aabbMax.y - 1.0f}, .size{aabbSize.x, 1.0f}, .color = command.color});
					instances.putRectangleInstance(graphics::RectangleInstance2D{.position{aabbMin.x, aabbMin.y + 1.0f}, .size{1.0f, aabbSize.y - 2.0f}, .color = command.color});
					instances.putRectangleInstance(
						graphics::RectangleInstance2D{.position{aabbMax.x - 1.0f, aabbMin.y + 1.0f}, .size{1.0f, aabbSize.y - 2.0f}, .color = command.color});
					break;
				}
				GREM_CASE(const DrawWorldPointCommand& command) {
					instances.putRectangleInstance(graphics::RectangleInstance2D{
						.position = lengthScale * command.point.in(Position2D::UNIT),
						.size = vec2{4.0f * command.radiusCoefficient},
						.origin{0.5f, 0.5f},
						.color = command.color,
					});
					break;
				}
				GREM_CASE(const DrawWorldLineSegmentCommand& command) {
					const Length2D difference = command.pointB - command.pointA;
					if (difference != 0) {
						instances.putRectangleInstance(graphics::RectangleInstance2D{
							.position = lengthScale * command.pointA.in(Position2D::UNIT),
							.angle = (-90.0f * DEGREES).as(RADIANS) + getAngle(difference),
							.size{2.0f * command.thicknessCoefficient, length(lengthScale * difference.in(Distance::UNIT))},
							.origin{0.5f, 0.0f},
							.color = command.color,
						});
					}
					break;
				}
			}
		}
	}

	/**
	 * Add the world debug visualization to a batch of instances.
	 *
	 * \param renderer3D renderer of the batch.
	 * \param instances batch of instances to add to.
	 * \param lengthUnit length unit of the world space.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putWorldVisualizationInstances(graphics::Renderer3D& renderer3D, graphics::Instances3D& instances, Length3D lengthUnit = Length3D{1.0f * METERS}) const requires(N == 3) {
		const mat4 lengthScale = scale(vec3{1.0f * METERS / lengthUnit});
		for (const WorldDrawCommand& worldDrawCommand : worldDrawCommands) {
			GREM_MATCH(worldDrawCommand) {
				GREM_CASE(const DrawWorldCubeCommand& command) {
					instances.putShadedModelInstance(renderer3D.getHDRUnlitModel3DShaderPipelineSet(), renderer3D.getCubeModel3D(), lengthScale * command.transformation,
						graphics::ModelInstance3D{.color = command.color});
					break;
				}
				GREM_CASE(const DrawWorldCubeWireframeCommand& command) {
					instances.putShadedModelInstance(renderer3D.getHDRWireframeModel3DShaderPipelineSet(), renderer3D.getCubeModel3D(), lengthScale * command.transformation,
						graphics::ModelInstance3D{.color = command.color});
					break;
				}
				GREM_CASE(const DrawWorldAABBWireframeCommand& command) {
					const Position3D aabbCenter = midpoint(command.aabb.min, command.aabb.max);
					const Length3D aabbHalfExtents = (command.aabb.max - command.aabb.min) * 0.5f;
					const mat4 transformation = translateScale(aabbCenter, aabbHalfExtents.in(Length3D::UNIT));
					instances.putShadedModelInstance(renderer3D.getHDRWireframeModel3DShaderPipelineSet(), renderer3D.getCubeModel3D(), lengthScale * transformation,
						graphics::ModelInstance3D{.color = command.color});
					break;
				}
				GREM_CASE(const DrawWorldPointCommand& command) {
					const mat4 transformation = translateScale(command.point, Scale3D{0.02f * command.radiusCoefficient});
					instances.putShadedModelInstance(renderer3D.getHDRUnlitModel3DShaderPipelineSet(), renderer3D.getCubeModel3D(), lengthScale * transformation,
						graphics::ModelInstance3D{.color = command.color});
					break;
				}
				GREM_CASE(const DrawWorldLineSegmentCommand& command) {
					const Length3D difference = command.pointB - command.pointA;
					if (difference != 0) {
						const Position3D midpoint = command.pointA + difference * 0.5f;
						const mat4 transformation = translateRotateScale(midpoint, rotation(vec3{0.0f, 0.0f, 1.0f}, vec3{normalize(difference.in(Length3D::UNIT))}),
							Scale3D{0.01f * command.thicknessCoefficient, 0.01f * command.thicknessCoefficient, 0.5f * length(difference).in(Distance::UNIT)});
						instances.putShadedModelInstance(renderer3D.getHDRUnlitModel3DShaderPipelineSet(), renderer3D.getCubeModel3D(), lengthScale * transformation,
							graphics::ModelInstance3D{.color = command.color});
					}
					break;
				}
			}
		}
	}

	/**
	 * Add the UI debug visualization to a batch of instances.
	 *
	 * \param renderer2D renderer of the batch.
	 * \param instances batch of instances to add to.
	 * \param font font to use for text rendering.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void putUIVisualizationInstances(graphics::Renderer2D& renderer2D, graphics::Instances2D& instances, graphics::Font2D& font) const {
		(void)renderer2D;
		for (const UIDrawCommand& uiDrawCommand : uiDrawCommands) {
			GREM_MATCH(uiDrawCommand) {
				GREM_CASE(const DrawUITextCommand& command) {
					instances.putTextStringInstance(font, UTF8StringView{stringData}.substr(command.stringDataOffset, command.stringDataSize),
						graphics::TextStringInstance2D{.characterSize = command.characterSize, .position = command.position, .color = command.color});
					break;
				}
			}
		}
	}

private:
	struct DrawWorldCubeCommand {
		mat4 transformation;
		Color color;
	};

	struct DrawWorldCubeWireframeCommand {
		mat4 transformation;
		Color color;
	};

	struct DrawWorldAABBWireframeCommand {
		Box<N> aabb;
		Color color;
	};

	struct DrawWorldPointCommand {
		Position<N> point;
		Color color;
		Coefficient radiusCoefficient;
	};

	struct DrawWorldLineSegmentCommand {
		Position<N> pointA;
		Position<N> pointB;
		Color color;
		Coefficient thicknessCoefficient;
	};

	struct DrawUITextCommand {
		vec2 position;
		uint32_t characterSize;
		Color color;
		uint32_t stringDataOffset;
		uint32_t stringDataSize;
	};

	using WorldDrawCommand = Variant<  //
		DrawWorldCubeCommand,          //
		DrawWorldCubeWireframeCommand, //
		DrawWorldAABBWireframeCommand, //
		DrawWorldPointCommand,         //
		DrawWorldLineSegmentCommand>;
	using UIDrawCommand = Variant< //
		DrawUITextCommand>;

	ArrayList<WorldDrawCommand> worldDrawCommands{};
	ArrayList<UIDrawCommand> uiDrawCommands{};
	UTF8String stringData{};
}
#endif
;

using DebugVisualization2D = DebugVisualization<2>; ///< Bundle of draw commands for drawing debugging information about a 2-dimensional Simulation.
using DebugVisualization3D = DebugVisualization<3>; ///< Bundle of draw commands for drawing debugging information about a 3-dimensional Simulation.

} // namespace grem::physics

#endif
