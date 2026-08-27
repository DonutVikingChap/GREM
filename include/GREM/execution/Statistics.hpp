// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_STATISTICS_HPP
#define GREM_EXECUTION_STATISTICS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>

namespace grem::execution {

struct Statistics {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
	struct alignas(64) Worker {
		struct Task {
			size_t taskIndex;
			TimePoint startTime;
			TimePoint endTime;
		};

		TimePoint startTime{};
		TimePoint endTime{};
		Buffer<Task> tasks{};
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	TimePoint startTime{};
	TimePoint endTime{};
	Buffer<Worker> workers{};
};

} // namespace grem::execution

#endif
