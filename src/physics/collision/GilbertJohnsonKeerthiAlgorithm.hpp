// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_GILBERT_JOHNSON_KEERTHI_ALGORITHM_HPP
#define GREM_PHYSICS_COLLISION_GILBERT_JOHNSON_KEERTHI_ALGORITHM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include "minkowski_difference.hpp"

namespace grem::physics {

template <size_t N>
class GilbertJohnsonKeerthiAlgorithm {
public:
	static constexpr Direction<N> DEFAULT_INITIAL_DIRECTION = Direction<N>::reinterpret(Scale<N>{(N == 3) ? numbers::INV_SQRT3 : 1.0f / numbers::SQRT2});

	struct FindAnyIntersectionResult {
		MinkowskiSimplex<N> simplex{};
		size_t iteration = 0;
		bool foundIntersection = false;
	};

	[[nodiscard]] FindAnyIntersectionResult findAnyIntersection(FunctionView<MinkowskiVertex<N>(Direction<N> direction)> support, Distance collisionDistanceErrorTolerance,
		size_t maxIterationCount, Direction<N> initialDirection = DEFAULT_INITIAL_DIRECTION) {
		GREM_PROFILE_FUNCTION();

		// If the Minkowski difference of A and B encloses the origin, that means A and B are colliding.
		// To determine if this is the case, the algorithm iteratively tries to enclose the origin with
		// a tetrahedron of 4 vertices (or a triangle of 3 vertices in 2D) from the Minkowski difference hull.
		// If it fails after a given number of iterations, we assume that there is no collision.
		// If it cannot find a vertex on the other side of the origin, we can be sure there is no collision, and terminate early.

		FindAnyIntersectionResult result{};

		// Get the farthest vertex on the Minkowski difference hull in the initial direction as an initial guess.
		result.simplex.push_back(support(initialDirection));

		// Start looking for vertices on the other side of the origin.
		Scale<N> directionScale = (result.simplex[0].second - result.simplex[0].first) / Length<N>::UNIT;

		// Try to enclose the origin `maxIterationCount` times.
		while (result.iteration < maxIterationCount) {
			++result.iteration;

			// Normalize the direction vector while making sure it's still valid.
			const Scale1D directionLengthSquared = length2(directionScale);
			if (isnan(directionLengthSquared) || directionLengthSquared <= Scale1D::MACHINE_EPSILON) {
				break; // The direction vector has become invalid. The algorithm can't continue.
			}
			const Direction<N> direction = Direction<N>::reinterpret(directionScale / sqrt(directionLengthSquared));

			// Get the farthest supporting vertex on the Minkowski difference hull in the current search direction.
			const MinkowskiVertex<N> newVertex = support(direction);

			// Make sure that the farthest vertex is actually on the other side of the origin.
			if (dot(newVertex.first - newVertex.second, direction) <= Length1D::MACHINE_EPSILON) {
				break; // We can't enclose the origin. There is no collision.
			}

			// Add the vertex and reduce the new simplex to the feature that now contains the point closest to the origin,
			// and update the search direction to point outwards from the region (towards the other side of the origin).
			result.simplex.push_back(newVertex);
			if (const Optional<Scale<N>> newDirection = reduceSimplexToFeatureClosestToOrigin(result.simplex, collisionDistanceErrorTolerance,
					[](const MinkowskiVertex<N>& vertex) -> Length<N> { return vertex.first - vertex.second; })) {
				directionScale = *newDirection;
			} else {
				// The simplex has successfully enclosed the origin with a tetrahedron (or triangle in 2D),
				// meaning the Minkowski difference contains the origin, so the shapes must be colliding.
				result.foundIntersection = true;
				break;
			}
		}
		return result;
	}

