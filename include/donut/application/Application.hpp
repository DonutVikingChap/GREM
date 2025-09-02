#ifndef DONUT_APPLICATION_APPLICATION_HPP
#define DONUT_APPLICATION_APPLICATION_HPP

#include <donut/application/FrameInfo.hpp>
#include <donut/application/TickInfo.hpp>

#include <chrono> // std::chrono::...

namespace donut::application {

/**
 * Configuration options for an Application.
 */
struct ApplicationOptions {
	/**
	 * The tick rate of the application, in hertz (ticks per second).
	 *
	 * This is the rate at which the application will try to trigger the
	 * processing of a tick, which is the main mechanism for providing
	 * application subsystems with updates at a fixed interval, independent from
	 * the main frame rate of the application.
	 *
	 * Tick processing is performed on each frame of the application, which may
	 * result in anywhere from 0 to tickRate/minFrameRate ticks being processed,
	 * depending on the time since the previous frame. When not enough time has
	 * passed to process any ticks within a frame, the time is accumulated for
	 * the next frame, and so on, until enough time has passed to process more
	 * ticks. If several ticks' worth of time passed since the previous frame,
	 * multiple ticks will be processed, and any remaining time will carry over
	 * to the next frame. This results in a fixed average interval between
	 * ticks even in the event of high framerates or small frame rate drops.
	 *
	 * If set to a non-positive value, no tick processing will occur, and tick()
	 * will never be called.
	 *
	 * \sa minFrameRate
	 * \sa maxFrameRate
	 */
	float tickRate = 60.0f;

	/**
	 * Minimum frame rate of the application, in hertz (frames per second),
	 * before tick slowdown occurs.
	 *
	 * If the frame rate drops below this limit, the application will start to
	 * skip the processing of some ticks in order to avoid a spiral of death
	 * where the amount of ticks to process continues to increase faster than
	 * they can be processed, which would lead to the application becoming
	 * completely unresponsive.
	 *
	 * If set to 0 or lower, or to a value higher than the tick rate, the
	 * maximum number of ticks per frame will be set to 1, causing slowdown to
	 * always occur whenever the frame rate goes below the tick rate. This is
	 * generally not recommended.
	 *
	 * \sa tickRate
	 * \sa maxFrameRate
	 */
	float minFrameRate = 1.0f;

	/**
	 * Maximum frame rate of the application, in hertz (frames per second),
	 * before frames are delayed.
	 *
	 * If the frame rate goes above this limit, the application will wait until
	 * enough time has passed for the next frame to begin.
	 *
	 * Set to 0 or a negative value for no frame rate limit.
	 *
	 * \sa tickRate
	 * \sa minFrameRate
	 * \sa frameRateLimiterSleepEnabled
	 */
	float maxFrameRate = 480.0f;

	/**
	 * Put the thread that is running the application to sleep until the next
	 * frame is supposed to begin if the maximum frame rate is exceeded.
	 *
	 * This helps reduce the CPU usage of the application in low-load scenarios.
	 *
	 * \note This option is only applicable when there is a frame rate limit,
	 *       i.e. when #maxFrameRate is positive.
	 *
	 * \sa maxFrameRate
	 * \sa frameRateLimiterSleepBias
	 */
	bool frameRateLimiterSleepEnabled = true;

	/**
	 * The duration offset to subtract from the requested wake-up time when
	 * frame rate limiter sleep is enabled.
	 *
	 * Since there is some overhead associated with waking a thread from sleep,
	 * the thread must be requested to wake up slightly before the next frame is
	 * supposed to begin to avoid missing the deadline, otherwise the actual
	 * frame rate may fluctuate and deviate from the intended target frame rate.
	 *
	 * However, the larger this duration is, the more time will be spent
	 * busy-waiting before each frame begins, which will increase the CPU usage
	 * and limit the effectiveness of frame rate limiter sleep.
	 *
	 * The default value is tuned to produce decent results on most harware,
	 * though it may need to be adjusted depending on application-specific
	 * requirements.
	 *
	 * \note This option is only applicable when #frameRateLimiterSleepEnabled
	 *       is set to true.
	 *
	 * \warning This value must be non-negative.
	 *
	 * \sa frameRateLimiterSleepEnabled
	 */
	std::chrono::steady_clock::duration frameRateLimiterSleepBias = std::chrono::microseconds{100};
};

/**
 * Main application base class.
 *
 * The application controls the main loop, including frame pacing
 * and fixed-interval frame rate-independent tick updates.
 *
 * Concrete applications should derive from this class and implement the
 * associated virtual functions in order to receive all of the relevant
 * callbacks. Deriving from this class also ensures that it is constructed
 * before any code in the concrete application is executed, which means that any
 * global systems will be properly initialized before any code that may depend
 * on them is able to run, assuming all user code is constrained to being called
 * from an instance of the concrete application class.
 */
class Application {
public:
	/**
	 * Construct the base of the main application.
	 *
	 * \param options initial configuration of the application, see
	 *        ApplicationOptions.
	 */
	explicit Application(const ApplicationOptions& options = {});

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
	 * \note Under emscripten-based WebAssembly builds, this function will never
	 *       return. Instead, it explicitly calls the virtual application
	 *       destructor in order to perform any necessary cleanup when the main
	 *       loop has ended. It is therefore expected that any application-
	 *       specific cleanup or shutdown code be called from the overriden
	 *       destructor, rather than being called from main after run() has
	 *       finished.
	 *
	 * \throws any unhandled exception which was thrown during the execution of
	 *         the main loop, unless running under emscripten, in which case
	 *         exceptions are simply printed before shutting down.
	 *
	 * \remark The intended usage of this function is to call it once at the end
	 *         of main function of the program as the last code to be executed,
	 *         save for any catch blocks that are specific to non-emscripten
	 *         builds.
	 *
	 * \warning The result of calling this function while the application is
	 *          already running is undefined.
	 */
	void run();

