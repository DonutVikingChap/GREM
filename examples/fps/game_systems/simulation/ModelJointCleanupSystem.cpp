// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"

class ModelJointCleanupSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<InvalidatedModelJoints>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<InvalidatedModelJoints>();
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<destroyModelJointsWithoutValidTargets>("Destroy model joints without valid targets");
	}

private:
	struct InvalidatedModelJoints {
		ArrayList<EntityID> entities{};
	};

	static void destroyModelJointsWithoutValidTargets(EntityRegistry& registry, InvalidatedModelJoints& invalidatedModelJoints,
		const SynchronizedEntityMap& synchronizedEntityMap) {
		invalidatedModelJoints.entities.clear();
		for (auto&& [entityID, modelJointController] : registry.getEntities<const ModelJointController>()) {
			if (const EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, modelJointController.target)) {
				if (registry.hasComponent<ModelType>(targetEntityID)) {
					continue;
				}
			}
			invalidatedModelJoints.entities.push_back(entityID);
		}
		for (auto&& [entityID, modelJointLight] : registry.getEntities<const ModelJointLight>()) {
			if (const EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, modelJointLight.target)) {
				if (registry.hasComponent<ModelType>(targetEntityID)) {
					continue;
				}
			}
			invalidatedModelJoints.entities.push_back(entityID);
		}
		for (const EntityID entityID : invalidatedModelJoints.entities) {
			registry.destroyEntity(entityID);
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createModelJointCleanupSystem() { // NOLINT(misc-use-internal-linkage)
	return new ModelJointCleanupSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
