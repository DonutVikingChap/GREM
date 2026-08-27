// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_STATE_HPP
#define GREM_EXAMPLES_FPS_GAME_STATE_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>

#include "GameSystems.hpp"
#include "System.hpp"
#include "build_config.hpp"

class AssetCache;
struct Audio;
struct Graphics;
struct Event;
struct WorldView;
struct LocalPlayerID;

class GameState {
public:
	using StackIndex = uint32_t;

	FPS_SHARED_API GameState(AssetCache& assetCache, Audio* audio, Graphics* graphics, const GameSystems& gameSystems, exec::Executor& executor);
	FPS_SHARED_API ~GameState();

	GameState(const GameState&) = delete;
	GameState(GameState&&) = delete;
	GameState& operator=(const GameState&) = delete;
	GameState& operator=(GameState&&) = delete;

	FPS_SHARED_API void clearState() noexcept;
	FPS_SHARED_API void setState(Span<const SystemsLayerType> newSystemsLayerTypes);
	FPS_SHARED_API StackIndex pushState(Span<const SystemsLayerType> addedSystemsLayerTypes);
	FPS_SHARED_API void popState(StackIndex stackIndex) noexcept;

	FPS_SHARED_API void reloadAssets();

	FPS_SHARED_API void updateCurrentPlayer();

	FPS_SHARED_API void tick();

	FPS_SHARED_API void emitEvent(Audio& audio, Graphics& graphics, TickIndex tickIndex, const Event& event);
	FPS_SHARED_API void cancelEvent(Audio& audio, Graphics& graphics, TickIndex tickIndex, const Event& event) noexcept;

	FPS_SHARED_API void stageAudio(Audio& audio, const WorldView& worldView);

	FPS_SHARED_API void stage3DGraphicsSharedBetweenLocalPlayers(Graphics& graphics, const WorldView& worldView);

	FPS_SHARED_API void prepareToRender3DGraphics(Graphics& graphics, bool isSplitScreen);

	FPS_SHARED_API void stageLocalPlayer3DGraphics(Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const gfx::Viewport& viewport,
		const gfx::Camera3D& camera);
	FPS_SHARED_API void stageLocalPlayer2DGraphics(Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const Region2D& viewRegion);

	FPS_SHARED_API void renderLocalPlayer3DGraphics(Graphics& graphics, bool isSplitScreen, const WorldView& worldView, const LocalPlayerID& localPlayerID,
		const gfx::Viewport& viewport, const gfx::Camera3D& camera);

	FPS_SHARED_API void renderAudio(Audio& audio, const WorldView& worldView);

	FPS_SHARED_API void renderGraphics(Graphics& graphics, const WorldView& worldView, gfx::Texture* renderTargetOverride);

	[[nodiscard]] EntityRegistry& getRegistry() noexcept {
		return registry;
	}

	[[nodiscard]] const EntityRegistry& getRegistry() const noexcept {
		return registry;
	}

	[[nodiscard]] ResourceRegistry& getResources() noexcept {
		return resources;
	}

	[[nodiscard]] const ResourceRegistry& getResources() const noexcept {
		return resources;
	}

	[[nodiscard]] StackIndex getNextIndex() const noexcept {
		return static_cast<StackIndex>(systemsLayerStack.size());
	}

private:
	struct SystemsLayerStackEntry {
		SystemsLayerType systemsLayerType;
		Span<System* const> systemList;
	};

	void unregisterUses(auto& useCounts, const auto& items, size_t end, auto getKey, auto remove) {
		for (size_t i = end; i-- > 0;) {
			const auto& key = (this->*getKey)(items[i]);
			const auto it = useCounts.find(key);
			if (it->second-- == 1) {
				(this->*remove)(key);
				useCounts.erase(it);
			}
		}
	}

	void registerUses(auto& useCounts, const auto& items, auto getKey, auto add, auto remove) {
		for (size_t i = 0; i < items.size(); ++i) {
			try {
				const auto& key = (this->*getKey)(items[i]);
				const auto [it, inserted] = useCounts.try_emplace(key, 0);
				if (inserted) {
					try {
						(this->*add)(key);
					} catch (...) {
						useCounts.erase(it);
						throw;
					}
				}
				++it->second;
			} catch (...) {
				unregisterUses(useCounts, items, i, getKey, remove);
				throw;
			}
		}
	}

	[[nodiscard]] const StateResourceDescription* getStateResourceKey(StateResourceType stateResourceType) const {
		return &gameSystems.getStateResourceDescription(stateResourceType);
	}

	void addStateResource(const StateResourceDescription* stateResourceDescription) {
		stateResourceDescription->add(resources);
	}

	void removeStateResource(const StateResourceDescription* stateResourceDescription) noexcept {
		stateResourceDescription->remove(resources);
	}

	[[nodiscard]] const IntermediateResourceDescription* getIntermediateResourceKey(IntermediateResourceType intermediateResourceType) const {
		return &gameSystems.getIntermediateResourceDescription(intermediateResourceType);
	}

	void addIntermediateResource(const IntermediateResourceDescription* intermediateResourceDescription) {
		intermediateResourceDescription->add(resources);
	}

	void removeIntermediateResource(const IntermediateResourceDescription* intermediateResourceDescription) noexcept {
		intermediateResourceDescription->remove(resources);
	}

	[[nodiscard]] const ClientsideResourceDescription* getClientsideResourceKey(ClientsideResourceType clientsideResourceType) const {
		return &gameSystems.getClientsideResourceDescription(clientsideResourceType);
	}

	void addClientsideResource(const ClientsideResourceDescription* clientsideResourceDescription) {
		clientsideResourceDescription->add(resources);
	}

	void removeClientsideResource(const ClientsideResourceDescription* clientsideResourceDescription) noexcept {
		clientsideResourceDescription->remove(resources);
	}

	[[nodiscard]] System* getSystemKey(System* system) const {
		return system;
	}

	void addSystemResources(System* system) {
		system->addRequiredResources(resources, systemAudio, systemGraphics, executor.getMaxParallelism());
	}

	void removeSystemResources(System* system) noexcept {
		system->removeResources(resources, systemAudio, systemGraphics);
	}

	FPS_SHARED_API void rescheduleIfNecessary();

	Audio* systemAudio;
	Graphics* systemGraphics;
	const GameSystems& gameSystems;
	exec::Executor& executor;
	EntityRegistry registry{};
	ResourceRegistry resources{};
	ArrayList<SystemsLayerStackEntry> systemsLayerStack{};
	StackIndex currentSystemsLayerStackOffset = 0;
	ArrayList<StackIndex> previousSystemsLayerStackOffsets{};
	HashMap<const StateResourceDescription*, size_t> stateResourceUseCounts{};
	HashMap<const IntermediateResourceDescription*, size_t> intermediateResourceUseCounts{};
	HashMap<const ClientsideResourceDescription*, size_t> clientsideResourceUseCounts{};
	HashMap<System*, size_t> systemUseCounts{};
	Schedule updateCurrentPlayerSchedule{};
	Schedule tickSchedule{};
	bool rescheduleNecessary = false;
};

#endif
