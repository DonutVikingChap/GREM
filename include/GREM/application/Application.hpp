// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_APPLICATION_HPP
#define GREM_APPLICATION_APPLICATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/application/FrameInfo.hpp>
#include <GREM/application/TickInfo.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/statistics.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE

namespace grem::application {

/**
 * Configuration options for an Application.
 */
struct ApplicationOptions {
	/**
	 * Desired average time between ticks, i.e. the reciprocal of the
	 * application's desired tick rate.
	 *
	 * This controls the rate at which the application will try to execute calls
	 * to tick(), which is the main mechanism for providing application
	 * subsystems with updates at a fixed interval, independent from the main
	 * frame rate of the application.
	 *
	 * Tick processing is performed on each frame of the application, which may
	 * result in anywhere from 0 to `maxFrameTime / tickInterval` ticks being
	 * processed, depending on the time elapsed since the previous frame. When
	 * not enough time has passed to process any ticks within a frame, the time
	 * is accumulated for the next frame, and so on, until enough time has
	 * passed to process another tick. If several ticks' worth of time passed
	 * since the previous frame, multiple ticks will be processed (up to
	 * `maxAccumulatedTickTime`, after which ticks will be skipped), and any
	 * remaining (unskipped) time will carry over to the next frame. This
	 * results in a fixed average interval between ticks, even in the event of
	 * high framerates or small frame rate drops (unless
	 * `maxAccumulatedTickTime` is hit).
	 *
	 * If set to zero or lower, no tick processing will occur, and tick() will
	 * never be called.
	 *
	 * \sa maxAccumulatedTickTime
	 * \sa minFrameTime
	 * \sa maxFrameTime
	 */
	Duration tickInterval = duration_cast<Duration>(FloatSeconds{1.0f}) / 60; // 60 Hz

	/**
	 * Maximum time to process accumulated ticks for before starting to skip
	 * ticks.
	 *
	 * If set to zero or lower, an unlimited amount of tick time may accumulate,
	 * and the maximum number of ticks allowed by `maxFrameTime` will keep being
	 * processed every frame until the CPU catches up. This will ensure eventual
	 * consistency with the real wall clock time, but may cause a prolonged
	 * "fast forward" effect when tabbing back into a program that has been
	 * suspended for a long time.
	 *
	 * \sa tickInterval
	 * \sa maxFrameTime
	 */
	Duration maxAccumulatedTickTime = Seconds{1};

	/**
	 * Minimum frame time before frames are delayed, i.e. the reciprocal of the
	 * application's maximum frame rate.
	 *
	 * If the frame rate is too fast, and the frame time goes below this limit,
	 * the application will wait until enough time has passed for the next frame
	 * to begin before continuing.
	 *
	 * If set to zero or lower, the application will have no frame rate limit.
	 *
	 * \sa tickInterval
	 * \sa maxFrameTime
	 * \sa frameRateLimiterSleepEnabled
	 */
	Duration minFrameTime =
#ifdef __EMSCRIPTEN__
		{}
#else
		duration_cast<Duration>(FloatSeconds{1.0f}) / 480 // 480 Hz
#endif
	;

	/**
	 * Maximum frame time before tick time is accumulated and deferred to later
	 * frames, i.e. the reciprocal of the application's minimum acceptable frame
	 * rate.
	 *
	 * If the frame rate is too slow, and the frame time goes above this limit,
	 * the application will start to delay the processing of some ticks in order
	 * to avoid a spiral of death where the amount of ticks to process continues
	 * to increase faster than they can be processed, which would lead to the
	 * application becoming completely unresponsive.
	 *
	 * If set to zero or lower, or to a value lower than the tick interval, the
	 * maximum number of ticks per frame will be set to 1, causing ticks to
	 * always be delayed whenever the frame rate goes below the tick rate. This
	 * is generally not recommended.
	 *
	 * \sa tickInterval
	 * \sa maxAccumulatedTickTime
	 * \sa minFrameTime
	 */
	Duration maxFrameTime = duration_cast<Duration>(FloatSeconds{1.0f}) / 5; // 5 Hz

