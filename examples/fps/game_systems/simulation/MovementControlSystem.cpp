// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../PlayerEntityMap.hpp"
#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"
#include "../../game_events.hpp"

class MovementControlSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<controlMovement>("Control movement");
	}

private:
	static void controlMovement(Entities<phys::ObjectActivity, phys::Position3D, phys::Scale3D, phys::LinearVelocity3D, phys::LinearAcceleration3D, MovementState,
									const MovementType, const phys::Orientation3D, const phys::AngularVelocity3D, const phys::Collider3D>
									entities,
		Entities<Aim> aimEntities, Entities<const Inventory> inventoryEntities, Entities<WeaponState, const WeaponType> weaponEntities, Entities<const PlayerID> playerEntities,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities, Events& events,
		const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions, const Schema& schema, const SynchronizedEntityMap& synchronizedEntityMap,
		TickIndex tickIndex) {
		for (auto&& [entityID, activity, position, scale, linearVelocity, gravityAcceleration, movementState, movementType, orientation, angularVelocity, collider] : entities) {
			if ((entityID.getFlags() & (ENTITY_CLIENTSIDE | ENTITY_PHYSICS_PREDICTED)) == ENTITY_CLIENTSIDE) {
				gravityAcceleration = {};
				continue;
			}

			activity.energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;

			const bool jumping = movementState.flags.contains(MovementState::JUMPING);
			const bool crouching = movementState.flags.contains(MovementState::CROUCHING);
			const bool sprinting = movementState.flags.contains(MovementState::SPRINTING);
			const bool flying = movementState.flags.contains(MovementState::FLYING);

			const MovementDescription& movementDescription = schema.getMovementDescription(movementType);
			const phys::Time deltaTime = simulationOptions.stepInterval;
			const phys::Time timeSinceJump = static_cast<float>(tickIndex - movementState.lastJumpTickIndex) * deltaTime;
			const bool canJump =
				movementState.groundNormal && timeSinceJump >= movementDescription.jumpRegroundDelay && !movementState.flags.contains(MovementState::ALREADY_JUMPED);

			if (flying || (canJump && jumping)) {
				movementState.lastJumpTickIndex = tickIndex;
				movementState.groundNormal.reset();
			}

			const bool grounded = movementState.groundNormal.has_value();

			const phys::Time crouchDuration = clamp(movementState.timeSpentChangingCrouchAmount, movementDescription.crouchDurationMin, movementDescription.crouchDurationMax);
			const phys::Coefficient crouchAmountDelta = ((crouching) ? deltaTime : -deltaTime) / crouchDuration;
			const phys::Coefficient oldCrouchAmount = movementState.crouchAmount;
			const phys::Box3D untransformedBoundingBox = phys::ShapeView{collider.shape}.getBoundingBox(phys::Transformation3D{}).value_or(phys::Box3D{.min{}, .max{}});
			const phys::Distance baseHeight = untransformedBoundingBox.max.getY() - untransformedBoundingBox.min.getY();
			const phys::Distance crouchHeight = baseHeight * movementDescription.crouchHeightScale;
			const phys::Distance maxStepHeight = baseHeight * movementDescription.stepHeightCoefficient;
			updateCrouchAmount(movementState.crouchAmount, position, orientation, scale, grounded, entityID, collider, objectEntities, broadphase, simulationOptions, baseHeight,
				crouchHeight, maxStepHeight, crouchAmountDelta);
			const phys::Distance height = mix(baseHeight, crouchHeight, movementState.crouchAmount);
			const phys::Position3D stepPosition = position - phys::Y_AXIS_3D * height * 0.5_x;

			if (grounded && movementState.crouchAmount != oldCrouchAmount) {
				countup(movementState.timeSpentChangingCrouchAmount, deltaTime, movementDescription.crouchDurationMax);
			} else {
				countdown(movementState.timeSpentChangingCrouchAmount, deltaTime);
			}

			phys::Speed targetSpeed = movementDescription.baseSpeed;
			targetSpeed *= (flying) ? 1_x : mix(1_x, movementDescription.crouchSpeedCoefficient, movementState.crouchAmount);
			targetSpeed *= (sprinting) ? movementDescription.sprintSpeedCoefficient : 1_x;
			targetSpeed *= (flying) ? movementDescription.flySpeedCoefficient : 1_x;

			const WeaponDescription* weaponDescription = nullptr;
			if (inventoryEntities.containsEntity(entityID)) {
				const auto& [inventoryEntityID, inventory] = inventoryEntities[entityID];
				if (const EntityID weaponEntityID = synchronizedEntityMap.findEntity(weaponEntities, inventory.equippedWeapon)) {
					auto&& [weaponEntityID_, weaponState, weaponType] = weaponEntities[weaponEntityID];
					weaponDescription = &schema.getWeaponDescription(weaponType);
					weaponState.decayedCrouchAmount =
						expDecay(weaponState.decayedCrouchAmount, movementState.crouchAmount, weaponDescription->crouchExponentialDecayRate, deltaTime);
					targetSpeed *= mix(1_x, weaponDescription->aimingDownSightsMovementSpeedCoefficient, weaponState.aimingDownSightsAmount);
				}
			}

			if (aimEntities.containsEntity(entityID)) {
				Aim& aim = aimEntities.getComponent<Aim>(entityID);
				aim.offset = phys::Y_AXIS_3D * height * movementDescription.aimOffsetCoefficient;
			}

			const PlayerID playerID = playerEntities.getComponentOr<PlayerID>(entityID, PlayerID{});

			if (flying) {
				const phys::Scale1D desiredVerticalComponent = static_cast<float>(jumping) - static_cast<float>(crouching);
				const phys::Scale3D desiredDirectionScale{movementState.desiredDirectionScale.getX(), desiredVerticalComponent, movementState.desiredDirectionScale.getY()};
				flyMove(linearVelocity, desiredDirectionScale, targetSpeed, movementDescription.accelerationDuration, movementDescription.minGroundSpeed, deltaTime);
				gravityAcceleration = {};
			} else {
				if (canJump && jumping) {
					movementState.flags |= MovementState::ALREADY_JUMPED;
					jump(linearVelocity, movementDescription.gravityAcceleration, movementDescription.jumpHeight);
					if (movementDescription.jumpSoundType != SoundType{}) {
						if (playerID) {
							events.push_back(PlayerAssociatedWorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.jumpSoundType,
								.position = stepPosition,
								.associatedPlayerID = playerID,
							});
						} else {
							events.push_back(WorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.jumpSoundType,
								.position = stepPosition,
							});
						}
					}
				}

				if (grounded) {
					groundMove(linearVelocity, movementState.desiredDirectionScale, targetSpeed, movementDescription.accelerationDuration, movementDescription.minGroundSpeed,
						*movementState.groundNormal, deltaTime);
				} else {
					phys::Acceleration airAcceleration = movementDescription.airAcceleration;
					airAcceleration *= mix(1_x, movementDescription.crouchSpeedCoefficient, movementState.crouchAmount);
					airMove(linearVelocity, movementState.desiredDirectionScale, movementDescription.targetAirSpeed, airAcceleration, deltaTime);
				}
				gravityAcceleration = phys::Y_AXIS_3D * -movementDescription.gravityAcceleration;
			}

			if (weaponDescription) {
				const phys::Speed horizontalSpeed = length(linearVelocity.get(phys::X, phys::Z));
				const phys::Angle oldBobbingPhase = movementState.bobbingPhase;
				const phys::Angle newBobbingPhase = calculateNewBobbingPhase(oldBobbingPhase, *weaponDescription, grounded, horizontalSpeed, deltaTime);
				movementState.bobbingPhase = newBobbingPhase;
				if (grounded && horizontalSpeed > 0 && !movementDescription.footstepSoundTypes.empty()) {
					if ((fmod(oldBobbingPhase + 0.6_turns, 1_turns) < 0.5_turns) != (fmod(newBobbingPhase + 0.6_turns, 1_turns) < 0.5_turns)) {
						rng::Xoroshiro128PlusPlusEngine::result_type seed = static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(tickIndex - TickIndex{});
						seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(stepPosition.getX().in(phys::METERS), 65536.0f));
						seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(stepPosition.getY().in(phys::METERS), 65536.0f));
						seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(stepPosition.getZ().in(phys::METERS), 65536.0f));
						rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
						rng::UniformIntegerDistribution<size_t> distribution{0, movementDescription.footstepSoundTypes.size() - 1};
						if (playerID) {
							events.push_back(PlayerAssociatedWorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.footstepSoundTypes[distribution(numberGenerator) % movementDescription.footstepSoundTypes.size()],
								.position = stepPosition,
								.associatedPlayerID = playerID,
							});
						} else {
							events.push_back(WorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.footstepSoundTypes[distribution(numberGenerator) % movementDescription.footstepSoundTypes.size()],
								.position = stepPosition,
							});
						}
					}
				}
			}

			movementState.oldPosition = position;
			movementState.oldLinearVelocity = linearVelocity;
		}
	}

	static void jump(phys::LinearVelocity3D& linearVelocity, phys::LinearAcceleration1D gravityAcceleration, phys::Distance jumpHeight) {
		linearVelocity[phys::Y] = sqrt(2_x * gravityAcceleration * jumpHeight);
	}

	static void updateCrouchAmount(phys::Coefficient& crouchAmount, phys::Position3D& position, phys::Orientation3D orientation, phys::Scale3D& scale, bool grounded,
		EntityID entityID, const phys::Collider3D& collider,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
		const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions, phys::Distance baseHeight, phys::Distance crouchHeight,
		phys::Distance maxStepHeight, phys::Coefficient crouchAmountDelta) {
		const phys::Coefficient oldCrouchAmount = crouchAmount;
		crouchAmount = clamp(crouchAmount + crouchAmountDelta, 0_x, 1_x);

		const phys::Distance newHeight = mix(baseHeight, crouchHeight, crouchAmount);
		const phys::Distance newMaxStepHeight = min(maxStepHeight, newHeight * 0.95_x);
		scale.setY(tryDivide(newHeight, baseHeight).value_or(1_x));

		if (crouchAmount != oldCrouchAmount) {
			const phys::Distance oldHeight = mix(baseHeight, crouchHeight, oldCrouchAmount);
			const phys::Length1D heightDelta = newHeight - oldHeight;
			const phys::Position1D oldPositionY = position.getY();
			if (grounded) {
				position[phys::Y] += heightDelta * 0.5_x;
			}

			// If the new crouch amount would cause us to get stuck, roll back to the old crouch amount.
			if (broadphase.testShape(
					abs(heightDelta) * 0.5_x, collider.shape, collider.filter,
					translateRotateScale(position + phys::Length3D{0, newMaxStepHeight * 0.5_x, 0}, orientation,
						scale * phys::Scale3D{1_x, (newHeight - newMaxStepHeight) / newHeight, 1_x}),
					objectEntities, simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
					[&](EntityID, phys::CollisionFilterTestResult) -> bool { return true; }, [&](EntityID otherEntityID) -> bool { return otherEntityID != entityID; })) {
				crouchAmount = oldCrouchAmount;
				scale.setY(tryDivide(oldHeight, baseHeight).value_or(1_x));
				position[phys::Y] = oldPositionY;
			}
		}
	}

	static void groundMove(phys::LinearVelocity3D& linearVelocity, phys::Scale2D desiredDirectionScale, phys::Speed targetSpeed, phys::Time accelerationDuration,
		phys::Speed minSpeed, phys::Scale3D groundNormal, phys::Time deltaTime) {
		const phys::Scale1D desiredDirectionScaleMagnitude = length(desiredDirectionScale);
		const phys::Scale3D desiredDirection = tryDivide(desiredDirectionScale, desiredDirectionScaleMagnitude).value_or(phys::Scale2D{}).get(phys::X, 0, phys::Y);
		const Optional<phys::Direction3D> desiredDirectionAlongGround = tryNormalize(cross(groundNormal, cross(desiredDirection, groundNormal)));
		const phys::Scale3D targetDirectionScale =
			(desiredDirectionAlongGround) ? min(desiredDirectionScaleMagnitude, 1_x) * dot(*desiredDirectionAlongGround, desiredDirection) * desiredDirection : phys::Scale3D{};
		const phys::LinearVelocity3D targetVelocity = targetDirectionScale * targetSpeed;
		const phys::Speed currentSpeed = length(linearVelocity);
		const phys::LinearVelocity1D potentialSpeedDelta = (length(targetVelocity) - currentSpeed) * (deltaTime / accelerationDuration);
		if ((targetVelocity == 0 && currentSpeed < minSpeed) || currentSpeed + potentialSpeedDelta <= 0) {
			linearVelocity = {};
		} else {
			const phys::LinearVelocity3D velocityRemaining = targetVelocity - linearVelocity;
			const phys::LinearVelocity3D addedVelocity = velocityRemaining * (deltaTime / accelerationDuration);
			linearVelocity += addedVelocity;
		}
	}

	static void airMove(phys::LinearVelocity3D& linearVelocity, phys::Scale2D desiredDirectionScale, phys::Speed targetSpeed, phys::LinearAcceleration1D airAcceleration,
		phys::Time deltaTime) {
		const phys::Scale1D desiredDirectionScaleMagnitude = length(desiredDirectionScale);
		const phys::Scale2D desiredDirection = tryDivide(desiredDirectionScale, desiredDirectionScaleMagnitude).value_or(phys::Scale2D{});
		const phys::LinearVelocity1D currentVelocityInDesiredDirection = dot(linearVelocity.get(phys::X, phys::Z), desiredDirection);
		const phys::LinearVelocity1D velocityRemainingInDesiredDirection = targetSpeed - currentVelocityInDesiredDirection;
		if (velocityRemainingInDesiredDirection > 0) {
			const phys::Speed addedSpeed = min(airAcceleration * deltaTime, velocityRemainingInDesiredDirection);
			const phys::LinearVelocity2D addedVelocity = min(desiredDirectionScaleMagnitude, 1_x) * addedSpeed * desiredDirection;
			linearVelocity += addedVelocity.get(phys::X, 0, phys::Y);
		}
	}

	static void flyMove(phys::LinearVelocity3D& linearVelocity, phys::Scale3D desiredDirectionScale, phys::Speed targetSpeed, phys::Time accelerationDuration, phys::Speed minSpeed,
		phys::Time deltaTime) {
		const phys::LinearVelocity3D targetVelocity = clampLength(desiredDirectionScale, 1_x) * targetSpeed;
		const phys::Speed currentSpeed = length(linearVelocity);
		const phys::LinearVelocity1D potentialSpeedDelta = (length(targetVelocity) - currentSpeed) * (deltaTime / accelerationDuration);
		if ((targetVelocity == 0 && currentSpeed < minSpeed) || currentSpeed + potentialSpeedDelta <= 0) {
			linearVelocity = {};
		} else {
			const phys::LinearVelocity3D velocityRemaining = targetVelocity - linearVelocity;
			const phys::LinearVelocity3D addedVelocity = velocityRemaining * (deltaTime / accelerationDuration);
			linearVelocity += addedVelocity;
		}
	}

	[[nodiscard]] static phys::Angle calculateNewBobbingPhase(phys::Angle bobbingPhase, const WeaponDescription& weaponDescription, bool grounded, phys::Speed horizontalSpeed,
		Duration deltaTime) {
		const WeaponDescription::ProceduralAnimation& proceduralAnimation = weaponDescription.proceduralAnimation;

		if (grounded && horizontalSpeed > 0) {
			const phys::Frequency speedScaledBobbingRate = log2(1.0f + 2.0f * float{(horizontalSpeed * proceduralAnimation.bobbingRate).in(phys::HERTZ)}) * phys::HERTZ;
			return bobbingPhase + speedScaledBobbingRate * 0.5_turns * deltaTime;
		}

		const phys::Angle wrappedPhase = fmod(bobbingPhase, 1_turns);
		const phys::Angle decayPhaseShift = copysign(proceduralAnimation.bobbingDecayRate * deltaTime, wrappedPhase - 0.5_turns);
		const phys::Angle decayedPhaseInWrappedRange = wrappedPhase + decayPhaseShift;
		if (decayedPhaseInWrappedRange <= 0_turns || decayedPhaseInWrappedRange >= 1_turns) {
			return 0_turns;
		}

		return bobbingPhase + decayPhaseShift;
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createMovementControlSystem() { // NOLINT(misc-use-internal-linkage)
	return new MovementControlSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
