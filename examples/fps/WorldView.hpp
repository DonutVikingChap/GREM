// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_WORLD_VIEW_HPP
#define GREM_EXAMPLES_FPS_WORLD_VIEW_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/physics/quantities.hpp>

#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "Snapshot.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "game_components.hpp"
#include "game_data.hpp"
#include "game_functions.hpp"

struct WorldView {
	const EntityRegistry& subtickRegistry;
	const ResourceRegistry& subtickResources;
	Optional<SnapshotInterpolationView> receivedInterpolation;
	Optional<SnapshotInterpolationView> predictionInterpolation;
	Timestamp receivedInterpolationTimestamp;
	Timestamp predictionInterpolationTimestamp;
	Timestamp subtickTimestamp;
	Duration tickInterval;
	PlayerID playerID;
	StridedSpan<const Frustum<float>> frustums;

	[[nodiscard]] bool isPotentiallyVisible(const phys::Sphere3D& sphere) const {
		return frustums.empty() || anyOf(frustums, [&](const Frustum<float>& frustum) -> bool { return frustum.isPotentiallyIntersecting(sphere.in(phys::METERS)); });
	}

	[[nodiscard]] bool isPotentiallyVisible(const phys::Box3D& box) const {
		return frustums.empty() || anyOf(frustums, [&](const Frustum<float>& frustum) -> bool { return frustum.isPotentiallyIntersecting(box.in(phys::METERS)); });
	}

	[[nodiscard]] phys::Sphere3D getBindPoseBoundingSphere(const gfx::Model3D& model, phys::Position3D position, phys::Coefficient scale) const {
		return {.center = position, .radius = model.getBindPoseBoundingRadius() * scale * phys::METERS};
	}

	[[nodiscard]] phys::Box3D getBindPoseBoundingBox(const gfx::Model3D& model, phys::Position3D position, phys::Orientation3D orientation, phys::Scale3D scale) const {
		return getTransformedBoundingBox(translateRotateScale(position, orientation, scale), model.getBindPoseBoundingBox() * phys::METERS);
	}

	[[nodiscard]] phys::Box3D getBindPoseBoundingBox(const gfx::Model3D& model, res::Model::TransformationView transformation) const {
		return getTransformedBoundingBox(transformation.jointMatrices[0], model.getBindPoseBoundingBox()) * phys::METERS;
	}

