// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_SCHEDULER_HPP
#define GREM_EXECUTION_SCHEDULER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/Thread.hpp>
#include <GREM/execution/Chunk.hpp>
#include <GREM/execution/Error.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/execution/component.hpp>
#include <GREM/execution/component_pool.hpp>
#include <GREM/execution/entity_range.hpp>
#include <GREM/execution/resource.hpp>

#include <stdexcept>   // std::length_error
#include <type_traits> // std::is_const_v, std::remove_..._t, std::conditional_t
#include <typeindex>   // std::type_index
#include <typeinfo>    // IWYU pragma: keep // typeid
#include <utility>     // std::move, std::in_place

namespace grem::execution {

namespace detail {

template <typename T>
struct remove_optional_or_pointer {
	using type = T;
};

template <typename T>
struct remove_optional_or_pointer<T*> {
	using type = T;
};

template <typename T>
struct remove_optional_or_pointer<Optional<T>> {
	using type = T;
};

template <typename T>
using remove_optional_or_pointer_t = typename remove_optional_or_pointer<T>::type;

template <typename T>
[[nodiscard]] constexpr decltype(auto) moveIfNotPointer(auto& value) {
	if constexpr (pointer<T>) {
		return value;
	} else {
		return std::move(value);
	}
}

using AccessFlags = uint8_t;
enum AccessFlag : AccessFlags { // NOLINT(cppcoreguidelines-use-enum-class)
	ACCESS_MUTABLE = 1 << 0,
	ACCESS_MUTABLE_RESOURCE_REGISTRY = 1 << 1,
	ACCESS_IMMUTABLE_RESOURCE_REGISTRY = 1 << 2,
	ACCESS_MUTABLE_ENTITY_REGISTRY = 1 << 3,
	ACCESS_IMMUTABLE_ENTITY_REGISTRY = 1 << 4,
};

template <typename EntReg, typename ResReg, typename EntitiesOrResource>
struct extract_accesses;

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, Resource&> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<Resource>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, Resource> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<Resource>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, const Resource> : extract_accesses<EntReg, ResReg, Resource> {};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, const Resource&> : extract_accesses<EntReg, ResReg, Resource> {};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, EntReg&> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS = ACCESS_MUTABLE_ENTITY_REGISTRY;
};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, EntReg> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS = ACCESS_IMMUTABLE_ENTITY_REGISTRY;
};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, const EntReg> : extract_accesses<EntReg, ResReg, EntReg> {};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, const EntReg&> : extract_accesses<EntReg, ResReg, EntReg> {};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, ResReg&> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS = ACCESS_MUTABLE_RESOURCE_REGISTRY;
};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, ResReg> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS = ACCESS_IMMUTABLE_RESOURCE_REGISTRY;
};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, const ResReg> : extract_accesses<EntReg, ResReg, ResReg> {};

template <typename EntReg, typename ResReg>
struct extract_accesses<EntReg, ResReg, const ResReg&> : extract_accesses<EntReg, ResReg, ResReg> {};

template <typename EntReg, typename ResReg, typename Resource, auto Projection>
struct extract_accesses<EntReg, ResReg, Chunk<Resource, Projection>> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<Resource>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, typename Resource, auto Projection>
struct extract_accesses<EntReg, ResReg, Chunk<const Resource, Projection>> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<Resource>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, typename Resource, auto Projection>
struct extract_accesses<EntReg, ResReg, Chunk<Resource, Projection>&&> : extract_accesses<EntReg, ResReg, Chunk<Resource, Projection>> {};

template <typename EntReg, typename ResReg, typename Resource, auto Projection>
struct extract_accesses<EntReg, ResReg, const Chunk<Resource, Projection>> : extract_accesses<EntReg, ResReg, Chunk<Resource, Projection>> {};

template <typename EntReg, typename ResReg, typename Resource, auto Projection>
struct extract_accesses<EntReg, ResReg, const Chunk<Resource, Projection>&> : extract_accesses<EntReg, ResReg, Chunk<Resource, Projection>> {};

template <typename EntReg, typename ResReg, typename EntityRange>
requires(is_entity_range_v<EntityRange>) struct extract_accesses<EntReg, ResReg, EntityRange> {
	using EntityRanges = meta::TypeList<EntityRange>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = typename EntityRange::MutableComponents;
	using ImmutableComponents = meta::type_list_concat_t<typename EntityRange::ImmutableComponents, typename EntityRange::ExcludedComponents>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, typename EntityRange>
requires(is_entity_range_v<EntityRange>) struct extract_accesses<EntReg, ResReg, EntityRange&&> : extract_accesses<EntReg, ResReg, EntityRange> {};

template <typename EntReg, typename ResReg, typename EntityRange>
requires(is_entity_range_v<EntityRange>) struct extract_accesses<EntReg, ResReg, const EntityRange> : extract_accesses<EntReg, ResReg, EntityRange> {};

template <typename EntReg, typename ResReg, typename EntityRange>
requires(is_entity_range_v<EntityRange>) struct extract_accesses<EntReg, ResReg, const EntityRange&> : extract_accesses<EntReg, ResReg, EntityRange> {};

template <typename EntReg, typename ResReg, typename ComponentPool>
requires(is_component_pool_v<ComponentPool>) struct extract_accesses<EntReg, ResReg, ComponentPool> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<ComponentPool>;
	using MutableComponents = std::conditional_t<std::is_const_v<typename ComponentPool::component_type>, meta::TypeList<>, meta::TypeList<typename ComponentPool::component_type>>;
	using ImmutableComponents =
		std::conditional_t<std::is_const_v<typename ComponentPool::component_type>, meta::TypeList<std::remove_const_t<typename ComponentPool::component_type>>, meta::TypeList<>>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, typename ComponentPool>
requires(is_component_pool_v<ComponentPool>) struct extract_accesses<EntReg, ResReg, ComponentPool&> : extract_accesses<EntReg, ResReg, ComponentPool> {};

template <typename EntReg, typename ResReg, typename ComponentPool>
requires(is_component_pool_v<ComponentPool>) struct extract_accesses<EntReg, ResReg, const ComponentPool> : extract_accesses<EntReg, ResReg, ComponentPool> {};

template <typename EntReg, typename ResReg, typename ComponentPool>
requires(is_component_pool_v<ComponentPool>) struct extract_accesses<EntReg, ResReg, const ComponentPool&> : extract_accesses<EntReg, ResReg, ComponentPool> {};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, Exclude<Resource>> {
	using EntityRanges = meta::TypeList<>;
	using ComponentPools = meta::TypeList<>;
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using MutableResources = meta::TypeList<>;
	using ImmutableResources = meta::TypeList<Resource>;
	using MutableResourceChunks = meta::TypeList<>;
	using ImmutableResourceChunks = meta::TypeList<>;
	static constexpr AccessFlags FLAGS{};
};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, Exclude<Resource>&&> : extract_accesses<EntReg, ResReg, Exclude<Resource>> {};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, const Exclude<Resource>> : extract_accesses<EntReg, ResReg, Exclude<Resource>> {};

template <typename EntReg, typename ResReg, resource Resource>
struct extract_accesses<EntReg, ResReg, const Exclude<Resource>&> : extract_accesses<EntReg, ResReg, Exclude<Resource>> {};

template <typename EntReg, typename ResReg, typename... Args>
struct task_access_traits {
	using Arguments = meta::TypeList<Args...>;
	using EntityRanges = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::EntityRanges...>;
	using ComponentPools = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ComponentPools...>;
	using MutableComponents = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableComponents...>;
	using ImmutableComponents = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableComponents...>;
	using MutableResources = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableResources...>;
	using ImmutableResources = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableResources...>;
	using MutableResourceChunks = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableResourceChunks...>;
	using ImmutableResourceChunks = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableResourceChunks...>;
	static constexpr AccessFlags FLAGS = (extract_accesses<EntReg, ResReg, Args>::FLAGS | ... | AccessFlags{});
};

template <typename EntReg, typename ResReg, typename R, typename... Args>
struct reduction_function_access_traits {
	using Resource = R;
	using Arguments = meta::TypeList<Args...>;
	using EntityRanges = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::EntityRanges...>;
	using ComponentPools = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ComponentPools...>;
	using MutableComponents = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableComponents...>;
	using ImmutableComponents = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableComponents...>;
	using MutableResources = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableResources...>;
	using ImmutableResources = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableResources...>;
	using MutableResourceChunks = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::MutableResourceChunks...>;
	using ImmutableResourceChunks = meta::type_list_concat_t<typename extract_accesses<EntReg, ResReg, Args>::ImmutableResourceChunks...>;
	static constexpr AccessFlags FLAGS = (extract_accesses<EntReg, ResReg, Args>::FLAGS | ... | AccessFlags{});
};

template <typename EntReg, typename ResReg, typename... Args>
[[nodiscard]] consteval task_access_traits<EntReg, ResReg, Args...> getTaskFunctionAccessTraits(void (*)(Args...));

template <typename EntReg, typename ResReg, resource Resource, typename... Args>
[[nodiscard]] consteval reduction_function_access_traits<EntReg, ResReg, Resource, Args...> getReductionFunctionAccessTraits(
	Resource (*)(const Resource&, const Resource&, Args...));

struct UnscheduledTask {
	Task::Function function;
	Task::SharedMemoryOffset sharedMemoryOffset;
	Task::ParallelCount parallelism;
	String name;
};

struct Accessor {
	Task::GraphIndex taskIndex;
	AccessFlags accessFlags;

	[[nodiscard]] bool hasMutableAccess() const noexcept {
		return (accessFlags & ACCESS_MUTABLE) != 0;
	}
};

[[nodiscard]] GREM_API(execution) ArrayList<Task> buildSchedule(Span<const UnscheduledTask> tasks, const HashMap<std::type_index, Buffer<Accessor>>& componentAccesses,
	const HashMap<std::type_index, Buffer<Accessor>>& resourceAccesses, Span<const Accessor> entityRegistryAccesses, Span<const Accessor> resourceRegistryAccesses);

} // namespace detail

