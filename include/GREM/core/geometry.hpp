// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_GEOMETRY_HPP
#define GREM_CORE_GEOMETRY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem {

/**
 * Generic point in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
using Point = vec<N, T>;

/**
 * Generic length in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
using Length = vec<N, T>;

/**
 * Generic ray with an origin and direction in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Ray {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	/**
	 * Starting position of the ray.
	 */
	Point<N, T> origin;

	/**
	 * Unit vector pointing in the direction of the ray.
	 *
	 * \warning Must be a unit vector.
	 */
	vec<N, T> direction;

	/**
	 * Maximum hit distance of the ray.
	 *
	 * \warning Must be non-negative.
	 */
	T maxDistance = Limits<T>::MAX;

	/**
	 * Component-wise reciprocal of the ray direction.
	 *
	 * \warning Must be equal to the reciprocal of #direction.
	 */
	vec<N, T> directionInverse = vec<N, T>{T{1}} / direction;
};

/**
 * Result of a raycast miss.
 */
struct RayMiss {};

/**
 * Result of a raycast hit.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct RayHit {
	vec<N, T> localOffset; ///< Offset from the object's center of mass in shape-local space at which the hit occured.
	T distance;            ///< Distance from the ray origin, along the ray direction, at which the hit occured.
	vec<N, T> normal;      ///< Unit normal vector of the surface that was hit.
};

/**
 * Result of an interor raycast hit.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct RayHitInterior {
	vec<N, T> localOffset; ///< Offset from the object's center of mass in shape-local space at which the hit occured.
};

/**
 * Result of a raycast.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct RaycastResult : Variant<RayMiss, RayHit<N, T>, RayHitInterior<N, T>> {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	using Variant<RayMiss, RayHit<N, T>, RayHitInterior<N, T>>::Variant;
};

/**
 * Generic axis-aligned box shape with minimum and maximum extents in
 * N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Box {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<N, T> min; ///< Position with the minimum coordinates of the box extents on each coordinate axis.
	Point<N, T> max; ///< Position with the maximum coordinates of the box extents on each coordinate axis.

	/**
	 * Compare this box to another for equality.
	 *
	 * \param other the box to compare this box to.
	 *
	 * \return true if the boxes are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Box& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this box.
	 *
	 * \param point point to check.
	 *
	 * \return true if the box contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<N, T>& point) const noexcept {
		return all(greaterThanEqual(point, min) & lessThan(point, max));
	}

	/**
	 * Check if this box intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<N, T>& ray) const noexcept {
		const vec<N, T> minIntersection = (min - ray.origin) * ray.directionInverse;
		const vec<N, T> maxIntersection = (max - ray.origin) * ray.directionInverse;
		const vec<N, T> tEnter = grem::min(minIntersection, maxIntersection);
		const vec<N, T> tLeave = grem::max(maxIntersection, minIntersection);
		T t{};
		T minLeave{};
		if constexpr (N == 2) {
			t = grem::max(tEnter.x, grem::max(tEnter.y, T{}));
			minLeave = grem::min(tLeave.x, grem::min(tLeave.y, ray.maxDistance));
		} else {
			t = grem::max(tEnter.x, grem::max(tEnter.y, grem::max(tEnter.z, T{})));
			minLeave = grem::min(tLeave.x, grem::min(tLeave.y, grem::min(tLeave.z, ray.maxDistance)));
		}
		return t <= minLeave;
	}

	/**
	 * Find the closest intersection of this box with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<N, T> raycast(const Ray<N, T>& ray) const noexcept {
		const vec<N, T> minIntersection = (min - ray.origin) * ray.directionInverse;
		const vec<N, T> maxIntersection = (max - ray.origin) * ray.directionInverse;
		const vec<N, T> tEnter = grem::min(minIntersection, maxIntersection);
		const vec<N, T> tLeave = grem::max(maxIntersection, minIntersection);
		T t{};
		T minLeave{};
		if constexpr (N == 2) {
			t = grem::max(tEnter.x, grem::max(tEnter.y, T{}));
			minLeave = grem::min(tLeave.x, grem::min(tLeave.y, ray.maxDistance));
		} else {
			t = grem::max(tEnter.x, grem::max(tEnter.y, grem::max(tEnter.z, T{})));
			minLeave = grem::min(tLeave.x, grem::min(tLeave.y, grem::min(tLeave.z, ray.maxDistance)));
		}
		if (t > minLeave) {
			return RayMiss{};
		}
		if (contains(ray.origin)) {
			return RayHitInterior<N, T>{.localOffset = ray.origin};
		}
		const vec<N, T> point = ray.origin + t * ray.direction;
		return RayHit<N, T>{
			.localOffset = point,
			.distance = t,
			.normal = normalize(trunc((point - midpoint(min, max)) / ((max - min) * static_cast<T>(0.49999)))),
		};
	}

	/**
	 * Get the axis-aligned bounding box of the box.
	 *
	 * \return an axis-aligned box that contains the entire box.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return *this;
	}

	/**
	 * Convert this shape to an equivalent axis-aligned bounding box.
	 *
	 * \return an axis-aligned box that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Box<N, T> toBox() const noexcept {
		return *this;
	}

	/**
	 * Get a version of the box that is symmetrically expanded by a certain
	 * amount.
	 *
	 * \param expansion distance to expand the box by along each axis. Each
	 *        component must be non-negative.
	 *
	 * \return the expanded box.
	 */
	[[nodiscard]] constexpr Box<N, T> getExpanded(Length<N, T> expansion) const noexcept {
		GREM_ASSERT(all(greaterThanEqual(expansion, Length<N, T>{})));
		return {.min = min - expansion, .max = max + expansion};
	}

	/**
	 * Get a version of the box that is symmetrically expanded by a certain
	 * amount.
	 *
	 * \param expansion distance to expand the box by along each axis. Must be
	 *        non-negative.
	 *
	 * \return the expanded box.
	 */
	[[nodiscard]] constexpr Box<N, T> getExpanded(T expansion) const noexcept {
		return getExpanded(Length<N, T>{expansion});
	}
};

