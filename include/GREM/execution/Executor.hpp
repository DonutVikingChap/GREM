// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_EXECUTOR_HPP
#define GREM_EXECUTION_EXECUTOR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Thread.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Task.hpp>

#include <iterator>    // std::data, std::size
#include <type_traits> // std::remove_pointer_t, std::decay_t
#include <utility>     // std::move, std::declval, std::...index_sequence

namespace grem::execution {

struct Statistics; // Forward declaration, to avoid including Statistics.hpp.

class Executor {
public:
	virtual ~Executor() = default;

	template <typename EntReg, typename ResReg>
	void executeSchedule(const Schedule<EntReg, ResReg>& schedule, EntReg& entities, ResReg& resources, Statistics* statistics = nullptr) {
		typename Schedule<EntReg, ResReg>::TaskContext taskContext{
			.entities = entities,
			.resources = resources,
		};
		executeTaskGraph(schedule.getTasks(), &taskContext, schedule.getRequiredSharedMemorySize(), statistics);
	}

	template <typename Range, typename UnaryProcedure>
	void executeOperation(Range&& range, UnaryProcedure operation) { // NOLINT(cppcoreguidelines-missing-std-forward)
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		for (T& element : elements) {
			operation(element);
		}
	}

	template <typename Range, typename UnaryProcedure>
	void executeParallelOperation(Range&& range, UnaryProcedure operation) { // NOLINT(cppcoreguidelines-missing-std-forward)
#ifdef GREM_USE_MULTITHREADING
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elements.size() < chunkCount) {
			[[unlikely]];
			return executeOperation(range, std::forward<UnaryProcedure>(operation));
		}

		struct ParallelTaskContext {
			UnaryProcedure operation;
			SmallArrayList<Span<T>, 32> chunks{};
		} parallelTaskContext{std::move(operation)};

		const size_t chunkSize = (elements.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elements.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elements.subspan(chunkOffset, min(chunkSize, elements.size() - chunkOffset)));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				for (T& element : parallelTaskContext.chunks[subTaskIndex]) {
					parallelTaskContext.operation(element);
				}
			},
			0, nullptr);
#else
		return executeOperation(range, std::forward<UnaryProcedure>(operation));
#endif
	}

	template <typename IndexRange, typename Range, typename UnaryProcedure>
	void executeSparseOperation(const IndexRange& indices, Range&& range, UnaryProcedure operation) { // NOLINT(cppcoreguidelines-missing-std-forward)
		using T = std::remove_pointer_t<decltype(std::data(range))>;
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const Span<T> elements{std::data(range), std::size(range)};
		for (const Index index : elementIndices) {
			operation(elements[index]);
		}
	}

	template <typename IndexRange, typename Range, typename UnaryProcedure>
	void executeParallelSparseOperation(const IndexRange& indices, Range&& range, UnaryProcedure operation) { // NOLINT(cppcoreguidelines-missing-std-forward)
#ifdef GREM_USE_MULTITHREADING
		using T = std::remove_pointer_t<decltype(std::data(range))>;
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const Span<T> elements{std::data(range), std::size(range)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elementIndices.size() < chunkCount) {
			[[unlikely]];
			return executeSparseOperation(indices, range, std::forward<UnaryProcedure>(operation));
		}

		struct ParallelTaskContext {
			UnaryProcedure operation;
			T* pointer;
			SmallArrayList<Span<const Index>, 32> chunks{};
		} parallelTaskContext{std::move(operation), elements.data()};

		const size_t chunkSize = (elementIndices.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elementIndices.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elementIndices.subspan(chunkOffset, min(chunkSize, elementIndices.size() - chunkOffset)));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				for (const Index index : parallelTaskContext.chunks[subTaskIndex]) {
					parallelTaskContext.operation(parallelTaskContext.pointer[index]);
				}
			},
			0, nullptr);
#else
		return executeSparseOperation(indices, range, std::forward<UnaryProcedure>(operation));
