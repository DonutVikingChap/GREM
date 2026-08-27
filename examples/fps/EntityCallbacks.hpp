// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_ENTITY_CALLBACKS_HPP
#define GREM_EXAMPLES_FPS_ENTITY_CALLBACKS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Registry.hpp>

#include "System.hpp"

struct EntityCallbacks {
	struct SpawnCallbackID : RegistryElementIDBase<SpawnCallbackID> {
		using RegistryElementIDBase::RegistryElementIDBase;
	};

	using SpawnCallback = void (*)(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID);

	struct KillCallbackID : RegistryElementIDBase<KillCallbackID> {
		using RegistryElementIDBase::RegistryElementIDBase;
	};

	using KillCallback = void (*)(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID);

	Registry<SpawnCallback, SpawnCallbackID> onSpawn{};
	Registry<KillCallback, KillCallbackID> onKill{};
};

#endif
