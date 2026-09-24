// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_SCHEDULE_HPP
#define GREM_EXECUTION_SCHEDULE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/execution/Task.hpp>

#include <utility> // std::move

namespace grem::execution {

namespace detail {

[[nodiscard]] GREM_API(execution) String formatScheduleDOTGraph(Span<const Task> tasks, CStringView name);

} // namespace detail

/**
 * Statically scheduled task graph built by a Scheduler.
 *
 * \tparam EntReg concrete entity registry type used by the scheduled tasks,
 *         e.g. some specialization of EntityRegistry or EntityTable.
 * \tparam ResReg concrete resource registry type used by the scheduled tasks,
 *         e.g. some specialization of ResourceRegistry or ResourceTable.
 *
 * \sa Scheduler
 */
template <typename EntReg, typename ResReg>
class Schedule {
public:
	/**
	 * Task context that the scheduled tasks expect to receive a pointer to
	 * through their `void* context` parameter.
	 */
	struct TaskContext {
		EntReg& entities;  ///< Reference to the entity registry provided to the scheduled tasks.
		ResReg& resources; ///< Reference to the resource registry provided to the scheduled tasks.
	};

	/**
	 * Construct an empty schedule with an empty name.
	 *
	 * \sa Scheduler
	 */
	Schedule() noexcept = default;

	/**
	 * Construct a schedule.
	 *
	 * \param tasks topologically ordered list of scheduled tasks. The tasks
	 *        must not depend on the results of tasks that appear later in the
	 *        list. Dependencies between tasks must be specified through
	 *        Task::getDependencyIndices(), all of which must be less than the
	 *        index of the dependent task. Each task function must not access
	 *        its `byte* taskSharedMemory` parameter outside of the offset
	 *        interval `[0, requiredSharedMemorySize - task.getSharedMemoryOffset())`.
	 * \param requiredSharedMemorySize total amount of contiguous shared memory
	 *        required by the `byte* sharedMemory` parameter of Task::execute().
	 *        Must be greater than or equal to `task.getSharedMemoryOffset()`
	 *        for all tasks.
	 * \param name UTF-8-encoded human-readable name of the schedule, or an
	 *        empty string for an unnamed schedule.
	 *
	 * \sa Scheduler
	 */
	Schedule(ArrayList<Task> tasks, Task::SharedMemorySize requiredSharedMemorySize, String name)
		: tasks(std::move(tasks))
		, requiredSharedMemorySize(requiredSharedMemorySize)
		, name(std::move(name)) {}

	/**
	 * Get the list of scheduled tasks.
	 *
	 * \return a topologically ordered view over the tasks defined by the
	 *         schedule.
	 */
	[[nodiscard]] Span<const Task> getTasks() const noexcept {
		return tasks;
	}

	/**
	 * Get the total amount of contiguous shared memory required by the
	 * `byte* sharedMemory` parameter of Task::execute().
	 *
	 * \return the required shared memory size, in bytes.
	 */
	[[nodiscard]] Task::SharedMemorySize getRequiredSharedMemorySize() const noexcept {
		return requiredSharedMemorySize;
	}

	/**
	 * Get the human-readable name of the schedule.
	 *
	 * \return a UTF-8-encoded string containing the human-readable name of the
	 *         schedule, or an empty string if the schedule is unnamed.
	 */
	[[nodiscard]] CStringView getName() const noexcept {
		return name;
	}

	/**
	 * Build a string representation of the directed acyclic graph formed by the
	 * schedule's tasks, and the dependencies between them, in Graphviz DOT
	 * format.
	 *
	 * \return a string in the DOT graph description language that can, for
	 *         example, be saved to a file `schedule.gv` and visualized using
	 *         the Graphviz command `dot -Tpng -oschedule.png schedule.gv`.
	 */
	[[nodiscard]] String formatDOTGraph() const {
		return detail::formatScheduleDOTGraph(tasks, name);
	}

private:
	ArrayList<Task> tasks{};
	Task::SharedMemorySize requiredSharedMemorySize = 0;
	String name{};
};

} // namespace grem::execution

#endif
