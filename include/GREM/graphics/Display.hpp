// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_DISPLAY_HPP
#define GREM_GRAPHICS_DISPLAY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>

#include <iterator> // std::forward_iterator_tag

namespace grem::graphics {

class Window;  // Forward declaration, to avoid a circular include of Window.hpp.
class Display; // Forward declaration.

/**
 * Information about a specific display mode available to a Display.
 */
class DisplayMode {
public:
	/**
	 * Get the size of the display mode.
	 *
	 * \return the size of the display mode, in screen coordinates (typically
	 *         pixels).
	 */
	[[nodiscard]] Extent2D getSize() const noexcept {
		return size;
	}

	/**
	 * Get the pixel density of the display mode.
	 *
	 * \return the pixel density scale of the display mode. For example, a
	 *         1920x1080 mode with a pixel density of 2 would have 3840x2160
	 *         pixels.
	 */
	[[nodiscard]] float getPixelDensity() const noexcept {
		return pixelDensity;
	}

	/**
	 * Get the refresh rate of the display mode.
	 *
	 * \return the refresh rate of the display mode, in Hertz, or 0 if
     *         unspecified.
	 */
	[[nodiscard]] float getRefreshRate() const noexcept {
		return refreshRate;
	}

	/**
	 * Get the color depth of the display mode.
	 *
	 * \return the number of bits of color information per pixel in the display
     *         mode's pixel format.
	 */
	[[nodiscard]] GREM_API(graphics) int getBitsPerPixel() const noexcept;

private:
	friend Display;
	friend Window;

	constexpr DisplayMode(uint32_t displayID, uint32_t format, Extent2D size, float pixelDensity, float refreshRate, int refreshRateNumerator, int refreshRateDenominator,
		void* internal) noexcept
		: displayID(displayID)
		, format(format)
		, size(size)
		, pixelDensity(pixelDensity)
		, refreshRate(refreshRate)
		, refreshRateNumerator(refreshRateNumerator)
		, refreshRateDenominator(refreshRateDenominator)
		, internal(internal) {}

	uint32_t displayID;
	uint32_t format;
	Extent2D size;
	float pixelDensity;
	float refreshRate;
	int refreshRateNumerator;
	int refreshRateDenominator;
	void* internal;
};

/**
 * Information about a display on the host system.
 */
class Display {
private:
	struct DisplaySentinel;
	class DisplayIterator;
	class DisplayRange;
	struct DisplayModeSentinel;
	class DisplayModeIterator;
	class DisplayModeRange;

public:
	enum class Orientation : int32_t { // NOLINT(performance-enum-size)
		UNKNOWN = 0,                   ///< Unknown display orientation.
		LANDSCAPE = 1,                 ///< Landscape mode, rotated counter-clockwise relative to portrait mode.
		LANDSCAPE_FLIPPED = 2,         ///< Landscape mode, rotated clockwise relative to portrait mode.
		PORTRAIT = 3,                  ///< Portrait mode.
		PORTRAIT_FLIPPED = 4,          ///< Portrait mode, upside down.
	};

	/**
     * Get a list of all available displays.
     *
     * \return a read-only range containing all currently available displays.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
     */
	[[nodiscard]] GREM_API(graphics) static DisplayRange getAll();

	/**
     * Get the current primary display.
     *
     * \return information about the current primary display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
     */
	[[nodiscard]] GREM_API(graphics) static Display getPrimary();

	/**
	 * Get the display with a specific identifier.
	 *
	 * \param id identifier to get the display of.
	 *
	 * \return the display with the given identifier, or an empty optional if no
	 *         display with the given identifier exists.
	 */
	[[nodiscard]] GREM_API(graphics) static Optional<Display> findByID(uint32_t id);

	/**
     * Get the manufacturer-specified name of the display.
     *
     * \return a read-only view over a UTF-8-encoded string containing the name
     *         of the display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
     */
	[[nodiscard]] GREM_API(graphics) CStringView getName() const;

	/**
	 * Get the bounds of the display region on the desktop.
	 *
	 * \return a rectangle representing the display's position and size on the
     *         desktop.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) Region2D getBounds() const;

	/**
	 * Get the bounds of the usable display region on the desktop, which may
     * exclude areas reserved by the system.
	 *
	 * \return a rectangle representing the display's usable position and size
     *         on the desktop.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) Region2D getUsableBounds() const;

	/**
	 * Get the current orientation of the display.
	 *
	 * \return the orientation of the display, or Orientation::UNKNOWN if
	 *         unavailable.
	 */
	[[nodiscard]] GREM_API(graphics) Orientation getOrientation() const noexcept;

	/**
	 * Get the current display mode of this display.
	 *
	 * \return the current display mode of this display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) DisplayMode getDisplayMode() const;

	/**
	 * Get the expected scale for content based on the DPI settings of the
	 * display.
	 *
	 * \return the content scale of this display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) float getContentScale() const;

	/**
     * Get the display mode of the full desktop.
     *
     * \return a display mode representing the full desktop for this display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
     */
	[[nodiscard]] GREM_API(graphics) DisplayMode getDesktopDisplayMode() const;

