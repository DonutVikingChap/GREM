// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "GameServer.hpp"

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/networking/Socket.hpp>
#include <GREM/physics/DebugVisualization.hpp>
#include <GREM/physics/quantities.hpp>

#include "AssetCache.hpp"
#include "Connection.hpp"
#include "EntityCallbacks.hpp"
#include "GameState.hpp"
#include "GameSystems.hpp"
#include "MessageForGameClient.hpp"
#include "MessageForGameServer.hpp"
#include "Prefab.hpp"
#include "Schema.hpp"
#include "Snapshot.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "game_commands.hpp"
#include "game_components.hpp"
#include "game_events.hpp"
#include "game_map.hpp"
#include "game_resources.hpp"

#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)
#include <utility>      // std::move, std::forward, std::swap

namespace {

constexpr size_t MAX_RECEIVED_PACKETS_PER_FRAME_PER_CLIENT = 32;

using ConnectionToClient = Connection<MessageForGameServer, MessageForGameClient>;

class CommandBuffer {
public:
	static constexpr TickDifference MAX_SIZE = 512;

	explicit CommandBuffer(TickIndex tickIndex)
		: firstCommandTickIndex(tickIndex) {}

	void insertCommand(TickIndex tickIndex, TickCommand&& tickCommand) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		if (tickIndex < firstCommandTickIndex) {
			return;
		}

		size_t index = static_cast<size_t>(tickIndex - firstCommandTickIndex);
		while (tickCommands.size() <= index) {
			if (tickCommands.size() >= size_t{MAX_SIZE}) {
				tickCommands.pop_front();
				++firstCommandTickIndex;
				--index;
			}
			tickCommands.push_back_and_overwrite([](ReceivedTickCommand& receivedTickCommand) -> void {
				receivedTickCommand.received = false;
				receivedTickCommand.used = false;
			});
		}

		// Use swap to reuse memory between us and the buffer in LatestCommandsMessageForGameServer.
		std::swap(tickCommands[index].tickCommand, tickCommand);
		tickCommands[index].received = true;
	}

