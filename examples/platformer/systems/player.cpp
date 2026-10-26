// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "player.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Audio.hpp"
#include "../Graphics.hpp"
#include "../System.hpp"
#include "../physics.hpp"
#include "level.hpp"
#include "object.hpp"
#include "vehicle.hpp"

#include <utility> // std::move

namespace {

constexpr phys::CollisionFilter PLAYER_COLLISION_FILTER{
	.layers = COLLISION_LAYER_PLAYER,
};

constexpr phys::CollisionFilter PLAYER_INTERACTION_COLLISION_FILTER{
	.layers = COLLISION_LAYER_DEFAULT,
	.detectionLayers = COLLISION_LAYER_VEHICLE,
	.responseLayers{},
};

constexpr phys::CollisionFilter PLAYER_CAMERA_COLLISION_FILTER{
	.layers = COLLISION_LAYER_CAMERA,
	.detectionLayers = ~(COLLISION_LAYER_PLAYER | COLLISION_LAYER_PHYSICS_OBJECT),
	.responseLayers = ~(COLLISION_LAYER_PLAYER | COLLISION_LAYER_PHYSICS_OBJECT),
};

struct PlayerSettingsJSON {
	static constexpr CStringView FILEPATH = "schema/player.json";

	struct Model {
		String filepath{};
		vec3 rootTranslation{};
		quat rootRotation{0.0f, 0.0f, 0.0f, 1.0f};
		vec3 rootScale{1.0f};
	} model{};

	struct Interaction {
		phys::Distance range = 2_meters;
		phys::Distance radius = 1_meters;
	} interaction{};

	struct Dynamics {
		phys::Mass mass = 75_kilograms;
		phys::Acceleration gravity = 9.82_meters_per_second_squared;
	} dynamics{};

	struct Dimensions {
		phys::Distance radius = 0.5_meters;
		phys::Distance height = 1.8_meters;
	} dimensions{};

	struct Step {
		struct Sound {
			String filepath{};
			float volume = 1.0f;
		};

		ArrayList<Sound> sounds{};
		phys::Distance height = 0.3_meters;
	} step{};

	struct Turning {
		phys::Frequency yawDecayRate = 8_Hertz;
	} turning{};

	struct Flying {
		phys::Speed minSpeed = 1_meter_per_second;
		phys::Speed baseSpeed = 20_meters_per_second;
		phys::Speed sprintSpeed = 50_meters_per_second;
		phys::Time accelerationDuration = 0.1_seconds;
		bool allowed = false;
	} flying{};

	struct Grounded {
		phys::Angle maxSlopeAngle = 45_degrees;
		phys::Speed minSpeed = 1_meter_per_second;
		phys::Speed baseSpeed = 10_meters_per_second;
		phys::Speed sprintSpeed = 50_meters_per_second;
		phys::Time accelerationDuration = 0.3_seconds;
	} grounded{};

	struct Air {
		phys::Speed speed = 0.5_meters_per_second;
		phys::Acceleration acceleration = 100_meters_per_second_squared;
	} air{};

	struct Jump {
		struct Sound {
			String filepath{};
			float volume = 1.0f;
		} sound{};

		phys::Acceleration gravityMin = 20_meters_per_second_squared;
		phys::Acceleration gravityMax = 50_meters_per_second_squared;
		phys::Distance height = 3_meters;
		phys::Time regroundDelay = 100_milliseconds;
		bool allowed = true;
	} jump{};

	struct DoubleJump {
		phys::Time jumpTimeOffset = 0.1_seconds;
		bool allowed = true;
	} doubleJump{};

	struct GroundPound {
		phys::Speed speed = 10_meters_per_second;
		phys::Coefficient horizontalCoefficient = 0.2_x;
		phys::Time jumpTimeOffset = 0.25_seconds;
		bool allowed = true;
	} groundPound{};

	struct Attack {
		phys::Coefficient speedCoefficient = 0.25_x;
		phys::Time interval = 0.6_seconds;
		bool allowed = true;
	} attack{};

	struct Camera {
		phys::Distance minDistance = 0.8_meters;
		phys::Distance maxDistance = 10_meters;
		phys::Distance minDistanceInVehicle = 3_meters;
		phys::Distance maxDistanceInVehicle = 15_meters;
		phys::Distance targetDistanceAtZeroSpeed = 7_meters;
		phys::Distance targetDistanceAtBaseSpeed = 5_meters;
		phys::Distance targetDistanceAtZeroSpeedInVehicle = 8_meters;
		phys::Distance targetDistanceAtBaseSpeedInVehicle = 7_meters;
		phys::Coefficient targetDistanceMaxAffectingCoefficientOfBaseSpeed = 1.5_x;
		phys::Coefficient targetDistanceMaxAffectingCoefficientOfBaseSpeedInVehicle = 1.5_x;
		phys::Coefficient verticalTargetOffsetHeightCoefficient = 0.3_x;
		phys::Frequency verticalTargetPositionDecayRate = 5_Hertz;
		phys::Coefficient yawMinHorizontalDirectionScale = 0.5_x;
		phys::Frequency yawDecayRateAtBaseSpeed = 1.25_Hertz;
		phys::Frequency yawDecayRateAtBaseSpeedInVehicle = 3_Hertz;
		phys::Coefficient yawDecayRateMaxAffectingCoefficientOfBaseSpeed = 1.25_x;
		phys::Coefficient yawDecayRateMaxAffectingCoefficientOfBaseSpeedInVehicle = 1.5_x;
		phys::Time yawFollowCooldownAfterAdjustment = 1_second;
		phys::Time yawFollowCooldownAfterAdjustmentInVehicle = 1_seconds;
		phys::Time yawFollowCooldownSmoothingTime = 0.5_seconds;
		phys::Time yawFollowCooldownSmoothingTimeInVehicle = 0.95_seconds;
		phys::Angle pitchAtZeroSpeed = -40_degrees;
		phys::Angle pitchAtBaseSpeed = -15_degrees;
		phys::Angle pitchAtZeroSpeedInVehicle = -40_degrees;
		phys::Angle pitchAtBaseSpeedInVehicle = -20_degrees;
		phys::Frequency pitchDecayRate = 0.5_Hertz;
		phys::Frequency pitchDecayRateInVehicle = 2_Hertz;
		phys::Frequency smoothedSpeedDecayRate = 2_Hertz;
		phys::Coefficient collisionRadiusCoefficient = 0.8_x;
	} camera{};

	struct Animation {
		phys::Time transitionDuration = 0.25_seconds;
		phys::Speed walkingSpeed = 1.5_meters_per_second;
		phys::Speed runningSpeed = 10_meters_per_second;
		phys::Speed runningTimeSpeedupMaxSpeed = 15_meters_per_second;
		phys::Coefficient footstepThresholdOffset = 0.22_x;
		phys::Coefficient jumpAmountReductionAtRunningSpeed = 0.25_x;
		phys::Frequency angerDecayRate = 2_Hertz;
		phys::Time collectibleCollectionDuration = 300_milliseconds;
	} animation{};
};

// Resources:

struct PlayerSettings : PlayerSettingsJSON {
	phys::CapsuleShape3D collisionShape{.radius = dimensions.radius, .halfLength = (dimensions.height - step.height) * 0.5_x - dimensions.radius};
	phys::Length1D verticalFeetOffset = (dimensions.height - step.height) * -0.5_x - step.height;
	phys::Length3D feetOffset{0, verticalFeetOffset, 0};
	phys::Speed jumpSpeed = sqrt(2_x * jump.gravityMin * jump.height);
	phys::Time jumpDuration = 2_x * jumpSpeed / jump.gravityMin;
	phys::Length1D cameraVerticalTargetOffset = (dimensions.height - step.height) * camera.verticalTargetOffsetHeightCoefficient;
	phys::SphereShape3D cameraCollisionShape{.radius = (cameraVerticalTargetOffset - verticalFeetOffset) * camera.collisionRadiusCoefficient};

