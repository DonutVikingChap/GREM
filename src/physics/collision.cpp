// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>

#include "collision/GilbertJohnsonKeerthiAlgorithm.hpp"

// IWYU pragma: begin_keep
#include "collision/CollisionDetector.hpp"
#include "collision/CollisionDetector_any_convex_polytope_vs_any_convex_polytope.hpp"
#include "collision/CollisionDetector_any_convex_polytope_vs_infinite_half_space.hpp"
#include "collision/CollisionDetector_any_convex_polytope_vs_infinite_plane_3d.hpp"
#include "collision/CollisionDetector_any_convex_polytope_vs_triangle_mesh.hpp"
#include "collision/CollisionDetector_any_convex_vs_any_convex.hpp"
#include "collision/CollisionDetector_any_convex_vs_infinite_half_space.hpp"
#include "collision/CollisionDetector_any_convex_vs_infinite_plane_3d.hpp"
#include "collision/CollisionDetector_any_convex_vs_triangle_mesh.hpp"
#include "collision/CollisionDetector_any_vs_compound.hpp"
#include "collision/CollisionDetector_any_vs_locally_transformed.hpp"
#include "collision/CollisionDetector_compound_vs_compound.hpp"
#include "collision/CollisionDetector_ignored.hpp"
#include "collision/CollisionDetector_locally_transformed_vs_compound.hpp"
#include "collision/CollisionDetector_locally_transformed_vs_locally_transformed.hpp"
#include "collision/CollisionDetector_sphere_vs_sphere.hpp"
#include "collision/CollisionDetector_triangle_mesh_vs_infinite_half_space.hpp"
#include "collision/CollisionDetector_triangle_mesh_vs_infinite_plane_3d.hpp"
// IWYU pragma: end_keep

namespace grem::physics {

namespace {

template <size_t N>
[[nodiscard]] CollisionFilterTestResult containsPoint(const Position<N>& pointA, CollisionFilter filterA, ColliderView<N> colliderB, const Transformation<N>& transformationB,
	CollisionFilterTest filterTest) {
	const CollisionFilterTestResult filterTestResult = filterTest(filterA, colliderB.filter);
	if (!filterTestResult) {
		return {};
	}

	GREM_MATCH(static_cast<const Shape<N>&>(colliderB.shape)) {
		GREM_CASE(const LocallyTransformedShape<N>& locallyTransformedShape) {
			const LocalTransformation<N> localTransformation =
				translateRotateScale(locallyTransformedShape.localOffset, locallyTransformedShape.localOrientation, locallyTransformedShape.localScale);
			return containsPoint<N>(pointA, filterA, ColliderView<N>{*locallyTransformedShape.shape, colliderB.filter}, transformationB * localTransformation, filterTest);
		}
		GREM_CASE(const CompoundColliderShape<N>& compoundColliderShape) {
			for (const SubCollider<N>& subCollider : compoundColliderShape.getSubColliders()) {
				const LocalTransformation<N> subLocalTransformation = translateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale);
				if (const CollisionFilterTestResult subTestResult = containsPoint<N>(pointA, filterA, subCollider.collider, transformationB * subLocalTransformation, filterTest)) {
					return subTestResult;
				}
			}
			return {};
		}
		GREM_CASE_DEFAULT(const auto& otherShape) {
			if (otherShape.isConvexShapeType()) {
				const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
				const Length<N> localPointB = inverseTransformationB(pointA);
				if (ConvexShapeView<N>{otherShape}.containsLocalPoint(localPointB)) {
					return filterTestResult;
				}
			}
			return {};
		}
	}
	unreachable();
}

template <size_t N>
[[nodiscard]] Pair<RaycastResult<N>, CollisionFilterTestResult> raycast(const Ray<N>& rayA, CollisionFilter filterA, ColliderView<N> colliderB,
	const Transformation<N>& transformationB, CollisionFilterTest filterTest) {
	CollisionFilterTestResult filterTestResult = filterTest(filterA, colliderB.filter);
	if (!filterTestResult) {
		return {RayMiss{}, {}};
	}

	if (const CompoundColliderShape<N>* const compoundColliderShape = static_cast<const Shape<N>&>(colliderB.shape).template get_if<CompoundColliderShape<N>>()) {
		RaycastResult<N> raycastResult{};
		filterTestResult = {};
		for (const SubCollider<N>& subCollider : compoundColliderShape->getSubColliders()) {
			const LocalTransformation<N> subLocalTransformation = translateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale);
			const auto [subRaycastResult, subTestResult] = raycast<N>(rayA, filterA, subCollider.collider, transformationB * subLocalTransformation, filterTest);
			GREM_MATCH(subRaycastResult) {
				GREM_CASE(const RayMiss& miss) break;
				GREM_CASE(const RayHit<N>& hit) {
					if (raycastResult.template is<RayMiss>() || (raycastResult.template is<RayHit<N>>() && hit.distance < raycastResult.template as<RayHit<N>>().distance)) {
						raycastResult = hit;
						filterTestResult = subTestResult;
					}
					break;
				}
				GREM_CASE(const RayHitInterior<N>& hitInterior) {
					raycastResult = hitInterior;
					filterTestResult = subTestResult;
					break;
				}
			}
		}
		return {raycastResult, filterTestResult};
	}