/**
 * Flat 2D axis-aligned rectangle shape with a position and size.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Rectangle {
	static constexpr size_t RANK = 2; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<2, T> position; ///< Position of the bottom left corner of the rectangle.
	Length<2, T> size;    ///< Width and height of the rectangle.

	/**
	 * Convert this rectangle to an equivalent 2D axis-aligned box.
	 *
	 * \return the rectangle as an axis-aligned box.
	 */
	constexpr operator Box<2, T>() const noexcept {
		return toBox();
	}

	/**
	 * Compare this rectangle to another for equality.
	 *
	 * \param other the rectangle to compare this rectangle to.
	 *
	 * \return true if the rectangles are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Rectangle& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this rectangle.
	 *
	 * \param point point to check.
	 *
	 * \return true if the rectangle contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<2, T>& point) const noexcept {
		return toBox().contains(point);
	}

	/**
	 * Check if this rectangle intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<2, T>& ray) const noexcept {
		return toBox().intersects(ray);
	}

	/**
	 * Find the closest intersection of this rectangle with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<2, T> raycast(const Ray<2, T>& ray) const noexcept {
		return toBox().raycast(ray);
	}

	/**
	 * Get the axis-aligned bounding box of the rectangle.
	 *
	 * \return an axis-aligned box that contains the entire rectangle.
	 */
	[[nodiscard]] constexpr Box<2, T> getBoundingBox() const noexcept {
		return toBox();
	}

	/**
	 * Convert this shape to an equivalent axis-aligned bounding box.
	 *
	 * \return an axis-aligned box that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Box<2, T> toBox() const noexcept {
		return Box<2, T>{.min = position, .max = position + size};
	}
};

/**
 * Flat 2D axis-aligned square shape with a position and width.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Square {
	static constexpr size_t RANK = 2; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<2, T> position; ///< Position of the bottom left corner of the square.
	T width;              ///< Width and height of the square.

	/**
	 * Convert this square to an equivalent rectangle.
	 *
	 * \return the square as a rectangle.
	 */
	constexpr operator Rectangle<T>() const noexcept {
		return Rectangle<T>{.position = position, .size{width}};
	}

	/**
	 * Convert this square to an equivalent 2D axis-aligned box.
	 *
	 * \return the square as an axis-aligned box.
	 */
	constexpr operator Box<2, T>() const noexcept {
		return toBox();
	}

	/**
	 * Compare this square to another for equality.
	 *
	 * \param other the square to compare this square to.
	 *
	 * \return true if the squares are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Square& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this square.
	 *
	 * \param point point to check.
	 *
	 * \return true if the square contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<2, T>& point) const noexcept {
		return toBox().contains(point);
	}

	/**
	 * Check if this square intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<2, T>& ray) const noexcept {
		return toBox().intersects(ray);
	}

	/**
	 * Find the closest intersection of this square with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<2, T> raycast(const Ray<2, T>& ray) const noexcept {
		return toBox().raycast(ray);
	}

	/**
	 * Get the axis-aligned bounding box of the square.
	 *
	 * \return an axis-aligned box that contains the entire square.
	 */
	[[nodiscard]] constexpr Box<2, T> getBoundingBox() const noexcept {
		return toBox();
	}

	/**
	 * Convert this shape to an equivalent axis-aligned bounding box.
	 *
	 * \return an axis-aligned box that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Box<2, T> toBox() const noexcept {
		return Box<2, T>{.min = position, .max = position + Length<2, T>{width}};
	}
};

/**
 * 3D axis-aligned cube shape with a position and width.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Cube {
	static constexpr size_t RANK = 3; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<3, T> position; ///< Position of the far bottom left corner of the cube.
	T width;              ///< Width, height and depth of the cube.

	/**
	 * Convert this cube to an equivalent 2D axis-aligned box.
	 *
	 * \return the cube as an axis-aligned box.
	 */
	constexpr operator Box<3, T>() const noexcept {
		return toBox();
	}

	/**
	 * Compare this cube to another for equality.
	 *
	 * \param other the cube to compare this cube to.
	 *
	 * \return true if the cubes are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Cube& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this cube.
	 *
	 * \param point point to check.
	 *
	 * \return true if the cube contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<3, T>& point) const noexcept {
		return toBox().contains(point);
	}

	/**
	 * Check if this cube intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<3, T>& ray) const noexcept {
		return toBox().intersects(ray);
	}

	/**
	 * Find the closest intersection of this cube with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<3, T> raycast(const Ray<3, T>& ray) const noexcept {
		return toBox().raycast(ray);
	}

	/**
	 * Get the axis-aligned bounding box of the cube.
	 *
	 * \return an axis-aligned box that contains the entire cube.
	 */
	[[nodiscard]] constexpr Box<3, T> getBoundingBox() const noexcept {
		return toBox();
	}

	/**
	 * Convert this shape to an equivalent axis-aligned bounding box.
	 *
	 * \return an axis-aligned box that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Box<3, T> toBox() const noexcept {
		return Box<3, T>{.min = position, .max = position + Length<3, T>{width}};
	}
};

/**
 * Generic triangle shape in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Triangle {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<N, T> pointA; ///< First point of the triangle.
	Point<N, T> pointB; ///< Second point of the triangle.
	Point<N, T> pointC; ///< Third point of the triangle.

	/**
	 * Compare this triangle to another for equality.
	 *
	 * \param other the triangle to compare this triangle to.
	 *
	 * \return true if the triangles are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Triangle& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this triangle.
	 *
	 * \param point point to check.
	 *
	 * \return true if the triangle contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<N, T>& point) const noexcept requires(N == 2) {
		return (pointA.x - pointB.x) * (point.y - pointA.y) - (pointA.y - pointB.y) * (point.x - pointA.x) >= T{} && //
		       (pointB.x - pointC.x) * (point.y - pointB.y) - (pointB.y - pointC.y) * (point.x - pointB.x) >= T{} && //
		       (pointC.x - pointA.x) * (point.y - pointC.y) - (pointC.y - pointA.y) * (point.x - pointC.x) >= T{};
	}

	/**
	 * Check if this triangle intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<N, T>& ray) const noexcept {
		return !raycast(ray).template is<RayMiss>();
	}

	/**
	 * Find the closest intersection of this triangle with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<N, T> raycast(const Ray<N, T>& ray) const noexcept {
		if constexpr (N == 2) {
			// Treat as a convex polygon with 3 sides.

			const Length<N, T> ab = pointB - pointA;
			const Length<N, T> bc = pointC - pointB;
			const Length<N, T> ca = pointA - pointC;
			const Array<Point<N, T>, 3> facePoints{pointA, pointB, pointC};
			const Array<vec<N, T>, 3> faceNormals{
				vec<N, T>{ab.y, -ab.x},
				vec<N, T>{bc.y, -bc.x},
				vec<N, T>{ca.y, -ca.x},
			};

			T farthestFrontFacingDistance = Limits<T>::MIN;
			T closestBackFacingDistance = Limits<T>::MAX;
			size_t farthestFrontFacingFaceIndex = 0;
			for (size_t faceIndex = 0; faceIndex < 3; ++faceIndex) {
				const Point<N, T> facePoint = facePoints[faceIndex];
				const vec<N, T> faceNormal = faceNormals[faceIndex];

				const T faceNormalAlongRay = dot(faceNormal, ray.direction);
				const T rayOffsetFromFaceAlongFaceNormal = dot(ray.origin - facePoint, faceNormal);

				const bool rayIsParallelWithFacePlane = abs(faceNormalAlongRay) < Limits<T>::MACHINE_EPSILON;
				if (rayIsParallelWithFacePlane) {
					const bool rayIsInFrontOfFacePlane = rayOffsetFromFaceAlongFaceNormal > T{};
					if (rayIsInFrontOfFacePlane) {
						// The ray is in front of a face and parallel with it.
						// This means we have missed the entire shape, since it's convex.
						return RayMiss{};
					}

					// We can't hit a parallel face. Ignore it.
					continue;
				}

				// Compute signed hit distance.
				const T t = -rayOffsetFromFaceAlongFaceNormal / faceNormalAlongRay;

				const bool faceIsFrontFacing = faceNormalAlongRay < T{};
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

			if (farthestFrontFacingDistance >= T{}) {
				// We hit a front face.
				if (farthestFrontFacingDistance > ray.maxDistance) {
					return RayMiss{};
				}
				return RayHit<N, T>{
					.localOffset = ray.origin + farthestFrontFacingDistance * ray.direction,
					.distance = farthestFrontFacingDistance,
					.normal = tryNormalize(faceNormals[farthestFrontFacingFaceIndex]).value_or(-ray.direction),
				};
			}

			if (closestBackFacingDistance >= T{}) {
				// We hit a back face from inside the shape.
				return RayHitInterior<N, T>{.localOffset = ray.origin};
			}

			return RayMiss{};
		} else {
			const Length<N, T> ab = pointB - pointA;
			const Length<N, T> ac = pointC - pointA;
			const Length<N, T> ao = ray.origin - pointA;
			const vec<N, T> n = cross(ab, ac);
			const vec<N, T> q = cross(ao, ray.direction);
			const T nAlongRay = dot(n, ray.direction);
			if (abs(nAlongRay) <= Limits<T>::MACHINE_EPSILON) {
				return RayMiss{};
			}
			const T d = T{1} / nAlongRay;
			const T u = d * dot(-q, ac);
			const T v = d * dot(q, ab);
			if (u < T{} || v < T{} || u + v > T{1}) {
				return RayMiss{};
			}
			const T t = d * dot(-n, ao);
			if (t < T{} || t > ray.maxDistance) {
				return RayMiss{};
			}
			return RayHit<N, T>{
				.localOffset = ray.origin + t * ray.direction,
				.distance = t,
				.normal = tryNormalize(flipSignIf(n, !signbit(nAlongRay))).value_or(-ray.direction),
			};
		}
	}

	/**
	 * Get the axis-aligned bounding box of the triangle.
	 *
	 * \return an axis-aligned box that contains the entire triangle.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return {.min = min(min(pointA, pointB), pointC), .max = max(max(pointA, pointB), pointC)};
	}
};

/**
 * Generic line segment between two points in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct LineSegment {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<N, T> pointA; ///< Position of the first point of the line segment.
	Point<N, T> pointB; ///< Position of the second point of the line segment.

	/**
	 * Compare this line segment to another for equality.
	 *
	 * \param other the line segment to compare this line segment to.
	 *
	 * \return true if the line segments are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const LineSegment& other) const = default;

	/**
	 * Get the axis-aligned bounding box of the line segment.
	 *
	 * \return an axis-aligned box that contains the entire line segment.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return {
			.min = min(pointA, pointB),
			.max = max(pointA, pointB),
		};
	}
};

/**
 * Generic ellipsoid shape with a center and axis-aligned radii in N-dimensional
 * space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Ellipsoid {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<N, T> center; ///< Position of the center of the ellipsoid.
	Length<N, T> radii; ///< Radii of the ellipsoid along each axis.

	/**
	 * Compare this ellipsoid to another for equality.
	 *
	 * \param other the ellipsoid to compare this ellipsoid to.
	 *
	 * \return true if the ellipsoids are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Ellipsoid& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this ellipsoid.
	 *
	 * \param point point to check.
	 *
	 * \return true if the ellipsoid contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<N, T>& point) const noexcept {
		return length2((point - center) / radii) < T{1};
	}

	/**
	 * Check if this ellipsoid intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<N, T>& ray) const noexcept {
		const Length<N, T> localRayOrigin = ray.origin - center;
		const vec<N, T> offsetPerRadii = localRayOrigin / radii;
		const T offsetPerRadiiSquared = length2(offsetPerRadii);
		if (offsetPerRadiiSquared < T{1}) {
			return true;
		}
		const vec<N, T> directionPerRadii = ray.direction / radii;
		const T directionPerRadiiSquared = length2(directionPerRadii);
		const T offsetAlongDirectionPerRadii = dot(offsetPerRadii, directionPerRadii);
		const T discriminant = length2(offsetAlongDirectionPerRadii) - directionPerRadiiSquared * (offsetPerRadiiSquared - T{1});
		if (discriminant < T{}) {
			return false;
		}
		const T t = (-offsetAlongDirectionPerRadii - sqrt(discriminant)) / directionPerRadiiSquared;
		return t >= T{} && t <= ray.maxDistance;
	}

	/**
	 * Find the closest intersection of this ellipsoid with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<N, T> raycast(const Ray<N, T>& ray) const noexcept {
		const Length<N, T> localRayOrigin = ray.origin - center;
		const vec<N, T> offsetPerRadii = localRayOrigin / radii;
		const T offsetPerRadiiSquared = length2(offsetPerRadii);
		if (offsetPerRadiiSquared < T{1}) {
			return RayHitInterior<N, T>{.localOffset = ray.origin};
		}
		const vec<N, T> directionPerRadii = ray.direction / radii;
		const T directionPerRadiiSquared = length2(directionPerRadii);
		const T offsetAlongDirectionPerRadii = dot(offsetPerRadii, directionPerRadii);
		const T discriminant = length2(offsetAlongDirectionPerRadii) - directionPerRadiiSquared * (offsetPerRadiiSquared - T{1});
		if (discriminant < T{}) {
			return RayMiss{};
		}
		const T t = (-offsetAlongDirectionPerRadii - sqrt(discriminant)) / directionPerRadiiSquared;
		if (t < T{} || t > ray.maxDistance) {
			return RayMiss{};
		}
		const Point<N, T> point = ray.origin + t * ray.direction;
		return RayHit<N, T>{
			.localOffset = point,
			.distance = t,
			.normal = tryNormalize(point - center).value_or(-ray.direction),
		};
	}

	/**
	 * Get the axis-aligned bounding box of the sphere.
	 *
	 * \return an axis-aligned box that contains the entire sphere.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return {
			.min = center - radii,
			.max = center + radii,
		};
	}

	/**
	 * Convert this shape to an equivalent ellipsoid.
	 *
	 * \return an ellipsoid that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Ellipsoid<N, T> toEllipsoid() const noexcept {
		return *this;
	}
};

/**
 * Flat 2D ellipse shape with a center and axis-aligned radii.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Ellipse {
	static constexpr size_t RANK = 2; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<2, T> center; ///< Position of the center of the ellipse.
	Length<2, T> radii; ///< Radii of the ellipse along each axis.

	/**
	 * Convert this ellipse to an equivalent 2D ellipsoid.
	 *
	 * \return the ellipse as an ellipsoid.
	 */
	constexpr operator Ellipsoid<2, T>() const noexcept {
		return toEllipsoid();
	}

	/**
	 * Compare this ellipse to another for equality.
	 *
	 * \param other the ellipse to compare this ellipse to.
	 *
	 * \return true if the ellipses are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Ellipse& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this ellipse.
	 *
	 * \param point point to check.
	 *
	 * \return true if the ellipse contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<2, T>& point) const noexcept {
		return toEllipsoid().contains(point);
	}

	/**
	 * Get the axis-aligned bounding box of the ellipse.
	 *
	 * \return an axis-aligned box that contains the entire ellipse.
	 */
	[[nodiscard]] constexpr Box<2, T> getBoundingBox() const noexcept {
		return {
			.min = center - radii,
			.max = center + radii,
		};
	}

	/**
	 * Convert this shape to an equivalent ellipsoid.
	 *
	 * \return an ellipsoid that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Ellipsoid<2, T> toEllipsoid() const noexcept {
		return Ellipsoid<2, T>{.center = center, .radii = radii};
	}
};

/**
 * Generic sphere shape with a center and radius in N-dimensional space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Sphere {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<N, T> center; ///< Position of the center of the sphere.
	T radius;           ///< Radius of the sphere.

	/**
	 * Convert this sphere to an equivalent ellipsoid.
	 *
	 * \return the sphere as an ellipsoid.
	 */
	constexpr operator Ellipsoid<N, T>() const noexcept {
		return toEllipsoid();
	}

	/**
	 * Convert this sphere to an equivalent 2D ellipse.
	 *
	 * \return the sphere as an ellipse.
	 */
	constexpr operator Ellipse<T>() const noexcept requires(N == 2) {
		return Ellipse<T>{.center = center, .radii{radius}};
	}

	/**
	 * Compare this sphere to another for equality.
	 *
	 * \param other the sphere to compare this sphere to.
	 *
	 * \return true if the spheres are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Sphere& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this sphere.
	 *
	 * \param point point to check.
	 *
	 * \return true if the sphere contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<N, T>& point) const noexcept {
		return distance2(center, point) < length2(radius);
	}

	/**
	 * Check if this sphere intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] constexpr bool intersects(const Ray<N, T>& ray) const noexcept {
		const Length<N, T> difference = ray.origin - center;
		const T differenceAlongRay = dot(difference, ray.direction);
		const T differenceOfSquares = length2(difference) - length2(radius);
		if (differenceOfSquares < T{}) {
			return true;
		}
		if (differenceAlongRay > T{}) {
			return false;
		}
		const T discriminant = length2(differenceAlongRay) - differenceOfSquares;
		if (discriminant < T{}) {
			return false;
		}
		const T t = -differenceAlongRay - sqrt(discriminant);
		return t >= T{} && t <= ray.maxDistance;
	}

	/**
	 * Find the closest intersection of this sphere with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] constexpr RaycastResult<N, T> raycast(const Ray<N, T>& ray) const noexcept {
		const Length<N, T> difference = ray.origin - center;
		const T differenceAlongRay = dot(difference, ray.direction);
		const T differenceOfSquares = length2(difference) - length2(radius);
		if (differenceOfSquares < T{}) {
			return RayHitInterior<N, T>{.localOffset = ray.origin};
		}
		if (differenceAlongRay > T{}) {
			return RayMiss{};
		}
		const T discriminant = length2(differenceAlongRay) - differenceOfSquares;
		if (discriminant < T{}) {
			return RayMiss{};
		}
		const T t = -differenceAlongRay - sqrt(discriminant);
		if (t < T{} || t > ray.maxDistance) {
			return RayMiss{};
		}
		const Point<N, T> point = ray.origin + t * ray.direction;
		return RayHit<N, T>{
			.localOffset = point,
			.distance = t,
			.normal = tryNormalize(point - center).value_or(-ray.direction),
		};
	}

	/**
	 * Get the axis-aligned bounding box of the sphere.
	 *
	 * \return an axis-aligned box that contains the entire sphere.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return {
			.min = center - Length<N, T>{radius},
			.max = center + Length<N, T>{radius},
		};
	}

	/**
	 * Convert this shape to an equivalent ellipsoid.
	 *
	 * \return an ellipsoid that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Ellipsoid<N, T> toEllipsoid() const noexcept {
		return Ellipsoid<N, T>{.center = center, .radii{radius}};
	}

	/**
	 * Convert this shape to an equivalent sphere.
	 *
	 * \return a sphere that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Sphere<N, T> toSphere() const noexcept {
		return *this;
	}
};

/**
 * Flat 2D circle shape with a center and radius.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Circle {
	static constexpr size_t RANK = 2; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	Point<2, T> center; ///< Position of the center of the circle.
	T radius;           ///< Radius of the circle.

	/**
	 * Convert this circle to an equivalent 2D ellipsoid.
	 *
	 * \return the circle as an ellipsoid.
	 */
	constexpr operator Ellipsoid<2, T>() const noexcept {
		return toEllipsoid();
	}

	/**
	 * Convert this circle to an equivalent 2D ellipse.
	 *
	 * \return the circle as an ellipse.
	 */
	constexpr operator Ellipse<T>() const noexcept {
		return Ellipse<T>{.center = center, .radii{radius}};
	}

	/**
	 * Convert this circle to an equivalent 2D sphere.
	 *
	 * \return the circle as a sphere.
	 */
	constexpr operator Sphere<2, T>() const noexcept {
		return toSphere();
	}

	/**
	 * Compare this circle to another for equality.
	 *
	 * \param other the circle to compare this circle to.
	 *
	 * \return true if the circles are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Circle& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this circle.
	 *
	 * \param point point to check.
	 *
	 * \return true if the circle contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<2, T>& point) const noexcept {
		return static_cast<Sphere<2, T>>(*this).contains(point);
	}

	/**
	 * Get the axis-aligned bounding box of the circle.
	 *
	 * \return an axis-aligned box that contains the entire circle.
	 */
	[[nodiscard]] constexpr Box<2, T> getBoundingBox() const noexcept {
		return {
			.min = center - Length<2, T>{radius},
			.max = center + Length<2, T>{radius},
		};
	}

	/**
	 * Convert this shape to an equivalent ellipsoid.
	 *
	 * \return an ellipsoid that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Ellipsoid<2, T> toEllipsoid() const noexcept {
		return Ellipsoid<2, T>{.center = center, .radii{radius}};
	}

	/**
	 * Convert this shape to an equivalent sphere.
	 *
	 * \return a sphere that is equivalent to this shape.
	 */
	[[nodiscard]] constexpr Sphere<2, T> toSphere() const noexcept {
		return Sphere<2, T>{.center = center, .radius = radius};
	}
};