	[[nodiscard]] const TickCommand& useTickCommand(TickIndex tickIndex) {
		const auto missOneTick = [&]() -> void {
			latestUsedTickCommand.receivedInterpolationTimestampAtTickBegin.addTicks(1);
		};

		const auto applyTickCommand = [&](const ReceivedTickCommand& tickCommand) -> void {
			GREM_ASSERT(isSorted(tickCommand.tickCommand.localPlayerCommands));
			GREM_ASSERT(isSorted(latestUsedTickCommand.localPlayerCommands));
			GREM_ASSERT(adjacentFind(tickCommand.tickCommand.localPlayerCommands) == tickCommand.tickCommand.localPlayerCommands.end());
			GREM_ASSERT(adjacentFind(latestUsedTickCommand.localPlayerCommands) == latestUsedTickCommand.localPlayerCommands.end());
			latestUsedTickCommand.receivedInterpolationTimestampAtTickBegin = tickCommand.tickCommand.receivedInterpolationTimestampAtTickBegin;
			erase_if(latestUsedTickCommand.localPlayerCommands, [&](const TickCommand::LocalPlayerCommand& latestUsedTickLocalPlayerCommand) -> bool {
				return !binarySearch(tickCommand.tickCommand.localPlayerCommands, latestUsedTickLocalPlayerCommand.localPlayerID);
			});
			GREM_ASSERT(latestUsedTickCommand.localPlayerCommands.size() <= tickCommand.tickCommand.localPlayerCommands.size());
			auto it = latestUsedTickCommand.localPlayerCommands.begin();
			for (const TickCommand::LocalPlayerCommand& localPlayerCommand : tickCommand.tickCommand.localPlayerCommands) {
				if (it == latestUsedTickCommand.localPlayerCommands.end() || it->localPlayerID != localPlayerCommand.localPlayerID) {
					it = latestUsedTickCommand.localPlayerCommands.insert_unspecified_value(it);
					it->localPlayerID = localPlayerCommand.localPlayerID;
					it->subtickCommands.clear();
				}
				it->desiredDirectionScale = localPlayerCommand.desiredDirectionScale;
				it->aimRotationRates = localPlayerCommand.aimRotationRates;
				for (const SubtickCommand& command : localPlayerCommand.subtickCommands) {
					it->subtickCommands.push_back(command);
				}
				++it;
			}
			GREM_ASSERT(latestUsedTickCommand.localPlayerCommands.size() == tickCommand.tickCommand.localPlayerCommands.size());
		};

		// Clear transient subtick commands.
		// Keep properties that are normally overridden, but might still be useful as an extrapolated guess in the case of a miss (e.g. last movement direction).
		for (TickCommand::LocalPlayerCommand& latestUsedTickLocalPlayerCommand : latestUsedTickCommand.localPlayerCommands) {
			latestUsedTickLocalPlayerCommand.subtickCommands.clear();
		}

		if (tickIndex >= firstCommandTickIndex) {
			// The requested tick might be in the buffer.
			const size_t index = static_cast<size_t>(tickIndex - firstCommandTickIndex);
			if (index >= tickCommands.size()) {
				// The requested tick would lie after everything currently in the buffer.
				if (anyOf(tickCommands, [](const ReceivedTickCommand& tickCommand) -> bool { return tickCommand.received && !tickCommand.used; })) {
					// We've received at least one tick command that was missed earlier and hasn't been used yet.
					// Use them.
					for (ReceivedTickCommand& tickCommand : tickCommands) {
						if (tickCommand.received && !tickCommand.used) {
							tickCommand.used = true;
							applyTickCommand(tickCommand);
						}
					}
					// Set the subtick time offsets of all of the missed commands to 0 so that they get executed as soon as possible.
					for (TickCommand::LocalPlayerCommand& latestUsedTickLocalPlayerCommand : latestUsedTickCommand.localPlayerCommands) {
						for (SubtickCommand& command : latestUsedTickLocalPlayerCommand.subtickCommands) {
							command.timeOffset = {};
						}
					}
				} else {
					// No received tick commands are available for use.
					missOneTick();
				}
			} else {
				// The requested tick might still be in the buffer.
				if (anyOf(Subrange{tickCommands.begin(), tickCommands.begin() + static_cast<ptrdiff_t>(index) + 1},
						[](const ReceivedTickCommand& tickCommand) -> bool { return tickCommand.received && !tickCommand.used; })) {
					// We've received at least one tick command up to and including the requested tick that hasn't been used yet.
					// Use any of the ones we have missed.
					for (TickIndex oldTickIndex = firstCommandTickIndex; oldTickIndex < tickIndex; ++oldTickIndex) {
						const size_t oldIndex = static_cast<size_t>(oldTickIndex - firstCommandTickIndex);
						ReceivedTickCommand& tickCommand = tickCommands[oldIndex];
						if (tickCommand.received && !tickCommand.used) {
							tickCommand.used = true;
							applyTickCommand(tickCommand);
						}
					}
					// Set the subtick time offsets of all of the missed commands to 0 so that they get executed as soon as possible, before any of the requested tick's subtick commands.
					for (TickCommand::LocalPlayerCommand& latestUsedTickLocalPlayerCommand : latestUsedTickCommand.localPlayerCommands) {
						for (SubtickCommand& command : latestUsedTickLocalPlayerCommand.subtickCommands) {
							command.timeOffset = {};
						}
					}

					// If we've received the requested tick command, and it hasn't been used yet, use it.
					ReceivedTickCommand& tickCommand = tickCommands[index];
					if (tickCommands[index].received && !tickCommands[index].used) {
						tickCommands[index].used = true;
						applyTickCommand(tickCommand);
					}
				} else {
					// No tick commands received up to and including the requested tick are available for use.
					missOneTick();
				}
			}
		} else {
			// The requested tick would lie before everything currently in the buffer.
			missOneTick();
		}
		return latestUsedTickCommand;
	}

private:
	struct ReceivedTickCommand {
		TickCommand tickCommand;
		bool received;
		bool used;
	};

	RingBuffer<ReceivedTickCommand> tickCommands{};
	TickIndex firstCommandTickIndex;
	TickCommand latestUsedTickCommand{};
};

class EventBuffer {
public:
	explicit EventBuffer(TickIndex tickIndex)
		: firstSnapshotTickIndex(tickIndex) {}

	void pushEventConfirmation(TickIndex tickIndex, const Event& event) {
		if (tickIndex < firstSnapshotTickIndex) {
			unacknowledgedEventConfirmations.clear();
			firstSnapshotTickIndex = tickIndex;
		}

		while (tickIndex >= firstSnapshotTickIndex + static_cast<TickDifference>(unacknowledgedEventConfirmations.size())) {
			unacknowledgedEventConfirmations.push_back_unspecified_value().clear();
		}

		GREM_ASSERT(!unacknowledgedEventConfirmations.empty());
		unacknowledgedEventConfirmations[static_cast<size_t>(tickIndex - firstSnapshotTickIndex)].push_back(
			UpdateGameStateMessageForGameClient::ConfirmEvent{.tickIndex = tickIndex, .event = event});
	}

	void acknowledgeEventConfirmations(TickIndex latestReceivedSnapshotTickIndex) {
		while (!unacknowledgedEventConfirmations.empty() && firstSnapshotTickIndex < latestReceivedSnapshotTickIndex) {
			unacknowledgedEventConfirmations.pop_front();
			++firstSnapshotTickIndex;
		}
	}