	explicit PlayerSettings(PlayerSettingsJSON&& settingsJSON)
		: PlayerSettingsJSON(std::move(settingsJSON)) {}
};

struct PlayerModel {
	gfx::Model3D model;
};

struct PlayerSounds {
	ArrayList<aud::Sound> footstepSounds;
	aud::Sound jumpSound;
};

// Components:

struct PlayerInteraction {
	phys::EntityID targetEntityID{};
};

struct PlayerMovement {
	phys::AbsoluteAngle headingYaw;
	phys::Scale3D desiredDirectionScale{};
	phys::Speed desiredSpeed{};
	phys::Time attackCooldown{};
	phys::Time timeSinceAttack{};
	phys::Time timeSinceJump{};
	phys::Time timeSinceGrounded{};
	phys::Time timeOnGround{};
	Optional<phys::Direction3D> groundNormal{};
	phys::Coefficient groundFriction = 1_x;
	bool alreadyAttacked = false;
	bool alreadyJumped = false;
	bool doubleJumped = false;
	bool groundPounded = false;
	bool flying = false;
};

struct PlayerCamera {
	phys::Speed smoothedSpeed{};
	phys::Speed smoothedHorizontalSpeed{};
	phys::Direction2D horizontalDirection;
	phys::Scale2D horizontalDirectionScale = horizontalDirection;
	phys::Time horizontalDirectionFollowCooldown{};
	phys::Position1D verticalTargetPosition;
	phys::Position1D previousVerticalTargetPosition = verticalTargetPosition;
	phys::PitchYaw angles;
	phys::PitchYaw previousAngles = angles;
	phys::Distance distance;
	phys::Distance previousDistance = distance;
	phys::AbsoluteAngle desiredPitch{};
};

struct PlayerAnimation {
	res::Model::AnimationIndex idleAnimationIndex;
	res::Model::AnimationIndex walkingAnimationIndex;
	res::Model::AnimationIndex runningAnimationIndex;
	res::Model::AnimationIndex jumpAnimationIndex;
	res::Model::AnimationIndex attackAnimationIndex;
	phys::Time runningAnimationDuration;
	phys::Time jumpAnimationDuration;
	phys::Time attackAnimationDuration;
	phys::Time idleAnimationTime{};
	phys::Time runningAnimationTime{};
	phys::Coefficient movingAmount{};
	phys::Coefficient angerAmount{};
	rng::Xoroshiro128PlusPlusEngine footstepSoundIndexGenerator{};
};

struct PlayerInventory {
	size_t collectibles = 0;
	phys::Time collectAnimationCooldown{};
};

// Functions:

void ground(PlayerMovement& movement, Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds, phys::Position3D position, phys::Direction3D normal,
	phys::Coefficient groundFriction) {
	if (!movement.groundNormal && movement.timeSinceGrounded >= settings.jump.regroundDelay && !sounds.footstepSounds.empty()) {
		audio.soundStage.play3DSound(sounds.footstepSounds.front(), (position + settings.feetOffset).in(phys::METERS), {});
	}

	movement.groundNormal = normal;
	movement.groundFriction = groundFriction;
	movement.timeSinceGrounded = {};
	movement.doubleJumped = false;
	movement.groundPounded = false;
}

void unground(PlayerMovement& movement) {
	movement.timeOnGround = {};
	movement.groundNormal.reset();
	movement.groundFriction = 1_x;
}

void jump(PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds, phys::Position3D position) {
	audio.soundStage.play3DSound(sounds.jumpSound, (position + settings.feetOffset).in(phys::METERS), {});

	linearVelocity.setY(settings.jumpSpeed);
	movement.timeSinceJump = {};
	movement.alreadyJumped = true;

	unground(movement);
}

void doubleJump(PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, phys::Scale3D desiredDirectionScale, Audio& audio, const PlayerSettings& settings,
	const PlayerSounds& sounds, phys::Position3D position) {
	const aud::SoundInstanceID soundInstanceID = audio.soundStage.createPaused3DSound(sounds.jumpSound, (position + settings.feetOffset).in(phys::METERS), vec3{});
	audio.soundStage.setSoundPlaybackSpeed(soundInstanceID, 3.0f);
	audio.soundStage.resumeSound(soundInstanceID);

	const phys::Scale2D horizontalDesiredDirectionScale = clampLength(desiredDirectionScale.get(phys::X, phys::Z), 1_x);
	const phys::LinearVelocity2D horizontalVelocity = linearVelocity.get(phys::X, phys::Z);
	const phys::LinearVelocity1D velocityInHorizontalDesiredDirection = dot(horizontalVelocity, horizontalDesiredDirectionScale);
	const phys::LinearVelocity2D newHorizontalVelocity = horizontalDesiredDirectionScale * max(settings.jumpSpeed, velocityInHorizontalDesiredDirection);
	linearVelocity = {
		newHorizontalVelocity.getX(),
		settings.jumpSpeed,
		newHorizontalVelocity.getY(),
	};
	movement.timeSinceJump = settings.doubleJump.jumpTimeOffset;
	movement.alreadyJumped = true;
	movement.doubleJumped = true;

	unground(movement);
}

void groundPound(PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds,
	phys::Position3D position) {
	const aud::SoundInstanceID soundInstanceID = audio.soundStage.createPaused3DSound(sounds.jumpSound, (position + settings.feetOffset).in(phys::METERS), vec3{});
	audio.soundStage.setSoundPlaybackSpeed(soundInstanceID, 0.5f);
	audio.soundStage.resumeSound(soundInstanceID);

	linearVelocity = {
		linearVelocity.getX() * settings.groundPound.horizontalCoefficient,
		linearVelocity.getY() - settings.groundPound.speed,
		linearVelocity.getZ() * settings.groundPound.horizontalCoefficient,
	};
	movement.timeSinceJump = settings.groundPound.jumpTimeOffset;
	movement.groundPounded = true;
}

void groundMove(const PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, phys::LinearAcceleration3D& gravityAcceleration, const PlayerSettings& settings,
	phys::Time deltaTime) {
	const phys::Direction3D groundNormal = movement.groundNormal.value_or(phys::Y_AXIS_3D);
	const phys::Scale2D horizontalDesiredDirectionScale = movement.desiredDirectionScale.get(phys::X, phys::Z);
	const phys::Scale1D horizontalDesiredDirectionScaleMagnitude = length(horizontalDesiredDirectionScale);
	const phys::Scale3D desiredDirection = tryDivide(horizontalDesiredDirectionScale, horizontalDesiredDirectionScaleMagnitude).value_or(phys::Scale2D{}).get(phys::X, 0, phys::Y);
	const Optional<phys::Direction3D> desiredDirectionAlongGround = tryNormalize(cross(groundNormal, cross(desiredDirection, groundNormal)));
	const phys::Scale3D targetDirectionScale =
		(desiredDirectionAlongGround) ? min(horizontalDesiredDirectionScaleMagnitude, 1_x) * dot(*desiredDirectionAlongGround, desiredDirection) * desiredDirection
		                              : phys::Scale3D{};
	const phys::LinearVelocity3D targetVelocity = targetDirectionScale * movement.desiredSpeed * ((movement.attackCooldown > 0) ? settings.attack.speedCoefficient : 1_x);
	const phys::Speed currentSpeed = length(linearVelocity);
	const phys::Coefficient amountOfRemainingSpeedAdded = movement.groundFriction * deltaTime / settings.grounded.accelerationDuration;
	const phys::LinearVelocity1D potentialSpeedDelta = (length(targetVelocity) - currentSpeed) * amountOfRemainingSpeedAdded;
	if ((targetVelocity == 0 && currentSpeed < settings.grounded.minSpeed) || currentSpeed + potentialSpeedDelta <= 0) {
		linearVelocity = {};
	} else {
		const phys::LinearVelocity3D velocityRemaining = targetVelocity - linearVelocity;
		const phys::LinearVelocity3D addedVelocity = velocityRemaining * amountOfRemainingSpeedAdded;
		linearVelocity += addedVelocity;
	}
	gravityAcceleration = {0, -settings.dynamics.gravity, 0};
}

void airMove(const PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, phys::LinearAcceleration3D& gravityAcceleration, const PlayerSettings& settings,
	phys::Time deltaTime) {
	const phys::Scale2D horizontalDesiredDirectionScale = movement.desiredDirectionScale.get(phys::X, phys::Z);
	const phys::Scale1D horizontalDesiredDirectionScaleMagnitude = length(horizontalDesiredDirectionScale);
	const phys::Scale2D desiredDirection = tryDivide(horizontalDesiredDirectionScale, horizontalDesiredDirectionScaleMagnitude).value_or(phys::Scale2D{});
	const phys::LinearVelocity1D currentVelocityInDesiredDirection = dot(linearVelocity.get(phys::X, phys::Z), desiredDirection);
	const phys::LinearVelocity1D velocityRemainingInDesiredDirection = settings.air.speed - currentVelocityInDesiredDirection;
	if (velocityRemainingInDesiredDirection > 0) {
		const phys::Speed addedSpeed = min(settings.air.acceleration * deltaTime, velocityRemainingInDesiredDirection);
		const phys::LinearVelocity2D addedVelocity = min(horizontalDesiredDirectionScaleMagnitude, 1_x) * addedSpeed * desiredDirection;
		linearVelocity += addedVelocity.get(phys::X, 0, phys::Y);
	}
	if (!movement.groundNormal && movement.timeSinceJump <= settings.jumpDuration) {
		gravityAcceleration = {0, (movement.desiredDirectionScale.getY() > 0.5_x || linearVelocity.getY() < 0) ? -settings.jump.gravityMin : -settings.jump.gravityMax, 0};
	} else {
		gravityAcceleration = {0, -settings.dynamics.gravity, 0};
	}
}

void flyMove(PlayerMovement& movement, phys::LinearVelocity3D& linearVelocity, phys::LinearAcceleration3D& gravityAcceleration, const PlayerSettings& settings,
	phys::Time deltaTime) {
	const phys::LinearVelocity3D targetVelocity = clampLength(movement.desiredDirectionScale, 1_x) * movement.desiredSpeed;
	const phys::Speed currentSpeed = length(linearVelocity);
	const phys::LinearVelocity1D potentialSpeedDelta = (length(targetVelocity) - currentSpeed) * (deltaTime / settings.flying.accelerationDuration);
	if ((targetVelocity == 0 && currentSpeed < settings.flying.minSpeed) || currentSpeed + potentialSpeedDelta <= 0) {
		linearVelocity = {};
	} else {
		const phys::LinearVelocity3D velocityRemaining = targetVelocity - linearVelocity;
		const phys::LinearVelocity3D addedVelocity = velocityRemaining * (deltaTime / settings.flying.accelerationDuration);
		linearVelocity += addedVelocity;
	}
	gravityAcceleration = {};
	movement.timeSinceJump = {};
	movement.timeOnGround = {};
	movement.groundNormal.reset();
}

void setCameraPosition(PlayerCamera& camera, phys::Position3D newPosition, phys::Position3D targetPosition) {
	const phys::Length3D newOffset = newPosition - targetPosition;
	const phys::Distance newDistance = length(newOffset);
	camera.distance = newDistance;
	if (newDistance > phys::Distance::MACHINE_EPSILON) {
		const phys::Direction3D newDirection = phys::Direction3D::reinterpret(-newOffset / newDistance);
		const phys::PitchYaw newAngles = convertForwardDirectionToAngles(newDirection);
		const phys::AbsoluteAngle newYaw = camera.angles.getY() + getAngleDifference(camera.angles.getY(), newAngles.getY());
		camera.angles = {newAngles.getX(), newYaw};
		camera.horizontalDirection = flipY(rotate90DegreesCounterclockwise(convertAnglesToForwardDirection(camera.angles.getY())));
		camera.horizontalDirectionScale = camera.horizontalDirection * length(camera.horizontalDirectionScale);
	}
}

// Systems:

void animatePlayerInventories(exec::Entities<PlayerInventory> playerEntities, const app::FrameInfo& frameInfo) {
	for (auto&& [entityID, inventory] : playerEntities) {
		countdown(inventory.collectAnimationCooldown, frameInfo.deltaTime);
	}
}

void handlePlayerInteractions(phys::EntityRegistry3D& registry, const PlayerSettings& settings, const phys::Broadphase3D& broadphase,
	const phys::SimulationOptions3D& simulationOptions) {
	for (auto&& [entityID, interaction, position, input, camera] : registry.getEntities<PlayerInteraction, const phys::Position3D, const PlayerInput, const PlayerCamera>()) {
		const PlayerCameraView playerCameraView = getPlayerCameraView(registry, entityID, 1.0f);

		const phys::Direction3D direction = convertAnglesToForwardDirection(playerCameraView.angles);
		const phys::Distance distance = camera.distance + settings.interaction.range;

		if (registry.hasComponent<VehicleOccupant>(entityID)) {
			interaction.targetEntityID = {};

			if (input.justPressed(PlayerAction::INTERACT)) {
				exitVehicle(registry, entityID);
			}
		} else {
			const auto predicate = [&](phys::EntityID otherObjectID) -> bool {
				return otherObjectID != entityID;
			};
			phys::Position3D closestPoint{};
			if (const Optional<phys::Broadphase3D::RaycastResult> raycastResult =
					broadphase.raycastClosestHit(phys::Ray3D{.origin = playerCameraView.position, .direction = direction, .maxDistance = distance + settings.interaction.radius},
						PLAYER_INTERACTION_COLLISION_FILTER, registry, phys::CollisionFilterTest::DETECTION, predicate)) {
				interaction.targetEntityID = raycastResult->objectID;
				closestPoint = playerCameraView.position + direction * raycastResult->distance;
			} else {
				interaction.targetEntityID = {};
				phys::SquaredDistance closestSquaredDistance{};
				const phys::CapsuleShape3D shape{.radius = settings.interaction.radius, .halfLength = distance * 0.5_x};
				const phys::Position3D shapePosition = playerCameraView.position + direction * shape.halfLength;
				const phys::Orientation3D shapeOrientation = phys::Orientation3D::lookAt(direction, phys::Y_AXIS_3D) * phys::Orientation3D::pitch(90_degrees);
				broadphase.collide(
					shape, PLAYER_INTERACTION_COLLISION_FILTER, translateRotate(shapePosition, shapeOrientation), registry, simulationOptions.collisionAlgorithmOptions,
					phys::CollisionFilterTest::DETECTION,
					[&](const phys::Broadphase3D::CollisionResult& collision) -> bool {
						for (const phys::ContactPoint3D& point : collision.manifold.points) {
							const phys::Position3D shapePoint = registry.getComponent<phys::Position3D>(collision.objectID) + point.offsets.second;
							const phys::SquaredDistance squaredDistance = distance2(playerCameraView.position, shapePoint);
							if (!interaction.targetEntityID || squaredDistance < closestSquaredDistance) {
								interaction.targetEntityID = collision.objectID;
								closestSquaredDistance = squaredDistance;
								closestPoint = shapePoint;
							}
						}
						return false;
					},
					predicate);
			}

			if (interaction.targetEntityID) {
				if (distance2(position, closestPoint) > length2(settings.interaction.range)) {
					interaction.targetEntityID = {};
				} else if (const Optional<phys::Ray3D> ray = phys::Ray3D::between(position, closestPoint)) {
					if (broadphase.raycastAnyHit(*ray, PLAYER_CAMERA_COLLISION_FILTER, registry, phys::CollisionFilterTest::RESPONSE,
							[&](phys::EntityID otherObjectID) -> bool { return otherObjectID != entityID && otherObjectID != interaction.targetEntityID; })) {
						interaction.targetEntityID = {};
					}
				}
			}

			if (input.justPressed(PlayerAction::INTERACT)) {
				if (registry.hasComponent<VehicleTag>(interaction.targetEntityID)) {
					enterVehicle(registry, entityID, interaction.targetEntityID);
				}
			}
		}
	}
}

void controlPlayers(exec::Entities<PlayerInput, PlayerMovement, PlayerCamera, phys::Orientation3D, phys::LinearVelocity3D, phys::LinearAcceleration3D, const phys::Position3D,
						const phys::InverseMass, const phys::Collider3D>
						playerEntities,
	exec::Entities<const VehicleOccupant> vehicleOccupantEntities, Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds,
	const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, input, movement, camera, orientation, linearVelocity, gravityAcceleration, position, inverseMass, collider] : playerEntities) {
		camera.previousVerticalTargetPosition = camera.verticalTargetPosition;
		camera.previousAngles = camera.angles;
		camera.previousDistance = camera.distance;

		movement.timeSinceAttack += deltaTime;
		movement.timeSinceJump += deltaTime;

		for (const evt::InputManager::OutputEvent& event : input.pollOutputEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::InputManager::OutputMoved& moved) break;
				GREM_CASE(const evt::InputManager::OutputPressed& pressed) {
					switch (static_cast<PlayerAction>(pressed.getOutputIndex())) {
						case PlayerAction::TOGGLE_FLYING:
							if (settings.flying.allowed) {
								movement.flying = !movement.flying;
							}
							break;
						default: break;
					}
					break;
				}
				GREM_CASE(const evt::InputManager::OutputReleased& released) break;
				GREM_CASE_DEFAULT(const auto& other) break;
			}
		}

		const bool isInVehicle = vehicleOccupantEntities.containsEntity(entityID);

		const vec2 cameraAimScale = input.getRelativeState2D(PlayerAction::AIM_DOWN, PlayerAction::AIM_UP, PlayerAction::AIM_RIGHT, PlayerAction::AIM_LEFT).motion;
		const vec2 cameraLookScale = input.getCurrentState2D(PlayerAction::LOOK_DOWN, PlayerAction::LOOK_UP, PlayerAction::LOOK_RIGHT, PlayerAction::LOOK_LEFT).value;
		const float cameraDistanceScale = input.getRelativeState1D(PlayerAction::ZOOM_IN, PlayerAction::ZOOM_OUT).motion;
		const phys::PitchYawRotations angleRotations = cameraAimScale * input.settings.aimSensitivity + cameraLookScale * input.settings.lookSensitivity * deltaTime;
		if (angleRotations != 0 &&
			(!isInVehicle || (input.isPressed(PlayerAction::STEER_WITH_AIM) == input.settings.invertSteerWithAim && !input.isPressed(PlayerAction::GEAR_SHIFT_WITH_AIM)))) {
			const phys::OrthonormalBasis2D yawRotation = rotate(-angleRotations.getY());
			camera.horizontalDirectionScale = yawRotation * camera.horizontalDirectionScale;
			camera.horizontalDirection = yawRotation * camera.horizontalDirection;
			camera.angles += angleRotations;
			camera.horizontalDirectionFollowCooldown = (isInVehicle) ? settings.camera.yawFollowCooldownAfterAdjustmentInVehicle : settings.camera.yawFollowCooldownAfterAdjustment;
		}

		const phys::Length1D minDistance = (isInVehicle) ? settings.camera.minDistanceInVehicle : settings.camera.minDistance;
		const phys::Length1D maxDistance = (isInVehicle) ? settings.camera.maxDistanceInVehicle : settings.camera.maxDistance;
		const phys::Length1D newCameraDistance = camera.distance + cameraDistanceScale * phys::METERS;
		if (newCameraDistance < minDistance) {
			camera.distance = min(camera.distance, minDistance);
		} else {
			camera.distance = min(newCameraDistance, maxDistance);
		}

		if (isInVehicle) {
			movement.desiredDirectionScale = {};
			movement.desiredSpeed = settings.grounded.baseSpeed;
			movement.alreadyAttacked = false;
			movement.alreadyJumped = false;
			countdown(movement.attackCooldown, deltaTime);
			gravityAcceleration = {};
			movement.timeOnGround = {};
			movement.groundNormal.reset();
			movement.headingYaw = camera.angles.getY();
		} else {
			const phys::LinearVelocity2D horizontalVelocity = linearVelocity.get(phys::X, phys::Z);

			const evt::InputManager::ControlState3D movementInput = input.getCurrentState3D( //
				PlayerAction::MOVE_LEFT, PlayerAction::MOVE_RIGHT,                           //
				PlayerAction::MOVE_BACKWARD, PlayerAction::MOVE_FORWARD,                     //
				PlayerAction::GROUND_POUND, PlayerAction::JUMP);
			const phys::Scale2D desiredHorizontalDirectionScale = phys::Orientation2D{0 - camera.angles.getY()}(phys::Scale2D{movementInput.value.x, -movementInput.value.y});
			movement.desiredDirectionScale = {desiredHorizontalDirectionScale.getX(), movementInput.value.z, desiredHorizontalDirectionScale.getY()};
			movement.desiredSpeed = (input.isPressed(PlayerAction::SPRINT)) ? ((movement.flying) ? settings.flying.sprintSpeed : settings.grounded.sprintSpeed)
			                                                                : ((movement.flying) ? settings.flying.baseSpeed : settings.grounded.baseSpeed);

			const bool wantsToAttack = input.isPressed(PlayerAction::ATTACK) || input.justPressed(PlayerAction::ATTACK);
			if (input.justPressed(PlayerAction::ATTACK)) {
				movement.alreadyAttacked = false;
			}

			const bool wantsToJump = input.isPressed(PlayerAction::JUMP) || input.justPressed(PlayerAction::JUMP);
			if (input.justPressed(PlayerAction::JUMP)) {
				movement.alreadyJumped = false;
			}

			const bool wantsToGroundPound = input.isPressed(PlayerAction::GROUND_POUND) || input.justPressed(PlayerAction::GROUND_POUND);

			if (countdownLoop(movement.attackCooldown, deltaTime, settings.attack.interval, wantsToAttack && !movement.alreadyAttacked && settings.attack.allowed)) {
				movement.timeSinceAttack = {};
				movement.alreadyAttacked = true;
			}

			if (movement.flying) {
				flyMove(movement, linearVelocity, gravityAcceleration, settings, deltaTime);
			} else {
				if (wantsToJump && (movement.groundNormal || !movement.doubleJumped) && movement.timeSinceJump >= settings.jump.regroundDelay && !movement.alreadyJumped) {
					if (movement.groundNormal) {
						if (settings.jump.allowed) {
							jump(movement, linearVelocity, audio, settings, sounds, position);
						}
					} else {
						if (settings.doubleJump.allowed) {
							doubleJump(movement, linearVelocity, movement.desiredDirectionScale, audio, settings, sounds, position);
						}
					}
				}

				if (movement.groundNormal) {
					groundMove(movement, linearVelocity, gravityAcceleration, settings, deltaTime);
				} else {
					airMove(movement, linearVelocity, gravityAcceleration, settings, deltaTime);
					if (wantsToGroundPound && !movement.groundPounded && settings.groundPound.allowed) {
						groundPound(movement, linearVelocity, audio, settings, sounds, position);
					}
				}
			}

			const phys::LinearVelocity2D desiredHorizontalVelocity = movement.desiredDirectionScale.get(phys::X, phys::Z) * movement.desiredSpeed;
			if (const Optional<phys::Direction2D> desiredHeading = tryNormalize(midpoint(desiredHorizontalVelocity, horizontalVelocity))) {
				const phys::AbsoluteAngle desiredWrappedYaw = getAngle(rotate90DegreesClockwise(flipY(*desiredHeading)));
				const phys::AbsoluteAngle desiredYaw = movement.headingYaw + getAngleDifference(movement.headingYaw, desiredWrappedYaw);
				movement.headingYaw = expDecay(movement.headingYaw, desiredYaw, settings.turning.yawDecayRate, deltaTime);
			}
			orientation = phys::Orientation3D::yaw(movement.headingYaw);
		}
	}
}

