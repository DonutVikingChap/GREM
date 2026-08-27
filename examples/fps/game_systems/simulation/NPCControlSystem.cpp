// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"

class NPCControlSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<RobotAnimations>(resources.getResource<AssetCache>(), resources.getResource<Schema>());
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<RobotAnimations>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics*) override {
		resources.getResource<RobotAnimations>() = RobotAnimations{resources.getResource<AssetCache>(), resources.getResource<Schema>()};
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<tickNPCStates>("Tick NPC states");
		scheduler.addTask<animateRobotNPCs>("Animate robot NPCs");
	}

private:
	struct RobotAnimations {
		Optional<res::Model::AnimationIndex> walkingAnimationIndex{};
		Optional<res::Model::AnimationIndex> runningAnimationIndex{};

		RobotAnimations(AssetCache& assetCache, const Schema& schema) {
			if (const ModelDescription* const modelDescription = schema.findLoadedModelDescription(ModelType{"ROBOT"})) {
				const SharedPointer<res::Model> model = assetCache.getModel(modelDescription->filepath, modelDescription->options);
				walkingAnimationIndex = model->findAnimationIndex("Walking");
				runningAnimationIndex = model->findAnimationIndex("Running");
			}
		}
	};

	static void tickNPCStates(Entities<phys::ObjectActivity, phys::Orientation3D, phys::LinearVelocity3D, phys::LinearAcceleration3D, NPCState, const NPCInfo> entities,
		const phys::SimulationOptions3D& simulationOptions) {
		const phys::Time deltaTime = simulationOptions.stepInterval;
		for (auto&& [entityID, activity, orientation, linearVelocity, gravityAcceleration, npcState, npcInfo] : entities) {
			if ((entityID.getFlags() & (ENTITY_CLIENTSIDE | ENTITY_PHYSICS_PREDICTED)) == ENTITY_CLIENTSIDE) {
				gravityAcceleration = {};
				continue;
			}

			activity.energyLevel = phys::ObjectActivity::MAX_ENERGY_LEVEL;

			npcState.targetAngle += npcInfo.turnRate * deltaTime;

			const phys::Direction2D targetDirection = convertAnglesToForwardDirection(npcState.targetAngle);
			const phys::LinearVelocity2D targetVelocity = targetDirection * npcInfo.targetSpeed;
			const phys::LinearVelocity2D velocityRemaining = targetVelocity - linearVelocity.get(phys::X, phys::Z);
			const phys::LinearVelocity2D addedVelocity = velocityRemaining * (deltaTime / npcInfo.accelerationDuration);
			linearVelocity += addedVelocity.get(phys::X, 0, phys::Y);

			if (linearVelocity.get(phys::X, phys::Z) != 0) {
				const phys::Direction2D direction = normalize(linearVelocity.get(phys::X, phys::Z));
				npcState.desiredDirectionScale = expDecay(npcState.desiredDirectionScale, direction, 8_per_second, deltaTime);
				if (const Optional<phys::Angle> angle = tryGetAngle(npcState.desiredDirectionScale.get(phys::Y, phys::X))) {
					orientation = phys::Orientation3D::fromAngles(0, *angle, 0);
				}
			}

			npcState.mood += deltaTime.in(phys::SECONDS);
		}
	}

	static void animateRobotNPCs(Entities<ModelPose, const phys::LinearVelocity3D, const NPCState, const NPCInfo, const ModelType> entities, const RobotAnimations& robotAnimations,
		const phys::SimulationOptions3D& simulationOptions) {
		const phys::Time deltaTime = simulationOptions.stepInterval;
		for (auto&& [entityID, modelPose, linearVelocity, npcState, npcInfo, modelType] : entities) {
			if (modelType == ModelType{"ROBOT"}) {
				const phys::Speed speed = length(linearVelocity);
				const float runningAmount = clamp(static_cast<float>(speed / 10_kilometers_per_hour) - 0.5f, 0.0f, 1.0f);

				modelPose.animationTime += deltaTime * speed.in(phys::METERS_PER_SECOND) * ((speed < 10_kilometers_per_hour) ? 0.3_x : 0.18_x);
				modelPose.animationLayers.clear();
				if (robotAnimations.walkingAnimationIndex) {
					const float blendWeight = (robotAnimations.runningAnimationIndex) ? 1.0f - runningAmount : 1.0f;
					modelPose.animationLayers.push_back(ModelPose::AnimationLayer{.animationIndex = *robotAnimations.walkingAnimationIndex, .blendWeight = blendWeight});
				}
				if (robotAnimations.runningAnimationIndex) {
					const float blendWeight = (robotAnimations.walkingAnimationIndex) ? runningAmount : 1.0f;
					modelPose.animationLayers.push_back(ModelPose::AnimationLayer{.animationIndex = *robotAnimations.runningAnimationIndex, .blendWeight = blendWeight});
				}
				modelPose.morphTargetWeights = {
					0.5f + 0.5f * sin(npcState.mood),
					0.5f + 0.5f * sin(npcState.mood * 0.5f),
					0.5f + 0.5f * sin(npcState.mood * 2.0f),
				};
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createNPCControlSystem() { // NOLINT(misc-use-internal-linkage)
	return new NPCControlSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
