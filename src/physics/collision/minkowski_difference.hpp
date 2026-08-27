// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_MINKOWSKI_DIFFERENCE_HPP
#define GREM_PHYSICS_COLLISION_MINKOWSKI_DIFFERENCE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

template <size_t N>
struct MinkowskiVertex : Pair<Position<N>> {
	using Pair<Position<N>>::Pair;

	constexpr MinkowskiVertex(Pair<Position<N>> points) noexcept
		: Pair<Position<N>>(points) {}
};
using MinkowskiVertex2D = MinkowskiVertex<2>;
using MinkowskiVertex3D = MinkowskiVertex<3>;

template <size_t N>
using MinkowskiSimplex = InplaceBuffer<MinkowskiVertex<N>, N + 1>;
using MinkowskiSimplex2D = MinkowskiSimplex<2>;
using MinkowskiSimplex3D = MinkowskiSimplex<3>;

template <size_t N>
[[nodiscard]] Array<Scale1D, 2> getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(Length<N> a, Length<N> b) noexcept {
	const Length<N> ab = b - a;

	const SquaredDistance squaredDistanceAB = length2(ab);
	if (squaredDistanceAB < SquaredDistance::MACHINE_EPSILON) {
		if (length2(a) < length2(b)) {
			return {1.0f, 0.0f};
		}
		return {0.0f, 1.0f};
	}

	const Scale1D v = -dot(a, ab) / squaredDistanceAB;
	const Scale1D u = 1.0f - v;
	return {u, v};
}

template <size_t N>
[[nodiscard]] Array<Scale1D, 3> getBarycentricCoordinatesClosestToOriginOnMinkowskiPlane(Length<N> a, Length<N> b, Length<N> c) noexcept {
	const Length<N> ab = b - a;
	const Length<N> ac = c - a;
	const Length<N> bc = c - b;

	const SquaredDistance squaredDistanceAB = length2(ab);
	const SquaredDistance squaredDistanceAC = length2(ac);
	const SquaredDistance squaredDistanceBC = length2(bc);
	if (squaredDistanceAB <= squaredDistanceBC) {
		const Area abAlongAC = dot(ab, ac);
		const SquaredArea denominator = squaredDistanceAB * squaredDistanceAC - length2(abAlongAC);
		if (denominator < length2(SquaredDistance::MACHINE_EPSILON)) {
			if (squaredDistanceAB <= squaredDistanceAC) {
				const auto [u, w] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(a, c);
				return {u, 0.0f, w};
			}
			const auto [u, v] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(a, b);
			return {u, v, 0.0f};
		}
		const auto scale = 1.0f / denominator;
		const Area aAlongAB = dot(a, ab);
		const Area aAlongAC = dot(a, ac);
		const Scale1D v = (abAlongAC * aAlongAC - squaredDistanceAC * aAlongAB) * scale;
		const Scale1D w = (abAlongAC * aAlongAB - squaredDistanceAB * aAlongAC) * scale;
		const Scale1D u = 1.0f - v - w;
		return {u, v, w};
	}

	const Area bcAlongAC = dot(bc, ac);
	const SquaredArea denominator = squaredDistanceBC * squaredDistanceAC - length2(bcAlongAC);
	if (denominator < length2(SquaredDistance::MACHINE_EPSILON)) {
		if (squaredDistanceAC <= squaredDistanceBC) {
			const auto [v, w] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(b, c);
			return {0.0f, v, w};
		}
		const auto [u, w] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(a, c);
		return {u, 0.0f, w};
	}

	const auto scale = 1.0f / denominator;
	const Area cAlongAC = dot(c, ac);
	const Area cAlongBC = dot(c, bc);
	const Scale1D u = (squaredDistanceBC * cAlongAC - bcAlongAC * cAlongBC) * scale;
	const Scale1D v = (squaredDistanceAC * cAlongBC - bcAlongAC * cAlongAC) * scale;
	const Scale1D w = 1.0f - u - v;
	return {u, v, w};
}

template <size_t N>
[[nodiscard]] inline MinkowskiVertex<N> getPointClosestToOriginOnMinkowskiLine(const MinkowskiVertex<N>& a, const MinkowskiVertex<N>& b, auto projection) noexcept {
	const auto [u, v] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(projection(a), projection(b));
	return {
		((a.first - 0) * u + (b.first - 0) * v),
		((a.second - 0) * u + (b.second - 0) * v),
	};
}

