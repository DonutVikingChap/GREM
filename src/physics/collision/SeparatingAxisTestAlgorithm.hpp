// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_SEPARATING_AXIS_TEST_ALGORITHM_HPP
#define GREM_PHYSICS_COLLISION_SEPARATING_AXIS_TEST_ALGORITHM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

template <size_t N>
class SeparatingAxisTestAlgorithm {
public:
	struct FaceSeparationA {
		ConvexPolytopeFaceIndex faceIndex;
	};

	struct FaceSeparationB {
		ConvexPolytopeFaceIndex faceIndex;
	};

	struct EdgeSeparation {
		Pair<ConvexPolytopeEdgeIndex> edgeIndices;
	};

	struct FaceCollision {
		Pair<ConvexPolytopeFaceIndex> faceIndices;
		Pair<Direction<N>> normals;
		Pair<Length1D> penetrationDepths;
	};

	struct EdgeCollision {
		Pair<ConvexPolytopeEdgeIndex> edgeIndices;
		Direction<N> normal;
		Length1D penetrationDepth;
	};

	using FindPenetrationMinimizingFeaturesResult = Variant<FaceSeparationA, FaceSeparationB, EdgeSeparation, FaceCollision, EdgeCollision>;

	[[nodiscard]] FindPenetrationMinimizingFeaturesResult findPenetrationMinimizingFeatures(const convex_polytope_shape<N> auto& shapeA, const Transformation<N>& transformationA,
		const InverseTransformation<N>& inverseTransformationA, const convex_polytope_shape<N> auto& shapeB, const Transformation<N>& transformationB,
		const InverseTransformation<N>& inverseTransformationB, const CollisionAlgorithmOptions<N>& options) {
		GREM_PROFILE_FUNCTION();

		const FindPenetrationMinimizingFaceResult faceResultA = findPenetrationMinimizingFace(shapeA, transformationA, shapeB, transformationB, inverseTransformationB);
		if (faceResultA.penetrationDepth < -options.maxCollisionTouchingDistance) {
			return FaceSeparationA{.faceIndex = faceResultA.faceIndex};
		}

		const FindPenetrationMinimizingFaceResult faceResultB = findPenetrationMinimizingFace(shapeB, transformationB, shapeA, transformationA, inverseTransformationA);
		if (faceResultB.penetrationDepth < -options.maxCollisionTouchingDistance) {
			return FaceSeparationB{.faceIndex = faceResultB.faceIndex};
		}

		if constexpr (N == 3) {
			const FindPenetrationMinimizingEdgesResult edgeResult =
				findPenetrationMinimizingEdges(shapeA, transformationA, inverseTransformationA, shapeB, transformationB, inverseTransformationB);
			if (edgeResult.penetrationDepth < -options.maxCollisionTouchingDistance) {
				return EdgeSeparation{.edgeIndices = edgeResult.edgeIndices};
			}

			if ((edgeResult.penetrationDepth <= faceResultA.penetrationDepth - options.biasFaceOverEdge && //
					edgeResult.penetrationDepth <= faceResultB.penetrationDepth - options.biasFaceOverEdge)) {
				return EdgeCollision{
					.edgeIndices = edgeResult.edgeIndices,
					.normal = Direction3D::reinterpret(edgeResult.normal),
					.penetrationDepth = edgeResult.penetrationDepth,
				};
			}
		}

		return FaceCollision{
			.faceIndices{faceResultA.faceIndex, faceResultB.faceIndex},
			.normals{Direction<N>::reinterpret(faceResultA.normal), Direction<N>::reinterpret(faceResultB.normal)},
			.penetrationDepths{faceResultA.penetrationDepth, faceResultB.penetrationDepth},
		};
	}

	struct FacePenetrationResult {
		Direction<N> normal;
		Length1D penetrationDepth;
	};

	[[nodiscard]] GREM_ALWAYS_INLINE GREM_FLATTEN FacePenetrationResult getFacePenetration(const convex_polytope_shape<N> auto& shapeA, const Transformation<N>& transformationA,
		ConvexPolytopeFaceIndex faceIndexA, const convex_shape<N> auto& shapeB, const Transformation<N>& transformationB, const InverseTransformation<N>& inverseTransformationB) {
		const Position<N> facePointA = transformationA(shapeA.getLocalFaceOffset(faceIndexA));
		const Direction<N> faceNormalA = transformationA.getDirection(shapeA.getLocalFaceNormal(faceIndexA));
		const Position<N> pointB = transformationB(shapeB.getLocalSupportPointOffset(inverseTransformationB.getDirection(-faceNormalA)));
		const Length1D penetrationDepth = dot(faceNormalA, facePointA - pointB);
		return {.normal = faceNormalA, .penetrationDepth = penetrationDepth};
	}

	struct FindPenetrationMinimizingFaceResult {
		ConvexPolytopeFaceIndex faceIndex;
		Scale<N> normal;
		Length1D penetrationDepth;
	};

