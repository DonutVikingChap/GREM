// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"
#include "../../game_map.hpp"

class MapBoundsEnforcementSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<OutOfBoundsEntities>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<OutOfBoundsEntities>();
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<destroyObjectsAndProjectilesOutsideMapBounds>("Destroy objects and projectiles outside map bounds");
		scheduler.addTask<teleportObjectsOutsideMapBounds>("Teleport objects outside map bounds");
	}

private:
	struct OutOfBoundsEntities {
		ArrayList<EntityID> entities{};
	};

	static void destroyObjectsAndProjectilesOutsideMapBounds(EntityRegistry& registry, OutOfBoundsEntities& outOfBoundsEntities, const MapInfo& mapInfo) {
		outOfBoundsEntities.entities.clear();
		for (auto&& [entityID, position, destroyIfOutsideMapBoundsTag] : registry.getEntities<const phys::Position3D, const DestroyIfOutsideMapBoundsTag>()) {
			if (!mapInfo.bounds.contains(position)) {
				outOfBoundsEntities.entities.push_back(entityID); // Defer destruction to avoid iterator invalidation.
			}
		}
		for (auto&& [entityID, projectileState, destroyIfOutsideMapBoundsTag] : registry.getEntities<const ProjectileState, const DestroyIfOutsideMapBoundsTag>()) {
			if (!mapInfo.bounds.contains(projectileState.position)) {
				outOfBoundsEntities.entities.push_back(entityID); // Defer destruction to avoid iterator invalidation.
			}
		}
		for (const EntityID entityID : outOfBoundsEntities.entities) {
			registry.destroyEntity(entityID);
		}
	}

	static void teleportObjectsOutsideMapBounds(Entities<phys::Position3D, phys::LinearVelocity3D, const TeleportIfPhysicsObjectOutsideMapBoundsTag> entities,
		const MapInfo& mapInfo) {
		for (auto&& [entityID, position, linearVelocity, teleportIfOutsideMapBoundsTag] : entities) {
			if (!mapInfo.bounds.contains(position)) {
				position = {};
				linearVelocity = {};
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createMapBoundsEnforcementSystem() { // NOLINT(misc-use-internal-linkage)
	return new MapBoundsEnforcementSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
