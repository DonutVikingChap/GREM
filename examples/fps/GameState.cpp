// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "GameState.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/graphics/Texture.hpp>

#include "AssetCache.hpp"
#include "GameSystems.hpp"
#include "System.hpp"

GameState::GameState(AssetCache& assetCache, Audio* audio, Graphics* graphics, const GameSystems& gameSystems, exec::Executor& executor)
	: systemAudio(audio)
	, systemGraphics(graphics)
	, gameSystems(gameSystems)
	, executor(executor) {
	resources.addExternalResource<AssetCache>(&assetCache);
}

GameState::~GameState() {
	for (auto&& [system, useCount] : systemUseCounts) {
		system->removeResources(resources, systemAudio, systemGraphics);
	}
}

void GameState::clearState() noexcept {
	systemsLayerStack.clear();
	currentSystemsLayerStackOffset = 0;
	previousSystemsLayerStackOffsets.clear();

	for (auto&& [system, useCount] : systemUseCounts) {
		system->removeResources(resources, systemAudio, systemGraphics);
	}
	systemUseCounts.clear();

	for (auto&& [clientsideResourceDescription, useCount] : clientsideResourceUseCounts) {
		clientsideResourceDescription->remove(resources);
	}
	clientsideResourceUseCounts.clear();

	for (auto&& [intermediateResourceDescription, useCount] : intermediateResourceUseCounts) {
		intermediateResourceDescription->remove(resources);
	}
	intermediateResourceUseCounts.clear();

	for (auto&& [stateResourceDescription, useCount] : stateResourceUseCounts) {
		stateResourceDescription->remove(resources);
	}
	stateResourceUseCounts.clear();

	updateCurrentPlayerSchedule = {};
	tickSchedule = {};

	rescheduleNecessary = false;
}

void GameState::setState(Span<const SystemsLayerType> newSystemsLayerTypes) {
	GREM_PROFILE_FUNCTION();

	systemsLayerStack.clear();
	currentSystemsLayerStackOffset = 0;
	previousSystemsLayerStackOffsets.clear();
	for (auto&& [stateResourceType, useCount] : stateResourceUseCounts) {
		useCount = 0;
	}
	for (auto&& [intermediateResourceType, useCount] : intermediateResourceUseCounts) {
		useCount = 0;
	}
	for (auto&& [clientsideResourceType, useCount] : clientsideResourceUseCounts) {
		useCount = 0;
	}
	for (auto&& [system, useCount] : systemUseCounts) {
		useCount = 0;
	}
	updateCurrentPlayerSchedule = {};
	tickSchedule = {};
	rescheduleNecessary = false;

	pushState(newSystemsLayerTypes);

	for (auto&& [system, useCount] : systemUseCounts) {
		if (useCount == 0) {
			system->removeResources(resources, systemAudio, systemGraphics);
		}
	}
	erase_if(systemUseCounts, [&](const auto& kv) -> bool { return kv.second == 0; });

	for (auto&& [clientsideResourceDescription, useCount] : clientsideResourceUseCounts) {
		if (useCount == 0) {
			clientsideResourceDescription->remove(resources);
		}
	}
	erase_if(clientsideResourceUseCounts, [&](const auto& kv) -> bool { return kv.second == 0; });

	for (auto&& [intermediateResourceDescription, useCount] : intermediateResourceUseCounts) {
		if (useCount == 0) {
			intermediateResourceDescription->remove(resources);
		}
	}
	erase_if(intermediateResourceUseCounts, [&](const auto& kv) -> bool { return kv.second == 0; });

	for (auto&& [stateResourceDescription, useCount] : stateResourceUseCounts) {
		if (useCount == 0) {
			stateResourceDescription->remove(resources);
		}
	}
	erase_if(stateResourceUseCounts, [&](const auto& kv) -> bool { return kv.second == 0; });
}

