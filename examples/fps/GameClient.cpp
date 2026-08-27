// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "GameClient.hpp"

#include <GREM/aliases.hpp>
#include <GREM/application/FrameInfo.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formats/gltf.hpp>
#include <GREM/core/formats/obj.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/system/NativeFilesystem.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/Input.hpp>
#include <GREM/events/InputManager.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics_2d/Camera2D.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/networking/Socket.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Image.hpp>

#include "AssetCache.hpp"
#include "Audio.hpp"
#include "ClientAudioStats.hpp"
#include "ClientConnectionStats.hpp"
#include "ClientPerformanceStats.hpp"
#include "ClientReceivedChatMessages.hpp"
#include "ClientReceivedSnapshotBuffer.hpp"
#include "ClientSettings.hpp"
#include "ClientState.hpp"
#include "Connection.hpp"
#include "EntityCallbacks.hpp"
#include "GameState.hpp"
#include "GameSystems.hpp"
#include "Graphics.hpp"
#include "MessageForGameClient.hpp"
#include "MessageForGameServer.hpp"
#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "Snapshot.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "Timestamp.hpp"
#include "WorldView.hpp"
#include "game_actions.hpp"
#include "game_components.hpp"
#include "game_events.hpp"
#include "game_map.hpp"
#include "game_resources.hpp"

#include <imgui.h>      // Im...
#include <system_error> // std::error_code
#include <utility>      // std::move, std::forward, std::exchange

namespace {

constexpr size_t MAX_RECEIVED_PACKETS_PER_FRAME = 512;

constexpr Duration FALLBACK_TICK_INTERVAL = 50_milliseconds;

constexpr Duration PREDICTION_DURATION_MARGIN = 5_milliseconds;
constexpr Duration MAX_DESIRED_PREDICTION_DURATION = 300_milliseconds;
constexpr Duration PREDICTION_TIME_ADJUSTMENT_DURATION = 400_milliseconds;
constexpr Duration PREDICTION_DURATION_PROBLEM_THRESHOLD = MAX_DESIRED_PREDICTION_DURATION + (PREDICTION_TIME_ADJUSTMENT_DURATION + PREDICTION_DURATION_MARGIN) * 2;

constexpr Duration MAX_EXTRAPOLATION_TIME = 50_milliseconds;
constexpr Duration RECEIVE_INTERPOLATION_OFFSET_ADJUSTMENT_DURATION = 500_milliseconds;

using ConnectionToServer = Connection<MessageForGameClient, MessageForGameServer>;

struct PredictionStats {
	phys::Position3D predictedPosition;
	phys::Time predictedReloadTimeRemaining;
	phys::PitchYaw predictedAimAngles;
	phys::Coefficient predictedAimingDownSightsAmount;
};

struct LocalPlayer {
	LocalPlayerID localPlayerID;
	phys::Quantity<1, phys::Degrees> fieldOfView = 90_degrees;
	phys::Scale2D aimSensitivity{1_x};
	phys::PitchYawRates turnSensitivity{200_degrees_per_second};
	evt::InputManager inputManager{{.emitOutputEvents = true}};
	Optional<uint32_t> controllerID;
	phys::PitchYaw visualAimAngles{};
	phys::PitchYawRotations aimRotationsSinceLastTick{};
	phys::PitchYawRotations uncommittedAimRotations{};
	Duration latestUncommittedAimRotationsTimeOffset{};
	phys::Distance aimDistance{};
	Duration stepSpamTimer{};
	Duration crateSpamTimer{};
	Frustum<float> frustum{};
	Region2D viewRegion{};
	gfx::Viewport viewport{};
	gfx::Camera3D camera;

	LocalPlayer(Filesystem& filesystem, gfx::Device& device, CStringView configurationFilepath, LocalPlayerID localPlayerID, Optional<uint32_t> controllerID)
		: localPlayerID(localPlayerID)
		, controllerID(controllerID)
		, camera(device) {
		if (Optional<String> fileContents = filesystem.tryReadInputFileString(configurationFilepath)) {
			const auto getOutputIndex = [](StringView actionName) -> Optional<evt::InputManager::OutputIndex> {
				if (const Optional<Action> action = findActionByIdentifier(actionName)) {
					return static_cast<evt::InputManager::OutputIndex>(*action);
				}
				return {};
			};
			const auto readExtraProperty = [&](StringView key, json::Reader& reader) -> void {
				if (key == "fieldOfView") {
					fieldOfView = reader.readNumber<float>() * phys::DEGREES;
				} else if (key == "aimSensitivityPitch") {
					aimSensitivity.setX(reader.readNumber<float>());
				} else if (key == "aimSensitivityYaw") {
					aimSensitivity.setY(reader.readNumber<float>());
				} else if (key == "turnSensitivityPitch") {
					turnSensitivity.setX(reader.readNumber<float>() * phys::DEGREES_PER_SECOND);
				} else if (key == "turnSensitivityYaw") {
					turnSensitivity.setY(reader.readNumber<float>() * phys::DEGREES_PER_SECOND);
				}
			};
			inputManager.loadConfiguration(std::move(*fileContents), configurationFilepath, getOutputIndex, readExtraProperty);
		} else {
			for (const auto& [action, defaultInputs] : getDefaultActionInputs()) {
				for (const evt::Input input : defaultInputs) {
					inputManager.addBinding(input, action);
				}
			}
			saveConfiguration(filesystem, configurationFilepath);
		}
	}

	void saveConfiguration(Filesystem& filesystem, CStringView configurationFilepath) const {
		const auto getActionName = [](evt::InputManager::OutputIndex outputIndex) -> Optional<String> {
			if (outputIndex < meta::enum_size_v<Action>) {
				return String{getActionIdentifier(static_cast<Action>(outputIndex))};
			}
			return {};
		};
		const Array extraProperties{
			Pair<StringView, json::Variant>{"fieldOfView", static_cast<float>(fieldOfView.in(phys::DEGREES))},
			Pair<StringView, json::Variant>{"aimSensitivityPitch", static_cast<float>(aimSensitivity.getX())},
			Pair<StringView, json::Variant>{"aimSensitivityYaw", static_cast<float>(aimSensitivity.getY())},
			Pair<StringView, json::Variant>{"turnSensitivityPitch", static_cast<float>(turnSensitivity.getX().in(phys::DEGREES_PER_SECOND))},
			Pair<StringView, json::Variant>{"turnSensitivityYaw", static_cast<float>(turnSensitivity.getY().in(phys::DEGREES_PER_SECOND))},
		};
		inputManager.saveConfiguration(filesystem, configurationFilepath, getActionName, extraProperties);
	}
};

class PredictedEventBuffer {
private:
	struct PredictedEvent {
		struct Compare {
			[[nodiscard]] bool operator()(const PredictedEvent& a, const PredictedEvent& b) const {
				return a.tickIndex < b.tickIndex;
			}

			[[nodiscard]] bool operator()(const PredictedEvent& a, TickIndex b) const {
				return a.tickIndex < b;
			}

			[[nodiscard]] bool operator()(TickIndex a, const PredictedEvent& b) const {
				return a < b.tickIndex;
			}
		};

		TickIndex tickIndex;
		Event event;
		bool confirmed;
	};

public:
	[[nodiscard]] bool predictEvent(TickIndex tickIndex, const Event& event) {
		GREM_ASSERT(isSorted(events, PredictedEvent::Compare{}));

		if (tickIndex < firstUnconfirmedTickIndex) {
			return false;
		}

		auto it = lowerBound(events, tickIndex, PredictedEvent::Compare{});
		while (it != events.end() && it->tickIndex == tickIndex) {
			if (it->event == event) {
				return false;
			}
			++it;
		}
		events.insert(it, PredictedEvent{.tickIndex = tickIndex, .event = event, .confirmed = false});
		return true;
	}

	[[nodiscard]] bool confirmEvent(TickIndex tickIndex, const Event& event) {
		GREM_ASSERT(isSorted(events, PredictedEvent::Compare{}));

		if (tickIndex < firstUnconfirmedTickIndex) {
			return false;
		}

		bool found = false;
		auto it = lowerBound(events, tickIndex, PredictedEvent::Compare{});
		while (it != events.end() && it->tickIndex == tickIndex) {
			if (it->event == event) {
				it->confirmed = true;
				found = true;
			}
			++it;
		}
		if (!found) {
			events.insert(it, PredictedEvent{.tickIndex = tickIndex, .event = event, .confirmed = true});
			return true;
		}
		return false;
	}

	void cancelUnconfirmedEventsUntil(TickIndex tickIndex, FunctionView<void(TickIndex tickIndex, const Event& event)> cancelEvent) {
		GREM_ASSERT(isSorted(events, PredictedEvent::Compare{}));

		if (tickIndex > firstUnconfirmedTickIndex) {
			while (!events.empty() && events.front().tickIndex < tickIndex) {
				const PredictedEvent& predictedEvent = events.front();
				if (!predictedEvent.confirmed) {
					cancelEvent(predictedEvent.tickIndex, predictedEvent.event);
				}
				events.pop_front();
			}
			firstUnconfirmedTickIndex = tickIndex;
		}
	}

	void reset(FunctionView<void(TickIndex tickIndex, const Event& event)> cancelEvent) {
		GREM_ASSERT(isSorted(events, PredictedEvent::Compare{}));

		while (!events.empty()) {
			const PredictedEvent& predictedEvent = events.front();
			cancelEvent(predictedEvent.tickIndex, predictedEvent.event);
			events.pop_front();
		}
		firstUnconfirmedTickIndex = {};
	}

private:
	DoubleEndedQueue<PredictedEvent> events{};
	TickIndex firstUnconfirmedTickIndex{};
};

struct Prediction {
	PredictedEventBuffer eventBuffer{};
	SnapshotBuffer snapshots{};
	Duration subtickBeginTimeOffset{};
	Duration fallbackTickTimer{};

	void reset(Audio& audio, Graphics& graphics, GameState& gameState) noexcept {
		eventBuffer.reset([&](TickIndex tickIndex, const Event& event) -> void { gameState.cancelEvent(audio, graphics, tickIndex, event); });
		snapshots.clear();
		subtickBeginTimeOffset = {};
		fallbackTickTimer = {};
	}
};

[[nodiscard]] PredictionStats getPredictionStats(const EntityRegistry& registry, const ResourceRegistry& resources, PlayerID playerID) {
	PredictionStats result{};
	const SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
	resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, LocalPlayerID{.value = 1}, [&](EntityID entityID) -> bool {
		if (const Inventory* const playerInventory = registry.findComponent<Inventory>(entityID)) {
			if (const EntityID weaponEntityID = synchronizedEntityMap.findEntity(registry, playerInventory->equippedWeapon)) {
				const phys::Position3D* const playerPosition = registry.findComponent<phys::Position3D>(entityID);
				const Aim* const playerAim = registry.findComponent<Aim>(entityID);
				const WeaponState* const weaponState = registry.findComponent<WeaponState>(weaponEntityID);
				if (playerPosition && playerAim && weaponState) {
					result = {
						.predictedPosition = *playerPosition,
						.predictedReloadTimeRemaining = weaponState->reloadTimeRemaining,
						.predictedAimAngles = playerAim->angles,
						.predictedAimingDownSightsAmount = weaponState->aimingDownSightsAmount,
					};
					return true;
				}
			}
		}
		return false;
	});
	return result;
}

void receiveIncomingPackets(net::UDPSocket& socket, const net::Endpoint& endpoint, ConnectionToServer& connection, ClientConnectionStats& connectionStats, auto handleMessage) {
	GREM_PROFILE_FUNCTION();

	for (size_t i = 0; i < MAX_RECEIVED_PACKETS_PER_FRAME; ++i) {
		PacketBuffer packetBuffer{};
		if (const Optional<Pair<Span<byte>, net::Endpoint>> received = socket.receiveFrom(packetBuffer)) {
			const auto& [packet, sender] = *received;
			if (sender == endpoint) {
				if (!connection.receive(packet, handleMessage)) {
					if (const std::error_code errorCode = connection.getErrorCode()) {
						throw net::Error{connection.getErrorCode()};
					}
					return;
				}
				connectionStats.incomingDataRateAccumulator += packet.size();
				connectionStats.incomingDataPerTickAccumulator += packet.size();
			} else {
				eprintln("Client received packet from unknown sender: {}", sender);
			}
		} else {
			break;
		}
	}
	connectionStats.roundTripTimeStatistics = connection.getRecentRoundTripTimeStatistics();
	connectionStats.recentIncomingPacketLossFraction = connection.getRecentIncomingPacketLossFraction();
}

void runPredictedTick(Audio& audio, Graphics& graphics, GameState& gameState, PredictedEventBuffer& eventBuffer, PlayerID playerID, SnapshotBufferView receivedSnapshots,
	SnapshotBufferView predictionSnapshots, const TickCommand& tickCommand) {
	GREM_PROFILE_FUNCTION();

	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();
	Events& events = resources.getResource<Events>();
	const TickIndex tickIndex = resources.getResource<TickIndex>();
	const Duration tickInterval = resources.getResource<Duration>();

	tickCommand.beginTick(gameState, playerID);
	tickCommand.runSubtick(gameState, playerID, receivedSnapshots, predictionSnapshots, Duration{}, tickInterval);
	tickCommand.endTick(gameState, playerID);

	if (!resources.getResource<SessionState>().flags.contains(SessionState::PAUSED)) {
		gameState.tick();
	}
	++resources.getResource<TickIndex>();

	resources.getResource<SynchronizedEntityMap>().removeDestroyedEntities(registry);
	resources.getResource<PlayerEntityMap>().update(registry);

	for (const Event& event : events) {
		if (const Optional<Event> filteredEvent = event.filter(gameState, playerID)) {
			if (filteredEvent->isPredicted(playerID)) {
				if (eventBuffer.predictEvent(tickIndex, *filteredEvent)) {
					gameState.emitEvent(audio, graphics, tickIndex, *filteredEvent);
				}
			}
		}
	}
	events.clear();
}

void runUpcomingSubtickPrediction(Audio& audio, Graphics& graphics, GameState& gameState, PredictedEventBuffer& eventBuffer, PlayerID playerID,
	SnapshotBufferView receivedSnapshots, SnapshotBufferView predictionSnapshots, const TickCommand& nextTickCommand, Duration oldTimeOffset, Duration newTimeOffset) {
	GREM_PROFILE_FUNCTION();

	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();
	Events& events = resources.getResource<Events>();
	const TickIndex tickIndex = resources.getResource<TickIndex>();

	nextTickCommand.runSubtick(gameState, playerID, receivedSnapshots, predictionSnapshots, oldTimeOffset, newTimeOffset);

	resources.getResource<SynchronizedEntityMap>().removeDestroyedEntities(registry);
	resources.getResource<PlayerEntityMap>().update(registry);

	for (const Event& event : events) {
		if (const Optional<Event> filteredEvent = event.filter(gameState, playerID)) {
			if (filteredEvent->isPredicted(playerID)) {
				if (eventBuffer.predictEvent(tickIndex, *filteredEvent)) {
					gameState.emitEvent(audio, graphics, tickIndex, *filteredEvent);
				}
			}
		}
	}
	events.clear();
}

