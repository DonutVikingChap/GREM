// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_PRIORITY_QUEUE_HPP
#define GREM_CORE_DATA_PRIORITY_QUEUE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>

#include <functional> // std::less
#include <memory>     // std::allocator
#include <queue>      // std::priority_queue

namespace grem {

template <typename T, typename Container = ArrayList<T, std::allocator<T>>, typename Compare = std::less<typename Container::value_type>>
using PriorityQueue = std::priority_queue<T, Container, Compare>;

} // namespace grem

#endif
