// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/Display.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/resource/Image.hpp>

#include <SDL3/SDL.h> // SDL...

namespace grem::graphics {

Window* Window::getFocused() {
	if (SDL_Window* const sdlWindow = SDL_GetKeyboardFocus()) {
		const SDL_PropertiesID properties = SDL_GetWindowProperties(sdlWindow);
		if (properties != 0) {
			void* const window = SDL_GetPointerProperty(properties, "GREM.window", nullptr);
			if (window) {
				return static_cast<Window*>(window);
			}
		}
	}
	return nullptr;
}

Window* Window::findByID(uint32_t id) {
	if (SDL_Window* const sdlWindow = SDL_GetWindowFromID(id)) {
		const SDL_PropertiesID properties = SDL_GetWindowProperties(sdlWindow);
		if (properties != 0) {
			void* const window = SDL_GetPointerProperty(properties, "GREM.window", nullptr);
			if (window) {
				return static_cast<Window*>(window);
			}
		}
	}
	return nullptr;
}

Window::Window(const WindowOptions& options)
	: implementation(nullptr, options) {
	const SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(get()));
	if (properties != 0) {
		SDL_SetPointerProperty(properties, "GREM.window", this);
	}
	if (options.opacity != 1.0f) {
		setOpacity(options.opacity);
	}
	if (options.resizable) {
		try {
			setFullscreenDisplayMode(getDisplay().getDesktopDisplayMode());
		} catch (const Error&) {
		}
	} else if (options.fullscreen) {
		setFullscreen(true);
	}
	if (options.relativeMouseMode) {
		setRelativeMouseMode(true);
	}
}

Window::Window(Window& parent, const WindowOptions& options)
	: implementation(&parent, options) {
	const SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(get()));
	if (properties != 0) {
		SDL_SetPointerProperty(properties, "GREM.window", this);
	}
	if (options.opacity != 1.0f) {
		setOpacity(options.opacity);
	}
	if (options.resizable) {
		try {
			setFullscreenDisplayMode(getDisplay().getDesktopDisplayMode());
		} catch (const Error&) {
		}
	} else if (options.fullscreen) {
		setFullscreen(true);
	}
	if (options.relativeMouseMode) {
		setRelativeMouseMode(true);
	}
}

void Window::setIcon(const resource::ImageView& newIconImage) {
	if (newIconImage.getType() != resource::ImageType::IMAGE_2D) {
		throw graphics::Error{"Invalid window icon image type."};
	}
	if (newIconImage.getWidth() > uint32_t{Limits<int>::MAX / 4} || newIconImage.getHeight() > uint32_t{Limits<int>::MAX} || newIconImage.getDepth() > 1) {
		throw graphics::Error{"Maximum window icon size exceeded."};
	}
	if (newIconImage.getWidth() == 0 || newIconImage.getHeight() == 0 || newIconImage.getDepth() == 0 ||
		newIconImage.getContents().size() < size_t{newIconImage.getWidth()} * size_t{newIconImage.getHeight()} * 4) {
		throw graphics::Error{"Empty window icon image."};
	}
	if (newIconImage.getFormat() != resource::ImageFormat::R8G8B8A8_UINT) {
		throw graphics::Error{"Invalid window icon image format."};
	}
	SDL_Surface* const icon = SDL_CreateSurfaceFrom(static_cast<int>(newIconImage.getWidth()), static_cast<int>(newIconImage.getHeight()), SDL_PIXELFORMAT_RGBA8888,
		const_cast<byte*>(newIconImage.data()), static_cast<int>(newIconImage.getWidth() * 4));
	try {
		if (!SDL_SetWindowIcon(static_cast<SDL_Window*>(get()), icon)) {
			throw graphics::Error{String{"Failed to set window icon:\n"} + SDL_GetError()};
		}
	} catch (...) {
		SDL_DestroySurface(icon);
		throw;
	}
	SDL_DestroySurface(icon);
}

void Window::setParent(Window* newParent) { // NOLINT(readability-make-member-function-const)
#ifdef __APPLE__
	(void)newParent;
#else
	if (!SDL_SetWindowParent(static_cast<SDL_Window*>(implementation.window), (newParent) ? static_cast<SDL_Window*>(newParent->get()) : nullptr)) {
		throw graphics::Error{String{"Failed to set window parent:\n"} + SDL_GetError()};
	}
#endif
}

void Window::setTitle(CStringView newTitle) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowTitle(static_cast<SDL_Window*>(get()), newTitle.c_str())) {
		throw graphics::Error{String{"Failed to set window title:\n"} + SDL_GetError()};
	}
}

void Window::setPosition(Offset2D newPosition) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowPosition(static_cast<SDL_Window*>(get()), static_cast<int>(newPosition.x), static_cast<int>(newPosition.y))) {
		throw graphics::Error{String{"Failed to set window position:\n"} + SDL_GetError()};
	}
}

void Window::setSize(Extent2D newSize) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowSize(static_cast<SDL_Window*>(get()), static_cast<int>(newSize.width), static_cast<int>(newSize.height))) {
		throw graphics::Error{String{"Failed to set window size:\n"} + SDL_GetError()};
	}
}

