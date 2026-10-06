// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_RESOURCE_REGISTRY_HPP
#define GREM_EXECUTION_RESOURCE_REGISTRY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/execution/resource.hpp>

#include <stdexcept> // std::out_of_range, std::logic_error
#include <typeindex> // std::type_index
#include <typeinfo>  // IWYU pragma: keep // typeid
#include <utility>   // std::move, std::forward

namespace grem::execution {

/**
 * Dynamic container of singleton resource instances, available to tasks defined
 * through a Scheduler.
 *
 * This container supports extending the set of possible resource types
 * dynamically at runtime.
 *
 * \tparam KnownResourcesOrPointers compile-time-known set of possible resource
 *         types, or pointers to external resources, which are given dedicated
 *         slots in the resource registry to speed up access to them. Any
 *         unknown resource types that are not in this list will be stored in a
 *         dynamic associative container keyed by the resource's typeid, which
 *         is looked up at runtime, incurring some performance overhead.
 *
 * \sa ResourceTable
 */
template <typename... KnownResourcesOrPointers>
class ResourceRegistry : private Tuple<KnownResourcesOrPointers...> {
private:
	using KnownResourceTuple = Tuple<KnownResourcesOrPointers...>;
	using KnownResourceTypeList = meta::TypeList<KnownResourcesOrPointers...>;

public:
	static_assert(((resource<KnownResourcesOrPointers> || pointer<KnownResourcesOrPointers>) && ...));

	using KnownResourceTuple::Tuple;

	/** Destructor. */
	~ResourceRegistry() = default;

	/** Move constructor. */
	ResourceRegistry(ResourceRegistry&&) noexcept = default;

	/** Copy constructor. */
	ResourceRegistry(const ResourceRegistry& other) {
		*this = other;
	}

	/** Move assignment. */
	ResourceRegistry& operator=(ResourceRegistry&&) noexcept = default;

	/** Copy assignment. */
	ResourceRegistry& operator=(const ResourceRegistry& other) {
		if (this == &other) {
			return *this;
		}
		static_cast<KnownResourceTuple&>(*this) = static_cast<const KnownResourceTuple&>(other);
		try {
			erase_if(resources, [&](const auto& kv) -> bool { return !other.resources.contains(kv.first); });
			for (const auto& [typeIndex, otherResource] : other.resources) {
				const Deleter& otherDeleter = otherResource.get_deleter();
				const auto [it, inserted] = resources.try_emplace(typeIndex, nullptr, Deleter{otherDeleter.destroy, otherDeleter.copyAssign, otherDeleter.get});
				otherDeleter.copyAssign(it->second, otherResource.get());
			}
		} catch (...) {
			resources.clear();
			throw;
		}
		return *this;
	}

	/**
	 * Erase all resources from the registry.
	 *
	 * \note Each removed resource object is destroyed if it is owned by the
	 *       registry, or if it has shared ownership and removing it causes its
	 *       reference count to hit 0. Otherwise, e.g. if the removed resource
	 *       was added using addExternalResource(), the object is not destroyed.
	 */
	void clear() noexcept {
		meta::forEach(static_cast<KnownResourceTuple&>(*this), [&]<typename KnownResourceOrPointer>(KnownResourceOrPointer& resource) -> void {
			if constexpr (requires { resource.clear(); }) {
				resource.clear();
			} else {
				resource = KnownResourceOrPointer{};
			}
		});
		resources.clear();
	}