	struct OutgoingEventConfirmations {
		TickIndex firstSnapshotTickIndex;
		RingBuffer<SmallBuffer<UpdateGameStateMessageForGameClient::ConfirmEvent, 2>>::const_iterator begin;
		RingBuffer<SmallBuffer<UpdateGameStateMessageForGameClient::ConfirmEvent, 2>>::const_iterator end;
	};

	[[nodiscard]] OutgoingEventConfirmations getLatestOutgoingEventConfirmations() const {
		return {.firstSnapshotTickIndex = firstSnapshotTickIndex, .begin = unacknowledgedEventConfirmations.begin(), .end = unacknowledgedEventConfirmations.end()};
	}

private:
	TickIndex firstSnapshotTickIndex;
	RingBuffer<SmallBuffer<UpdateGameStateMessageForGameClient::ConfirmEvent, 2>> unacknowledgedEventConfirmations{};
};

struct Client {
	struct Player {
		PlayerID playerID;
		ArrayList<LocalPlayerID> localPlayerIDs{};
		CommandBuffer commandBuffer;
		EventBuffer eventBuffer;
		SnapshotBuffer snapshots{};
		TickIndex latestReceivedCommandTickIndex{};
		TickIndex latestAcknowledgedSnapshotTickIndex{};
		bool sendingReliableSnapshot = false;

		Player(PlayerID playerID, TickIndex tickIndex)
			: playerID(playerID)
			, commandBuffer(tickIndex)
			, eventBuffer(tickIndex) {}
	};

	ConnectionToClient connection{};
	Optional<Player> player{};
	size_t ticksUntilNextSend = 0;
	Duration timeSpentLoadingMap{};
};

using Clients = OrderedMap<net::Endpoint, Client>;

void runTick(GameState& gameState, Clients& clients) {
	GREM_PROFILE_FUNCTION();

	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();
	Events& events = resources.getResource<Events>();
	const TickIndex tickIndex = resources.getResource<TickIndex>();
	const Duration tickInterval = resources.getResource<Duration>();

	for (auto&& [endpoint, client] : clients) {
		if (!client.player) {
			continue;
		}

		const TickCommand& tickCommand = client.player->commandBuffer.useTickCommand(tickIndex);
		tickCommand.beginTick(gameState, client.player->playerID);
		tickCommand.runSubtick(gameState, client.player->playerID, client.player->snapshots, client.player->snapshots, Duration{}, tickInterval);
		tickCommand.endTick(gameState, client.player->playerID);

		resources.getResource<SynchronizedEntityMap>().removeDestroyedEntities(registry);
		resources.getResource<PlayerEntityMap>().update(registry);
	}

	if (!resources.getResource<SessionState>().flags.contains(SessionState::PAUSED)) {
		gameState.tick();
	}
	++resources.getResource<TickIndex>();

	resources.getResource<SynchronizedEntityMap>().removeDestroyedEntities(registry);
	resources.getResource<PlayerEntityMap>().update(registry);

	for (auto&& [endpoint, client] : clients) {
		if (!client.player) {
			continue;
		}

		while (client.player->snapshots.size() >= SNAPSHOT_BUFFER_WINDOW_SIZE) {
			client.player->snapshots.pop_front();
		}
		client.player->snapshots.push_back_and_overwrite([&](Snapshot& snapshot) -> void { saveSnapshot(snapshot, gameState, client.player->playerID); });

		for (const Event& event : events) {
			if (const Optional<Event> filteredEvent = event.filter(gameState, client.player->playerID)) {
				client.player->eventBuffer.pushEventConfirmation(tickIndex, *filteredEvent);
			}
		}
	}
	events.clear();
}

} // namespace

