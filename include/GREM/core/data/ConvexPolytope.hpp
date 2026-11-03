// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_CONVEX_POLYTOPE_HPP
#define GREM_CORE_DATA_CONVEX_POLYTOPE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>

#include <type_traits> // std::conditional_t
#include <utility>     // std::move

namespace grem {

/**
 * Index of a vertex in a convex polytope.
 */
using ConvexPolytopeVertexIndex = uint8_t;

/**
 * Index of an edge in a convex polytope.
 */
using ConvexPolytopeEdgeIndex = uint8_t;

/**
 * Index of a face in a convex polytope.
 */
using ConvexPolytopeFaceIndex = uint8_t;

/**
 * Vertex of a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 * \tparam Scalar scalar coordinate component type.
 */
template <size_t N, typename Scalar>
using ConvexPolytopeVertex = vec<N, Scalar>;

/**
 * Edge of a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 * \tparam Scalar scalar coordinate component type.
 */
template <size_t N, typename Scalar>
struct ConvexPolytopeEdge;

template <typename Scalar>
struct ConvexPolytopeEdge<2, Scalar> {};

template <typename Scalar>
struct ConvexPolytopeEdge<3, Scalar> {
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

/**
 * Face on a convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 * \tparam Scalar scalar coordinate component type.
 */
template <size_t N, typename Scalar>
struct ConvexPolytopeFace;

template <typename Scalar>
struct ConvexPolytopeFace<2, Scalar> {
	/**
	 * Direction perpendicular to the face, pointing outwards from the center of
	 * the shape.
	 */
	vec<2, Scalar> normal;
};

template <typename Scalar>
struct ConvexPolytopeFace<3, Scalar> {
	/**
	 * Direction perpendicular to the face, pointing outwards from the center of
	 * the shape.
	 */
	vec<3, Scalar> normal;

	/**
	 * Index of the first edge that makes up the face.
	 *
	 * The other edges can be found by following their "next" indices. The
	 * winding order is counter-clockwise when viewed from outside of the shape.
	 */
	ConvexPolytopeEdgeIndex firstEdgeIndex;
};

namespace detail {

struct NoEdges {};

template <size_t N, typename Scalar>
struct ConvexPolytopeData {
	Allocation<ConvexPolytopeVertex<N, Scalar>> vertices{};
	[[no_unique_address]] std::conditional_t<N == 3, Allocation<ConvexPolytopeEdgeIndex>, NoEdges> vertexEdgeIndices{};
	[[no_unique_address]] std::conditional_t<N == 3, Allocation<ConvexPolytopeEdge<3, Scalar>>, NoEdges> edges{};
	Allocation<ConvexPolytopeFace<N, Scalar>> faces{};
};

GREM_API(core)
ConvexPolytopeData<2, float> buildConvexHull2DFloat(Span<const ConvexPolytopeVertex<2, float>> vertices, const mat3& transformation, ConvexPolytopeVertexIndex maxVertexCount);

GREM_API(core)
ConvexPolytopeData<2, double> buildConvexHull2DDouble(Span<const ConvexPolytopeVertex<2, double>> vertices, const dmat3& transformation, ConvexPolytopeVertexIndex maxVertexCount);

GREM_API(core)
ConvexPolytopeData<3, float> buildConvexHull3DFloat(Span<const ConvexPolytopeVertex<3, float>> vertices, const mat4& transformation, ConvexPolytopeVertexIndex maxVertexCount);

GREM_API(core)
ConvexPolytopeData<3, double> buildConvexHull3DDouble(Span<const ConvexPolytopeVertex<3, double>> vertices, const dmat4& transformation, ConvexPolytopeVertexIndex maxVertexCount);

template <size_t N, typename Scalar>
[[nodiscard]] inline ConvexPolytopeData<N, Scalar> buildConvexHull(Span<const ConvexPolytopeVertex<N, Scalar>> vertices, const mat<N + 1, N + 1, Scalar>& transformation,
	ConvexPolytopeVertexIndex maxVertexCount) {
	if constexpr (N == 2) {
		if constexpr (same_as<Scalar, float>) {
			return buildConvexHull2DFloat(vertices, transformation, maxVertexCount);
		} else {
			return buildConvexHull2DDouble(vertices, transformation, maxVertexCount);
		}
	} else {
		if constexpr (same_as<Scalar, float>) {
			return buildConvexHull3DFloat(vertices, transformation, maxVertexCount);
		} else {
			return buildConvexHull3DDouble(vertices, transformation, maxVertexCount);
		}
	}
}

} // namespace detail

/**
 * Convex polytope.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 * \tparam Scalar scalar coordinate component type.
 */
template <size_t N, typename Scalar>
class ConvexPolytope {
public:
	using Vertex = ConvexPolytopeVertex<N, Scalar>; ///< Vertex type of the convex polytope.
	using Edge = ConvexPolytopeEdge<N, Scalar>;     ///< Edge type of the convex polytope.
	using Face = ConvexPolytopeFace<N, Scalar>;     ///< Face type of the convex polytope.

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
	ConvexPolytope(Allocation<Vertex> vertices, Allocation<Face> faces, const Box<2, Scalar>& boundingBox, Scalar boundingRadius) requires(N == 2)
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
		const Box<3, Scalar>& boundingBox, Scalar boundingRadius) requires(N == 3)
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
	ConvexPolytope(Span<const Vertex> vertices, const mat<N + 1, N + 1, Scalar>& transformation, ConvexPolytopeVertexIndex maxVertexCount = Limits<ConvexPolytopeVertexIndex>::MAX)
		: data(detail::buildConvexHull<N>(vertices, transformation, maxVertexCount))
		, boundingBox(calculateBoundingBox<N, Scalar>(data.vertices))
		, boundingRadius(calculateBoundingRadius<N, Scalar>(data.vertices)) {}

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
		: ConvexPolytope(vertices, mat<N + 1, N + 1, Scalar>{Scalar{1}}, maxVertexCount) {}

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
	[[nodiscard]] Box<N, Scalar> getBoundingBox() const noexcept {
		return boundingBox;
	}

	/**
	 * Get the bounding radius of the convex polytope.
	 *
	 * \return the bounding radius of the convex polytope.
	 */
	[[nodiscard]] Scalar getBoundingRadius() const noexcept {
		return boundingRadius;
	}

private:
	detail::ConvexPolytopeData<N, Scalar> data{};
	Box<N, Scalar> boundingBox{};
	Scalar boundingRadius{};
};

} // namespace grem

#endif
