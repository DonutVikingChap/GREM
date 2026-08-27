// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/TriangleMesh.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include <utility> // std::move

namespace grem::physics {

template struct PointShape<2>;
template struct PointShape<3>;

template struct LineSegmentShape<2>;
template struct LineSegmentShape<3>;

template struct InfiniteLineShape<2>;
template struct InfiniteLineShape<3>;

template <size_t N>
RaycastResult<N> InfiniteHalfSpaceShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	if (signbit(localRayOrigin.getY())) {
		return RayHitInterior<N>{.localOffset = localRayOrigin};
	}
	if (localRayDirection.getY() == 0 || !signbit(localRayDirection.getY())) {
		return RayMiss{};
	}
	const Distance distance = localRayOrigin.getY() / -localRayDirection.getY();
	if (distance > maxLocalRayDistance) {
		return RayMiss{};
	}
	return RayHit<N>{.localOffset = localRayOrigin + distance * localRayDirection, .distance = distance, .normal = Y_AXIS<N>};
}

template struct InfiniteHalfSpaceShape<2>;
template struct InfiniteHalfSpaceShape<3>;

RaycastResult3D InfinitePlaneShape3D::castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const {
	if (localRayDirection.getY() == 0 || signbit(localRayOrigin.getY()) == signbit(localRayDirection.getY())) {
		return RayMiss{};
	}
	const Distance distance = localRayOrigin.getY() / -localRayDirection.getY();
	if (distance > maxLocalRayDistance) {
		return RayMiss{};
	}
	return RayHit3D{
		.localOffset = localRayOrigin + distance * localRayDirection,
		.distance = distance,
		.normal = Direction3D::reinterpret(Scale3D{0, copysign(Coefficient{1}, localRayOrigin.getY()), 0}),
	};
}

template <size_t N>
Volume BoxShape<N>::calculateVolume() const {
	if constexpr (N == 3) {
		return 8.0f * product(halfExtents);
	} else {
		return 4.0f * product(halfExtents) * (1.0f * METERS);
	}
}

template <size_t N>
PrincipalMomentsOfInertia<N> BoxShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(all(greaterThan(halfExtents, 0)));
	if constexpr (N == 3) {
		const Quantity<3, SquaredLength::Unit> squaredHalfExtents = halfExtents * halfExtents;
		return (1.0f / 3.0f) * mass *
		       Quantity<3, SquaredLength::Unit>{
				   squaredHalfExtents.getY() + squaredHalfExtents.getZ(),
				   squaredHalfExtents.getX() + squaredHalfExtents.getZ(),
				   squaredHalfExtents.getX() + squaredHalfExtents.getY(),
			   };
	} else {
		return PrincipalMomentsOfInertia2D{(1.0f / 3.0f) * mass * (length2(halfExtents.getX()) + length2(halfExtents.getY()))};
	}
}

template <size_t N>
Optional<Area> BoxShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	if constexpr (N == 3) {
		return 2.0f * (halfExtents.getY() * halfExtents.getZ() * abs(dot(direction, basis[X])) +    //
						  halfExtents.getX() * halfExtents.getZ() * abs(dot(direction, basis[Y])) + //
						  halfExtents.getX() * halfExtents.getY() * abs(dot(direction, basis[Z])));
	} else {
		return 2.0f * (halfExtents.getY() * abs(dot(direction, basis[X])) + halfExtents.getX() * abs(dot(direction, basis[Y]))) * (1.0f * METERS);
	}
}

template <size_t N>
ConvexPolytopeVertexIndex BoxShape<N>::getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex) const {
	GREM_ASSERT(all(greaterThan(halfExtents, 0)));
	const vec<N, bool> signs = signbit(localDirection);
	if constexpr (N == 2) {
		return static_cast<ConvexPolytopeVertexIndex>(                                                                //
			static_cast<ConvexPolytopeVertexIndex>(static_cast<ConvexPolytopeVertexIndex>(signs.x != signs.y) << 0) | //
			static_cast<ConvexPolytopeVertexIndex>(static_cast<ConvexPolytopeVertexIndex>(!signs.y) << 1));
	} else {
		return static_cast<ConvexPolytopeVertexIndex>(                                                      //
			static_cast<ConvexPolytopeVertexIndex>(static_cast<ConvexPolytopeVertexIndex>(!signs.x) << 0) | //
			static_cast<ConvexPolytopeVertexIndex>(static_cast<ConvexPolytopeVertexIndex>(!signs.y) << 1) | //
			static_cast<ConvexPolytopeVertexIndex>(static_cast<ConvexPolytopeVertexIndex>(signs.z) << 2));
	}
}

template <size_t N>
Length<N> BoxShape<N>::getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const {
	if constexpr (N == 2) {
		switch (vertexIndex) {
			case 0: return Length2D{-halfExtents.getX(), -halfExtents.getY()};
			case 1: return Length2D{halfExtents.getX(), -halfExtents.getY()};
			case 2: return Length2D{halfExtents.getX(), halfExtents.getY()};
			case 3: return Length2D{-halfExtents.getX(), halfExtents.getY()};
			default: unreachable();
		}
		return {};
	} else {
		return Length3D{
			flipSignIf(halfExtents.getX(), (vertexIndex & 0b001) == 0),
			flipSignIf(halfExtents.getY(), (vertexIndex & 0b010) == 0),
			flipSignIf(halfExtents.getZ(), (vertexIndex & 0b100) != 0),
		};
	}
}

template <size_t N>
Length<N> BoxShape<N>::getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const {
	if constexpr (N == 2) {
		return getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
	} else {
		switch (faceIndex) {
			case 0: return Length3D{-halfExtents.getX(), -halfExtents.getY(), halfExtents.getZ()};  // Vertex 0
			case 1: return Length3D{halfExtents.getX(), -halfExtents.getY(), halfExtents.getZ()};   // Vertex 1
			case 2: return Length3D{halfExtents.getX(), halfExtents.getY(), halfExtents.getZ()};    // Vertex 3
			case 3: return Length3D{halfExtents.getX(), halfExtents.getY(), -halfExtents.getZ()};   // Vertex 7
			case 4: return Length3D{-halfExtents.getX(), halfExtents.getY(), -halfExtents.getZ()};  // Vertex 6
			case 5: return Length3D{-halfExtents.getX(), -halfExtents.getY(), -halfExtents.getZ()}; // Vertex 4
			default: break;
		}
		unreachable();
	}
}

template <size_t N>
Direction<N> BoxShape<N>::getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const {
	if constexpr (N == 2) {
		static constexpr Array<Direction2D, 4> FACE_NORMALS{
			Direction2D::reinterpret(vec2{0.0f, -1.0f}),
			Direction2D::reinterpret(vec2{1.0f, 0.0f}),
			Direction2D::reinterpret(vec2{0.0f, 1.0f}),
			Direction2D::reinterpret(vec2{-1.0f, 0.0f}),
		};
		return FACE_NORMALS[faceIndex];
	} else {
		static constexpr Array<Direction3D, 6> FACE_NORMALS{
			Direction3D::reinterpret(vec3{0.0f, 0.0f, 1.0f}),
			Direction3D::reinterpret(vec3{1.0f, 0.0f, 0.0f}),
			Direction3D::reinterpret(vec3{0.0f, 1.0f, 0.0f}),
			Direction3D::reinterpret(vec3{0.0f, 0.0f, -1.0f}),
			Direction3D::reinterpret(vec3{-1.0f, 0.0f, 0.0f}),
			Direction3D::reinterpret(vec3{0.0f, -1.0f, 0.0f}),
		};
		return FACE_NORMALS[faceIndex];
	}
}

template <size_t N>
ConvexPolytopeFaceIndex BoxShape<N>::getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex) const {
	if constexpr (N == 2) {
		switch (abs(localDirection).getMaxIndex()) {
			case 0: return (signbit(localDirection.getX())) ? 3 : 1;
			case 1: return (signbit(localDirection.getY())) ? 0 : 2;
			default: break;
		}
	} else {
		switch (abs(localDirection).getMaxIndex()) {
			case 0: return (signbit(localDirection.getX())) ? 4 : 1;
			case 1: return (signbit(localDirection.getY())) ? 5 : 2;
			case 2: return (signbit(localDirection.getZ())) ? 3 : 0;
			default: break;
		}
	}
	unreachable();
}

template <size_t N>
ConvexPolytopeVertexIndex BoxShape<N>::getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	static constexpr Array<ConvexPolytopeVertexIndex, 24> VERTEX_INDICES{
		0, // 0
		1, // 1
		1, // 2
		3, // 3
		3, // 4
		2, // 5
		2, // 6
		0, // 7
		1, // 8
		5, // 9
		5, // 10
		7, // 11
		7, // 12
		3, // 13
		7, // 14
		6, // 15
		6, // 16
		2, // 17
		5, // 18
		4, // 19
		4, // 20
		6, // 21
		4, // 22
		0, // 23
	};
	return VERTEX_INDICES[edgeIndex];
}

template <size_t N>
ConvexPolytopeFaceIndex BoxShape<N>::getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	static constexpr Array<ConvexPolytopeFaceIndex, 24> FACE_INDICES{
		0, // 0
		5, // 1
		0, // 2
		1, // 3
		0, // 4
		2, // 5
		0, // 6
		4, // 7
		1, // 8
		5, // 9
		1, // 10
		3, // 11
		1, // 12
		2, // 13
		2, // 14
		3, // 15
		2, // 16
		4, // 17
		3, // 18
		5, // 19
		3, // 20
		4, // 21
		4, // 22
		5, // 23
	};
	return FACE_INDICES[edgeIndex];
}

