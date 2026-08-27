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

template <typename... ResourcesOrPointers>
class ResourceTable : private Tuple<ResourcesOrPointers...> {
private:
	using ResourceTuple = Tuple<ResourcesOrPointers...>;
	using ResourceTypeList = meta::TypeList<ResourcesOrPointers...>;

public:
	static_assert(((resource<ResourcesOrPointers> || pointer<ResourcesOrPointers>) && ...));

	using ResourceTuple::Tuple;

	template <resource T>
	[[nodiscard]] constexpr bool hasResource() const noexcept {
		return meta::type_list_contains_v<ResourceTypeList, T*> || meta::type_list_contains_v<ResourceTypeList, T>;
	}

	template <resource T>
	[[nodiscard]] constexpr T& getResource() {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return *get<T*>(*static_cast<ResourceTuple*>(this));
		} else {
			return get<T>(*static_cast<ResourceTuple*>(this));
		}
	}

	template <resource T>
	[[nodiscard]] constexpr const T& getResource() const {
		if constexpr (meta::type_list_contains_v<ResourceTypeList, T*>) {
			return *get<T*>(*static_cast<const ResourceTuple*>(this));
		} else {
			return get<T>(*static_cast<const ResourceTuple*>(this));
		}
	}

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
