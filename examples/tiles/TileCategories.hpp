// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_TILE_CATEGORIES_HPP
#define GREM_EXAMPLES_TILES_TILE_CATEGORIES_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>

struct TileCategoryID : RegistryElementIDBase<TileCategoryID> {
	using RegistryElementIDBase::RegistryElementIDBase;
};

enum class TileCollisionType : uint8_t {
	NONE,
	SOLID,
	LIQUID,
};

struct TileCategory {
	TileCollisionType collisionType = TileCollisionType::NONE;
	float friction = 1.0f;
};

using TileCategories = Registry<TileCategory, TileCategoryID>;

#endif
