// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_TASK_HPP
#define GREM_EXECUTION_TASK_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>

#include <utility> // std::move

namespace grem::execution {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
/**
 * Statically scheduled unit of work to be executed by an Executor as part of a
 * task graph.
 *
 * \sa Scheduler
 */
class alignas(64) Task {
public:
	/** Index of a parallel sub-task. */
	using ParallelIndex = uint16_t;

	/** Number of a parallel sub-tasks. */
	using ParallelCount = ParallelIndex;

	/** Callback function pointer that executes a task. */
	using Function = void (*)(void* context, byte* taskSharedMemory, ParallelIndex parallelIndex, ParallelCount parallelism);

	/** Index of a task in a task graph. */
	using GraphIndex = ParallelIndex;

	/** Number of tasks in a task graph. */
	using Count = ParallelCount;

	/** Byte offset into the shared memory allocated for a task. */
	using SharedMemoryOffset = uint32_t;

	/** Size, in bytes, of shared memory allocated for a task. */
	using SharedMemorySize = uint32_t;

	/** List of indices of previous tasks that a task depends on. */
	using DependencyIndices = SmallArrayList<GraphIndex, 12>;

	/** The maximum supported number of tasks in a task graph. */
	static constexpr size_t MAX_GRAPH_SIZE = size_t{Limits<Count>::MAX};

	/**
	 * Construct a task.
	 *
	 * \param function callback function that executes the task.
	 * \param sharedMemoryOffset offset of the first byte allocated for this
	 *        task in the shared task memory.
	 * \param parallelIndex sub-task index of the task. Must be less than
	 *        `parallelism`.
	 * \param parallelism number of parallel sub-tasks that this task is part
	 *        of. Must be positive.
	 * \param dependencyIndices list of indices of previous tasks that this task
	 *        depends on.
	 * \param name owning pointer to a null-terminated UTF-8-encoded string
	 *        containing the human-readable name of the task, or nullptr to
	 *        create an unnamed task.
	 */
	Task(Function function, SharedMemoryOffset sharedMemoryOffset, ParallelIndex parallelIndex, ParallelCount parallelism, DependencyIndices dependencyIndices,
		UniquePointer<char[]> name) noexcept
		: function(function)
		, sharedMemoryOffset(sharedMemoryOffset)
		, parallelIndex(parallelIndex)
		, parallelism(parallelism)
		, dependencyIndices(std::move(dependencyIndices))
		, name(std::move(name)) {}

	/**
	 * Execute the task.
	 *
	 * \param context user-defined context pointer to pass to the callback
	 *        function's `void* context` parameter.
	 * \param sharedMemory non-owning pointer to the start of the memory
	 *        allocated for all tasks in the task graph. The callback function
	 *        will be passed this pointer plus the value of the task's
	 *        getSharedMemoryOffset().
	 *
	 * \throws any exception thrown by the task's callback function.
	 */
	void execute(void* context, byte* sharedMemory) const {
		byte* const taskSharedMemory = sharedMemory + sharedMemoryOffset;
		function(context, taskSharedMemory, parallelIndex, parallelism);
	}

	/**
	 * Get the expected offset of this task's reserved memory range in the total
	 * shared task memory allocated for the task graph.
	 *
	 * \return the shared memory offset of the task.
	 */
	[[nodiscard]] SharedMemoryOffset getSharedMemoryOffset() const noexcept {
		return sharedMemoryOffset;
	}

	/**
	 * Get the task indices of this task's dependencies in the task graph.
	 *
	 * \return the list of indices of previous tasks that this task depends on.
	 */
	[[nodiscard]] Span<const GraphIndex> getDependencyIndices() const noexcept {
		return dependencyIndices;
	}

	/**
	 * Get the human-readable name of the task.
	 *
	 * \return a UTF-8-encoded string containing the task name, or an empty
	 *         string if the task is unnamed.
	 */
	[[nodiscard]] CStringView getName() const noexcept {
		return (name) ? CStringView{name.get()} : CStringView{};
	}

private:
	Function function;
	SharedMemoryOffset sharedMemoryOffset;
	ParallelIndex parallelIndex;
	ParallelCount parallelism;
	DependencyIndices dependencyIndices;
	UniquePointer<char[]> name;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * Chunkable task whose chunks can be run in parallel.
 */
class ParallelTask {
public:
	/** Callback function pointer that executes a chunk of a parallel task. */
	using Function = void (*)(void* subTaskContext, byte* taskSharedMemory, Task::ParallelIndex subTaskIndex, Task::ParallelCount subTaskCount);

	/**
	 * Construct a parallel task.
	 *
	 * \param function callback function that executes a chunk of the parallel
	 *        task.
	 */
	ParallelTask(Function function) noexcept
		: function(function) {}

	/**
	 * Execute a chunk of the parallel task.
	 *
	 * \param subTaskContext user-defined context pointer to pass to the
	 *        callback function's `void* subTaskContext` parameter.
	 * \param taskSharedMemory non-owning pointer to the shared memory to
	 *        provide to the callback function's `byte* taskSharedMemory`
	 *        parameter.
	 * \param subTaskIndex index of the chunk to execute. Must be less than
	 *        `subTaskCount`.
	 * \param subTaskCount number of chunks that this invocation is part of.
	 *        Must be positive.
	 *
	 * \throws any exception thrown by the parallel task's callback function.
	 */
	void execute(void* subTaskContext, byte* taskSharedMemory, Task::ParallelIndex subTaskIndex, Task::ParallelCount subTaskCount) const {
		function(subTaskContext, taskSharedMemory, subTaskIndex, subTaskCount);
	}

private:
	Function function;
};

} // namespace grem::execution

#endif
