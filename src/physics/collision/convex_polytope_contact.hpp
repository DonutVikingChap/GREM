// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_CONVEX_POLYTOPE_CONTACT_HPP
#define GREM_PHYSICS_COLLISION_CONVEX_POLYTOPE_CONTACT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "ClippingAlgorithm.hpp"
#include "CollisionDetector.hpp"

#include <utility> // std::move

namespace grem::physics {

template <size_t N>
[[nodiscard]] inline ArrayList<Position<N>, ArenaAllocator<Position<N>>> cullPointsAbovePlaneAndProjectOthersOntoIt(ArenaResource* memoryResource,
	ArrayList<Position<N>, ArenaAllocator<Position<N>>>& pointsToCull, const Plane<N>& plane, Distance maxDistance) {
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> result{memoryResource};
	result.reserve(pointsToCull.size());
	for (auto it = pointsToCull.begin(); it != pointsToCull.end(); ++it) {
		const Position<N> point = *it;
		const Length1D pointAlongNormal = dot(point - plane.point, plane.normal);
		if (pointAlongNormal > maxDistance) {
			for (auto next = it; ++next != pointsToCull.end();) {
				const Position<N> point = *next;
				const Length1D pointAlongNormal = dot(point - plane.point, plane.normal);
				if (pointAlongNormal <= maxDistance) {
					result.push_back(point - plane.normal * pointAlongNormal);
					*it++ = point;
				}
			}
			pointsToCull.erase(it, pointsToCull.end());
			break;
		}
		result.push_back(point - plane.normal * pointAlongNormal);
	}
	return result;
}

template <size_t N>
inline void reducePointsToSimplifiedManifold(ArrayList<Position<N>, ArenaAllocator<Position<N>>>& referencePoints,
	ArrayList<Position<N>, ArenaAllocator<Position<N>>>& incidentPoints, const Plane<N>& plane) {
	GREM_ASSERT(referencePoints.size() == incidentPoints.size());
	const size_t pointCount = referencePoints.size();
	GREM_ASSERT(pointCount > 0);

	if constexpr (N == 3) {
		// Find the combination of 4 contact points with the largest area that doesn't destabilize the manifold.

		// Choose the deepest point as the first point.
		size_t pointIndexA = 0;
		Length1D deepestPenetrationDepth{};
		for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
			const Length1D penetrationDepth = dot(plane.point - incidentPoints[pointIndex], plane.normal);
			if (penetrationDepth > deepestPenetrationDepth) {
				pointIndexA = pointIndex;
				deepestPenetrationDepth = penetrationDepth;
			}
		}
		const Position3D referencePointA = referencePoints[pointIndexA];
		const Position3D incidentPointA = incidentPoints[pointIndexA];

		// Find the point with the largest square distance to the first point.
		size_t pointIndexB = pointIndexA;
		SquaredDistance largestSquareDistance{};
		for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
			if (pointIndex == pointIndexA) {
				continue;
			}
			const Position3D referencePointB = referencePoints[pointIndex];
			const SquaredDistance squareDistance = distance2(referencePointA, referencePointB);
			if (squareDistance > largestSquareDistance) {
				pointIndexB = pointIndex;
				largestSquareDistance = squareDistance;
			}
		}
		const Position3D referencePointB = referencePoints[pointIndexB];
		const Position3D incidentPointB = incidentPoints[pointIndexB];