/**
 * Statically scheduled task graph builder.
 *
 * This class is intended to be used at application startup to automatically
 * determine the data dependencies between a dynamic list of ordered tasks
 * (which can e.g. be loaded by dynamically linked plugins) and generate a
 * static execution graph in the form of a Schedule that specifies which tasks
 * are able to be run independently in parallel, such that they can be
 * efficiently executed by a multithreaded Executor any number of times.
 *
 * There are four kinds of tasks that can be scheduled, using the following
 * categories of functions:
 * - `addTask()`: For basic tasks that perform some self-contained unit of work,
 *   with arbitrary read/write access to any entities, component pools or
 *   resources needed by the task. These tasks can be run in parallel with other
 *   tasks that don't have overlapping access to the same mutable
 *   components/resources.
 * - `addParallelTask()`: For tasks that perform a chunked operation over a
 *   specific resource, with read-only access to all other data. These tasks are
 *   split into per-chunk sub-tasks that can be run in parallel with each other,
 *   as well as with any other tasks that don't have overlapping access to the
 *   same mutable components/resources.
 * - `addParallelTransformationTask()`: For tasks that perform a chunked
 *   transformation over a specific entity range, with read-only access to all
 *   other data. These tasks are split into per-chunk sub-tasks that can be run
 *   in parallel with each other, as well as with any other tasks that don't
 *   have overlapping access to the same mutable components/resources.
 * - `addParallelReductionTask()`: For tasks that perform a chunked reduction
 *   operation (e.g. a sum or min/max) over a specific entity range and output
 *   the result to a specific resource, with read-only access to all other
 *   data. These tasks are split into per-chunk sub-tasks that can be run
 *   in parallel with each other, as well as with any other tasks that don't
 *   have overlapping access to the same mutable components/resources, followed
 *   by a single task that combines all of the intermediate per-chunk results
 *   and writes the final output to the resource.
 *
 * The parallel task types also have "unsafe" variants, e.g.
 * `addUnsafeParallelTask()`, that don't enforce read-only access to the
 * non-chunked data. These can be useful when doing manual
 * synchronization/separation of the shared data, separate from the scheduling
 * system.
 *
 * Each task type also has a corresponding "optional" variant, e.g.
 * `addOptionalTask()`, that does not throw an exception when there are missing
 * task arguments, and simply skips the task instead in that case.
 *
 * \tparam EntReg concrete entity registry type used by the scheduled tasks,
 *         e.g. some specialization of EntityRegistry or EntityTable.
 * \tparam ResReg concrete resource registry type used by the scheduled tasks,
 *         e.g. some specialization of ResourceRegistry or ResourceTable.
 */
template <typename EntReg, typename ResReg>
class Scheduler {
public:
	/**
	 * Erase all tasks from the task list and reset the scheduler to an empty
	 * state.
	 */
	void clear() noexcept {
		tasks.clear();
		componentAccesses.clear();
		resourceAccesses.clear();
		entityRegistryAccesses.clear();
		resourceRegistryAccesses.clear();
		requiredSharedMemorySize = 0;
		barrier = false;
	}