void advanceCommandBufferUntil(LatestCommandsMessageForGameServer& latestCommands, TickCommand& nextTickCommand, Duration tickInterval, TickIndex newTickIndex) {
	GREM_ASSERT(newTickIndex >= latestCommands.firstCommandTickIndex);
	while (latestCommands.firstCommandTickIndex + static_cast<TickDifference>(latestCommands.tickCommands.size()) < newTickIndex) {
		// Write a new command.
		TickCommand& tickCommand = latestCommands.tickCommands.push_back_unspecified_value();
		tickCommand.receivedInterpolationTimestampAtTickBegin = nextTickCommand.receivedInterpolationTimestampAtTickBegin;
		tickCommand.localPlayerCommands.resize(nextTickCommand.localPlayerCommands.size());
		for (size_t i = 0; i < nextTickCommand.localPlayerCommands.size(); ++i) {
			TickCommand::LocalPlayerCommand& localPlayerCommand = tickCommand.localPlayerCommands[i];
			TickCommand::LocalPlayerCommand& nextTickLocalPlayerCommand = nextTickCommand.localPlayerCommands[i];
			localPlayerCommand.localPlayerID = nextTickLocalPlayerCommand.localPlayerID;
			localPlayerCommand.desiredDirectionScale = nextTickLocalPlayerCommand.desiredDirectionScale;
			localPlayerCommand.aimRotationRates = nextTickLocalPlayerCommand.aimRotationRates;

			// Take one tick's worth of upcoming sub-tick commands and move them into the new command.
			const auto subtickCommandsEnd = lowerBound(nextTickLocalPlayerCommand.subtickCommands, tickInterval);
			localPlayerCommand.subtickCommands.assign(nextTickLocalPlayerCommand.subtickCommands.begin(), subtickCommandsEnd);
			nextTickLocalPlayerCommand.subtickCommands.erase(nextTickLocalPlayerCommand.subtickCommands.begin(), subtickCommandsEnd);
			// Subtract one tick of time from the remaining subtick commands.
			for (SubtickCommand& subtickCommand : nextTickLocalPlayerCommand.subtickCommands) {
				subtickCommand.timeOffset -= tickInterval;
			}
		}

		nextTickCommand.receivedInterpolationTimestampAtTickBegin.addTicks(1);
	}
}

void reportPredictionErrors(ClientConnectionStats& connectionStats, const PredictionStats& a, const PredictionStats& b) {
	connectionStats.positionPredictionError = distance(a.predictedPosition, b.predictedPosition);
	connectionStats.reloadTimeRemainingPredictionError = a.predictedReloadTimeRemaining - b.predictedReloadTimeRemaining;
	connectionStats.aimAnglesPredictionError = distance(a.predictedAimAngles, b.predictedAimAngles);
	connectionStats.aimingDownSightsPredictionError = a.predictedAimingDownSightsAmount - b.predictedAimingDownSightsAmount;
	if (connectionStats.positionPredictionError > 0.05_meters) {
		eprintln("Position prediction error: {:.8f}", connectionStats.positionPredictionError);
	}
	if (abs(connectionStats.reloadTimeRemainingPredictionError) > 0.00001_seconds) {
		eprintln("Reload time remaining prediction error: {:.8f}", connectionStats.reloadTimeRemainingPredictionError);
	}
	if (connectionStats.aimAnglesPredictionError > 0.00001_radians) {
		eprintln("Aim angles prediction error: {:.8f}", connectionStats.aimAnglesPredictionError);
	}
	if (abs(connectionStats.aimingDownSightsPredictionError) > 0.00001_x) {
		eprintln("Aiming down sights prediction error: {:+.8f}", connectionStats.aimingDownSightsPredictionError);
	}
}

TickIndex rollbackPredictionToReceivedBaseSnapshot(GameState& gameState, const ClientReceivedSnapshotBuffer& receivedSnapshotBuffer) {
	GREM_PROFILE_FUNCTION();

	const TickIndex tickIndex = gameState.getResources().getResource<TickIndex>();

	const SnapshotBufferView receivedSnapshots = receivedSnapshotBuffer.getSnapshots();
	if (receivedSnapshots.empty()) {
		return tickIndex;
	}

	ptrdiff_t baseSnapshotIndex = static_cast<ptrdiff_t>(receivedSnapshots.size() - 1);
	while (baseSnapshotIndex > 0 &&
		   (receivedSnapshots[baseSnapshotIndex].tickIndex > tickIndex || !receivedSnapshotBuffer.isSnapshotReceived(receivedSnapshots[baseSnapshotIndex].tickIndex))) {
		--baseSnapshotIndex;
	}

	const Snapshot& baseSnapshot = receivedSnapshots[baseSnapshotIndex];
	const TickIndex baseSnapshotTickIndex = baseSnapshot.tickIndex;
	if (baseSnapshotTickIndex > tickIndex) {
		return tickIndex;
	}

	loadSnapshot(gameState, baseSnapshot);
	return baseSnapshotTickIndex;
}

void runPredictionUntil(Audio& audio, Graphics& graphics, GameState& gameState, PredictedEventBuffer& eventBuffer, LatestCommandsMessageForGameServer& latestCommands,
	ClientPerformanceStats& performanceStats, ClientConnectionStats& connectionStats, PlayerID playerID, SnapshotBufferView receivedSnapshots,
	SnapshotBufferView predictionSnapshots, TickIndex newTickIndex) {
	constexpr phys::CollisionLayer UNPREDICTED_OBJECT_LAYER = phys::CollisionLayer::MAX;

	GREM_PROFILE_FUNCTION();

	EntityRegistry& registry = gameState.getRegistry();
	ResourceRegistry& resources = gameState.getResources();

	TickIndex tickIndex = resources.getResource<TickIndex>();
	const Duration tickInterval = resources.getResource<Duration>();
	if (tickIndex < newTickIndex) {
		// Add all non-predicted objects to the unpredicted object layer.
		for (auto&& [objectID, collider] : registry.getEntities<phys::Collider3D>()) {
			if ((objectID.getFlags() & ENTITY_PHYSICS_PREDICTED) == 0) {
				collider.filter.layers |= UNPREDICTED_OBJECT_LAYER;
			}
		}

		// Find all physics objects colliding with a player and remove them from the unpredicted object layer.
		resources.getResource<PlayerEntityMap>().forEachPlayerEntity(playerID, [&](EntityID entityID) -> void {
			const phys::Collider3D* const playerCollider = registry.findComponent<phys::Collider3D>(entityID);
			const MovementType* const playerMovementType = registry.findComponent<MovementType>(entityID);
			const phys::Position3D* const playerPosition = registry.findComponent<phys::Position3D>(entityID);
			const phys::Orientation3D* const playerOrientation = registry.findComponent<phys::Orientation3D>(entityID);
			const phys::Scale3D* const playerScale = registry.findComponent<phys::Scale3D>(entityID);
			const phys::LinearVelocity3D* const playerLinearVelocity = registry.findComponent<phys::LinearVelocity3D>(entityID);
			if (playerCollider && playerMovementType && playerPosition && playerOrientation && playerScale && playerLinearVelocity) {
				const phys::Broadphase3D& broadphase = resources.getResource<phys::Broadphase3D>();
				const MovementDescription& movementDescription = resources.getResource<Schema>().getMovementDescription(*playerMovementType);
				const phys::Box3D playerBoundingBox =
					phys::ShapeView{playerCollider->shape}
						.getBoundingBox(translateRotateScale(*playerPosition, *playerOrientation, *playerScale))
						.value_or(phys::Box3D{.min{}, .max{}});
				const phys::Coefficient playerScaleExpansionFactor = 1.1_x;
				const phys::Distance playerAdditionalExpansion =
					movementDescription.baseSpeed * movementDescription.sprintSpeedCoefficient * movementDescription.accelerationDuration;
				const phys::Scale3D playerAdditionalExpansionFactor = 2_x * playerAdditionalExpansion / (playerBoundingBox.max - playerBoundingBox.min);
				const phys::Scale3D expandedPlayerScale =
					*playerScale * playerScaleExpansionFactor + select(isfinite(playerAdditionalExpansionFactor), playerAdditionalExpansionFactor, phys::Scale3D{});
				const phys::Transformation3D transformation = translateRotateScale(*playerPosition, *playerOrientation, expandedPlayerScale);
				const phys::Speed speed = length(*playerLinearVelocity);
				if (speed > phys::Speed::MACHINE_EPSILON && playerCollider->shape.isConvexShapeType()) {
					const phys::Direction3D direction = phys::Direction3D::reinterpret(*playerLinearVelocity / speed);
					const phys::Distance maxDistance = speed * (static_cast<Duration::rep>(newTickIndex - tickIndex) * tickInterval);
					broadphase.shapecast(
						phys::ConvexShapeView{playerCollider->shape}, playerCollider->filter, transformation, direction, maxDistance,
						registry.getEntities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D>(),
						resources.getResource<phys::SimulationOptions3D>().collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
						[&](const phys::Broadphase3D::ShapecastResult& hit) -> bool {
							if (phys::Collider3D* const hitObjectCollider = registry.findComponent<phys::Collider3D>(hit.objectID)) {
								hitObjectCollider->filter.layers &= ~UNPREDICTED_OBJECT_LAYER;
							}
							return false;
						},
						[&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; });
				} else {
					broadphase.testShape(
						phys::Length1D{}, playerCollider->shape, playerCollider->filter, transformation,
						registry.getEntities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D>(),
						resources.getResource<phys::SimulationOptions3D>().collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
						[&](EntityID hitObjectID, phys::CollisionFilterTestResult) -> bool {
							if (phys::Collider3D* const hitObjectCollider = registry.findComponent<phys::Collider3D>(hitObjectID)) {
								hitObjectCollider->filter.layers &= ~UNPREDICTED_OBJECT_LAYER;
							}
							return false;
						},
						[&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; });
				}
			}
		});

		// Make any correctable unpredicted objects that aren't colliding with the player uncorrectable.
		for (auto&& [objectID, activity, linearVelocity, angularVelocity, gravityAcceleration, collider] :
			registry.getEntities<phys::ObjectActivity, phys::LinearVelocity3D, phys::AngularVelocity3D, phys::LinearAcceleration3D, phys::Collider3D>()) {
			if (activity.isCorrectable != 0 && collider.filter.layers.contains(UNPREDICTED_OBJECT_LAYER)) {
				activity.isCorrectable = 0;

				if (activity.wasCorrected == 0) {
					// If the object's movement is unobstructed, extrapolate its position, but don't detect collisions.
					collider.filter.detectionLayers = {};
					collider.filter.responseLayers = {};
				} else {
					// If the object's movement is obstructed, freeze its position and detect collisions.
					linearVelocity = {};
					angularVelocity = {};
					gravityAcceleration = {};
					activity.energyLevel = 0;
				}
			}

			collider.filter.layers &= ~UNPREDICTED_OBJECT_LAYER;
		}

		performanceStats.latestPhysicsTime = {};
		do {
			GREM_ASSERT(resources.getResource<TickIndex>() == tickIndex);
			GREM_ASSERT(tickIndex >= latestCommands.firstCommandTickIndex);
			const size_t tickCommandIndex = static_cast<size_t>(tickIndex - latestCommands.firstCommandTickIndex);
			GREM_ASSERT(tickCommandIndex < latestCommands.tickCommands.size());
			const TickCommand& tickCommand = latestCommands.tickCommands[tickCommandIndex];
			runPredictedTick(audio, graphics, gameState, eventBuffer, playerID, receivedSnapshots, predictionSnapshots, tickCommand);
			if (const TickPerformanceStats* const tickPerformanceStats = resources.findResource<TickPerformanceStats>()) {
				performanceStats.latestPhysicsTime += tickPerformanceStats->latestPhysicsTime;
			}
			if (!predictionSnapshots.empty() && tickIndex.getNext() == predictionSnapshots.back().tickIndex) {
				const PredictionStats oldPredictionStats = getPredictionStats(predictionSnapshots.back().registry, predictionSnapshots.back().resources, playerID);
				const PredictionStats newPredictionStats = getPredictionStats(registry, resources, playerID);
				reportPredictionErrors(connectionStats, oldPredictionStats, newPredictionStats);
			}
			++tickIndex;
		} while (tickIndex < newTickIndex);
	} else if (!predictionSnapshots.empty() && tickIndex.getNext() == predictionSnapshots.back().tickIndex) {
		const PredictionStats oldPredictionStats = getPredictionStats(predictionSnapshots.back().registry, predictionSnapshots.back().resources, playerID);
		const PredictionStats newPredictionStats = getPredictionStats(registry, resources, playerID);
		reportPredictionErrors(connectionStats, oldPredictionStats, newPredictionStats);
	}
	GREM_ASSERT(tickIndex == newTickIndex);
}

void sendOutgoingPackets(net::UDPSocket& socket, const net::Endpoint& endpoint, ConnectionToServer& connection, ClientConnectionStats& connectionStats) {
	GREM_PROFILE_FUNCTION();

	if (!connection.send([&](Span<const byte> packet) -> bool {
			connectionStats.outgoingDataRateAccumulator += packet.size();
			connectionStats.outgoingDataPerTickAccumulator += packet.size();

			std::error_code errorCode{};
			socket.sendTo(endpoint, packet, errorCode);
			if (errorCode && errorCode != net::SocketError::WAIT) {
				connection.close(errorCode);
				return false;
			}
			return true;
		})) {
		if (const std::error_code errorCode = connection.getErrorCode()) {
			throw net::Error{connection.getErrorCode()};
		}
	}
}

} // namespace

