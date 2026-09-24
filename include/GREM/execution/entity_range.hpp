// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_ENTITY_RANGE_HPP
#define GREM_EXECUTION_ENTITY_RANGE_HPP

#include <GREM/build_config.hpp>

#include <type_traits> // std::false_type, std::true_type

namespace grem::execution {

/**
 * Component exclusion tag type, for excluding entities from an entity range if
 * they have a specific component.
 *
 * \tparam Component component type that is to be excluded.
 */
template <typename Component>
struct Exclude {};

/// \cond
template <typename T>
struct remove_exclude {
	using type = T;
};

template <typename Component>
struct remove_exclude<Exclude<Component>> {
	using type = Component;
};
/// \endcond

/**
 * The excluded component type of a component exclusion type.
 *
 * If the given type is not a specialization of Exclude, the resulting type is
 * the given type itself, unmodified.
 *
 * \tparam T type to get the excluded component type of.
 */
template <typename T>
using remove_exclude_t = typename remove_exclude<T>::type;

/// \cond
template <typename T>
struct is_exclude : std::false_type {};

template <typename T>
struct is_exclude<Exclude<T>> : std::true_type {};
/// \endcond

/**
 * Boolean that evaluates to true if the given type is a specialization of
 * Exclude.
 *
 * \tparam T type to check.
 */
template <typename T>
inline constexpr bool is_exclude_v = is_exclude<T>::value;

/// \cond
template <typename T>
struct is_entity_range : std::false_type {};
/// \endcond

/**
 * Boolean that evaluates to true if the given type is considered an entity
 * range type.
 *
 * \tparam T type to check.
 *
 * \note To define a custom type `T` as an entity range type, specialize
 *       `template <> struct grem::execution::is_entity_range<T>` as a struct
 *       derived from `std::true_type`, and
 *       `template <> struct grem::execution::entity_range_components_and_exclusions<T>`
 *       as a struct with a nested type `type` that is a meta::TypeList of all
 *       the component and component exclusion types of the entity range.
 */
template <typename T>
inline constexpr bool is_entity_range_v = is_entity_range<T>::value;

/// \cond
template <typename EntityRange>
struct entity_range_components_and_exclusions;
/// \endcond

/**
 * Component and component exclusion types of an entity range type, provided as
 * a meta::TypeList.
 *
 * \tparam EntityRange entity range type to get the component and component
 *         exclusion types of. Must be a valid entity range type.
 *
 * \note To define a custom type `T` as an entity range type, specialize
 *       `template <> struct grem::execution::is_entity_range<T>` as a struct
 *       derived from `std::true_type`, and
 *       `template <> struct grem::execution::entity_range_components_and_exclusions<T>`
 *       as a struct with a nested type `type` that is a meta::TypeList of all
 *       the component and component exclusion types of the entity range.
 */
template <typename EntityRange>
using entity_range_components_and_exclusions_t = typename entity_range_components_and_exclusions<EntityRange>::type;

} // namespace grem::execution

#endif
