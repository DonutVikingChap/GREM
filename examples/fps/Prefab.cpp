// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "Prefab.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "AssetCache.hpp"
#include "Schema.hpp"
#include "SynchronizedEntityMap.hpp"
#include "game_components.hpp"

#include <stdexcept>   // std::invalid_argument
#include <type_traits> // std::remove_cvref_t
#include <utility>     // std::move, std::forward

struct PrefabEntity { // NOLINT(misc-use-internal-linkage)
	struct ID {
		SynchronizedEntityID* initializerAddress;
		SynchronizedEntityID relativeID;
	};

	EntityType entityType;
	Allocation<ComponentInitializer, ArenaAllocator<ComponentInitializer>> sortedComponentInitializers;
	Allocation<ID, ArenaAllocator<ID>> synchronizedEntityIDs;
};

struct PrefabDestructor { // NOLINT(misc-use-internal-linkage)
	void* value;
	void (*destroy)(void* value) noexcept;
};

struct PrefabImplementation { // NOLINT(misc-use-internal-linkage)
	String name{};
	Allocation<PrefabEntity, ArenaAllocator<PrefabEntity>> entities;
	ArrayList<PrefabDestructor> destructors{};
};

namespace {

void loadJSONPrefab(Arena<928>& arena, PrefabImplementation& prefab, AssetCache& assetCache, Schema& schema, CStringView filepath) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load JSON prefab {}", filepath));

#if defined(NDEBUG) && !defined(__EMSCRIPTEN__)
	constexpr bool PREFAB_IS_DEBUG = false;
#else
	constexpr bool PREFAB_IS_DEBUG = true;
#endif
	try {
		prefab.name = filepath;

		const json::Value prefabValue = json::Value::parse(assetCache.getFilesystem().readInputFileString(filepath));
		if (const json::Value* const name = prefabValue.findProperty("name")) {
			prefab.name = name->getString();
		}

		const json::Array& entitiesArray = prefabValue.getArrayProperty("entities");

		size_t entityIndex = 0;
		SynchronizedEntityIDAddresses synchronizedEntityIDAddresses{};
		ParseEntityComponentContext context{};
		for (const json::Value& entityValue : entitiesArray) {
			if (const json::Value* const groupName = entityValue.findProperty("group")) {
				if constexpr (PREFAB_IS_DEBUG) {
					if (groupName->getString() == "NOT_DEBUG") {
						continue;
					}
				} else {
					if (groupName->getString() == "DEBUG") {
						continue;
					}
				}
			}

			if (const json::Value* const entityName = entityValue.findProperty("id")) {
				context.namedEntities.emplace(entityName->getString(), SynchronizedEntityID{.value = entityIndex});
			}

			size_t countX = 1;
			size_t countY = 1;
			size_t countZ = 1;
			if (const json::Value* const count = entityValue.findProperty("count")) {
				if (count->isArray()) {
					parseEntityComponent(context, count->getItem(0), countX);
					parseEntityComponent(context, count->getItem(1), countY);
					parseEntityComponent(context, count->getItem(2), countZ);
				} else {
					parseEntityComponent(context, *count, countX);
				}
			}
			entityIndex += countX * countY * countZ;
		}

		if (entityIndex == 0) {
			throw std::invalid_argument{"No entities in prefab."};
		}

		prefab.entities.resize(entityIndex, PrefabEntity{.entityType{}, .sortedComponentInitializers{&arena}, .synchronizedEntityIDs{&arena}});
		entityIndex = 0;
		for (const json::Value& entityValue : entitiesArray) {
			if (const json::Value* const groupName = entityValue.findProperty("group")) {
				if constexpr (PREFAB_IS_DEBUG) {
					if (groupName->getString() == "NOT_DEBUG") {
						continue;
					}
				} else {
					if (groupName->getString() == "DEBUG") {
						continue;
					}
				}
			}

			size_t countX = 1;
			size_t countY = 1;
			size_t countZ = 1;
			if (const json::Value* const count = entityValue.findProperty("count")) {
				if (count->isArray()) {
					parseEntityComponent(context, count->getItem(0), countX);
					parseEntityComponent(context, count->getItem(1), countY);
					parseEntityComponent(context, count->getItem(2), countZ);
				} else {
					parseEntityComponent(context, *count, countX);
				}
			}

			const json::String& entityTypeName = entityValue.getStringProperty("EntityType");
			const EntityType entityType{CRC32{entityTypeName}};
			const EntityDescription* const entityDescription = schema.findEntityDescription(entityType);
			if (!entityDescription) {
				throw std::invalid_argument{formatString("Invalid entity type \"{}\".", entityTypeName)};
			}

			for (size_t z = 0; z < countZ; ++z) {
				for (size_t y = 0; y < countY; ++y) {
					for (size_t x = 0; x < countX; ++x) {
						context.stepIndex = {x, y, z};

						InplaceArrayList<ComponentInitializer, tuple_size_v<decltype(VALID_STATE_COMPONENT_TYPES)>> componentInitializers{};
						synchronizedEntityIDAddresses.clear();
						for (const StateComponentDescription& stateComponentDescription : entityDescription->stateComponents) {
							if (void* const value = stateComponentDescription.constructInPrefab(synchronizedEntityIDAddresses, &arena, context, schema, entityValue.getObject(),
									stateComponentDescription.name)) {
								try {
									prefab.destructors.push_back(PrefabDestructor{.value = value, .destroy = stateComponentDescription.destroyInPrefab});
								} catch (...) {
									stateComponentDescription.destroyInPrefab(value);
									throw;
								}
								componentInitializers.push_back(ComponentInitializer{.componentNameCRC32 = stateComponentDescription.nameCRC32, .value = value});
							}
						}
						sort(componentInitializers, ComponentInitializer::Compare{});

						PrefabEntity& entity = prefab.entities[entityIndex];
						entity.entityType = entityType;
						entity.sortedComponentInitializers.assign_range(componentInitializers);
						entity.synchronizedEntityIDs.resize(synchronizedEntityIDAddresses.size());
						for (size_t i = 0; i < synchronizedEntityIDAddresses.size(); ++i) {
							PrefabEntity::ID& id = entity.synchronizedEntityIDs[i];
							SynchronizedEntityID* const initializerAddress = synchronizedEntityIDAddresses[i];
							id.initializerAddress = initializerAddress;
							id.relativeID = *initializerAddress;
						}
						++entityIndex;
					}
				}
			}
			context.stepIndex = {};
		}
	} catch (...) {
		Error::throwWithNested(Error{formatString("Failed to load prefab \"{}\".", filepath)});
	}
}

