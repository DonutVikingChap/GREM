// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_RESOURCES_HPP
#define GREM_EXAMPLES_FPS_GAME_RESOURCES_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/resource.hpp>
#include <GREM/physics/quantities.hpp>

#include "Flags.hpp"

template <typename Resource>
struct ResourceTypeDeclaration {
	CStringView name;
};

//==============================================================================
// State resources:
//==============================================================================

struct SessionState {
	enum Flag : uint8_t {
		PAUSED = 1 << 0,
	};

	Flags<Flag> flags{};

	[[nodiscard]] bool operator==(const SessionState&) const = default;
};

struct MapState {
	phys::Time playerRespawnTime = 5_seconds;
	size_t nextSpawnpointIndex = 0;

	[[nodiscard]] bool operator==(const MapState&) const = default;
};

constexpr Tuple VALID_STATE_RESOURCE_TYPES{
	ResourceTypeDeclaration<SessionState>{"SessionState"},
	ResourceTypeDeclaration<MapState>{"MapState"},
};

//==============================================================================
// Intermediate resources:
//==============================================================================

struct TickPerformanceStats {
	Duration latestPhysicsTime{};
};

constexpr Tuple VALID_INTERMEDIATE_RESOURCE_TYPES{
	ResourceTypeDeclaration<TickPerformanceStats>{"TickPerformanceStats"},
};

//==============================================================================
// Clientside resources:
//==============================================================================

constexpr Tuple VALID_CLIENTSIDE_RESOURCE_TYPES{};

//==============================================================================

static_assert(
	[]() -> bool {
		constexpr size_t TOTAL_RESOURCE_TYPE_COUNT =                    //
			tuple_size_v<decltype(VALID_STATE_RESOURCE_TYPES)> +        //
			tuple_size_v<decltype(VALID_INTERMEDIATE_RESOURCE_TYPES)> + //
			tuple_size_v<decltype(VALID_CLIENTSIDE_RESOURCE_TYPES)>;
		Array<CRC32, TOTAL_RESOURCE_TYPE_COUNT> allResourceNameCRC32s{};
		size_t index = 0;
		const auto addNameCRC32 = [&](const auto& validResource) -> void {
			allResourceNameCRC32s[index++] = CRC32{validResource.name};
		};
		meta::forEach(VALID_STATE_RESOURCE_TYPES, addNameCRC32);
		meta::forEach(VALID_INTERMEDIATE_RESOURCE_TYPES, addNameCRC32);
		meta::forEach(VALID_CLIENTSIDE_RESOURCE_TYPES, addNameCRC32);
		sort(allResourceNameCRC32s, [&](const CRC32& a, const CRC32& b) -> bool { return static_cast<uint32_t>(a) < static_cast<uint32_t>(b); });
		return adjacentFind(allResourceNameCRC32s) == allResourceNameCRC32s.end();
	}(),
	"All resource name CRC32 hashes must be unique.");

template <exec::resource T>
constexpr CStringView RESOURCE_NAME = []() -> CStringView {
	CStringView result{};
	meta::forEach(VALID_STATE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if constexpr (same_as<Resource, T>) {
			result = validResource.name;
		}
	});
	meta::forEach(VALID_INTERMEDIATE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if constexpr (same_as<Resource, T>) {
			result = validResource.name;
		}
	});
	meta::forEach(VALID_CLIENTSIDE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if constexpr (same_as<Resource, T>) {
			result = validResource.name;
		}
	});
	return result;
}();

template <exec::resource T>
constexpr CRC32 RESOURCE_NAME_CRC32{RESOURCE_NAME<T>};

#endif