	[[nodiscard]] FindPenetrationMinimizingFaceResult findPenetrationMinimizingFace(const convex_polytope_shape<N> auto& shapeA, const Transformation<N>& transformationA,
		const convex_shape<N> auto& shapeB, const Transformation<N>& transformationB, const InverseTransformation<N>& inverseTransformationB) {
		GREM_PROFILE_FUNCTION();

		FindPenetrationMinimizingFaceResult result{
			.faceIndex = 0,
			.normal{},
			.penetrationDepth = Length1D::MAX,
		};
		const ConvexPolytopeFaceIndex faceCountA = shapeA.getFaceCount();
		GREM_ASSERT(faceCountA > 0);
		for (ConvexPolytopeFaceIndex faceIndexA = 0; faceIndexA < faceCountA; ++faceIndexA) {
			const FacePenetrationResult facePenetration = getFacePenetration(shapeA, transformationA, faceIndexA, shapeB, transformationB, inverseTransformationB);
			if (facePenetration.penetrationDepth < result.penetrationDepth) {
				result = {
					.faceIndex = faceIndexA,
					.normal = facePenetration.normal,
					.penetrationDepth = facePenetration.penetrationDepth,
				};
			}
		}
		return result;
	}

	struct EdgePenetrationResult {
		Scale3D normal;
		Length1D penetrationDepth;
	};

	[[nodiscard]] GREM_ALWAYS_INLINE GREM_FLATTEN EdgePenetrationResult getEdgePenetration(const convex_polytope_shape_3d auto& shapeA, const Transformation3D& transformationA,
		const InverseTransformation3D& inverseTransformationA, ConvexPolytopeEdgeIndex edgeIndexA, const convex_polytope_shape_3d auto& shapeB,
		const Transformation3D& transformationB, const InverseTransformation3D& inverseTransformationB, ConvexPolytopeEdgeIndex edgeIndexB) requires(N == 3) {
		const Length3D localEdgeOriginA = shapeA.getLocalVertexOffset(shapeA.getFirstVertexIndexOfEdge(edgeIndexA));
		const Length3D localEdgeTargetA = shapeA.getLocalVertexOffset(shapeA.getFirstVertexIndexOfEdge(edgeIndexA ^ 1));
		const Length3D localEdgeOriginB = shapeB.getLocalVertexOffset(shapeB.getFirstVertexIndexOfEdge(edgeIndexB));
		const Length3D localEdgeTargetB = shapeB.getLocalVertexOffset(shapeB.getFirstVertexIndexOfEdge(edgeIndexB ^ 1));
		const Length3D edgeOffsetA = transformationA.getRelative(localEdgeOriginA);
		const Length3D edgeOffsetB = transformationB.getRelative(localEdgeOriginB);
		const Position3D edgeOriginA = transformationA.getOrigin() + edgeOffsetA;
		const Position3D edgeOriginB = transformationB.getOrigin() + edgeOffsetB;
		const Position3D edgeTargetA = transformationA(localEdgeTargetA);
		const Position3D edgeTargetB = transformationB(localEdgeTargetB);
		const Length3D edgeVectorA = edgeTargetA - edgeOriginA;
		const Length3D edgeVectorB = edgeTargetB - edgeOriginB;
		const ConvexPolytopeFaceIndex faceIndexA = shapeA.getFaceIndexOfEdge(edgeIndexA);
		const ConvexPolytopeFaceIndex twinFaceIndexA = shapeA.getFaceIndexOfEdge(edgeIndexA ^ 1);
		const ConvexPolytopeFaceIndex faceIndexB = shapeB.getFaceIndexOfEdge(edgeIndexB);
		const ConvexPolytopeFaceIndex twinFaceIndexB = shapeB.getFaceIndexOfEdge(edgeIndexB ^ 1);
		const GaussMapArc arcA{
			.normals{
				transformationA.getBasis() * shapeA.getLocalFaceNormal(faceIndexA),
				transformationA.getBasis() * shapeA.getLocalFaceNormal(twinFaceIndexA),
			},
			.crossProduct = edgeVectorA,
		};
		const GaussMapArc arcB{
			.normals{
				-(transformationB.getBasis() * shapeB.getLocalFaceNormal(faceIndexB)),
				-(transformationB.getBasis() * shapeB.getLocalFaceNormal(twinFaceIndexB)),
			},
			.crossProduct = edgeVectorB,
		};
		if (!isMinkowskiFace(arcA, arcB)) {
			return {.normal{}, .penetrationDepth = Length1D::MAX};
		}

		const Optional<Direction3D> edgeDirectionA = tryNormalize(edgeVectorA);
		const Optional<Direction3D> edgeDirectionB = tryNormalize(edgeVectorB);
		if (!edgeDirectionA || !edgeDirectionB) {
			return {.normal{}, .penetrationDepth = Length1D::MAX};
		}

		const Optional<Direction3D> edgeCrossDirection = tryNormalize(cross(*edgeDirectionA, *edgeDirectionB));
		if (!edgeCrossDirection) {
			return {.normal{}, .penetrationDepth = Length1D::MAX};
		}

		const Scale3D bNPlaneNormal = cross(*edgeDirectionB, *edgeCrossDirection);
		const Scale3D aNPlaneNormal = cross(*edgeDirectionA, *edgeCrossDirection);
		const Length1D originADistanceToBPlane = dot(edgeOriginA - edgeOriginB, bNPlaneNormal);
		const Length1D targetADistanceToBPlane = dot(edgeTargetA - edgeOriginB, bNPlaneNormal);
		const Length1D originBDistanceToAPlane = dot(edgeOriginB - edgeOriginA, aNPlaneNormal);
		const Length1D targetBDistanceToAPlane = dot(edgeTargetB - edgeOriginA, aNPlaneNormal);
		if (signbit(originADistanceToBPlane) == signbit(targetADistanceToBPlane) || signbit(originBDistanceToAPlane) == signbit(targetBDistanceToAPlane)) {
			return {.normal{}, .penetrationDepth = Length1D::MAX};
		}

		const Direction3D normal = flipSignIf(*edgeCrossDirection, signbit(dot(*edgeCrossDirection, edgeOffsetA)));

		const Direction3D localNormalA = inverseTransformationA.getDirection(normal);
		const Direction3D localNormalB = inverseTransformationB.getDirection(normal);
		const Position3D minPointA = transformationA(shapeA.getLocalSupportPointOffset(-localNormalA));
		const Position3D maxPointA = transformationA(shapeA.getLocalSupportPointOffset(localNormalA));
		const Position3D minPointB = transformationB(shapeB.getLocalSupportPointOffset(-localNormalB));
		const Position3D maxPointB = transformationB(shapeB.getLocalSupportPointOffset(localNormalB));

		const Length1D minA = dot(minPointA - 0, normal);
		const Length1D maxA = dot(maxPointA - 0, normal);
		const Length1D minB = dot(minPointB - 0, normal);
		const Length1D maxB = dot(maxPointB - 0, normal);

		const Length1D penetrationDepth = maxA - minB;
		const Length1D oppositePenetrationDepth = maxB - minA;
		if (oppositePenetrationDepth < penetrationDepth) {
			return {.normal{}, .penetrationDepth = Length1D::MAX};
		}

		return {.normal = normal, .penetrationDepth = penetrationDepth};
	}

