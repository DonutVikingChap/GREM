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

/**
 * Interface to a pool of execution resources (e.g.\ threads) for executing
 * ad-hoc parallel tasks and pre-built task graphs defined through a Scheduler.
 */
class Executor {
public:
	/** Virtual destructor. */
	virtual ~Executor() = default;

	/**
	 * Execute a pre-built task Schedule using this executor.
	 *
	 * \param schedule schedule of tasks to execute.
	 * \param entities registry containing the entity ranges and component pools to
	 *        provide to tasks in the schedule.
	 * \param resources registry containing the resources to provide to tasks in
	 *        the schedule.
	 * \param statistics pointer to a set of statistics to fill out with the
	 *        statistics resulting from executing the task graph, or nullptr to
	 *        ignore.
	 *
	 * \throws execution::Error with a std::nested_exception of any exception
	 *         thrown by the scheduled tasks. If multiple parallel tasks throw
	 *         an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename EntReg, typename ResReg>
	void executeSchedule(const Schedule<EntReg, ResReg>& schedule, EntReg& entities, ResReg& resources, Statistics* statistics = nullptr) {
		typename Schedule<EntReg, ResReg>::TaskContext taskContext{
			.entities = entities,
			.resources = resources,
		};
		executeTaskGraph(schedule.getTasks(), &taskContext, schedule.getRequiredSharedMemorySize(), schedule.getName(), statistics);
	}

	/**
	 * Execute an operation on each element in a given range, sequentially.
	 *
	 * \param range contiguous, sized random-access range of elements to execute
	 *        the operation on.
	 * \param operation function object that takes a reference to each element
	 *        and performs some in-place operation on it, returning void.
	 *
	 * \throws any exception thrown by the operation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelOperation() to running sequentially,
	 *       and vice versa. There is no other direct benefit compared to a
	 *       basic for-loop or forEach().
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename Range, typename UnaryProcedure>
	void executeOperation(Range&& range, UnaryProcedure operation) { // NOLINT(cppcoreguidelines-missing-std-forward)
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		for (T& element : elements) {
			operation(element);
		}
	}

	/**
	 * Execute an operation on each element in a given range, in up to
	 * `getMaxParallelism()` parallel chunks using this executor.
	 *
	 * \param range contiguous, sized random-access range of elements to execute
	 *        the operation on.
	 * \param operation function object that takes a reference to each element
	 *        and performs some in-place operation on it, returning void.
	 *
	 * \throws any exception thrown by the operation. If multiple parallel tasks
	 *         throw an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute an operation on each indexed element specified by a given range
	 * of indices, sequentially.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        elements in `range` to execute the operation on. Each value must
	 *        be implicitly convertible to a size_t that is less than
	 *        `std::size(range)`.
	 * \param range contiguous, sized random-access range of elements, indexed
	 *        by `indices`, to execute the operation on. May contain unused
	 *        elements, which are ignored.
	 * \param operation function object that takes a reference to each indexed
	 *        element and performs some in-place operation on it, returning
	 *        void.
	 *
	 * \throws any exception thrown by the operation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelSparseOperation() to running
	 *       sequentially, and vice versa. There is no other direct benefit
	 *       compared to a basic for-loop.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute an operation on each indexed element specified by a given range
	 * of indices, in up to `getMaxParallelism()` parallel chunks using this
	 * executor.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        elements in `range` to execute the operation on. Each value must
	 *        be implicitly convertible to a size_t that is less than
	 *        `std::size(range)`.
	 * \param range contiguous, sized random-access range of elements, indexed
	 *        by `indices`, to execute the operation on. May contain unused
	 *        elements, which are ignored.
	 * \param operation function object that takes a reference to each indexed
	 *        element and performs some in-place operation on it, returning
	 *        void.
	 *
	 * \throws any exception thrown by the operation. If multiple parallel tasks
	 *         throw an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute an operation for each index in a given interval, sequentially.
	 *
	 * \param begin first index in the interval. Must be less than or equal to
	 *        `end`.
	 * \param end one past the last index in the interval. Must be greater than
	 *        or equal to `begin`.
	 * \param operation function object that takes each integer in the interval
	 *        `[begin, end)` as a size_t and performs some operation using it,
	 *        returning void.
	 *
	 * \throws any exception thrown by the operation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelIndexedOperation() to running
	 *       sequentially, and vice versa. There is no other direct benefit
	 *       compared to a basic for-loop.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename IndexedProcedure>
	void executeIndexedOperation(size_t begin, size_t end, IndexedProcedure operation) {
		GREM_ASSERT(begin <= end);
		for (size_t i = begin; i < end; ++i) {
			operation(size_t{i});
		}
	}

	/**
	 * Execute an operation for each index in a given interval, in up to
	 * `getMaxParallelism()` parallel chunks using this executor.
	 *
	 * \param begin first index in the interval. Must be less than or equal to
	 *        `end`.
	 * \param end one past the last index in the interval. Must be greater than
	 *        or equal to `begin`.
	 * \param operation function object that takes each integer in the interval
	 *        `[begin, end)` as a size_t and performs some operation using it,
	 *        returning void.
	 *
	 * \throws any exception thrown by the operation. If multiple parallel tasks
	 *         throw an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute an operation for each row of elements in a given set of equally
	 * sized column ranges, sequentially.
	 *
	 * \param operation function object that takes references to the elements in
	 *        each row of the given ranges and performs some in-place operation
	 *        on them, returning void.
	 * \param range first contiguous, sized random-access range in the set of
	 *        column ranges.
	 * \param ranges other contiguous, sized random-access ranges in the set of
	 *        column ranges. Each must have the same size as the first range.
	 *
	 * \throws any exception thrown by the operation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelApplication() to running sequentially,
	 *       and vice versa. There is no other direct benefit compared to a
	 *       basic for-loop.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeApplication(Procedure operation, FirstRange&& range, OtherRanges&&... ranges) { // NOLINT(cppcoreguidelines-missing-std-forward)
		const size_t size = std::size(range);
		Tuple pointers{std::data(range), std::data(ranges)...};
		for (size_t i = 0; i < size; ++i) {
			[&]<size_t... Indices>(std::index_sequence<Indices...>) -> void {
				operation(*(get<Indices>(pointers) + i)...);
			}(std::make_index_sequence<1 + sizeof...(OtherRanges)>{});
		}
	}

	/**
	 * Execute an operation for each row of elements in a given set of equally
	 * sized column ranges, in up to `getMaxParallelism()` parallel chunks using
	 * this executor.
	 *
	 * \param operation function object that takes references to the elements in
	 *        each row of the given ranges and performs some in-place operation
	 *        on them, returning void.
	 * \param range first contiguous, sized random-access range in the set of
	 *        column ranges.
	 * \param ranges other contiguous, sized random-access ranges in the set of
	 *        column ranges. Each must have the same size as the first range.
	 *
	 * \throws any exception thrown by the operation. If multiple parallel tasks
	 *         throw an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute an operation for each indexed row of elements specified by a
	 * given range of indices in a given set of equally sized column ranges,
	 * sequentially.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        rows in the given column ranges to execute the operation on. Each
	 *        value must be implicitly convertible to a size_t that is less than
	 *        the size of each of the column ranges.
	 * \param operation function object that takes references to the elements in
	 *        each row of the given ranges and performs some in-place operation
	 *        on them, returning void.
	 * \param range first contiguous, sized random-access range in the set of
	 *        column ranges, indexed by `indices`. May contain unused elements,
	 *        which are ignored.
	 * \param ranges other contiguous, sized random-access ranges in the set of
	 *        column ranges, indexed by `indices`. May contain unused elements,
	 *        which are ignored. Each must have the same size as the first range.
	 *
	 * \throws any exception thrown by the operation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelSparseApplication() to running
	 *       sequentially, and vice versa. There is no other direct benefit
	 *       compared to a basic for-loop.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename IndexRange, typename Procedure, typename FirstRange, typename... OtherRanges>
	void executeSparseApplication(const IndexRange& indices, Procedure operation, //
		FirstRange&& range, OtherRanges&&... ranges) {                            // NOLINT(cppcoreguidelines-missing-std-forward)
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		for (const Index index : elementIndices) {
			operation(range[index], ranges[index]...);
		}
	}

	/**
	 * Execute an operation for each indexed row of elements specified by a
	 * given range of indices in a given set of equally sized column ranges, in
	 * up to `getMaxParallelism()` parallel chunks using this executor.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        rows in the given column ranges to execute the operation on. Each
	 *        value must be implicitly convertible to a size_t that is less than
	 *        the size of each of the column ranges.
	 * \param operation function object that takes references to the elements in
	 *        each row of the given ranges and performs some in-place operation
	 *        on them, returning void.
	 * \param range first contiguous, sized random-access range in the set of
	 *        column ranges, indexed by `indices`. May contain unused elements,
	 *        which are ignored.
	 * \param ranges other contiguous, sized random-access ranges in the set of
	 *        column ranges, indexed by `indices`. May contain unused elements,
	 *        which are ignored. Each must have the same size as the first range.
	 *
	 * \throws any exception thrown by the operation. If multiple parallel tasks
	 *         throw an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Transform each element in a given range, sequentially.
	 *
	 * \param range contiguous, sized random-access range of elements to
	 *        transform.
	 * \param output random access iterator to write the transformed outputs to.
	 *        The range `[output, output + std::size(range))` must form a valid
	 *        writable range.
	 * \param transformationFunction function object that takes a read-only
	 *        reference to each element and performs some transformation on it,
	 *        returning the result.
	 *
	 * \throws any exception thrown by the transformation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelTransformation() to running
	 *       sequentially, and vice versa. There is no other direct benefit
	 *       compared to a basic for-loop or transform().
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeTransformation(const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
		using T = std::remove_pointer_t<decltype(std::data(range))>;

		const Span<T> elements{std::data(range), std::size(range)};
		for (size_t i = 0; i < elements.size(); ++i) {
			output[static_cast<ptrdiff_t>(i)] = transformationFunction(elements[i]);
		}
	}

	/**
	 * Transform each element in a given range, in up to `getMaxParallelism()`
	 * parallel chunks using this executor.
	 *
	 * \param range contiguous, sized random-access range of elements to
	 *        transform.
	 * \param output random access iterator to write the transformed outputs to.
	 *        The range `[output, output + std::size(range))` must form a valid
	 *        writable range.
	 * \param transformationFunction function object that takes a read-only
	 *        reference to each element and performs some transformation on it,
	 *        returning the result.
	 *
	 * \throws any exception thrown by the transformation. If multiple parallel
	 *         tasks throw an exception while running in parallel, one of the
	 *         tasks' exceptions will get propagated, but it is unspecified
	 *         which one. The other parallel tasks' exceptions are discarded,
	 *         but may print a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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
					parallelTaskContext.output[static_cast<ptrdiff_t>(i)] = parallelTaskContext.transformationFunction(chunk[i]);
				}
			},
			0, nullptr);
#else
		return executeTransformation(range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
#endif
	}

	/**
	 * Transform each indexed element specified by a given range of indices,
	 * sequentially.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        elements in `range` to transform. Each value must be implicitly
	 *        convertible to a size_t that is less than `std::size(range)`.
	 * \param range contiguous, sized random-access range of elements to
	 *        transform, indexed by `indices`. May contain unused elements,
	 *        which are ignored.
	 * \param output random access iterator to write the transformed outputs to.
	 *        The range `[output, output + std::size(indices))` must form a
	 *        valid writable range.
	 * \param transformationFunction function object that takes a read-only
	 *        reference to each element and performs some transformation on it,
	 *        returning the result.
	 *
	 * \throws any exception thrown by the transformation.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelSparseTransformation() to running
	 *       sequentially, and vice versa. There is no other direct benefit
	 *       compared to a basic for-loop.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	template <typename IndexRange, typename Range, typename RandomAccessIterator, typename UnaryMapping>
	void executeSparseTransformation(const IndexRange& indices, const Range& range, RandomAccessIterator output, UnaryMapping transformationFunction) {
		using T = std::remove_pointer_t<decltype(std::data(range))>;
		using Index = std::decay_t<decltype(*std::data(indices))>;

		const Span<const Index> elementIndices{std::data(indices), std::size(indices)};
		const Span<T> elements{std::data(range), std::size(range)};
		for (size_t i = 0; i < elementIndices.size(); ++i) {
			output[static_cast<ptrdiff_t>(i)] = transformationFunction(elements[elementIndices[i]]);
		}
	}

	/**
	 * Transform each indexed element specified by a given range of indices, in
	 * up to `getMaxParallelism()` parallel chunks using this executor.
	 *
	 * \param indices contiguous, sized random-access range of indices to the
	 *        elements in `range` to transform. Each value must be implicitly
	 *        convertible to a size_t that is less than `std::size(range)`.
	 * \param range contiguous, sized random-access range of elements to
	 *        transform, indexed by `indices`. May contain unused elements,
	 *        which are ignored.
	 * \param output random access iterator to write the transformed outputs to.
	 *        The range `[output, output + std::size(indices))` must form a
	 *        valid writable range.
	 * \param transformationFunction function object that takes a read-only
	 *        reference to each element and performs some transformation on it,
	 *        returning the result.
	 *
	 * \throws any exception thrown by the transformation. If multiple parallel
	 *         tasks throw an exception while running in parallel, one of the
	 *         tasks' exceptions will get propagated, but it is unspecified
	 *         which one. The other parallel tasks' exceptions are discarded,
	 *         but may print a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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
					parallelTaskContext.output[static_cast<ptrdiff_t>(i)] = parallelTaskContext.transformationFunction(parallelTaskContext.pointer[chunk[i]]);
				}
			},
			0, nullptr);
#else
		return executeSparseTransformation(indices, range, std::forward<RandomAccessIterator>(output), std::forward<UnaryMapping>(transformationFunction));
#endif
	}

	/**
	 * Perform a reduction over each element in a given range, sequentially.
	 *
	 * \param range contiguous, sized random-access range of elements to perform
	 *        the reduction over.
	 * \param initialValue initial value of the reduction.
	 * \param reductionFunction function object that takes a reduction value and
	 *        a read-only reference to each element and returns the new combined
	 *        reduction value.
	 *
	 * \throws any exception thrown by the reduction function.
	 *
	 * \note This function is provided to make it easier to transition parallel
	 *       code based on executeParallelReduction() to running sequentially,
	 *       and vice versa. There is no other direct benefit compared to a
	 *       basic for-loop or reduce().
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Perform a reduction over each element in a given range, in up to
	 * `getMaxParallelism()` parallel chunks using this executor.
	 *
	 * \param range contiguous, sized random-access range of elements to perform
	 *        the reduction over.
	 * \param initialValue initial value of the reduction.
	 * \param reductionFunction function object that takes a reduction value and
	 *        a read-only reference to each element and returns the new combined
	 *        reduction value.
	 *
	 * \throws any exception thrown by the reduction function. If multiple
	 *         parallel tasks throw an exception while running in parallel, one
	 *         of the tasks' exceptions will get propagated, but it is
	 *         unspecified which one. The other parallel tasks' exceptions are
	 *         discarded, but may print a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
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

	/**
	 * Execute a pre-built task graph using this executor.
	 *
	 * \param tasks topologically ordered list of tasks to execute. The tasks
	 *        must not depend on the results of tasks that appear later in the
	 *        list. Dependencies between tasks must be specified through
	 *        Task::getDependencyIndices(), all of which must be less than the
	 *        index of the dependent task.
	 * \param context user-defined context pointer to provide to each task.
	 * \param requiredSharedMemorySize total amount of contiguous memory, in
	 *        bytes, to pre-allocate for all of the tasks to use as shared
	 *        memory between their sub-tasks during execution. Each task is
	 *        provided a non-owning pointer to the first byte in its slice of
	 *        the allocated memory, based on `task.getSharedMemoryOffset()`,
	 *        which it can use freely as long as the access does not overlap
	 *        between sub-tasks or other tasks, or if the overlapping access is
	 *        properly synchronized (e.g. through AtomicRef).
	 * \param name UTF-8-encoded human-readable name of the task graph, which
	 *        may be used in error messages.
	 * \param statistics pointer to a set of statistics to fill out with the
	 *        statistics resulting from executing the task graph, or nullptr to
	 *        ignore.
	 *
	 * \throws execution::Error with a std::nested_exception of any exception
	 *         thrown by the scheduled tasks. If multiple parallel tasks throw
	 *         an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	virtual void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, CStringView name, Statistics* statistics) = 0;

	/**
	 * Execute a set parallel sub-tasks through a ParallelTask function that
	 * performs a specific chunk of work based on the index of each sub-task
	 * (and the total number of them), using this executor.
	 *
	 * \param subTaskContext user-defined context pointer to provide to each
	 *        sub-task.
	 * \param subTaskCount number of parallel sub-tasks to execute.
	 * \param subTask parallel task function to execute for each sub-task.
	 * \param taskRequiredSharedMemorySize amount of contiguous memory, in
	 *        bytes, to pre-allocate for use as shared memory between the
	 *        sub-tasks during execution. Each sub-task is provided a non-owning
	 *        pointer to the first byte of the allocated memory, which it can
	 *        use freely as long as the access does not overlap between
	 *        sub-tasks, or if the overlapping access is properly synchronized
	 *        (e.g. through AtomicRef).
	 * \param statistics pointer to a set of statistics to fill out with the
	 *        statistics resulting from executing the sub-tasks, or nullptr to
	 *        ignore.
	 *
	 * \throws execution::Error with a std::nested_exception of any exception
	 *         thrown by the parallel tasks. If multiple parallel tasks throw
	 *         an exception while running in parallel, one of the tasks'
	 *         exceptions will get propagated, but it is unspecified which one.
	 *         The other parallel tasks' exceptions are discarded, but may print
	 *         a warning message to stderr first.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Tasks must not dynamically execute nested tasks on the executor
	 *          that is running them.
	 */
	virtual void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize taskRequiredSharedMemorySize,
		Statistics* statistics) = 0;

	/**
	 * Get the maximum number of parallel threads of execution that this
	 * executor can run tasks on simultaneously.
	 *
	 * \return the maximum task parallelism.
	 */
	[[nodiscard]] virtual Task::ParallelCount getMaxParallelism() const noexcept = 0;