void loadModelPrefab(Arena<928>& arena, PrefabImplementation& prefab, AssetCache& assetCache, Schema& schema, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	const ModelDescription& modelDescription = schema.loadModelDescription(assetCache.getFilesystem(), modelType);
	try {
		const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);
		GREM_PROFILE_BLOCK_DYNAMIC(formatString("Model {}", modelDescription.filepath));

		prefab.name = modelDescription.filepath;

		size_t entityIndex = 1;
		Allocation<size_t> physicsObjectEntityIndices(modelObjectDescription.physicsObjectDescriptions.size(), size_t{0});
		for (res::Model::PhysicsObjectIndex physicsObjectIndex = 0; physicsObjectIndex < modelObjectDescription.physicsObjectDescriptions.size(); ++physicsObjectIndex) {
			if (modelObjectDescription.physicsObjectDescriptions[physicsObjectIndex].jointIndex == 0) {
				continue;
			}
			physicsObjectEntityIndices[physicsObjectIndex] = entityIndex++;
		}
		entityIndex += modelObjectDescription.physicsJointDescriptions.size();
		entityIndex += modelObjectDescription.lightDescriptions.size();

		prefab.entities.resize(entityIndex, PrefabEntity{.entityType{}, .sortedComponentInitializers{&arena}, .synchronizedEntityIDs{&arena}});

		entityIndex = 0;
		SynchronizedEntityIDAddresses synchronizedEntityIDAddresses{};
		InplaceArrayList<ComponentInitializer, tuple_size_v<decltype(VALID_STATE_COMPONENT_TYPES)>> componentInitializers{};

		const auto addEntityComponent = [&]<typename C>(C&& component) -> void {
			using T = std::remove_cvref_t<C>;
			constexpr CRC32 NAME_CRC32 = COMPONENT_NAME_CRC32<T>;
			static_assert(NAME_CRC32 != CRC32{}, "Invalid component type in initializer.");
			T* const result = new (ArenaAllocator<T>{&arena}.allocate(1)) T{std::forward<C>(component)}; // NOLINT(cppcoreguidelines-owning-memory)
			try {
				collectSynchronizedEntityIDAddresses(synchronizedEntityIDAddresses, *result);
				prefab.destructors.push_back(PrefabDestructor{.value = result, .destroy = [](void* value) noexcept -> void { static_cast<T*>(value)->~T(); }});
			} catch (...) {
				result->~T();
				throw;
			}
			componentInitializers.push_back(ComponentInitializer{.componentNameCRC32 = NAME_CRC32, .value = result});
		};

		const auto addEntity = [&](EntityType entityType) -> void {
			sort(componentInitializers, ComponentInitializer::Compare{});

			PrefabEntity& entity = prefab.entities[entityIndex];
			entity.entityType = entityType;
			entity.sortedComponentInitializers.assign_range(componentInitializers);
			entity.synchronizedEntityIDs.resize(synchronizedEntityIDAddresses.size());
			for (size_t i = 0; i < synchronizedEntityIDAddresses.size(); ++i) {
				PrefabEntity::ID& id = entity.synchronizedEntityIDs[i];
				SynchronizedEntityID* const initializerAddress = synchronizedEntityIDAddresses[i];
				id.initializerAddress = initializerAddress;
				id.relativeID = *initializerAddress;
			}
			++entityIndex;
			synchronizedEntityIDAddresses.clear();
			componentInitializers.clear();
		};

		for (const ModelObjectDescription::PhysicsObjectDescription& physicsObjectDescription : modelObjectDescription.physicsObjectDescriptions) {
			if (physicsObjectDescription.jointIndex == 0) {
				if (physicsObjectDescription.initialLocalOffset != 0) {
					addEntityComponent(phys::Position3D{physicsObjectDescription.initialLocalOffset});
				}
				if (physicsObjectDescription.initialLocalOrientation != phys::Orientation3D{}) {
					addEntityComponent(phys::Orientation3D{physicsObjectDescription.initialLocalOrientation});
				}
				if (physicsObjectDescription.initialLocalScale != phys::Scale3D{1_x}) {
					addEntityComponent(phys::Scale3D{physicsObjectDescription.initialLocalScale});
				}
				if (physicsObjectDescription.initialLinearVelocity != phys::LinearVelocity3D{}) {
					addEntityComponent(phys::LinearVelocity3D{physicsObjectDescription.initialLinearVelocity});
				}
				if (physicsObjectDescription.initialAngularVelocity != phys::AngularVelocity3D{}) {
					addEntityComponent(phys::AngularVelocity3D{physicsObjectDescription.initialAngularVelocity});
				}
				break;
			}
		}
		addEntityComponent(ModelType{modelType});
		addEntity(EntityType{"MODEL_OBJECT"});
		for (const ModelObjectDescription::PhysicsObjectDescription& physicsObjectDescription : modelObjectDescription.physicsObjectDescriptions) {
			if (physicsObjectDescription.jointIndex == 0) {
				continue;
			}
			componentInitializers.clear();
			synchronizedEntityIDAddresses.clear();
			if (physicsObjectDescription.initialLocalOffset != 0) {
				addEntityComponent(phys::Position3D{physicsObjectDescription.initialLocalOffset});
			}
			if (physicsObjectDescription.initialLocalOrientation != phys::Orientation3D{}) {
				addEntityComponent(phys::Orientation3D{physicsObjectDescription.initialLocalOrientation});
			}
			if (physicsObjectDescription.initialLocalScale != phys::Scale3D{1_x}) {
				addEntityComponent(phys::Scale3D{physicsObjectDescription.initialLocalScale});
			}
			if (physicsObjectDescription.initialLinearVelocity != phys::LinearVelocity3D{}) {
				addEntityComponent(phys::LinearVelocity3D{physicsObjectDescription.initialLinearVelocity});
			}
			if (physicsObjectDescription.initialAngularVelocity != phys::AngularVelocity3D{}) {
				addEntityComponent(phys::AngularVelocity3D{physicsObjectDescription.initialAngularVelocity});
			}
			addEntityComponent(ModelJointController{
				.target = SynchronizedEntityID{.value = 0},
				.jointIndex = physicsObjectDescription.jointIndex,
			});
			addEntity(EntityType{"MODEL_JOINT_OBJECT"});
		}
		for (const ModelObjectDescription::PhysicsJointDescription& physicsJointDescription : modelObjectDescription.physicsJointDescriptions) {
			addEntityComponent(JointConnectedObjects{
				.first = SynchronizedEntityID{.value = (physicsJointDescription.objectIndices.first == Limits<res::Model::PhysicsObjectIndex>::MAX)
			                                               ? 0
			                                               : physicsObjectEntityIndices[physicsJointDescription.objectIndices.first]},
				.second = SynchronizedEntityID{.value = (physicsJointDescription.objectIndices.second == Limits<res::Model::PhysicsObjectIndex>::MAX)
			                                                ? 0
			                                                : physicsObjectEntityIndices[physicsJointDescription.objectIndices.second]},
			});
			addEntityComponent(phys::GenericJointOptions3D{physicsJointDescription.genericJointOptions});
			addEntity(EntityType{"JOINT"});
		}
		for (res::Model::LightIndex lightIndex = 0; lightIndex < modelObjectDescription.lightDescriptions.size(); ++lightIndex) {
			addEntityComponent(ModelJointLight{
				.target = SynchronizedEntityID{.value = 0},
				.jointIndex = modelObjectDescription.lightDescriptions[lightIndex].jointIndex,
				.lightIndex = lightIndex,
			});
			addEntity(EntityType{"MODEL_JOINT_LIGHT"});
		}
	} catch (...) {
		Error::throwWithNested(Error{formatString("Failed to load prefab from model \"{}\".", modelDescription.filepath)});
	}
}

} // namespace

