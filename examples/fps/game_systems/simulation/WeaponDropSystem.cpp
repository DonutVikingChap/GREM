// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../EntityCallbacks.hpp"
#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"

class WeaponDropSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<WeaponDropCallbacks>(WeaponDropCallbacks{
			.killCallbackID = resources.getResource<EntityCallbacks>().onKill.insert(dropEquippedWeapon)->first,
		});
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.getResource<EntityCallbacks>().onKill.erase(resources.getResource<WeaponDropCallbacks>().killCallbackID);
		resources.removeResource<WeaponDropCallbacks>();
	}

private:
	struct WeaponDropCallbacks {
		EntityCallbacks::KillCallbackID killCallbackID;
	};

	static void dropEquippedWeapon(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID) {
		const Inventory* const inventory = registry.findComponent<Inventory>(entityID);
		if (!inventory || !inventory->equippedWeapon) {
			return;
		}

		const phys::Position3D* const position = registry.findComponent<phys::Position3D>(entityID);
		const phys::LinearVelocity3D* const linearVelocity = registry.findComponent<phys::LinearVelocity3D>(entityID);
		const Aim* const aim = registry.findComponent<Aim>(entityID);
		if (!position || !linearVelocity || !aim) {
			return;
		}

		const Schema& schema = resources.getResource<Schema>();
		const SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
		const TickIndex tickIndex = resources.getResource<TickIndex>();
		const Duration tickInterval = resources.getResource<Duration>();

		const EntityID weaponEntityID = synchronizedEntityMap.findEntity(registry, inventory->equippedWeapon);
		const WeaponType* const weaponType = registry.findComponent<WeaponType>(weaponEntityID);
		if (!weaponType || *weaponType == WeaponType{}) {
			return;
		}

		const WeaponDescription& weaponDescription = schema.getWeaponDescription(*weaponType);
		const phys::PitchYawRoll angles{aim->angles, 0};
		spawnEntity(registry, resources, EntityType{"DROPPED_WEAPON"}, EntityID::Flags{ENTITY_PHYSICS_PREDICTED | ENTITY_DISPLAY_PREDICTED},
			ComponentInitializers{
				phys::Position3D{*position + aim->offset + rotate(angles) * weaponDescription.proceduralAnimation.hipFireBaseOffset},
				phys::LinearVelocity3D{*linearVelocity},
				phys::Orientation3D::fromAngles(angles + weaponDescription.proceduralAnimation.hipFireBaseRotations),
				phys::ObjectActivity{phys::ObjectActivity::MAX_ENERGY_LEVEL},
				WeaponType{*weaponType},
				DestroyCountdown{.destroyOnTickIndex = Timestamp{tickIndex, weaponDescription.droppedDespawnTime, tickInterval}.getTickIndex()},
			});
		registry.destroyEntity(weaponEntityID);
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createWeaponDropSystem() { // NOLINT(misc-use-internal-linkage)
	return new WeaponDropSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