protected:
	/**
	 * Basic implementation of executeTaskGraph() that executes the tasks
	 * sequentially, directly on the calling thread.
	 *
	 * This may be used as a fallback implementation for executors with a
	 * maximum task parallelism of 1. For example, it is used in the
	 * implementation of SequentialExecutor.
	 *
	 * \param tasks topologically ordered list of tasks to execute. The tasks
	 *        must not depend on the results of tasks that appear later in the
	 *        list. Dependencies between tasks must be specified through
	 *        Task::getDependencyIndices(), all of which must be less than the
	 *        index of the dependent task.
	 * \param context user-defined context pointer to provide to each task.
	 * \param sharedMemory non-owning pointer to the shared memory to provide to
	 *        the tasks. Each task is provided a non-owning pointer to the first
	 *        byte in its slice of the shared memory, based on
	 *        `task.getSharedMemoryOffset()`, which it can use freely as long as
	 *        the access does not overlap between sub-tasks or other tasks, or
	 *        if the overlapping access is properly synchronized (e.g. through
	 *        AtomicRef). The amount of memory accessible through this pointer
	 *        must be greater than or equal to the total amount required by the
	 *        tasks. If the tasks use no shared memory, it may be nullptr.
	 * \param name UTF-8-encoded human-readable name of the task graph, which
	 *        may be used in error messages.
	 * \param statistics pointer to a set of statistics to fill out with the
	 *        statistics resulting from executing the task graph, or nullptr to
	 *        ignore.
	 *
	 * \throws execution::Error with a std::nested_exception of any exception
	 *         thrown by the tasks.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(execution)
	static void executeTaskGraphSequentially(Span<const Task> tasks, void* context, byte* sharedMemory, CStringView name, Statistics* statistics);

	/**
	 * Basic implementation of executeParallelTasks() that executes the
	 * sub-tasks sequentially, directly on the calling thread.
	 *
	 * This may be used as a fallback implementation for executors with a
	 * maximum task parallelism of 1. For example, it is used in the
	 * implementation of SequentialExecutor.
	 *
	 * \param subTaskContext user-defined context pointer to provide to each
	 *        sub-task.
	 * \param subTaskCount number of parallel sub-tasks to execute.
	 * \param subTask parallel task function to execute for each sub-task.
	 * \param taskSharedMemory non-owning pointer to the shared memory to
	 *        provide to the sub-tasks. Each sub-task is provided this pointer,
	 *        which it can use freely as long as the access does not overlap
	 *        between sub-tasks, or if the overlapping access is properly
	 *        synchronized (e.g. through AtomicRef). The amount of memory
	 *        accessible through this pointer must be greater than or equal to
	 *        the total amount required by the sub-tasks. If the sub-tasks use
	 *        no shared memory, it may be nullptr.
	 * \param statistics pointer to a set of statistics to fill out with the
	 *        statistics resulting from executing the sub-tasks, or nullptr to
	 *        ignore.
	 *
	 * \throws execution::Error with a std::nested_exception of any exception
	 *         thrown by the tasks.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(execution)
	static void executeParallelTasksSequentially(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, byte* taskSharedMemory, Statistics* statistics);
};

/**
 * Basic Executor implementation that runs all tasks sequentially, directly on
 * the caller's thread.
 *
 * \sa DynamicExecutor
 */