GameState::StackIndex GameState::pushState(Span<const SystemsLayerType> addedSystemsLayerTypes) {
	GREM_PROFILE_FUNCTION();

	previousSystemsLayerStackOffsets.push_back(currentSystemsLayerStackOffset);
	currentSystemsLayerStackOffset = getNextIndex();
	try {
		for (size_t addedLayerIndex = 0; addedLayerIndex < addedSystemsLayerTypes.size(); ++addedLayerIndex) {
			try {
				const SystemsLayerType systemsLayerType = addedSystemsLayerTypes[addedLayerIndex];
				const SystemsLayer& layer = gameSystems.getLayer(systemsLayerType);
				registerUses(stateResourceUseCounts, layer.stateResources, &GameState::getStateResourceKey, &GameState::addStateResource, &GameState::removeStateResource);
				try {
					registerUses(intermediateResourceUseCounts, layer.intermediateResources, &GameState::getIntermediateResourceKey, &GameState::addIntermediateResource,
						&GameState::removeIntermediateResource);
					try {
						registerUses(clientsideResourceUseCounts, layer.clientsideResources, &GameState::getClientsideResourceKey, &GameState::addClientsideResource,
							&GameState::removeClientsideResource);
						try {
							registerUses(systemUseCounts, layer.systemList, &GameState::getSystemKey, &GameState::addSystemResources, &GameState::removeSystemResources);
							try {
								systemsLayerStack.push_back(SystemsLayerStackEntry{
									.systemsLayerType = systemsLayerType,
									.systemList = layer.systemList,
								});
								rescheduleNecessary = true;
							} catch (...) {
								unregisterUses(systemUseCounts, layer.systemList, layer.systemList.size(), &GameState::getSystemKey, &GameState::removeSystemResources);
								throw;
							}
						} catch (...) {
							unregisterUses(clientsideResourceUseCounts, layer.clientsideResources, layer.clientsideResources.size(), &GameState::getClientsideResourceKey,
								&GameState::removeClientsideResource);
							throw;
						}
					} catch (...) {
						unregisterUses(intermediateResourceUseCounts, layer.intermediateResources, layer.intermediateResources.size(), &GameState::getIntermediateResourceKey,
							&GameState::removeIntermediateResource);
						throw;
					}
				} catch (...) {
					unregisterUses(stateResourceUseCounts, layer.stateResources, layer.stateResources.size(), &GameState::getStateResourceKey, &GameState::removeStateResource);
					throw;
				}
			} catch (...) {
				while (addedLayerIndex-- > 0) {
					const SystemsLayer& layer = gameSystems.getLayer(addedSystemsLayerTypes.back());
					unregisterUses(systemUseCounts, layer.systemList, layer.systemList.size(), &GameState::getSystemKey, &GameState::removeSystemResources);
					unregisterUses(clientsideResourceUseCounts, layer.clientsideResources, layer.clientsideResources.size(), &GameState::getClientsideResourceKey,
						&GameState::removeClientsideResource);
					unregisterUses(intermediateResourceUseCounts, layer.intermediateResources, layer.intermediateResources.size(), &GameState::getIntermediateResourceKey,
						&GameState::removeIntermediateResource);
					unregisterUses(stateResourceUseCounts, layer.stateResources, layer.stateResources.size(), &GameState::getStateResourceKey, &GameState::removeStateResource);
					systemsLayerStack.pop_back();
				}
				throw;
			}
		}
	} catch (...) {
		currentSystemsLayerStackOffset = previousSystemsLayerStackOffsets.back();
		previousSystemsLayerStackOffsets.pop_back();
		throw;
	}
	return currentSystemsLayerStackOffset;
}

void GameState::popState(StackIndex stackIndex) noexcept {
	GREM_PROFILE_FUNCTION();

	if (stackIndex == 0) {
		clearState();
		return;
	}

	while (currentSystemsLayerStackOffset >= stackIndex) {
		rescheduleNecessary = true;

		while (systemsLayerStack.size() > currentSystemsLayerStackOffset) {
			const SystemsLayer& layer = gameSystems.getLayer(systemsLayerStack.back().systemsLayerType);
			unregisterUses(systemUseCounts, layer.systemList, layer.systemList.size(), &GameState::getSystemKey, &GameState::removeSystemResources);
			unregisterUses(clientsideResourceUseCounts, layer.clientsideResources, layer.clientsideResources.size(), &GameState::getClientsideResourceKey,
				&GameState::removeClientsideResource);
			unregisterUses(intermediateResourceUseCounts, layer.intermediateResources, layer.intermediateResources.size(), &GameState::getIntermediateResourceKey,
				&GameState::removeIntermediateResource);
			unregisterUses(stateResourceUseCounts, layer.stateResources, layer.stateResources.size(), &GameState::getStateResourceKey, &GameState::removeStateResource);
			systemsLayerStack.pop_back();
		}

		if (previousSystemsLayerStackOffsets.empty()) {
			currentSystemsLayerStackOffset = 0;
		} else {
			currentSystemsLayerStackOffset = previousSystemsLayerStackOffsets.back();
			previousSystemsLayerStackOffsets.pop_back();
		}
	}
}

