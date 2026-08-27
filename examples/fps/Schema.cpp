// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "Schema.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "AssetCache.hpp"
#include "EntityCallbacks.hpp"
#include "EntityType.hpp"
#include "PlayerEntityMap.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "game_components.hpp"

#include <sstream> // std::istringstream
#include <utility> // std::move

namespace {

[[nodiscard]] phys::Collider3D translateCollider(Schema& schema, const res::Model& model, const res::Model::Collider& collider) {
	const auto translateCollisionLayers = [&](res::Model::CollisionLayers collisionLayers) -> phys::CollisionLayers {
		phys::CollisionLayers result{};
		for (const auto& [layerName, collisionLayerIndex] : model.collisionLayerMap) {
			if (collisionLayers[collisionLayerIndex]) {
				result |= schema.getModelCollisionLayer(layerName);
			}
		}
		return result;
	};

	phys::Collider3D result{
		.shape{},
		.filter{
			.layers = translateCollisionLayers(collider.layers),
			.detectionLayers = schema.getModelObjectDefaultDetectionLayers() | translateCollisionLayers(collider.detectionLayers),
			.noDetectionLayers = translateCollisionLayers(collider.noDetectionLayers),
			.responseLayers = schema.getModelObjectDefaultResponseLayers() | translateCollisionLayers(collider.responseLayers),
			.noResponseLayers = translateCollisionLayers(collider.noResponseLayers),
		},
	};
	GREM_MATCH(collider.shape) {
		GREM_CASE(const res::Model::PlaneShape& plane) {
			if (isinf(plane.sizeX) && isinf(plane.sizeZ)) {
				if (plane.doubleSided) {
					result.shape = phys::InfinitePlaneShape3D{};
				} else {
					result.shape = phys::InfiniteHalfSpaceShape3D{};
				}
			} else {
				result.shape = phys::BoxShape3D{.halfExtents{
					isinf(plane.sizeX) ? 1000_meters : plane.sizeX * phys::METERS,
					phys::Length1D::MACHINE_EPSILON,
					isinf(plane.sizeZ) ? 1000_meters : plane.sizeZ * phys::METERS,
				}};
			}
			break;
		}
		GREM_CASE(const res::Model::SphereShape& sphere) {
			result.shape = phys::SphereShape3D{.radius = sphere.radius * phys::METERS};
			break;
		}
		GREM_CASE(const res::Model::BoxShape& box) {
			if (box.size.x == box.size.y && box.size.x == box.size.z) {
				result.shape = phys::CubeShape3D{.halfExtent = box.size.x * 0.5_meters};
			} else {
				result.shape = phys::BoxShape3D{.halfExtents = (box.size * 0.5f) * phys::METERS};
			}
			break;
		}
		GREM_CASE(const res::Model::CylinderShape& cylinder) {
			if (cylinder.bottomRadius == cylinder.topRadius) {
				result.shape = phys::CylinderShape3D{
					.radius = cylinder.bottomRadius * phys::METERS,
					.halfLength = cylinder.halfLength * phys::METERS,
				};
			} else {
				result.shape = phys::TaperedCylinderShape3D{
					.bottomRadius = cylinder.bottomRadius * phys::METERS,
					.topRadius = cylinder.topRadius * phys::METERS,
					.halfLength = cylinder.halfLength * phys::METERS,
				};
			}
			break;
		}
		GREM_CASE(const res::Model::CapsuleShape& capsule) {
			if (capsule.bottomRadius == capsule.topRadius) {
				result.shape = phys::CapsuleShape3D{
					.radius = capsule.bottomRadius * phys::METERS,
					.halfLength = capsule.halfLength * phys::METERS,
				};
			} else {
				result.shape = phys::TaperedCapsuleShape3D{
					.bottomRadius = capsule.bottomRadius * phys::METERS,
					.topRadius = capsule.topRadius * phys::METERS,
					.halfLength = capsule.halfLength * phys::METERS,
				};
			}
			break;
		}
		GREM_CASE(const res::Model::ConvexPolytopeShape& convexPolytope) {
			result.shape = phys::ConvexPolytopeShape3D{convexPolytope};
			break;
		}
		GREM_CASE(const res::Model::TriangleMeshShape& triangleMesh) {
			result.shape = phys::TriangleMeshShape3D{triangleMesh};
			break;
		}
	}
	return result;
}

[[nodiscard]] ModelObjectDescription::PhysicsObjectDescription createModelObjectPhysicsObjectDescription(Schema& schema, const res::Model& model,
	Span<const mat4> bindPoseJointMatrices, res::Model::PhysicsJointIndex physicsObjectIndex, const res::Model::PhysicsObject& physicsObject) {
	ModelObjectDescription::PhysicsObjectDescription result{};

	result.jointIndex = physicsObject.jointIndex;

	const mat4& jointMatrix = bindPoseJointMatrices[physicsObject.jointIndex];
	const auto [translation, rotation, scale] = decomposeTranslationRotationScale(jointMatrix);
	result.initialLocalOffset = translation * phys::METERS;
	result.initialLocalOrientation = rotation;
	result.initialLocalScale = scale;

	size_t colliderCount = 0;
	res::Model::JointIndex colliderJointIndex = 0;
	for (res::Model::JointIndex jointIndex = 0; jointIndex < model.bindPose.localJoints.size(); ++jointIndex) {
		if (model.jointColliders[jointIndex] && model.jointPhysicsObjectIndices[jointIndex] == physicsObjectIndex) {
			colliderJointIndex = jointIndex;
			++colliderCount;
		}
	}

	if (colliderCount == 0) {
		result.collider = {.shape{}, .filter{.layers{}, .detectionLayers{}, .noDetectionLayers{}, .responseLayers{}, .noResponseLayers{}}};
	} else if (colliderCount == 1) {
		result.collider = translateCollider(schema, model, *model.jointColliders[colliderJointIndex]);
		if (colliderJointIndex != physicsObject.jointIndex) {
			const mat4& colliderJointMatrix = bindPoseJointMatrices[colliderJointIndex];
			const mat4 colliderLocalTransformation = inverse(jointMatrix) * colliderJointMatrix;
			const auto [colliderLocalTranslation, colliderLocalRotation, colliderLocalScale] = decomposeTranslationRotationScale(colliderLocalTransformation);
			SharedPointer<phys::Shape3D> shape = SharedPointer<phys::Shape3D>::create(std::move(result.collider.shape));
			result.collider.shape = phys::LocallyTransformedShape3D{
				.shape = std::move(shape),
				.localOffset = colliderLocalTranslation * phys::METERS,
				.localOrientation = colliderLocalRotation,
				.localScale = colliderLocalScale,
			};
		}
	} else {
		SharedPointer<phys::SubCollider3D[]> subColliders = SharedPointer<phys::SubCollider3D[]>::create(colliderCount);
		colliderCount = 0;
		for (res::Model::JointIndex jointIndex = 0; jointIndex < model.bindPose.localJoints.size(); ++jointIndex) {
			if (model.jointColliders[jointIndex] && model.jointPhysicsObjectIndices[jointIndex] == physicsObjectIndex) {
				phys::SubCollider3D& subCollider = subColliders[static_cast<ptrdiff_t>(colliderCount)];
				subCollider.collider = translateCollider(schema, model, *model.jointColliders[jointIndex]);
				result.collider.filter.layers |= subCollider.collider.filter.layers;
				result.collider.filter.detectionLayers |= subCollider.collider.filter.detectionLayers;
				result.collider.filter.responseLayers |= subCollider.collider.filter.responseLayers;
				const mat4& colliderJointMatrix = bindPoseJointMatrices[jointIndex];
				const mat4 colliderLocalTransformation = inverse(jointMatrix) * colliderJointMatrix;
				const auto [colliderLocalTranslation, colliderLocalRotation, colliderLocalScale] = decomposeTranslationRotationScale(colliderLocalTransformation);
				subCollider.localOffset = colliderLocalTranslation * phys::METERS;
				subCollider.localOrientation = colliderLocalRotation;
				subCollider.localScale = colliderLocalScale;
				++colliderCount;
			}
		}
		result.collider.shape = phys::CompoundColliderShape3D{std::move(subColliders)};
	}

	result.centerOfMass = physicsObject.centerOfMass * phys::METERS;
	if (result.centerOfMass != 0) {
		result.collider.shape =
			phys::LocallyTransformedShape3D{.shape = SharedPointer<phys::Shape3D>::create(std::move(result.collider.shape)), .localOffset = -result.centerOfMass};
		result.initialLocalOffset += result.initialLocalOrientation(result.initialLocalScale * result.centerOfMass);
	}

	result.mass = physicsObject.mass * phys::KILOGRAMS;

	result.principalMomentsOfInertia = physicsObject.principalMomentsOfInertia * phys::KILOGRAM_SQUARE_METERS;
	result.localInertiaOrientation = phys::LocalInertiaOrientation3D{physicsObject.inertiaOrientation};

	phys::Material material{};
	material.staticFriction = physicsObject.staticFriction;
	material.kineticFriction = physicsObject.dynamicFriction;
	if (physicsObject.dynamicFriction == 0) {
		material.rollingResistance = {};
	}
	material.restitution = physicsObject.restitution;
	material.frictionCombine = static_cast<phys::Material::FrictionCombine>(physicsObject.frictionCombine);
	material.restitutionCombine = static_cast<phys::Material::RestitutionCombine>(material.restitutionCombine);
	result.material = material;

	result.initialLinearVelocity = physicsObject.initialLinearVelocity * phys::METERS_PER_SECOND;
	result.initialAngularVelocity = physicsObject.initialAngularVelocity * phys::RADIANS_PER_SECOND;
	result.gravityFactor = physicsObject.gravityFactor;

	return result;
}

[[nodiscard]] ModelObjectDescription::PhysicsJointDescription createModelObjectPhysicsJointDescription(const res::Model& model, Span<const mat4> bindPoseJointMatrices,
	const res::Model::PhysicsJoint& physicsJoint) {
	ModelObjectDescription::PhysicsJointDescription result{};

	Pair<phys::Length3D> attachmentOffsets{};
	Pair<phys::Orientation3D> attachmentOrientations{};
	phys::Mass combinedMass{};
	phys::PrincipalMomentsOfInertia3D combinedPrincipalMomentsOfInertia{};
	if (physicsJoint.objectIndices.first != Limits<res::Model::PhysicsObjectIndex>::MAX) {
		const res::Model::PhysicsObject& physicsObjectA = model.physicsObjects[physicsJoint.objectIndices.first];
		const auto [translationA, rotationA, scaleA] = decomposeTranslationRotationScale(bindPoseJointMatrices[physicsObjectA.jointIndex]);
		attachmentOffsets.first = translationA * phys::METERS;
		attachmentOrientations.first = rotationA;
		if (isfinite(physicsObjectA.mass)) {
			combinedMass += physicsObjectA.mass * phys::KILOGRAMS;
		}
		combinedPrincipalMomentsOfInertia += select(isfinite(physicsObjectA.principalMomentsOfInertia), physicsObjectA.principalMomentsOfInertia * phys::KILOGRAM_SQUARE_METERS,
			phys::PrincipalMomentsOfInertia3D{});
	}
	if (physicsJoint.objectIndices.second != Limits<res::Model::PhysicsObjectIndex>::MAX) {
		const res::Model::PhysicsObject& physicsObjectB = model.physicsObjects[physicsJoint.objectIndices.second];
		const auto [translationB, rotationB, scaleB] = decomposeTranslationRotationScale(bindPoseJointMatrices[physicsObjectB.jointIndex]);
		attachmentOffsets.second = translationB * phys::METERS;
		attachmentOrientations.second = rotationB;
		if (isfinite(physicsObjectB.mass)) {
			combinedMass += physicsObjectB.mass * phys::KILOGRAMS;
		}
		combinedPrincipalMomentsOfInertia += select(isfinite(physicsObjectB.principalMomentsOfInertia), physicsObjectB.principalMomentsOfInertia * phys::KILOGRAM_SQUARE_METERS,
			phys::PrincipalMomentsOfInertia3D{});
	}
	result.objectIndices = physicsJoint.objectIndices;
	result.genericJointOptions = phys::GenericJointOptions3D{
		.attachmentOffsets = attachmentOffsets,
		.attachmentOrientations = attachmentOrientations,
		.linearConstraint =
			phys::JointLinearConstraint3D{
				.driveTargetVelocities = physicsJoint.targetLinearVelocity * phys::METERS_PER_SECOND,
				.driveMaxForces{
					((physicsJoint.driveIgnoresMassX) ? (combinedMass * physicsJoint.maxForce.x * phys::METERS_PER_SECOND_SQUARED) : physicsJoint.maxForce.x * phys::NEWTON),
					((physicsJoint.driveIgnoresMassY) ? (combinedMass * physicsJoint.maxForce.y * phys::METERS_PER_SECOND_SQUARED) : physicsJoint.maxForce.y * phys::NEWTON),
					((physicsJoint.driveIgnoresMassZ) ? (combinedMass * physicsJoint.maxForce.z * phys::METERS_PER_SECOND_SQUARED) : physicsJoint.maxForce.z * phys::NEWTON),
				},
				.minOffsets = select(isfinite(physicsJoint.minDistances), physicsJoint.minDistances * phys::METERS, phys::Length3D::MIN),
				.maxOffsets = select(isfinite(physicsJoint.maxDistances), physicsJoint.maxDistances * phys::METERS, phys::Length3D::MAX),
				.limitStiffnesses = select(isfinite(physicsJoint.linearStiffnesses) & notEqual(physicsJoint.linearStiffnesses, vec3{}),
					physicsJoint.linearStiffnesses * phys::HERTZ, phys::Rate3D{phys::DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS}),
				.limitDampingRatios = select(isfinite(physicsJoint.linearDamping) & notEqual(physicsJoint.linearDamping, vec3{}), physicsJoint.linearDamping,
					vec3{phys::DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO}),
			},
		.distanceConstraint{},
		.angularConstraint =
			phys::JointAngularConstraint3D{
				.driveTargetVelocities = physicsJoint.targetAngularVelocity * phys::RADIANS_PER_SECOND,
				.driveMaxTorques{
					((physicsJoint.driveIgnoresMomentOfInertiaX) ? combinedPrincipalMomentsOfInertia.getX() * (physicsJoint.maxTorque.x * phys::RADIANS_PER_SECOND_SQUARED)
																 : physicsJoint.maxTorque.x * phys::NEWTON_METERS),
					((physicsJoint.driveIgnoresMomentOfInertiaY) ? combinedPrincipalMomentsOfInertia.getY() * (physicsJoint.maxTorque.y * phys::RADIANS_PER_SECOND_SQUARED)
																 : physicsJoint.maxTorque.y * phys::NEWTON_METERS),
					((physicsJoint.driveIgnoresMomentOfInertiaZ) ? combinedPrincipalMomentsOfInertia.getZ() * (physicsJoint.maxTorque.z * phys::RADIANS_PER_SECOND_SQUARED)
																 : physicsJoint.maxTorque.z * phys::NEWTON_METERS),
				},
				.minAngles = select(isfinite(physicsJoint.minAngles), physicsJoint.minAngles * phys::RADIANS, phys::Rotation3D::MIN),
				.maxAngles = select(isfinite(physicsJoint.maxAngles), physicsJoint.maxAngles * phys::RADIANS, phys::Rotation3D::MAX),
				.limitStiffnesses =
					select(isfinite(physicsJoint.angularStiffnesses) & notEqual(physicsJoint.angularStiffnesses, vec3{}), physicsJoint.angularStiffnesses * phys::HERTZ,
						phys::AngularFrequency3D{phys::DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS}),
				.limitDampingRatios =
					select(isfinite(physicsJoint.angularDamping) & notEqual(physicsJoint.angularDamping, vec3{}), physicsJoint.angularDamping,
						vec3{phys::DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO}),
			},
		.coneConstraint{},
		.twistConstraint{},
		.flags{},
	};

	return result;
}

[[nodiscard]] ModelObjectDescription::LightDescription createModelObjectLightDescription(const res::Model::Light& light) {
	ModelObjectDescription::LightDescription result{};

	result.jointIndex = light.jointIndex;

	GREM_MATCH(light) {
		GREM_CASE(const res::Model::DirectionalLight& directionalLight) {
			result.lightOptions = gfx::DirectionalLightOptions3D{
				.color = Color::fromLinear(directionalLight.color, directionalLight.intensity),
			};
			break;
		}
		GREM_CASE(const res::Model::PointLight& pointLight) {
			result.lightOptions = gfx::PointLightOptions3D{
				.range = pointLight.range,
				.color = Color::fromLinear(pointLight.color, pointLight.intensity),
			};
			break;
		}
		GREM_CASE(const res::Model::SpotLight& spotLight) {
			result.lightOptions = gfx::SpotLightOptions3D{
				.range = spotLight.range,
				.innerConeAngle = spotLight.innerConeAngle,
				.outerConeAngle = spotLight.outerConeAngle,
				.color = Color::fromLinear(spotLight.color, spotLight.intensity),
			};
			break;
		}
	}

	return result;
}

} // namespace

