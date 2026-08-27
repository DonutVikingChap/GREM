// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_PERFORMANCE_STATS_HPP
#define GREM_EXAMPLES_FPS_CLIENT_PERFORMANCE_STATS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/system/Clock.hpp>

#include "TimeSampleBuffer.hpp"

struct ClientPerformanceStats {
	TimeSampleBuffer<500> frameTimeSampleBuffer{};
	TimeSampleBuffer<32> frameWaitTimeSampleBuffer{};
	Duration latestPhysicsTime{};
	TimeSampleBufferStatistics frameTimeStatistics{};
	size_t lastSecondFrameCount = 0;
	Duration latestServerPhysicsTime{};
};

#endif
