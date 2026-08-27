// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_ANY_CONVEX_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_ANY_CONVEX_HPP

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
#include "ExpandingPolytopeAlgorithm.hpp"
#include "GilbertJohnsonKeerthiAlgorithm.hpp"

namespace grem::physics {

// Fallback collision detector that supports any pair of convex shapes.
// Uses single-point GJK+EPA, which may result in poor contact stability, but is fine for rounded shapes.
template <size_t N>
class CollisionDetector_any_convex_vs_any_convex final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		const ConvexShapeView<N> convexShapeA{colliderA.shape};
		const ConvexShapeView<N> convexShapeB{colliderB.shape};

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const Length1D maxCollisionDistance = min(-minPenetrationDepth, options.maxCollisionTouchingDistance);
		const Distance margin = max(options.collisionDistanceErrorTolerance, maxCollisionDistance);
		const typename GJK::FindAnyIntersectionResult gjkResult = GJK{}.findAnyIntersection(convexShapeA, transformationA, inverseTransformationA, convexShapeB, transformationB,
			inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxGJKIterationCount);
		if (gjkResult.foundIntersection) {
			if (maxCollisionDistance == 0) {
				return filterTestResult;
			}
			const typename EPA::FindDeepestPenetrationResult epaResult = EPA{}.findDeepestPenetration(temporaryMemoryResource, Span{gjkResult.simplex}.template first<N + 1>(),
				convexShapeA, transformationA, inverseTransformationA, convexShapeB, transformationB, inverseTransformationB, margin, options.collisionDistanceErrorTolerance,
				options.maxEPAIterationCount);
			if (epaResult.depth >= -maxCollisionDistance) {
				return filterTestResult;
			}
		}
		return {};
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return;
		}

		const ConvexShapeView<N> convexShapeA{colliderA.shape};
		const ConvexShapeView<N> convexShapeB{colliderB.shape};

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const Distance margin = max(options.collisionDistanceErrorTolerance, options.maxCollisionTouchingDistance);
		const typename GJK::FindAnyIntersectionResult gjkResult = GJK{}.findAnyIntersection(convexShapeA, transformationA, inverseTransformationA, convexShapeB, transformationB,
			inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxGJKIterationCount);
		if (gjkResult.foundIntersection) {
			const typename EPA::FindDeepestPenetrationResult epaResult = EPA{}.findDeepestPenetration(temporaryMemoryResource, Span{gjkResult.simplex}.template first<N + 1>(),
				convexShapeA, transformationA, inverseTransformationA, convexShapeB, transformationB, inverseTransformationB, margin, options.collisionDistanceErrorTolerance,
				options.maxEPAIterationCount);
			if (epaResult.depth >= -options.maxCollisionTouchingDistance) {
				callback(CollisionAlgorithmResult<N>{
					.manifold = convertPointsToContactManifold(epaResult.witnessPoints.first, inverseTransformationA, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0,
						epaResult.witnessPoints.second, inverseTransformationB, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, epaResult.normal, filterTestResult),
				});
			}
		}
	}

private:
	using GJK = GilbertJohnsonKeerthiAlgorithm<N>;
	using EPA = ExpandingPolytopeAlgorithm<N>;
};

template <size_t N, convex_shape<N> ConvexShapeA, convex_shape<N> ConvexShapeB>
struct choose_collision_detector<N, ConvexShapeA, ConvexShapeB> {
	using type = CollisionDetector_any_convex_vs_any_convex<N>;
};

} // namespace grem::physics

#endif