void Schema::extend(String fileContents, CStringView filepath, const Filesystem* filesystem, HashSet<CStringView>* visitedFilepaths) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Extend schema {}", filepath));

	if (visitedFilepaths && !visitedFilepaths->insert(filepath).second) {
		throw Error{"Cycle detected in schema."};
	}

	crc32.append(fileContents);
	try {
		try {
			std::istringstream stream{std::move(fileContents)};
			json::Reader reader{stream};
			reader.readCustomObject([&](const json::SourceLocation& source, const json::String& key) -> void {
				if (key == "extends") {
					const StringView filepathPrefix = filepath.substr(0, filepath.find_last_of("/\\") + 1);
					if (reader.nextIsString()) {
						if (!filesystem || !visitedFilepaths) {
							throw json::Error{"Nested schema files are not supported in this context.", source};
						}
						const String nestedFilepath = String{filepathPrefix} + reader.readString();
						extend(filesystem->readInputFileString(nestedFilepath), nestedFilepath, filesystem);
					} else {
						reader.readCustomArray([&](const json::SourceLocation&) -> void {
							if (!filesystem || !visitedFilepaths) {
								throw json::Error{"Nested schema files are not supported in this context.", source};
							}
							const String nestedFilepath = String{filepathPrefix} + reader.readString();
							extend(filesystem->readInputFileString(nestedFilepath), nestedFilepath, filesystem);
						});
					}
				} else if (key == "name") {
					name = reader.readString();
					eprintln("Loading schema \"{}\"...", name);
				} else if (key == "player") {
					playerPrefabFilepath = reader.readString();
				} else if (key == "dead_player") {
					deadPlayerPrefabFilepath = reader.readString();
				} else if (key == "modelObjectDefaultLayer") {
					modelObjectDefaultLayer = parseCollisionLayer(reader.readValue());
				} else if (key == "modelObjectDefaultDetectionLayers") {
					modelObjectDefaultDetectionLayers = parseCollisionLayers(reader.readValue());
				} else if (key == "modelObjectDefaultResponseLayers") {
					modelObjectDefaultResponseLayers = parseCollisionLayers(reader.readValue());
				} else if (key == "modelCollisionLayerMap") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						modelCollisionLayerMap.emplace(key, parseCollisionLayer(reader.readValue()));
					});
				} else if (key == "sounds") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), soundDescriptions[SoundType{CRC32{key}}]);
					});
				} else if (key == "decalMaterials") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), decalMaterialDescriptions[DecalMaterialType{CRC32{key}}]);
					});
				} else if (key == "sprites") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), spriteDescriptions[SpriteType{CRC32{key}}]);
					});
				} else if (key == "models") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), modelDescriptions[ModelType{CRC32{key}}]);
					});
				} else if (key == "projectiles") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), projectileDescriptions[ProjectileType{CRC32{key}}]);
					});
				} else if (key == "weapons") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), weaponDescriptions[WeaponType{CRC32{key}}]);
					});
				} else if (key == "movements") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), movementDescriptions[MovementType{CRC32{key}}]);
					});
				} else if (key == "particles") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), particleDescriptions[ParticleType{CRC32{key}}]);
					});
				} else if (key == "damageables") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void { //
						parseValue(reader.readValue(), damageableDescriptions[DamageableType{CRC32{key}}]);
					});
				} else if (key == "entities") {
					reader.readCustomObject([&](const json::SourceLocation& entitySource, const json::String& key) -> void {
						EntityDescription entityDescription{.name = key};
						reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
							if (key == "flags") {
								reader.readCustomArray([&](const json::SourceLocation&) -> void {
									EntityFlag flag{};
									parseValue(reader.readValue(), flag);
									entityDescription.flags |= flag;
								});
							} else if (key == "state") {
								reader.readCustomArray([&](const json::SourceLocation& componentSource) -> void {
									if (entityDescription.stateComponents.size() >= 64) {
										throw json::Error{"Maximum state component count exceeded.", componentSource};
									}
									const String componentName = reader.readString();
									bool found = false;
									meta::forEach(VALID_STATE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
										if (!found && componentName == validComponent.name) {
											const StateComponentDescription stateComponentDescription = StateComponentDescription::create<Component>(validComponent.name);
											if (containsBy<&StateComponentDescription::nameCRC32>(entityDescription.stateComponents, stateComponentDescription.nameCRC32)) {
												throw json::Error{formatString("Duplicate state component \"{}\".", componentName), componentSource};
											}
											entityDescription.stateComponents.push_back(stateComponentDescription);
											found = true;
										}
									});
									if (!found) {
										throw json::Error{formatString("Invalid state component type \"{}\".", componentName), componentSource};
									}
								});
								GREM_ASSERT(entityDescription.stateComponents.size() <= tuple_size_v<decltype(VALID_STATE_COMPONENT_TYPES)>);
							} else if (key == "intermediate") {
								reader.readCustomArray([&](const json::SourceLocation& componentSource) -> void {
									const String componentName = reader.readString();
									bool found = false;
									meta::forEach(VALID_INTERMEDIATE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
										if (!found && componentName == validComponent.name) {
											const IntermediateComponentDescription intermediateComponentDescription =
												IntermediateComponentDescription::create<Component>(validComponent.name);
											if (containsBy<&IntermediateComponentDescription::nameCRC32>(entityDescription.intermediateComponents,
													intermediateComponentDescription.nameCRC32)) {
												throw json::Error{formatString("Duplicate intermediate component \"{}\".", componentName), componentSource};
											}
											entityDescription.intermediateComponents.push_back(intermediateComponentDescription);
											found = true;
										}
									});
									if (!found) {
										throw json::Error{formatString("Invalid intermediate component type \"{}\".", componentName), componentSource};
									}
								});
								GREM_ASSERT(entityDescription.intermediateComponents.size() <= tuple_size_v<decltype(VALID_INTERMEDIATE_COMPONENT_TYPES)>);
							} else if (key == "clientside") {
								reader.readCustomArray([&](const json::SourceLocation& componentSource) -> void {
									const String componentName = reader.readString();
									bool found = false;
									meta::forEach(VALID_CLIENTSIDE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
										if (!found && componentName == validComponent.name) {
											const ClientsideComponentDescription clientsideComponentDescription =
												ClientsideComponentDescription::create<Component>(validComponent.name);
											if (containsBy<&ClientsideComponentDescription::nameCRC32>(entityDescription.clientsideComponents,
													clientsideComponentDescription.nameCRC32)) {
												throw json::Error{formatString("Duplicate clientside component \"{}\".", componentName), componentSource};
											}
											entityDescription.clientsideComponents.push_back(clientsideComponentDescription);
											found = true;
										}
									});
									if (!found) {
										throw json::Error{formatString("Invalid clientside component type \"{}\".", componentName), componentSource};
									}
								});
								GREM_ASSERT(entityDescription.clientsideComponents.size() <= tuple_size_v<decltype(VALID_CLIENTSIDE_COMPONENT_TYPES)>);
							} else if (key == "physics") {
								parseValue(reader.readValue(), entityDescription.physicsObjectOptions);
							}
						});
						const EntityType entityType{CRC32{key}};
						if (entityType == EntityType{}) {
							throw json::Error{formatString("Hash collision detected with entity type name \"{}\".", ""), entitySource};
						}
						if (const auto [it, inserted] = entityDescriptions.emplace(entityType, std::move(entityDescription)); !inserted) {
							throw json::Error{formatString("Hash collision detected with entity type name \"{}\".", it->first), entitySource};
						}
					});
				}
			});
		} catch (...) {
			Error::throwWithNestedFilepath(filepath);
		}
	} catch (...) {
		Error::throwWithNested(Error{"Failed to load schema."});
	}

	if (visitedFilepaths) {
		visitedFilepaths->erase(filepath);
	}
}

