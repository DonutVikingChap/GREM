// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "game_commands.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "AssetCache.hpp"
#include "GameState.hpp"
#include "PlayerEntityMap.hpp"
#include "Prefab.hpp"
#include "Schema.hpp"
#include "Snapshot.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "WorldView.hpp"
#include "game_components.hpp"
#include "game_events.hpp"
#include "game_resources.hpp"

void RotateAimCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	if (all(isfinite(aimRotations))) {
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
			if (all(isfinite(aimRotations))) {
				if (Aim* const playerAim = registry.findComponent<Aim>(entityID)) {
					playerAim->angles += aimRotations;
				}
			}
		});
	}
}

void StartJumpingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags &= ~MovementState::ALREADY_JUMPED;
			playerMovementState->flags |= MovementState::JUMPING;
		}
	});
}

void StopJumpingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags &= ~MovementState::ALREADY_JUMPED;
			playerMovementState->flags &= ~MovementState::JUMPING;
		}
	});
}

void StartCrouchingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags |= MovementState::CROUCHING;
		}
	});
}

void StopCrouchingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags &= ~MovementState::CROUCHING;
		}
	});
}

void StartSprintingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags |= MovementState::SPRINTING;
		}
	});
}

void StopSprintingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags &= ~MovementState::SPRINTING;
		}
	});
}

void ToggleFlyingCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
			playerMovementState->flags ^= MovementState::FLYING;
		}
	});
}

void ToggleSimulationPausedCommand::execute(GameState& gameState, PlayerID, LocalPlayerID) const {
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<SessionState>().flags ^= SessionState::PAUSED;
}

void SingleStepPausedSimulationCommand::execute(GameState& gameState, PlayerID, LocalPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	if (resources.getResource<SessionState>().flags.contains(SessionState::PAUSED)) {
		Scheduler scheduler{};
		phys::Simulation3D::scheduleStep(scheduler, resources.getResource<phys::SimulationOptions3D>());
		const Schedule schedule = scheduler.buildSchedule();
		exec::SequentialExecutor{}.executeSchedule(schedule, registry, resources);
	}
}

void StartPrimaryFireCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				if (WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID)) {
					weaponState->flags &= ~WeaponState::TRIGGER_CLICKED;
					weaponState->flags |= WeaponState::PULLING_TRIGGER;
				}
			}
		}
	});
}

void StopPrimaryFireCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				if (WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID)) {
					weaponState->flags &= ~WeaponState::TRIGGER_CLICKED;
					weaponState->flags &= ~WeaponState::PULLING_TRIGGER;
				}
			}
		}
	});
}

void StartAimingDownSightsCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				if (WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID)) {
					weaponState->flags |= WeaponState::AIMING_DOWN_SIGHTS;
				}
			}
		}
	});
}

void StopAimingDownSightsCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				if (WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID)) {
					weaponState->flags &= ~WeaponState::AIMING_DOWN_SIGHTS;
				}
			}
		}
	});
}

void ReloadWeaponCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				if (WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID)) {
					weaponState->flags |= WeaponState::STARTING_RELOAD;
				}
			}
		}
	});
}

void ChangeFireModeLeftCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
				const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
				if (weaponState && weaponType && *weaponType != WeaponType{}) {
					const WeaponDescription& weaponDescription = resources.getResource<Schema>().getWeaponDescription(*weaponType);
					const auto it = find(weaponDescription.capableFireModes, weaponState->fireMode);
					if (it != weaponDescription.capableFireModes.begin()) {
						weaponState->fireMode = *(it - 1);
						weaponState->drawTimeRemaining = max(weaponState->drawTimeRemaining, phys::Time{0.08_seconds});

						if (weaponDescription.changeFireModeSoundType != SoundType{}) {
							if (const SynchronizedEntityID* const synchronizedPlayerEntityID = registry.findComponent<SynchronizedEntityID>(entityID)) {
								resources.getResource<Events>().push_back(
									EntityParentedSoundPlayedEvent{.soundType = weaponDescription.changeFireModeSoundType, .emitter = *synchronizedPlayerEntityID});
							}
						}
					}
				}
			}
		}
	});
}

void ChangeFireModeRightCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
				const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
				if (weaponState && weaponType && *weaponType != WeaponType{}) {
					const WeaponDescription& weaponDescription = resources.getResource<Schema>().getWeaponDescription(*weaponType);
					const auto it = find(weaponDescription.capableFireModes, weaponState->fireMode);
					if (it != weaponDescription.capableFireModes.end() && (it + 1) != weaponDescription.capableFireModes.end()) {
						weaponState->fireMode = *(it + 1);
						weaponState->drawTimeRemaining = max(weaponState->drawTimeRemaining, phys::Time{0.08_seconds});

						if (weaponDescription.changeFireModeSoundType != SoundType{}) {
							if (const SynchronizedEntityID* const synchronizedPlayerEntityID = registry.findComponent<SynchronizedEntityID>(entityID)) {
								resources.getResource<Events>().push_back(
									EntityParentedSoundPlayedEvent{.soundType = weaponDescription.changeFireModeSoundType, .emitter = *synchronizedPlayerEntityID});
							}
						}
					}
				}
			}
		}
	});
}

void CycleFireModeCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = resources.getResource<SynchronizedEntityMap>().findEntity(registry, playerInventory->equippedWeapon)) {
				WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
				const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
				if (weaponState && weaponType && *weaponType != WeaponType{}) {
					const WeaponDescription& weaponDescription = resources.getResource<Schema>().getWeaponDescription(*weaponType);
					const auto it = find(weaponDescription.capableFireModes, weaponState->fireMode);
					if (it != weaponDescription.capableFireModes.end()) {
						if ((it + 1) != weaponDescription.capableFireModes.end()) {
							weaponState->fireMode = *(it + 1);
						} else {
							weaponState->fireMode = *weaponDescription.capableFireModes.begin();
						}
						weaponState->drawTimeRemaining = max(weaponState->drawTimeRemaining, phys::Time{0.08_seconds});

						if (weaponDescription.changeFireModeSoundType != SoundType{}) {
							if (const SynchronizedEntityID* const synchronizedPlayerEntityID = registry.findComponent<SynchronizedEntityID>(entityID)) {
								resources.getResource<Events>().push_back(
									EntityParentedSoundPlayedEvent{.soundType = weaponDescription.changeFireModeSoundType, .emitter = *synchronizedPlayerEntityID});
							}
						}
					}
				}
			}
		}
	});
}

void ToggleFlashlightCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
		if (FlashlightState* const flashlightState = registry.findComponent<FlashlightState>(entityID)) {
			flashlightState->on = !flashlightState->on;
		}
	});
}

void PlaceDecalCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	if (resources.getResource<Schema>().findDecalMaterialDescription(decalMaterialType) && all(greaterThan(decalSize, 0)) && decalRange > 0) {
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
			const Aim* const playerAim = registry.findComponent<Aim>(entityID);
			const phys::Position3D* const playerPosition = registry.findComponent<phys::Position3D>(entityID);
			if (playerAim && playerPosition) {
				const phys::Position3D playerAimPosition = *playerPosition + playerAim->offset;
				const phys::Direction3D playerAimDirection = convertAnglesToForwardDirection(playerAim->angles);

				const Optional<phys::Broadphase3D::RaycastResult> hit = resources.getResource<phys::Broadphase3D>().raycastClosestHit(
					phys::Ray3D{.origin = playerAimPosition, .direction = playerAimDirection, .maxDistance = 2_meters}, phys::CollisionFilter{},
					registry.getEntities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D>(),
					phys::CollisionFilterTest::RESPONSE, [&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; });
				if (hit) {
					const TickIndex tickIndex = resources.getResource<TickIndex>();
					const Duration tickInterval = resources.getResource<Duration>();
					const phys::Position3D decalPosition = playerAimPosition + playerAimDirection * hit->distance;
					const phys::Orientation3D decalOrientation = phys::Orientation3D::lookAt(-hit->normal, cross(cross(playerAimDirection, phys::Y_AXIS_3D), -hit->normal));
					spawnDecal(registry, resources, decalMaterialType, EntityID::Flags{ENTITY_DISPLAY_SUBTICK_PREDICTED}, decalPosition, decalOrientation, decalSize, decalRange,
						hit->objectID, tickIndex, tickInterval);
				}
			}
		});
	}
}

void SpawnPrefabCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	AssetCache& assetCache = resources.getResource<AssetCache>();
	Schema& schema = resources.getResource<Schema>();
	SharedPointer<Prefab> prefab{};
	try {
		prefab = assetCache.getPrefab(schema, prefabFilepath);
	} catch (...) {
		return;
	}
	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> bool {
		const Aim* const playerAim = registry.findComponent<Aim>(entityID);
		const phys::Position3D* const playerPosition = registry.findComponent<phys::Position3D>(entityID);
		if (playerAim && playerPosition) {
			const phys::Position3D playerAimPosition = *playerPosition + playerAim->offset;
			const phys::Direction3D playerAimDirection = convertAnglesToForwardDirection(playerAim->angles);
			const phys::Position3D position = playerAimPosition + playerAimDirection * 2_meters;
			prefab->spawn(registry, resources, EntityID::Flags{}, position);
			return true;
		}
		return false;
	});
}

void SpawnModelObjectCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	if (resources.getResource<Schema>().findLoadedModelDescription(modelType)) {
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> void {
			const Aim* const playerAim = registry.findComponent<Aim>(entityID);
			const phys::Position3D* const playerPosition = registry.findComponent<phys::Position3D>(entityID);
			if (playerAim && playerPosition) {
				const phys::Position3D playerAimPosition = *playerPosition + playerAim->offset;
				const phys::Direction3D playerAimDirection = convertAnglesToForwardDirection(playerAim->angles);
				spawnEntity(registry, resources, EntityType{"MODEL_OBJECT"}, EntityID::Flags{},
					ComponentInitializers{
						ModelType{modelType},
						phys::Position3D{playerAimPosition + playerAimDirection * 2_meters},
					});
			}
		});
	}
}

void RemoveModelObjectCommand::execute(GameState& gameState, PlayerID, LocalPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	SynchronizedEntityID lastSynchronizedEntityID{};
	EntityID lastEntityID{};
	for (auto&& [entityID, modelType, synchronizedEntityID, entityType] : registry.getEntities<const ModelType, const SynchronizedEntityID, const EntityType>()) {
		if (entityType == EntityType{"MODEL_OBJECT"} && (entityID.getFlags() & ENTITY_PART_OF_MAP) == 0) {
			if (!lastSynchronizedEntityID || synchronizedEntityID > lastSynchronizedEntityID) {
				lastSynchronizedEntityID = synchronizedEntityID;
				lastEntityID = entityID;
			}
		}
	}
	killEntity(registry, resources, lastEntityID);
}

void RainBoxesCommand::execute(GameState& gameState, PlayerID, LocalPlayerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	const TickIndex tickIndex = resources.getResource<TickIndex>();
	const rng::Xoroshiro128PlusPlusEngine::result_type seed = static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(tickIndex - TickIndex{});
	rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
	rng::UniformRealDistribution<float> distributionXZ{-99.0f, 99.0f};
	rng::UniformRealDistribution<float> distributionY{0.0f, 50.0f};
	for (int i = 0; i < 1000; ++i) {
		const phys::Length1D x = distributionXZ(numberGenerator) * phys::METERS;
		const phys::Length1D y = distributionY(numberGenerator) * phys::METERS;
		const phys::Length1D z = distributionXZ(numberGenerator) * phys::METERS;
		spawnEntity(registry, resources, EntityType{"MODEL_OBJECT"}, EntityID::Flags{},
			ComponentInitializers{
				ModelType{"SMALL_BOX"},
				phys::Position3D{x, y, z},
			});
	}
}

void SubtickCommand::execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const {
	match (*this)([&]<typename Command>(const Command& command) -> void {
		GREM_PROFILE_BLOCK_DYNAMIC(formatString("Execute {}", meta::unqualified_type_name_v<Command>));
		command.execute(gameState, playerID, localPlayerID);
	});
}

void TickCommand::beginTick(GameState& gameState, PlayerID playerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	for (const LocalPlayerCommand& localPlayerCommand : localPlayerCommands) {
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerCommand.localPlayerID, [&](EntityID entityID) -> void {
			if (MovementState* const playerMovementState = registry.findComponent<MovementState>(entityID)) {
				if (all(isfinite(localPlayerCommand.desiredDirectionScale))) {
					playerMovementState->desiredDirectionScale = localPlayerCommand.desiredDirectionScale;
				}
			}
		});
	}
}