class GameClient::Implementation {
public:
	Implementation(AssetCache& assetCache, Audio& audio, Graphics& graphics, const GameSystems& gameSystems, exec::Executor& executor, GameState& gameState,
		const net::Endpoint& endpoint, const GameClientOptions& options)
		: assetCache(assetCache)
		, audio(audio)
		, graphics(graphics)
		, gameSystems(gameSystems)
		, executor(executor)
		, gameState(gameState)
		, endpoint(endpoint)
		, settingsFilepath(options.settingsFilepath)
		, settings(options.settings)
#ifdef GREM_USE_PROFILING
		, captureLoadTimeProfile(options.captureLoadTimeProfile)
#endif
	{
		audio.soundStage.setOutputVolume(settings.audio.outputVolume);

		if (settings.graphics.verticalRenderResolution == 0) {
			try {
				settings.graphics.verticalRenderResolution = gfx::Display::getPrimary().getDesktopDisplayMode().getSize().height;
			} catch (...) {
				settings.graphics.verticalRenderResolution = graphics.swapchain.getHeight();
			}
		}

		ResourceRegistry& resources = gameState.getResources();

		resources.addExternalResource<Schema>(&schema);
		resources.addSharedResource<EntityCallbacks>();
		resources.addSharedResource<Events>();
		resources.addSharedResource<MapInfo>();
		resources.addResource<SynchronizedEntityMap>();
		resources.addResource<PlayerEntityMap>();
		resources.addResource<TickIndex>();
		resources.addResource<Duration>();
		resources.addExternalResource<PlayerID>(&receivedPlayerID);
		resources.addExternalResource<ClientSettings>(&settings);
		resources.addExternalResource<ClientState>(&state);
		resources.addExternalResource<ClientPerformanceStats>(&performanceStats);
		resources.addExternalResource<ClientConnectionStats>(&connectionStats);
		resources.addExternalResource<ClientAudioStats>(&audioStats);
		resources.addExternalResource<ClientReceivedSnapshotBuffer>(&receivedSnapshotBuffer);
		resources.addExternalResource<ClientReceivedChatMessages>(&receivedChatMessages);

		resize(graphics.swapchain.getSize2D());

		if (!connection.requestConnection()) {
			throw net::Error{connection.getErrorCode()};
		}

		gameState.setState({SystemsLayerType{"LOADING_SCREEN_GRAPHICS_RENDERING"}});
		state = ClientState::CONNECTING;
		eprintln("Client opened on {}, connecting to {}...", socket.getLocalEndpoint(), endpoint);
	}