template <size_t N>
ConvexPolytopeEdgeIndex BoxShape<N>::getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	static constexpr Array<ConvexPolytopeEdgeIndex, 24> NEXT_EDGE_INDICES{
		2,  // 0
		23, // 1
		4,  // 2
		8,  // 3
		6,  // 4
		13, // 5
		0,  // 6
		17, // 7
		10, // 8
		1,  // 9
		12, // 10
		18, // 11
		3,  // 12
		14, // 13
		16, // 14
		11, // 15
		5,  // 16
		21, // 17
		20, // 18
		9,  // 19
		15, // 20
		22, // 21
		7,  // 22
		19, // 23
	};
	return NEXT_EDGE_INDICES[edgeIndex];
}

template <size_t N>
ConvexPolytopeEdgeIndex BoxShape<N>::getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3) {
	static constexpr Array<ConvexPolytopeEdgeIndex, 6> FIRST_EDGE_INDICES{
		0,  // 0
		8,  // 1
		13, // 2
		11, // 3
		21, // 4
		19, // 5
	};
	return FIRST_EDGE_INDICES[faceIndex];
}

template <size_t N>
ConvexPolytopeEdgeIndex BoxShape<N>::getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3) {
	static constexpr Array<ConvexPolytopeEdgeIndex, 8> EDGE_INDICES{
		0,  // 0
		1,  // 1
		5,  // 2
		3,  // 3
		19, // 4
		9,  // 5
		15, // 6
		11, // 7
	};
	return EDGE_INDICES[vertexIndex];
}

template struct BoxShape<2>;
template struct BoxShape<3>;

template struct CubeShape<2>;
template struct CubeShape<3>;

template <size_t N>
Volume EllipsoidShape<N>::calculateVolume() const {
	if constexpr (N == 3) {
		return (4.0f / 3.0f) * PI * product(radii);
	} else {
		return PI * product(radii) * (1.0f * METERS);
	}
}

template <size_t N>
PrincipalMomentsOfInertia<N> EllipsoidShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(all(greaterThan(radii, 0)));
	if constexpr (N == 3) {
		const Quantity<3, SquaredLength::Unit> squaredRadii = radii * radii;
		return (1.0f / 5.0f) * mass *
		       Quantity<3, SquaredLength::Unit>{
				   squaredRadii.getY() + squaredRadii.getZ(),
				   squaredRadii.getX() + squaredRadii.getZ(),
				   squaredRadii.getX() + squaredRadii.getY(),
			   };
	} else {
		return PrincipalMomentsOfInertia2D{(1.0f / 5.0f) * mass * (length2(radii.getX()) + length2(radii.getY()))};
	}
}

template <size_t N>
Optional<Area> EllipsoidShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	if constexpr (N == 3) {
		return PI * (radii.getY() * radii.getZ() * abs(dot(direction, basis[X])) +    //
						radii.getX() * radii.getZ() * abs(dot(direction, basis[Y])) + //
						radii.getX() * radii.getY() * abs(dot(direction, basis[Z])));
	} else {
		return 2.0f * (radii.getY() * abs(dot(direction, basis[X])) + radii.getX() * abs(dot(direction, basis[Y]))) * (1.0f * METERS);
	}
}

template <size_t N>
RaycastResult<N> EllipsoidShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	GREM_ASSERT(all(greaterThan(radii, 0)));
	const grem::Ellipsoid<N, float> ellipsoid{.center{}, .radii = radii.in(Length<N>::UNIT)};
	const grem::Ray<N, float> r{
		.origin = localRayOrigin.in(Length<N>::UNIT),
		.direction = localRayDirection,
		.maxDistance = maxLocalRayDistance.in(Distance::UNIT),
	};
	return ellipsoid.raycast(r) * RaycastResult<N>::UNIT;
}

template <size_t N>
bool EllipsoidShape<N>::containsLocalPoint(Length<N> localPoint) const {
	GREM_ASSERT(all(greaterThan(radii, 0)));
	const grem::Ellipsoid<N, float> ellipsoid{.center{}, .radii = radii.in(Length<N>::UNIT)};
	return ellipsoid.contains(localPoint.in(Length<N>::UNIT));
}

template <size_t N>
Length<N> EllipsoidShape<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	GREM_ASSERT(all(greaterThan(radii, 0)));
	return radii * localDirection;
}

template struct EllipsoidShape<2>;
template struct EllipsoidShape<3>;

template <size_t N>
RaycastResult<N> SphereShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	GREM_ASSERT(radius > Distance{});
	const grem::Sphere<N, float> sphere{.center{}, .radius = radius.in(Length<N>::UNIT)};
	const grem::Ray<N, float> r{
		.origin = localRayOrigin.in(Length<N>::UNIT),
		.direction = localRayDirection,
		.maxDistance = maxLocalRayDistance.in(Distance::UNIT),
	};
	return sphere.raycast(r) * RaycastResult<N>::UNIT;
}

template <size_t N>
bool SphereShape<N>::containsLocalPoint(Length<N> localPoint) const {
	GREM_ASSERT(radius > Distance{});
	const grem::Sphere<N, float> sphere{.center{}, .radius = radius.in(Length<N>::UNIT)};
	return sphere.contains(localPoint.in(Length<N>::UNIT));
}

template <size_t N>
Length<N> SphereShape<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	GREM_ASSERT(radius > Distance{});
	return radius * localDirection;
}

template struct SphereShape<2>;
template struct SphereShape<3>;

template <size_t N>
Volume CapsuleShape<N>::calculateVolume() const {
	if constexpr (N == 3) {
		return PI * radius * radius * ((4.0f / 3.0f) * radius + 2.0f * halfLength);
	} else {
		return (PI * radius * radius + 4.0f * radius * halfLength) * (1.0f * METERS);
	}
}

template <size_t N>
PrincipalMomentsOfInertia<N> CapsuleShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const SquaredDistance squaredRadius = length2(radius);
	const Volume hemisphereVolume = (2.0f / 3.0f) * PI * squaredRadius * radius;
	const Volume halfCylinderVolume = PI * squaredRadius * halfLength;
	const Mass halfMass = mass * 0.5f;
	const Mass hemisphereMass = halfMass * hemisphereVolume / (hemisphereVolume + halfCylinderVolume);
	const Mass cylinderMass = mass - hemisphereMass * 2.0f;
	const MomentOfInertia2D hemisphereMomentOfInertiaAroundBase = (2.0f / 5.0f) * hemisphereMass * squaredRadius;
	const Distance hemisphereCenterOfMassOffset = (3.0f / 8.0f) * radius;
	const MomentOfInertia2D hemisphereMomentOfInertia =
		hemisphereMomentOfInertiaAroundBase + hemisphereMass * (length2(hemisphereCenterOfMassOffset + halfLength) - length2(hemisphereCenterOfMassOffset));
	const MomentOfInertia2D cylinderMomentOfInertia = cylinderMass * ((1.0f / 4.0f) * squaredRadius + (1.0f / 3.0f) * length2(halfLength));
	const MomentOfInertia2D capsuleMomentOfInertia = cylinderMomentOfInertia + 2.0f * hemisphereMomentOfInertia;
	if constexpr (N == 3) {
		return PrincipalMomentsOfInertia3D{
			capsuleMomentOfInertia,
			0.5f * cylinderMass * squaredRadius + 2.0f * hemisphereMomentOfInertiaAroundBase,
			capsuleMomentOfInertia,
		};
	} else {
		return PrincipalMomentsOfInertia2D{capsuleMomentOfInertia};
	}
}

template <size_t N>
Optional<Area> CapsuleShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	// Approximate as half the area of the corresponding box shape.
	if (const Optional<Area> result = BoxShape<N>{.halfExtents = Y_AXIS<N> * halfLength + Length<N>{radius}}.getReferenceArea(basis, direction)) {
		return *result * 0.5f;
	}
	return {};
}