void Schema::preloadAssets(AssetCache& assetCache) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [modelType, modelDescription] : getLoadedModelDescriptions()) {
		if (modelDescription.preloadObjectDescription) {
			loadModelObjectDescription(assetCache, modelType);
		}
		if (modelDescription.preloadConvexHullShape) {
			loadModelConvexHullShape(assetCache, modelType);
		}
		if (modelDescription.preloadTriangleMeshShape) {
			loadModelTriangleMeshShape(assetCache, modelType);
		}
	}
}

const ModelDescription& Schema::loadModelDescription(const Filesystem& filesystem, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	const auto [it, inserted] = modelDescriptions.try_emplace(modelType);
	if (inserted) {
		try {
			String filepath{"models"};

			const auto checkFilename = [&](const auto& self, CStringView filename) -> bool {
				const size_t filepathOffset = filepath.size();
				filepath.push_back('/');
				filepath.append(filename);
				if (filesystem.getInputFileMetadata(filepath).kind == File::Kind::DIRECTORY) {
					bool found = false;
					filesystem.forEachInputFilenameInDirectory(filepath, [&](CStringView filename) -> bool {
						if (self(self, filename)) {
							found = true;
							return true;
						}
						return false;
					});
					if (found) {
						return true;
					}
					filepath.resize(filepathOffset);
					return false;
				}
				if (ModelType{CRC32{filepath}} == modelType) {
					return true;
				}
				filepath.resize(filepathOffset);
				return false;
			};
			bool found = false;
			filesystem.forEachInputFilenameInDirectory("models", [&](CStringView filename) -> bool {
				if (checkFilename(checkFilename, filename)) {
					found = true;
					return true;
				}
				return false;
			});
			if (!found) {
				throw Error{"Specified ModelType missing from schema."};
			}
			it->second.filepath = std::move(filepath);
		} catch (...) {
			modelDescriptions.erase(it);
			throw;
		}
	}
	return it->second;
}

