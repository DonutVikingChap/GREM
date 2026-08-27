// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

#include <utility> // std::move, std::swap

namespace grem {

namespace {

template <size_t N>
[[nodiscard]] size_t findFarthestVertexInDirection(Span<const vec<N, float>> vertices, vec<N, float> direction) noexcept {
	if (vertices.empty()) {
		[[unlikely]];
		return 0;
	}
	size_t farthestVertexIndex = 0;
	float farthestSignedDistance = dot(vertices.front(), direction);
	for (size_t vertexIndex = 1; vertexIndex < vertices.size(); ++vertexIndex) {
		const float signedDistance = dot(vertices[vertexIndex], direction);
		if (signedDistance > farthestSignedDistance) {
			farthestVertexIndex = vertexIndex;
			farthestSignedDistance = signedDistance;
		}
	}
	return farthestVertexIndex;
}

template <size_t N>
[[nodiscard]] size_t findFarthestUnvisitedVertexInDirection(Span<const vec<N, float>> vertices, Span<const bool> visited, vec<N, float> direction) noexcept {
	size_t farthestVertexIndex = vertices.size();
	float farthestSignedDistance = Limits<float>::MIN;
	for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
		if (visited[vertexIndex]) {
			continue;
		}
		const float signedDistance = dot(vertices[vertexIndex], direction);
		if (signedDistance > farthestSignedDistance) {
			farthestVertexIndex = vertexIndex;
			farthestSignedDistance = signedDistance;
		}
	}
	return farthestVertexIndex;
}

[[nodiscard]] bool buildSimplex(Buffer<size_t>& outputVertexIndices, Span<const vec3> vertices) {
	const vec3 initialDirection = normalize(vec3{1.0f});
	const size_t a = findFarthestVertexInDirection<3>(vertices, initialDirection);
	const size_t b = findFarthestVertexInDirection<3>(vertices, -initialDirection);
	if (a == b) {
		return false;
	}

	const vec3 ab = vertices[b] - vertices[a];
	const vec3 abX = cross(vec3{1.0f, 0.0f, 0.0f}, ab);
	const vec3 abY = cross(vec3{0.0f, 1.0f, 0.0f}, ab);
	const vec3 secondDirection = (length2(abX) > length2(abY)) ? abX : abY;
	size_t c = findFarthestVertexInDirection<3>(vertices, secondDirection);
	if (c == a || c == b) {
		c = findFarthestVertexInDirection<3>(vertices, -secondDirection);
		if (c == a || c == b) {
			return false;
		}
	}

	const vec3 ac = vertices[c] - vertices[a];
	const vec3 abc = cross(ab, ac);
	size_t d = findFarthestVertexInDirection<3>(vertices, abc);
	if (d == a || d == b || d == c) {
		d = findFarthestVertexInDirection<3>(vertices, -abc);
		if (d == a || d == b || d == c) {
			return false;
		}
	}

	const vec3 ad = vertices[d] - vertices[a];
	if (dot(ad, abc) < 0.0f) {
		std::swap(c, d);
	}

	outputVertexIndices.push_back(a);
	outputVertexIndices.push_back(b);
	outputVertexIndices.push_back(c);
	outputVertexIndices.push_back(d);
	return true;
}

} // namespace

