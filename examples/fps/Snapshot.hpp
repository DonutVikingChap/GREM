// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SNAPSHOT_HPP
#define GREM_EXAMPLES_FPS_SNAPSHOT_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/physics/quantities.hpp>

#include "PlayerEntityMap.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "build_config.hpp"

class GameState;
class GameSystems;

inline constexpr size_t SNAPSHOT_BUFFER_WINDOW_SIZE = 30;
inline constexpr size_t SNAPSHOT_BUFFER_WINDOW_MARGIN = 2;

struct SnapshotView {
	const EntityRegistry& registry;
	const ResourceRegistry& resources;
};

struct Snapshot {
	struct Compare {
		[[nodiscard]] bool operator()(const Snapshot& a, const Snapshot& b) const noexcept {
			return a.tickIndex < b.tickIndex;
		}

		[[nodiscard]] bool operator()(TickIndex a, const Snapshot& b) const noexcept {
			return a < b.tickIndex;
		}

		[[nodiscard]] bool operator()(const Snapshot& a, TickIndex b) const noexcept {
			return a.tickIndex < b;
		}
	};

	TickIndex tickIndex{};
	EntityRegistry registry{};
	ResourceRegistry resources{};

	operator SnapshotView() const noexcept {
		return SnapshotView{.registry = registry, .resources = resources};
	}
};

struct InterpolatedEntityView {
	SnapshotView snapshotA;
	SnapshotView snapshotB;
	float interpolationAlpha;
	Pair<EntityID> entityIDs;

	template <exec::component T>
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasComponent() const {
		const bool result = snapshotA.registry.hasComponent<T>(entityIDs.first);
		GREM_ASSERT(result == snapshotB.registry.hasComponent<T>(entityIDs.second));
		return result;
	}

	template <exec::component T>
	[[nodiscard]] GREM_ALWAYS_INLINE const T& getOldComponent() const {
		return snapshotA.registry.getComponent<T>(entityIDs.first);
	}

	template <exec::component T>
	[[nodiscard]] GREM_ALWAYS_INLINE const T& getNewComponent() const {
		return snapshotB.registry.getComponent<T>(entityIDs.second);
	}

	template <exec::component T>
	[[nodiscard]] GREM_ALWAYS_INLINE T getInterpolatedComponent() const {
		const T& oldComponent = getOldComponent<T>();
		const T& newComponent = getNewComponent<T>();
		return mix(oldComponent, newComponent, interpolationAlpha);
	}

	template <exec::component T>
	[[nodiscard]] GREM_ALWAYS_INLINE T getInterpolatedComponentWithMargin(auto margin) const {
		const T& oldComponent = getOldComponent<T>();
		const T& newComponent = getNewComponent<T>();
		if (distance(oldComponent, newComponent) <= margin) {
			return mix(oldComponent, newComponent, interpolationAlpha);
		}
		return newComponent;
	}

	template <auto Member>
	[[nodiscard]] GREM_ALWAYS_INLINE const auto& getOldAttribute() const {
		return snapshotA.registry.getComponent<meta::member_pointer_object_type_t<Member>>(entityIDs.first).*Member;
	}

	template <auto Member>
	[[nodiscard]] GREM_ALWAYS_INLINE const auto& getNewAttribute() const {
		return snapshotB.registry.getComponent<meta::member_pointer_object_type_t<Member>>(entityIDs.second).*Member;
	}

	template <auto Member>
	[[nodiscard]] GREM_ALWAYS_INLINE auto getInterpolatedAttribute() const {
		const auto& oldValue = getOldAttribute<Member>();
		const auto& newValue = getNewAttribute<Member>();
		return mix(oldValue, newValue, interpolationAlpha);
	}

	template <auto Member>
	[[nodiscard]] GREM_ALWAYS_INLINE auto getInterpolatedAttributeWithMargin(auto margin) const {
		const auto& oldValue = getOldAttribute<Member>();
		const auto& newValue = getNewAttribute<Member>();
		if (distance(oldValue, newValue) <= margin) {
			return mix(oldValue, newValue, interpolationAlpha);
		}
		return newValue;
	}
};