#endif
	}

	template <typename IndexedProcedure>
	void executeIndexedOperation(size_t begin, size_t end, IndexedProcedure operation) {
		GREM_ASSERT(begin <= end);
		for (size_t i = begin; i < end; ++i) {
			operation(size_t{i});
		}
	}

	template <typename IndexedProcedure>
	void executeParallelIndexedOperation(size_t begin, size_t end, IndexedProcedure operation) {
#ifdef GREM_USE_MULTITHREADING
		GREM_ASSERT(begin <= end);
		const size_t size = end - begin;
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || size < chunkCount) {
			[[unlikely]];
			return executeIndexedOperation(begin, end, std::forward<IndexedProcedure>(operation));
		}

		struct ParallelTaskContext {
			IndexedProcedure operation;
			SmallArrayList<Pair<size_t>, 32> chunks{};
		} parallelTaskContext{std::move(operation)};

		const size_t chunkSize = (size + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = begin; chunkOffset < end; chunkOffset += chunkSize) {
			parallelTaskContext.chunks.emplace_back(chunkOffset, min(chunkSize, end - chunkOffset));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				const Pair<size_t> chunk = parallelTaskContext.chunks[subTaskIndex];
				const size_t end = chunk.first + chunk.second;
				for (size_t i = chunk.first; i < end; ++i) {
					parallelTaskContext.operation(size_t{i});
				}
			},
			0, nullptr);
#else
		return executeIndexedOperation(begin, end, std::forward<IndexedProcedure>(operation));
#endif
	}

	template <typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeApplication(Procedure operation, FirstRange&& range, OtherRanges&&... ranges) { // NOLINT(cppcoreguidelines-missing-std-forward)
		const size_t size = std::size(range);
		for (size_t i = 0; i < size; ++i) {
			operation(range[i], ranges[i]...);
		}
	}

	template <typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeParallelApplication(Procedure operation, FirstRange&& range, OtherRanges&&... ranges) {
#ifdef GREM_USE_MULTITHREADING
		const size_t size = std::size(range);
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || size < chunkCount) {
			[[unlikely]];
			return executeApplication(std::forward<Procedure>(operation), std::forward<FirstRange>(range), std::forward<OtherRanges>(ranges)...);
		}

		struct ParallelTaskContext {
			Procedure operation;
			Tuple<decltype(std::data(range)), decltype(std::data(ranges))...> pointers;
			SmallArrayList<Pair<size_t>, 32> chunks{};
		} parallelTaskContext{std::move(operation), Tuple{std::data(range), std::data(ranges)...}};

		const size_t chunkSize = (size + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < size; chunkOffset += chunkSize) {
			parallelTaskContext.chunks.emplace_back(chunkOffset, min(chunkSize, size - chunkOffset));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				const Pair<size_t> chunk = parallelTaskContext.chunks[subTaskIndex];
				const size_t end = chunk.first + chunk.second;
				for (size_t i = chunk.first; i < end; ++i) {
					[&]<size_t... Indices>(std::index_sequence<Indices...>) -> void {
						parallelTaskContext.operation(*(get<Indices>(parallelTaskContext.pointers) + i)...);
					}(std::make_index_sequence<1 + sizeof...(OtherRanges)>{});
				}
			},
			0, nullptr);
#else
		return executeApplication(std::forward<Procedure>(operation), std::forward<FirstRange>(range), std::forward<OtherRanges>(ranges)...);
#endif
	}

	template <typename IndexRange, typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeSparseApplication(const IndexRange& indices, Procedure operation, //
		FirstRange&& range, OtherRanges&&... ranges) {                            // NOLINT(cppcoreguidelines-missing-std-forward)
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		for (const Index index : elementIndices) {
			operation(range[index], ranges[index]...);
		}
	}

	template <typename IndexRange, typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeParallelSparseApplication(const IndexRange& indices, Procedure operation, FirstRange&& range, OtherRanges&&... ranges) {
#ifdef GREM_USE_MULTITHREADING
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elementIndices.size() < chunkCount) {
			[[unlikely]];
			return executeSparseApplication(indices, std::forward<Procedure>(operation), std::forward<FirstRange>(range), std::forward<OtherRanges>(ranges)...);
		}

		struct ParallelTaskContext {
			Procedure operation;
			Tuple<decltype(std::data(range)), decltype(std::data(ranges))...> pointers;
			SmallArrayList<Span<const Index>, 32> chunks{};
		} parallelTaskContext{std::move(operation), Tuple{std::data(range), std::data(ranges)...}};

		const size_t chunkSize = (elementIndices.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elementIndices.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elementIndices.subspan(chunkOffset, min(chunkSize, elementIndices.size() - chunkOffset)));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				for (const Index index : parallelTaskContext.chunks[subTaskIndex]) {
					[&]<size_t... Indices>(std::index_sequence<Indices...>) -> void {
						parallelTaskContext.operation(*(get<Indices>(parallelTaskContext.pointers) + index)...);
					}(std::make_index_sequence<1 + sizeof...(OtherRanges)>{});
				}
			},
			0, nullptr);
