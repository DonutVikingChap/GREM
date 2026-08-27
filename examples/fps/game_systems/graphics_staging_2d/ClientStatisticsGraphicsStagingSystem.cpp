// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/events/InputManager.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../ClientAudioStats.hpp"
#include "../../ClientConnectionStats.hpp"
#include "../../ClientPerformanceStats.hpp"
#include "../../ClientReceivedSnapshotBuffer.hpp"
#include "../../ClientSettings.hpp"
#include "../../Graphics.hpp"
#include "../../PlayerEntityMap.hpp"
#include "../../Snapshot.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_resources.hpp"

class ClientStatisticsGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"ClientStatisticsGraphicsStagingSystem requires graphics."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void stageLocalPlayer2DGraphics(exec::Executor&, Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const Region2D& viewRegion) override {
		GREM_PROFILE_FUNCTION();

		const SessionState& sessionState = worldView.subtickResources.getResource<SessionState>();
		const ClientSettings& settings = worldView.subtickResources.getResource<ClientSettings>();
		const ClientConnectionStats& connectionStats = worldView.subtickResources.getResource<ClientConnectionStats>();

		if (localPlayerID.value == 1) {
			const ClientPerformanceStats& performanceStats = worldView.subtickResources.getResource<ClientPerformanceStats>();
			const ClientAudioStats& audioStats = worldView.subtickResources.getResource<ClientAudioStats>();
			const ClientReceivedSnapshotBuffer& receivedSnapshotBuffer = worldView.subtickResources.getResource<ClientReceivedSnapshotBuffer>();

			if (settings.graphics.showFPS) {
				const phys::Frequency fps = static_cast<float>(performanceStats.lastSecondFrameCount) * phys::HERTZ;
				const vec2 fpsTextPosition{
					static_cast<float>(viewRegion.offset.x) + 15.0f + 2.0f,
					static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) - 15.0f - 20.0f,
				};
				graphics.put2DText(fpsTextPosition, getDurationColor(1_x / fps), formatSmallString<16>("FPS: {}", fps), 2.0f);
			}

			if (settings.graphics.showPerformanceStats) {
				putPerformanceStats(graphics, viewRegion, performanceStats);
			}

			if (settings.connection.showConnectionStats) {
				putConnectionStats(graphics, viewRegion, connectionStats, worldView.tickInterval);
			}

			if (settings.connection.showTimeline) {
				putTimeline(graphics, viewRegion, connectionStats, receivedSnapshotBuffer, worldView.receivedInterpolationTimestamp, worldView.predictionInterpolationTimestamp,
					worldView.subtickTimestamp.getTickIndex(), worldView.tickInterval);
			}

			if (settings.audio.showAudioStats) {
				putAudioStats(graphics, viewRegion, audioStats);
			}

			if (settings.world.showPosition) {
				worldView.subtickResources.getResource<PlayerEntityMap>().forEachPlayerEntity(worldView.playerID, localPlayerID, [&](EntityID entityID) -> bool {
					const LocalPlayerPerspective* const localPlayerPerspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(entityID);
					if (!localPlayerPerspective) {
						return false;
					}

					const vec2 aimAnglesTextPosition{
						static_cast<float>(viewRegion.offset.x) + 15.0f,
						static_cast<float>(viewRegion.offset.y) + 15.0f + 120.0f,
					};
					graphics.put2DText(aimAnglesTextPosition, Color::WHITE,
						formatSmallString<256>("Angles:    {:>8.3f}\n"
											   "Direction: {:>8.3f}\n"
											   "Position:  {:>8.3f}\n"
											   "Velocity:  {:>8.3f}",
							(localPlayerPerspective->aimAngles - 0).as(phys::DEGREES), convertAnglesToForwardDirection(localPlayerPerspective->aimAngles),
							localPlayerPerspective->position, localPlayerPerspective->linearVelocity));
					return true;
				});
			}
		}

		if (sessionState.flags.contains(SessionState::PAUSED)) {
			const vec2 pausedTextPosition{
				static_cast<float>(viewRegion.offset.x) + static_cast<float>(viewRegion.size.width / 2),
				static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) - 15.0f,
			};
			graphics.put2DText(pausedTextPosition, Color::RED, "PAUSED", 2.0f, gfx::TextAlign::CENTER_HORIZONTALLY_TOP);
		}

		if (connectionStats.connectionProblem) {
			graphics.put2DText(viewRegion.offset + viewRegion.size / 2, Color::RED, "CONNECTION PROBLEM", 4.0f, gfx::TextAlign::CENTER);
		}

