// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_FRAME_INFO_HPP
#define GREM_APPLICATION_FRAME_INFO_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/system/Clock.hpp>

namespace grem::application {

/**
 * Transient information about the current frame of an Application.
 */
struct FrameInfo {
	/**
	 * The ratio of the latest processed tick's importance compared to the tick
	 * processed before it, for use when interpolating data between the two.
	 */
	float tickInterpolationAlpha;

	/**
	 * Clock time point marking the beginning of the current frame.
	 */
	TimePoint startTime;

	/**
	 * The time that had elapsed since the start of the application at the
	 * beginning of the current frame.
	 */
	Duration totalElapsedTime;

	/**
	 * The time elapsed between the beginning of the previous frame and the
	 * beginning of the current frame.
	 */
	Duration deltaTime;
};

} // namespace grem::application

#endif
