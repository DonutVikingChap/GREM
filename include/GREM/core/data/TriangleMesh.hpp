// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_TRIANGLE_MESH_HPP
#define GREM_CORE_DATA_TRIANGLE_MESH_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/LooseOrthtree.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>

#include <utility> // std::move

namespace grem {

template <size_t N>
using TriangleMeshVertex = vec<N, float>;           ///< Vertex in a triangle mesh.
using TriangleMeshVertex2D = TriangleMeshVertex<2>; ///< Vertex in a 2-dimensional triangle mesh.
using TriangleMeshVertex3D = TriangleMeshVertex<3>; ///< Vertex in a 3-dimensional triangle mesh.

using TriangleMeshVertexIndex = uint32_t; ///< Index of a vertex in a triangle mesh.
using TriangleMeshFaceIndex = uint32_t;   ///< Index of a triangle in a triangle mesh, equal to the index of the first vertex index divided by 3.

/**
 * Triangle mesh.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class TriangleMesh {
public:
	using Vertex = TriangleMeshVertex<N>;        ///< Vertex type of the triangle mesh.
	using VertexIndex = TriangleMeshVertexIndex; ///< Vertex index type of the triangle mesh.
	using FaceIndex = TriangleMeshFaceIndex;     ///< Face index type of the triangle mesh.

	/**
	 * Construct a triangle mesh from a set of vertices.
	 *
	 * \param vertices set of vertices to construct the triangle mesh from.
	 * \param indices set of vertex indices to construct the triangle mesh from.
	 *        Each index must be less than the number of vertices. Size must be
	 *        a multiple of 3.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	TriangleMesh(Allocation<Vertex> vertices, Allocation<VertexIndex> indices)
		: vertices(std::move(vertices))
		, indices(std::move(indices))
		, boundingBox([&]() -> Box<N, float> {
			if (this->vertices.empty()) {
				return Box<N, float>{};
			}
			Box<N, float> result{
				.min = this->vertices.front(),
				.max = this->vertices.front(),
			};
			for (const Vertex vertex : Span{this->vertices}.subspan(1)) {
				result.min = min(result.min, vertex);
				result.max = max(result.max, vertex);
			}
			return result;
		}())
		, boundingRadius([&]() -> float {
			float result = 0.0f;
			for (const Vertex vertex : this->vertices) {
				result = max(result, length2(vertex));
			}
			return sqrt(result);
		}())
		, faceOrthtree(boundingBox, [&]() -> float {
			const Length<N, float> extents = boundingBox.max - boundingBox.min;
			float result = extents[0];
			for (size_t i = 1; i < N; ++i) {
				result = max(result, extents[i]);
			}
			return result;
		}() * 0.01f) {
		GREM_ASSERT(this->indices.size() % 3 == 0);
		GREM_ASSERT(this->indices.size() / 3 <= size_t{Limits<FaceIndex>::MAX});
		const FaceIndex faceCount = static_cast<FaceIndex>(this->indices.size() / 3);
		for (FaceIndex faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			const size_t indexOffset = static_cast<size_t>(faceIndex) * 3;
			GREM_ASSERT(this->indices[indexOffset + 0] < this->vertices.size());
			GREM_ASSERT(this->indices[indexOffset + 1] < this->vertices.size());
			GREM_ASSERT(this->indices[indexOffset + 2] < this->vertices.size());
			const Triangle<N, float> triangle{
				.pointA = this->vertices[this->indices[indexOffset + 0]],
				.pointB = this->vertices[this->indices[indexOffset + 1]],
				.pointC = this->vertices[this->indices[indexOffset + 2]],
			};
			const Box<N, float> triangleAABB = triangle.getBoundingBox();
			faceOrthtree.insert(triangleAABB, faceIndex);
		}
	}

	/**
	 * Construct a triangle mesh from a set of vertices and indices.
	 *
	 * \param vertices read-only view over a set of vertices to construct the
	 *        triangle mesh from.
	 * \param indices read-only view over a set of vertex indices to construct
	 *        the triangle mesh from. Each index must be less than the number of
	 *        vertices. Size must be a multiple of 3.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	TriangleMesh(Span<const Vertex> vertices, Span<const VertexIndex> indices)
		: TriangleMesh(Allocation<Vertex>{vertices.begin(), vertices.end()}, Allocation<VertexIndex>{indices.begin(), indices.end()}) {}

	/**
	 * Get the vertices of the mesh.
	 *
	 * \return a read-only view over the vertices of the mesh.
	 */
	[[nodiscard]] Span<const Vertex> getVertices() const noexcept {
		return vertices;
	}

	/**
	 * Get the vertex indices of the mesh.
	 *
	 * \return a read-only view over the indices of the mesh, defining triangles
	 *         in sets of 3, with counter-clockwise winding order. Its size must
	 *         be a multiple of 3, and each index must be less than the number
	 *         of vertices in the mesh.
	 */
	[[nodiscard]] Span<const VertexIndex> getIndices() const noexcept {
		return indices;
	}

	/**
	 * Get the bounding box of the mesh.
	 *
	 * \return a local axis-aligned bounding box that contains all vertices in
	 *         the triangle mesh.
	 */
	[[nodiscard]] Box<N, float> getBoundingBox() const noexcept {
		return boundingBox;
	}

	/**
	 * Get the bounding radius of the mesh.
	 *
	 * \return the bounding radius of the triangle mesh.
	 */
	[[nodiscard]] float getBoundingRadius() const noexcept {
		return boundingRadius;
	}

	/**
	 * Get the orthtree of faces in the mesh.
	 *
	 * \return a loose orthtree structure for accelerating intersection queries
	 *         against the faces (triangles) of the mesh, that stores face
	 *         indices, which are equal to the indices of the first
	 *         corresponding vertex indices divided by 3.
	 */
	[[nodiscard]] const LooseOrthtree<N, FaceIndex>& getFaceOrthtree() const noexcept {
		return faceOrthtree;
	}

private:
	Allocation<Vertex> vertices;
	Allocation<VertexIndex> indices;
	Box<N, float> boundingBox;
	float boundingRadius;
	LooseOrthtree<N, FaceIndex> faceOrthtree;
};
using TriangleMesh2D = TriangleMesh<2>; ///< Triangle mesh in 2-dimensional space.
using TriangleMesh3D = TriangleMesh<3>; ///< Triangle mesh in 3-dimensional space.

} // namespace grem

#endif
