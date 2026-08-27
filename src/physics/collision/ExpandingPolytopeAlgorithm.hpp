// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_EXPANDING_POLYTOPE_ALGORITHM_HPP
#define GREM_PHYSICS_COLLISION_EXPANDING_POLYTOPE_ALGORITHM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include "minkowski_difference.hpp"

namespace grem::physics {

template <size_t N>
class ExpandingPolytopeAlgorithm {
public:
	struct FindClosestFeatureResult {
		Array<MinkowskiVertex<N>, N> vertices{};
		Direction<N> normal = Y_AXIS<N>;
		Length1D depth{};
		size_t iteration = 0;
	};

	[[nodiscard]] FindClosestFeatureResult findClosestFeature(ArenaResource* temporaryMemoryResource, Span<const MinkowskiVertex<N>, N + 1> initialSimplex,
		FunctionView<MinkowskiVertex<N>(Direction<N> direction)> support, Distance collisionDistanceErrorTolerance, size_t maxIterationCount) {
		GREM_PROFILE_FUNCTION();

		// Expand the initial tetrahedron (or triangle in 2D) that encloses the origin with additional vertices in the Minkowski difference
		// until the face that is closest to the origin cannot be expanded further.
		// When this is done, we have found the closest face to the origin that is on the actual hulls of the shapes.

		FindClosestFeatureResult result{};

		ArrayList<MinkowskiVertex<N>, ArenaAllocator<MinkowskiVertex<N>>> vertices{temporaryMemoryResource};
		ArrayList<Face, ArenaAllocator<Face>> faces{temporaryMemoryResource};
		ArrayList<Edge, ArenaAllocator<Edge>> uniqueRemovedEdges{temporaryMemoryResource};

		vertices.reserve((N + 1) * 2);
		faces.reserve((N + 1) * 3);
		uniqueRemovedEdges.reserve(N * 3);

		// Initialize hull to the initial simplex.
		vertices.assign_range(initialSimplex);
		for (const Array<Index, N>& initialFaceVertexIndices : INITIAL_FACES_VERTEX_INDICES) {
			const Optional<Face> newFace = createFace(initialFaceVertexIndices, vertices);
			if (!newFace) {
				[[unlikely]];
				// We failed to calculate all face normals of the initial simplex. Look for any valid face to return as a backup.
				for (const Array<Index, N>& initialFaceVertexIndicesAgain : INITIAL_FACES_VERTEX_INDICES) {
					if (const Optional<Face> face = createFace(initialFaceVertexIndicesAgain, vertices)) {
						for (size_t i = 0; i < N; ++i) {
							result.vertices[i] = vertices[face->indices[i]];
						}
						result.normal = face->normal;
						result.depth = face->depth;
						return result;
					}
				}
				return result;
			}
			faces.push_back(*newFace);
		}

		while (result.iteration < maxIterationCount) {
			++result.iteration;

			// Find the closest face.
			GREM_ASSERT(!faces.empty());
			const Face closestFace = *minElement(faces, [](const Face& a, const Face& b) { return a.depth < b.depth; });

			// Update the result to reflect the found closest face.
			for (size_t i = 0; i < N; ++i) {
				result.vertices[i] = vertices[closestFace.indices[i]];
			}
			result.normal = closestFace.normal;
			result.depth = closestFace.depth;

			// Check if there is a vertex further out in the normal direction of the current face.
			const MinkowskiVertex<N> vertex = support(closestFace.normal);
			const Length<N> v = vertex.first - vertex.second;
			if (dot(v, closestFace.normal) <= closestFace.depth + collisionDistanceErrorTolerance) {
				break; // The current face cannot be expanded further, so the algorithm is done.
			}

			// Remove all faces that are facing the new vertex.
			removeFacesTowardVertex(faces, uniqueRemovedEdges, v);
			if (uniqueRemovedEdges.empty()) {
				return result; // Something went wrong; there aren't any edges left to attach the new vertex to. Bail out.
			}

			// Add the new vertex.
			if (vertices.size() > size_t{Limits<Index>::MAX}) {
				return result; // Too many vertices. Bail out.
			}
			const Index newVertexIndex = static_cast<Index>(vertices.size());
			vertices.push_back(vertex);

			// Add the faces created by the new vertex.
			for (const Edge& edge : uniqueRemovedEdges) {
				const Array<Index, N> newFaceVertexIndices = [&]() -> Array<Index, N> {
					if constexpr (N == 2) {
						return {edge.indices[0], newVertexIndex};
					} else {
						return {edge.indices[0], edge.indices[1], newVertexIndex};
					}
				}();
				const Optional<Face> newFace = createFace(newFaceVertexIndices, vertices);
				if (!newFace) {
					return result; // Failed to calculate new face normal. Bail out.
				}
				faces.push_back(*newFace);
			}
		}

		// Return the closest face that we were able to find.
		return result;
	}

	struct FindDeepestPenetrationResult {
		Pair<Position<N>> witnessPoints{};
		Direction<N> normal{};
		Length1D depth{};
		size_t iteration;
	};