	/**
	 * Add an owned resource to the registry.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param args arguments to forward to the resource constructor.
	 *
	 * \return a reference to the newly created resource.
	 *
	 * \throws std::logic_error if the registry already contained a resource of
	 *         the specified type.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the resource constructor.
	 */
	template <resource T, typename... Args>
	T& addResource(Args&&... args) {
		static_assert(!meta::type_list_contains_v<KnownResourceTypeList, T> && !meta::type_list_contains_v<KnownResourceTypeList, T*>, "Cannot add known resource.");

		constexpr auto destroy = [](void* resource) noexcept -> void {
			if (resource) {
				delete static_cast<T*>(resource); // NOLINT(cppcoreguidelines-owning-memory)
			}
		};
		constexpr auto copyAssign = [](UniquePointer<void, Deleter>& output, void* input) -> void {
			if constexpr (!copyable<T>) {
				throw std::logic_error{("Attempted to copy uncopyable resource \"" + meta::unqualified_type_name_v<T> + "\".").c_str()};
			} else {
				if (output) {
					*static_cast<T*>(output.get()) = *static_cast<T*>(input);
				} else {
					output.reset(new T(*static_cast<T*>(input))); // NOLINT(cppcoreguidelines-owning-memory)
				}
			}
		};
		constexpr auto get = [](void* resource) noexcept -> void* {
			return static_cast<T*>(resource);
		};

		T* const resource = new T(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
		try {
			if (!resources.try_emplace(typeid(T), resource, Deleter{destroy, copyAssign, get}).second) {
				throw std::logic_error{("Resource \"" + meta::unqualified_type_name_v<T> + "\" was already added to the registry.").c_str()};
			}
		} catch (...) {
			delete resource; // NOLINT(cppcoreguidelines-owning-memory)
			throw;
		}
		return *resource;
	}

	/**
	 * Add an owned resource to the registry if it doesn't already have it.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param args arguments to forward to the resource constructor.
	 *
	 * \return a pointer to the newly created resource, or nullptr if the
	 *         registry already contains a resource of the specified type.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the resource constructor.
	 */
	template <resource T, typename... Args>
	T* addResourceIfMissing(Args&&... args) {
		if (hasResource<T>()) {
			return nullptr;
		}
		return &addResource<T>(std::forward<Args>(args)...);
	}

	/**
	 * Add a reference-counted resource to the registry, whose ownership is
	 * shared between copies of the registry.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param args arguments to forward to the resource constructor.
	 *
	 * \return a reference to the newly created resource.
	 *
	 * \throws std::logic_error if the registry already contained a resource of
	 *         the specified type.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the resource constructor.
	 */
	template <resource T, typename... Args>
	T& addSharedResource(Args&&... args) {
		static_assert(!meta::type_list_contains_v<KnownResourceTypeList, T> && !meta::type_list_contains_v<KnownResourceTypeList, T*>, "Cannot add known resource.");

		struct ControlBlock {
			T value;
			Atomic<size_t> referenceCount;
		};

		constexpr auto destroy = [](void* resource) noexcept -> void {
			if (resource) {
				ControlBlock* const controlBlock = static_cast<ControlBlock*>(resource);
				if (controlBlock->referenceCount.fetch_sub(1, MemoryOrder::ACQUIRE_RELEASE) == 1) {
					delete controlBlock; // NOLINT(cppcoreguidelines-owning-memory)
				}
			}
		};
		constexpr auto copyAssign = [](UniquePointer<void, Deleter>& output, void* input) -> void {
			if (output.get() != input) {
				static_cast<ControlBlock*>(input)->referenceCount.fetch_add(1, MemoryOrder::RELAXED);
				output.reset(input);
			}
		};
		constexpr auto get = [](void* resource) noexcept -> void* {
			return &static_cast<ControlBlock*>(resource)->value;
		};

		ControlBlock* const controlBlock = new // NOLINT(cppcoreguidelines-owning-memory)
			ControlBlock{.value = T(std::forward<Args>(args)...), .referenceCount = 1};
		try {
			if (!resources.try_emplace(typeid(T), controlBlock, Deleter{destroy, copyAssign, get}).second) {
				throw std::logic_error{("Resource \"" + meta::unqualified_type_name_v<T> + "\" was already added to the registry.").c_str()};
			}
		} catch (...) {
			delete controlBlock; // NOLINT(cppcoreguidelines-owning-memory)
			throw;
		}
		return controlBlock->value;
	}

	/**
	 * Add a reference-counted resource to the registry, whose ownership is
	 * shared between copies of the registry, if it doesn't already have it.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param args arguments to forward to the resource constructor.
	 *
	 * \return a pointer to the newly created resource, or nullptr if the
	 *         registry already contains a resource of the specified type.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the resource constructor.
	 */
	template <resource T, typename... Args>
	T* addSharedResourceIfMissing(Args&&... args) {
		if (hasResource<T>()) {
			return nullptr;
		}
		return &addSharedResource<T>(std::forward<Args>(args)...);
	}

	/**
	 * Add an unowned external resource reference to the registry.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param resource non-owning pointer to the resource to add to the
	 *        registry. Must not be nullptr.
	 *
	 * \return `*resource`.
	 *
	 * \throws std::logic_error if the registry already contained a resource of
	 *         the specified type.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <resource T>
	T& addExternalResource(T* resource) {
		static_assert(!meta::type_list_contains_v<KnownResourceTypeList, T> && !meta::type_list_contains_v<KnownResourceTypeList, T*>, "Cannot add known resource.");

		constexpr auto destroy = [](void*) noexcept -> void {
		};
		constexpr auto copyAssign = [](UniquePointer<void, Deleter>& output, void* input) -> void {
			output.reset(input);
		};
		constexpr auto get = [](void* resource) noexcept -> void* {
			return static_cast<T*>(resource);
		};

		GREM_ASSERT(resource);
		if (!resources.try_emplace(typeid(T), resource, Deleter{destroy, copyAssign, get}).second) {
			throw std::logic_error{("Resource \"" + meta::unqualified_type_name_v<T> + "\" was already added to the registry.").c_str()};
		}
		return *resource;
	}

	/**
	 * Add an unowned external resource reference to the registry if it doesn't
	 * already have it.
	 *
	 * \tparam T resource type to add.
	 *
	 * \param resource non-owning pointer to the resource to add to the
	 *        registry. Must not be nullptr.
	 *
	 * \return `resource`, or nullptr if the registry already contains a
	 *         resource of the specified type.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <resource T>
	T* addExternalResourceIfMissing(T* resource) {
		if (hasResource<T>()) {
			return nullptr;
		}
		return &addExternalResource<T>(resource);
	}

	/**
	 * Erase a resource from the registry.
	 *
	 * \tparam T resource type to remove.
	 *
	 * \return true if the registry contained a resource of the specified type,
	 *         false otherwise.
	 *
	 * \note The removed resource object is destroyed if it is owned by the
	 *       registry, or if it has shared ownership and removing it causes its
	 *       reference count to hit 0. Otherwise, e.g. if the removed resource
	 *       was added using addExternalResource(), the object is not destroyed.
	 */
	template <resource T>
	bool removeResource() noexcept {
		static_assert(!meta::type_list_contains_v<KnownResourceTypeList, T> && !meta::type_list_contains_v<KnownResourceTypeList, T*>, "Cannot remove known resource.");

		return resources.erase(typeid(T)) > 0;
	}

	/**
	 * Check if the registry contains a specific resource.
	 *
	 * \tparam T resource type to check for.
	 *
	 * \return true if the registry contains a resource of the specified type,
	 *         false otherwise.
	 */
	template <resource T>
	[[nodiscard]] bool hasResource() const noexcept {
		if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T> || meta::type_list_contains_v<KnownResourceTypeList, T*>) {
			return true;
		} else {
			return resources.contains(typeid(T));
		}
	}

