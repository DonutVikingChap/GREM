// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_EVENTS_HPP
#define GREM_EXAMPLES_FPS_GAME_EVENTS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/physics/quantities.hpp>

#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "SynchronizedEntityMap.hpp"
#include "build_config.hpp"

class GameState;
struct Event;

struct SoundPlayedEvent {
	SoundType soundType{};

	[[nodiscard]] bool operator==(const SoundPlayedEvent&) const = default;

	[[nodiscard]] bool isPredicted(PlayerID) const {
		return false;
	}
};

struct WorldSpaceSoundPlayedEvent {
	SoundType soundType{};
	phys::Position3D position{};

	[[nodiscard]] bool operator==(const WorldSpaceSoundPlayedEvent& other) const {
		return soundType == other.soundType && distance2(position, other.position) <= length2(0.5_meters);
	}

	[[nodiscard]] bool isPredicted(PlayerID) const {
		return false;
	}
};

struct PlayerAssociatedWorldSpaceSoundPlayedEvent {
	SoundType soundType{};
	phys::Position3D position{};
	PlayerID associatedPlayerID{};

	[[nodiscard]] bool operator==(const PlayerAssociatedWorldSpaceSoundPlayedEvent& other) const {
		return soundType == other.soundType && distance2(position, other.position) <= length2(0.5_meters) && associatedPlayerID == other.associatedPlayerID;
	}

	[[nodiscard]] bool isPredicted(PlayerID playerID) const {
		return playerID == associatedPlayerID;
	}

	[[nodiscard]] FPS_SHARED_API Optional<Event> filter(const GameState& gameState, PlayerID playerID) const;
};

struct SelfAssociatedWorldSpaceSoundPlayedEvent {
	SoundType soundType{};
	phys::Position3D position{};

	[[nodiscard]] bool operator==(const SelfAssociatedWorldSpaceSoundPlayedEvent& other) const {
		return soundType == other.soundType && distance2(position, other.position) <= length2(0.5_meters);
	}

	[[nodiscard]] bool isPredicted(PlayerID) const {
		return true;
	}
};

struct EntityParentedSoundPlayedEvent {
	SoundType soundType{};
	SynchronizedEntityID emitter{};

	[[nodiscard]] bool operator==(const EntityParentedSoundPlayedEvent&) const = default;

	[[nodiscard]] bool isPredicted(PlayerID) const {
		return false;
	}

	[[nodiscard]] FPS_SHARED_API Optional<Event> filter(const GameState& gameState, PlayerID playerID) const;
};

struct SelfParentedSoundPlayedEvent {
	SoundType soundType{};

	[[nodiscard]] bool operator==(const SelfParentedSoundPlayedEvent&) const = default;

	[[nodiscard]] bool isPredicted(PlayerID) const {
		return true;
	}
};

struct Event
	: Variant<                                        //
		  SoundPlayedEvent,                           //
		  WorldSpaceSoundPlayedEvent,                 //
		  PlayerAssociatedWorldSpaceSoundPlayedEvent, //
		  SelfAssociatedWorldSpaceSoundPlayedEvent,   //
		  EntityParentedSoundPlayedEvent,             //
		  SelfParentedSoundPlayedEvent> {
	using Variant::Variant;

	[[nodiscard]] bool operator==(const Event&) const = default;

	[[nodiscard]] bool isPredicted(PlayerID playerID) const {
		return match(*this)([&](const auto& event) -> bool {
			if constexpr (requires { event.isPredicted(playerID); }) {
				return event.isPredicted(playerID);
			} else {
				return true;
			}
		});
	}

	[[nodiscard]] Optional<Event> filter(const GameState& gameState, PlayerID playerID) const {
		return match(*this)([&](const auto& event) -> Optional<Event> {
			if constexpr (requires { event.filter(gameState, playerID); }) {
				return event.filter(gameState, playerID);
			} else {
				return event;
			}
		});
	}
};

using Events = ArrayList<Event>;

#endif
