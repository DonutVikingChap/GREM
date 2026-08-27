// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "Snapshot.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Simulation.hpp>

#include "AssetCache.hpp"
#include "EntityType.hpp"
#include "GameState.hpp"
#include "GameSystems.hpp"
#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "game_resources.hpp"
#include "serialization.hpp"

void saveSnapshot(Snapshot& snapshot, const GameState& gameState, PlayerID playerID) {
	GREM_PROFILE_FUNCTION();

	const EntityRegistry& registry = gameState.getRegistry();
	const ResourceRegistry& resources = gameState.getResources();

	(void)playerID; // TODO: If playerID is not null, cull any entities that the specified player cannot perceive (with some margin to avoid pop-in).

	snapshot.tickIndex = resources.getResource<TickIndex>();

	// TODO: Store minimal state in snapshots instead of copying the whole registry.
	// The current approach works fine for now, since the assignment operator reuses the existing memory which at least avoids reallocation,
	// but it's still suboptimal due to the sheer amount of data being copied when the world has thousands of objects.
	// In theory, the snapshots only need to store the same information that the server sends (plus some extra visual stuff on the client),
	// but then SnapshotView also needs an efficient way to perform random access into that data (by entity ID and component type) for interpolation.
	// That's not a particularly difficult data structure to implement, but reusing the same EntityRegistry structure as the main world state made it easier to get something working fast (it's hard to beat 2 lines of code!).
	snapshot.registry = registry;
	snapshot.resources = resources;
}

void loadSnapshot(GameState& gameState, const Snapshot& snapshot) {
	GREM_PROFILE_FUNCTION();

	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	GREM_ASSERT(&resources.getResource<AssetCache>() == &snapshot.resources.getResource<AssetCache>());
	GREM_ASSERT(snapshot.tickIndex == snapshot.resources.getResource<TickIndex>());

	// TODO: Store minimal state in snapshots instead of copying the whole registry (see comment in saveSnapshot() above).
	registry = snapshot.registry;
	resources = snapshot.resources;
}

void buildSnapshotDelta(SnapshotDelta& delta, const Snapshot& oldSnapshot, const Snapshot& newSnapshot) {
	GREM_PROFILE_FUNCTION();

	delta.oldTickIndex = oldSnapshot.tickIndex;
	delta.newTickIndex = newSnapshot.tickIndex;

	const SynchronizedEntityMap& oldSynchronizedEntityMap = oldSnapshot.resources.getResource<SynchronizedEntityMap>();
	const SynchronizedEntityMap& newSynchronizedEntityMap = newSnapshot.resources.getResource<SynchronizedEntityMap>();
	const Schema& schema = newSnapshot.resources.getResource<Schema>();

	delta.addedResources.clear();
	delta.updatedResources.clear();
	delta.removedResources.clear();
	meta::forEach(VALID_STATE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>&) -> void {
		if (const Resource* const newResource = newSnapshot.resources.findResource<Resource>()) {
			if (const Resource* const oldResource = oldSnapshot.resources.findResource<Resource>()) {
				if (*newResource != *oldResource) {
					serialize(StateResourceType{RESOURCE_NAME_CRC32<Resource>}, delta.updatedResources);
					serialize(*newResource, delta.updatedResources);
				}
			} else {
				serialize(StateResourceType{RESOURCE_NAME_CRC32<Resource>}, delta.addedResources);
				serialize(*newResource, delta.addedResources);
			}
		} else if (oldSnapshot.resources.hasResource<Resource>()) {
			serialize(StateResourceType{RESOURCE_NAME_CRC32<Resource>}, delta.removedResources);
		}
	});

	delta.spawnedEntities.clear();
	delta.updatedEntities.clear();
	GREM_ASSERT(isSorted(newSynchronizedEntityMap.synchronizedEntityMappings, [&](const auto& a, const auto& b) -> bool { return a.first < b.first; }));
	for (const auto& [synchronizedEntityID, synchronizedEntity] : newSynchronizedEntityMap.synchronizedEntityMappings) {
		const EntityID entityID = synchronizedEntity.id;
		const EntityType entityType = synchronizedEntity.type;
		const EntityDescription& entityDescription = schema.getEntityDescription(entityType);
		if (const auto it = oldSynchronizedEntityMap.synchronizedEntityMappings.find(synchronizedEntityID); it != oldSynchronizedEntityMap.synchronizedEntityMappings.end()) {
			GREM_ASSERT(it->second.type == entityType);
			const EntityID oldEntityID = it->second.id;
			const size_t componentCount = entityDescription.stateComponents.size();
			uint64_t mask = 0;
			for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
				const StateComponentDescription& stateComponentDescription = entityDescription.stateComponents[componentIndex];
				if (stateComponentDescription.hasStateDelta(oldSnapshot.registry, oldEntityID, newSnapshot.registry, entityID)) {
					mask |= uint64_t{1} << componentIndex;
				}
			}
			if (mask != 0) {
				Writer{delta.updatedEntities}.writeUIntLEB128(synchronizedEntityID.value);
				if (componentCount <= 8) {
					serialize(static_cast<uint8_t>(mask), delta.updatedEntities);
				} else if (componentCount <= 16) {
					serialize(static_cast<uint16_t>(mask), delta.updatedEntities);
				} else if (componentCount <= 32) {
					serialize(static_cast<uint32_t>(mask), delta.updatedEntities);
				} else {
					serialize(static_cast<uint64_t>(mask), delta.updatedEntities);
				}
				for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
					const StateComponentDescription& stateComponentDescription = entityDescription.stateComponents[componentIndex];
					if ((mask & (uint64_t{1} << componentIndex)) != 0) {
						stateComponentDescription.serializeState(newSnapshot.registry, entityID, delta.updatedEntities);
					}
				}
			}
		} else {
			const EntityID::Flags flags = entityID.getFlags();
			Writer{delta.spawnedEntities}.writeUIntLEB128(synchronizedEntityID.value);
			serialize(entityType, delta.spawnedEntities);
			serialize(flags, delta.spawnedEntities);
			for (const StateComponentDescription& stateComponentDescription : entityDescription.stateComponents) {
				stateComponentDescription.serializeState(newSnapshot.registry, entityID, delta.spawnedEntities);
			}
		}
	}

	delta.destroyedEntities.clear();
	for (const auto& [synchronizedEntityID, synchronizedEntity] : oldSynchronizedEntityMap.synchronizedEntityMappings) {
		if (!newSynchronizedEntityMap.synchronizedEntityMappings.contains(synchronizedEntityID)) {
			Writer{delta.destroyedEntities}.writeUIntLEB128(synchronizedEntityID.value);
		}
	}
}

