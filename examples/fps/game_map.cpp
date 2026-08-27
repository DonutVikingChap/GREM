// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "game_map.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/physics/quantities.hpp>

#include "AssetCache.hpp"
#include "EntityType.hpp"
#include "Prefab.hpp"
#include "Schema.hpp"
#include "System.hpp"
#include "game_resources.hpp"

#include <sstream>   // std::istringstream
#include <stdexcept> // std::invalid_argument
#include <utility>   // std::move

void loadMap(EntityRegistry& registry, ResourceRegistry& resources, CStringView mapFilepath) {
	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load map {}", mapFilepath));

#if defined(NDEBUG) && !defined(__EMSCRIPTEN__)
	constexpr bool MAP_IS_DEBUG = false;
#else
	constexpr bool MAP_IS_DEBUG = true;
#endif

	try {
		ParseEntityComponentContext context{};

		AssetCache& assetCache = resources.getResource<AssetCache>();
		Schema& schema = resources.getResource<Schema>();
		MapInfo& mapInfo = resources.getResource<MapInfo>();
		MapState& mapState = resources.getResource<MapState>();

		String fileContents = assetCache.getFilesystem().readInputFileString(mapFilepath);
		try {
			const uint32_t mapCRC32 = static_cast<uint32_t>(CRC32{fileContents});
			std::istringstream stream{std::move(fileContents)};
			json::Reader reader{stream};
			reader.readCustomObject([&](const json::SourceLocation&, const json::String& mapKey) -> void {
				if (mapKey == "name") {
					mapInfo.name = reader.readString();
					if ((schema.getEntityFlags() & ENTITY_CLIENTSIDE) != 0) {
						eprintln("Client loading map \"{}\"...", mapInfo.name);
					} else {
						eprintln("Server loading map \"{}\"...", mapInfo.name);
					}
				} else if (mapKey == "fog") {
					parseValue(reader.readValue(), mapInfo.fogOptions);
				} else if (mapKey == "sky") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
						if (key == "image") {
							mapInfo.skyImageFilepath = reader.readString();
						} else {
							parseProperty(key, reader.readValue(), mapInfo.skyOptions);
						}
					});
				} else if (mapKey == "lightProbeVolumes") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& volumesKey) -> void {
						if (volumesKey == "volumes") {
							reader.readCustomArray([&](const json::SourceLocation& source) -> void {
								gfx::LightProbeVolumeOptions3D volumeOptions{
									.center{},
									.probeSpacing{1.0f, 2.0f, 1.0f},
									.probeCounts{16, 8, 16},
								};
								bool excluded = false;
								reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
									if (key == "filter") {
										const json::String filterName = reader.readString();
										if ((filterName == "NOT_DEBUG" && MAP_IS_DEBUG) || (filterName == "DEBUG" && !MAP_IS_DEBUG)) {
											excluded = true;
										}
									} else {
										parseProperty(key, reader.readValue(), volumeOptions);
									}
								});
								if (!excluded) {
									if (any(lessThanEqual(volumeOptions.probeSpacing, vec3{}))) {
										throw json::Error{"Invalid light probe volume probe spacing.", source};
									}
									if (any(lessThanEqual(volumeOptions.probeCounts, u32vec3{}))) {
										throw json::Error{"Invalid light probe volume probe counts.", source};
									}
									if (!isPowerOf2(volumeOptions.irradianceMapResolution) || volumeOptions.irradianceMapResolution < 4) {
										throw json::Error{"Invalid light probe volumes irradiance map resolution.", source};
									}
									if (!isPowerOf2(volumeOptions.distanceMapResolution) || volumeOptions.distanceMapResolution < 4) {
										throw json::Error{"Invalid light probe volumes distance map resolution.", source};
									}
									mapInfo.lightProbeVolumesVolumeOptions.push_back(volumeOptions);
								}
							});
						} else {
							parseProperty(volumesKey, reader.readValue(), mapInfo.lightProbeVolumesOptions);
						}
					});
				} else if (mapKey == "reflectionProbes") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
						if (key == "probes") {
							reader.readCustomArray([&](const json::SourceLocation& source) -> void {
								gfx::ReflectionProbeOptions3D probeOptions{
									.center{},
									.size{5.0f},
									.localAffectedRegionSize{5.0f},
									.blendWidthsOnNegativeSides{1.0f},
									.blendWidthsOnPositiveSides{1.0f},
								};
								bool excluded = false;
								bool localAffectedRegionSizeSpecified = false;
								reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
									if (key == "filter") {
										const json::String filterName = reader.readString();
										if ((filterName == "NOT_DEBUG" && MAP_IS_DEBUG) || (filterName == "DEBUG" && !MAP_IS_DEBUG)) {
											excluded = true;
										}
									} else {
										if (key == "localAffectedRegionSize") {
											localAffectedRegionSizeSpecified = true;
										}
										parseProperty(key, reader.readValue(), probeOptions);
									}
								});
								if (!excluded) {
									if (!localAffectedRegionSizeSpecified) {
										probeOptions.localAffectedRegionSize = probeOptions.size;
									}
									if (any(lessThanEqual(probeOptions.size, vec3{}))) {
										throw json::Error{"Invalid reflection probe size.", source};
									}
									if (any(lessThanEqual(probeOptions.localAffectedRegionSize, vec3{}))) {
										throw json::Error{"Invalid reflection probe local affected region size.", source};
									}
									if (any(lessThan(probeOptions.blendWidthsOnNegativeSides, vec3{}))) {
										throw json::Error{"Invalid reflection probe blend widths on negative sides.", source};
									}
									if (any(lessThan(probeOptions.blendWidthsOnPositiveSides, vec3{}))) {
										throw json::Error{"Invalid reflection probe blend widths on positive sides.", source};
									}
									mapInfo.reflectionProbesProbeOptions.push_back(probeOptions);
								}
							});
						} else {
							parseProperty(key, reader.readValue(), mapInfo.reflectionProbesOptions);
						}
					});
				} else if (mapKey == "lightBaker") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
						if (key == "bounceCount") {
							parseValue(reader.readValue(), mapInfo.lightBakerBounceCount);
						} else {
							parseProperty(key, reader.readValue(), mapInfo.lightBakerOptions);
						}
					});
				} else if (mapKey == "bounds") {
					reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
						if (key == "min") {
							mapInfo.bounds.min = parseVector<3, float>(reader.readValue()) * phys::METERS;
						} else if (key == "max") {
							mapInfo.bounds.max = parseVector<3, float>(reader.readValue()) * phys::METERS;
						}
					});
				} else if (mapKey == "playerRespawnTime") {
					mapState.playerRespawnTime = parseNumber<float>(reader.readValue()) * phys::SECONDS;
				} else if (mapKey == "spawnpoints") {
					reader.readCustomArray([&](const json::SourceLocation&) -> void {
						MapInfo::Spawnpoint spawnpoint{};
						parseValue(reader.readValue(), spawnpoint);
						mapInfo.spawnpoints.push_back(spawnpoint);
					});
				} else if (mapKey == "entities") {
					reader.readCustomArray([&](const json::SourceLocation& source) -> void {
						const json::Object object = reader.readObject();

						if (const auto it = object.find("filter"); it != object.end()) {
							const json::String filterName = it->second.getString();
							if (filterName == "NOT_DEBUG" && MAP_IS_DEBUG) {
								return;
							}
							if (filterName == "DEBUG" && !MAP_IS_DEBUG) {
								return;
							}
						}

						const json::String* idName = nullptr;
						if (const auto it = object.find("id"); it != object.end()) {
							idName = &it->second.getString();
						}

						size_t countX = 1;
						size_t countY = 1;
						size_t countZ = 1;
						if (const auto it = object.find("count"); it != object.end()) {
							if (it->second.isArray()) {
								parseEntityComponent(context, it->second.getItem(0), countX);
								parseEntityComponent(context, it->second.getItem(1), countY);
								parseEntityComponent(context, it->second.getItem(2), countZ);
							} else {
								parseEntityComponent(context, it->second, countX);
							}
						}

						const auto itPrefab = object.find("prefab");
						const auto itEntityType = object.find("EntityType");
						if (itPrefab == object.end() && itEntityType == object.end()) {
							throw json::Error{"Missing EntityType.", source};
						}

						if (itPrefab != object.end()) {
							if (itEntityType != object.end()) {
								throw json::Error{"Cannot override EntityType of prefab.", itEntityType->second.getSource()};
							}

							const SharedPointer<Prefab> prefab = assetCache.getPrefab(schema, itPrefab->second.getString());

							for (size_t z = 0; z < countZ; ++z) {
								for (size_t y = 0; y < countY; ++y) {
									for (size_t x = 0; x < countX; ++x) {
										context.stepIndex = {x, y, z};
										const auto [entityID, synchronizedEntityID] =
											prefab->spawn(registry, resources, context, EntityID::Flags{ENTITY_PART_OF_MAP}, object).front();
										if (idName) {
											context.namedEntities.emplace(*idName, synchronizedEntityID);
										}
									}
								}
							}
						} else if (itEntityType != object.end()) {
							const json::String& entityTypeName = itEntityType->second.getString();
							const EntityType entityType{CRC32{entityTypeName}};
							if (!schema.findEntityDescription(entityType)) {
								throw json::Error{formatString("Invalid entity type \"{}\".", entityTypeName), itEntityType->second.getSource()};
							}

							for (size_t z = 0; z < countZ; ++z) {
								for (size_t y = 0; y < countY; ++y) {
									for (size_t x = 0; x < countX; ++x) {
										context.stepIndex = {x, y, z};
										const auto [entityID, synchronizedEntityID] =
											spawnEntity(registry, resources, context, entityType, EntityID::Flags{ENTITY_PART_OF_MAP}, object);
										if (idName) {
											context.namedEntities.emplace(*idName, synchronizedEntityID);
										}
									}
								}
							}
						}
						context.stepIndex = {};
					});
				}
			});

			if (mapInfo.fogOptions.startDistance >= mapInfo.fogOptions.endDistance) {
				throw std::invalid_argument{"Invalid fog fade distances."};
			}
			if (mapInfo.skyOptions.radianceMapResolution != 0 && !isPowerOf2(mapInfo.skyOptions.radianceMapResolution)) {
				throw std::invalid_argument{"Invalid sky radiance map resolution."};
			}
			if (mapInfo.skyOptions.irradianceMapResolution != 0 && !isPowerOf2(mapInfo.skyOptions.irradianceMapResolution)) {
				throw std::invalid_argument{"Invalid sky irradiance map resolution."};
			}
			if (mapInfo.skyOptions.reflectionMapResolution != 0 && !isPowerOf2(mapInfo.skyOptions.reflectionMapResolution)) {
				throw std::invalid_argument{"Invalid sky reflection map resolution."};
			}
			if (mapInfo.lightProbeVolumesVolumeOptions.size() > size_t{Limits<uint32_t>::MAX}) {
				throw std::invalid_argument{"Invalid light probe volume count."};
			}
			if (mapInfo.reflectionProbesProbeOptions.size() > size_t{Limits<uint32_t>::MAX}) {
				throw std::invalid_argument{"Invalid reflection probe count."};
			}
			if (!isPowerOf2(mapInfo.reflectionProbesOptions.reflectionMapResolution)) {
				throw std::invalid_argument{"Invalid reflection probes reflection map resolution."};
			}
			if (mapInfo.lightBakerOptions.skyIrradianceSampleCount == 0) {
				throw std::invalid_argument{"Invalid sky irradiance sample count."};
			}
			if (mapInfo.lightBakerOptions.skyReflectionSampleCount == 0) {
				throw std::invalid_argument{"Invalid sky reflection sample count."};
			}
			if (!isPowerOf2(mapInfo.lightBakerOptions.lightProbeRenderResolution)) {
				throw std::invalid_argument{"Invalid light probe render resolution."};
			}
			if (mapInfo.lightBakerOptions.lightProbeNearZ > mapInfo.lightBakerOptions.lightProbeFarZ) {
				throw std::invalid_argument{"Invalid light probe near/far planes."};
			}
			if (mapInfo.lightBakerOptions.lightProbeIrradianceSampleCount == 0) {
				throw std::invalid_argument{"Invalid light probe irradiance sample count."};
			}
			if (mapInfo.lightBakerOptions.lightProbeDistanceSampleCount == 0) {
				throw std::invalid_argument{"Invalid light probe distance sample count."};
			}
			if (!isPowerOf2(mapInfo.lightBakerOptions.reflectionProbeRenderResolution)) {
				throw std::invalid_argument{"Invalid reflection probe render resolution."};
			}
			if (mapInfo.lightBakerOptions.reflectionProbeNearZ > mapInfo.lightBakerOptions.reflectionProbeFarZ) {
				throw std::invalid_argument{"Invalid reflection probe near/far planes."};
			}
			if (mapInfo.lightBakerOptions.reflectionProbeReflectionSampleCount == 0) {
				throw std::invalid_argument{"Invalid reflection probe reflection sample count."};
			}

			if constexpr (MAP_IS_DEBUG) {
				mapInfo.lightProbeVolumesIrradianceAtlasFilepath = formatString("{}.{:08X}.lightProbeVolumesIrradianceAtlas.debug.ktx2", mapFilepath, mapCRC32);
				mapInfo.lightProbeVolumesDistanceAtlasFilepath = formatString("{}.{:08X}.lightProbeVolumesDistanceAtlas.debug.ktx2", mapFilepath, mapCRC32);
				mapInfo.reflectionProbesReflectionMapsFilepath = formatString("{}.{:08X}.reflectionProbesReflectionMaps.debug.ktx2", mapFilepath, mapCRC32);
			} else {
				mapInfo.lightProbeVolumesIrradianceAtlasFilepath = formatString("{}.{:08X}.lightProbeVolumesIrradianceAtlas.ktx2", mapFilepath, mapCRC32);
				mapInfo.lightProbeVolumesDistanceAtlasFilepath = formatString("{}.{:08X}.lightProbeVolumesDistanceAtlas.ktx2", mapFilepath, mapCRC32);
				mapInfo.reflectionProbesReflectionMapsFilepath = formatString("{}.{:08X}.reflectionProbesReflectionMaps.ktx2", mapFilepath, mapCRC32);
			}
		} catch (...) {
			Error::throwWithNestedFilepath(mapFilepath);
		}
	} catch (...) {
		Error::throwWithNested(Error{"Failed to load map."});
	}
}