	/**
	 * Get a specific resource in the registry.
	 *
	 * \tparam T resource type to get.
	 *
	 * \return a reference to the specified resource.
	 *
	 * \throws std::out_of_range if the registry does not contain a resource of
	 *         the specified type.
	 */
	template <resource T>
	[[nodiscard]] T& getResource() {
		if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T*>) {
			return *get<T*>(*static_cast<KnownResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T>) {
			return get<T>(*static_cast<KnownResourceTuple*>(this));
		} else {
			T* const result = findResource<T>();
			if (!result) {
				throw std::out_of_range{("Resource \"" + meta::unqualified_type_name_v<T> + "\" not found in registry.").c_str()};
			}
			return *result;
		}
	}

	/**
	 * Get a specific resource in the registry.
	 *
	 * \tparam T resource type to get.
	 *
	 * \return a read-only reference to the specified resource.
	 *
	 * \throws std::out_of_range if the registry does not contain a resource of
	 *         the specified type.
	 */
	template <resource T>
	[[nodiscard]] const T& getResource() const {
		if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T*>) {
			return *get<T*>(*static_cast<const KnownResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T>) {
			return get<T>(*static_cast<const KnownResourceTuple*>(this));
		} else {
			const T* const result = findResource<T>();
			if (!result) {
				throw std::out_of_range{("Resource \"" + meta::unqualified_type_name_v<T> + "\" not found in registry.").c_str()};
			}
			return *result;
		}
	}

	/**
	 * Try to get a specific resource in the registry.
	 *
	 * \tparam T resource type to get.
	 *
	 * \return a pointer to the specified resource, or nullptr if the registry
	 *         does not contain a resource of the specified type.
	 */
	template <resource T>
	[[nodiscard]] T* findResource() noexcept {
		if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T*>) {
			return get<T*>(*static_cast<KnownResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T>) {
			return &get<T>(*static_cast<KnownResourceTuple*>(this));
		} else {
			if (const auto it = resources.find(typeid(T)); it != resources.end()) {
				return static_cast<T*>(it->second.get_deleter().get(it->second.get()));
			}
			return nullptr;
		}
	}

	/**
	 * Try to get a specific resource in the registry.
	 *
	 * \tparam T resource type to get.
	 *
	 * \return a read-only pointer to the specified resource, or nullptr if the
	 *         registry does not contain a resource of the specified type.
	 */
	template <resource T>
	[[nodiscard]] const T* findResource() const noexcept {
		if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T*>) {
			return get<T*>(*static_cast<const KnownResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<KnownResourceTypeList, T>) {
			return &get<T>(*static_cast<const KnownResourceTuple*>(this));
		} else {
			if (const auto it = resources.find(typeid(T)); it != resources.end()) {
				return static_cast<T*>(it->second.get_deleter().get(it->second.get()));
			}
			return nullptr;
		}
	}

private:
	struct Deleter {
		using Destroy = void (*)(void* resource) noexcept;
		using CopyAssign = void (*)(UniquePointer<void, Deleter>& output, void* input);
		using Get = void* (*)(void* resource) noexcept;

		Destroy destroy;
		CopyAssign copyAssign;
		Get get;

		void operator()(void* resource) const noexcept {
			destroy(resource);
		}
	};

	HashMap<std::type_index, UniquePointer<void, Deleter>> resources{};
};

} // namespace grem::execution

#endif
