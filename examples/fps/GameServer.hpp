// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_SERVER_HPP
#define GREM_EXAMPLES_FPS_GAME_SERVER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/physics/DebugVisualization.hpp>
#include <GREM/physics/quantities.hpp>

class AssetCache;
class GameState;
class GameSystems;

struct GameServerOptions {
	size_t maxClientCount = 20;
	size_t maxLocalPlayerCountPerClient = 9;
	size_t maxOutgoingDataRatePerClient = 125000;
	size_t maxSendSkipTicks = 3;
	phys::Frequency tickRate = 64_Hertz;
	phys::Frequency minFrameRate = 10_Hertz;
	Duration mapLoadGracePeriod = 10_seconds;
	Duration mapLoadTimeout = 5_minutes;
};

class GameServer {
public:
	struct PerformanceStats {
		Duration latestPhysicsTime{};
	};

	GameServer(AssetCache& assetCache, GameState& gameState, const net::Endpoint& endpoint, String schemaFilepath, String mapFilepath, const GameServerOptions& options);

	~GameServer();

	GameServer(const GameServer&) = delete;
	GameServer(GameServer&&) = delete;
	GameServer& operator=(const GameServer&) = delete;
	GameServer& operator=(GameServer&&) = delete;

	void shutdown();
	void reloadAssets();

	void update(size_t tickCount, phys::DebugVisualization3D* physicsDebugVisualization = nullptr);

	[[nodiscard]] bool isShuttingDown() const noexcept;
	[[nodiscard]] size_t getClientCount() const noexcept;
	[[nodiscard]] Duration getTickInterval() const noexcept;
	[[nodiscard]] const PerformanceStats& getPerformanceStats() const noexcept;

private:
	class Implementation;

	UniquePointer<Implementation> implementation;
};

#endif