	[[nodiscard]] FindAnyIntersectionResult findAnyIntersection(const convex_shape<N> auto& shapeA, const Transformation<N>& transformationA,
		const InverseTransformation<N>& inverseTransformationA, const convex_shape<N> auto& shapeB, const Transformation<N>& transformationB,
		const InverseTransformation<N>& inverseTransformationB, Distance margin, Distance collisionDistanceErrorTolerance, size_t maxIterationCount,
		Direction<N> initialDirection = DEFAULT_INITIAL_DIRECTION) {
		return findAnyIntersection(
			[&](Direction<N> direction) -> MinkowskiVertex<N> {
				const Direction<N> localDirectionA = inverseTransformationA.getDirection(direction);
				const Direction<N> localDirectionB = inverseTransformationB.getDirection(direction);
				return {
					transformationA(shapeA.getLocalSupportPointOffset(localDirectionA)) + direction * margin,
					transformationB(shapeB.getLocalSupportPointOffset(-localDirectionB)) - direction * margin,
				};
			},
			collisionDistanceErrorTolerance, maxIterationCount, initialDirection);
	}

	struct FindRayIntersectionResult {
		MinkowskiVertex<N> vertex{};
		Scale<N> normal{};
		Distance distance{};
		size_t iteration = 0;
		bool hit = false;
		bool interior = false;
	};

	[[nodiscard]] FindRayIntersectionResult findRayIntersection(Position<N> rayOrigin, Direction<N> rayDirection, Distance maxRayDistance,
		FunctionView<MinkowskiVertex<N>(Direction<N> direction)> support, Distance collisionDistanceErrorTolerance, size_t maxIterationCount) {
		GREM_PROFILE_FUNCTION();

		// Reference: Gino van den Bergen: "Ray Casting against General Convex Objects with Application to Continuous Collision Detection": Playlogic Game Factory (2004): http://dtecta.com/papers/jgt04raycast.pdf

		const SquaredDistance collisionDistanceErrorToleranceSquared = max(length2(collisionDistanceErrorTolerance), SquaredDistance::MACHINE_EPSILON * 100.0f);

		FindRayIntersectionResult result{.vertex = support(rayDirection)};

		Position<N> rayPosition = rayOrigin;

		const auto projection = [&](const MinkowskiVertex<N>& vertex) -> Length<N> {
			return (rayPosition - 0) - (vertex.first - vertex.second);
		};

		Length<N> offsetToRayPosition = projection(result.vertex);
		SquaredDistance squaredDistanceToRayPosition = length2(offsetToRayPosition);
		if (isnan(squaredDistanceToRayPosition)) {
			return result;
		}

		MinkowskiSimplex<N> simplex{};
		SquaredDistance maxSquaredDistanceToRayPosition = SquaredDistance::MAX;
		bool rayPositionMoved = false;

		while (result.iteration < maxIterationCount) {
			++result.iteration;

			const Distance distance = result.distance;
			const Direction<N> newNormal = Direction<N>::reinterpret(offsetToRayPosition / sqrt(squaredDistanceToRayPosition));
			const MinkowskiVertex<N> newVertex = support(newNormal);
			const Area newVertexTowardsRayPosition = dot(projection(newVertex), offsetToRayPosition);
			if (newVertexTowardsRayPosition > 0) {
				const Length1D rayTowardsRayPosition = dot(rayDirection, offsetToRayPosition);
				if (rayTowardsRayPosition >= -Length1D::MACHINE_EPSILON) {
					break;
				}

				result.distance -= newVertexTowardsRayPosition / rayTowardsRayPosition;
				if (result.distance >= maxRayDistance) {
					break;
				}

				if (result.distance == distance) {
					result.hit = true;
					break;
				}

				rayPosition = rayOrigin + result.distance * rayDirection;
				result.normal = newNormal;

				maxSquaredDistanceToRayPosition = SquaredDistance::MAX;
				rayPositionMoved = true;
			}

			simplex.push_back(newVertex);
			if (simplex.size() != 1 && !reduceSimplexToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection)) {
				if (distance == 0) {
					result.interior = true;
				} else {
					result.hit = true;
				}
				break;
			}

			if constexpr (N == 2) {
				switch (simplex.size()) {
					case 1: result.vertex = simplex[0]; break;
					case 2: result.vertex = getPointClosestToOriginOnMinkowskiLineSegment(simplex[0], simplex[1], projection); break;
					default: unreachable();
				}
			} else {
				switch (simplex.size()) {
					case 1: result.vertex = simplex[0]; break;
					case 2: result.vertex = getPointClosestToOriginOnMinkowskiLineSegment(simplex[0], simplex[1], projection); break;
					case 3: result.vertex = getPointClosestToOriginOnMinkowskiTriangle(simplex[0], simplex[1], simplex[2], projection); break;
					default: unreachable();
				}
			}

			offsetToRayPosition = projection(result.vertex);
			const SquaredDistance newSquaredDistanceToRayPosition = length2(offsetToRayPosition);
			if (isnan(newSquaredDistanceToRayPosition)) {
				break;
			}

			if (newSquaredDistanceToRayPosition <= collisionDistanceErrorToleranceSquared) {
				result.hit = true;
				break;
			}

			if (newSquaredDistanceToRayPosition >= maxSquaredDistanceToRayPosition) {
				if (!rayPositionMoved) {
					break;
				}

				simplex = {newVertex};
				offsetToRayPosition = projection(newVertex);
				squaredDistanceToRayPosition = length2(offsetToRayPosition);
				if (isnan(squaredDistanceToRayPosition)) {
					break;
				}
				maxSquaredDistanceToRayPosition = SquaredDistance::MAX;
				rayPositionMoved = false;
			} else {
				squaredDistanceToRayPosition = newSquaredDistanceToRayPosition;
				maxSquaredDistanceToRayPosition = newSquaredDistanceToRayPosition;
			}
		}

