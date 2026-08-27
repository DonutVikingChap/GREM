// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../PlayerEntityMap.hpp"
#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_data.hpp"
#include "../../game_events.hpp"
#include "../../game_functions.hpp"

class WeaponHandlingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void scheduleCurrentPlayerUpdate(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<updateCurrentPlayerWeaponHandling>("Update current player weapon handling");
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<tickWeaponHandling>("Tick weapon handing");
	}

private:
	static void updateCurrentPlayerWeaponHandling(EntityRegistry& registry, ResourceRegistry& resources, CurrentPlayer& currentPlayer, Events& events, const Schema& schema,
		const SynchronizedEntityMap& synchronizedEntityMap, const PlayerEntityMap& playerEntityMap) {
		playerEntityMap.forEachPlayerEntity(currentPlayer.playerID, currentPlayer.localPlayerID, [&](EntityID entityID) -> void {
			const SynchronizedEntityID* const synchronizedPlayerEntityID = registry.findComponent<SynchronizedEntityID>(entityID);
			Aim* const playerAim = registry.findComponent<Aim>(entityID);
			const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID);
			if (!synchronizedPlayerEntityID || !playerAim || !playerInventory) {
				return;
			}

			const SynchronizedEntityID synchronizedWeaponEntityID = playerInventory->equippedWeapon;
			const EntityID weaponEntityID = synchronizedEntityMap.findEntity(registry, synchronizedWeaponEntityID);
			if (!weaponEntityID) {
				return;
			}

			WeaponIntermediateState* const weaponIntermediateState = registry.findComponent<WeaponIntermediateState>(weaponEntityID);
			WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
			const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
			if (!weaponIntermediateState || !weaponState || !weaponType) {
				return;
			}

			const WeaponDescription& weaponDescription = schema.getWeaponDescription(*weaponType);

			const Duration deltaTime = getTimeBetween(currentPlayer.subtickBeginTimestamp, currentPlayer.subtickEndTimestamp, currentPlayer.tickInterval);
			const Duration tickInterval = currentPlayer.tickInterval;
			countdown(weaponState->drawTimeRemaining, deltaTime);

			const bool reloadStarted = weaponState->flags.contains(WeaponState::STARTING_RELOAD) && weaponState->loadedRoundCount < weaponDescription.maxMagazineCapacity &&
			                           !weaponState->flags.contains(WeaponState::RELOADING) && !weaponState->flags.contains(WeaponState::FIRING);
			if (reloadStarted) {
				weaponState->flags &= ~WeaponState::STARTING_RELOAD;
				weaponState->loadedRoundCount = 0;
				weaponState->reloadTimeRemaining = weaponDescription.reloadDuration;
				if (weaponDescription.reloadSoundType != SoundType{}) {
					events.push_back(EntityParentedSoundPlayedEvent{.soundType = weaponDescription.reloadSoundType, .emitter = *synchronizedPlayerEntityID});
				}
				weaponState->flags |= WeaponState::RELOADING;
			} else if (weaponState->loadedRoundCount == weaponDescription.maxMagazineCapacity) {
				weaponState->flags &= ~WeaponState::STARTING_RELOAD;
			}

			const bool reloadFinished = weaponState->flags.contains(WeaponState::RELOADING) && countdown(weaponState->reloadTimeRemaining, deltaTime);
			if (reloadFinished) {
				weaponState->loadedRoundCount = weaponDescription.maxMagazineCapacity;
				weaponState->flags &= ~WeaponState::RELOADING;
			}

			const bool aimingDownSights = weaponState->flags.contains(WeaponState::AIMING_DOWN_SIGHTS);
			if (aimingDownSights) {
				weaponState->aimingDownSightsAmount = expDecay(weaponState->aimingDownSightsAmount, 1_x, weaponDescription.aimDownSightsExponentialDecayRate, deltaTime);
			} else {
				weaponState->aimingDownSightsAmount = expDecay(weaponState->aimingDownSightsAmount, weaponDescription.unaimDownSightsExponentialDecayRate, deltaTime);
			}

			playerAim->angles += weaponState->recoilAngularRates * deltaTime;

			const phys::Time timeSinceFire = getTimeBetween(weaponState->lastFiredTimestamp, currentPlayer.subtickBeginTimestamp, tickInterval);
			if (weaponDescription.recoil.cosineCutoff * (timeSinceFire / weaponDescription.recoilDuration) < 0.5_x * (1_x - weaponState->aimingDownSightsAmount)) {
				weaponState->recoilInducedAimDeviation +=
					weaponState->recoilAngularRates * deltaTime *
					phys::Scale2D{weaponDescription.recoil.hipFireAimDeviationCoefficientVertical, weaponDescription.recoil.hipFireAimDeviationCoefficientHorizontal} *
					(1_x - weaponState->aimingDownSightsAmount);
				weaponState->recoilInducedAimDeviation = clampLength(weaponState->recoilInducedAimDeviation, weaponDescription.aimDeviationMax);
			} else {
				weaponState->recoilInducedAimDeviation = expDecay(weaponState->recoilInducedAimDeviation, weaponDescription.recoil.aimDeviationExponentialDecayRate, deltaTime);
			}

			weaponState->smoothingInterpolationTime += deltaTime;
			const phys::PitchYawRates interpolatedSmoothedAimAngularRates =
				mix(weaponState->previousSmoothedAimAngularRates, weaponState->smoothedAimAngularRates, weaponState->smoothingInterpolationTime / phys::Time{tickInterval});
			const phys::PitchYawRates aimDeviationInducingRecoilAngularRates{weaponState->recoilAngularRates.getX() * ((weaponState->recoilAngularRates.getX() < 0) ? 0.15_x : 1_x),
				weaponState->recoilAngularRates.getY()};
			weaponState->rotationInducedAimDeviation = {
				aimDeviationInducingRecoilAngularRates.getX() * 0.1_seconds,
				interpolatedSmoothedAimAngularRates.getY() * 0.014_seconds * (1_x - weaponState->aimingDownSightsAmount) +
					aimDeviationInducingRecoilAngularRates.getY() * 0.08_seconds,
			};
			weaponState->rotationInducedAimDeviation = clampLength(weaponState->rotationInducedAimDeviation, weaponDescription.aimDeviationMax);

			const phys::Coefficient recoilStrengthCoefficient = mix(1_x, weaponDescription.recoil.aimingDownSightsStrengthCoefficient, weaponState->aimingDownSightsAmount);
			const phys::PitchYawRates recoilStrengthMin =
				recoilStrengthCoefficient * phys::PitchYawRates{weaponDescription.recoil.strengthVerticalMin, weaponDescription.recoil.strengthHorizontalMin};
			const phys::PitchYawRates recoilStrengthMax = max(recoilStrengthMin,
				recoilStrengthCoefficient * phys::PitchYawRates{weaponDescription.recoil.strengthVerticalMax, weaponDescription.recoil.strengthHorizontalMax});

			const phys::PitchYawRotations aimDeviation = weaponState->recoilInducedAimDeviation + weaponState->rotationInducedAimDeviation;
			const phys::PitchYaw fireAngles = playerAim->angles + aimDeviation;

			rng::Xoroshiro128PlusPlusEngine::result_type seed =
				static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(currentPlayer.subtickBeginTimestamp.getTickIndex() - TickIndex{});
			seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(fireAngles.getX().in(phys::DEGREES), 360.0f));
			seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(fireAngles.getY().in(phys::DEGREES), 360.0f));
			rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
			rng::UniformRealDistribution<float> recoilStrengthPitchDistribution{
				recoilStrengthMin.getX().in(phys::RADIANS_PER_SECOND),
				recoilStrengthMax.getX().in(phys::RADIANS_PER_SECOND),
			};
			rng::UniformRealDistribution<float> recoilStrengthYawDistribution{
				recoilStrengthMin.getY().in(phys::RADIANS_PER_SECOND),
				recoilStrengthMax.getY().in(phys::RADIANS_PER_SECOND),
			};

			weaponIntermediateState->projectilesToFire = 0;
			const bool cycling = (weaponState->flags.contains(WeaponState::FIRING) || weaponState->flags.contains(WeaponState::PULLING_TRIGGER)) &&
			                     !weaponState->flags.contains(WeaponState::RELOADING) && !weaponState->flags.contains(WeaponState::TRIGGER_CLICKED);
			if (cycling) {
				const Duration cycleDuration = weaponDescription.cycleDuration;

				Timestamp nextFireTimestamp = weaponState->lastFiredTimestamp.withTimeAdded(cycleDuration, tickInterval);
				if (!weaponState->flags.contains(WeaponState::FIRING)) {
					weaponState->flags |= WeaponState::FIRING;
					nextFireTimestamp = max(nextFireTimestamp, currentPlayer.subtickBeginTimestamp);
				}

				while (nextFireTimestamp <= currentPlayer.subtickEndTimestamp) {
					if (!weaponState->flags.contains(WeaponState::PULLING_TRIGGER)) {
						weaponState->flags &= ~WeaponState::FIRING;
						break;
					}

					if (weaponState->loadedRoundCount == 0 || weaponState->fireMode == WeaponDescription::FireMode::SAFE) {
						if (weaponDescription.dryFireSoundType != SoundType{}) {
							events.push_back(EntityParentedSoundPlayedEvent{.soundType = weaponDescription.dryFireSoundType, .emitter = *synchronizedPlayerEntityID});
						}
						weaponState->flags |= WeaponState::TRIGGER_CLICKED;
						weaponState->flags &= ~WeaponState::FIRING;
						break;
					}

					if (weaponDescription.fireSoundType != SoundType{}) {
						events.push_back(EntityParentedSoundPlayedEvent{.soundType = weaponDescription.fireSoundType, .emitter = *synchronizedPlayerEntityID});
					}
					if (weaponDescription.caseEjectionParticleType != ParticleType{}) {
						const WorldView worldView = currentPlayer.getWorldView(registry, resources, {});
						if (const Optional<InterpolatedEntityView> weaponEntity = worldView.findEntity(EntityID::Flags{ENTITY_DISPLAY_PREDICTED}, synchronizedWeaponEntityID)) {
							if (const Optional<WorldTransformation> weaponDisplayTransformation = worldView.getWeaponEntityDisplayTransformation(*weaponEntity, true)) {
								spawnParticle(registry, resources, numberGenerator, weaponDescription.caseEjectionParticleType, EntityID::Flags{ENTITY_DISPLAY_PREDICTED},
									(*weaponDisplayTransformation)(weaponDescription.proceduralAnimation.caseEjectionLocalOffset),
									weaponDisplayTransformation->orientation * weaponDescription.proceduralAnimation.caseEjectionLocalOrientation, weaponState->smoothedVelocity,
									entityID, currentPlayer.subtickBeginTimestamp.getTickIndex(), worldView.tickInterval);
							}
						}
					}

					++weaponIntermediateState->projectilesToFire;
					--weaponState->loadedRoundCount;
					weaponState->recoilStrengthOfLatestShot =
						vec2{recoilStrengthPitchDistribution(numberGenerator), -recoilStrengthYawDistribution(numberGenerator)} * phys::RADIANS_PER_SECOND;
					weaponState->lastFiredTimestamp = nextFireTimestamp;
					weaponState->recoilAngularRates = calculateRecoilAngularRates(phys::Time{}, weaponState->recoilStrengthOfLatestShot, weaponDescription);
					nextFireTimestamp.addTime(cycleDuration, tickInterval);

					if (weaponState->fireMode == WeaponDescription::FireMode::SEMI_AUTOMATIC) {
						weaponState->flags |= WeaponState::TRIGGER_CLICKED;
						weaponState->flags &= ~WeaponState::FIRING;
						break;
					}
				}
			}
		});
	}

	static void tickWeaponHandling(Entities<WeaponState, const WeaponType> entities, Entities<const Aim, const phys::LinearVelocity3D> holderEntities, const Schema& schema,
		const SynchronizedEntityMap& synchronizedEntityMap, TickIndex tickIndex, Duration tickInterval) {
		for (auto&& [entityID, weaponState, weaponType] : entities) {
			weaponState.smoothingInterpolationTime = {};

			const EntityID holderEntityID = synchronizedEntityMap.findEntity(holderEntities, weaponState.holder);
			if (!holderEntityID) {
				weaponState.previousSmoothedVelocity = {};
				weaponState.smoothedVelocity = {};
				weaponState.previousSmoothedAimAngularRates = {};
				weaponState.smoothedAimAngularRates = {};
				weaponState.recoilAngularRates = {};
				continue;
			}

			const auto& [holderEntityID_, aim, linearVelocity] = holderEntities[holderEntityID];
			const WeaponDescription& weaponDescription = schema.getWeaponDescription(weaponType);

			weaponState.previousSmoothedVelocity = weaponState.smoothedVelocity;
			weaponState.smoothedVelocity = expDecay(weaponState.smoothedVelocity, linearVelocity, weaponDescription.velocitySmoothingExponentialDecayRate, tickInterval);

			weaponState.previousSmoothedAimAngularRates = weaponState.smoothedAimAngularRates;
			weaponState.smoothedAimAngularRates =
				expDecay(weaponState.smoothedAimAngularRates, aim.rotationRates, weaponDescription.aimAngleSmoothingExponentialDecayRate, tickInterval);

			const phys::Time timeSinceFire = getTimeBetween(weaponState.lastFiredTimestamp, tickIndex, tickInterval);
			weaponState.recoilAngularRates = calculateRecoilAngularRates(timeSinceFire, weaponState.recoilStrengthOfLatestShot, weaponDescription);
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createWeaponHandlingSystem() { // NOLINT(misc-use-internal-linkage)
	return new WeaponHandlingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
