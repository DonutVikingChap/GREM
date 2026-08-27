// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_CONTACTS_HPP
#define GREM_EXAMPLES_TILES_CONTACTS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>

#include "Coordinate.hpp"

struct ContactPoint {
	Coordinates2D pointA;
	vec2 offsetB;
	vec2 normal;
	InplaceArrayList<Coordinates2D, 2> featureVerticesA;

	[[nodiscard]] constexpr float getPenetrationDepth(Coordinates2D positionB) const noexcept {
		const Coordinates2D pointB = positionB + offsetB;
		return dot(vec2{pointA - pointB}, normal);
	}
};

[[nodiscard]] inline ContactPoint findSmallestSeparation(Box<2, Coordinate> boxA, Coordinates2D positionB, float radiusB) {
	const Array<Coordinates2D, 4> boxCorners{
		boxA.max,
		Coordinates2D{boxA.min.x, boxA.max.y},
		Coordinates2D{boxA.max.x, boxA.min.y},
		boxA.min,
	};
	const Coordinates2D boxCenter = midpoint(boxA.min, boxA.max);
	const Coordinates2D offsetFromBoxCenter{positionB - boxCenter};
	const bvec2 offsetFromBoxCenterSigns = lessThan(offsetFromBoxCenter, Coordinates2D{0, 0});
	const Coordinates2D closestCorner = boxCorners[static_cast<size_t>(offsetFromBoxCenterSigns.y) * 2 + static_cast<size_t>(offsetFromBoxCenterSigns.x)];

	if (!any(greaterThanEqual(positionB, boxA.min) & lessThanEqual(positionB, boxA.max))) {
		// Corner.
		const vec2 normal = normalize(vec2{positionB - closestCorner});
		return {
			.pointA = closestCorner,
			.offsetB = normal * -radiusB,
			.normal = normal,
			.featureVerticesA{closestCorner},
		};
	}

	// Side.
	const size_t majorAxis = static_cast<size_t>(abs(offsetFromBoxCenter.y) > abs(offsetFromBoxCenter.x));
	const size_t minorAxis = 1 - majorAxis;
	Coordinates2D positionBProjectedOntoBoxSide;
	positionBProjectedOntoBoxSide[majorAxis] = closestCorner[majorAxis];
	positionBProjectedOntoBoxSide[minorAxis] = positionB[minorAxis];
	vec2 normal;
	normal[majorAxis] = 1.0f - 2.0f * static_cast<float>(offsetFromBoxCenterSigns[majorAxis]);
	normal[minorAxis] = 0.0f;
	bvec2 otherCornerSigns{};
	otherCornerSigns[majorAxis] = offsetFromBoxCenterSigns[majorAxis];
	otherCornerSigns[minorAxis] = !offsetFromBoxCenterSigns[minorAxis];
	const Coordinates2D otherCorner = boxCorners[static_cast<size_t>(otherCornerSigns.y) * 2 + static_cast<size_t>(otherCornerSigns.x)];
	return {
		.pointA = positionBProjectedOntoBoxSide,
		.offsetB = normal * -radiusB,
		.normal = normal,
		.featureVerticesA{closestCorner, otherCorner},
	};
}

struct Contacts {
	ArrayList<ContactPoint> contactPoints{};
	ArrayList<Coordinates2D> voidedVertices{};
};

#endif