const ModelObjectDescription& Schema::loadModelObjectDescription(AssetCache& assetCache, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	const auto [it, inserted] = modelObjectDescriptions.try_emplace(modelType);
	if (inserted) {
		try {
			const ModelDescription& modelDescription = loadModelDescription(assetCache.getFilesystem(), modelType);
			GREM_PROFILE_BLOCK_DYNAMIC(formatString("Model {}", modelDescription.filepath));

			const SharedPointer<res::Model> model = assetCache.getModel(modelDescription.filepath, modelDescription.options);

			res::Model::Transformation bindPoseTransformation{};
			bindPoseTransformation.assign(mat4{1.0f}, model->bindPose.localJoints, model->bindPose.localMorphTargetWeights, model->jointParentIndices);

			ModelObjectDescription modelObjectDescription{};
			modelObjectDescription.jointDescriptions.assign(model->bindPose.localJoints.size(), ModelObjectDescription::JointDescription{});
			modelObjectDescription.physicsObjectDescriptions.resize(model->physicsObjects.size());
			modelObjectDescription.physicsJointDescriptions.resize(model->physicsJoints.size());
			modelObjectDescription.lightDescriptions.resize(model->lights.size());

			for (res::Model::PhysicsObjectIndex physicsObjectIndex = 0; physicsObjectIndex < model->physicsObjects.size(); ++physicsObjectIndex) {
				const res::Model::PhysicsObject& physicsObject = model->physicsObjects[physicsObjectIndex];
				modelObjectDescription.physicsObjectDescriptions[physicsObjectIndex] =
					createModelObjectPhysicsObjectDescription(*this, *model, bindPoseTransformation.jointMatrices, physicsObjectIndex, physicsObject);
				modelObjectDescription.jointDescriptions[physicsObject.jointIndex].physicsObjectIndex = physicsObjectIndex;
			}

			bool hasRootCollider = false;
			for (res::Model::JointIndex jointIndex = 0; jointIndex < model->bindPose.localJoints.size(); ++jointIndex) {
				if (model->jointColliders[jointIndex] && model->jointPhysicsObjectIndices[jointIndex] == Limits<res::Model::PhysicsObjectIndex>::MAX) {
					hasRootCollider = true;
					break;
				}
			}

			if (!modelObjectDescription.jointDescriptions.front().physicsObjectIndex && hasRootCollider) {
				const res::Model::PhysicsObjectIndex physicsObjectIndex = static_cast<res::Model::PhysicsObjectIndex>(modelObjectDescription.physicsObjectDescriptions.size());
				modelObjectDescription.physicsObjectDescriptions.push_back(createModelObjectPhysicsObjectDescription(*this, *model, bindPoseTransformation.jointMatrices,
					Limits<res::Model::PhysicsObjectIndex>::MAX,
					res::Model::PhysicsObject{
						.jointIndex = 0,
						.mass = Limits<float>::INF,
						.centerOfMass{},
						.principalMomentsOfInertia{Limits<float>::INF},
						.inertiaOrientation{0.0f, 0.0f, 0.0f, 1.0f},
						.initialLinearVelocity{},
						.initialAngularVelocity{},
						.gravityFactor = 0.0f,
						.staticFriction = phys::Material{}.staticFriction,
						.dynamicFriction = phys::Material{}.kineticFriction,
						.rollingResistance = phys::Material{}.rollingResistance,
						.restitution = phys::Material{}.restitution,
						.frictionCombine = static_cast<res::Model::FrictionCombine>(phys::Material{}.frictionCombine),
						.restitutionCombine = static_cast<res::Model::RestitutionCombine>(phys::Material{}.restitutionCombine),
					}));
				modelObjectDescription.jointDescriptions.front().physicsObjectIndex = physicsObjectIndex;
			} else if (!modelObjectDescription.physicsObjectDescriptions.empty() && !modelObjectDescription.jointDescriptions.front().physicsObjectIndex) {
				const res::Model::PhysicsObjectIndex physicsObjectIndex = static_cast<res::Model::PhysicsObjectIndex>(modelObjectDescription.physicsObjectDescriptions.size());
				modelObjectDescription.physicsObjectDescriptions.push_back(ModelObjectDescription::PhysicsObjectDescription{
					.jointIndex = 0,
					.initialLocalOffset{},
					.initialLocalOrientation{},
					.initialLocalScale{},
					.mass = phys::Mass::INF,
					.centerOfMass{},
					.principalMomentsOfInertia = phys::PrincipalMomentsOfInertia3D::INF,
					.localInertiaOrientation{},
					.collider{.shape{}, .filter{.layers{}, .detectionLayers{}, .responseLayers{}}},
					.material{},
					.initialLinearVelocity{},
					.initialAngularVelocity{},
					.gravityFactor = 1_x,
				});
				modelObjectDescription.jointDescriptions.front().physicsObjectIndex = physicsObjectIndex;
			}

			for (res::Model::PhysicsJointIndex physicsJointIndex = 0; physicsJointIndex < model->physicsJoints.size(); ++physicsJointIndex) {
				const res::Model::PhysicsJoint& physicsJoint = model->physicsJoints[physicsJointIndex];
				ModelObjectDescription::PhysicsJointDescription& physicsJointDescription = modelObjectDescription.physicsJointDescriptions[physicsJointIndex];
				physicsJointDescription = createModelObjectPhysicsJointDescription(*model, bindPoseTransformation.jointMatrices, physicsJoint);
				if (!physicsJoint.enableCollision) {
					if (physicsJointDescription.objectIndices.first != Limits<res::Model::PhysicsObjectIndex>::MAX ||
						physicsJointDescription.objectIndices.second != Limits<res::Model::PhysicsObjectIndex>::MAX) {
						const res::Model::PhysicsObjectIndex physicsObjectIndexA =
							(physicsJointDescription.objectIndices.first == Limits<res::Model::PhysicsObjectIndex>::MAX)
								? static_cast<res::Model::PhysicsObjectIndex>(modelObjectDescription.physicsObjectDescriptions.size() - 1)
								: physicsJointDescription.objectIndices.first;
						const res::Model::PhysicsObjectIndex physicsObjectIndexB =
							(physicsJointDescription.objectIndices.second == Limits<res::Model::PhysicsObjectIndex>::MAX)
								? static_cast<res::Model::PhysicsObjectIndex>(modelObjectDescription.physicsObjectDescriptions.size() - 1)
								: physicsJointDescription.objectIndices.second;
						const phys::CollisionLayer layerA = getModelCollisionLayer(formatString("{}[{}]", modelDescription.filepath, physicsObjectIndexA));
						const phys::CollisionLayer layerB = getModelCollisionLayer(formatString("{}[{}]", modelDescription.filepath, physicsObjectIndexB));
						phys::Collider3D& colliderA = modelObjectDescription.physicsObjectDescriptions[physicsObjectIndexA].collider;
						phys::Collider3D& colliderB = modelObjectDescription.physicsObjectDescriptions[physicsObjectIndexB].collider;
						colliderA.filter.layers |= layerA;
						colliderA.filter.noDetectionLayers |= layerB;
						colliderA.filter.noResponseLayers |= layerB;
						colliderB.filter.layers |= layerB;
						colliderB.filter.noDetectionLayers |= layerA;
						colliderB.filter.noResponseLayers |= layerA;
					}
				}
			}

			for (res::Model::LightIndex lightIndex = 0; lightIndex < model->lights.size(); ++lightIndex) {
				modelObjectDescription.lightDescriptions[lightIndex] = createModelObjectLightDescription(model->lights[lightIndex]);
			}

			it->second = std::move(modelObjectDescription);
		} catch (...) {
			modelObjectDescriptions.erase(it);
			throw;
		}
	}
	return it->second;
}