void Window::setOpacity(float newOpacity) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowOpacity(static_cast<SDL_Window*>(get()), newOpacity)) {
		throw graphics::Error{String{"Failed to set window opacity:\n"} + SDL_GetError()};
	}
}

void Window::show(bool activate) { // NOLINT(readability-make-member-function-const)
	SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, (activate) ? "1" : "0");
	if (!SDL_ShowWindow(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to show window:\n"} + SDL_GetError()};
	}
}

void Window::hide() { // NOLINT(readability-make-member-function-const)
	if (!SDL_HideWindow(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to hide window:\n"} + SDL_GetError()};
	}
}

void Window::raise() { // NOLINT(readability-make-member-function-const)
	if (!SDL_RaiseWindow(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to raise window:\n"} + SDL_GetError()};
	}
}

void Window::minimize() { // NOLINT(readability-make-member-function-const)
	if (!SDL_MinimizeWindow(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to minimize window:\n"} + SDL_GetError()};
	}
}

void Window::setResizable(bool newResizable) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowResizable(static_cast<SDL_Window*>(get()), newResizable)) {
		throw graphics::Error{String{"Failed to set window resizable:\n"} + SDL_GetError()};
	}
}

void Window::setFullscreen(bool newFullscreen) {
	SDL_SyncWindow(static_cast<SDL_Window*>(get()));
	const bool resizable = isResizable();
	if (!resizable) {
		try {
			if (newFullscreen) {
				const Extent2D size = getSize();
				const Display::DisplayModeRange displayModes = getDisplay().getAvailableFullscreenDisplayModes();
				Optional<DisplayMode> bestDisplayMode{};
				for (const DisplayMode& mode : displayModes) {
					if (mode.getSize() == size &&
						(!bestDisplayMode || (mode.getRefreshRate() > bestDisplayMode->getRefreshRate() && mode.getBitsPerPixel() >= 24) ||
							(mode.getRefreshRate() >= bestDisplayMode->getRefreshRate() && mode.getBitsPerPixel() > bestDisplayMode->getBitsPerPixel()))) {
						bestDisplayMode = mode;
					}
				}
				if (bestDisplayMode) {
					setFullscreenDisplayMode(*bestDisplayMode);
				} else {
					DisplayMode displayMode = getDisplay().getDesktopDisplayMode();
					displayMode.size = size;
					setFullscreenDisplayMode(displayMode);
				}
			} else {
				setFullscreenDisplayMode(getDisplay().getDesktopDisplayMode());
			}
			SDL_SyncWindow(static_cast<SDL_Window*>(get()));
		} catch (const Error&) {
		}
	}
	if (!SDL_SetWindowFullscreen(static_cast<SDL_Window*>(get()), newFullscreen)) {
		throw graphics::Error{String{"Failed to set window fullscreen:\n"} + SDL_GetError()};
	}
	SDL_SyncWindow(static_cast<SDL_Window*>(get()));
}

void Window::setFullscreenDisplayMode(Optional<DisplayMode> newDisplayMode) { // NOLINT(readability-make-member-function-const)
#ifdef __EMSCRIPTEN__
	(void)newDisplayMode;
#else
	SDL_DisplayMode mode{};
	if (newDisplayMode) {
		mode = {
			.displayID = newDisplayMode->displayID,
			.format = static_cast<SDL_PixelFormat>(newDisplayMode->format),
			.w = static_cast<int>(newDisplayMode->size.width),
			.h = static_cast<int>(newDisplayMode->size.height),
			.pixel_density = newDisplayMode->pixelDensity,
			.refresh_rate = newDisplayMode->refreshRate,
			.refresh_rate_numerator = newDisplayMode->refreshRateNumerator,
			.refresh_rate_denominator = newDisplayMode->refreshRateDenominator,
			.internal = static_cast<SDL_DisplayModeData*>(newDisplayMode->internal),
		};
	}
	if (!SDL_SetWindowFullscreenMode(static_cast<SDL_Window*>(get()), (newDisplayMode) ? &mode : nullptr)) {
		throw graphics::Error{String{"Failed to set window fullscreen display mode:\n"} + SDL_GetError()};
	}
#endif
}

void Window::setRelativeMouseMode(bool newRelativeMouseMode) { // NOLINT(readability-make-member-function-const)
	if (!SDL_SetWindowRelativeMouseMode(static_cast<SDL_Window*>(get()), newRelativeMouseMode)) {
		throw graphics::Error{String{"Failed to set window relative mouse mode:\n"} + SDL_GetError()};
	}
}

void Window::setTextInputArea(const Region2D& area, int32_t cursorOffset) { // NOLINT(readability-make-member-function-const)
	const SDL_Rect rect{
		.x = static_cast<int>(area.offset.x),
		.y = static_cast<int>(area.offset.y),
		.w = static_cast<int>(area.size.width),
		.h = static_cast<int>(area.size.height),
	};
	if (!SDL_SetTextInputArea(static_cast<SDL_Window*>(get()), &rect, static_cast<int>(cursorOffset))) {
		throw graphics::Error{String{"Failed to set text input area:\n"} + SDL_GetError()};
	}
}

void Window::startTextInput() { // NOLINT(readability-make-member-function-const)
	if (!SDL_StartTextInput(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to start text input:\n"} + SDL_GetError()};
	}
}

void Window::stopTextInput() { // NOLINT(readability-make-member-function-const)
	if (!SDL_StopTextInput(static_cast<SDL_Window*>(get()))) {
		throw graphics::Error{String{"Failed to stop text input:\n"} + SDL_GetError()};
	}
}

bool Window::isTextInputActive() const {
	return SDL_TextInputActive(static_cast<SDL_Window*>(get()));
}

bool Window::hasScreenKeyboardSupport() const {
	return SDL_HasScreenKeyboardSupport();
}

bool Window::isScreenKeyboardShown() const {
	return SDL_ScreenKeyboardShown(static_cast<SDL_Window*>(get()));
}

bool Window::isHighPixelDensity() const {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(get()));
	return (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) != 0;
}

bool Window::hasFocus() const {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(get()));
	return (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool Window::isMinimized() const {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(get()));
	return (flags & SDL_WINDOW_MINIMIZED) != 0;
}

bool Window::isResizable() const {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(get()));
	return (flags & SDL_WINDOW_RESIZABLE) != 0;
}

