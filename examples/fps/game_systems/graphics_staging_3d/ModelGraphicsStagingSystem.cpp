// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "../../AssetCache.hpp"
#include "../../Graphics.hpp"
#include "../../Schema.hpp"
#include "../../Snapshot.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_data.hpp"

#include <memory>    // std::uninitialized_..., std::construct_at
#include <stdexcept> // std::length_error
#include <utility>   // std::move

class ModelGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"ModelGraphicsStagingSystem requires graphics."};
		}
		AssetCache& assetCache = resources.getResource<AssetCache>();
		Schema& schema = resources.getResource<Schema>();
		resources.addSharedResource<HullAssets>(graphics->device, assetCache);
		resources.addSharedResource<ModelAssets>(graphics->device, graphics->renderer3D, assetCache, schema);
		resources.addSharedResource<ModelInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<ModelInstances>();
		resources.removeResource<ModelAssets>();
		resources.removeResource<HullAssets>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics* graphics) override {
		GREM_ASSERT(graphics);
		AssetCache& assetCache = resources.getResource<AssetCache>();
		Schema& schema = resources.getResource<Schema>();
		resources.getResource<HullAssets>() = HullAssets{graphics->device, assetCache};
		resources.getResource<ModelAssets>() = ModelAssets{graphics->device, graphics->renderer3D, assetCache, schema};
	}

	void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor& executor, Graphics& graphics, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		AssetCache& assetCache = const_cast<AssetCache&>(worldView.subtickResources.getResource<AssetCache>());
		Schema& schema = const_cast<Schema&>(worldView.subtickResources.getResource<Schema>());
		ModelAssets& modelAssets = const_cast<ModelAssets&>(worldView.subtickResources.getResource<ModelAssets>());
		ModelInstances& modelInstances = const_cast<ModelInstances&>(worldView.subtickResources.getResource<ModelInstances>());

		const Entities<const ModelType> subtickModelEntities = worldView.subtickRegistry.getEntities<const ModelType>();
		const Entities<const ModelType> predictionInterpolationModelEntities =
			(worldView.predictionInterpolation) ? worldView.predictionInterpolation->snapshotB.registry.getEntities<const ModelType>() : Entities<const ModelType>{};
		const Entities<const ModelType> receivedInterpolationModelEntities =
			(worldView.receivedInterpolation) ? worldView.receivedInterpolation->snapshotB.registry.getEntities<const ModelType>() : Entities<const ModelType>{};

		{
			GREM_PROFILE_BLOCK("Allocate model instances");
			modelAssets.clearInstances();

			modelInstances.instanceReferences.clear();
			modelInstances.subtickInstanceIndices.assign(subtickModelEntities.size(), Limits<uint32_t>::MAX);
			modelInstances.predictionInterpolationInstanceIndices.assign(predictionInterpolationModelEntities.size(), Limits<uint32_t>::MAX);
			modelInstances.receivedInterpolationInstanceIndices.assign(receivedInterpolationModelEntities.size(), Limits<uint32_t>::MAX);
			const auto addInstance = [&](Span<uint32_t> instanceIndices, Span<const ModelType> modelEntities, SynchronizedEntityID synchronizedEntityID, EntityType entityType,
										 InterpolatedEntityView entity) -> void {
				const ModelType& modelType = entity.getNewComponent<ModelType>();
				if (modelType != ModelType{}) {
					const uint32_t modelInfoIndex = modelAssets.loadModelInfo(graphics.device, graphics.renderer3D, assetCache, schema, modelType);
					const uint32_t instanceIndex = modelAssets.addInstance(modelInfoIndex);
					instanceIndices[static_cast<size_t>(&modelType - modelEntities.data())] = instanceIndex;
					modelInstances.instanceReferences.push_back(ModelInstanceReference{
						.modelInfoIndex = modelInfoIndex,
						.instanceIndex = instanceIndex,
						.entityType = entityType,
						.entityFlags = entity.entityIDs.second.getFlags(),
						.synchronizedEntityID = synchronizedEntityID,
					});
				}
			};
			worldView.forEachEntitySeparatedByRegistryWithComponents<const ModelType>(
				[&](SynchronizedEntityID synchronizedEntityID, EntityType entityType, InterpolatedEntityView entity) -> void {
					addInstance(modelInstances.subtickInstanceIndices, subtickModelEntities, synchronizedEntityID, entityType, entity);
				},
				[&](SynchronizedEntityID synchronizedEntityID, EntityType entityType, InterpolatedEntityView entity) -> void {
					addInstance(modelInstances.predictionInterpolationInstanceIndices, predictionInterpolationModelEntities, synchronizedEntityID, entityType, entity);
				},
				[&](SynchronizedEntityID synchronizedEntityID, EntityType entityType, InterpolatedEntityView entity) -> void {
					addInstance(modelInstances.receivedInterpolationInstanceIndices, receivedInterpolationModelEntities, synchronizedEntityID, entityType, entity);
				});

			modelAssets.allocateInstanceData(modelInstances.arena);
		}

		{
			GREM_PROFILE_BLOCK("Transform model instances");
			executor.executeParallelOperation(modelInstances.instanceReferences, [&](ModelInstanceReference instanceReference) -> void {
				const auto& [modelType, modelInfo] = modelAssets.getAtIndex(instanceReference.modelInfoIndex);
				const size_t instanceIndex = instanceReference.instanceIndex;
				const SynchronizedEntityID synchronizedEntityID = instanceReference.synchronizedEntityID;
				const EntityType entityType = instanceReference.entityType;
				const Optional<InterpolatedEntityView> entity = worldView.findEntity(instanceReference.entityFlags, synchronizedEntityID, entityType);
				if (!entity) {
					return;
				}

				const Optional<WorldTransformation> displayTransformation = worldView.getEntityDisplayTransformation(*entity);
				if (!displayTransformation) {
					return;
				}

				Color tintColor = modelInfo.tintColor;
				if (entity->hasComponent<DamageableType>() && entity->hasComponent<DamageableState>()) {
					const DamageableType damageableType = entity->getNewComponent<DamageableType>();
					const DamageableState& damageableState = entity->getNewComponent<DamageableState>();
					if (damageableType != DamageableType{} && damageableState.lastDamagedOnTickIndex != TickIndex{}) {
						const Timestamp timestamp = worldView.getEntityDisplayTimestamp(instanceReference.entityFlags);
						const DamageableDescription& damageableDescription = schema.getDamageableDescription(damageableType);
						const phys::Time timeSinceDamage = getTimeBetween(Timestamp{damageableState.lastDamagedOnTickIndex}, timestamp, worldView.tickInterval);
						if (timeSinceDamage > 0 && timeSinceDamage < damageableDescription.flashDuration) {
							tintColor *= mix(damageableDescription.flashTintColor, Color::WHITE, timeSinceDamage / damageableDescription.flashDuration);
						}
					}
				}

				const res::Model::JointCount jointCount = modelInfo.visibleModel->getJointCount();
				mat4* const jointMatrices = modelInfo.jointMatrices + instanceIndex * static_cast<size_t>(jointCount);
				bool* const jointsVisible = modelInfo.jointsVisible + instanceIndex * static_cast<size_t>(jointCount);
				const phys::Length3D centerOfMass = modelInfo.jointCentersOfMass.front();
				jointMatrices[0] = translateRotateScale(displayTransformation->position, displayTransformation->orientation, displayTransformation->scale) *
				                   translate(-centerOfMass) * modelInfo.rootTransformation;
				jointsVisible[0] = true;

				const bool isSharedBetweenLocalPlayers =
					graphics.baking || determineEntityRenderCategory(worldView, LocalPlayerID{}, synchronizedEntityID, entityType, *entity, modelInfo.orientationAlignment) ==
										   EntityRenderCategory::SHARED_BETWEEN_LOCAL_PLAYERS;

				ModelAssets::DrawData& drawData = modelInfo.instancesDrawData[instanceIndex];
				drawData.instance.color = tintColor;
				drawData.instance.instanceIdentifier = static_cast<uint32_t>(synchronizedEntityID.value % (uint64_t{Limits<uint32_t>::MAX} + 1));
				drawData.renderedInDepthPrepass = isSharedBetweenLocalPlayers;
				drawData.renderedInWorld = isSharedBetweenLocalPlayers;
				drawData.castsShadow = isSharedBetweenLocalPlayers;
			});
		}

		{
			GREM_PROFILE_BLOCK("Set physics-controlled model joints");
			const auto setControlledModelJoint = [&](Span<const uint32_t> instanceIndices, Span<const ModelType> modelEntities, InterpolatedEntityView entity) -> void {
				const ModelJointController& modelJointController = entity.getNewComponent<ModelJointController>();
				const SynchronizedEntityMap& synchronizedEntityMapB = entity.snapshotB.resources.getResource<SynchronizedEntityMap>();
				const res::Model::JointIndex jointIndex = modelJointController.jointIndex;
				const EntityID targetEntityID = synchronizedEntityMapB.findEntity(entity.snapshotB.registry, modelJointController.target);
				if (!targetEntityID) {
					return;
				}
				const ModelType* const modelType = entity.snapshotB.registry.findComponent<ModelType>(targetEntityID);
				if (!modelType || *modelType == ModelType{}) {
					return;
				}
				const uint32_t instanceIndex = instanceIndices[static_cast<size_t>(modelType - modelEntities.data())];
				if (instanceIndex == Limits<uint32_t>::MAX) {
					return;
				}

				ModelAssets::ModelInfo& modelInfo = modelAssets.getAtIndex(modelAssets.getLoadedModelInfoIndex(*modelType)).second;
				const res::Model::JointCount jointCount = modelInfo.visibleModel->getJointCount();
				GREM_ASSERT(jointIndex < jointCount);

				res::Model::Joint& localJoint = modelInfo.localJoints[static_cast<size_t>(instanceIndex) * static_cast<size_t>(jointCount) + jointIndex];
				res::Model::JointIndex& jointParentIndex = modelInfo.jointParentIndices[static_cast<size_t>(instanceIndex) * static_cast<size_t>(jointCount) + jointIndex];
				const mat4* const jointMatrices = modelInfo.jointMatrices + static_cast<size_t>(instanceIndex) * static_cast<size_t>(jointCount);

				const phys::Length3D centerOfMass = modelInfo.jointCentersOfMass[jointIndex];
				const mat4 jointMatrix = translateRotateScale(entity.getInterpolatedComponent<phys::Position3D>(), entity.getInterpolatedComponent<phys::Orientation3D>(),
											 entity.getInterpolatedComponent<phys::Scale3D>()) *
				                         translate(-centerOfMass);
				const mat4 rootJointMatrix = jointMatrices[0];
				const mat4 localJointMatrix = inverse(rootJointMatrix) * jointMatrix;
				const auto [translation, rotation, scale] = decomposeTranslationRotationScale(localJointMatrix);
				localJoint.translation = translation;
				localJoint.rotation = rotation;
				localJoint.scale = scale;
				jointParentIndex = 0;
			};
			worldView.forEachEntitySeparatedByRegistryWithComponents<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const ModelJointController>(
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					setControlledModelJoint(modelInstances.subtickInstanceIndices, subtickModelEntities, entity);
				},
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					setControlledModelJoint(modelInstances.predictionInterpolationInstanceIndices, predictionInterpolationModelEntities, entity);
				},
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					setControlledModelJoint(modelInstances.receivedInterpolationInstanceIndices, receivedInterpolationModelEntities, entity);
				});
		}

		{
			GREM_PROFILE_BLOCK("Pose model instances");
			executor.executeParallelOperation(modelInstances.instanceReferences, [&](ModelInstanceReference instanceReference) -> void {
				const auto& [modelType, modelInfo] = modelAssets.getAtIndex(instanceReference.modelInfoIndex);
				const size_t instanceIndex = instanceReference.instanceIndex;
				const SynchronizedEntityID synchronizedEntityID = instanceReference.synchronizedEntityID;
				const EntityType entityType = instanceReference.entityType;
				const Optional<InterpolatedEntityView> entity = worldView.findEntity(instanceReference.entityFlags, synchronizedEntityID, entityType);
				if (!entity) {
					return;
				}

				const gfx::Model3D& model = *modelInfo.visibleModel;
				const size_t jointCount = static_cast<size_t>(model.getJointCount());
				const size_t morphTargetWeightCount = static_cast<size_t>(model.getMorphTargetWeightCount());
				const res::Model::PoseReference pose{
					.localJoints = modelInfo.localJoints + instanceIndex * jointCount,
					.localMorphTargetWeights = modelInfo.localMorphTargetWeights + instanceIndex * morphTargetWeightCount,
				};
				const res::Model::TransformationReference transformation{
					.jointMatrices = modelInfo.jointMatrices + instanceIndex * jointCount,
					.jointsVisible = modelInfo.jointsVisible + instanceIndex * jointCount,
					.morphTargetWeights = modelInfo.morphTargetWeights + instanceIndex * morphTargetWeightCount,
				};
				const res::Model::JointIndex* const jointParentIndices = modelInfo.jointParentIndices + instanceIndex * jointCount;

				if (entity->hasComponent<ModelPose>()) {
					const float oldWeight = 1.0f - entity->interpolationAlpha;
					const float newWeight = entity->interpolationAlpha;
					const ModelPose& oldModelPose = entity->getOldComponent<ModelPose>();
					const ModelPose& newModelPose = entity->getNewComponent<ModelPose>();
					for (size_t animationLayerIndex = 0; true; ++animationLayerIndex) {
						const bool hasOldLayer = animationLayerIndex < oldModelPose.animationLayers.size();
						const bool hasNewLayer = animationLayerIndex < newModelPose.animationLayers.size();
						if (hasOldLayer && hasNewLayer) {
							const ModelPose::AnimationLayer& oldAnimationLayer = oldModelPose.animationLayers[animationLayerIndex];
							const ModelPose::AnimationLayer& newAnimationLayer = newModelPose.animationLayers[animationLayerIndex];
							pose.applyAnimation({
								.animation = model.getAnimationAtIndex(oldAnimationLayer.animationIndex),
								.time = oldModelPose.animationTime * oldWeight + newModelPose.animationTime * newWeight,
								.blendWeight = oldAnimationLayer.blendWeight * oldWeight + newAnimationLayer.blendWeight * newWeight,
							});
						}
						if (!hasOldLayer && !hasNewLayer) {
							break;
						}
					}
					for (size_t morphTargetWeightIndex = 0; morphTargetWeightIndex < morphTargetWeightCount; ++morphTargetWeightIndex) {
						const bool hasOldMorphTargetWeight = morphTargetWeightIndex < oldModelPose.morphTargetWeights.size();
						const bool hasNewMorphTargetWeight = morphTargetWeightIndex < newModelPose.morphTargetWeights.size();
						if (hasOldMorphTargetWeight && hasNewMorphTargetWeight) {
							pose.localMorphTargetWeights[morphTargetWeightIndex] =
								oldModelPose.morphTargetWeights[morphTargetWeightIndex] * oldWeight + newModelPose.morphTargetWeights[morphTargetWeightIndex] * newWeight;
						}
						if (!hasOldMorphTargetWeight && !hasNewMorphTargetWeight) {
							break;
						}
					}
				}

				fill(Span{transformation.morphTargetWeights, morphTargetWeightCount}, 0.0f);
				transformation.pose(Span{pose.localJoints, jointCount}, Span{pose.localMorphTargetWeights, morphTargetWeightCount}, Span{jointParentIndices, jointCount});
			});
		}

		{
			GREM_PROFILE_BLOCK("Pose model joint lights");
			const auto poseModelJointLight = [&](Span<const uint32_t> instanceIndices, Span<const ModelType> modelEntities, InterpolatedEntityView entity) -> void {
				const ModelJointLight& modelJointLight = entity.getNewComponent<ModelJointLight>();
				const SynchronizedEntityMap& synchronizedEntityMapB = entity.snapshotB.resources.getResource<SynchronizedEntityMap>();
				if (const EntityID targetEntityID = synchronizedEntityMapB.findEntity(entity.snapshotB.registry, modelJointLight.target)) {
					if (const ModelType* const modelType = entity.snapshotB.registry.findComponent<ModelType>(targetEntityID)) {
						if (*modelType != ModelType{}) {
							const uint32_t instanceIndex = instanceIndices[static_cast<size_t>(modelType - modelEntities.data())];
							if (instanceIndex != Limits<uint32_t>::MAX) {
								const ModelAssets::ModelInfo& modelInfo = modelAssets.getAtIndex(modelAssets.getLoadedModelInfoIndex(*modelType)).second;
								const res::Model::JointCount jointCount = modelInfo.visibleModel->getJointCount();
								const mat4* const jointMatrices = modelInfo.jointMatrices + static_cast<size_t>(instanceIndex) * static_cast<size_t>(jointCount);
								GREM_ASSERT(modelJointLight.jointIndex < jointCount);
								const auto [translation, rotation, scale] = decomposeTranslationRotationScale(jointMatrices[modelJointLight.jointIndex]);
								if (entity.hasComponent<gfx::DirectionalLightOptions3D>()) {
									gfx::DirectionalLightOptions3D& directionalLightOptionsA =
										const_cast<gfx::DirectionalLightOptions3D&>(entity.getOldComponent<gfx::DirectionalLightOptions3D>());
									gfx::DirectionalLightOptions3D& directionalLightOptionsB =
										const_cast<gfx::DirectionalLightOptions3D&>(entity.getNewComponent<gfx::DirectionalLightOptions3D>());
									const vec3 direction = rotation * vec3{0.0f, 0.0f, -1.0f};
									directionalLightOptionsA.direction = direction;
									directionalLightOptionsB.direction = direction;
								}
								if (entity.hasComponent<gfx::PointLightOptions3D>()) {
									gfx::PointLightOptions3D& pointLightOptionsA = const_cast<gfx::PointLightOptions3D&>(entity.getOldComponent<gfx::PointLightOptions3D>());
									gfx::PointLightOptions3D& pointLightOptionsB = const_cast<gfx::PointLightOptions3D&>(entity.getNewComponent<gfx::PointLightOptions3D>());
									pointLightOptionsA.position = translation;
									pointLightOptionsB.position = translation;
								}
								if (entity.hasComponent<gfx::SpotLightOptions3D>()) {
									gfx::SpotLightOptions3D& spotLightOptionsA = const_cast<gfx::SpotLightOptions3D&>(entity.getOldComponent<gfx::SpotLightOptions3D>());
									gfx::SpotLightOptions3D& spotLightOptionsB = const_cast<gfx::SpotLightOptions3D&>(entity.getNewComponent<gfx::SpotLightOptions3D>());
									const vec3 direction = rotation * vec3{0.0f, 0.0f, -1.0f};
									spotLightOptionsA.position = translation;
									spotLightOptionsB.position = translation;
									spotLightOptionsA.direction = direction;
									spotLightOptionsB.direction = direction;
								}
							}
						}
					}
				}
			};
			worldView.forEachEntitySeparatedByRegistryWithComponents<const ModelJointLight>(
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					poseModelJointLight(modelInstances.subtickInstanceIndices, subtickModelEntities, entity);
				},
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					poseModelJointLight(modelInstances.predictionInterpolationInstanceIndices, predictionInterpolationModelEntities, entity);
				},
				[&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
					poseModelJointLight(modelInstances.receivedInterpolationInstanceIndices, receivedInterpolationModelEntities, entity);
				});
		}

		{
			GREM_PROFILE_BLOCK("Cull model instances not in view");
			executor.executeParallelOperation(modelInstances.instanceReferences, [&](ModelInstanceReference instanceReference) -> void {
				const auto& [modelType, modelInfo] = modelAssets.getAtIndex(instanceReference.modelInfoIndex);
				const size_t instanceIndex = instanceReference.instanceIndex;
				ModelAssets::DrawData& drawData = modelInfo.instancesDrawData[instanceIndex];
				if (drawData.renderedInDepthPrepass | drawData.renderedInWorld) {
					const gfx::Model3D& model = *modelInfo.visibleModel;
					const size_t jointCount = static_cast<size_t>(model.getJointCount());
					const size_t morphTargetWeightCount = static_cast<size_t>(model.getMorphTargetWeightCount());
					const res::Model::TransformationReference transformation{
						.jointMatrices = modelInfo.jointMatrices + instanceIndex * jointCount,
						.jointsVisible = modelInfo.jointsVisible + instanceIndex * jointCount,
						.morphTargetWeights = modelInfo.morphTargetWeights + instanceIndex * morphTargetWeightCount,
					};
					const bool visible = worldView.isBoundingBoxPotentiallyVisible(model, transformation);
					drawData.renderedInDepthPrepass &= visible;
					drawData.renderedInWorld &= visible;
				}
			});
		}

		{
			GREM_PROFILE_BLOCK("Put model instances");
			for (const auto& [modelType, modelInfo] : modelAssets.getLoadedModelInfos()) {
				GREM_PROFILE_BLOCK_DYNAMIC(modelInfo.filepath);
				const Span<const ModelAssets::DrawData> instancesDrawData{modelInfo.instancesDrawData, modelInfo.instanceCount};
				const StridedSpan<const res::Model::TransformationView> transformations{instancesDrawData, &ModelAssets::DrawData::transformation};
				const StridedSpan<const gfx::ModelInstance3D> instances{instancesDrawData, &ModelAssets::DrawData::instance};
				if (!modelInfo.excludeFromDepthPrepass) {
					const StridedSpan<const bool> depthPrepassVisibility{instancesDrawData, &ModelAssets::DrawData::renderedInDepthPrepass};
					graphics.depthPrepassInstances3D.putVisibleShadedModelInstances(graphics.renderer3D.getShadowMapModel3DShaderPipelineSet(), *modelInfo.visibleModel,
						transformations, instances, depthPrepassVisibility);
				}
				const StridedSpan<const bool> worldVisibility{instancesDrawData, &ModelAssets::DrawData::renderedInWorld};
				graphics.visibleInstances3D.putVisibleHDRPBRModelInstances(*modelInfo.visibleModel, transformations, instances, worldVisibility);
				if (modelInfo.shadowCasterModel) {
					const StridedSpan<const bool> shadowVisibility{instancesDrawData, &ModelAssets::DrawData::castsShadow};
					graphics.shadowCasterInstances3D.putVisibleShadedModelInstances(graphics.renderer3D.getShadowMapModel3DShaderPipelineSet(), *modelInfo.shadowCasterModel,
						transformations, instances, shadowVisibility);
				}
				if (graphics.baking) {
					graphics.baking->lightProbeOccluderInstances3D.putVisibleShadedModelInstances(graphics.renderer3D.getDistanceModel3DShaderPipelineSet(),
						*modelInfo.visibleModel, transformations, instances, worldVisibility);
				}
			}
			if (graphics.serverPhysicsDebugVisualization) {
				graphics.serverPhysicsDebugVisualization->putWorldVisualizationInstances(graphics.renderer3D, graphics.visibleInstances3D);
				graphics.serverPhysicsDebugVisualization->putUIVisualizationInstances(graphics.renderer2D, graphics.instances2D, *graphics.mainFont);
			}
			if (graphics.clientPhysicsDebugVisualization) {
				graphics.clientPhysicsDebugVisualization->putWorldVisualizationInstances(graphics.renderer3D, graphics.visibleInstances3D);
				graphics.clientPhysicsDebugVisualization->putUIVisualizationInstances(graphics.renderer2D, graphics.instances2D, *graphics.mainFont);
			}
		}

		if (graphics.serverPhysicsDebugVisualization || graphics.clientPhysicsDebugVisualization) {
			GREM_PROFILE_BLOCK("Put hull instances");

			HullAssets& hullAssets = const_cast<HullAssets&>(worldView.subtickResources.getResource<HullAssets>());
			uint32_t instanceOffset = graphics.hullInstanceBuffer.size();
			for (const auto& [modelType, modelInfo] : modelAssets.getLoadedModelInfos()) {
				if (const Optional<HullMesh>& hullMesh = hullAssets.loadHullMeshOfLoadedModel(graphics.device, assetCache, schema, modelType)) {
					uint32_t instanceCount = 0;
					for (const ModelAssets::DrawData& instanceDrawData : Span{modelInfo.instancesDrawData, modelInfo.instanceCount}) {
						if (instanceDrawData.renderedInWorld) {
							graphics.hullInstanceBuffer.push(HullInstance{.instanceTransformation = instanceDrawData.transformation.jointMatrices[0]});
							++instanceCount;
						}
					}
					if (instanceCount > 0) {
						graphics.hullDrawCommandBuffer.append(hullAssets.getShaderPipeline(), *hullMesh, instanceOffset, instanceCount);
						instanceOffset += instanceCount;
					}
				}
			}
		}
	}

	void stageLocalPlayer3DGraphics(exec::Executor& executor, Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const gfx::Viewport&,
		const gfx::Camera3D& camera) override {
		if (graphics.baking) {
			return;
		}

		ModelAssets& modelAssets = const_cast<ModelAssets&>(worldView.subtickResources.getResource<ModelAssets>());
		ModelInstances& modelInstances = const_cast<ModelInstances&>(worldView.subtickResources.getResource<ModelInstances>());

		{
			GREM_PROFILE_BLOCK("Transform local model instances");
			executor.executeParallelOperation(modelInstances.instanceReferences, [&](ModelInstanceReference instanceReference) -> void {
				const auto& [modelType, modelInfo] = modelAssets.getAtIndex(instanceReference.modelInfoIndex);
				const size_t instanceIndex = instanceReference.instanceIndex;
				const SynchronizedEntityID synchronizedEntityID = instanceReference.synchronizedEntityID;
				const EntityType entityType = instanceReference.entityType;
				const Optional<InterpolatedEntityView> entity = worldView.findEntity(instanceReference.entityFlags, synchronizedEntityID, entityType);
				if (!entity) {
					return;
				}

				const Optional<WorldTransformation> displayTransformation = worldView.getEntityDisplayTransformation(*entity);
				if (!displayTransformation) {
					return;
				}

				ModelAssets::DrawData& drawData = modelInfo.instancesDrawData[instanceIndex];
				const EntityRenderCategory renderCategory =
					determineEntityRenderCategory(worldView, localPlayerID, synchronizedEntityID, entityType, *entity, modelInfo.orientationAlignment);
				switch (renderCategory) {
					case EntityRenderCategory::SHARED_BETWEEN_LOCAL_PLAYERS:
						drawData.renderedInDepthPrepass = false;
						drawData.renderedInWorld = false;
						drawData.renderedInView = false;
						drawData.castsShadow = false;
						break;
					case EntityRenderCategory::ORIENTATION_ALIGNED_PER_LOCAL_PLAYER: {
						const gfx::Model3D& model = *modelInfo.visibleModel;
						mat4* const jointMatrices = modelInfo.jointMatrices + instanceIndex * static_cast<size_t>(model.getJointCount());
						const phys::Length3D centerOfMass = modelInfo.jointCentersOfMass.front();
						const phys::Orientation3D alignedOrientation = worldView.getAlignedOrientation(displayTransformation->orientation, modelInfo.orientationAlignment, camera);
						jointMatrices[0] = translateRotateScale(displayTransformation->position, alignedOrientation, displayTransformation->scale) * translate(-centerOfMass) *
						                   modelInfo.rootTransformation;
						drawData.renderedInDepthPrepass = true;
						drawData.renderedInWorld = true;
						drawData.renderedInView = false;
						drawData.castsShadow = true;
						break;
					}
					case EntityRenderCategory::OTHER_LOCAL_PLAYER_ENTITY: [[fallthrough]];
					case EntityRenderCategory::CURRENT_LOCAL_PLAYER_ENTITY_THIRD_PERSON_PERSPECTIVE:
						drawData.renderedInDepthPrepass = true;
						drawData.renderedInWorld = true;
						drawData.renderedInView = false;
						drawData.castsShadow = true;
						break;
					case EntityRenderCategory::CURRENT_LOCAL_PLAYER_ENTITY_FIRST_PERSON_PERSPECTIVE:
						drawData.renderedInDepthPrepass = false;
						drawData.renderedInWorld = false;
						drawData.renderedInView = false;
						drawData.castsShadow = true;
						break;
					case EntityRenderCategory::OTHER_LOCAL_PLAYER_WEAPON_ENTITY: [[fallthrough]];
					case EntityRenderCategory::CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_THIRD_PERSON_PERSPECTIVE:
						drawData.renderedInDepthPrepass = false;
						drawData.renderedInWorld = true;
						drawData.renderedInView = false;
						drawData.castsShadow = true;
						break;
					case EntityRenderCategory::CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_FIRST_PERSON_PERSPECTIVE:
						drawData.renderedInDepthPrepass = true;
						drawData.renderedInWorld = false;
						drawData.renderedInView = true;
						drawData.castsShadow = true;
						break;
				}
			});
		}

		{
			GREM_PROFILE_BLOCK("Cull local model instances not in view");
			executor.executeParallelOperation(modelInstances.instanceReferences, [&](ModelInstanceReference instanceReference) -> void {
				const auto& [modelType, modelInfo] = modelAssets.getAtIndex(instanceReference.modelInfoIndex);
				const size_t instanceIndex = instanceReference.instanceIndex;
				ModelAssets::DrawData& drawData = modelInfo.instancesDrawData[instanceIndex];
				if (drawData.renderedInDepthPrepass | drawData.renderedInWorld | drawData.renderedInView) {
					const gfx::Model3D& model = *modelInfo.visibleModel;
					const size_t jointCount = static_cast<size_t>(model.getJointCount());
					const size_t morphTargetWeightCount = static_cast<size_t>(model.getMorphTargetWeightCount());
					const res::Model::TransformationReference transformation{
						.jointMatrices = modelInfo.jointMatrices + instanceIndex * jointCount,
						.jointsVisible = modelInfo.jointsVisible + instanceIndex * jointCount,
						.morphTargetWeights = modelInfo.morphTargetWeights + instanceIndex * morphTargetWeightCount,
					};
					const bool visible = worldView.isBoundingBoxPotentiallyVisible(model, transformation);
					drawData.renderedInDepthPrepass &= visible;
					drawData.renderedInWorld &= visible;
					drawData.renderedInView &= visible;
				}
			});
		}

		{
			GREM_PROFILE_BLOCK("Put local model instances");
			for (const auto& [modelType, modelInfo] : modelAssets.getLoadedModelInfos()) {
				GREM_PROFILE_BLOCK_DYNAMIC(modelInfo.filepath);
				const Span<const ModelAssets::DrawData> instancesDrawData{modelInfo.instancesDrawData, modelInfo.instanceCount};
				const StridedSpan<const res::Model::TransformationView> transformations{instancesDrawData, &ModelAssets::DrawData::transformation};
				const StridedSpan<const gfx::ModelInstance3D> instances{instancesDrawData, &ModelAssets::DrawData::instance};
				if (!modelInfo.excludeFromDepthPrepass) {
					const StridedSpan<const bool> depthPrepassVisibility{instancesDrawData, &ModelAssets::DrawData::renderedInDepthPrepass};
					graphics.localPlayerDepthPrepassInstances3D.putVisibleShadedModelInstances(graphics.renderer3D.getShadowMapModel3DShaderPipelineSet(), *modelInfo.visibleModel,
						transformations, instances, depthPrepassVisibility);
				}
				const StridedSpan<const bool> worldVisibility{instancesDrawData, &ModelAssets::DrawData::renderedInWorld};
				graphics.localPlayerVisibleInWorldInstances3D.putVisibleHDRPBRModelInstances(*modelInfo.visibleModel, transformations, instances, worldVisibility);
				const StridedSpan<const bool> viewVisibility{instancesDrawData, &ModelAssets::DrawData::renderedInView};
				graphics.localPlayerVisibleInViewInstances3D.putVisibleHDRPBRModelInstances(*modelInfo.visibleModel, transformations, instances, viewVisibility);
				if (modelInfo.shadowCasterModel) {
					const StridedSpan<const bool> shadowVisibility{instancesDrawData, &ModelAssets::DrawData::castsShadow};
					graphics.localPlayerShadowCasterInstances3D.putVisibleShadedModelInstances(graphics.renderer3D.getShadowMapModel3DShaderPipelineSet(),
						*modelInfo.shadowCasterModel, transformations, instances, shadowVisibility);
				}
			}
		}
	}