	[[nodiscard]] phys::Box3D getBoundingBox(const gfx::Model3D& model, res::Model::TransformationView transformation) const {
		Box<3, float> boundingBox{.min{Limits<float>::MAX}, .max{Limits<float>::MIN}};
		for (const gfx::Model3D::Node& node : model.getNodes()) {
			const mat4 jointMatrix = transformation.jointMatrices[((node.shaderConfiguration.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : node.jointIndex];
			const Box<3, float> nodeBoundingBox = getTransformedBoundingBox(jointMatrix, node.boundingBox);
			boundingBox.min = min(boundingBox.min, nodeBoundingBox.min);
			boundingBox.max = max(boundingBox.max, nodeBoundingBox.max);
		}
		return boundingBox * phys::METERS;
	}

	[[nodiscard]] bool isBindPoseBoundingSpherePotentiallyVisible(const gfx::Model3D& model, phys::Position3D position, phys::Coefficient scale) const {
		return isPotentiallyVisible(getBindPoseBoundingSphere(model, position, scale));
	}

	[[nodiscard]] bool isBindPoseBoundingBoxPotentiallyVisible(const gfx::Model3D& model, phys::Position3D position, phys::Orientation3D orientation, phys::Scale3D scale) const {
		return isPotentiallyVisible(getBindPoseBoundingBox(model, position, orientation, scale));
	}

	[[nodiscard]] bool isBindPoseBoundingBoxPotentiallyVisible(const gfx::Model3D& model, res::Model::TransformationView transformation) const {
		return isPotentiallyVisible(getBindPoseBoundingBox(model, transformation));
	}

	[[nodiscard]] bool isBoundingBoxPotentiallyVisible(const gfx::Model3D& model, res::Model::TransformationView transformation) const {
		return isPotentiallyVisible(getBoundingBox(model, transformation));
	}

	[[nodiscard]] Timestamp getEntityDisplayTimestamp(EntityID::Flags entityFlags) const {
		if ((entityFlags & ENTITY_DISPLAY_SUBTICK_PREDICTED) != 0) {
			return subtickTimestamp;
		}
		if ((entityFlags & ENTITY_DISPLAY_PREDICTED) != 0) {
			return predictionInterpolationTimestamp;
		}
		return receivedInterpolationTimestamp;
	}

	[[nodiscard]] Optional<InterpolatedEntityView> findEntity(EntityID::Flags entityFlags, SynchronizedEntityID synchronizedEntityID) const {
		if ((entityFlags & ENTITY_DISPLAY_SUBTICK_PREDICTED) != 0) {
			if (const EntityID entityID = subtickResources.getResource<SynchronizedEntityMap>().findEntity(subtickRegistry, synchronizedEntityID)) {
				const SnapshotView subtickSnapshot{.registry = subtickRegistry, .resources = subtickResources};
				return InterpolatedEntityView{.snapshotA = subtickSnapshot, .snapshotB = subtickSnapshot, .interpolationAlpha = 0.0f, .entityIDs{entityID, entityID}};
			}
			return {};
		}
		if ((entityFlags & ENTITY_DISPLAY_PREDICTED) != 0) {
			if (predictionInterpolation) {
				return predictionInterpolation->findEntity(synchronizedEntityID);
			}
			return {};
		}
		if (receivedInterpolation) {
			return receivedInterpolation->findEntity(synchronizedEntityID);
		}
		return {};
	}

	[[nodiscard]] Optional<InterpolatedEntityView> findEntity(EntityID::Flags entityFlags, SynchronizedEntityID synchronizedEntityID, EntityType entityType) const {
		if ((entityFlags & ENTITY_DISPLAY_SUBTICK_PREDICTED) != 0) {
			if (const EntityID entityID = subtickResources.getResource<SynchronizedEntityMap>().findEntity(subtickRegistry, synchronizedEntityID, entityType)) {
				const SnapshotView subtickSnapshot{.registry = subtickRegistry, .resources = subtickResources};
				return InterpolatedEntityView{.snapshotA = subtickSnapshot, .snapshotB = subtickSnapshot, .interpolationAlpha = 0.0f, .entityIDs{entityID, entityID}};
			}
			return {};
		}
		if ((entityFlags & ENTITY_DISPLAY_PREDICTED) != 0) {
			if (predictionInterpolation) {
				return predictionInterpolation->findEntity(synchronizedEntityID, entityType);
			}
			return {};
		}
		if (receivedInterpolation) {
			return receivedInterpolation->findEntity(synchronizedEntityID, entityType);
		}
		return {};
	}

	template <typename... ComponentsAndExclusions>
	void forEachEntityWithComponents(auto callback) const {
		forEachEntitySeparatedByRegistryWithComponents<ComponentsAndExclusions...>(callback, callback, callback);
	}

	template <typename... ComponentsAndExclusions>
	void forEachEntitySeparatedByRegistryWithComponents(auto subtickCallback, auto predictionInterpolationCallback, auto receivedInterpolationCallback) const {
		for (const auto& entityReference : subtickRegistry.getEntities<const SynchronizedEntityID, const EntityType, ComponentsAndExclusions...>()) {
			const EntityID entityID = get<0>(entityReference);
			if ((entityID.getFlags() & ENTITY_DISPLAY_SUBTICK_PREDICTED) != 0) {
				const SynchronizedEntityID synchronizedEntityID = get<1>(entityReference);
				const EntityType entityType = get<2>(entityReference);
				const SnapshotView subtickSnapshot{.registry = subtickRegistry, .resources = subtickResources};
				const InterpolatedEntityView entity{.snapshotA = subtickSnapshot, .snapshotB = subtickSnapshot, .interpolationAlpha = 0.0f, .entityIDs{entityID, entityID}};
				subtickCallback(synchronizedEntityID, entityType, entity);
			}
		}
		if (predictionInterpolation) {
			const SynchronizedEntityMap& predictionSynchronizedEntityMapA = predictionInterpolation->snapshotA.resources.getResource<SynchronizedEntityMap>();
			for (const auto& entityReferenceB :
				predictionInterpolation->snapshotB.registry.getEntities<const SynchronizedEntityID, const EntityType, ComponentsAndExclusions...>()) {
				const EntityID entityIDB = get<0>(entityReferenceB);
				if ((entityIDB.getFlags() & (ENTITY_DISPLAY_SUBTICK_PREDICTED | ENTITY_DISPLAY_PREDICTED)) == ENTITY_DISPLAY_PREDICTED) {
					const SynchronizedEntityID synchronizedEntityID = get<1>(entityReferenceB);
					const EntityType entityType = get<2>(entityReferenceB);
					const auto itA = predictionSynchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
					const InterpolatedEntityView entity =
						(itA != predictionSynchronizedEntityMapA.synchronizedEntityMappings.end() && itA->second.type == entityType && predictionInterpolation->snapshotA.registry.containsEntity(itA->second.id)) ?
						InterpolatedEntityView{
							.snapshotA = predictionInterpolation->snapshotA,
							.snapshotB = predictionInterpolation->snapshotB,
							.interpolationAlpha = predictionInterpolation->interpolationAlpha,
							.entityIDs{itA->second.id, entityIDB},
						}: InterpolatedEntityView{
							.snapshotA = predictionInterpolation->snapshotB,
							.snapshotB = predictionInterpolation->snapshotB,
							.interpolationAlpha = 0.0f,
							.entityIDs{entityIDB, entityIDB},
						};
					predictionInterpolationCallback(synchronizedEntityID, entityType, entity);
				}
			}
		}
		if (receivedInterpolation) {
			const SynchronizedEntityMap& receivedSynchronizedEntityMapA = receivedInterpolation->snapshotA.resources.getResource<SynchronizedEntityMap>();
			for (const auto& entityReferenceB : receivedInterpolation->snapshotB.registry.getEntities<const SynchronizedEntityID, const EntityType, ComponentsAndExclusions...>()) {
				const EntityID entityIDB = get<0>(entityReferenceB);
				if ((entityIDB.getFlags() & (ENTITY_DISPLAY_SUBTICK_PREDICTED | ENTITY_DISPLAY_PREDICTED)) == 0) {
					const SynchronizedEntityID synchronizedEntityID = get<1>(entityReferenceB);
					const EntityType entityType = get<2>(entityReferenceB);
					const auto itA = receivedSynchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
					const InterpolatedEntityView entity =
						(itA != receivedSynchronizedEntityMapA.synchronizedEntityMappings.end() && itA->second.type == entityType && receivedInterpolation->snapshotA.registry.containsEntity(itA->second.id)) ?
						InterpolatedEntityView{
							.snapshotA = receivedInterpolation->snapshotA,
							.snapshotB = receivedInterpolation->snapshotB,
							.interpolationAlpha = receivedInterpolation->interpolationAlpha,
							.entityIDs{itA->second.id, entityIDB},
						}: InterpolatedEntityView{
							.snapshotA = receivedInterpolation->snapshotB,
							.snapshotB = receivedInterpolation->snapshotB,
							.interpolationAlpha = 0.0f,
							.entityIDs{entityIDB, entityIDB},
						};
					receivedInterpolationCallback(synchronizedEntityID, entityType, entity);
				}
			}
		}
	}

	[[nodiscard]] phys::Orientation3D getAlignedOrientation(phys::Orientation3D orientation, OrientationAlignment orientationAlignment, const gfx::Camera3D& camera) const {
		switch (orientationAlignment) {
			case OrientationAlignment::NONE: break;
			case OrientationAlignment::ALIGN_WITH_CAMERA: {
				const phys::OrthonormalBasis3D cameraBasis = phys::OrthonormalBasis3D::reinterpret(transpose(mat3{camera.getViewMatrix()}));
				orientation = phys::Orientation3D::fromBasis(cameraBasis);
				break;
			}
			case OrientationAlignment::ALIGN_Z_WITH_CAMERA_Z_AROUND_WORLD_Y: {
				const phys::OrthonormalBasis3D basis = rotate(orientation);
				const phys::OrthonormalBasis3D cameraBasis = phys::OrthonormalBasis3D::reinterpret(transpose(mat3{camera.getViewMatrix()}));
				orientation = phys::Orientation3D::yaw(getAngleDifferenceAroundAxis(phys::Y_AXIS_3D, basis[phys::Z], cameraBasis[phys::Z])) * orientation;
				break;
			}
			case OrientationAlignment::ALIGN_Z_WITH_CAMERA_Z_AROUND_LOCAL_Y: {
				const phys::OrthonormalBasis3D basis = rotate(orientation);
				const phys::OrthonormalBasis3D cameraBasis = phys::OrthonormalBasis3D::reinterpret(transpose(mat3{camera.getViewMatrix()}));
				orientation = phys::Orientation3D::angleAxis(getAngleDifferenceAroundAxis(basis[phys::Y], basis[phys::Z], cameraBasis[phys::Z]), basis[phys::Y]) * orientation;
				break;
			}
		}
		return orientation;
	}

	[[nodiscard]] Optional<WorldTransformation> getWeaponEntityDisplayTransformation(InterpolatedEntityView entity, bool subtick = false) const {
		if (!entity.hasComponent<WeaponState>() || !entity.hasComponent<WeaponType>()) {
			return {};
		}

		const EntityID::Flags entityFlags = (subtick) ? EntityID::Flags{ENTITY_DISPLAY_SUBTICK_PREDICTED} : entity.entityIDs.second.getFlags();
		const SynchronizedEntityID holderSynchronizedEntityID = entity.getNewAttribute<&WeaponState::holder>();
		const Optional<InterpolatedEntityView> holderEntity = findEntity(entityFlags, holderSynchronizedEntityID);
		if (!holderEntity || !holderEntity->hasComponent<phys::Position3D>() || !holderEntity->hasComponent<phys::LinearVelocity3D>() || !holderEntity->hasComponent<Aim>()) {
			return {};
		}

		const Schema& schema = subtickResources.getResource<Schema>();
		const WeaponDescription& weaponDescription = schema.getWeaponDescription(entity.getNewComponent<WeaponType>());
		const Timestamp displayTimestamp = getEntityDisplayTimestamp(entityFlags);

		const phys::Coefficient decayedCrouchAmount = entity.getInterpolatedAttribute<&WeaponState::decayedCrouchAmount>();
		const phys::Angle bobbingPhase =
			(holderEntity->hasComponent<MovementState>()) ? holderEntity->getInterpolatedAttributeWithMargin<&MovementState::bobbingPhase>(90_degrees) : phys::Angle{};
		const PlayerID* const holderPlayerID = (holderEntity->hasComponent<PlayerID>()) ? &holderEntity->getNewComponent<PlayerID>() : nullptr;
		const phys::Position3D holderPosition = holderEntity->getInterpolatedComponentWithMargin<phys::Position3D>(SnapshotInterpolationView::TELEPORTATION_MARGIN);
		const phys::Speed holderSpeed = length(holderEntity->getInterpolatedComponent<phys::LinearVelocity3D>());
		const phys::Length3D aimOffset = holderEntity->getInterpolatedAttribute<&Aim::offset>();
		const phys::Length3D decayingVisualAimOffset = holderEntity->getInterpolatedAttribute<&Aim::decayingVisualOffset>();
		const phys::Position3D aimPosition = holderPosition + aimOffset + decayingVisualAimOffset;
		if (holderPlayerID && *holderPlayerID == playerID && entity.hasComponent<SynchronizedEntityID>() && entity.hasComponent<EntityType>()) {
			const SynchronizedEntityID synchronizedEntityID = entity.getNewComponent<SynchronizedEntityID>();
			const EntityType entityType = entity.getNewComponent<EntityType>();
			const EntityID subtickEntityID = subtickResources.getResource<SynchronizedEntityMap>().findEntity(subtickRegistry, synchronizedEntityID, entityType);
			const EntityID subtickHolderEntityID = subtickResources.getResource<SynchronizedEntityMap>().findEntity(subtickRegistry, holderSynchronizedEntityID);
			const WeaponState* const weaponState = subtickRegistry.findComponent<WeaponState>(subtickEntityID);
			const Aim* const holderAim = subtickRegistry.findComponent<Aim>(subtickHolderEntityID);
			if (weaponState && holderAim) {
				const phys::PitchYawRotations aimDeviation = weaponState->recoilInducedAimDeviation + weaponState->rotationInducedAimDeviation;
				const phys::Coefficient smoothingInterpolationAlpha = weaponState->smoothingInterpolationTime / tickInterval;
				const phys::LinearVelocity3D interpolatedSmoothedVelocity = mix(weaponState->previousSmoothedVelocity, weaponState->smoothedVelocity, smoothingInterpolationAlpha);
				const phys::PitchYawRates interpolatedSmoothedAimAngularRates =
					mix(weaponState->previousSmoothedAimAngularRates, weaponState->smoothedAimAngularRates, smoothingInterpolationAlpha);
				const LocalPlayerPerspective* const holderLocalPlayerPerspective = subtickRegistry.findComponent<LocalPlayerPerspective>(subtickHolderEntityID);
				const phys::PitchYaw displayAimAngles = (holderLocalPlayerPerspective && !subtick) ? holderLocalPlayerPerspective->aimAngles : holderAim->angles;
				const phys::Time timeSinceFire = getTimeBetween(weaponState->lastFiredTimestamp, displayTimestamp, tickInterval);
				const phys::PitchYawRates recoilAngularRates = calculateRecoilAngularRates(timeSinceFire, weaponState->recoilStrengthOfLatestShot, weaponDescription);
				return calculateWeaponTransformation(aimPosition, displayAimAngles, aimDeviation, decayedCrouchAmount, weaponState->aimingDownSightsAmount,
					interpolatedSmoothedVelocity, interpolatedSmoothedAimAngularRates, recoilAngularRates, weaponState->reloadTimeRemaining, weaponState->drawTimeRemaining,
					holderSpeed, bobbingPhase, weaponDescription);
			}
		}

		const phys::PitchYawRotations aimDeviation =
			entity.getInterpolatedAttribute<&WeaponState::recoilInducedAimDeviation>() + entity.getInterpolatedAttribute<&WeaponState::rotationInducedAimDeviation>();
		const phys::Coefficient aimingDownSightsAmount = entity.getInterpolatedAttribute<&WeaponState::aimingDownSightsAmount>();
		const phys::LinearVelocity3D smoothedVelocity = entity.getInterpolatedAttribute<&WeaponState::smoothedVelocity>();
		const phys::Time reloadTimeRemaining = entity.getInterpolatedAttributeWithMargin<&WeaponState::reloadTimeRemaining>(weaponDescription.reloadDuration * 0.5_x);
		const phys::Time drawTimeRemaining = entity.getInterpolatedAttribute<&WeaponState::drawTimeRemaining>();
		const phys::PitchYawRates smoothedAimAngularRates = entity.getInterpolatedAttribute<&WeaponState::smoothedAimAngularRates>();
		const phys::PitchYaw aimAngles = holderEntity->getInterpolatedAttribute<&Aim::angles>();
		const phys::Time timeSinceFire = getTimeBetween(entity.getNewAttribute<&WeaponState::lastFiredTimestamp>(), displayTimestamp, tickInterval);
		const phys::PitchYawRates recoilAngularRates =
			calculateRecoilAngularRates(timeSinceFire, entity.getNewAttribute<&WeaponState::recoilStrengthOfLatestShot>(), weaponDescription);
		return calculateWeaponTransformation(aimPosition, aimAngles, aimDeviation, decayedCrouchAmount, aimingDownSightsAmount, smoothedVelocity, smoothedAimAngularRates,
			recoilAngularRates, reloadTimeRemaining, drawTimeRemaining, holderSpeed, bobbingPhase, weaponDescription);
	}

	[[nodiscard]] Optional<WorldTransformation> getProjectileEntityDisplayTransformation(InterpolatedEntityView entity) const {
		if (!entity.hasComponent<ProjectileState>()) {
			return {};
		}

		return WorldTransformation{
			.position = mix(entity.getNewAttribute<&ProjectileState::previousPosition>(), entity.getNewAttribute<&ProjectileState::position>(),
				(predictionInterpolation) ? predictionInterpolation->interpolationAlpha : 1.0f),
			.orientation = phys::Orientation3D::lookAt(normalize(entity.getInterpolatedAttribute<&ProjectileState::linearVelocity>()), phys::Y_AXIS_3D),
			.scale{1_x},
		};
	}

	[[nodiscard]] Optional<WorldTransformation> getPhysicsEntityDisplayTransformation(InterpolatedEntityView entity) const {
		if (!entity.hasComponent<phys::Position3D>()) {
			return {};
		}

		if (entity.hasComponent<ParticleType>() && &entity.snapshotA.registry == &entity.snapshotB.registry && &entity.snapshotA.registry != &subtickRegistry) {
			return {};
		}

		return WorldTransformation{
			.position = entity.getInterpolatedComponent<phys::Position3D>(),
			.orientation = (entity.hasComponent<phys::Orientation3D>()) ? entity.getInterpolatedComponent<phys::Orientation3D>() : phys::Orientation3D{},
			.scale = (entity.hasComponent<phys::Scale3D>()) ? entity.getInterpolatedComponent<phys::Scale3D>() : phys::Scale3D{1_x},
		};
	}

	[[nodiscard]] Optional<WorldTransformation> getEntityDisplayTransformation(InterpolatedEntityView entity) const {
		Optional<WorldTransformation> result = getWeaponEntityDisplayTransformation(entity);
		if (!result) {
			result = getProjectileEntityDisplayTransformation(entity);
			if (!result) {
				result = getPhysicsEntityDisplayTransformation(entity);
			}
		}
		return result;
	}

	[[nodiscard]] LocalPlayerPerspective getLocalPlayerPerspective(LocalPlayerID localPlayerID, phys::PitchYaw visualAimAngles, phys::Distance aimDistance) const {
		phys::Position3D subtickPosition{};
		phys::Position3D subtickAimPosition{};
		phys::LinearVelocity3D subtickLinearVelocity{};
		subtickResources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityID) -> bool {
			const phys::Position3D* const position = subtickRegistry.findComponent<phys::Position3D>(entityID);
			const Aim* const aim = subtickRegistry.findComponent<Aim>(entityID);
			if (position && aim) {
				subtickPosition = *position;
				subtickAimPosition = *position + aim->offset + aim->decayingVisualOffset;
				subtickLinearVelocity = subtickRegistry.getComponentOr<phys::LinearVelocity3D>(entityID, phys::LinearVelocity3D{});
				return true;
			}
			return false;
		});

		if (!predictionInterpolation) {
			return {
				.aimPosition = subtickAimPosition,
				.aimAngles = visualAimAngles,
				.viewPosition = subtickAimPosition - convertAnglesToForwardDirection(visualAimAngles) * aimDistance,
				.position = subtickPosition,
				.linearVelocity = subtickLinearVelocity,
			};
		}

		phys::Position3D positionA{};
		phys::LinearVelocity3D linearVelocityA{};
		phys::Length3D aimOffsetA{};
		phys::Length3D decayingVisualAimOffsetA{};
		predictionInterpolation->snapshotA.resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityIDA) -> bool {
			const phys::Position3D* const position = predictionInterpolation->snapshotA.registry.findComponent<phys::Position3D>(entityIDA);
			const Aim* const aim = predictionInterpolation->snapshotA.registry.findComponent<Aim>(entityIDA);
			if (position && aim) {
				positionA = *position;
				linearVelocityA = predictionInterpolation->snapshotA.registry.getComponentOr<phys::LinearVelocity3D>(entityIDA, phys::LinearVelocity3D{});
				aimOffsetA = aim->offset;
				decayingVisualAimOffsetA = aim->decayingVisualOffset;
				return true;
			}
			return false;
		});

		phys::Position3D positionB{};
		phys::LinearVelocity3D linearVelocityB{};
		phys::Length3D aimOffsetB{};
		phys::Length3D decayingVisualAimOffsetB{};
		predictionInterpolation->snapshotB.resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, localPlayerID, [&](EntityID entityIDB) -> bool {
			const phys::Position3D* const position = predictionInterpolation->snapshotB.registry.findComponent<phys::Position3D>(entityIDB);
			const Aim* const aim = predictionInterpolation->snapshotB.registry.findComponent<Aim>(entityIDB);
			if (position && aim) {
				positionB = *position;
				linearVelocityB = predictionInterpolation->snapshotB.registry.getComponentOr<phys::LinearVelocity3D>(entityIDB, phys::LinearVelocity3D{});
				aimOffsetB = aim->offset;
				decayingVisualAimOffsetB = aim->decayingVisualOffset;
				return true;
			}
			return false;
		});

		const float interpolationAlpha = predictionInterpolation->interpolationAlpha;
		const phys::Position3D position =
			(distance(positionA, positionB) <= SnapshotInterpolationView::TELEPORTATION_MARGIN) ? mix(positionA, positionB, interpolationAlpha) : positionB;
		const phys::LinearVelocity3D linearVelocity = mix(linearVelocityA, linearVelocityB, interpolationAlpha);
		const phys::Length3D aimOffset = mix(aimOffsetA, aimOffsetB, interpolationAlpha);
		const phys::Length3D decayingVisualAimOffset = mix(decayingVisualAimOffsetA, decayingVisualAimOffsetB, interpolationAlpha);
		const phys::Position3D aimPosition = position + aimOffset + decayingVisualAimOffset;
		return {
			.aimPosition = aimPosition,
			.aimAngles = visualAimAngles,
			.viewPosition = aimPosition - convertAnglesToForwardDirection(visualAimAngles) * aimDistance,
			.position = position,
			.linearVelocity = linearVelocity,
		};
	}

