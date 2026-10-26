// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_PLAYER_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_PLAYER_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

enum class PlayerAction : uint8_t {
	CONFIRM,
	CANCEL,
	MOVE_FORWARD,
	MOVE_BACKWARD,
	MOVE_LEFT,
	MOVE_RIGHT,
	AIM_UP,
	AIM_DOWN,
	AIM_LEFT,
	AIM_RIGHT,
	LOOK_UP,
	LOOK_DOWN,
	LOOK_LEFT,
	LOOK_RIGHT,
	SPRINT,
	GROUND_POUND,
	JUMP,
	ATTACK,
	ZOOM_IN,
	ZOOM_OUT,
	TOGGLE_FLYING,
	INTERACT,
	IGNITION,
	GAS,
	GAS_SMOOTH,
	BRAKE,
	BRAKE_SMOOTH,
	HANDBRAKE,
	STEER_LEFT,
	STEER_LEFT_SMOOTH,
	STEER_RIGHT,
	STEER_RIGHT_SMOOTH,
	STEER_WITH_AIM,
	CLUTCH,
	CLUTCH_SMOOTH,
	GEAR_SHIFT_UP,
	GEAR_SHIFT_DOWN,
	GEAR_SHIFT_WITH_AIM,
};

extern System* const playerSystem;
extern System* const playerControlSystem;
extern System* const playerGroundingSystem;

void createPlayer(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, const Filesystem& filesystem, CStringView configurationFilepath, phys::Position3D position,
	phys::PitchYaw angles, Optional<uint32_t> controllerID);

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

// Components:

struct PlayerTag {};

struct PlayerInput : evt::InputManager {
	struct Settings {
		phys::Scale2D aimSensitivity{1.0f, 1.0f};
		phys::PitchYawRates lookSensitivity{200_degrees_per_second};
		phys::Frequency gasSmoothDecayRate = 3_Hertz;
		phys::Frequency brakeSmoothDecayRate = 3_Hertz;
		phys::Frequency clutchSmoothDecayRate = 5_Hertz;
		phys::Frequency steeringSmoothDecayRate = 6_Hertz;
		phys::Coefficient steeringFromAimSensitivity = 0.5_x;
		phys::Coefficient shiftFromAimSensitivity = 10_x;
		bool invertSteerWithAim = false;
	};

	Settings settings{};
	Optional<uint32_t> controllerID;
	bool hasKeyboardControl = !controllerID;

	PlayerInput(const Filesystem& filesystem, CStringView configurationFilepath, Optional<uint32_t> controllerID)
		: evt::InputManager({.emitOutputEvents = true})
		, controllerID(controllerID) {
		loadConfiguration<PlayerAction>(filesystem, configurationFilepath, [&](StringView key, json::Reader& reader) -> void { //
			reader.readAggregateField(key, settings);
		});
	}
};

#endif
