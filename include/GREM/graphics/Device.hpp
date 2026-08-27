// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_DEVICE_HPP
#define GREM_GRAPHICS_DEVICE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Texture.hpp>

namespace grem {

class Filesystem; // Forward declaration, to avoid including Filesystem.hpp.

} // namespace grem

namespace grem::graphics {

class Window;    // Forward declaration, to avoid including Window.hpp.
class Swapchain; // Forward declaration, to avoid a circular include of Swapchain.hpp.

struct DeviceImplementation; ///< Backend-specific implementation of Device.

/**
 * Identifier for a frame that has been submitted to a Device for presentation.
 */
enum class PresentationSubmissionID : uint64_t {};

/**
 * Configuration options for a Device.
 */
struct DeviceOptions {
	/**
	 * Virtual input filepath to load the shader cache from when the rendering
	 * context is created, or an empty string to not load the shader cache from
	 * the filesystem.
	 *
	 * \note This option only applies to certain graphics backends.
	 */
	CStringView shaderCacheInputFilepath = "shader_cache.dat";

	/**
	 * Virtual output filepath to save the shader cache to when the rendering
	 * context is destroyed, or an empty string to not save the shader cache to
	 * the filesystem.
	 *
	 * \note This option only applies to certain graphics backends.
	 * \note When saving, any parent directories of the specified output
	 *       filepath will be created if they don't already exist.
	 */
	CStringView shaderCacheOutputFilepath = shaderCacheInputFilepath;
};

/**
 * Persistent system managing the rendering context of a Window.
 *
 * An instance of this class should typically be kept throughout the lifetime of
 * the application in order to continuously render the visual state of the
 * latest produced frame through the application::Application::display()
 * callback.
 */
class Device {
public:
	/**
	 * Information about a frame that has been submitted for presentation.
	 */
	struct PresentationSubmission {
		/**
		 * Identifier for the frame.
		 */
		PresentationSubmissionID id;

		/**
		 * Total accumulated statistics of all render passes rendered in the
		 * frame.
		 */
		RenderPass::Statistics totalRenderPassStatistics{};

		/**
		 * Estimation of the total time spent waiting for resources to become
		 * available while preparing the frame.
		 */
		Duration totalWaitTime{};

		/**
		 * Total number of render passes rendered in the frame.
		 */
		size_t totalRenderPassCount = 0;

		/**
		 * Total number of texture blits performed in the frame.
		 */
		size_t totalBlitCount = 0;
	};

	/**
	 * Construct a device.
	 *
	 * \param window window for which to manage the rendering context, and to
	 *        which frames presented through this device will be rendered. Must
	 *        outlive the device.
	 * \param options initial configuration of the device, see DeviceOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) explicit Device(Window& window, const DeviceOptions& options = {});

	/**
	 * Construct a device with an associated filesystem, allowing for graphics
	 * pipelines to be cached.
	 *
	 * \param filesystem filesystem to load the shader and graphics pipeline
	 *        caches from, and save the caches to, when the device is destroyed,
	 *        if caching is supported by the implementation.
	 * \param window window for which to manage the rendering context, and to
	 *        which frames presented through this device will be rendered. Must
	 *        outlive the device.
	 * \param options initial configuration of the device, see DeviceOptions.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Loading/saving the pipeline cache might silently fail if the file
	 *       doesn't exist or couldn't be written.
	 */
	GREM_API(graphics) Device(Filesystem& filesystem, Window& window, const DeviceOptions& options = {});

	/** Destructor. */
	GREM_API(graphics) ~Device();

	/** Copying a device is not allowed. */
	Device(const Device&) = delete;

	/** Moving a device is not allowed. */
	Device(Device&&) = delete;

	/** Copying a device is not allowed. */
	Device& operator=(const Device&) = delete;

	/** Moving a device is not allowed. */
	Device& operator=(Device&&) = delete;

	/**
	 * Copy and paste a rectangular region of a texture onto a region of another
	 * texture.
	 *
	 * \param renderTarget target region to paste to. Must reference a valid
	 *        region of a non-multisampled texture.
	 * \param renderSource source region to copy from. Must reference a valid
	 *        region of a non-multisampled texture.
	 * \param filter texture filtering method to use if the target and source
	 *        extents don't match.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Use the unscaled version of blit() instead to allow for
	 *       multisampled textures.
	 */
	GREM_API(graphics) void blit(TextureRegion2DReference renderTarget, TextureRegion2DConstReference renderSource, TextureFilter filter);

