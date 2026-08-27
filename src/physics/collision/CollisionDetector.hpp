// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

template <size_t N, typename ShapeA, typename ShapeB>
struct choose_collision_detector {
	static_assert(N == 0, "Missing collision detector specialization for the given pair of shape types.");
};

template <size_t N, typename BaseCollisionDetector>
class ReversedCollisionDetector final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) override {
		return base.hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderB, transformationB, colliderA, transformationA, options, filterTest);
	}

	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) override {
		base.detectCollisions(temporaryMemoryResource, colliderB, transformationB, colliderA, transformationA, options, filterTest,
			[&](const CollisionAlgorithmResult<N>& collision) -> void {
				GREM_ASSERT(collision.manifold.rollingResistanceMomentum == 0);
				GREM_ASSERT(collision.manifold.rollingResistanceImpulse == 0);
				CollisionAlgorithmResult<N> reversedCollision{.manifold{
					.featureTypes = reversed(collision.manifold.featureTypes),
					.featureIndices = reversed(collision.manifold.featureIndices),
					.points = collision.manifold.points,
					.normal = -collision.manifold.normal,
					.filterTestResult = collision.manifold.filterTestResult,
				}};
				for (ContactPoint<N>& point : reversedCollision.manifold.points) {
					GREM_ASSERT(point.tangent == 0);
					GREM_ASSERT(point.relativeVelocityInTangentSpace == 0);
					GREM_ASSERT(point.momentumInTangentSpace == 0);
					GREM_ASSERT(point.impulseInTangentSpace == 0);
					GREM_ASSERT(point.restitutionMomentum == 0);
					GREM_ASSERT(point.restitutionImpulse == 0);
					point.offsets = reversed(point.offsets);
					point.localOffsets = reversed(point.localOffsets);
				}
				callback(reversedCollision);
			});
	}

private:
	BaseCollisionDetector base{};
};

template <size_t N>
[[nodiscard]] inline ArrayList<Position<N>, ArenaAllocator<Position<N>>> getFacePoints(ArenaResource* memoryResource, ConvexPolytopeFaceIndex faceIndex,
	const convex_polytope_shape<N> auto& shape, const Transformation<N>& transformation) {
	ArrayList<Position<N>, ArenaAllocator<Position<N>>> result{memoryResource};
	if constexpr (N == 3) {
		result.reserve(4);
	} else {
		result.reserve(2);
	}
	forEachVertexIndexInFace(shape, faceIndex, [&](ConvexPolytopeVertexIndex vertexIndex) -> void { //
		result.push_back(transformation(shape.getLocalVertexOffset(vertexIndex)));
	});
	return result;
}

template <size_t N>
[[nodiscard]] inline ContactManifold<N> convertPointsToContactManifold(Span<const Position<N>> pointsA, const InverseTransformation<N>& inverseTransformationA,
	ContactFeatureType featureTypeA, uint32_t featureIndexA, Span<const Position<N>> pointsB, const InverseTransformation<N>& inverseTransformationB,
	ContactFeatureType featureTypeB, uint32_t featureIndexB, Direction<N> normal, CollisionFilterTestResult filterTestResult) {
	GREM_ASSERT(pointsA.size() == pointsB.size());
	const size_t pointCount = pointsA.size();
	GREM_ASSERT(pointCount > 0);
	ContactManifold<N> result{
		.featureTypes{featureTypeA, featureTypeB},
		.featureIndices{featureIndexA, featureIndexB},
		.points{},
		.normal = normal,
		.filterTestResult = filterTestResult,
	};
	for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
		const Position<N> pointA = pointsA[pointIndex];
		const Position<N> pointB = pointsB[pointIndex];
		const Length<N> offsetA = pointA - inverseTransformationA.getOriginalOrigin();
		const Length<N> offsetB = pointB - inverseTransformationB.getOriginalOrigin();
		const Length<N> localOffsetA = inverseTransformationA.getBasis() * offsetA;
		const Length<N> localOffsetB = inverseTransformationB.getBasis() * offsetB;
		result.points.push_back(ContactPoint<N>{.offsets{offsetA, offsetB}, .localOffsets{localOffsetA, localOffsetB}});
	}
	return result;
}

template <size_t N>
[[nodiscard]] inline ContactManifold<N> convertPointsToContactManifold(Position<N> pointA, const InverseTransformation<N>& inverseTransformationA, ContactFeatureType featureTypeA,
	uint32_t featureIndexA, Position<N> pointB, const InverseTransformation<N>& inverseTransformationB, ContactFeatureType featureTypeB, uint32_t featureIndexB,
	Direction<N> normal, CollisionFilterTestResult filterTestResult) {
	const Length<N> offsetA = pointA - inverseTransformationA.getOriginalOrigin();
	const Length<N> offsetB = pointB - inverseTransformationB.getOriginalOrigin();
	const Length<N> localOffsetA = inverseTransformationA.getBasis() * offsetA;
	const Length<N> localOffsetB = inverseTransformationB.getBasis() * offsetB;
	return ContactManifold<N>{
		.featureTypes{featureTypeA, featureTypeB},
		.featureIndices{featureIndexA, featureIndexB},
		.points{ContactPoint<N>{.offsets{offsetA, offsetB}, .localOffsets{localOffsetA, localOffsetB}}},
		.normal = normal,
		.filterTestResult = filterTestResult,
	};
}

} // namespace grem::physics

#endif