void GameState::reloadAssets() {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->reloadAssets(resources, systemAudio, systemGraphics);
		}
	}
}

void GameState::updateCurrentPlayer() {
	GREM_PROFILE_FUNCTION();

	rescheduleIfNecessary();
	executor.executeSchedule(updateCurrentPlayerSchedule, registry, resources);
}

void GameState::tick() {
	GREM_PROFILE_FUNCTION();

	rescheduleIfNecessary();
	executor.executeSchedule(tickSchedule, registry, resources);
}

void GameState::emitEvent(Audio& audio, Graphics& graphics, TickIndex tickIndex, const Event& event) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->emitEvent(audio, graphics, registry, resources, tickIndex, event);
		}
	}
}

void GameState::cancelEvent(Audio& audio, Graphics& graphics, TickIndex tickIndex, const Event& event) noexcept {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->cancelEvent(audio, graphics, registry, resources, tickIndex, event);
		}
	}
}

void GameState::stageAudio(Audio& audio, const WorldView& worldView) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->stageAudio(executor, audio, worldView);
		}
	}
}

void GameState::stage3DGraphicsSharedBetweenLocalPlayers(Graphics& graphics, const WorldView& worldView) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->stage3DGraphicsSharedBetweenLocalPlayers(executor, graphics, worldView);
		}
	}
}

void GameState::prepareToRender3DGraphics(Graphics& graphics, bool isSplitScreen) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->prepareToRender3DGraphics(graphics, isSplitScreen);
		}
	}
}

void GameState::stageLocalPlayer3DGraphics(Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const gfx::Viewport& viewport,
	const gfx::Camera3D& camera) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->stageLocalPlayer3DGraphics(executor, graphics, worldView, localPlayerID, viewport, camera);
		}
	}
}

void GameState::stageLocalPlayer2DGraphics(Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const Region2D& viewRegion) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->stageLocalPlayer2DGraphics(executor, graphics, worldView, localPlayerID, viewRegion);
		}
	}
}

void GameState::renderLocalPlayer3DGraphics(Graphics& graphics, bool isSplitScreen, const WorldView& worldView, const LocalPlayerID& localPlayerID, const gfx::Viewport& viewport,
	const gfx::Camera3D& camera) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->renderLocalPlayer3DGraphics(graphics, isSplitScreen, worldView, localPlayerID, viewport, camera);
		}
	}
}

void GameState::renderAudio(Audio& audio, const WorldView& worldView) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->renderAudio(audio, worldView);
		}
	}
}

void GameState::renderGraphics(Graphics& graphics, const WorldView& worldView, gfx::Texture* renderTargetOverride) {
	GREM_PROFILE_FUNCTION();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->renderGraphics(graphics, worldView, renderTargetOverride);
		}
	}
}

void GameState::rescheduleIfNecessary() {
	if (!rescheduleNecessary) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	Scheduler scheduler{};

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->scheduleCurrentPlayerUpdate(scheduler, resources, executor.getMaxParallelism());
		}
	}
	updateCurrentPlayerSchedule = scheduler.buildSchedule();

	for (const SystemsLayerStackEntry& systemsLayerStackEntry : Span{systemsLayerStack}.subspan(currentSystemsLayerStackOffset)) {
		for (System* const system : systemsLayerStackEntry.systemList) {
			system->scheduleTick(scheduler, resources, executor.getMaxParallelism());
		}
	}
	tickSchedule = scheduler.buildSchedule();

	rescheduleNecessary = false;
}
