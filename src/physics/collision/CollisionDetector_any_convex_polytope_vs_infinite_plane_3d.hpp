// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_INFINITE_PLANE_3D_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_INFINITE_PLANE_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"
#include "convex_polytope_contact.hpp"

namespace grem::physics {

// Collision detector for any convex polytope shape made of flat surfaces VS InfinitePlaneShape3D.
// Uses the deepest face on the shape to the plane to produce a stable contact.
class CollisionDetector_any_convex_polytope_vs_infinite_plane_3d final : public CollisionAlgorithmImplementation3D {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView3D colliderA, const Transformation3D& transformationA,
		ColliderView3D colliderB, const Transformation3D& transformationB, const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		GREM_ASSERT(colliderA.shape.isConvexPolytopeShapeType());
		const ConvexPolytopeShapeView3D convexPolytopeShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Direction3D farLocalDirectionA = inverseTransformationA.getDirection(plane.normal);
		const ConvexPolytopeVertexIndex nearVertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(-farLocalDirectionA, latestNearVertexIndexA);
		const ConvexPolytopeVertexIndex farVertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(farLocalDirectionA, latestFarVertexIndexA);
		latestNearVertexIndexA = nearVertexIndexA;
		latestFarVertexIndexA = farVertexIndexA;
		const Position3D nearPointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(nearVertexIndexA));
		const Position3D farPointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(farVertexIndexA));
		const Length1D nearSignedDistance = dot(nearPointA - plane.point, plane.normal);
		const Length1D farSignedDistance = dot(plane.point - farPointA, plane.normal);
		if (max(nearSignedDistance, farSignedDistance) <= min(-minPenetrationDepth, options.maxCollisionTouchingDistance)) {
			return filterTestResult;
		}
		return {};
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView3D colliderA, const Transformation3D& transformationA, ColliderView3D colliderB,
		const Transformation3D& transformationB, const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult3D& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return;
		}

		GREM_ASSERT(colliderA.shape.isConvexPolytopeShapeType());
		const ConvexPolytopeShapeView3D convexPolytopeShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);
		const InverseTransformation3D inverseTransformationB = inverse(transformationB);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Direction3D farLocalDirectionA = inverseTransformationA.getDirection(plane.normal);
		const ConvexPolytopeVertexIndex nearVertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(-farLocalDirectionA, latestNearVertexIndexA);
		const ConvexPolytopeVertexIndex farVertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(farLocalDirectionA, latestFarVertexIndexA);
		latestNearVertexIndexA = nearVertexIndexA;
		latestFarVertexIndexA = farVertexIndexA;
		const Position3D nearPointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(nearVertexIndexA));
		const Position3D farPointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(farVertexIndexA));
		const Length1D nearSignedDistance = dot(nearPointA - plane.point, plane.normal);
		const Length1D farSignedDistance = dot(plane.point - farPointA, plane.normal);

		const auto [signedDistance, vertexIndexA, normal, localDirectionA] =
			(nearSignedDistance > farSignedDistance)
				? Tuple{nearSignedDistance, nearVertexIndexA, -plane.normal, -farLocalDirectionA}
				: Tuple{farSignedDistance, farVertexIndexA, plane.normal, farLocalDirectionA};
		if (signedDistance <= options.maxCollisionTouchingDistance) {
			Optional<ConvexPolytopeFaceIndex> faceIndexA{};
			Scale1D largestDotProduct = Scale1D::MIN;
			forEachFaceIndexAroundVertex(convexPolytopeShapeA, vertexIndexA, [&](ConvexPolytopeFaceIndex faceIndex) -> void {
				const Scale1D dotProduct = dot(localDirectionA, convexPolytopeShapeA.getLocalFaceNormal(faceIndex));
				if (dotProduct > largestDotProduct) {
					faceIndexA = faceIndex;
					largestDotProduct = dotProduct;
				}
			});

			if (faceIndexA) {
				ArrayList<Position3D, ArenaAllocator<Position3D>> incidentPoints = getFacePoints(temporaryMemoryResource, *faceIndexA, convexPolytopeShapeA, transformationA);
				ArrayList<Position3D, ArenaAllocator<Position3D>> referencePoints = cullPointsAbovePlaneAndProjectOthersOntoIt(temporaryMemoryResource, incidentPoints,
					Plane3D{.point = plane.point, .normal = -normal}, options.maxCollisionTouchingDistance);
				if (!referencePoints.empty()) {
					reducePointsToSimplifiedManifold(referencePoints, incidentPoints, Plane3D{.point = plane.point, .normal = -normal});
					callback(CollisionAlgorithmResult3D{
						.manifold = convertPointsToContactManifold<3>(incidentPoints, inverseTransformationA, ContactFeatureType::CONVEX_POLYTOPE_FACE, *faceIndexA,
							referencePoints, inverseTransformationB, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, normal, filterTestResult),
					});
				}
			}
		}
	}

private:
	ConvexPolytopeVertexIndex latestNearVertexIndexA = 0;
	ConvexPolytopeVertexIndex latestFarVertexIndexA = 0;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeA>
struct choose_collision_detector<N, ConvexPolytopeShapeA, InfinitePlaneShape3D> {
	using type = CollisionDetector_any_convex_polytope_vs_infinite_plane_3d;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeB>
struct choose_collision_detector<N, InfinitePlaneShape3D, ConvexPolytopeShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_polytope_vs_infinite_plane_3d>;
};

} // namespace grem::physics

#endif