void groundPlayers(
	exec::Entities<PlayerMovement, phys::Position3D, phys::LinearVelocity3D, const phys::LinearAcceleration3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D,
		const phys::Volume, const phys::FluidDensity, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, exec::Exclude<VehicleOccupant>>
		playerEntities,
	exec::Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
	exec::Entities<const phys::Material> materialEntities, Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds, const phys::Broadphase3D& broadphase,
	const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;
	const phys::Scale1D minGroundNormalY = cos(settings.grounded.maxSlopeAngle);

	for (auto&& [entityID, movement, position, linearVelocity, gravityAcceleration, inverseMass, inverseMomentOfInertiaTensor, volume, fluidDensity, orientation, scale, collider] :
		playerEntities) {
		movement.timeSinceGrounded += deltaTime;

		const phys::ConvexShapeView3D castShape{collider.shape};
		const phys::Distance castDistance = settings.step.height * ((movement.groundNormal) ? 2_x : 1_x);
		const phys::Transformation3D castTransformation = translateRotateScale(position, orientation, scale * phys::Scale3D{0.8_x, 1_x, 0.8_x});
		const phys::Direction3D castDirection = -phys::Y_AXIS_3D;
		const Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(castShape, collider.filter, castTransformation, castDirection,
			castDistance, objectEntities, simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
			[&](phys::EntityID otherObjectID) -> bool { return otherObjectID != entityID; });

		if (!shapecastResult || shapecastResult->distance == 0) {
			unground(movement);
			continue;
		}

		const phys::Length3D step{0, settings.step.height - shapecastResult->distance, 0};
		const phys::Direction3D normal = shapecastResult->normal;
		if (linearVelocity.getY() > phys::Speed::MACHINE_EPSILON || normal.getY() < minGroundNormalY) {
			const phys::Distance penetrationDepth = step.getY() * normal.getY();
			if (penetrationDepth > 0) {
				position += penetrationDepth * normal;
				linearVelocity -= dot(linearVelocity, normal) * normal;
			}
			unground(movement);
			continue;
		}

		position += step;
		linearVelocity.setY(0);

		const phys::Coefficient groundFriction = clamp(unlerp(materialEntities.getComponent<phys::Material>(shapecastResult->objectID).kineticFriction, 0_x, 0.707_x), 0_x, 1_x);
		ground(movement, audio, settings, sounds, position, normal, groundFriction);
		movement.timeOnGround += deltaTime;
	}
}