struct SnapshotInterpolationView {
	static constexpr phys::Distance TELEPORTATION_MARGIN = 5_meters;

	SnapshotView snapshotA;
	SnapshotView snapshotB;
	float interpolationAlpha;

	[[nodiscard]] Optional<InterpolatedEntityView> findEntity(SynchronizedEntityID synchronizedEntityID) const {
		const SynchronizedEntityMap& synchronizedEntityMapA = snapshotA.resources.getResource<SynchronizedEntityMap>();
		const SynchronizedEntityMap& synchronizedEntityMapB = snapshotB.resources.getResource<SynchronizedEntityMap>();
		if (const auto itB = synchronizedEntityMapB.synchronizedEntityMappings.find(synchronizedEntityID);
			itB != synchronizedEntityMapB.synchronizedEntityMappings.end() && snapshotB.registry.containsEntity(itB->second.id)) {
			GREM_ASSERT(snapshotB.registry.getComponent<EntityType>(itB->second.id) == itB->second.type);
			if (const auto itA = synchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
				itA != synchronizedEntityMapA.synchronizedEntityMappings.end() && snapshotA.registry.containsEntity(itA->second.id)) {
				GREM_ASSERT(snapshotA.registry.getComponent<EntityType>(itA->second.id) == itA->second.type);
				if (itA->second.type == itB->second.type) {
					return InterpolatedEntityView{
						.snapshotA = snapshotA,
						.snapshotB = snapshotB,
						.interpolationAlpha = interpolationAlpha,
						.entityIDs{itA->second.id, itB->second.id},
					};
				}
			}
			return InterpolatedEntityView{
				.snapshotA = snapshotB,
				.snapshotB = snapshotB,
				.interpolationAlpha = interpolationAlpha,
				.entityIDs{itB->second.id, itB->second.id},
			};
		}
		if (const auto itA = synchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
			itA != synchronizedEntityMapA.synchronizedEntityMappings.end() && snapshotA.registry.containsEntity(itA->second.id)) {
			GREM_ASSERT(snapshotA.registry.getComponent<EntityType>(itA->second.id) == itA->second.type);
			return InterpolatedEntityView{
				.snapshotA = snapshotA,
				.snapshotB = snapshotA,
				.interpolationAlpha = interpolationAlpha,
				.entityIDs{itA->second.id, itA->second.id},
			};
		}
		return {};
	}

	[[nodiscard]] Optional<InterpolatedEntityView> findEntity(SynchronizedEntityID synchronizedEntityID, EntityType entityType) const {
		const SynchronizedEntityMap& synchronizedEntityMapA = snapshotA.resources.getResource<SynchronizedEntityMap>();
		const SynchronizedEntityMap& synchronizedEntityMapB = snapshotB.resources.getResource<SynchronizedEntityMap>();
		if (const auto itB = synchronizedEntityMapB.synchronizedEntityMappings.find(synchronizedEntityID);
			itB != synchronizedEntityMapB.synchronizedEntityMappings.end() && itB->second.type == entityType && snapshotB.registry.containsEntity(itB->second.id)) {
			GREM_ASSERT(snapshotB.registry.getComponent<EntityType>(itB->second.id) == itB->second.type);
			if (const auto itA = synchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
				itA != synchronizedEntityMapA.synchronizedEntityMappings.end() && itA->second.type == entityType && snapshotA.registry.containsEntity(itA->second.id)) {
				GREM_ASSERT(snapshotA.registry.getComponent<EntityType>(itA->second.id) == itA->second.type);
				return InterpolatedEntityView{
					.snapshotA = snapshotA,
					.snapshotB = snapshotB,
					.interpolationAlpha = interpolationAlpha,
					.entityIDs{itA->second.id, itB->second.id},
				};
			}
			return InterpolatedEntityView{
				.snapshotA = snapshotB,
				.snapshotB = snapshotB,
				.interpolationAlpha = interpolationAlpha,
				.entityIDs{itB->second.id, itB->second.id},
			};
		}
		if (const auto itA = synchronizedEntityMapA.synchronizedEntityMappings.find(synchronizedEntityID);
			itA != synchronizedEntityMapA.synchronizedEntityMappings.end() && itA->second.type == entityType && snapshotA.registry.containsEntity(itA->second.id)) {
			GREM_ASSERT(snapshotA.registry.getComponent<EntityType>(itA->second.id) == itA->second.type);
			return InterpolatedEntityView{
				.snapshotA = snapshotA,
				.snapshotB = snapshotA,
				.interpolationAlpha = interpolationAlpha,
				.entityIDs{itA->second.id, itA->second.id},
			};
		}
		return {};
	}
};