template <size_t N>
RaycastResult<N> CapsuleShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const Distance centerLineLength = 2.0f * halfLength;
	const SquaredDistance squaredRadius = length2(radius);
	const Length<N> offsetFromBottomCenter = localRayOrigin + Y_AXIS<N> * halfLength;
	const SquaredDistance squaredCenterLineLength = length2(centerLineLength);
	const Length1D directionAlongCenterLine = localRayDirection.getY() * centerLineLength;
	const SquaredDistance signedOffsetFromBottomOnCenterLineSquared = (localRayOrigin.getY() + halfLength) * centerLineLength;
	const Length1D offsetFromBottomCenterAlongRay = dot(offsetFromBottomCenter, localRayDirection);
	const SquaredDistance distanceFromBottomCenterSquared = length2(offsetFromBottomCenter);
	const SquaredDistance differenceOfSquares = squaredCenterLineLength - length2(directionAlongCenterLine);
	const Volume offsetAlongDirectionAlongCenterLineSquared =
		squaredCenterLineLength * offsetFromBottomCenterAlongRay - signedOffsetFromBottomOnCenterLineSquared * directionAlongCenterLine;
	const SquaredVolume discriminant =
		length2(offsetAlongDirectionAlongCenterLineSquared) -
		differenceOfSquares *
			(squaredCenterLineLength * distanceFromBottomCenterSquared - length2(signedOffsetFromBottomOnCenterLineSquared) - squaredRadius * squaredCenterLineLength);
	if (discriminant < 0) {
		return RayMiss{};
	}
	const Length1D t = (-offsetAlongDirectionAlongCenterLineSquared - sqrt(discriminant)) / differenceOfSquares;
	const SquaredDistance hitSignedOffsetFromBottomCenterSquared = signedOffsetFromBottomOnCenterLineSquared + t * directionAlongCenterLine;
	if (hitSignedOffsetFromBottomCenterSquared >= 0 && hitSignedOffsetFromBottomCenterSquared < squaredCenterLineLength) {
		Length<N> offsetFromCenter = localRayOrigin;
		offsetFromCenter.setY((localRayOrigin.getY() <= 0) ? localRayOrigin.getY() + halfLength : localRayOrigin.getY() - halfLength);
		if (length2(offsetFromCenter) < squaredRadius) {
			return RayHitInterior{.localOffset = localRayOrigin};
		}
		if (t < 0) {
			return RayMiss{};
		}
		const Length<N> point = localRayOrigin + t * localRayDirection;
		Length<N> horizontalPoint = point;
		horizontalPoint.setY(0);
		return RayHit<N>{.localOffset = point, .distance = t, .normal = tryNormalize(horizontalPoint).value_or(-localRayDirection)};
	}
	const Length<N> offsetFromSphereCenter = (hitSignedOffsetFromBottomCenterSquared <= 0) ? offsetFromBottomCenter : localRayOrigin - Y_AXIS<N> * halfLength;
	return SphereShape<N>{.radius = radius}.castLocalRay(offsetFromSphereCenter, localRayDirection, maxLocalRayDistance);
}

template <size_t N>
bool CapsuleShape<N>::containsLocalPoint(Length<N> localPoint) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const grem::Length<N, float> halfLine = (Y_AXIS<N> * halfLength).in(Length<N>::UNIT);
	const grem::Capsule<N, float> capsule{.centerLine{.pointA = -halfLine, .pointB = halfLine}, .radius = radius.in(Length<N>::UNIT)};
	return capsule.contains(localPoint.in(Length<N>::UNIT));
}

template <size_t N>
Length<N> CapsuleShape<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	return Y_AXIS<N> * copysign(halfLength, localDirection.getY()) + radius * localDirection;
}

template struct CapsuleShape<2>;
template struct CapsuleShape<3>;

template <size_t N>
Volume TaperedCapsuleShape<N>::calculateVolume() const {
	// Approximate as the average volume of the capsules with the top and bottom radii.
	const Volume volumeWithBottomRadius = CapsuleShape<N>{.radius = bottomRadius, .halfLength = halfLength}.calculateVolume();
	const Volume volumeWithTopRadius = CapsuleShape<N>{.radius = topRadius, .halfLength = halfLength}.calculateVolume();
	return midpoint(volumeWithBottomRadius, volumeWithTopRadius);
}

template <size_t N>
PrincipalMomentsOfInertia<N> TaperedCapsuleShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const SquaredDistance squaredBottomRadius = length2(bottomRadius);
	const SquaredDistance squaredTopRadius = length2(topRadius);
	const SquaredDistance squaredCenterLineLength = length2(2.0f * halfLength);
	const Volume cubedBottomRadius = squaredBottomRadius * bottomRadius;
	const Volume cubedTopRadius = squaredTopRadius * topRadius;
	const SquaredArea squaredSquaredBottomRadius = length2(squaredBottomRadius);
	const SquaredArea squaredSquaredTopRadius = length2(squaredTopRadius);
	const SquaredArea a =
		squaredSquaredBottomRadius + cubedBottomRadius * topRadius + squaredBottomRadius * squaredTopRadius + bottomRadius * cubedTopRadius + squaredSquaredTopRadius;
	const SquaredDistance b = (squaredBottomRadius + bottomRadius * topRadius + squaredTopRadius) * PI;
	const MomentOfInertia2D xz =
		mass *
		((a + 2.0f * (squaredBottomRadius + 3.0f * bottomRadius * topRadius + 6.0f * squaredBottomRadius) * squaredCenterLineLength) / (6.0f * b) + squaredBottomRadius +
			squaredTopRadius + 2.0f * squaredCenterLineLength) /
		10.0f;
	if constexpr (N == 3) {
		const MomentOfInertia2D y = mass * (a / b + squaredBottomRadius + squaredTopRadius) / 10.0f;
		return PrincipalMomentsOfInertia3D{xz, y, xz};
	} else {
		return PrincipalMomentsOfInertia2D{xz};
	}
}

template <size_t N>
Optional<Area> TaperedCapsuleShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	// Approximate as half the area of the corresponding box shape for the average radius.
	if (const Optional<Area> result = BoxShape<N>{.halfExtents = Y_AXIS<N> * halfLength + Length<N>{midpoint(bottomRadius, topRadius)}}.getReferenceArea(basis, direction)) {
		return *result * 0.5f;
	}
	return {};
}

template <size_t N>
RaycastResult<N> TaperedCapsuleShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	if (containsLocalPoint(localRayOrigin)) {
		return RayHitInterior{.localOffset = localRayOrigin};
	}

	const Distance centerLineLength = 2.0f * halfLength;
	const SquaredDistance squaredBottomRadius = length2(bottomRadius);
	const SquaredDistance squaredTopRadius = length2(topRadius);
	const Length<N> offsetFromBottomCenter = localRayOrigin + Y_AXIS<N> * halfLength;
	const Length<N> offsetFromTopCenter = localRayOrigin - Y_AXIS<N> * halfLength;
	const Length1D radiusDifference = bottomRadius - topRadius;
	const SquaredDistance squaredCenterLineLength = length2(centerLineLength);
	const SquaredDistance signedOffsetFromBottomOnCenterLineSquared = offsetFromBottomCenter.getY() * centerLineLength;
	const Length1D directionAlongCenterLine = localRayDirection.getY() * centerLineLength;
	const Length1D offsetFromBottomCenterAlongRay = dot(offsetFromBottomCenter, localRayDirection);
	const SquaredDistance squaredOffsetFromBottomCenter = length2(offsetFromBottomCenter);
	const Length1D offsetFromTopCenterAlongRay = dot(offsetFromTopCenter, localRayDirection);
	const SquaredDistance squaredOffsetFromTopCenter = length2(offsetFromTopCenter);

	const SquaredDistance lengthRadiusDifferenceSquared = squaredCenterLineLength - length2(radiusDifference);
	const SquaredDistance differenceOfSquares = lengthRadiusDifferenceSquared - length2(directionAlongCenterLine);
	const Volume offsetAlongDirectionAlongCenterLineSquared =
		lengthRadiusDifferenceSquared * offsetFromBottomCenterAlongRay - signedOffsetFromBottomOnCenterLineSquared * directionAlongCenterLine +
		directionAlongCenterLine * radiusDifference * bottomRadius;
	const SquaredVolume discriminant =
		length2(offsetAlongDirectionAlongCenterLineSquared) -
		(lengthRadiusDifferenceSquared * squaredOffsetFromBottomCenter - length2(signedOffsetFromBottomOnCenterLineSquared) +
			signedOffsetFromBottomOnCenterLineSquared * radiusDifference * bottomRadius * 2.0f - squaredCenterLineLength * squaredBottomRadius) *
			differenceOfSquares;
	if (discriminant < 0) {
		return RayMiss{};
	}
	const Length1D t = (-offsetAlongDirectionAlongCenterLineSquared - sqrt(discriminant)) / differenceOfSquares;
	if (t < 0 || t > maxLocalRayDistance) {
		return RayMiss{};
	}
	const SquaredDistance hitSignedOffsetFromBottomCenterSquared = signedOffsetFromBottomOnCenterLineSquared - bottomRadius * radiusDifference + t * directionAlongCenterLine;
	if (hitSignedOffsetFromBottomCenterSquared >= 0 && hitSignedOffsetFromBottomCenterSquared < lengthRadiusDifferenceSquared) {
		return RayHit<N>{
			.localOffset = localRayOrigin + t * localRayDirection,
			.distance = t,
			.normal = tryNormalize(
				lengthRadiusDifferenceSquared * (offsetFromBottomCenter + t * localRayDirection) - Y_AXIS<N> * centerLineLength * hitSignedOffsetFromBottomCenterSquared)
		        .value_or(-localRayDirection),
		};
	}

	const SquaredDistance bottomDiscriminant = length2(offsetFromBottomCenterAlongRay) - squaredOffsetFromBottomCenter + squaredBottomRadius;
	const SquaredDistance topDiscriminant = length2(offsetFromTopCenterAlongRay) - squaredOffsetFromTopCenter + squaredTopRadius;
	if (max(bottomDiscriminant, topDiscriminant) <= 0) {
		return RayMiss{};
	}
	RaycastResult<N> result = RayMiss{};
	if (bottomDiscriminant > 0) {
		const Length1D t = -offsetFromBottomCenterAlongRay - sqrt(bottomDiscriminant);
		if (t >= 0 && t <= maxLocalRayDistance) {
			result = RayHit<N>{
				.localOffset = localRayOrigin + t * localRayDirection,
				.distance = t,
				.normal = Direction<N>::reinterpret((offsetFromBottomCenter + t * localRayDirection) / bottomRadius),
			};
		}
	}
	if (topDiscriminant > 0) {
		const Length1D t = -offsetFromTopCenterAlongRay - sqrt(topDiscriminant);
		if (t >= 0 && t <= maxLocalRayDistance && (!result.template is<RayHit<N>>() || t < result.template as<RayHit<N>>().distance)) {
			result = RayHit<N>{
				.localOffset = localRayOrigin + t * localRayDirection,
				.distance = t,
				.normal = Direction<N>::reinterpret((offsetFromTopCenter + t * localRayDirection) / topRadius),
			};
		}
	}
	return result;
}

