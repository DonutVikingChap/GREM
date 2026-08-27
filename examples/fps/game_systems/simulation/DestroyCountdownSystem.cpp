// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/execution/Task.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"

class DestroyCountdownSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<ExpiredEntities>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<ExpiredEntities>();
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<destroyExpiredEntities>("Destroy expired entities");
	}

private:
	struct ExpiredEntities {
		ArrayList<EntityID> entities{};
	};

	static void destroyExpiredEntities(EntityRegistry& registry, ExpiredEntities& expiredEntities, TickIndex tickIndex) {
		expiredEntities.entities.clear();
		for (auto&& [entityID, destroyCountdown] : registry.getEntities<DestroyCountdown>()) {
			if (tickIndex >= destroyCountdown.destroyOnTickIndex) {
				expiredEntities.entities.push_back(entityID); // Defer destruction to avoid iterator invalidation.
			}
		}
		for (const EntityID entityID : expiredEntities.entities) {
			registry.destroyEntity(entityID);
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createDestroyCountdownSystem() { // NOLINT(misc-use-internal-linkage)
	return new DestroyCountdownSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