private:
	[[nodiscard]] static WorldTransformation calculateWeaponTransformation(phys::Position3D aimPosition, phys::PitchYaw displayAimAngles, phys::PitchYawRotations aimDeviation,
		phys::Coefficient decayedCrouchAmount, phys::Coefficient aimingDownSightsAmount, phys::LinearVelocity3D smoothedVelocity, phys::PitchYawRates smoothedAimAngularRates,
		phys::PitchYawRates recoilAngularRates, phys::Time reloadTimeRemaining, phys::Time drawTimeRemaining, phys::Speed holderSpeed, phys::Angle bobbingPhase,
		const WeaponDescription& weaponDescription) {
		const phys::Time reloadTime = (reloadTimeRemaining <= 0) ? phys::Time{} : weaponDescription.reloadDuration - reloadTimeRemaining;
		const phys::Angle reloadPhase = phys::PI * reloadTime / weaponDescription.reloadDuration;
		const phys::Coefficient reloadAmount = sin(reloadPhase);
		const phys::Coefficient reloadAmount2 = sin(clamp((reloadPhase - numbers::PI * 0.4_x) * 3.5_x, 0.0_x, phys::Coefficient{numbers::PI}));
		const phys::Coefficient reloadAmount3 = sin(clamp((reloadPhase - numbers::PI * 0.45_x) * 3.0_x, 0.0_x, phys::Coefficient{numbers::PI}));
		const phys::Coefficient drawRemainingAmount = drawTimeRemaining / weaponDescription.drawDuration;
		const phys::OrthonormalBasis3D aimBasis = rotate(phys::PitchYawRoll{displayAimAngles, 0_radians});

		if (recoilAngularRates.getX() < 0) {
			recoilAngularRates.setX(recoilAngularRates.getX() * 0.15_x);
		}

		const phys::LinearVelocity3D smoothedAimVelocity = transpose(aimBasis) * smoothedVelocity;
		const phys::LinearVelocity2D bobbing = holderSpeed * phys::Scale2D{sin(bobbingPhase), sin(bobbingPhase * 2_x)};

		const WeaponDescription::ProceduralAnimation& proceduralAnimation = weaponDescription.proceduralAnimation;

		const phys::Length3D hipFireCrouchOffset = proceduralAnimation.hipFireCrouchOffset * decayedCrouchAmount;
		const phys::PitchYawRollRotations hipFireCrouchRotations = proceduralAnimation.hipFireCrouchRotations * decayedCrouchAmount;

		const phys::Length3D hipFireSwayOffset =
			smoothedAimVelocity * proceduralAnimation.hipFireOffsetVelocityContribution +              //
			smoothedAimAngularRates.getX() * proceduralAnimation.hipFireOffsetPitchRateContribution +  //
			smoothedAimAngularRates.getY() * proceduralAnimation.hipFireOffsetYawRateContribution +    //
			recoilAngularRates.getX() * proceduralAnimation.hipFireOffsetRecoilPitchRateContribution + //
			recoilAngularRates.getY() * proceduralAnimation.hipFireOffsetRecoilYawRateContribution +   //
			phys::Length3D{bobbing * proceduralAnimation.hipFireOffsetBobbingContribution, 0};
		const phys::PitchYawRollRotations hipFireSwayRotations =
			smoothedAimAngularRates.getX() * proceduralAnimation.hipFireRotationsPitchRateContribution + //
			smoothedAimAngularRates.getY() * proceduralAnimation.hipFireRotationsYawRateContribution;

		const phys::Length3D aimingDownSightsSwayOffset =
			smoothedAimVelocity * proceduralAnimation.aimingDownSightsOffsetVelocityContribution +              //
			smoothedAimAngularRates.getX() * proceduralAnimation.aimingDownSightsOffsetPitchRateContribution +  //
			smoothedAimAngularRates.getY() * proceduralAnimation.aimingDownSightsOffsetYawRateContribution +    //
			recoilAngularRates.getX() * proceduralAnimation.aimingDownSightsOffsetRecoilPitchRateContribution + //
			recoilAngularRates.getY() * proceduralAnimation.aimingDownSightsOffsetRecoilYawRateContribution +   //
			phys::Length3D{bobbing * proceduralAnimation.aimingDownSightsOffsetBobbingContribution, 0};
		const phys::PitchYawRollRotations aimingDownSightsSwayRotations =
			smoothedAimAngularRates.getX() * proceduralAnimation.aimingDownSightsRotationsPitchRateContribution + //
			smoothedAimAngularRates.getY() * proceduralAnimation.aimingDownSightsRotationsYawRateContribution;

		const phys::Length3D reloadAnimationOffset{
			reloadAmount * 0.08_meters,
			(reloadAmount - reloadAmount2 * 0.35_x) * -0.2_meters,
			0,
		};
		const phys::PitchYawRollRotations reloadAnimationRotations{
			reloadAmount * -0.5_radians + reloadAmount2 * 0.2_radians + reloadAmount3 * 0.1_radians,
			reloadAmount * -0.3_radians + reloadAmount3 * 0.05_radians,
			reloadAmount * -0.3_radians,
		};

		const phys::Length3D drawAnimationOffset{0, drawRemainingAmount * -0.5_meters, 0};
		const phys::PitchYawRollRotations drawAnimationRotations{drawRemainingAmount * -0.7_radians, 0, 0};

		const phys::PitchYawRollRotations aimDeviationRotations{aimDeviation.getX(), aimDeviation.getY(), 0};

		const phys::Length3D hipFireOffset = proceduralAnimation.hipFireBaseOffset + hipFireCrouchOffset + hipFireSwayOffset + reloadAnimationOffset + drawAnimationOffset;
		const phys::PitchYawRollRotations hipFireRotations =
			proceduralAnimation.hipFireBaseRotations + hipFireCrouchRotations + hipFireSwayRotations + aimDeviationRotations + reloadAnimationRotations + drawAnimationRotations;

		const phys::Length3D aimingDownSightsOffset = proceduralAnimation.aimingDownSightsBaseOffset + aimingDownSightsSwayOffset + reloadAnimationOffset;
		const phys::PitchYawRollRotations aimingDownSightsRotations =
			proceduralAnimation.aimingDownSightsBaseRotations + aimingDownSightsSwayRotations + aimDeviationRotations + reloadAnimationRotations;

		return {
			.position = aimPosition + aimBasis * mix(hipFireOffset, aimingDownSightsOffset, aimingDownSightsAmount),
			.orientation = phys::Orientation3D::fromBasis(aimBasis * rotate(mix(hipFireRotations, aimingDownSightsRotations, aimingDownSightsAmount))),
			.scale{1_x},
		};
	}
};