/**
 * Generic capsule shape with a center line segment and radius in N-dimensional
 * space.
 *
 * \tparam N number of vector dimensions.
 * \tparam T scalar coordinate component type.
 */
template <size_t N, typename T>
struct Capsule {
	static constexpr size_t RANK = N; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	LineSegment<N, T> centerLine; ///< Center line of the capsule.
	T radius;                     ///< Radius of the capsule from the center line.

	/**
	 * Compare this capsule to another for equality.
	 *
	 * \param other the capsule to compare this capsule to.
	 *
	 * \return true if the capsules are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Capsule& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this capsule.
	 *
	 * \param point point to check.
	 *
	 * \return true if the capsule contains the given point, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(const Point<N, T>& point) const noexcept;

	/**
	 * Get the axis-aligned bounding box of the capsule.
	 *
	 * \return an axis-aligned box that contains the entire capsule.
	 */
	[[nodiscard]] constexpr Box<N, T> getBoundingBox() const noexcept {
		return {
			.min = min(centerLine.pointA, centerLine.pointB) - Length<N, T>{radius},
			.max = max(centerLine.pointA, centerLine.pointB) + Length<N, T>{radius},
		};
	}
};

/**
 * Truncated pyramid shape in 3D space.
 *
 * \tparam T scalar coordinate component type.
 */