void adjustPlayerCameras(exec::Entities<PlayerCamera, const PlayerMovement, const phys::Position3D, const phys::LinearVelocity3D> playerEntities,
	exec::Entities<const VehicleOccupant> vehicleOccupantEntities,
	exec::Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
	const PlayerSettings& settings, const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, camera, movement, position, linearVelocity] : playerEntities) {
		const VehicleOccupant* const vehicleOccupant = vehicleOccupantEntities.findComponent<VehicleOccupant>(entityID);
		const phys::Position3D* const vehiclePosition = (vehicleOccupant) ? objectEntities.findComponent<phys::Position3D>(vehicleOccupant->vehicleEntityID) : nullptr;
		const bool isInVehicle = vehicleOccupant != nullptr;

		const phys::Speed speed = length(linearVelocity);
		camera.smoothedSpeed = expDecay(camera.smoothedSpeed, speed, settings.camera.smoothedSpeedDecayRate, deltaTime);

		const phys::LinearVelocity2D horizontalVelocity = linearVelocity.get(phys::X, phys::Z);
		const phys::Speed horizontalSpeed = length(horizontalVelocity);
		camera.smoothedHorizontalSpeed = expDecay(camera.smoothedHorizontalSpeed, horizontalSpeed, settings.camera.smoothedSpeedDecayRate, deltaTime);

		const phys::LinearVelocity1D verticalVelocity = linearVelocity.getY();
		const phys::Position3D desiredTargetPosition = position + phys::Length3D{0, settings.cameraVerticalTargetOffset, 0};
		if (movement.groundNormal || movement.flying || movement.doubleJumped || movement.timeSinceJump > settings.jumpDuration ||
			abs(camera.verticalTargetPosition - desiredTargetPosition.getY()) > settings.jump.height) {
			camera.verticalTargetPosition = expDecay(camera.verticalTargetPosition, desiredTargetPosition.getY(), settings.camera.verticalTargetPositionDecayRate, deltaTime);
		}

		if (const Optional<phys::Scale2D> horizontalDirection = tryDivide(horizontalVelocity, horizontalSpeed)) {
			camera.horizontalDirection = phys::Direction2D::reinterpret(*horizontalDirection);
		}

		const phys::Time yawFollowCooldownSmoothingTime = (isInVehicle) ? settings.camera.yawFollowCooldownSmoothingTimeInVehicle : settings.camera.yawFollowCooldownSmoothingTime;
		countdown(camera.horizontalDirectionFollowCooldown, deltaTime);
		if (camera.horizontalDirectionFollowCooldown <= yawFollowCooldownSmoothingTime) {
			const phys::Coefficient yawDecayRateSmoothingCoefficient = 1_x - camera.horizontalDirectionFollowCooldown / yawFollowCooldownSmoothingTime;
			const phys::Frequency yawDecayRateAtBaseSpeed = (isInVehicle) ? settings.camera.yawDecayRateAtBaseSpeedInVehicle : settings.camera.yawDecayRateAtBaseSpeed;
			const phys::Coefficient yawDecayRateMaxAffectingCoefficientOfBaseSpeed =
				((isInVehicle) ? settings.camera.yawDecayRateMaxAffectingCoefficientOfBaseSpeedInVehicle : settings.camera.yawDecayRateMaxAffectingCoefficientOfBaseSpeed);
			const phys::Frequency yawDecayRate =
				yawDecayRateSmoothingCoefficient * yawDecayRateAtBaseSpeed * min(horizontalSpeed / settings.grounded.baseSpeed, yawDecayRateMaxAffectingCoefficientOfBaseSpeed);
			const phys::Scale2D newHorizontalDirectionScale = expDecay(camera.horizontalDirectionScale, camera.horizontalDirection, yawDecayRate, deltaTime);
			if (const Optional<phys::Direction2D> newHorizontalDirection = tryNormalize(newHorizontalDirectionScale)) {
				camera.horizontalDirectionScale =
					(length2(newHorizontalDirectionScale) < length2(settings.camera.yawMinHorizontalDirectionScale))
				        ? *newHorizontalDirection * settings.camera.yawMinHorizontalDirectionScale
				        : newHorizontalDirectionScale;
				camera.horizontalDirection = *newHorizontalDirection;

				const phys::AbsoluteAngle newWrappedYaw = getAngle(rotate90DegreesClockwise(flipY(*newHorizontalDirection)));
				const phys::AbsoluteAngle newYaw = camera.angles.getY() + getAngleDifference(camera.angles.getY(), newWrappedYaw);
				camera.angles.setY(newYaw);
			}
		}

		if (movement.groundNormal || movement.flying || movement.timeSinceJump > settings.jumpDuration) {
			const phys::Speed desiredHorizontalSpeed = length(movement.desiredDirectionScale.get(phys::X, phys::Z) * movement.desiredSpeed);
			const phys::Speed verticalSpeed = abs(verticalVelocity);
			if (isInVehicle || desiredHorizontalSpeed > settings.grounded.minSpeed || verticalSpeed > settings.grounded.minSpeed) {
				const phys::LinearVelocity1D velocityInHorizontalDirection = dot(horizontalVelocity, camera.horizontalDirection);
				const phys::Angle pitchAtZeroSpeed = (isInVehicle) ? settings.camera.pitchAtZeroSpeedInVehicle : settings.camera.pitchAtZeroSpeed;
				const phys::Angle pitchAtBaseSpeed = (isInVehicle) ? settings.camera.pitchAtBaseSpeedInVehicle : settings.camera.pitchAtBaseSpeed;
				const phys::Angle pitchOffset = lerp(pitchAtZeroSpeed, pitchAtBaseSpeed, clamp(velocityInHorizontalDirection / settings.grounded.baseSpeed, 0_x, 1_x));
				camera.desiredPitch = (horizontalSpeed > phys::Speed::MACHINE_EPSILON)
				                          ? phys::Angle{clamp(atan2(verticalVelocity, horizontalSpeed) + pitchOffset, -90_degrees, 90_degrees)}
				                          : phys::Angle{(verticalSpeed > settings.grounded.minSpeed) ? copysign(90_degrees, verticalVelocity) : pitchOffset};
			}
			const phys::Frequency pitchDecayRate = (isInVehicle) ? settings.camera.pitchDecayRateInVehicle : settings.camera.pitchDecayRate;
			const phys::AbsoluteAngle newPitch = expDecay(camera.angles.getX(), camera.desiredPitch,
				min(max(camera.smoothedHorizontalSpeed, verticalSpeed), settings.grounded.baseSpeed) * (pitchDecayRate / settings.grounded.baseSpeed), deltaTime);
			camera.angles.setX(clamp(newPitch, phys::AbsoluteAngle{-90_degrees}, phys::AbsoluteAngle{90_degrees}));
		}

		const phys::Distance targetDistanceAtZeroSpeed = (isInVehicle) ? settings.camera.targetDistanceAtZeroSpeedInVehicle : settings.camera.targetDistanceAtZeroSpeed;
		const phys::Distance targetDistanceAtBaseSpeed = (isInVehicle) ? settings.camera.targetDistanceAtBaseSpeedInVehicle : settings.camera.targetDistanceAtBaseSpeed;
		const phys::Coefficient targetDistanceMaxAffectingCoefficientOfBaseSpeed =
			(isInVehicle) ? settings.camera.targetDistanceMaxAffectingCoefficientOfBaseSpeedInVehicle : settings.camera.targetDistanceMaxAffectingCoefficientOfBaseSpeed;
		const phys::Distance targetDistance =
			lerp(targetDistanceAtZeroSpeed, targetDistanceAtBaseSpeed, min(camera.smoothedSpeed / settings.grounded.baseSpeed, targetDistanceMaxAffectingCoefficientOfBaseSpeed));
		camera.distance = expDecay(camera.distance, targetDistance, camera.smoothedSpeed / 5_meters, deltaTime);
		const phys::Position3D targetPosition = (vehiclePosition) ? *vehiclePosition : phys::Position3D{position.getX(), camera.verticalTargetPosition, position.getZ()};

		const phys::EntityID vehicleEntityID = (vehicleOccupant) ? vehicleOccupant->vehicleEntityID : phys::EntityID{};
		const auto predicate = [&](phys::EntityID otherEntityID) -> bool {
			return otherEntityID != entityID && otherEntityID != vehicleEntityID;
		};

		phys::Position3D cameraPosition = targetPosition - convertAnglesToForwardDirection(camera.angles) * camera.distance;
		broadphase.collide(
			settings.cameraCollisionShape, PLAYER_CAMERA_COLLISION_FILTER, translate(cameraPosition), objectEntities, simulationOptions.collisionAlgorithmOptions,
			phys::CollisionFilterTest::RESPONSE,
			[&](const phys::Broadphase3D::CollisionResult& collision) -> bool {
				const phys::Position3D positionB = objectEntities.getComponent<phys::Position3D>(collision.objectID);
				const phys::Orientation3D orientationB = objectEntities.getComponent<phys::Orientation3D>(collision.objectID);
				const phys::Scale3D scaleB = objectEntities.getComponent<phys::Scale3D>(collision.objectID);
				for (const phys::ContactPoint3D& point : collision.manifold.points) {
					const phys::Position3D pointA = cameraPosition + point.localOffsets.first;
					const phys::Position3D pointB = positionB + orientationB(scaleB * point.localOffsets.second);
					const phys::Length1D penetrationDepth = dot(pointA - pointB, collision.manifold.normal);
					if (penetrationDepth > 0) {
						cameraPosition -= penetrationDepth * collision.manifold.normal;
						setCameraPosition(camera, cameraPosition, targetPosition);
					}
				}
				return false;
			},
			predicate);

		const phys::Direction3D castDirection = -convertAnglesToForwardDirection(camera.angles);
		if (const Optional<phys::Broadphase3D::RaycastResult> raycastResult =
				broadphase.raycastClosestHit(phys::Ray3D{.origin = desiredTargetPosition, .direction = castDirection, .maxDistance = camera.distance},
					PLAYER_CAMERA_COLLISION_FILTER, objectEntities, phys::CollisionFilterTest::RESPONSE, predicate)) {
			if (const Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(settings.cameraCollisionShape, PLAYER_CAMERA_COLLISION_FILTER,
					translate(desiredTargetPosition), castDirection, camera.distance, objectEntities, simulationOptions.collisionAlgorithmOptions,
					phys::CollisionFilterTest::RESPONSE, predicate)) {
				camera.distance = shapecastResult->distance;
			} else {
				camera.distance = raycastResult->distance - settings.cameraCollisionShape.radius;
			}
			if (signbit(camera.distance)) {
				camera.distance = -camera.distance;
				camera.angles.setY(camera.angles.getY() + 180_degrees);
				camera.horizontalDirectionScale = -camera.horizontalDirectionScale;
				camera.horizontalDirection = -camera.horizontalDirection;
			}
			camera.verticalTargetPosition = desiredTargetPosition.getY();
		}
	}
}