struct CurrentPlayer {
	PlayerID playerID;
	LocalPlayerID localPlayerID;
	SnapshotBufferView receivedSnapshots;
	SnapshotBufferView predictionSnapshots;
	Timestamp receivedInterpolationTimestamp;
	Timestamp predictionInterpolationTimestamp;
	Timestamp subtickBeginTimestamp;
	Timestamp subtickEndTimestamp;
	Duration tickInterval;

	[[nodiscard]] WorldView getWorldView(const EntityRegistry& registry, const ResourceRegistry& resources, StridedSpan<const Frustum<float>> frustums) const {
		return {
			.subtickRegistry = registry,
			.subtickResources = resources,
			.receivedInterpolation = receivedSnapshots.getInterpolationView(subtickBeginTimestamp, tickInterval),
			.predictionInterpolation = predictionSnapshots.getInterpolationView(subtickBeginTimestamp, tickInterval),
			.receivedInterpolationTimestamp = receivedInterpolationTimestamp,
			.predictionInterpolationTimestamp = predictionInterpolationTimestamp,
			.subtickTimestamp = subtickBeginTimestamp,
			.tickInterval = tickInterval,
			.playerID = playerID,
			.frustums = frustums,
		};
	}

	[[nodiscard]] WorldView getWorldViewAtTimestamp(const EntityRegistry& registry, const ResourceRegistry& resources, StridedSpan<const Frustum<float>> frustums,
		Timestamp timestamp) const {
		const Duration receivedInterpolationTimeDifference = getTimeBetween(receivedInterpolationTimestamp, subtickBeginTimestamp, tickInterval);
		const Duration predictionInterpolationTimeDifference = getTimeBetween(predictionInterpolationTimestamp, subtickBeginTimestamp, tickInterval);
		const Timestamp adjustedReceivedInterpolationTimestamp = timestamp.withTimeAdded(-receivedInterpolationTimeDifference, tickInterval);
		const Timestamp adjustedPredictionInterpolationTimestamp = timestamp.withTimeAdded(-predictionInterpolationTimeDifference, tickInterval);
		return {
			.subtickRegistry = registry,
			.subtickResources = resources,
			.receivedInterpolation = receivedSnapshots.getInterpolationView(adjustedReceivedInterpolationTimestamp, tickInterval),
			.predictionInterpolation = predictionSnapshots.getInterpolationView(adjustedPredictionInterpolationTimestamp, tickInterval),
			.receivedInterpolationTimestamp = receivedInterpolationTimestamp,
			.predictionInterpolationTimestamp = predictionInterpolationTimestamp,
			.subtickTimestamp = timestamp,
			.tickInterval = tickInterval,
			.playerID = playerID,
			.frustums = frustums,
		};
	}
};

#endif