const phys::Shape3D& Schema::loadModelConvexHullShape(AssetCache& assetCache, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	const auto [it, inserted] = modelConvexHullShapes.try_emplace(modelType);
	if (inserted) {
		try {
			const ModelDescription& modelDescription = loadModelDescription(assetCache.getFilesystem(), modelType);
			GREM_PROFILE_BLOCK_DYNAMIC(formatString("Model {}", modelDescription.filepath));

			const SharedPointer<res::Model> model = assetCache.getModel(modelDescription.filepath, modelDescription.options);

			mat4 rootTransformation{1.0f};
			if (modelDescription.centerOfMassOverride && *modelDescription.centerOfMassOverride != 0) {
				rootTransformation = translate(-*modelDescription.centerOfMassOverride);
			}

			res::Model::Transformation bindPoseTransformation{};
			bindPoseTransformation.assign(rootTransformation, model->bindPose.localJoints, model->bindPose.localMorphTargetWeights, model->jointParentIndices);

			Buffer<ConvexPolytopeVertex3D> vertices{};
			for (const res::Model::Instance& instance : model->instances) {
				const res::Model::Mesh& mesh = model->meshes.at(instance.meshIndex);
				const mat4 jointMatrix = bindPoseTransformation.jointMatrices[((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : instance.jointIndex];
				const Span<const vec3> meshPositions{
					std::launder(reinterpret_cast<const vec3*>(
						model->meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4 + static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t))),
					static_cast<size_t>(mesh.vertexCount)};
				vertices.reserve(vertices.size() + meshPositions.size());
				for (const vec3 position : meshPositions) {
					vertices.push_back(ConvexPolytopeVertex3D{vec3{jointMatrix * vec4{position, 1.0f}}});
				}
			}

			it->second = phys::ConvexPolytopeShape3D{vertices, 32};
		} catch (...) {
			modelConvexHullShapes.erase(it);
			throw;
		}
	}
	return it->second;
}

const phys::Shape3D& Schema::loadModelTriangleMeshShape(AssetCache& assetCache, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	const auto [it, inserted] = modelTriangleMeshShapes.try_emplace(modelType);
	if (inserted) {
		try {
			const ModelDescription& modelDescription = loadModelDescription(assetCache.getFilesystem(), modelType);
			GREM_PROFILE_BLOCK_DYNAMIC(formatString("Model {}", modelDescription.filepath));

			const SharedPointer<res::Model> model = assetCache.getModel(modelDescription.filepath, modelDescription.options);

			mat4 rootTransformation{1.0f};
			if (modelDescription.centerOfMassOverride && *modelDescription.centerOfMassOverride != 0) {
				rootTransformation = translate(-*modelDescription.centerOfMassOverride);
			}

			res::Model::Transformation bindPoseTransformation{};
			bindPoseTransformation.assign(rootTransformation, model->bindPose.localJoints, model->bindPose.localMorphTargetWeights, model->jointParentIndices);

			Buffer<TriangleMeshVertex3D> vertices{};
			Buffer<TriangleMeshVertexIndex> indices{};
			for (const res::Model::Instance& instance : model->instances) {
				const res::Model::Mesh& mesh = model->meshes.at(instance.meshIndex);
				const mat4 jointMatrix = bindPoseTransformation.jointMatrices[((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : instance.jointIndex];
				const Span<const vec3> meshPositions{
					std::launder(reinterpret_cast<const vec3*>(
						model->meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4 + static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t))),
					static_cast<size_t>(mesh.vertexCount)};

				const TriangleMeshVertexIndex vertexIndexOffset = static_cast<TriangleMeshVertexIndex>(vertices.size());
				vertices.reserve(vertices.size() + meshPositions.size());
				for (const vec3 position : meshPositions) {
					vertices.push_back(vec3{jointMatrix * vec4{position, 1.0f}});
				}

				if (mesh.indexCount > 0) {
					const Span<const uint32_t> meshIndices{std::launder(reinterpret_cast<const uint32_t*>(model->meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4)),
						static_cast<size_t>(mesh.indexCount)};
					indices.reserve(indices.size() + meshIndices.size());
					for (const uint32_t index : meshIndices) {
						indices.push_back(static_cast<TriangleMeshVertexIndex>(vertexIndexOffset + index));
					}
				} else {
					indices.resize(indices.size() + meshPositions.size());
					iota(Span{indices}.last(meshPositions.size()), vertexIndexOffset);
				}
			}

			it->second = phys::TriangleMeshShape3D{vertices, indices};
		} catch (...) {
			modelTriangleMeshShapes.erase(it);
			throw;
		}
	}
	return it->second;
}

void ModelType::setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) {
	GREM_PROFILE_FUNCTION();

	setImpliedModelJointComponents(entityBuilder, *this, entityBuilder.getComponent<EntityType>(), 0, registry, resources);
}

void ProjectileType::setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) {
	const ModelType modelType = resources.getResource<Schema>().getProjectileDescription(*this).modelType;
	entityBuilder.addOrAssignComponent<ModelType>(modelType).setImpliedComponents(entityBuilder, registry, resources);
}

