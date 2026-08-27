// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VIEWPORT_HPP
#define GREM_GRAPHICS_VIEWPORT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Optional.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::graphics {

/**
 * Region of a render target.
 */
class Viewport {
public:
	/**
	 * Rectangular region of the viewport in the render target, in pixels.
	 */
	Region2D region{.size{1, 1}};

	/**
	 * Minimum depth value in the depth range of the viewport.
	 *
	 * Must be between 0 and 1 (inclusive).
	 */
	float minDepth = 0.0f;

	/**
	 * Maximum depth value in the depth range of the viewport.
	 *
	 * Must be between 0 and 1 (inclusive).
	 */
	float maxDepth = 1.0f;

	/**
	 * Optional rectangular scissor region, outside of which any attempts to
	 * render a pixel will be discarded.
	 */
	Optional<Region2D> scissor{};

	/**
	 * Compare this viewport to another for equality.
	 *
	 * \param other the viewport to compare this viewport to.
	 *
	 * \return true if the viewports are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Viewport& other) const = default;
};

} // namespace grem::graphics

#endif
