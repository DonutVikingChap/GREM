// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_INFINITE_HALF_SPACE_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_INFINITE_HALF_SPACE_HPP

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

// Fallback collision detector for any convex shape VS InfiniteHalfSpaceShape.
// Produces the deepest point on the shape to the plane as the contact point.
template <size_t N>
class CollisionDetector_any_convex_vs_infinite_half_space final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView<N> colliderA, const Transformation<N>& transformationA,
		ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		GREM_ASSERT(colliderA.shape.isConvexShapeType());
		const ConvexShapeView<N> convexShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Direction<N> localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const Position<N> pointA = transformationA(convexShapeA.getLocalSupportPointOffset(localDirectionA));

		const Length1D signedDistance = dot(pointA - plane.point, plane.normal);
		if (signedDistance <= min(-minPenetrationDepth, options.maxCollisionTouchingDistance)) {
			return filterTestResult;
		}
		return {};
	}

	void detectCollisions(ArenaResource*, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB,
		const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest, FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return;
		}

		GREM_ASSERT(colliderA.shape.isConvexShapeType());
		const ConvexShapeView<N> convexShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Direction<N> localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const Position<N> pointA = transformationA(convexShapeA.getLocalSupportPointOffset(localDirectionA));

		const Length1D signedDistance = dot(pointA - plane.point, plane.normal);
		if (signedDistance <= options.maxCollisionTouchingDistance) {
			const Position<N> pointB = pointA - plane.normal * signedDistance;
			callback(CollisionAlgorithmResult<N>{
				.manifold = convertPointsToContactManifold(pointA, inverseTransformationA, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, pointB, inverseTransformationB,
					ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, -plane.normal, filterTestResult),
			});
		}
	}
};

template <size_t N, convex_shape<N> ConvexShapeA>
struct choose_collision_detector<N, ConvexShapeA, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_any_convex_vs_infinite_half_space<N>;
};

template <size_t N, convex_shape<N> ConvexShapeB>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, ConvexShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_vs_infinite_half_space<N>>;
};

} // namespace grem::physics

#endif
