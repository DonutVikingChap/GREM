// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEM_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEM_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct Graphics;
struct Audio;

struct System {
	virtual ~System() = default;

	virtual void addRequiredResources(phys::ResourceRegistry3D& resources, Graphics& graphics, Audio& audio, const Filesystem& filesystem) {
		(void)resources;
		(void)graphics;
		(void)audio;
		(void)filesystem;
	}

	virtual void removeResources(phys::ResourceRegistry3D& resources) noexcept {
		(void)resources;
	}

	virtual void scheduleUpdate(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D& resources, exec::Task::ParallelCount parallelism) {
		(void)scheduler;
		(void)resources;
		(void)parallelism;
	}

	virtual void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D& resources, exec::Task::ParallelCount parallelism) {
		(void)scheduler;
		(void)resources;
		(void)parallelism;
	}

	virtual void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D& resources, exec::Task::ParallelCount parallelism) {
		(void)scheduler;
		(void)resources;
		(void)parallelism;
	}

	virtual void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) {
		(void)graphics;
		(void)simulation;
	}
};

#endif