#else
		return executeSparseApplication(indices, std::forward<Procedure>(operation), std::forward<FirstRange>(range), std::forward<OtherRanges>(ranges)...);
#endif
	}

	template <typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeTransformation(const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		for (size_t i = 0; i < elements.size(); ++i) {
			output[i] = transformationFunction(elements[i]);
		}
	}

	template <typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeParallelTransformation(const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
#ifdef GREM_USE_MULTITHREADING
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elements.size() < chunkCount) {
			[[unlikely]];
			return executeTransformation(range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
		}

		struct ParallelTaskContext {
			UnaryMapping transformationFunction;
			RandomAccessIterator output;
			SmallArrayList<Span<T>, 32> chunks{};
		} parallelTaskContext{std::move(transformationFunction), std::move(output)};

		const size_t chunkSize = (elements.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elements.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elements.subspan(chunkOffset, min(chunkSize, elements.size() - chunkOffset)));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				const Span<T> chunk = parallelTaskContext.chunks[subTaskIndex];
				for (size_t i = 0; i < chunk.size(); ++i) {
					parallelTaskContext.output[i] = parallelTaskContext.transformationFunction(chunk[i]);
				}
			},
			0, nullptr);
#else
		return executeTransformation(range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
#endif
	}

	template <typename IndexRange, typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeSparseTransformation(const IndexRange& indices, const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
		using T = std::remove_pointer_t<decltype(std::data(range))>;
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const Span<T> elements{std::data(range), std::size(range)};
		for (size_t i = 0; i < elementIndices.size(); ++i) {
			output[i] = transformationFunction(elements[elementIndices[i]]);
		}
	}

	template <typename IndexRange, typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeParallelSparseTransformation(const IndexRange& indices, const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
#ifdef GREM_USE_MULTITHREADING
		using T = std::remove_pointer_t<decltype(std::data(range))>;
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const Span<T> elements{std::data(range), std::size(range)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elementIndices.size() < chunkCount) {
			[[unlikely]];
			return executeSparseTransformation(indices, range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
		}

		struct ParallelTaskContext {
			UnaryMapping transformationFunction;
			T* pointer;
			RandomAccessIterator output;
			SmallArrayList<Span<const Index>, 32> chunks{};
		} parallelTaskContext{std::move(transformationFunction), elements.data(), std::move(output)};

		const size_t chunkSize = (elementIndices.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elementIndices.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elementIndices.subspan(chunkOffset, min(chunkSize, elementIndices.size() - chunkOffset)));
		}

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				const Span<const Index> chunk = parallelTaskContext.chunks[subTaskIndex];
				for (size_t i = 0; i < chunk.size(); ++i) {
					parallelTaskContext.output[i] = parallelTaskContext.transformationFunction(parallelTaskContext.pointer[chunk[i]]);
				}
			},
			0, nullptr);
