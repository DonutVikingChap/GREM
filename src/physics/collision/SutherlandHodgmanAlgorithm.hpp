// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_SUTHERLAND_HODGMAN_ALGORITHM_HPP
#define GREM_PHYSICS_COLLISION_SUTHERLAND_HODGMAN_ALGORITHM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

template <size_t N>
class SutherlandHodgmanAlgorithm {
public:
	void clipPointsToWithinShape(ArrayList<Position2D, ArenaAllocator<Position2D>>& points, const convex_polytope_shape_2d auto& shape, const Transformation2D& transformation)
		requires(N == 2) {
		GREM_PROFILE_FUNCTION();

		ArrayList<Position2D, ArenaAllocator<Position2D>> input{points.get_allocator()};
		input.reserve(points.size());
		const ConvexPolytopeFaceIndex faceCount = shape.getFaceCount();
		for (ConvexPolytopeFaceIndex faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			input.swap(points);
			points.clear();

			if (!input.empty()) {
				const Plane2D clippingPlane{
					.point = transformation(shape.getLocalFaceOffset(faceIndex)),
					.normal = transformation.getDirection(shape.getLocalFaceNormal(faceIndex)),
				};

				Position2D previousPoint = input.back();
				bool previousPointIsBehindPlane = signbit(dot(previousPoint - clippingPlane.point, clippingPlane.normal));
				for (const Position2D currentPoint : input) {
					const bool currentPointIsBehindPlane = signbit(dot(currentPoint - clippingPlane.point, clippingPlane.normal));
					if (currentPointIsBehindPlane) {
						if (!previousPointIsBehindPlane) {
							points.push_back(getIntersectionPoint(LineSegment2D{previousPoint, currentPoint}, clippingPlane));
						}
						points.push_back(currentPoint);
					} else if (previousPointIsBehindPlane) {
						points.push_back(getIntersectionPoint(LineSegment2D{previousPoint, currentPoint}, clippingPlane));
					}
					previousPoint = currentPoint;
					previousPointIsBehindPlane = currentPointIsBehindPlane;
				}
			}
		}
	}

	void clipPointsToWithinFace(ArrayList<Position3D, ArenaAllocator<Position3D>>& points, ConvexPolytopeFaceIndex faceIndex, const convex_polytope_shape_3d auto& shape,
		const Transformation3D& transformation) requires(N == 3) {
		GREM_PROFILE_FUNCTION();

		ArrayList<Position3D, ArenaAllocator<Position3D>> input{points.get_allocator()};
		input.reserve(points.size());
		const Direction3D localFaceNormal = shape.getLocalFaceNormal(faceIndex);
		const ConvexPolytopeEdgeIndex firstEdgeIndex = shape.getFirstEdgeIndexOfFace(faceIndex);
		ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
		do {
			input.swap(points);
			points.clear();

			if (!input.empty()) {
				const Length3D localEdgeOrigin = shape.getLocalVertexOffset(shape.getFirstVertexIndexOfEdge(edgeIndex));
				const Length3D localEdgeTarget = shape.getLocalVertexOffset(shape.getFirstVertexIndexOfEdge(static_cast<ConvexPolytopeEdgeIndex>(edgeIndex ^ 1)));
				const Position3D edgeOrigin = transformation(localEdgeOrigin);
				const Plane3D clippingPlane{.point = edgeOrigin, .normal = transformation.getDirection(normalize(cross(localEdgeTarget - localEdgeOrigin, localFaceNormal)))};

				Position3D previousPoint = input.back();
				bool previousPointIsBehindPlane = signbit(dot(previousPoint - clippingPlane.point, clippingPlane.normal));
				for (const Position3D currentPoint : input) {
					const bool currentPointIsBehindPlane = signbit(dot(currentPoint - clippingPlane.point, clippingPlane.normal));
					if (currentPointIsBehindPlane) {
						if (!previousPointIsBehindPlane) {
							points.push_back(getIntersectionPoint(LineSegment3D{previousPoint, currentPoint}, clippingPlane));
						}
						points.push_back(currentPoint);
					} else if (previousPointIsBehindPlane) {
						points.push_back(getIntersectionPoint(LineSegment3D{previousPoint, currentPoint}, clippingPlane));
					}
					previousPoint = currentPoint;
					previousPointIsBehindPlane = currentPointIsBehindPlane;
				}
			}

			edgeIndex = shape.getNextEdgeIndex(edgeIndex);
		} while (edgeIndex != firstEdgeIndex);
	}

private:
	[[nodiscard]] static Position<N> getIntersectionPoint(const LineSegment<N>& lineSegment, const Plane<N>& plane) {
		const Length<N> ab = lineSegment.pointB - lineSegment.pointA;
		const Length1D abAlongNormal = dot(ab, plane.normal);
		if (abAlongNormal == 0) {
			return lineSegment.pointA;
		}
		return lineSegment.pointA - (dot(lineSegment.pointA - plane.point, plane.normal) / abAlongNormal) * ab;
	}
};

} // namespace grem::physics

#endif