template <size_t N>
bool TaperedCapsuleShape<N>::containsLocalPoint(Length<N> localPoint) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const Length1D radiusDifference = bottomRadius - topRadius;
	const Distance centerLineLength = 2.0f * halfLength;
	const Scale1D b = radiusDifference / centerLineLength;
	const Scale1D a = sqrt(1.0f - length2(b));

	Distance horizontalPointDistance{};
	if constexpr (N == 3) {
		horizontalPointDistance = length(localPoint.get(X, Z));
	} else {
		horizontalPointDistance = abs(localPoint.getX());
	}

	const Length1D verticalPoint = localPoint.getY();

	const Length1D k = verticalPoint * a - horizontalPointDistance * b;
	if (k < 0) {
		return length(Length2D{horizontalPointDistance, verticalPoint}) < bottomRadius;
	}
	if (k > a * centerLineLength) {
		return length(Length2D{horizontalPointDistance, verticalPoint - centerLineLength}) < topRadius;
	}
	return horizontalPointDistance * a + verticalPoint * b < bottomRadius;
}

template <size_t N>
Length<N> TaperedCapsuleShape<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const Length1D radiusDifference = bottomRadius - topRadius;
	Coefficient horizontalDirectionAmount{};
	if constexpr (N == 3) {
		horizontalDirectionAmount = length(localDirection.get(X, Z));
	} else {
		horizontalDirectionAmount = abs(localDirection.getX());
	}
	const bool isTop = (abs(radiusDifference) <= Distance::MACHINE_EPSILON || horizontalDirectionAmount <= Coefficient::MACHINE_EPSILON)
	                       ? (localDirection.getY() >= 0)
	                       : ((localDirection.getY() / horizontalDirectionAmount) <= (2.0f * halfLength) / radiusDifference);
	const Length1D radius = (isTop) ? topRadius : bottomRadius;
	return Y_AXIS<N> * ((isTop) ? halfLength : -halfLength) + radius * localDirection;
}

template struct TaperedCapsuleShape<2>;
template struct TaperedCapsuleShape<3>;

Volume CylinderShape3D::calculateVolume() const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	return PI * radius * radius * 2.0f * halfLength;
}

PrincipalMomentsOfInertia3D CylinderShape3D::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const SquaredDistance squaredRadius = length2(radius);
	const MomentOfInertia2D cylinderMomentOfInertia = (1.0f / 12.0f) * mass * (3.0f * squaredRadius + 4.0f * length2(halfLength));
	return PrincipalMomentsOfInertia3D{
		cylinderMomentOfInertia,
		0.5f * mass * squaredRadius,
		cylinderMomentOfInertia,
	};
}

Optional<Area> CylinderShape3D::getReferenceArea(const Basis3D& basis, Direction3D direction) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const SquaredDistance squaredCenterLineLength = length2(2.0f * halfLength);
	const SquaredDistance squaredRadius = length2(radius);
	return squaredCenterLineLength * abs(dot(direction, basis[X])) + //
	       PI * squaredRadius * abs(dot(direction, basis[Y])) +      //
	       squaredCenterLineLength * abs(dot(direction, basis[Z]));
}

RaycastResult3D CylinderShape3D::castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const {
	if (containsLocalPoint(localRayOrigin)) {
		return RayHitInterior{.localOffset = localRayOrigin};
	}

	const Distance centerLineLength = 2.0f * halfLength;
	const SquaredDistance squaredRadius = length2(radius);
	const Length3D offsetFromBottomCenter = localRayOrigin + Y_AXIS_3D * halfLength;
	const SquaredDistance squaredCenterLineLength = length2(centerLineLength);
	const Length1D directionAlongCenterLine = localRayDirection.getY() * centerLineLength;
	const SquaredDistance signedOffsetFromBottomOnCenterLineSquared = (localRayOrigin.getY() + halfLength) * centerLineLength;
	const Length1D offsetFromBottomCenterAlongRay = dot(offsetFromBottomCenter, localRayDirection);
	const SquaredDistance distanceFromBottomCenterSquared = length2(offsetFromBottomCenter);
	const SquaredDistance differenceOfSquares = squaredCenterLineLength - length2(directionAlongCenterLine);

	const Volume offsetAlongDirectionAlongCenterLineSquared =
		squaredCenterLineLength * offsetFromBottomCenterAlongRay - signedOffsetFromBottomOnCenterLineSquared * directionAlongCenterLine;
	const SquaredVolume discriminant =
		length2(offsetAlongDirectionAlongCenterLineSquared) -
		differenceOfSquares *
			(squaredCenterLineLength * distanceFromBottomCenterSquared - length2(signedOffsetFromBottomOnCenterLineSquared) - squaredRadius * squaredCenterLineLength);
	if (discriminant < 0) {
		return RayMiss{};
	}
	const Volume discriminantSqrt = sqrt(discriminant);
	const Length1D t = (-offsetAlongDirectionAlongCenterLineSquared - discriminantSqrt) / differenceOfSquares;
	const SquaredDistance hitSignedOffsetFromBottomCenterSquared = signedOffsetFromBottomOnCenterLineSquared + t * directionAlongCenterLine;
	if (hitSignedOffsetFromBottomCenterSquared >= 0 && hitSignedOffsetFromBottomCenterSquared < squaredCenterLineLength) {
		if (t < 0 || t > maxLocalRayDistance) {
			return RayMiss{};
		}
		const Length3D point = localRayOrigin + t * localRayDirection;
		return RayHit3D{.localOffset = point, .distance = t, .normal = tryNormalize(point.get(X, 0, Z)).value_or(-localRayDirection)};
	}
	if (abs(directionAlongCenterLine) < Distance::MACHINE_EPSILON) {
		return RayMiss{};
	}
	const Length1D tCap =
		(((hitSignedOffsetFromBottomCenterSquared < 0) ? SquaredDistance{} : squaredCenterLineLength) - signedOffsetFromBottomOnCenterLineSquared) / directionAlongCenterLine;
	if (abs(offsetAlongDirectionAlongCenterLineSquared + differenceOfSquares * tCap) < discriminantSqrt) {
		if (tCap < 0 || tCap > maxLocalRayDistance) {
			return RayMiss{};
		}
		return RayHit3D{
			.localOffset = localRayOrigin + tCap * localRayDirection,
			.distance = tCap,
			.normal = Direction3D::reinterpret(Scale3D{0, copysign(Coefficient{1}, hitSignedOffsetFromBottomCenterSquared), 0}),
		};
	}
	return RayMiss{};
}

bool CylinderShape3D::containsLocalPoint(Length3D localPoint) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	return (abs(localPoint.getY()) < halfLength && length2(localPoint.get(X, Z)) < length2(radius));
}

Length3D CylinderShape3D::getLocalSupportPointOffset(Direction3D localDirection) const {
	GREM_ASSERT(radius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const Length3D linePoint = Y_AXIS_3D * ((localDirection.getY() >= 0) ? halfLength : -halfLength);
	return Length3D{
		linePoint.getX() + radius * localDirection.getX(),
		linePoint.getY(),
		linePoint.getZ() + radius * localDirection.getZ(),
	};
}

Volume TaperedCylinderShape3D::calculateVolume() const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	return (2.0f / 3.0f) * PI * halfLength * (length2(bottomRadius) + length2(topRadius) + bottomRadius * topRadius);
}

PrincipalMomentsOfInertia3D TaperedCylinderShape3D::calculatePrincipalMomentsOfInertia(Mass mass) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const SquaredDistance squaredBottomRadius = length2(bottomRadius);
	const SquaredDistance squaredTopRadius = length2(topRadius);
	const SquaredDistance squaredHalfLength = length2(halfLength);
	const Volume cubedBottomRadius = squaredBottomRadius * bottomRadius;
	const Volume cubedTopRadius = squaredTopRadius * topRadius;
	const SquaredArea squaredSquaredBottomRadius = length2(squaredBottomRadius);
	const SquaredArea squaredSquaredTopRadius = length2(squaredTopRadius);
	const SquaredArea numerator =
		3.0f * (squaredSquaredBottomRadius + cubedBottomRadius * topRadius + squaredBottomRadius * squaredTopRadius + bottomRadius * cubedTopRadius + squaredSquaredTopRadius);
	const SquaredDistance denominator = 10.0f * PI * (squaredBottomRadius + bottomRadius * topRadius + squaredTopRadius);
	const MomentOfInertia2D xz =
		mass * (numerator + 8.0f * (squaredBottomRadius + 3.0f * bottomRadius * topRadius + 6.0f * squaredTopRadius) * squaredHalfLength) / (2.0f * denominator);
	const MomentOfInertia2D y = mass * numerator / denominator;
	return PrincipalMomentsOfInertia3D{xz, y, xz};
}

Optional<Area> TaperedCylinderShape3D::getReferenceArea(const Basis3D& basis, Direction3D direction) const {
	// Approximate as the reference area of the cylinder with the average radius.
	return CylinderShape3D{.radius = midpoint(bottomRadius, topRadius), .halfLength = halfLength}.getReferenceArea(basis, direction);
}

