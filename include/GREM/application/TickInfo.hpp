// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_TICK_INFO_HPP
#define GREM_APPLICATION_TICK_INFO_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>

namespace grem::application {

/**
 * Transient information about the current tick of an Application.
 */
struct TickInfo {
	/**
	 * The average time that should elapse between each tick.
	 *
	 * This is essentially the TickInfo equivalent of FrameInfo::deltaTime, and
	 * is determined by ApplicationOptions::tickInterval (or
	 * Application::setTickInterval()). The application tries to ensure that
	 * `1 / tickInterval` calls to tick() are executed every second on average.
	 *
	 * The tick interval should be used as the time delta when updating any
	 * physics simulations, timers, etc. within a tick. Using a fixed update
	 * interval like this generally results in more stable, predictable and
	 * consistent behavior compared to using a variable update interval (i.e.
	 * deltaTime), especially with regard to floating-point error and numerical
	 * integration methods which may produce different results depending on the
	 * step size.
	 *
	 * Since common tick rates like 60 Hz are often lower than the refresh rates
	 * of users' monitors (usually 60-240 Hz), and can easily get out of sync
	 * with the variable-rate frame rate regardless, some form of interpolation
	 * and/or extrapolation should always be used in the Application::display()
	 * callback in order to smooth out the result of the fixed-rate ticks
	 * whenever possible.
	 *
	 * A common pattern for linear interpolation in Application::display() is:
	 * ```cpp
	 * displayPosition = mix(previousPosition, position, frameInfo.tickInterpolationAlpha)
	 * ```
	 * where `position` is some simulated value from the latest tick and
	 * `previousPosition` is the previous simulated value saved from the tick
	 * before that.
	 */
	Duration tickInterval;

	/**
	 * Number of ticks that have been fully processed since the start of the
	 * application.
	 */
	size_t totalProcessedTickCount;

	/**
	 * The accumulated time of all ticks that had been processed since the start
	 * of the application at the beginning of the current tick.
	 */
	Duration totalProcessedTickTime;
};

} // namespace grem::application

#endif
