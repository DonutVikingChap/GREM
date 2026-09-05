// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_HPP
#define GREM_EXAMPLES_FPS_GAME_HPP

#include <GREM/aliases.hpp>
#include <GREM/application/Application.hpp>
#include <GREM/application/FrameInfo.hpp>
#include <GREM/application/TickInfo.hpp>
#include <GREM/audio/SoundStage.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/core/time.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/EventPump.hpp>
#include <GREM/events/Input.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/imgui/GraphicalUserInterface.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/physics/DebugVisualization.hpp>

#include "AssetCache.hpp"
#include "Audio.hpp"
#include "Console.hpp"
#include "GameClient.hpp"
#include "GameServer.hpp"
#include "GameState.hpp"
#include "GameSystems.hpp"
#include "Graphics.hpp"

#include <imgui.h>          // Im...
#include <imgui_internal.h> // ImGui::DockBuilder...
#include <utility>          // std::move

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/Thread.hpp>

#include <exception> // std::exception_ptr, std::current_exception, std::rethrow_exception
#endif

struct GameSettings {
	[[nodiscard]] static GameSettings load(const Filesystem& filesystem, CStringView filepath) {
		return json::deserializeFromString<GameSettings>(filesystem.readInputFileString(filepath));
	}

#ifdef __EMSCRIPTEN__
	uint32_t maxFPS = 0;
	bool frameRateLimiterSleep = false;
#else
	uint32_t maxFPS = 480;
	bool frameRateLimiterSleep = true;
#endif
	bool rawInput = true;
	uint32_t windowWidth = 1280;
	uint32_t windowHeight = 720;
	bool fullscreen = false;
	uint32_t maxBufferedFrames = 0;
#ifdef __EMSCRIPTEN__
	bool vSync = true;
#else
	bool vSync = false;
#endif
	uint32_t rendererTileSize = 64;
	uint32_t rendererDepthBins = 1024;
	uint32_t rendererBRDFResolution = 512;
	uint32_t rendererBRDFSamples = 2048;
	uint32_t audioOutputChannels = 2;
	size_t maxSimultaneousSoundInstances = 128;
	bool audioRoundoff = true;

	void save(Filesystem& filesystem, CStringView filepath) const {
		const String fileContents = json::serializeToString(*this);
		filesystem.createParentOutputDirectories(filepath);
		filesystem.openEmptyOutputFile(filepath).write(fileContents);
	}
};

struct GameArguments {
	Optional<String> remoteEndpoint{};
};

struct GameOptions {
	static constexpr net::PortNumber DEFAULT_PORT_NUMBER = 25701;

	CStringView settingsFilepath = "configuration/game.json";
	GameSettings settings{};
	net::PortNumber listenServerPort = DEFAULT_PORT_NUMBER;
	CStringView systems = "systems.json5";
	CStringView schema = "schema.json5";
	CStringView listenServerMap{};
	exec::Task::ParallelCount workerThreads = exec::DynamicExecutorOptions{}.targetParallelism;
	GameServerOptions sv{};
	GameClientOptions cl{};
	GraphicsOptions gfx{};
	bool captureStartupTimeProfile = false;
};

