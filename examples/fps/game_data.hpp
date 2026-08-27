// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_DATA_HPP
#define GREM_EXAMPLES_FPS_GAME_DATA_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/quantities.hpp>

enum class OrientationAlignment : uint8_t {
	NONE,
	ALIGN_WITH_CAMERA,
	ALIGN_Z_WITH_CAMERA_Z_AROUND_WORLD_Y,
	ALIGN_Z_WITH_CAMERA_Z_AROUND_LOCAL_Y,
};

struct WorldTransformation {
	phys::Position3D position{};
	phys::Orientation3D orientation{};
	phys::Scale3D scale{1_x};

	[[nodiscard]] phys::Position3D operator()(phys::Length3D localOffset) const noexcept {
		return position + orientation(scale * localOffset);
	}
};

#endif
