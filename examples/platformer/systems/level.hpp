// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_LEVEL_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_LEVEL_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

extern System* const levelSystem;

void loadLevel(phys::EntityRegistry3D& registry, phys::ResourceRegistry3D& resources, gfx::Device& device, gfx::Renderer3D& renderer3D, const Filesystem& filesystem,
	CStringView filepath);

[[nodiscard]] size_t getLevelCollectibleCount(const phys::ResourceRegistry3D& resources);

[[nodiscard]] phys::Box3D getLevelBounds(const phys::ResourceRegistry3D& resources);

[[nodiscard]] const gfx::Sky3D& getLevelSky(const phys::ResourceRegistry3D& resources);

[[nodiscard]] Pair<phys::Position3D, phys::PitchYaw> getLevelPlayerSpawnPositionAndAngles(const phys::ResourceRegistry3D& resources);

#endif