template <typename T>
struct Frustum {
	static constexpr size_t RANK = 3; ///< Number of dimensions of the vector space.
	using Component = T;              ///< Scalar coordinate component type.

	/**
	 * Create a view frustum from a combined view-projection matrix.
	 *
	 * \param viewProjectionMatrix combined view-projection matrix to create the
	 *        frustum from.
	 */
	static constexpr Frustum fromViewProjectionMatrix(const mat<4, 4, T>& viewProjectionMatrix) noexcept {
		constexpr mat4 CONVERT_DEPTH_REVERSE_RANGE{
			// clang-format off
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f,-1.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 1.0f,
			// clang-format on
		};

		constexpr mat4 CONVERT_DEPTH_FROM_ZO_TO_NO{
			// clang-format off
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 2.0f, 0.0f,
			0.0f, 0.0f,-1.0f, 1.0f,
			// clang-format on
		};

		constexpr mat4 DEPTH_CONVERSION_MATRIX = CONVERT_DEPTH_FROM_ZO_TO_NO * CONVERT_DEPTH_REVERSE_RANGE;

		const mat<4, 4, T> transposedViewProjectionMatrix = transpose(DEPTH_CONVERSION_MATRIX * viewProjectionMatrix);

		const vec<4, T> left = transposedViewProjectionMatrix[3] + transposedViewProjectionMatrix[0];
		const vec<4, T> right = transposedViewProjectionMatrix[3] - transposedViewProjectionMatrix[0];
		const vec<4, T> bottom = transposedViewProjectionMatrix[3] + transposedViewProjectionMatrix[1];
		const vec<4, T> top = transposedViewProjectionMatrix[3] - transposedViewProjectionMatrix[1];
		const vec<4, T> farPlane = transposedViewProjectionMatrix[3] + transposedViewProjectionMatrix[2];
		const vec<4, T> nearPlane = transposedViewProjectionMatrix[3] - transposedViewProjectionMatrix[2];

		const Array planeNormalCrossProducts{
			cross(vec<3, T>{left}, vec<3, T>{right}),
			cross(vec<3, T>{left}, vec<3, T>{bottom}),
			cross(vec<3, T>{left}, vec<3, T>{top}),
			cross(vec<3, T>{left}, vec<3, T>{farPlane}),
			cross(vec<3, T>{left}, vec<3, T>{nearPlane}),
			cross(vec<3, T>{right}, vec<3, T>{bottom}),
			cross(vec<3, T>{right}, vec<3, T>{top}),
			cross(vec<3, T>{right}, vec<3, T>{farPlane}),
			cross(vec<3, T>{right}, vec<3, T>{nearPlane}),
			cross(vec<3, T>{bottom}, vec<3, T>{top}),
			cross(vec<3, T>{bottom}, vec<3, T>{farPlane}),
			cross(vec<3, T>{bottom}, vec<3, T>{nearPlane}),
			cross(vec<3, T>{top}, vec<3, T>{farPlane}),
			cross(vec<3, T>{top}, vec<3, T>{nearPlane}),
			cross(vec<3, T>{farPlane}, vec<3, T>{nearPlane}),
		};

		const Array planes{left, right, bottom, top, farPlane, nearPlane};

		const Array corners{
			getCorner<0, 2, 4>(planes, planeNormalCrossProducts),
			getCorner<0, 3, 4>(planes, planeNormalCrossProducts),
			getCorner<1, 2, 4>(planes, planeNormalCrossProducts),
			getCorner<1, 3, 4>(planes, planeNormalCrossProducts),
			getCorner<0, 2, 5>(planes, planeNormalCrossProducts),
			getCorner<0, 3, 5>(planes, planeNormalCrossProducts),
			getCorner<1, 2, 5>(planes, planeNormalCrossProducts),
			getCorner<1, 3, 5>(planes, planeNormalCrossProducts),
		};

		return Frustum{.planes = planes, .corners = corners};
	}