	/**
	 * Put the thread that is running the application to sleep until the next
	 * frame is supposed to begin if the maximum frame rate is exceeded.
	 *
	 * This helps reduce the CPU usage of the application in low-load scenarios.
	 *
	 * \note This option is only applicable when there is a frame rate limit,
	 *       i.e. when #minFrameTime is positive.
	 *
	 * \sa maxFrameRate
	 */
	bool frameRateLimiterSleepEnabled =
#ifdef __EMSCRIPTEN__
		false
#else
		true
#endif
		;
};

/**
 * Main application base class.
 *
 * Deriving from this class provides a platform-agnostic wrapper for the main
 * loop that includes a built-in frame rate limiter and fixed-interval frame
 * rate-independent tick callbacks.
 */
class Application {
public:
	/**
	 * Construct the base of the main application.
	 *
	 * \param options initial configuration of the application, see
	 *        ApplicationOptions.
	 */
	GREM_API(application) explicit Application(const ApplicationOptions& options = {});

	/**
	 * Virtual destructor which must be overridden by the concrete application
	 * class in order to perform any application-specific cleanup before
	 * shutdown.
	 *
	 * May also be overridden implicitly by the compiler-generated destructor of
	 * the derived class.
	 */
	virtual ~Application() = default;

	/**
	 * Start the main loop of the application and keep running until the
	 * application quits or an unhandled exception is thrown.
	 *
	 * \throws any unhandled exception which was thrown during the execution of
	 *         the main loop, unless running under emscripten, in which case
	 *         exceptions are simply printed before shutting down.
	 *
	 * \note Under emscripten-based WebAssembly builds, this function will never
	 *       return. Instead, it explicitly calls the virtual application
	 *       destructor in order to perform any necessary cleanup when the main
	 *       loop has ended. It is therefore expected that any application-
	 *       specific cleanup or shutdown code be called from the overriden
	 *       destructor, rather than being called from main after run() has
	 *       finished.
	 * \note If the application has already been shut down, it will not enter
	 *       the running state again. To start a new main loop, a new
	 *       application must be constructed.
	 *
	 * \warning The result of calling this function while the application is
	 *          already running is undefined.
	 *
	 * \remark The intended usage of this function is to call it once at the end
	 *         of the main function of the program as the last code to be
	 *         executed, save for any catch blocks that are specific to
	 *         non-emscripten builds.
	 */
	GREM_API(application) void run();

	/**
	 * Initiate the shutdown process, meaning that the current frame will be the
	 * last to be processed before the main loop ends.
	 *
	 * If called during update(), no ticks will be processed afterwards, and the
	 * final frame will not be displayed.
	 *
	 * This method may be overridden by the concrete application to intercept
	 * requests to quit and perform application-specific processing before
	 * deciding whether to actually quit or not by either calling the base
	 * implementation or choosing to ignore the request.
	 *
	 * \throws any exception thrown by the concrete implementation.
	 *
	 * \note This function will be called automatically if an unhandled
	 *       exception is thrown from the main loop.
	 */
	GREM_API(application) virtual void quit() noexcept;

	/**
	 * Check if the application is currently running, meaning that it is fully
	 * initialized, that run() has been called and has started the main loop,
	 * and that it is not in the process of shutting down.
	 *
	 * \return true if the application is currently running, false otherwise.
	 */
	[[nodiscard]] bool isRunning() const noexcept {
		return running;
	}

	/**
	 * Check if the application is in the process of shutting down.
	 *
	 * \return true if quit() has been called, false otherwise.
	 */
	[[nodiscard]] bool isQuitting() const noexcept {
		return quitting;
	}

	/**
	 * Get the number of frames displayed during the last measured second of the
	 * application's run time, which approximates the average frame rate.
	 *
	 * This is measured automatically by counting the number of frames displayed
	 * between each second that passes while the application is running.
	 *
	 * \return the number of frames displayed during the last second that was
	 *         measured, or 0 if less than one full second has passed since the
	 *         start of the application.
	 *
	 * \note This approximation of the frame rate does not update frequently
	 *       enough to be used as an accurate time delta between frames. Use the
	 *       values that are supplied in the FrameInfo struct to each
	 *       relevant callback for this purpose instead.
	 */
	[[nodiscard]] size_t getLastSecondFrameCount() const noexcept {
		return lastSecondFrameCount;
	}