		// Find the point that forms the largest triangle area with the previous two points and has counter-clockwise winding order.
		size_t pointIndexC = pointIndexB;
		Area maxSignedDoubleTriangleArea{};
		for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
			if (pointIndex == pointIndexA || pointIndex == pointIndexB) {
				continue;
			}
			const Position3D referencePointC = referencePoints[pointIndex];
			const Area signedDoubleTriangleArea = dot(cross(referencePointA - referencePointC, referencePointB - referencePointC), plane.normal);
			if (signedDoubleTriangleArea > maxSignedDoubleTriangleArea) {
				pointIndexC = pointIndex;
				maxSignedDoubleTriangleArea = signedDoubleTriangleArea;
			}
		}
		const Position3D referencePointC = referencePoints[pointIndexC];
		const Position3D incidentPointC = incidentPoints[pointIndexC];

		// Find the point that forms the largest triangle area with two of the previous three points and has clockwise winding order.
		size_t pointIndexD = pointIndexC;
		Area minSignedDoubleTriangleArea{};
		for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
			if (pointIndex == pointIndexA || pointIndex == pointIndexB || pointIndex == pointIndexC) {
				continue;
			}
			const Position3D referencePointD = referencePoints[pointIndex];
			const Area abd = dot(cross(referencePointA - referencePointD, referencePointB - referencePointD), plane.normal);
			const Area bcd = dot(cross(referencePointB - referencePointD, referencePointC - referencePointD), plane.normal);
			const Area cad = dot(cross(referencePointC - referencePointD, referencePointA - referencePointD), plane.normal);
			const Area signedDoubleTriangleArea = min(min(abd, bcd), cad);
			if (signedDoubleTriangleArea < minSignedDoubleTriangleArea) {
				pointIndexD = pointIndex;
				minSignedDoubleTriangleArea = signedDoubleTriangleArea;
			}
		}
		const Position3D referencePointD = referencePoints[pointIndexD];
		const Position3D incidentPointD = incidentPoints[pointIndexD];

		// Replace the existing points with our new reduced set of points.
		referencePoints.clear();
		incidentPoints.clear();
		referencePoints.push_back(referencePointA);
		incidentPoints.push_back(incidentPointA);
		if (pointIndexB != pointIndexA) {
			referencePoints.push_back(referencePointB);
			incidentPoints.push_back(incidentPointB);
		}
		if (pointIndexC != pointIndexB && pointIndexC != pointIndexA) {
			referencePoints.push_back(referencePointC);
			incidentPoints.push_back(incidentPointC);
		}
		if (pointIndexD != pointIndexC && pointIndexD != pointIndexB && pointIndexD != pointIndexA) {
			referencePoints.push_back(referencePointD);
			incidentPoints.push_back(incidentPointD);
		}
	}
}

template <size_t N>
struct FaceContactManifoldPoints {
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> referencePoints{};
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> incidentPoints{};
};

template <size_t N>
[[nodiscard]] inline Optional<FaceContactManifoldPoints<N>> generateFaceContactManifoldPoints(ArenaResource* memoryResource, const convex_polytope_shape<N> auto& referenceShape,
	const Transformation<N>& referenceTransformation, ConvexPolytopeFaceIndex referenceFaceIndex, Direction<N> referenceNormal, const convex_polytope_shape<N> auto& incidentShape,
	const Transformation<N>& incidentTransformation, ConvexPolytopeFaceIndex& latestIncidentFaceIndex, const CollisionAlgorithmOptions<N>& options) {
	// Find the incident face as the most anti-parallel face on the incident shape with respect to the reference face.
	const ConvexPolytopeFaceIndex incidentFaceIndex =
		incidentShape.getFaceIndexWithMostFittingLocalNormal(inverse(incidentTransformation).getDirection(-referenceNormal), latestIncidentFaceIndex);
	latestIncidentFaceIndex = incidentFaceIndex;

	// Get the points of the incident face.
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> incidentPoints = getFacePoints(memoryResource, incidentFaceIndex, incidentShape, incidentTransformation);

	// Clip the points to the reference face.
	ClippingAlgorithm<N>{}.clipPointsToWithinFace(incidentPoints, referenceFaceIndex, referenceShape, referenceTransformation);

	// Cull incident points and generate reference points.
	const Position<N> referencePoint = referenceTransformation(referenceShape.getLocalFaceOffset(referenceFaceIndex));
	const Plane<N> referencePlane{.point = referencePoint, .normal = referenceNormal};
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> referencePoints =
		cullPointsAbovePlaneAndProjectOthersOntoIt(memoryResource, incidentPoints, referencePlane, options.maxCollisionTouchingDistance);
	if (referencePoints.empty()) {
		return {};
	}

	// Simplify manifold.
	reducePointsToSimplifiedManifold(referencePoints, incidentPoints, referencePlane);

	return FaceContactManifoldPoints<N>{.referencePoints = std::move(referencePoints), .incidentPoints = std::move(incidentPoints)};
}

