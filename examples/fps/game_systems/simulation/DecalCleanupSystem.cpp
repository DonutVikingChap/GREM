// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"

class DecalCleanupSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<InvalidatedDecals>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<InvalidatedDecals>();
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<destroyDecalsWithoutValidTargets>("Destroy decals without valid targets");
	}

private:
	struct InvalidatedDecals {
		ArrayList<EntityID> entities{};
	};

	static void destroyDecalsWithoutValidTargets(EntityRegistry& registry, InvalidatedDecals& invalidatedDecals, const SynchronizedEntityMap& synchronizedEntityMap) {
		invalidatedDecals.entities.clear();
		for (auto&& [entityID, decalAttachmentFrame] : registry.getEntities<const DecalAttachmentFrame>()) {
			if (const EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, decalAttachmentFrame.target)) {
				if (registry.hasComponent<phys::Position3D>(targetEntityID) && registry.hasComponent<phys::Orientation3D>(targetEntityID) &&
					registry.hasComponent<phys::Scale3D>(targetEntityID)) {
					continue;
				}
			}
			invalidatedDecals.entities.push_back(entityID);
		}
		for (const EntityID entityID : invalidatedDecals.entities) {
			registry.destroyEntity(entityID);
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createDecalCleanupSystem() { // NOLINT(misc-use-internal-linkage)
	return new DecalCleanupSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