[[nodiscard]] inline MinkowskiVertex3D getPointClosestToOriginOnMinkowskiPlane(const MinkowskiVertex3D& a, const MinkowskiVertex3D& b, const MinkowskiVertex3D& c,
	auto projection) noexcept {
	const auto [u, v, w] = getBarycentricCoordinatesClosestToOriginOnMinkowskiPlane(projection(a), projection(b), projection(c));
	return {
		((a.first - 0) * u + (b.first - 0) * v + (c.first - 0) * w),
		((a.second - 0) * u + (b.second - 0) * v + (c.second - 0) * w),
	};
}

template <size_t N>
[[nodiscard]] inline MinkowskiVertex<N> getPointClosestToOriginOnMinkowskiLineSegment(const MinkowskiVertex<N>& a, const MinkowskiVertex<N>& b, auto projection) noexcept {
	const auto [u, v] = getBarycentricCoordinatesClosestToOriginOnMinkowskiLine(projection(a), projection(b));
	if (v <= 0) {
		return a;
	}
	if (u <= 0) {
		return b;
	}
	return {
		((a.first - 0) * u + (b.first - 0) * v),
		((a.second - 0) * u + (b.second - 0) * v),
	};
}

[[nodiscard]] inline MinkowskiVertex3D getPointClosestToOriginOnMinkowskiTriangle(const MinkowskiVertex3D& a, const MinkowskiVertex3D& b, const MinkowskiVertex3D& c,
	auto projection) noexcept {
	const Length3D vA = projection(a);
	const Length3D vB = projection(b);
	const Length3D vC = projection(c);

	const Length3D ab = vB - vA;
	const Length3D ac = vC - vA;
	const Length3D bc = vC - vB;

	const Area abAlongA = dot(ab, vA);
	const Area acAlongA = dot(ac, vA);
	if (abAlongA >= 0 && acAlongA >= 0) {
		return a;
	}

	const Area abAlongB = dot(ab, vB);
	const Area acAlongB = dot(ac, vB);
	if (abAlongB <= 0 && acAlongB >= abAlongB) {
		return b;
	}

	if (abAlongA * acAlongB <= abAlongB * acAlongA && abAlongA <= 0 && abAlongB >= 0) {
		const Scale1D t = abAlongA / (abAlongA - abAlongB);
		const Scale1D u = 1.0f - t;
		const Scale1D w = t;
		return {
			((a.first - 0) * u + (c.first - 0) * w),
			((a.second - 0) * u + (c.second - 0) * w),
		};
	}

	const Area abAlongC = dot(ab, vC);
	const Area acAlongC = dot(ac, vC);
	if (acAlongC <= 0 && abAlongC >= acAlongC) {
		return c;
	}

	if (abAlongC * acAlongA <= abAlongA * acAlongC && acAlongA <= 0 && acAlongC >= 0) {
		const Scale1D t = acAlongA / (acAlongA - acAlongC);
		const Scale1D u = 1.0f - t;
		const Scale1D w = t;
		return {
			((a.first - 0) * u + (c.first - 0) * w),
			((a.second - 0) * u + (c.second - 0) * w),
		};
	}

	const Area bcAlongB = acAlongB - abAlongB;
	const Area bcAlongC = acAlongC - abAlongC;
	if (abAlongB * acAlongC <= abAlongC * acAlongB && bcAlongB <= 0 && bcAlongC >= 0) {
		const Scale1D t = bcAlongB / (bcAlongC - bcAlongB);
		const Scale1D v = 1.0f - t;
		const Scale1D w = t;
		return {
			((b.first - 0) * v + (c.first - 0) * w),
			((b.second - 0) * v + (c.second - 0) * w),
		};
	}

	const auto [u, v, w] = getBarycentricCoordinatesClosestToOriginOnMinkowskiPlane(vA, vB, vC);
	return {
		((a.first - 0) * u + (b.first - 0) * v + (c.first - 0) * w),
		((a.second - 0) * u + (b.second - 0) * v + (c.second - 0) * w),
	};
}

} // namespace grem::physics

#endif
