// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_TRIANGLE_MESH_VS_INFINITE_HALF_SPACE_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_TRIANGLE_MESH_VS_INFINITE_HALF_SPACE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"

namespace grem::physics {

// Collision detector for TriangleMeshShape VS InfiniteHalfSpaceShape.
template <size_t N>
class CollisionDetector_triangle_mesh_vs_infinite_half_space final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView<N> colliderA, const Transformation<N>& transformationA,
		ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		const TriangleMeshShape<N>& triangleMeshShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<TriangleMeshShape<N>>();
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Length<N> meshLocalPlaneOffset = inverseTransformationA(plane.point);
		const Direction<N> meshLocalPlaneNormal = inverseTransformationA.getDirection(plane.normal);
		const TriangleMesh<N>& mesh = *triangleMeshShapeA.getTriangleMesh();
		const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		const Length1D maxCollisionDistance = min(-minPenetrationDepth, options.maxCollisionTouchingDistance);
		if (mesh.getFaceOrthtree().traverseElements(
				[&](const TriangleMeshFaceIndex& faceIndex) -> bool {
					const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
					const Array meshLocalTrianglePoints{
						vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
						vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
						vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
					};

					for (const Length<N> meshLocalTrianglePoint : meshLocalTrianglePoints) {
						const Length1D signedDistance = dot(meshLocalTrianglePoint - meshLocalPlaneOffset, meshLocalPlaneNormal);
						if (signedDistance <= maxCollisionDistance) {
							return true;
						}
					}
					return false;
				},
				[&](const grem::Box<N, float>& meshLocalBoundingBox) -> bool {
					const Length<N> meshLocalMin = meshLocalBoundingBox.min * Length<N>::UNIT;
					const Length<N> meshLocalMax = meshLocalBoundingBox.max * Length<N>::UNIT;
					const Length<N> meshLocalCenter = midpoint(meshLocalMin, meshLocalMax);
					const Length<N> halfExtents = (meshLocalMax - meshLocalMin) * 0.5f;
					const Length<N> meshLocalNearPoint = meshLocalCenter + copysign(halfExtents, -meshLocalPlaneNormal);
					const Length<N> meshLocalFarPoint = meshLocalCenter + copysign(halfExtents, meshLocalPlaneNormal);
					return dot(meshLocalNearPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= maxCollisionDistance ||
			               dot(meshLocalFarPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= maxCollisionDistance;
				})) {
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

		const TriangleMeshShape<N>& triangleMeshShapeA = static_cast<const Shape<N>&>(colliderA.shape).template as<TriangleMeshShape<N>>();
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Length<N> meshLocalPlaneOffset = inverseTransformationA(plane.point);
		const Direction<N> meshLocalPlaneNormal = inverseTransformationA.getDirection(plane.normal);
		const TriangleMesh<N>& mesh = *triangleMeshShapeA.getTriangleMesh();
		const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		mesh.getFaceOrthtree().traverseElements(
			[&](const TriangleMeshFaceIndex& faceIndex) -> void {
				const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
				const Array meshLocalTrianglePoints{
					vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
					vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
					vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
				};

				InplaceArrayList<Position<N>, 3> pointsA{};
				InplaceArrayList<Position<N>, 3> pointsB{};
				for (const Length<N> meshLocalTrianglePoint : meshLocalTrianglePoints) {
					const Length1D signedDistance = dot(meshLocalTrianglePoint - meshLocalPlaneOffset, meshLocalPlaneNormal);
					if (signedDistance <= options.maxCollisionTouchingDistance) {
						const Position<N> pointA = transformationA(meshLocalTrianglePoint);
						const Position<N> pointB = pointA - plane.normal * signedDistance;
						pointsA.push_back(pointA);
						pointsB.push_back(pointB);
					}
				}

				if (!pointsA.empty()) {
					callback(CollisionAlgorithmResult<N>{
						.manifold = convertPointsToContactManifold<N>(pointsA, inverseTransformationA, ContactFeatureType::TRIANGLE_MESH_FACE, faceIndex, pointsB,
							inverseTransformationB, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, -plane.normal, filterTestResult),
					});
				}
			},
			[&](const grem::Box<N, float>& meshLocalBoundingBox) -> bool {
				const Length<N> meshLocalMin = meshLocalBoundingBox.min * Length<N>::UNIT;
				const Length<N> meshLocalMax = meshLocalBoundingBox.max * Length<N>::UNIT;
				const Length<N> meshLocalCenter = midpoint(meshLocalMin, meshLocalMax);
				const Length<N> halfExtents = (meshLocalMax - meshLocalMin) * 0.5f;
				const Length<N> meshLocalNearPoint = meshLocalCenter + copysign(halfExtents, -meshLocalPlaneNormal);
				const Length<N> meshLocalFarPoint = meshLocalCenter + copysign(halfExtents, meshLocalPlaneNormal);
				return dot(meshLocalNearPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= options.maxCollisionTouchingDistance ||
			           dot(meshLocalFarPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= options.maxCollisionTouchingDistance;
			});
	}
};

template <size_t N>
struct choose_collision_detector<N, TriangleMeshShape<N>, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_triangle_mesh_vs_infinite_half_space<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, TriangleMeshShape<N>> {
	using type = ReversedCollisionDetector<N, CollisionDetector_triangle_mesh_vs_infinite_half_space<N>>;
};

} // namespace grem::physics

#endif