Prefab::Prefab(AssetCache& assetCache, Schema& schema, CStringView filepath) {
	GREM_PROFILE_FUNCTION();

	PrefabImplementation& prefab = *new (ArenaAllocator<PrefabImplementation>{&arena}.allocate(1)) PrefabImplementation{
		.entities{&arena},
	};
	implementation = &prefab;
	try {
		if (filepath.starts_with("models/")) {
			loadModelPrefab(arena, prefab, assetCache, schema, ModelType{CRC32{filepath}});
		} else {
			loadJSONPrefab(arena, prefab, assetCache, schema, filepath);
		}
	} catch (...) {
		for (const PrefabDestructor& destructor : prefab.destructors) {
			destructor.destroy(destructor.value);
		}
		prefab.~PrefabImplementation();
		throw;
	}
}

Prefab::Prefab(AssetCache& assetCache, Schema& schema, ModelType modelType) {
	GREM_PROFILE_FUNCTION();

	PrefabImplementation& prefab = *new (ArenaAllocator<PrefabImplementation>{&arena}.allocate(1)) PrefabImplementation{
		.entities{&arena},
	};
	implementation = &prefab;
	try {
		loadModelPrefab(arena, prefab, assetCache, schema, modelType);
	} catch (...) {
		for (const PrefabDestructor& destructor : prefab.destructors) {
			destructor.destroy(destructor.value);
		}
		prefab.~PrefabImplementation();
		throw;
	}
}