detail::ConvexPolytopeData<2> detail::buildConvexHull2D(Span<const ConvexPolytopeVertex2D> vertices, const mat3& transformation, ConvexPolytopeVertexIndex maxVertexCount) {
	detail::ConvexPolytopeData<2> result{};
	if (min(vertices.size(), size_t{maxVertexCount}) < 3) {
		return result;
	}

	result.vertices.resize(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i) {
		result.vertices[i] = vec2{transformation * vec3{vertices[i], 1.0f}};
	}

	const Box<2, float> fullBoundingBox = detail::calculateBoundingBox<2>(result.vertices);
	const float epsilon = length(fullBoundingBox.max - fullBoundingBox.min) * 0.001f;
	const float epsilonSquared = length2(epsilon);

	// Reference: R.L. Graham: "An efficient algorith for determining the convex hull of a finite planar set": Information Processing Letters 1 (1972) 132-133: https://doi.org/10.1016%2F0020-0190%2872%2990045-2

	ConvexPolytopeVertex2D lowestVertex = result.vertices[0];
	for (const ConvexPolytopeVertex2D& vertex : Span{result.vertices}.subspan(1)) {
		if ((vertex.y == lowestVertex.y && vertex.x < lowestVertex.x) || vertex.y < lowestVertex.y) {
			lowestVertex = vertex;
		}
	}

	const auto getOrientation = [](ConvexPolytopeVertex2D a, ConvexPolytopeVertex2D b, ConvexPolytopeVertex2D c) -> double {
		// Testing on real-world meshes shows that double precision seems necessary for numerical stability in practice.
		const dvec2 ab = dvec2{b} - dvec2{a};
		const dvec2 bc = dvec2{c} - dvec2{b};
		return dot(dvec2{ab.y, -ab.x}, bc);
	};

	sort(result.vertices, [&](const ConvexPolytopeVertex2D& a, const ConvexPolytopeVertex2D& b) -> bool {
		const double ordering = getOrientation(lowestVertex, a, b);
		return (ordering == 0.0 && distance2(a, lowestVertex) < distance2(b, lowestVertex)) || ordering < 0.0;
	});

	Buffer<ConvexPolytopeVertex2D> stack{result.vertices[0]};
	for (const ConvexPolytopeVertex2D& vertex : Span{result.vertices}.subspan(1)) {
		while (stack.size() >= 2 && getOrientation(stack[stack.size() - 2], stack.back(), vertex) >= 0.0) {
			stack.pop_back();
		}
		if (distance2(vertex, stack.back()) > epsilonSquared) {
			stack.push_back(vertex);
		}
	}

	if (stack.size() < 3) {
		result.vertices.clear();
		return result;
	}

	if (stack.size() > size_t{maxVertexCount}) {
		// Grow epsilon exponentially until the stack has been reduced to the desired vertex count.
		// If we accidentally remove so many vertices that less than 3 remain,
		// keep the previous set before that and instead choose vertices by iterating around the hull with a uniform step size (handled by the loop after this).
		Buffer<ConvexPolytopeVertex2D> reducedVertices{};
		float newEpsilon = max(epsilon, Limits<float>::MACHINE_EPSILON * 8.0f);
		do {
			newEpsilon *= 1.125f;
			const float newEpsilonSquared = length(newEpsilon);
			reducedVertices.push_back(stack[0]);
			for (const ConvexPolytopeVertex2D& vertex : Span{stack}.subspan(1)) {
				if (distance2(vertex, reducedVertices.back()) > newEpsilonSquared) {
					reducedVertices.push_back(vertex);
				}
			}
			if (reducedVertices.size() < 3) {
				break;
			}
			stack.swap(reducedVertices);
			reducedVertices.clear();
		} while (stack.size() > size_t{maxVertexCount});
	}

	const size_t vertexCount = min(stack.size(), size_t{maxVertexCount});
	const size_t step = stack.size() / vertexCount;
	result.vertices.resize(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i) {
		result.vertices[i] = stack[i * step];
	}

	result.faces.resize(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i) {
		const vec2 difference = result.vertices[(i + 1) % vertexCount] - result.vertices[i];
		result.faces[i].normal = normalize(vec2{difference.y, -difference.x});
	}
	return result;
}