	~Implementation() {
		gameState.clearState();

		ResourceRegistry& resources = gameState.getResources();
		resources.removeResource<ClientReceivedChatMessages>();
		resources.removeResource<ClientReceivedSnapshotBuffer>();
		resources.removeResource<ClientAudioStats>();
		resources.removeResource<ClientConnectionStats>();
		resources.removeResource<ClientPerformanceStats>();
		resources.removeResource<ClientState>();
		resources.removeResource<PlayerID>();
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

	void disconnect() {
		connection.requestDisconnect();
	}

	void reloadAssets() {
		graphics.finishedLoadingAssets = false;
		gameState.reloadAssets();
	}

	void pushFrameWaitTime(Duration frameWaitTime) {
		performanceStats.frameWaitTimeSampleBuffer.update(frameWaitTime);
	}

	void sendChatMessage(String message) {
		if (message.size() > MAX_CHAT_MESSAGE_SIZE) {
			message.resize(MAX_CHAT_MESSAGE_SIZE);
		}
		connection.writeReliableMessage(ChatMessageForGameServer{.message = std::move(message)});
	}

	void receiveChatMessage(String senderName, String message) {
		receivedChatMessages.messages.push_back({
			.receiveTimestamp = Clock::now(),
			.senderName = std::move(senderName),
			.message = std::move(message),
		});
	}

	void prepareForEvents() {
		for (LocalPlayer& localPlayer : localPlayers) {
			localPlayer.inputManager.update();
		}
	}

	void handleEvent(const evt::Event& event, gfx::Window& window) {
		GREM_MATCH(event) {
			GREM_CASE(const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
				if (drawableSizeChanged.windowID == window.getID()) {
					resize(drawableSizeChanged.windowDrawableSize);
				}
				break;
			}
			GREM_CASE(const evt::WindowKeyboardFocusLostEvent& keyboardFocusLost) {
				if (keyboardFocusLost.windowID == window.getID()) {
					setHasControl(window, false);
				}
				break;
			}
			GREM_CASE(const evt::WindowMouseFocusLostEvent& mouseFocusLost) {
				if (mouseFocusLost.windowID == window.getID()) {
					setHasControl(window, false);
				}
				break;
			}
			GREM_CASE(const evt::KeyPressedEvent& pressed) {
				if (pressed.windowID == window.getID() && pressed.keyCode == evt::KeyCode::ESCAPE) {
					setHasControl(window, !hasControl());
					return;
				}
				if (!hasControl()) {
					return;
				}
				break;
			}
			GREM_CASE(const evt::MouseButtonPressedEvent& pressed) {
				if (pressed.windowID == window.getID() && pressed.mouseButton == evt::MouseButton::LEFT) {
					if (!hasControl()) {
						setHasControl(window, true);
						return;
					}
				}
				if (!hasControl()) {
					return;
				}
				break;
			}
			GREM_CASE(const evt::MouseMovedEvent& moved) {
				if (!hasControl()) {
					return;
				}
				break;
			}
			GREM_CASE(const evt::DroppedFileEvent& droppedFile) {
				GREM_ASSERT(localPlayers.size() == nextTickCommand.localPlayerCommands.size());
				if (droppedFile.windowID == window.getID() && !localPlayers.empty()) {
					StringView inputFilepathPrefix{};
					CStringView inputFilename = droppedFile.droppedFilepath;
					const size_t lastSlashPosition = droppedFile.droppedFilepath.find_last_of("/\\");
					if (lastSlashPosition != String::npos) {
						inputFilepathPrefix = inputFilename.substr(0, lastSlashPosition + 1);
						inputFilename = inputFilename.substr(lastSlashPosition + 1);
					}
					Filesystem& filesystem = assetCache.getFilesystem();
					String prefabFilepath = formatString("models/{}", inputFilename);
					try {
						if (!filesystem.inputFileExists(prefabFilepath)) {
							const NativeFilesystem nativeFilesystem{};
							Allocation<byte> fileContents = nativeFilesystem.readInputFile(droppedFile.droppedFilepath);
							const res::ModelFileType fileType = res::Model::determineFileType(fileContents);
							switch (fileType) {
								case res::ModelFileType::UNKNOWN: eprintln("Unknown model file type."); return;
								case res::ModelFileType::OBJ: {
									const obj::Asset asset = obj::Asset::parse(StringView{reinterpret_cast<const char*>(fileContents.data()), fileContents.size()});
									for (const String& materialLibraryFilename : asset.materialLibraryFilenames) {
										const String materialLibraryFilepath = formatString("models/{}", materialLibraryFilename);
										filesystem.createParentOutputDirectories(materialLibraryFilepath);
										OutputFileHandle file = filesystem.openEmptyOutputFile(materialLibraryFilepath);
										file.write(nativeFilesystem.readInputFile(String{inputFilepathPrefix} + materialLibraryFilename));
										file.flush();
									}
									break;
								}
								case res::ModelFileType::GLTF: [[fallthrough]];
								case res::ModelFileType::GLTF_BINARY: {
									const gltf::Asset asset = (fileType == res::ModelFileType::GLTF_BINARY)
									                              ? gltf::Asset::parseBinary(fileContents)
									                              : gltf::Asset::parse(StringView{reinterpret_cast<const char*>(fileContents.data()), fileContents.size()});
									for (const gltf::Buffer& buffer : asset.buffers) {
										if (const gltf::RelativePath* const bufferRelativePath = buffer.uri.get_if<gltf::RelativePath>()) {
											const String bufferFilepath = formatString("models/{}", bufferRelativePath->path);
											filesystem.createParentOutputDirectories(bufferFilepath);
											OutputFileHandle file = filesystem.openEmptyOutputFile(bufferFilepath);
											file.write(nativeFilesystem.readInputFile(String{inputFilepathPrefix} + bufferRelativePath->path));
											file.flush();
										}
									}
									for (const gltf::Image& image : asset.images) {
										if (image.uri) {
											if (const gltf::RelativePath* const imageRelativePath = image.uri->get_if<gltf::RelativePath>()) {
												const String imageFilepath = formatString("models/{}", imageRelativePath->path);
												filesystem.createParentOutputDirectories(imageFilepath);
												OutputFileHandle file = filesystem.openEmptyOutputFile(imageFilepath);
												file.write(nativeFilesystem.readInputFile(String{inputFilepathPrefix} + imageRelativePath->path));
												file.flush();
											}
										}
									}
									break;
								}
							}
							filesystem.createParentOutputDirectories(prefabFilepath);
							OutputFileHandle file = filesystem.openEmptyOutputFile(prefabFilepath);
							file.write(fileContents);
							file.flush();
						}
						const SharedPointer<Prefab> prefab = assetCache.getPrefab(schema, prefabFilepath);
						(void)prefab;
					} catch (...) {
						eprintln("{}", Error::formatCurrentExceptionMessage());
						return;
					}
					nextTickCommand.localPlayerCommands.front().insertSubtickCommand(Duration{}, SpawnPrefabCommand{.prefabFilepath = std::move(prefabFilepath)});
				}
				break;
			}
			GREM_CASE_DEFAULT(const auto& other) {
				if (state != ClientState::PLAYING_GAME) {
					return;
				}
				const Optional<uint32_t> eventControllerID = match(other)(                                                                                  //
					[&](const derived_from<evt::ControllerEventBase> auto& controllerEvent) -> Optional<uint32_t> { return controllerEvent.controllerID; }, //
					[&](const auto&) -> Optional<uint32_t> { return {}; });
				if (eventControllerID) {
					if (event.is<evt::ControllerRemovedEvent>()) {
						if (!localPlayers.empty()) {
							if (localPlayers.front().controllerID == *eventControllerID) {
								localPlayers.front().inputManager.handleEvent(event);
								localPlayers.front().controllerID.reset();
								return;
							}
							if (const auto it = findBy<&LocalPlayer::controllerID>(Span{localPlayers}.subspan(1), *eventControllerID); it != localPlayers.end()) {
								it->inputManager.handleEvent(event);
								it->controllerID.reset();
								connection.writeReliableMessage(LeaveGameRequestMessageForGameServer{
									.localPlayerID = it->localPlayerID,
								});
							}
						}
						return;
					}
					if (const evt::ControllerButtonPressedEvent* const pressed = event.get_if<evt::ControllerButtonPressedEvent>()) {
						if (pressed->controllerButton == evt::ControllerButton::START) {
							if (!localPlayers.empty()) {
								if (localPlayers.front().controllerID == *eventControllerID) {
									localPlayers.front().inputManager.handleEvent(evt::Event{
										evt::ControllerRemovedEvent{evt::ControllerEventBase{evt::InputEventBase{evt::EventBase{Clock::now()}, 0}, *eventControllerID}}});
									localPlayers.front().controllerID.reset();
									addLocalPlayer(*eventControllerID);
									return;
								}
								if (const auto it = findBy<&LocalPlayer::controllerID>(Span{localPlayers}.subspan(1), *eventControllerID); it != localPlayers.end()) {
									connection.writeReliableMessage(LeaveGameRequestMessageForGameServer{
										.localPlayerID = it->localPlayerID,
									});
									if (!localPlayers.front().controllerID) {
										localPlayers.front().controllerID = *eventControllerID;
									}
									return;
								}
								if (const auto it = findBy<&LocalPlayer::controllerID>(Span{localPlayers}.subspan(1), Optional<uint32_t>{}); it != localPlayers.end()) {
									it->controllerID = *eventControllerID;
								} else {
									addLocalPlayer(*eventControllerID);
								}
							}
							return;
						}
					}
					for (LocalPlayer& localPlayer : localPlayers) {
						if (localPlayer.controllerID == *eventControllerID) {
							localPlayer.inputManager.handleEvent(event);
							return;
						}
					}
					if (!localPlayers.empty() && !localPlayers.front().controllerID) {
						localPlayers.front().controllerID = *eventControllerID;
						localPlayers.front().inputManager.handleEvent(event);
						return;
					}
					return;
				}
				break;
			}
		}

		if (!localPlayers.empty()) {
			const bool wasOpenChatPressed = localPlayers.front().inputManager.isPressed(Action::OPEN_CHAT);
			localPlayers.front().inputManager.handleEvent(event);
			const bool isOpenChatPressed = localPlayers.front().inputManager.isPressed(Action::OPEN_CHAT);
			if (isOpenChatPressed && !wasOpenChatPressed) {
				setHasControl(window, false);
				openingChat = true;
			}
		}
	}

	void update(const app::FrameInfo& frameInfo, size_t lastSecondFrameCount, Duration latestServerPhysicsTime, phys::DebugVisualization3D* physicsDebugVisualization) {
		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		performanceStats.frameTimeSampleBuffer.update(frameInfo.deltaTime);
		performanceStats.frameTimeStatistics = performanceStats.frameTimeSampleBuffer.getStatistics();
		performanceStats.lastSecondFrameCount = lastSecondFrameCount;
		performanceStats.latestServerPhysicsTime = latestServerPhysicsTime;

		audioStats.outputFFT = audio.soundStage.calculateOutputFastFourierTransformStatistics();
		audioStats.outputWave = audio.soundStage.getOutputWaveStatistics();
		audioStats.leftOutputVolume = audio.soundStage.getOutputChannelVolumeStatistics(0);
		audioStats.rightOutputVolume = audio.soundStage.getOutputChannelVolumeStatistics(1);

		receiveIncomingPackets(socket, endpoint, connection, connectionStats, [&]<typename Message>(Message&& message) -> void { handleMessage(std::forward<Message>(message)); });

		for (LocalPlayer& localPlayer : localPlayers) {
			localPlayer.inputManager.pollOutputEvents();
		}

		switch (state) {
			case ClientState::IDLE: break;
			case ClientState::CONNECTING: break;
			case ClientState::LOADING_MAP: break;
			case ClientState::LIGHT_BAKING:
				if (graphics.baking->state == Graphics::Baking::State::DONE) {
					endLightBaking();
					joinGame();
				}
				break;
			case ClientState::JOINING_GAME: break;
			case ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT:
				if (!receivedSnapshotBuffer.getSnapshots().empty()) {
					gameState.setState({
						SystemsLayerType{"SESSION"},
						SystemsLayerType{"SIMULATION"},
						SystemsLayerType{"AUDIO_STAGING"},
						SystemsLayerType{"GRAPHICS_STAGING_3D"},
						SystemsLayerType{"GRAPHICS_STAGING_2D"},
						SystemsLayerType{"WORLD_VIEW_AUDIO_RENDERING"},
						SystemsLayerType{"WORLD_VIEW_GRAPHICS_RENDERING"},
					});
					prediction.snapshots.clear();
					saveSnapshot(prediction.snapshots.push_back_unspecified_value(), gameState, {});
					connectionStats.firstPredictionSnapshotTickIndex = prediction.snapshots.front().tickIndex;
					connectionStats.lastPredictionSnapshotTickIndex = prediction.snapshots.back().tickIndex;
					state = ClientState::PLAYING_GAME;
				}
				break;
			case ClientState::PLAYING_GAME: {
				const TickIndex oldTickIndex = gameState.getResources().getResource<TickIndex>();
				const Duration tickInterval = resources.getResource<Duration>();
				Timestamp oldPredictionTimestamp{oldTickIndex, prediction.subtickBeginTimeOffset, tickInterval};

				const Duration deltaTimeAdjustment =
					max(Duration{connectionStats.predictionTimeAdjustmentRate * phys::Time{min(frameInfo.deltaTime, connectionStats.predictionTimeAdjustmentTimeRemaining)}},
						-(frameInfo.deltaTime / 2));
				const Duration dilatedDeltaTime = frameInfo.deltaTime + deltaTimeAdjustment;
				connectionStats.predictionTimeSpeedup = phys::Time{dilatedDeltaTime} / phys::Time{frameInfo.deltaTime};
				connectionStats.predictionTimeAdjustmentTimeRemaining = max(connectionStats.predictionTimeAdjustmentTimeRemaining - frameInfo.deltaTime, Duration{});

				const Duration receiveInterpolationAdjustment{connectionStats.receiveInterpolationOffsetAdjustmentRate *
															  phys::Time{min(frameInfo.deltaTime, connectionStats.receiveInterpolationOffsetAdjustmentTimeRemaining)}};
				connectionStats.receiveInterpolationOffset += receiveInterpolationAdjustment;
				connectionStats.receiveInterpolationOffsetAdjustmentTimeRemaining =
					max(connectionStats.receiveInterpolationOffsetAdjustmentTimeRemaining - frameInfo.deltaTime, Duration{});

				const Duration timeSinceOldTick = prediction.subtickBeginTimeOffset + dilatedDeltaTime;
				const Timestamp newPredictionTimestamp{oldTickIndex, timeSinceOldTick, tickInterval};

				handleLocalPlayerInput(dilatedDeltaTime);

				if (getTimeBetween(Timestamp{receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex()}, newPredictionTimestamp, tickInterval) >
					PREDICTION_DURATION_PROBLEM_THRESHOLD) {
					connectionStats.connectionProblem = true;
				}

				if (!connectionStats.connectionProblem) {
					// Advance prediction time offset and run predicted ticks.
					if (newPredictionTimestamp.getTickIndex() > oldPredictionTimestamp.getTickIndex()) {
						connectionStats.incomingDataPerTick = std::exchange(connectionStats.incomingDataPerTickAccumulator, size_t{0});
						connectionStats.outgoingDataPerTick = std::exchange(connectionStats.outgoingDataPerTickAccumulator, size_t{0});

						GREM_ASSERT(localPlayers.size() == nextTickCommand.localPlayerCommands.size());
						for (size_t i = 0; i < localPlayers.size(); ++i) {
							TickCommand::LocalPlayerCommand& nextTickLocalPlayerCommand = nextTickCommand.localPlayerCommands[i];
							LocalPlayer& localPlayer = localPlayers[i];
							GREM_ASSERT(nextTickLocalPlayerCommand.localPlayerID == localPlayer.localPlayerID);

							if (localPlayer.uncommittedAimRotations != 0) {
								nextTickLocalPlayerCommand.insertSubtickCommand(min(localPlayer.latestUncommittedAimRotationsTimeOffset, tickInterval - Duration{1}),
									RotateAimCommand{.aimRotations = localPlayer.uncommittedAimRotations});
								localPlayer.uncommittedAimRotations = {};
							}

							nextTickLocalPlayerCommand.aimRotationRates = localPlayer.aimRotationsSinceLastTick / timeSinceOldTick;
							localPlayer.aimRotationsSinceLastTick = {};
							localPlayer.latestUncommittedAimRotationsTimeOffset = {};
						}

						// Rewind prediction to received snapshot.
						const TickIndex baseSnapshotTickIndex = rollbackPredictionToReceivedBaseSnapshot(gameState, receivedSnapshotBuffer);
						latestCommands.firstCommandTickIndex = min(latestCommands.firstCommandTickIndex, baseSnapshotTickIndex);
						connectionStats.firstCommandTickIndex = latestCommands.firstCommandTickIndex;

						// Advance the command buffer to the start of the new tick.
						advanceCommandBufferUntil(latestCommands, nextTickCommand, tickInterval, newPredictionTimestamp.getTickIndex());

						// Re-run the prediction to the start of the new tick.
						runPredictionUntil(audio, graphics, gameState, prediction.eventBuffer, latestCommands, performanceStats, connectionStats, receivedPlayerID,
							receivedSnapshotBuffer.getSnapshots(), prediction.snapshots, newPredictionTimestamp.getTickIndex());

						if (physicsDebugVisualization) {
							physicsDebugVisualization->clear();
							phys::Simulation3D::drawDebugVisualization(*physicsDebugVisualization, registry, resources);
						}

						oldPredictionTimestamp = Timestamp{newPredictionTimestamp.getTickIndex()};
						prediction.subtickBeginTimeOffset = {};

						// Save a snapshot of the prediction state at the start of the new tick.
						removePredictionSnapshotsAfter(newPredictionTimestamp.getTickIndex());
						cleanupPredictionSnapshotsOlderThan(baseSnapshotTickIndex);
						savePredictionSnapshot();

						// Write outgoing messages.
						writeOutgoingMessages();

						// Start the new tick.
						lastPredictionTickEventTimestamp = Clock::now();
						nextTickCommand.receivedInterpolationTimestampAtTickBegin =
							min(Timestamp{newPredictionTimestamp.getTickIndex(), -connectionStats.receiveInterpolationOffset, tickInterval},
								Timestamp{receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex(), MAX_EXTRAPOLATION_TIME, tickInterval});
					}

					// Run upcoming sub-tick prediction from the old to the new predicted time offset.
					runUpcomingSubtickPrediction(audio, graphics, gameState, prediction.eventBuffer, receivedPlayerID, receivedSnapshotBuffer.getSnapshots(), prediction.snapshots,
						nextTickCommand, oldPredictionTimestamp.getTimeOffset(), newPredictionTimestamp.getTimeOffset());
					prediction.subtickBeginTimeOffset = newPredictionTimestamp.getTimeOffset();
				}
				break;
			}
		}

		if (state != ClientState::PLAYING_GAME || connectionStats.connectionProblem) {
			// If we're not writing messages at the in-game tick-synchronized rate, use a fallback timer with a fixed interval instead.
			if (countdownLoop(prediction.fallbackTickTimer, frameInfo.deltaTime, FALLBACK_TICK_INTERVAL) > 0) {
				writeOutgoingMessages();
			}
			lastPredictionTickEventTimestamp = Clock::now();
		}

		sendOutgoingPackets(socket, endpoint, connection, connectionStats);

		if (countupLoop(connectionStats.dataRateTimer, frameInfo.deltaTime, 1_second) > 0) {
			connectionStats.incomingDataRate = std::exchange(connectionStats.incomingDataRateAccumulator, size_t{0});
			connectionStats.outgoingDataRate = std::exchange(connectionStats.outgoingDataRateAccumulator, size_t{0});
		}
	}

	void display(const phys::DebugVisualization3D* serverPhysicsDebugVisualization, const phys::DebugVisualization3D* clientPhysicsDebugVisualization) {
		graphics.serverPhysicsDebugVisualization = serverPhysicsDebugVisualization;
		graphics.clientPhysicsDebugVisualization = clientPhysicsDebugVisualization;

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		for (LocalPlayer& localPlayer : localPlayers) {
			localPlayer.inputManager.pollOutputEvents();
		}

		if (state == ClientState::PLAYING_GAME) {
			handleLocalPlayerInput(Duration{});
		}

		const WorldView worldView = [&]() -> WorldView {
			if (state == ClientState::PLAYING_GAME) {
				const TickIndex tickIndex = resources.getResource<TickIndex>();
				const Duration tickInterval = resources.getResource<Duration>();
				const bool paused = resources.hasResource<SessionState>() && resources.getResource<SessionState>().flags.contains(SessionState::PAUSED);
				const Timestamp subtickTimestamp{tickIndex, prediction.subtickBeginTimeOffset, tickInterval};
				const Timestamp receivedInterpolationTimestamp =
					(paused) ? Timestamp{receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex()}
							 : min(subtickTimestamp.withTimeAdded(-connectionStats.receiveInterpolationOffset, tickInterval),
								   Timestamp{receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex(), MAX_EXTRAPOLATION_TIME, tickInterval});
				const Timestamp predictionInterpolationTimestamp = (paused) ? Timestamp{tickIndex.getPrevious()} : subtickTimestamp.withTicksAdded(-1);
				return {
					.subtickRegistry = registry,
					.subtickResources = resources,
					.receivedInterpolation = receivedSnapshotBuffer.getSnapshots().getInterpolationView(receivedInterpolationTimestamp, tickInterval),
					.predictionInterpolation = prediction.snapshots.getInterpolationView(predictionInterpolationTimestamp, tickInterval),
					.receivedInterpolationTimestamp = receivedInterpolationTimestamp,
					.predictionInterpolationTimestamp = predictionInterpolationTimestamp,
					.subtickTimestamp = subtickTimestamp,
					.tickInterval = tickInterval,
					.playerID = receivedPlayerID,
					.frustums{localPlayers, &LocalPlayer::frustum},
				};
			}
			return {
				.subtickRegistry = registry,
				.subtickResources = resources,
				.receivedInterpolation{},
				.predictionInterpolation{},
				.receivedInterpolationTimestamp{},
				.predictionInterpolationTimestamp{},
				.subtickTimestamp{},
				.tickInterval = FALLBACK_TICK_INTERVAL,
				.playerID{},
				.frustums{},
			};
		}();

		for (LocalPlayer& localPlayer : localPlayers) {
			LocalPlayerPerspective localPlayerPerspective{};
			resources.getResource<PlayerEntityMap>().forEachPlayerEntity(receivedPlayerID, localPlayer.localPlayerID, [&](EntityID entityID) -> void {
				if (const Aim* const playerAim = registry.findComponent<Aim>(entityID)) {
					localPlayer.visualAimAngles = playerAim->angles + localPlayer.uncommittedAimRotations;
				}
				localPlayerPerspective = worldView.getLocalPlayerPerspective(localPlayer.localPlayerID, localPlayer.visualAimAngles, localPlayer.aimDistance);
				if (LocalPlayerPerspective* const perspective = registry.findComponent<LocalPlayerPerspective>(entityID)) {
					*perspective = localPlayerPerspective;
				}
			});

			localPlayer.camera.setView(gfx::WorldView3D{
				.position = localPlayerPerspective.viewPosition.in(phys::METERS),
				.orientation = phys::Orientation3D::fromAngles(phys::PitchYawRoll{localPlayerPerspective.aimAngles, 0_radians}),
			});
			localPlayer.frustum = Frustum<float>::fromViewProjectionMatrix(localPlayer.camera.getProjectionMatrix() * localPlayer.camera.getViewMatrix());
		}

		const bool isSplitScreen = localPlayers.size() >= 2;
		gfx::Texture screenshotTexture{};
		if (savingScreenshot) {
			savingScreenshot = false;
			screenshotTexture =
				gfx::Texture::create(graphics.device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R8G8B8A8_SRGB, graphics.screenViewport.region.size, 1, nullptr);
		}

		gameState.stageAudio(audio, worldView);
		gameState.stage3DGraphicsSharedBetweenLocalPlayers(graphics, worldView);

		gameState.prepareToRender3DGraphics(graphics, isSplitScreen);

		for (LocalPlayer& localPlayer : localPlayers) {
			gameState.stageLocalPlayer3DGraphics(graphics, worldView, localPlayer.localPlayerID, localPlayer.viewport, localPlayer.camera);
			gameState.stageLocalPlayer2DGraphics(graphics, worldView, localPlayer.localPlayerID, localPlayer.viewRegion);

			gameState.renderLocalPlayer3DGraphics(graphics, isSplitScreen, worldView, localPlayer.localPlayerID, localPlayer.viewport, localPlayer.camera);
		}

		gameState.renderAudio(audio, worldView);
		gameState.renderGraphics(graphics, worldView, (screenshotTexture) ? &screenshotTexture : nullptr);

		if (screenshotTexture) {
			graphics.device.blit(graphics.swapchain, screenshotTexture);

			GREM_PROFILE_BLOCK("Save screenshot");
			res::Image::savePNG(screenshotTexture.downloadImage(), assetCache.getFilesystem(),
				formatString("screenshots/ExampleFPS_{}.png", UTCTimestamp::now().toISO8601({.timeSeparator = '.'})));
		}
	}

	[[nodiscard]] bool showSettingsGUI() {
		bool shouldSaveSettings = false;

		if (ImGui::BeginTabItem("Audio")) {
			float volume = settings.audio.outputVolume * 100.0f;
			if (ImGui::SliderFloat("Volume", &volume, 0.0f, 100.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
				settings.audio.outputVolume = static_cast<float>(volume) / 100.0f;
			}
			shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::Separator();
			shouldSaveSettings |= ImGui::Checkbox("Show Audio Stats", &settings.audio.showAudioStats);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Graphics")) {
			int renderResolution = static_cast<int>(settings.graphics.verticalRenderResolution);
			if (ImGui::SliderInt("Render resolution", &renderResolution, 1, 4320, "%dp", ImGuiSliderFlags_AlwaysClamp)) {
				settings.graphics.verticalRenderResolution = static_cast<uint32_t>(renderResolution);
			}
			shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
			const uint32_t maxMultisampleCount = graphics.device.getSupportedFeatures().maxSupportedMultisampleCount;
			int multisampleCount = static_cast<int>(settings.graphics.maxMultisampleCount);
			if (ImGui::SliderInt("Multisample count", &multisampleCount, 1, static_cast<int>(maxMultisampleCount), "%dx", ImGuiSliderFlags_AlwaysClamp)) {
				settings.graphics.maxMultisampleCount = static_cast<uint32_t>(multisampleCount);
			}
			shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
			shouldSaveSettings |= ImGui::Checkbox("Bloom", &settings.graphics.enableBloom);
			shouldSaveSettings |= ImGui::Checkbox("Blur", &settings.graphics.enableBlur);
			shouldSaveSettings |= ImGui::Checkbox("Vertical Split Screen Layout", &settings.graphics.useVerticalSplitScreenLayout);
			shouldSaveSettings |= ImGui::Checkbox("Show Light Probes", &settings.graphics.showLightProbeVolumesDebugVisualization);
			ImGui::Separator();
			shouldSaveSettings |= ImGui::Checkbox("Show FPS", &settings.graphics.showFPS);
			shouldSaveSettings |= ImGui::Checkbox("Show Performance Stats", &settings.graphics.showPerformanceStats);

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Player")) {
			ImGui::SeparatorText("Chat");
			int displayDurationSeconds = static_cast<int>(duration_cast<Seconds>(settings.chat.displayDuration).count());
			ImGui::SliderInt("Display Duration", &displayDurationSeconds, 0, 60, "%d s");
			settings.chat.displayDuration = Seconds{static_cast<Seconds::rep>(displayDurationSeconds)};
			ImGui::Checkbox("Display All", &settings.chat.displayAll);

			for (LocalPlayer& localPlayer : localPlayers) {
				bool shouldSaveLocalPlayerConfiguration = false;

				const String label = formatString("Local Player {}", localPlayer.localPlayerID.value);
				ImGui::PushID(label.c_str());
				ImGui::SeparatorText(label.c_str());

				{
					bool edited = false;
					float fieldOfView = localPlayer.fieldOfView.in(phys::DEGREES);
					edited |= ImGui::SliderFloat("Field of view", &fieldOfView, 1.0f, 170.0f, "%.0f° (H @ 4:3)");
					shouldSaveLocalPlayerConfiguration |= ImGui::IsItemDeactivatedAfterEdit();
					if (edited) {
						localPlayer.fieldOfView = fieldOfView * phys::DEGREES;
						resize(graphics.swapchain.getSize2D());
					}
				}

				{
					bool edited = false;
					vec2 aimSensitivity = localPlayer.aimSensitivity;
					edited |= ImGui::SliderFloat("Aim sensitivity (pitch)", &aimSensitivity.x, 0.05f, 5.0f);
					shouldSaveLocalPlayerConfiguration |= ImGui::IsItemDeactivatedAfterEdit();
					edited |= ImGui::SliderFloat("Aim sensitivity (yaw)", &aimSensitivity.y, 0.05f, 5.0f);
					shouldSaveLocalPlayerConfiguration |= ImGui::IsItemDeactivatedAfterEdit();
					if (edited) {
						localPlayer.aimSensitivity = aimSensitivity;
					}
				}

				{
					bool edited = false;
					ivec2 turnSensitivity{vec2{localPlayer.turnSensitivity.in(phys::DEGREES_PER_SECOND)}};
					edited |= ImGui::SliderInt("Turn sensitivity (pitch)", &turnSensitivity.x, 5, 500, "%d°/s");
					shouldSaveLocalPlayerConfiguration |= ImGui::IsItemDeactivatedAfterEdit();
					edited |= ImGui::SliderInt("Turn sensitivity (yaw)", &turnSensitivity.y, 5, 500, "%d°/s");
					shouldSaveLocalPlayerConfiguration |= ImGui::IsItemDeactivatedAfterEdit();
					if (edited) {
						localPlayer.turnSensitivity = vec2{turnSensitivity} * phys::DEGREES_PER_SECOND;
					}
				}

				if (shouldSaveLocalPlayerConfiguration) {
					try {
						localPlayer.saveConfiguration(assetCache.getFilesystem(), formatString("configuration/player{}.json", localPlayer.localPlayerID.value));
					} catch (...) {
						eprintln("Warning: Failed to save player settings.");
					}
				}
				ImGui::PopID();
			}

			if (state == ClientState::PLAYING_GAME) {
				ImGui::Separator();
				if (ImGui::Button("Add Local Player")) {
					addLocalPlayer({});
				}

				if (localPlayers.size() >= 2) {
					ImGui::Separator();
					if (ImGui::Button("Remove Local Player")) {
						const LocalPlayer& localPlayerToRemove = localPlayers.back();
						connection.writeReliableMessage(LeaveGameRequestMessageForGameServer{
							.localPlayerID = localPlayerToRemove.localPlayerID,
						});
						if (!localPlayers.front().controllerID) {
							localPlayers.front().controllerID = localPlayerToRemove.controllerID;
						}
					}
				}
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Connection")) {
			ImGui::SeparatorText("Fake lag");

			ImGui::PushID("Incoming");
			shouldSaveSettings |= ImGui::Checkbox("Incoming", &settings.connection.enableIncomingFakeLag);
			if (settings.connection.enableIncomingFakeLag) {
				ImGui::Indent();
				ImGui::SliderFloat("Mean", &settings.connection.incomingFakeLagMeanMilliseconds, 1.0f, 500.0f, "%.3f ms");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::SliderFloat("Stddev", &settings.connection.incomingFakeLagStddevMilliseconds, 0.0f, 500.0f, "%.3f ms");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::Unindent();
			}
			ImGui::PopID();

			ImGui::PushID("Outgoing");
			shouldSaveSettings |= ImGui::Checkbox("Outgoing", &settings.connection.enableOutgoingFakeLag);
			if (settings.connection.enableOutgoingFakeLag) {
				ImGui::Indent();
				ImGui::SliderFloat("Mean", &settings.connection.outgoingFakeLagMeanMilliseconds, 1.0f, 500.0f, "%.3f ms");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::SliderFloat("Stddev", &settings.connection.outgoingFakeLagStddevMilliseconds, 0.0f, 500.0f, "%.3f ms");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::Unindent();
			}
			ImGui::PopID();

			ImGui::PopID();

			ImGui::PushID("Fake loss");
			ImGui::SeparatorText("Fake loss");

			ImGui::PushID("Incoming");
			shouldSaveSettings |= ImGui::Checkbox("Incoming", &settings.connection.enableIncomingFakeLoss);
			if (settings.connection.enableIncomingFakeLoss) {
				ImGui::Indent();
				ImGui::SliderFloat("Drop", &settings.connection.incomingFakeLossPercent, 0.05f, 100.0f, "%.3f %%");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::Unindent();
			}
			ImGui::PopID();

			ImGui::PushID("Outgoing");
			shouldSaveSettings |= ImGui::Checkbox("Outgoing", &settings.connection.enableOutgoingFakeLoss);
			if (settings.connection.enableOutgoingFakeLoss) {
				ImGui::Indent();
				ImGui::SliderFloat("Drop", &settings.connection.outgoingFakeLossPercent, 0.05f, 100.0f, "%.3f %%");
				shouldSaveSettings |= ImGui::IsItemDeactivatedAfterEdit();
				ImGui::Unindent();
			}
			ImGui::PopID();

			ImGui::Separator();
			shouldSaveSettings |= ImGui::Checkbox("Show Connection Stats", &settings.connection.showConnectionStats);
			shouldSaveSettings |= ImGui::Checkbox("Show Timeline", &settings.connection.showTimeline);

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("World")) {
			bool paused = false;
			if (const SessionState* const sessionState = gameState.getResources().findResource<SessionState>()) {
				paused = sessionState->flags.contains(SessionState::PAUSED);
			}
			if (state == ClientState::PLAYING_GAME && !localPlayers.empty()) {
				const Span<const evt::Input> boundInputs = localPlayers.front().inputManager.getBoundInputs(Action::TOGGLE_SIMULATION_PAUSED);
				if (ImGui::Checkbox((boundInputs.empty()) ? "Pause" : formatString("Pause ({})", evt::getInputString(boundInputs.front())).c_str(), &paused)) {
					nextTickCommand.localPlayerCommands.front().insertSubtickCommand(Duration{}, ToggleSimulationPausedCommand{});
				}
			}
			ImGui::Separator();
			ImGui::InputTextMultiline("Schema Extension", schemaExtensionInputBuffer.data(), schemaExtensionInputBuffer.size(), ImVec2{0.0f, 0.0f},
				ImGuiInputTextFlags_AllowTabInput);
			if (ImGui::Button("Extend Schema")) {
				if (state == ClientState::PLAYING_GAME) {
					connection.writeReliableMessage(RequestSchemaExtensionMessageForGameServer{
						.previousSchemaCRC32 = schema.getCRC32(),
						.schemaExtension = schemaExtensionInputBuffer.data(),
					});
				}
			}
			if (!localPlayers.empty()) {
				ImGui::Separator();
				bool spawned = ImGui::InputText("Prefab Filepath", prefabFilepathInputBuffer.data(), prefabFilepathInputBuffer.size(),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft);
				if (spawned) {
					ImGui::SetKeyboardFocusHere(-1);
				}
				spawned |= ImGui::Button("Spawn Prefab");
				if (spawned) {
					String prefabFilepath = prefabFilepathInputBuffer.data();
					size_t lastSlashPosition = prefabFilepath.find_last_of("/\\");
					if (lastSlashPosition == String::npos) {
						prefabFilepath = "prefabs/" + prefabFilepath;
						lastSlashPosition = 7;
					}
					if (prefabFilepath.find('.', lastSlashPosition + 1) == String::npos) {
						prefabFilepath.append(".json5");
					}
					try {
						const SharedPointer<Prefab> prefab = assetCache.getPrefab(schema, prefabFilepath);
						(void)prefab;
						nextTickCommand.localPlayerCommands.front().insertSubtickCommand(Duration{}, SpawnPrefabCommand{.prefabFilepath = std::move(prefabFilepath)});
					} catch (...) {
						eprintln("{}", Error::formatCurrentExceptionMessage());
					}
				}
				ImGui::Separator();
				if (ImGui::Button("Remove Model Object")) {
					nextTickCommand.localPlayerCommands.front().insertSubtickCommand(Duration{}, RemoveModelObjectCommand{});
				}
			}
			ImGui::Separator();
			shouldSaveSettings |= ImGui::Checkbox("Show Position", &settings.world.showPosition);
			shouldSaveSettings |= ImGui::Checkbox("Show HUD", &settings.world.showHUD);
			ImGui::EndTabItem();
		}

		return shouldSaveSettings;
	}

	void applyEditedSettings(const ClientSettings& oldSettings) {
		if (settings.audio.outputVolume != oldSettings.audio.outputVolume) {
			audio.soundStage.setOutputVolume(settings.audio.outputVolume);
		}

		if (settings.graphics.verticalRenderResolution != oldSettings.graphics.verticalRenderResolution ||
			settings.graphics.maxMultisampleCount != oldSettings.graphics.maxMultisampleCount || settings.graphics.enableBloom != oldSettings.graphics.enableBloom ||
			settings.graphics.enableBlur != oldSettings.graphics.enableBlur ||
			settings.graphics.useVerticalSplitScreenLayout != oldSettings.graphics.useVerticalSplitScreenLayout) {
			const uint32_t maxMultisampleCount = graphics.device.getSupportedFeatures().maxSupportedMultisampleCount;
			settings.graphics.maxMultisampleCount = clamp(roundUpToPowerOf2(settings.graphics.maxMultisampleCount), uint32_t{1}, maxMultisampleCount);
			resize(graphics.swapchain.getSize2D());
		}

		if (settings.connection.enableIncomingFakeLag != oldSettings.connection.enableIncomingFakeLag ||
			settings.connection.incomingFakeLagMeanMilliseconds != oldSettings.connection.incomingFakeLagMeanMilliseconds ||
			settings.connection.incomingFakeLagStddevMilliseconds != oldSettings.connection.incomingFakeLagStddevMilliseconds) {
			if (settings.connection.enableIncomingFakeLag) {
				connection.enableIncomingFakeLag(settings.connection.incomingFakeLagMeanMilliseconds * phys::MILLISECONDS,
					settings.connection.incomingFakeLagStddevMilliseconds * phys::MILLISECONDS);
			} else {
				connection.disableIncomingFakeLag();
			}
		}

		if (settings.connection.enableOutgoingFakeLag != oldSettings.connection.enableOutgoingFakeLag ||
			settings.connection.outgoingFakeLagMeanMilliseconds != oldSettings.connection.outgoingFakeLagMeanMilliseconds ||
			settings.connection.outgoingFakeLagStddevMilliseconds != oldSettings.connection.outgoingFakeLagStddevMilliseconds) {
			if (settings.connection.enableOutgoingFakeLag) {
				connection.enableOutgoingFakeLag(settings.connection.outgoingFakeLagMeanMilliseconds * phys::MILLISECONDS,
					settings.connection.outgoingFakeLagStddevMilliseconds * phys::MILLISECONDS);
			} else {
				connection.disableOutgoingFakeLag();
			}
		}

		if (settings.connection.enableIncomingFakeLoss != oldSettings.connection.enableIncomingFakeLoss ||
			settings.connection.incomingFakeLossPercent != oldSettings.connection.incomingFakeLossPercent) {
			if (settings.connection.enableIncomingFakeLoss) {
				connection.enableIncomingFakeLoss(settings.connection.incomingFakeLossPercent * 0.01f);
			} else {
				connection.disableIncomingFakeLoss();
			}
		}

		if (settings.connection.enableOutgoingFakeLoss != oldSettings.connection.enableOutgoingFakeLoss ||
			settings.connection.outgoingFakeLossPercent != oldSettings.connection.outgoingFakeLossPercent) {
			if (settings.connection.enableOutgoingFakeLoss) {
				connection.enableOutgoingFakeLoss(settings.connection.outgoingFakeLossPercent * 0.01f);
			} else {
				connection.disableOutgoingFakeLoss();
			}
		}
	}

	void saveSettings() {
		if (!settingsFilepath.empty()) {
			try {
				settings.save(assetCache.getFilesystem(), settingsFilepath);
			} catch (...) {
				eprintln("Warning: Failed to save client settings.");
			}
		}
	}

	void saveScreenshot() {
		savingScreenshot = true;
	}

	void stopOpeningChat() {
		openingChat = false;
	}

	[[nodiscard]] ClientSettings& getSettings() noexcept {
		return settings;
	}

	[[nodiscard]] const ClientSettings& getSettings() const noexcept {
		return settings;
	}

	[[nodiscard]] bool isClosed() const noexcept {
		return connection.isClosed();
	}

	[[nodiscard]] bool isConnecting() const noexcept {
		return connection.isConnecting();
	}

	[[nodiscard]] bool isConnected() const noexcept {
		return connection.isConnected();
	}

	[[nodiscard]] bool isDisconnecting() const noexcept {
		return connection.isDisconnecting();
	}

	[[nodiscard]] bool hasFinishedLoadingAssets() const noexcept {
		return graphics.finishedLoadingAssets;
	}

	[[nodiscard]] bool hasControl() const noexcept {
		return controlling;
	}

	[[nodiscard]] bool isOpeningChat() const noexcept {
		return openingChat;
	}

private:
	struct LightBakingExcludedModelType {
		ModelType modelType;
	};

	void handleMessage(const LoadMapMessageForGameClient& message) {
#ifdef GREM_USE_PROFILING
		if (captureLoadTimeProfile) {
			GREM_PROFILER_END_FRAME();
			GREM_PROFILER_SAVE_NEXT_FRAME("fps_profiler_client_load_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
			GREM_PROFILER_BEGIN_FRAME();
		}
#endif

		GREM_PROFILE_FUNCTION();

		if (message.tickInterval <= Duration{}) {
			throw Error{"Invalid tick interval received."};
		}
		if (!message.mapFilepath.starts_with("maps/")) {
			throw Error{"Invalid map filepath received."};
		}

		eprintln("Client connected. Server info:\n  Tick rate: {}\n  Schema: {}\n  Map: {}", 1.0f / phys::Time{message.tickInterval}, message.schemaFilepath, message.mapFilepath);

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		prediction.reset(audio, graphics, gameState);
		localPlayers.clear();

		latestCommands.tickCommands.clear();
		latestCommands.firstCommandTickIndex = {};
		nextTickCommand = {};
		localPlayers.clear();
		lastPredictionTickEventTimestamp = Clock::now();

		receivedSnapshotBuffer.reset({});
		state = ClientState::LOADING_MAP;

		graphics.reset();

		broadphaseUpdateSchedule = {};

		initialSnapshot = {};
		schema = Schema{EntityID::Flags{ENTITY_CLIENTSIDE}};

		const Filesystem& filesystem = assetCache.getFilesystem();
		HashSet<CStringView> visitedFilepaths{};
		schema.extend(filesystem.readInputFileString(message.schemaFilepath), message.schemaFilepath, &filesystem, &visitedFilepaths);
		schema.preloadAssets(assetCache);

		resources.getResource<Events>() = {};
		resources.getResource<MapInfo>() = {};
		resources.getResource<SynchronizedEntityMap>() = {};
		resources.getResource<PlayerEntityMap>() = {};
		resources.getResource<TickIndex>() = {};
		resources.getResource<Duration>() = message.tickInterval;

		gameState.setState({SystemsLayerType{"SESSION"}});
		loadMap(registry, resources, message.mapFilepath);

		if (!beginLightBaking()) {
			joinGame();
		}
	}

	void handleMessage(const JoinedGameMessageForGameClient& message) {
		GREM_PROFILE_FUNCTION();

		if (state != ClientState::JOINING_GAME && (!receivedPlayerID || (state != ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT && state != ClientState::PLAYING_GAME))) {
			throw Error{"Invalid message order."};
		}

		if (!receivedPlayerID) {
			GREM_ASSERT(latestCommands.tickCommands.empty());

			const Duration tickInterval = gameState.getResources().getResource<Duration>();
			const Duration timeFluctuation = 3 * max(connectionStats.roundTripTimeStatistics.standardDeviation, performanceStats.frameTimeStatistics.standardDeviation);

			gameState.getResources().getResource<TickIndex>() = message.tickIndex;
			latestCommands.firstCommandTickIndex = message.tickIndex;

			receivedSnapshotBuffer.reset(message.tickIndex);
			receivedPlayerID = message.playerID;

			connectionStats.predictionTimeAdjustmentTimeRemaining = {};
			connectionStats.predictionTimeAdjustmentRate = 0.0f;
			connectionStats.predictionTimeSpeedup = 1.0f;

			connectionStats.receiveInterpolationOffset = tickInterval + tickInterval / 2 + timeFluctuation;
			connectionStats.receiveInterpolationOffsetAdjustmentTimeRemaining = {};
			connectionStats.receiveInterpolationOffsetAdjustmentRate = 0.0f;

			connectionStats.firstCommandTickIndex = message.tickIndex;
			connectionStats.firstPredictionSnapshotTickIndex = message.tickIndex;
			connectionStats.lastPredictionSnapshotTickIndex = message.tickIndex;

			state = ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT;
			eprintln("Client started.");
		}
	}

	void handleMessage(const LeftGameMessageForGameClient& message) {
		GREM_PROFILE_FUNCTION();

		if (state != ClientState::JOINING_GAME && (!receivedPlayerID || (state != ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT && state != ClientState::PLAYING_GAME))) {
			throw Error{"Invalid message order."};
		}

		GREM_ASSERT(localPlayers.size() == nextTickCommand.localPlayerCommands.size());
		if (const auto it = lowerBound(nextTickCommand.localPlayerCommands, message.localPlayerID);
			it != nextTickCommand.localPlayerCommands.end() && it->localPlayerID == message.localPlayerID) {
			const ptrdiff_t i = static_cast<ptrdiff_t>(it - nextTickCommand.localPlayerCommands.begin());
			GREM_ASSERT(localPlayers[static_cast<size_t>(i)].localPlayerID == message.localPlayerID);
			localPlayers.erase(localPlayers.begin() + i);
			nextTickCommand.localPlayerCommands.erase(it);
			resize(graphics.swapchain.getSize2D());
		}
	}

	void handleMessage(const ExtendSchemaMessageForGameClient& message) {
		GREM_PROFILE_FUNCTION();

		if (state != ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT && state != ClientState::PLAYING_GAME) {
			throw Error{"Invalid message order."};
		}

		schema.extend(message.schemaExtension);
	}

	void handleMessage(const UpdateGameStateMessageForGameClient& message) {
		GREM_PROFILE_FUNCTION();

		if ((state != ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT && state != ClientState::PLAYING_GAME) ||
			message.delta.newTickIndex <= receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex() || message.schemaCRC32 != schema.getCRC32()) {
			// Game state updates are mostly sent as unreliable messages, and may thus be received out of order.
			// So if we receive one out of order, just ignore it, instead of throwing an error.
			return;
		}

		if (!receivedSnapshotBuffer.insertReceivedSnapshot(initialSnapshot, message.delta.oldTickIndex, message.delta.newTickIndex, [&](Snapshot& snapshot) -> void {
				applySnapshotDelta(snapshot, message.delta, gameSystems);
				executor.executeSchedule(broadphaseUpdateSchedule, snapshot.registry, snapshot.resources);
			})) {
			return;
		}

		if (connectionStats.connectionProblem) {
			const TickIndex baseSnapshotTickIndex = rollbackPredictionToReceivedBaseSnapshot(gameState, receivedSnapshotBuffer);
			prediction.eventBuffer.reset([&](TickIndex tickIndex, const Event& event) -> void { gameState.cancelEvent(audio, graphics, tickIndex, event); });

			nextTickCommand.receivedInterpolationTimestampAtTickBegin = Timestamp{baseSnapshotTickIndex};
			for (TickCommand::LocalPlayerCommand& nextTickLocalPlayerCommand : nextTickCommand.localPlayerCommands) {
				nextTickLocalPlayerCommand.desiredDirectionScale = {};
				nextTickLocalPlayerCommand.aimRotationRates = {};
				nextTickLocalPlayerCommand.subtickCommands.clear();
			}
			lastPredictionTickEventTimestamp = Clock::now();
			prediction.subtickBeginTimeOffset = {};
			prediction.fallbackTickTimer = {};
			connectionStats.connectionProblem = false;
		}

		const TickIndex lastExpiredCommandTickIndex = min(message.latestReceivedCommandTickIndex, receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex().getPrevious());
		while (latestCommands.tickCommands.size() >= 2 && latestCommands.firstCommandTickIndex <= lastExpiredCommandTickIndex) {
			latestCommands.tickCommands.pop_front();
			++latestCommands.firstCommandTickIndex;
		}
		connectionStats.firstCommandTickIndex = latestCommands.firstCommandTickIndex;

		for (const UpdateGameStateMessageForGameClient::ConfirmEvent& confirmEvent : message.confirmEvents) {
			if (prediction.eventBuffer.confirmEvent(confirmEvent.tickIndex, confirmEvent.event)) {
				gameState.emitEvent(audio, graphics, confirmEvent.tickIndex, confirmEvent.event);
			}
		}
		prediction.eventBuffer.cancelUnconfirmedEventsUntil(message.delta.newTickIndex,
			[&](TickIndex tickIndex, const Event& event) -> void { gameState.cancelEvent(audio, graphics, tickIndex, event); });

		connectionStats.recentOutgoingPacketLossFraction = message.recentPacketLossFraction;

		connectionStats.remotePredictionDurationTicks = message.latestReceivedCommandTickIndex - message.delta.newTickIndex;

		const TickIndex tickIndex = gameState.getResources().getResource<TickIndex>();
		const Duration tickInterval = gameState.getResources().getResource<Duration>();
		const Duration timeFluctuation = 3 * max(connectionStats.roundTripTimeStatistics.standardDeviation, performanceStats.frameTimeStatistics.standardDeviation);
		const Duration minTimeError = tickInterval / 2;
		const Timestamp subtickBeginTimestamp{tickIndex, prediction.subtickBeginTimeOffset, tickInterval};

		const Duration desiredPredictionDuration =
			min(connectionStats.roundTripTimeStatistics.mean + PREDICTION_DURATION_MARGIN + timeFluctuation, MAX_DESIRED_PREDICTION_DURATION);
		const Duration currentPredictionDuration = getTimeBetween(Timestamp{receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex()}, subtickBeginTimestamp, tickInterval);
		const Duration predictionDurationError = desiredPredictionDuration - currentPredictionDuration;
		connectionStats.predictionDurationErrorSampleBuffer.update(predictionDurationError);
		connectionStats.predictionDurationErrorStatistics = connectionStats.predictionDurationErrorSampleBuffer.getStatistics();

		if (abs(predictionDurationError) < minTimeError) {
			connectionStats.predictionTimeAdjustmentRate = 0.0f;
		} else {
			connectionStats.predictionTimeAdjustmentRate = phys::Time{predictionDurationError} / phys::Time{PREDICTION_TIME_ADJUSTMENT_DURATION};
		}
		connectionStats.predictionTimeAdjustmentTimeRemaining = PREDICTION_TIME_ADJUSTMENT_DURATION;

		const Duration desiredReceiveInterpolationOffset = desiredPredictionDuration + tickInterval + tickInterval / 2 + timeFluctuation;
		const Duration currentReceiveInterpolationOffset = connectionStats.receiveInterpolationOffset;
		const Duration receiveInterpolationOffsetError = desiredReceiveInterpolationOffset - currentReceiveInterpolationOffset;

		if (abs(receiveInterpolationOffsetError) < minTimeError) {
			connectionStats.receiveInterpolationOffsetAdjustmentRate = 0.0f;
		} else {
			connectionStats.receiveInterpolationOffsetAdjustmentRate = phys::Time{receiveInterpolationOffsetError} / phys::Time{RECEIVE_INTERPOLATION_OFFSET_ADJUSTMENT_DURATION};
		}
		connectionStats.receiveInterpolationOffsetAdjustmentTimeRemaining = RECEIVE_INTERPOLATION_OFFSET_ADJUSTMENT_DURATION;
	}

	void handleMessage(ChatMessageForGameClient&& message) {
		GREM_PROFILE_FUNCTION();

		receiveChatMessage(std::move(message).senderName, std::move(message).message);
	}

	void removePredictionSnapshotsAfter(TickIndex tickIndex) {
		while (!prediction.snapshots.empty() && prediction.snapshots.back().tickIndex >= tickIndex) {
			prediction.snapshots.pop_back();
		}
	}

	void cleanupPredictionSnapshotsOlderThan(TickIndex tickIndex) {
		// Try to keep at least one snapshot, so that we always have a previous one to interpolate from.
		while (prediction.snapshots.size() >= 2 && prediction.snapshots[1].tickIndex < tickIndex) {
			prediction.snapshots.pop_front();
		}
	}

	void savePredictionSnapshot() {
		GREM_PROFILE_FUNCTION();

		saveSnapshot(prediction.snapshots.push_back_unspecified_value(), gameState, {});
		connectionStats.firstPredictionSnapshotTickIndex = prediction.snapshots.front().tickIndex;
		connectionStats.lastPredictionSnapshotTickIndex = prediction.snapshots.back().tickIndex;
	}

	void writeOutgoingMessages() {
		GREM_PROFILE_FUNCTION();

		if (!latestCommands.tickCommands.empty()) {
			connection.writeUnreliableMessage(latestCommands);
		}
		if (receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex() > TickIndex{}) {
			connection.writeUnreliableMessage(
				AcknowledgeSnapshotMessageForGameServer{.latestReceivedSnapshotTickIndex = receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex()});
		}
	}

	void resize(Extent2D newDrawableSize) {
		GREM_PROFILE_FUNCTION();

		const Region2D screenRegion{.size = newDrawableSize};
		const gfx::FeatureSupport supportedFeatures = graphics.device.getSupportedFeatures();
		const uint32_t maxHorizontalRenderResolution =
			min(min(supportedFeatures.max2DTextureResolution, supportedFeatures.maxFramebufferSize.width), static_cast<uint32_t>(settings.graphics.verticalRenderResolution * 3));
		const uint32_t horizontalRenderResolution =
			clamp(static_cast<uint32_t>(round(static_cast<float>(settings.graphics.verticalRenderResolution) * newDrawableSize.getAspectRatio())), uint32_t{1},
				maxHorizontalRenderResolution);
		const Extent2D renderResolution{horizontalRenderResolution, settings.graphics.verticalRenderResolution};
		graphics.resize(screenRegion, renderResolution, settings.graphics.maxMultisampleCount, settings.graphics.enableBloom, settings.graphics.enableBlur);

		bool expandHorizontally = !settings.graphics.useVerticalSplitScreenLayout;
		size_t viewColumnCount = 1;
		size_t viewRowCount = 1;
		size_t viewCount = 1;
		size_t nextScreenFillingViewCount = 1;
		while (viewCount < localPlayers.size()) {
			if (viewCount == nextScreenFillingViewCount) {
				if (expandHorizontally) {
					++viewColumnCount;
				} else {
					++viewRowCount;
				}
				expandHorizontally = !expandHorizontally;
				nextScreenFillingViewCount = viewColumnCount * viewRowCount;
			}
			++viewCount;
		}

		const Extent2D viewSize = Extent2D::from(u32vec2{screenRegion.size} / u32vec2{static_cast<uint32_t>(viewColumnCount), static_cast<uint32_t>(viewRowCount)});
		const Extent2D renderSize = Extent2D::from(u32vec2{renderResolution} / u32vec2{static_cast<uint32_t>(viewColumnCount), static_cast<uint32_t>(viewRowCount)});
		const size_t gapCount = static_cast<uint32_t>(nextScreenFillingViewCount - viewCount);
		const int32_t lastViewRowHorizontalOffset = static_cast<int32_t>((gapCount * viewSize.width) / 2);
		const int32_t lastRenderRowHorizontalOffset = static_cast<int32_t>((gapCount * renderSize.width) / 2);

		for (size_t i = 0; i < localPlayers.size(); ++i) {
			LocalPlayer& localPlayer = localPlayers[i];
			const uint32_t x = static_cast<uint32_t>(i % viewColumnCount);
			const uint32_t y = static_cast<uint32_t>(i / viewColumnCount);
			localPlayer.viewRegion = {
				.offset =
					{
						screenRegion.offset.x + static_cast<int32_t>(x * viewSize.width),
						screenRegion.offset.y + static_cast<int32_t>(screenRegion.size.height) - static_cast<int32_t>((y + 1) * viewSize.height),
					},
				.size = viewSize,
			};
			localPlayer.viewport.region = {
				.offset =
					{
						static_cast<int32_t>(x * renderSize.width),
						static_cast<int32_t>(renderResolution.height) - static_cast<int32_t>((y + 1) * renderSize.height),
					},
				.size = renderSize,
			};
			if (y == viewRowCount - 1) {
				localPlayer.viewRegion.offset.x += lastViewRowHorizontalOffset;
				localPlayer.viewport.region.offset.x += lastRenderRowHorizontalOffset;
			}
			localPlayer.camera.setProjection(gfx::PerspectiveProjection3D{
				.verticalFieldOfView = 2.0f * atan((3.0f / 4.0f) * tan(localPlayer.fieldOfView.in(phys::RADIANS) * 0.5f)),
				.aspectRatio = renderSize.getAspectRatio(),
				.nearZ = 0.1f,
				.farZ = 5000.0f,
			});
		}
	}

	void setHasControl(gfx::Window& window, bool newHasControl) {
		if (newHasControl != controlling) {
			if (controlling) {
				try {
					window.setRelativeMouseMode(false);
				} catch (...) {
				}
				for (LocalPlayer& localPlayer : localPlayers) {
					localPlayer.inputManager.releaseAll(Clock::now());
				}
			} else {
				try {
					window.setRelativeMouseMode(true);
				} catch (...) {
				}
				ImGui::SetWindowFocus(nullptr);
				openingChat = false;
			}
			controlling = newHasControl;
		}
	}

	[[nodiscard]] bool beginLightBaking() {
		GREM_PROFILE_FUNCTION();

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		const Filesystem& filesystem = assetCache.getFilesystem();
		const MapInfo& mapInfo = gameState.getResources().getResource<MapInfo>();

		graphics.baking.emplace(Graphics::Baking{
			.lightBaker{graphics.device, graphics.renderer3D, mapInfo.lightBakerOptions},
			.radianceOnlySky{graphics.device},
			.previousBounceLightProbeVolumes{graphics.device},
			.previousBounceReflectionProbes{graphics.device},
			.lightProbeOccluderInstances3D{graphics.device, graphics.renderer3D},
			.state = Graphics::Baking::State::DONE,
		});
		if (mapInfo.skyImageFilepath.empty()) {
			graphics.sky.setSky(mapInfo.skyOptions);
		} else {
			graphics.baking->lightBaker.bakeSkybox(graphics.sky, res::Image{filesystem, mapInfo.skyImageFilepath}, mapInfo.skyOptions);
		}
		graphics.baking->radianceOnlySky.setSky(graphics.sky.getRadianceMap(), {}, {}, graphics.sky.getOptions());

		if ((mapInfo.lightProbeVolumesVolumeOptions.empty() ||
				(filesystem.inputFileExists(mapInfo.lightProbeVolumesIrradianceAtlasFilepath) && filesystem.inputFileExists(mapInfo.lightProbeVolumesDistanceAtlasFilepath))) &&
			(mapInfo.reflectionProbesProbeOptions.empty() || filesystem.inputFileExists(mapInfo.reflectionProbesReflectionMapsFilepath))) {
			graphics.fog.setFog(mapInfo.fogOptions);
			graphics.baking.reset();

			gfx::Texture lightProbeVolumesIrradianceAtlasTexture{};
			gfx::Texture lightProbeVolumesDistanceAtlasTexture{};
			if (!mapInfo.lightProbeVolumesVolumeOptions.empty()) {
				const res::Image lightProbeVolumesIrradianceAtlas{filesystem, mapInfo.lightProbeVolumesIrradianceAtlasFilepath, {.requiredType = res::ImageType::IMAGE_2D_ARRAY}};
				if (lightProbeVolumesIrradianceAtlas.getWidth() != lightProbeVolumesIrradianceAtlas.getHeight() || lightProbeVolumesIrradianceAtlas.getDepth() == 0) {
					throw Error{"Invalid light probe volumes irradiance atlas size."};
				}
				lightProbeVolumesIrradianceAtlasTexture = gfx::Texture{graphics.device, lightProbeVolumesIrradianceAtlas,
					gfx::TextureImageUploadOptions{.transferFunction = Color::TransferFunction::LINEAR, .convertToPremultipliedAlpha = false, .generateMipmap = false},
					gfx::LightProbeVolumes3D::IRRADIANCE_ATLAS_SAMPLER_OPTIONS};

				const res::Image lightProbeVolumesDistanceAtlas{filesystem, mapInfo.lightProbeVolumesDistanceAtlasFilepath, {.requiredType = res::ImageType::IMAGE_2D_ARRAY}};
				if (lightProbeVolumesDistanceAtlas.getWidth() != lightProbeVolumesDistanceAtlas.getHeight() || lightProbeVolumesDistanceAtlas.getDepth() == 0) {
					throw Error{"Invalid light probe volumes distance atlas size."};
				}
				lightProbeVolumesDistanceAtlasTexture = gfx::Texture{graphics.device, lightProbeVolumesDistanceAtlas,
					gfx::TextureImageUploadOptions{.transferFunction = Color::TransferFunction::LINEAR, .convertToPremultipliedAlpha = false, .generateMipmap = false},
					gfx::LightProbeVolumes3D::DISTANCE_ATLAS_SAMPLER_OPTIONS};
			}

			gfx::Texture reflectionProbesReflectionMapsTexture{};
			if (!mapInfo.reflectionProbesProbeOptions.empty()) {
				const res::Image reflectionProbesReflectionMaps{filesystem, mapInfo.reflectionProbesReflectionMapsFilepath, {.requiredType = res::ImageType::IMAGE_CUBE_ARRAY}};
				if (reflectionProbesReflectionMaps.getWidth() != reflectionProbesReflectionMaps.getHeight() ||
					static_cast<size_t>(reflectionProbesReflectionMaps.getDepth()) != mapInfo.reflectionProbesProbeOptions.size() * 6) {
					throw Error{"Invalid reflection probes reflection maps size."};
				}
				reflectionProbesReflectionMapsTexture = gfx::Texture{graphics.device, reflectionProbesReflectionMaps,
					gfx::TextureImageUploadOptions{.transferFunction = Color::TransferFunction::LINEAR, .convertToPremultipliedAlpha = false, .generateMipmap = false},
					gfx::ReflectionProbes3D::REFLECTION_MAPS_SAMPLER_OPTIONS};
			}

			graphics.lightProbeVolumes.setLightProbeVolumes(std::move(lightProbeVolumesIrradianceAtlasTexture), std::move(lightProbeVolumesDistanceAtlasTexture),
				mapInfo.lightProbeVolumesVolumeOptions, mapInfo.lightProbeVolumesOptions);
			graphics.reflectionProbes.setReflectionProbes(std::move(reflectionProbesReflectionMapsTexture), mapInfo.reflectionProbesProbeOptions, mapInfo.reflectionProbesOptions);
			return false;
		}

		graphics.lightProbeVolumes.setLightProbeVolumes({}, {}, mapInfo.lightProbeVolumesVolumeOptions, mapInfo.lightProbeVolumesOptions);
		graphics.reflectionProbes.setReflectionProbes({}, mapInfo.reflectionProbesProbeOptions, mapInfo.reflectionProbesOptions);

		if (mapInfo.lightBakerBounceCount > 0) {
			if (!mapInfo.lightProbeVolumesVolumeOptions.empty()) {
				graphics.baking->state = Graphics::Baking::State::BAKING_LIGHT_PROBES_DISTANCE;
			} else if (!mapInfo.reflectionProbesProbeOptions.empty()) {
				graphics.baking->state = Graphics::Baking::State::BAKING_REFLECTION_PROBES;
			}
		}

		if (graphics.baking->state == Graphics::Baking::State::DONE) {
			graphics.fog.setFog(mapInfo.fogOptions);
			graphics.baking.reset();
			return false;
		}

		graphics.baking->bounceCount = mapInfo.lightBakerBounceCount;

		const size_t totalLightProbeCount =
			accumulate(mapInfo.lightProbeVolumesVolumeOptions, size_t{0}, [](size_t sum, const gfx::LightProbeVolumeOptions3D& volumeOptions) -> size_t {
				return sum + static_cast<size_t>(volumeOptions.probeCounts.x * volumeOptions.probeCounts.y * volumeOptions.probeCounts.z);
			});
		graphics.baking->totalProgressCount = totalLightProbeCount + mapInfo.lightBakerBounceCount * (totalLightProbeCount + mapInfo.reflectionProbesProbeOptions.size());

		Optional<gfx::Decals3D> stagedDecals{};
		Optional<gfx::Lights3D> stagedLights{};
		{
			const GameState::StackIndex gameStateIndex = gameState.pushState({SystemsLayerType{"GRAPHICS_STAGING_3D"}});
			try {
				for (auto&& [entityID, modelType, excludeFromLightBakeTag] : registry.getEntities<ModelType, const ExcludeFromLightBakeTag>()) {
					registry.addComponent<LightBakingExcludedModelType>(entityID, LightBakingExcludedModelType{.modelType = modelType});
					modelType = {};
				}

				const TickIndex tickIndex = resources.getResource<TickIndex>();
				const Duration tickInterval = resources.getResource<Duration>();
				const SnapshotView snapshot{.registry = registry, .resources = resources};
				const WorldView worldView{
					.subtickRegistry = registry,
					.subtickResources = resources,
					.receivedInterpolation = SnapshotInterpolationView{.snapshotA = snapshot, .snapshotB = snapshot, .interpolationAlpha = 0.0f},
					.predictionInterpolation = SnapshotInterpolationView{.snapshotA = snapshot, .snapshotB = snapshot, .interpolationAlpha = 0.0f},
					.receivedInterpolationTimestamp{tickIndex},
					.predictionInterpolationTimestamp{tickIndex},
					.subtickTimestamp{tickIndex},
					.tickInterval = tickInterval,
					.playerID{},
					.frustums{},
				};

				gameState.stage3DGraphicsSharedBetweenLocalPlayers(graphics, worldView);
				stagedDecals = std::move(graphics.decals);
				stagedLights = std::move(graphics.lights);
				graphics.lights.clearLights();
			} catch (...) {
				gameState.popState(gameStateIndex);
				throw;
			}
			gameState.popState(gameStateIndex);
		}

		graphics.decals = std::move(*stagedDecals);
		graphics.lights = std::move(*stagedLights);

		graphics.lights.forEachLight([&](gfx::LightID lightID) -> void {
			if (const Optional<gfx::SunLightOptions3D> sunLightOptions = graphics.lights.getSunLightOptions(lightID)) {
				graphics.lights.setLightShadowMapNormalOffsetBiasConstantFactor(lightID, sunLightOptions->shadowMapNormalOffsetBiasConstantFactor * 128.0f);
			} else if (const Optional<gfx::DirectionalLightOptions3D> directionalLightOptions = graphics.lights.getDirectionalLightOptions(lightID)) {
				graphics.lights.setLightShadowMapNormalOffsetBiasConstantFactor(lightID, directionalLightOptions->shadowMapNormalOffsetBiasConstantFactor * 128.0f);
			}
		});

		graphics.renderer3D.renderAllShadowMapsForFullZoomedOutView(graphics.lights, resources.getResource<MapInfo>().bounds.in(phys::METERS),
			[&](gfx::RenderPass& renderPass, const gfx::Camera3D& camera) -> void {
				graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
					{{graphics.shadowCasterInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}}}, camera);
			});

		gameState.setState({
			SystemsLayerType{"SESSION"},
			SystemsLayerType{"LIGHT_BAKING"},
			SystemsLayerType{"LOADING_SCREEN_GRAPHICS_RENDERING"},
		});
		state = ClientState::LIGHT_BAKING;
		return true;
	}

	void endLightBaking() {
		GREM_PROFILE_FUNCTION();

		EntityRegistry& registry = gameState.getRegistry();
		ResourceRegistry& resources = gameState.getResources();

		Filesystem& filesystem = assetCache.getFilesystem();
		const MapInfo& mapInfo = resources.getResource<MapInfo>();

		for (auto&& [entityID, modelType, excludeFromLightBakeTag] : registry.getEntities<ModelType, const ExcludeFromLightBakeTag>()) {
			modelType = registry.getComponent<LightBakingExcludedModelType>(entityID).modelType;
		}
		registry.removeComponentFromAllEntities<LightBakingExcludedModelType>();

		graphics.lights.forEachLight([&](gfx::LightID lightID) -> void {
			if (const Optional<gfx::SunLightOptions3D> sunLightOptions = graphics.lights.getSunLightOptions(lightID)) {
				graphics.lights.setLightShadowMapNormalOffsetBiasConstantFactor(lightID, sunLightOptions->shadowMapNormalOffsetBiasConstantFactor / 128.0f);
			} else if (const Optional<gfx::DirectionalLightOptions3D> directionalLightOptions = graphics.lights.getDirectionalLightOptions(lightID)) {
				graphics.lights.setLightShadowMapNormalOffsetBiasConstantFactor(lightID, directionalLightOptions->shadowMapNormalOffsetBiasConstantFactor / 128.0f);
			}
		});

		graphics.fog.setFog(mapInfo.fogOptions);
		graphics.baking.reset();
		graphics.depthPrepassInstances3D.clear();
		graphics.visibleInstances3D.clear();
		graphics.shadowCasterInstances3D.clear();
		graphics.hullDrawCommandBuffer.clear();
		graphics.hullInstanceBuffer.clear();
		graphics.instances2D.clear();
		graphics.decals.clearDecals();
		graphics.decals.clearDecalMaterials();
		graphics.lights.clearLights();

		const Span<const gfx::LightProbeVolumeOptions3D> lightProbeVolumesVolumeOptions = graphics.lightProbeVolumes.getVolumeOptions();
		if (!lightProbeVolumesVolumeOptions.empty()) {
			eprintln("Saving baked diffuse lighting...");
			filesystem.createParentOutputDirectories(mapInfo.lightProbeVolumesIrradianceAtlasFilepath);
			filesystem.createParentOutputDirectories(mapInfo.lightProbeVolumesDistanceAtlasFilepath);
			res::Image::save(graphics.lightProbeVolumes.getIrradianceAtlasTexture().downloadImage({.convertFromPremultipliedAlpha = false}), filesystem,
				mapInfo.lightProbeVolumesIrradianceAtlasFilepath);
			res::Image::save(graphics.lightProbeVolumes.getDistanceAtlasTexture().downloadImage({.convertFromPremultipliedAlpha = false}), filesystem,
				mapInfo.lightProbeVolumesDistanceAtlasFilepath);
		}

		const Span<const gfx::ReflectionProbeOptions3D> reflectionProbesProbeOptions = graphics.reflectionProbes.getProbeOptions();
		if (!reflectionProbesProbeOptions.empty()) {
			eprintln("Saving baked specular reflections...");
			filesystem.createParentOutputDirectories(mapInfo.reflectionProbesReflectionMapsFilepath);
			res::Image::save(graphics.reflectionProbes.getReflectionMaps().downloadImage({.convertFromPremultipliedAlpha = false}), filesystem,
				mapInfo.reflectionProbesReflectionMapsFilepath);
		}
	}

	void joinGame() {
		GREM_PROFILE_FUNCTION();

		gameState.setState({
			SystemsLayerType{"SESSION"},
			SystemsLayerType{"SIMULATION"},
			SystemsLayerType{"AUDIO_STAGING"},
			SystemsLayerType{"GRAPHICS_STAGING_3D"},
			SystemsLayerType{"GRAPHICS_STAGING_2D"},
			SystemsLayerType{"WORLD_VIEW_AUDIO_RENDERING"},
			SystemsLayerType{"WORLD_VIEW_GRAPHICS_RENDERING"},
			SystemsLayerType{"LOADING_SCREEN_GRAPHICS_RENDERING"},
		});
		saveSnapshot(initialSnapshot, gameState, {});

		Scheduler broadphaseUpdateScheduler{};
		phys::Simulation3D::scheduleBroadphaseUpdate(broadphaseUpdateScheduler, gameState.getResources().getResource<phys::SimulationOptions3D>());
		broadphaseUpdateSchedule = broadphaseUpdateScheduler.buildSchedule();

		state = ClientState::JOINING_GAME;
		addLocalPlayer({});
	}

	void addLocalPlayer(Optional<uint32_t> controllerID) {
		GREM_ASSERT(state == ClientState::JOINING_GAME || state == ClientState::JOINED_GAME_AWAITING_FIRST_SNAPSHOT || state == ClientState::PLAYING_GAME);
		GREM_ASSERT(localPlayers.size() == nextTickCommand.localPlayerCommands.size());
		if (localPlayers.size() >= MAX_LOCAL_PLAYER_COUNT) {
			throw Error{"Too many local players."};
		}
		LocalPlayerID localPlayerID{.value = 1};
		auto it = nextTickCommand.localPlayerCommands.begin();
		while (it != nextTickCommand.localPlayerCommands.end() && it->localPlayerID == localPlayerID) {
			++it;
			++localPlayerID.value;
		}
		it = nextTickCommand.localPlayerCommands.insert(it, TickCommand::LocalPlayerCommand{.localPlayerID = localPlayerID});
		try {
			const ptrdiff_t i = static_cast<ptrdiff_t>(it - nextTickCommand.localPlayerCommands.begin());
			localPlayers.emplace(localPlayers.begin() + i, assetCache.getFilesystem(), graphics.device, formatString("configuration/player{}.json", localPlayerID.value),
				localPlayerID, controllerID);
		} catch (...) {
			nextTickCommand.localPlayerCommands.erase(it);
			throw;
		}

		resize(graphics.swapchain.getSize2D());

		connection.writeReliableMessage(JoinGameRequestMessageForGameServer{.localPlayerID = localPlayerID});
	}

	void handleLocalPlayerInput(Duration deltaTime) {
		GREM_ASSERT(localPlayers.size() == nextTickCommand.localPlayerCommands.size());
		for (size_t i = 0; i < localPlayers.size(); ++i) {
			LocalPlayer& localPlayer = localPlayers[i];
			TickCommand::LocalPlayerCommand& nextTickLocalPlayerCommand = nextTickCommand.localPlayerCommands[i];
			GREM_ASSERT(localPlayer.localPlayerID == nextTickLocalPlayerCommand.localPlayerID);

			const phys::Length1D aimDistanceAdjustment = localPlayer.inputManager.getRelativeState1D(Action::SCROLL_UP, Action::SCROLL_DOWN).motion * phys::METERS;
			localPlayer.aimDistance = max(localPlayer.aimDistance + aimDistanceAdjustment, phys::Distance{});

			const phys::Scale2D turnScale = localPlayer.inputManager.getCurrentState2D(Action::TURN_DOWN, Action::TURN_UP, Action::TURN_RIGHT, Action::TURN_LEFT).value;
			const phys::PitchYawRotations turnRotations = turnScale * localPlayer.turnSensitivity * deltaTime;
			localPlayer.aimRotationsSinceLastTick += turnRotations;
			localPlayer.uncommittedAimRotations += turnRotations;

			const phys::Scale2D movementInputScale =
				clampLength(localPlayer.inputManager.getCurrentState2D(Action::MOVE_LEFT, Action::MOVE_RIGHT, Action::MOVE_BACKWARD, Action::MOVE_FORWARD).value, 1.0f);
			for (size_t steps = countdownLoop(localPlayer.stepSpamTimer, deltaTime, 0.02_seconds, localPlayer.inputManager.isPressed(Action::SLOWLY_ADVANCE_PAUSED_SIMULATION));
				steps-- > 0;) {
				nextTickLocalPlayerCommand.insertSubtickCommand(prediction.subtickBeginTimeOffset, SingleStepPausedSimulationCommand{});
			}
			if (countdownLoop(localPlayer.crateSpamTimer, deltaTime, 0.1_seconds, localPlayer.inputManager.isPressed(Action::SPAM_CRATES)) > 0) {
				nextTickLocalPlayerCommand.insertSubtickCommand(prediction.subtickBeginTimeOffset, SpawnModelObjectCommand{.modelType{"CRATE"}});
			}

			for (const evt::InputManager::OutputEvent& event : localPlayer.inputManager.getLatestPolledOutputEvents()) {
				GREM_MATCH(event) {
					GREM_CASE(const evt::InputManager::OutputMoved& moved) {
						const Duration timeOffset = max(moved.getTimestamp() - lastPredictionTickEventTimestamp, prediction.subtickBeginTimeOffset);
						const float motion = moved.getDelta().motion;
						switch (static_cast<Action>(moved.getOutputIndex())) {
							case Action::AIM_DOWN: {
								if (motion > 0.0f) {
									const phys::PitchYawRotations aimRotations = phys::Scale2D{-motion, 0} * localPlayer.aimSensitivity;
									localPlayer.aimRotationsSinceLastTick += aimRotations;
									localPlayer.uncommittedAimRotations += aimRotations;
									localPlayer.latestUncommittedAimRotationsTimeOffset = timeOffset;
								}
								break;
							}
							case Action::AIM_UP: {
								if (motion > 0.0f) {
									const phys::PitchYawRotations aimRotations = phys::Scale2D{motion, 0} * localPlayer.aimSensitivity;
									localPlayer.aimRotationsSinceLastTick += aimRotations;
									localPlayer.uncommittedAimRotations += aimRotations;
									localPlayer.latestUncommittedAimRotationsTimeOffset = timeOffset;
								}
								break;
							}
							case Action::AIM_LEFT: {
								if (motion > 0.0f) {
									const phys::PitchYawRotations aimRotations = phys::Scale2D{0, motion} * localPlayer.aimSensitivity;
									localPlayer.aimRotationsSinceLastTick += aimRotations;
									localPlayer.uncommittedAimRotations += aimRotations;
									localPlayer.latestUncommittedAimRotationsTimeOffset = timeOffset;
								}
								break;
							}
							case Action::AIM_RIGHT: {
								if (motion > 0.0f) {
									const phys::PitchYawRotations aimRotations = phys::Scale2D{0, -motion} * localPlayer.aimSensitivity;
									localPlayer.aimRotationsSinceLastTick += aimRotations;
									localPlayer.uncommittedAimRotations += aimRotations;
									localPlayer.latestUncommittedAimRotationsTimeOffset = timeOffset;
								}
								break;
							}
							default: break;
						}
						break;
					}
					GREM_CASE(const evt::InputManager::OutputPressed& pressed) {
						const Duration timeOffset = max(pressed.getTimestamp() - lastPredictionTickEventTimestamp, prediction.subtickBeginTimeOffset);
						switch (static_cast<Action>(pressed.getOutputIndex())) {
							case Action::SPRINT: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StartSprintingCommand{}); break;
							case Action::CROUCH: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StartCrouchingCommand{}); break;
							case Action::JUMP: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StartJumpingCommand{}); break;
							case Action::ATTACK:
								if (localPlayer.uncommittedAimRotations != 0) {
									nextTickLocalPlayerCommand.insertSubtickCommand(
										min(localPlayer.latestUncommittedAimRotationsTimeOffset, gameState.getResources().getResource<Duration>() - Duration{1}),
										RotateAimCommand{.aimRotations = localPlayer.uncommittedAimRotations});
									localPlayer.uncommittedAimRotations = {};
								}
								nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StartPrimaryFireCommand{});
								break;
							case Action::RELOAD: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ReloadWeaponCommand{}); break;
							case Action::CHANGE_FIRE_MODE_LEFT: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ChangeFireModeLeftCommand{}); break;
							case Action::CHANGE_FIRE_MODE_RIGHT: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ChangeFireModeRightCommand{}); break;
							case Action::CYCLE_FIRE_MODE: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, CycleFireModeCommand{}); break;
							case Action::TOGGLE_FLASHLIGHT: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ToggleFlashlightCommand{}); break;
							case Action::PLACE_DECAL:
								nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset,
									PlaceDecalCommand{
										.decalMaterialType{"TEST_DECAL"},
										.decalSize{1_meter},
										.decalRange = 0.2_meters,
									});
								break;
							case Action::AIM_DOWN_SIGHTS: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StartAimingDownSightsCommand{}); break;
							case Action::TOGGLE_FLYING: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ToggleFlyingCommand{}); break;
							case Action::TOGGLE_SIMULATION_PAUSED: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, ToggleSimulationPausedCommand{}); break;
							case Action::SINGLE_STEP_PAUSED_SIMULATION: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, SingleStepPausedSimulationCommand{}); break;
							case Action::SPAWN_CARROT_CAKE: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, SpawnModelObjectCommand{.modelType{"CARROT_CAKE"}}); break;
							case Action::SPAWN_TELEVISION: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, SpawnModelObjectCommand{.modelType{"TELEVISION"}}); break;
							case Action::REMOVE_PROP: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, RemoveModelObjectCommand{}); break;
							case Action::RAIN_BOXES: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, RainBoxesCommand{}); break;
							default: break;
						}
						break;
					}
					GREM_CASE(const evt::InputManager::OutputReleased& released) {
						const Duration timeOffset = max(released.getTimestamp() - lastPredictionTickEventTimestamp, prediction.subtickBeginTimeOffset);
						switch (static_cast<Action>(released.getOutputIndex())) {
							case Action::SPRINT: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StopSprintingCommand{}); break;
							case Action::CROUCH: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StopCrouchingCommand{}); break;
							case Action::JUMP: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StopJumpingCommand{}); break;
							case Action::ATTACK: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StopPrimaryFireCommand{}); break;
							case Action::AIM_DOWN_SIGHTS: nextTickLocalPlayerCommand.insertSubtickCommand(timeOffset, StopAimingDownSightsCommand{}); break;
							default: break;
						}
						break;
					}
					GREM_CASE_DEFAULT(const auto& other) break;
				}
			}

			nextTickLocalPlayerCommand.desiredDirectionScale =
				phys::Orientation2D{0 - localPlayer.visualAimAngles.getY()}(phys::Scale2D{movementInputScale.getX(), -movementInputScale.getY()});
		}
	}

	GREM_PROFILE_CONSTRUCTOR_BEGIN();
	AssetCache& assetCache;
	Audio& audio;
	Graphics& graphics;
	const GameSystems& gameSystems;
	exec::Executor& executor;
	GameState& gameState;
	net::UDPSocket socket{net::BlockingMode::NON_BLOCKING, net::Endpoint{net::IPv4Address::ANY}};
	net::Endpoint endpoint;
	ConnectionToServer connection{};
	Schema schema{EntityID::Flags{ENTITY_CLIENTSIDE}};
	Snapshot initialSnapshot{};
	String settingsFilepath;
	ClientSettings settings;
	ClientState state = ClientState::IDLE;
	ClientPerformanceStats performanceStats{};
	ClientConnectionStats connectionStats{};
	ClientAudioStats audioStats{};
	ClientReceivedSnapshotBuffer receivedSnapshotBuffer{};
	ClientReceivedChatMessages receivedChatMessages{};
	PlayerID receivedPlayerID{};
	ArrayList<LocalPlayer> localPlayers{};
	LatestCommandsMessageForGameServer latestCommands{};
	TickCommand nextTickCommand{};
	TimePoint lastPredictionTickEventTimestamp{};
	Prediction prediction{};
	Schedule broadphaseUpdateSchedule{};