RaycastResult3D TaperedCylinderShape3D::castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const {
	if (containsLocalPoint(localRayOrigin)) {
		return RayHitInterior{.localOffset = localRayOrigin};
	}

	const Distance centerLineLength = 2.0f * halfLength;
	const Length3D offsetFromBottomCenter = localRayOrigin + Y_AXIS_3D * halfLength;
	const Length3D offsetFromTopCenter = localRayOrigin - Y_AXIS_3D * halfLength;
	const SquaredDistance squaredCenterLineLength = length2(centerLineLength);
	const SquaredArea squaredCenterLineLengthSquared = length2(squaredCenterLineLength);
	const SquaredDistance signedOffsetFromBottomOnCenterLineSquared = offsetFromBottomCenter.getY() * centerLineLength;
	const Length1D directionAlongCenterLine = localRayDirection.getY() * centerLineLength;
	const SquaredLength directionAlongCenterLineSquared = length2(directionAlongCenterLine);
	const Length1D offsetFromBottomCenterAlongRay = dot(offsetFromBottomCenter, localRayDirection);
	const SquaredDistance squaredOffsetFromBottomCenter = length2(offsetFromBottomCenter);
	const SquaredDistance offsetFromTopCenterAlongCenterLine = offsetFromTopCenter.getY() * centerLineLength;

	if (abs(directionAlongCenterLine) >= Distance::MACHINE_EPSILON) {
		if (signedOffsetFromBottomOnCenterLineSquared < 0) {
			if (length2(offsetFromBottomCenter * directionAlongCenterLine - localRayDirection * signedOffsetFromBottomOnCenterLineSquared) <
				(length2(bottomRadius) * directionAlongCenterLineSquared)) {
				const Length1D t = -signedOffsetFromBottomOnCenterLineSquared / directionAlongCenterLine;
				if (t < 0 || t > maxLocalRayDistance) {
					return RayMiss{};
				}
				return RayHit3D{.localOffset = localRayOrigin + t * localRayDirection, .distance = t, .normal = -Y_AXIS_3D};
			}
		} else if (offsetFromTopCenterAlongCenterLine > 0) {
			const Length1D t = -offsetFromTopCenterAlongCenterLine / directionAlongCenterLine;
			if (length2(offsetFromTopCenter + t * localRayDirection) < length2(topRadius)) {
				if (t < 0 || t > maxLocalRayDistance) {
					return RayMiss{};
				}
				return RayHit3D{.localOffset = localRayOrigin + t * localRayDirection, .distance = t, .normal = Y_AXIS_3D};
			}
		}
	}

	const Length1D radiusDifference = bottomRadius - topRadius;
	const SquaredDistance lengthRadiusDifferenceSquared = squaredCenterLineLength + length2(radiusDifference);
	const SquaredArea k2 = squaredCenterLineLengthSquared - directionAlongCenterLineSquared * lengthRadiusDifferenceSquared;
	const auto k1 =
		squaredCenterLineLengthSquared * offsetFromBottomCenterAlongRay - signedOffsetFromBottomOnCenterLineSquared * directionAlongCenterLine * lengthRadiusDifferenceSquared +
		squaredCenterLineLength * bottomRadius * radiusDifference * directionAlongCenterLine;
	const SquaredVolume k0 =
		squaredCenterLineLengthSquared * squaredOffsetFromBottomCenter - length2(signedOffsetFromBottomOnCenterLineSquared) * lengthRadiusDifferenceSquared +
		squaredCenterLineLength * bottomRadius * (radiusDifference * signedOffsetFromBottomOnCenterLineSquared * 2.0f - squaredCenterLineLength * bottomRadius);
	const auto discriminant = length2(k1) - k2 * k0;
	if (discriminant < 0) {
		return RayMiss{};
	}
	const Length1D t = (-k1 - sqrt(discriminant)) / k2;
	const SquaredDistance hitSignedOffsetFromBottomCenterSquared = signedOffsetFromBottomOnCenterLineSquared + t * directionAlongCenterLine;
	if (hitSignedOffsetFromBottomCenterSquared < 0 || hitSignedOffsetFromBottomCenterSquared > squaredCenterLineLength) {
		return RayMiss{};
	}
	if (t < 0 || t > maxLocalRayDistance) {
		return RayMiss{};
	}
	return RayHit3D{
		.localOffset = localRayOrigin + t * localRayDirection,
		.distance = t,
		.normal = tryNormalize(squaredCenterLineLength *
								   (squaredCenterLineLength * (offsetFromBottomCenter + t * localRayDirection) + radiusDifference * (Y_AXIS_3D * centerLineLength) * bottomRadius) -
							   (Y_AXIS_3D * centerLineLength) * lengthRadiusDifferenceSquared * hitSignedOffsetFromBottomCenterSquared)
	        .value_or(-localRayDirection),
	};
}

bool TaperedCylinderShape3D::containsLocalPoint(Length3D localPoint) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});

	const Distance centerLineLength = 2.0f * halfLength;
	const Length1D verticalPoint = localPoint.getY();
	if (abs(verticalPoint) >= centerLineLength) {
		return false;
	}

	const Length1D radiusDifference = bottomRadius - topRadius;
	const Distance horizontalPointDistance = length(localPoint.get(X, Z));
	const Scale1D radiusDifferenceScale = ((horizontalPointDistance - topRadius) * radiusDifference + (centerLineLength - verticalPoint) * 2.0f * centerLineLength) /
	                                      (length2(radiusDifference) + length2(2.0f * centerLineLength));
	return horizontalPointDistance - topRadius - radiusDifference * clamp(radiusDifferenceScale, Scale1D{}, Scale1D{1}) < 0;
}

Length3D TaperedCylinderShape3D::getLocalSupportPointOffset(Direction3D localDirection) const {
	GREM_ASSERT(bottomRadius >= Distance{});
	GREM_ASSERT(topRadius >= Distance{});
	GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
	GREM_ASSERT(halfLength > Distance{});
	const Length3D linePoint = Y_AXIS_3D * ((localDirection.getY() >= 0) ? halfLength : -halfLength);
	const Distance radius = (localDirection.getY() >= 0) ? topRadius : bottomRadius;
	return Length3D{
		linePoint.getX() + radius * localDirection.getX(),
		linePoint.getY(),
		linePoint.getZ() + radius * localDirection.getZ(),
	};
}

template <size_t N>
ConvexPolytopeShape<N>::ConvexPolytopeShape(Span<const Vertex> vertices, ConvexPolytopeVertexIndex maxVertexCount)
	: ConvexPolytopeShape(SharedPointer<ConvexPolytope<N>>::create(vertices, maxVertexCount)) {}

template <size_t N>
Volume ConvexPolytopeShape<N>::calculateVolume() const {
	// Approximate as the volume of the bounding box.
	const grem::Box<N, float> boundingBox = polytope->getBoundingBox();
	const grem::Length<N, float> aabbExtents = boundingBox.max - boundingBox.min;
	return product(aabbExtents) * Volume::UNIT;
}

template <size_t N>
PrincipalMomentsOfInertia<N> ConvexPolytopeShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	// Approximate as the moment of inertia of the bounding box.
	const grem::Box<N, float> boundingBox = polytope->getBoundingBox();
	const Length<N> halfExtents = ((boundingBox.max - boundingBox.min) * 0.5f) * Length<N>::UNIT;
	if (any(lessThanEqual(halfExtents, 0))) {
		return {};
	}
	return BoxShape<N>{.halfExtents = halfExtents}.calculatePrincipalMomentsOfInertia(mass);
}

template <size_t N>
Optional<Area> ConvexPolytopeShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	// Approximate as the reference area of the bounding box.
	const grem::Box<N, float> boundingBox = polytope->getBoundingBox();
	const Length<N> halfExtents = ((boundingBox.max - boundingBox.min) * 0.5f) * Length<N>::UNIT;
	if (any(lessThanEqual(halfExtents, 0))) {
		return {};
	}
	return BoxShape<N>{.halfExtents = halfExtents}.getReferenceArea(basis, direction);
}