void TickCommand::runSubtick(GameState& gameState, PlayerID playerID, SnapshotBufferView receivedSnapshots, SnapshotBufferView predictionSnapshots, Duration oldTimeOffset,
	Duration newTimeOffset) const {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(oldTimeOffset >= Duration{});
	GREM_ASSERT(oldTimeOffset <= newTimeOffset);

	ResourceRegistry& resources = gameState.getResources();
	const TickIndex tickIndex = resources.getResource<TickIndex>();
	const Duration tickInterval = resources.getResource<Duration>();
	GREM_ASSERT(newTimeOffset <= tickInterval);

	if (receivedSnapshots.empty() || predictionSnapshots.empty()) {
		return;
	}
	while (receivedSnapshots.back().tickIndex >= tickIndex) {
		receivedSnapshots = SnapshotBufferView{receivedSnapshots.begin(), receivedSnapshots.end() - 1};
		if (receivedSnapshots.empty()) {
			return;
		}
	}
	while (predictionSnapshots.back().tickIndex >= tickIndex) {
		predictionSnapshots = SnapshotBufferView{predictionSnapshots.begin(), predictionSnapshots.end() - 1};
		if (predictionSnapshots.empty()) {
			return;
		}
	}

	CurrentPlayer currentPlayer{
		.playerID = playerID,
		.localPlayerID{},
		.receivedSnapshots = receivedSnapshots,
		.predictionSnapshots = predictionSnapshots,
		.receivedInterpolationTimestamp{},
		.predictionInterpolationTimestamp{},
		.subtickBeginTimestamp{},
		.subtickEndTimestamp{},
		.tickInterval = tickInterval,
	};
	resources.addExternalResource<CurrentPlayer>(&currentPlayer);

	for (const LocalPlayerCommand& localPlayerCommand : localPlayerCommands) {
		Timestamp timestamp{tickIndex, oldTimeOffset, tickInterval};
		currentPlayer.localPlayerID = localPlayerCommand.localPlayerID;
		auto it = lowerBound(localPlayerCommand.subtickCommands, oldTimeOffset);
		while (it != localPlayerCommand.subtickCommands.end() && it->timeOffset < newTimeOffset) {
			const Duration nextTimeOffset = clamp(it->timeOffset, Duration{}, tickInterval);
			const Timestamp nextTimestamp{tickIndex, nextTimeOffset, tickInterval};
			if (timestamp.getTimeOffset() < nextTimeOffset && !resources.getResource<SessionState>().flags.contains(SessionState::PAUSED)) {
				currentPlayer.receivedInterpolationTimestamp = receivedInterpolationTimestampAtTickBegin.withTimeAdded(timestamp.getTimeOffset(), tickInterval);
				currentPlayer.predictionInterpolationTimestamp = timestamp.withTicksAdded(-1);
				currentPlayer.subtickBeginTimestamp = timestamp;
				currentPlayer.subtickEndTimestamp = nextTimestamp;
				gameState.updateCurrentPlayer();
			}
			timestamp = nextTimestamp;
			it->execute(gameState, playerID, localPlayerCommand.localPlayerID);
			++it;
		}
		if (timestamp.getTimeOffset() < newTimeOffset && !resources.getResource<SessionState>().flags.contains(SessionState::PAUSED)) {
			currentPlayer.receivedInterpolationTimestamp = receivedInterpolationTimestampAtTickBegin.withTimeAdded(timestamp.getTimeOffset(), tickInterval);
			currentPlayer.predictionInterpolationTimestamp = timestamp.withTicksAdded(-1);
			currentPlayer.subtickBeginTimestamp = timestamp;
			currentPlayer.subtickEndTimestamp = Timestamp{tickIndex, newTimeOffset, tickInterval};
			gameState.updateCurrentPlayer();
		}
	}

	resources.removeResource<CurrentPlayer>();
}

void TickCommand::endTick(GameState& gameState, PlayerID playerID) const {
	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	for (const LocalPlayerCommand& localPlayerCommand : localPlayerCommands) {
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerCommand.localPlayerID, [&](EntityID entityID) -> void {
			if (Aim* const playerAim = registry.findComponent<Aim>(entityID)) {
				if (all(isfinite(localPlayerCommand.aimRotationRates))) {
					playerAim->rotationRates = localPlayerCommand.aimRotationRates;
				}
			}
		});
	}
}
