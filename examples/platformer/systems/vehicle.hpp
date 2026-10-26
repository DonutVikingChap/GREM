// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_VEHICLE_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_VEHICLE_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

extern System* const vehicleSystem;

void createVehicle(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, phys::Position3D position, phys::Orientation3D orientation, phys::Scale3D scale,
	const String& typeName);

void enterVehicle(phys::EntityRegistry3D& registry, phys::EntityID occupantEntityID, phys::EntityID vehicleEntityID);
void exitVehicle(phys::EntityRegistry3D& registry, phys::EntityID occupantEntityID);

// Components:

struct VehicleTag {};

struct VehicleOccupant {
	phys::EntityID vehicleEntityID{};
	phys::CollisionLayers oldCollisionResponseLayers;
	bool oldVisibility;
};

struct VehicleDriverTag {};

#endif