	const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
	const Length<N> localRayOriginB = inverseTransformationB(rayA.origin);
	const Length<N> localRayVectorB = inverseTransformationB.getRelative(rayA.direction * rayA.maxDistance);
	const Distance maxLocalRayDistanceB = length(localRayVectorB);
	const Direction<N> localRayDirectionB = Direction<N>::reinterpret(localRayVectorB / maxLocalRayDistanceB);
	GREM_MATCH(colliderB.shape.castLocalRay(localRayOriginB, localRayDirectionB, maxLocalRayDistanceB)) {
		GREM_CASE(const RayMiss& miss) return {RayMiss{}, {}};
		GREM_CASE(const RayHit<N>& hit) {
			const Distance distance = transformationB.getDistance(localRayDirectionB * hit.distance);
			const Direction<N> normal = transformationB.getDirection(hit.normal);
			return {RayHit<N>{.localOffset = hit.localOffset, .distance = distance, .normal = normal}, filterTestResult};
		}
		GREM_CASE(const RayHitInterior<N>& hitInterior) {
			return {RayHitInterior<N>{.localOffset = hitInterior.localOffset}, filterTestResult};
		}
	}
	unreachable();
}

template <size_t N>
[[nodiscard]] Pair<ShapecastResult<N>, CollisionFilterTestResult> shapecast(ConvexShapeView<N> convexShapeA, CollisionFilter filterA, const Transformation<N>& transformationA,
	ColliderView<N> colliderB, const Transformation<N>& transformationB, Direction<N> direction, Distance maxDistance, const CollisionAlgorithmOptions<N>& options,
	CollisionFilterTest filterTest) {
	CollisionFilterTestResult filterTestResult = filterTest(filterA, colliderB.filter);
	if (!filterTestResult) {
		return {ShapecastMiss{}, {}};
	}

	if constexpr (N == 3) {
		if (static_cast<const Shape<N>&>(colliderB.shape).template is<InfinitePlaneShape3D>()) {
			const InverseTransformation3D inverseTransformationA = inverse(transformationA);
			const InverseTransformation3D inverseTransformationB = inverse(transformationB);
			const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
			const Direction3D localDirectionA = inverseTransformationA.getDirection(-plane.normal);
			const Position3D nearPointA = transformationA(convexShapeA.getLocalSupportPointOffset(localDirectionA));
			const Position3D farPointA = transformationA(convexShapeA.getLocalSupportPointOffset(-localDirectionA));
			const Length1D nearSignedDistance = dot(nearPointA - plane.point, plane.normal);
			const Length1D farSignedDistance = dot(plane.point - farPointA, plane.normal);
			const auto [signedDistance, pointA, normal] =
				(nearSignedDistance > farSignedDistance) ? Tuple{nearSignedDistance, nearPointA, -plane.normal} : Tuple{farSignedDistance, farPointA, plane.normal};
			const Scale1D directionAlongNormal = -dot(direction, normal);
			const Distance distance = (directionAlongNormal != 0) ? max(signedDistance / directionAlongNormal, Length1D{}) : Distance::INF;
			if (distance > maxDistance) {
				return {ShapecastMiss{}, {}};
			}
			const Position3D pointB = pointA + normal * signedDistance;
			const Pair<Length3D> localOffsets{inverseTransformationA(pointA), inverseTransformationB(pointB)};
			return {ShapecastHit3D{.localOffsets = localOffsets, .normal = normal, .distance = distance}, filterTestResult};
		}
	}
	GREM_MATCH(static_cast<const Shape<N>&>(colliderB.shape)) {
		GREM_CASE(const InfiniteHalfSpaceShape<N>& infiniteHalfSpaceShape) {
			const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
			const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
			const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
			const Position<N> pointA = transformationA(convexShapeA.getLocalSupportPointOffset(inverseTransformationA.getDirection(-plane.normal)));
			const Scale1D directionAlongPlaneNormal = dot(direction, plane.normal);
			const Length1D signedDistance = dot(plane.point - pointA, plane.normal);
			const Distance distance = (directionAlongPlaneNormal < 0) ? max(signedDistance / directionAlongPlaneNormal, Length1D{}) : Distance::INF;
			if (distance > maxDistance) {
				return {ShapecastMiss{}, {}};
			}
			const Position<N> pointB = pointA + plane.normal * signedDistance;
			const Pair<Length<N>> localOffsets{inverseTransformationA(pointA), inverseTransformationB(pointB)};
			return {ShapecastHit<N>{.localOffsets = localOffsets, .normal = plane.normal, .distance = signbit(signedDistance) ? distance : Distance{}}, filterTestResult};
		}
		GREM_CASE(const TriangleMeshShape<N>& triangleMeshShapeB) {
			const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
			const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
			const TriangleMesh<N>& mesh = *triangleMeshShapeB.getTriangleMesh();
			const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
			const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
			const Optional<Box<N>> meshLocalBounds = convexShapeA.getBoundingBox(Transformation<N>{0, inverseTransformationB * transformationA});
			const Length<N> meshLocalRayVector = inverseTransformationB.getRelative(direction * maxDistance);
			const Distance meshLocalMaxRayDistance = length(meshLocalRayVector);
			const Ray<N> meshLocalRay{
				.origin = (meshLocalBounds) ? midpoint(meshLocalBounds->min, meshLocalBounds->max) : Position<N>{},
				.direction = Direction<N>::reinterpret(meshLocalRayVector / meshLocalMaxRayDistance),
				.maxDistance = meshLocalMaxRayDistance,
			};
			const Length<N> meshLocalRayExpansion =
				(meshLocalBounds) ? (meshLocalBounds->max - meshLocalBounds->min) * 0.5f + Length<N>{options.maxCollisionTouchingDistance} : Length<N>::MAX;

			ShapecastResult<N> shapecastResult{};
			CollisionFilterTestResult shapecastTestResult{};
			mesh.getFaceOrthtree().traverseElements(
				[&](const TriangleMeshFaceIndex& faceIndex) -> void {
					const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
					const Array meshLocalTrianglePoints{
						vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
						vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
						vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
					};
					const Triangle<N> meshLocalTriangle{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
					if (meshLocalTriangle.getBoundingBox().getExpanded(meshLocalRayExpansion).intersects(meshLocalRay)) {
						const detail::TriangleShape<N> triangleShapeB{
							.pointA = meshLocalTrianglePoints[0],
							.pointB = meshLocalTrianglePoints[1],
							.pointC = meshLocalTrianglePoints[2],
						};
						const auto gjkResult = GilbertJohnsonKeerthiAlgorithm<N>{}.shapecast(direction, maxDistance, convexShapeA, transformationA, inverseTransformationA,
							triangleShapeB, transformationB, inverseTransformationB, options.gjkRaycastMargin, options.collisionDistanceErrorTolerance,
							options.maxGJKRaycastIterationCount);
						if (gjkResult.interior) {
							shapecastResult = ShapecastHitInterior<N>{};
							shapecastTestResult = filterTestResult;
						} else if (gjkResult.hit) {
							if (shapecastResult.template is<ShapecastMiss>() ||
								(shapecastResult.template is<ShapecastHit<N>>() && gjkResult.distance < shapecastResult.template as<ShapecastHit<N>>().distance)) {
								shapecastResult = ShapecastHit<N>{
									.localOffsets{inverseTransformationA(gjkResult.witnessPoints.first), inverseTransformationB(gjkResult.witnessPoints.second)},
									.normal = Direction<N>::reinterpret(gjkResult.normal),
									.distance = gjkResult.distance,
								};
								shapecastTestResult = filterTestResult;
							}
						}
					}
				},
				[&](const grem::Box<N, float>& boundingBox) -> bool { return (boundingBox * Box<N>::UNIT).getExpanded(meshLocalRayExpansion).intersects(meshLocalRay); });
			return {shapecastResult, shapecastTestResult};
		}
		GREM_CASE(const LocallyTransformedShape<N>& locallyTransformedShapeB) {
			const LocalTransformation<N> localTransformationB =
				translateRotateScale(locallyTransformedShapeB.localOffset, locallyTransformedShapeB.localOrientation, locallyTransformedShapeB.localScale);
			const Transformation<N> globalTransformationB = transformationB * localTransformationB;
			return shapecast(convexShapeA, filterA, transformationA, ColliderView<N>{*locallyTransformedShapeB.shape, colliderB.filter}, globalTransformationB, direction,
				maxDistance, options, filterTest);
		}
		GREM_CASE(const CompoundColliderShape<N>& compoundColliderShapeB) {
			ShapecastResult<N> shapecastResult{};
			filterTestResult = {};
			for (const SubCollider<N>& subColliderB : compoundColliderShapeB.getSubColliders()) {
				const LocalTransformation<N> localTransformationB = translateRotateScale(subColliderB.localOffset, subColliderB.localOrientation, subColliderB.localScale);
				const Transformation<N> globalTransformationB = transformationB * localTransformationB;
				const auto [subShapecastResult, subTestResult] =
					shapecast(convexShapeA, filterA, transformationA, subColliderB.collider, globalTransformationB, direction, maxDistance, options, filterTest);
				GREM_MATCH(subShapecastResult) {
					GREM_CASE(const ShapecastMiss& miss) break;
					GREM_CASE(const ShapecastHit<N>& hit) {
						if (shapecastResult.template is<ShapecastMiss>() ||
							(shapecastResult.template is<ShapecastHit<N>>() && hit.distance < shapecastResult.template as<ShapecastHit<N>>().distance)) {
							shapecastResult = hit;
							filterTestResult = subTestResult;
						}
						break;
					}
					GREM_CASE(const ShapecastHitInterior<N>& hitInterior) {
						shapecastResult = hitInterior;
						filterTestResult = subTestResult;
						break;
					}
				}
			}
			return {shapecastResult, filterTestResult};
		}
		GREM_CASE_DEFAULT(const auto& otherShape) {
			if (otherShape.isConvexShapeType()) {
				const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
				const InverseTransformation<N> inverseTransformationB = inverse(transformationB);
				const auto gjkResult = GilbertJohnsonKeerthiAlgorithm<N>{}.shapecast(direction, maxDistance, convexShapeA, transformationA, inverseTransformationA,
					ConvexShapeView<N>{colliderB.shape}, transformationB, inverseTransformationB, options.gjkRaycastMargin, options.collisionDistanceErrorTolerance,
					options.maxGJKRaycastIterationCount);
				if (gjkResult.interior) {
					return {ShapecastHitInterior<N>{}, filterTestResult};
				}
				if (gjkResult.hit) {
					return {
						ShapecastHit<N>{
							.localOffsets{inverseTransformationA(gjkResult.witnessPoints.first), inverseTransformationB(gjkResult.witnessPoints.second)},
							.normal = Direction<N>::reinterpret(gjkResult.normal),
							.distance = gjkResult.distance,
						},
						filterTestResult,
					};
				}
			}
			return {ShapecastMiss{}, {}};
		}
	}
	unreachable();
}

} // namespace

