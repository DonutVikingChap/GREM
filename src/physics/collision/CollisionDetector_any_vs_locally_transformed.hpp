// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_VS_LOCALLY_TRANSFORMED_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_VS_LOCALLY_TRANSFORMED_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Indirect.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"

namespace grem::physics {

// Collision detector for any shape vs LocallyTransformedShape.
template <size_t N>
class CollisionDetector_any_vs_locally_transformed final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const LocallyTransformedShape<N>& locallyTransformedShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<LocallyTransformedShape<N>>();
		const Optional<Box<N>> aabbA = colliderA.shape.getBoundingBox(transformationA);
		const LocalTransformation<N> localTransformationB =
			translateRotateScale(locallyTransformedShapeB.localOffset, locallyTransformedShapeB.localOrientation, locallyTransformedShapeB.localScale);
		const Transformation<N> globalTransformationB = transformationB * localTransformationB;
		const Optional<Box<N>> aabbB = ShapeView<N>{*locallyTransformedShapeB.shape}.getBoundingBox(globalTransformationB);
		if (!aabbA || !aabbB || intersects(*aabbA, *aabbB)) {
			if (!collisionAlgorithm) {
				collisionAlgorithm.emplace(CollisionAlgorithm<N>::chooseImplementation(colliderA.shape, *locallyTransformedShapeB.shape));
			}
			return (*collisionAlgorithm)
			    ->hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA, transformationA, ColliderView<N>{*locallyTransformedShapeB.shape, colliderB.filter},
					globalTransformationB, options, filterTest);
		}
		return {};
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const LocallyTransformedShape<N>& locallyTransformedShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<LocallyTransformedShape<N>>();
		const Optional<Box<N>> aabbA = colliderA.shape.getBoundingBox(transformationA);
		const LocalTransformation<N> localTransformationB =
			translateRotateScale(locallyTransformedShapeB.localOffset, locallyTransformedShapeB.localOrientation, locallyTransformedShapeB.localScale);
		const Transformation<N> globalTransformationB = transformationB * localTransformationB;
		const Optional<Box<N>> aabbB = ShapeView<N>{*locallyTransformedShapeB.shape}.getBoundingBox(globalTransformationB);
		if (!aabbA || !aabbB || intersects(*aabbA, *aabbB)) {
			if (!collisionAlgorithm) {
				collisionAlgorithm.emplace(CollisionAlgorithm<N>::chooseImplementation(colliderA.shape, *locallyTransformedShapeB.shape));
			}
			(*collisionAlgorithm)
				->detectCollisions(temporaryMemoryResource, colliderA, transformationA, ColliderView<N>{*locallyTransformedShapeB.shape, colliderB.filter}, globalTransformationB,
					options, filterTest, [&](const CollisionAlgorithmResult<N>& localCollision) -> void {
						CollisionAlgorithmResult<N> collision = localCollision;
						for (ContactPoint<N>& point : collision.manifold.points) {
							point.offsets.second = locallyTransformedShapeB.localOffset + point.offsets.second;
							point.localOffsets.second = localTransformationB(point.localOffsets.second) - 0;
						}
						callback(collision);
					});
		}
	}

private:
	Optional<Indirect<CollisionAlgorithm<N>>> collisionAlgorithm{};
};

template <size_t N, shape<N> ShapeA>
struct choose_collision_detector<N, ShapeA, LocallyTransformedShape<N>> {
	using type = CollisionDetector_any_vs_locally_transformed<N>;
};

template <size_t N, shape<N> ShapeB>
struct choose_collision_detector<N, LocallyTransformedShape<N>, ShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_vs_locally_transformed<N>>;
};

} // namespace grem::physics

#endif
