// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "level.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Graphics.hpp"
#include "../System.hpp"
#include "../WorldView.hpp"
#include "../collision_layers.hpp"
#include "../shaders.hpp"
#include "collectible.hpp"
#include "object.hpp"
#include "player.hpp"

#include <memory>  // std::uninitialized_...
#include <new>     // std::launder
#include <utility> // std::move

namespace {

// Resources:

struct LevelObjectsOutOfBounds : ArrayList<phys::EntityID> {};

struct Level {
	struct Model {
		struct PropInstance {
			phys::Box3D boundingBox;
			res::Model::TransformationView transformation;
			bool visible = false;
		};

		gfx::Model3D model{};
		phys::Shape3D shape{};
		ArrayList<PropInstance> propInstances{};
	};

	Arena<0> arena{};
	phys::Box3D bounds;
	gfx::Model3D geometryModel;
	gfx::Sky3D sky;
	Pair<phys::Position3D, phys::PitchYaw> playerSpawnPositionAndAngles;
	size_t collectibleCount;
	ArrayList<PlatformerModelShaderLight> lights;
	HashMap<String, Indirect<Model>> models;
	ArrayList<phys::EntityID> entityIDs;

	[[nodiscard]] Model& loadModel(gfx::Device& device, gfx::Renderer3D& renderer3D, const Filesystem& filesystem, const String& filepath) {
		GREM_PROFILE_FUNCTION();

		const auto [it, inserted] = models.try_emplace(filepath);
		try {
			if (inserted) {
				res::Model model{filesystem, filepath};

				res::Model::Transformation bindPoseTransformation{};
				bindPoseTransformation.assign(mat4{1.0f}, model.bindPose.localJoints, model.bindPose.localMorphTargetWeights, model.jointParentIndices);

				Buffer<ConvexPolytopeVertex3D> vertices{};
				for (const res::Model::Instance& instance : model.instances) {
					const res::Model::Mesh& mesh = model.meshes.at(instance.meshIndex);
					const mat4 jointMatrix = bindPoseTransformation.jointMatrices[((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : instance.jointIndex];
					const Span<const vec3> meshPositions{
						std::launder(reinterpret_cast<const vec3*>(
							model.meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4 + static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t))),
						static_cast<size_t>(mesh.vertexCount)};
					vertices.reserve(vertices.size() + meshPositions.size());
					for (const vec3 position : meshPositions) {
						vertices.push_back(ConvexPolytopeVertex3D{vec3{jointMatrix * vec4{position, 1.0f}}});
					}
				}
				it->second->shape = phys::ConvexPolytopeShape3D{vertices, 32};

				it->second->model = gfx::Model3D{device, renderer3D, std::move(model)};
			}
			return *it->second;
		} catch (...) {
			models.erase(it);
			throw;
		}
	}
};

// Components:

struct LevelProp {
	Level::Model* model;
	size_t instanceIndex;
};

// Systems:

void enforceLevelBounds(phys::EntityRegistry3D& registry, LevelObjectsOutOfBounds& objectsOutOfBounds, const Level& level, const phys::ResourceRegistry3D& resources) {
	objectsOutOfBounds.clear();
	for (auto&& [entityID, position, previousPosition] : registry.getEntities<const phys::Position3D, const ObjectPreviousPosition>()) {
		if (!level.bounds.contains(position)) {
			objectsOutOfBounds.push_back(entityID);
		}
	}

	for (const phys::EntityID entityID : objectsOutOfBounds) {
		if (isPlayer(registry, entityID)) {
			teleportPlayer(registry, resources, entityID, level.playerSpawnPositionAndAngles.first, level.playerSpawnPositionAndAngles.second, phys::LinearVelocity3D{});
		} else {
			registry.destroyEntity(entityID);
		}
	}
}

void cullLevelProps(exec::Entities<LevelProp> propEntities, const WorldView& worldView) {
	for (auto&& [entityID, prop] : propEntities) {
		Level::Model::PropInstance& propInstance = prop.model->propInstances[prop.instanceIndex];
		propInstance.visible = worldView.isPotentiallyVisible(propInstance.boundingBox);
	}
}

struct LevelSystem final : System {
	void addRequiredResources(phys::ResourceRegistry3D& resources, Graphics&, Audio&, const Filesystem&) override {
		resources.addResource<LevelObjectsOutOfBounds>();
	}

