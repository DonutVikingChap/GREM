// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Simulation.hpp>

#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"
#include "../../game_resources.hpp"

class PhysicsSimulationSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount parallelism) override {
		resources.addSharedResource<PerformanceTimer>();
		resources.addSharedResource<ExpiredJoints>();
		phys::SimulationOptions3D simulationOptions{
			.stepInterval = resources.getResource<Duration>(),
			.targetParallelism = parallelism,
			.subStepCount = 4,
			.contactStiffness = 30_Hertz,
		};
		if ((resources.getResource<Schema>().getEntityFlags() & ENTITY_CLIENTSIDE) != 0) {
			simulationOptions.contactColorCount = 0;
		}
		phys::Simulation3D::addRequiredResources(resources, simulationOptions);
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		phys::Simulation3D::removeResources(resources);
		resources.removeResource<ExpiredJoints>();
		resources.removeResource<PerformanceTimer>();
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry& resources, exec::Task::ParallelCount) override {
		scheduler.addTask<startPerformanceTimer>("Start physics performance timer");
		scheduler.addTask<updatePhysicsJoints>("Update physics joints");
		phys::Simulation3D::scheduleStep(scheduler, resources.getResource<phys::SimulationOptions3D>());
		scheduler.addOptionalTask<savePhysicsTime>("Save physics time");
	}

private:
	struct PerformanceTimer {
		TimePoint physicsStartTime{};
	};

	struct ExpiredJoints {
		ArrayList<EntityID> entities{};
	};

	static void startPerformanceTimer(EntityRegistry&, ResourceRegistry&, PerformanceTimer& performanceTimer) {
		performanceTimer.physicsStartTime = Clock::now();
	}

	static void updatePhysicsJoints(EntityRegistry& registry, const ResourceRegistry& resources, ExpiredJoints& expiredJoints, const SynchronizedEntityMap& synchronizedEntityMap) {
		for (const auto& [entityID, jointConnectedObjects, genericJointOptions] : registry.getEntities<const JointConnectedObjects, const phys::GenericJointOptions3D>()) {
			const EntityID objectIDA = synchronizedEntityMap.findEntity(registry, jointConnectedObjects.first);
			const EntityID objectIDB = synchronizedEntityMap.findEntity(registry, jointConnectedObjects.second);
			if (!registry.hasComponent<phys::ObjectActivity>(objectIDA) || !registry.hasComponent<phys::ObjectActivity>(objectIDB)) {
				expiredJoints.entities.push_back(entityID);
				continue;
			}

			if (phys::JointConnectedObjects* const jointConnectedObjectIDs = registry.findComponent<phys::JointConnectedObjects>(entityID)) {
				*jointConnectedObjectIDs = {objectIDA, objectIDB};
				registry.getComponent<phys::JointAttachmentOffsets3D>(entityID) = genericJointOptions.attachmentOffsets;
				registry.getComponent<phys::JointAttachmentOrientations3D>(entityID) = genericJointOptions.attachmentOrientations;
				if (genericJointOptions.linearConstraint) {
					registry.getComponent<phys::JointLinearConstraint3D>(entityID) = *genericJointOptions.linearConstraint;
				}
				if (genericJointOptions.distanceConstraint) {
					registry.getComponent<phys::JointDistanceConstraint3D>(entityID) = *genericJointOptions.distanceConstraint;
				}
				if (genericJointOptions.coneConstraint) {
					registry.getComponent<phys::JointConeConstraint3D>(entityID) = *genericJointOptions.coneConstraint;
				}
				if (genericJointOptions.twistConstraint) {
					registry.getComponent<phys::JointTwistConstraint3D>(entityID) = *genericJointOptions.twistConstraint;
				}
				if (genericJointOptions.angularConstraint) {
					registry.getComponent<phys::JointAngularConstraint3D>(entityID) = *genericJointOptions.angularConstraint;
				}
				registry.getComponent<phys::JointFlags3D>(entityID) = genericJointOptions.flags;
			} else {
				phys::Simulation3D::addGenericJointComponents(registry, resources, entityID, {objectIDA, objectIDB}, genericJointOptions);
			}
		}

		for (const EntityID entityID : expiredJoints.entities) {
			registry.destroyEntity(entityID);
		}
	}

	static void savePhysicsTime(EntityRegistry&, ResourceRegistry&, TickPerformanceStats& tickPerformanceStats, const PerformanceTimer& performanceTimer) {
		const TimePoint physicsEndTime = Clock::now();
		tickPerformanceStats.latestPhysicsTime = physicsEndTime - performanceTimer.physicsStartTime;
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createPhysicsSimulationSystem() { // NOLINT(misc-use-internal-linkage)
	return new PhysicsSimulationSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
