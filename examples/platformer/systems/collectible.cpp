// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "collectible.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Audio.hpp"
#include "../Graphics.hpp"
#include "../System.hpp"
#include "../collision_layers.hpp"
#include "../shaders.hpp"
#include "player.hpp"

#include <utility> // std::move

namespace {

constexpr phys::CollisionFilter COLLECTIBLE_COLLISION_FILTER{
	.layers = COLLISION_LAYER_COLLECTIBLE,
	.detectionLayers = COLLISION_LAYER_PLAYER,
	.responseLayers{},
};

struct CollectibleSettingsJSON {
	static constexpr CStringView FILEPATH = "schema/collectible.json";

	struct Model {
		String filepath{};
		vec3 rootTranslation{};
		quat rootRotation{0.0f, 0.0f, 0.0f, 1.0f};
		vec3 rootScale{1.0f};
	} model{};

	struct Collection {
		phys::Distance radius = 1.5_meters;
		phys::Distance targetRadius = 0.5_meters;
		phys::Speed speed = 5_meters_per_second;
	} collection{};

	struct Effect {
		phys::Time duration = 200_milliseconds;
		phys::Distance size = 1_meter;

		struct Sprite {
			String filepath{};
			size_t frameCount = 1;
			vec2 frameSize{1.0f};
			vec4 color{1.0f};
		} sprite{};

		struct Sound {
			String filepath{};
			float volume = 1.0f;
		} sound{};
	} effect{};
};

// Resources:

struct CollectibleSettings : CollectibleSettingsJSON {
	explicit CollectibleSettings(CollectibleSettingsJSON&& settingsJSON)
		: CollectibleSettingsJSON(std::move(settingsJSON)) {}
};

struct CollectibleModel {
	gfx::Model3D model;
};

struct CollectibleEffect {
	ArrayList<gfx::SpriteID> spriteFrames;
	aud::Sound collectSound;
};

struct CollectibleExpiredEntities : ArrayList<phys::EntityID> {};

struct CollectibleEffectsToSpawn : ArrayList<phys::Position3D> {};

// Components:

struct Collectible {
	Duration timeOffset;
};

struct CollectibleBeingCollected {
	phys::EntityID targetEntityID;
	phys::Position3D position;
	phys::Position3D previousPosition = position;
};

struct CollectibleEffectInstance {
	phys::Position3D position;
	phys::Time time{};
	phys::Time previousTime = time;
};

struct CollectibleModelInstance {
	res::Model::Transformation transformation{};
	res::Model::TransformationView transformationView{};
	gfx::ModelInstance3D instance{};
	bool visible = false;
};

// Functions:

void poseCollectible(CollectibleModelInstance& modelInstance, phys::Position3D position, phys::EntityID entityID, Duration totalElapsedTime, const WorldView& worldView,
	const Collectible& collectible, const CollectibleModel& model) {
	const float time = duration_cast<FloatSeconds>(totalElapsedTime + collectible.timeOffset).count();
	const phys::Position3D displayPosition = position + phys::Y_AXIS_3D * sin(time * phys::PI) * 0.2_meters;
	const phys::Orientation3D displayOrientation = phys::Orientation3D::yaw(time) * phys::Orientation3D::pitch(time * 2_x) * phys::Orientation3D::yaw(time * 2_x);
	modelInstance.transformation.assign(translateRotate(displayPosition, displayOrientation), model.model.getBindPose().localJoints,
		model.model.getBindPose().localMorphTargetWeights, model.model.getJointParentIndices());
	modelInstance.transformationView = modelInstance.transformation;
	modelInstance.instance = {.instanceIdentifier = entityID.getIndex()};
	modelInstance.visible = worldView.isPotentiallyVisible(phys::Sphere3D{
		.center = displayPosition,
		.radius = model.model.getBindPoseBoundingRadius() * phys::METERS,
	});
}

// Systems:

void collectCollectibles(phys::EntityRegistry3D& registry, Audio& audio, CollectibleExpiredEntities& expiredEntities, CollectibleEffectsToSpawn& effectsToSpawn,
	const CollectibleSettings& settings, const CollectibleEffect& effect, const phys::CollisionEvents3D& collisionEvents, const phys::SimulationOptions3D& simulationOptions,
	const phys::ResourceRegistry3D& resources) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	expiredEntities.clear();
	for (auto&& [entityID, effectInstance] : registry.getEntities<CollectibleEffectInstance>()) {
		effectInstance.previousTime = effectInstance.time;
		if (countup(effectInstance.time, deltaTime, settings.effect.duration)) {
			expiredEntities.push_back(entityID);
		}
	}
	for (const phys::EntityID entityID : expiredEntities) {
		registry.destroyEntity(entityID);
	}