class GameServer::Implementation {
public:
	Implementation(AssetCache& assetCache, GameState& gameState, const net::Endpoint& endpoint, String schemaFilepath, String mapFilepath, const GameServerOptions& options)
		: assetCache(assetCache)
		, gameState(gameState)
		, socket(net::BlockingMode::NON_BLOCKING, endpoint)
		, schemaFilepath(std::move(schemaFilepath))
		, mapFilepath(std::move(mapFilepath))
		, maxClientCount(options.maxClientCount)
		, maxLocalPlayerCountPerClient(min(options.maxLocalPlayerCountPerClient, MAX_LOCAL_PLAYER_COUNT))
		, maxTicksPerFrame((options.minFrameRate <= 0 || options.tickRate <= options.minFrameRate) ? size_t{1} : static_cast<size_t>(options.tickRate / options.minFrameRate))
		, maxOutgoingDataRatePerClient(options.maxOutgoingDataRatePerClient)
		, maxSendSkipTicks(options.maxSendSkipTicks)
		, mapLoadGracePeriod(options.mapLoadGracePeriod)
		, mapLoadTimeout(options.mapLoadTimeout) {
		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();
		resources.addExternalResource<Schema>(&schema);
		resources.addSharedResource<EntityCallbacks>();
		resources.addSharedResource<Events>();
		resources.addSharedResource<MapInfo>();
		resources.addResource<SynchronizedEntityMap>();
		resources.addResource<PlayerEntityMap>();
		resources.addResource<TickIndex>();
		resources.addResource<Duration>(1_x / options.tickRate);

		const Filesystem& filesystem = assetCache.getFilesystem();
		HashSet<CStringView> visitedFilepaths{};
		schema.extend(filesystem.readInputFileString(this->schemaFilepath), this->schemaFilepath, &filesystem, &visitedFilepaths);

		gameState.setState({SystemsLayerType{"SESSION"}, SystemsLayerType{"SIMULATION"}});
		loadMap(registry, resources, this->mapFilepath);

		playerPrefab = assetCache.getPrefab(schema, schema.getPlayerPrefabFilepath()); // NOLINT(cppcoreguidelines-prefer-member-initializer)

		saveSnapshot(initialSnapshot, gameState, {});

		eprintln("Server opened on {}.", endpoint);

		GREM_PROFILE_CONSTRUCTOR_END();
	}

	~Implementation() {
		gameState.clearState();

		ResourceRegistry& resources = gameState.getResources();
		resources.removeResource<Duration>();
		resources.removeResource<TickIndex>();
		resources.removeResource<PlayerEntityMap>();
		resources.removeResource<SynchronizedEntityMap>();
		resources.removeResource<MapInfo>();
		resources.removeResource<Events>();
		resources.removeResource<Schema>();
	}

	Implementation(const Implementation&) = delete;
	Implementation(Implementation&&) = delete;
	Implementation& operator=(const Implementation&) = delete;
	Implementation& operator=(Implementation&&) = delete;

	void shutdown() {
		if (isShuttingDown()) {
			return;
		}

		maxClientCount = 0;
		for (auto&& [endpoint, client] : clients) {
			client.connection.requestDisconnect();
		}
		cleanupClosedConnections();
	}

	void reloadAssets() {
		gameState.reloadAssets();
		playerPrefab = assetCache.getPrefab(schema, schema.getPlayerPrefabFilepath());
	}

	void update(size_t tickCount, phys::DebugVisualization3D* physicsDebugVisualization) {
		receiveIncomingPackets();
		handleMapLoadTimeout(tickCount * getTickInterval());
		for (size_t i = min(tickCount, maxTicksPerFrame); i-- > 0;) {
			runTick(gameState, clients);
			if (const TickPerformanceStats* const tickPerformanceStats = gameState.getResources().findResource<TickPerformanceStats>()) {
				performanceStats.latestPhysicsTime = tickPerformanceStats->latestPhysicsTime;
			}
		}

		if (physicsDebugVisualization) {
			physicsDebugVisualization->clear();
			phys::Simulation3D::drawDebugVisualization(*physicsDebugVisualization, gameState.getRegistry(), gameState.getResources());
		}

		writeOutgoingMessages();
		sendOutgoingPackets();
	}

	[[nodiscard]] bool isShuttingDown() const noexcept {
		return maxClientCount == 0;
	}

	[[nodiscard]] size_t getClientCount() const noexcept {
		return clients.size();
	}

	[[nodiscard]] Duration getTickInterval() const noexcept {
		return gameState.getResources().getResource<Duration>();
	}

	[[nodiscard]] const GameServer::PerformanceStats& getPerformanceStats() const noexcept {
		return performanceStats;
	}

private:
	void handleMessage(Client&, RequestSchemaExtensionMessageForGameServer&& message) {
		GREM_PROFILE_FUNCTION();

		if (schema.getCRC32() == message.previousSchemaCRC32) {
			try {
				schema.extend(message.schemaExtension);
			} catch (...) {
				eprintln("{}", Error::formatCurrentExceptionMessage());
				return;
			}
			for (auto&& [endpoint, client] : clients) {
				client.connection.writeReliableMessage(ExtendSchemaMessageForGameClient{.schemaExtension = message.schemaExtension});
			}
			schemaExtensions.push_back(std::move(message).schemaExtension);
		}
	}

