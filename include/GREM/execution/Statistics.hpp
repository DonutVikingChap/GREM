// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_STATISTICS_HPP
#define GREM_EXECUTION_STATISTICS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>

namespace grem::execution {

/**
 * Performance statistics produced by an Executor invocation.
 */
struct Statistics {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
	/**
	 * Statistics produced by a distinct worker thread.
	 */
	struct alignas(64) Worker {
		/**
		 * Statistics produced by a distinct task.
		 */
		struct Task {
			size_t taskIndex;    ///< Index of the task in the topologically ordered task list of the task graph.
			TimePoint startTime; ///< Time when execution of the task started.
			TimePoint endTime;   ///< Time when execution of the task ended.
		};

		TimePoint startTime{}; ///< Time when execution of the full task graph started on this worker thread.
		TimePoint endTime{};   ///< Time when execution of the full task graph ended on this worker thread.
		Buffer<Task> tasks{};  ///< Sub-statistics of each distinct task that was executed.
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	TimePoint startTime{};    ///< Time when execution of the full task graph started.
	TimePoint endTime{};      ///< Time when execution of the full task graph ended.
	Buffer<Worker> workers{}; ///< Sub-statistics of each distinct worker thread.
};

} // namespace grem::execution

#endif