#ifdef GREM_USE_PROFILING
	bool captureLoadTimeProfile;
#endif
	bool savingScreenshot = false;
	bool openingChat = false;
	bool controlling = false;
	Array<char, 8192> schemaExtensionInputBuffer{};
	Array<char, 256> prefabFilepathInputBuffer{};
	GREM_PROFILE_CONSTRUCTOR_END();
};

GameClient::GameClient(AssetCache& assetCache, Audio& audio, Graphics& graphics, const GameSystems& gameSystems, exec::Executor& executor, GameState& gameState,
	const net::Endpoint& endpoint, const GameClientOptions& options)
	: implementation(UniquePointer<Implementation>::create(assetCache, audio, graphics, gameSystems, executor, gameState, endpoint, options)) {}

GameClient::~GameClient() = default;

void GameClient::disconnect() {
	GREM_PROFILE_FUNCTION();

	implementation->disconnect();
}

void GameClient::reloadAssets() {
	GREM_PROFILE_FUNCTION();

	implementation->reloadAssets();
}

void GameClient::pushFrameWaitTime(Duration frameWaitTime) {
	implementation->pushFrameWaitTime(frameWaitTime);
}

void GameClient::sendChatMessage(String message) {
	implementation->sendChatMessage(std::move(message));
}

void GameClient::receiveChatMessage(String senderName, String message) {
	implementation->receiveChatMessage(std::move(senderName), std::move(message));
}