struct SnapshotBufferView : Subrange<RingBuffer<Snapshot>::const_iterator, RingBuffer<Snapshot>::const_iterator, SubrangeKind::SIZED> {
	using Subrange::Subrange;

	[[nodiscard]] Optional<SnapshotInterpolationView> getInterpolationView(Timestamp timestamp, Duration tickInterval) const {
		GREM_ASSERT(isSorted(*this, Snapshot::Compare{}));

		if (empty()) {
			return {};
		}

		auto first = lowerBound(*this, timestamp.getTickIndex(), Snapshot::Compare{});
		if (first == end() || (first != begin() && first->tickIndex > timestamp.getTickIndex())) {
			--first;
		}
		auto last = first;
		while (last + 1 != end() && (last->tickIndex <= timestamp.getTickIndex() || first->tickIndex == last->tickIndex)) {
			++last;
		}
		while (first->tickIndex == last->tickIndex && first != begin()) {
			--first;
		}
		if (first->tickIndex == last->tickIndex) {
			return SnapshotInterpolationView{
				.snapshotA = *first,
				.snapshotB = *last,
				.interpolationAlpha = 0.0f,
			};
		}
		const Duration timeOffset = getTimeBetween(Timestamp{first->tickIndex}, timestamp, tickInterval);
		return SnapshotInterpolationView{
			.snapshotA = *first,
			.snapshotB = *last,
			.interpolationAlpha = duration_cast<FloatSeconds>(timeOffset) / duration_cast<FloatSeconds>(tickInterval * (last->tickIndex - first->tickIndex)),
		};
	}
};

struct SnapshotBuffer : RingBuffer<Snapshot> {
	[[nodiscard]] Optional<SnapshotInterpolationView> getInterpolationView(Timestamp timestamp, Duration tickInterval) const {
		return SnapshotBufferView{*this}.getInterpolationView(timestamp, tickInterval);
	}
};

struct SnapshotDelta {
	TickIndex oldTickIndex{};
	TickIndex newTickIndex{};

	// Layout: { StateResourceType stateResourceType; StateResource stateResource }... addedResources;
	Buffer<byte> addedResources{};

	// Layout: { StateResourceType stateResourceType; StateResource newStateResource }... updatedResources;
	Buffer<byte> updatedResources{};

	// Layout: { StateResourceType stateResourceType; }... removedResources;
	Buffer<byte> removedResources{};

	// Layout: { ULEB128 synchronizedEntityID; EntityType entityType; EntityID::Flags flags; StateComponents... stateComponents; }... spawnedEntities;
	Buffer<byte> spawnedEntities{};

	// Layout: { ULEB128 synchronizedEntityID; UInt<roundUpToMultiple(STATE_COMPONENT_COUNT, 8)> changedStateComponentMask; StateComponents... changedStateComponents; }... updatedEntities;
	Buffer<byte> updatedEntities{};

	// Layout: { ULEB128 synchronizedEntityID; }... destroyedEntities;
	Buffer<byte> destroyedEntities{};
};

FPS_SHARED_API void saveSnapshot(Snapshot& snapshot, const GameState& gameState, PlayerID playerID);
FPS_SHARED_API void loadSnapshot(GameState& gameState, const Snapshot& snapshot);

FPS_SHARED_API void buildSnapshotDelta(SnapshotDelta& delta, const Snapshot& oldSnapshot, const Snapshot& newSnapshot);
FPS_SHARED_API void applySnapshotDelta(Snapshot& snapshot, const SnapshotDelta& delta, const GameSystems& gameSystems);

#endif