template <size_t N>
[[nodiscard]] inline Optional<ContactManifold<N>> createFaceContact(ArenaResource* temporaryMemoryResource, Pair<ConvexPolytopeFaceIndex>& latestIncidentFaceIndices,
	const convex_polytope_shape<N> auto& shapeA, const Transformation<N>& transformationA, const InverseTransformation<N>& inverseTransformationA,
	ConvexPolytopeFaceIndex faceIndexA, Direction<N> normalA, Length1D penetrationDepthA, const convex_polytope_shape<N> auto& shapeB, const Transformation<N>& transformationB,
	const InverseTransformation<N>& inverseTransformationB, ConvexPolytopeFaceIndex faceIndexB, Direction<N> normalB, Length1D penetrationDepthB,
	const CollisionAlgorithmOptions<N>& options, CollisionFilterTestResult filterTestResult) {
	if (penetrationDepthA > penetrationDepthB - options.biasReferenceFaceBOverA) {
		// The reference face is B.
		if (const Optional<FaceContactManifoldPoints<N>> faceContactManifoldPoints = generateFaceContactManifoldPoints(temporaryMemoryResource, shapeB, transformationB, faceIndexB,
				normalB, shapeA, transformationA, latestIncidentFaceIndices.first, options)) {
			return convertPointsToContactManifold<N>(faceContactManifoldPoints->incidentPoints, inverseTransformationA, ContactFeatureType::CONVEX_POLYTOPE_FACE, faceIndexA,
				faceContactManifoldPoints->referencePoints, inverseTransformationB, ContactFeatureType::CONVEX_POLYTOPE_FACE, faceIndexB, -normalB, filterTestResult);
		}
	} else {
		// The reference face is A.
		if (const Optional<FaceContactManifoldPoints<N>> faceContactManifoldPoints = generateFaceContactManifoldPoints(temporaryMemoryResource, shapeA, transformationA, faceIndexA,
				normalA, shapeB, transformationB, latestIncidentFaceIndices.second, options)) {
			return convertPointsToContactManifold<N>(faceContactManifoldPoints->referencePoints, inverseTransformationA, ContactFeatureType::CONVEX_POLYTOPE_FACE, faceIndexA,
				faceContactManifoldPoints->incidentPoints, inverseTransformationB, ContactFeatureType::CONVEX_POLYTOPE_FACE, faceIndexB, normalA, filterTestResult);
		}
	}
	return {};
}

[[nodiscard]] inline Optional<ContactManifold3D> createEdgeContact(const convex_polytope_shape_3d auto& shapeA, const Transformation3D& transformationA,
	const InverseTransformation3D& inverseTransformationA, ConvexPolytopeEdgeIndex edgeIndexA, const convex_polytope_shape_3d auto& shapeB, const Transformation3D& transformationB,
	const InverseTransformation3D& inverseTransformationB, ConvexPolytopeEdgeIndex edgeIndexB, Direction3D normal, CollisionFilterTestResult filterTestResult) {
	const Length3D localEdgeOriginA = shapeA.getLocalVertexOffset(shapeA.getFirstVertexIndexOfEdge(edgeIndexA));
	const Length3D localEdgeTargetA = shapeA.getLocalVertexOffset(shapeA.getFirstVertexIndexOfEdge(edgeIndexA ^ 1));
	const Length3D localEdgeOriginB = shapeB.getLocalVertexOffset(shapeB.getFirstVertexIndexOfEdge(edgeIndexB));
	const Length3D localEdgeTargetB = shapeB.getLocalVertexOffset(shapeB.getFirstVertexIndexOfEdge(edgeIndexB ^ 1));
	const Position3D originA = transformationA(localEdgeOriginA);
	const Position3D originB = transformationB(localEdgeOriginB);
	const Position3D destinationA = transformationA(localEdgeTargetA);
	const Position3D destinationB = transformationB(localEdgeTargetB);
	const Length3D vectorA = destinationA - originA;
	const Length3D vectorB = destinationB - originB;
	const SquaredLength lengthSquaredA = length2(vectorA);
	const SquaredLength lengthSquaredB = length2(vectorB);
	if (lengthSquaredA < SquaredLength::MACHINE_EPSILON || lengthSquaredB < SquaredLength::MACHINE_EPSILON) {
		return {};
	}

	const SquaredLength dotProduct = dot(vectorA, vectorB);
	const Length3D originDifference = originA - originB;
	const SquaredLength originDifferenceDotA = dot(originDifference, vectorA);
	const SquaredLength originDifferenceDotB = dot(originDifference, vectorB);
	const SquaredArea denominatorA = lengthSquaredA * lengthSquaredB - dotProduct * dotProduct;
	if (abs(denominatorA) < SquaredArea::MACHINE_EPSILON) {
		return {};
	}

	const Coefficient amountA = (originDifferenceDotB * dotProduct - originDifferenceDotA * lengthSquaredB) / denominatorA;
	const Coefficient amountB = (originDifferenceDotB + dotProduct * amountA) / lengthSquaredB;
	const Position3D pointA = originA + vectorA * clamp(amountA, Coefficient{}, Coefficient{1});
	const Position3D pointB = originB + vectorB * clamp(amountB, Coefficient{}, Coefficient{1});
	return convertPointsToContactManifold(pointA, inverseTransformationA, ContactFeatureType::CONVEX_POLYTOPE_EDGE, edgeIndexA, pointB, inverseTransformationB,
		ContactFeatureType::CONVEX_POLYTOPE_EDGE, edgeIndexB, normal, filterTestResult);
}

} // namespace grem::physics

#endif