	/**
	 * Initiate the shutdown process, meaning that the current frame will be the
	 * last to be processed and displayed before the main loop ends.
	 *
	 * This method may be overridden by the concrete application to intercept
	 * requests to quit and perform application-specific processing before
	 * deciding whether to actually quit or not by either calling the base
	 * implementation or choosing to ignore the request.
	 *
	 * \throws any exception thrown by the concrete implementation.
	 */
	virtual void quit();

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
	[[nodiscard]] unsigned getLastSecondFrameCount() const noexcept {
		return lastSecondFrameCount;
	}

	/**
	 * Set the frame rate parameters of the application.
	 *
	 * \param tickRate desired tick rate of the application, in hertz (ticks per
	 *        second). Set to 0 or a negative value for no ticks.
	 * \param minFrameRate minimum frame rate of the application, in hertz
	 *        (frames per second), before tick slowdown occurs.
	 * \param maxFrameRate maximum frame rate of the application, in hertz
	 *        (frames per second), before frames are delayed. Set to 0 or a
	 *        negative value for no frame rate limit.
	 *
	 * \sa ApplicationOptions::tickRate
	 * \sa ApplicationOptions::minFrameRate
	 * \sa ApplicationOptions::maxFrameRate
	 * \sa setFrameRateLimiterSleepEnabled()
	 * \sa setFrameRateLimiterSleepBias()
	 */
	void setFrameRateParameters(float tickRate, float minFrameRate, float maxFrameRate);

	/**
	 * Enable or disable frame rate limiter sleep.
	 *
	 * \param frameRateLimiterSleepEnabled true to enable frame rate limiter sleep,
	 *        false to disable.
	 *
	 * \sa ApplicationOptions::frameRateLimiterSleepEnabled
	 * \sa setFrameRateParameters()
	 * \sa setFrameRateLimiterSleepBias()
	 */
	void setFrameRateLimiterSleepEnabled(bool frameRateLimiterSleepEnabled);

	/**
	 * Set the frame rate limiter sleep bias.
	 *
	 * \param frameRateLimiterSleepBias new bias to set. Must be non-negative.
	 *
	 * \sa ApplicationOptions::frameRateLimiterSleepBias
	 * \sa setFrameRateParameters()
	 * \sa setFrameRateLimiterSleepEnabled()
	 */
	void setFrameRateLimiterSleepBias(std::chrono::steady_clock::duration frameRateLimiterSleepBias);

	/**
	 * Get information about the latest tick.
	 *
	 * \return the latest tick information.
	 */
	[[nodiscard]] TickInfo getLatestTickInfo() const noexcept {
		return latestTickInfo;
	}

	/**
	 * Get information about the latest frame.
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
	 * \note Any exception that is thrown out of this function will percolate up
	 *       to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \warning The behavior of calling this function manually is undefined.
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
	 * \note Any exception that is thrown out of this function will percolate up
	 *       to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \warning The behavior of calling this function manually is undefined.
	 *
	 * \sa update()
	 * \sa display()
	 * \sa getLatestFrameInfo()
	 */
	virtual void tick(TickInfo tickInfo) {
		(void)tickInfo;
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
	 * \param tickInfo information about the latest tick, see TickInfo.
	 * \param frameInfo information about the latest frame, see FrameInfo.
	 *
	 * \note Any exception that is thrown out of this function will percolate up
	 *       to run() and cause the main loop to stop.
	 * \note The default implementation of this function does nothing.
	 *
	 * \warning The behavior of calling this function manually is undefined.
	 *
	 * \sa update()
	 * \sa tick()
	 * \sa getLatestTickInfo()
	 */
	virtual void display(TickInfo tickInfo, FrameInfo frameInfo) {
		(void)tickInfo;
		(void)frameInfo;
	}

private:
	void runFrame();

	using Clock = std::chrono::steady_clock;

	Clock::duration tickInterval{};
	Clock::duration minFrameInterval{};
	Clock::rep maxTicksPerFrame{};
	Clock::time_point startTime{};
	Clock::time_point latestFrameTime{};
	Clock::time_point latestTickProcessingEndTime{};
	Clock::time_point latestFrameCountTime{};
	unsigned lastSecondFrameCount = 0u;
	unsigned frameCounter = 0u;
	TickInfo latestTickInfo{};
	FrameInfo latestFrameInfo{};
	Clock::duration frameRateLimiterSleepBias{};
	bool frameRateLimiterSleepEnabled = false;
	bool running = false;
};

} // namespace donut::application

#endif
