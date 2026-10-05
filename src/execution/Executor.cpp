// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/execution/Error.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Statistics.hpp>
#include <GREM/execution/Task.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Thread.hpp>
#include <GREM/core/system/synchronization.hpp>

#include <cstdio>    // stderr, std::fprintf
#include <exception> // std::exception, std::exception_ptr, std::current_exception, std::rethrow_exception
#include <utility>   // std::exchange
#endif

namespace grem::execution {

#ifdef GREM_USE_MULTITHREADING
class ThreadPoolExecutor : public Executor { // NOLINT(misc-use-internal-linkage)
public:
	explicit ThreadPoolExecutor(size_t workerThreadCount) {
		GREM_ASSERT(workerThreadCount >= 2);
		workerThreads.resize(workerThreadCount);
		for (size_t workerThreadIndex = 0; workerThreadIndex < workerThreadCount; ++workerThreadIndex) {
			try {
				workerThreads[workerThreadIndex] = Thread{&ThreadPoolExecutor::work, this, workerThreadIndex, ThreadID::getCurrent()};
			} catch (...) {
				GREM_ASSERT(taskGraphTasks.empty());
				readyFlag.test_and_set();
				readyFlag.notify_all();
				while (workerThreadIndex-- > 0) {
					workerThreads[workerThreadIndex].join();
				}
				throw;
			}
		}
	}

	~ThreadPoolExecutor() override {
		if (!workerThreads.empty()) {
			taskGraphTasks = {};
			readyFlag.test_and_set();
			readyFlag.notify_all();
			for (Thread& workerThread : workerThreads) {
				workerThread.join();
			}
		}
	}

	ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
	ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;

	void executeTaskGraph(Span<const Task> tasks, void* context, Task::SharedMemorySize requiredSharedMemorySize, CStringView name, Statistics* statistics) override {
		GREM_ASSERT(!workerThreads.empty());
		if (tasks.size() < workerThreads.size() || tasks.size() > Task::MAX_GRAPH_SIZE) {
			if (requiredSharedMemorySize > sharedMemory.size()) {
				sharedMemory.resize(static_cast<size_t>(requiredSharedMemorySize));
			}
			executeTaskGraphSequentially(tasks, context, sharedMemory.data(), name, statistics);
			return;
		}

		if (statistics) {
			const TimePoint startTime = Clock::now();
			statistics->startTime = startTime;
			statistics->endTime = startTime;
			statistics->workers.resize(workerThreads.size());
			for (Statistics::Worker& worker : statistics->workers) {
				worker.startTime = startTime;
				worker.endTime = startTime;
				worker.tasks.clear();
			}
			taskGraphStatistics = statistics->workers.data();
		} else {
			taskGraphStatistics = nullptr;
		}

		if (tasks.empty()) {
			return;
		}

		if (requiredSharedMemorySize > sharedMemory.size()) {
			sharedMemory.resize(static_cast<size_t>(requiredSharedMemorySize));
		}

		taskGraphContext = context;
		taskGraphName = name;
		taskGraphTasks = tasks;
		for (size_t i = 0; i < tasks.size(); ++i) {
			tasksDone[i].flag.clear(MemoryOrder::RELAXED);
		}
		nextTaskIndex.store(0, MemoryOrder::RELAXED);

		while (workFinishedFlag.test()) {
		}
		readyFlag.test_and_set();
		readyFlag.notify_all();
		workFinishedFlag.wait(false);
		readyFlag.clear();

		if (statistics) {
			statistics->endTime = Clock::now();
		}

		rethrowParallelException();
	}

	void executeParallelTasks(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, Task::SharedMemorySize taskRequiredSharedMemorySize,
		Statistics* statistics) override {
		struct TaskContext {
			ParallelTask subTask;
			void* subTaskContext;
		} taskContext{subTask, subTaskContext};

		SmallArrayList<Task, 32> tasks{};
		tasks.reserve(subTaskCount);
		for (Task::ParallelIndex subTaskIndex = 0; subTaskIndex < subTaskCount; ++subTaskIndex) {
			const Task::Function function = [](void* context, byte* taskSharedMemory, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
				const TaskContext& taskContext = *static_cast<TaskContext*>(context);
				taskContext.subTask.execute(taskContext.subTaskContext, taskSharedMemory, parallelIndex, parallelism);
			};
			tasks.emplace_back(function, 0, subTaskIndex, subTaskCount, Task::DependencyIndices{}, UniquePointer<char[]>{});
		}

		try {
			executeTaskGraph(tasks, &taskContext, taskRequiredSharedMemorySize, {}, statistics);
		} catch (const execution::Error& e) {
			std::rethrow_if_nested(e);
			throw;
		}
	}

