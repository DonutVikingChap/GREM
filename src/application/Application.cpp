// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/application/Application.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Thread.hpp>

#ifdef __EMSCRIPTEN__
#include <GREM/core/Error.hpp>
#include <GREM/core/formatting.hpp>

#include <emscripten.h> // emscripten_...
#endif

namespace grem::application {

Application::Application(const ApplicationOptions& options)
	: minFrameTime(max(options.minFrameTime, Duration{}))
	, maxFrameTime(max(options.maxFrameTime, Duration{}))
	, maxAccumulatedTickTime(max(options.maxAccumulatedTickTime, Duration{}))
	, latestTickInfo{.tickInterval = max(options.tickInterval, Duration{}), .totalProcessedTickCount = 0, .totalProcessedTickTime{}}
	, frameRateLimiterSleepEnabled(options.frameRateLimiterSleepEnabled) {
	updateMaxTicksPerFrame();

	GREM_PROFILER_SET_THREAD_INFO("Main thread", 0, ThreadID{});
	GREM_PROFILER_BEGIN_FRAME();
}

void Application::run() {
	GREM_PROFILER_END_FRAME();

	startTime = Clock::now();
	latestFrameBeginTime = startTime;
	latestTickProcessingEndTime = startTime;
	latestFrameCountTime = startTime;
	frameRateLimiterSleepError.reset();
	lastSecondFrameCount = 0u;
	frameCounter = 0u;
	latestTickInfo.totalProcessedTickCount = 0;
	latestTickInfo.totalProcessedTickTime = {};
	latestFrameInfo.tickInterpolationAlpha = 0.0f;
	latestFrameInfo.startTime = startTime;
	latestFrameInfo.totalElapsedTime = {};
	latestFrameInfo.deltaTime = {};
	running = !quitting;

#ifdef __EMSCRIPTEN__
	if (isRunning()) {
		constexpr auto runEmscriptenFrame = [](void* arg) -> void {
			Application* const application = static_cast<Application*>(arg);
			try {
				application->runFrame();
			} catch (...) {
				eprintln("Fatal error: {}", Error::formatCurrentExceptionMessage());
				application->running = false;
			}
			if (!application->isRunning()) {
				application->~Application();
				EM_ASM(FS.syncfs(false, function(err){}););
				emscripten_cancel_main_loop();
			}
		};
		emscripten_set_main_loop_arg(runEmscriptenFrame, this, 0, 1);
	}
#else
	while (isRunning()) {
		try {
			runFrame();
		} catch (...) {
			running = false;
			throw;
		}
	}
#endif
}

void Application::quit() noexcept {
	running = false;
	quitting = true;
}

void Application::resetTickTimer() noexcept {
	latestTickProcessingEndTime = Clock::now();
}

void Application::setTickInterval(Duration newTickInterval) noexcept {
	latestTickInfo.tickInterval = max(newTickInterval, Duration{});
	updateMaxTicksPerFrame();
}

void Application::setMinFrameTime(Duration newMinFrameTime) noexcept {
	minFrameTime = max(newMinFrameTime, Duration{});
	updateMaxTicksPerFrame();
}

void Application::setMaxFrameTime(Duration newMaxFrameTime) noexcept {
	maxFrameTime = max(newMaxFrameTime, Duration{});
	updateMaxTicksPerFrame();
}

void Application::setMaxAccumulatedTickTime(Duration newMaxAccumulatedTickTime) noexcept {
	maxAccumulatedTickTime = max(newMaxAccumulatedTickTime, Duration{});
}

void Application::setFrameRateLimiterSleepEnabled(bool newFrameRateLimiterSleepEnabled) noexcept {
	frameRateLimiterSleepEnabled = newFrameRateLimiterSleepEnabled;
}

void Application::updateMaxTicksPerFrame() noexcept {
	if (latestTickInfo.tickInterval <= Duration{}) {
		maxTicksPerFrame = Clock::rep{0};
	} else if (maxFrameTime <= Duration{} || maxFrameTime <= latestTickInfo.tickInterval) {
		maxTicksPerFrame = Clock::rep{1};
	} else if (minFrameTime > Duration{} && maxFrameTime <= minFrameTime) {
		maxTicksPerFrame = minFrameTime / latestTickInfo.tickInterval;
	} else {
		maxTicksPerFrame = maxFrameTime / latestTickInfo.tickInterval;
	}
}

void Application::runFrame() {
	const TimePoint waitEndTime = latestFrameBeginTime + minFrameTime;
	TimePoint currentTime = Clock::now();
#ifndef __EMSCRIPTEN__
	if (frameRateLimiterSleepEnabled) {
		constexpr float EXPONENTIAL_MOVING_AVERAGE_ALPHA = 0.05f;
		constexpr float BIAS_COEFFICIENT = 1.5f;
		constexpr Duration MAX_BIAS = Milliseconds{30};
		constexpr Duration MAX_REASONABLE_ERROR_MEASUREMENT = Milliseconds{100};

		const Duration frameRateLimiterSleepBias = clamp(duration_cast<Duration>(frameRateLimiterSleepError.get() * BIAS_COEFFICIENT), Duration{}, MAX_BIAS);
		const Duration desiredSleepDuration = waitEndTime - currentTime;
		const Duration specifiedSleepDuration = desiredSleepDuration - frameRateLimiterSleepBias;
		if (specifiedSleepDuration > Duration{}) {
			const TimePoint sleepStartTime = currentTime;
			sleepFor(specifiedSleepDuration);
			currentTime = Clock::now();

			const Duration actualSleepDuration = currentTime - sleepStartTime;
			const Duration sleepDurationError = actualSleepDuration - specifiedSleepDuration;
			if (sleepDurationError > Duration{} && sleepDurationError < MAX_REASONABLE_ERROR_MEASUREMENT) {
				frameRateLimiterSleepError.update(duration_cast<FloatMilliseconds>(sleepDurationError), EXPONENTIAL_MOVING_AVERAGE_ALPHA);
			}
		}
	}
#endif

	GREM_PROFILER_BEGIN_FRAME();

	{
		GREM_PROFILE_BLOCK("Busy wait for update time");
		while (currentTime < waitEndTime) {
			currentTime = Clock::now();
		}
	}

	++frameCounter;
	if (currentTime - latestFrameCountTime >= Seconds{1}) {
		latestFrameCountTime = currentTime;
		lastSecondFrameCount = frameCounter;
		frameCounter = 0;
	}

	latestFrameInfo.startTime = currentTime;
	latestFrameInfo.totalElapsedTime = currentTime - startTime;
	latestFrameInfo.deltaTime = currentTime - latestFrameBeginTime;
	latestFrameBeginTime = currentTime;

	{
		GREM_PROFILE_BLOCK("Update");
		update(latestFrameInfo);
	}

	if (!running) {
		return;
	}

	if (maxTicksPerFrame > 0) {
		Duration timeSinceLatestTick = currentTime - latestTickProcessingEndTime;
		while (maxAccumulatedTickTime > Duration{} && timeSinceLatestTick > maxAccumulatedTickTime) {
			{
				GREM_PROFILE_BLOCK("Skip tick");
				skipTick(latestTickInfo.tickInterval);
			}
			latestTickProcessingEndTime += latestTickInfo.tickInterval;
			timeSinceLatestTick = currentTime - latestTickProcessingEndTime;
		}

		for (Clock::rep ticksToProcess = min(timeSinceLatestTick / latestTickInfo.tickInterval, maxTicksPerFrame); ticksToProcess-- > 0;) {
			{
				GREM_PROFILE_BLOCK("Tick");
				tick(latestTickInfo);
			}
			++latestTickInfo.totalProcessedTickCount;
			latestTickInfo.totalProcessedTickTime += latestTickInfo.tickInterval;
			latestTickProcessingEndTime += latestTickInfo.tickInterval;
		}

		latestFrameInfo.tickInterpolationAlpha =
			min(1.0f, duration_cast<FloatSeconds>(currentTime - latestTickProcessingEndTime) / duration_cast<FloatSeconds>(latestTickInfo.tickInterval));
	} else {
		latestFrameInfo.tickInterpolationAlpha = 0.0f;
	}

	{
		GREM_PROFILE_BLOCK("Display");
		display(latestFrameInfo);
	}

	GREM_PROFILER_END_FRAME();
}

} // namespace grem::application