	for (const phys::CollisionEvent3D& collisionEvent : collisionEvents) {
		phys::EntityID collectibleID{};
		phys::EntityID collectorID{};
		if (registry.hasComponent<Collectible>(collisionEvent.objectIDs.first)) {
			if (isPlayer(registry, collisionEvent.objectIDs.second)) {
				collectibleID = collisionEvent.objectIDs.first;
				collectorID = collisionEvent.objectIDs.second;
			}
		} else if (registry.hasComponent<Collectible>(collisionEvent.objectIDs.second)) {
			if (isPlayer(registry, collisionEvent.objectIDs.first)) {
				collectibleID = collisionEvent.objectIDs.second;
				collectorID = collisionEvent.objectIDs.first;
			}
		}

		if (collectibleID && collectorID) {
			if (registry.addComponentIfMissing<CollectibleBeingCollected>(collectibleID,
					CollectibleBeingCollected{
						.targetEntityID = collectorID,
						.position = registry.getComponent<phys::Position3D>(collectibleID),
					})) {
				phys::Simulation3D::removeObjectComponents(registry, collectibleID);
			}
		}
	}

	expiredEntities.clear();
	effectsToSpawn.clear();
	for (auto&& [entityID, beingCollected, collectible] : registry.getEntities<CollectibleBeingCollected, const Collectible>()) {
		beingCollected.previousPosition = beingCollected.position;

		if (const phys::Position3D* const targetPosition = registry.findComponent<phys::Position3D>(beingCollected.targetEntityID)) {
			const phys::LinearVelocity3D targetVelocity = registry.getComponentOr<phys::LinearVelocity3D>(beingCollected.targetEntityID, phys::LinearVelocity3D{});
			beingCollected.position = moveTowards(beingCollected.position, *targetPosition, max(settings.collection.speed, length(targetVelocity) * 1.1_x) * deltaTime);
			if (distance2(beingCollected.position, *targetPosition) < length2(settings.collection.targetRadius)) {
				if (isPlayer(registry, beingCollected.targetEntityID)) {
					givePlayerCollectible(registry, resources, beingCollected.targetEntityID);
				}

				const float soundPlaybackSpeed = 0.9f + 0.2f * duration_cast<FloatSeconds>(collectible.timeOffset) / duration_cast<FloatSeconds>(Milliseconds{65536});
				const aud::SoundInstanceID soundInstanceID =
					audio.soundStage.createPaused3DSound(effect.collectSound, beingCollected.position.in(phys::METERS), targetVelocity.in(phys::METERS_PER_SECOND));
				audio.soundStage.setSoundPlaybackSpeed(soundInstanceID, soundPlaybackSpeed);
				audio.soundStage.resumeSound(soundInstanceID);

				effectsToSpawn.push_back(beingCollected.position);

				expiredEntities.push_back(entityID);
			}
		} else {
			expiredEntities.push_back(entityID);
		}
	}
	for (const phys::EntityID entityID : expiredEntities) {
		registry.destroyEntity(entityID);
	}

	for (const phys::Position3D position : effectsToSpawn) {
		phys::EntityBuilder3D entity = registry.createEntity();
		entity.addComponent<CollectibleEffectInstance>(CollectibleEffectInstance{.position = position});
		entity.build();
	}
}

