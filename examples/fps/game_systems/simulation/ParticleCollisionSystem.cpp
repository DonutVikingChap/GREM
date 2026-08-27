// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"

class ParticleCollisionSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<handleParticleCollisions>("Handle particle collisions");
	}

private:
	static bool spawnLandingDecal(EntityRegistry& registry, ResourceRegistry& resources, const Schema& schema, EntityID particleEntityID, ParticleType particleType,
		EntityID hitObjectID, phys::Position3D hitPosition, phys::Direction3D hitNormal, TickIndex tickIndex, Duration tickInterval) {
		const ParticleDescription& particleDescription = schema.getParticleDescription(particleType);
		if (particleDescription.landingDecalMaterialType != DecalMaterialType{}) {
			rng::Xoroshiro128PlusPlusEngine::result_type seed = static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(tickIndex - TickIndex{});
			seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(hitPosition.getX().in(phys::METERS), 65536.0f));
			seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(hitPosition.getY().in(phys::METERS), 65536.0f));
			seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(hitPosition.getZ().in(phys::METERS), 65536.0f));
			rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
			rng::UniformRealDistribution<float> distribution{-1.0f, 1.0f};
			const phys::Scale3D randomVector{distribution(numberGenerator), distribution(numberGenerator), distribution(numberGenerator)};
			const phys::Direction3D decalUp = tryNormalize(randomVector).value_or(phys::Y_AXIS_3D);
			const phys::Position3D decalPosition = hitPosition;
			const phys::Orientation3D decalOrientation = phys::Orientation3D::lookAt(-hitNormal, decalUp);
			spawnDecal(registry, resources, particleDescription.landingDecalMaterialType, particleEntityID.getFlags(), decalPosition, decalOrientation,
				particleDescription.landingDecalSize, particleDescription.landingDecalRange, hitObjectID, tickIndex, tickInterval);
			return true;
		}
		return false;
	}

	static void handleParticleCollisions(EntityRegistry& registry, ResourceRegistry& resources, const phys::CollisionEvents3D& collisionEvents, const Schema& schema,
		const SynchronizedEntityMap& synchronizedEntityMap, TickIndex tickIndex, Duration tickInterval) {
		for (const phys::CollisionEvent3D& collisionEvent : collisionEvents) {
			if (const ParticleType* const particleType = registry.findComponent<ParticleType>(collisionEvent.objectIDs.first)) {
				if (const ParticleState* const particleState = registry.findComponent<ParticleState>(collisionEvent.objectIDs.first)) {
					const EntityID ownerEntityID = synchronizedEntityMap.findEntity(registry, particleState->owner);
					if (!ownerEntityID || ownerEntityID != collisionEvent.objectIDs.second) {
						if (spawnLandingDecal(registry, resources, schema, collisionEvent.objectIDs.first, *particleType, collisionEvent.objectIDs.second,
								collisionEvent.objectTransformations.second(collisionEvent.localOffsets.second), -collisionEvent.normal, tickIndex, tickInterval)) {
							registry.destroyEntity(collisionEvent.objectIDs.first);
						}
					}
				}
			}
			if (const ParticleType* const particleType = registry.findComponent<ParticleType>(collisionEvent.objectIDs.second)) {
				if (const ParticleState* const particleState = registry.findComponent<ParticleState>(collisionEvent.objectIDs.second)) {
					const EntityID ownerEntityID = synchronizedEntityMap.findEntity(registry, particleState->owner);
					if (!ownerEntityID || ownerEntityID != collisionEvent.objectIDs.first) {
						if (spawnLandingDecal(registry, resources, schema, collisionEvent.objectIDs.second, *particleType, collisionEvent.objectIDs.first,
								collisionEvent.objectTransformations.first(collisionEvent.localOffsets.first), collisionEvent.normal, tickIndex, tickInterval)) {
							registry.destroyEntity(collisionEvent.objectIDs.second);
						}
					}
				}
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createParticleCollisionSystem() { // NOLINT(misc-use-internal-linkage)
	return new ParticleCollisionSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