		return result;
	}

	struct RaycastResult {
		Position<N> point;
		Scale<N> normal;
		Distance distance;
		size_t iteration;
		bool hit;
		bool interior;
	};

	[[nodiscard]] RaycastResult raycast(Position<N> rayOrigin, Direction<N> rayDirection, Distance maxRayDistance, const convex_shape<N> auto& shape,
		const Transformation<N>& transformation, const InverseTransformation<N>& inverseTransformation, Distance margin, Distance collisionDistanceErrorTolerance,
		size_t maxIterationCount) {
		const FindRayIntersectionResult result = findRayIntersection(
			rayOrigin, rayDirection, maxRayDistance,
			[&](Direction<N> direction) -> MinkowskiVertex<N> {
				return {transformation(shape.getLocalSupportPointOffset(inverseTransformation.getDirection(direction))) + direction * margin, Position<N>{}};
			},
			collisionDistanceErrorTolerance, maxIterationCount);
		return {
			.point = result.vertex.first,
			.normal = result.normal,
			.distance = result.distance,
			.iteration = result.iteration,
			.hit = result.hit,
			.interior = result.interior,
		};
	}

	struct ShapecastResult {
		Pair<Position<N>> witnessPoints;
		Scale<N> normal;
		Distance distance;
		size_t iteration;
		bool hit;
		bool interior;
	};