	/**
	 * Copy and paste a rectangular region of a texture at its original size
	 * onto another texture at a specific offset.
	 *
	 * \param renderTarget target to paste to. Must reference a valid
	 *        subresource of a valid texture.
	 * \param targetOffset offset, in texels, from the bottom left corner of the
	 *        target texture at which to paste the copied texture region.
	 * \param renderSource source region to copy from. Must reference a valid
	 *        region of a valid texture that fits into the render target at the
	 *        specified offset.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This version of blit() may be used to resolve a multisampled
	 *       texture into a non-multisampled one. However, for better
	 *       performance, it should generally be preferred to use a resolve
	 *       render target directly in the render pass that performed the
	 *       multisampling instead.
	 *
	 * \warning If the source texture is multisampled and the target texture is
	 *          not, the target and source textures must have the same texture
	 *          format.
	 */
	GREM_API(graphics) void blit(TextureSubresourceReference renderTarget, Offset2D targetOffset, TextureRegion2DConstReference renderSource);

	/**
	 * Copy and paste a rectangular region of a texture at its original size
	 * onto another texture at its bottom left corner.
	 *
	 * \param renderTarget target to paste to. Must reference a valid
	 *        subresource of a valid texture.
	 * \param renderSource source region to copy from. Must reference a valid
	 *        region of a valid texture that fits into the render target.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This version of blit() may be used to resolve a multisampled
	 *       texture into a non-multisampled one. However, for better
	 *       performance, it should generally be preferred to use a resolve
	 *       render target directly in the render pass that performed the
	 *       multisampling instead.
	 *
	 * \warning If the source texture is multisampled and the target texture is
	 *          not, the target and source textures must have the same texture
	 *          format.
	 */
	void blit(TextureSubresourceReference renderTarget, TextureRegion2DConstReference renderSource) {
		blit(renderTarget, Offset2D{.x = 0, .y = 0}, renderSource);
	}

	/**
     * Submit a list of graphics commands to be rendered.
     *
     * \param renderPass list of commands to execute.
	 *
	 * \throws graphics::Error on failure to render the render pass.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
     */
	GREM_API(graphics) void render(const RenderPass& renderPass);

	/**
     * Wait for all submitted graphics commands to finish being rendered and
	 * presented.
     */
	GREM_API(graphics) void await() noexcept;

	/**
	 * Wait for a specific frame to finish being rendered and presented.
	 *
	 * \param swapchain swapchain that the frame was presented to.
	 * \param presentationSubmissionID identifier for the frame to wait for.
	 *        Must be a valid identifier acquired from a call to present() on
	 *        this device with the given swapchain.
	 * \param timeout maximum duration to wait before returning early, or a
	 *        non-positive value for an infinite timeout.
	 *
	 * \return true if the frame was successfully awaited, false otherwise.
	 *
	 * \note This function requires frame waiting to be supported, which can be
	 *       queried using `device.getSupportedFeatures().supportsAwaitPresentation`.
	 *       If the feature is not supported, awaitPresentation() will
	 *       immediately return false.
	 */
	GREM_API(graphics) bool awaitPresentation(const Swapchain& swapchain, PresentationSubmissionID presentationSubmissionID, Duration timeout = {}) noexcept;

	/**
     * Submit the current back buffer of a swapchain for display to its
	 * associated window, and acquire the next back buffer frame of the
	 * swapchain for subsequent rendering.
	 *
	 * \param swapchain swapchain to present the back buffer of.
	 *
	 * \throws graphics::Error on failure to acquire the next frame.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \return information about the submitted frame.
     */
	GREM_API(graphics) PresentationSubmission present(Swapchain& swapchain);

	/**
	 * Get the features supported by the device.
	 *
	 * \return a read-only reference to the supported features.
	 */
	[[nodiscard]] GREM_API(graphics) const FeatureSupport& getSupportedFeatures() const noexcept;

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] DeviceImplementation* get() const noexcept {
		return implementation.get();
	}

private:
	UniquePointer<DeviceImplementation> implementation;
};

} // namespace grem::graphics

#endif
