// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_COMPOUND_VS_COMPOUND_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_COMPOUND_VS_COMPOUND_HPP

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

// Collision detector for CompoundColliderShape VS CompoundColliderShape.
template <size_t N>
class CollisionDetector_compound_vs_compound final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		updateSubContacts(colliderA.shape, transformationA, colliderB.shape, transformationB);

		const CompoundColliderShape<N>& compoundColliderShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<CompoundColliderShape<N>>();
		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersA = compoundColliderShapeA.getSubColliders();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		for (SubContact& subContact : subContacts) {
			const SubCollider<N>& subColliderA = subCollidersA[subContact.subColliderIndices.first];
			const SubCollider<N>& subColliderB = subCollidersB[subContact.subColliderIndices.second];
			const LocalTransformation<N> localTransformationA = translateRotateScale(subColliderA.localOffset, subColliderA.localOrientation, subColliderA.localScale);
			const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
			const Transformation<N> globalTransformationA = transformationA * localTransformationA;
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			if (const CollisionFilterTestResult filterTestResult = subContact.collisionAlgorithm.hasCollision(temporaryMemoryResource, minPenetrationDepth, subColliderA.collider,
					globalTransformationA, subColliderB.collider, globalTransformationB, options, filterTest)) {
				return filterTestResult;
			}
		}
		return {};
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		updateSubContacts(colliderA.shape, transformationA, colliderB.shape, transformationB);

		const CompoundColliderShape<N>& compoundColliderShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<CompoundColliderShape<N>>();
		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersA = compoundColliderShapeA.getSubColliders();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		for (SubContact& subContact : subContacts) {
			const SubCollider<N>& subColliderA = subCollidersA[subContact.subColliderIndices.first];
			const SubCollider<N>& subColliderB = subCollidersB[subContact.subColliderIndices.second];
			const LocalTransformation<N> localTransformationA = translateRotateScale(subColliderA.localOffset, subColliderA.localOrientation, subColliderA.localScale);
			const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
			const Transformation<N> globalTransformationA = transformationA * localTransformationA;
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			subContact.collisionAlgorithm.detectCollisions(temporaryMemoryResource, subColliderA.collider, globalTransformationA, subColliderB.collider, globalTransformationB,
				options, filterTest, [&](const CollisionAlgorithmResult<N>& subCollision) -> void {
					CollisionAlgorithmResult<N> collision{.manifold = subCollision.manifold};
					for (ContactPoint<N>& point : collision.manifold.points) {
						point.offsets.first = subColliderA.localOffset + point.offsets.first;
						point.offsets.second = subColliderB.localOffset + point.offsets.second;
						point.localOffsets.first = localTransformationA(point.localOffsets.first) - 0;
						point.localOffsets.second = localTransformationB(point.localOffsets.second) - 0;
					}
					callback(collision);
				});
		}
	}

private:
	struct SubContact {
		Pair<size_t> subColliderIndices;
		CollisionAlgorithm<N> collisionAlgorithm;
	};

	void updateSubContacts(ShapeView<N> shapeA, const Transformation<N>& transformationA, ShapeView<N> shapeB, const Transformation<N>& transformationB) {
		const CompoundColliderShape<N>& compoundColliderShapeA = static_cast<const Shape<N>&>(shapeA).template as<CompoundColliderShape<N>>();
		const CompoundColliderShape<N>& compoundColliderShapeB = static_cast<const Shape<N>&>(shapeB).template as<CompoundColliderShape<N>>();
		const Span<const SubCollider<N>> subCollidersA = compoundColliderShapeA.getSubColliders();
		const Span<const SubCollider<N>> subCollidersB = compoundColliderShapeB.getSubColliders();
		newSubContacts.clear();
		size_t subContactIndex = 0;
		for (size_t subColliderIndexA = 0; subColliderIndexA < subCollidersA.size(); ++subColliderIndexA) {
			for (size_t subColliderIndexB = 0; subColliderIndexB < subCollidersB.size(); ++subColliderIndexB) {
				const Pair<size_t> subColliderIndices{subColliderIndexA, subColliderIndexB};
				const SubCollider<N>& subColliderA = subCollidersA[subColliderIndexA];
				const SubCollider<N>& subColliderB = subCollidersB[subColliderIndexB];
				const LocalTransformation<N> localTransformationA = translateRotateScale(subColliderA.localOffset, subColliderA.localOrientation, subColliderA.localScale);
				const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
				const Transformation<N> globalTransformationA = transformationA * localTransformationA;
				const Transformation<N> globalTransformationB = transformationB * localTransformationB;
				const Optional<Box<N>> aabbA = ShapeView<N>{subColliderA.collider.shape}.getBoundingBox(globalTransformationA);
				const Optional<Box<N>> aabbB = ShapeView<N>{subColliderB.collider.shape}.getBoundingBox(globalTransformationB);
				if (!aabbA || !aabbB || intersects(*aabbA, *aabbB)) {
					if (subContactIndex < subContacts.size() && subContacts[subContactIndex].subColliderIndices == subColliderIndices) {
						newSubContacts.push_back(SubContact{
							.subColliderIndices = subColliderIndices,
							.collisionAlgorithm = std::move(subContacts[subContactIndex].collisionAlgorithm),
						});
						++subContactIndex;
					} else {
						newSubContacts.push_back(SubContact{
							.subColliderIndices = subColliderIndices,
							.collisionAlgorithm = CollisionAlgorithm<N>::chooseImplementation(subColliderA.collider.shape, subColliderB.collider.shape),
						});
					}
				} else if (subContactIndex < subContacts.size() && subContacts[subContactIndex].subColliderIndices == subColliderIndices) {
					++subContactIndex;
				}
			}
		}
		subContacts.swap(newSubContacts);
	}

	ArrayList<SubContact> subContacts{};
	ArrayList<SubContact> newSubContacts{};
};

template <size_t N>
struct choose_collision_detector<N, CompoundColliderShape<N>, CompoundColliderShape<N>> {
	using type = CollisionDetector_compound_vs_compound<N>;
};

} // namespace grem::physics

#endif
