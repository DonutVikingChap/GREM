// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_PLAYER_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_PLAYER_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

extern System* const playerSystem;
extern System* const playerControlSystem;
extern System* const playerGroundingSystem;

void createPlayer(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, const Filesystem& filesystem, CStringView configurationFilepath, phys::Position3D position,
	phys::PitchYaw angles, Optional<uint32_t> controllerID);

[[nodiscard]] bool isPlayer(const phys::EntityRegistry3D& registry, phys::EntityID entityID);

struct PlayerCameraView {
	phys::Position3D position;
	phys::PitchYaw angles;
	phys::LinearVelocity3D linearVelocity;
};
[[nodiscard]] PlayerCameraView getPlayerCameraView(const phys::EntityRegistry3D& registry, phys::EntityID entityID, float tickInterpolationAlpha);

void handlePlayerEvent(phys::EntityRegistry3D& registry, const evt::Event& event, bool hasControl);

void teleportPlayer(phys::EntityRegistry3D& registry, const phys::ResourceRegistry3D& resources, phys::EntityID entityID, phys::Position3D newPosition, phys::PitchYaw newAngles,
	phys::LinearVelocity3D newLinearVelocity);

void givePlayerCollectible(phys::EntityRegistry3D& registry, const phys::ResourceRegistry3D& resources, phys::EntityID entityID);

void resetAllPlayerCollectibles(phys::EntityRegistry3D& registry);

#endif
