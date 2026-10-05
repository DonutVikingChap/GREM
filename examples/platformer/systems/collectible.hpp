// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_COLLECTIBLE_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_COLLECTIBLE_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

extern System* const collectibleSystem;

void createCollectible(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, phys::Position3D position);

#endif
