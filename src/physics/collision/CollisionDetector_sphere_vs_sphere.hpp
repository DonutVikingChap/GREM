// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_SPHERE_VS_SPHERE_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_SPHERE_VS_SPHERE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"
#include "CollisionDetector_any_convex_vs_any_convex.hpp"

namespace grem::physics {

// Specialized collision detector for SphereShape VS SphereShape.
// Produces the optimal contact points using the simple analytic formula for sphere-vs-sphere collision if the objects both have uniform scale.
// Falls back to convex vs convex otherwise.
template <size_t N>
class CollisionDetector_sphere_vs_sphere final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const Scale<N> squaredScaleA = transformationA.getBasis().getScale2();
		const Scale<N> squaredScaleB = transformationB.getBasis().getScale2();
		if (isUniform(squaredScaleA) && isUniform(squaredScaleB)) {
			const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
			if (!filterTestResult) {
				return {};
			}

			const SphereShape<N>& sphereShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<SphereShape<N>>();
			const SphereShape<N>& sphereShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<SphereShape<N>>();
			const Length1D scaledRadiusA = sqrt(squaredScaleA.getX()) * sphereShapeA.radius;
			const Length1D scaledRadiusB = sqrt(squaredScaleB.getX()) * sphereShapeB.radius;
			const Length<N> originDifference = transformationB.getOrigin() - transformationA.getOrigin();
			const Distance originDistance = length(originDifference);
			const Distance combinedScaledRadius = scaledRadiusA + scaledRadiusB;
			const Length1D signedDistance = originDistance - combinedScaledRadius;
			if (signedDistance <= min(-minPenetrationDepth, options.maxCollisionTouchingDistance)) {
				return filterTestResult;
			}
			return {};
		}
		return fallback.hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA, transformationA, colliderB, transformationB, options, filterTest);
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const Scale<N> squaredScaleA = transformationA.getBasis().getScale2();
		const Scale<N> squaredScaleB = transformationB.getBasis().getScale2();
		if (isUniform(squaredScaleA) && isUniform(squaredScaleB)) {
			const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
			if (!filterTestResult) {
				return;
			}

			const SphereShape<N>& sphereShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<SphereShape<N>>();
			const SphereShape<N>& sphereShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<SphereShape<N>>();
			const Length1D scaledRadiusA = sqrt(squaredScaleA.getX()) * sphereShapeA.radius;
			const Length1D scaledRadiusB = sqrt(squaredScaleB.getX()) * sphereShapeB.radius;
			const Length<N> originDifference = transformationB.getOrigin() - transformationA.getOrigin();
			const Distance originDistance = length(originDifference);
			const Distance combinedScaledRadius = scaledRadiusA + scaledRadiusB;
			const Length1D signedDistance = originDistance - combinedScaledRadius;
			if (signedDistance <= options.maxCollisionTouchingDistance) {
				const Direction<N> normal = (originDistance > 0) ? Direction<N>::reinterpret(originDifference / originDistance) : Y_AXIS<N>;
				const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
				const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
				const Position<N> pointA = transformationA.getOrigin() + normal * scaledRadiusA;
				const Position<N> pointB = transformationB.getOrigin() + normal * -scaledRadiusB;
				callback(CollisionAlgorithmResult<N>{
					.manifold = convertPointsToContactManifold(pointA, inverseTransformationA, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, pointB, inverseTransformationB,
						ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, normal, filterTestResult),
				});
			}
			return;
		}
		fallback.detectCollisions(temporaryMemoryResource, colliderA, transformationA, colliderB, transformationB, options, filterTest, callback);
	}

private:
	[[nodiscard]] static constexpr bool isUniform(Scale<N> scale) noexcept {
		if constexpr (N == 2) {
			return scale == scale.get(X, X);
		} else if constexpr (N == 3) {
			return scale == scale.get(X, X, X);
		}
	}

	CollisionDetector_any_convex_vs_any_convex<N> fallback{};
};

template <size_t N>
struct choose_collision_detector<N, SphereShape<N>, SphereShape<N>> {
	using type = CollisionDetector_sphere_vs_sphere<N>;
};

} // namespace grem::physics

#endif
