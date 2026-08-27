// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_COMPONENT_POOL_HPP
#define GREM_EXECUTION_COMPONENT_POOL_HPP

#include <GREM/build_config.hpp>

#include <type_traits> // std::false_type

namespace grem::execution {

template <typename T>
struct is_component_pool : std::false_type {};

template <typename T>
inline constexpr bool is_component_pool_v = is_component_pool<T>::value;

} // namespace grem::execution

#endif