	/**
	 * Append a basic task to the task list.
	 *
	 * \tparam TaskFunction contextless function object that executes the task.
	 *         The task function should return void and may accept the following
	 *         kinds of parameters:
	 *         - `T&`: Mutable access to a singleton #resource of type `T` from
	 *           the resource registry that is provided on execution of the
	 *           schedule.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *         - A component pool reference type, such as ComponentPool, from
	 *           the entity registry that is provided on execution of the
	 *           schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 *
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addTask(String name = {}) {
		return addTaskImplementation<TaskFunction, []<typename... Args>(TaskContext& taskContext, meta::TypeList<Args...> argumentTypes) -> void {
			auto taskArguments = getTaskArguments(taskContext.entities, taskContext.resources, argumentTypes);
			[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
				(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
			}(std::make_index_sequence<sizeof...(Args)>{});
		}>(std::move(name));
	}

	/**
	 * Append an optional basic task to the task list.
	 *
	 * \tparam TaskFunction contextless function object that executes the task.
	 *         The task function should return void and may accept the following
	 *         kinds of parameters:
	 *         - `T&`: Mutable access to a singleton #resource of type `T` from
	 *           the resource registry that is provided on execution of the
	 *           schedule.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *         - A component pool reference type, such as ComponentPool, from
	 *           the entity registry that is provided on execution of the
	 *           schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 *
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addOptionalTask(String name = {}) {
		return addTaskImplementation<TaskFunction, []<typename... Args>(TaskContext& taskContext, meta::TypeList<Args...> argumentTypes) -> void {
			bool foundAll = true;
			executeOptionalTaskFunction(TaskFunction, foundAll, getOptionalTaskArguments(foundAll, taskContext.entities, taskContext.resources, argumentTypes));
		}>(std::move(name));
	}

	/**
	 * Append a data-parallel task to the task list that performs a chunked
	 * operation over a specific #resource.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `Chunk<T>`: Mutable access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `Chunk<const T>`: Read-only access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - A read-only entity range type, such as Entities or Columns,
	 *           from the entity registry that is provided on execution of the
	 *           schedule.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addParallelTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelTaskImplementation<TaskFunction,
			[]<typename... Args>(TaskContext& taskContext, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, meta::TypeList<Args...> argumentTypes) -> void {
				auto taskArguments = getParallelTaskArguments(parallelIndex, parallelism, taskContext.entities, taskContext.resources, argumentTypes);
				[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
					(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
				}(std::make_index_sequence<sizeof...(Args)>{});
			}>(parallelism, std::move(name));
	}

	/**
	 * Append an optional data-parallel task to the task list that performs a
	 * chunked operation over a specific #resource.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `Chunk<T>`: Mutable access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `Chunk<const T>`: Read-only access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - A read-only entity range type, such as Entities or Columns,
	 *           from the entity registry that is provided on execution of the
	 *           schedule.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addOptionalParallelTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelTaskImplementation<TaskFunction,
			[]<typename... Args>(TaskContext& taskContext, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, meta::TypeList<Args...> argumentTypes) -> void {
				bool foundAll = true;
				executeOptionalTaskFunction(TaskFunction, foundAll,
					getOptionalParallelTaskArguments(foundAll, parallelIndex, parallelism, taskContext.entities, taskContext.resources, argumentTypes));
			}>(parallelism, std::move(name));
	}

	/**
	 * Append a data-parallel task to the task list that performs a chunked
	 * operation over a specific #resource, without any of the compile-time
	 * safety checks that prevent sub-tasks from accidentally mutating shared
	 * data.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `Chunk<T>`: Mutable access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `Chunk<const T>`: Read-only access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - A read-only entity range type, such as Entities or Columns,
	 *           from the entity registry that is provided on execution of the
	 *           schedule.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addUnsafeParallelTask(Task::ParallelCount parallelism, String name = {}) {
		return addUnsafeParallelTaskImplementation<TaskFunction,
			[]<typename... Args>(TaskContext& taskContext, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, meta::TypeList<Args...> argumentTypes) -> void {
				auto taskArguments = getParallelTaskArguments(parallelIndex, parallelism, taskContext.entities, taskContext.resources, argumentTypes);
				[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
					(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
				}(std::make_index_sequence<sizeof...(Args)>{});
			}>(parallelism, std::move(name));
	}

	/**
	 * Append an optional data-parallel task to the task list that performs a
	 * chunked operation over a specific #resource, without any of the
	 * compile-time safety checks that prevent sub-tasks from accidentally
	 * mutating shared data.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `Chunk<T>`: Mutable access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `Chunk<const T>`: Read-only access to a subrange of a singleton
	 *           #resource of type `T`. Each parallel invocation of the task
	 *           function will be provided a distinct non-overlapping subrange
	 *           of the full resource, such that the full range is covered.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - A read-only entity range type, such as Entities or Columns,
	 *           from the entity registry that is provided on execution of the
	 *           schedule.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addOptionalUnsafeParallelTask(Task::ParallelCount parallelism, String name = {}) {
		return addUnsafeParallelTaskImplementation<TaskFunction,
			[]<typename... Args>(TaskContext& taskContext, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, meta::TypeList<Args...> argumentTypes) -> void {
				bool foundAll = true;
				executeOptionalTaskFunction(TaskFunction, foundAll,
					getOptionalParallelTaskArguments(foundAll, parallelIndex, parallelism, taskContext.entities, taskContext.resources, argumentTypes));
			}>(parallelism, std::move(name));
	}

	/**
	 * Append a data-parallel task to the task list that performs a chunked
	 * transformation over a specific entity range.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addParallelTransformationTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelTransformationTaskImplementation<TaskFunction,
			[]<typename EntityRange, typename... Args>(TaskContext& taskContext, const EntityRange& chunk, meta::TypeList<Args...> argumentTypes) -> void {
				auto taskArguments = getParallelTransformationTaskArguments(chunk, taskContext.entities, taskContext.resources, argumentTypes);
				[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
					(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
				}(std::make_index_sequence<sizeof...(Args)>{});
			}>(parallelism, std::move(name));
	}

	/**
	 * Append an optional data-parallel task to the task list that performs a
	 * chunked transformation over a specific entity range.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addOptionalParallelTransformationTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelTransformationTaskImplementation<TaskFunction,
			[]<typename EntityRange, typename... Args>(TaskContext& taskContext, const EntityRange& chunk, meta::TypeList<Args...> argumentTypes) -> void {
				bool foundAll = true;
				executeOptionalTaskFunction(TaskFunction, foundAll,
					getOptionalParallelTransformationTaskArguments(foundAll, chunk, taskContext.entities, taskContext.resources, argumentTypes));
			}>(parallelism, std::move(name));
	}

	/**
	 * Append a data-parallel task to the task list that performs a chunked
	 * transformation over a specific entity range, without any of the
	 * compile-time safety checks that prevent sub-tasks from accidentally
	 * mutating shared data.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addUnsafeParallelTransformationTask(Task::ParallelCount parallelism, String name = {}) {
		return addUnsafeParallelTransformationTaskImplementation<TaskFunction,
			[]<typename EntityRange, typename... Args>(TaskContext& taskContext, const EntityRange& chunk, meta::TypeList<Args...> argumentTypes) -> void {
				auto taskArguments = getParallelTransformationTaskArguments(chunk, taskContext.entities, taskContext.resources, argumentTypes);
				[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
					(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
				}(std::make_index_sequence<sizeof...(Args)>{});
			}>(parallelism, std::move(name));
	}

	/**
	 * Append an optional data-parallel task to the task list that performs a
	 * chunked transformation over a specific entity range, without any of the
	 * compile-time safety checks that prevent sub-tasks from accidentally
	 * mutating shared data.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction>
	Scheduler& addOptionalUnsafeParallelTransformationTask(Task::ParallelCount parallelism, String name = {}) {
		return addUnsafeParallelTransformationTaskImplementation<TaskFunction,
			[]<typename EntityRange, typename... Args>(TaskContext& taskContext, const EntityRange& chunk, meta::TypeList<Args...> argumentTypes) -> void {
				bool foundAll = true;
				executeOptionalTaskFunction(TaskFunction, foundAll,
					getOptionalUnsafeParallelTransformationTaskArguments(foundAll, chunk, taskContext.entities, taskContext.resources, argumentTypes));
			}>(parallelism, std::move(name));
	}

	/**
	 * Append a data-parallel task to the task list that performs a chunked
	 * reduction operation over a specific entity range and outputs the result
	 * to a specific #resource.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T&`: Mutable access to a singleton #resource of type `T` to
	 *           write the result of the reduction operation to.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and throws an
	 *         execution::Error if any of the task function's parameters are
	 *         missing from the provided registries when the task is executed.
	 * \tparam ReductionFunction contextless function object that takes two
	 *         instances of the resource type output by `TaskFunction` and
	 *         reduces them into one, returning the result. This reduction
	 *         function is used to combine the intermediate results produced by
	 *         the main task function for each chunk. It may also, in addition
	 *         to the two resource instances, accept the same kinds of read-only
	 *         parameters as the main task function at the end of the parameter
	 *         list.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction, auto ReductionFunction>
	Scheduler& addParallelReductionTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelReductionTaskImplementation<TaskFunction, ReductionFunction,
			[]<typename EntityRange, typename Resource, typename... TaskArgs, typename... ReductionArgs>(TaskContext& taskContext, const EntityRange& chunk, Resource* resource,
				meta::TypeList<TaskArgs...> taskArgumentTypes, meta::TypeList<ReductionArgs...> reductionArgumentTypes) -> void {
				if (!hasParallelReductionTaskArguments<Resource>(taskContext.resources, taskArgumentTypes, reductionArgumentTypes)) {
					throw execution::Error{"Missing parallel reduction task arguments."};
				}
				GREM_ASSERT(resource);
				auto taskArguments = getParallelReductionTaskArguments(chunk, *resource, taskContext.entities, taskContext.resources, taskArgumentTypes);
				[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
					(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
				}(std::make_index_sequence<sizeof...(TaskArgs)>{});
			},
			[]<typename Resource, typename... TaskArgs, typename... ReductionArgs>(TaskContext& taskContext, Resource* output, const Resource* inputs,
				Task::ParallelCount parallelism, meta::TypeList<TaskArgs...> taskArgumentTypes, meta::TypeList<ReductionArgs...> reductionArgumentTypes) -> void {
				if (!hasParallelReductionTaskArguments<Resource>(taskContext.resources, taskArgumentTypes, reductionArgumentTypes)) {
					throw execution::Error{"Missing parallel reduction task arguments."};
				}
				GREM_ASSERT(output);
				GREM_ASSERT(inputs);
				Resource result = inputs[0];
				for (Task::ParallelIndex i = 1; i < parallelism; ++i) {
					auto reductionArguments = getParallelReductionFunctionArguments(result, inputs[i], taskContext.entities, taskContext.resources, reductionArgumentTypes);
					result = [&reductionArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> Resource {
						return (+ReductionFunction)(get<Indices>(std::move(reductionArguments))...);
					}(std::make_index_sequence<2 + sizeof...(ReductionArgs)>{});
				}
				*output = result;
			}>(parallelism, std::move(name));
	}

	/**
	 * Append an optional data-parallel task to the task list that performs a
	 * chunked reduction operation over a specific entity range and outputs the
	 * result to a specific #resource.
	 *
	 * \tparam TaskFunction contextless function object that executes one chunk
	 *         of the task. The task function should return void and may accept
	 *         the following kinds of parameters:
	 *         - `T&`: Mutable access to a singleton #resource of type `T` to
	 *           write the result of the reduction operation to.
	 *         - `T` or `const T&`: Read-only access to a singleton #resource of
	 *           type `T` from the resource registry that is provided on
	 *           execution of the schedule.
	 *         - An entity range type, such as Entities or Columns, from the
	 *           entity registry that is provided on execution of the schedule.
	 *           Each parallel invocation of the task function will be provided
	 *           a distinct non-overlapping subrange of the full entity range,
	 *           such that the full range is covered.
	 *         - A read-only component pool reference type, such as
	 *           ComponentPool, from the entity registry that is provided on
	 *           execution of the schedule.
	 *         The task function will be wrapped in a thunk that retrieves the
	 *         required arguments from the task context and skips the task if
	 *         any of the task function's parameters are missing from the
	 *         provided registries when the task is executed.
	 * \tparam ReductionFunction contextless function object that takes two
	 *         instances of the resource type output by `TaskFunction` and
	 *         reduces them into one, returning the result. This reduction
	 *         function is used to combine the intermediate results produced by
	 *         the main task function for each chunk. It may also, in addition
	 *         to the two resource instances, accept the same kinds of read-only
	 *         parameters as the main task function at the end of the parameter
	 *         list.
	 *
	 * \param parallelism number of parallel chunks to split the task into.
	 *        Should typically be less than or equal to the
	 *        `executor.getMaxParallelism()` value of the executor that the
	 *        schedule will be executed on.
	 * \param name optional UTF-8-encoded human-readable name of the task.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <auto TaskFunction, auto ReductionFunction>
	Scheduler& addOptionalParallelReductionTask(Task::ParallelCount parallelism, String name = {}) {
		return addParallelReductionTaskImplementation<TaskFunction, ReductionFunction,
			[]<typename EntityRange, typename Resource, typename... TaskArgs, typename... ReductionArgs>(TaskContext& taskContext, const EntityRange& chunk, Resource* resource,
				meta::TypeList<TaskArgs...> taskArgumentTypes, meta::TypeList<ReductionArgs...> reductionArgumentTypes) -> void {
				if (hasParallelReductionTaskArguments<Resource>(taskContext.resources, taskArgumentTypes, reductionArgumentTypes)) {
					GREM_ASSERT(resource);
					auto taskArguments = getParallelReductionTaskArguments(chunk, *resource, taskContext.entities, taskContext.resources, taskArgumentTypes);
					[&taskArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> void {
						(+TaskFunction)(get<Indices>(std::move(taskArguments))...);
					}(std::make_index_sequence<sizeof...(TaskArgs)>{});
				}
			},
			[]<typename Resource, typename... TaskArgs, typename... ReductionArgs>(TaskContext& taskContext, Resource* output, const Resource* inputs,
				Task::ParallelCount parallelism, meta::TypeList<TaskArgs...> taskArgumentTypes, meta::TypeList<ReductionArgs...> reductionArgumentTypes) -> void {
				if (hasParallelReductionTaskArguments<Resource>(taskContext.resources, taskArgumentTypes, reductionArgumentTypes)) {
					GREM_ASSERT(output);
					GREM_ASSERT(inputs);
					Resource result = inputs[0];
					for (Task::ParallelIndex i = 1; i < parallelism; ++i) {
						auto reductionArguments = getParallelReductionFunctionArguments(result, inputs[i], taskContext.entities, taskContext.resources, reductionArgumentTypes);
						result = [&reductionArguments]<std::size_t... Indices>(std::index_sequence<Indices...>) -> Resource {
							return (+ReductionFunction)(get<Indices>(std::move(reductionArguments))...);
						}(std::make_index_sequence<2 + sizeof...(ReductionArgs)>{});
					}
					*output = result;
				}
			}>(parallelism, std::move(name));
	}

	/**
	 * Append a full synchronization barrier to the task list.
	 * 
	 * This causes all subsequent tasks added to the task list to
	 * unconditionally synchronize with all tasks that were added before the
	 * barrier, preventing them from running in parallel with those tasks.
	 *
	 * This may be useful for synchronizing tasks that have global data
	 * dependencies, unknown to the scheduling system.
	 *
	 * \return `*this`, for chaining.
	 */
	Scheduler& addBarrier() {
		barrier = true;
		return *this;
	}

