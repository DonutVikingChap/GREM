// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_PLAYER_ENTITY_MAP_HPP
#define GREM_EXAMPLES_FPS_PLAYER_ENTITY_MAP_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/OrderedMultimap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>

#include "System.hpp"

#include <cstddef>    // std::size_t
#include <functional> // std::hash

struct PlayerID {
	using value_type = uint16_t;

	value_type value = 0;

	constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	[[nodiscard]] constexpr bool operator==(const PlayerID& other) const noexcept = default;
	[[nodiscard]] constexpr auto operator<=>(const PlayerID& other) const noexcept = default;

	void parseValueFrom(const json::Value& jsonValue) {
		if (const json::Number* const number = jsonValue.get_if<json::Number>();
			number && trunc(*number) == *number && *number >= 0 && *number <= json::Number{Limits<value_type>::MAX}) {
			value = static_cast<value_type>(*number);
		} else {
			throw json::Error{"Expected a string or a non-negative integer.", jsonValue.getSource()};
		}
	}

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(value);
	}
};

template <>
struct std::hash<PlayerID> {
	[[nodiscard]] std::size_t operator()(const PlayerID& type) const {
		return hasher(type.value);
	}

private:
	[[no_unique_address]] std::hash<PlayerID::value_type> hasher;
};

struct LocalPlayerID {
	using value_type = uint8_t;

	value_type value = 0;

	constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	[[nodiscard]] constexpr bool operator==(const LocalPlayerID& other) const noexcept = default;
	[[nodiscard]] constexpr auto operator<=>(const LocalPlayerID& other) const noexcept = default;

	void parseValueFrom(const json::Value& jsonValue) {
		if (const json::Number* const number = jsonValue.get_if<json::Number>();
			number && trunc(*number) == *number && *number >= 0 && *number <= json::Number{Limits<value_type>::MAX}) {
			value = static_cast<value_type>(*number);
		} else {
			throw json::Error{"Expected a string or a non-negative integer.", jsonValue.getSource()};
		}
	}

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(value);
	}
};

template <>
struct std::hash<LocalPlayerID> {
	[[nodiscard]] std::size_t operator()(const LocalPlayerID& type) const {
		return hasher(type.value);
	}

private:
	[[no_unique_address]] std::hash<LocalPlayerID::value_type> hasher;
};

inline constexpr size_t MAX_LOCAL_PLAYER_COUNT = size_t{Limits<LocalPlayerID::value_type>::MAX};

struct PlayerEntityMap {
	ArrayList<Pair<PlayerID, Pair<LocalPlayerID, EntityID>>> temporaryPlayerEntityIDValues{};
	OrderedMultimap<PlayerID, Pair<LocalPlayerID, EntityID>> playerEntityIDs{};

	auto forEachPlayerEntity(PlayerID playerID, auto callback) const {
		constexpr bool CALLBACK_RETURNS_BOOL = convertible_to<decltype(callback(EntityID{})), bool>;

		const auto [first, last] = playerEntityIDs.equal_range(playerID);
		for (const auto& [playerID_, kv] : Subrange{first, last}) {
			if constexpr (CALLBACK_RETURNS_BOOL) {
				if (callback(kv.second)) {
					return true;
				}
			} else {
				callback(kv.second);
			}
		}
		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

	auto forEachPlayerEntity(PlayerID playerID, LocalPlayerID localPlayerID, auto callback) const {
		constexpr bool CALLBACK_RETURNS_BOOL = convertible_to<decltype(callback(EntityID{})), bool>;

		const auto [first, last] = playerEntityIDs.equal_range(playerID);
		for (const auto& [playerID_, kv] : Subrange{first, last}) {
			if (kv.first == localPlayerID) {
				if constexpr (CALLBACK_RETURNS_BOOL) {
					if (callback(kv.second)) {
						return true;
					}
				} else {
					callback(kv.second);
				}
			}
		}
		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

	void update(const EntityRegistry& registry) {
		temporaryPlayerEntityIDValues.clear();
		for (const auto& [entityID, playerID, localPlayerID] : registry.getEntities<const PlayerID, const LocalPlayerID>()) {
			temporaryPlayerEntityIDValues.emplace_back(playerID, Pair{localPlayerID, entityID});
		}
		playerEntityIDs.clear();
		playerEntityIDs.insert_range(temporaryPlayerEntityIDValues);
	}
};

#endif