	struct FindPenetrationMinimizingEdgesResult {
		Pair<ConvexPolytopeEdgeIndex> edgeIndices;
		Scale3D normal;
		Length1D penetrationDepth;
	};

	[[nodiscard]] FindPenetrationMinimizingEdgesResult findPenetrationMinimizingEdges(const convex_polytope_shape_3d auto& shapeA, const Transformation3D& transformationA,
		const InverseTransformation3D& inverseTransformationA, const convex_polytope_shape_3d auto& shapeB, const Transformation3D& transformationB,
		const InverseTransformation3D& inverseTransformationB) requires(N == 3) {
		GREM_PROFILE_FUNCTION();

		FindPenetrationMinimizingEdgesResult result{
			.edgeIndices{},
			.normal{},
			.penetrationDepth = Length1D::MAX,
		};

		const ConvexPolytopeEdgeIndex edgeCountA = shapeA.getEdgeCount();
		const ConvexPolytopeEdgeIndex edgeCountB = shapeB.getEdgeCount();
		GREM_ASSERT(edgeCountA > 0);
		GREM_ASSERT(edgeCountB > 0);
		for (ConvexPolytopeEdgeIndex edgeIndexA = 0; edgeIndexA < edgeCountA; edgeIndexA += 2) {
			for (ConvexPolytopeEdgeIndex edgeIndexB = 0; edgeIndexB < edgeCountB; edgeIndexB += 2) {
				const EdgePenetrationResult edgePenetration =
					getEdgePenetration(shapeA, transformationA, inverseTransformationA, edgeIndexA, shapeB, transformationB, inverseTransformationB, edgeIndexB);
				if (edgePenetration.penetrationDepth < result.penetrationDepth) {
					result = {
						.edgeIndices{edgeIndexA, edgeIndexB},
						.normal = edgePenetration.normal,
						.penetrationDepth = edgePenetration.penetrationDepth,
					};
				}
			}
		}
		return result;
	}

private:
	struct GaussMapArc {
		Pair<Scale3D> normals;
		Length3D crossProduct;
	};

	[[nodiscard]] GREM_ALWAYS_INLINE GREM_FLATTEN static bool isMinkowskiFace(GaussMapArc arcA, GaussMapArc arcB) {
		const Scale3D a = arcA.normals.first;
		const Scale3D b = arcA.normals.second;
		const Scale3D c = arcB.normals.first;
		const Scale3D d = arcB.normals.second;
		const Length3D bxa = arcA.crossProduct;
		const Length3D dxc = arcB.crossProduct;
		const Length1D cba = dot(c, bxa);
		const Length1D dba = dot(d, bxa);
		const Length1D adc = dot(a, dxc);
		const Length1D bdc = dot(b, dxc);
		return signbit(cba * dba) & signbit(adc * bdc) & !signbit(cba * bdc);
	}
};

} // namespace grem::physics

#endif