	void handleMessage(Client& client, const JoinGameRequestMessageForGameServer& message) {
		GREM_PROFILE_FUNCTION();

		if (client.player) {
			if (binarySearch(client.player->localPlayerIDs, message.localPlayerID)) {
				return;
			}

			if (client.player->localPlayerIDs.size() >= maxLocalPlayerCountPerClient) {
				if (client.player->playerID) {
					client.connection.writeReliableMessage(ChatMessageForGameClient{
						.senderName = "[SERVER]",
						.message = formatString("Maximum local player limit reached ({}).", maxLocalPlayerCountPerClient),
					});
				}
				client.connection.writeReliableMessage(LeftGameMessageForGameClient{
					.localPlayerID = message.localPlayerID,
				});
				return;
			}
		}

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		const TickIndex tickIndex = resources.getResource<TickIndex>();

		const bool firstJoin = !client.player;
		if (firstJoin) {
			client.player.emplace(nextPlayerID, tickIndex);
			++nextPlayerID.value;
			client.timeSpentLoadingMap = {};

			for (auto&& [otherEndpoint, otherClient] : clients) {
				if (otherClient.player->playerID) {
					otherClient.connection.writeReliableMessage(ChatMessageForGameClient{
						.senderName = "[SERVER]",
						.message = formatString("Player {} joined the game.", client.player->playerID.value),
					});
				}
			}
		} else {
			for (auto&& [otherEndpoint, otherClient] : clients) {
				if (otherClient.player->playerID) {
					otherClient.connection.writeReliableMessage(ChatMessageForGameClient{
						.senderName = "[SERVER]",
						.message = formatString("Player {} added a local player.", client.player->playerID.value),
					});
				}
			}
		}

		client.player->localPlayerIDs.insert(upperBound(client.player->localPlayerIDs, message.localPlayerID), message.localPlayerID);

		phys::Position3D position{};
		phys::PitchYaw aimAngles{};
		const MapInfo& mapInfo = resources.getResource<MapInfo>();
		MapState& mapState = resources.getResource<MapState>();
		if (!mapInfo.spawnpoints.empty()) {
			const MapInfo::Spawnpoint& spawnpoint = mapInfo.spawnpoints[mapState.nextSpawnpointIndex++ % mapInfo.spawnpoints.size()];
			position = spawnpoint.position;
			aimAngles = spawnpoint.aimAngles;
		}

		const auto [playerEntityID, playerSynchronizedEntityID] =
			playerPrefab
				->spawn(registry, resources, EntityID::Flags{},
					ComponentInitializers{
						PlayerID{client.player->playerID},
						LocalPlayerID{message.localPlayerID},
					},
					position)
				.front();
		if (Aim* const aim = registry.findComponent<Aim>(playerEntityID)) {
			aim->angles = aimAngles;
		}
		if (firstJoin) {
			saveSnapshot(client.player->snapshots.push_back_unspecified_value(), gameState, client.player->playerID);
		}
		client.connection.writeReliableMessage(JoinedGameMessageForGameClient{
			.playerID = client.player->playerID,
			.localPlayerID = message.localPlayerID,
			.tickIndex = tickIndex,
		});
	}

	void handleMessage(Client& client, const LeaveGameRequestMessageForGameServer& message) {
		GREM_PROFILE_FUNCTION();

		if (!client.player) {
			return;
		}

		const auto itLocalPlayerID = lowerBound(client.player->localPlayerIDs, message.localPlayerID);
		if (itLocalPlayerID == client.player->localPlayerIDs.end() || *itLocalPlayerID != message.localPlayerID) {
			return;
		}

		client.player->localPlayerIDs.erase(itLocalPlayerID);
		if (client.player->localPlayerIDs.empty()) {
			removePlayer(client);
		} else {
			removeLocalPlayer(client, message.localPlayerID);
		}
		client.connection.writeReliableMessage(LeftGameMessageForGameClient{
			.localPlayerID = message.localPlayerID,
		});
	}

	void handleMessage(Client& client, const AcknowledgeSnapshotMessageForGameServer& message) {
		GREM_PROFILE_FUNCTION();

		const TickIndex tickIndex = gameState.getResources().getResource<TickIndex>();

		if (message.latestReceivedSnapshotTickIndex > tickIndex) {
			client.connection.close(make_error_code(std::errc::bad_message));
			return;
		}

		if (client.player && message.latestReceivedSnapshotTickIndex > client.player->latestAcknowledgedSnapshotTickIndex) {
			client.player->latestAcknowledgedSnapshotTickIndex = message.latestReceivedSnapshotTickIndex;
			client.player->eventBuffer.acknowledgeEventConfirmations(message.latestReceivedSnapshotTickIndex);
		}
	}

	void handleMessage(Client& client, LatestCommandsMessageForGameServer&& message) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		GREM_PROFILE_FUNCTION();

		constexpr TickCount MAX_COMMAND_TICKS_AHEAD = 256;

		if (message.tickCommands.empty()) {
			client.connection.close(make_error_code(std::errc::bad_message));
			return;
		}

