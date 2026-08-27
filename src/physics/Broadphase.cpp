// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Any.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/joints.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

using namespace grem::physics_literals;

namespace grem::physics {

template <size_t N>
bool Broadphase<N>::testPoint(Position<N> point, CollisionFilter filter,
	execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities, CollisionFilterTest filterTest,
	FunctionView<bool(EntityID objectID, CollisionFilterTestResult filterTestResult)> callback, FunctionView<bool(EntityID objectID)> predicate) const {
	GREM_PROFILE_FUNCTION();

	bool foundCollision = false;
	traverseEntities(
		[&](const EntityID& objectID) -> bool {
			if (!entities.containsEntity(objectID) || !entities.template getComponent<ObjectBounds<N>>(objectID).boundingBox.contains(point) || !predicate(objectID)) {
				return false;
			}
			const auto& [entityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = entities[objectID];
			const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
			if (const CollisionFilterTestResult filterTestResult = physics::containsPoint(point, filter, colliderB, transformationB, filterTest)) {
				foundCollision = true;
				return callback(objectID, filterTestResult);
			}
			return false;
		},
		[&](const Box<N>& boundingBox) -> bool { return boundingBox.contains(point); });
	return foundCollision;
}

template <size_t N>
bool Broadphase<N>::testShape(Length1D minPenetrationDepth, ShapeView<N> shape, CollisionFilter filter, const Transformation<N>& transformation,
	execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities, const CollisionAlgorithmOptions<N>& options,
	CollisionFilterTest filterTest, FunctionView<bool(EntityID objectID, CollisionFilterTestResult filterTestResult)> callback,
	FunctionView<bool(EntityID objectID)> predicate) const {
	GREM_PROFILE_FUNCTION();

	Arena<8192> arena{};
	bool foundCollision = false;
	const auto testObject = [&](const EntityID& objectID) -> bool {
		if (!predicate(objectID)) {
			return false;
		}
		const auto& [entityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = entities[objectID];
		const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
		arena.release();
		if (const CollisionFilterTestResult filterTestResult = CollisionAlgorithm<N>::chooseImplementation(shape, colliderB.shape)
		        .hasCollision(&arena, minPenetrationDepth, ColliderView<N>{shape, filter}, transformation, colliderB, transformationB, options, filterTest)) {
			foundCollision = true;
			return callback(objectID, filterTestResult);
		}
		return false;
	};

	if (const Optional<Box<N>> shapeBoundingBox = shape.getBoundingBox(transformation)) {
		const Box<N> expandedShapeBoundingBox = shapeBoundingBox->getExpanded(options.maxCollisionTouchingDistance * 2.0f);
		traverseEntities(
			[&](const EntityID& objectID) -> bool {
				if (entities.containsEntity(objectID) && intersects(entities.template getComponent<ObjectBounds<N>>(objectID).boundingBox, expandedShapeBoundingBox)) {
					return testObject(objectID);
				}
				return false;
			},
			[&](const Box<N>& boundingBox) -> bool { return intersects(boundingBox, expandedShapeBoundingBox); });
	} else {
		traverseEntities([&](const EntityID& objectID) -> bool {
			if (entities.containsEntity(objectID)) {
				return testObject(objectID);
			}
			return false;
		});
	}
	return foundCollision;
}

template <size_t N>
bool Broadphase<N>::collide(ShapeView<N> shape, CollisionFilter filter, const Transformation<N>& transformation,
	execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities, const CollisionAlgorithmOptions<N>& options,
	CollisionFilterTest filterTest, FunctionView<bool(const CollisionResult& collision)> callback, FunctionView<bool(EntityID objectID)> predicate) const {
	GREM_PROFILE_FUNCTION();

	Arena<8192> arena{};
	bool foundCollision = false;
	const auto testObject = [&](const EntityID& objectID) -> bool {
		if (!predicate(objectID)) {
			return false;
		}
		const auto& [entityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = entities[objectID];
		const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
		bool exitedEarly = false;
		arena.release();
		CollisionAlgorithm<N>::chooseImplementation(shape, colliderB.shape)
			.detectCollisions(&arena, ColliderView<N>{shape, filter}, transformation, colliderB, transformationB, options, filterTest,
				[&](const CollisionAlgorithmResult<N>& collision) -> void {
					if (!exitedEarly) {
						foundCollision = true;
						exitedEarly = callback(CollisionResult{.objectID = objectID, .manifold = collision.manifold});
					}
				});
		return exitedEarly;
	};

	if (const Optional<Box<N>> shapeBoundingBox = ShapeView<N>{shape}.getBoundingBox(transformation)) {
		const Box<N> expandedShapeBoundingBox = shapeBoundingBox->getExpanded(options.maxCollisionTouchingDistance * 2.0f);
		traverseEntities(
			[&](const EntityID& objectID) -> bool {
				if (entities.containsEntity(objectID) && intersects(entities.template getComponent<ObjectBounds<N>>(objectID).boundingBox, expandedShapeBoundingBox)) {
					return testObject(objectID);
				}
				return false;
			},
			[&](const Box<N>& boundingBox) -> bool { return intersects(boundingBox, expandedShapeBoundingBox); });
	} else {
		traverseEntities([&](const EntityID& objectID) -> bool {
			if (entities.containsEntity(objectID)) {
				return testObject(objectID);
			}
			return false;
		});
	}
	return foundCollision;
}

template <size_t N>
bool Broadphase<N>::raycast(const Ray<N>& ray, CollisionFilter filter,
	execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities, CollisionFilterTest filterTest,
	FunctionView<bool(const RaycastResult& hit)> callback, FunctionView<bool(EntityID objectID)> predicate) const {
	GREM_PROFILE_FUNCTION();

	bool foundCollision = false;
	traverseEntities(
		[&](const EntityID& objectID) -> bool {
			if (!entities.containsEntity(objectID) || !entities.template getComponent<ObjectBounds<N>>(objectID).boundingBox.intersects(ray) || !predicate(objectID)) {
				return false;
			}
			const auto& [entityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = entities[objectID];
			const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
			const auto [raycastResult, filterTestResult] = physics::raycast(ray, filter, colliderB, transformationB, filterTest);
			GREM_MATCH(raycastResult) {
				GREM_CASE(const RayMiss& miss) break;
				GREM_CASE(const RayHit<N>& hit) {
					foundCollision = true;
					return callback(RaycastResult{
						.objectID = objectID,
						.localOffset = hit.localOffset,
						.normal = hit.normal,
						.distance = hit.distance,
						.filterTestResult = filterTestResult,
					});
				}
				GREM_CASE(const RayHitInterior<N>& hitInterior) {
					foundCollision = true;
					return callback(RaycastResult{
						.objectID = objectID,
						.localOffset = hitInterior.localOffset,
						.normal = -ray.direction,
						.distance{},
						.filterTestResult = filterTestResult,
					});
				}
			}
			return false;
		},
		[&](const Box<N>& boundingBox) -> bool { return boundingBox.intersects(ray); });
	return foundCollision;
}

template <size_t N>
bool Broadphase<N>::shapecast(ConvexShapeView<N> convexShape, CollisionFilter filter, const Transformation<N>& transformation, Direction<N> direction, Distance maxDistance,
	execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities, const CollisionAlgorithmOptions<N>& options,
	CollisionFilterTest filterTest, FunctionView<bool(const ShapecastResult& hit)> callback, FunctionView<bool(EntityID objectID)> predicate) const {
	GREM_PROFILE_FUNCTION();

	const Optional<Box<N>> convexShapeBoundingBox = convexShape.getBoundingBox(transformation);
	const Ray<N> ray{
		.origin = (convexShapeBoundingBox) ? midpoint(convexShapeBoundingBox->min, convexShapeBoundingBox->max) : Position<N>{},
		.direction = direction,
		.maxDistance = maxDistance,
	};
	const Length<N> rayExpansion =
		(convexShapeBoundingBox) ? (convexShapeBoundingBox->max - convexShapeBoundingBox->min) * 0.5f + Length<N>{options.maxCollisionTouchingDistance} : Length<N>::MAX;

	bool foundCollision = false;
	traverseEntities(
		[&](const EntityID& objectID) -> bool {
			if (!entities.containsEntity(objectID) || !entities.template getComponent<ObjectBounds<N>>(objectID).boundingBox.getExpanded(rayExpansion).intersects(ray) ||
				!predicate(objectID)) {
				return false;
			}
			const auto& [entityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = entities[objectID];
			const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
			const auto [shapecastResult, filterTestResult] =
				physics::shapecast(convexShape, filter, transformation, colliderB, transformationB, direction, maxDistance, options, filterTest);
			GREM_MATCH(shapecastResult) {
				GREM_CASE(const ShapecastMiss& miss) break;
				GREM_CASE(const ShapecastHit<N>& hit) {
					foundCollision = true;
					return callback(ShapecastResult{
						.objectID = objectID,
						.localOffsets = hit.localOffsets,
						.normal = hit.normal,
						.distance = hit.distance,
						.filterTestResult = filterTestResult,
					});
				}
				GREM_CASE(const ShapecastHitInterior<N>& hitInterior) {
					foundCollision = true;
					return callback(ShapecastResult{
						.objectID = objectID,
						.localOffsets{},
						.normal = -direction,
						.distance{},
						.filterTestResult = filterTestResult,
					});
				}
			}
			return false;
		},
		[&](const Box<N>& boundingBox) -> bool { return boundingBox.getExpanded(rayExpansion).intersects(ray); });
	return foundCollision;
}

template class Broadphase<2>;
template class Broadphase<3>;

} // namespace grem::physics
