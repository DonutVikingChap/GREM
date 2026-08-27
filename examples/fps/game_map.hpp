// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_MAP_HPP
#define GREM_EXAMPLES_FPS_GAME_MAP_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics_3d/Fog3D.hpp>
#include <GREM/graphics_3d/LightBaker3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include "System.hpp"
#include "build_config.hpp"

struct MapInfo {
	struct Spawnpoint {
		phys::Position3D position{};
		phys::LinearVelocity3D velocity{};
		phys::PitchYaw aimAngles{};

		[[nodiscard]] bool operator==(const Spawnpoint&) const = default;
	};

	String name{};
	String skyImageFilepath{};
	String lightProbeVolumesIrradianceAtlasFilepath{};
	String lightProbeVolumesDistanceAtlasFilepath{};
	String reflectionProbesReflectionMapsFilepath{};
	phys::Box3D bounds{.min{-100_meters}, .max{100_meters}};
	ArrayList<Spawnpoint> spawnpoints{};
	gfx::Fog3DOptions fogOptions{};
	gfx::Sky3DOptions skyOptions{};
	ArrayList<gfx::LightProbeVolumeOptions3D> lightProbeVolumesVolumeOptions{};
	gfx::LightProbeVolumes3DOptions lightProbeVolumesOptions{};
	ArrayList<gfx::ReflectionProbeOptions3D> reflectionProbesProbeOptions{};
	gfx::ReflectionProbes3DOptions reflectionProbesOptions{};
	gfx::LightBaker3DOptions lightBakerOptions{};
	size_t lightBakerBounceCount = 2;
};

FPS_SHARED_API void loadMap(EntityRegistry& registry, ResourceRegistry& resources, CStringView mapFilepath);

#endif