Prefab::~Prefab() {
	PrefabImplementation& prefab = *static_cast<PrefabImplementation*>(implementation);
	for (const PrefabDestructor& destructor : prefab.destructors) {
		destructor.destroy(destructor.value);
	}
	prefab.~PrefabImplementation();
}

StringView Prefab::getName() const noexcept {
	const PrefabImplementation& prefab = *static_cast<const PrefabImplementation*>(implementation);
	return prefab.name;
}

Prefab::SpawnedEntities Prefab::spawn(EntityRegistry& registry, ResourceRegistry& resources, EntityID::Flags flags, phys::Position3D position,
	phys::Orientation3D orientation) const {
	return spawn(registry, resources, flags, EntityInitializer{}, position, orientation);
}

Prefab::SpawnedEntities Prefab::spawn(EntityRegistry& registry, ResourceRegistry& resources, EntityID::Flags flags, const EntityInitializer& rootEntityInitializer,
	phys::Position3D position, phys::Orientation3D orientation) const {
	ParseEntityComponentContext context{};
	return spawn(registry, resources, context, flags, rootEntityInitializer, position, orientation);
}

Prefab::SpawnedEntities Prefab::spawn(EntityRegistry& registry, ResourceRegistry& resources, ParseEntityComponentContext& context, EntityID::Flags flags,
	const EntityInitializer& rootEntityInitializer, phys::Position3D position, phys::Orientation3D orientation) const {
	GREM_PROFILE_FUNCTION();

	const PrefabImplementation& prefab = *static_cast<const PrefabImplementation*>(implementation);
	const uint64_t synchronizedEntityIDBase = resources.getResource<SynchronizedEntityMap>().nextSynchronizedEntityID.value;
	SpawnedEntities result{};
	try {
		GREM_ASSERT(!prefab.entities.empty());
		for (const PrefabEntity::ID& id : prefab.entities.front().synchronizedEntityIDs) {
			*id.initializerAddress = SynchronizedEntityID{.value = synchronizedEntityIDBase + id.relativeID.value};
		}
		const auto [rootEntityID, rootSynchronizedEntityID] = spawnEntity(registry, resources, context, prefab.entities.front().entityType, flags,
			Span<const EntityInitializer>{rootEntityInitializer, ComponentInitializersView{.sortedInitializers = prefab.entities.front().sortedComponentInitializers}}, position,
			orientation);
		try {
			result.push_back(SpawnedEntity{rootEntityID, rootSynchronizedEntityID});
		} catch (...) {
			registry.destroyEntity(rootEntityID);
			throw;
		}

		for (const PrefabEntity& entity : Span{prefab.entities}.subspan(1)) {
			for (const PrefabEntity::ID& id : entity.synchronizedEntityIDs) {
				*id.initializerAddress = SynchronizedEntityID{.value = synchronizedEntityIDBase + id.relativeID.value};
			}
			const auto [entityID, synchronizedEntityID] = spawnEntity(registry, resources, context, entity.entityType, flags,
				ComponentInitializersView{.sortedInitializers = entity.sortedComponentInitializers}, position, orientation);
			try {
				result.push_back(SpawnedEntity{entityID, synchronizedEntityID});
			} catch (...) {
				registry.destroyEntity(entityID);
				throw;
			}
		}
	} catch (...) {
		for (const auto& [entityID, synchronizedEntityID] : result) {
			registry.destroyEntity(entityID);
		}
		Error::throwWithNested(Error{formatString("Failed to spawn prefab \"{}\".", prefab.name)});
	}
	return result;
}