template <>
CollisionAlgorithm2D CollisionAlgorithm2D::chooseImplementation(ShapeView2D shapeA, ShapeView2D shapeB) {
	return match(static_cast<const Shape2D&>(shapeA))([&shapeB]<typename ShapeA>(const ShapeA&) -> CollisionAlgorithm2D { //
		return match(static_cast<const Shape2D&>(shapeB))([]<typename ShapeB>(const ShapeB&) -> CollisionAlgorithm2D {    //
			return CollisionAlgorithm2D::create<typename choose_collision_detector<2, ShapeA, ShapeB>::type>();
		});
	});
}

template <>
CollisionAlgorithm3D CollisionAlgorithm3D::chooseImplementation(ShapeView3D shapeA, ShapeView3D shapeB) {
	return match(static_cast<const Shape3D&>(shapeA))([&shapeB]<typename ShapeA>(const ShapeA&) -> CollisionAlgorithm3D { //
		return match(static_cast<const Shape3D&>(shapeB))([]<typename ShapeB>(const ShapeB&) -> CollisionAlgorithm3D {    //
			return CollisionAlgorithm3D::create<typename choose_collision_detector<3, ShapeA, ShapeB>::type>();
		});
	});
}

CollisionFilterTestResult containsPoint(const Position2D& pointA, CollisionFilter filterA, ColliderView2D colliderB, const Transformation2D& transformationB,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return containsPoint<2>(pointA, filterA, colliderB, transformationB, filterTest);
}