void animatePlayers(
	exec::Entities<ObjectAnimationState, PlayerAnimation, const phys::Position3D, const phys::LinearVelocity3D, const PlayerMovement, exec::Exclude<VehicleOccupant>>
		playerEntities,
	Audio& audio, const PlayerSettings& settings, const PlayerSounds& sounds, const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, animationState, animation, position, linearVelocity, movement] : playerEntities) {
		animationState.previousAnimationLayers = animationState.animationLayers;
		animationState.previousMorphTargetWeights = animationState.morphTargetWeights;

		const phys::LinearVelocity2D horizontalVelocity = linearVelocity.get(phys::X, phys::Z);
		const phys::Speed horizontalSpeed = length(horizontalVelocity);

		const phys::Coefficient runningAnimationTimeSpeedup = min(horizontalSpeed, settings.animation.runningTimeSpeedupMaxSpeed) * (2_x / settings.animation.runningSpeed);
		const phys::Time oldWrappedRunningAnimationTime = wrap(animation.runningAnimationTime, animation.runningAnimationDuration * 0.5_x);
		animation.runningAnimationTime += deltaTime * runningAnimationTimeSpeedup;

		const phys::Time newWrappedRunningAnimationTime = wrap(animation.runningAnimationTime, animation.runningAnimationDuration * 0.5_x);
		const phys::Time footstepThreshold = animation.runningAnimationDuration * settings.animation.footstepThresholdOffset;
		const bool midAttack = movement.attackCooldown > 0;
		if (movement.groundNormal && !midAttack && oldWrappedRunningAnimationTime < footstepThreshold && newWrappedRunningAnimationTime >= footstepThreshold &&
			!sounds.footstepSounds.empty()) {
			const size_t footstepSoundIndex = static_cast<size_t>(animation.footstepSoundIndexGenerator()) % sounds.footstepSounds.size();
			audio.soundStage.play3DSound(sounds.footstepSounds[footstepSoundIndex], (position + settings.feetOffset).in(phys::METERS), {});
		}

		const bool moving = horizontalSpeed >= settings.grounded.minSpeed;
		if (moving) {
			countup(animation.movingAmount, deltaTime / settings.animation.transitionDuration, 1_x);
		} else {
			countdown(animation.movingAmount, deltaTime / settings.animation.transitionDuration);
		}

		const phys::Coefficient attackingAmount = 1_x - clamp((movement.timeSinceAttack - settings.attack.interval) / settings.animation.transitionDuration, 0_x, 1_x);
		const phys::Coefficient runningAmount = clamp(unlerp(horizontalSpeed, settings.animation.walkingSpeed, settings.animation.runningSpeed), 0_x, 1_x);
		const phys::Coefficient maxJumpingAmount = 1_x - runningAmount * settings.animation.jumpAmountReductionAtRunningSpeed;
		const phys::Coefficient jumpingAmount =
			(movement.flying) ? 0_x
			                  : maxJumpingAmount * (1_x - clamp((movement.timeSinceJump - animation.jumpAnimationDuration) / settings.animation.transitionDuration, 0_x, 1_x));
		if (jumpingAmount >= maxJumpingAmount || animation.movingAmount >= 1_x) {
			animation.idleAnimationTime = {};
		} else {
			animation.idleAnimationTime += deltaTime;
		}

		animation.angerAmount = expDecay(animation.angerAmount, (midAttack) ? 1_x : 0_x, settings.animation.angerDecayRate, deltaTime);

		animationState.animationLayers = {
			{
				.animationIndex = animation.idleAnimationIndex,
				.time = animation.idleAnimationTime,
				.blendWeight = (1_x - attackingAmount) * (1_x - jumpingAmount) * (1_x - animation.movingAmount),
				.looping = true,
			},
			{
				.animationIndex = animation.jumpAnimationIndex,
				.time = movement.timeSinceJump,
				.blendWeight = (1_x - attackingAmount) * jumpingAmount,
				.looping = false,
			},
			{
				.animationIndex = animation.walkingAnimationIndex,
				.time = animation.runningAnimationTime,
				.blendWeight = (1_x - attackingAmount) * (1_x - jumpingAmount) * animation.movingAmount * (1_x - runningAmount),
				.looping = true,
			},
			{
				.animationIndex = animation.runningAnimationIndex,
				.time = animation.runningAnimationTime,
				.blendWeight = (1_x - attackingAmount) * (1_x - jumpingAmount) * animation.movingAmount * runningAmount,
				.looping = true,
			},
			{
				.animationIndex = animation.attackAnimationIndex,
				.time = movement.timeSinceAttack * (animation.attackAnimationDuration / settings.attack.interval),
				.blendWeight = attackingAmount,
				.looping = false,
			},
		};

		animationState.morphTargetWeights = {
			animation.angerAmount,
			0_x,
			0_x,
		};
	}
}