	[[nodiscard]] Task::ParallelCount getMaxParallelism() const noexcept override {
		return static_cast<Task::ParallelCount>(min(workerThreads.size(), size_t{Task::MAX_GRAPH_SIZE - 1}));
	}

private:
	struct alignas(64) PaddedAtomicFlag { // Pad to typical 64-byte cache line size to mitigate false sharing.
		AtomicFlag flag{};
	};

	void work(size_t workerThreadIndex, [[maybe_unused]] ThreadID parentThreadID) {
#ifdef GREM_USE_PROFILING
		bool hasSetThreadInfo = false;
#endif

		const auto taskIsDone = [tasksDone = Span{tasksDone}](Task::GraphIndex taskIndex) -> bool {
			return tasksDone[taskIndex].flag.test(MemoryOrder::RELAXED);
		};

		while (true) {
			if (workingCount.fetch_add(1) == workerThreads.size() - 1) {
				workFinishedFlag.clear();
				workFinishedFlag.notify_all();
			}
			readyFlag.wait(false);
			if (taskGraphTasks.empty()) {
				break;
			}

			if (taskGraphStatistics) {
				Statistics::Worker& worker = taskGraphStatistics[workerThreadIndex];
				const TimePoint startTime = Clock::now();
				worker.startTime = startTime;
				worker.endTime = startTime;
				worker.tasks.clear();
			}

			while (true) {
				const size_t taskIndex = nextTaskIndex.fetch_add(1, MemoryOrder::RELAXED);
				if (taskIndex >= taskGraphTasks.size()) {
					break;
				}
				const Task& task = taskGraphTasks[taskIndex];
				const Span<const Task::GraphIndex> dependencyIndices = task.getDependencyIndices();
				if (!dependencyIndices.empty()) {
					// Reduce read contention by starting at an offset correlated with our task index.
					const size_t slicePoint = taskIndex % dependencyIndices.size();
					const Span<const Task::GraphIndex> firstSlice = dependencyIndices.first(slicePoint);
					const Span<const Task::GraphIndex> secondSlice = dependencyIndices.subspan(slicePoint);
					while (!allOf(secondSlice, taskIsDone) || !allOf(firstSlice, taskIsDone)) {
					}
					atomicThreadFence(MemoryOrder::ACQUIRE);
				}
				if (!errorFlag.test(MemoryOrder::RELAXED)) {
					try {
#ifdef GREM_USE_PROFILING
						if (!hasSetThreadInfo) {
							[[unlikely]];
							hasSetThreadInfo = true;
							GREM_PROFILER_SET_THREAD_INFO("Worker thread", workerThreadIndex, parentThreadID);
						}
#endif

						const TimePoint startTime = Clock::now();
						{
							GREM_PROFILE_BLOCK_DYNAMIC((task.getName().empty()) ? "Task" : task.getName());
							task.execute(taskGraphContext, sharedMemory.data());
						}
						const TimePoint endTime = Clock::now();

						if (taskGraphStatistics) {
							taskGraphStatistics[workerThreadIndex].tasks.push_back(Statistics::Worker::Task{
								.taskIndex = taskIndex,
								.startTime = startTime,
								.endTime = endTime,
							});
						}
					} catch (...) {
						handleCurrentException(task.getName());
					}
				}
				tasksDone[taskIndex].flag.test_and_set(MemoryOrder::RELEASE);
			}

			if (taskGraphStatistics) {
				taskGraphStatistics[workerThreadIndex].endTime = Clock::now();
			}

			if (workingCount.fetch_sub(1) == 1) {
				workFinishedFlag.test_and_set();
				workFinishedFlag.notify_all();
			}

			while (readyFlag.test()) {
			}
		}
	}

	void handleCurrentException(CStringView taskName) {
		try {
			String message{"Error in task"};
			if (!taskName.empty()) {
				message.append(" \"");
				message.append(taskName);
				message.push_back('\"');
			}
			if (!taskGraphName.empty()) {
				message.append(" while executing \"");
				message.append(taskGraphName);
				message.push_back('\"');
			}
			message.push_back('.');
			Error::throwWithNested(execution::Error{message});
		} catch (...) {
			bool handled = false;
			{
				const ScopedLock lock{errorMutex};
				if (!errorPointer) {
					errorPointer = std::current_exception();
					errorFlag.test_and_set(MemoryOrder::RELEASE);
					handled = true;
				}
			}
			if (!handled) {
				std::fprintf(stderr, "Unhandled exception in executor: %s\n", Error::formatCurrentExceptionMessage().c_str());
			}
		}
	}