class SequentialExecutor final : public Executor {
public:
	void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, CStringView name, Statistics* statistics) override {
		if (requiredSharedMemorySize > sharedMemory.size()) {
			sharedMemory.resize(static_cast<size_t>(requiredSharedMemorySize));
		}
		executeTaskGraphSequentially(tasks, context, sharedMemory.data(), name, statistics);
	}

	void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize taskRequiredSharedMemorySize,
		Statistics* statistics) override {
		if (taskRequiredSharedMemorySize > sharedMemory.size()) {
			sharedMemory.resize(static_cast<size_t>(taskRequiredSharedMemorySize));
		}
		executeParallelTasksSequentially(subTaskContext, subTaskCount, subTask, sharedMemory.data(), statistics);
	}

	[[nodiscard]] Task::ParallelCount getMaxParallelism() const noexcept override {
		return 1;
	}

	/**
	 * Allocated shared memory available for reuse in subsequent executions.
	 *
	 * This may be resized and overwritten whenever tasks are executed on this
	 * executor.
	 */
	Allocation<byte> sharedMemory{};
};

/**
 * Configuration options for a DynamicExecutor.
 */
struct DynamicExecutorOptions {
	/**
	 * Desired level of parallelism of the chosen executor implementation.
	 *
	 * If this is less than 2, the chosen executor implementation will always be
	 * SequentialExecutor. Otherwise, if multithreading is supported by the
	 * platform, and enabled through `GREM_USE_MULTITHREADING`, the executor
	 * will attempt to use a thread pool-based implementation with a number of
	 * worker threads that corresponds to this number as closely as possible.
	 * Otherwise, the implementation is unspecified, and may fall back to
	 * SequentialExecutor.
	 *
	 * The default value tries to choose a reasonable degree of parallelism
	 * based on the number of available threads reported by the standard library
	 * implementation (and in turn, the operating system), which is then
	 * decremented by 1 to leave some room for the main thread. A different
	 * estimate may be required for better performance if the program has
	 * multiple executors running simultaneously, or if the system has other
	 * resource-intensive programs running that are contending for CPU time.
	 */
	Task::ParallelCount targetParallelism = static_cast<Task::ParallelCount>(clamp(Thread::hardware_concurrency(), 2u, 32u) - 1);

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const DynamicExecutorOptions& other) const = default;
};