void advancePlayerInputStates(exec::Entities<PlayerInput> playerEntities) {
	for (auto&& [entityID, input] : playerEntities) {
		input.update();
	}
}

struct PlayerSystem final : System {
	void addRequiredResources(phys::ResourceRegistry3D& resources, Graphics& graphics, Audio&, const Filesystem& filesystem) override {
		PlayerSettingsJSON settingsJSON{};
		try {
			json::deserializeFromString(filesystem.readInputFileString(PlayerSettingsJSON::FILEPATH), settingsJSON);
		} catch (...) {
			Error::throwWithNestedFilepath(PlayerSettingsJSON::FILEPATH);
		}
		const PlayerSettings& settings = resources.addResource<PlayerSettings>(std::move(settingsJSON));
		resources.addResource<PlayerModel>(PlayerModel{
			.model{graphics.device, graphics.renderer3D, res::Model{filesystem, settings.model.filepath}},
		});
		ArrayList<aud::Sound> footstepSounds{};
		for (const PlayerSettingsJSON::Step::Sound& sound : settings.step.sounds) {
			footstepSounds.emplace_back(filesystem, sound.filepath, aud::SoundOptions{.volume = sound.volume, .rolloffFactor = 0.05f});
		}
		resources.addResource<PlayerSounds>(PlayerSounds{
			.footstepSounds = std::move(footstepSounds),
			.jumpSound{filesystem, settings.jump.sound.filepath, {.volume = settings.jump.sound.volume, .rolloffFactor = 0.05f}},
		});
	}

