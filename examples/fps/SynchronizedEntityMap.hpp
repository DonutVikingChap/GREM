// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SYNCHRONIZED_ENTITY_MAP_HPP
#define GREM_EXAMPLES_FPS_SYNCHRONIZED_ENTITY_MAP_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>

#include "EntityType.hpp"
#include "System.hpp"

#include <cstddef>    // std::size_t
#include <functional> // std::hash

struct SynchronizedEntityID {
	uint64_t value = 0;

	constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	[[nodiscard]] constexpr bool operator==(const SynchronizedEntityID& other) const noexcept = default;
	[[nodiscard]] constexpr auto operator<=>(const SynchronizedEntityID& other) const noexcept = default;

	void parseValueFrom(const json::Value& jsonValue) {
		if (const json::Number* const number = jsonValue.get_if<json::Number>(); number && trunc(*number) == *number && *number >= 0) {
			value = static_cast<uint64_t>(*number);
		} else {
			throw json::Error{"Expected a string or a non-negative integer.", jsonValue.getSource()};
		}
	}

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(value);
	}
};

template <>
struct std::hash<SynchronizedEntityID> {
	[[nodiscard]] std::size_t operator()(const SynchronizedEntityID& type) const {
		return hasher(type.value);
	}

private:
	[[no_unique_address]] std::hash<uint64_t> hasher;
};

struct SynchronizedEntityMapping {
	EntityID id;
	EntityType type;
};

struct SynchronizedEntityMap {
	// Note: Must be OrderedMap for defined iteration order (currently required when building snapshot delta, etc.)!
	// Don't change to HashMap unless some other method of fast in-order iteration is also added.
	OrderedMap<SynchronizedEntityID, SynchronizedEntityMapping> synchronizedEntityMappings{};
	SynchronizedEntityID nextSynchronizedEntityID{.value = 1};

	[[nodiscard]] EntityID findEntity(const auto& entities, SynchronizedEntityID synchronizedEntityID) const noexcept {
		if (const auto it = synchronizedEntityMappings.find(synchronizedEntityID); it != synchronizedEntityMappings.end() && entities.containsEntity(it->second.id)) {
			if constexpr (convertible_to<decltype(entities), EntityRegistry>) {
				GREM_ASSERT(entities.template getComponent<EntityType>(it->second.id) == it->second.type);
			}
			return it->second.id;
		}
		return {};
	}

	[[nodiscard]] EntityID findEntity(const auto& entities, SynchronizedEntityID synchronizedEntityID, EntityType type) const noexcept {
		if (const auto it = synchronizedEntityMappings.find(synchronizedEntityID);
			it != synchronizedEntityMappings.end() && it->second.type == type && entities.containsEntity(it->second.id)) {
			if constexpr (convertible_to<decltype(entities), EntityRegistry>) {
				GREM_ASSERT(entities.template getComponent<EntityType>(it->second.id) == it->second.type);
			}
			return it->second.id;
		}
		return {};
	}

	void removeDestroyedEntities(const EntityRegistry& registry) {
		erase_if(synchronizedEntityMappings, [&](const auto& kv) -> bool { return !registry.containsEntity(kv.second.id); });
	}
};

#endif