class Game final : public app::Application {
public:
	Game(Filesystem& filesystem, const GameArguments& arguments, const GameOptions& options)
		: app::Application(app::ApplicationOptions{
			  .tickInterval{},
			  .minFrameTime = (options.settings.maxFPS > 0) ? 1.0_seconds / options.settings.maxFPS : Duration{},
			  .frameRateLimiterSleepEnabled = options.settings.frameRateLimiterSleep,
		  })
		, settingsFilepath(options.settingsFilepath)
		, settings(options.settings)
		, eventPump(evt::EventPumpOptions{})
		, window(gfx::WindowOptions{
			  .title = "Example FPS",
			  .size{.width = settings.windowWidth, .height = settings.windowHeight},
			  .fullscreen = settings.fullscreen,
		  })
		, device(filesystem, window, gfx::DeviceOptions{})
		, swapchain(device, window,
			  gfx::SwapchainOptions{
				  .maxBufferedFrameCount = settings.maxBufferedFrames,
				  .useVerticalSynchronization = settings.vSync,
			  })
		, renderer2D(device, gfx::Renderer2DOptions{})
		, renderer3D(device, renderer2D,
			  gfx::Renderer3DOptions{
				  .tileSize = settings.rendererTileSize,
				  .depthBinCount = settings.rendererDepthBins,
				  .specularSplitSumBRDFIntegrationMapResolution = settings.rendererBRDFResolution,
				  .specularSplitSumBRDFIntegrationMapSampleCount = settings.rendererBRDFSamples,
			  })
		, gui(filesystem, eventPump, window, device, swapchain, renderer2D, imgui::GraphicalUserInterfaceOptions{})
		, soundStage(aud::SoundStageOptions{
			  .outputChannelCount = settings.audioOutputChannels,
			  .maxSimultaneousSoundInstanceCount = settings.maxSimultaneousSoundInstances,
			  .useRoundoff = settings.audioRoundoff,
			  .enableStatistics = true,
		  })
		, assetCache(filesystem)
		, audio(soundStage, assetCache)
		, graphics(device, swapchain, renderer2D, renderer3D, assetCache, options.gfx)
		, gameSystems(filesystem, options.systems)
		, executor(exec::DynamicExecutorOptions{
			  .targetParallelism =
				  static_cast<exec::Task::ParallelCount>(max(static_cast<size_t>((arguments.remoteEndpoint) ? options.workerThreads : options.workerThreads / 2), size_t{4}) - 3),
		  })
		, gameState(assetCache, &audio, &graphics, gameSystems, executor) {
		addBaseConsoleCommands();

		static constexpr auto getGameSettings = [](Game& game) -> GameSettings& {
			return game.settings;
		};
		addConsoleCommandsForSettings<GameSettings, getGameSettings>({});

		window.setIcon(res::Image{filesystem, "textures/icon.png"});

		// Set up the GUI.
		const float contentScale = window.getDisplay().getContentScale();

		ImGui::SetCurrentContext(gui.getContext());
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(contentScale);
		style.FontScaleDpi = contentScale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;

		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;

		// Start a client.
		gameClient.emplace(assetCache, audio, graphics, gameSystems, executor, gameState, resolveClientEndpoint(arguments.remoteEndpoint, options.listenServerPort), options.cl);
		static constexpr auto getClientSettings = [](Game& game) -> ClientSettings& {
			return game.gameClient->getSettings();
		};
		addConsoleCommandsForSettings<ClientSettings, getClientSettings>("cl.");

		if (!arguments.remoteEndpoint) {
			// Start a local listen server for the client to connect to.
			const net::IPv4Endpoint endpoint{net::IPv4Address::ANY, options.listenServerPort};
			String schemaFilepath{options.schema};
			String mapFilepath = (options.listenServerMap.empty()) ? getFirstAvailableMapFilepath(filesystem) : String{options.listenServerMap};
			const exec::DynamicExecutorOptions executorOptions{
				.targetParallelism = static_cast<exec::Task::ParallelCount>(max(static_cast<size_t>(options.workerThreads / 2), size_t{4}) - 3),
			};
			serverThread.emplace(assetCache, gameSystems, endpoint, std::move(schemaFilepath), std::move(mapFilepath), executorOptions, options.sv);

			localServerLocalClientCount = (gameClient) ? 1 : 0;
		}

		assetCacheNeedsCleanup = true;

		GREM_PROFILE_CONSTRUCTOR_END();
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		if (gameClient) {
			gameClient->prepareForEvents();
		}

		{
			GREM_PROFILE_BLOCK("Handle events");
			handleEvents();
		}

		{
			GREM_PROFILE_BLOCK("Update GUI");
			gui.update(frameInfo.deltaTime);
			ImGui::NewFrame();
		}

		setupGUIDockspace();

		if (serverThread) {
			serverThread->update();
		}

		if (gameClient) {
			gameClient->update(frameInfo, getLastSecondFrameCount(), (serverThread) ? serverThread->getLatestPhysicsTime() : Duration{},
				(clientPhysicsDebugVisualization) ? &*clientPhysicsDebugVisualization : nullptr);
			if (gameClient->isClosed()) {
				if (serverThread) {
					if (!serverThread->isShuttingDown()) {
						serverThread->shutdown();
					}
					if (serverThread->getClientCount() <= localServerLocalClientCount) {
						serverThread->close();
						serverThread.reset();
						quit();
						return;
					}
				} else {
					quit();
					return;
				}
			}

			// Wait for the client to finish connecting and loading before clearing the asset cache,
			// giving the listen server a chance to use the assets that were cached by the client.
			if (assetCacheNeedsCleanup && gameClient->hasFinishedLoadingAssets()) {
				assetCache.cleanup();
				assetCacheNeedsCleanup = false;
			}
		} else if (serverThread && serverThread->isShuttingDown() && serverThread->getClientCount() <= localServerLocalClientCount) {
			serverThread->close();
			serverThread.reset();
			quit();
			return;
		}

		if (!gameClient || !gameClient->hasControl()) {
			const GameSettings oldSettings = settings;
			const ClientSettings oldClientSettings = (gameClient) ? gameClient->getSettings() : ClientSettings{};
			showSettingsGUI();
			showChatGUI();
			applyEditedSettings(oldSettings, oldClientSettings);
		}

		if (settingsNeedSaving) {
			saveSettings();
			settingsNeedSaving = false;
		}

		{
			GREM_PROFILE_BLOCK("Handle late events");
			handleEvents();
		}
	}