template <size_t N>
RaycastResult<N> ConvexPolytopeShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	Length1D farthestFrontFacingDistance = Length1D::MIN;
	Length1D closestBackFacingDistance = Length1D::MAX;
	ConvexPolytopeFaceIndex farthestFrontFacingFaceIndex = 0;

	const ConvexPolytopeFaceIndex faceCount = getFaceCount();
	for (ConvexPolytopeFaceIndex faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
		const Length<N> localFacePoint = getLocalFaceOffset(faceIndex);
		const Direction<N> localFaceNormal = getLocalFaceNormal(faceIndex);

		const Scale1D faceNormalAlongRay = dot(localFaceNormal, localRayDirection);
		const Length1D rayOffsetFromFaceAlongFaceNormal = dot(localRayOrigin - localFacePoint, localFaceNormal);

		const bool rayIsParallelWithFacePlane = abs(faceNormalAlongRay) < Scale1D::MACHINE_EPSILON;
		if (rayIsParallelWithFacePlane) {
			const bool rayIsInFrontOfFacePlane = rayOffsetFromFaceAlongFaceNormal > 0;
			if (rayIsInFrontOfFacePlane) {
				// The ray is in front of a face and parallel with it.
				// This means we have missed the entire shape, since it's convex.
				return RayMiss{};
			}

			// We can't hit a parallel face. Ignore it.
			continue;
		}

		// Compute signed hit distance.
		const Length1D t = -rayOffsetFromFaceAlongFaceNormal / faceNormalAlongRay;

		const bool faceIsFrontFacing = signbit(faceNormalAlongRay);
		if (faceIsFrontFacing) {
			// Face is front-facing. Make sure it's not further than any previous back-facing face.
			if (t > closestBackFacingDistance) {
				return RayMiss{};
			}

			// Update farthest front-facing distance.
			if (t > farthestFrontFacingDistance) {
				farthestFrontFacingDistance = t;
				farthestFrontFacingFaceIndex = faceIndex;
			}
		} else {
			// Face is back-facing. Make sure it's not closer than any previous front-facing face.
			if (t < farthestFrontFacingDistance) {
				return RayMiss{};
			}

			// Update closest back-facing distance.
			if (t < closestBackFacingDistance) {
				closestBackFacingDistance = t;
			}
		}
	}

	if (!signbit(farthestFrontFacingDistance)) {
		// We hit a front face.
		if (farthestFrontFacingDistance > maxLocalRayDistance) {
			return RayMiss{};
		}
		return RayHit<N>{
			.localOffset = localRayOrigin + farthestFrontFacingDistance * localRayDirection,
			.distance = farthestFrontFacingDistance,
			.normal = getLocalFaceNormal(farthestFrontFacingFaceIndex),
		};
	}

	if (!signbit(closestBackFacingDistance)) {
		// We hit a back face from inside the shape.
		return RayHitInterior{.localOffset = localRayOrigin};
	}

	return RayMiss{};
}

template <size_t N>
[[nodiscard]] bool ConvexPolytopeShape<N>::containsLocalPoint(Length<N> localPoint) const {
	const ConvexPolytopeFaceIndex faceCount = getFaceCount();
	for (ConvexPolytopeFaceIndex faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
		const Length<N> localFacePoint = getLocalFaceOffset(faceIndex);
		const Direction<N> localFaceNormal = getLocalFaceNormal(faceIndex);
		if (dot(localPoint - localFacePoint, localFaceNormal) >= 0) {
			return false;
		}
	}
	return true;
}

template <size_t N>
Length<N> ConvexPolytopeShape<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	const Span<const Vertex> vertices = polytope->getVertices();
	if (vertices.empty()) {
		[[unlikely]];
		return {};
	}
	return vertices[getLocalSupportPointVertexIndex(localDirection, 0)] * Length<N>::UNIT;
}

template <size_t N>
ConvexPolytopeVertexIndex ConvexPolytopeShape<N>::getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex searchStartVertexIndex) const {
	const Span<const Vertex> vertices = polytope->getVertices();
	if (vertices.empty()) {
		[[unlikely]];
		return 0;
	}
	if constexpr (N == 3) {
		const Span<const Edge> edges = polytope->getEdges();
		ConvexPolytopeVertexIndex farthestVertexIndex = searchStartVertexIndex;
		float farthestSignedDistance = dot(vertices[farthestVertexIndex], vec<N, float>{localDirection});
		ConvexPolytopeEdgeIndex farthestVertexEdgeIndex = polytope->getVertexEdgeIndices()[farthestVertexIndex];
		ConvexPolytopeEdgeIndex oldFarthestVertexEdgeIndex{};
		do {
			oldFarthestVertexEdgeIndex = farthestVertexEdgeIndex;
			const ConvexPolytopeEdgeIndex firstEdgeIndex = farthestVertexEdgeIndex;
			ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
			do {
				const ConvexPolytopeEdgeIndex twinEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndex ^ 1);
				const ConvexPolytopeVertexIndex twinVertexIndex = edges[twinEdgeIndex].vertexIndex;
				const float twinSignedDistance = dot(vertices[twinVertexIndex], vec<N, float>{localDirection});
				if (twinSignedDistance - farthestSignedDistance > Limits<float>::MACHINE_EPSILON * 2.0f) {
					farthestVertexIndex = twinVertexIndex;
					farthestSignedDistance = twinSignedDistance;
					farthestVertexEdgeIndex = twinEdgeIndex;
				}
				edgeIndex = edges[twinEdgeIndex].nextEdgeIndex;
			} while (edgeIndex != firstEdgeIndex);
		} while (farthestVertexEdgeIndex != oldFarthestVertexEdgeIndex);
		return farthestVertexIndex;
	} else {
		ConvexPolytopeVertexIndex farthestVertexIndex = 0;
		float farthestSignedDistance = dot(vertices.front(), vec<N, float>{localDirection});
		for (size_t vertexIndex = 1; vertexIndex < vertices.size(); ++vertexIndex) {
			const float signedDistance = dot(vertices[vertexIndex], vec<N, float>{localDirection});
			if (signedDistance > farthestSignedDistance) {
				farthestVertexIndex = static_cast<ConvexPolytopeVertexIndex>(vertexIndex);
				farthestSignedDistance = signedDistance;
			}
		}
		return farthestVertexIndex;
	}
}

template <size_t N>
Length<N> ConvexPolytopeShape<N>::getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const {
	return polytope->getVertices()[vertexIndex] * Length<N>::UNIT;
}

template <size_t N>
Length<N> ConvexPolytopeShape<N>::getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const {
	if constexpr (N == 3) {
		return getLocalVertexOffset(polytope->getEdges()[polytope->getFaces()[faceIndex].firstEdgeIndex].vertexIndex);
	} else {
		return getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
	}
}

template <size_t N>
Direction<N> ConvexPolytopeShape<N>::getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const {
	return Direction<N>::reinterpret(polytope->getFaces()[faceIndex].normal);
}

template <size_t N>
ConvexPolytopeFaceIndex ConvexPolytopeShape<N>::getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex searchStartFaceIndex) const {
	if constexpr (N == 3) {
		const Span<const Face> faces = polytope->getFaces();
		if (faces.empty()) {
			[[unlikely]];
			return 0;
		}
		const Span<const Edge> edges = polytope->getEdges();
		const ConvexPolytopeVertexIndex vertexIndex = getLocalSupportPointVertexIndex(localDirection, edges[faces[searchStartFaceIndex].firstEdgeIndex].vertexIndex);
		ConvexPolytopeFaceIndex mostFittingFaceIndex = 0;
		Scale1D largestDotProduct = Scale1D::MIN;
		forEachFaceIndexAroundVertex(*this, vertexIndex, [&](ConvexPolytopeFaceIndex faceIndex) -> void {
			const Scale1D dotProduct = dot(localDirection, getLocalFaceNormal(faceIndex));
			if (dotProduct > largestDotProduct) {
				mostFittingFaceIndex = faceIndex;
				largestDotProduct = dotProduct;
			}
		});
		return mostFittingFaceIndex;
	} else {
		ConvexPolytopeFaceIndex mostFittingFaceIndex = 0;
		Scale1D largestDotProduct = Scale1D::MIN;
		const ConvexPolytopeFaceIndex faceCount = getFaceCount();
		for (ConvexPolytopeFaceIndex faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			const Scale1D dotProduct = dot(localDirection, getLocalFaceNormal(faceIndex));
			if (dotProduct > largestDotProduct) {
				mostFittingFaceIndex = faceIndex;
				largestDotProduct = dotProduct;
			}
		}
		return mostFittingFaceIndex;
	}
}

template <size_t N>
ConvexPolytopeVertexIndex ConvexPolytopeShape<N>::getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return polytope->getEdges()[edgeIndex].vertexIndex;
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeFaceIndex ConvexPolytopeShape<N>::getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return polytope->getEdges()[edgeIndex].faceIndex;
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShape<N>::getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return polytope->getEdges()[edgeIndex].nextEdgeIndex;
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShape<N>::getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return polytope->getFaces()[faceIndex].firstEdgeIndex;
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShape<N>::getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return polytope->getVertexEdgeIndices()[vertexIndex];
	} else {
		unreachable();
	}
}

template class ConvexPolytopeShape<2>;
template class ConvexPolytopeShape<3>;

template <size_t N>
TriangleMeshShape<N>::TriangleMeshShape(Allocation<Vertex> vertices, Allocation<VertexIndex> indices)
	: mesh(SharedPointer<TriangleMesh<N>>::create(std::move(vertices), std::move(indices))) {}

template <size_t N>
TriangleMeshShape<N>::TriangleMeshShape(Span<const Vertex> vertices, Span<const VertexIndex> indices)
	: mesh(SharedPointer<TriangleMesh<N>>::create(vertices, indices)) {}

template <size_t N>
Volume TriangleMeshShape<N>::calculateVolume() const {
	// Approximate as the volume of the bounding box.
	const grem::Box<N, float> boundingBox = mesh->getBoundingBox();
	const grem::Length<N, float> aabbExtents = boundingBox.max - boundingBox.min;
	return product(aabbExtents) * Volume::UNIT;
}

template <size_t N>
PrincipalMomentsOfInertia<N> TriangleMeshShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	// Approximate as the moment of inertia of the bounding box.
	const grem::Box<N, float> boundingBox = mesh->getBoundingBox();
	const Length<N> halfExtents = ((boundingBox.max - boundingBox.min) * 0.5f) * Length<N>::UNIT;
	if (any(lessThanEqual(halfExtents, 0))) {
		return {};
	}
	return BoxShape<N>{.halfExtents = halfExtents}.calculatePrincipalMomentsOfInertia(mass);
}

