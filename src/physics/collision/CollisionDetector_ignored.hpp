// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_IGNORED_HPP
#define GREM_PHYSICS_COLLISION_COLLISION_DETECTOR_IGNORED_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/quantities.hpp>

#include "CollisionDetector.hpp"

namespace grem::physics {

// Collision detector that does nothing.
template <size_t N>
class CollisionDetector_ignored final : public CollisionAlgorithmImplementation<N> {
public:
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource*, Length1D, ColliderView<N>, const Transformation<N>&, ColliderView<N>, const Transformation<N>&,
		const CollisionAlgorithmOptions<N>&, CollisionFilterTest) override {
		GREM_PROFILE_FUNCTION();

		return {};
	}

	void detectCollisions(ArenaResource*, ColliderView<N>, const Transformation<N>&, ColliderView<N>, const Transformation<N>&, const CollisionAlgorithmOptions<N>&,
		CollisionFilterTest, FunctionView<void(const CollisionAlgorithmResult<N>&)>) override {
		GREM_PROFILE_FUNCTION();
	}
};

template <size_t N>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfiniteLineShape<N>, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, InfiniteLineShape<N>> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfinitePlaneShape3D, InfiniteHalfSpaceShape<N>> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfiniteHalfSpaceShape<N>, InfinitePlaneShape3D> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfinitePlaneShape3D, InfinitePlaneShape3D> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfiniteLineShape<N>, InfinitePlaneShape3D> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, InfinitePlaneShape3D, InfiniteLineShape<N>> {
	using type = CollisionDetector_ignored<N>;
};

template <size_t N>
struct choose_collision_detector<N, TriangleMeshShape<N>, TriangleMeshShape<N>> {
	// Note: This interaction (triangle mesh vs triangle mesh) is unsupported due to the high computational complexity of a straight-forward implementation given our current data structure.
	// This limitation is common among many mainstream physics engines, since triangle meshes are expected to be used mainly for static objects that do not interact with each other.
	// Concave triangle mesh shapes that need to be dynamic should be decomposed into convex hulls (or primitive convex shapes) and make use of CompoundColliderShape instead, which is also more robust and perfomant.
	using type = CollisionDetector_ignored<N>;
};

} // namespace grem::physics

#endif
