// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_CONNECTION_STATS_HPP
#define GREM_EXAMPLES_FPS_CLIENT_CONNECTION_STATS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/physics/quantities.hpp>

#include "TimeSampleBuffer.hpp"
#include "Timestamp.hpp"

struct ClientConnectionStats {
	TimeSampleBufferStatistics roundTripTimeStatistics{};
	float recentIncomingPacketLossFraction = 0.0f;
	float recentOutgoingPacketLossFraction = 0.0f;
	TimeSampleBuffer<32> predictionDurationErrorSampleBuffer{};
	TimeSampleBufferStatistics predictionDurationErrorStatistics{};
	TickDifference remotePredictionDurationTicks{};
	Duration predictionTimeAdjustmentTimeRemaining{};
	float predictionTimeAdjustmentRate = 0.0f;
	float predictionTimeSpeedup = 1.0f;
	Duration receiveInterpolationOffset{};
	Duration receiveInterpolationOffsetAdjustmentTimeRemaining{};
	float receiveInterpolationOffsetAdjustmentRate = 0.0f;
	TickIndex firstCommandTickIndex{};
	TickIndex firstPredictionSnapshotTickIndex{};
	TickIndex lastPredictionSnapshotTickIndex{};
	size_t incomingDataRate = 0;
	size_t outgoingDataRate = 0;
	size_t incomingDataRateAccumulator = 0;
	size_t outgoingDataRateAccumulator = 0;
	Duration dataRateTimer{};
	size_t incomingDataPerTick = 0;
	size_t outgoingDataPerTick = 0;
	size_t incomingDataPerTickAccumulator = 0;
	size_t outgoingDataPerTickAccumulator = 0;
	phys::Distance positionPredictionError{};
	phys::Time reloadTimeRemainingPredictionError{};
	phys::Angle aimAnglesPredictionError{};
	phys::Coefficient aimingDownSightsPredictionError{};
	bool connectionProblem = false;
};

#endif
