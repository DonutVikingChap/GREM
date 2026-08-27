// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_ANY_CONVEX_POLYTOPE_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_ANY_CONVEX_POLYTOPE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"
#include "CollisionDetector_any_convex_vs_any_convex.hpp"
#include "SeparatingAxisTestAlgorithm.hpp"
#include "convex_polytope_contact.hpp"

namespace grem::physics {

// General collision detector that supports any pair of convex polytope shapes, where both are made of flat surfaces.
// Uses SAT, which should produce relatively stable contacts.
template <size_t N>
class CollisionDetector_any_convex_polytope_vs_any_convex_polytope final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		return CollisionDetector_any_convex_vs_any_convex<N>{}.hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA, transformationA, colliderB, transformationB,
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
		const ConvexPolytopeShapeView<N> convexPolytopeShapeB{colliderB.shape};

		if (convexPolytopeShapeA.getFaceCount() == 0 || convexPolytopeShapeB.getFaceCount() == 0) {
			return;
		}
		if constexpr (N == 3) {
			if (convexPolytopeShapeA.getEdgeCount() == 0 || convexPolytopeShapeB.getEdgeCount() == 0) {
				return;
			}
		}

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		if (satResult) {
			GREM_MATCH(*satResult) {
				GREM_CASE(const typename SAT::FaceSeparationA& previousFaceSeparationA) {
					if (SAT{}
							.getFacePenetration(convexPolytopeShapeA, transformationA, previousFaceSeparationA.faceIndex, convexPolytopeShapeB, transformationB,
								inverseTransformationB)
							.penetrationDepth < -options.maxCollisionTouchingDistance) {
						return;
					}
					break;
				}
				GREM_CASE(const typename SAT::FaceSeparationB& previousFaceSeparationB) {
					if (SAT{}
							.getFacePenetration(convexPolytopeShapeB, transformationB, previousFaceSeparationB.faceIndex, convexPolytopeShapeA, transformationA,
								inverseTransformationA)
							.penetrationDepth < -options.maxCollisionTouchingDistance) {
						return;
					}
					break;
				}
				GREM_CASE(const typename SAT::EdgeSeparation& previousEdgeSeparation) {
					if constexpr (N == 3) {
						if (SAT{}
								.getEdgePenetration(convexPolytopeShapeA, transformationA, inverseTransformationA, previousEdgeSeparation.edgeIndices.first, convexPolytopeShapeB,
									transformationB, inverseTransformationB, previousEdgeSeparation.edgeIndices.second)
								.penetrationDepth < -options.maxCollisionTouchingDistance) {
							return;
						}
					} else {
						unreachable();
					}
					break;
				}
				GREM_CASE(const typename SAT::FaceCollision& previousFaces) {
					const typename SAT::FacePenetrationResult facePenetrationA = SAT{}.getFacePenetration(convexPolytopeShapeA, transformationA, previousFaces.faceIndices.first,
						convexPolytopeShapeB, transformationB, inverseTransformationB);
					if (facePenetrationA.penetrationDepth < -options.maxCollisionTouchingDistance) {
						break;
					}
					const typename SAT::FacePenetrationResult facePenetrationB = SAT{}.getFacePenetration(convexPolytopeShapeB, transformationB, previousFaces.faceIndices.second,
						convexPolytopeShapeA, transformationA, inverseTransformationA);
					if (facePenetrationB.penetrationDepth < -options.maxCollisionTouchingDistance) {
						break;
					}
					const Optional<ContactManifold<N>> manifold = createFaceContact(temporaryMemoryResource, latestIncidentFaceIndices, convexPolytopeShapeA, transformationA,
						inverseTransformationA, previousFaces.faceIndices.first, facePenetrationA.normal, facePenetrationA.penetrationDepth, convexPolytopeShapeB, transformationB,
						inverseTransformationB, previousFaces.faceIndices.second, facePenetrationB.normal, facePenetrationB.penetrationDepth, options, filterTestResult);
					if (manifold && manifold->points.size() >= previousContactPointCount) {
						previousContactPointCount = manifold->points.size();
						callback(CollisionAlgorithmResult<N>{.manifold = *manifold});
						return;
					}
					break;
				}
				GREM_CASE(const typename SAT::EdgeCollision& previousEdges) {
					if constexpr (N == 3) {
						const typename SAT::EdgePenetrationResult edgePenetration = SAT{}.getEdgePenetration(convexPolytopeShapeA, transformationA, inverseTransformationA,
							previousEdges.edgeIndices.first, convexPolytopeShapeB, transformationB, inverseTransformationB, previousEdges.edgeIndices.second);
						if (edgePenetration.penetrationDepth > options.maxCollisionTouchingDistance || edgePenetration.penetrationDepth < -options.maxCollisionTouchingDistance) {
							break;
						}

						const ConvexPolytopeFaceIndex faceIndexA = convexPolytopeShapeA.getFaceIndexOfEdge(previousEdges.edgeIndices.first);
						if (SAT{}.getFacePenetration(convexPolytopeShapeA, transformationA, faceIndexA, convexPolytopeShapeB, transformationB, inverseTransformationB)
									.penetrationDepth -
								options.biasFaceOverEdge <
							edgePenetration.penetrationDepth) {
							break;
						}

						const ConvexPolytopeFaceIndex twinFaceIndexA = convexPolytopeShapeA.getFaceIndexOfEdge(previousEdges.edgeIndices.first ^ 1);
						if (SAT{}.getFacePenetration(convexPolytopeShapeA, transformationA, twinFaceIndexA, convexPolytopeShapeB, transformationB, inverseTransformationB)
									.penetrationDepth -
								options.biasFaceOverEdge <
							edgePenetration.penetrationDepth) {
							break;
						}

						const ConvexPolytopeFaceIndex faceIndexB = convexPolytopeShapeB.getFaceIndexOfEdge(previousEdges.edgeIndices.second);
						if (SAT{}.getFacePenetration(convexPolytopeShapeB, transformationB, faceIndexB, convexPolytopeShapeA, transformationA, inverseTransformationA)
									.penetrationDepth -
								options.biasFaceOverEdge <
							edgePenetration.penetrationDepth) {
							break;
						}

						const ConvexPolytopeFaceIndex twinFaceIndexB = convexPolytopeShapeB.getFaceIndexOfEdge(previousEdges.edgeIndices.second ^ 1);
						if (SAT{}.getFacePenetration(convexPolytopeShapeB, transformationB, twinFaceIndexB, convexPolytopeShapeA, transformationA, inverseTransformationA)
									.penetrationDepth -
								options.biasFaceOverEdge <
							edgePenetration.penetrationDepth) {
							break;
						}

						const Optional<ContactManifold3D> manifold = createEdgeContact(convexPolytopeShapeA, transformationA, inverseTransformationA,
							previousEdges.edgeIndices.first, convexPolytopeShapeB, transformationB, inverseTransformationB, previousEdges.edgeIndices.second,
							Direction3D::reinterpret(edgePenetration.normal), filterTestResult);
						if (manifold && manifold->points.size() >= previousContactPointCount) {
							previousContactPointCount = manifold->points.size();
							callback(CollisionAlgorithmResult3D{.manifold = *manifold});
							return;
						}
					} else {
						unreachable();
					}
					break;
				}
			}
		}