	void display(app::FrameInfo) override {
		if (gameClient) {
			UniqueLock<Mutex> serverDebugVisualizationLock{};
			phys::DebugVisualization3D* serverPhysicsDebugVisualization = nullptr;
			if (serverDebugVisualization) {
				serverDebugVisualizationLock = UniqueLock{serverDebugVisualization->mutex};
				serverPhysicsDebugVisualization = &serverDebugVisualization->physicsDebugVisualization;
			}
			gameClient->display(serverPhysicsDebugVisualization, (clientPhysicsDebugVisualization) ? &*clientPhysicsDebugVisualization : nullptr);
		}

		{
			GREM_PROFILE_BLOCK("Render GUI");
			ImGui::Render();

			const ImDrawData& drawData = *ImGui::GetDrawData();
			if (!drawData.CmdLists.empty()) {
				gfx::RenderPass renderPass{device, swapchain, gfx::RetainValues{}};
				gui.drawFrame(renderPass, drawData);
				device.render(renderPass);
			}
		}

		{
			GREM_PROFILE_BLOCK("Present");
			const gfx::Device::PresentationSubmission presentationSubmission = device.present(swapchain);
			if (gameClient) {
				gameClient->pushFrameWaitTime(presentationSubmission.totalWaitTime);
			}
		}

		{
			GREM_PROFILE_BLOCK("Render extra GUI windows");
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

private:
	struct ServerDebugVisualization {
		Mutex mutex{};
		phys::DebugVisualization3D physicsDebugVisualization{};
	};

	class ServerThread {
	public:
#ifdef GREM_USE_MULTITHREADING
		ServerThread(AssetCache& assetCache, const GameSystems& gameSystems, const net::Endpoint& endpoint, String schemaFilepath, String mapFilepath,
			const exec::DynamicExecutorOptions& executorOptions, const GameServerOptions& gameServerOptions) {
			GREM_PROFILE_FUNCTION();

			running.test_and_set();
			thread = Thread{[this, &assetCache, &gameSystems, endpoint, schemaFilepath = std::move(schemaFilepath), mapFilepath = std::move(mapFilepath), executorOptions,
								gameServerOptions]() mutable -> void {
				try {
					GREM_PROFILER_SET_THREAD_INFO("Server thread", 0, ThreadID{});

					exec::DynamicExecutor serverExecutor{executorOptions};
					GameState serverGameState{assetCache, nullptr, nullptr, gameSystems, serverExecutor};
					GameServer gameServer{assetCache, serverGameState, endpoint, std::move(schemaFilepath), std::move(mapFilepath), gameServerOptions};
					TimePoint latestTickTime = Clock::now();
					while (running.test()) {
						const TimePoint currentTime = Clock::now();
						const Duration tickInterval = gameServer.getTickInterval();
						if (currentTime < latestTickTime + tickInterval) {
							sleepUntil(latestTickTime + tickInterval);
							continue;
						}

						if (shuttingDown.test() && !gameServer.isShuttingDown()) {
							gameServer.shutdown();
						}

						if (reloadingAssets.test()) {
							reloadingAssets.clear();
							gameServer.reloadAssets();
						}

						size_t tickCount = 0;
						do {
							++tickCount;
							latestTickTime += tickInterval;
						} while (latestTickTime + tickInterval <= currentTime);

						UniqueLock<Mutex> debugVisualizationLock{};
						phys::DebugVisualization3D* physicsDebugVisualization = nullptr;
						if (ServerDebugVisualization* const debugVisualization = externalDebugVisualization.load()) {
							debugVisualizationLock = UniqueLock{debugVisualization->mutex};
							physicsDebugVisualization = &debugVisualization->physicsDebugVisualization;
						}

						gameServer.update(tickCount, physicsDebugVisualization);
						clientCount.store(gameServer.getClientCount());
						latestPhysicsTime.store(gameServer.getPerformanceStats().latestPhysicsTime.count(), MemoryOrder::RELAXED);
					}
				} catch (...) {
					error = std::current_exception();
					running.clear();
				}
			}};
		}

		~ServerThread() {
			if (thread.joinable()) {
				running.clear();
				thread.join();
			}
		}
#else
		ServerThread(AssetCache& assetCache, const GameSystems& gameSystems, const net::Endpoint& endpoint, String schemaFilepath, String mapFilepath,
			const exec::DynamicExecutorOptions& executorOptions, const GameServerOptions& gameServerOptions)
			: serverExecutor(executorOptions)
			, serverGameState(assetCache, nullptr, nullptr, gameSystems, serverExecutor)
			, gameServer(assetCache, serverGameState, endpoint, std::move(schemaFilepath), std::move(mapFilepath), gameServerOptions) {}
#endif

		void shutdown() {
#ifdef GREM_USE_MULTITHREADING
			shuttingDown.test_and_set();
#else
			gameServer.shutdown();
#endif
		}

		void close() {
#ifdef GREM_USE_MULTITHREADING
			running.clear();
			thread.join();
			if (error) {
				std::rethrow_exception(std::exchange(error, {}));
			}
#endif
		}

		void reloadAssets() {
#ifdef GREM_USE_MULTITHREADING
			reloadingAssets.test_and_set();
#else
			gameServer.reloadAssets();
#endif
		}

		void update() {
			GREM_PROFILE_FUNCTION();

#ifdef GREM_USE_MULTITHREADING
			if (!running.test()) {
				GREM_ASSERT(error);
				std::rethrow_exception(std::exchange(error, {}));
			}
#else
			const TimePoint currentTime = Clock::now();
			const Duration tickInterval = gameServer.getTickInterval();
			if (currentTime < latestTickTime + tickInterval) {
				return;
			}

			size_t tickCount = 0;
			do {
				++tickCount;
				latestTickTime += tickInterval;
			} while (latestTickTime + tickInterval <= currentTime);

			UniqueLock<Mutex> debugVisualizationLock{};
			phys::DebugVisualization3D* physicsDebugVisualization = nullptr;
			if (ServerDebugVisualization* const debugVisualization = externalDebugVisualization.load()) {
				debugVisualizationLock = UniqueLock{debugVisualization->mutex};
				physicsDebugVisualization = &debugVisualization->physicsDebugVisualization;
			}
			gameServer.update(tickCount, physicsDebugVisualization);
			latestPhysicsTime.store(gameServer.getPerformanceStats().latestPhysicsTime.count(), MemoryOrder::RELAXED);
#endif
		}

		void setDebugVisualization(ServerDebugVisualization* debugVisualization) {
			externalDebugVisualization.store(debugVisualization);
		}

		[[nodiscard]] bool isShuttingDown() const noexcept {
#ifdef GREM_USE_MULTITHREADING
			return shuttingDown.test(MemoryOrder::RELAXED);
#else
			return gameServer.isShuttingDown();
#endif
		}

		[[nodiscard]] size_t getClientCount() const noexcept {
			return clientCount.load();
		}

		[[nodiscard]] Duration getLatestPhysicsTime() const noexcept {
			return Clock::duration{latestPhysicsTime.load(MemoryOrder::RELAXED)};
		}

	private:
		Atomic<size_t> clientCount{};
		Atomic<Clock::rep> latestPhysicsTime{};
#ifdef GREM_USE_MULTITHREADING
		std::exception_ptr error{};
		AtomicFlag running{};
		AtomicFlag shuttingDown{};
		AtomicFlag reloadingAssets{};
		Thread thread{};
#else
		exec::DynamicExecutor serverExecutor;
		GameState serverGameState;
		GameServer gameServer;
		TimePoint latestTickTime = Clock::now();
#endif
		Atomic<ServerDebugVisualization*> externalDebugVisualization{};
	};

	[[nodiscard]] static net::Endpoint resolveClientEndpoint(const Optional<String>& remoteEndpoint, net::PortNumber listenServerPort) {
		if (remoteEndpoint) {
			net::Endpoint resolvedEndpoint = net::Endpoint::resolve(*remoteEndpoint);
			if (const Optional<net::IPv4Endpoint> ipv4Endpoint = resolvedEndpoint.getIPv4Endpoint()) {
				if (ipv4Endpoint->getPortNumber() == 0) {
					return net::IPv4Endpoint{ipv4Endpoint->getAddress(), GameOptions::DEFAULT_PORT_NUMBER};
				}
			}
			return resolvedEndpoint;
		}
		return net::IPv4Endpoint{net::IPv4Address::LOOPBACK, listenServerPort};
	}

	[[nodiscard]] static String getFirstAvailableMapFilepath(const Filesystem& filesystem) {
		ArrayList<String> mapFilenames{};
		filesystem.forEachInputFilenameInDirectory("maps", [&](CStringView filename) -> void {
			if (filename.ends_with(".json") || filename.ends_with(".json5")) {
				mapFilenames.emplace_back(filename);
			}
		});
		if (mapFilenames.empty()) {
			throw Error{"No map to load!"};
		}
		partialSort(mapFilenames, mapFilenames.begin() + 1);
		return formatString("maps/{}", mapFilenames.front());
	}

	void addBaseConsoleCommands() {
		console.addCommand("help", [](Game& game, CStringView arguments) -> String {
			ArrayList<String> commandNames{};
			const String prefix = (arguments.empty()) ? String{} : String{arguments} + ".";
			for (const auto& [name, command] : game.console.getCommands()) {
				if (name == arguments) {
					return formatString("Command: \"{}\".", name);
				}
				if (name.starts_with(prefix)) {
					if (const size_t dotPosition = name.find('.', prefix.size()); dotPosition != String::npos) {
						commandNames.push_back("help " + name.substr(0, dotPosition));
					} else {
						commandNames.push_back(name);
					}
				}
			}
			if (commandNames.empty()) {
				return formatString("Unknown command prefix \"{}\".", arguments);
			}
			sort(commandNames);
			commandNames.erase(unique(commandNames), commandNames.end());
			String result = "Available commands:";
			for (const String& commandName : commandNames) {
				result.append("\n/");
				result.append(commandName);
			}
			return result;
		});

		console.addCommand("quit", [](Game& game, CStringView) -> String {
			game.disconnectAndQuit();
			return {};
		});
	}

	template <typename Settings, auto GetSettings>
	void addConsoleCommandsForSettings(StringView prefix) {
		meta::forEachIndexedNamedFieldType<Settings>([&]<typename T>(auto index, StringView name, meta::Type<T>) -> void {
			static constexpr size_t INDEX = index;
			if constexpr (std::is_aggregate_v<T>) {
				static constexpr auto getNestedSettings = [](Game& game) -> auto& {
					return get<INDEX>(meta::getFields(GetSettings(game)));
				};
				addConsoleCommandsForSettings<T, getNestedSettings>(formatString("{}{}.", prefix, name));
			} else {
				console.addCommand(formatString("{}{}", prefix, name), [](Game& game, CStringView arguments) -> String {
					if (arguments.empty()) {
						return json::serializeToString(get<INDEX>(meta::getFields(GetSettings(game))));
					}
					try {
						json::deserializeFromString(String{arguments}, get<INDEX>(meta::getFields(GetSettings(game))));
						game.settingsNeedSaving = true;
					} catch (const json::Error& e) {
						return e.what();
					}
					return {};
				});
			}
		});
	}

	void disconnectAndQuit() {
		if (gameClient) {
			if (gameClient->isConnected()) {
				gameClient->disconnect();
			} else {
				if (serverThread) {
					serverThread->close();
					serverThread.reset();
				}
				quit();
			}
		} else if (serverThread) {
			if (!serverThread->isShuttingDown()) {
				serverThread->shutdown();
			} else {
				serverThread->close();
				serverThread.reset();
				quit();
			}
		} else {
			quit();
		}
	}

	void handleEvents() {
		for (const evt::Event& event : eventPump.pollEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
					disconnectAndQuit();
					break;
				}
				GREM_CASE(const evt::WindowResizedEvent& resized) {
					if (!settings.fullscreen) {
						settings.windowWidth = resized.windowSize.width;
						settings.windowHeight = resized.windowSize.height;
						settingsNeedSaving = true;
					}
					break;
				}
				GREM_CASE(const evt::KeyPressedEvent& pressed) {
					switch (pressed.keyCode) {
						case evt::KeyCode::F4:
							if (pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT)) {
								disconnectAndQuit();
							}
							break;
						case evt::KeyCode::F8: GREM_PROFILER_SAVE_NEXT_N_FRAMES(8, "fps_profiler_trace_", ProfileFormat::TRACE_EVENT_FORMAT); break;
						case evt::KeyCode::F10: disconnectAndQuit(); break;
						case evt::KeyCode::F11:
							settings.fullscreen = !window.isFullscreen();
							settingsNeedSaving = true;
							window.setFullscreen(settings.fullscreen);
							break;
						case evt::KeyCode::F12:
							if (gameClient) {
								gameClient->saveScreenshot();
							}
							break;
						case evt::KeyCode::RETURN:
							if (pressed.keyModifiers.containsAnyOf(evt::KeyModifiers::ALT)) {
								settings.fullscreen = !window.isFullscreen();
								settingsNeedSaving = true;
								window.setFullscreen(settings.fullscreen);
							}
							break;
						default: break;
					}
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}
			if (gui.handleEvent(event)) {
				continue;
			}
			if (gameClient) {
				gameClient->handleEvent(event, window);
			}
		}
	}

