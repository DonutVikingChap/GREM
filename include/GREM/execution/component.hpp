// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_COMPONENT_HPP
#define GREM_EXECUTION_COMPONENT_HPP

#include <GREM/build_config.hpp>

#include <type_traits> // std::is_..._v

namespace grem::execution {

template <typename T>
concept component = !std::is_const_v<T> && !std::is_reference_v<T> && !std::is_array_v<T> && std::is_nothrow_move_assignable_v<T>;

} // namespace grem::execution

#endif
