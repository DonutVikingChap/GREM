// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "GameSystems.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Filesystem.hpp>

#include "System.hpp"
#include "game_resources.hpp"

#include <sstream> // std::istringstream
#include <utility> // std::move

#ifdef GREM_SHARED_LIBRARY

#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/system/SharedLibrary.hpp>
#include <GREM/core/system/synchronization.hpp>

namespace {

[[nodiscard]] UniquePointer<System> loadSystem(const Filesystem& filesystem, const json::String& name) {
	static HashMap<json::String, SharedLibrary> loadedLibraries{};
	static Mutex loadedLibrariesMutex{};
	try {
		ScopedLock lock{loadedLibrariesMutex};
		SharedLibrary& sharedLibrary = loadedLibraries[name];
		if (!sharedLibrary.is_open()) {
			sharedLibrary.open(filesystem, formatString("systems/{}.{}", name, SharedLibrary::getLibraryFilenameExtension()));
		}
		return UniquePointer<System>{sharedLibrary.getSymbol<System*()>(formatString("ExampleFPS_create{}", name))()};
	} catch (...) {
		Error::throwWithNested(Error{formatString("Failed to load system \"{}\".", name)});
	}
}

} // namespace

#else

#include "game_systems/audio_staging/SoundAudioStagingSystem.cpp"
#include "game_systems/graphics_staging_2d/ClientStatisticsGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_2d/HUDGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_3d/DecalGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_3d/LightGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_3d/LightProbeDebugGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_3d/ModelGraphicsStagingSystem.cpp"
#include "game_systems/graphics_staging_3d/SpriteGraphicsStagingSystem.cpp"
#include "game_systems/light_baking/LightBakingSystem.cpp"
#include "game_systems/loading_screen_graphics_rendering/LoadingScreenGraphicsRenderingSystem.cpp"
#include "game_systems/simulation/DecalCleanupSystem.cpp"
#include "game_systems/simulation/DestroyCountdownSystem.cpp"
#include "game_systems/simulation/MapBoundsEnforcementSystem.cpp"
#include "game_systems/simulation/ModelJointCleanupSystem.cpp"
#include "game_systems/simulation/MovementControlSystem.cpp"
#include "game_systems/simulation/MovementGroundingSystem.cpp"
#include "game_systems/simulation/NPCControlSystem.cpp"
#include "game_systems/simulation/ParticleCollisionSystem.cpp"
#include "game_systems/simulation/PhysicsSimulationSystem.cpp"
#include "game_systems/simulation/PlayerRespawnSystem.cpp"
#include "game_systems/simulation/ProjectileHitDetectionSystem.cpp"
#include "game_systems/simulation/VelocityOrientationSystem.cpp"
#include "game_systems/simulation/WeaponDropSystem.cpp"
#include "game_systems/simulation/WeaponHandlingSystem.cpp"
#include "game_systems/world_view_audio_rendering/WorldViewAudioRenderingSystem.cpp"
#include "game_systems/world_view_graphics_rendering/WorldViewGraphicsRenderingSystem.cpp"

namespace {

template <typename System>
struct SystemTypeDeclaration {};

constexpr Tuple VALID_SYSTEM_TYPES{
	// game_systems/audio_staging:
	SystemTypeDeclaration<SoundAudioStagingSystem>{},

	// game_systems/graphics_staging_2d:
	SystemTypeDeclaration<ClientStatisticsGraphicsStagingSystem>{},
	SystemTypeDeclaration<HUDGraphicsStagingSystem>{},

	// game_systems/graphics_staging_3d:
	SystemTypeDeclaration<DecalGraphicsStagingSystem>{},
	SystemTypeDeclaration<LightGraphicsStagingSystem>{},
	SystemTypeDeclaration<LightProbeDebugGraphicsStagingSystem>{},
	SystemTypeDeclaration<ModelGraphicsStagingSystem>{},
	SystemTypeDeclaration<SpriteGraphicsStagingSystem>{},

	// game_systems/light_baking:
	SystemTypeDeclaration<LightBakingSystem>{},

	// game_systems/loading_screen_graphics_rendering:
	SystemTypeDeclaration<LoadingScreenGraphicsRenderingSystem>{},

	// game_systems/simulation:
	SystemTypeDeclaration<DecalCleanupSystem>{},
	SystemTypeDeclaration<DestroyCountdownSystem>{},
	SystemTypeDeclaration<MapBoundsEnforcementSystem>{},
	SystemTypeDeclaration<ModelJointCleanupSystem>{},
	SystemTypeDeclaration<MovementControlSystem>{},
	SystemTypeDeclaration<MovementGroundingSystem>{},
	SystemTypeDeclaration<NPCControlSystem>{},
	SystemTypeDeclaration<VelocityOrientationSystem>{},
	SystemTypeDeclaration<PhysicsSimulationSystem>{},
	SystemTypeDeclaration<ParticleCollisionSystem>{},
	SystemTypeDeclaration<PlayerRespawnSystem>{},
	SystemTypeDeclaration<ProjectileHitDetectionSystem>{},
	SystemTypeDeclaration<WeaponDropSystem>{},
	SystemTypeDeclaration<WeaponHandlingSystem>{},

	// game_systems/world_view_audio_rendering:
	SystemTypeDeclaration<WorldViewAudioRenderingSystem>{},

	// game_systems/world_view_graphics_rendering:
	SystemTypeDeclaration<WorldViewGraphicsRenderingSystem>{},
};

[[nodiscard]] UniquePointer<System> loadSystem(const Filesystem& filesystem, const json::String& name) {
	(void)filesystem;
	UniquePointer<System> result{};
	meta::forEach(VALID_SYSTEM_TYPES, [&]<typename System>(const SystemTypeDeclaration<System>&) -> void {
		if (!result && name == meta::unqualified_type_name_v<System>) {
			result = UniquePointer<System>::create();
		}
	});
	if (!result) {
		throw Error{formatString("Failed to load system \"{}\".", name)};
	}
	return result;
}

} // namespace