	[[nodiscard]] ShapecastResult shapecast(Direction<N> rayDirection, Distance maxRayDistance, const convex_shape<N> auto& shapeA, const Transformation<N>& transformationA,
		const InverseTransformation<N>& inverseTransformationA, const convex_shape<N> auto& shapeB, const Transformation<N>& transformationB,
		const InverseTransformation<N>& inverseTransformationB, Distance margin, Distance collisionDistanceErrorTolerance, size_t maxIterationCount) {
		const FindRayIntersectionResult result = findRayIntersection(
			Position<N>{}, rayDirection, maxRayDistance,
			[&](Direction<N> direction) -> MinkowskiVertex<N> {
				const Direction<N> localDirectionA = inverseTransformationA.getDirection(direction);
				const Direction<N> localDirectionB = inverseTransformationB.getDirection(direction);
				return {
					transformationB(shapeB.getLocalSupportPointOffset(localDirectionB)) + direction * margin,
					transformationA(shapeA.getLocalSupportPointOffset(-localDirectionA)) - direction * margin,
				};
			},
			collisionDistanceErrorTolerance, maxIterationCount);
		return {
			.witnessPoints{result.vertex.second, result.vertex.first},
			.normal = result.normal,
			.distance = result.distance,
			.iteration = result.iteration,
			.hit = result.hit,
			.interior = result.interior,
		};
	}

private:
	[[nodiscard]] Optional<Scale<N>> reduceSimplexToFeatureClosestToOrigin(MinkowskiSimplex<N>& simplex, Distance collisionDistanceErrorTolerance, auto projection) {
		static_assert(N == 2 || N == 3);
		if constexpr (N == 2) {
			switch (simplex.size()) {
				case 2: return reduceLineSegmentToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection);
				case 3: return reduceTriangleToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection);
				default: break;
			}
		} else {
			switch (simplex.size()) {
				case 2: return reduceLineSegmentToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection);
				case 3: return reduceTriangleToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection);
				case 4: return reduceTetrahedronToFeatureClosestToOrigin(simplex, collisionDistanceErrorTolerance, projection);
				default: break;
			}
		}
		unreachable();
	}

	[[nodiscard]] Scale<N> reduceLineSegmentToFeatureClosestToOrigin(MinkowskiSimplex<N>& simplex, Distance collisionDistanceErrorTolerance, auto projection) {
		GREM_ASSERT(simplex.size() == 2);

		const Area collisionDistanceErrorToleranceSquared = length2(collisionDistanceErrorTolerance);

		const MinkowskiVertex<N> a = simplex[1];
		const MinkowskiVertex<N> b = simplex[0];
		const Length<N> vA = projection(a);
		const Length<N> vB = projection(b);
		const Length<N> a0 = -vA;
		const Length<N> ab = vB - vA;

		Scale<N> newDirection{};
		if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
			simplex = {b, a};
			newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
		} else {
			simplex = {a};
			newDirection = a0 / Length1D::UNIT;
		}
		return newDirection;
	}

	[[nodiscard]] Optional<Scale<N>> reduceTriangleToFeatureClosestToOrigin(MinkowskiSimplex<N>& simplex, Distance collisionDistanceErrorTolerance, auto projection) {
		GREM_ASSERT(simplex.size() == 3);

		const Area collisionDistanceErrorToleranceSquared = length2(collisionDistanceErrorTolerance);
		const SquaredArea collisionDistanceErrorToleranceSquaredSquared = length2(collisionDistanceErrorToleranceSquared);

		const MinkowskiVertex<N> a = simplex[2];
		const MinkowskiVertex<N> b = simplex[1];
		const MinkowskiVertex<N> c = simplex[0];
		const Length<N> vA = projection(a);
		const Length<N> vB = projection(b);
		const Length<N> vC = projection(c);
		const Length<N> a0 = -vA;
		const Length<N> ab = vB - vA;
		const Length<N> ac = vC - vA;
		const auto abc = cross(ab, ac);

		Optional<Scale<N>> newDirection{};
		if (dot(cross(abc, ac), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
			if (dot(ac, a0) >= -collisionDistanceErrorToleranceSquared) {
				simplex = {c, a};
				newDirection = cross(cross(ac, a0), ac) / Volume::UNIT;
			} else if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
				simplex = {b, a};
				newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
			} else {
				simplex = {a};
				newDirection = a0 / Length1D::UNIT;
			}
		} else if (dot(cross(ab, abc), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
			if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
				simplex = {b, a};
				newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
			} else {
				simplex = {a};
				newDirection = a0 / Length1D::UNIT;
			}
		} else if constexpr (N == 3) {
			const Volume collisionDistanceErrorToleranceCubed = collisionDistanceErrorToleranceSquared * collisionDistanceErrorTolerance;
			if (dot(abc, a0) >= -collisionDistanceErrorToleranceCubed) {
				simplex = {c, b, a};
				newDirection = abc / Area::UNIT;
			} else {
				simplex = {b, c, a};
				newDirection = -abc / Area::UNIT;
			}
		}
		return newDirection;
	}

	[[nodiscard]] Optional<Scale3D> reduceTetrahedronToFeatureClosestToOrigin(MinkowskiSimplex3D& simplex, Distance collisionDistanceErrorTolerance, auto projection)
		requires(N == 3) {
		GREM_ASSERT(simplex.size() == 4);

		const Area collisionDistanceErrorToleranceSquared = length2(collisionDistanceErrorTolerance);
		const Volume collisionDistanceErrorToleranceCubed = collisionDistanceErrorToleranceSquared * collisionDistanceErrorTolerance;
		const SquaredArea collisionDistanceErrorToleranceSquaredSquared = length2(collisionDistanceErrorToleranceSquared);

		const MinkowskiVertex3D a = simplex[3];
		const MinkowskiVertex3D b = simplex[2];
		const MinkowskiVertex3D c = simplex[1];
		const MinkowskiVertex3D d = simplex[0];
		const Length3D vA = projection(a);
		const Length3D vB = projection(b);
		const Length3D vC = projection(c);
		const Length3D vD = projection(d);
		const Length3D a0 = -vA;
		const Length3D ab = vB - vA;
		const Length3D ac = vC - vA;
		const Length3D ad = vD - vA;
		const auto abc = cross(ab, ac);
		const auto acd = cross(ac, ad);
		const auto adb = cross(ad, ab);

		using RegionFlags = uint8_t;
		enum RegionFlag : RegionFlags {
			ENCLOSED = 0,
			ABC = 1 << 0,
			ACD = 1 << 1,
			ADB = 1 << 2,
			AC = ABC | ACD,
			AB = ABC | ADB,
			AD = ACD | ADB,
			A = ABC | ACD | ADB,
		};
		const RegionFlags regionFlags =                                                                              //
			static_cast<uint8_t>(static_cast<uint8_t>(dot(abc, a0) >= -collisionDistanceErrorToleranceCubed) << 0) | //
			static_cast<uint8_t>(static_cast<uint8_t>(dot(acd, a0) >= -collisionDistanceErrorToleranceCubed) << 1) | //
			static_cast<uint8_t>(static_cast<uint8_t>(dot(adb, a0) >= -collisionDistanceErrorToleranceCubed) << 2);

		Optional<Scale<N>> newDirection{};
		switch (regionFlags) {
			case ENCLOSED: break; // Origin is enclosed by tetrahedron.
			case ABC:
				if (dot(cross(abc, ac), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ac, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {c, a};
						newDirection = cross(cross(ac, a0), ac) / Volume::UNIT;
					} else if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {b, a};
						newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else if (dot(cross(ab, abc), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {b, a};
						newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else {
					simplex = {c, b, a};
					newDirection = abc / Area::UNIT;
				}
				break;
			case ACD:
				if (dot(cross(acd, ad), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ad, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {d, a};
						newDirection = cross(cross(ad, a0), ad) / Volume::UNIT;
					} else if (dot(ac, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {c, a};
						newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else if (dot(cross(ac, acd), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ac, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {c, a};
						newDirection = cross(cross(ac, a0), ac) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else {
					simplex = {d, c, a};
					newDirection = acd / Area::UNIT;
				}
				break;
			case AC:
				if (dot(ac, a0) >= -collisionDistanceErrorToleranceSquared) {
					simplex = {c, a};
					newDirection = cross(cross(ac, a0), ac) / Volume::UNIT;
				} else {
					simplex = {a};
					newDirection = a0 / Length1D::UNIT;
				}
				break;
			case ADB:
				if (dot(cross(adb, ab), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {b, a};
						newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
					} else if (dot(ad, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {d, a};
						newDirection = cross(cross(ad, a0), ad) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else if (dot(cross(ad, adb), a0) >= -collisionDistanceErrorToleranceSquaredSquared) {
					if (dot(ad, a0) >= -collisionDistanceErrorToleranceSquared) {
						simplex = {d, a};
						newDirection = cross(cross(ad, a0), ad) / Volume::UNIT;
					} else {
						simplex = {a};
						newDirection = a0 / Length1D::UNIT;
					}
				} else {
					simplex = {b, d, a};
					newDirection = adb / Area::UNIT;
				}
				break;
			case AB:
				if (dot(ab, a0) >= -collisionDistanceErrorToleranceSquared) {
					simplex = {b, a};
					newDirection = cross(cross(ab, a0), ab) / Volume::UNIT;
				} else {
					simplex = {a};
					newDirection = a0 / Length1D::UNIT;
				}
				break;
			case AD:
				if (dot(ad, a0) >= -collisionDistanceErrorToleranceSquared) {
					simplex = {d, a};
					newDirection = cross(cross(ad, a0), ad) / Volume::UNIT;
				} else {
					simplex = {a};
					newDirection = a0 / Length1D::UNIT;
				}
				break;
			case A:
				simplex = {a};
				newDirection = a0 / Length1D::UNIT;
				break;
		}
		return newDirection;
	}
};

} // namespace grem::physics

#endif
