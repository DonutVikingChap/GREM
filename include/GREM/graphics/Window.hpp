// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_WINDOW_HPP
#define GREM_GRAPHICS_WINDOW_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/Display.hpp>

namespace grem::resource {
class ImageView; // Forward declaration, to avoid including Image.hpp.
} // namespace grem::resource

namespace grem::graphics {

/**
 * Configuration options for a Window.
 */
struct WindowOptions {
	/**
	 * Null-terminated UTF-8 string of the displayed title of the window.
	 */
	CStringView title = "Application";

	/**
	 * The desired horizontal position of the window, in screen coordinates
	 * (typically pixels), or an empty optional to choose a position
	 * automatically.
	 */
	Optional<int32_t> positionX{};

	/**
	 * The desired vertical position of the window, in screen coordinates
	 * (typically pixels), or an empty optional to choose a position
	 * automatically.
	 */
	Optional<int32_t> positionY{};

	/**
	 * The desired size of the window, in screen coordinates (typically pixels).
	 *
	 * \warning Both the width and height must be positive.
	 */
	Extent2D size{.width = 800, .height = 600};

	/**
	 * Number of samples used for multisample anti-aliasing (MSAA) when
	 * rendering directly to the window.
	 *
	 * This can be used to mitigate aliasing artifacts on the edges of 3D
	 * objects, at the cost of some performance.
	 *
	 * If set to 1 or lower, MSAA will not be used.
	 *
	 * \remark Typical values are 1, 2 and 4.
	 *
	 * \note Current GPUs (as of 2026) rarely support values greater than 8.
	 */
	uint32_t multisampleCount = 1;

	/**
	 * Opacity of the window, from 0 (transparent) to 1 (opaque).
	 */
	float opacity = 1.0f;

	/**
	 * Whether the window's backbuffer should have high pixel density (if
	 * possible) or not.
	 */
	bool highPixelDensity = false;

	/**
	 * Whether the window should start hidden or not.
	 */
	bool hidden = false;

	/**
	 * Whether the window should start with input focus or not.
	 */
	bool focus = true;

	/**
	 * Whether the window should start minimized or not.
	 */
	bool minimized = false;

	/**
	 * Whether the user should be allowed to resize the window or not.
	 */
	bool resizable = true;

	/**
	 * Whether the window should start in fullscreen mode or not.
	 */
	bool fullscreen = false;

	/**
	 * Whether the window should be borderless or not.
	 */
	bool borderless = false;

	/**
	 * Whether the window should be hidden from the taskbar and window list and
	 * be treated as a utility window or not.
	 */
	bool hideFromTaskbar = false;

	/**
	 * Whether the window should always be on top of other windows or not.
	 */
	bool alwaysOnTop = false;

	/**
	 * Whether the window should start with relative mouse mode enabled or not.
	 *
	 * When relative mouse mode is enabled, it hides the mouse cursor, locks it
	 * to the window and causes all further mouse motion to be provided as
	 * relative motion events until it is disabled again.
	 */
	bool relativeMouseMode = false;
};

/**
 * Graphical window that can be rendered to.
 */
class Window {
public:
	/**
	 * Get the window that currently has keyboard focus.
	 *
	 * \return a non-owning pointer to the window that currently has focus, or
	 *         nullptr if no window that this application controls has focus.
	 */
	[[nodiscard]] GREM_API(graphics) static Window* getFocused();

	/**
	 * Get the window with a specific identifier.
	 *
	 * \param id identifier to get the window of.
	 *
	 * \return a non-owning pointer to the window with the given identifier, or
	 *         nullptr if no window with the given identifier exists.
	 */
	[[nodiscard]] GREM_API(graphics) static Window* findByID(uint32_t id);

	/**
	 * Create a new standalone window.
	 *
	 * \param options initial configuration of the window, see WindowOptions.
	 *
	 * \throws graphics::Error if context or window setup failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) explicit Window(const WindowOptions& options);

	/**
	 * Create a new child window.
	 *
	 * \param parent parent window to create this window from.
	 * \param options initial configuration of the window, see WindowOptions.
	 *
	 * \throws graphics::Error if context or window setup failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) Window(Window& parent, const WindowOptions& options);

	/** Destructor. */
	~Window() = default;

