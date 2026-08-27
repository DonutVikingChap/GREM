// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SINGLY_LINKED_LIST_HPP
#define GREM_CORE_DATA_SINGLY_LINKED_LIST_HPP

#include <GREM/build_config.hpp>

#include <forward_list> // std::forward_list, std::allocator

namespace grem {

template <typename T, typename Allocator = std::allocator<T>>
using SinglyLinkedList = std::forward_list<T, Allocator>;

} // namespace grem

#endif
