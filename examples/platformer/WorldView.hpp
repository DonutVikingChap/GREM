// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_WORLD_VIEW_HPP
#define GREM_EXAMPLES_PLATFORMER_WORLD_VIEW_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct WorldView {
	Frustum<float> frustum{};
	Box<3, float> visibleBounds{};

	[[nodiscard]] bool isPotentiallyVisible(const phys::Box3D& box) const {
		return frustum.isPotentiallyIntersecting(box.in(phys::METERS));
	}

	[[nodiscard]] bool isPotentiallyVisible(const phys::Sphere3D& sphere) const {
		return frustum.isPotentiallyIntersecting(sphere.in(phys::METERS));
	}
};

#endif