bool Window::isFullscreen() const {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(get()));
	return (flags & SDL_WINDOW_FULLSCREEN) != 0;
}

Window* Window::getParent() const {
	if (SDL_Window* const parentSDLWindow = SDL_GetWindowParent(static_cast<SDL_Window*>(get()))) {
		const SDL_PropertiesID parentWindowProperties = SDL_GetWindowProperties(parentSDLWindow);
		if (parentWindowProperties != 0) {
			void* const parentWindow = SDL_GetPointerProperty(parentWindowProperties, "GREM.window", nullptr);
			if (parentWindow) {
				return static_cast<Window*>(parentWindow);
			}
		}
	}
	return nullptr;
}

Offset2D Window::getPosition() const {
	int x = 0;
	int y = 0;
	if (!SDL_GetWindowPosition(static_cast<SDL_Window*>(get()), &x, &y)) {
		throw graphics::Error{String{"Failed to get window position:\n"} + SDL_GetError()};
	}
	return Offset2D{.x = static_cast<int32_t>(x), .y = static_cast<int32_t>(y)};
}

Extent2D Window::getSize() const {
	int width = 0;
	int height = 0;
	if (!SDL_GetWindowSize(static_cast<SDL_Window*>(get()), &width, &height)) {
		throw graphics::Error{String{"Failed to get window size:\n"} + SDL_GetError()};
	}
	return Extent2D{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

Extent2D Window::getDrawableSize() const {
	int width = 0;
	int height = 0;
	if (!SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(get()), &width, &height)) {
		throw graphics::Error{String{"Failed to get window drawable size:\n"} + SDL_GetError()};
	}
	return Extent2D{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

float Window::getDisplayScale() const {
	const float displayScale = SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(get()));
	if (displayScale == 0.0f) {
		throw graphics::Error{String{"Failed to get window display scale:\n"} + SDL_GetError()};
	}
	return displayScale;
}

float Window::getOpacity() const {
	const float opacity = SDL_GetWindowOpacity(static_cast<SDL_Window*>(get()));
	if (opacity == -1.0f) {
		throw graphics::Error{String{"Failed to get window opacity:\n"} + SDL_GetError()};
	}
	return opacity;
}

Display Window::getDisplay() const {
	const SDL_DisplayID displayID = SDL_GetDisplayForWindow(static_cast<SDL_Window*>(get()));
	if (displayID == 0) {
		throw graphics::Error{String{"Failed to get window display:\n"} + SDL_GetError()};
	}
	return Display{displayID};
}

Optional<DisplayMode> Window::getFullscreenDisplayMode() const {
	const SDL_DisplayMode* const mode = SDL_GetWindowFullscreenMode(static_cast<SDL_Window*>(get()));
	if (!mode) {
		return {};
	}
	const Extent2D size{.width = static_cast<uint32_t>(mode->w), .height = static_cast<uint32_t>(mode->h)};
	return Display::createDisplayMode(mode->displayID, mode->format, size, mode->pixel_density, mode->refresh_rate, mode->refresh_rate_numerator, mode->refresh_rate_denominator,
		mode->internal);
}

uint32_t Window::getID() const {
	const SDL_WindowID id = SDL_GetWindowID(static_cast<SDL_Window*>(get()));
	if (id == 0) {
		throw graphics::Error{String{"Failed to get window ID:\n"} + SDL_GetError()};
	}
	return id;
}

void* Window::getNativeHandle() const {
#if defined(_WIN32) && !defined(__WINRT__)
	const SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(get()));
	if (properties == 0) {
		throw graphics::Error{String{"Failed to get window properties:\n"} + SDL_GetError()};
	}
	void* const result = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
	if (!result) {
		throw graphics::Error{String{"Failed to get window HWND pointer:\n"} + SDL_GetError()};
	}
	return result;
#else
	return nullptr;
#endif
}

} // namespace grem::graphics
