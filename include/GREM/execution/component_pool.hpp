// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_COMPONENT_POOL_HPP
#define GREM_EXECUTION_COMPONENT_POOL_HPP

#include <GREM/build_config.hpp>

#include <type_traits> // std::false_type

namespace grem::execution {

/// \cond
template <typename T>
struct is_component_pool : std::false_type {};
/// \endcond

/**
 * Boolean that evaluates to true if the given type is considered a component
 * pool reference type.
 *
 * \tparam T type to check.
 *
 * \note To define a custom type `T` as a component pool reference type,
 *       specialize `template <> struct grem::execution::is_component_pool<T>`
 *       as a struct derived from `std::true_type`. The component pool type `T`
 *       must have a nested type `component_type` that is the potentially
 *       const-qualified component type being stored.
 */
template <typename T>
inline constexpr bool is_component_pool_v = is_component_pool<T>::value;

} // namespace grem::execution

#endif
