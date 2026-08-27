// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_INFINITE_PLANE_3D_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_INFINITE_PLANE_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"

namespace grem::physics {

// Fallback collision detector for any convex shape VS InfinitePlaneShape3D.
// Produces the deepest point on the shape to the plane as the contact point.
class CollisionDetector_any_convex_vs_infinite_plane_3d final : public CollisionAlgorithmImplementation3D {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView3D colliderA, const Transformation3D& transformationA,
		ColliderView3D colliderB, const Transformation3D& transformationB, const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		GREM_ASSERT(colliderA.shape.isConvexShapeType());
		const ConvexShapeView3D convexShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Direction3D localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const Position3D nearPointA = transformationA(convexShapeA.getLocalSupportPointOffset(localDirectionA));
		const Position3D farPointA = transformationA(convexShapeA.getLocalSupportPointOffset(-localDirectionA));
		const Length1D nearSignedDistance = dot(nearPointA - plane.point, plane.normal);
		const Length1D farSignedDistance = dot(plane.point - farPointA, plane.normal);
		if (max(nearSignedDistance, farSignedDistance) <= min(-minPenetrationDepth, options.maxCollisionTouchingDistance)) {
			return filterTestResult;
		}
		return {};
	}

	void detectCollisions(ArenaResource*, ColliderView3D colliderA, const Transformation3D& transformationA, ColliderView3D colliderB, const Transformation3D& transformationB,
		const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest, FunctionView<void(const CollisionAlgorithmResult3D& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return;
		}

		GREM_ASSERT(colliderA.shape.isConvexShapeType());
		const ConvexShapeView3D convexShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);
		const InverseTransformation3D inverseTransformationB = inverse(transformationB);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Direction3D localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const Position3D nearPointA = transformationA(convexShapeA.getLocalSupportPointOffset(localDirectionA));
		const Position3D farPointA = transformationA(convexShapeA.getLocalSupportPointOffset(-localDirectionA));
		const Length1D nearSignedDistance = dot(nearPointA - plane.point, plane.normal);
		const Length1D farSignedDistance = dot(plane.point - farPointA, plane.normal);

		const auto [signedDistance, pointA, normal] =
			(nearSignedDistance > farSignedDistance) ? Tuple{nearSignedDistance, nearPointA, -plane.normal} : Tuple{farSignedDistance, farPointA, plane.normal};
		if (signedDistance <= options.maxCollisionTouchingDistance) {
			const Position3D pointB = pointA + normal * signedDistance;
			callback(CollisionAlgorithmResult3D{
				.manifold = convertPointsToContactManifold(pointA, inverseTransformationA, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, pointB, inverseTransformationB,
					ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, normal, filterTestResult),
			});
		}
	}
};

template <size_t N, convex_shape<N> ConvexShapeA>
struct choose_collision_detector<N, ConvexShapeA, InfinitePlaneShape3D> {
	using type = CollisionDetector_any_convex_vs_infinite_plane_3d;
};

template <size_t N, convex_shape<N> ConvexShapeB>
struct choose_collision_detector<N, InfinitePlaneShape3D, ConvexShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_vs_infinite_plane_3d>;
};

} // namespace grem::physics

#endif