	void setupGUIDockspace() {
		const ImGuiID dockspaceID = ImGui::GetID("Main Dockspace");
		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

			ImGuiID remainingDockspaceID = dockspaceID;
			const ImGuiID settingsDockID = ImGui::DockBuilderSplitNode(remainingDockspaceID, ImGuiDir_Right, 0.25f, nullptr, &remainingDockspaceID);
			const ImGuiID chatDockID = ImGui::DockBuilderSplitNode(remainingDockspaceID, ImGuiDir_Down, 0.15f, nullptr, &remainingDockspaceID);

			ImGui::DockBuilderDockWindow("Settings", settingsDockID);
			ImGui::DockBuilderDockWindow("Chat", chatDockID);
			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpaceOverViewport(dockspaceID, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
	}

	void showSettingsGUI() {
		ImGui::Begin("Settings");

		ImGui::BeginChild("Settings Tabs", ImVec2{0.0f, -(ImGui::GetStyle().SeparatorSize + ImGui::GetFrameHeightWithSpacing() * 2.0f - ImGui::GetFrameHeight())});
		if (ImGui::BeginTabBar("Tabs")) {
			if (ImGui::BeginTabItem("Game")) {
				if (ImGui::Button("Reload assets")) {
					assetCache.clear();
					if (serverThread) {
						serverThread->reloadAssets();
					}
					if (gameClient) {
						gameClient->reloadAssets();
					}
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Display")) {
				{
					const bool oldFullscreen = settings.fullscreen;
					settings.fullscreen = window.isFullscreen();
					if (ImGui::Checkbox("Fullscreen", &settings.fullscreen)) {
						settingsNeedSaving |= settings.fullscreen != oldFullscreen;
					}
				}
				ImGui::Separator();
				{
					bool limitFPS = getMinFrameTime() > Duration{};
					if (ImGui::Checkbox("Limit FPS", &limitFPS)) {
						settings.maxFPS = (limitFPS) ? 480 : 0;
						settingsNeedSaving = true;
					}
					if (limitFPS) {
						ImGui::Indent();
						int fpsLimit = static_cast<int>(settings.maxFPS);
						if (ImGui::SliderInt("FPS limit", &fpsLimit, 10, 480, "%d Hz")) {
							settings.maxFPS = static_cast<uint32_t>(max(fpsLimit, 1));
						}
						settingsNeedSaving |= ImGui::IsItemDeactivatedAfterEdit();
						ImGui::Unindent();
					}
				}
				ImGui::Separator();
				{
					bool vSync = swapchain.isVerticalSynchronizationEnabled();
					if (ImGui::Checkbox("VSync", &vSync)) {
						settings.vSync = vSync;
						settingsNeedSaving = true;
					}

					int maxBufferedFrames = static_cast<int>(swapchain.getMaxBufferedFrameCount());
					if (ImGui::SliderInt("Max buffered frames", &maxBufferedFrames, 0, 2)) {
						settings.maxBufferedFrames = static_cast<uint32_t>(maxBufferedFrames);
						settingsNeedSaving = true;
					}
				}
#ifdef GREM_USE_PROFILING
				ImGui::Separator();
				{
					if (ImGui::Button("Capture performance trace (F8)")) {
						GREM_PROFILER_SAVE_NEXT_N_FRAMES(8, "fps_profiler_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
					}
				}
#endif
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Physics")) {
				bool enableServerDebugVisualization = serverDebugVisualization.has_value();
				if (ImGui::Checkbox("Server debug visualization", &enableServerDebugVisualization)) {
					if (enableServerDebugVisualization != serverDebugVisualization.has_value()) {
						if (serverDebugVisualization) {
							if (serverThread) {
								serverThread->setDebugVisualization(nullptr);
								ScopedLock lock{serverDebugVisualization->mutex};
								atomicThreadFence(MemoryOrder::SEQUENTIALLY_CONSISTENT);
							}
							serverDebugVisualization.reset();
						} else {
							serverDebugVisualization.emplace();
							if (serverThread) {
								serverThread->setDebugVisualization(&*serverDebugVisualization);
							}
						}
					}
				}
				bool enableClientDebugVisualization = clientPhysicsDebugVisualization.has_value();
				if (ImGui::Checkbox("Client debug visualization", &enableClientDebugVisualization)) {
					if (enableClientDebugVisualization) {
						clientPhysicsDebugVisualization.emplace();
					} else {
						clientPhysicsDebugVisualization.reset();
					}
				}
				ImGui::EndTabItem();
			}

			if (gameClient) {
				settingsNeedSaving |= gameClient->showSettingsGUI();
			}

			ImGui::EndTabBar();
		}
		ImGui::EndChild();
		ImGui::Separator();
		if (ImGui::Button("Quit Game (F10)")) {
			disconnectAndQuit();
		}

		ImGui::End();
	}

	void showChatGUI() {
		ImGui::Begin("Chat");
		bool submitted = ImGui::InputText(
			"Message", chatInputBuffer.data(), chatInputBuffer.size(),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
			[](ImGuiInputTextCallbackData* data) -> int {
				GREM_ASSERT(data->BufSize == sizeof(chatInputBuffer));
				Game& game = *static_cast<Game*>(data->UserData);
				switch (data->EventKey) {
					case ImGuiKey_UpArrow:
						if (const Optional<CStringView> command = game.console.previousCommandInHistory()) {
							data->DeleteChars(0, data->BufTextLen);
							data->InsertChars(0, command->c_str());
						}
						break;
					case ImGuiKey_DownArrow:
						if (const Optional<CStringView> command = game.console.nextCommandInHistory()) {
							data->DeleteChars(0, data->BufTextLen);
							data->InsertChars(0, command->c_str());
						}
						break;
					case ImGuiKey_Tab:
						if (data->BufTextLen > 0 && *data->Buf == '/') {
							const StringView text{data->Buf + 1, static_cast<size_t>(data->BufTextLen - 1)};
							ArrayList<StringView> candidateNames{};
							StringView commonPrefix{};
							for (const auto& [name, command] : game.console.getCommands()) {
								if (name.starts_with(text)) {
									if (candidateNames.empty()) {
										commonPrefix = name;
									} else {
										const auto [commonPrefixMismatch, nameMismatch] = mismatch(commonPrefix, name);
										commonPrefix = StringView{commonPrefix.begin(), commonPrefixMismatch};
									}
									candidateNames.emplace_back(name);
								}
							}
							if (candidateNames.size() == 1 || commonPrefix.size() > text.size()) {
								const StringView newText = commonPrefix.substr(text.size());
								data->InsertChars(data->BufTextLen, newText.data(), newText.data() + newText.size());
							} else if (!candidateNames.empty() && game.gameClient) {
								String message{"Candidates:\n/"};
								size_t lineBegin = 0;
								message.append(candidateNames.front());
								for (const StringView candidateName : Span{candidateNames}.subspan(1)) {
									if (message.size() + 2 + candidateName.size() - lineBegin > 80) {
										message.append("\n/");
										lineBegin = message.size();
									} else {
										message.append(" /");
									}
									message.append(candidateName);
								}
								game.gameClient->receiveChatMessage("[CONSOLE]", std::move(message));
							}
						}
						break;
					default: break;
				}
				return 0;
			},
			this);
		if (gameClient && gameClient->isOpeningChat()) {
			ImGui::SetKeyboardFocusHere(-1);
			gameClient->stopOpeningChat();
		} else if (submitted) {
			ImGui::SetKeyboardFocusHere(-1);
		}
		submitted |= ImGui::Button("Send");
		if (submitted && chatInputBuffer.front() != '\0') {
			if (chatInputBuffer.front() == '/') {
				const char* const commandNameBegin = chatInputBuffer.data() + 1;
				const char* p = commandNameBegin;
				while (*p != '\0' && *p != ' ') {
					++p;
				}
				const char* const commandNameEnd = p;
				while (*p == ' ') {
					++p;
				}
				const char* const commandArguments = p;
				String result = console.executeCommand(*this, StringView{commandNameBegin, commandNameEnd}, commandArguments);
				if (gameClient && !result.empty()) {
					gameClient->receiveChatMessage("[CONSOLE]", std::move(result));
				}
				console.saveCommandToHistory(chatInputBuffer.data());
			} else if (gameClient) {
				gameClient->sendChatMessage(chatInputBuffer.data());
			}
			chatInputBuffer.front() = '\0';
		}
		ImGui::End();
	}

	void applyEditedSettings(const GameSettings& oldSettings, const ClientSettings& oldClientSettings) {
		if (settings.fullscreen != oldSettings.fullscreen) {
			window.setFullscreen(settings.fullscreen);
		}

		if ((settings.windowWidth != oldSettings.windowWidth || settings.windowHeight != oldSettings.windowHeight) && !window.isFullscreen()) {
			window.setSize({settings.windowWidth, settings.windowHeight});
		}

		if (settings.maxFPS != oldSettings.maxFPS) {
			setMinFrameTime((settings.maxFPS > 0) ? 1.0_seconds / settings.maxFPS : Duration{});
		}

		if (settings.vSync != oldSettings.vSync) {
			swapchain.setVerticalSynchronizationEnabled(settings.vSync);
		}

		if (settings.maxBufferedFrames != oldSettings.maxBufferedFrames) {
			swapchain.setMaxBufferedFrameCount(settings.maxBufferedFrames);
		}

		if (gameClient) {
			gameClient->applyEditedSettings(oldClientSettings);
		}
	}

	void saveSettings() {
		if (gameClient) {
			gameClient->saveSettings();
		}

		if (!settingsFilepath.empty()) {
			try {
				settings.save(assetCache.getFilesystem(), settingsFilepath);
			} catch (...) {
			}
		}
	}

	GREM_PROFILE_CONSTRUCTOR_BEGIN();
	String settingsFilepath;
	GameSettings settings;
	evt::EventPump eventPump;
	gfx::Window window;
	gfx::Device device;
	gfx::Swapchain swapchain;
	gfx::Renderer2D renderer2D;
	gfx::Renderer3D renderer3D;
	imgui::GraphicalUserInterface gui;
	aud::SoundStage soundStage;
	AssetCache assetCache;
	Audio audio;
	Graphics graphics;
	GameSystems gameSystems;
	exec::DynamicExecutor executor;
	GameState gameState;
	Optional<GameClient> gameClient{};
	Optional<ServerDebugVisualization> serverDebugVisualization{};
	Optional<phys::DebugVisualization3D> clientPhysicsDebugVisualization{};
	Optional<ServerThread> serverThread{};
	size_t localServerLocalClientCount = 0;
	bool assetCacheNeedsCleanup = false;
	bool settingsNeedSaving = false;
	Console console{};
	Array<char, 256> chatInputBuffer{};
};

#endif