void WeaponType::setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) {
	const ModelType modelType = resources.getResource<Schema>().getWeaponDescription(*this).modelType;
	entityBuilder.addOrAssignComponent<ModelType>(modelType).setImpliedComponents(entityBuilder, registry, resources);
}

void ParticleType::setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) {
	const ParticleDescription& particleDescription = resources.getResource<Schema>().getParticleDescription(*this);
	if (particleDescription.modelType != ModelType{}) {
		entityBuilder.addOrAssignComponent<ModelType>(particleDescription.modelType).setImpliedComponents(entityBuilder, registry, resources);
	}
	if (particleDescription.spriteType != SpriteType{}) {
		entityBuilder.addOrAssignComponent<SpriteType>(particleDescription.spriteType);
	}
	if (phys::Collider3D* const collider = entityBuilder.findComponent<phys::Collider3D>()) {
		if (particleDescription.layersOverride) {
			collider->filter.layers = *particleDescription.layersOverride;
		}
		if (particleDescription.detectionLayersOverride) {
			collider->filter.detectionLayers = *particleDescription.detectionLayersOverride;
		}
		if (particleDescription.noDetectionLayersOverride) {
			collider->filter.noDetectionLayers = *particleDescription.noDetectionLayersOverride;
		}
		if (particleDescription.responseLayersOverride) {
			collider->filter.responseLayers = *particleDescription.responseLayersOverride;
		}
		if (particleDescription.noResponseLayersOverride) {
			collider->filter.noResponseLayers = *particleDescription.noResponseLayersOverride;
		}
	}
	if (particleDescription.orientationFollowsVelocity) {
		entityBuilder.addOrAssignComponent<OrientPhysicsObjectByVelocity>(OrientPhysicsObjectByVelocity{
			.localOrientation = particleDescription.localOrientation,
		});
	}
}

void setImpliedModelJointComponents(EntityBuilder& entityBuilder, ModelType modelType, EntityType entityType, res::Model::JointIndex jointIndex, EntityRegistry& registry,
	ResourceRegistry& resources) {
	GREM_PROFILE_FUNCTION();

	AssetCache& assetCache = resources.getResource<AssetCache>();
	Schema& schema = resources.getResource<Schema>();

	const EntityDescription& entityDescription = schema.getEntityDescription(entityType);
	if (!entityDescription.physicsObjectOptions) {
		return;
	}

	const ModelDescription& modelDescription = schema.loadModelDescription(assetCache.getFilesystem(), modelType);
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Joint {} of model {}", jointIndex, modelDescription.filepath));

	const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);

	phys::Collider3D& collider = entityBuilder.getComponent<phys::Collider3D>();
	if (modelDescription.shapeOverride) {
		if (jointIndex == 0) {
			collider.shape = *modelDescription.shapeOverride;
		}
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size() && modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
		collider = modelObjectDescription.physicsObjectDescriptions[*modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex].collider;
	} else if (modelObjectDescription.physicsObjectDescriptions.empty()) {
		if (jointIndex == 0) {
			if (entityType == EntityType{"STATIC_MODEL_OBJECT"}) {
				collider.shape = schema.loadModelTriangleMeshShape(assetCache, modelType);
			} else {
				collider.shape = schema.loadModelConvexHullShape(assetCache, modelType);
			}
		}
	}

	phys::Mass mass = entityDescription.physicsObjectOptions->mass;
	phys::InverseMass& inverseMass = entityBuilder.getComponent<phys::InverseMass>();
	if (modelDescription.massOverride) {
		mass = *modelDescription.massOverride;
		if (mass <= 0) {
			mass = phys::ShapeView3D{collider.shape}.calculateVolume() * 0.5_grams_per_cubic_centimeter;
			if (mass <= 0) {
				mass = phys::Mass::INF;
			}
		}
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size() && modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
		const res::Model::PhysicsJointIndex physicsObjectIndex = *modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex;
		mass = modelObjectDescription.physicsObjectDescriptions[physicsObjectIndex].mass;
		if (mass <= 0) {
			mass = phys::ShapeView3D{collider.shape}.calculateVolume() * 0.5_grams_per_cubic_centimeter;
			if (mass <= 0) {
				mass = phys::Mass::INF;
			}
		}
	} else {
		if (mass <= 0) {
			mass = phys::ShapeView3D{collider.shape}.calculateVolume() * 0.5_grams_per_cubic_centimeter;
			if (mass <= 0) {
				mass = phys::Mass::INF;
			}
		}
	}
	inverseMass = phys::calculateInverseMass(mass);

	phys::PrincipalMomentsOfInertia3D principalMomentsOfInertia = entityDescription.physicsObjectOptions->principalMomentsOfInertia;
	phys::InversePrincipalMomentsOfInertia3D& inversePrincipalMomentsOfInertia = entityBuilder.getComponent<phys::InversePrincipalMomentsOfInertia3D>();
	if (modelDescription.principalMomentsOfInertiaOverride) {
		principalMomentsOfInertia = *modelDescription.principalMomentsOfInertiaOverride;
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size()) {
		if (const Optional<res::Model::PhysicsJointIndex> physicsObjectIndex = modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
			principalMomentsOfInertia = modelObjectDescription.physicsObjectDescriptions[*physicsObjectIndex].principalMomentsOfInertia;
		}
	}
	if (any(equal(principalMomentsOfInertia, 0))) {
		const phys::PrincipalMomentsOfInertia3D calculatedPrincipalMomentsOfInertia = phys::calculatePrincipalMomentsOfInertia(collider.shape, mass);
		meta::forEachIndex<3>([&](auto index) -> void {
			if (principalMomentsOfInertia[index] == 0) {
				principalMomentsOfInertia[index] = calculatedPrincipalMomentsOfInertia[index];
			}
		});
	}
	inversePrincipalMomentsOfInertia = phys::calculateInverseMomentOfInertia(principalMomentsOfInertia);

	if (modelDescription.localInertiaOrientationOverride) {
		entityBuilder.getComponent<phys::LocalInertiaOrientation3D>() = *modelDescription.localInertiaOrientationOverride;
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size()) {
		if (const Optional<res::Model::PhysicsJointIndex> physicsObjectIndex = modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
			entityBuilder.getComponent<phys::LocalInertiaOrientation3D>() = modelObjectDescription.physicsObjectDescriptions[*physicsObjectIndex].localInertiaOrientation;
		}
	}

	if (modelDescription.materialOverride) {
		entityBuilder.getComponent<phys::Material>() = *modelDescription.materialOverride;
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size()) {
		if (const Optional<res::Model::PhysicsJointIndex> physicsObjectIndex = modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
			entityBuilder.getComponent<phys::Material>() = modelObjectDescription.physicsObjectDescriptions[*physicsObjectIndex].material;
		}
	}

	if (modelDescription.gravityAccelerationOverride) {
		entityBuilder.getComponent<phys::LinearAcceleration3D>() = *modelDescription.gravityAccelerationOverride;
	} else if (jointIndex < modelObjectDescription.jointDescriptions.size()) {
		if (const Optional<res::Model::PhysicsJointIndex> physicsObjectIndex = modelObjectDescription.jointDescriptions[jointIndex].physicsObjectIndex) {
			entityBuilder.getComponent<phys::LinearAcceleration3D>() *= modelObjectDescription.physicsObjectDescriptions[*physicsObjectIndex].gravityFactor;
		}
	}

	phys::Simulation3D::updateObjectMomentOfInertiaTensor(registry, resources, entityBuilder.getEntityID());
	phys::Simulation3D::updateObjectBounds(registry, resources, entityBuilder.getEntityID());

	phys::ObjectActivity& activity = entityBuilder.getComponent<phys::ObjectActivity>();
	activity.isCorrectable = (isfinite(mass) || inversePrincipalMomentsOfInertia != 0) ? 1 : 0;
}