void poseCollectibles(exec::Entities<CollectibleModelInstance, const Collectible, const phys::Position3D, exec::Exclude<CollectibleBeingCollected>> collectibleEntities,
	const app::FrameInfo& frameInfo, const WorldView& worldView, const CollectibleModel& model) {
	for (auto&& [entityID, modelInstance, collectible, position] : collectibleEntities) {
		poseCollectible(modelInstance, position, entityID, frameInfo.totalElapsedTime, worldView, collectible, model);
	}
}

void poseCollectiblesBeingCollected(exec::Entities<CollectibleModelInstance, const Collectible, const CollectibleBeingCollected> collectibleEntities,
	const app::FrameInfo& frameInfo, const WorldView& worldView, const CollectibleModel& model) {
	for (auto&& [entityID, modelInstance, collectible, beingCollected] : collectibleEntities) {
		const phys::Position3D position = mix(beingCollected.previousPosition, beingCollected.position, frameInfo.tickInterpolationAlpha);
		poseCollectible(modelInstance, position, entityID, frameInfo.totalElapsedTime, worldView, collectible, model);
	}
}

struct CollectibleSystem final : System {
	void addRequiredResources(phys::ResourceRegistry3D& resources, Graphics& graphics, Audio&, const Filesystem& filesystem) override {
		CollectibleSettingsJSON settingsJSON{};
		try {
			json::deserializeFromString(filesystem.readInputFileString(CollectibleSettingsJSON::FILEPATH), settingsJSON);
		} catch (...) {
			Error::throwWithNestedFilepath(CollectibleSettingsJSON::FILEPATH);
		}
		const CollectibleSettings& settings = resources.addResource<CollectibleSettings>(std::move(settingsJSON));
		resources.addResource<CollectibleModel>(CollectibleModel{
			.model{
				graphics.device,
				graphics.renderer3D,
				res::Model{
					filesystem,
					settings.model.filepath,
					res::ModelOptions{
						.rootTranslation = settings.model.rootTranslation,
						.rootRotation = settings.model.rootRotation,
						.rootScale = settings.model.rootScale,
					},
				},
			},
		});
		resources.addResource<CollectibleEffect>(CollectibleEffect{
			.spriteFrames = graphics.loadSpriteAnimation(filesystem, settings.effect.sprite.filepath, settings.effect.sprite.frameCount, settings.effect.sprite.frameSize),
			.collectSound{filesystem, settings.effect.sound.filepath, aud::SoundOptions{.volume = settings.effect.sound.volume, .rolloffFactor = 0.05f}},
		});
		resources.addResource<CollectibleExpiredEntities>();
		resources.addResource<CollectibleEffectsToSpawn>();
	}

