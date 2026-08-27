// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_PREFAB_HPP
#define GREM_EXAMPLES_FPS_PREFAB_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "Schema.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "build_config.hpp"

class AssetCache;
class EntityInitializer;
struct ParseEntityComponentContext;

class Prefab {
public:
	struct SpawnedEntity {
		EntityID entityID;
		SynchronizedEntityID synchronizedEntityID;
	};

	using SpawnedEntities = SmallArrayList<SpawnedEntity, 8>;

	FPS_SHARED_API Prefab(AssetCache& assetCache, Schema& schema, CStringView filepath);
	FPS_SHARED_API Prefab(AssetCache& assetCache, Schema& schema, ModelType modelType);
	FPS_SHARED_API ~Prefab();

	Prefab(const Prefab&) = delete;
	Prefab(Prefab&&) = delete;
	Prefab& operator=(const Prefab&) = delete;
	Prefab& operator=(Prefab&&) = delete;

	[[nodiscard]] FPS_SHARED_API StringView getName() const noexcept;

	FPS_SHARED_API SpawnedEntities spawn(EntityRegistry& registry, ResourceRegistry& resources, EntityID::Flags flags, phys::Position3D position = {},
		phys::Orientation3D orientation = {}) const;

	FPS_SHARED_API SpawnedEntities spawn(EntityRegistry& registry, ResourceRegistry& resources, EntityID::Flags flags, const EntityInitializer& rootEntityInitializer,
		phys::Position3D position = {}, phys::Orientation3D orientation = {}) const;

	FPS_SHARED_API SpawnedEntities spawn(EntityRegistry& registry, ResourceRegistry& resources, ParseEntityComponentContext& context, EntityID::Flags flags,
		const EntityInitializer& rootEntityInitializer, phys::Position3D position = {}, phys::Orientation3D orientation = {}) const;

private:
	void* implementation = nullptr;
	Arena<928> arena{};
};

#endif
