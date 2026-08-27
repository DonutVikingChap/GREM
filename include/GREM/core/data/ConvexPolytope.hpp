// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_CONVEX_POLYTOPE_HPP
#define GREM_CORE_DATA_CONVEX_POLYTOPE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>

#include <type_traits> // std::conditional_t
#include <utility>     // std::move

namespace grem {

/**
 * Index of a vertex in a polytope.
 */
using ConvexPolytopeVertexIndex = uint8_t;

/**
 * Index of an edge in a polytope.
 */
using ConvexPolytopeEdgeIndex = uint8_t;

/**
 * Index of a face in a polytope.
 */
using ConvexPolytopeFaceIndex = uint8_t;

/**
 * Vertex of a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using ConvexPolytopeVertex = vec<N, float>;

using ConvexPolytopeVertex2D = ConvexPolytopeVertex<2>; ///< Vertex of a convex polytope (polygon) in 2-dimensional space.
using ConvexPolytopeVertex3D = ConvexPolytopeVertex<3>; ///< Vertex of a convex polytope (polyhedron) in 3-dimensional space.

/**
 * Edge of a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ConvexPolytopeEdge;

template <>
struct ConvexPolytopeEdge<2> {};

template <>
struct ConvexPolytopeEdge<3> {
	/**
	 * Index of the vertex corresponding to the first point of the edge.
	 *
	 * The second point can be found in the twin edge that directly follows (or
	 * precedes) this one.
	 */
	ConvexPolytopeVertexIndex vertexIndex;

	/**
	 * The index of the face that this edge belongs to.
	 *
	 * The other face that shares this edge can be found in the twin edge that
	 * directly follows (or precedes) this one.
	 */
	ConvexPolytopeFaceIndex faceIndex;

	/**
	 * Index of the next edge on the face, which will eventually loop back
	 * around to this edge again.
	 *
	 * The winding order is counter-clockwise when viewed from outside of the
	 * shape.
	 */
	ConvexPolytopeEdgeIndex nextEdgeIndex;
};

using ConvexPolytopeEdge2D = ConvexPolytopeEdge<2>; ///< Edge on a convex polytope (polygon) in 2-dimensional space.
using ConvexPolytopeEdge3D = ConvexPolytopeEdge<3>; ///< Edge on a convex polytope (polyhedron) in 3-dimensional space.

/**
 * Face on a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ConvexPolytopeFace;

template <>
struct ConvexPolytopeFace<2> {
	/**
	 * Direction perpendicular to the face, pointing outwards from the center of
	 * the shape.
	 */
	vec<2, float> normal;
};

template <>
struct ConvexPolytopeFace<3> {
	/**
	 * Direction perpendicular to the face, pointing outwards from the center of
	 * the shape.
	 */
	vec<3, float> normal;

	/**
	 * Index of the first edge that makes up the face.
	 *
	 * The other edges can be found by following their "next" indices. The
	 * winding order is counter-clockwise when viewed from outside of the shape.
	 */
	ConvexPolytopeEdgeIndex firstEdgeIndex;
};

using ConvexPolytopeFace2D = ConvexPolytopeFace<2>; ///< Face on a convex polytope (polygon) in 2-dimensional space.
using ConvexPolytopeFace3D = ConvexPolytopeFace<3>; ///< Face on a convex polytope (polyhedron) in 3-dimensional space.

namespace detail {

struct NoEdges {};

template <size_t N>
struct ConvexPolytopeData {
	Allocation<ConvexPolytopeVertex<N>> vertices{};
	[[no_unique_address]] std::conditional_t<N == 3, Allocation<ConvexPolytopeEdgeIndex>, NoEdges> vertexEdgeIndices{};
	[[no_unique_address]] std::conditional_t<N == 3, Allocation<ConvexPolytopeEdge3D>, NoEdges> edges{};
	Allocation<ConvexPolytopeFace<N>> faces{};
};

GREM_API(core) ConvexPolytopeData<2> buildConvexHull2D(Span<const ConvexPolytopeVertex2D> vertices, const mat3& transformation, ConvexPolytopeVertexIndex maxVertexCount);
GREM_API(core) ConvexPolytopeData<3> buildConvexHull3D(Span<const ConvexPolytopeVertex3D> vertices, const mat4& transformation, ConvexPolytopeVertexIndex maxVertexCount);

template <size_t N>
[[nodiscard]] inline ConvexPolytopeData<N> buildConvexHull(Span<const ConvexPolytopeVertex<N>> vertices, const mat<N + 1, N + 1, float>& transformation,
	ConvexPolytopeVertexIndex maxVertexCount) {
	if constexpr (N == 2) {
		return buildConvexHull2D(vertices, transformation, maxVertexCount);
	} else {
		return buildConvexHull3D(vertices, transformation, maxVertexCount);
	}
}

template <size_t N>
[[nodiscard]] inline Box<N, float> calculateBoundingBox(Span<const vec<N, float>> vertices) noexcept {
	if (vertices.empty()) {
		return Box<N, float>{};
	}
	Box<N, float> result{
		.min = vertices.front(),
		.max = vertices.front(),
	};
	for (const vec<N, float> vertex : vertices.subspan(1)) {
		result.min = min(result.min, vertex);
		result.max = max(result.max, vertex);
	}
	return result;
}

template <size_t N>
[[nodiscard]] inline float calculateBoundingRadius(Span<const vec<N, float>> vertices) noexcept {
	float result = 0.0f;
	for (const vec<N, float> vertex : vertices) {
		result = max(result, length2(vertex));
	}
	return sqrt(result);
}

} // namespace detail

