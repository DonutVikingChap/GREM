// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_GRAPHICS_HPP
#define GREM_EXAMPLES_TILES_GRAPHICS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

struct Graphics {
	Extent2D windowSize{};
	Extent2D windowDrawableSize{};
	Extent2D renderSize;
	Region2D worldViewportRenderRegion;
	gfx::Renderer2D renderer2D;
	gfx::Camera2D camera2D;
	gfx::Viewport viewport{};
	uint32_t viewportScale = 1;
	gfx::Font2D mainFont;
	gfx::Text2D temporaryText{};
	gfx::Instances2D instances2D;

	Graphics(const Filesystem& filesystem, gfx::Device& device, Extent2D renderSize, Region2D worldViewportRenderRegion)
		: renderSize(renderSize)
		, worldViewportRenderRegion(worldViewportRenderRegion)
		, renderer2D(device)
		, camera2D(device)
		, mainFont(filesystem, "fonts/unscii/unscii-8.ttf")
		, instances2D(device, renderer2D) {}

	void resize(Extent2D newWindowSize, Extent2D newWindowDrawableSize) {
		GREM_PROFILE_FUNCTION();

		windowSize = newWindowSize;
		windowDrawableSize = newWindowDrawableSize;
		viewport.region = {.size = renderSize};
		viewportScale = viewport.region.fitCenteredIntegerScaled({.size = windowDrawableSize});
		camera2D.setProjection(gfx::OrthographicProjection2D{.size = renderSize});
	}

	Box<2, float> put2DText(vec2 position, Color color, StringView string, float scale = 1.0f, gfx::TextAlign alignment = {}) {
		GREM_PROFILE_FUNCTION();

		temporaryText.assign(mainFont, 8, string, {0.0f, 0.0f}, vec2{scale});
		instances2D.putTextInstance(temporaryText, {.position = position + vec2{1.0f, -1.0f}, .alignment = alignment, .color = color * Color::fromLinear(0.2f)});
		instances2D.putTextInstance(temporaryText, {.position = position, .alignment = alignment, .color = color});
		const Box<2, float> boundingBox = temporaryText.getBoundingBox();
		const vec2 offset = position + temporaryText.getAlignmentOffset(alignment);
		return {.min = boundingBox.min + offset, .max = boundingBox.max + offset};
	}

	[[nodiscard]] vec2 convertScreenToRenderCoordinates(vec2 screenCoordinates) const {
		const vec2 screenCoordinatesFromBottomLeft{screenCoordinates.x, static_cast<float>(windowSize.height) - screenCoordinates.y};
		return ((screenCoordinatesFromBottomLeft - vec2{viewport.region.offset}) / vec2{viewport.region.size}) * vec2{renderSize};
	}
};

#endif
