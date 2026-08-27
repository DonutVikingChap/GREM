// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_INFINITE_HALF_SPACE_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_ANY_CONVEX_POLYTOPE_VS_INFINITE_HALF_SPACE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
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
#include "convex_polytope_contact.hpp"

namespace grem::physics {

// Collision detector for any convex polytope shape made of flat surfaces VS InfiniteHalfSpaceShape.
// Uses the deepest face on the shape to the plane to produce a stable contact.
template <size_t N>
class CollisionDetector_any_convex_polytope_vs_infinite_half_space final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D minPenetrationDepth, ColliderView<N> colliderA, const Transformation<N>& transformationA,
		ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest) override {
		GREM_PROFILE_FUNCTION();

		const CollisionFilterTestResult filterTestResult = filterTest(colliderA.filter, colliderB.filter);
		if (!filterTestResult) {
			return {};
		}

		GREM_ASSERT(colliderA.shape.isConvexPolytopeShapeType());
		const ConvexPolytopeShapeView<N> convexPolytopeShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Direction<N> localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const ConvexPolytopeVertexIndex vertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(localDirectionA, latestVertexIndexA);
		latestVertexIndexA = vertexIndexA;
		const Position<N> pointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(vertexIndexA));

		const Length1D signedDistance = dot(pointA - plane.point, plane.normal);
		if (signedDistance <= min(-minPenetrationDepth, options.maxCollisionTouchingDistance)) {
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

		GREM_ASSERT(colliderA.shape.isConvexPolytopeShapeType());
		const ConvexPolytopeShapeView<N> convexPolytopeShapeA{colliderA.shape};
		GREM_ASSERT(static_cast<const Shape<N>&>(colliderB.shape).template is<InfiniteHalfSpaceShape<N>>());

		const InverseTransformation<N> inverseTransformationA = inverse(transformationA);
		const InverseTransformation<N> inverseTransformationB = inverse(transformationB);

		const Plane<N> plane{.point = transformationB.getOrigin(), .normal = transformationB.getDirection(Y_AXIS<N>)};
		const Direction<N> localDirectionA = inverseTransformationA.getDirection(-plane.normal);
		const ConvexPolytopeVertexIndex vertexIndexA = convexPolytopeShapeA.getLocalSupportPointVertexIndex(localDirectionA, latestVertexIndexA);
		latestVertexIndexA = vertexIndexA;
		const Position<N> pointA = transformationA(convexPolytopeShapeA.getLocalVertexOffset(vertexIndexA));

		const Length1D signedDistance = dot(pointA - plane.point, plane.normal);
		if (signedDistance <= options.maxCollisionTouchingDistance) {
			Optional<ConvexPolytopeFaceIndex> faceIndexA{};
			Scale1D largestDotProduct = Scale1D::MIN;
			forEachFaceIndexAroundVertex(convexPolytopeShapeA, vertexIndexA, [&](ConvexPolytopeFaceIndex faceIndex) -> void {
				const Scale1D dotProduct = dot(localDirectionA, convexPolytopeShapeA.getLocalFaceNormal(faceIndex));
				if (dotProduct > largestDotProduct) {
					faceIndexA = faceIndex;
					largestDotProduct = dotProduct;
				}
			});

			if (faceIndexA) {
				ArrayList<Position<N>, ArenaAllocator<Position<N>>> incidentPoints = getFacePoints(temporaryMemoryResource, *faceIndexA, convexPolytopeShapeA, transformationA);
				ArrayList<Position<N>, ArenaAllocator<Position<N>>> referencePoints =
					cullPointsAbovePlaneAndProjectOthersOntoIt(temporaryMemoryResource, incidentPoints, plane, options.maxCollisionTouchingDistance);
				if (!referencePoints.empty()) {
					reducePointsToSimplifiedManifold(referencePoints, incidentPoints, plane);
					callback(CollisionAlgorithmResult<N>{
						.manifold = convertPointsToContactManifold<N>(incidentPoints, inverseTransformationA, ContactFeatureType::CONVEX_POLYTOPE_FACE, *faceIndexA,
							referencePoints, inverseTransformationB, ContactFeatureType::GENERIC_CONVEX_SURFACE, 0, -plane.normal, filterTestResult),
					});
				}
			}
		}
	}

private:
	ConvexPolytopeVertexIndex latestVertexIndexA = 0;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeA>
struct choose_collision_detector<N, ConvexPolytopeShapeA, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_any_convex_polytope_vs_infinite_half_space<N>;
};

template <size_t N, convex_polytope_shape<N> ConvexPolytopeShapeB>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, ConvexPolytopeShapeB> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_convex_polytope_vs_infinite_half_space<N>>;
};

} // namespace grem::physics

#endif
