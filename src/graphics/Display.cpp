// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Display.hpp>
#include <GREM/graphics/Error.hpp>

#include <SDL3/SDL.h> // SDL...

namespace grem::graphics {

int DisplayMode::getBitsPerPixel() const noexcept {
	return SDL_BITSPERPIXEL(format);
}

Display::DisplayRange Display::getAll() {
	int displayCount{};
	SDL_DisplayID* const displayIDs = SDL_GetDisplays(&displayCount);
	if (!displayIDs) {
		throw graphics::Error{String{"Failed to get video displays:\n"} + SDL_GetError()};
	}
	return DisplayRange{displayIDs, displayCount};
}

Display Display::getPrimary() {
	const SDL_DisplayID display = SDL_GetPrimaryDisplay();
	if (display == 0) {
		throw graphics::Error{String{"Failed to get the primary video display:\n"} + SDL_GetError()};
	}
	return Display{display};
}

Optional<Display> Display::findByID(uint32_t id) {
	int displayCount{};
	SDL_DisplayID* const displayIDs = SDL_GetDisplays(&displayCount);
	if (!displayIDs) {
		return {};
	}
	if (contains(Span{displayIDs, static_cast<size_t>(displayCount)}, SDL_DisplayID{id})) {
		SDL_free(displayIDs);
		return Display{id};
	}
	SDL_free(displayIDs);
	return {};
}

CStringView Display::getName() const {
	const char* name = SDL_GetDisplayName(displayID);
	if (!name) {
		throw graphics::Error{String{"Failed to get the name of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	return CStringView{name};
}

Region2D Display::getBounds() const {
	SDL_Rect rect;
	if (!SDL_GetDisplayBounds(displayID, &rect)) {
		throw graphics::Error{String{"Failed to get the bounds of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	return Region2D{
		.offset{static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y)},
		.size{static_cast<uint32_t>(rect.w), static_cast<uint32_t>(rect.h)},
	};
}

Region2D Display::getUsableBounds() const {
	SDL_Rect rect;
	if (!SDL_GetDisplayUsableBounds(displayID, &rect)) {
		throw graphics::Error{String{"Failed to get the usable bounds of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	return Region2D{
		.offset{static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y)},
		.size{static_cast<uint32_t>(rect.w), static_cast<uint32_t>(rect.h)},
	};
}

Display::Orientation Display::getOrientation() const noexcept {
	return static_cast<Orientation>(SDL_GetCurrentDisplayOrientation(displayID));
}

DisplayMode Display::getDisplayMode() const {
	const SDL_DisplayMode* const mode = SDL_GetCurrentDisplayMode(displayID);
	if (!mode) {
		throw graphics::Error{String{"Failed to get the display mode of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	const Extent2D size{.width = static_cast<uint32_t>(mode->w), .height = static_cast<uint32_t>(mode->h)};
	return createDisplayMode(mode->displayID, mode->format, size, mode->pixel_density, mode->refresh_rate, mode->refresh_rate_numerator, mode->refresh_rate_denominator,
		mode->internal);
}

float Display::getContentScale() const {
	const float contentScale = SDL_GetDisplayContentScale(displayID);
	if (contentScale == 0.0f) {
		throw graphics::Error{String{"Failed to get the content scale of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	return contentScale;
}

DisplayMode Display::getDesktopDisplayMode() const {
	const SDL_DisplayMode* const mode = SDL_GetDesktopDisplayMode(displayID);
	if (!mode) {
		throw graphics::Error{String{"Failed to get the desktop display mode of display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	const Extent2D size{.width = static_cast<uint32_t>(mode->w), .height = static_cast<uint32_t>(mode->h)};
	return createDisplayMode(mode->displayID, mode->format, size, mode->pixel_density, mode->refresh_rate, mode->refresh_rate_numerator, mode->refresh_rate_denominator,
		mode->internal);
}

Display::DisplayModeRange Display::getAvailableFullscreenDisplayModes() const {
	int displayModeCount{};
	SDL_DisplayMode** const displayModes = SDL_GetFullscreenDisplayModes(displayID, &displayModeCount);
	if (!displayModes) {
		throw graphics::Error{String{"Failed to get the display modes for display "} + toString(displayID) + ":\n" + SDL_GetError()};
	}
	return DisplayModeRange{reinterpret_cast<void**>(displayModes), displayModeCount};
}

void Display::DisplayModeRange::DisplayModesDeleter::operator()(void** displayModes) const noexcept {
	SDL_free(displayModes);
}

uint32_t Display::getID() const noexcept {
	return static_cast<uint32_t>(displayID);
}

void Display::DisplayRange::DisplayIDsDeleter::operator()(uint32_t* displayIDs) const noexcept {
	SDL_free(displayIDs);
}

Display::DisplayModeIterator& Display::DisplayModeIterator::operator++() {
	++p;
	if (*p) {
		const SDL_DisplayMode* mode = static_cast<const SDL_DisplayMode*>(*p);
		const Extent2D size{.width = static_cast<uint32_t>(mode->w), .height = static_cast<uint32_t>(mode->h)};
		displayMode = createDisplayMode(mode->displayID, mode->format, size, mode->pixel_density, mode->refresh_rate, mode->refresh_rate_numerator, mode->refresh_rate_denominator,
			mode->internal);
	}
	return *this;
}

Display::DisplayModeIterator::DisplayModeIterator(void** p)
	: p(p) {
	if (*p) {
		const SDL_DisplayMode* mode = static_cast<const SDL_DisplayMode*>(*p);
		const Extent2D size{.width = static_cast<uint32_t>(mode->w), .height = static_cast<uint32_t>(mode->h)};
		displayMode = createDisplayMode(mode->displayID, mode->format, size, mode->pixel_density, mode->refresh_rate, mode->refresh_rate_numerator, mode->refresh_rate_denominator,
			mode->internal);
	}
}

} // namespace grem::graphics