	/**
	 * Plane equation coefficients of the 6 planes of the frustum in the
	 * following order:
	 * - [0]: Left
	 * - [1]: Right
	 * - [2]: Bottom
	 * - [3]: Top
	 * - [4]: Far
	 * - [5]: Near
	 */
	Array<vec<4, T>, 6> planes;

	/**
	 * Positions of the 8 corners of the frustum, in the following order:
	 * - [0]: Far bottom left
	 * - [1]: Far top left
	 * - [2]: Far bottom right
	 * - [3]: Far top right
	 * - [4]: Near bottom left
	 * - [5]: Near top left
	 * - [6]: Near bottom right
	 * - [7]: Near top right
	 */
	Array<Point<3, T>, 8> corners;

	/**
	 * Check if an axis-aligned bounding box is potentially intersecting with
	 * the frustum.
	 *
	 * \param aabb axis-aligned bounding box to check.
	 *
	 * \return true if the box may potentially be intersecting with the frustum,
	 *         false if it is definitely not intersecting.
	 */
	[[nodiscard]] constexpr bool isPotentiallyIntersecting(const Box<3, float>& aabb) const noexcept {
		const Array aabbCorners{
			Point<3, T>{aabb.min.x, aabb.min.y, aabb.min.z},
			Point<3, T>{aabb.max.x, aabb.min.y, aabb.min.z},
			Point<3, T>{aabb.min.x, aabb.max.y, aabb.min.z},
			Point<3, T>{aabb.max.x, aabb.max.y, aabb.min.z},
			Point<3, T>{aabb.min.x, aabb.min.y, aabb.max.z},
			Point<3, T>{aabb.max.x, aabb.min.y, aabb.max.z},
			Point<3, T>{aabb.min.x, aabb.max.y, aabb.max.z},
			Point<3, T>{aabb.max.x, aabb.max.y, aabb.max.z},
		};
		return noneOf(planes, [&](const vec<4, T>& plane) -> bool {
			return allOf(aabbCorners, [&](const Point<3, T>& aabbCorner) -> bool { return dot(aabbCorner, vec<3, T>{plane}) < -plane.w; });
		});
	}

