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
class alignas(64) Task {
public:
	using ParallelIndex = uint16_t;
	using ParallelCount = ParallelIndex;
	using Function = void (*)(void* context, byte* sharedMemory, ParallelIndex parallelIndex, ParallelCount parallelism);
	using GraphIndex = ParallelIndex;
	using Count = ParallelCount;
	using SharedMemoryOffset = uint32_t;
	using SharedMemorySize = uint32_t;
	using DependencyIndices = SmallArrayList<GraphIndex, 12>;

	static constexpr size_t MAX_GRAPH_SIZE = size_t{Limits<Count>::MAX};

	Task(Function function, SharedMemoryOffset sharedMemoryOffset, ParallelIndex parallelIndex, ParallelCount parallelism, DependencyIndices dependencyIndices,
		UniquePointer<char[]> name) noexcept
		: function(function)
		, sharedMemoryOffset(sharedMemoryOffset)
		, parallelIndex(parallelIndex)
		, parallelism(parallelism)
		, dependencyIndices(std::move(dependencyIndices))
		, name(std::move(name)) {}

	void execute(void* context, byte* sharedMemory) const {
		function(context, sharedMemory + sharedMemoryOffset, parallelIndex, parallelism);
	}

	[[nodiscard]] SharedMemoryOffset getSharedMemoryOffset() const noexcept {
		return sharedMemoryOffset;
	}

	[[nodiscard]] Span<const GraphIndex> getDependencyIndices() const noexcept {
		return dependencyIndices;
	}

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

class ParallelTask {
public:
	using Function = void (*)(void* subTaskContext, byte* sharedMemory, Task::ParallelIndex subTaskIndex, Task::ParallelCount subTaskCount);

	ParallelTask() noexcept = default;

	ParallelTask(Function function) noexcept
		: function(function) {}

	void execute(void* subTaskContext, byte* sharedMemory, Task::ParallelIndex subTaskIndex, Task::ParallelCount subTaskCount) const {
		function(subTaskContext, sharedMemory, subTaskIndex, subTaskCount);
	}

private:
	Function function;
};

} // namespace grem::execution

#endif