SpawnEntityResult spawnEntity(EntityRegistry& registry, ResourceRegistry& resources, ParseEntityComponentContext& context, EntityType entityType, EntityID::Flags flags,
	const EntityInitializer& initializer, phys::Position3D position, phys::Orientation3D orientation) {
	GREM_PROFILE_FUNCTION();

	const Schema& schema = resources.getResource<Schema>();
	SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
	const SynchronizedEntityID synchronizedEntityID{synchronizedEntityMap.nextSynchronizedEntityID.value};
	const EntityDescription& entityDescription = schema.getEntityDescription(entityType);
	flags |= schema.getEntityFlags();
	flags |= entityDescription.flags;
	EntityBuilder entityBuilder = registry.createEntity(flags);
	entityBuilder.addComponent<EntityType>(entityType);
	entityBuilder.addComponent<SynchronizedEntityID>(synchronizedEntityID);
	[[maybe_unused]] const auto [it, inserted] =
		synchronizedEntityMap.synchronizedEntityMappings.emplace(synchronizedEntityID, SynchronizedEntityMapping{.id = entityBuilder.getEntityID(), .type = entityType});
	GREM_ASSERT(inserted);
	try {
		for (const StateComponentDescription& stateComponentDescription : entityDescription.stateComponents) {
			stateComponentDescription.add(entityBuilder, context, schema, initializer);
		}
		for (const IntermediateComponentDescription& intermediateComponentDescription : entityDescription.intermediateComponents) {
			intermediateComponentDescription.add(entityBuilder);
		}
		if ((flags & ENTITY_CLIENTSIDE) != 0) {
			for (const ClientsideComponentDescription& clientsideComponentDescription : entityDescription.clientsideComponents) {
				clientsideComponentDescription.add(entityBuilder);
			}
		}
		if (entityDescription.physicsObjectOptions) {
			entityBuilder.extend([&](EntityRegistry& registry, EntityID entityID) -> void {
				phys::Simulation3D::addObjectComponents(registry, resources, entityID, phys::ObjectOptions3D{*entityDescription.physicsObjectOptions});
			});
		}
		for (const StateComponentDescription& stateComponentDescription : entityDescription.stateComponents) {
			stateComponentDescription.setImpliedComponents(entityBuilder, registry, resources);
		}

		if (position != phys::Position3D{} || orientation != phys::Orientation3D{}) {
			for (const StateComponentDescription& stateComponentDescription : entityDescription.stateComponents) {
				stateComponentDescription.transform(registry, entityBuilder.getEntityID(), position, orientation);
			}
			if (entityDescription.physicsObjectOptions) {
				phys::Simulation3D::updateObjectMomentOfInertiaTensor(registry, resources, entityBuilder.getEntityID());
				phys::Simulation3D::updateObjectBounds(registry, resources, entityBuilder.getEntityID());
			}
		}

		if (const PlayerID* const receivedPlayerID = resources.findResource<PlayerID>()) {
			if (const PlayerID* const playerID = entityBuilder.findComponent<PlayerID>()) {
				if (*playerID == *receivedPlayerID) {
					flags |= ENTITY_PHYSICS_PREDICTED | ENTITY_DISPLAY_PREDICTED;
				}
			}
			if (const WeaponState* const weaponState = entityBuilder.findComponent<WeaponState>()) {
				if (const EntityID holderEntityID = synchronizedEntityMap.findEntity(registry, weaponState->holder)) {
					if (const PlayerID* const holderPlayerID = registry.findComponent<PlayerID>(holderEntityID)) {
						if (*holderPlayerID == *receivedPlayerID) {
							flags |= ENTITY_DISPLAY_PREDICTED;
						}
					}
				}
			}
			if (const ParticleState* const particleState = entityBuilder.findComponent<ParticleState>()) {
				const phys::EntityID ownerEntityID = synchronizedEntityMap.findEntity(registry, particleState->owner);
				if ((ownerEntityID.getFlags() & (ENTITY_DISPLAY_SUBTICK_PREDICTED | ENTITY_DISPLAY_PREDICTED)) != 0) {
					flags |= ENTITY_DISPLAY_PREDICTED;
				}
				const phys::InverseMass* const inverseMass = registry.findComponent<phys::InverseMass>(ownerEntityID);
				const phys::InversePrincipalMomentsOfInertia3D* const inversePrincipalMomentsOfInertia =
					registry.findComponent<phys::InversePrincipalMomentsOfInertia3D>(ownerEntityID);
				if (inverseMass && inversePrincipalMomentsOfInertia && *inverseMass == 0 && *inversePrincipalMomentsOfInertia == 0) {
					flags |= ENTITY_DISPLAY_PREDICTED;
				}
			}
			if (const DecalAttachmentFrame* const decalAttachmentFrame = entityBuilder.findComponent<DecalAttachmentFrame>()) {
				const phys::EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, decalAttachmentFrame->target);
				const phys::InverseMass* const inverseMass = registry.findComponent<phys::InverseMass>(targetEntityID);
				const phys::InversePrincipalMomentsOfInertia3D* const inversePrincipalMomentsOfInertia =
					registry.findComponent<phys::InversePrincipalMomentsOfInertia3D>(targetEntityID);
				if (inverseMass && inversePrincipalMomentsOfInertia && *inverseMass == 0 && *inversePrincipalMomentsOfInertia == 0) {
					flags |= ENTITY_DISPLAY_SUBTICK_PREDICTED;
				}
			}
			it->second.id = entityBuilder.setEntityFlags(flags);
		}

		const EntityID entityID = entityBuilder.build();
		++synchronizedEntityMap.nextSynchronizedEntityID.value;

		for (const auto& [id, callback] : resources.getResource<EntityCallbacks>().onSpawn) {
			callback(registry, resources, entityID);
		}

		return {entityID, synchronizedEntityID};
	} catch (...) {
		synchronizedEntityMap.synchronizedEntityMappings.erase(it);
		throw;
	}
}

void spawnDecal(EntityRegistry& registry, ResourceRegistry& resources, DecalMaterialType decalMaterialType, EntityID::Flags flags, phys::Position3D position,
	phys::Orientation3D orientation, phys::Length2D size, phys::Distance range, EntityID targetEntityID, TickIndex tickIndex, Duration tickInterval) {
	GREM_PROFILE_FUNCTION();

	const SynchronizedEntityID* const targetSynchronizedEntityID = registry.findComponent<SynchronizedEntityID>(targetEntityID);
	if (!targetSynchronizedEntityID) {
		return;
	}

	const Schema& schema = resources.getResource<Schema>();
	const DecalMaterialDescription& decalMaterialDescription = schema.getDecalMaterialDescription(decalMaterialType);
	const phys::Position3D targetPosition = registry.getComponent<phys::Position3D>(targetEntityID);
	const phys::Orientation3D targetOrientation = registry.getComponent<phys::Orientation3D>(targetEntityID);
	const phys::Scale3D targetScale = registry.getComponent<phys::Scale3D>(targetEntityID);
	spawnEntity(registry, resources, EntityType{"DECAL"}, flags,
		ComponentInitializers{
			DecalMaterialType{decalMaterialType},
			DecalAttachmentFrame{
				.target = *targetSynchronizedEntityID,
				.localOffset = inverse(targetOrientation)(position - targetPosition) / targetScale,
				.localOrientation = inverse(targetOrientation) * orientation,
				.size = size,
				.range = range,
			},
			DestroyCountdown{.destroyOnTickIndex = (isinf(decalMaterialDescription.maxLifetime))
	                                                   ? TickIndex{}.getNext(Limits<TickCount>::MAX)
	                                                   : Timestamp{tickIndex, decalMaterialDescription.maxLifetime, tickInterval}.getTickIndex()},
		});
}

