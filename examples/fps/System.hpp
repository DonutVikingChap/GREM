// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SYSTEM_HPP
#define GREM_EXAMPLES_FPS_SYSTEM_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/execution/EntityRegistry.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/ResourceRegistry.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Scheduler.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/execution/component_pool.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/physics/Simulation.hpp>

#include "Timestamp.hpp"

struct Audio;
struct Graphics;
struct Event;
struct WorldView;
struct LocalPlayerID;

using exec::ComponentPool;
using exec::Entities;
using exec::EntityID;
using exec::Exclude;
using EntityRegistry = phys::EntityRegistry3D;
using EntityBuilder = phys::EntityBuilder3D;
using ResourceRegistry = phys::ResourceRegistry3D;
using Scheduler = phys::Scheduler3D;
using Schedule = phys::Schedule3D;

class System {
public:
	System() noexcept = default;
	System(const System&) = delete;
	System(System&&) = delete;
	System& operator=(const System&) = delete;
	System& operator=(System&&) = delete;

	virtual ~System() = default;

	virtual void addRequiredResources(ResourceRegistry& resources, Audio* audio, Graphics* graphics, exec::Task::ParallelCount parallelism) {
		(void)resources;
		(void)audio;
		(void)graphics;
		(void)parallelism;
	}

	virtual void removeResources(ResourceRegistry& resources, Audio* audio, Graphics* graphics) noexcept {
		(void)resources;
		(void)audio;
		(void)graphics;
	}

	virtual void reloadAssets(ResourceRegistry& resources, Audio* audio, Graphics* graphics) {
		(void)resources;
		(void)audio;
		(void)graphics;
	}

	virtual void scheduleCurrentPlayerUpdate(Scheduler& scheduler, const ResourceRegistry& resources, exec::Task::ParallelCount parallelism) {
		(void)scheduler;
		(void)resources;
		(void)parallelism;
	}

	virtual void scheduleTick(Scheduler& scheduler, const ResourceRegistry& resources, exec::Task::ParallelCount parallelism) {
		(void)scheduler;
		(void)resources;
		(void)parallelism;
	}

	virtual void emitEvent(Audio& audio, Graphics& graphics, EntityRegistry& registry, ResourceRegistry& resources, TickIndex tickIndex, const Event& event) {
		(void)audio;
		(void)graphics;
		(void)registry;
		(void)resources;
		(void)tickIndex;
		(void)event;
	}

	virtual void cancelEvent(Audio& audio, Graphics& graphics, EntityRegistry& registry, ResourceRegistry& resources, TickIndex tickIndex, const Event& event) noexcept {
		(void)audio;
		(void)graphics;
		(void)registry;
		(void)resources;
		(void)tickIndex;
		(void)event;
	}

	virtual void stageAudio(exec::Executor& executor, Audio& audio, const WorldView& worldView) {
		(void)executor;
		(void)audio;
		(void)worldView;
	}

	virtual void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor& executor, Graphics& graphics, const WorldView& worldView) {
		(void)executor;
		(void)graphics;
		(void)worldView;
	}

	virtual void prepareToRender3DGraphics(Graphics& graphics, bool isSplitScreen) {
		(void)graphics;
		(void)isSplitScreen;
	}

	virtual void stageLocalPlayer3DGraphics(exec::Executor& executor, Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID,
		const gfx::Viewport& viewport, const gfx::Camera3D& camera) {
		(void)executor;
		(void)graphics;
		(void)worldView;
		(void)localPlayerID;
		(void)viewport;
		(void)camera;
	}

	virtual void stageLocalPlayer2DGraphics(exec::Executor& executor, Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID,
		const Region2D& viewRegion) {
		(void)executor;
		(void)graphics;
		(void)worldView;
		(void)localPlayerID;
		(void)viewRegion;
	}

	virtual void renderLocalPlayerAudio(Audio& audio, const WorldView& worldView, const LocalPlayerID& localPlayerID) {
		(void)audio;
		(void)worldView;
		(void)localPlayerID;
	}

	virtual void renderLocalPlayer3DGraphics(Graphics& graphics, bool isSplitScreen, const WorldView& worldView, const LocalPlayerID& localPlayerID, const gfx::Viewport& viewport,
		const gfx::Camera3D& camera) {
		(void)graphics;
		(void)isSplitScreen;
		(void)worldView;
		(void)localPlayerID;
		(void)viewport;
		(void)camera;
	}

	virtual void renderAudio(Audio& audio, const WorldView& worldView) {
		(void)audio;
		(void)worldView;
	}

	virtual void renderGraphics(Graphics& graphics, const WorldView& worldView, gfx::Texture* renderTargetOverride) {
		(void)graphics;
		(void)worldView;
		(void)renderTargetOverride;
	}
};

#endif