detail::ConvexPolytopeData<3> detail::buildConvexHull3D(Span<const ConvexPolytopeVertex3D> vertices, const mat4& transformation, ConvexPolytopeVertexIndex maxVertexCount) {
	detail::ConvexPolytopeData<3> result{};
	if (min(vertices.size(), size_t{maxVertexCount}) < 4) {
		return result;
	}

	result.vertices.resize(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i) {
		result.vertices[i] = vec3{transformation * vec4{vertices[i], 1.0f}};
	}

	const Box<3, float> fullBoundingBox = detail::calculateBoundingBox<3>(result.vertices);
	const float epsilon = length(fullBoundingBox.max - fullBoundingBox.min) * 0.001f;

	// References:
	// - Randy Gaul: "Convex Hull Generation with Quick Hull" (2013): https://randygaul.github.io/math/collision-detection/2013/11/01/Convex-Hull-Generation.html
	// - Dirk Gregorius (Valve Software): "Implementing Quickhull": Game Developers Conference (2014): https://media.gdcvault.com/GDC2014/Presentations/gregorius_dirk_implementing_quickhull.pdf

	Buffer<size_t> vertexIndices{};
	if (!buildSimplex(vertexIndices, result.vertices)) {
		result.vertices.clear();
		return result;
	}

	Buffer<ConvexPolytopeEdge3D> newEdges{
		ConvexPolytopeEdge3D{.vertexIndex = 0, .faceIndex = 0, .nextEdgeIndex = 2},  // 0
		ConvexPolytopeEdge3D{.vertexIndex = 2, .faceIndex = 2, .nextEdgeIndex = 9},  // 1
		ConvexPolytopeEdge3D{.vertexIndex = 2, .faceIndex = 0, .nextEdgeIndex = 4},  // 2
		ConvexPolytopeEdge3D{.vertexIndex = 1, .faceIndex = 3, .nextEdgeIndex = 11}, // 3
		ConvexPolytopeEdge3D{.vertexIndex = 1, .faceIndex = 0, .nextEdgeIndex = 0},  // 4
		ConvexPolytopeEdge3D{.vertexIndex = 0, .faceIndex = 1, .nextEdgeIndex = 6},  // 5
		ConvexPolytopeEdge3D{.vertexIndex = 1, .faceIndex = 1, .nextEdgeIndex = 8},  // 6
		ConvexPolytopeEdge3D{.vertexIndex = 3, .faceIndex = 3, .nextEdgeIndex = 3},  // 7
		ConvexPolytopeEdge3D{.vertexIndex = 3, .faceIndex = 1, .nextEdgeIndex = 5},  // 8
		ConvexPolytopeEdge3D{.vertexIndex = 0, .faceIndex = 2, .nextEdgeIndex = 10}, // 9
		ConvexPolytopeEdge3D{.vertexIndex = 3, .faceIndex = 2, .nextEdgeIndex = 1},  // 10
		ConvexPolytopeEdge3D{.vertexIndex = 2, .faceIndex = 3, .nextEdgeIndex = 7},  // 11
	};

	const auto getFaceNormal = [&](const ConvexPolytopeEdgeIndex firstEdgeIndex) -> vec3 {
		vec3 normal{};
		ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
		do {
			const ConvexPolytopeEdge3D& edge = newEdges[edgeIndex];
			const ConvexPolytopeEdgeIndex nextEdgeIndex = edge.nextEdgeIndex;
			const ConvexPolytopeEdge3D& nextEdge = newEdges[nextEdgeIndex];

			const vec3 vertex = result.vertices[vertexIndices[edge.vertexIndex]];
			const vec3 nextVertex = result.vertices[vertexIndices[nextEdge.vertexIndex]];

			normal.x += (vertex.y - nextVertex.y) * (vertex.z + nextVertex.z);
			normal.y += (vertex.z - nextVertex.z) * (vertex.x + nextVertex.x);
			normal.z += (vertex.x - nextVertex.x) * (vertex.y + nextVertex.y);

			edgeIndex = nextEdgeIndex;
		} while (edgeIndex != firstEdgeIndex);
		return normalize(normal);
	};

	Buffer<ConvexPolytopeFace3D> newFaces{
		ConvexPolytopeFace3D{.normal = getFaceNormal(0), .firstEdgeIndex = 0},
		ConvexPolytopeFace3D{.normal = getFaceNormal(5), .firstEdgeIndex = 5},
		ConvexPolytopeFace3D{.normal = getFaceNormal(1), .firstEdgeIndex = 1},
		ConvexPolytopeFace3D{.normal = getFaceNormal(3), .firstEdgeIndex = 3},
	};

	Allocation<bool> visitedVertices(result.vertices.size(), false);
	for (const size_t i : vertexIndices) {
		visitedVertices[i] = true;
	}
	for (const ConvexPolytopeFace3D& face : newFaces) {
		const vec3 facePoint = result.vertices[vertexIndices[newEdges[face.firstEdgeIndex].vertexIndex]];
		for (size_t vertexIndex = 0; vertexIndex < result.vertices.size(); ++vertexIndex) {
			if (dot(result.vertices[vertexIndex] - facePoint, vec3{face.normal}) > epsilon) {
				visitedVertices[vertexIndex] = false;
			}
		}
	}

	Buffer<ConvexPolytopeEdgeIndex> horizonLine{};
	Buffer<bool> verticesToKeep{};
	Buffer<bool> edgesToDelete{};
	Buffer<bool> facesToDelete{};
	Buffer<ConvexPolytopeVertexIndex> vertexIndicesAfterDeletion{};
	Buffer<ConvexPolytopeEdgeIndex> edgeIndicesAfterDeletion{};
	Buffer<ConvexPolytopeFaceIndex> faceIndicesAfterDeletion{};
	while (vertexIndices.size() < size_t{maxVertexCount}) {
		float farthestExpansion = epsilon;
		size_t farthestVertexIndex = result.vertices.size();
		size_t farthestFaceIndex = 0;

		const size_t edgeCount = newEdges.size();
		const size_t faceCount = newFaces.size();

		// Find new vertex that maximizes the expansion of the hull.
		for (size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			const ConvexPolytopeFace3D& face = newFaces[faceIndex];
			const vec3 facePoint = result.vertices[vertexIndices[newEdges[face.firstEdgeIndex].vertexIndex]];

			const size_t vertexIndex = findFarthestUnvisitedVertexInDirection<3>(result.vertices, visitedVertices, vec3{face.normal});
			if (vertexIndex < result.vertices.size()) {
				const float expansion = dot(result.vertices[vertexIndex] - facePoint, vec3{face.normal});
				if (expansion > farthestExpansion) {
					farthestExpansion = expansion;
					farthestVertexIndex = vertexIndex;
					farthestFaceIndex = faceIndex;
				}
			}
		}
		if (farthestVertexIndex >= result.vertices.size()) {
			break;
		}
		const vec3 newVertex = result.vertices[farthestVertexIndex];

		// Find the horizon line of the current mesh when viewed from the new vertex.
		horizonLine.clear();
		edgesToDelete.clear();
		facesToDelete.clear();
		edgesToDelete.resize(edgeCount, false);
		facesToDelete.resize(faceCount, false);
		size_t edgesToDeleteCount = 0;
		size_t facesToDeleteCount = 0;

		const auto isFaceVisible = [&](const ConvexPolytopeFace3D& face) -> bool {
			const vec3 facePoint = result.vertices[vertexIndices[newEdges[face.firstEdgeIndex].vertexIndex]];
			const float signedDistanceAlongFaceNormal = dot(newVertex - facePoint, vec3{face.normal});
			return signedDistanceAlongFaceNormal > -epsilon;
		};

		const auto findHorizonLine = [&](const auto& self, size_t faceIndex, ConvexPolytopeEdgeIndex startingEdgeIndex) -> void {
			facesToDelete[faceIndex] = true;
			++facesToDeleteCount;
			ConvexPolytopeEdgeIndex edgeIndex = startingEdgeIndex;
			do {
				const ConvexPolytopeEdgeIndex nextEdgeIndex = newEdges[edgeIndex].nextEdgeIndex;
				if (!edgesToDelete[edgeIndex]) {
					const ConvexPolytopeEdgeIndex twinEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndex ^ 1);
					GREM_ASSERT(!edgesToDelete[twinEdgeIndex]);
					const ConvexPolytopeFaceIndex twinFaceIndex = newEdges[twinEdgeIndex].faceIndex;
					if (facesToDelete[twinFaceIndex]) {
						edgesToDelete[edgeIndex] = true;
						edgesToDelete[twinEdgeIndex] = true;
						edgesToDeleteCount += 2;
					} else if (isFaceVisible(newFaces[twinFaceIndex])) {
						edgesToDelete[edgeIndex] = true;
						edgesToDelete[twinEdgeIndex] = true;
						edgesToDeleteCount += 2;
						self(self, twinFaceIndex, twinEdgeIndex);
					} else {
						horizonLine.push_back(edgeIndex);
					}
				}
				edgeIndex = nextEdgeIndex;
			} while (edgeIndex != startingEdgeIndex);
		};

		findHorizonLine(findHorizonLine, farthestFaceIndex, newFaces[farthestFaceIndex].firstEdgeIndex);

		GREM_ASSERT(edgesToDeleteCount % 2 == 0);
		if (horizonLine.size() < 3 || newEdges.size() - edgesToDeleteCount + horizonLine.size() * 2 > Limits<ConvexPolytopeEdgeIndex>::MAX ||
			newFaces.size() - facesToDeleteCount + horizonLine.size() > Limits<ConvexPolytopeFaceIndex>::MAX) {
			// We can't safely add more edges/faces.
			break;
		}

		// Delete faces visible to the new vertex.
		faceIndicesAfterDeletion.clear();
		size_t remainingFaceCount = 0;
		for (size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			if (facesToDelete[faceIndex]) {
				faceIndicesAfterDeletion.push_back(Limits<ConvexPolytopeFaceIndex>::MAX);
			} else {
				newFaces[remainingFaceCount] = newFaces[faceIndex];
				faceIndicesAfterDeletion.push_back(static_cast<ConvexPolytopeFaceIndex>(remainingFaceCount));
				++remainingFaceCount;
			}
		}
		GREM_ASSERT(remainingFaceCount == newFaces.size() - facesToDeleteCount);
		newFaces.resize(remainingFaceCount);

		// Delete the interior edges of the deleted faces, and fixup the face indices of the remaining edges.
		edgeIndicesAfterDeletion.clear();
		size_t remainingEdgeCount = 0;
		for (size_t edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex) {
			if (edgesToDelete[edgeIndex]) {
				edgeIndicesAfterDeletion.push_back(Limits<ConvexPolytopeEdgeIndex>::MAX);
			} else {
				ConvexPolytopeEdge3D& newEdge = newEdges[remainingEdgeCount];
				newEdge = newEdges[edgeIndex];
				newEdge.faceIndex = static_cast<ConvexPolytopeFaceIndex>(faceIndicesAfterDeletion[newEdge.faceIndex]);
				edgeIndicesAfterDeletion.push_back(static_cast<ConvexPolytopeEdgeIndex>(remainingEdgeCount));
				++remainingEdgeCount;
			}
		}
		GREM_ASSERT(remainingEdgeCount == newEdges.size() - edgesToDeleteCount);
		newEdges.resize(remainingEdgeCount);

		// Fixup the edge indices of remaining faces.
		for (ConvexPolytopeFace3D& face : newFaces) {
			GREM_ASSERT(edgeIndicesAfterDeletion[face.firstEdgeIndex] != Limits<ConvexPolytopeEdgeIndex>::MAX);
			face.firstEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndicesAfterDeletion[face.firstEdgeIndex]);
		}
		// Fixup the edge indices of remaining edges.
		for (ConvexPolytopeEdge3D& edge : newEdges) {
			GREM_ASSERT(edgeIndicesAfterDeletion[edge.nextEdgeIndex] != Limits<ConvexPolytopeEdgeIndex>::MAX || edge.faceIndex == Limits<ConvexPolytopeFaceIndex>::MAX);
			edge.nextEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndicesAfterDeletion[edge.nextEdgeIndex]);
		}
		// Fixup the edge indices of the horizon line.
		for (ConvexPolytopeEdgeIndex& edgeIndex : horizonLine) {
			GREM_ASSERT(edgeIndicesAfterDeletion[edgeIndex] != Limits<ConvexPolytopeEdgeIndex>::MAX);
			edgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndicesAfterDeletion[edgeIndex]);
			GREM_ASSERT(newEdges[edgeIndex].faceIndex == Limits<ConvexPolytopeFaceIndex>::MAX);
		}

		// Delete any vertices that are no longer referenced by an edge.
		if (edgesToDeleteCount >= 4) {
			verticesToKeep.clear();
			verticesToKeep.resize(vertexIndices.size(), false);
			for (const ConvexPolytopeEdge3D& edge : newEdges) {
				verticesToKeep[edge.vertexIndex] = true;
			}

			vertexIndicesAfterDeletion.clear();
			size_t remainingVertexCount = 0;
			for (size_t i = 0; i < vertexIndices.size(); ++i) {
				if (!verticesToKeep[i]) {
					vertexIndicesAfterDeletion.push_back(Limits<ConvexPolytopeVertexIndex>::MAX);
				} else {
					size_t& newVertexIndex = vertexIndices[remainingVertexCount];
					newVertexIndex = vertexIndices[i];
					vertexIndicesAfterDeletion.push_back(static_cast<ConvexPolytopeVertexIndex>(remainingVertexCount));
					++remainingVertexCount;
				}
			}
			vertexIndices.resize(remainingVertexCount);

			// Fixup the vertex indices of edges.
			for (ConvexPolytopeEdge3D& edge : newEdges) {
				GREM_ASSERT(vertexIndicesAfterDeletion[edge.vertexIndex] != Limits<ConvexPolytopeVertexIndex>::MAX);
				edge.vertexIndex = static_cast<ConvexPolytopeVertexIndex>(vertexIndicesAfterDeletion[edge.vertexIndex]);
			}
		}

		// Add the new vertex to the mesh.
		const ConvexPolytopeVertexIndex newVertexIndex = static_cast<ConvexPolytopeVertexIndex>(vertexIndices.size());
		vertexIndices.push_back(farthestVertexIndex);

		// Add new faces and edges towards the new vertex.
		const auto getNewFaceNormal = [&](vec3 firstVertex, vec3 secondVertex) -> vec3 {
			const vec3 result = normalize(cross(secondVertex - firstVertex, newVertex - firstVertex));
			GREM_ASSERT(!any(isnan(result)));
			return result;
		};

		size_t currentHorizonIndex = 0;
		vec3 currentVertex = result.vertices[vertexIndices[newEdges[horizonLine[1]].vertexIndex]];
		vec3 currentNormal = getNewFaceNormal(result.vertices[vertexIndices[newEdges[horizonLine[0]].vertexIndex]], currentVertex);
		const vec3 firstNormal = currentNormal;
		const ConvexPolytopeEdgeIndex firstUpEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(newEdges.size());
		newEdges.push_back(ConvexPolytopeEdge3D{.vertexIndex = newEdges[horizonLine[0]].vertexIndex, .faceIndex = 0, .nextEdgeIndex = 0});
		const ConvexPolytopeEdgeIndex firstDownEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(newEdges.size());
		const ConvexPolytopeFaceIndex firstFaceIndex = static_cast<ConvexPolytopeFaceIndex>(newFaces.size());

		const auto addNewFaceUntil = [&](size_t endHorizonIndex) -> void {
			const ConvexPolytopeFaceIndex faceIndex = static_cast<ConvexPolytopeFaceIndex>(newFaces.size());
			newFaces.push_back(ConvexPolytopeFace3D{.normal = currentNormal, .firstEdgeIndex = horizonLine[currentHorizonIndex]});

			const ConvexPolytopeEdgeIndex downEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(newEdges.size());
			newEdges.push_back(ConvexPolytopeEdge3D{.vertexIndex = newVertexIndex, .faceIndex = faceIndex, .nextEdgeIndex = horizonLine[currentHorizonIndex]});
			const ConvexPolytopeEdgeIndex upEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(newEdges.size());
			newEdges.push_back(ConvexPolytopeEdge3D{.vertexIndex = newEdges[horizonLine[endHorizonIndex]].vertexIndex, .faceIndex = faceIndex, .nextEdgeIndex = downEdgeIndex});
			for (size_t i = currentHorizonIndex; i < endHorizonIndex - 1; ++i) {
				newEdges[horizonLine[i]].faceIndex = faceIndex;
				newEdges[horizonLine[i]].nextEdgeIndex = horizonLine[i + 1];
			}
			newEdges[horizonLine[endHorizonIndex - 1]].faceIndex = faceIndex;
			newEdges[horizonLine[endHorizonIndex - 1]].nextEdgeIndex = upEdgeIndex;
		};

		for (size_t horizonIndex = 1; horizonIndex < horizonLine.size(); ++horizonIndex) {
			const vec3 nextVertex = result.vertices[vertexIndices[newEdges[horizonLine[(horizonIndex + 1) % horizonLine.size()]].vertexIndex]];
			const vec3 nextNormal = getNewFaceNormal(currentVertex, nextVertex);
			if (1.0f - dot(nextNormal, currentNormal) > Limits<float>::MACHINE_EPSILON) {
				addNewFaceUntil(horizonIndex);
				currentHorizonIndex = horizonIndex;
				currentNormal = nextNormal;
			}
			currentVertex = nextVertex;
		}

		if (currentHorizonIndex > 0) {
			if (1.0f - dot(firstNormal, currentNormal) > Limits<float>::MACHINE_EPSILON) {
				const ConvexPolytopeFaceIndex faceIndex = static_cast<ConvexPolytopeFaceIndex>(newFaces.size());
				newFaces.push_back(ConvexPolytopeFace3D{.normal = currentNormal, .firstEdgeIndex = horizonLine[currentHorizonIndex]});

				const ConvexPolytopeEdgeIndex downEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(newEdges.size());
				newEdges.push_back(ConvexPolytopeEdge3D{.vertexIndex = newVertexIndex, .faceIndex = faceIndex, .nextEdgeIndex = horizonLine[currentHorizonIndex]});

				newEdges[firstUpEdgeIndex].faceIndex = faceIndex;
				newEdges[firstUpEdgeIndex].nextEdgeIndex = downEdgeIndex;

				for (size_t i = currentHorizonIndex; i < horizonLine.size() - 1; ++i) {
					newEdges[horizonLine[i]].faceIndex = faceIndex;
					newEdges[horizonLine[i]].nextEdgeIndex = horizonLine[i + 1];
				}
				newEdges[horizonLine.back()].faceIndex = faceIndex;
				newEdges[horizonLine.back()].nextEdgeIndex = firstUpEdgeIndex;
			} else {
				newEdges[firstUpEdgeIndex] = newEdges.back();
				newEdges.pop_back();
				newEdges[firstDownEdgeIndex].nextEdgeIndex = horizonLine[currentHorizonIndex];

				newEdges[horizonLine[currentHorizonIndex - 1]].nextEdgeIndex = firstUpEdgeIndex;

				for (size_t i = currentHorizonIndex; i < horizonLine.size() - 1; ++i) {
					newEdges[horizonLine[i]].faceIndex = firstFaceIndex;
					newEdges[horizonLine[i]].nextEdgeIndex = horizonLine[i + 1];
				}
				newEdges[horizonLine.back()].faceIndex = firstFaceIndex;
				newEdges[horizonLine.back()].nextEdgeIndex = horizonLine[0];
			}
		} else {
			// We found that the normals of all new faces would be roughly equal.
			// This should pretty much never happen, but if it does, skip adding the new vertex,
			// make a new face that covers the entire horizon, and terminate the algorithm.
			vertexIndices.pop_back();
			newEdges.pop_back();

			const ConvexPolytopeFaceIndex faceIndex = static_cast<ConvexPolytopeFaceIndex>(newFaces.size());
			newFaces.push_back(ConvexPolytopeFace3D{.normal = currentNormal, .firstEdgeIndex = horizonLine[0]});

			for (size_t i = 0; i < horizonLine.size(); ++i) {
				newEdges[horizonLine[i]].faceIndex = faceIndex;
				newEdges[horizonLine[i]].nextEdgeIndex = horizonLine[(i + 1) % horizonLine.size()];
			}
			break;
		}

		visitedVertices[farthestVertexIndex] = true;
	}

	for (ConvexPolytopeFace3D& face : newFaces) {
		face.normal = getFaceNormal(face.firstEdgeIndex);
	}

	Allocation<vec3> finalVertices(vertexIndices.size());
	for (size_t i = 0; i < vertexIndices.size(); ++i) {
		finalVertices[i] = result.vertices[vertexIndices[i]];
	}
	result.vertices = std::move(finalVertices);
	result.edges.assign_range(newEdges);
	result.faces.assign_range(newFaces);

	result.vertexEdgeIndices.resize(result.vertices.size(), Limits<ConvexPolytopeEdgeIndex>::MAX);
	for (ConvexPolytopeEdgeIndex edgeIndex = 0; edgeIndex < result.edges.size(); ++edgeIndex) {
		result.vertexEdgeIndices[result.edges[edgeIndex].vertexIndex] = edgeIndex;
	}
	GREM_ASSERT(!contains(result.vertexEdgeIndices, Limits<ConvexPolytopeEdgeIndex>::MAX));
	return result;
}

} // namespace grem
