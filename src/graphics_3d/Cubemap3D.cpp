// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics_3d/Cubemap3D.hpp>

namespace grem::graphics {

constexpr Array<Cubemap3D::Vertex, 8> Cubemap3D::MESH_VERTICES{
	Vertex{.vertexPosition{1.0f, -1.0f, -1.0f}},
	Vertex{.vertexPosition{1.0f, 1.0f, -1.0f}},
	Vertex{.vertexPosition{1.0f, -1.0f, 1.0f}},
	Vertex{.vertexPosition{1.0f, 1.0f, 1.0f}},

	Vertex{.vertexPosition{-1.0f, -1.0f, -1.0f}},
	Vertex{.vertexPosition{-1.0f, 1.0f, -1.0f}},
	Vertex{.vertexPosition{-1.0f, -1.0f, 1.0f}},
	Vertex{.vertexPosition{-1.0f, 1.0f, 1.0f}},
};

constexpr Array<Cubemap3D::Index, 36> Cubemap3D::MESH_INDICES{
	// clang-format off
	4, 2, 0,
	2, 7, 3,
	6, 5, 7,
	1, 7, 5,
	0, 3, 1,
	4, 1, 5,
	4, 6, 2,
	2, 6, 7,
	6, 4, 5,
	1, 3, 7,
	0, 2, 3,
	4, 0, 1,
	// clang-format on
};

} // namespace grem::graphics