/**
 * Dynamic Executor whose implementation is chosen at runtime, defaulting to a
 * parallel thread pool on supported platforms.
 *
 * \sa SequentialExecutor
 */
class DynamicExecutor final : public Executor {
public:
	/**
	 * Construct a dynamic executor with an implementation chosen based on a set
	 * of requirements.
	 *
	 * \param options executor options, see DynamicExecutorOptions.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(execution) explicit DynamicExecutor(const DynamicExecutorOptions& options);

	/**
	 * Construct a dynamic executor with a specific implementation.
	 *
	 * \param implementation non-null owning pointer to the concrete Executor
	 *        implementation to use, such as SequentialExecutor or a custom
	 *        user-defined class that derives from Executor and implements its
	 *        virtual interface.
	 */
	explicit DynamicExecutor(UniquePointer<Executor> implementation)
		: implementation(std::move(implementation)) {
		GREM_ASSERT(this->implementation);
	}

	void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, CStringView name, Statistics* statistics) override {
		GREM_ASSERT(implementation);
		implementation->executeTaskGraph(tasks, context, requiredSharedMemorySize, name, statistics);
	}

	void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize taskRequiredSharedMemorySize,
		Statistics* statistics) override {
		GREM_ASSERT(implementation);
		implementation->executeParallelTasks(subTaskContext, subTaskCount, subTask, taskRequiredSharedMemorySize, statistics);
	}

	[[nodiscard]] Task::ParallelCount getMaxParallelism() const noexcept override {
		return (implementation) ? implementation->getMaxParallelism() : 0;
	}

	/**
	 * Get a pointer to the underlying Executor implementation.
	 *
	 * \return a non-owning pointer to the underlying executor.
	 */
	[[nodiscard]] Executor* get() noexcept {
		return implementation.get();
	}

	/**
	 * Get a pointer to the underlying Executor implementation.
	 *
	 * \return a non-owning read-only pointer to the underlying executor.
	 */
	[[nodiscard]] const Executor* get() const noexcept {
		return implementation.get();
	}

private:
	UniquePointer<Executor> implementation{};
};

} // namespace grem::execution

#endif