#else
		return executeSparseTransformation(indices, range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
#endif
	}

	template <typename Range, typename InitialValue, typename AssociativeBinaryFunction>
	[[nodiscard]] InitialValue executeReduction(const Range& range, const InitialValue& initialValue, AssociativeBinaryFunction reductionFunction) {
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		InitialValue result = initialValue;
		for (T& element : elements) {
			result = reductionFunction(std::move(result), element);
		}
		return result;
	}

	template <typename Range, typename InitialValue, typename AssociativeBinaryFunction>
	[[nodiscard]] InitialValue executeParallelReduction(const Range& range, const InitialValue& initialValue, AssociativeBinaryFunction reductionFunction) {
#ifdef GREM_USE_MULTITHREADING
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		const size_t chunkCount = getMaxParallelism();
		if (chunkCount <= 1 || elements.size() < chunkCount) {
			[[unlikely]];
			return executeReduction(range, initialValue, std::forward<AssociativeBinaryFunction>(reductionFunction));
		}

		struct alignas(64) IntermediateResult {
			InitialValue value;
		};

		struct ParallelTaskContext {
			AssociativeBinaryFunction reductionFunction;
			SmallArrayList<Span<T>, 32> chunks{};
			SmallArrayList<IntermediateResult, 32> intermediateResults{};
		} parallelTaskContext{std::move(reductionFunction)};

		const size_t chunkSize = (elements.size() + chunkCount - 1) / chunkCount;
		for (size_t chunkOffset = 0; chunkOffset < elements.size(); chunkOffset += chunkSize) {
			parallelTaskContext.chunks.push_back(elements.subspan(chunkOffset, min(chunkSize, elements.size() - chunkOffset)));
		}

		if (parallelTaskContext.chunks.empty()) {
			return initialValue;
		}
		parallelTaskContext.intermediateResults.resize(parallelTaskContext.chunks.size(), IntermediateResult{initialValue});

		executeParallelTasks(
			&parallelTaskContext, static_cast<Task::ParallelCount>(parallelTaskContext.chunks.size()),
			+[](void* subTaskContext, byte*, Task::ParallelIndex subTaskIndex, Task::ParallelCount) -> void {
				ParallelTaskContext& parallelTaskContext = *static_cast<ParallelTaskContext*>(subTaskContext);
				InitialValue& intermediateResult = parallelTaskContext.intermediateResults[subTaskIndex].value;
				InitialValue localResult = std::move(intermediateResult);
				for (T& element : parallelTaskContext.chunks[subTaskIndex]) {
					localResult = parallelTaskContext.reductionFunction(std::move(localResult), element);
				}
				intermediateResult = std::move(localResult);
			},
			0, nullptr);

		InitialValue result = std::move(parallelTaskContext.intermediateResults.front().value);
		for (size_t i = 1; i < parallelTaskContext.chunks.size(); ++i) {
			result = parallelTaskContext.reductionFunction(std::move(result), std::move(parallelTaskContext.intermediateResults[i].value));
		}
		return result;
#else
		return executeReduction(range, initialValue, std::forward<AssociativeBinaryFunction>(reductionFunction));
#endif
	}

	virtual void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, Statistics* statistics) = 0;
	virtual void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize requiredSharedMemorySize,
		Statistics* statistics) = 0;
	[[nodiscard]] virtual Task::ParallelCount getMaxParallelism() const noexcept = 0;

protected:
	GREM_API(execution)
	static void executeTaskGraphSequentially(Span<const Task> tasks, void* context, byte* sharedMemory, Statistics* statistics);

	GREM_API(execution)
	static void executeParallelTasksSequentially(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, byte* sharedMemory, Statistics* statistics);
};

class SequentialExecutor final : public Executor {
public:
	void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, Statistics* statistics) override {
		if (requiredSharedMemorySize > sharedMemory.size()) {
			sharedMemory.resize(static_cast<size_t>(requiredSharedMemorySize));
		}
		executeTaskGraphSequentially(tasks, context, sharedMemory.data(), statistics);
	}

	void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize requiredSharedMemorySize,
		Statistics* statistics) override {
		if (requiredSharedMemorySize > sharedMemory.size()) {
			sharedMemory.resize(static_cast<size_t>(requiredSharedMemorySize));
		}
		executeParallelTasksSequentially(subTaskContext, subTaskCount, subTask, sharedMemory.data(), statistics);
	}

	[[nodiscard]] Task::ParallelCount getMaxParallelism() const noexcept override {
		return 1;
	}

	Allocation<byte> sharedMemory{};
};

struct DynamicExecutorOptions {
	Task::ParallelCount targetParallelism = static_cast<Task::ParallelCount>(clamp(Thread::hardware_concurrency(), 2u, 32u) - 1);
};

class DynamicExecutor final : public Executor {
public:
	GREM_API(execution) explicit DynamicExecutor(const DynamicExecutorOptions& options);

	explicit DynamicExecutor(UniquePointer<Executor> implementation)
		: implementation(std::move(implementation)) {
		GREM_ASSERT(this->implementation);
	}

	void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, Statistics* statistics) override {
		GREM_ASSERT(implementation);
		implementation->executeTaskGraph(tasks, context, requiredSharedMemorySize, statistics);
	};

	void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize requiredSharedMemorySize,
		Statistics* statistics) override {
		GREM_ASSERT(implementation);
		implementation->executeParallelTasks(subTaskContext, subTaskCount, subTask, requiredSharedMemorySize, statistics);
	}

	[[nodiscard]] Task::ParallelCount getMaxParallelism() const noexcept override {
		return (implementation) ? implementation->getMaxParallelism() : 0;
	}

	[[nodiscard]] Executor* get() noexcept {
		return implementation.get();
	}

	[[nodiscard]] const Executor* get() const noexcept {
		return implementation.get();
	}

private:
	UniquePointer<Executor> implementation{};
};

} // namespace grem::execution

#endif