CollisionFilterTestResult containsPoint(const Position3D& pointA, CollisionFilter filterA, ColliderView3D colliderB, const Transformation3D& transformationB,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return containsPoint<3>(pointA, filterA, colliderB, transformationB, filterTest);
}

Pair<RaycastResult2D, CollisionFilterTestResult> raycast(const Ray2D& rayA, CollisionFilter filterA, ColliderView2D colliderB, const Transformation2D& transformationB,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return raycast<2>(rayA, filterA, colliderB, transformationB, filterTest);
}

Pair<RaycastResult3D, CollisionFilterTestResult> raycast(const Ray3D& rayA, CollisionFilter filterA, ColliderView3D colliderB, const Transformation3D& transformationB,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return raycast<3>(rayA, filterA, colliderB, transformationB, filterTest);
}

Pair<ShapecastResult2D, CollisionFilterTestResult> shapecast(ConvexShapeView2D convexShapeA, CollisionFilter filterA, const Transformation2D& transformationA,
	ColliderView2D colliderB, const Transformation2D& transformationB, Direction2D direction, Distance maxDistance, const CollisionAlgorithmOptions2D& options,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return shapecast<2>(convexShapeA, filterA, transformationA, colliderB, transformationB, direction, maxDistance, options, filterTest);
}

Pair<ShapecastResult3D, CollisionFilterTestResult> shapecast(ConvexShapeView3D convexShapeA, CollisionFilter filterA, const Transformation3D& transformationA,
	ColliderView3D colliderB, const Transformation3D& transformationB, Direction3D direction, Distance maxDistance, const CollisionAlgorithmOptions3D& options,
	CollisionFilterTest filterTest) {
	GREM_PROFILE_FUNCTION();
	return shapecast<3>(convexShapeA, filterA, transformationA, colliderB, transformationB, direction, maxDistance, options, filterTest);
}

} // namespace grem::physics