	/** Copying a window is not allowed, since it manages global state. */
	Window(const Window&) = delete;

	/** Moving a window is not allowed, since it manages global state. */
	Window(Window&&) = delete;

	/** Copying a window is not allowed, since it manages global state. */
	Window& operator=(const Window&) = delete;

	/** Moving a window is not allowed, since it manages global state. */
	Window& operator=(Window&&) = delete;

	/**
	 * Set the parent window of the window.
	 *
	 * \param newParent window to parent the window to, or nullptr to make the
	 *        window standalone. Must not be a current sibling or child of this
	 *        window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::parent
	 * \sa getParent()
	 */
	GREM_API(graphics) void setParent(Window* newParent);

	/**
	 * Set the icon of the window.
	 *
	 * \param newIconImage new icon image to set. Should be a 2D image in
	 *        resource::ImageFormat::R8G8B8A8_UINT format in sRGB color.
	 *
	 * \throws graphics::Error on failure, or if the image type/size/format is
	 *         invalid.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void setIcon(const resource::ImageView& newIconImage);

	/**
	 * Set the displayed title of the window.
	 *
	 * \param newTitle null-terminated UTF-8 string containing the title.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::title
	 * \sa getTitle()
	 */
	GREM_API(graphics) void setTitle(CStringView newTitle);

	/**
	 * Set the position of the window in the windowing system.
	 *
	 * \param newPosition new position of the window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa getPosition()
	 */
	GREM_API(graphics) void setPosition(Offset2D newPosition);

	/**
	 * Set the size of the window.
	 *
	 * \param newSize new size of the window, in screen coordinates (typically
	 *        pixels). Both the width and height must be positive.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::size
	 * \sa getSize()
	 */
	GREM_API(graphics) void setSize(Extent2D newSize);

	/**
	 * Set the opacity of the window.
	 *
	 * \param newOpacity new opacity of the window, from 0 (transparent) to 1
	 *        (opaque).
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::opacity
	 * \sa getOpacity()
	 */
	GREM_API(graphics) void setOpacity(float newOpacity);

	/**
	 * Show the window.
	 *
	 * \param activate whether the window should be activated when shown or not.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::hidden
	 * \sa hide()
	 * \sa raise()
	 */
	GREM_API(graphics) void show(bool activate = true);

	/**
	 * Hide the window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::hidden
	 * \sa show()
	 */
	GREM_API(graphics) void hide();

	/**
	 * Request the window to be raised above other windows and gain focus.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa show()
	 * \sa hasFocus()
	 */
	GREM_API(graphics) void raise();

	/**
	 * Request the window to be minimized to the taskbar.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::minimized
	 * \sa isMinimized()
	 */
	GREM_API(graphics) void minimize();

	/**
	 * Set whether to allow the window to be resized by the user or not.
	 *
	 * \param newResizable true to allow resizing, false to disallow.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::resizable
	 * \sa isResizable()
	 */
	GREM_API(graphics) void setResizable(bool newResizable);

	/**
	 * Set the fullscreen state of the window.
	 *
	 * \param newFullscreen true for fullscreen mode, false for windowed mode.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the window is resizable, the fullscreen display mode will be the
	 *       desktop mode of the window's associated display. Otherwise, the
	 *       fullscreen display mode will be the one set using
	 *       setFullscreenDisplayMode().
	 *
	 * \sa WindowOptions::fullscreen
	 * \sa isFullscreen()
	 * \sa setFullscreenDisplayMode()
	 */
	GREM_API(graphics) void setFullscreen(bool newFullscreen);

	/**
	 * Set the display mode that will be used in fullscreen for this window if
	 * the window is not resizable.
	 *
	 * \param newDisplayMode the display mode to use in fullscreen, or an empty
	 *        optional for borderless desktop display mode.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The effect of this function will only be visible after calling
	 *       setFullscreen(true) while the window is not resizable.
	 *
	 * \sa setFullscreen()
	 * \sa getFullscreenDisplayMode()
	 */
	GREM_API(graphics) void setFullscreenDisplayMode(Optional<DisplayMode> newDisplayMode);

