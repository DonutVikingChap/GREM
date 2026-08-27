// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_LOCALLY_TRANSFORMED_VS_COMPOUND_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_LOCALLY_TRANSFORMED_VS_COMPOUND_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

#include "CollisionDetector.hpp"
#include "CollisionDetector_any_vs_compound.hpp"

namespace grem::physics {

template <size_t N>
struct choose_collision_detector<N, LocallyTransformedShape<N>, CompoundColliderShape<N>> {
	using type = CollisionDetector_any_vs_compound<N>;
};

template <size_t N>
struct choose_collision_detector<N, CompoundColliderShape<N>, LocallyTransformedShape<N>> {
	using type = ReversedCollisionDetector<N, CollisionDetector_any_vs_compound<N>>;
};

} // namespace grem::physics

#endif
