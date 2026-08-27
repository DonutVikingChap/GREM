// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/DebugVisualization.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../EntityType.hpp"
#include "../../Graphics.hpp"
#include "../../PlayerEntityMap.hpp"
#include "../../Snapshot.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../Timestamp.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_events.hpp"
#include "../../game_functions.hpp"

class ProjectileHitDetectionSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	static constexpr phys::Acceleration GRAVITY_ACCELERATION = 9.82_meters_per_second_squared;
	static constexpr phys::Distance MAX_LAG_COMPENSATION_DISTANCE_PER_TICK = 1_meter;
	static constexpr Duration MAX_LAG_COMPENSATION_DURATION = 300_milliseconds;

	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<ProjectileHits>();
		resources.addSharedResource<ProjectilesToDestroy>();
		resources.addSharedResource<ProjectileDebugVisualization>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<ProjectileHits>();
		resources.removeResource<ProjectilesToDestroy>();
		resources.removeResource<ProjectileDebugVisualization>();
	}

	void scheduleCurrentPlayerUpdate(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<clearProjectileHits>("Clear projectile hits.");
		scheduler.addTask<fireCurrentPlayerProjectiles>("Fire current player projectiles");
		scheduler.addTask<processProjectileHits>("Process projectile hits");
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<clearProjectileHits>("Clear projectile hits.");
		scheduler.addTask<tickProjectiles>("Tick projectiles");
		scheduler.addTask<processProjectileHits>("Process projectile hits");
	}

	void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor&, Graphics& graphics, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		ProjectileDebugVisualization& projectileDebugVisualization =
			const_cast<ProjectileDebugVisualization&>(worldView.subtickResources.getResource<ProjectileDebugVisualization>());
		if (graphics.clientPhysicsDebugVisualization) {
			projectileDebugVisualization.physicsDebugVisualization.putWorldVisualizationInstances(graphics.renderer3D, graphics.visibleInstances3D);
		} else {
			projectileDebugVisualization.physicsDebugVisualization.clear();
		}
	}