	/**
	 * Check if a sphere is potentially intersecting with the frustum.
	 *
	 * \param sphere sphere to check.
	 *
	 * \return true if the sphere may potentially be intersecting with the
	 *         frustum, false if it is definitely not intersecting.
	 */
	[[nodiscard]] constexpr bool isPotentiallyIntersecting(const Sphere<3, float>& sphere) const noexcept {
		bool culled = false;
		for (const vec<4, T>& plane : planes) {
			culled |= dot(sphere.center, vec<3, T>{plane}) + sphere.radius < -plane.w;
		}
		return !culled;
	}

private:
	[[nodiscard]] static constexpr size_t getPlaneNormalCrossProductIndex(size_t planeIndexA, size_t planeIndexB) noexcept {
		return planeIndexA * (9 - planeIndexA) / 2 + planeIndexB - 1;
	}

	template <size_t PlaneIndexA, size_t PlaneIndexB, size_t PlaneIndexC>
	[[nodiscard]] static constexpr Point<3, T> getCorner(Span<const vec<4, T>, 6> planes, Span<const vec<3, T>, 15> planeNormalCrossProducts) noexcept {
		const vec<4, T>& a = planes[PlaneIndexA];
		const vec<4, T>& b = planes[PlaneIndexB];
		const vec<4, T>& c = planes[PlaneIndexC];
		constexpr size_t ab = getPlaneNormalCrossProductIndex(PlaneIndexA, PlaneIndexB);
		constexpr size_t ac = getPlaneNormalCrossProductIndex(PlaneIndexA, PlaneIndexC);
		constexpr size_t bc = getPlaneNormalCrossProductIndex(PlaneIndexB, PlaneIndexC);
		const vec<3, T> axb = planeNormalCrossProducts[ab];
		const vec<3, T> axc = planeNormalCrossProducts[ac];
		const vec<3, T> bxc = planeNormalCrossProducts[bc];
		return (mat<3, 3, T>{bxc, axc, axb} * vec<3, T>{a.w, -b.w, c.w}) * (-1.0f / dot(vec<3, T>{a}, bxc));
	}
};