template <size_t N>
Optional<Area> TriangleMeshShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	// Approximate as the reference area of the bounding box.
	const grem::Box<N, float> boundingBox = mesh->getBoundingBox();
	const Length<N> halfExtents = ((boundingBox.max - boundingBox.min) * 0.5f) * Length<N>::UNIT;
	if (any(lessThanEqual(halfExtents, 0))) {
		return {};
	}
	return BoxShape<N>{.halfExtents = halfExtents}.getReferenceArea(basis, direction);
}

template <size_t N>
RaycastResult<N> TriangleMeshShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	const Ray<N> ray{.origin = localRayOrigin, .direction = localRayDirection, .maxDistance = maxLocalRayDistance};
	const Span<const Vertex> vertices = mesh->getVertices();
	const Span<const VertexIndex> indices = mesh->getIndices();

	RaycastResult<N> result{};
	mesh->getFaceOrthtree().traverseElements(
		[&](const FaceIndex& faceIndex) -> void {
			const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
			const Triangle<N> triangle{
				.pointA = vertices[indices[indexOffset + 0]] * Position<N>::UNIT,
				.pointB = vertices[indices[indexOffset + 1]] * Position<N>::UNIT,
				.pointC = vertices[indices[indexOffset + 2]] * Position<N>::UNIT,
			};
			GREM_MATCH(triangle.raycast(ray)) {
				GREM_CASE(const RayMiss& miss) break;
				GREM_CASE(const RayHit<N>& hit) {
					if (result.template is<RayMiss>() || (result.template is<RayHit<N>>() && hit.distance < result.template as<RayHit<N>>().distance)) {
						result = hit;
					}
					break;
				}
				GREM_CASE(const RayHitInterior<N>& hitInterior) {
					result = hitInterior;
					break;
				}
			}
		},
		[&](const grem::Box<N, float>& boundingBox) -> bool { return boundingBox.intersects(ray.in(Ray<N>::UNIT)); });
	return result;
}

template class TriangleMeshShape<2>;
template class TriangleMeshShape<3>;

template <size_t N>
Volume LocallyTransformedShape<N>::calculateVolume() const {
	return product(localScale) * ShapeView<N>{*shape}.calculateVolume();
}

template <size_t N>
PrincipalMomentsOfInertia<N> LocallyTransformedShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	if constexpr (N == 3) {
		const PrincipalMomentsOfInertia3D principalMomentsOfInertia = ShapeView3D{*shape}.calculatePrincipalMomentsOfInertia(mass);
		// Estimate by assuming that localOrientation is 0, since the result would be a non-diagonal matrix otherwise.
		return {
			localScale.getY() * localScale.getZ() * principalMomentsOfInertia.getX() + mass * length2(localOffset.get(Y, Z)),
			localScale.getX() * localScale.getZ() * principalMomentsOfInertia.getY() + mass * length2(localOffset.get(X, Z)),
			localScale.getX() * localScale.getY() * principalMomentsOfInertia.getZ() + mass * length2(localOffset.get(X, Y)),
		};
	} else {
		return localScale.getX() * localScale.getY() * ShapeView<N>{*shape}.calculatePrincipalMomentsOfInertia(mass) + mass * length2(localOffset);
	}
}

template <size_t N>
Optional<Box<N>> LocallyTransformedShape<N>::getBoundingBox(const Transformation<N>& transformation) const {
	return ShapeView<N>{*shape}.getBoundingBox(transformation * translateRotateScale(localOffset, localOrientation, localScale));
}

template <size_t N>
Optional<Distance> LocallyTransformedShape<N>::getBoundingRadius(const Basis<N>& basis) const {
	return ShapeView<N>{*shape}.getBoundingRadius(basis * rotateScale(localOrientation, localScale));
}

template <size_t N>
Optional<Area> LocallyTransformedShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	return ShapeView<N>{*shape}.getReferenceArea(basis * rotateScale(localOrientation, localScale), direction);
}

template <size_t N>
RaycastResult<N> LocallyTransformedShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	const LocalTransformation<N> localTransformation = translateRotateScale(localOffset, localOrientation, localScale);
	const InverseLocalTransformation<N> inverseLocalTransformation = inverseTranslateRotateScale(localOffset, localOrientation, localScale);
	const Length<N> shapeLocalRayOrigin = inverseLocalTransformation(localRayOrigin);
	const Length<N> shapeLocalRayVector = inverseLocalTransformation.getRelative(localRayDirection * maxLocalRayDistance);
	const Distance maxShapeLocalRayDistance = length(shapeLocalRayVector);
	const Direction<N> shapeLocalRayDirection = Direction<N>::reinterpret(shapeLocalRayVector / maxShapeLocalRayDistance);
	GREM_ASSERT(maxShapeLocalRayDistance > Distance{});
	GREM_MATCH(ShapeView<N>{*shape}.castLocalRay(shapeLocalRayOrigin, shapeLocalRayDirection, maxShapeLocalRayDistance)) {
		GREM_CASE(const RayMiss& miss) return RayMiss{};
		GREM_CASE(const RayHit<N>& hit) {
			return RayHit<N>{
				.localOffset = localTransformation(hit.localOffset),
				.distance = localTransformation.getDistance(localRayDirection * hit.distance),
				.normal = localTransformation.getDirection(hit.normal),
			};
		}
		GREM_CASE(const RayHitInterior<N>& hitInterior) {
			return RayHitInterior<N>{
				.localOffset = localTransformation(hitInterior.localOffset),
			};
			break;
		}
	}
	unreachable();
}

template class LocallyTransformedShape<2>;
template class LocallyTransformedShape<3>;

template <size_t N>
CompoundColliderShape<N>::CompoundColliderShape(Span<const SubCollider<N>> subColliders)
	: subColliders(SharedPointer<SubCollider<N>[]>::create(subColliders.size()))
	, boundingBox(calculateCompoundBoundingBox(subColliders))
	, boundingRadius(calculateCompoundBoundingRadius(subColliders)) {
	copy(subColliders, this->subColliders.get());
}

template <size_t N>
Volume CompoundColliderShape<N>::calculateVolume() const {
	Volume result{};
	for (const SubCollider<N>& subCollider : getSubColliders()) {
		result += product(subCollider.localScale) * ShapeView<N>{subCollider.collider.shape}.calculateVolume();
	}
	return result;
}

template <size_t N>
PrincipalMomentsOfInertia<N> CompoundColliderShape<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	const Volume totalVolume = calculateVolume();
	const auto inverseTotalVolume = Coefficient{1} / totalVolume;
	PrincipalMomentsOfInertia<N> result{};
	for (const SubCollider<N>& subCollider : getSubColliders()) {
		const Coefficient volumeAmount = product(subCollider.localScale) * ShapeView<N>{subCollider.collider.shape}.calculateVolume() * inverseTotalVolume;
		const Mass shapeMass = volumeAmount * mass;
		if constexpr (N == 3) {
			const PrincipalMomentsOfInertia<N> shapePrincipalMomentsOfInertia = ShapeView<N>{subCollider.collider.shape}.calculatePrincipalMomentsOfInertia(shapeMass);
			// Estimate by assuming that localOrientation is 0, since the result would be a non-diagonal matrix otherwise.
			result[X] +=
				subCollider.localScale.getY() * subCollider.localScale.getZ() * shapePrincipalMomentsOfInertia.getX() + shapeMass * length2(subCollider.localOffset.get(Y, Z));
			result[Y] +=
				subCollider.localScale.getX() * subCollider.localScale.getZ() * shapePrincipalMomentsOfInertia.getY() + shapeMass * length2(subCollider.localOffset.get(X, Z));
			result[Z] +=
				subCollider.localScale.getX() * subCollider.localScale.getY() * shapePrincipalMomentsOfInertia.getZ() + shapeMass * length2(subCollider.localOffset.get(X, Y));
		} else {
			result += subCollider.localScale.getX() * subCollider.localScale.getY() * ShapeView<N>{subCollider.collider.shape}.calculatePrincipalMomentsOfInertia(shapeMass) +
			          shapeMass * length2(subCollider.localOffset);
		}
	}
	return result;
}

template <size_t N>
Optional<Area> CompoundColliderShape<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	// Approximate as the sum of the subshapes' reference areas, even though they might be overlapping.
	Optional<Area> result{};
	for (const SubCollider<N>& subCollider : getSubColliders()) {
		if (const Optional<Area> shapeReferenceArea =
				ShapeView<N>{subCollider.collider.shape}.getReferenceArea(basis * rotateScale(subCollider.localOrientation, subCollider.localScale), direction)) {
			if (result) {
				*result += *shapeReferenceArea;
			} else {
				result = shapeReferenceArea;
			}
		}
	}
	return result;
}

