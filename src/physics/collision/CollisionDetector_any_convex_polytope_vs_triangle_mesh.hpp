// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_TRIANGLE_MESH_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_TRIANGLE_MESH_HPP

#include <GREM/build_config.hpp>

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
#include "CollisionDetector_any_convex_vs_triangle_mesh.hpp"
#include "SeparatingAxisTestAlgorithm.hpp"
#include "convex_polytope_contact.hpp"

namespace grem::physics {

// Collision detector for any convex polytope shape VS TriangleMeshShape.
template <size_t N>
class CollisionDetector_any_convex_polytope_vs_triangle_mesh final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		return CollisionDetector_any_convex_vs_triangle_mesh<N>{}.hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA, transformationA, colliderB, transformationB,
			options, filterTest);
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return;
		}

		const ConvexPolytopeShapeView<N> convexPolytopeShapeA{colliderA.shape};
		if (convexPolytopeShapeA.getFaceCount() == 0) {
			return;
		}
		if constexpr (N == 3) {
			if (convexPolytopeShapeA.getEdgeCount() == 0) {
				return;
			}
		}

		const TriangleMeshShape<N>& triangleMeshShapeB = static_cast<const Shape<N>&>(colliderB.shape).template as<TriangleMeshShape<N>>();

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const TriangleMesh<N>& mesh = *triangleMeshShapeB.getTriangleMesh();
		const Span<const TriangleMeshVertex<N>> vertices = mesh.getVertices();
		const Span<const TriangleMeshVertexIndex> indices = mesh.getIndices();
		const Optional<Box<N>> meshLocalBounds = convexPolytopeShapeA.getBoundingBox(Transformation<N>{0, inverseTransformationB * transformationA});
		const Box<N> expandedMeshLocalAABB =
			(meshLocalBounds) ? meshLocalBounds->getExpanded(options.maxCollisionTouchingDistance) : Box<N>{.min = Position<N>::MIN, .max = Position<N>::MAX};
		mesh.getFaceOrthtree().test(expandedMeshLocalAABB.in(Box<N>::UNIT), [&](const TriangleMeshFaceIndex& faceIndex) -> void {
			const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
			const Array meshLocalTrianglePoints{
				vertices[indices[indexOffset + 0]] * Length<N>::UNIT,
				vertices[indices[indexOffset + 1]] * Length<N>::UNIT,
				vertices[indices[indexOffset + 2]] * Length<N>::UNIT,
			};
			const Triangle<N> meshLocalTriangle{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};
			if (intersects(meshLocalTriangle.getBoundingBox(), expandedMeshLocalAABB)) {
				const detail::TriangleShape<N> triangleShapeB{.pointA = meshLocalTrianglePoints[0], .pointB = meshLocalTrianglePoints[1], .pointC = meshLocalTrianglePoints[2]};

				const auto satResult = SAT{}.findPenetrationMinimizingFeatures(convexPolytopeShapeA, transformationA, inverseTransformationA, triangleShapeB, transformationB,
					inverseTransformationB, options);
				GREM_MATCH(satResult) {
					GREM_CASE(const typename SAT::FaceSeparationA& faceSeparationA) break;
					GREM_CASE(const typename SAT::FaceSeparationB& faceSeparationB) break;
					GREM_CASE(const typename SAT::EdgeSeparation& edgeSeparation) {
						if constexpr (N != 3) {
							unreachable();
						}
						break;
					}
					GREM_CASE(const typename SAT::FaceCollision& faces) {
						if (Optional<ContactManifold<N>> manifold = createFaceContact(temporaryMemoryResource, latestIncidentFaceIndices, convexPolytopeShapeA, transformationA,
								inverseTransformationA, faces.faceIndices.first, faces.normals.first, faces.penetrationDepths.first, triangleShapeB, transformationB,
								inverseTransformationB, faces.faceIndices.second, faces.normals.second, faces.penetrationDepths.second, options, filterTestResult)) {
							manifold->featureTypes.second = ContactFeatureType::TRIANGLE_MESH_FACE;
							manifold->featureIndices.second = faceIndex;
							callback(CollisionAlgorithmResult<N>{.manifold = *manifold});
						}
						break;
					}
					GREM_CASE(const typename SAT::EdgeCollision& edges) {
						if constexpr (N == 3) {
							if (Optional<ContactManifold<N>> manifold = createEdgeContact(convexPolytopeShapeA, transformationA, inverseTransformationA, edges.edgeIndices.first,
									triangleShapeB, transformationB, inverseTransformationB, edges.edgeIndices.second, edges.normal, filterTestResult)) {
								manifold->featureTypes.second = ContactFeatureType::TRIANGLE_MESH_EDGE;
								manifold->featureIndices.second = static_cast<uint32_t>(indexOffset + static_cast<size_t>(manifold->featureIndices.second >> 1));
								callback(CollisionAlgorithmResult<N>{.manifold = *manifold});
							}
						} else {
							unreachable();
						}
						break;
					}
				}
			}
		});
	}

private:
	using SAT = SeparatingAxisTestAlgorithm<N>;

	Pair<ConvexPolytopeFaceIndex> latestIncidentFaceIndices{};
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeA>
struct choose_collision_detector<N, ConvexPolytopeShapeA, TriangleMeshShape<N>> {
	using type = CollisionDetector_any_convex_polytope_vs_triangle_mesh<N>;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeB>
struct choose_collision_detector<N, TriangleMeshShape<N>, ConvexPolytopeShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_polytope_vs_triangle_mesh<N>>;
};

} // namespace grem::physics

#endif