/**
 * Concept that checks if a given type is a raycastable shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept raycastable_shape = requires(const T t, const Point<T::RANK, typename T::Component> point, const Ray<T::RANK, typename T::Component> ray) {
	{ t.contains(point) } -> convertible_to<bool>;
	{ t.intersects(ray) } -> convertible_to<bool>;
	{ t.raycast(ray) } -> convertible_to<RaycastResult<T::RANK, typename T::Component>>;
};

/**
 * Concept that checks if a given type is an axis-aligned box shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept axis_aligned_box_shape = requires(const T t) {
	{ t.toBox() } -> convertible_to<Box<T::RANK, typename T::Component>>;
};

/**
 * Concept that checks if a given type is an axis-aligned ellipsoid shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept axis_aligned_ellipsoid_shape = requires(const T t) {
	{ t.toEllipsoid() } -> convertible_to<Ellipsoid<T::RANK, typename T::Component>>;
};

/**
 * Concept that checks if a given type is a spherical shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept spherical_shape = requires(const T t) {
	{ t.toSphere() } -> convertible_to<Sphere<T::RANK, typename T::Component>>;
};

/**
 * Get the axis-aligned bounding box of an axis-aligned box with an affine
 * transformation applied to it.
 *
 * \param transformation affine transformation to apply to the box.
 * \param aabb box to transform and get the axis-aligned bounding box of.
 *
 * \return an axis-aligned box that contains the given box after applying the
 *         transformation.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr Box<N, T> getTransformedBoundingBox(const mat<N + 1, N + 1, T>& transformation, const Box<N, T>& aabb) noexcept requires(floating_point<T>) {
	const Length<N, T> localCenter = midpoint(aabb.min, aabb.max);
	const Point<N, T> center = Point<N, T>{transformation * Length<N + 1, T>{localCenter, T{1}}};
	const Length<N, T> halfExtents = localCenter - aabb.min;
	Box<N, T> result{.min = center, .max = center};
	for (size_t y = 0; y < N; ++y) {
		for (size_t x = 0; x < N; ++x) {
			const T a = transformation[y][x] * -halfExtents[y];
			const T b = transformation[y][x] * halfExtents[y];
			result.min[x] += min(a, b);
			result.max[x] += max(a, b);
		}
	}
	return result;
}

/**
 * Check if two spherical shapes intersect.
 *
 * \param a first sphere.
 * \param b second sphere.
 *
 * \return true if the first and second spheres are colliding with each other,
 *         false otherwise.
 */
[[nodiscard]] constexpr bool intersects(const spherical_shape auto& a, const spherical_shape auto& b) noexcept {
	const auto sphereA = a.toSphere();
	const auto sphereB = b.toSphere();
	return distance2(sphereA.center, sphereB.center) < length2(sphereA.radius + sphereB.radius);
}

/**
 * Check if two axis-aligned box shapes intersect.
 *
 * \param a first box.
 * \param b second box.
 *
 * \return true if the first and second boxes are colliding with each other,
 *         false otherwise.
 */
[[nodiscard]] constexpr bool intersects(const axis_aligned_box_shape auto& a, const axis_aligned_box_shape auto& b) noexcept {
	const auto boxA = a.toBox();
	const auto boxB = b.toBox();
	return all(lessThan(boxA.min, boxB.max) & greaterThan(boxA.max, boxB.min));
}