template <size_t N>
RaycastResult<N> CompoundColliderShape<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	RaycastResult<N> result{};
	for (const SubCollider<N>& subCollider : getSubColliders()) {
		const LocalTransformation<N> subLocalTransformation = translateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale);
		const InverseLocalTransformation<N> inverseSubLocalTransformation =
			inverseTranslateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale);
		const Length<N> subLocalRayOrigin = inverseSubLocalTransformation(localRayOrigin);
		const Length<N> subLocalRayVector = inverseSubLocalTransformation.getRelative(localRayDirection * maxLocalRayDistance);
		const Distance maxSubLocalRayDistance = length(subLocalRayVector);
		const Direction<N> subLocalRayDirection = Direction<N>::reinterpret(subLocalRayVector / maxSubLocalRayDistance);
		GREM_ASSERT(maxSubLocalRayDistance > Distance{});
		GREM_MATCH(ShapeView<N>{subCollider.collider.shape}.castLocalRay(subLocalRayOrigin, subLocalRayDirection, maxSubLocalRayDistance)) {
			GREM_CASE(const RayMiss& miss) break;
			GREM_CASE(const RayHit<N>& hit) {
				const Distance distance = subLocalTransformation.getDistance(subLocalRayDirection * hit.distance);
				if (result.template is<RayMiss>() || (result.template is<RayHit<N>>() && distance < result.template as<RayHit<N>>().distance)) {
					result = RayHit<N>{
						.localOffset = subLocalTransformation(hit.localOffset),
						.distance = distance,
						.normal = subLocalTransformation.getDirection(hit.normal),
					};
				}
				break;
			}
			GREM_CASE(const RayHitInterior<N>& hitInterior) {
				result = RayHitInterior<N>{
					.localOffset = subLocalTransformation(hitInterior.localOffset),
				};
				break;
			}
		}
	}
	return result;
}

template <size_t N>
Optional<Box<N>> CompoundColliderShape<N>::calculateCompoundBoundingBox(Span<const SubCollider<N>> subColliders) noexcept {
	Optional<Box<N>> result{};
	for (const SubCollider<N>& subCollider : subColliders) {
		if (const Optional<Box<N>> shapeBoundingBox = ShapeView<N>{subCollider.collider.shape}.getBoundingBox(
				Transformation<N>{0, translateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale)})) {
			if (result) {
				result->min = min(result->min, shapeBoundingBox->min);
				result->max = max(result->max, shapeBoundingBox->max);
			} else {
				result = shapeBoundingBox;
			}
		}
	}
	return result;
}

template <size_t N>
Optional<Distance> CompoundColliderShape<N>::calculateCompoundBoundingRadius(Span<const SubCollider<N>> subColliders) noexcept {
	Optional<Distance> result{};
	for (const SubCollider<N>& subCollider : subColliders) {
		if (const Optional<Distance> shapeBoundingRadius =
				ShapeView<N>{subCollider.collider.shape}.getBoundingRadius(rotateScale(subCollider.localOrientation, subCollider.localScale))) {
			const Distance shapeBoundingExtent = length(subCollider.localOffset) + *shapeBoundingRadius;
			if (result) {
				result = max(*result, shapeBoundingExtent);
			} else {
				result = shapeBoundingExtent;
			}
		}
	}
	return result;
}

template class CompoundColliderShape<2>;
template class CompoundColliderShape<3>;

template <size_t N>
Volume ShapeView<N>::calculateVolume() const {
	return match(shape)([](const auto& shape) -> Volume { return shape.calculateVolume(); });
}

template <size_t N>
PrincipalMomentsOfInertia<N> ShapeView<N>::calculatePrincipalMomentsOfInertia(Mass mass) const {
	return match(shape)([mass](const auto& shape) -> PrincipalMomentsOfInertia<N> { return shape.calculatePrincipalMomentsOfInertia(mass); });
}

template <size_t N>
Optional<Box<N>> ShapeView<N>::getBoundingBox(const Transformation<N>& transformation) const {
	return match(shape)([&](const auto& shape) -> Optional<Box<N>> { return shape.getBoundingBox(transformation); });
}

template <size_t N>
Optional<Distance> ShapeView<N>::getBoundingRadius(const Basis<N>& basis) const {
	return match(shape)([&](const auto& shape) -> Optional<Distance> { return shape.getBoundingRadius(basis); });
}

template <size_t N>
Optional<Area> ShapeView<N>::getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
	return match(shape)([&](const auto& shape) -> Optional<Area> { return shape.getReferenceArea(basis, direction); });
}

template <size_t N>
RaycastResult<N> ShapeView<N>::castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
	return match(shape)([&](const auto& shape) -> RaycastResult<N> { return shape.castLocalRay(localRayOrigin, localRayDirection, maxLocalRayDistance); });
}

template class ShapeView<2>;
template class ShapeView<3>;

template <size_t N>
bool ConvexShapeView<N>::containsLocalPoint(Length<N> localPoint) const {
	return match(this->shape)(                                                                                       //
		[&](const convex_shape<N> auto& convexShape) -> bool { return convexShape.containsLocalPoint(localPoint); }, //
		[&](const auto&) -> bool { throw BadVariantAccess{}; });
}

template <size_t N>
Length<N> ConvexShapeView<N>::getLocalSupportPointOffset(Direction<N> localDirection) const {
	return match(this->shape)(                                                                                                        //
		[&](const convex_shape<N> auto& convexShape) -> Length<N> { return convexShape.getLocalSupportPointOffset(localDirection); }, //
		[&](const auto&) -> Length<N> { throw BadVariantAccess{}; });
}

template class ConvexShapeView<2>;
template class ConvexShapeView<3>;

template <size_t N>
ConvexPolytopeVertexIndex ConvexPolytopeShapeView<N>::getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex searchStartVertexIndex) const {
	return match(this->shape)( //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeVertexIndex {
			return convexPolytopeShape.getLocalSupportPointVertexIndex(localDirection, searchStartVertexIndex);
		}, //
		[&](const auto&) -> ConvexPolytopeVertexIndex { throw BadVariantAccess{}; });
}

template <size_t N>
ConvexPolytopeVertexIndex ConvexPolytopeShapeView<N>::getVertexCount() const {
	return match(this->shape)(                                                                                                                       //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeVertexIndex { return convexPolytopeShape.getVertexCount(); }, //
		[&](const auto&) -> ConvexPolytopeVertexIndex { throw BadVariantAccess{}; });
}

template <size_t N>
ConvexPolytopeFaceIndex ConvexPolytopeShapeView<N>::getFaceCount() const {
	return match(this->shape)(                                                                                                                   //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeFaceIndex { return convexPolytopeShape.getFaceCount(); }, //
		[&](const auto&) -> ConvexPolytopeFaceIndex { throw BadVariantAccess{}; });
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShapeView<N>::getEdgeCount() const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                   //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeEdgeIndex { return convexPolytopeShape.getEdgeCount(); }, //
			[&](const auto&) -> ConvexPolytopeEdgeIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template <size_t N>
Length<N> ConvexPolytopeShapeView<N>::getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const {
	return match(this->shape)(                                                                                                                        //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> Length<N> { return convexPolytopeShape.getLocalVertexOffset(vertexIndex); }, //
		[&](const auto&) -> Length<N> { throw BadVariantAccess{}; });
}

template <size_t N>
Length<N> ConvexPolytopeShapeView<N>::getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const {
	return match(this->shape)(                                                                                                                    //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> Length<N> { return convexPolytopeShape.getLocalFaceOffset(faceIndex); }, //
		[&](const auto&) -> Length<N> { throw BadVariantAccess{}; });
}

template <size_t N>
Direction<N> ConvexPolytopeShapeView<N>::getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const {
	return match(this->shape)(                                                                                                                       //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> Direction<N> { return convexPolytopeShape.getLocalFaceNormal(faceIndex); }, //
		[&](const auto&) -> Direction<N> { throw BadVariantAccess{}; });
}

template <size_t N>
ConvexPolytopeFaceIndex ConvexPolytopeShapeView<N>::getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex searchStartFaceIndex) const {
	return match(this->shape)( //
		[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeFaceIndex {
			return convexPolytopeShape.getFaceIndexWithMostFittingLocalNormal(localDirection, searchStartFaceIndex);
		}, //
		[&](const auto&) -> ConvexPolytopeFaceIndex { throw BadVariantAccess{}; });
}

template <size_t N>
ConvexPolytopeVertexIndex ConvexPolytopeShapeView<N>::getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                                           //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeVertexIndex { return convexPolytopeShape.getFirstVertexIndexOfEdge(edgeIndex); }, //
			[&](const auto&) -> ConvexPolytopeVertexIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeFaceIndex ConvexPolytopeShapeView<N>::getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                                  //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeFaceIndex { return convexPolytopeShape.getFaceIndexOfEdge(edgeIndex); }, //
			[&](const auto&) -> ConvexPolytopeFaceIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShapeView<N>::getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                                //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeEdgeIndex { return convexPolytopeShape.getNextEdgeIndex(edgeIndex); }, //
			[&](const auto&) -> ConvexPolytopeEdgeIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShapeView<N>::getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                                       //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeEdgeIndex { return convexPolytopeShape.getFirstEdgeIndexOfFace(faceIndex); }, //
			[&](const auto&) -> ConvexPolytopeEdgeIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template <size_t N>
ConvexPolytopeEdgeIndex ConvexPolytopeShapeView<N>::getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3) {
	if constexpr (N == 3) {
		return match(this->shape)(                                                                                                                                          //
			[&](const convex_polytope_shape<N> auto& convexPolytopeShape) -> ConvexPolytopeEdgeIndex { return convexPolytopeShape.getSomeEdgeIndexOfVertex(vertexIndex); }, //
			[&](const auto&) -> ConvexPolytopeEdgeIndex { throw BadVariantAccess{}; });
	} else {
		unreachable();
	}
}

template class ConvexPolytopeShapeView<2>;
template class ConvexPolytopeShapeView<3>;

template struct ColliderView<2>;
template struct ColliderView<3>;

} // namespace grem::physics
