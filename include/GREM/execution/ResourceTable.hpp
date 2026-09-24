// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_RESOURCE_TABLE_HPP
#define GREM_EXECUTION_RESOURCE_TABLE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/execution/resource.hpp>

namespace grem::execution {

/**
 * Container of a specific tuple of singleton #resource types, available to
 * tasks defined through a Scheduler.
 *
 * This container can be used as a simpler and more performant alternative to
 * ResourceRegistry when the full set of resource types is known at compile
 * time.
 *
 * \tparam ResourcesOrPointers #resource types, or pointers to external
 *         resources, to store in the table.
 *
 * \sa ResourceRegistry
 */
template <typename... ResourcesOrPointers>
class ResourceTable : private Tuple<ResourcesOrPointers...> {
private:
	using ResourceTuple = Tuple<ResourcesOrPointers...>;
	using ResourceTypeList = meta::TypeList<ResourcesOrPointers...>;

public:
	static_assert(((resource<ResourcesOrPointers> || pointer<ResourcesOrPointers>) && ...));

	using ResourceTuple::Tuple;

	/**
	 * Check if the table contains a specific resource.
	 *
	 * \tparam T #resource type to check for.
	 *
	 * \return true if the table contains a resource of the specified type,
	 *         false otherwise.
	 */
	template <resource T>
	[[nodiscard]] constexpr bool hasResource() const noexcept {
		return meta::type_list_contains_v<ResourceTypeList, T*> || meta::type_list_contains_v<ResourceTypeList, T>;
	}

	/**
	 * Get a specific resource in the table.
	 *
	 * \tparam T #resource type to get. Must be one of the resource types
	 *         contained in the table.
	 *
	 * \return a reference to the specified resource.
	 */
	template <resource T>
	[[nodiscard]] constexpr T& getResource() {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return *get<T*>(*static_cast<ResourceTuple*>(this));
		} else {
			static_assert(meta::type_list_contains_v<ResourceTypeList, T>, "ResourceTable does not contain the specified resource type.");
			return get<T>(*static_cast<ResourceTuple*>(this));
		}
	}

	/**
	 * Get a specific resource in the table.
	 *
	 * \tparam T #resource type to get. Must be one of the resource types
	 *         contained in the table.
	 *
	 * \return a read-only reference to the specified resource.
	 */
	template <resource T>
	[[nodiscard]] constexpr const T& getResource() const {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return *get<T*>(*static_cast<const ResourceTuple*>(this));
		} else {
			static_assert(meta::type_list_contains_v<ResourceTypeList, T>, "ResourceTable does not contain the specified resource type.");
			return get<T>(*static_cast<const ResourceTuple*>(this));
		}
	}

	/**
	 * Try to get a specific resource in the table.
	 *
	 * \tparam T #resource type to get.
	 *
	 * \return a pointer to the specified resource, or nullptr if the table does
	 *         not contain a resource of the specified type.
	 */
	template <resource T>
	[[nodiscard]] constexpr T* findResource() noexcept {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return get<T*>(*static_cast<ResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<ResourceTypeList, T>) {
			return &get<T>(*static_cast<ResourceTuple*>(this));
		} else {
			return nullptr;
		}
	}

	/**
	 * Try to get a specific resource in the table.
	 *
	 * \tparam T #resource type to get.
	 *
	 * \return a read-only pointer to the specified resource, or nullptr if the
	 *         table does not contain a resource of the specified type.
	 */
	template <resource T>
	[[nodiscard]] constexpr const T* findResource() const noexcept {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return get<T*>(*static_cast<const ResourceTuple*>(this));
		} else if constexpr (meta::type_list_contains_v<ResourceTypeList, T>) {
			return &get<T>(*static_cast<const ResourceTuple*>(this));
		} else {
			return nullptr;
		}
	}
};

} // namespace grem::execution

#endif