		satResult = SAT{}.findPenetrationMinimizingFeatures(convexPolytopeShapeA, transformationA, inverseTransformationA, convexPolytopeShapeB, transformationB,
			inverseTransformationB, options);
		GREM_MATCH(*satResult) {
			GREM_CASE(const typename SAT::FaceSeparationA& faceSeparationA) {
				previousContactPointCount = 0;
				break;
			}
			GREM_CASE(const typename SAT::FaceSeparationB& faceSeparationB) {
				previousContactPointCount = 0;
				break;
			}
			GREM_CASE(const typename SAT::EdgeSeparation& edgeSeparation) {
				if constexpr (N == 3) {
					previousContactPointCount = 0;
				} else {
					unreachable();
				}
				break;
			}
			GREM_CASE(const typename SAT::FaceCollision& faces) {
				if (const Optional<ContactManifold<N>> manifold = createFaceContact(temporaryMemoryResource, latestIncidentFaceIndices, convexPolytopeShapeA, transformationA,
						inverseTransformationA, faces.faceIndices.first, faces.normals.first, faces.penetrationDepths.first, convexPolytopeShapeB, transformationB,
						inverseTransformationB, faces.faceIndices.second, faces.normals.second, faces.penetrationDepths.second, options, filterTestResult)) {
					previousContactPointCount = manifold->points.size();
					callback(CollisionAlgorithmResult<N>{.manifold = *manifold});
				} else {
					previousContactPointCount = 0;
				}
				break;
			}
			GREM_CASE(const typename SAT::EdgeCollision& edges) {
				if constexpr (N == 3) {
					if (const Optional<ContactManifold3D> manifold = createEdgeContact(convexPolytopeShapeA, transformationA, inverseTransformationA, edges.edgeIndices.first,
							convexPolytopeShapeB, transformationB, inverseTransformationB, edges.edgeIndices.second, edges.normal, filterTestResult)) {
						previousContactPointCount = manifold->points.size();
						callback(CollisionAlgorithmResult3D{.manifold = *manifold});
					} else {
						previousContactPointCount = 0;
					}
				} else {
					unreachable();
				}
				break;
			}
		}
	}

private:
	using SAT = SeparatingAxisTestAlgorithm<N>;

	Optional<typename SAT::FindPenetrationMinimizingFeaturesResult> satResult{};
	Pair<ConvexPolytopeFaceIndex> latestIncidentFaceIndices{};
	size_t previousContactPointCount = 0;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeA, convex_polytope_shape<N> ConvexPolytopeShapeB>
struct choose_collision_detector<N, ConvexPolytopeShapeA, ConvexPolytopeShapeB> {
	using type = CollisionDetector_any_convex_polytope_vs_any_convex_polytope<N>;
};

} // namespace grem::physics

#endif
