// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_BRUSHES_HPP
#define GREM_EXAMPLES_TILES_BRUSHES_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>

#include "Tile.hpp"

struct BrushID : RegistryElementIDBase<BrushID> {
	using RegistryElementIDBase::RegistryElementIDBase;
};

struct Brush {
	struct TilePlacement {
		Offset3D offset;
		CategorizedTile tile;
	};

	vec2 origin{};
	ArrayList<TilePlacement> tilePlacements{};
};

using Brushes = Registry<Brush, BrushID>;

#endif
