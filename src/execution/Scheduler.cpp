// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Scheduler.hpp>
#include <GREM/execution/Task.hpp>

#include <iterator>  // std::prev, std::next
#include <stdexcept> // std::length_error
#include <typeindex> // std::type_index

namespace grem::execution {

namespace detail {

namespace {

class DirectedGraphEdges {
public:
	explicit DirectedGraphEdges(size_t nodeCount)
		: adjacencyMatrix(nodeCount * nodeCount, false)
		, nodeCount(nodeCount) {}

	void insertEdge(size_t fromNodeIndex, size_t toNodeIndex) {
		adjacencyMatrix[fromNodeIndex * nodeCount + toNodeIndex] = true;
	}

	void removeEdge(size_t fromNodeIndex, size_t toNodeIndex) {
		adjacencyMatrix[fromNodeIndex * nodeCount + toNodeIndex] = false;
	}

	[[nodiscard]] bool hasEdge(size_t fromNodeIndex, size_t toNodeIndex) const {
		return adjacencyMatrix[fromNodeIndex * nodeCount + toNodeIndex];
	}

	[[nodiscard]] bool hasEdgeToEarlierNode(size_t nodeIndex) const {
		for (size_t i = 0; i <= nodeIndex; ++i) {
			if (hasEdge(nodeIndex, i)) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] bool isTopLevelNode(size_t nodeIndex) const {
		size_t i = nodeIndex;
		do {
			if (adjacencyMatrix[i]) {
				return false;
			}
			i += nodeCount;
		} while (i < adjacencyMatrix.size());
		return true;
	}

	void transitiveClosure() {
		for (size_t x = 0; x < nodeCount; ++x) {
			for (size_t y = 0; y < nodeCount; ++y) {
				for (size_t z = 0; z < nodeCount; ++z) {
					if (hasEdge(y, x) && hasEdge(x, z)) {
						insertEdge(y, z);
					}
				}
			}
		}
	}

	void transitiveReduction() {
		for (size_t i = 0; i < nodeCount; ++i) {
			removeEdge(i, i);
		}
		for (size_t z = 0; z < nodeCount; ++z) {
			for (size_t y = 0; y < nodeCount; ++y) {
				if (hasEdge(y, z)) {
					for (size_t x = 0; x < nodeCount; ++x) {
						if (hasEdge(z, x)) {
							removeEdge(y, x);
						}
					}
				}
			}
		}
	}

private:
	Allocation<bool> adjacencyMatrix;
	size_t nodeCount;
};

[[nodiscard]] DirectedGraphEdges buildDependencyGraph(size_t taskCount, const HashMap<std::type_index, Buffer<Accessor>>& componentAccesses,
	const HashMap<std::type_index, Buffer<Accessor>>& resourceAccesses, Span<const Accessor> entityRegistryAccesses, Span<const Accessor> resourceRegistryAccesses) {
	DirectedGraphEdges dependencies{taskCount};
	const auto insertDependencies = [&](Span<const Accessor> accesses) -> void {
		auto previousMutableAccessesEnd = accesses.begin();
		while (true) {
			// Find the next mutable access.
			const auto mutableAccess = findIf(Subrange{previousMutableAccessesEnd, accesses.end()}, [](const Accessor& accessor) -> bool { return accessor.hasMutableAccess(); });

			// Each access between the previous and next mutable accesses depends on the previous mutable access (so that the results of the mutation are observed).
			if (previousMutableAccessesEnd != accesses.begin()) {
				const auto previousMutableAccess = std::prev(previousMutableAccessesEnd);
				for (auto intermediateAccess = previousMutableAccessesEnd; intermediateAccess != mutableAccess; ++intermediateAccess) {
					dependencies.insertEdge(previousMutableAccess->taskIndex, intermediateAccess->taskIndex);
				}
			}

			if (mutableAccess == accesses.end()) {
				break; // There are no more mutable accesses.
			}

			// This mutable access depends on the previous mutable access (so that the mutations are properly sequenced).
			if (previousMutableAccessesEnd != accesses.begin()) {
				const auto previousMutableAccess = std::prev(previousMutableAccessesEnd);
				dependencies.insertEdge(previousMutableAccess->taskIndex, mutableAccess->taskIndex);
			}

			// This mutable access depends on each intermediate access between it and the previous mutable access (so that the intermediate accesses' memory doesn't get stomped).
			for (auto intermediateAccess = previousMutableAccessesEnd; intermediateAccess != mutableAccess; ++intermediateAccess) {
				dependencies.insertEdge(intermediateAccess->taskIndex, mutableAccess->taskIndex);
			}

			previousMutableAccessesEnd = std::next(mutableAccess);
		}
	};

	for (const auto& [typeIndex, accesses] : componentAccesses) {
		insertDependencies(accesses);
	}
	for (const auto& [typeIndex, accesses] : resourceAccesses) {
		insertDependencies(accesses);
	}
	insertDependencies(entityRegistryAccesses);
	insertDependencies(resourceRegistryAccesses);

	dependencies.transitiveClosure();
	dependencies.transitiveReduction();
	return dependencies;
}

} // namespace

ArrayList<Task> buildSchedule(Span<const UnscheduledTask> tasks, // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	const HashMap<std::type_index, Buffer<Accessor>>& componentAccesses, const HashMap<std::type_index, Buffer<Accessor>>& resourceAccesses,
	Span<const Accessor> entityRegistryAccesses, Span<const Accessor> resourceRegistryAccesses) {
	GREM_ASSERT(tasks.size() <= Task::MAX_GRAPH_SIZE);
	const Task::GraphIndex taskCount = static_cast<Task::GraphIndex>(tasks.size());
	const DirectedGraphEdges dependencies = buildDependencyGraph(taskCount, componentAccesses, resourceAccesses, entityRegistryAccesses, resourceRegistryAccesses);

	struct Node {
		ArrayList<Task::GraphIndex> dependencyIndices{};
		Task::Count level = 0;
	};

	// Build task graph.
	Allocation<Node> graph(tasks.size());
	for (Task::GraphIndex taskIndex = 0; taskIndex < taskCount; ++taskIndex) {
		GREM_ASSERT(!dependencies.hasEdgeToEarlierNode(taskIndex));

		Node& node = graph[taskIndex];
		const Task::Count level = node.level;

		for (Task::GraphIndex dependentTaskIndex = taskIndex + 1; dependentTaskIndex < taskCount; ++dependentTaskIndex) {
			if (dependencies.hasEdge(taskIndex, dependentTaskIndex)) {
				Node& dependentNode = graph[dependentTaskIndex];
				dependentNode.level = max(dependentNode.level, static_cast<Task::Count>(level + 1));
				dependentNode.dependencyIndices.push_back(taskIndex);
			}
		}
	}

	// Sort task graph.
	Allocation<Task::GraphIndex> taskIndicesSortedByLevel(taskCount);
	iota(taskIndicesSortedByLevel, Task::GraphIndex{0});
	stableSort(taskIndicesSortedByLevel, [&](Task::GraphIndex taskIndexA, Task::GraphIndex taskIndexB) -> bool { return graph[taskIndexA].level < graph[taskIndexB].level; });

	Allocation<Pair<Task::GraphIndex>> newTaskIndexRanges(taskCount);
	Task::GraphIndex newTaskOffset = 0;
	for (Task::GraphIndex newTaskIndex = 0; newTaskIndex < taskCount; ++newTaskIndex) {
		const Task::GraphIndex taskIndex = taskIndicesSortedByLevel[newTaskIndex];
		const size_t parallelism{tasks[taskIndex].parallelism};
		if (static_cast<size_t>(Limits<Task::GraphIndex>::MAX - newTaskOffset) < parallelism) {
			throw std::length_error{"Maximum task count exceeded when expanding parallel tasks."};
		}
		const Task::GraphIndex newTaskIndicesEnd = static_cast<Task::GraphIndex>(static_cast<size_t>(newTaskOffset) + parallelism);
		newTaskIndexRanges[taskIndex] = {newTaskOffset, newTaskIndicesEnd};
		newTaskOffset = newTaskIndicesEnd;
	}

	ArrayList<Task> result{};
	result.reserve(newTaskOffset);
	for (Task::GraphIndex newTaskIndex = 0; newTaskIndex < taskCount; ++newTaskIndex) {
		const Task::GraphIndex taskIndex = taskIndicesSortedByLevel[newTaskIndex];
		const UnscheduledTask& task = tasks[taskIndex];
		const Node& node = graph[taskIndex];
		const auto [newTaskIndicesBegin, newTaskIndicesEnd] = newTaskIndexRanges[taskIndex];
		GREM_ASSERT(static_cast<size_t>(newTaskIndicesEnd - newTaskIndicesBegin) == size_t{task.parallelism});

		Task::DependencyIndices newDependencyIndices{};
		for (const Task::GraphIndex dependencyIndex : graph[taskIndex].dependencyIndices) {
			const auto [newDependentTaskNodeIndicesBegin, newDependentTaskNodeIndicesEnd] = newTaskIndexRanges[dependencyIndex];
			for (Task::GraphIndex newDependencyIndex = newDependentTaskNodeIndicesBegin; newDependencyIndex < newDependentTaskNodeIndicesEnd; ++newDependencyIndex) {
				GREM_ASSERT(newDependencyIndex < newTaskIndicesBegin);
				newDependencyIndices.push_back(newDependencyIndex);
			}
		}

		Task::ParallelIndex parallelIndex = 0;
		for (Task::GraphIndex newTaskIndex = newTaskIndicesBegin; newTaskIndex < newTaskIndicesEnd; ++newTaskIndex) {
			UniquePointer<char[]> name{};
			if (!task.name.empty()) {
				if (task.parallelism > 1) {
					const auto taskName = formatSmallString<64>("{} (chunk {}/{})", task.name, parallelIndex + 1, task.parallelism);
					name = UniquePointer<char[]>::create(taskName.size() + 1);
					memcpy(name.get(), taskName.data(), taskName.size());
					name[taskName.size()] = '\0';
				} else {
					name = UniquePointer<char[]>::create(task.name.size() + 1);
					memcpy(name.get(), task.name.c_str(), task.name.size() + 1);
				}
			}
			result.emplace_back(task.function, task.sharedMemoryOffset, parallelIndex, task.parallelism, newDependencyIndices, std::move(name));
			++parallelIndex;
		}
	}
	return result;
}

} // namespace detail

} // namespace grem::execution
