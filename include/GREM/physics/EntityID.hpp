// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_ENTITY_ID_HPP
#define GREM_PHYSICS_ENTITY_ID_HPP

#include <GREM/build_config.hpp>

#include <GREM/execution/EntityRegistry.hpp>

namespace grem::physics {

/**
 * Opaque handle to a specific entity in a Simulation's EntityRegistry.
 */
using EntityID = execution::EntityID;

} // namespace grem::physics

#endif
