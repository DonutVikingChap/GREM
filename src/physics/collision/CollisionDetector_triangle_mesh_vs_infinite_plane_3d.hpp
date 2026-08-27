// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_TRIANGLE_MESH_VS_INFINITE_PLANE_3D_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_TRIANGLE_MESH_VS_INFINITE_PLANE_3D_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"

namespace grem::physics {

// Collision detector for TriangleMeshShape3D VS InfinitePlaneShape3D.
class CollisionDetector_triangle_mesh_vs_infinite_plane_3d final : public CollisionAlgorithmImplementation3D {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView3D colliderA, const Transformation3D& transformationA,
		ColliderView3D colliderB, const Transformation3D& transformationB, const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		const TriangleMeshShape3D& triangleMeshShapeA = static_cast<const Shape3D&>(colliderA.shape).as<TriangleMeshShape3D>();
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Length3D meshLocalPlaneOffset = inverseTransformationA(plane.point);
		const Direction3D meshLocalPlaneNormal = inverseTransformationA.getDirection(plane.normal);
		const TriangleMesh3D& mesh = *triangleMeshShapeA.getTriangleMesh();
		const Span<const TriangleMeshVertex3D> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		const Length1D maxCollisionDistance = min(-minPenetrationDepth, options.maxCollisionTouchingDistance);
		if (mesh.getFaceOrthtree().traverseElements(
				[&](const TriangleMeshFaceIndex& faceIndex) -> bool {
					const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
					const Array meshLocalTrianglePoints{
						vertices[indices[indexOffset + 0]] * Length3D::UNIT,
						vertices[indices[indexOffset + 1]] * Length3D::UNIT,
						vertices[indices[indexOffset + 2]] * Length3D::UNIT,
					};
					const Array signedTrianglePointDistances{
						dot(meshLocalTrianglePoints[0] - meshLocalPlaneOffset, meshLocalPlaneNormal),
						dot(meshLocalTrianglePoints[1] - meshLocalPlaneOffset, meshLocalPlaneNormal),
						dot(meshLocalTrianglePoints[2] - meshLocalPlaneOffset, meshLocalPlaneNormal),
					};
					const auto itFarSignedTrianglePointDistance = maxElement(signedTrianglePointDistances);
					const auto itNearSignedTrianglePointDistance = minElement(signedTrianglePointDistances);
					const Length1D nearSignedDistance = *itNearSignedTrianglePointDistance;
					const Length1D farSignedDistance = -*itFarSignedTrianglePointDistance;

					const auto [signedDistance, itPoint, normal] =
						(nearSignedDistance > farSignedDistance)
							? Tuple{nearSignedDistance, itNearSignedTrianglePointDistance, -plane.normal}
							: Tuple{farSignedDistance, itFarSignedTrianglePointDistance, plane.normal};
					return signedDistance <= maxCollisionDistance;
				},
				[&](const grem::Box<3, float>& meshLocalBoundingBox) -> bool {
					const Length3D meshLocalMin = meshLocalBoundingBox.min * Length3D::UNIT;
					const Length3D meshLocalMax = meshLocalBoundingBox.max * Length3D::UNIT;
					const Length3D meshLocalCenter = midpoint(meshLocalMin, meshLocalMax);
					const Length3D halfExtents = (meshLocalMax - meshLocalMin) * 0.5f;
					const Length3D meshLocalNearPoint = meshLocalCenter + copysign(halfExtents, -meshLocalPlaneNormal);
					const Length3D meshLocalFarPoint = meshLocalCenter + copysign(halfExtents, meshLocalPlaneNormal);
					return (dot(meshLocalNearPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= maxCollisionDistance) !=
			               (dot(meshLocalFarPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= maxCollisionDistance);
				})) {
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

		const TriangleMeshShape3D& triangleMeshShapeA = static_cast<const Shape3D&>(colliderA.shape).as<TriangleMeshShape3D>();
		GREM_ASSERT(static_cast<const Shape3D&>(colliderB.shape).is<InfinitePlaneShape3D>());

		const InverseTransformation3D inverseTransformationA = inverse(transformationA);
		const InverseTransformation3D inverseTransformationB = inverse(transformationB);

		const Plane3D plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS_3D)};
		const Length3D meshLocalPlaneOffset = inverseTransformationA(plane.point);
		const Direction3D meshLocalPlaneNormal = inverseTransformationA.getDirection(plane.normal);
		const TriangleMesh3D& mesh = *triangleMeshShapeA.getTriangleMesh();
		const Span<const TriangleMeshVertex3D> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		mesh.getFaceOrthtree().traverseElements(
			[&](const TriangleMeshFaceIndex& faceIndex) -> void {
				const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
				const Array meshLocalTrianglePoints{
					vertices[indices[indexOffset + 0]] * Length3D::UNIT,
					vertices[indices[indexOffset + 1]] * Length3D::UNIT,
					vertices[indices[indexOffset + 2]] * Length3D::UNIT,
				};
				const Array signedTrianglePointDistances{
					dot(meshLocalTrianglePoints[0] - meshLocalPlaneOffset, meshLocalPlaneNormal),
					dot(meshLocalTrianglePoints[1] - meshLocalPlaneOffset, meshLocalPlaneNormal),
					dot(meshLocalTrianglePoints[2] - meshLocalPlaneOffset, meshLocalPlaneNormal),
				};
				const auto itFarSignedTrianglePointDistance = maxElement(signedTrianglePointDistances);
				const auto itNearSignedTrianglePointDistance = minElement(signedTrianglePointDistances);
				const Length1D nearSignedDistance = *itNearSignedTrianglePointDistance;
				const Length1D farSignedDistance = -*itFarSignedTrianglePointDistance;

				const auto [signedDistance, itPoint, normal] =
					(nearSignedDistance > farSignedDistance)
						? Tuple{nearSignedDistance, itNearSignedTrianglePointDistance, -plane.normal}
						: Tuple{farSignedDistance, itFarSignedTrianglePointDistance, plane.normal};
				if (signedDistance <= options.maxCollisionTouchingDistance) {
					const Position3D pointA = transformationA(meshLocalTrianglePoints[static_cast<size_t>(itPoint - signedTrianglePointDistances.begin())]);
					const Position3D pointB = pointA + normal * signedDistance;
					callback(CollisionAlgorithmResult3D{
						.manifold = convertPointsToContactManifold(pointA, inverseTransformationA, ContactFeatureType::TRIANGLE_MESH_FACE, faceIndex, pointB,
							inverseTransformationB, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, normal, filterTestResult),
					});
				}
			},
			[&](const grem::Box<3, float>& meshLocalBoundingBox) -> bool {
				const Length3D meshLocalMin = meshLocalBoundingBox.min * Length3D::UNIT;
				const Length3D meshLocalMax = meshLocalBoundingBox.max * Length3D::UNIT;
				const Length3D meshLocalCenter = midpoint(meshLocalMin, meshLocalMax);
				const Length3D halfExtents = (meshLocalMax - meshLocalMin) * 0.5f;
				const Length3D meshLocalNearPoint = meshLocalCenter + copysign(halfExtents, -meshLocalPlaneNormal);
				const Length3D meshLocalFarPoint = meshLocalCenter + copysign(halfExtents, meshLocalPlaneNormal);
				return (dot(meshLocalNearPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= options.maxCollisionTouchingDistance) !=
			           (dot(meshLocalFarPoint - meshLocalPlaneOffset, meshLocalPlaneNormal) <= options.maxCollisionTouchingDistance);
			});
	}
};

template <size_t N>
struct choose_collision_detector<N, TriangleMeshShape<N>, InfinitePlaneShape3D> {
	using type = CollisionDetector_triangle_mesh_vs_infinite_plane_3d;
};

template <size_t N>
struct choose_collision_detector<N, InfinitePlaneShape3D, TriangleMeshShape<N>> {
	using type = ReversedCollisionDetector<N, CollisionDetector_triangle_mesh_vs_infinite_plane_3d>;
};

} // namespace grem::physics

#endif