	void removeResources(phys::ResourceRegistry3D& resources) noexcept override {
		resources.removeResource<PlayerSettings>();
		resources.removeResource<PlayerModel>();
		resources.removeResource<PlayerSounds>();
	}

	void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<animatePlayerInventories>("Animate player inventories");
	}

	void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) override {
		GREM_PROFILE_FUNCTION();

		const WorldView& worldView = simulation.resources.getResource<WorldView>();
		const app::FrameInfo& frameInfo = simulation.resources.getResource<app::FrameInfo>();

		const PlayerInteraction& interaction = simulation.registry.getComponent<PlayerInteraction>(worldView.playerEntityID);
		const PlayerInput& input = simulation.registry.getComponent<PlayerInput>(worldView.playerEntityID);

		const Offset2D screenCenter = graphics.viewport.region.offset + graphics.viewport.region.size / 2;

		const Span<const evt::Input> interactionInputs = input.getBoundInputs(PlayerAction::INTERACT);
		if (!interactionInputs.empty()) {
			if (simulation.registry.hasComponent<VehicleOccupant>(worldView.playerEntityID)) {
				graphics.put2DText({screenCenter.x, 6}, Color::WHITE, formatString("[{}] Exit vehicle", evt::getInputString(interactionInputs.front())), 2.0f,
					gfx::TextAlign::CENTER_HORIZONTALLY);
			} else {
				const phys::Position3D* const interactionTargetPosition = simulation.registry.findComponent<phys::Position3D>(interaction.targetEntityID);
				const ObjectPreviousPosition* const interactionTargetPreviousPosition = simulation.registry.findComponent<ObjectPreviousPosition>(interaction.targetEntityID);
				if (interactionTargetPosition && interactionTargetPreviousPosition) {
					const phys::Position3D interactionTargetDisplayPosition = mix(*interactionTargetPreviousPosition, *interactionTargetPosition, frameInfo.tickInterpolationAlpha);
					if (simulation.registry.hasComponent<VehicleTag>(interaction.targetEntityID)) {
						const phys::Position3D textPosition = interactionTargetDisplayPosition + phys::Length3D{0, 1.5_meters, 0};
						const phys::Orientation3D textOrientation = convert3x3MatrixToQuaternion(transpose(mat3{graphics.camera3D.getViewMatrix()}));
						graphics.put3DText(textPosition, textOrientation, 0.02f, Color::WHITE, formatString("[{}] Enter vehicle", evt::getInputString(interactionInputs.front())),
							gfx::TextAlign::CENTER);
					}
				}
			}
		}

		if (const PlayerInventory* const inventory = simulation.registry.findComponent<PlayerInventory>(worldView.playerEntityID)) {
			const PlayerSettings& settings = simulation.resources.getResource<PlayerSettings>();
			const float scale = 2.0f + 0.5f * sin(inventory->collectAnimationCooldown * (phys::PI / settings.animation.collectibleCollectionDuration));
			graphics.put2DText({6, 6}, Color::GREEN_YELLOW, formatString("Carrot Cakes: {}", inventory->collectibles), scale);

			if (inventory->collectibles >= getLevelCollectibleCount(simulation.resources)) {
				graphics.put2DText(screenCenter, Color::GREEN_YELLOW, "You collected all the things! :D", 4.0f, gfx::TextAlign::CENTER);
				graphics.put2DText(screenCenter + Offset2D{0, -72}, Color::GREEN_YELLOW, "(Press F5 to reload the level)", 2.0f, gfx::TextAlign::CENTER);
			}
		}
	}
} playerSystemImplementation{};

struct PlayerControlSystem final : System {
	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<handlePlayerInteractions>("Handle player interactions");
		scheduler.addTask<controlPlayers>("Control players");
	}
} playerControlSystemImplementation{};

struct PlayerGroundingSystem final : System {
	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<groundPlayers>("Ground players");
		scheduler.addTask<adjustPlayerCameras>("Adjust player cameras");
		scheduler.addTask<animatePlayers>("Animate players");
		scheduler.addTask<advancePlayerInputStates>("Advance player input states");
	}
} playerGroundingSystemImplementation{};

} // namespace

System* const playerSystem = &playerSystemImplementation;
System* const playerControlSystem = &playerControlSystemImplementation;
System* const playerGroundingSystem = &playerGroundingSystemImplementation;

