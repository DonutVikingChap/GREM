// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_TRIANGLE_MESH_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_VS_TRIANGLE_MESH_HPP

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
#include "ExpandingPolytopeAlgorithm.hpp"
#include "GilbertJohnsonKeerthiAlgorithm.hpp"

namespace grem::physics {

// Collision detector for any convex shape VS TriangleMeshShape.
template <size_t N>
class CollisionDetector_any_convex_vs_triangle_mesh final : public CollisionAlgorithmImplementation<N> {
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
		const TriangleMeshShape<N>& triangleMeshShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<TriangleMeshShape<N>>();

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const TriangleMesh<N>& mesh = *triangleMeshShapeB.getTriangleMesh();
		const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		const Length1D maxCollisionDistance = min(-minPenetrationDepth, options.maxCollisionTouchingDistance);
		const Distance margin = max(options.collisionDistanceErrorTolerance, maxCollisionDistance);
		const Optional<Box<N>> meshLocalBounds = convexShapeA.getBoundingBox(Transformation<N>{0, inverseTransformationB * transformationA});
		const Box<N> meshLocalAABB = (meshLocalBounds) ? *meshLocalBounds : Box<N>{.min = Position<N>::MIN, .max = Position<N>::MAX};
		if (mesh.getFaceOrthtree().test(meshLocalAABB.in(Box<N>::UNIT), [&](const TriangleMeshFaceIndex& faceIndex) {
				const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
				const Array meshLocalTrianglePoints{
					vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
					vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
					vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
				};
				const Triangle<N> meshLocalTriangle{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
				if (intersects(meshLocalTriangle.getBoundingBox(), meshLocalAABB)) {
					const detail::TriangleShape<N> triangleShapeB{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
					const typename GJK::FindAnyIntersectionResult gjkResult = GJK{}.findAnyIntersection(convexShapeA, transformationA, inverseTransformationA, triangleShapeB,
						transformationB, inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxGJKIterationCount);
					if (gjkResult.foundIntersection) {
						if (maxCollisionDistance == 0) {
							return true;
						}
						const typename EPA::FindDeepestPenetrationResult epaResult = EPA{}.findDeepestPenetration(temporaryMemoryResource,
							Span{gjkResult.simplex}.template first<N + 1>(), convexShapeA, transformationA, inverseTransformationA, triangleShapeB, transformationB,
							inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxEPAIterationCount);
						return epaResult.depth >= -maxCollisionDistance;
					}
				}
				return false;
			})) {
			return filterTestResult;
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
		const TriangleMeshShape<N>& triangleMeshShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<TriangleMeshShape<N>>();

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const TriangleMesh<N>& mesh = *triangleMeshShapeB.getTriangleMesh();
		const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		const Distance margin = max(options.collisionDistanceErrorTolerance, options.maxCollisionTouchingDistance);
		const Optional<Box<N>> meshLocalBounds = convexShapeA.getBoundingBox(Transformation<N>{0, inverseTransformationB * transformationA});
		const Box<N> meshLocalAABB = (meshLocalBounds) ? *meshLocalBounds : Box<N>{.min = Position<N>::MIN, .max = Position<N>::MAX};
		mesh.getFaceOrthtree().test(meshLocalAABB.in(Box<N>::UNIT), [&](const TriangleMeshFaceIndex& faceIndex) -> void {
			const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
			const Array meshLocalTrianglePoints{
				vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
				vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
				vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
			};
			const Triangle<N> meshLocalTriangle{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
			if (intersects(meshLocalTriangle.getBoundingBox(), meshLocalAABB)) {
				const detail::TriangleShape<N> triangleShapeB{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
				const typename GJK::FindAnyIntersectionResult gjkResult = GJK{}.findAnyIntersection(convexShapeA, transformationA, inverseTransformationA, triangleShapeB,
					transformationB, inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxGJKIterationCount);
				if (gjkResult.foundIntersection) {
					const typename EPA::FindDeepestPenetrationResult epaResult = EPA{}.findDeepestPenetration(temporaryMemoryResource,
						Span{gjkResult.simplex}.template first<N + 1>(), convexShapeA, transformationA, inverseTransformationA, triangleShapeB, transformationB,
						inverseTransformationB, margin, options.collisionDistanceErrorTolerance, options.maxEPAIterationCount);
					if (epaResult.depth >= -options.maxCollisionTouchingDistance) {
						callback(CollisionAlgorithmResult<N>{
							.manifold = convertPointsToContactManifold(epaResult.witnessPoints.first, inverseTransformationA, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0,
								epaResult.witnessPoints.second, inverseTransformationB, ContactFeatureType::TRIANGLE_MESH_FACE, faceIndex, epaResult.normal, filterTestResult),
						});
					}
				}
			}
		});
	}

private:
	using GJK = GilbertJohnsonKeerthiAlgorithm<N>;
	using EPA = ExpandingPolytopeAlgorithm<N>;
};

template <size_t N, convex_shape<N> ConvexShapeA>
struct choose_collision_detector<N, ConvexShapeA, TriangleMeshShape<N>> {
	using type = CollisionDetector_any_convex_vs_triangle_mesh<N>;
};

template <size_t N, convex_shape<N> ConvexShapeB>
struct choose_collision_detector<N, TriangleMeshShape<N>, ConvexShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_vs_triangle_mesh<N>>;
};

} // namespace grem::physics

#endif