/**
 * Check if a spherical shape intersects an axis-aligned box shape.
 *
 * \param a sphere.
 * \param b box.
 *
 * \return true if the sphere and box are colliding with each other, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool intersects(const spherical_shape auto& a, const axis_aligned_box_shape auto& b) noexcept {
	const auto sphereA = a.toSphere();
	const auto boxB = b.toBox();
	return distance2(sphereA.center, clamp(sphereA.center, boxB.min, boxB.max)) < length2(sphereA.radius);
}

/**
 * Check if an axis-aligned box shape intersects a spherical shape.
 *
 * \param a box.
 * \param b sphere.
 *
 * \return true if the box and sphere are colliding with each other, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool intersects(const axis_aligned_box_shape auto& a, const spherical_shape auto& b) noexcept {
	return intersects(b, a);
}

/**
 * Check if a raycastable shape intersects a line segment.
 *
 * \param a raycastable shape.
 * \param b line segment.
 *
 * \return true if the shape and line segment are colliding with each other,
 *         false otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const raycastable_shape auto& a, const LineSegment<N, T>& b) noexcept {
	const vec<N, T> lineDifference = b.pointB - b.pointA;
	const T lineLength = length(lineDifference);
	if (lineLength <= Limits<T>::MACHINE_EPSILON) {
		return a.contains(b.pointA);
	}
	const Ray<N, T> ray{.origin = b.pointA, .direction = lineDifference * (T{1} / lineLength), .maxDistance = lineLength};
	return a.intersects(ray);
}

/**
 * Check if a line segment intersects a raycastable shape.
 *
 * \param a line segment.
 * \param b raycastable shape.
 *
 * \return true if the line segment and shape are colliding with each other,
 *         false otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const LineSegment<N, T>& a, const raycastable_shape auto& b) noexcept {
	return intersects(b, a);
}

/**
 * Check if a 2D axis-aligned box shape intersects a 2D capsule.
 *
 * \param a box.
 * \param b capsule.
 *
 * \return true if the box and capsule are colliding with each other, false
 *         otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool intersects(const axis_aligned_box_shape auto& a, const Capsule<2, T>& b) noexcept {
	const Box<2, T> boxA = a.toBox();
	const Circle<T> capsuleCircleA{.center = b.centerLine.pointA, .radius = b.radius};
	const Circle<T> capsuleCircleB{.center = b.centerLine.pointB, .radius = b.radius};
	if (intersects(boxA, capsuleCircleA) || intersects(boxA, capsuleCircleB)) {
		return true;
	}
	const vec<2, T> centerLineDifference = b.centerLine.pointB - b.centerLine.pointA;
	const float centerLineLength = length(centerLineDifference);
	if (centerLineLength <= Limits<T>::MACHINE_EPSILON) {
		return false;
	}
	const vec<2, T> centerLineDirection = centerLineDifference * (T{1} / centerLineLength);
	const vec<2, T> boxExpansion = b.radius * vec<2, T>{abs(centerLineDirection.y), abs(centerLineDirection.x)};
	const Ray<2, T> ray{.origin = b.centerLine.pointA, .direction = centerLineDirection, .maxDistance = centerLineLength};
	return boxA.getExpanded(boxExpansion).intersects(ray);
}

/**
 * Check if a 2D capsule intersects a 2D axis-aligned box shape.
 *
 * \param a capsule.
 * \param b box.
 *
 * \return true if the capsule and box are colliding with each other, false
 *         otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool intersects(const Capsule<2, T>& a, const axis_aligned_box_shape auto& b) noexcept {
	return intersects(b, a);
}

/**
 * Check if a spherical shape intersects a capsule.
 *
 * \param a sphere.
 * \param b capsule.
 *
 * \return true if the sphere and capsule are colliding with each other, false
 *         otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const spherical_shape auto& a, const Capsule<N, T>& b) noexcept {
	const Sphere<N, T> sphereA = a.toSphere();
	const T combinedRadiusSquared = length2(sphereA.radius + b.radius);
	const vec<N, T> linePointAToPointB = b.centerLine.pointB - b.centerLine.pointA;
	const vec<N, T> linePointAToSphereCenter = sphereA.center - b.centerLine.pointA;
	const T linePointAToSphereCenterAlongLine = dot(linePointAToSphereCenter, linePointAToPointB);
	if (linePointAToSphereCenterAlongLine <= 0.0f) {
		return length2(linePointAToSphereCenter) < combinedRadiusSquared;
	}
	const vec<N, T> linePointBToSphereCenter = sphereA.center - b.centerLine.pointB;
	const T linePointBToSphereCenterAlongLine = dot(linePointBToSphereCenter, linePointAToPointB);
	if (linePointBToSphereCenterAlongLine >= 0.0f) {
		return length2(linePointBToSphereCenter) < combinedRadiusSquared;
	}
	const vec<N, T> lineToSphereCenterOrthogonal = linePointAToSphereCenter - linePointAToPointB * (linePointAToSphereCenterAlongLine / length2(linePointAToPointB));
	return length2(lineToSphereCenterOrthogonal) < combinedRadiusSquared;
}

/**
 * Check if a capsule intersects a spherical shape.
 *
 * \param a capsule.
 * \param b sphere.
 *
 * \return true if the capsule and sphere are colliding with each other, false
 *         otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const Capsule<N, T>& a, const spherical_shape auto& b) noexcept {
	return intersects(b, a);
}

/**
 * Check if a sphere intersects a line segment.
 *
 * \param a sphere.
 * \param b line segment.
 *
 * \return true if the sphere and line segment are colliding with each other,
 *         false otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const Sphere<N, T>& a, const LineSegment<N, T>& b) noexcept {
	return intersects(a, Capsule<N, T>{.centerLine = b, .radius = T{}});
}

/**
 * Check if a line segment intersects a sphere.
 *
 * \param a line segment.
 * \param b sphere.
 *
 * \return true if the line segment and sphere are colliding with each other,
 *         false otherwise.
 */
template <size_t N, typename T>
[[nodiscard]] constexpr bool intersects(const LineSegment<N, T>& a, const Sphere<N, T>& b) noexcept {
	return intersects(b, a);
}

/**
 * Check if a circle intersects a line segment.
 *
 * \param a circle.
 * \param b line segment.
 *
 * \return true if the circle and line segment are colliding with each other,
 *         false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool intersects(const Circle<T>& a, const LineSegment<2, T>& b) noexcept {
	return intersects(static_cast<Sphere<2, T>>(a), b);
}

/**
 * Check if a line segment intersects a circle.
 *
 * \param a line segment.
 * \param b circle.
 *
 * \return true if the line segment and circle are colliding with each other,
 *         false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool intersects(const LineSegment<2, T>& a, const Circle<T>& b) noexcept {
	return intersects(b, a);
}

template <size_t N, typename T>
constexpr bool Capsule<N, T>::contains(const Point<N, T>& point) const noexcept {
	return intersects(*this, Sphere<N, T>{.center = point, .radius = T{}});
}

} // namespace grem

#endif
