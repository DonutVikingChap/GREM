// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_CLIENT_HPP
#define GREM_EXAMPLES_FPS_GAME_CLIENT_HPP

#include <GREM/aliases.hpp>
#include <GREM/application/FrameInfo.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/physics/DebugVisualization.hpp>
#include <GREM/physics/quantities.hpp>

#include "ClientSettings.hpp"

class AssetCache;
struct Audio;
struct Graphics;
class GameState;
class GameSystems;

struct GameClientOptions {
	CStringView settingsFilepath = "configuration/client.json";
	ClientSettings settings{};
	bool captureLoadTimeProfile = false;
};

class GameClient {
public:
	GameClient(AssetCache& assetCache, Audio& audio, Graphics& graphics, const GameSystems& gameSystems, exec::Executor& executor, GameState& gameState,
		const net::Endpoint& endpoint, const GameClientOptions& options);

	~GameClient();

	GameClient(const GameClient&) = delete;
	GameClient(GameClient&&) = delete;
	GameClient& operator=(const GameClient&) = delete;
	GameClient& operator=(GameClient&&) = delete;

	void disconnect();
	void reloadAssets();

	void pushFrameWaitTime(Duration frameWaitTime);
	void sendChatMessage(String message);
	void receiveChatMessage(String senderName, String message);

	void prepareForEvents();
	void handleEvent(const evt::Event& event, gfx::Window& window);
	void update(const app::FrameInfo& frameInfo, size_t lastSecondFrameCount, Duration latestServerPhysicsTime, phys::DebugVisualization3D* physicsDebugVisualization = nullptr);
	void display(const phys::DebugVisualization3D* serverPhysicsDebugVisualization, const phys::DebugVisualization3D* clientPhysicsDebugVisualization);

	[[nodiscard]] bool showSettingsGUI();
	void applyEditedSettings(const ClientSettings& oldSettings);
	void saveSettings();
	void saveScreenshot();
	void stopOpeningChat();

	[[nodiscard]] ClientSettings& getSettings() noexcept;
	[[nodiscard]] const ClientSettings& getSettings() const noexcept;

	[[nodiscard]] bool isClosed() const noexcept;
	[[nodiscard]] bool isConnecting() const noexcept;
	[[nodiscard]] bool isConnected() const noexcept;
	[[nodiscard]] bool isDisconnecting() const noexcept;
	[[nodiscard]] bool hasFinishedLoadingAssets() const noexcept;
	[[nodiscard]] bool hasControl() const noexcept;
	[[nodiscard]] bool isOpeningChat() const noexcept;

private:
	class Implementation;

	UniquePointer<Implementation> implementation;
};

#endif
