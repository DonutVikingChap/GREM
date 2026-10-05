// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_COLLISION_LAYERS_HPP
#define GREM_EXAMPLES_PLATFORMER_COLLISION_LAYERS_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

inline constexpr phys::CollisionLayer COLLISION_LAYER_DEFAULT{0};
inline constexpr phys::CollisionLayer COLLISION_LAYER_LEVEL_GEOMETRY{1};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PROP{2};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PHYSICS_OBJECT{3};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PLAYER{4};
inline constexpr phys::CollisionLayer COLLISION_LAYER_CAMERA{5};
inline constexpr phys::CollisionLayer COLLISION_LAYER_COLLECTIBLE{6};

#endif
