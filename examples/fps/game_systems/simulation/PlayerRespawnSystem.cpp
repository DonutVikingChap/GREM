// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../AssetCache.hpp"
#include "../../EntityCallbacks.hpp"
#include "../../PlayerEntityMap.hpp"
#include "../../Prefab.hpp"
#include "../../Schema.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"
#include "../../game_map.hpp"
#include "../../game_resources.hpp"

class PlayerRespawnSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics*, exec::Task::ParallelCount) override {
		resources.addSharedResource<PlayerRespawnPrefabs>(resources.getResource<AssetCache>(), resources.getResource<Schema>());
		resources.addSharedResource<PlayerRespawnCallbacks>(PlayerRespawnCallbacks{
			.killCallbackID = resources.getResource<EntityCallbacks>().onKill.insert(createDeadPlayerEntity)->first,
		});
		resources.addSharedResource<PlayersToRespawn>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.getResource<EntityCallbacks>().onKill.erase(resources.getResource<PlayerRespawnCallbacks>().killCallbackID);
		resources.removeResource<PlayersToRespawn>();
		resources.removeResource<PlayerRespawnCallbacks>();
		resources.removeResource<PlayerRespawnPrefabs>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics*) override {
		resources.getResource<PlayerRespawnPrefabs>() = PlayerRespawnPrefabs{resources.getResource<AssetCache>(), resources.getResource<Schema>()};
	}

	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<respawnPlayers>("Respawn players");
	}

private:
	struct PlayerRespawnPrefabs {
		SharedPointer<Prefab> playerPrefab;
		SharedPointer<Prefab> deadPlayerPrefab;

		PlayerRespawnPrefabs(AssetCache& assetCache, Schema& schema)
			: playerPrefab(assetCache.getPrefab(schema, schema.getPlayerPrefabFilepath()))
			, deadPlayerPrefab(assetCache.getPrefab(schema, schema.getDeadPlayerPrefabFilepath())) {}
	};

	struct PlayerRespawnCallbacks {
		EntityCallbacks::KillCallbackID killCallbackID;
	};

	struct PlayerToRespawn {
		PlayerID playerID;
		LocalPlayerID localPlayerID;
		EntityID entityID;
	};

	using PlayersToRespawn = ArrayList<PlayerToRespawn>;

	static void createDeadPlayerEntity(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID) {
		const PlayerID* const playerID = registry.findComponent<PlayerID>(entityID);
		if (!playerID) {
			return;
		}

		const LocalPlayerID* const localPlayerID = registry.findComponent<LocalPlayerID>(entityID);
		const phys::Position3D* const position = registry.findComponent<phys::Position3D>(entityID);
		const Aim* const aim = registry.findComponent<Aim>(entityID);
		if (!localPlayerID || !position || !aim) {
			return;
		}

		const PlayerRespawnPrefabs& playerRespawnPrefabs = resources.getResource<PlayerRespawnPrefabs>();
		const MapState& mapState = resources.getResource<MapState>();
		const TickIndex tickIndex = resources.getResource<TickIndex>();
		const Duration tickInterval = resources.getResource<Duration>();

		const Aim oldAim = *aim;
		const auto [newEntityID, newSynchronizedEntityID] =
			playerRespawnPrefabs.deadPlayerPrefab
				->spawn(registry, resources, entityID.getFlags(),
					ComponentInitializers{
						PlayerID{*playerID},
						LocalPlayerID{*localPlayerID},
						PlayerRespawnCountdown{.respawnOnTickIndex = Timestamp{tickIndex.getNext(), mapState.playerRespawnTime, tickInterval}.getTickIndex()},
					},
					phys::Position3D{*position})
				.front();
		if (Aim* const newAim = registry.findComponent<Aim>(newEntityID)) {
			*newAim = oldAim;
		}
	}

	static void respawnPlayers(EntityRegistry& registry, ResourceRegistry& resources, PlayerEntityMap& playerEntityMap, PlayersToRespawn& playersToRespawn, MapState& mapState,
		const MapInfo& mapInfo, const PlayerRespawnPrefabs& playerRespawnPrefabs, TickIndex tickIndex) {
		playersToRespawn.clear();
		for (const auto& [entityID, playerID, localPlayerID, playerRespawnCountdown] : registry.getEntities<const PlayerID, const LocalPlayerID, const PlayerRespawnCountdown>()) {
			if (tickIndex >= playerRespawnCountdown.respawnOnTickIndex) {
				playersToRespawn.push_back(PlayerToRespawn{.playerID = playerID, .localPlayerID = localPlayerID, .entityID = entityID});
			}
		}

		for (const PlayerToRespawn& playerToRespawn : playersToRespawn) {
			registry.destroyEntity(playerToRespawn.entityID);

			phys::Position3D position{};
			phys::PitchYaw aimAngles{};
			if (!mapInfo.spawnpoints.empty()) {
				const MapInfo::Spawnpoint& spawnpoint = mapInfo.spawnpoints[mapState.nextSpawnpointIndex++ % mapInfo.spawnpoints.size()];
				position = spawnpoint.position;
				aimAngles = spawnpoint.aimAngles;
			}

			const auto [newEntityID, newSynchronizedEntityID] =
				playerRespawnPrefabs.playerPrefab
					->spawn(registry, resources, playerToRespawn.entityID.getFlags(),
						ComponentInitializers{
							PlayerID{playerToRespawn.playerID},
							LocalPlayerID{playerToRespawn.localPlayerID},
						},
						position)
					.front();
			if (Aim* const newPlayerAim = registry.findComponent<Aim>(newEntityID)) {
				newPlayerAim->angles = aimAngles;
			}

			const auto [first, last] = playerEntityMap.playerEntityIDs.equal_range(playerToRespawn.playerID);
			for (auto it = first; it != last; ++it) {
				if (it->second.second == playerToRespawn.entityID) {
					playerEntityMap.playerEntityIDs.erase(it);
					break;
				}
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createPlayerRespawnSystem() { // NOLINT(misc-use-internal-linkage)
	return new PlayerRespawnSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