	/**
	 * Get a list of all available fulscreen display modes for this display.
	 *
	 * \return a read-only range containing all available fullscreen display
	 *         modes for this display.
     *
     * \throws graphics::Error on failure.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(graphics) DisplayModeRange getAvailableFullscreenDisplayModes() const;

	/**
	 * Get the unique identifier of this display.
	 *
	 * \return the identifier corresponding to this display.
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t getID() const noexcept;

private:
	friend Window;

	[[nodiscard]] static constexpr DisplayMode createDisplayMode(uint32_t displayID, uint32_t format, Extent2D size, float pixelDensity, float refreshRate,
		int refreshRateNumerator, int refreshRateDenominator, void* internal) noexcept {
		return DisplayMode{displayID, format, size, pixelDensity, refreshRate, refreshRateNumerator, refreshRateDenominator, internal};
	}

	constexpr explicit Display(uint32_t displayID) noexcept
		: displayID(displayID) {}

	uint32_t displayID;
};

struct Display::DisplaySentinel {};

class Display::DisplayIterator {
public:
	using difference_type = ptrdiff_t;
	using value_type = Display;
	using reference = const value_type&;
	using pointer = const value_type*;
	using iterator_category = std::forward_iterator_tag;
	using sentinel = DisplaySentinel;

	DisplayIterator() noexcept = default;

	[[nodiscard]] bool operator==(const DisplayIterator& other) const {
		return p == other.p;
	}

	[[nodiscard]] bool operator==(sentinel) const {
		return display.displayID == 0;
	}

	[[nodiscard]] reference operator*() const {
		GREM_ASSERT(p && *p != 0);
		return display;
	}

	[[nodiscard]] pointer operator->() const {
		return &**this;
	}

	DisplayIterator& operator++() {
		GREM_ASSERT(p);
		++p;
		display = Display{*p};
		return *this;
	}

	DisplayIterator operator++(int) {
		DisplayIterator old = *this;
		++*this;
		return old;
	}

private:
	friend DisplayRange;

	constexpr explicit DisplayIterator(uint32_t* p) noexcept
		: p(p)
		, display(*p) {}

	uint32_t* p = nullptr;
	Display display{0};
};

class Display::DisplayRange {
public:
	using value_type = Display;
	using size_type = size_t;
	using difference_type = DisplayIterator::difference_type;
	using reference = DisplayIterator::reference;
	using const_reference = reference;
	using pointer = DisplayIterator::pointer;
	using const_pointer = pointer;
	using iterator = DisplayIterator;
	using const_iterator = iterator;
	using sentinel = DisplaySentinel;

	[[nodiscard]] iterator begin() const noexcept {
		return iterator{displayIDs.get()};
	}

	[[nodiscard]] sentinel end() const noexcept {
		return sentinel{};
	}

	[[nodiscard]] size_type size() const noexcept {
		return static_cast<size_type>(displayCount);
	}

	[[nodiscard]] bool empty() const noexcept {
		return displayCount == 0;
	}

private:
	friend Display;

	struct DisplayIDsDeleter {
		GREM_API(graphics) void operator()(uint32_t* displayIDs) const noexcept;
	};

	constexpr DisplayRange(uint32_t* displayIDs, int displayCount) noexcept
		: displayIDs(displayIDs)
		, displayCount(displayCount) {}

	UniqueHandle<uint32_t*, DisplayIDsDeleter> displayIDs;
	int displayCount;
};

struct Display::DisplayModeSentinel {};

class Display::DisplayModeIterator {
public:
	using difference_type = ptrdiff_t;
	using value_type = DisplayMode;
	using reference = const value_type&;
	using pointer = const value_type*;
	using iterator_category = std::forward_iterator_tag;
	using sentinel = DisplayModeSentinel;

	DisplayModeIterator() noexcept = default;

	[[nodiscard]] bool operator==(const DisplayModeIterator& other) const {
		return p == other.p;
	}

	[[nodiscard]] bool operator==(sentinel) const {
		GREM_ASSERT(p);
		return *p == nullptr;
	}

	[[nodiscard]] reference operator*() const {
		GREM_ASSERT(p && *p != nullptr);
		return displayMode;
	}

	[[nodiscard]] pointer operator->() const {
		return &**this;
	}

	DisplayModeIterator& operator++();

	DisplayModeIterator operator++(int) {
		DisplayModeIterator old = *this;
		++*this;
		return old;
	}

private:
	friend DisplayModeRange;

	DisplayModeIterator(void** p);

	void** p = nullptr;
	DisplayMode displayMode = createDisplayMode(0, 0, {}, 0.0f, 0.0f, 0, 0, nullptr);
};

class Display::DisplayModeRange {
public:
	using value_type = DisplayMode;
	using size_type = size_t;
	using difference_type = DisplayModeIterator::difference_type;
	using reference = DisplayModeIterator::reference;
	using const_reference = reference;
	using pointer = DisplayModeIterator::pointer;
	using const_pointer = pointer;
	using iterator = DisplayModeIterator;
	using const_iterator = iterator;
	using sentinel = DisplayModeSentinel;

	[[nodiscard]] iterator begin() const noexcept {
		return iterator{displayModes.get()};
	}

	[[nodiscard]] sentinel end() const noexcept {
		return sentinel{};
	}

	[[nodiscard]] size_type size() const noexcept {
		return static_cast<size_type>(displayModeCount);
	}

	[[nodiscard]] bool empty() const noexcept {
		return displayModeCount == 0;
	}

private:
	friend Display;

	struct DisplayModesDeleter {
		GREM_API(graphics) void operator()(void** displayModes) const noexcept;
	};

	constexpr DisplayModeRange(void** displayModes, int displayModeCount) noexcept
		: displayModes(displayModes)
		, displayModeCount(displayModeCount) {}

	UniqueHandle<void**, DisplayModesDeleter> displayModes;
	int displayModeCount;
};

} // namespace grem::graphics

#endif