/**
 * Convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class ConvexPolytope {
public:
	using Vertex = ConvexPolytopeVertex<N>; ///< Vertex type of the convex polytope.
	using Edge = ConvexPolytopeEdge<N>;     ///< Edge type of the convex polytope.
	using Face = ConvexPolytopeFace<N>;     ///< Face type of the convex polytope.

	/**
	 * Construct a convex polygon from pre-calculated data.
	 *
	 * \param vertices vertices of the convex polyhedron.
	 * \param faces faces of the convex polygon. Must form a valid convex
	 *        polygon with the given vertices.
	 * \param boundingBox local bounding box of the convex polygon. Must contain
	 *        all of the given vertices.
	 * \param boundingRadius maximum distance from the origin of any vertex.
	 *        Must be greater than or equal to the largest vertex vector length.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	ConvexPolytope(Allocation<Vertex> vertices, Allocation<Face> faces, const Box<2, float>& boundingBox, float boundingRadius) requires(N == 2)
		: data{.vertices = std::move(vertices), .faces = std::move(faces)}
		, boundingBox(boundingBox)
		, boundingRadius(boundingRadius) {}

	/**
	 * Construct a convex polyhedron from pre-calculated data.
	 *
	 * \param vertices vertices of the convex polyhedron.
	 * \param vertexEdgeIndices outgoing edges corresponding to each vertex.
	 *        Must have the same size as the vertex array.
	 * \param edges edges of the convex polyhedron. Must form a valid convex
	 *        polyhedron with the given vertices and faces.
	 * \param faces faces of the convex polyhedron. Must form a valid convex
	 *        polyhedron with the given vertices and edges.
	 * \param boundingBox local bounding box of the convex polygon. Must contain
	 *        all of the given vertices.
	 * \param boundingRadius maximum distance from the origin of any vertex.
	 *        Must be greater than or equal to the largest vertex vector length.
	 */
	ConvexPolytope(Allocation<Vertex> vertices, Allocation<ConvexPolytopeEdgeIndex> vertexEdgeIndices, Allocation<Edge> edges, Allocation<Face> faces,
		const Box<3, float>& boundingBox, float boundingRadius) requires(N == 3)
		: data{.vertices = std::move(vertices), .vertexEdgeIndices = std::move(vertexEdgeIndices), .edges = std::move(edges), .faces = std::move(faces)}
		, boundingBox(boundingBox)
		, boundingRadius(boundingRadius) {}

	/**
	 * Construct a convex polytope from the convex hull of a set of vertices.
	 *
	 * \param vertices set of vertices to construct the convex polytope from.
	 * \param transformation transformation to apply to the vertices when
	 *        building the convex hull.
	 * \param maxVertexCount maximum number of vertices to build the convex hull
	 *        out of.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	ConvexPolytope(Span<const Vertex> vertices, const mat<N + 1, N + 1, float>& transformation, ConvexPolytopeVertexIndex maxVertexCount = Limits<ConvexPolytopeVertexIndex>::MAX)
		: data(detail::buildConvexHull<N>(vertices, transformation, maxVertexCount))
		, boundingBox(detail::calculateBoundingBox<N>(data.vertices))
		, boundingRadius(detail::calculateBoundingRadius<N>(data.vertices)) {}

	/**
	 * Construct a convex polytope from a set of vertices.
	 *
	 * \param vertices set of vertices to construct the convex polytope from.
	 * \param maxVertexCount maximum number of vertices to build the convex hull
	 *        out of.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit ConvexPolytope(Span<const Vertex> vertices, ConvexPolytopeVertexIndex maxVertexCount = Limits<ConvexPolytopeVertexIndex>::MAX)
		: ConvexPolytope(vertices, mat<N + 1, N + 1, float>{1.0f}, maxVertexCount) {}

	/**
	 * Get the vertices of the convex polytope.
	 *
	 * \return the vertices of the convex polytope.
	 */
	[[nodiscard]] Span<const Vertex> getVertices() const noexcept {
		return data.vertices;
	}

	/**
	 * Get the vertex edge indices of the convex polytope.
	 *
	 * \return the outgoing edge indices corresponding to each vertex.
	 */
	[[nodiscard]] Span<const ConvexPolytopeEdgeIndex> getVertexEdgeIndices() const noexcept requires(N == 3) {
		return data.vertexEdgeIndices;
	}

	/**
	 * Get the edges of the convex polytope.
	 *
	 * \return the edges of the convex polytope.
	 */
	[[nodiscard]] Span<const Edge> getEdges() const noexcept requires(N == 3) {
		return data.edges;
	}

	/**
	 * Get the faces of the convex polytope.
	 *
	 * \return the faces of the convex polytope.
	 */
	[[nodiscard]] Span<const Face> getFaces() const noexcept {
		return data.faces;
	}

	/**
	 * Get the bounding box of the convex polytope.
	 *
	 * \return a local axis-aligned bounding box that contains all vertices in
	 *         the convex polytope.
	 */
	[[nodiscard]] Box<N, float> getBoundingBox() const noexcept {
		return boundingBox;
	}

	/**
	 * Get the bounding radius of the convex polytope.
	 *
	 * \return the bounding radius of the convex polytope.
	 */
	[[nodiscard]] float getBoundingRadius() const noexcept {
		return boundingRadius;
	}

private:
	detail::ConvexPolytopeData<N> data{};
	Box<N, float> boundingBox{};
	float boundingRadius{};
};
using ConvexPolytope2D = ConvexPolytope<2>; ///< Convex polytope in 2-dimensional space.
using ConvexPolytope3D = ConvexPolytope<3>; ///< Convex polytope in 3-dimensional space.

} // namespace grem

#endif