		const TickIndex tickIndex = gameState.getResources().getResource<TickIndex>();
		const TickIndex maxEndTickIndex = tickIndex.getNext(MAX_COMMAND_TICKS_AHEAD);
		auto it = message.tickCommands.begin();
		auto end = message.tickCommands.end();
		TickDifference tickCommandCount = static_cast<TickDifference>(end - it);
		TickIndex commandTickIndex = message.firstCommandTickIndex;
		TickIndex endTickIndex = commandTickIndex + static_cast<TickDifference>(tickCommandCount);
		if (endTickIndex > maxEndTickIndex) {
			const TickDifference excessTickCommandCount = min(tickCommandCount, endTickIndex - maxEndTickIndex);
			end -= static_cast<ptrdiff_t>(excessTickCommandCount);
			tickCommandCount -= excessTickCommandCount;
			endTickIndex -= excessTickCommandCount;
		}
		if (tickCommandCount > CommandBuffer::MAX_SIZE) {
			const TickDifference excessTickCommandCount = tickCommandCount - CommandBuffer::MAX_SIZE;
			it += static_cast<ptrdiff_t>(excessTickCommandCount);
			commandTickIndex += static_cast<TickDifference>(excessTickCommandCount);
		}
		client.player->latestReceivedCommandTickIndex = max(client.player->latestReceivedCommandTickIndex, endTickIndex.getPrevious());
		while (it != end) {
			if (it->localPlayerCommands.size() > MAX_LOCAL_PLAYER_COUNT || !isSorted(it->localPlayerCommands) ||
				adjacentFind(it->localPlayerCommands) != it->localPlayerCommands.end() ||
				anyOf(it->localPlayerCommands, [&](const TickCommand::LocalPlayerCommand& localPlayerCommand) -> bool { return !isSorted(localPlayerCommand.subtickCommands); })) {
				client.connection.close(make_error_code(std::errc::bad_message));
				return;
			}
			client.player->commandBuffer.insertCommand(commandTickIndex, std::move(*it));
			++it;
			++commandTickIndex;
		}
	}

	void handleMessage(Client& client, const ChatMessageForGameServer& message) {
		GREM_PROFILE_FUNCTION();

		if (message.message.size() > MAX_CHAT_MESSAGE_SIZE) {
			client.connection.close(make_error_code(std::errc::bad_message));
			return;
		}

		if (!client.player) {
			return;
		}

		for (auto&& [otherEndpoint, otherClient] : clients) {
			if (otherClient.player) {
				otherClient.connection.writeReliableMessage(ChatMessageForGameClient{
					.senderName = formatString("Player {}", client.player->playerID.value),
					.message = message.message,
				});
			}
		}
	}

	void receiveIncomingPackets() {
		GREM_PROFILE_FUNCTION();

		const size_t maxReceivedPacketCount = (clients.size() + 1) * MAX_RECEIVED_PACKETS_PER_FRAME_PER_CLIENT;
		for (size_t i = 0; i < maxReceivedPacketCount; ++i) {
			PacketBuffer packetBuffer{};
			std::error_code errorCode{};
			const Optional<Pair<Span<byte>, net::Endpoint>> received = socket.receiveFrom(packetBuffer, errorCode);
			if (!received) {
				if (errorCode && errorCode != net::SocketError::WAIT) {
					eprintln("Server failed to receive packets: {}", errorCode.message());
				}
				break;
			}

			const auto& [packet, sender] = *received;
			if (const auto it = clients.find(sender); it != clients.end()) {
				Client& client = it->second;
				client.connection.receive(packet, [&]<typename Message>(Message&& message) -> void { handleMessage(client, std::forward<Message>(message)); });
				continue;
			}

			if (maxClientCount == 0) {
				continue;
			}

			if (clients.size() >= maxClientCount) {
				if (const auto it = findIf(clients,
						[&](const auto& kv) -> bool {
							return !kv.first.isAddressLoopback() && kv.second.timeSpentLoadingMap > mapLoadGracePeriod && !kv.second.connection.isDisconnecting();
						});
					it != clients.end()) {
					Client& client = it->second;
					client.connection.requestDisconnect();
				}
				eprintln("Server failed to add new client: {}", "Maximum client count exceeded.");
				continue;
			}

			try {
				Client client{};
				if (!client.connection.acceptConnection(packet)) {
					throw net::Error{client.connection.getErrorCode()};
				}
				client.connection.writeReliableMessage(LoadMapMessageForGameClient{
					.schemaFilepath{schemaFilepath},
					.mapFilepath{mapFilepath},
					.tickInterval = gameState.getResources().getResource<Duration>(),
				});
				for (const String& schemaExtension : schemaExtensions) {
					client.connection.writeReliableMessage(ExtendSchemaMessageForGameClient{.schemaExtension = schemaExtension});
				}
				clients.emplace(sender, std::move(client));
				eprintln("Server accepted a new client.");
			} catch (...) {
				eprintln("Server failed to add new client: {}", Error::formatCurrentExceptionMessage());
			}
		}
		cleanupClosedConnections();
	}

	void handleMapLoadTimeout(Duration deltaTime) {
		for (auto&& [endpoint, client] : clients) {
			if (!client.connection.isConnected()) {
				continue;
			}

			if (!client.player && !endpoint.isAddressLoopback() && countup(client.timeSpentLoadingMap, deltaTime, mapLoadTimeout)) {
				client.connection.requestDisconnect();
			}
		}
		cleanupClosedConnections();
	}

	void writeOutgoingMessages() {
		GREM_PROFILE_FUNCTION();

		const ResourceRegistry& resources = gameState.getResources();
		const CRC32 schemaCRC32 = resources.getResource<Schema>().getCRC32();
		const TickIndex newTickIndex = resources.getResource<TickIndex>();
		GREM_ASSERT(newTickIndex > TickIndex{});

		for (auto&& [endpoint, client] : clients) {
			if (client.ticksUntilNextSend > 0 || !client.player) {
				continue;
			}

			GREM_ASSERT(!client.player->snapshots.empty());

			gameStateUpdateMessage.schemaCRC32 = schemaCRC32;

			const TickDifference oldSnapshotReverseOffset = newTickIndex - client.player->latestAcknowledgedSnapshotTickIndex;
			GREM_ASSERT(oldSnapshotReverseOffset >= 1);
			const TickIndex oldTickIndex =
				(oldSnapshotReverseOffset > static_cast<TickDifference>(client.player->snapshots.size() - 1))
					? TickIndex{}
					: newTickIndex.getPrevious(static_cast<TickCount>(oldSnapshotReverseOffset));
			const Snapshot& oldSnapshot =
				(oldTickIndex == TickIndex{}) ? initialSnapshot : client.player->snapshots[client.player->snapshots.size() - 1 - static_cast<size_t>(oldSnapshotReverseOffset)];
			const Snapshot& newSnapshot = client.player->snapshots.back();
			GREM_ASSERT(newSnapshot.tickIndex > TickIndex{});

			buildSnapshotDelta(gameStateUpdateMessage.delta, oldSnapshot, newSnapshot);

			const EventBuffer::OutgoingEventConfirmations latestOutgoingEventConfirmations = client.player->eventBuffer.getLatestOutgoingEventConfirmations();
			gameStateUpdateMessage.confirmEvents.clear();
			for (auto it = latestOutgoingEventConfirmations.begin; it != latestOutgoingEventConfirmations.end; ++it) {
				gameStateUpdateMessage.confirmEvents.append_range(*it);
			}

			gameStateUpdateMessage.latestReceivedCommandTickIndex = client.player->latestReceivedCommandTickIndex;
			gameStateUpdateMessage.recentPacketLossFraction = client.connection.getRecentIncomingPacketLossFraction();

			if (oldTickIndex == TickIndex{}) {
				if (!client.player->sendingReliableSnapshot) {
					client.player->sendingReliableSnapshot = true;
					client.connection.writeReliableMessage(gameStateUpdateMessage);
				} else {
					client.connection.writeUnreliableMessage(gameStateUpdateMessage);
				}
			} else {
				client.player->sendingReliableSnapshot = false;
				client.connection.writeUnreliableMessage(gameStateUpdateMessage);
			}
		}
		cleanupClosedConnections();
	}

	void sendOutgoingPackets() {
		GREM_PROFILE_FUNCTION();

		const ResourceRegistry& resources = gameState.getResources();
		const Duration tickInterval = resources.getResource<Duration>();
		for (auto&& [endpoint, client] : clients) {
			if (client.ticksUntilNextSend > 0) {
				--client.ticksUntilNextSend;
				continue;
			}

			size_t bytesSent = 0;
			client.connection.send([&](Span<const byte> packet) -> bool {
				std::error_code errorCode{};
				socket.sendTo(endpoint, packet, errorCode);
				if (errorCode && errorCode != net::SocketError::WAIT) {
					client.connection.close(errorCode);
					return false;
				}
				bytesSent += packet.size();
				return true;
			});

			const size_t bytesSentPerSecond = static_cast<size_t>(duration_cast<Duration>(Seconds{bytesSent}) / tickInterval);
			client.ticksUntilNextSend = min(bytesSentPerSecond / maxOutgoingDataRatePerClient, maxSendSkipTicks);
		}
		cleanupClosedConnections();
	}

	void cleanupClosedConnections() {
		for (const auto& [endpoint, client] : clients) {
			if (client.player && !client.connection.isConnected()) {
				removePlayer(client);
			}
			if (client.connection.isClosed()) {
				if (const std::error_code errorCode = client.connection.getErrorCode()) {
					eprintln("Server closed client {}: {}", endpoint, errorCode.message());
				} else {
					eprintln("Server closed a disconnected client.");
				}
			}
		}
		erase_if(clients, [](const auto& kv) -> bool { return kv.second.connection.isClosed(); });
	}

	void removePlayer(Client& client) {
		GREM_ASSERT(client.player);

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
		PlayerEntityMap& playerEntityMap = resources.getResource<PlayerEntityMap>();

		for (auto&& [otherEndpoint, otherClient] : clients) {
			if (otherClient.player && otherClient.player->playerID != client.player->playerID) {
				otherClient.connection.writeReliableMessage(ChatMessageForGameClient{
					.senderName = "[SERVER]",
					.message = formatString("Player {} left the game.", client.player->playerID.value),
				});
			}
		}
		playerEntityMap.forEachPlayerEntity(client.player->playerID, [&](EntityID entityID) -> void {
			if (const SynchronizedEntityID* const synchronizedEntityID = registry.findComponent<SynchronizedEntityID>(entityID)) {
				synchronizedEntityMap.synchronizedEntityMappings.erase(*synchronizedEntityID);
			}
			registry.destroyEntity(entityID);
		});
		playerEntityMap.playerEntityIDs.erase(client.player->playerID);
		client.player.reset();
	}

	void removeLocalPlayer(Client& client, LocalPlayerID localPlayerID) {
		GREM_ASSERT(client.player);

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
		PlayerEntityMap& playerEntityMap = resources.getResource<PlayerEntityMap>();

		for (auto&& [otherEndpoint, otherClient] : clients) {
			if (otherClient.player) {
				otherClient.connection.writeReliableMessage(ChatMessageForGameClient{
					.senderName = "[SERVER]",
					.message = formatString("Player {} removed a local player.", client.player->playerID.value),
				});
			}
		}

		auto [it, last] = playerEntityMap.playerEntityIDs.equal_range(client.player->playerID);
		size_t count = static_cast<size_t>(last - it);
		while (count-- > 0) {
			if (it->first == client.player->playerID && it->second.first == localPlayerID) {
				if (const SynchronizedEntityID* const synchronizedEntityID = registry.findComponent<SynchronizedEntityID>(it->second.second)) {
					synchronizedEntityMap.synchronizedEntityMappings.erase(*synchronizedEntityID);
				}
				registry.destroyEntity(it->second.second);
				it = playerEntityMap.playerEntityIDs.erase(it);
			} else {
				++it;
			}
		}
	}

	GREM_PROFILE_CONSTRUCTOR_BEGIN();
	AssetCache& assetCache;
	GameState& gameState;
	net::UDPSocket socket;
	String schemaFilepath;
	String mapFilepath;
	ArrayList<String> schemaExtensions{};
	Schema schema{EntityID::Flags{}};
	Snapshot initialSnapshot{};
	PlayerID nextPlayerID{.value = 1};
	size_t maxClientCount;
	size_t maxLocalPlayerCountPerClient;
	Clients clients{};
	size_t maxTicksPerFrame;
	size_t maxOutgoingDataRatePerClient;
	size_t maxSendSkipTicks;
	Duration mapLoadGracePeriod;
	Duration mapLoadTimeout;
	UpdateGameStateMessageForGameClient gameStateUpdateMessage{};
	GameServer::PerformanceStats performanceStats{};
	SharedPointer<Prefab> playerPrefab{};
};

GameServer::GameServer(AssetCache& assetCache, GameState& gameState, const net::Endpoint& endpoint, String schemaFilepath, String mapFilepath, const GameServerOptions& options)
	: implementation(UniquePointer<Implementation>::create(assetCache, gameState, endpoint, std::move(schemaFilepath), std::move(mapFilepath), options)) {}

GameServer::~GameServer() = default;

void GameServer::shutdown() {
	GREM_PROFILE_FUNCTION();

	implementation->shutdown();
}

void GameServer::reloadAssets() {
	GREM_PROFILE_FUNCTION();

	implementation->reloadAssets();
}

void GameServer::update(size_t tickCount, phys::DebugVisualization3D* physicsDebugVisualization) {
	GREM_PROFILE_FUNCTION();

	implementation->update(tickCount, physicsDebugVisualization);
}

bool GameServer::isShuttingDown() const noexcept {
	return implementation->isShuttingDown();
}

size_t GameServer::getClientCount() const noexcept {
	return implementation->getClientCount();
}

Duration GameServer::getTickInterval() const noexcept {
	return implementation->getTickInterval();
}

const GameServer::PerformanceStats& GameServer::getPerformanceStats() const noexcept {
	return implementation->getPerformanceStats();
}