	/**
	 * Consume the current task list, calculate the dependencies between the
	 * tasks and schedule the final execution graph.
	 *
	 * \param name optional UTF-8-encoded human-readable name of the schedule.
	 *
	 * \return the final task schedule built by the scheduler.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function resets the scheduler to an empty state.
	 */
	[[nodiscard]] Schedule<EntReg, ResReg> buildSchedule(String name = {}) {
		Schedule<EntReg, ResReg> result{
			detail::buildSchedule(tasks, componentAccesses, resourceAccesses, entityRegistryAccesses, resourceRegistryAccesses),
			requiredSharedMemorySize,
			std::move(name),
		};
		clear();
		return result;
	}

private:
	using TaskContext = typename Schedule<EntReg, ResReg>::TaskContext;

	template <typename Function, typename... OptionalArgs>
	static void executeOptionalTaskFunction(Function&& function, const bool& foundAll,
		Tuple<OptionalArgs...>&& arguments) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		static_assert(((resource<std::remove_cvref_t<detail::remove_optional_or_pointer_t<OptionalArgs>>> ||
						   is_exclude_v<std::remove_cvref_t<detail::remove_optional_or_pointer_t<OptionalArgs>>>) ||
						  ...),
			"This optional task would always be executed since it does not require any resources or exclusions. Add it as a non-optional task instead.");
		if (foundAll) {
			return [&]<typename F, std::size_t... Indices>(F&& f, std::index_sequence<Indices...>) -> decltype(auto) { // NOLINT(cppcoreguidelines-missing-std-forward)
				return std::forward<F>(f)(*get<Indices>(detail::moveIfNotPointer<OptionalArgs>(arguments))...);
			}(std::forward<Function>(function), std::make_index_sequence<sizeof...(OptionalArgs)>{});
		}
	}

	template <typename... Args>
	[[nodiscard]] static auto getTaskArguments(EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<decltype(getTaskArgument<Args>(entities, resources))...>(getTaskArgument<Args>(entities, resources)...);
	}

	template <typename Arg>
	[[nodiscard]] static decltype(auto) getTaskArgument(EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (is_entity_range_v<T>) {
			return getEntities(entities, entity_range_components_and_exclusions_t<T>{});
		} else if constexpr (is_component_pool_v<T>) {
			return entities.template getComponentPool<std::remove_const_t<typename T::component_type>>();
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else {
			auto* resource = resources.template findResource<T>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *resource;
		}
	}

	template <typename... Args>
	[[nodiscard]] static auto getOptionalTaskArguments(bool& foundAll, EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		return Tuple(getOptionalTaskArgument<Args>(foundAll, entities, resources)...);
	}

	template <typename Arg>
	[[nodiscard]] static auto getOptionalTaskArgument(bool& foundAll, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return &entities;
		} else if constexpr (same_as<T, ResReg>) {
			return &resources;
		} else if constexpr (is_entity_range_v<T>) {
			return Optional<T>{std::in_place, getEntities(entities, entity_range_components_and_exclusions_t<T>{})};
		} else if constexpr (is_component_pool_v<T>) {
			return Optional<T>{std::in_place, entities.template getComponentPool<std::remove_const_t<typename T::component_type>>()};
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				foundAll = false;
				return Optional<T>{};
			}
			return Optional<T>{std::in_place, T{}};
		} else {
			return resources.template findResource<T>();
		}
	}

	template <typename... Args>
	[[nodiscard]] static auto getParallelTaskArguments(Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, EntReg& entities, ResReg& resources,
		meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<decltype(getParallelTaskArgument<Args>(parallelIndex, parallelism, entities, resources))...>(
			getParallelTaskArgument<Args>(parallelIndex, parallelism, entities, resources)...);
	}

	template <typename Arg>
	[[nodiscard]] static decltype(auto) getParallelTaskArgument(Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (is_entity_range_v<T>) {
			return getEntities(entities, entity_range_components_and_exclusions_t<T>{});
		} else if constexpr (is_component_pool_v<T>) {
			return entities.template getComponentPool<std::remove_const_t<typename T::component_type>>();
		} else if constexpr (is_chunk_v<T>) {
			auto* resource = resources.template findResource<typename T::resource_type>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<typename T::resource_type> + "\"."};
			}
			return T{*resource, size_t{parallelIndex}, size_t{parallelism}};
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else {
			auto* resource = resources.template findResource<T>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *resource;
		}
	}

	template <typename... Args>
	[[nodiscard]] static auto getOptionalParallelTaskArguments(bool& foundAll, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, EntReg& entities,
		ResReg& resources, meta::TypeList<Args...>) {
		return Tuple(getOptionalParallelTaskArgument<Args>(foundAll, parallelIndex, parallelism, entities, resources)...);
	}

	template <typename Arg>
	[[nodiscard]] static auto getOptionalParallelTransformationTaskArgument(bool& foundAll, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, EntReg& entities,
		ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return &entities;
		} else if constexpr (same_as<T, ResReg>) {
			return &resources;
		} else if constexpr (is_entity_range_v<T>) {
			return Optional<T>{std::in_place, getEntities(entities, entity_range_components_and_exclusions_t<T>{})};
		} else if constexpr (is_component_pool_v<T>) {
			return Optional<T>{std::in_place, entities.template getComponentPool<std::remove_const_t<typename T::component_type>>()};
		} else if constexpr (is_chunk_v<T>) {
			auto* resource = resources.template findResource<typename T::resource_type>();
			if (!resource) {
				foundAll = false;
				return Optional<T>{};
			}
			return Optional<T>{std::in_place, *resource, size_t{parallelIndex}, size_t{parallelism}};
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				foundAll = false;
				return Optional<T>{};
			}
			return Optional<T>{std::in_place, T{}};
		} else {
			return resources.template findResource<T>();
		}
	}

	template <typename EntityRange, typename... Args>
	[[nodiscard]] static auto getParallelTransformationTaskArguments(const EntityRange& chunk, EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<decltype(getParallelTransformationTaskArgument<EntityRange, Args>(chunk, entities, resources))...>(
			getParallelTransformationTaskArgument<EntityRange, Args>(chunk, entities, resources)...);
	}

	template <typename EntityRange, typename Arg>
	[[nodiscard]] static decltype(auto) getParallelTransformationTaskArgument(const EntityRange& chunk, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (same_as<T, EntityRange>) {
			return chunk;
		} else if constexpr (is_entity_range_v<T>) {
			static_assert(isConstEntityRange(entity_range_components_and_exclusions_t<T>{}),
				"Only the first entity range of a parallel transformation task may contain mutable components.");
			return getEntities(entities, entity_range_components_and_exclusions_t<T>{});
		} else if constexpr (is_component_pool_v<T>) {
			return entities.template getComponentPool<std::remove_const_t<typename T::component_type>>();
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else {
			auto* resource = resources.template findResource<T>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *resource;
		}
	}

	template <typename EntityRange, typename... Args>
	[[nodiscard]] static auto getOptionalParallelTransformationTaskArguments(bool& foundAll, const EntityRange& chunk, EntReg& entities, ResReg& resources,
		meta::TypeList<Args...>) {
		return Tuple(getOptionalParallelTransformationTaskArgument<EntityRange, Args>(foundAll, chunk, entities, resources)...);
	}

	template <typename EntityRange, typename Arg>
	[[nodiscard]] static auto getOptionalParallelTransformationTaskArgument(bool& foundAll, const EntityRange& chunk, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return &entities;
		} else if constexpr (same_as<T, ResReg>) {
			return &resources;
		} else if constexpr (same_as<T, EntityRange>) {
			return Optional<T>{std::in_place, chunk};
		} else if constexpr (is_entity_range_v<T>) {
			static_assert(isConstEntityRange(entity_range_components_and_exclusions_t<T>{}),
				"Only the first entity range of a parallel transformation task may contain mutable components.");
			return Optional<T>{std::in_place, getEntities(entities, entity_range_components_and_exclusions_t<T>{})};
		} else if constexpr (is_component_pool_v<T>) {
			return Optional<T>{std::in_place, entities.template getComponentPool<std::remove_const_t<typename T::component_type>>()};
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				foundAll = false;
				return Optional<T>{};
			}
			return Optional<T>{std::in_place, T{}};
		} else {
			return resources.template findResource<T>();
		}
	}

	template <typename EntityRange, typename... Args>
	[[nodiscard]] static auto getUnsafeParallelTransformationTaskArguments(const EntityRange& chunk, EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<decltype(getUnsafeParallelTransformationTaskArgument<EntityRange, Args>(chunk, entities, resources))...>(
			getUnsafeParallelTransformationTaskArgument<EntityRange, Args>(chunk, entities, resources)...);
	}

	template <typename EntityRange, typename Arg>
	[[nodiscard]] static decltype(auto) getUnsafeParallelTransformationTaskArgument(const EntityRange& chunk, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (same_as<T, EntityRange>) {
			return chunk;
		} else if constexpr (is_entity_range_v<T>) {
			return getEntities(entities, entity_range_components_and_exclusions_t<T>{});
		} else if constexpr (is_component_pool_v<T>) {
			return entities.template getComponentPool<std::remove_const_t<typename T::component_type>>();
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else {
			auto* resource = resources.template findResource<T>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *resource;
		}
	}

	template <typename EntityRange, typename... Args>
	[[nodiscard]] static auto getOptionalUnsafeParallelTransformationTaskArguments(bool& foundAll, const EntityRange& chunk, EntReg& entities, ResReg& resources,
		meta::TypeList<Args...>) {
		return Tuple(getOptionalUnsafeParallelTransformationTaskArgument<EntityRange, Args>(foundAll, chunk, entities, resources)...);
	}

	template <typename EntityRange, typename Arg>
	[[nodiscard]] static auto getOptionalUnsafeParallelTransformationTaskArgument(bool& foundAll, const EntityRange& chunk, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return &entities;
		} else if constexpr (same_as<T, ResReg>) {
			return &resources;
		} else if constexpr (same_as<T, EntityRange>) {
			return Optional<T>{std::in_place, chunk};
		} else if constexpr (is_entity_range_v<T>) {
			return Optional<T>{std::in_place, getEntities(entities, entity_range_components_and_exclusions_t<T>{})};
		} else if constexpr (is_component_pool_v<T>) {
			return Optional<T>{std::in_place, entities.template getComponentPool<std::remove_const_t<typename T::component_type>>()};
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				foundAll = false;
				return Optional<T>{};
			}
			return Optional<T>{std::in_place, T{}};
		} else {
			return resources.template findResource<T>();
		}
	}

	template <typename Resource, typename... TaskArgs, typename... ReductionFunctionArgs>
	[[nodiscard]] static auto hasParallelReductionTaskArguments(ResReg& resources, meta::TypeList<TaskArgs...>, meta::TypeList<ReductionFunctionArgs...>) {
		return (hasParallelReductionTaskArgument<Resource, TaskArgs>(resources) && ...) && (hasParallelReductionFunctionArgument<ReductionFunctionArgs>(resources) && ...);
	}

	template <typename Resource, typename Arg>
	[[nodiscard]] static bool hasParallelReductionTaskArgument(ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return true;
		} else if constexpr (same_as<T, ResReg>) {
			return true;
		} else if constexpr (is_entity_range_v<T>) {
			return true;
		} else if constexpr (is_component_pool_v<T>) {
			return true;
		} else if constexpr (is_exclude_v<T>) {
			return !resources.template hasResource<T>();
		} else if constexpr (same_as<T, Resource>) {
			return resources.template hasResource<Resource>();
		} else {
			return resources.template hasResource<T>();
		}
	}

	template <typename Arg>
	[[nodiscard]] static bool hasParallelReductionFunctionArgument(ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return true;
		} else if constexpr (same_as<T, ResReg>) {
			return true;
		} else if constexpr (is_exclude_v<T>) {
			return !resources.template hasResource<T>();
		} else {
			return resources.template hasResource<T>();
		}
	}

	template <typename EntityRange, typename Resource, typename... Args>
	[[nodiscard]] static auto getParallelReductionTaskArguments(const EntityRange& chunk, Resource& resource, EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<decltype(getParallelReductionTaskArgument<EntityRange, Resource, Args>(chunk, resource, entities, resources))...>(
			getParallelReductionTaskArgument<EntityRange, Resource, Args>(chunk, resource, entities, resources)...);
	}

	template <typename EntityRange, typename Resource, typename Arg>
	[[nodiscard]] static decltype(auto) getParallelReductionTaskArgument(const EntityRange& chunk, Resource& resource, EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (same_as<T, EntityRange>) {
			return chunk;
		} else if constexpr (is_entity_range_v<T>) {
			return getEntities(entities, entity_range_components_and_exclusions_t<T>{});
		} else if constexpr (is_component_pool_v<T>) {
			return entities.template getComponentPool<std::remove_const_t<typename T::component_type>>();
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else if constexpr (same_as<T, Resource>) {
			return resource;
		} else {
			auto* otherResource = resources.template findResource<T>();
			if (!otherResource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *otherResource;
		}
	}

	template <typename Resource, typename... Args>
	[[nodiscard]] static auto getParallelReductionFunctionArguments(const Resource& a, const Resource& b, EntReg& entities, ResReg& resources, meta::TypeList<Args...>) {
		// Note: Don't use forward_as_tuple, since we need values to remain values rather than rvalue references.
		return Tuple<const Resource&, const Resource&, decltype(getParallelReductionFunctionArgument<Args>(entities, resources))...>(a, b,
			getParallelReductionFunctionArgument<Args>(entities, resources)...);
	}

	template <typename Arg>
	[[nodiscard]] static decltype(auto) getParallelReductionFunctionArgument(EntReg& entities, ResReg& resources) {
		using T = std::remove_cvref_t<Arg>;
		if constexpr (same_as<T, EntReg>) {
			return entities;
		} else if constexpr (same_as<T, ResReg>) {
			return resources;
		} else if constexpr (is_exclude_v<T>) {
			if (resources.template hasResource<T>()) {
				throw execution::Error{"Found excluded resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return T{};
		} else {
			auto* resource = resources.template findResource<T>();
			if (!resource) {
				throw execution::Error{"Missing required resource \"" + meta::unqualified_type_name_v<T> + "\"."};
			}
			return *resource;
		}
	}

	template <typename Component, typename... ComponentsAndExclusions>
	[[nodiscard]] static consteval bool hasComponentAccess(meta::TypeList<ComponentsAndExclusions...>) noexcept {
		return (same_as<Component, std::remove_const_t<remove_exclude_t<ComponentsAndExclusions>>> || ...);
	}

	template <typename Component, typename... EntityRanges>
	[[nodiscard]] static consteval bool anyHasComponentAccess(meta::TypeList<EntityRanges...>) noexcept {
		return (hasComponentAccess<Component>(entity_range_components_and_exclusions_t<EntityRanges>{}) || ...);
	}

	template <typename... EntityRanges, typename... ComponentPools>
	[[nodiscard]] static consteval bool hasMutableComponentPoolOverlap(meta::TypeList<EntityRanges...>, meta::TypeList<ComponentPools...>) noexcept {
		return ((!std::is_const_v<typename ComponentPools::component_type> && anyHasComponentAccess<typename ComponentPools::component_type>(meta::TYPE_LIST<EntityRanges...>)) ||
				...);
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] static consteval bool isConstEntityRange(meta::TypeList<ComponentsAndExclusions...>) noexcept {
		return ((is_exclude_v<std::remove_const_t<ComponentsAndExclusions>> || std::is_const_v<ComponentsAndExclusions>) && ...);
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] static auto getEntities(EntReg& entities, meta::TypeList<ComponentsAndExclusions...>) {
		return entities.template getEntities<ComponentsAndExclusions...>();
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] static auto getEntitiesChunk(EntReg& entities, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism, meta::TypeList<ComponentsAndExclusions...>) {
		return entities.template getEntitiesChunk<ComponentsAndExclusions...>(size_t{parallelIndex}, size_t{parallelism});
	}

	template <detail::AccessFlags Flags, typename... MutableComponents, typename... ImmutableComponents, typename... MutableResources, typename... ImmutableResources>
	void addAccesses(meta::TypeList<MutableComponents...>, meta::TypeList<ImmutableComponents...>, meta::TypeList<MutableResources...>, meta::TypeList<ImmutableResources...>) {
		if constexpr ((Flags & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) != 0) {
			addEntityRegistryAccess<Flags | detail::ACCESS_MUTABLE>();
		} else if (barrier) {
			addEntityRegistryAccess<Flags | detail::ACCESS_MUTABLE>();
		} else if constexpr ((Flags & detail::ACCESS_IMMUTABLE_ENTITY_REGISTRY) != 0) {
			addEntityRegistryAccess<Flags>();
			for (auto&& [typeIndex, accesses] : componentAccesses) {
				addAccess<Flags>(accesses);
			}
		} else if constexpr (sizeof...(MutableComponents) > 0 || sizeof...(ImmutableComponents) > 0) {
			addEntityRegistryAccess<Flags>();
		}
		(addComponentAccess<Flags | detail::ACCESS_MUTABLE, MutableComponents>(), ...);
		(addComponentAccess<Flags, ImmutableComponents>(), ...);

		if constexpr ((Flags & detail::ACCESS_MUTABLE_RESOURCE_REGISTRY) != 0) {
			addResourceRegistryAccess<Flags | detail::ACCESS_MUTABLE>();
		} else if (barrier) {
			addResourceRegistryAccess<Flags | detail::ACCESS_MUTABLE>();
		} else if constexpr ((Flags & detail::ACCESS_IMMUTABLE_RESOURCE_REGISTRY) != 0) {
			addResourceRegistryAccess<Flags>();
			for (auto&& [typeIndex, accesses] : resourceAccesses) {
				addAccess<Flags>(accesses);
			}
		} else if (sizeof...(MutableResources) > 0 || sizeof...(ImmutableResources) > 0) {
			addResourceRegistryAccess<Flags>();
		}
		(addResourceAccess<Flags | detail::ACCESS_MUTABLE, MutableResources>(), ...);
		(addResourceAccess<Flags, ImmutableResources>(), ...);

		barrier = false;
	}

	template <detail::AccessFlags Flags>
	void addAccess(Buffer<detail::Accessor>& accesses) {
		accesses.push_back(detail::Accessor{.taskIndex = static_cast<Task::GraphIndex>(tasks.size()), .accessFlags = Flags});
	}

	template <detail::AccessFlags Flags, component T>
	void addComponentAccess() {
		addAccess<Flags>(componentAccesses[typeid(T)]);
	}

	template <detail::AccessFlags Flags, resource T>
	void addResourceAccess() {
		addAccess<Flags>(resourceAccesses[typeid(T)]);
	}

	template <detail::AccessFlags Flags>
	void addEntityRegistryAccess() {
		addAccess<Flags>(entityRegistryAccesses);
	}

	template <detail::AccessFlags Flags>
	void addResourceRegistryAccess() {
		addAccess<Flags>(resourceRegistryAccesses);
	}

	template <auto TaskFunction, auto Execute>
	[[nodiscard]] Scheduler& addTaskImplementation(String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> && meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Non-parallel tasks must not access resource chunks.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) == 0 ||
						  (meta::type_list_empty_v<typename AccessTraits::EntityRanges> && meta::type_list_empty_v<typename AccessTraits::ComponentPools>),
			"Tasks that have mutable access to the entire entity registry must not access any entity ranges or component pools seperately.");
		static_assert(!hasMutableComponentPoolOverlap(typename AccessTraits::EntityRanges{}, typename AccessTraits::ComponentPools{}),
			"Tasks that have mutable access to component pools must not also access those components separately.");

		if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		addAccesses<AccessTraits::FLAGS>(                 //
			typename AccessTraits::MutableComponents{},   //
			typename AccessTraits::ImmutableComponents{}, //
			typename AccessTraits::MutableResources{},    //
			typename AccessTraits::ImmutableResources{});
		tasks.push_back(detail::UnscheduledTask{
			.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
				Execute(*static_cast<TaskContext*>(context), typename AccessTraits::Arguments{});
			},
			.sharedMemoryOffset = requiredSharedMemorySize,
			.parallelism = 1,
			.name = std::move(name),
		});
		return *this;
	}

	template <auto TaskFunction, auto Execute>
	[[nodiscard]] Scheduler& addParallelTaskImplementation(Task::ParallelCount parallelism, String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(!meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> || !meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Parallel tasks must access at least one resource chunk.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::ComponentPools>, "Parallel tasks must not access component pools.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableComponents>, "Parallel tasks must not mutate entity components.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResources>, "Parallel tasks must not mutate shared resources.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) == 0, "Parallel tasks must not have mutable access to the entire entity registry.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_RESOURCE_REGISTRY) == 0, "Parallel tasks must not have mutable access to the entire resource registry.");

		if (parallelism <= 1) {
			if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
				throw std::length_error{"Maximum task count exceeded."};
			}

			addAccesses<AccessTraits::FLAGS>(                                                                                      //
				typename AccessTraits::MutableComponents{},                                                                        //
				typename AccessTraits::ImmutableComponents{},                                                                      //
				meta::type_list_concat_t<typename AccessTraits::MutableResources, typename AccessTraits::MutableResourceChunks>{}, //
				meta::type_list_concat_t<typename AccessTraits::ImmutableResources, typename AccessTraits::ImmutableResourceChunks>{});
			tasks.push_back(detail::UnscheduledTask{
				.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
					TaskContext& taskContext = *static_cast<TaskContext*>(context);
					Execute(taskContext, 0, 1, typename AccessTraits::Arguments{});
				},
				.sharedMemoryOffset = requiredSharedMemorySize,
				.parallelism = 1,
				.name = std::move(name),
			});
			return *this;
		}

		if (tasks.size() > Task::MAX_GRAPH_SIZE - size_t{parallelism}) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		const Task::Function function = [](void* context, byte*, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
			TaskContext& taskContext = *static_cast<TaskContext*>(context);
			Execute(taskContext, parallelIndex, parallelism, typename AccessTraits::Arguments{});
		};

		addAccesses<AccessTraits::FLAGS>(                                                                                      //
			typename AccessTraits::MutableComponents{},                                                                        //
			typename AccessTraits::ImmutableComponents{},                                                                      //
			meta::type_list_concat_t<typename AccessTraits::MutableResources, typename AccessTraits::MutableResourceChunks>{}, //
			meta::type_list_concat_t<typename AccessTraits::ImmutableResources, typename AccessTraits::ImmutableResourceChunks>{});
		tasks.push_back(detail::UnscheduledTask{
			.function = function,
			.sharedMemoryOffset = requiredSharedMemorySize,
			.parallelism = parallelism,
			.name = std::move(name),
		});
		return *this;
	}

	template <auto TaskFunction, auto Execute>
	[[nodiscard]] Scheduler& addUnsafeParallelTaskImplementation(Task::ParallelCount parallelism, String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(!meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> || !meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Parallel tasks must access at least one resource chunk.");

		if (parallelism <= 1) {
			if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
				throw std::length_error{"Maximum task count exceeded."};
			}

			addAccesses<AccessTraits::FLAGS>(                                                                                      //
				typename AccessTraits::MutableComponents{},                                                                        //
				typename AccessTraits::ImmutableComponents{},                                                                      //
				meta::type_list_concat_t<typename AccessTraits::MutableResources, typename AccessTraits::MutableResourceChunks>{}, //
				meta::type_list_concat_t<typename AccessTraits::ImmutableResources, typename AccessTraits::ImmutableResourceChunks>{});
			tasks.push_back(detail::UnscheduledTask{
				.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
					TaskContext& taskContext = *static_cast<TaskContext*>(context);
					Execute(taskContext, 0, 1, typename AccessTraits::Arguments{});
				},
				.sharedMemoryOffset = requiredSharedMemorySize,
				.parallelism = 1,
				.name = std::move(name),
			});
			return *this;
		}

		if (tasks.size() > Task::MAX_GRAPH_SIZE - size_t{parallelism}) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		const Task::Function function = [](void* context, byte*, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
			TaskContext& taskContext = *static_cast<TaskContext*>(context);
			Execute(taskContext, parallelIndex, parallelism, typename AccessTraits::Arguments{});
		};

		addAccesses<AccessTraits::FLAGS>(                                                                                      //
			typename AccessTraits::MutableComponents{},                                                                        //
			typename AccessTraits::ImmutableComponents{},                                                                      //
			meta::type_list_concat_t<typename AccessTraits::MutableResources, typename AccessTraits::MutableResourceChunks>{}, //
			meta::type_list_concat_t<typename AccessTraits::ImmutableResources, typename AccessTraits::ImmutableResourceChunks>{});
		tasks.push_back(detail::UnscheduledTask{
			.function = function,
			.sharedMemoryOffset = requiredSharedMemorySize,
			.parallelism = parallelism,
			.name = std::move(name),
		});
		return *this;
	}

	template <auto TaskFunction, auto Execute>
	[[nodiscard]] Scheduler& addParallelTransformationTaskImplementation(Task::ParallelCount parallelism, String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> && meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Parallel transformation tasks must not access resource chunks.");
		static_assert(meta::type_list_size_v<typename AccessTraits::EntityRanges> >= 1, "Parallel transformation tasks must operate on at least one entity range.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::ComponentPools>, "Parallel transformation tasks must not access any component pools directly.");
		static_assert(!meta::type_list_empty_v<typename AccessTraits::MutableComponents>, "Parallel transformation tasks must mutate at least one component.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResources>, "Parallel transformation tasks must not mutate shared resources.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) == 0,
			"Parallel transformation tasks must not have mutable access to the entire entity registry.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_RESOURCE_REGISTRY) == 0,
			"Parallel transformation tasks must not have mutable access to the entire resource registry.");
		static_assert(!hasMutableComponentPoolOverlap(typename AccessTraits::EntityRanges{}, typename AccessTraits::ComponentPools{}),
			"Tasks that have mutable access to component pools must not also access those components separately.");
		using EntityRange = meta::type_list_type_t<typename AccessTraits::EntityRanges, 0>;

		if (parallelism <= 1) {
			if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
				throw std::length_error{"Maximum task count exceeded."};
			}

			addAccesses<AccessTraits::FLAGS>(                 //
				typename AccessTraits::MutableComponents{},   //
				typename AccessTraits::ImmutableComponents{}, //
				typename AccessTraits::MutableResources{},    //
				typename AccessTraits::ImmutableResources{});
			tasks.push_back(detail::UnscheduledTask{
				.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
					TaskContext& taskContext = *static_cast<TaskContext*>(context);
					const EntityRange entityRange = getEntities(taskContext.entities, entity_range_components_and_exclusions_t<EntityRange>{});
					Execute(taskContext, entityRange, typename AccessTraits::Arguments{});
				},
				.sharedMemoryOffset = requiredSharedMemorySize,
				.parallelism = 1,
				.name = std::move(name),
			});
			return *this;
		}

		if (tasks.size() > Task::MAX_GRAPH_SIZE - size_t{parallelism}) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		const Task::Function function = [](void* context, byte*, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
			TaskContext& taskContext = *static_cast<TaskContext*>(context);
			const EntityRange chunk = getEntitiesChunk(taskContext.entities, parallelIndex, parallelism, entity_range_components_and_exclusions_t<EntityRange>{});
			Execute(taskContext, chunk, typename AccessTraits::Arguments{});
		};

		addAccesses<AccessTraits::FLAGS>(                 //
			typename AccessTraits::MutableComponents{},   //
			typename AccessTraits::ImmutableComponents{}, //
			typename AccessTraits::MutableResources{},    //
			typename AccessTraits::ImmutableResources{});
		tasks.push_back(detail::UnscheduledTask{
			.function = function,
			.sharedMemoryOffset = requiredSharedMemorySize,
			.parallelism = parallelism,
			.name = std::move(name),
		});
		return *this;
	}

	template <auto TaskFunction, auto Execute>
	[[nodiscard]] Scheduler& addUnsafeParallelTransformationTaskImplementation(Task::ParallelCount parallelism, String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> && meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Parallel transformation tasks must not access resource chunks.");
		static_assert(meta::type_list_size_v<typename AccessTraits::EntityRanges> >= 1, "Parallel transformation tasks must operate on at least one entity range.");
		using EntityRange = meta::type_list_type_t<typename AccessTraits::EntityRanges, 0>;

		if (parallelism <= 1) {
			if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
				throw std::length_error{"Maximum task count exceeded."};
			}

			addAccesses<AccessTraits::FLAGS>(                 //
				typename AccessTraits::MutableComponents{},   //
				typename AccessTraits::ImmutableComponents{}, //
				typename AccessTraits::MutableResources{},    //
				typename AccessTraits::ImmutableResources{});
			tasks.push_back(detail::UnscheduledTask{
				.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
					TaskContext& taskContext = *static_cast<TaskContext*>(context);
					const EntityRange entityRange = getEntities(taskContext.entities, entity_range_components_and_exclusions_t<EntityRange>{});
					Execute(taskContext, entityRange, typename AccessTraits::Arguments{});
				},
				.sharedMemoryOffset = requiredSharedMemorySize,
				.parallelism = 1,
				.name = std::move(name),
			});
			return *this;
		}

		if (tasks.size() > Task::MAX_GRAPH_SIZE - size_t{parallelism}) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		const Task::Function function = [](void* context, byte*, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
			TaskContext& taskContext = *static_cast<TaskContext*>(context);
			const EntityRange chunk = getEntitiesChunk(taskContext.entities, parallelIndex, parallelism, entity_range_components_and_exclusions_t<EntityRange>{});
			Execute(taskContext, chunk, typename AccessTraits::Arguments{});
		};

		addAccesses<AccessTraits::FLAGS>(                 //
			typename AccessTraits::MutableComponents{},   //
			typename AccessTraits::ImmutableComponents{}, //
			typename AccessTraits::MutableResources{},    //
			typename AccessTraits::ImmutableResources{});
		tasks.push_back(detail::UnscheduledTask{
			.function = function,
			.sharedMemoryOffset = requiredSharedMemorySize,
			.parallelism = parallelism,
			.name = std::move(name),
		});
		return *this;
	}

	template <auto TaskFunction, auto ReductionFunction, auto ExecuteTask, auto ExecuteReduction>
	[[nodiscard]] Scheduler& addParallelReductionTaskImplementation(Task::ParallelCount parallelism, String name) {
		using AccessTraits = decltype(detail::getTaskFunctionAccessTraits<EntReg, ResReg>(TaskFunction));
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableResourceChunks> && meta::type_list_empty_v<typename AccessTraits::ImmutableResourceChunks>,
			"Parallel reduction tasks must not access resource chunks.");
		static_assert(meta::type_list_size_v<typename AccessTraits::EntityRanges> == 1, "Parallel reduction tasks must operate on exactly one entity range.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::ComponentPools>, "Parallel reduction tasks must not access any component pools directly.");
		static_assert(meta::type_list_empty_v<typename AccessTraits::MutableComponents>, "Parallel reduction tasks must not mutate entity components.");
		static_assert(meta::type_list_size_v<typename AccessTraits::MutableResources> == 1, "Parallel reduction tasks must mutate exactly one resource.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) == 0, "Parallel reduction tasks must not have mutable access to the entire entity registry.");
		static_assert((AccessTraits::FLAGS & detail::ACCESS_MUTABLE_RESOURCE_REGISTRY) == 0,
			"Parallel reduction tasks must not have mutable access to the entire resource registry.");
		using EntityRange = meta::type_list_type_t<typename AccessTraits::EntityRanges, 0>;
		using Resource = meta::type_list_type_t<typename AccessTraits::MutableResources, 0>;
		static_assert(trivially_copyable<Resource>, "Parallel reduction resource must be trivially copyable.");

		using ReductionFunctionAccessTraits = decltype(detail::getReductionFunctionAccessTraits<EntReg, ResReg>(ReductionFunction));
		static_assert(meta::type_list_empty_v<typename ReductionFunctionAccessTraits::ComponentPools>, "Parallel reduction functions must not access any component pools.");
		static_assert(meta::type_list_empty_v<typename ReductionFunctionAccessTraits::EntityRanges>, "Parallel reduction functions must not access any entity ranges.");
		static_assert((ReductionFunctionAccessTraits::FLAGS & detail::ACCESS_MUTABLE_ENTITY_REGISTRY) == 0,
			"Parallel reduction functions must not have mutable access to the entire entity registry.");
		static_assert((ReductionFunctionAccessTraits::FLAGS & detail::ACCESS_MUTABLE_RESOURCE_REGISTRY) == 0,
			"Parallel reduction functions must not have mutable access to the entire resource registry.");
		static_assert(same_as<typename ReductionFunctionAccessTraits::Resource, Resource>,
			"Parallel reduction functions must operate on the same resource as their corresponding task function.");

		if (parallelism <= 1) {
			if (tasks.size() >= Task::MAX_GRAPH_SIZE) {
				throw std::length_error{"Maximum task count exceeded."};
			}

			addAccesses<AccessTraits::FLAGS>(                 //
				typename AccessTraits::MutableComponents{},   //
				typename AccessTraits::ImmutableComponents{}, //
				typename AccessTraits::MutableResources{},    //
				typename AccessTraits::ImmutableResources{});
			tasks.push_back(detail::UnscheduledTask{
				.function = [](void* context, byte*, Task::ParallelIndex, Task::ParallelCount) -> void {
					TaskContext& taskContext = *static_cast<TaskContext*>(context);
					const EntityRange entityRange = getEntities(taskContext.entities, entity_range_components_and_exclusions_t<EntityRange>{});
					ExecuteTask(taskContext, entityRange, taskContext.resources.template findResource<Resource>(), typename AccessTraits::Arguments{},
						typename ReductionFunctionAccessTraits::Arguments{});
				},
				.sharedMemoryOffset = requiredSharedMemorySize,
				.parallelism = 1,
				.name = std::move(name),
			});
			return *this;
		}

		if (tasks.size() > Task::MAX_GRAPH_SIZE - (size_t{parallelism} + 1)) {
			throw std::length_error{"Maximum task count exceeded."};
		}

		static constexpr size_t SHARED_MEMORY_INTERMEDIATE_RESOURCES_OFFSET = max(sizeof(Task::ParallelCount), alignof(Resource));
		const size_t taskRequiredSharedMemorySize = SHARED_MEMORY_INTERMEDIATE_RESOURCES_OFFSET + sizeof(Resource) * size_t{parallelism};
		if (taskRequiredSharedMemorySize > static_cast<size_t>(Limits<Task::SharedMemorySize>::MAX) ||
			static_cast<size_t>(requiredSharedMemorySize) > static_cast<size_t>(Limits<Task::SharedMemorySize>::MAX) - taskRequiredSharedMemorySize) {
			throw std::length_error{"Maximum shared memory size exceeded."};
		}
		const Task::SharedMemoryOffset sharedMemoryOffset = requiredSharedMemorySize;
		requiredSharedMemorySize += static_cast<Task::SharedMemorySize>(taskRequiredSharedMemorySize);

		const Task::Function function = [](void* context, byte* taskSharedMemory, Task::ParallelIndex parallelIndex, Task::ParallelCount parallelism) -> void {
			TaskContext& taskContext = *static_cast<TaskContext*>(context);
			const EntityRange chunk = getEntitiesChunk(taskContext.entities, parallelIndex, parallelism, entity_range_components_and_exclusions_t<EntityRange>{});

			if (parallelIndex == 0) {
				memcpy(taskSharedMemory, &parallelism, sizeof(parallelism));
			}

			Resource* localResource = nullptr;
			if (const Resource* const resource = taskContext.resources.template findResource<Resource>()) {
				byte* const localResourceMemory = taskSharedMemory + SHARED_MEMORY_INTERMEDIATE_RESOURCES_OFFSET + parallelIndex * sizeof(Resource);
				memcpy(localResourceMemory, resource, sizeof(Resource));
				localResource = std::launder(reinterpret_cast<Resource*>(localResourceMemory));
			}
			ExecuteTask(taskContext, chunk, localResource, typename AccessTraits::Arguments{}, typename ReductionFunctionAccessTraits::Arguments{});
		};

		String reductionTaskName = (name.empty()) ? String{} : name + " (reduction)";
		addAccesses<AccessTraits::FLAGS>(                 //
			typename AccessTraits::MutableComponents{},   //
			typename AccessTraits::ImmutableComponents{}, //
			typename AccessTraits::MutableResources{},    //
			typename AccessTraits::ImmutableResources{});
		tasks.push_back(detail::UnscheduledTask{
			.function = function,
			.sharedMemoryOffset = sharedMemoryOffset,
			.parallelism = parallelism,
			.name = std::move(name),
		});

		addAccesses<ReductionFunctionAccessTraits::FLAGS>( //
			meta::TYPE_LIST<>,                             //
			meta::TYPE_LIST<>,                             //
			typename AccessTraits::MutableResources{},     //
			typename ReductionFunctionAccessTraits::ImmutableResources{});
		tasks.push_back(detail::UnscheduledTask{
			.function = [](void* context, byte* taskSharedMemory, Task::ParallelIndex, Task::ParallelCount) -> void {
				TaskContext& taskContext = *static_cast<TaskContext*>(context);
				Resource* const output = taskContext.resources.template findResource<Resource>();
				const Resource* const inputs = (output) ? std::launder(reinterpret_cast<Resource*>(taskSharedMemory + SHARED_MEMORY_INTERMEDIATE_RESOURCES_OFFSET)) : nullptr;
				const Task::ParallelCount parallelism = *std::launder(reinterpret_cast<Task::ParallelCount*>(taskSharedMemory));
				ExecuteReduction(taskContext, output, inputs, parallelism, typename AccessTraits::Arguments{}, typename ReductionFunctionAccessTraits::Arguments{});
			},
			.sharedMemoryOffset = sharedMemoryOffset,
			.parallelism = 1,
			.name = std::move(reductionTaskName),
		});
		return *this;
	}

	ArrayList<detail::UnscheduledTask> tasks{};
	HashMap<std::type_index, Buffer<detail::Accessor>> componentAccesses{};
	HashMap<std::type_index, Buffer<detail::Accessor>> resourceAccesses{};
	Buffer<detail::Accessor> entityRegistryAccesses{};
	Buffer<detail::Accessor> resourceRegistryAccesses{};
	Task::SharedMemorySize requiredSharedMemorySize = 0;
	bool barrier = false;
};

} // namespace grem::execution

#endif
