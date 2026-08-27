// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "game_events.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/data/Optional.hpp>

#include "GameState.hpp"
#include "PlayerEntityMap.hpp"
#include "System.hpp"

Optional<Event> PlayerAssociatedWorldSpaceSoundPlayedEvent::filter(const GameState&, PlayerID playerID) const {
	if (associatedPlayerID == playerID) {
		return SelfAssociatedWorldSpaceSoundPlayedEvent{.soundType = soundType, .position = position};
	}
	return WorldSpaceSoundPlayedEvent{.soundType = soundType, .position = position};
}

Optional<Event> EntityParentedSoundPlayedEvent::filter(const GameState& gameState, PlayerID playerID) const {
	const EntityRegistry& registry = gameState.getRegistry();
	const ResourceRegistry& resources = gameState.getResources();

	if (resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, [&](EntityID entityID) -> bool {
			if (const SynchronizedEntityID* const synchronizedEntityID = registry.findComponent<SynchronizedEntityID>(entityID)) {
				if (emitter == *synchronizedEntityID) {
					return true;
				}
			}
			return false;
		})) {
		return SelfParentedSoundPlayedEvent{.soundType = soundType};
	}
	return *this;
}
