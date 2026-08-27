// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_CLIPPING_ALGORITHM_HPP
#define GREM_PHYSICS_COLLISION_CLIPPING_ALGORITHM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include "SutherlandHodgmanAlgorithm.hpp"

namespace grem::physics {

template <size_t N>
class ClippingAlgorithm;

template <>
class ClippingAlgorithm<2> {
public:
	void clipPointsToWithinFace(ArrayList<Position2D, ArenaAllocator<Position2D>>& points, ConvexPolytopeFaceIndex faceIndex, const convex_polytope_shape_2d auto& shape,
		const Transformation2D& transformation) {
		const Length2D localOffsetA = shape.getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
		const Length2D localOffsetB = shape.getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>((faceIndex + 1) % shape.getFaceCount()));
		const Position2D pointA = transformation(localOffsetA);
		const Position2D pointB = transformation(localOffsetB);
		const Direction2D faceNormal = transformation.getDirection(shape.getLocalFaceNormal(faceIndex));
		const Direction2D faceSideNormalA = rotate90DegreesClockwise(faceNormal);
		const Direction2D faceSideNormalB = rotate90DegreesCounterclockwise(faceNormal);
		const Array clippingPlanes{
			Plane2D{.point = pointA, .normal = faceSideNormalA},
			Plane2D{.point = pointB, .normal = faceSideNormalB},
		};
		for (Position2D& point : points) {
			point = clipPointBetweenPlanes2D(point, clippingPlanes);
		}
	}

private:
	[[nodiscard]] Position2D clipPointBetweenPlanes2D(Position2D point, Span<const Plane2D> planes) {
		for (const Plane2D& plane : planes) {
			const Length1D pointOffsetAlongPlaneNormal = dot(point - plane.point, plane.normal);
			if (pointOffsetAlongPlaneNormal > 0) {
				point -= plane.normal * pointOffsetAlongPlaneNormal;
			}
		}
		return point;
	}
};

template <>
class ClippingAlgorithm<3> {
public:
	void clipPointsToWithinFace(ArrayList<Position3D, ArenaAllocator<Position3D>>& points, ConvexPolytopeFaceIndex faceIndex, const convex_polytope_shape_3d auto& shape,
		const Transformation3D& transformation) {
		SutherlandHodgmanAlgorithm<3>{}.clipPointsToWithinFace(points, faceIndex, shape, transformation);
	}
};

} // namespace grem::physics

#endif