	void removeResources(phys::ResourceRegistry3D& resources) noexcept override {
		resources.removeResource<CollectibleSettings>();
		resources.removeResource<CollectibleModel>();
		resources.removeResource<CollectibleEffect>();
		resources.removeResource<CollectibleExpiredEntities>();
		resources.removeResource<CollectibleEffectsToSpawn>();
	}

	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<collectCollectibles>("Collect collectibles");
	}

	void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount parallelism) override {
		scheduler.addParallelTransformationTask<poseCollectibles>(parallelism, "Pose collectibles");
		scheduler.addTask<poseCollectiblesBeingCollected>("Pose collectibles being collected");
	}

	void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) override {
		GREM_PROFILE_FUNCTION();

		const app::FrameInfo& frameInfo = simulation.resources.getResource<app::FrameInfo>();
		const WorldView& worldView = simulation.resources.getResource<WorldView>();
		const CollectibleSettings& settings = simulation.resources.getResource<CollectibleSettings>();
		const CollectibleModel& model = simulation.resources.getResource<CollectibleModel>();
		const CollectibleEffect& effect = simulation.resources.getResource<CollectibleEffect>();

		const exec::Entities<const CollectibleModelInstance> collectibleEntities = simulation.registry;
		graphics.instances3D.putVisibleShadedModelInstances(graphics.modelShaderPipelineSet, model.model,
			StridedSpan{Span{collectibleEntities}, &CollectibleModelInstance::transformationView}, StridedSpan{Span{collectibleEntities}, &CollectibleModelInstance::instance},
			StridedSpan{Span{collectibleEntities}, &CollectibleModelInstance::visible});

		for (const auto& [entityID, modelInstance] : collectibleEntities) {
			const phys::Sphere3D boundingSphere{
				.center = vec3{modelInstance.transformation.jointMatrices[0][3]} * phys::METERS,
				.radius = model.model.getBindPoseBoundingRadius() * phys::METERS,
			};
			const phys::Position3D blobShadowPosition{boundingSphere.center.getX(), boundingSphere.center.getY() - boundingSphere.radius, boundingSphere.center.getZ()};
			const phys::Distance blobShadowRadius = boundingSphere.radius * 0.8_x;
			const PlatformerModelShaderBlobShadow blobShadow{
				.blobShadowPosition = blobShadowPosition.in(phys::METERS),
				.blobShadowRadius = blobShadowRadius.in(phys::METERS),
				.blobShadowInstanceIdentifier = entityID.getIndex(),
			};
			if (worldView.isPotentiallyVisible(blobShadow.getBoundingBox(worldView.visibleBounds) * phys::METERS)) {
				graphics.blobShadows.push_back(blobShadow);
			}
		}

		const size_t frameCount = effect.spriteFrames.size();
		for (const auto& [entityID, effectInstance] : simulation.registry.getEntities<const CollectibleEffectInstance>()) {
			if (worldView.isPotentiallyVisible(phys::Sphere3D{.center = effectInstance.position, .radius = settings.effect.size * numbers::SQRT2})) {
				const phys::Time time = mix(effectInstance.previousTime, effectInstance.time, frameInfo.tickInterpolationAlpha);
				const size_t frameIndex = min(static_cast<size_t>(time * (static_cast<float>(frameCount) / settings.effect.duration)), frameCount - 1);
				const gfx::SpriteID spriteID = effect.spriteFrames[frameIndex];
				graphics.instances3D.putSpriteInstance(graphics.spriteAtlas, spriteID,
					gfx::SpriteInstance3D{
						.position = effectInstance.position.in(phys::METERS),
						.orientation = convert3x3MatrixToQuaternion(transpose(mat3{graphics.camera3D.getViewMatrix()})),
						.size{settings.effect.size.in(phys::METERS)},
						.origin{0.5f, 0.5f},
						.color = Color::fromLinear(settings.effect.sprite.color),
					});
			}
		}
	}
} collectibleSystemImplementation{};

} // namespace

System* const collectibleSystem = &collectibleSystemImplementation;

void createCollectible(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, phys::Position3D position) {
	GREM_PROFILE_FUNCTION();

	const CollectibleSettings& settings = resources.getResource<CollectibleSettings>();
	phys::Simulation3D::addObjectComponents(entity.getRegistry(), resources, entity.getEntityID(),
		phys::ObjectOptions3D{
			.position = position,
			.orientation{},
			.gravityAcceleration{},
			.mass = phys::Mass::INF,
			.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia3D::INF,
			.collider{
				.shape = phys::SphereShape3D{.radius = settings.collection.radius},
				.filter = COLLECTIBLE_COLLISION_FILTER,
			},
			.emitsCollisionEvents = true,
		});
	const vec3 positionInMeters = position.in(phys::METERS);
	rng::Xoroshiro128PlusPlusEngine::result_type seed{};
	seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(positionInMeters.x, 65536.0f));
	seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(positionInMeters.y, 65536.0f));
	seed ^= static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(wrap(positionInMeters.z, 65536.0f));
	rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
	rng::UniformIntegerDistribution<Milliseconds::rep> timeOffsetDistribution{0, 65536};
	entity.addComponent<Collectible>(Collectible{
		.timeOffset = Milliseconds{timeOffsetDistribution(numberGenerator)},
	});
	entity.addComponent<CollectibleModelInstance>();
}