private:
	class HullAssets {
	public:
		static constexpr HullVertexShaderConstants VERTEX_SHADER_CONSTANTS{};
		static constexpr HullFragmentShaderConstants FRAGMENT_SHADER_CONSTANTS{};

		static constexpr gfx::ShaderPipelineOptions SHADER_PIPELINE_OPTIONS{
			.depthTestPredicate = gfx::DepthTestPredicate::ALWAYS_PASS,
			.primitiveType = gfx::PrimitiveType::LINES,
		};

		HullAssets(gfx::Device& device, AssetCache& assetCache)
			: shaderPipeline(device, assetCache.getVertexShader<HullVertexShader>(device, "shaders/hull.vert"), VERTEX_SHADER_CONSTANTS,
				  assetCache.getFragmentShader<HullFragmentShader>(device, "shaders/hull.frag"), FRAGMENT_SHADER_CONSTANTS, SHADER_PIPELINE_OPTIONS) {}

		[[nodiscard]] const gfx::ShaderPipeline<HullMesh>& getShaderPipeline() const noexcept {
			return shaderPipeline;
		}

		[[nodiscard]] const Optional<HullMesh>& loadHullMeshOfLoadedModel(gfx::Device& device, AssetCache& assetCache, Schema& schema, ModelType modelType) {
			const auto [it, inserted] = hullMeshes.try_emplace(modelType);
			if (inserted) {
				try {
					it->second = loadHullMesh(device, assetCache, schema, modelType);
				} catch (...) {
					hullMeshes.erase(it);
					throw;
				}
			}
			return it->second;
		}

	private:
		[[nodiscard]] static Optional<HullMesh> loadHullMesh(gfx::Device& device, AssetCache& assetCache, Schema& schema, ModelType modelType) {
			const ModelDescription& modelDescription = schema.getLoadedModelDescription(modelType);

			const phys::Shape3D* shape = nullptr;
			if (modelDescription.shapeOverride) {
				shape = &*modelDescription.shapeOverride;
			} else {
				const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);
				if (modelObjectDescription.jointDescriptions.front().physicsObjectIndex) {
					shape = &modelObjectDescription.physicsObjectDescriptions[*modelObjectDescription.jointDescriptions.front().physicsObjectIndex].collider.shape;
				} else if (modelObjectDescription.physicsObjectDescriptions.empty()) {
					shape = &schema.loadModelConvexHullShape(assetCache, modelType);
				}
			}
			if (!shape) {
				return {};
			}
			ConvexPolytope3D* convexPolytope = nullptr;
			if (const phys::LocallyTransformedShape3D* const locallyTransformedShape = shape->get_if<phys::LocallyTransformedShape3D>()) {
				if (const phys::ConvexPolytopeShape3D* const convexPolytopeShape = locallyTransformedShape->shape->get_if<phys::ConvexPolytopeShape3D>()) {
					convexPolytope = convexPolytopeShape->getConvexPolytope().get();
				}
			} else {
				if (const phys::ConvexPolytopeShape3D* const convexPolytopeShape = shape->get_if<phys::ConvexPolytopeShape3D>()) {
					convexPolytope = convexPolytopeShape->getConvexPolytope().get();
				}
			}
			if (!convexPolytope) {
				return {};
			}
			const Span<const ConvexPolytopeVertex3D> vertices = convexPolytope->getVertices();
			const Span<const ConvexPolytopeEdge3D> edges = convexPolytope->getEdges();
			const phys::InverseLocalTransformation3D inverseRootTransformation =
				inverse(translateRotateScale(modelDescription.options.rootTranslation * phys::METERS, modelDescription.options.rootRotation, modelDescription.options.rootScale));
			GREM_ASSERT(edges.size() % 2 == 0);
			Buffer<HullVertex> hullWireframeVertices{};
			for (const ConvexPolytope3D::Edge& edge : edges) {
				hullWireframeVertices.push_back(HullVertex{.vertexPosition{inverseRootTransformation(vertices[edge.vertexIndex] * phys::METERS).in(phys::METERS)}});
			}
			return HullMesh{device, hullWireframeVertices};
		}

		HashMap<ModelType, Optional<HullMesh>> hullMeshes{};
		gfx::ShaderPipeline<HullMesh> shaderPipeline;
	};

	class ModelAssets {
	public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
		struct alignas(64) DrawData {
			res::Model::TransformationView transformation;
			gfx::ModelInstance3D instance;
			bool renderedInDepthPrepass;
			bool renderedInWorld;
			bool renderedInView;
			bool castsShadow;
		};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

		struct ModelInfo {
			String filepath{};
			SharedPointer<gfx::Model3D> visibleModel{};
			SharedPointer<gfx::Model3D> shadowCasterModel{};
			phys::LocalTransformation3D rootTransformation{};
			Allocation<phys::Length3D> jointCentersOfMass{};
			OrientationAlignment orientationAlignment = OrientationAlignment::NONE;
			Color tintColor = Color::WHITE;
			bool excludeFromDepthPrepass = false;
			res::Model::Joint* localJoints = nullptr;
			float* localMorphTargetWeights = nullptr;
			mat4* jointMatrices = nullptr;
			bool* jointsVisible = nullptr;
			float* morphTargetWeights = nullptr;
			res::Model::JointIndex* jointParentIndices = nullptr;
			DrawData* instancesDrawData = nullptr;
			size_t instanceCount = 0;
		};

		ModelAssets(gfx::Device& device, gfx::Renderer3D& renderer3D, AssetCache& assetCache, Schema& schema) {
			info.reserve(schema.getLoadedModelDescriptions().size());
			for (const auto& [modelType, modelDescription] : schema.getLoadedModelDescriptions()) {
				loadModelInfo(device, renderer3D, assetCache, schema, modelType);
			}
		}

		void clearInstances() noexcept {
			for (auto&& [modelType, modelInfo] : info) {
				modelInfo.instanceCount = 0;
			}
		}

		void allocateInstanceData(Arena<0>& arena) {
			static_assert(trivially_destructible<mat4>);
			static_assert(trivially_destructible<float>);
			static_assert(trivially_destructible<res::Model::Joint>);
			static_assert(trivially_destructible<res::Model::JointIndex>);
			static_assert(trivially_destructible<ModelAssets::DrawData>);
			arena.release();
			for (auto&& [modelType, modelInfo] : info) {
				const size_t instanceCount = modelInfo.instanceCount;
				if (instanceCount > 0) {
					const gfx::Model3D& model = *modelInfo.visibleModel;
					const size_t jointCount = static_cast<size_t>(model.getJointCount());
					const size_t morphTargetWeightCount = static_cast<size_t>(model.getMorphTargetWeightCount());
					const size_t totalJointCount = instanceCount * jointCount;
					const size_t totalMorphTargetWeightCount = instanceCount * morphTargetWeightCount;
					const res::Model::Joint* const localBindPoseJoints = model.getBindPose().localJoints.data();
					const float* const localBindPoseMorphTargetWeights = model.getBindPose().localMorphTargetWeights.data();
					const res::Model::JointIndex* const defaultJointParentIndices = model.getJointParentIndices().data();

					res::Model::Joint* const localJoints = ArenaAllocator<res::Model::Joint>{&arena}.allocate(totalJointCount);
					for (size_t i = 0; i < instanceCount; ++i) {
						std::uninitialized_copy_n(localBindPoseJoints, jointCount, localJoints + i * jointCount);
					}
					float* const localMorphTargetWeights = (totalMorphTargetWeightCount > 0) ? ArenaAllocator<float>{&arena}.allocate(totalMorphTargetWeightCount) : nullptr;
					for (size_t i = 0; i < instanceCount; ++i) {
						std::uninitialized_copy_n(localBindPoseMorphTargetWeights, morphTargetWeightCount, localMorphTargetWeights + i * morphTargetWeightCount);
					}

					mat4* const jointMatrices = ArenaAllocator<mat4>{&arena}.allocate(totalJointCount);
					std::uninitialized_default_construct_n(jointMatrices, totalJointCount);
					bool* const jointsVisible = ArenaAllocator<bool>{&arena}.allocate(totalJointCount);
					std::uninitialized_default_construct_n(jointsVisible, totalJointCount);
					float* const morphTargetWeights = (totalMorphTargetWeightCount > 0) ? ArenaAllocator<float>{&arena}.allocate(totalMorphTargetWeightCount) : nullptr;
					std::uninitialized_default_construct_n(morphTargetWeights, totalMorphTargetWeightCount);

					res::Model::JointIndex* const jointParentIndices = ArenaAllocator<res::Model::JointIndex>{&arena}.allocate(totalJointCount);
					for (size_t i = 0; i < instanceCount; ++i) {
						std::uninitialized_copy_n(defaultJointParentIndices, jointCount, jointParentIndices + i * jointCount);
					}

					ModelAssets::DrawData* const instancesDrawData = ArenaAllocator<ModelAssets::DrawData>{&arena}.allocate(instanceCount);
					for (size_t i = 0; i < instanceCount; ++i) {
						std::construct_at(instancesDrawData + i,
							ModelAssets::DrawData{
								.transformation{
									.jointMatrices = jointMatrices + i * jointCount,
									.jointsVisible = jointsVisible + i * jointCount,
									.morphTargetWeights = morphTargetWeights + i * morphTargetWeightCount,
								},
								.instance{},
								.renderedInDepthPrepass = false,
								.renderedInWorld = false,
								.renderedInView = false,
								.castsShadow = false,
							});
					}

					modelInfo.localJoints = localJoints;
					modelInfo.localMorphTargetWeights = localMorphTargetWeights;
					modelInfo.jointMatrices = jointMatrices;
					modelInfo.jointsVisible = jointsVisible;
					modelInfo.morphTargetWeights = morphTargetWeights;
					modelInfo.jointParentIndices = jointParentIndices;
					modelInfo.instancesDrawData = instancesDrawData;
				}
			}
		}

		uint32_t loadModelInfo(gfx::Device& device, gfx::Renderer3D& renderer3D, AssetCache& assetCache, Schema& schema, ModelType modelType) {
			const auto [it, inserted] = info.try_emplace(modelType);
			if (inserted) {
				try {
					const ModelDescription& modelDescription = schema.loadModelDescription(assetCache.getFilesystem(), modelType);
					const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);
					SharedPointer<gfx::Model3D> visibleModel = assetCache.getModel3D(device, renderer3D, modelDescription.filepath, modelDescription.options);
					SharedPointer<gfx::Model3D> shadowCasterModel{};
					switch (modelDescription.shadowCasterType) {
						case ModelDescription::ShadowCasterType::NONE: break;
						case ModelDescription::ShadowCasterType::FULL_QUALITY: shadowCasterModel = visibleModel; break;
						case ModelDescription::ShadowCasterType::INSET_CONVEX_HULL:
							shadowCasterModel = createShadowCasterModelFromConvexHull(device, renderer3D, assetCache, schema, modelType);
							break;
					}
					it->second.filepath = modelDescription.filepath;
					it->second.visibleModel = std::move(visibleModel);
					it->second.shadowCasterModel = std::move(shadowCasterModel);
					it->second.rootTransformation = modelDescription.rootTransformation;
					it->second.jointCentersOfMass.assign(modelObjectDescription.jointDescriptions.size(), phys::Length3D{});
					if (modelObjectDescription.physicsObjectDescriptions.empty()) {
						if (modelDescription.centerOfMassOverride) {
							it->second.jointCentersOfMass.front() = *modelDescription.centerOfMassOverride;
						}
					} else {
						for (const ModelObjectDescription::PhysicsObjectDescription& physicsObjectDescription : modelObjectDescription.physicsObjectDescriptions) {
							it->second.jointCentersOfMass[physicsObjectDescription.jointIndex] = physicsObjectDescription.centerOfMass;
						}
					}
					it->second.orientationAlignment = modelDescription.orientationAlignment;
					it->second.tintColor = modelDescription.tintColor;
					it->second.excludeFromDepthPrepass = modelDescription.excludeFromDepthPrepass;
				} catch (...) {
					info.erase(it);
					throw;
				}
			}
			return static_cast<uint32_t>(it.getIndex());
		}

		[[nodiscard]] uint32_t addInstance(uint32_t modelInfoIndex) {
			ModelAssets::ModelInfo& modelInfo = info.getAtIndex(modelInfoIndex).second;
			if (modelInfo.instanceCount >= size_t{Limits<uint32_t>::MAX}) {
				throw std::length_error{"Maximum model instance count exceeded."};
			}
			const uint32_t instanceIndex = static_cast<uint32_t>(modelInfo.instanceCount);
			++modelInfo.instanceCount;
			return instanceIndex;
		}

		[[nodiscard]] uint32_t getLoadedModelInfoIndex(ModelType modelType) {
			return static_cast<uint32_t>(info.find(modelType).getIndex());
		}

		[[nodiscard]] HashMap<ModelType, ModelInfo>::reference getAtIndex(uint32_t modelInfoIndex) {
			return info.getAtIndex(modelInfoIndex);
		}

		[[nodiscard]] HashMap<ModelType, ModelInfo>::const_reference getAtIndex(uint32_t modelInfoIndex) const {
			return info.getAtIndex(modelInfoIndex);
		}

		[[nodiscard]] const HashMap<ModelType, ModelInfo>& getLoadedModelInfos() const {
			return info;
		}

	private:
		[[nodiscard]] static SharedPointer<gfx::Model3D> createShadowCasterModelFromConvexHull(gfx::Device& device, gfx::Renderer3D& renderer3D, AssetCache& assetCache,
			Schema& schema, ModelType modelType) {
			const ModelDescription& modelDescription = schema.loadModelDescription(assetCache.getFilesystem(), modelType);

			const phys::Shape3D* shape = nullptr;
			if (modelDescription.shapeOverride) {
				shape = &*modelDescription.shapeOverride;
			} else if (schema.findEntityDescription(EntityType{"MODEL_OBJECT"})) {
				const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);
				if (!modelObjectDescription.jointDescriptions.empty() && modelObjectDescription.jointDescriptions.front().physicsObjectIndex) {
					shape = &modelObjectDescription.physicsObjectDescriptions[*modelObjectDescription.jointDescriptions.front().physicsObjectIndex].collider.shape;
				} else {
					shape = &schema.loadModelConvexHullShape(assetCache, modelType);
				}
			}
			if (!shape) {
				return {};
			}
			ConvexPolytope3D* convexPolytope = nullptr;
			if (const phys::LocallyTransformedShape3D* const locallyTransformedShape = shape->get_if<phys::LocallyTransformedShape3D>()) {
				if (const phys::ConvexPolytopeShape3D* const convexPolytopeShape = locallyTransformedShape->shape->get_if<phys::ConvexPolytopeShape3D>()) {
					convexPolytope = convexPolytopeShape->getConvexPolytope().get();
				}
			} else {
				if (const phys::ConvexPolytopeShape3D* const convexPolytopeShape = shape->get_if<phys::ConvexPolytopeShape3D>()) {
					convexPolytope = convexPolytopeShape->getConvexPolytope().get();
				}
			}
			if (!convexPolytope) {
				return {};
			}
			const phys::InverseLocalTransformation3D inverseRootTransformation =
				inverse(translateRotateScale(modelDescription.options.rootTranslation * phys::METERS, modelDescription.options.rootRotation, modelDescription.options.rootScale));
			return SharedPointer<gfx::Model3D>::create(device, renderer3D,
				res::Model{*convexPolytope, inverseRootTransformation * phys::LocalTransformation3D{scale(phys::Scale3D{0.95f})}});
		}

		HashMap<ModelType, ModelInfo> info{};
	};

	struct ModelInstanceReference {
		uint32_t modelInfoIndex;
		uint32_t instanceIndex;
		EntityType entityType;
		EntityID::Flags entityFlags;
		SynchronizedEntityID synchronizedEntityID;
	};

	struct ModelInstances {
		Arena<0> arena{8192};
		ArrayList<ModelInstanceReference> instanceReferences{};
		ArrayList<uint32_t> subtickInstanceIndices{};
		ArrayList<uint32_t> predictionInterpolationInstanceIndices{};
		ArrayList<uint32_t> receivedInterpolationInstanceIndices{};
	};

	enum class EntityRenderCategory : uint8_t {
		SHARED_BETWEEN_LOCAL_PLAYERS,
		ORIENTATION_ALIGNED_PER_LOCAL_PLAYER,
		OTHER_LOCAL_PLAYER_ENTITY,
		CURRENT_LOCAL_PLAYER_ENTITY_FIRST_PERSON_PERSPECTIVE,
		CURRENT_LOCAL_PLAYER_ENTITY_THIRD_PERSON_PERSPECTIVE,
		OTHER_LOCAL_PLAYER_WEAPON_ENTITY,
		CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_FIRST_PERSON_PERSPECTIVE,
		CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_THIRD_PERSON_PERSPECTIVE,
	};

	[[nodiscard]] static EntityRenderCategory determineEntityRenderCategory(const WorldView& worldView, LocalPlayerID localPlayerID, SynchronizedEntityID synchronizedEntityID,
		EntityType entityType, const InterpolatedEntityView& entity, OrientationAlignment orientationAlignment) {
		if (orientationAlignment != OrientationAlignment::NONE) {
			return EntityRenderCategory::ORIENTATION_ALIGNED_PER_LOCAL_PLAYER;
		}
		if (entity.hasComponent<PlayerID>() && entity.hasComponent<LocalPlayerID>() && entity.getNewComponent<PlayerID>() == worldView.playerID) {
			if (entity.getNewComponent<LocalPlayerID>() == localPlayerID) {
				const EntityID subtickEntityID =
					worldView.subtickResources.getResource<SynchronizedEntityMap>().findEntity(worldView.subtickRegistry, synchronizedEntityID, entityType);
				if (const LocalPlayerPerspective* const localPlayerPerspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(subtickEntityID)) {
					if (localPlayerPerspective->aimPosition == localPlayerPerspective->viewPosition) {
						return EntityRenderCategory::CURRENT_LOCAL_PLAYER_ENTITY_FIRST_PERSON_PERSPECTIVE;
					}
				}
				return EntityRenderCategory::CURRENT_LOCAL_PLAYER_ENTITY_THIRD_PERSON_PERSPECTIVE;
			}
			return EntityRenderCategory::OTHER_LOCAL_PLAYER_ENTITY;
		}
		if (entity.hasComponent<WeaponState>()) {
			const SynchronizedEntityID holderSynchronizedEntityID = entity.getNewAttribute<&WeaponState::holder>();
			const Optional<InterpolatedEntityView> holderEntity = worldView.findEntity(entity.entityIDs.second.getFlags(), holderSynchronizedEntityID);
			if (holderEntity && holderEntity->hasComponent<PlayerID>() && holderEntity->hasComponent<LocalPlayerID>() &&
				holderEntity->getNewComponent<PlayerID>() == worldView.playerID) {
				if (holderEntity->getNewComponent<LocalPlayerID>() == localPlayerID) {
					const EntityID subtickHolderEntityID = worldView.subtickResources.getResource<SynchronizedEntityMap>().findEntity(worldView.subtickRegistry,
						holderSynchronizedEntityID, holderEntity->getNewComponent<EntityType>());
					if (const LocalPlayerPerspective* const localPlayerPerspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(subtickHolderEntityID)) {
						if (localPlayerPerspective->aimPosition == localPlayerPerspective->viewPosition) {
							return EntityRenderCategory::CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_FIRST_PERSON_PERSPECTIVE;
						}
					}
					return EntityRenderCategory::CURRENT_LOCAL_PLAYER_WEAPON_ENTITY_THIRD_PERSON_PERSPECTIVE;
				}
				return EntityRenderCategory::OTHER_LOCAL_PLAYER_WEAPON_ENTITY;
			}
		}
		return EntityRenderCategory::SHARED_BETWEEN_LOCAL_PLAYERS;
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createModelGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new ModelGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
