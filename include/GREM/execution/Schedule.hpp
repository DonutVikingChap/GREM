// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_SCHEDULE_HPP
#define GREM_EXECUTION_SCHEDULE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/execution/Task.hpp>

namespace grem::execution {

namespace detail {

[[nodiscard]] GREM_API(execution) String getScheduleDOTGraph(Span<const Task> tasks);

} // namespace detail

template <typename EntReg, typename ResReg>
class Scheduler; // Forward declaration, to avoid a circular include of Scheduler.hpp.

template <typename EntReg, typename ResReg>
class Schedule {
public:
	struct TaskContext {
		EntReg& entities;
		ResReg& resources;
	};

	[[nodiscard]] Span<const Task> getTasks() const noexcept {
		return tasks;
	}

	[[nodiscard]] Task::SharedMemorySize getRequiredSharedMemorySize() const noexcept {
		return requiredSharedMemorySize;
	}

	[[nodiscard]] String getDOTGraph() const {
		return detail::getScheduleDOTGraph(tasks);
	}

private:
	friend Scheduler<EntReg, ResReg>;

	ArrayList<Task> tasks{};
	Task::SharedMemorySize requiredSharedMemorySize = 0;
};

} // namespace grem::execution

#endif
