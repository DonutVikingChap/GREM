// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_SWAPCHAIN_HPP
#define GREM_GRAPHICS_SWAPCHAIN_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/Texture.hpp>

namespace grem::graphics {

class Device; // Forward declaration, to avoid a circular include of Device.hpp.
class Window; // Forward declaration, to avoid including Window.hpp.

/**
 * Configuration options for a Swapchain.
 */
struct SwapchainOptions {
	/**
	 * Maximum number of additional swapchain frames to buffer and render while
	 * waiting for the oldest submitted frame to finish being rendered.
	 *
	 * Higher values may improve average framerate and frametime consistency at
	 * the cost of potentially higher input lag.
	 *
	 * \note Recommended values are:
	 *       - 0 to prioritize low input lag, or
	 *       - 1 to prioritize smoothness.
	 * \note This option only applies to certain graphics backends.
	 */
	uint32_t maxBufferedFrameCount = 0;

	/**
	 * Whether the swapchain should use vertical synchronization (VSync) or not.
	 *
	 * VSync ensures that any frame submitted for presentation is delayed until
	 * the previously submitted frame has finished being presented. This
	 * eliminates the tearing artifacts that may otherwise occur due to swapping
	 * the front buffer in the middle of a screen refresh, at the cost of higher
	 * latency and potentially limiting the frame rate.
	 *
	 * \note Enabling VSync is not recommended for applications that are
	 *       sensitive to input lag, like first person games, since it can
	 *       introduce a significant amount of latency.
	 */
	bool useVerticalSynchronization =
#ifdef __EMSCRIPTEN__
		true
#else
		false
#endif
		;
};

/**
 * Render target texture holding a chain of images leading to a window to be
 * presented to.
 */
class Swapchain : public Texture {
public:
	/**
	 * Construct a swapchain.
	 *
	 * \param device device to create the swapchain for. Must outlive the
	 *        swapchain.
	 * \param window window that the swapchain presents to.
	 * \param options initial configuration of the swapchain, see
	 *        SwapchainOptions.
	 */
	GREM_API(graphics) Swapchain(Device& device, Window& window, const SwapchainOptions& options = {});

	/** Destructor */
	~Swapchain() = default;

	/** Copying a swapchain is not allowed. */
	Swapchain(const Swapchain&) = delete;

	/** Moving a swapchain is not allowed. */
	Swapchain(Swapchain&&) = delete;

	/** Copying a swapchain is not allowed. */
	Swapchain& operator=(const Swapchain&) = delete;

	/** Moving a swapchain is not allowed. */
	Swapchain& operator=(Swapchain&&) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset, Region3D sourceRegion) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void pasteImage(Extent3D imageSize, const void* pixels, Offset3D destinationOffset = {.x = 0, .y = 0, .z = 0}, Offset3D sourceOffset = {.x = 0, .y = 0, .z = 0}) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void pasteTexture(const Texture& texture, Offset3D destinationOffset, Region3D sourceRegion) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void pasteTexture(const Texture& texture, Offset3D destinationOffset = {.x = 0, .y = 0, .z = 0}, Offset3D sourceOffset = {.x = 0, .y = 0, .z = 0}) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void fill(const ClearValues& values = {}) = delete;

	/** Directly modifying a swapchain is not allowed. */
	void generateMipmap() = delete;

	/** Copying a swapchain is not allowed. */
	[[nodiscard]] Texture copy() const = delete;

	/** Copying a swapchain is not allowed. */
	[[nodiscard]] Texture copyWithSamplerOptions(Optional<TextureSamplerOptions> newSamplerOptions) const = delete;

	/**
	 * Enable or disable vertical synchronization.
	 *
	 * \param useVerticalSynchronization true to enable VSync, false to disable.
	 *
	 * \sa SwapchainOptions::useVerticalSynchronization
	 * \sa isVerticalSynchronizationEnabled()
	 */
	GREM_API(graphics) void setVerticalSynchronizationEnabled(bool useVerticalSynchronization);

	/**
	 * Set the maximum buffered frame count.
	 *
	 * \param maxBufferedFrameCount new maximum number of buffered frames.
	 *
	 * \note This function only has an effect on certain graphics backends.
	 *
	 * \sa SwapchainOptions::maxBufferedFrameCount
	 * \sa getMaxBufferedFrameCount()
	 */
	GREM_API(graphics) void setMaxBufferedFrameCount(uint32_t maxBufferedFrameCount);

	/**
	 * Check if vertical synchronization is enabled.
	 *
	 * \return true if VSync is enabled, false otherwise.
	 *
	 * \sa SwapchainOptions::useVerticalSynchronization
	 * \sa setVerticalSynchronizationEnabled()
	 */
	[[nodiscard]] GREM_API(graphics) bool isVerticalSynchronizationEnabled() const;

	/**
	 * Get the maximum buffered frame count.
	 *
	 * \return the maximum number of buffered frames, or 0 if the current
	 *         graphics backend does not support limiting the number of buffered
	 *         frames.
	 *
	 * \sa SwapchainOptions::maxBufferedFrameCount
	 * \sa setMaxBufferedFrameCount()
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t getMaxBufferedFrameCount() const;
};

} // namespace grem::graphics

#endif