	void removeResources(phys::ResourceRegistry3D& resources) noexcept override {
		resources.removeResource<LevelObjectsOutOfBounds>();
	}

	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<enforceLevelBounds>("Enforce level bounds");
	}

	void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount parallelism) override {
		scheduler.addParallelTransformationTask<cullLevelProps>(parallelism, "Cull level props");
	}

	void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) override {
		GREM_PROFILE_FUNCTION();

		const Level& level = simulation.resources.getResource<Level>();
		graphics.lights.append_range(level.lights);
		graphics.instances3D.putShadedModelInstance(graphics.modelShaderPipelineSet, level.geometryModel, mat4{1.0f}, {.instanceIdentifier = phys::EntityID::INVALID_INDEX});
		for (const auto& [filepath, model] : level.models) {
			graphics.instances3D.putVisibleShadedModelInstances(graphics.modelShaderPipelineSet, model->model,
				StridedSpan{model->propInstances, &Level::Model::PropInstance::transformation}, StridedSpan{model->propInstances, &Level::Model::PropInstance::visible});
		}
	}
} levelSystemImplementation{};

} // namespace

System* const levelSystem = &levelSystemImplementation;

void loadLevel(phys::EntityRegistry3D& registry, phys::ResourceRegistry3D& resources, gfx::Device& device, gfx::Renderer3D& renderer3D, const Filesystem& filesystem,
	CStringView filepath) {
	GREM_PROFILE_FUNCTION();

	struct LevelJSON {
		struct DirectionalLight {
			phys::PitchYaw angles{};
			vec3 intensity{1.0f};
		};

		struct PointLight {
			phys::Position3D position{};
			vec3 intensity{1.0f};
		};

		struct SpotLight {
			phys::Position3D position{};
			phys::PitchYaw angles{};
			phys::Angle innerConeAngle{};
			phys::Angle outerConeAngle{};
			vec3 intensity{1.0f};
		};

		struct PlayerSpawn {
			phys::Position3D position{};
			phys::PitchYaw angles{};
		};

		struct Prop {
			phys::Position3D position{};
			quat orientation{};
			phys::Scale3D scale{1.0f};
			String model{};
			phys::Coefficient staticFriction = 0.8_x;
			phys::Coefficient kineticFriction = 0.707_x;
			phys::Coefficient rollingResistance = 0.01_x;
			phys::Coefficient restitution = 0.5_x;
			phys::Material::FrictionCombine frictionCombine = phys::Material::FrictionCombine::MINIMUM;
			phys::Material::RestitutionCombine restitutionCombine = phys::Material::RestitutionCombine::MAXIMUM;
		};

		struct PhysicsObject {
			phys::Position3D position{};
			quat orientation{};
			phys::Scale3D scale{1.0f};
			String model{};
			phys::LinearVelocity3D linearVelocity{};
			phys::AngularVelocity3D angularVelocity{};
			phys::LinearAcceleration3D gravityAcceleration = -phys::Y_AXIS_3D * 9.82_meters_per_second_squared;
			phys::Density surroundingFluidDensity = 1.2041_kilograms_per_cubic_meter;
			phys::Length3D centerOfBuoyancy{};
			phys::Mass mass{};
			phys::Length3D centerOfMass{};
			phys::PrincipalMomentsOfInertia3D principalMomentsOfInertia{};
			phys::LocalInertiaOrientation3D localInertiaOrientation{};
			phys::Coefficient staticFriction = 0.8_x;
			phys::Coefficient kineticFriction = 0.707_x;
			phys::Coefficient rollingResistance = 0.01_x;
			phys::Coefficient restitution = 0.5_x;
			phys::Coefficient linearDrag = 0.5_x;
			phys::Coefficient angularDrag = 0.5_x;
			phys::Material::FrictionCombine frictionCombine = phys::Material::FrictionCombine::MINIMUM;
			phys::Material::RestitutionCombine restitutionCombine = phys::Material::RestitutionCombine::MAXIMUM;
		};

		phys::Box3D bounds{.min{-1000_meters}, .max{1000_meters}};
		String geometry{};
		String skyImage{};
		vec3 ambientLight{};
		ArrayList<DirectionalLight> directionalLights{};
		ArrayList<PointLight> pointLights{};
		ArrayList<SpotLight> spotLights{};
		PlayerSpawn playerSpawn{};
		ArrayList<Prop> props{};
		ArrayList<PhysicsObject> physicsObjects{};
		ArrayList<phys::Position3D> collectibles{};
	} levelJSON{};
	try {
		json::deserializeFromString(filesystem.readInputFileString(filepath), levelJSON);
	} catch (...) {
		Error::throwWithNestedFilepath(filepath);
	}

	const StringView filepathPrefix = filepath.substr(0, filepath.find_last_of("/\\") + 1);
	const Allocation<byte> levelGeometryGlbData = filesystem.readInputFile(levelJSON.geometry);
	const gltf::Asset levelGeometryGlTFAsset = gltf::Asset::parseBinary(levelGeometryGlbData, levelJSON.geometry);
	const res::Model levelGeometryModel{
		levelGeometryGlTFAsset,
		[&](CStringView relativeFilepath, const res::ImageOptions& options) -> res::Image {
			const String fullFilepath = String{filepathPrefix} + relativeFilepath.c_str();
			if (filesystem.inputFileExists(fullFilepath)) {
				return res::Image{filesystem, fullFilepath, options};
			}
			return res::Image{filesystem, relativeFilepath, options};
		},
		[&](CStringView relativeFilepath) -> Variant<Allocation<byte>, Span<const byte>> {
			const String fullFilepath = String{filepathPrefix} + relativeFilepath.c_str();
			if (filesystem.inputFileExists(fullFilepath)) {
				return filesystem.readInputFile(fullFilepath);
			}
			return filesystem.readInputFile(relativeFilepath);
		},
		res::ModelOptions{
			.excludeAnimations = true,
			.excludeLights = true,
			.excludeColliders = true,
			.excludePhysics = true,
		},
	};

	Level level{
		.bounds = levelJSON.bounds,
		.geometryModel{device, renderer3D, levelGeometryModel},
		.sky{device, gfx::Sky3DOptions{.ambientColor = Color::fromLinear(levelJSON.ambientLight)}},
		.playerSpawnPositionAndAngles{levelJSON.playerSpawn.position, levelJSON.playerSpawn.angles},
		.collectibleCount = levelJSON.collectibles.size(),
		.lights{},
		.models{},
		.entityIDs{},
	};

	if (!levelJSON.skyImage.empty()) {
		gfx::LightBaker3D{device, renderer3D}.bakeSkybox(level.sky, res::Image{filesystem, levelJSON.skyImage},
			gfx::Sky3DOptions{.ambientColor = Color::fromLinear(levelJSON.ambientLight)});
	}

	const auto getEstimatedLightRange = [](vec3 intensity) -> float {
		return 5.29538f * pow(maxComponent(intensity), 0.383819f);
	};
	for (const LevelJSON::DirectionalLight& directionalLight : levelJSON.directionalLights) {
		level.lights.push_back(PlatformerModelShaderLight{
			.lightTypeAndRangeAndConeCosines{
				PlatformerModelShaderLight::LIGHT_TYPE_DIRECTIONAL,
				1.0f,
				1.0f,
				0.0f,
			},
			.lightPosition{},
			.lightDirection{convertAnglesToForwardDirection(directionalLight.angles), 0.0f},
			.lightIntensity{directionalLight.intensity, 0.0f},
		});
	}
	for (const LevelJSON::PointLight& pointLight : levelJSON.pointLights) {
		level.lights.push_back(PlatformerModelShaderLight{
			.lightTypeAndRangeAndConeCosines{
				PlatformerModelShaderLight::LIGHT_TYPE_POINT,
				getEstimatedLightRange(pointLight.intensity),
				1.0f,
				0.0f,
			},
			.lightPosition{pointLight.position.in(phys::METERS), 0.0f},
			.lightDirection{},
			.lightIntensity{pointLight.intensity, 0.0f},
		});
	}
	for (const LevelJSON::SpotLight& spotLight : levelJSON.spotLights) {
		level.lights.push_back(PlatformerModelShaderLight{
			.lightTypeAndRangeAndConeCosines{
				PlatformerModelShaderLight::LIGHT_TYPE_SPOT,
				getEstimatedLightRange(spotLight.intensity),
				cos(spotLight.innerConeAngle),
				cos(spotLight.outerConeAngle),
			},
			.lightPosition{spotLight.position.in(phys::METERS), 0.0f},
			.lightDirection{convertAnglesToForwardDirection(spotLight.angles), 0.0f},
			.lightIntensity{spotLight.intensity, 0.0f},
		});
	}

	struct LevelGeometryMaterial {
		phys::Coefficient staticFriction = 0.8_x;
		phys::Coefficient kineticFriction = 0.707_x;
		phys::Coefficient rollingResistance = 0.01_x;
		phys::Coefficient restitution = 0.5_x;
		phys::Material::FrictionCombine frictionCombine = phys::Material::FrictionCombine::MINIMUM;
		phys::Material::RestitutionCombine restitutionCombine = phys::Material::RestitutionCombine::MAXIMUM;
	};
	HashMap<res::Model::JointIndex, LevelGeometryMaterial> levelGeometryMaterials{};
	for (const gltf::Node& node : levelGeometryGlTFAsset.nodes) {
		if (!node.name.empty()) {
			LevelGeometryMaterial material{};
			if (const json::Value* const staticFriction = node.extras.findProperty("staticFriction")) {
				material.staticFriction = staticFriction->getNumber<float>();
			}
			if (const json::Value* const kineticFriction = node.extras.findProperty("kineticFriction")) {
				material.kineticFriction = kineticFriction->getNumber<float>();
			}
			if (const json::Value* const rollingResistance = node.extras.findProperty("rollingResistance")) {
				material.rollingResistance = rollingResistance->getNumber<float>();
			}
			if (const json::Value* const restitution = node.extras.findProperty("restitution")) {
				material.restitution = restitution->getNumber<float>();
			}
			if (const json::Value* const frictionCombine = node.extras.findProperty("frictionCombine")) {
				const json::String& name = frictionCombine->getString();
				if (name == "AVERAGE") {
					material.frictionCombine = phys::Material::FrictionCombine::AVERAGE;
				} else if (name == "MINIMUM") {
					material.frictionCombine = phys::Material::FrictionCombine::MINIMUM;
				} else if (name == "MAXIMUM") {
					material.frictionCombine = phys::Material::FrictionCombine::MAXIMUM;
				} else if (name == "MULTIPLY") {
					material.frictionCombine = phys::Material::FrictionCombine::MULTIPLY;
				} else {
					throw json::Error{"Expected \"AVERAGE\", \"MINIMUM\", \"MAXIMUM\" or \"MULTIPLY\".", frictionCombine->getSource()};
				}
			}
			if (const json::Value* const restitutionCombine = node.extras.findProperty("restitutionCombine")) {
				const json::String& name = restitutionCombine->getString();
				if (name == "AVERAGE") {
					material.restitutionCombine = phys::Material::RestitutionCombine::AVERAGE;
				} else if (name == "MINIMUM") {
					material.restitutionCombine = phys::Material::RestitutionCombine::MINIMUM;
				} else if (name == "MAXIMUM") {
					material.restitutionCombine = phys::Material::RestitutionCombine::MAXIMUM;
				} else if (name == "MULTIPLY") {
					material.restitutionCombine = phys::Material::RestitutionCombine::MULTIPLY;
				} else {
					throw json::Error{"Expected \"AVERAGE\", \"MINIMUM\", \"MAXIMUM\" or \"MULTIPLY\".", restitutionCombine->getSource()};
				}
			}
			if (const auto it = levelGeometryModel.jointMap.find(node.name); it != levelGeometryModel.jointMap.end()) {
				levelGeometryMaterials.emplace(it->second, material);
			}
		}
	}

	ArrayList<phys::EntityBuilder3D> entities{};
	{
		res::Model::Transformation levelGeometryBindPoseTransformation{};
		levelGeometryBindPoseTransformation.assign(mat4{1.0f}, levelGeometryModel.bindPose.localJoints, levelGeometryModel.bindPose.localMorphTargetWeights,
			levelGeometryModel.jointParentIndices);

		Buffer<TriangleMeshVertexIndex> temporaryIndices{};
		for (const res::Model::Instance& instance : levelGeometryModel.instances) {
			const res::Model::Mesh& mesh = levelGeometryModel.meshes.at(instance.meshIndex);
			if ((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) {
				continue;
			}

			const mat4 jointMatrix = levelGeometryBindPoseTransformation.jointMatrices[instance.jointIndex];
			const auto [translation, rotation, scale] = decomposeTranslationRotationScale(jointMatrix);

			const Span<const vec3> meshPositions{
				std::launder(reinterpret_cast<const vec3*>(
					levelGeometryModel.meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4 + static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t))),
				static_cast<size_t>(mesh.vertexCount)};

			Span<const uint32_t> meshIndices{std::launder(reinterpret_cast<const uint32_t*>(levelGeometryModel.meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4)),
				static_cast<size_t>(mesh.indexCount)};
			if (mesh.indexCount == 0) {
				temporaryIndices.resize(meshPositions.size());
				iota(Span{temporaryIndices}.last(meshPositions.size()), TriangleMeshVertexIndex{0});
				meshIndices = temporaryIndices;
			}

			LevelGeometryMaterial material{};
			if (const auto it = levelGeometryMaterials.find(instance.jointIndex); it != levelGeometryMaterials.end()) {
				material = it->second;
			}

			const phys::EntityBuilder3D& entity = entities.emplace_back(registry.createEntity());
			phys::Simulation3D::addObjectComponents(registry, resources, entity.getEntityID(),
				phys::ObjectOptions3D{
					.position = translation * phys::METERS,
					.orientation = rotation,
					.scale = scale,
					.gravityAcceleration{},
					.mass = phys::Mass::INF,
					.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia3D::INF,
					.collider{.shape = phys::TriangleMeshShape3D{meshPositions, meshIndices}, .filter{.layers = COLLISION_LAYER_LEVEL_GEOMETRY}},
					.material{
						.staticFriction = material.staticFriction,
						.kineticFriction = material.kineticFriction,
						.rollingResistance = material.rollingResistance,
						.restitution = material.restitution,
						.frictionCombine = material.frictionCombine,
						.restitutionCombine = material.restitutionCombine,
					},
					.energyLevel = 0,
				});
		}
	}

	for (const LevelJSON::Prop& propJSON : levelJSON.props) {
		Level::Model& model = level.loadModel(device, renderer3D, filesystem, propJSON.model);

		const size_t jointCount = model.model.getJointCount();
		const size_t morphTargetWeightCount = model.model.getMorphTargetWeightCount();

		mat4* const jointMatrices = ArenaAllocator<mat4>{&level.arena}.allocate(jointCount);
		std::uninitialized_default_construct_n(jointMatrices, jointCount);
		jointMatrices[0] = translateRotateScale(propJSON.position, propJSON.orientation, propJSON.scale);

		bool* const jointsVisible = ArenaAllocator<bool>{&level.arena}.allocate(jointCount);
		std::uninitialized_default_construct_n(jointsVisible, jointCount);
		jointsVisible[0] = true;

		float* const morphTargetWeights = ArenaAllocator<float>{&level.arena}.allocate(morphTargetWeightCount);
		std::uninitialized_fill_n(morphTargetWeights, morphTargetWeightCount, 0.0f);

		const res::Model::TransformationReference transformation{.jointMatrices = jointMatrices, .jointsVisible = jointsVisible, .morphTargetWeights = morphTargetWeights};
		transformation.pose(model.model.getBindPose().localJoints, model.model.getBindPose().localMorphTargetWeights, model.model.getJointParentIndices());

		phys::EntityBuilder3D& entity = entities.emplace_back(registry.createEntity());
		phys::Simulation3D::addObjectComponents(registry, resources, entity.getEntityID(),
			phys::ObjectOptions3D{
				.position = propJSON.position,
				.orientation = propJSON.orientation,
				.scale = propJSON.scale,
				.gravityAcceleration{},
				.mass = phys::Mass::INF,
				.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia3D::INF,
				.collider{.shape = model.shape, .filter{.layers = COLLISION_LAYER_PROP}},
				.material{
					.staticFriction = propJSON.staticFriction,
					.kineticFriction = propJSON.kineticFriction,
					.rollingResistance = propJSON.rollingResistance,
					.restitution = propJSON.restitution,
					.frictionCombine = propJSON.frictionCombine,
					.restitutionCombine = propJSON.restitutionCombine,
				},
				.energyLevel = 0,
			});
		entity.addComponent<LevelProp>(LevelProp{.model = &model, .instanceIndex = model.propInstances.size()});
		model.propInstances.push_back(Level::Model::PropInstance{
			.boundingBox = Graphics::getBindPoseBoundingBox(model.model, jointMatrices[0]),
			.transformation = transformation,
		});
	}

	for (const LevelJSON::PhysicsObject& physicsObject : levelJSON.physicsObjects) {
		const auto& model = level.loadModel(device, renderer3D, filesystem, physicsObject.model);
		phys::Shape3D shape = model.shape;
		if (physicsObject.centerOfMass != 0) {
			shape = phys::LocallyTransformedShape3D{SharedPointer<phys::Shape3D>::create(std::move(shape)), -physicsObject.centerOfMass, {}, phys::Scale3D{1_x}};
		}
		phys::EntityBuilder3D& entity = entities.emplace_back(registry.createEntity());
		createObject(entity, resources, model.model,
			phys::ObjectOptions3D{
				.position = physicsObject.position,
				.orientation = physicsObject.orientation,
				.scale = physicsObject.scale,
				.linearVelocity = physicsObject.linearVelocity,
				.angularVelocity = physicsObject.angularVelocity,
				.gravityAcceleration = physicsObject.gravityAcceleration,
				.surroundingFluidDensity = physicsObject.surroundingFluidDensity,
				.centerOfBuoyancy = physicsObject.centerOfBuoyancy,
				.mass = physicsObject.mass,
				.principalMomentsOfInertia = physicsObject.principalMomentsOfInertia,
				.localInertiaOrientation = physicsObject.localInertiaOrientation,
				.collider{.shape = std::move(shape), .filter{.layers = COLLISION_LAYER_PHYSICS_OBJECT}},
				.material{
					.staticFriction = physicsObject.staticFriction,
					.kineticFriction = physicsObject.kineticFriction,
					.rollingResistance = physicsObject.rollingResistance,
					.restitution = physicsObject.restitution,
					.linearDrag = physicsObject.linearDrag,
					.angularDrag = physicsObject.angularDrag,
					.frictionCombine = physicsObject.frictionCombine,
					.restitutionCombine = physicsObject.restitutionCombine,
				},
			});
		registry.getComponent<ObjectModelInstance>(entity.getEntityID()).localTransformation = translate(-physicsObject.centerOfMass);
	}

	for (const phys::Position3D collectiblePosition : levelJSON.collectibles) {
		phys::EntityBuilder3D& entity = entities.emplace_back(registry.createEntity());
		createCollectible(entity, resources, collectiblePosition);
	}

	level.entityIDs.reserve(entities.size());

	Level* output = resources.addResourceIfMissing<Level>(std::move(level));
	if (!output) {
		output = &resources.getResource<Level>();
		for (const phys::EntityID entityID : output->entityIDs) {
			registry.destroyEntity(entityID);
		}
		resetAllPlayerCollectibles(registry);
		*output = std::move(level);
	}

	for (phys::EntityBuilder3D& entity : entities) {
		output->entityIDs.push_back(entity.build());
	}
}

size_t getLevelCollectibleCount(const phys::ResourceRegistry3D& resources) {
	return resources.getResource<Level>().collectibleCount;
}

phys::Box3D getLevelBounds(const phys::ResourceRegistry3D& resources) {
	return resources.getResource<Level>().bounds;
}

const gfx::Sky3D& getLevelSky(const phys::ResourceRegistry3D& resources) {
	return resources.getResource<Level>().sky;
}

Pair<phys::Position3D, phys::PitchYaw> getLevelPlayerSpawnPositionAndAngles(const phys::ResourceRegistry3D& resources) {
	return resources.getResource<Level>().playerSpawnPositionAndAngles;
}