#endif

namespace {

[[nodiscard]] StateResourceDescription loadStateResourceDescription(CStringView name) {
	StateResourceDescription result{};
	bool found = false;
	meta::forEach(VALID_STATE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if (!found && name == validResource.name) {
			result = StateResourceDescription::create<Resource>(validResource.name);
			found = true;
		}
	});
	if (!found) {
		throw Error{formatString("Failed to load state resource description \"{}\".", name)};
	}
	return result;
}

[[nodiscard]] IntermediateResourceDescription loadIntermediateResourceDescription(CStringView name) {
	IntermediateResourceDescription result{};
	bool found = false;
	meta::forEach(VALID_INTERMEDIATE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if (!found && name == validResource.name) {
			result = IntermediateResourceDescription::create<Resource>(validResource.name);
			found = true;
		}
	});
	if (!found) {
		throw Error{formatString("Failed to load intermediate resource description \"{}\".", name)};
	}
	return result;
}

[[nodiscard]] ClientsideResourceDescription loadClientsideResourceDescription(CStringView name) {
	ClientsideResourceDescription result{};
	bool found = false;
	meta::forEach(VALID_CLIENTSIDE_RESOURCE_TYPES, [&]<typename Resource>(const ResourceTypeDeclaration<Resource>& validResource) -> void {
		if (!found && name == validResource.name) {
			result = ClientsideResourceDescription::create<Resource>(validResource.name);
			found = true;
		}
	});
	if (!found) {
		throw Error{formatString("Failed to load clientside resource description \"{}\".", name)};
	}
	return result;
}

} // namespace

GameSystems::GameSystems(const Filesystem& filesystem, CStringView filepath) {
	GREM_PROFILE_FUNCTION();

	String fileContents = filesystem.readInputFileString(filepath);
	try {
		std::istringstream stream{std::move(fileContents)};
		json::Reader reader{stream};
		reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
			if (key == "layers") {
				reader.readCustomObject([&](const json::SourceLocation& source, const json::String& key) -> void {
					const SystemsLayerType systemsLayerType{CRC32{key}};
					SystemsLayer layer{};
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
						if (key == "resources") {
							reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
								if (key == "state") {
									reader.readCustomArray([&](const json::SourceLocation&) -> void {
										const json::String name = reader.readString();
										const StateResourceType stateResourceType{CRC32{name}};
										const auto [it, inserted] = stateResourceDescriptions.emplace(stateResourceType, StateResourceDescription{});
										if (inserted) {
											try {
												it->second = loadStateResourceDescription(name);
											} catch (...) {
												stateResourceDescriptions.erase(it);
												throw;
											}
										}
										layer.stateResources.push_back(stateResourceType);
									});
								} else if (key == "intermediate") {
									reader.readCustomArray([&](const json::SourceLocation&) -> void {
										const json::String name = reader.readString();
										const IntermediateResourceType intermediateResourceType{CRC32{name}};
										const auto [it, inserted] = intermediateResourceDescriptions.emplace(intermediateResourceType, IntermediateResourceDescription{});
										if (inserted) {
											try {
												it->second = loadIntermediateResourceDescription(name);
											} catch (...) {
												intermediateResourceDescriptions.erase(it);
												throw;
											}
										}
										layer.intermediateResources.push_back(intermediateResourceType);
									});
								} else if (key == "clientside") {
									reader.readCustomArray([&](const json::SourceLocation&) -> void {
										const json::String name = reader.readString();
										const ClientsideResourceType clientsideResourceType{CRC32{name}};
										const auto [it, inserted] = clientsideResourceDescriptions.emplace(clientsideResourceType, ClientsideResourceDescription{});
										if (inserted) {
											try {
												it->second = loadClientsideResourceDescription(name);
											} catch (...) {
												clientsideResourceDescriptions.erase(it);
												throw;
											}
										}
										layer.clientsideResources.push_back(clientsideResourceType);
									});
								}
							});
						} else if (key == "systems") {
							reader.readCustomArray([&](const json::SourceLocation&) -> void {
								const json::String name = reader.readString();
								const SystemType systemType{CRC32{name}};
								const auto [it, inserted] = systems.emplace(systemType, nullptr);
								if (inserted) {
									try {
										it->second = loadSystem(filesystem, name);
									} catch (...) {
										systems.erase(it);
										throw;
									}
								}
								layer.systemList.push_back(it->second.get());
							});
						}
					});
					if (const auto [it, inserted] = layers.emplace(systemsLayerType, std::move(layer)); !inserted) {
						throw json::Error{formatString("Hash collision detected with systems layer type name \"{}\".", it->first), source};
					}
				});
			}
		});
	} catch (...) {
		Error::throwWithNestedFilepath(filepath);
	}
}