void spawnParticle(EntityRegistry& registry, ResourceRegistry& resources, rng::Xoroshiro128PlusPlusEngine& numberGenerator, ParticleType particleType, EntityID::Flags flags,
	phys::Position3D position, phys::Orientation3D orientation, phys::LinearVelocity3D linearVelocity, EntityID ownerEntityID, TickIndex tickIndex, Duration tickInterval) {
	GREM_PROFILE_FUNCTION();

	const SynchronizedEntityID* const ownerSynchronizedEntityID = registry.findComponent<SynchronizedEntityID>(ownerEntityID);
	if (!ownerSynchronizedEntityID) {
		return;
	}

	const Schema& schema = resources.getResource<Schema>();
	const ParticleDescription& particleDescription = schema.getParticleDescription(particleType);

	const EntityType entityType =
		(particleDescription.spriteType != SpriteType{} && schema.getSpriteDescription(particleDescription.spriteType).frameCount > 1)
			? EntityType{"ANIMATED_PARTICLE"}
			: EntityType{"PARTICLE"};
	const phys::Position3D launchPosition = position + orientation(particleDescription.localOffset);

	rng::UniformIntegerDistribution<uint16_t> launchCountDistribution{
		particleDescription.launchCountMin,
		max(particleDescription.launchCountMin, particleDescription.launchCountMax),
	};
	rng::UniformRealDistribution<float> launchSpeedDistribution{
		particleDescription.launchSpeedMin.in(phys::Speed::UNIT),
		max(particleDescription.launchSpeedMin, particleDescription.launchSpeedMax).in(phys::Speed::UNIT),
	};
	rng::UniformRealDistribution<float> launchSpreadAngleDistribution{
		particleDescription.launchSpreadAngleMin.in(phys::Angle::UNIT),
		max(particleDescription.launchSpreadAngleMin, particleDescription.launchSpreadAngleMax).in(phys::Angle::UNIT),
	};
	rng::UniformRealDistribution<float> launchRollAngleDistribution{
		particleDescription.launchRollAngleMin.in(phys::Angle::UNIT),
		max(particleDescription.launchRollAngleMin, particleDescription.launchRollAngleMax).in(phys::Angle::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAngularVelocityXDistrubition{
		particleDescription.launchTangentSpaceAngularVelocityMin.getX().in(phys::AngularVelocity3D::UNIT),
		max(particleDescription.launchTangentSpaceAngularVelocityMin.getX(), particleDescription.launchTangentSpaceAngularVelocityMax.getX()).in(phys::AngularVelocity3D::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAngularVelocityYDistrubition{
		particleDescription.launchTangentSpaceAngularVelocityMin.getY().in(phys::AngularVelocity3D::UNIT),
		max(particleDescription.launchTangentSpaceAngularVelocityMin.getY(), particleDescription.launchTangentSpaceAngularVelocityMax.getY()).in(phys::AngularVelocity3D::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAngularVelocityZDistrubition{
		particleDescription.launchTangentSpaceAngularVelocityMin.getZ().in(phys::AngularVelocity3D::UNIT),
		max(particleDescription.launchTangentSpaceAngularVelocityMin.getZ(), particleDescription.launchTangentSpaceAngularVelocityMax.getZ()).in(phys::AngularVelocity3D::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAccelerationXDistrubition{
		particleDescription.launchTangentSpaceAccelerationMin.getX().in(phys::LinearAcceleration3D::UNIT),
		max(particleDescription.launchTangentSpaceAccelerationMin.getX(), particleDescription.launchTangentSpaceAccelerationMax.getX()).in(phys::LinearAcceleration3D::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAccelerationYDistrubition{
		particleDescription.launchTangentSpaceAccelerationMin.getY().in(phys::LinearAcceleration3D::UNIT),
		max(particleDescription.launchTangentSpaceAccelerationMin.getY(), particleDescription.launchTangentSpaceAccelerationMax.getY()).in(phys::LinearAcceleration3D::UNIT),
	};
	rng::UniformRealDistribution<float> launchTangentSpaceAccelerationZDistrubition{
		particleDescription.launchTangentSpaceAccelerationMin.getZ().in(phys::LinearAcceleration3D::UNIT),
		max(particleDescription.launchTangentSpaceAccelerationMin.getZ(), particleDescription.launchTangentSpaceAccelerationMax.getZ()).in(phys::LinearAcceleration3D::UNIT),
	};

	const uint16_t launchCount = launchCountDistribution(numberGenerator);
	for (uint16_t i = 0; i < launchCount; ++i) {
		const phys::Speed launchSpeed = launchSpeedDistribution(numberGenerator) * phys::Speed::UNIT;
		const phys::Angle launchSpreadAngle = launchSpreadAngleDistribution(numberGenerator) * phys::Angle::UNIT;
		const phys::Angle launchRollAngle = launchRollAngleDistribution(numberGenerator) * phys::RADIANS;
		const phys::AngularVelocity3D launchTangentSpaceAngularVelocity{
			launchTangentSpaceAngularVelocityXDistrubition(numberGenerator) * phys::AngularVelocity3D::UNIT,
			launchTangentSpaceAngularVelocityYDistrubition(numberGenerator) * phys::AngularVelocity3D::UNIT,
			launchTangentSpaceAngularVelocityZDistrubition(numberGenerator) * phys::AngularVelocity3D::UNIT,
		};
		const phys::LinearAcceleration3D launchTangentSpaceAcceleration{
			launchTangentSpaceAccelerationXDistrubition(numberGenerator) * phys::LinearAcceleration3D::UNIT,
			launchTangentSpaceAccelerationYDistrubition(numberGenerator) * phys::LinearAcceleration3D::UNIT,
			launchTangentSpaceAccelerationZDistrubition(numberGenerator) * phys::LinearAcceleration3D::UNIT,
		};
		const phys::Orientation3D launchOrientation = orientation * phys::Orientation3D::pitch(launchSpreadAngle) * phys::Orientation3D::roll(launchRollAngle);
		const phys::OrthonormalBasis3D launchTangentSpaceBasis = rotate(launchOrientation);

		spawnEntity(registry, resources, entityType, flags,
			ComponentInitializers{
				phys::Position3D{launchPosition},
				phys::Orientation3D{launchOrientation * particleDescription.localOrientation},
				phys::LinearVelocity3D{linearVelocity + launchTangentSpaceBasis[phys::Z] * -launchSpeed},
				phys::AngularVelocity3D{launchTangentSpaceBasis * launchTangentSpaceAngularVelocity},
				phys::LinearAcceleration3D{particleDescription.gravityAcceleration + launchTangentSpaceBasis * launchTangentSpaceAcceleration},
				ParticleType{particleType},
				ParticleState{.owner = *ownerSynchronizedEntityID},
				SpriteAnimationState{.animationStartTimestamp{tickIndex.getNext()}},
				DestroyCountdown{
					.destroyOnTickIndex = (isinf(particleDescription.maxLifetime)) ? TickIndex{}.getNext(Limits<TickCount>::MAX)
		                                                                           : Timestamp{tickIndex, particleDescription.maxLifetime, tickInterval}.getTickIndex()},
			});
	}
}

void killEntity(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [id, callback] : resources.getResource<EntityCallbacks>().onKill) {
		callback(registry, resources, entityID);
	}
	registry.destroyEntity(entityID);
}
