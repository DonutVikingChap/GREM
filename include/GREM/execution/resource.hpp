// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_RESOURCE_HPP
#define GREM_EXECUTION_RESOURCE_HPP

#include <GREM/build_config.hpp>

#include <GREM/execution/component_pool.hpp>
#include <GREM/execution/entity_range.hpp>

#include <type_traits> // std::is_..._v

namespace grem::execution {

template <typename T>
concept resource = !std::is_const_v<T> && !std::is_reference_v<T> && !std::is_array_v<T> && !std::is_pointer_v<T> && !is_entity_range_v<T> && !is_component_pool_v<T>;

} // namespace grem::execution

#endif
