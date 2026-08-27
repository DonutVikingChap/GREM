// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_VS_COMPOUND_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_VS_COMPOUND_HPP

#include <GREM/build_config.hpp>

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

#include <utility> // std::move

namespace grem::physics {

// Collision detector for any shape vs CompoundColliderShape.
template <size_t N>
class CollisionDetector_any_vs_compound final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		if (!filterTest(colliderA.filter, colliderB.filter)) {
			return {};
		}

		updateSubContacts(colliderA.shape, transformationA, colliderB.shape, transformationB);

		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		for (SubContact& subContact : subContacts) {
			const SubCollider<N>& subColliderB = subCollidersB[subContact.subColliderIndexB];
			const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			if (const CollisionFilterTestResult filterTestResult = subContact.collisionAlgorithm.hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA,
					transformationA, subColliderB.collider, globalTransformationB, options, filterTest)) {
				return filterTestResult;
			}
		}
		return {};
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		if (!filterTest(colliderA.filter, colliderB.filter)) {
			return;
		}

		updateSubContacts(colliderA.shape, transformationA, colliderB.shape, transformationB);

		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		for (SubContact& subContact : subContacts) {
			const SubCollider<N>& subColliderB = subCollidersB[subContact.subColliderIndexB];
			const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			subContact.collisionAlgorithm.detectCollisions(temporaryMemoryResource, colliderA, transformationA, subColliderB.collider, globalTransformationB, options, filterTest,
				[&](const CollisionAlgorithmResult<N>& subCollision) -> void {
					CollisionAlgorithmResult<N> collision{.manifold = subCollision.manifold};
					for (ContactPoint<N>& point : collision.manifold.points) {
						point.offsets.second = subColliderB.localOffset + point.offsets.second;
						point.localOffsets.second = localTransformationB(point.localOffsets.second) - 0;
					}
					callback(collision);
				});
		}
	}

private:
	struct SubContact {
		size_t subColliderIndexB;
		CollisionAlgorithm<N> collisionAlgorithm;
	};

	void updateSubContacts(ShapeView<N> shapeA, const Transformation<N>& transformationA, ShapeView<N> shapeB, const Transformation<N>& transformationB) {
		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(shapeB).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		const Optional<Box<N>> aabbA = shapeA.getBoundingBox(transformationA);
		newSubContacts.clear();
		size_t subContactIndex = 0;
		for (size_t subColliderIndexB = 0; subColliderIndexB < subCollidersB.size(); ++subColliderIndexB) {
			const SubCollider<N>& subColliderB = subCollidersB[subColliderIndexB];
			const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			const Optional<Box<N>> aabbB = ShapeView<N>{subColliderB.collider.shape}.getBoundingBox(globalTransformationB);
			if (!aabbA || !aabbB || intersects(*aabbA, *aabbB)) {
				if (subContactIndex < subContacts.size() && subContacts[subContactIndex].subColliderIndexB == subColliderIndexB) {
					newSubContacts.push_back(SubContact{
						.subColliderIndexB = subColliderIndexB,
						.collisionAlgorithm = std::move(subContacts[subContactIndex].collisionAlgorithm),
					});
					++subContactIndex;
				} else {
					newSubContacts.push_back(SubContact{
						.subColliderIndexB = subColliderIndexB,
						.collisionAlgorithm = CollisionAlgorithm<N>::chooseImplementation(shapeA, subColliderB.collider.shape),
					});
				}
			} else if (subContactIndex < subContacts.size() && subContacts[subContactIndex].subColliderIndexB == subColliderIndexB) {
				++subContactIndex;
			}
		}
		subContacts.swap(newSubContacts);
	}

	ArrayList<SubContact> subContacts{};
	ArrayList<SubContact> newSubContacts{};
};

template <size_t N, shape<N> ShapeA>
struct choose_collision_detector<N, ShapeA, CompoundColliderShape<N>> {
	using type = CollisionDetector_any_vs_compound<N>;
};

template <size_t N, shape<N> ShapeB>
struct choose_collision_detector<N, CompoundColliderShape<N>, ShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_vs_compound<N>>;
};

} // namespace grem::physics

#endif