	[[nodiscard]] FindDeepestPenetrationResult findDeepestPenetration(ArenaResource* temporaryMemoryResource, Span<const MinkowskiVertex<N>, N + 1> initialSimplex,
		const convex_shape<N> auto& shapeA, const Transformation<N>& transformationA, const InverseTransformation<N>& inverseTransformationA, const convex_shape<N> auto& shapeB,
		const Transformation<N>& transformationB, const InverseTransformation<N>& inverseTransformationB, Distance margin, Distance collisionDistanceErrorTolerance,
		size_t maxIterationCount) {
		const FindClosestFeatureResult result = findClosestFeature(
			temporaryMemoryResource, initialSimplex,
			[&](Direction<N> direction) -> MinkowskiVertex<N> {
				const Direction<N> localDirectionA = inverseTransformationA.getDirection(direction);
				const Direction<N> localDirectionB = inverseTransformationB.getDirection(direction);
				return {
					transformationA(shapeA.getLocalSupportPointOffset(localDirectionA)) + direction * margin,
					transformationB(shapeB.getLocalSupportPointOffset(-localDirectionB)) - direction * margin,
				};
			},
			collisionDistanceErrorTolerance, maxIterationCount);
		const auto projection = [](const MinkowskiVertex<N>& vertex) -> Length<N> {
			return vertex.first - vertex.second;
		};
		MinkowskiVertex<N> vertex{};
		if constexpr (N == 2) {
			vertex = getPointClosestToOriginOnMinkowskiLine(result.vertices[0], result.vertices[1], projection);
		} else {
			vertex = getPointClosestToOriginOnMinkowskiPlane(result.vertices[0], result.vertices[1], result.vertices[2], projection);
		}
		return {
			.witnessPoints{
				vertex.first - result.normal * margin,
				vertex.second + result.normal * margin,
			},
			.normal = result.normal,
			.depth = result.depth - margin,
			.iteration = result.iteration,
		};
	}

private:
	using Index = uint8_t;

	struct Face {
		Array<Index, N> indices;
		Direction<N> normal;
		Length1D depth;
	};

	struct Edge {
		Array<Index, N - 1> indices;

		[[nodiscard]] bool operator==(const Edge&) const = default;
		[[nodiscard]] auto operator<=>(const Edge&) const = default;
	};

	static constexpr auto INITIAL_FACES_VERTEX_INDICES = []() -> Array<Array<Index, N>, N + 1> {
		if constexpr (N == 2) {
			return {{{0, 1}, {1, 2}, {2, 0}}};
		} else {
			return {{{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}}};
		}
	}();

	void removeFacesTowardVertex(ArrayList<Face, ArenaAllocator<Face>>& faces, ArrayList<Edge, ArenaAllocator<Edge>>& uniqueRemovedEdges, Length<N> v) {
		uniqueRemovedEdges.clear();
		erase_if(faces, [&](const Face& face) {
			if (dot(face.normal, v) > face.depth) {
				if constexpr (N == 2) {
					removeEdge(uniqueRemovedEdges, Edge{.indices{face.indices[0]}});
					removeEdge(uniqueRemovedEdges, Edge{.indices{face.indices[1]}});
				} else {
					removeEdge(uniqueRemovedEdges, Edge{.indices{face.indices[0], face.indices[1]}});
					removeEdge(uniqueRemovedEdges, Edge{.indices{face.indices[1], face.indices[2]}});
					removeEdge(uniqueRemovedEdges, Edge{.indices{face.indices[2], face.indices[0]}});
				}
				return true;
			}
			return false;
		});
	}

	void removeEdge(ArrayList<Edge, ArenaAllocator<Edge>>& uniqueRemovedEdges, const Edge& edge) {
		// Check if the reverse edge has already been removed.
		// If it has, remove it from the unique set. Otherwise, add the new edge to the unique removed set.
		Edge reverseEdge{};
		if constexpr (N == 2) {
			reverseEdge = edge;
		} else {
			reverseEdge = Edge{.indices{edge.indices[1], edge.indices[0]}};
		}
		if (const auto it = lowerBound(uniqueRemovedEdges, reverseEdge); it != uniqueRemovedEdges.end() && *it == reverseEdge) {
			uniqueRemovedEdges.erase(it); // Not unique.
		} else {
			uniqueRemovedEdges.insert(upperBound(uniqueRemovedEdges, edge), edge); // Unique.
		}
	}

	[[nodiscard]] Optional<Face> createFace(const Array<Index, N>& newFaceVertexIndices, Span<const MinkowskiVertex<N>> vertices) {
		// Calculate the normal and depth of this face.
		const MinkowskiVertex<N> a = vertices[newFaceVertexIndices[0]];
		const MinkowskiVertex<N> b = vertices[newFaceVertexIndices[1]];
		const Length<N> vA = a.first - a.second;
		const Length<N> vB = b.first - b.second;
		const Length<N> ab = vB - vA;
		const Optional<Direction<N>> normal = [&] {
			if constexpr (N == 2) {
				return tryNormalize(rotate90DegreesCounterclockwise(ab));
			} else {
				const MinkowskiVertex3D c = vertices[newFaceVertexIndices[2]];
				const Length3D vC = c.first - c.second;
				const Length3D ac = vC - vA;
				return tryNormalize(cross(ab, ac));
			}
		}();
		if (!normal) {
			return {}; // Invalid normal, bail out.
		}
		const Length1D depth = dot(vA, *normal);
		const bool flipNormal = signbit(depth); // Flip the normal if it's pointing in the wrong direction.
		return Face{
			.indices = newFaceVertexIndices,
			.normal = flipSignIf(*normal, flipNormal),
			.depth = flipSignIf(depth, flipNormal),
		};
	}
};

} // namespace grem::physics

#endif