#ifndef NDEBUG
		const vec2 debugBuildTextPosition{
			static_cast<float>(viewRegion.offset.x) + static_cast<float>(viewRegion.size.width) - 15.0f - 2.0f,
			static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) - 15.0f - 20.0f,
		};
		graphics.put2DText(debugBuildTextPosition, Color::RED, "DEBUG BUILD", 2.0f, gfx::TextAlign::RIGHT);
#endif
	}

private:
	template <typename T>
	[[nodiscard]] static Color getSignColor(const T& quantity) {
		return (quantity < T{}) ? Color::RED : Color::LIME;
	}

	[[nodiscard]] static Color getDurationColor(Duration time) {
		return (time > 1_x / 60_Hertz) ? Color::RED : (time > 1_x / 120_Hertz) ? Color::YELLOW : (time > 1_x / 240_Hertz) ? Color::GRAY : Color::LIME;
	}

	[[nodiscard]] static Color getVolumeColor(float sample) {
		sample = abs(sample);
		return (sample >= 0.75f) ? Color::RED : (sample >= 0.5f) ? Color::YELLOW : Color::LIME;
	}

	static void putPerformanceStats(Graphics& graphics, Region2D viewRegion, const ClientPerformanceStats& performanceStats) {
		GREM_PROFILE_FUNCTION();

		vec2 position{
			static_cast<float>(viewRegion.offset.x) + 15.0f + 2.0f,
			static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) - 15.0f - 40.0f,
		};
		const Duration latestFrameTime = (performanceStats.frameTimeSampleBuffer.getSamples().empty()) ? Duration{} : performanceStats.frameTimeSampleBuffer.getSamples().back();
		graphics.put2DText(position, getDurationColor(performanceStats.frameTimeStatistics.mean + performanceStats.frameTimeStatistics.standardDeviation),
			formatSmallString<64>("FT: {:.2f} ms (+/- {:.2f} ms), latest: {:.2f} ms", duration_cast<FloatMilliseconds>(performanceStats.frameTimeStatistics.mean).count(),
				duration_cast<FloatMilliseconds>(performanceStats.frameTimeStatistics.standardDeviation).count(), duration_cast<FloatMilliseconds>(latestFrameTime).count()),
			2.0f);
		position.y -= 20.0f;
		const TimeSampleBufferStatistics frameWaitTimeStatistics = performanceStats.frameWaitTimeSampleBuffer.getStatistics();
		const Duration averageFrameWaitTime = frameWaitTimeStatistics.mean + frameWaitTimeStatistics.standardDeviation;
		const Color frameWaitTimeColor =
			(averageFrameWaitTime > 5_milliseconds)   ? Color::RED
			: (averageFrameWaitTime > 2_milliseconds) ? Color::YELLOW
			: (averageFrameWaitTime > 1_milliseconds)
				? Color::GRAY
				: Color::LIME;
		const Duration frameWaitTime =
			(performanceStats.frameWaitTimeSampleBuffer.getSamples().empty()) ? Duration{} : performanceStats.frameWaitTimeSampleBuffer.getSamples().back();
		graphics.put2DText(position, frameWaitTimeColor,
			formatSmallString<64>("GPU wait: {:.2f} ms (+/- {:.2f} ms), latest: {:.2f} ms", duration_cast<FloatMilliseconds>(frameWaitTimeStatistics.mean).count(),
				duration_cast<FloatMilliseconds>(frameWaitTimeStatistics.standardDeviation).count(), duration_cast<FloatMilliseconds>(frameWaitTime).count()),
			2.0f);
		position.y -= 20.0f;
		graphics.put2DText(position, getDurationColor(performanceStats.latestPhysicsTime),
			formatSmallString<32>("Client physics: {:.2f} ms", duration_cast<FloatMilliseconds>(performanceStats.latestPhysicsTime).count()), 2.0f);
		position.y -= 20.0f;
		graphics.put2DText(position, getDurationColor(performanceStats.latestServerPhysicsTime),
			formatSmallString<32>("Server physics: {:.2f} ms", duration_cast<FloatMilliseconds>(performanceStats.latestServerPhysicsTime).count()), 2.0f);

		constexpr float SAMPLE_WIDTH = 1.0f;
		position = {
			static_cast<float>(viewRegion.offset.x) + 15.0f,
			static_cast<float>(viewRegion.offset.y) + 15.0f,
		};
		for (const Duration frameTime : performanceStats.frameTimeSampleBuffer.getSamples()) {
			const Color color = getDurationColor(frameTime);
			graphics.instances2D.putRectangleInstance({
				.position = position,
				.size{SAMPLE_WIDTH, duration_cast<DurationBase<float, Ratio<1, 2000>>>(frameTime).count()},
				.color = color,
			});
			position.x += SAMPLE_WIDTH;
		}
	}

	static void putConnectionStats(Graphics& graphics, Region2D viewRegion, const ClientConnectionStats& connectionStats, Duration tickInterval) {
		GREM_PROFILE_FUNCTION();

		constexpr float ERROR_BAR_HEIGHT = 6.0f;

		vec2 position{
			static_cast<float>(viewRegion.offset.x) + 15.0f + 2.0f,
			static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) * 0.5f + 144.0f,
		};

		graphics.put2DText(position, Color::GRAY, formatSmallString<32>("Tick interval: {:.6f} ms", duration_cast<FloatMilliseconds>(tickInterval).count()));
		position.y -= 10.0f;
		graphics.put2DText(position, Color::WHITE,
			formatSmallString<64>("Data in: {} B/s ({:.2f} Mbps, {} B/tick)", connectionStats.incomingDataRate,
				static_cast<float>(connectionStats.incomingDataRate) * 8.0f / 1'000'000.0f, connectionStats.incomingDataPerTick));
		position.y -= 10.0f;
		graphics.put2DText(position, Color::WHITE,
			formatSmallString<64>("Data out: {} B/s ({:.2f} Mbps, {} B/tick)", connectionStats.outgoingDataRate,
				static_cast<float>(connectionStats.outgoingDataRate) * 8.0f / 1'000'000.0f, connectionStats.outgoingDataPerTick));
		position.y -= 10.0f;
		graphics.put2DText(position,
			(connectionStats.recentIncomingPacketLossFraction == 0.0f && connectionStats.recentOutgoingPacketLossFraction == 0.0f) ? Color::GRAY : Color::RED,
			formatSmallString<64>("Packet loss: in: {:.2f} %, out: {:.2f} %)", connectionStats.recentIncomingPacketLossFraction * 100.0f,
				connectionStats.recentOutgoingPacketLossFraction * 100.0f));

		position.y -= 16.0f;
		graphics.put2DText(position, Color::GRAY,
			formatSmallString<64>("Round-trip time: {:.2f} ms (+/- {:.2f} ms)", duration_cast<FloatMilliseconds>(connectionStats.roundTripTimeStatistics.mean).count(),
				duration_cast<FloatMilliseconds>(connectionStats.roundTripTimeStatistics.standardDeviation).count()));
		position.y -= 10.0f;
		graphics.put2DText(position, Color::WHITE,
			formatSmallString<64>("Prediction duration error: {:.2f} ms (+/- {:.2f} ms)",
				duration_cast<FloatMilliseconds>(connectionStats.predictionDurationErrorStatistics.mean).count(),
				duration_cast<FloatMilliseconds>(connectionStats.predictionDurationErrorStatistics.standardDeviation).count()));
		position.y -= 10.0f;
		graphics.put2DText(position, getSignColor(connectionStats.remotePredictionDurationTicks),
			formatSmallString<64>("Remote prediction duration: {:.2f} ms", duration_cast<FloatMilliseconds>(connectionStats.remotePredictionDurationTicks * tickInterval).count()));
		position.y -= 10.0f;
		graphics.put2DText(position, getSignColor(connectionStats.predictionTimeSpeedup - 1_x),
			formatSmallString<64>("Prediction speedup: {:.6f} x", connectionStats.predictionTimeSpeedup));

		position.y -= 16.0f;
		graphics.put2DText(position, Color::WHITE, formatSmallString<48>("Position prediction error: {:.6f}", connectionStats.positionPredictionError.as(phys::MILLIMETERS)));
		position.y -= 6.0f + ERROR_BAR_HEIGHT;
		graphics.instances2D.putRectangleInstance({
			.position = position,
			.size{connectionStats.positionPredictionError * 10000_per_meter, ERROR_BAR_HEIGHT},
			.color = Color::RED,
		});
		position.y -= 16.0f;
		graphics.put2DText(position, Color::WHITE,
			formatSmallString<64>("Reload time remaining prediction error: {:+.6f}", connectionStats.reloadTimeRemainingPredictionError.as(phys::MILLISECONDS)));
		position.y -= 6.0f + ERROR_BAR_HEIGHT;
		graphics.instances2D.putRectangleInstance({
			.position = position,
			.size{abs(connectionStats.reloadTimeRemainingPredictionError) * 50000_per_second, ERROR_BAR_HEIGHT},
			.color = getSignColor(connectionStats.reloadTimeRemainingPredictionError),
		});
		position.y -= 16.0f;
		graphics.put2DText(position, Color::WHITE, formatSmallString<64>("Aim angles prediction error: {:.8f} deg", connectionStats.aimAnglesPredictionError.in(phys::DEGREES)));
		position.y -= 6.0f + ERROR_BAR_HEIGHT;
		graphics.instances2D.putRectangleInstance({
			.position = position,
			.size{connectionStats.aimAnglesPredictionError * 100000_x / 1_radians, ERROR_BAR_HEIGHT},
			.color = Color::RED,
		});
		position.y -= 16.0f;
		graphics.put2DText(position, Color::WHITE,
			formatSmallString<64>("Aiming down sights prediction error: {:+.8f} %", connectionStats.aimingDownSightsPredictionError * 100_x));
		position.y -= 6.0f + ERROR_BAR_HEIGHT;
		graphics.instances2D.putRectangleInstance({
			.position = position,
			.size{abs(connectionStats.aimingDownSightsPredictionError) * 10000_x, ERROR_BAR_HEIGHT},
			.color = getSignColor(connectionStats.aimingDownSightsPredictionError),
		});
	}

	static void putAudioStats(Graphics& graphics, Region2D viewRegion, const ClientAudioStats& audioStats) {
		GREM_PROFILE_FUNCTION();

		const float xStart = static_cast<float>(viewRegion.offset.x) + static_cast<float>(viewRegion.size.width) - 15.0f;
		vec2 position{xStart, static_cast<float>(viewRegion.offset.y) + 15.0f};

		constexpr float VOLUME_BAR_WIDTH = 16.0f;
		constexpr float VOLUME_BAR_MAX_HEIGHT = 640.0f;
		constexpr float SAMPLE_WIDTH = 1.0f;
		constexpr float SAMPLE_MAX_HEIGHT = 128.0f;
		constexpr float FFT_WIDTH = 1.0f;
		constexpr float FFT_HEIGHT = 8.0f;

		position.x -= VOLUME_BAR_WIDTH;
		graphics.instances2D.putRectangleInstance(
			{.position = position, .size{VOLUME_BAR_WIDTH, audioStats.rightOutputVolume * VOLUME_BAR_MAX_HEIGHT}, .color = getVolumeColor(audioStats.rightOutputVolume)});

		position.x -= 8.0f + VOLUME_BAR_WIDTH;
		graphics.instances2D.putRectangleInstance(
			{.position = position, .size{VOLUME_BAR_WIDTH, audioStats.leftOutputVolume * VOLUME_BAR_MAX_HEIGHT}, .color = getVolumeColor(audioStats.leftOutputVolume)});

		position.x -= 15.0f + 256.0f * max(SAMPLE_WIDTH, FFT_WIDTH);
		for (const float value : audioStats.outputFFT) {
			graphics.instances2D.putRectangleInstance({.position = position, .size{FFT_WIDTH, value * FFT_HEIGHT}, .color = getVolumeColor(value)});
			position.x += FFT_WIDTH;
		}

		position.x -= static_cast<float>(audioStats.outputFFT.size()) * FFT_WIDTH;
		position.y += 48.0f + 15.0f + SAMPLE_MAX_HEIGHT;
		for (const float sample : audioStats.outputWave) {
			graphics.instances2D.putRectangleInstance({.position = position, .size{SAMPLE_WIDTH, sample * SAMPLE_MAX_HEIGHT}, .color = getVolumeColor(sample)});
			position.x += SAMPLE_WIDTH;
		}
	}

	static void putTimeline(Graphics& graphics, Region2D viewRegion, const ClientConnectionStats& connectionStats, const ClientReceivedSnapshotBuffer& receivedSnapshotBuffer,
		Timestamp receivedInterpolationTimestamp, Timestamp predictionInterpolationTimestamp, TickIndex currentTickIndex, Duration tickInterval) {
		GREM_PROFILE_FUNCTION();

		constexpr float HEIGHT = 10.0f;
		constexpr float INNER_HEIGHT = 6.0f;
		constexpr float INNER_HEIGHT_MARGIN = (HEIGHT - INNER_HEIGHT) * 0.5f;
		constexpr float SMALL_HEIGHT = 2.0f;
		constexpr float SMALL_HEIGHT_MARGIN = (HEIGHT - SMALL_HEIGHT) * 0.5f;
		constexpr float TICK_MARKER_WIDTH = 2.0f;
		constexpr float TICK_MARKER_HEIGHT = 18.0f;
		constexpr float TICK_WIDTH = 18.0f;

		const size_t visibleTickCount = (SNAPSHOT_BUFFER_WINDOW_SIZE + SNAPSHOT_BUFFER_WINDOW_MARGIN) / 4 + static_cast<size_t>(100_milliseconds / tickInterval) + 1;
		const phys::Frequency timeWidthScale = TICK_WIDTH / phys::Time{tickInterval};
		const TickIndex baseTickIndex = predictionInterpolationTimestamp.getTickIndex().getPrevious(visibleTickCount - 2);

		vec2 position{
			static_cast<float>(viewRegion.offset.x) + 15.0f,
			static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height) * 0.5f - 70.0f,
		};

		const auto getTimestampX = [&](Timestamp timestamp) -> float {
			return position.x + getTimeBetween(Timestamp{baseTickIndex}, timestamp, tickInterval) * timeWidthScale;
		};

		const auto getTickX = [&](TickIndex tickIndex) -> float {
			return position.x + static_cast<float>(tickIndex - baseTickIndex) * TICK_WIDTH;
		};

		if (const SnapshotBufferView receivedSnapshots = receivedSnapshotBuffer.getSnapshots(); !receivedSnapshots.empty()) {
			const TickIndex firstReceivedSnapshotTickIndex = receivedSnapshots.front().tickIndex;
			const TickIndex lastReceivedSnapshotTickIndex = receivedSnapshots.back().tickIndex;
			for (TickIndex tickIndex = baseTickIndex; tickIndex <= currentTickIndex; ++tickIndex) {
				const bool isInBuffer = tickIndex >= firstReceivedSnapshotTickIndex && tickIndex <= lastReceivedSnapshotTickIndex;
				const bool isReceived = isInBuffer && receivedSnapshotBuffer.isSnapshotReceived(tickIndex);
				const Color color = (isReceived) ? Color::WHITE : (isInBuffer) ? Color::GRAY : Color::BLACK;
				graphics.instances2D.putRectangleInstance({.position{getTickX(tickIndex) - TICK_MARKER_WIDTH * 0.5f, position.y + (HEIGHT - TICK_MARKER_HEIGHT) * 0.5f},
					.size{TICK_MARKER_WIDTH, TICK_MARKER_HEIGHT},
					.color = color});
			}
			for (TickIndex tickIndex = firstReceivedSnapshotTickIndex.getNext(); tickIndex <= lastReceivedSnapshotTickIndex; ++tickIndex) {
				const TickIndex leftTickIndex = tickIndex.getPrevious();
				const TickIndex rightTickIndex = tickIndex;
				const bool isLeftReceived = receivedSnapshotBuffer.isSnapshotReceived(leftTickIndex);
				const bool isRightReceived = receivedSnapshotBuffer.isSnapshotReceived(rightTickIndex);
				const Color color = (isLeftReceived && isRightReceived) ? Color::GRAY : Color::DARK_GRAY;
				graphics.instances2D.putRectangleInstance({.position{getTickX(leftTickIndex), position.y}, .size{TICK_WIDTH, HEIGHT}, .color = color});
			}
			graphics.instances2D.putRectangleInstance({.position{getTickX(firstReceivedSnapshotTickIndex), position.y + INNER_HEIGHT_MARGIN},
				.size{getTimestampX(receivedInterpolationTimestamp) - getTickX(firstReceivedSnapshotTickIndex), INNER_HEIGHT},
				.color = Color::WHITE});
			graphics.instances2D.putRectangleInstance({.position{getTimestampX(receivedInterpolationTimestamp), position.y + INNER_HEIGHT_MARGIN},
				.size{connectionStats.roundTripTimeStatistics.standardDeviation * timeWidthScale, INNER_HEIGHT},
				.color = Color::DARK_GRAY});
			graphics.instances2D.putRectangleInstance({.position{getTimestampX(receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex()), position.y + SMALL_HEIGHT_MARGIN},
				.size{connectionStats.remotePredictionDurationTicks * tickInterval * timeWidthScale, SMALL_HEIGHT},
				.color = (connectionStats.remotePredictionDurationTicks < 0) ? Color::RED * Color::fromLinear(0.5f) : Color::LIME * Color::fromLinear(0.5f)});
		} else {
			for (TickIndex tickIndex = baseTickIndex; tickIndex <= currentTickIndex; ++tickIndex) {
				graphics.instances2D.putRectangleInstance({.position{getTickX(tickIndex) - TICK_MARKER_WIDTH * 0.5f, position.y + (HEIGHT - TICK_MARKER_HEIGHT) * 0.5f},
					.size{TICK_MARKER_WIDTH, TICK_MARKER_HEIGHT},
					.color = Color::BLACK});
			}
		}
		graphics.put2DText({getTickX(currentTickIndex) + 32.0f, position.y + HEIGHT * 0.5f}, Color::WHITE, "Received", 1.0f, gfx::TextAlign::CENTER_VERTICALLY);

		position.y -= 10.0f + TICK_MARKER_HEIGHT;

		for (TickIndex tickIndex = baseTickIndex; tickIndex <= currentTickIndex; ++tickIndex) {
			const bool isInBuffer = tickIndex >= connectionStats.firstPredictionSnapshotTickIndex && tickIndex <= connectionStats.lastPredictionSnapshotTickIndex;
			const Color color = (isInBuffer) ? Color::WHITE : Color::BLACK;
			graphics.instances2D.putRectangleInstance({.position{getTickX(tickIndex) - TICK_MARKER_WIDTH * 0.5f, position.y + (HEIGHT - TICK_MARKER_HEIGHT) * 0.5f},
				.size{TICK_MARKER_WIDTH, TICK_MARKER_HEIGHT},
				.color = color});
		}
		for (TickIndex tickIndex = connectionStats.firstPredictionSnapshotTickIndex.getNext(); tickIndex <= connectionStats.lastPredictionSnapshotTickIndex; ++tickIndex) {
			const TickIndex leftTickIndex = tickIndex.getPrevious();
			graphics.instances2D.putRectangleInstance({.position{getTickX(leftTickIndex), position.y}, .size{TICK_WIDTH, HEIGHT}, .color = Color::GRAY});
		}
		graphics.instances2D.putRectangleInstance({.position{getTickX(connectionStats.firstPredictionSnapshotTickIndex), position.y + INNER_HEIGHT_MARGIN},
			.size{getTimestampX(predictionInterpolationTimestamp) - getTickX(connectionStats.firstPredictionSnapshotTickIndex), INNER_HEIGHT},
			.color = Color::WHITE});
		graphics.instances2D.putRectangleInstance({.position{getTimestampX(predictionInterpolationTimestamp), position.y + INNER_HEIGHT_MARGIN},
			.size{connectionStats.predictionDurationErrorStatistics.standardDeviation * timeWidthScale, INNER_HEIGHT},
			.color = Color::LIGHT_GRAY});
		graphics.instances2D.putRectangleInstance({.position{getTimestampX(predictionInterpolationTimestamp), position.y + SMALL_HEIGHT_MARGIN},
			.size{-connectionStats.predictionDurationErrorStatistics.mean * timeWidthScale, SMALL_HEIGHT},
			.color = getSignColor(-connectionStats.predictionDurationErrorStatistics.mean)});

		graphics.put2DText({getTickX(currentTickIndex) + 32.0f, position.y + HEIGHT * 0.5f}, Color::WHITE, "Prediction", 1.0f, gfx::TextAlign::CENTER_VERTICALLY);

		position.y -= 10.0f + TICK_MARKER_HEIGHT;

		const TickIndex latestReceivedSnapshotTickIndex = receivedSnapshotBuffer.getLatestReceivedSnapshotTickIndex();
		for (TickIndex tickIndex = baseTickIndex; tickIndex <= currentTickIndex; ++tickIndex) {
			const bool isInBuffer = tickIndex >= connectionStats.firstCommandTickIndex && tickIndex < currentTickIndex;
			const bool isConfirmed = isInBuffer && tickIndex <= latestReceivedSnapshotTickIndex;
			const Color color = (isConfirmed) ? Color::WHITE : (isInBuffer) ? Color::GRAY : Color::BLACK;
			graphics.instances2D.putRectangleInstance({.position{getTickX(tickIndex) - TICK_MARKER_WIDTH * 0.5f, position.y + (HEIGHT - TICK_MARKER_HEIGHT) * 0.5f},
				.size{TICK_MARKER_WIDTH, TICK_MARKER_HEIGHT},
				.color = color});
		}
		for (TickIndex tickIndex = connectionStats.firstCommandTickIndex.getNext(); tickIndex <= currentTickIndex; ++tickIndex) {
			const TickIndex leftTickIndex = tickIndex.getPrevious();
			const TickIndex rightTickIndex = tickIndex;
			const bool isLeftConfirmed = leftTickIndex <= latestReceivedSnapshotTickIndex;
			const bool isRightConfirmed = rightTickIndex <= latestReceivedSnapshotTickIndex;
			const Color color = (isLeftConfirmed && isRightConfirmed) ? Color::GRAY : Color::DARK_GRAY;
			graphics.instances2D.putRectangleInstance({.position{getTickX(leftTickIndex), position.y}, .size{TICK_WIDTH, HEIGHT}, .color = color});
		}
		const Timestamp predictionTimestamp = predictionInterpolationTimestamp.withTicksAdded(1);
		graphics.instances2D.putRectangleInstance({.position{getTickX(latestReceivedSnapshotTickIndex), position.y + INNER_HEIGHT_MARGIN},
			.size{getTimestampX(predictionTimestamp) - getTickX(latestReceivedSnapshotTickIndex), INNER_HEIGHT},
			.color = Color::WHITE});
		graphics.instances2D.putRectangleInstance({.position{getTimestampX(predictionTimestamp), position.y + INNER_HEIGHT_MARGIN},
			.size{connectionStats.predictionDurationErrorStatistics.standardDeviation * timeWidthScale, INNER_HEIGHT},
			.color = Color::LIGHT_GRAY});
		graphics.instances2D.putRectangleInstance({.position{getTimestampX(predictionTimestamp), position.y + SMALL_HEIGHT_MARGIN},
			.size{-connectionStats.predictionDurationErrorStatistics.mean * timeWidthScale, SMALL_HEIGHT},
			.color = getSignColor(-connectionStats.predictionDurationErrorStatistics.mean)});
		graphics.put2DText({getTickX(currentTickIndex) + 32.0f, position.y + HEIGHT * 0.5f}, Color::WHITE, "Commands", 1.0f, gfx::TextAlign::CENTER_VERTICALLY);

		for (TickIndex tickIndex = baseTickIndex; tickIndex <= currentTickIndex; ++tickIndex) {
			if ((currentTickIndex - tickIndex) % 5 == 0) {
				graphics.put2DText({getTickX(tickIndex), position.y + (HEIGHT - TICK_MARKER_HEIGHT) * 0.5f - 12.0f}, Color::WHITE,
					formatSmallString<16>("{}", tickIndex - TickIndex{}), 1.0f, gfx::TextAlign::CENTER_HORIZONTALLY_TOP);
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createClientStatisticsGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new ClientStatisticsGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
