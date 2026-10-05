// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "object.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Graphics.hpp"
#include "../System.hpp"

#include <utility> // std::move

namespace {

// Systems:

void saveInterpolatedObjectPositions(exec::Entities<ObjectPreviousPosition, const phys::Position3D> objectEntities) {
	for (auto&& [entityID, previousPosition, position] : objectEntities) {
		previousPosition = {position};
	}
}

void saveInterpolatedObjectOrientations(exec::Entities<ObjectPreviousOrientation, const phys::Orientation3D> objectEntities) {
	for (auto&& [entityID, previousOrientation, orientation] : objectEntities) {
		previousOrientation = {orientation};
	}
}

void saveInterpolatedObjectScales(exec::Entities<ObjectPreviousScale, const phys::Scale3D> objectEntities) {
	for (auto&& [entityID, previousScale, scale] : objectEntities) {
		previousScale = {scale};
	}
}

void saveInterpolatedObjectLinearVelocities(exec::Entities<ObjectPreviousLinearVelocity, const phys::LinearVelocity3D> objectEntities) {
	for (auto&& [entityID, previousLinearVelocity, linearVelocity] : objectEntities) {
		previousLinearVelocity = {linearVelocity};
	}
}

void poseAnimatedObjects(exec::Entities<ObjectModelInstance, const ObjectAnimationState> objectEntities, const app::FrameInfo& frameInfo) {
	for (auto&& [entityID, modelInstance, animationState] : objectEntities) {
		modelInstance.pose = modelInstance.model->getBindPose();

		for (size_t animationLayerIndex = 0; true; ++animationLayerIndex) {
			const bool hasOldLayer = animationLayerIndex < animationState.previousAnimationLayers.size();
			const bool hasNewLayer = animationLayerIndex < animationState.animationLayers.size();
			if (hasOldLayer && hasNewLayer) {
				const ObjectAnimationState::AnimationLayer& oldAnimationLayer = animationState.previousAnimationLayers[animationLayerIndex];
				const ObjectAnimationState::AnimationLayer& newAnimationLayer = animationState.animationLayers[animationLayerIndex];
				if (newAnimationLayer.animationIndex == oldAnimationLayer.animationIndex) {
					const float newWeight = (newAnimationLayer.time < oldAnimationLayer.time) ? 1.0f : frameInfo.tickInterpolationAlpha;
					const float oldWeight = 1.0f - newWeight;
					if (const float blendWeight = oldAnimationLayer.blendWeight * oldWeight + newAnimationLayer.blendWeight * newWeight; blendWeight > 0.0f) {
						modelInstance.pose.applyAnimation({
							.animation = modelInstance.model->getAnimationAtIndex(newAnimationLayer.animationIndex),
							.time = oldAnimationLayer.time * oldWeight + newAnimationLayer.time * newWeight,
							.blendWeight = blendWeight,
							.looping = newAnimationLayer.looping,
						});
					}
				}
			}
			if (!hasOldLayer && !hasNewLayer) {
				break;
			}
		}

		const size_t morphTargetWeightCount = static_cast<size_t>(modelInstance.model->getMorphTargetWeightCount());
		for (size_t morphTargetWeightIndex = 0; morphTargetWeightIndex < morphTargetWeightCount; ++morphTargetWeightIndex) {
			const bool hasOldMorphTargetWeight = morphTargetWeightIndex < animationState.previousMorphTargetWeights.size();
			const bool hasNewMorphTargetWeight = morphTargetWeightIndex < animationState.morphTargetWeights.size();
			if (hasOldMorphTargetWeight && hasNewMorphTargetWeight) {
				modelInstance.pose.localMorphTargetWeights[morphTargetWeightIndex] =
					animationState.previousMorphTargetWeights[morphTargetWeightIndex] * (1.0f - frameInfo.tickInterpolationAlpha) +
					animationState.morphTargetWeights[morphTargetWeightIndex] * frameInfo.tickInterpolationAlpha;
			}
			if (!hasOldMorphTargetWeight && !hasNewMorphTargetWeight) {
				break;
			}
		}
	}
}

void poseObjectTransformations(exec::Entities<ObjectModelInstance, const phys::Position3D, const ObjectPreviousPosition, const phys::Orientation3D, const ObjectPreviousOrientation,
								   const phys::Scale3D, const ObjectPreviousScale>
								   interpolatedObjectEntities,
	const app::FrameInfo& frameInfo, const WorldView& worldView) {
	for (auto&& [entityID, modelInstance, position, previousPosition, orientation, previousOrientation, scale, previousScale] : interpolatedObjectEntities) {
		const phys::Position3D displayPosition = mix(previousPosition, position, frameInfo.tickInterpolationAlpha);
		const phys::Orientation3D displayOrientation = mix(previousOrientation, orientation, frameInfo.tickInterpolationAlpha);
		const phys::Scale3D displayScale = mix(previousScale, scale, frameInfo.tickInterpolationAlpha);
		modelInstance.transformation.assign(translateRotateScale(displayPosition, displayOrientation, displayScale) * modelInstance.localTransformation,
			modelInstance.pose.localJoints, modelInstance.pose.localMorphTargetWeights, modelInstance.model->getJointParentIndices());
		modelInstance.visible = worldView.isPotentiallyVisible(Graphics::getBoundingBox(*modelInstance.model, modelInstance.transformation));
	}
}

struct ObjectSystem final : System {
	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<saveInterpolatedObjectPositions>("Save interpolated object positions");
		scheduler.addTask<saveInterpolatedObjectOrientations>("Save interpolated object orientations");
		scheduler.addTask<saveInterpolatedObjectScales>("Save interpolated object scales");
		scheduler.addTask<saveInterpolatedObjectLinearVelocities>("Save interpolated object linear velocities");
	}