void createPlayer(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, const Filesystem& filesystem, CStringView configurationFilepath, phys::Position3D position,
	phys::PitchYaw angles, Optional<uint32_t> controllerID) {
	GREM_PROFILE_FUNCTION();

	const PlayerSettings& settings = resources.getResource<PlayerSettings>();
	const PlayerModel& model = resources.getResource<PlayerModel>();
	createAnimatedObject(entity, resources, model.model,
		phys::ObjectOptions3D{
			.position = position,
			.orientation = phys::Orientation3D::yaw(angles.getY()),
			.gravityAcceleration{0, -settings.dynamics.gravity, 0},
			.surroundingFluidDensity{},
			.mass = settings.dynamics.mass,
			.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia3D::INF,
			.collider{.shape = settings.collisionShape, .filter = PLAYER_COLLISION_FILTER},
			.material{
				.staticFriction{},
				.kineticFriction{},
				.restitution{},
				.frictionCombine = phys::Material::FrictionCombine::MINIMUM,
				.restitutionCombine = phys::Material::RestitutionCombine::MINIMUM,
			},
			.enableResting = false,
		});
	entity.getComponent<ObjectModelInstance>().localTransformation =
		translateRotateScale(settings.feetOffset + settings.model.rootTranslation * phys::METERS, settings.model.rootRotation, settings.model.rootScale);
	entity.addComponent<PlayerInput>(filesystem, configurationFilepath, controllerID);
	entity.addComponent<PlayerTag>();
	entity.addComponent<PlayerInteraction>();
	entity.addComponent<PlayerMovement>(PlayerMovement{
		.headingYaw = angles.getY(),
		.timeSinceAttack = settings.attack.interval + settings.animation.transitionDuration,
	});
	entity.addComponent<PlayerCamera>(PlayerCamera{
		.horizontalDirection = flipY(rotate90DegreesCounterclockwise(convertAnglesToForwardDirection(angles.getY()))),
		.verticalTargetPosition = position.getY() + settings.cameraVerticalTargetOffset,
		.angles = angles,
		.distance = settings.camera.targetDistanceAtZeroSpeed,
	});
	entity.addComponent<PlayerAnimation>(PlayerAnimation{
		.idleAnimationIndex = model.model.getAnimationIndex("Idle"),
		.walkingAnimationIndex = model.model.getAnimationIndex("Walking"),
		.runningAnimationIndex = model.model.getAnimationIndex("Running"),
		.jumpAnimationIndex = model.model.getAnimationIndex("Jump"),
		.attackAnimationIndex = model.model.getAnimationIndex("Punch"),
		.runningAnimationDuration = model.model.getAnimation("Running").maxTimePoint * phys::SECONDS,
		.jumpAnimationDuration = model.model.getAnimation("Jump").maxTimePoint * phys::SECONDS,
		.attackAnimationDuration = model.model.getAnimation("Punch").maxTimePoint * phys::SECONDS,
	});
	entity.addComponent<PlayerInventory>();
}

PlayerCameraView getPlayerCameraView(const phys::EntityRegistry3D& registry, phys::EntityID entityID, float tickInterpolationAlpha) {
	const VehicleOccupant* const vehicleOccupant = registry.findComponent<VehicleOccupant>(entityID);
	const ObjectPreviousPosition* const vehicleOldPosition = (vehicleOccupant) ? registry.findComponent<ObjectPreviousPosition>(vehicleOccupant->vehicleEntityID) : nullptr;
	const phys::Position3D* const vehicleNewPosition = (vehicleOccupant) ? registry.findComponent<phys::Position3D>(vehicleOccupant->vehicleEntityID) : nullptr;

	const PlayerCamera& camera = registry.getComponent<PlayerCamera>(entityID);
	const phys::PitchYaw angles = mix(camera.previousAngles, camera.angles, tickInterpolationAlpha);
	const phys::Distance distance = mix(camera.previousDistance, camera.distance, tickInterpolationAlpha);
	const phys::Position1D verticalTargetPosition = mix(camera.previousVerticalTargetPosition, camera.verticalTargetPosition, tickInterpolationAlpha);
	const phys::Position2D horizontalTargetPosition = mix(                             //
		registry.getComponent<ObjectPreviousPosition>(entityID).get(phys::X, phys::Z), //
		registry.getComponent<phys::Position3D>(entityID).get(phys::X, phys::Z),       //
		tickInterpolationAlpha);
	const phys::Position3D targetPosition =
		(vehicleOldPosition && vehicleNewPosition)
	        ? mix(*vehicleOldPosition, *vehicleNewPosition, tickInterpolationAlpha)
	        : phys::Position3D{horizontalTargetPosition.getX(), verticalTargetPosition, horizontalTargetPosition.getY()};
	const phys::Position3D position = targetPosition - convertAnglesToForwardDirection(angles) * distance;
	const phys::LinearVelocity3D linearVelocity = mix(                 //
		registry.getComponent<ObjectPreviousLinearVelocity>(entityID), //
		registry.getComponent<phys::LinearVelocity3D>(entityID),       //
		tickInterpolationAlpha);
	return {.position = position, .angles = angles, .linearVelocity = linearVelocity};
}

void handlePlayerEvent(phys::EntityRegistry3D& registry, const evt::Event& event, bool hasControl) {
	GREM_PROFILE_FUNCTION();

	const exec::Entities<PlayerInput> playerEntities = registry;
	match(event)(
		[&](const evt::ControllerRemovedEvent& controllerRemoved) -> void {
			for (auto&& [entityID, input] : playerEntities) {
				if (input.controllerID == controllerRemoved.controllerID) {
					input.handleEvent(event);
					input.controllerID.reset();
				}
			}
		},
		[&](const auto& e) -> void {
			if (!hasControl) {
				return;
			}
			if constexpr (requires { e.controllerID; }) {
				bool handled = false;
				for (auto&& [entityID, input] : playerEntities) {
					if (input.controllerID == e.controllerID) {
						input.handleEvent(event);
						handled = true;
					}
				}

				if (!handled && !playerEntities.empty()) {
					PlayerInput& firstPlayerInput = get<PlayerInput&>(*playerEntities.begin());
					firstPlayerInput.controllerID = e.controllerID;
					firstPlayerInput.handleEvent(event);
				}
			} else {
				for (auto&& [entityID, input] : playerEntities) {
					if (input.hasKeyboardControl) {
						input.handleEvent(event);
					}
				}
			}
		});
}

void teleportPlayer(phys::EntityRegistry3D& registry, const phys::ResourceRegistry3D& resources, phys::EntityID entityID, phys::Position3D newPosition, phys::PitchYaw newAngles,
	phys::LinearVelocity3D newLinearVelocity) {
	exitVehicle(registry, entityID);
	const PlayerSettings& settings = resources.getResource<PlayerSettings>();
	const phys::Orientation3D orientation = phys::Orientation3D::yaw(newAngles.getY());
	registry.getComponent<phys::Position3D>(entityID) = newPosition;
	registry.getComponent<ObjectPreviousPosition>(entityID) = {newPosition};
	registry.getComponent<phys::Orientation3D>(entityID) = orientation;
	registry.getComponent<ObjectPreviousOrientation>(entityID) = {orientation};
	registry.getComponent<phys::LinearVelocity3D>(entityID) = newLinearVelocity;
	registry.getComponent<ObjectPreviousLinearVelocity>(entityID) = {newLinearVelocity};
	PlayerCamera& camera = registry.getComponent<PlayerCamera>(entityID);
	camera.verticalTargetPosition = newPosition.getY();
	camera.previousVerticalTargetPosition = camera.verticalTargetPosition;
	camera.distance = settings.camera.targetDistanceAtZeroSpeed;
	camera.previousDistance = camera.distance;
	camera.angles = {newAngles.getX(), camera.angles.getY() + getAngleDifference(camera.angles.getY(), newAngles.getY())};
	camera.previousAngles = camera.angles;
	camera.horizontalDirection = flipY(rotate90DegreesCounterclockwise(convertAnglesToForwardDirection(camera.angles.getY())));
	camera.horizontalDirectionScale = camera.horizontalDirection * length(camera.horizontalDirectionScale);
}

void givePlayerCollectible(phys::EntityRegistry3D& registry, const phys::ResourceRegistry3D& resources, phys::EntityID entityID) {
	const PlayerSettings& settings = resources.getResource<PlayerSettings>();
	PlayerInventory& inventory = registry.getComponent<PlayerInventory>(entityID);
	++inventory.collectibles;
	inventory.collectAnimationCooldown = settings.animation.collectibleCollectionDuration;
}

void resetAllPlayerCollectibles(phys::EntityRegistry3D& registry) {
	for (auto&& [entityID, inventory] : registry.getEntities<PlayerInventory>()) {
		inventory.collectibles = 0;
	}
}