	/**
	 * Get the tick interval of the application.
	 *
	 * \return the tick interval of the application.
	 *
	 * \sa ApplicationOptions::tickInterval
	 * \sa setTickInterval()
	 */
	[[nodiscard]] Duration getTickInterval() const noexcept {
		return latestTickInfo.tickInterval;
	}

	/**
	 * Get the maximum accumulated tick time of the application.
	 *
	 * \return the maximum time to process accumulated ticks for before starting
	 *         to skip ticks.
	 *
	 * \sa ApplicationOptions::maxAccumulatedTickTime
	 * \sa setMaxAccumulatedTickTime()
	 */
	[[nodiscard]] Duration getMaxAccumulatedTickTime() const noexcept {
		return maxAccumulatedTickTime;
	}

	/**
	 * Get the minimum frame time of the application.
	 *
	 * \return the minimum frame time of the application before frames are
	 *         delayed.
	 *
	 * \sa ApplicationOptions::minFrameTime
	 * \sa setMinFrameTime()
	 */
	[[nodiscard]] Duration getMinFrameTime() const noexcept {
		return minFrameTime;
	}

	/**
	 * Get the maximum frame time of the application.
	 *
	 * \return the maximum frame time of the application before tick slowdown
	 *         occurs.
	 *
	 * \sa ApplicationOptions::maxFrameTime
	 * \sa setMaxFrameTime()
	 */
	[[nodiscard]] Duration getMaxFrameTime() const noexcept {
		return maxFrameTime;
	}

	/**
	 * Reset the internal tick accumulator, causing the next tick to start
	 * exactly one tick interval into the future.
	 */
	GREM_API(application) void resetTickTimer() noexcept;

	/**
	 * Set the tick interval of the application.
	 *
	 * \param newTickInterval desired tick interval of the application. Set to zero
	 *        or lower for no ticks.
	 *
	 * \sa ApplicationOptions::tickInterval
	 * \sa getTickInterval()
	 * \sa resetTickTimer()
	 */
	GREM_API(application) void setTickInterval(Duration newTickInterval) noexcept;

	/**
	 * Set the maximum accumulated tick time of the application.
	 *
	 * \param newMaxAccumulatedTickTime maximum time to process accumulated
	 *        ticks for before starting to skip ticks.
	 *
	 * \sa ApplicationOptions::maxAccumulatedTickTime
	 * \sa getMaxAccumulatedTickTime()
	 * \sa setMaxFrameTime()
	 */
	GREM_API(application) void setMaxAccumulatedTickTime(Duration newMaxAccumulatedTickTime) noexcept;

	/**
	 * Set the minimum frame time of the application.
	 *
	 * \param newMinFrameTime minimum frame time of the application before
	 *        frames are delayed. Set to zero or lower for no frame rate limit.
	 *
	 * \sa ApplicationOptions::minFrameTime
	 * \sa getMinFrameTime()
	 */
	GREM_API(application) void setMinFrameTime(Duration newMinFrameTime) noexcept;

	/**
	 * Set the maximum frame time of the application.
	 *
	 * \param newMaxFrameTime maximum frame time of the application before tick
	 *        slowdown occurs.
	 *
	 * \sa ApplicationOptions::maxFrameTime
	 * \sa getMaxFrameTime()
	 * \sa setMaxAccumulatedTickTime()
	 */
	GREM_API(application) void setMaxFrameTime(Duration newMaxFrameTime) noexcept;

	/**
	 * Enable or disable frame rate limiter sleep.
	 *
	 * \param newFrameRateLimiterSleepEnabled true to enable frame rate limiter
	 *        sleep, false to disable.
	 *
	 * \sa ApplicationOptions::frameRateLimiterSleepEnabled
	 */
	GREM_API(application) void setFrameRateLimiterSleepEnabled(bool newFrameRateLimiterSleepEnabled) noexcept;

	/**
	 * Get the latest available tick information, including any configuration
	 * changes since the last tick.
	 *
	 * \return the latest tick information.
	 */
	[[nodiscard]] TickInfo getLatestTickInfo() const noexcept {
		return latestTickInfo;
	}