void GameClient::prepareForEvents() {
	GREM_PROFILE_FUNCTION();

	implementation->prepareForEvents();
}

void GameClient::handleEvent(const evt::Event& event, gfx::Window& window) {
	GREM_PROFILE_FUNCTION();

	implementation->handleEvent(event, window);
}

void GameClient::update(const app::FrameInfo& frameInfo, size_t lastSecondFrameCount, Duration latestServerPhysicsTime, phys::DebugVisualization3D* physicsDebugVisualization) {
	GREM_PROFILE_FUNCTION();

	implementation->update(frameInfo, lastSecondFrameCount, latestServerPhysicsTime, physicsDebugVisualization);
}

void GameClient::display(const phys::DebugVisualization3D* serverPhysicsDebugVisualization, const phys::DebugVisualization3D* clientPhysicsDebugVisualization) {
	GREM_PROFILE_FUNCTION();

	implementation->display(serverPhysicsDebugVisualization, clientPhysicsDebugVisualization);
}

bool GameClient::showSettingsGUI() {
	GREM_PROFILE_FUNCTION();

	return implementation->showSettingsGUI();
}

void GameClient::applyEditedSettings(const ClientSettings& oldSettings) {
	GREM_PROFILE_FUNCTION();

	implementation->applyEditedSettings(oldSettings);
}

void GameClient::saveSettings() {
	implementation->saveSettings();
}

void GameClient::saveScreenshot() {
	implementation->saveScreenshot();
}

void GameClient::stopOpeningChat() {
	implementation->stopOpeningChat();
}

ClientSettings& GameClient::getSettings() noexcept {
	return implementation->getSettings();
}

const ClientSettings& GameClient::getSettings() const noexcept {
	return implementation->getSettings();
}

bool GameClient::isClosed() const noexcept {
	return implementation->isClosed();
}

bool GameClient::isConnecting() const noexcept {
	return implementation->isConnecting();
}

bool GameClient::isConnected() const noexcept {
	return implementation->isConnected();
}

bool GameClient::isDisconnecting() const noexcept {
	return implementation->isDisconnecting();
}

bool GameClient::hasFinishedLoadingAssets() const noexcept {
	return implementation->hasFinishedLoadingAssets();
}

bool GameClient::hasControl() const noexcept {
	return implementation->hasControl();
}

bool GameClient::isOpeningChat() const noexcept {
	return implementation->isOpeningChat();
}