void applySnapshotDelta(Snapshot& snapshot, const SnapshotDelta& delta, const GameSystems& gameSystems) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(delta.newTickIndex > TickIndex{});

	//eprintln("\nApplying snapshot delta {} -> {}...", delta.oldTickIndex - TickIndex{}, delta.newTickIndex - TickIndex{});

	snapshot.resources.getResource<TickIndex>() = delta.newTickIndex;
	snapshot.tickIndex = delta.newTickIndex;

	SynchronizedEntityMap& synchronizedEntityMap = snapshot.resources.getResource<SynchronizedEntityMap>();
	const Schema& schema = snapshot.resources.getResource<Schema>();

	//eprint("  Added resources:");
	Span<const byte> addedResources = delta.addedResources;
	while (!addedResources.empty()) {
		StateResourceType stateResourceType{};
		if (!deserialize(stateResourceType, addedResources)) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {}", stateResourceType.toJSON().toString());
		const StateResourceDescription& stateResourceDescription = gameSystems.getStateResourceDescription(stateResourceType);
		stateResourceDescription.add(snapshot.resources);
		stateResourceDescription.deserializeState(snapshot.resources, addedResources);
	}

	//eprint("\n  Updated resources:");
	Span<const byte> updatedResources = delta.updatedResources;
	while (!updatedResources.empty()) {
		StateResourceType stateResourceType{};
		if (!deserialize(stateResourceType, updatedResources)) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {}", stateResourceType.toJSON().toString());
		const StateResourceDescription& stateResourceDescription = gameSystems.getStateResourceDescription(stateResourceType);
		stateResourceDescription.deserializeState(snapshot.resources, updatedResources);
	}

	//eprint("\n  Removed resources:");
	Span<const byte> removedResources = delta.removedResources;
	while (!removedResources.empty()) {
		StateResourceType stateResourceType{};
		if (!deserialize(stateResourceType, removedResources)) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {}", stateResourceType.toJSON().toString());
		const StateResourceDescription& stateResourceDescription = gameSystems.getStateResourceDescription(stateResourceType);
		stateResourceDescription.remove(snapshot.resources);
	}

	//eprint("\n  Spawned entities:");
	Span<const byte> spawnedEntities = delta.spawnedEntities;
	while (!spawnedEntities.empty()) {
		const Optional<uint64_t> synchronizedEntityIDValue = SpanReader{spawnedEntities}.tryReadUIntLEB128();
		EntityType entityType{};
		EntityID::Flags flags{};
		if (!synchronizedEntityIDValue || !deserialize(entityType, spawnedEntities) || !deserialize(flags, spawnedEntities)) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {:>3}", *synchronizedEntityIDValue);
		if (*synchronizedEntityIDValue < synchronizedEntityMap.nextSynchronizedEntityID.value) {
			throw Error{"Snapshot delta tried to create an old entity."};
		}
		synchronizedEntityMap.nextSynchronizedEntityID = {.value = *synchronizedEntityIDValue};
		spawnEntity(snapshot.registry, snapshot.resources, entityType, flags, spawnedEntities);
	}

	//eprint("\n  Updated entities:");
	Span<const byte> updatedEntities = delta.updatedEntities;
	while (!updatedEntities.empty()) {
		const Optional<uint64_t> synchronizedEntityIDValue = SpanReader{updatedEntities}.tryReadUIntLEB128();
		if (!synchronizedEntityIDValue) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {:>3}", *synchronizedEntityIDValue);

		const auto itEntityMapping = synchronizedEntityMap.synchronizedEntityMappings.find(SynchronizedEntityID{.value = *synchronizedEntityIDValue});
		if (itEntityMapping == synchronizedEntityMap.synchronizedEntityMappings.end() || !snapshot.registry.containsEntity(itEntityMapping->second.id)) {
			throw Error{"Snapshot delta tried to update a missing entity."};
		}

		const EntityID entityID = itEntityMapping->second.id;
		const EntityDescription& entityDescription = schema.getEntityDescription(itEntityMapping->second.type);
		//eprint(" ({})", entityDescription.name);

		const size_t componentCount = entityDescription.stateComponents.size();
		uint64_t mask = 0;
		if (componentCount <= 8) {
			uint8_t value{};
			if (!deserialize(value, updatedEntities)) {
				throw Error{"Invalid snapshot delta layout."};
			}
			mask = uint64_t{value};
		} else if (componentCount <= 16) {
			uint16_t value{};
			if (!deserialize(value, updatedEntities)) {
				throw Error{"Invalid snapshot delta layout."};
			}
			mask = uint64_t{value};
		} else if (componentCount <= 32) {
			uint32_t value{};
			if (!deserialize(value, updatedEntities)) {
				throw Error{"Invalid snapshot delta layout."};
			}
			mask = uint64_t{value};
		} else {
			uint64_t value{};
			if (!deserialize(value, updatedEntities)) {
				throw Error{"Invalid snapshot delta layout."};
			}
			mask = uint64_t{value};
		}
		bool invalidatedPhysicsObjectBounds = false;
		for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
			const StateComponentDescription& stateComponentDescription = entityDescription.stateComponents[componentIndex];
			if ((mask & (uint64_t{1} << componentIndex)) != 0) {
				stateComponentDescription.deserializeState(snapshot.registry, entityID, updatedEntities);
				invalidatedPhysicsObjectBounds |= stateComponentDescription.mutationInvalidatesPhysicsObjectBounds;
			}
		}
		if (invalidatedPhysicsObjectBounds && entityDescription.physicsObjectOptions) {
			phys::Simulation3D::updateObjectMomentOfInertiaTensor(snapshot.registry, snapshot.resources, entityID);
			phys::Simulation3D::updateObjectBounds(snapshot.registry, snapshot.resources, entityID);
		}
	}

	//eprint("\n  Destroyed entities:");
	Span<const byte> destroyedEntities = delta.destroyedEntities;
	while (!destroyedEntities.empty()) {
		const Optional<uint64_t> synchronizedEntityIDValue = SpanReader{destroyedEntities}.tryReadUIntLEB128();
		if (!synchronizedEntityIDValue) {
			throw Error{"Invalid snapshot delta layout."};
		}
		//eprint(" {:>3}", *synchronizedEntityIDValue);
		const EntityID entityID = synchronizedEntityMap.findEntity(snapshot.registry, SynchronizedEntityID{.value = *synchronizedEntityIDValue});
		if (!entityID) {
			throw Error{"Snapshot delta tried to destroy a missing entity."};
		}
		snapshot.registry.destroyEntity(entityID);
	}

	//eprintln("");
	synchronizedEntityMap.removeDestroyedEntities(snapshot.registry);
	snapshot.resources.getResource<PlayerEntityMap>().update(snapshot.registry);
}