	/**
	 * Get the latest available frame information, including any configuration
	 * changes since the last frame.
	 *
	 * \return the latest frame information.
	 */
	[[nodiscard]] FrameInfo getLatestFrameInfo() const noexcept {
		return latestFrameInfo;
	}

protected:
	/**
	 * Per-frame update callback, called in the main loop once at the beginning
	 * of each frame, before processing ticks.
	 *
	 * This is the best time to poll events using an EventPump and apply any
	 * changes to interactive application state that depends on user input and
	 * is used by tick(), since it minimizes the average latency between
	 * processing an input event and it affecting the result of a subsequent
	 * tick.
	 *
	 * \param frameInfo information about the current frame, see FrameInfo.
	 *
	 * \note %Any exception that is thrown out of this function will percolate
	 *       up to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \sa tick()
	 * \sa display()
	 * \sa getLatestTickInfo()
	 */
	virtual void update(FrameInfo frameInfo) {
		(void)frameInfo;
	}

	/**
	 * Fixed-rate tick callback, called in the main loop 0 or more times
	 * during tick processing, which happens on each frame after calling
	 * update() and before calling display().
	 *
	 * See the documentation of TickInfo::tickInterval for an explanation of
	 * what this function may be useful for.
	 *
	 * \param tickInfo information about the current tick, see TickInfo.
	 *
	 * \note %Any exception that is thrown out of this function will percolate
	 *       up to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \sa update()
	 * \sa skipTick()
	 * \sa display()
	 * \sa getLatestFrameInfo()
	 */
	virtual void tick(TickInfo tickInfo) {
		(void)tickInfo;
	}

	/**
	 * Callback for ticks that were skipped due to the accumulated tick time
	 * exceeding ApplicationOptions::maxAccumulatedTickTime.
	 *
	 * \param tickInterval the average time that would have elapsed for this
	 *        tick if it hadn't been skipped.
	 *
	 * \note %Any exception that is thrown out of this function will percolate
	 *       up to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \sa tick()
	 */
	virtual void skipTick(Duration tickInterval) {
		(void)tickInterval;
	}

	/**
	 * Frame rendering callback, called in the main loop once at the end of each
	 * frame after processing ticks, in order to render the latest state of the
	 * application.
	 *
	 * Before rendering, this is also the best time to apply any final cosmetic
	 * changes to the state of the application that is about to be presented,
	 * such as interpolation of data that is updated in tick().
	 *
	 * \param frameInfo information about the current frame, see FrameInfo.
	 *
	 * \note %Any exception that is thrown out of this function will percolate
	 *       up to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \sa update()
	 * \sa tick()
	 * \sa getLatestTickInfo()
	 */
	virtual void display(FrameInfo frameInfo) {
		(void)frameInfo;
	}

private:
	GREM_API(application) void updateMaxTicksPerFrame() noexcept;

	GREM_API(application) void runFrame();

	Duration minFrameTime{};
	Duration maxFrameTime{};
	Duration maxAccumulatedTickTime{};
	Clock::rep maxTicksPerFrame{};
	TimePoint startTime{};
	TimePoint latestFrameBeginTime{};
	TimePoint latestTickProcessingEndTime{};
	TimePoint latestFrameCountTime{};
	ExponentialMovingAverage<FloatMilliseconds> frameRateLimiterSleepError{};
	size_t lastSecondFrameCount = 0;
	size_t frameCounter = 0;
	TickInfo latestTickInfo{};
	FrameInfo latestFrameInfo{};
	bool frameRateLimiterSleepEnabled = false;
	bool running = false;
	bool quitting = false;
};

/**
 * Exit code suitable for returning from main() or passing to std::exit().
 */
class ExitCode {
public:
	static const ExitCode SUCCESS; ///< Standard exit code value for indicating that the program completed successfully.
	static const ExitCode FAILURE; ///< Standard exit code value for indicating that the program encountered an error.

	/**
	 * Construct an exit code.
	 *
	 * \param value exit code value.
	 *
	 * \sa ExitCode::SUCCESS
	 * \sa ExitCode::FAILURE
	 */
	constexpr explicit ExitCode(int value) noexcept
		: value(value) {}

	/**
	 * Implicitly convert the exit code to its underlying value.
	 *
	 * \return the exit code value.
	 */
	constexpr operator int() const noexcept {
		return value;
	}

private:
	int value;
};

inline constexpr ExitCode ExitCode::SUCCESS{EXIT_SUCCESS};
inline constexpr ExitCode ExitCode::FAILURE{EXIT_FAILURE};

} // namespace grem::application

#endif