	void rethrowParallelException() {
		if (errorFlag.test(MemoryOrder::ACQUIRE)) {
			std::exception_ptr error{};
			{
				const ScopedLock lock{errorMutex};
				error = std::exchange(errorPointer, {});
				errorFlag.clear(MemoryOrder::RELEASE);
			}
			GREM_ASSERT(error);
			std::rethrow_exception(error);
		}
	}

	void* taskGraphContext = nullptr;
	CStringView taskGraphName{};
	Span<const Task> taskGraphTasks{};
	Statistics::Worker* taskGraphStatistics = nullptr;
	Atomic<size_t> nextTaskIndex{};
	Array<PaddedAtomicFlag, Task::MAX_GRAPH_SIZE> tasksDone{};
	AtomicFlag readyFlag{};
	AtomicFlag workFinishedFlag{};
	Atomic<size_t> workingCount{};
	Allocation<Thread> workerThreads{};
	AtomicFlag errorFlag{};
	Mutex errorMutex{};
	std::exception_ptr errorPointer{};
	Allocation<byte> sharedMemory{};
};
#endif

void Executor::executeTaskGraphSequentially(Span<const Task> tasks, void* context, byte* sharedMemory, CStringView name, Statistics* statistics) {
	if (statistics) {
		const TimePoint startTime = Clock::now();
		statistics->startTime = startTime;
		statistics->endTime = startTime;
		statistics->workers.resize(1);
		statistics->workers.front().startTime = startTime;
		statistics->workers.front().endTime = startTime;
		statistics->workers.front().tasks.clear();
	}

	for (size_t taskIndex = 0; taskIndex < tasks.size(); ++taskIndex) {
		const Task& task = tasks[taskIndex];
		try {
			const TimePoint startTime = Clock::now();
			{
				GREM_PROFILE_BLOCK_DYNAMIC((task.getName().empty()) ? "Task" : task.getName());
				task.execute(context, sharedMemory);
			}
			const TimePoint endTime = Clock::now();

			if (statistics) {
				statistics->workers.front().tasks.push_back(Statistics::Worker::Task{
					.taskIndex = taskIndex,
					.startTime = startTime,
					.endTime = endTime,
				});
			}
		} catch (...) {
			String message{"Error in task"};
			if (!task.getName().empty()) {
				message.append(" \"");
				message.append(task.getName());
				message.push_back('\"');
			}
			if (!name.empty()) {
				message.append(" while executing \"");
				message.append(name);
				message.push_back('\"');
			}
			message.push_back('.');
			Error::throwWithNested(execution::Error{message});
		}
	}

	if (statistics) {
		const TimePoint endTime = Clock::now();
		statistics->workers.front().endTime = endTime;
		statistics->endTime = endTime;
	}
}

void Executor::executeParallelTasksSequentially(void* subTaskContext, Task::ParallelCount subTaskCount, ParallelTask subTask, byte* taskSharedMemory, Statistics* statistics) {
	if (statistics) {
		const TimePoint startTime = Clock::now();
		statistics->startTime = startTime;
		statistics->endTime = startTime;
		statistics->workers.resize(1);
		statistics->workers.front().startTime = startTime;
		statistics->workers.front().endTime = startTime;
		statistics->workers.front().tasks.clear();
	}

	for (Task::ParallelIndex subTaskIndex = 0; subTaskIndex < subTaskCount; ++subTaskIndex) {
		const TimePoint startTime = Clock::now();
		{
			GREM_PROFILE_BLOCK("Parallel sub-task");
			subTask.execute(subTaskContext, taskSharedMemory, subTaskIndex, subTaskCount);
		}
		const TimePoint endTime = Clock::now();

		if (statistics) {
			statistics->workers.front().tasks.push_back(Statistics::Worker::Task{
				.taskIndex = subTaskIndex,
				.startTime = startTime,
				.endTime = endTime,
			});
		}
	}

	if (statistics) {
		const TimePoint endTime = Clock::now();
		statistics->workers.front().endTime = endTime;
		statistics->endTime = endTime;
	}
}

DynamicExecutor::DynamicExecutor([[maybe_unused]] const DynamicExecutorOptions& options) {
#ifdef GREM_USE_MULTITHREADING
	if (options.targetParallelism >= 2) {
		implementation = UniquePointer<ThreadPoolExecutor>::create(options.targetParallelism);
		return;
	}
#endif
	implementation = UniquePointer<SequentialExecutor>::create();
}

} // namespace grem::execution