	/**
	 * Enable or disable relative mouse mode for this window.
	 *
	 * When relative mouse mode is enabled, it hides the mouse cursor, locks it
	 * to the window and causes all further mouse motion to be provided as
	 * relative motion events until it is disabled again.
	 *
	 * \param newRelativeMouseMode true to enable relative mouse mode, false to
	 *        disable.
	 *
	 * \throws graphics::Error on failure to set the relative mouse mode.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa WindowOptions::relativeMouseMode
	 */
	GREM_API(graphics) void setRelativeMouseMode(bool newRelativeMouseMode);

	/**
	 * Set the window's input area for text input.
	 *
	 * \param area input area rectangle, in screen coordinates.
	 * \param cursorOffset horizontal offset of the cursor from area.offset.x,
	 *        in screen coordinates.
	 *
	 * \throws graphics::Error on failure to set the input area.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa startTextInput()
	 * \sa stopTextInput()
	 * \sa isTextInputActive()
	 */
	GREM_API(graphics) void setTextInputArea(const Region2D& area, int32_t cursorOffset);

	/**
	 * Start accepting text input events in the current text input rectangle.
	 *
	 * \sa setTextInputArea()
	 * \sa stopTextInput()
	 * \sa isTextInputActive()
	 *
	 * \throws graphics::Error on failure to start text input.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void startTextInput();

	/**
	 * Stop accepting text input events.
	 *
	 * \sa setTextInputArea()
	 * \sa startTextInput()
	 * \sa isTextInputActive()
	 *
	 * \throws graphics::Error on failure to stop text input.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void stopTextInput();

	/**
	 * Check if text input events are currently being accepted.
	 *
	 * \return true if text input is active, false otherwise.
	 *
	 * \sa setTextInputArea()
	 * \sa startTextInput()
	 * \sa stopTextInput()
	 */
	[[nodiscard]] GREM_API(graphics) bool isTextInputActive() const;

	/**
	 * Check if the application supports a screen keyboard.
	 *
	 * \return true if a screen keyboard is supported, false otherwise.
	 *
	 * \sa isScreenKeyboardShown()
	 */
	[[nodiscard]] GREM_API(graphics) bool hasScreenKeyboardSupport() const;

	/**
	 * Check if the screen keyboard is currently open.
	 *
	 * \return true if the screen keyboard is open, false otherwise.
	 *
	 * \sa hasScreenKeyboardSupport()
	 */
	[[nodiscard]] GREM_API(graphics) bool isScreenKeyboardShown() const;

	/**
	 * Check if the window's backbuffer is high pixel density.
	 *
	 * \return true if the window's backbuffer is high pixel density, false
	 *         otherwise.
	 *
	 * \sa WindowOptions::highPixelDensity
	 */
	[[nodiscard]] GREM_API(graphics) bool isHighPixelDensity() const;

	/**
	 * Check if the window currently has input focus.
	 *
	 * \return true if the window has focus, false otherwise.
	 *
	 * \sa WindowOptions::focus
	 * \sa raise()
	 */
	[[nodiscard]] GREM_API(graphics) bool hasFocus() const;

	/**
	 * Check if the window is currently minimized.
	 *
	 * \return true if the window is minimized, false otherwise.
	 *
	 * \sa minimize()
	 */
	[[nodiscard]] GREM_API(graphics) bool isMinimized() const;

	/**
	 * Check if the window is currently resizable.
	 *
	 * \return true if the window is resizable, false otherwise.
	 *
	 * \sa setResizable()
	 */
	[[nodiscard]] GREM_API(graphics) bool isResizable() const;

	/**
	 * Check if the window is currently in fullscreen mode.
	 *
	 * \return true if the window is in fullscreen mode, false otherwise.
	 *
	 * \sa setFullscreen()
	 * \sa setFullscreenDisplayMode()
	 */
	[[nodiscard]] GREM_API(graphics) bool isFullscreen() const;

