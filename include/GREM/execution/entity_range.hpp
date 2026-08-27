// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_ENTITY_RANGE_HPP
#define GREM_EXECUTION_ENTITY_RANGE_HPP

#include <GREM/build_config.hpp>

#include <type_traits> // std::false_type, std::true_type

namespace grem::execution {

template <typename Component>
struct Exclude {};

template <typename T>
struct remove_exclude {
	using type = T;
};

template <typename Component>
struct remove_exclude<Exclude<Component>> {
	using type = Component;
};

template <typename T>
using remove_exclude_t = typename remove_exclude<T>::type;

template <typename T>
struct is_exclude : std::false_type {};

template <typename T>
struct is_exclude<Exclude<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_exclude_v = is_exclude<T>::value;

template <typename T>
struct is_entity_range : std::false_type {};

template <typename T>
inline constexpr bool is_entity_range_v = is_entity_range<T>::value;

template <typename EntityRange>
struct entity_range_components_and_exclusions;

template <typename EntityRange>
using entity_range_components_and_exclusions_t = typename entity_range_components_and_exclusions<EntityRange>::type;

} // namespace grem::execution

#endif
