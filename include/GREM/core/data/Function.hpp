// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_FUNCTION_HPP
#define GREM_CORE_DATA_FUNCTION_HPP

#include <GREM/build_config.hpp>

#include <functional> // std::function

namespace grem {

template <typename Signature>
using Function = std::function<Signature>;

} // namespace grem

#endif