	/**
	 * Get the parent of the window.
	 *
	 * \return a non-owning pointer to the current parent window, or nullptr if
	 *         the window has no parent.
	 *
	 * \sa setParent()
	 */
	[[nodiscard]] GREM_API(graphics) Window* getParent() const;

	/**
	 * Get the position of the window as reported by the windowing system.
	 *
	 * \return the current position of the window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) Offset2D getPosition() const;

	/**
	 * Get the size of the window.
	 *
	 * \return the current size of the window, in screen coordinates (typically
	 *         pixels).
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa getDrawableSize()
	 */
	[[nodiscard]] GREM_API(graphics) Extent2D getSize() const;

	/**
	 * Get the drawable size of the window.
	 *
	 * \return the current drawable size of the window, in pixels.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa getSize()
	 */
	[[nodiscard]] GREM_API(graphics) Extent2D getDrawableSize() const;

	/**
	 * Get the number of samples used for multisample anti-aliasing (MSAA) when
	 * rendering to the window.
	 *
	 * \return the MSAA level of the window.
	 */
	[[nodiscard]] uint32_t getMultisampleCount() const {
		return implementation.multisampleCount;
	}

	/**
	 * Get the content display scale relative to the window's pixel size.
	 *
	 * \return the display scale of this window.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) float getDisplayScale() const;

	/**
	 * Get the opacity of the window.
	 *
	 * \return the current opacity of the window, from 0 (transparent) to 1
	 *         (opaque).
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa setOpacity()
	 */
	[[nodiscard]] GREM_API(graphics) float getOpacity() const;

	/**
	 * Get the display that this window is associated with.
	 *
	 * \return the display containing the center of the window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) Display getDisplay() const;

	/**
	 * Get the fullscreen display mode of the window.
	 *
	 * \return the display mode that will be used in fullscreen for this window
	 *         if the window is not resizable, or an empty optional for
	 *         borderless desktop display mode.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa setFullscreenDisplayMode()
	 */
	[[nodiscard]] GREM_API(graphics) Optional<DisplayMode> getFullscreenDisplayMode() const;

	/**
	 * Get a unique identifier for this window.
	 *
	 * \return the identifier corresponding to this window.
	 *
	 * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t getID() const;

	/**
	 * Get a pointer to the native handle of the window, if available for the
	 * current platform.
	 *
	 * \return a non-owning pointer representing the native handle of the window
	 *         used by the host platform, or nullptr if such a handle is not
	 *         available.
	 *
	 * \note On Windows, this is the HWND pointer. Other platforms are likely to
	 *       return null.
	 *
	 * \throws graphics::Error on failure to get the handle on a supported
	 *         platform.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) void* getNativeHandle() const;

	/**
	 * Get an opaque pointer to the internal representation of the window.
	 *
	 * \return an untyped non-owning pointer to the internal representation of
	 *         the window.
	 *
	 * \note This function is used internally by the implementations of various
	 *       abstractions and is not intended to be used outside of the graphics
	 *       module. The returned pointer has no meaning to application code.
	 */
	[[nodiscard]] void* get() const noexcept {
		return implementation.window;
	}

	/**
	 * Get an opaque pointer to the internal representation of the window
	 * surface.
	 *
	 * \return an untyped non-owning pointer to the internal representation of
	 *         the window surface.
	 *
	 * \note This function is used internally by the implementations of various
	 *       abstractions and is not intended to be used outside of the graphics
	 *       module. The returned pointer has no meaning to application code.
	 */
	[[nodiscard]] void* getSurface() const noexcept {
		return implementation.surface;
	}

private:
	struct Implementation {
		void* window = nullptr;  // SDL_Window*
		void* surface = nullptr; // VkSurfaceKHR on Vulkan, SDL_GLContext on OpenGL.
		uint32_t multisampleCount;

		GREM_API(graphics) Implementation(Window* parent, const WindowOptions& options);
		GREM_API(graphics) ~Implementation();

		Implementation(const Implementation&) = delete;
		Implementation(Implementation&&) = delete;
		Implementation& operator=(const Implementation&) = delete;
		Implementation& operator=(Implementation&&) = delete;
	};

	Implementation implementation;
};

} // namespace grem::graphics

#endif