private:
	struct Hit {
		SynchronizedEntityID synchronizedEntityID;
		EntityType entityType;
		phys::Position3D point;
		phys::Direction3D direction;
		phys::Direction3D normal;
		phys::Distance distance;
	};

	struct ProjectileHits {
		ArrayList<Pair<EntityID, Hit>> hits{};
	};

	struct ProjectilesToDestroy {
		ArrayList<EntityID> entities{};
	};

	struct ProjectileDebugVisualization {
		phys::DebugVisualization3D physicsDebugVisualization{};
	};

	static void integrateProjectileKinematics(phys::Position3D& position, phys::LinearVelocity3D& linearVelocity, phys::Time deltaTime) {
		const phys::LinearVelocity3D oldLinearVelocity = linearVelocity;
		linearVelocity[phys::Y] -= GRAVITY_ACCELERATION * deltaTime;
		// TODO: Air drag.
		position += midpoint(oldLinearVelocity, linearVelocity) * deltaTime;
	}

	[[nodiscard]] static Optional<Hit> detectHit(ProjectileDebugVisualization& projectileDebugVisualization,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D, const SynchronizedEntityID,
			const EntityType, Exclude<ParticleType>> targetEntities,
		const phys::Broadphase3D& broadphase, SynchronizedEntityID projectileOwner, phys::Position3D oldProjectilePosition, phys::Position3D newProjectilePosition) {
		const phys::Length3D difference = newProjectilePosition - oldProjectilePosition;
		const phys::SquaredDistance squaredDistance = length2(difference);
		if (squaredDistance < phys::SquaredDistance::MACHINE_EPSILON) {
			return {};
		}
		projectileDebugVisualization.physicsDebugVisualization.drawWorldLineSegment(oldProjectilePosition, newProjectilePosition, Color::LIME);
		projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(newProjectilePosition, Color::LIME, 1.5_x);
		const phys::Distance maxRayDistance = sqrt(squaredDistance);
		const phys::Direction3D rayDirection = phys::Direction3D::reinterpret(difference * (1_x / maxRayDistance));
		const phys::Ray3D ray{.origin = oldProjectilePosition, .direction = rayDirection, .maxDistance = maxRayDistance};

		Optional<Hit> result{};
		broadphase.traverseEntities(
			[&](const EntityID& objectID) -> bool {
				if (!targetEntities.containsEntity(objectID)) {
					return false;
				}

				const auto& [entityID, position, orientation, scale, collider, bounds, synchronizedEntityID, entityType] = targetEntities[objectID];
				if (synchronizedEntityID == projectileOwner || !bounds.boundingBox.intersects(ray)) {
					return false;
				}

				GREM_MATCH(phys::raycast(ray, phys::CollisionFilter{}, collider, translateRotateScale(position, orientation, scale), phys::CollisionFilterTest::RESPONSE).first) {
					GREM_CASE(const phys::RayMiss& miss) break;
					GREM_CASE(const phys::RayHit3D& hit) {
						const phys::Position3D point = ray.origin + ray.direction * hit.distance;
						projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(point, Color::RED, 2.4_x);
						projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(point, hit.normal, Color::DARK_RED, 0.9_x, 2.4_x);
						if (!result || hit.distance < result->distance) {
							GREM_ASSERT(abs(length(hit.normal) - 1.0f) < 0.0001f);
							result = Hit{
								.synchronizedEntityID = synchronizedEntityID,
								.entityType = entityType,
								.point = point,
								.direction = ray.direction,
								.normal = hit.normal,
								.distance = hit.distance,
							};
						}
						break;
					}
					GREM_CASE(const phys::RayHitInterior3D& hitInterior) {
						projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(ray.origin, Color::RED, 2.4_x);
						projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(ray.origin, -ray.direction, Color::DARK_RED, 0.9_x, 2.4_x);
						result = Hit{
							.synchronizedEntityID = synchronizedEntityID,
							.entityType = entityType,
							.point = ray.origin,
							.direction = ray.direction,
							.normal = -ray.direction,
							.distance{},
						};
						break;
					}
				}
				return false;
			},
			[&](const phys::Box3D& boundingBox) -> bool { return boundingBox.intersects(ray); });
		return result;
	}

	static Optional<Hit> detectLagCompensatedHit(ProjectileDebugVisualization& projectileDebugVisualization, const WorldView& worldView,
		SynchronizedEntityID projectileSynchronizedEntityID, SynchronizedEntityID projectileOwner, const phys::Ray3D& ray) {
		if (!worldView.predictionInterpolation) {
			return {};
		}
		Optional<Hit> result{};
		const SnapshotView closestSnapshot =
			(worldView.predictionInterpolation->interpolationAlpha < 0.5f) ? worldView.predictionInterpolation->snapshotA : worldView.predictionInterpolation->snapshotB;
		closestSnapshot.resources.getResource<phys::Broadphase3D>().traverseEntities(
			[&](const EntityID& objectID) -> bool {
				if (closestSnapshot.registry.hasComponent<ParticleType>(objectID)) {
					return false;
				}

				const SynchronizedEntityID* const synchronizedEntityID = closestSnapshot.registry.findComponent<SynchronizedEntityID>(objectID);
				const EntityType* const entityType = closestSnapshot.registry.findComponent<EntityType>(objectID);
				if (!synchronizedEntityID || !entityType || *synchronizedEntityID >= projectileSynchronizedEntityID || *synchronizedEntityID == projectileOwner) {
					return false;
				}

				const Optional<InterpolatedEntityView> entity = worldView.findEntity(objectID.getFlags(), *synchronizedEntityID, *entityType);
				if (!entity) {
					return false;
				}

				const phys::Box3D& boundingBoxA = entity->getOldComponent<phys::ObjectBounds3D>().boundingBox;
				const phys::Box3D& boundingBoxB = entity->getNewComponent<phys::ObjectBounds3D>().boundingBox;
				const phys::Box3D expandedBox{.min = min(boundingBoxA.min, boundingBoxB.min), .max = max(boundingBoxA.max, boundingBoxB.max)};
				if (!expandedBox.intersects(ray)) {
					return false;
				}

				const phys::Collider3D& collider = entity->getNewComponent<phys::Collider3D>();
				const phys::Position3D position = entity->getInterpolatedComponentWithMargin<phys::Position3D>(SnapshotInterpolationView::TELEPORTATION_MARGIN);
				const phys::Orientation3D orientation = entity->getInterpolatedComponent<phys::Orientation3D>();
				const phys::Scale3D scale = entity->getInterpolatedComponent<phys::Scale3D>();
				GREM_MATCH(phys::raycast(ray, phys::CollisionFilter{}, collider, translateRotateScale(position, orientation, scale), phys::CollisionFilterTest::RESPONSE).first) {
					GREM_CASE(const phys::RayMiss& miss) break;
					GREM_CASE(const phys::RayHit3D& hit) {
						const phys::Position3D point = ray.origin + ray.direction * hit.distance;
						projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(point, Color::RED, 2.4_x);
						projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(point, hit.normal, Color::DARK_RED, 0.9_x, 2.4_x);
						if (!result || hit.distance < result->distance) {
							GREM_ASSERT(abs(length(hit.normal) - 1.0f) < 0.0001f);
							result = Hit{
								.synchronizedEntityID = *synchronizedEntityID,
								.entityType = *entityType,
								.point = point,
								.direction = ray.direction,
								.normal = hit.normal,
								.distance = hit.distance,
							};
						}
						break;
					}
					GREM_CASE(const phys::RayHitInterior3D& hitInterior) {
						projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(ray.origin, Color::RED, 2.4_x);
						projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(ray.origin, -ray.direction, Color::DARK_RED, 0.9_x, 2.4_x);
						result = Hit{
							.synchronizedEntityID = *synchronizedEntityID,
							.entityType = *entityType,
							.point = ray.origin,
							.direction = ray.direction,
							.normal = -ray.direction,
							.distance{},
						};
						break;
					}
				}
				return false;
			},
			[&](const phys::Box3D& boundingBox) -> bool { return boundingBox.getExpanded(phys::Length3D{MAX_LAG_COMPENSATION_DISTANCE_PER_TICK}).intersects(ray); });
		return result;
	}

	[[nodiscard]] static Optional<Hit> fastForwardProjectile(ProjectileDebugVisualization& projectileDebugVisualization, ProjectileState& projectileState,
		SynchronizedEntityID synchronizedEntityID, const EntityRegistry& registry, const ResourceRegistry& resources, const CurrentPlayer& currentPlayer, Timestamp startTimestamp,
		Timestamp endTimestamp, Duration tickInterval) {
		GREM_ASSERT(tickInterval > Duration{});
		const phys::Position3D oldProjectilePosition = projectileState.position;
		fastForward(startTimestamp, endTimestamp, tickInterval,
			[&](Timestamp, Duration deltaTime) -> void { integrateProjectileKinematics(projectileState.position, projectileState.linearVelocity, deltaTime); });
		const phys::Position3D newProjectilePosition = projectileState.position;

		const phys::Length3D difference = newProjectilePosition - oldProjectilePosition;
		const phys::SquaredDistance squaredDistance = length2(difference);
		if (squaredDistance < phys::SquaredDistance::MACHINE_EPSILON) {
			return {};
		}
		projectileDebugVisualization.physicsDebugVisualization.drawWorldLineSegment(oldProjectilePosition, newProjectilePosition, Color::LIME);
		projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(newProjectilePosition, Color::LIME, 1.5_x);
		const phys::Distance maxRayDistance = sqrt(squaredDistance);
		const phys::Direction3D rayDirection = phys::Direction3D::reinterpret(difference * (1_x / maxRayDistance));
		const phys::Ray3D ray{.origin = oldProjectilePosition, .direction = rayDirection, .maxDistance = maxRayDistance};

		const WorldView worldView = currentPlayer.getWorldViewAtTimestamp(registry, resources, {}, startTimestamp);
		return detectLagCompensatedHit(projectileDebugVisualization, worldView, synchronizedEntityID, projectileState.owner, ray);
	}

	static void clearProjectileHits(ProjectileHits& projectileHits) {
		projectileHits.hits.clear();
	}

	static void fireCurrentPlayerProjectiles(EntityRegistry& registry, ResourceRegistry& resources, ProjectileDebugVisualization& projectileDebugVisualization,
		ProjectileHits& projectileHits, const Schema& schema, const SynchronizedEntityMap& synchronizedEntityMap, const PlayerEntityMap& playerEntityMap,
		const CurrentPlayer& currentPlayer, Duration tickInterval) {
		const Timestamp minInterpolationTimestamp = currentPlayer.subtickBeginTimestamp.withTimeAdded(-MAX_LAG_COMPENSATION_DURATION, tickInterval);

		playerEntityMap.forEachPlayerEntity(currentPlayer.playerID, currentPlayer.localPlayerID, [&](EntityID entityID) -> void {
			const SynchronizedEntityID* const synchronizedPlayerEntityID = registry.findComponent<SynchronizedEntityID>(entityID);
			const Aim* const playerAim = registry.findComponent<Aim>(entityID);
			if (!synchronizedPlayerEntityID || !playerAim) {
				return;
			}

			const Timestamp receivedInterpolationTimestamp = max(currentPlayer.receivedInterpolationTimestamp, minInterpolationTimestamp);
			const Timestamp predictionInterpolationTimestamp = max(currentPlayer.predictionInterpolationTimestamp, minInterpolationTimestamp);
			const Optional<SnapshotInterpolationView> predictionInterpolation =
				currentPlayer.predictionSnapshots.getInterpolationView(predictionInterpolationTimestamp, tickInterval);
			if (!predictionInterpolation) {
				return;
			}

			const Optional<InterpolatedEntityView> playerEntity = predictionInterpolation->findEntity(*synchronizedPlayerEntityID);
			if (!playerEntity || !playerEntity->hasComponent<phys::Position3D>() || !playerEntity->hasComponent<Inventory>()) {
				return;
			}

			const SynchronizedEntityID synchronizedWeaponEntityID = playerEntity->getNewAttribute<&Inventory::equippedWeapon>();
			const EntityID weaponEntityID = synchronizedEntityMap.findEntity(registry, synchronizedWeaponEntityID);
			if (!weaponEntityID) {
				return;
			}

			WeaponIntermediateState* const weaponIntermediateState = registry.findComponent<WeaponIntermediateState>(weaponEntityID);
			const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
			const WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
			if (!weaponIntermediateState || !weaponType || !weaponState) {
				return;
			}

			const WeaponDescription& weaponDescription = schema.getWeaponDescription(*weaponType);
			const phys::Position3D playerPosition = playerEntity->getInterpolatedComponentWithMargin<phys::Position3D>(SnapshotInterpolationView::TELEPORTATION_MARGIN);
			const phys::Position3D projectilePosition = playerPosition + playerAim->offset;
			const phys::PitchYawRotations aimDeviation = weaponState->recoilInducedAimDeviation + weaponState->rotationInducedAimDeviation;
			const phys::PitchYaw projectileAngles = playerAim->angles + aimDeviation;
			const phys::LinearVelocity3D projectileVelocity = convertAnglesToForwardDirection(projectileAngles) * weaponDescription.muzzleVelocity;
			for (size_t i = 0; i < weaponIntermediateState->projectilesToFire; ++i) {
				const auto [projectileEntityID, synchronizedProjectileEntityID] = spawnEntity(registry, resources, EntityType{"PROJECTILE"}, EntityID::Flags{},
					ComponentInitializers{
						ProjectileType{weaponDescription.projectileType},
						ProjectileState{
							.owner = *synchronizedPlayerEntityID,
							.position = projectilePosition,
							.previousPosition = projectilePosition,
							.linearVelocity = projectileVelocity,
						},
						DestroyCountdown{
							.destroyOnTickIndex = currentPlayer.subtickBeginTimestamp.withTimeAdded(weaponDescription.projectileLifeTime, tickInterval).getTickIndex()},
					});
				if (ProjectileState* const projectileState = registry.findComponent<ProjectileState>(projectileEntityID)) {
					if (registry.hasComponent<ProjectileType>(projectileEntityID)) {
						const Duration receivedInterpolationTimeDifference = getTimeBetween(receivedInterpolationTimestamp, currentPlayer.subtickBeginTimestamp, tickInterval);
						const Timestamp beginTimestamp = currentPlayer.subtickBeginTimestamp;
						const Timestamp endTimestamp = beginTimestamp.withTimeAdded(receivedInterpolationTimeDifference, tickInterval);
						if (const Optional<Hit> hit = fastForwardProjectile(projectileDebugVisualization, *projectileState, synchronizedProjectileEntityID, registry, resources,
								currentPlayer, beginTimestamp, endTimestamp, tickInterval)) {
							projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(hit->point, Color::GREEN, 2.5_x);
							projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(hit->point, hit->normal, Color::FOREST_GREEN, 1_x, 2.5_x);
							projectileHits.hits.emplace_back(projectileEntityID, *hit);
						}
					}
				}
			}
		});
	}

	static void tickProjectiles(Entities<ProjectileState> projectileEntities,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D, const SynchronizedEntityID,
			const EntityType, Exclude<ParticleType>>
			targetEntities,
		ProjectileDebugVisualization& projectileDebugVisualization, ProjectileHits& projectileHits, const phys::Broadphase3D& broadphase, Duration tickInterval) {
		for (auto&& [entityID, projectileState] : projectileEntities) {
			const phys::Position3D oldPosition = projectileState.position;
			projectileState.previousPosition = oldPosition;
			integrateProjectileKinematics(projectileState.position, projectileState.linearVelocity, tickInterval);
			if (const Optional<Hit> hit = detectHit(projectileDebugVisualization, targetEntities, broadphase, projectileState.owner, oldPosition, projectileState.position)) {
				projectileDebugVisualization.physicsDebugVisualization.drawWorldPoint(hit->point, Color::GREEN, 2.5_x);
				projectileDebugVisualization.physicsDebugVisualization.drawWorldVector(hit->point, hit->normal, Color::FOREST_GREEN, 1_x, 2.5_x);
				projectileHits.hits.emplace_back(entityID, *hit);
			}
		}
	}

	static void processProjectileHits(EntityRegistry& registry, ResourceRegistry& resources, Events& events, const ProjectileHits& projectileHits, const Schema& schema,
		const SynchronizedEntityMap& synchronizedEntityMap, TickIndex tickIndex, Duration tickInterval) {
		rng::Xoroshiro128PlusPlusEngine numberGenerator{static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(tickIndex - TickIndex{})};
		for (const auto& [projectileEntityID, hit] : projectileHits.hits) {
			const ProjectileDescription& projectileDescription = schema.getProjectileDescription(registry.getComponent<ProjectileType>(projectileEntityID));
			const phys::LinearVelocity3D projectileLinearVelocity = registry.getComponent<ProjectileState>(projectileEntityID).linearVelocity;
			registry.destroyEntity(projectileEntityID);

			const EntityID hitEntityID = synchronizedEntityMap.findEntity(registry, hit.synchronizedEntityID, hit.entityType);
			if (!hitEntityID) {
				continue;
			}

			const phys::Position3D hitObjectPosition = registry.getComponent<phys::Position3D>(hitEntityID);
			const phys::Orientation3D hitObjectOrientation = registry.getComponent<phys::Orientation3D>(hitEntityID);
			const phys::LinearVelocity3D hitObjectLinearVelocity = registry.getComponent<phys::LinearVelocity3D>(hitEntityID);
			const phys::AngularVelocity3D hitObjectAngularVelocity = registry.getComponent<phys::AngularVelocity3D>(hitEntityID);
			const phys::InverseMass hitObjectInverseMass = registry.getComponent<phys::InverseMass>(hitEntityID);
			const phys::InverseMomentOfInertiaTensor3D& hitObjectInverseMomentOfInertiaTensor = registry.getComponent<phys::InverseMomentOfInertiaTensor3D>(hitEntityID);
			const phys::ObjectActivity hitObjectActivity = registry.getComponent<phys::ObjectActivity>(hitEntityID);

			const phys::Orientation3D particleOrientation = phys::Orientation3D::lookAt(hit.normal, phys::Y_AXIS_3D);
			const phys::Orientation3D decalOrientation = phys::Orientation3D::lookAt(-hit.normal, phys::Y_AXIS_3D);

			if (const DamageableType* const damageableType = registry.findComponent<DamageableType>(hitEntityID); damageableType && *damageableType != DamageableType{}) {
				const DamageableDescription& damageableDescription = schema.getDamageableDescription(*damageableType);

				if (DamageableState* const damageableState = registry.findComponent<DamageableState>(hitEntityID)) {
					damageableState->lastDamagedOnTickIndex = tickIndex;
				}

				if (Health* const health = registry.findComponent<Health>(hitEntityID)) {
					if (health->health > 0.0f) {
						health->health -= projectileDescription.damage;
						if (health->health <= 0.0f) {
							health->health = 0.0f;
							for (const ParticleType particleType : damageableDescription.impactParticleTypes) {
								spawnParticle(registry, resources, numberGenerator, particleType, EntityID::Flags{}, hit.point, particleOrientation, hitObjectLinearVelocity,
									hitEntityID, tickIndex, tickInterval);
							}
							for (const ParticleType particleType : damageableDescription.deathParticleTypes) {
								spawnParticle(registry, resources, numberGenerator, particleType, EntityID::Flags{}, hitObjectPosition, hitObjectOrientation,
									hitObjectLinearVelocity, hitEntityID, tickIndex, tickInterval);
							}
							if (damageableDescription.unkillable) {
								if (damageableDescription.deathSound != SoundType{}) {
									events.push_back(EntityParentedSoundPlayedEvent{.soundType = damageableDescription.deathSound, .emitter = hit.synchronizedEntityID});
								}
							} else {
								if (damageableDescription.deathSound != SoundType{}) {
									const PlayerID playerID = registry.getComponentOr<PlayerID>(hitEntityID, PlayerID{});
									if (playerID) {
										events.push_back(PlayerAssociatedWorldSpaceSoundPlayedEvent{
											.soundType = damageableDescription.deathSound,
											.position = hitObjectPosition,
											.associatedPlayerID = playerID,
										});
									} else {
										events.push_back(WorldSpaceSoundPlayedEvent{
											.soundType = damageableDescription.deathSound,
											.position = hitObjectPosition,
										});
									}
								}
								killEntity(registry, resources, hitEntityID);
							}
							continue;
						}
					}
				}

				const phys::Speed speedLoss = damageableDescription.speedLossPerUnitDamage * projectileDescription.damage;
				if (speedLoss > 0) {
					phys::LinearVelocity3D& linearVelocity = registry.getComponent<phys::LinearVelocity3D>(hitEntityID);
					const phys::Speed speed = length(linearVelocity);
					if (speed <= speedLoss) {
						linearVelocity = {};
					} else {
						linearVelocity *= (speed - speedLoss) / speed;
					}
				}

				if (damageableDescription.impactDecalMaterialType != DecalMaterialType{}) {
					spawnDecal(registry, resources, damageableDescription.impactDecalMaterialType, EntityID::Flags{}, hit.point, decalOrientation,
						damageableDescription.impactDecalSize, damageableDescription.impactDecalRange, hitEntityID, tickIndex, tickInterval);
				}

				for (const ParticleType particleType : damageableDescription.impactParticleTypes) {
					spawnParticle(registry, resources, numberGenerator, particleType, EntityID::Flags{}, hit.point, particleOrientation, hitObjectLinearVelocity, hitEntityID,
						tickIndex, tickInterval);
				}
				continue;
			}

			if (hitObjectActivity.isCorrectable != 0) {
				if (const Optional<phys::Direction3D> projectileDirection = tryNormalize(projectileLinearVelocity)) {
					const phys::Direction3D counterProjectileDirection = -*projectileDirection;
					const phys::Length3D hitObjectOffset = hit.point - hitObjectPosition;
					const phys::LinearVelocity1D relativeVelocity =
						dot(hitObjectLinearVelocity + cross(hitObjectAngularVelocity, hitObjectOffset) - projectileLinearVelocity, counterProjectileDirection);
					const phys::MomentArm3D hitObjectMomentArm = cross(hitObjectOffset, counterProjectileDirection);
					const phys::InverseMass hitObjectGeneralizedInverseMass =
						hitObjectInverseMass + dot(hitObjectMomentArm, hitObjectInverseMomentOfInertiaTensor * hitObjectMomentArm);
					const phys::InverseMass projectileGeneralizedInverseMass = 1_x / projectileDescription.mass;
					const phys::InverseMass combinedGeneralizedInverseMass = hitObjectGeneralizedInverseMass + projectileGeneralizedInverseMass;
					const phys::Mass combinedGeneralizedMass = 1_x / combinedGeneralizedInverseMass;
					const phys::LinearImpulse1D impulse = -relativeVelocity * combinedGeneralizedMass;
					const phys::LinearImpulse3D linearImpulse = counterProjectileDirection * impulse;
					registry.getComponent<phys::ObjectActivity>(hitEntityID).wasCorrected = 1;
					registry.getComponent<phys::ObjectActivity>(hitEntityID).energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;
					registry.getComponent<phys::LinearVelocity3D>(hitEntityID) += hitObjectInverseMass * linearImpulse;
					registry.getComponent<phys::AngularVelocity3D>(hitEntityID) += hitObjectInverseMomentOfInertiaTensor * cross(hitObjectOffset, linearImpulse);
				}
			}

			if (projectileDescription.impactDecalMaterialType != DecalMaterialType{}) {
				spawnDecal(registry, resources, projectileDescription.impactDecalMaterialType, EntityID::Flags{}, hit.point, decalOrientation,
					projectileDescription.impactDecalSize, projectileDescription.impactDecalRange, hitEntityID, tickIndex, tickInterval);
			}

			for (const ParticleType particleType : projectileDescription.impactParticleTypes) {
				spawnParticle(registry, resources, numberGenerator, particleType, EntityID::Flags{}, hit.point, particleOrientation, hitObjectLinearVelocity, hitEntityID,
					tickIndex, tickInterval);
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createProjectileHitDetectionSystem() { // NOLINT(misc-use-internal-linkage)
	return new ProjectileHitDetectionSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