	void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount parallelism) override {
		scheduler.addParallelTransformationTask<poseAnimatedObjects>(parallelism, "Pose animated objects");
		scheduler.addParallelTransformationTask<poseObjectTransformations>(parallelism, "Pose object transformations");
	}

	void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) override {
		GREM_PROFILE_FUNCTION();

		const WorldView& worldView = simulation.resources.getResource<WorldView>();

		for (const auto& [entityID, modelInstance] : simulation.registry.getEntities<const ObjectModelInstance>()) {
			const Box<3, float> localBoundingBox = modelInstance.model->getBindPoseBoundingBox();
			const Length<3, float> localBoundingBoxSize = localBoundingBox.max - localBoundingBox.min;
			const float localBoundingRadius = modelInstance.model->getBindPoseBoundingRadius();
			const phys::Box3D boundingBox = Graphics::getBoundingBox(*modelInstance.model, modelInstance.transformation);
			const float maxScale = sqrt(maxComponent(vec3{
				length2(vec3{modelInstance.transformation.jointMatrices[0][0]}),
				length2(vec3{modelInstance.transformation.jointMatrices[0][1]}),
				length2(vec3{modelInstance.transformation.jointMatrices[0][2]}),
			}));
			const phys::Sphere3D boundingSphere{
				.center = vec3{modelInstance.transformation.jointMatrices[0][3]} * phys::METERS,
				.radius = maxScale * modelInstance.model->getBindPoseBoundingRadius() * phys::METERS,
			};
			const phys::Position3D blobShadowPosition{boundingSphere.center.getX(), boundingBox.min.getY(), boundingSphere.center.getZ()};
			const phys::Distance blobShadowRadius = boundingSphere.radius * min(localBoundingBoxSize.x, localBoundingBoxSize.z) * (0.5f / localBoundingRadius);
			const PlatformerModelShaderBlobShadow blobShadow{
				.blobShadowPosition = blobShadowPosition.in(phys::METERS),
				.blobShadowRadius = blobShadowRadius.in(phys::METERS),
				.blobShadowInstanceIdentifier = entityID.getIndex(),
			};
			if (worldView.isPotentiallyVisible(blobShadow.getBoundingBox(worldView.visibleBounds) * phys::METERS)) {
				graphics.blobShadows.push_back(blobShadow);
			}

			if (modelInstance.visible) {
				graphics.instances3D.putShadedModelInstance(graphics.modelShaderPipelineSet, *modelInstance.model, modelInstance.transformation,
					{.instanceIdentifier = entityID.getIndex()});
			}
		}
	}
} objectSystemImplementation{};

struct ObjectPhysicsSystem final : System {
	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D& resources, exec::Task::ParallelCount) override {
		phys::Simulation3D::scheduleStep(scheduler, resources.getResource<phys::SimulationOptions3D>());
	}
} objectPhysicsSystemImplementation{};

} // namespace

System* const objectSystem = &objectSystemImplementation;
System* const objectPhysicsSystem = &objectPhysicsSystemImplementation;

void createObject(phys::EntityBuilder3D& entity, const phys::ResourceRegistry3D& resources, const gfx::Model3D& model, phys::ObjectOptions3D&& options) {
	GREM_PROFILE_FUNCTION();

	phys::Simulation3D::addObjectComponents(entity.getRegistry(), resources, entity.getEntityID(), std::move(options));
	entity.addComponent<ObjectPreviousPosition>(entity.getComponent<phys::Position3D>());
	entity.addComponent<ObjectPreviousOrientation>(entity.getComponent<phys::Orientation3D>());
	entity.addComponent<ObjectPreviousScale>(entity.getComponent<phys::Scale3D>());
	entity.addComponent<ObjectPreviousLinearVelocity>(entity.getComponent<phys::LinearVelocity3D>());
	entity.addComponent<ObjectModelInstance>(ObjectModelInstance{.model = &model, .pose = model.getBindPose()});
}

void createAnimatedObject(phys::EntityBuilder3D& entity, const phys::ResourceRegistry3D& resources, const gfx::Model3D& model, phys::ObjectOptions3D&& options) {
	GREM_PROFILE_FUNCTION();

	createObject(entity, resources, model, std::move(options));
	entity.addComponent<ObjectAnimationState>();
}
