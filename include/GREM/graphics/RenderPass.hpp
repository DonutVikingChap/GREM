// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_RENDER_PASS_HPP
#define GREM_GRAPHICS_RENDER_PASS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

#include <utility> // std::move

namespace grem::graphics {

class Device; // Forward declaration, to avoid a circular include of Device.hpp.

struct RenderPassImplementation; ///< Backend-specific implementation of RenderPass.

/**
 * List of draw commands to be rendered onto a set of render targets using a
 * Device.
 */
class RenderPass {
public:
	/**
	 * Performance-related statistics of a render pass.
	 */
	struct Statistics {
		/**
		 * Total number of vertices processed in the render pass, including the
		 * duplicated vertices produced by instancing.
		 */
		size_t totalDrawnVertexCount = 0;

		/**
		 * Total number of vertex indices processed in the render pass,
		 * including the duplicated indices produced by instancing.
		 *
		 * \note Non-indexed meshes add their raw vertex count to this counter
		 *       instead.
		 */
		size_t totalDrawnIndexCount = 0;

		/**
		 * Total number of mesh instances processed in the render pass.
		 */
		size_t totalDrawnInstanceCount = 0;

		/**
		 * Total number of draw calls to the underlying graphics API required by
		 * the render pass for the current graphics backend.
		 */
		size_t totalDrawCallCount = 0;

		/**
		 * Compare this set of render pass statistics against another for
		 * equality.
		 *
		 * \param other statistics to compare against.
		 *
		 * \return true if the statistics are equal, false otherwise.
		 */
		[[nodiscard]] constexpr bool operator==(const Statistics& other) const noexcept = default;

		/**
		 * Add another set of render pass statistics to this one.
		 *
		 * \param other statistics to add.
		 *
		 * \return `*this`, for chaining.
		 */
		constexpr Statistics& operator+=(const Statistics& other) noexcept {
			totalDrawnVertexCount += other.totalDrawnVertexCount;
			totalDrawnIndexCount += other.totalDrawnIndexCount;
			totalDrawnInstanceCount += other.totalDrawnInstanceCount;
			totalDrawCallCount += other.totalDrawCallCount;
			return *this;
		}

		/**
		 * Subtract another set of render pass statistics from this one, using
		 * unsigned modular arithmetic.
		 *
		 * \param other statistics to subtract.
		 *
		 * \return `*this`, for chaining.
		 */
		constexpr Statistics& operator-=(const Statistics& other) noexcept {
			totalDrawnVertexCount -= other.totalDrawnVertexCount;
			totalDrawnIndexCount -= other.totalDrawnIndexCount;
			totalDrawnInstanceCount -= other.totalDrawnInstanceCount;
			totalDrawCallCount -= other.totalDrawCallCount;
			return *this;
		}

		/**
		 * Add two sets of render pass statistics together.
		 *
		 * \param a first statistics set.
		 * \param b second statistics set.
		 *
		 * \return the sum of the statistics sets.
		 */
		[[nodiscard]] friend constexpr Statistics operator+(const Statistics& a, const Statistics& b) noexcept {
			Statistics result = a;
			result += b;
			return result;
		}

		/**
		 * Subtract two sets of render pass statistics to produce the difference
		 * between them, using unsigned modular arithmetic.
		 *
		 * \param a first statistics set.
		 * \param b second statistics set.
		 *
		 * \return the difference between the statistics sets.
		 */
		[[nodiscard]] friend constexpr Statistics operator-(const Statistics& a, const Statistics& b) noexcept {
			Statistics result = a;
			result -= b;
			return result;
		}
	};

	/**
	 * Construct a render pass.
	 *
	 * \param device device to create the render pass for. Must outlive the
	 *        render pass.
	 * \param renderTargets set of render targets to render to. Must not be
	 *        empty. All textures must have the same 2D size, which must fit in
	 *        `device.getSupportedFeatures().maxFramebufferSize`. The texture
	 *        aspects must not overlap between targets. If any target is
	 *        multisampled, all other targets must also be multisampled with the
	 *        same number of samples.
	 * \param clearMode one of RetainValues, ClearValues or
	 *        UndefinedClearValues, specifying whether/how to retain/clear the
	 *        render target contents at the beginning of the render pass.
	 * \param viewport initial viewport to use for subsequently recorded draw
	 *        commands, or an empty optional to choose a default viewport that
	 *        fits the render targets.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The referenced render targets must remain valid for as long as
	 *          the render pass is used for rendering.
	 *
	 * \note Using UndefinedClearValues may allow for optimizations in the
	 *       graphics backend, since a new renderbuffer can be allocated without
	 *       copying any old values. This is useful in cases where the initial
	 *       render target contents do not matter for the render pass, such as
	 *       when the draw commands will completely overwrite the entire
	 *       framebuffer.
	 */
	GREM_API(graphics) RenderPass(Device& device, Span<const TextureSubresourceReference> renderTargets, const ClearMode& clearMode, Optional<Viewport> viewport = {});

	/**
	 * Construct a render pass.
	 *
	 * \param device device to create the render pass for. Must outlive the
	 *        render pass.
	 * \param renderTarget render target to render to, whose 2D size must fit in
	 *        `device.getSupportedFeatures().maxFramebufferSize`.
	 * \param clearMode one of RetainValues, ClearValues or
	 *        UndefinedClearValues, specifying whether/how to retain/clear the
	 *        render target contents at the beginning of the render pass.
	 * \param viewport initial viewport to use for subsequently recorded draw
	 *        commands, or an empty optional to choose a default viewport that
	 *        fits the render target.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The referenced render target must remain valid for as long as
	 *          the render pass is used for rendering.
	 *
	 * \note Using UndefinedClearValues may allow for optimizations in the
	 *       graphics backend, since a new renderbuffer can be allocated without
	 *       copying any old values. This is useful in cases where the initial
	 *       render target contents do not matter for the render pass, such as
	 *       when the draw commands will completely overwrite the entire
	 *       framebuffer.
	 */
	GREM_ALWAYS_INLINE RenderPass(Device& device, TextureSubresourceReference renderTarget, const ClearMode& clearMode, Optional<Viewport> viewport = {})
		: RenderPass(device, Span{&renderTarget, 1}, clearMode, viewport) {}

	/**
	 * Construct a multisampled render pass that resolves to a resolve target at
	 * the end of the render.
	 *
	 * \param device device to create the render pass for. Must outlive the
	 *        render pass.
	 * \param resolveTarget resolve target to resolve the color target to at the
	 *        end of each render. Must have the color texture aspect. Must not
	 *        be multisampled.
	 * \param resolveMode one of StoreIntermediateValues or
	 *        DiscardIntermediateValues, specifying whether/how to store/discard
	 *        the intermediate results after the resolve. Discarding the values
	 *        may help the driver to optimize memory usage, while storing them
	 *        allows the intermediate color/depth-stencil buffers to be sampled
	 *        or used in further render passes.
	 * \param renderTargets set of render targets to render to. Must not be
	 *        empty. All textures must have the same 2D size, which must fit in
	 *        `device.getSupportedFeatures().maxFramebufferSize`. The texture
	 *        aspects must not overlap between targets. All targets must be
	 *        multisampled and use the same number of samples. Must contain a
	 *        target with the color texture aspect.
	 * \param clearMode one of RetainValues, ClearValues or
	 *        UndefinedClearValues, specifying whether/how to retain/clear the
	 *        render target contents at the beginning of the render pass.
	 * \param viewport initial viewport to use for subsequently recorded draw
	 *        commands, or an empty optional to choose a default viewport that
	 *        fits the render targets.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The referenced resolve target and render targets must remain
	 *          valid for as long as the render pass is used for rendering.
	 */
	GREM_API(graphics)
	RenderPass(Device& device, TextureSubresourceReference resolveTarget, const ResolveMode& resolveMode, Span<const TextureSubresourceReference> renderTargets,
		const ClearMode& clearMode, Optional<Viewport> viewport = {});

	/**
	 * Construct a multisampled render pass that resolves to a resolve target at
	 * the end of the render.
	 *
	 * \param device device to create the render pass for. Must outlive the
	 *        render pass.
	 * \param resolveTarget resolve target to resolve the color target to at the
	 *        end of each render. Must have the color texture aspect. Must not
	 *        be multisampled.
	 * \param resolveMode one of StoreIntermediateValues or
	 *        DiscardIntermediateValues, specifying whether/how to store/discard
	 *        the intermediate results after the resolve. Discarding the values
	 *        may help the driver to optimize memory usage, while storing them
	 *        allows the intermediate color/depth-stencil buffers to be sampled
	 *        or used in further render passes.
	 * \param renderTarget render target to render to, whose 2D size must fit in
	 *        `device.getSupportedFeatures().maxFramebufferSize`. Must be
	 *        multisampled. Must have the color texture aspect.
	 * \param clearMode one of RetainValues, ClearValues or
	 *        UndefinedClearValues, specifying whether/how to retain/clear the
	 *        render target contents at the beginning of the render pass.
	 * \param viewport initial viewport to use for subsequently recorded draw
	 *        commands, or an empty optional to choose a default viewport that
	 *        fits the render target.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The referenced render target must remain valid for as long as
	 *          the render pass is used for rendering.
	 */
	GREM_ALWAYS_INLINE RenderPass(Device& device, TextureSubresourceReference resolveTarget, const ResolveMode& resolveMode, TextureSubresourceReference renderTarget,
		const ClearMode& clearMode, Optional<Viewport> viewport = {})
		: RenderPass(device, resolveTarget, resolveMode, Span{&renderTarget, 1}, clearMode, viewport) {}

	/** Destructor. */
	GREM_API(graphics) ~RenderPass();

	/** Copying a render pass is not allowed. */
	RenderPass(const RenderPass&) = delete;

	/** Moving a render pass is not allowed. */
	RenderPass(RenderPass&&) = delete;

	/** Copying a render pass is not allowed. */
	RenderPass& operator=(const RenderPass&) = delete;

	/** Moving a render pass is not allowed. */
	RenderPass& operator=(RenderPass&&) = delete;

	/**
	 * Set the viewport and scissor area to use for subsequently recorded draw
	 * commands.
	 *
	 * \param viewport region of the render targets to render to.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) RenderPass& setViewport(const Viewport& viewport);

	/**
	 * Set the scissor area to use for subsequently recoreded draw commands.
	 *
	 * \param scissor rectangular region outside of which any attempts to render
	 *        a pixel will be discarded, or an empty optional to use the full
	 *        viewport area.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The scissor area will be reset on the next viewport change.
	 *
	 * \sa setViewport()
	 */
	GREM_API(graphics) RenderPass& setViewportScissor(const Optional<Region2D>& scissor);

	/**
	 * Get the viewport that will be used for subsequently recorded draw
	 * commands.
	 *
	 * \return a read-only reference to the current viewport, valid until the
	 *         next non-const member function call on the render pass, or until
	 *         the render pass is destroyed, whichever happens first.
	 */
	[[nodiscard]] GREM_API(graphics) const Viewport& getViewport();

	/**
	 * Get the size of the framebuffer that the render pass will be rendering
	 * through.
	 *
	 * \return the framebuffer size of the render pass.
	 */
	[[nodiscard]] GREM_API(graphics) Extent2D getFramebufferSize();

	/**
	 * Record a command to fill the contents of a rectangular region of the
	 * render targets.
	 *
	 * \param targetRegion rectangular area of the render targets to fill. Must
	 *        be a valid region of the render targets.
	 * \param values values to fill the specified render target aspects with.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) RenderPass& fill(const Region2D& targetRegion, const ClearValues& values);

	/**
	 * Record a command to fill the full contents of the render targets.
	 *
	 * \param values values to fill the specified render target aspects with.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) RenderPass& fill(const ClearValues& values) {
		return fill(Region2D{.size = getFramebufferSize()}, values);
	}

	/**
	 * Record the commands to draw the contents of a draw command buffer using a
	 * set of shader buffers to the current viewport region of the render
	 * targets.
	 *
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param instanceBuffer the instance buffer referenced by the draw command
	 *        buffer.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename Instance, typename... Buffers>
	RenderPass& draw(const DrawCommandBuffer<Mesh<Vertex, Index, Parameters, Instance>>& drawCommandBuffer, const InstanceBuffer<Instance>& instanceBuffer,
		const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return draw(drawCommandBuffer.lock(), instanceBuffer.lock(), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of a draw command buffer using a
	 * set of shader buffers to the current viewport region of the render
	 * targets.
	 *
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename... Buffers>
	RenderPass& draw(const DrawCommandBuffer<Mesh<Vertex, Index, Parameters, NoInstance>>& drawCommandBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return draw(drawCommandBuffer.lock(), SharedPointer<InstanceBufferImplementation>{}, bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of a draw command buffer using a
	 * set of shader buffers to the current viewport region of the render
	 * targets.
	 *
	 * \param drawCommandBufferHandle handle to the buffer of draw commands to
	 *        enqueue.
	 * \param instanceBufferHandle handle to the instance buffer referenced by
	 *        the draw command buffer, or nullptr if the draw command buffer
	 *        does not have a corresponding instance buffer.
	 * \param bufferHandles mapping from unique buffer layout references to
	 *        handles to the buffers required by the shaders used in the draw
	 *        command buffer, sorted by the first element of the pair.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the buffer handles.
	 * \warning All buffer layout references must be acquired using
	 *          `Buffer::LAYOUT_REFERENCE`.
	 */
	RenderPass& draw(SharedPointer<DrawCommandBufferImplementation> drawCommandBufferHandle, SharedPointer<InstanceBufferImplementation> instanceBufferHandle,
		Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
		return drawShaded({}, std::move(drawCommandBufferHandle), std::move(instanceBufferHandle), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of a draw command buffer with
	 * specific shaders using a set of shader buffers to the current viewport
	 * region of the render targets.
	 *
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param instanceBuffer the instance buffer referenced by the draw command
	 *        buffer.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename Instance, typename... Buffers>
	RenderPass& drawShaded(const ShaderPipeline<Mesh<Vertex, Index, Parameters, Instance>>& shaderPipelineOverride,
		const DrawCommandBuffer<Mesh<Vertex, Index, Parameters, Instance>>& drawCommandBuffer, const InstanceBuffer<Instance>& instanceBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawShaded(shaderPipelineOverride.lock(), drawCommandBuffer.lock(), instanceBuffer.lock(), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of a draw command buffer with
	 * specific shaders using a set of shader buffers to the current viewport
	 * region of the render targets.
	 *
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename... Buffers>
	RenderPass& drawShaded(const ShaderPipeline<Mesh<Vertex, Index, Parameters, NoInstance>>& shaderPipelineOverride,
		const DrawCommandBuffer<Mesh<Vertex, Index, Parameters, NoInstance>>& drawCommandBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawShaded(shaderPipelineOverride.lock(), drawCommandBuffer.lock(), SharedPointer<InstanceBufferImplementation>{}, bufferHandles);
	}

	/**
	 * Record a command to draw the contents of a draw command buffer with
	 * specific shaders using a set of shader buffers to the current viewport
	 * region of the render targets.
	 *
	 * \param shaderPipelineOverrideHandle handle to the shader pipeline
	 *        override to use, or nullptr to not use a shader override. Must use
	 *        the same mesh type as the draw command buffer if set.
	 * \param drawCommandBufferHandle handle to the buffer of draw commands to
	 *        enqueue.
	 * \param instanceBufferHandle handle to the instance buffer referenced by
	 *        the draw command buffer, or nullptr if the draw command buffer
	 *        does not have a corresponding instance buffer.
	 * \param bufferHandles mapping from unique buffer layout references to
	 *        handles to the buffers required by the shaders used in the draw
	 *        command buffer, sorted by the first element of the pair.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the buffer handles.
	 * \warning All buffer layout references must be acquired using
	 *          `Buffer::LAYOUT_REFERENCE`.
	 */
	GREM_API(graphics)
	RenderPass& drawShaded(SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle, SharedPointer<DrawCommandBufferImplementation> drawCommandBufferHandle,
		SharedPointer<InstanceBufferImplementation> instanceBufferHandle, Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles);

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer using a set of shader buffers to the current viewport region of
	 * the render targets.
	 *
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param instanceBuffer the instance buffer referenced by the draw command
	 *        buffer.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename Instance, typename... Buffers>
	RenderPass& drawUnordered(const UnorderedDrawCommandBuffer<Mesh<Vertex, Index, Parameters, Instance>>& drawCommandBuffer, const InstanceBuffer<Instance>& instanceBuffer,
		const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{&Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawUnordered(drawCommandBuffer.lock(), instanceBuffer.lock(), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer using a set of shader buffers to the current viewport region of
	 * the render targets.
	 *
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename... Buffers>
	RenderPass& drawUnordered(const UnorderedDrawCommandBuffer<Mesh<Vertex, Index, Parameters, NoInstance>>& drawCommandBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{&Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawUnordered(drawCommandBuffer.lock(), SharedPointer<InstanceBufferImplementation>{}, bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer using a set of shader buffers to the current viewport region of
	 * the render targets.
	 *
	 * \param drawCommandBufferHandle handle to the unordered buffer of draw
	 *        commands to enqueue.
	 * \param instanceBufferHandle handle to the instance buffer referenced by
	 *        the draw command buffer, or nullptr if the draw command buffer
	 *        does not have a corresponding instance buffer.
	 * \param bufferHandles mapping from unique buffer layout references to
	 *        handles to the buffers required by the shaders used in the draw
	 *        command buffer, sorted by the first element of the pair.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the buffer handles.
	 * \warning All buffer layout references must be acquired using
	 *          `Buffer::LAYOUT_REFERENCE`.
	 */
	RenderPass& drawUnordered(SharedPointer<UnorderedDrawCommandBufferImplementation> drawCommandBufferHandle, SharedPointer<InstanceBufferImplementation> instanceBufferHandle,
		Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
		return drawShadedUnordered({}, std::move(drawCommandBufferHandle), std::move(instanceBufferHandle), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer with specific shaders using a set of shader buffers to the current
	 * viewport region of the render targets.
	 *
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param instanceBuffer the instance buffer referenced by the draw command
	 *        buffer.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename Instance, typename... Buffers>
	RenderPass& drawShadedUnordered(const ShaderPipeline<Mesh<Vertex, Index, Parameters, Instance>>& shaderPipelineOverride,
		const UnorderedDrawCommandBuffer<Mesh<Vertex, Index, Parameters, Instance>>& drawCommandBuffer, const InstanceBuffer<Instance>& instanceBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawShadedUnordered(shaderPipelineOverride.lock(), drawCommandBuffer.lock(), instanceBuffer.lock(), bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer with specific shaders using a set of shader buffers to the current
	 * viewport region of the render targets.
	 *
	 * \param shaderPipelineOverride shader pipeline override to use.
	 * \param drawCommandBuffer the buffer of draw commands to enqueue.
	 * \param buffers buffers required by the shaders used in the draw command
	 *        buffer.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the parameter pack.
	 */
	template <typename Vertex, typename Index, typename Parameters, typename... Buffers>
	RenderPass& drawShadedUnordered(const ShaderPipeline<Mesh<Vertex, Index, Parameters, NoInstance>>& shaderPipelineOverride,
		const UnorderedDrawCommandBuffer<Mesh<Vertex, Index, Parameters, NoInstance>>& drawCommandBuffer, const Buffers&... buffers) {
		Array<Pair<BufferLayoutReference, SharedPointer<void>>, sizeof...(Buffers)> bufferHandles{
			Pair<BufferLayoutReference, SharedPointer<void>>{Buffers::LAYOUT_REFERENCE, buffers.lock()}...,
		};
		sortBufferHandles(bufferHandles);
		return drawShadedUnordered(shaderPipelineOverride.lock(), drawCommandBuffer.lock(), SharedPointer<InstanceBufferImplementation>{}, bufferHandles);
	}

	/**
	 * Record the commands to draw the contents of an unordered draw command
	 * buffer with specific shaders using a set of shader buffers to the current
	 * viewport region of the render targets.
	 *
	 * \param shaderPipelineOverrideHandle handle to the shader pipeline
	 *        override to use, or nullptr to not use a shader override. Must use
	 *        the same mesh type as the draw command buffer if set.
	 * \param drawCommandBufferHandle handle to the unordered buffer of draw
	 *        commands to enqueue.
	 * \param instanceBufferHandle handle to the instance buffer referenced by
	 *        the draw command buffer, or nullptr if the draw command buffer
	 *        does not have a corresponding instance buffer.
	 * \param bufferHandles mapping from unique buffer layout references to
	 *        handles to the buffers required by the shaders used in the draw
	 *        command buffer, sorted by the first element of the pair.
	 *
	 * \return `*this`, for chaining.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning All instance buffer ranges specified by the draw command buffer
	 *          must be valid ranges of instances in the given instance buffer.
	 * \warning All buffers required by the shaders used in the draw command
	 *          buffer must be provided through the buffer handles.
	 * \warning All buffer layout references must be acquired using
	 *          `Buffer::LAYOUT_REFERENCE`.
	 */
	GREM_API(graphics)
	RenderPass& drawShadedUnordered(SharedPointer<ShaderPipelineImplementation> shaderPipelineOverrideHandle,
		SharedPointer<UnorderedDrawCommandBufferImplementation> drawCommandBufferHandle, SharedPointer<InstanceBufferImplementation> instanceBufferHandle,
		Span<const Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles);

	/**
	 * Get the current statistics of the render pass.
	 */
	[[nodiscard]] GREM_API(graphics) Statistics getStatistics() const;

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<RenderPassImplementation> lock() const {
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] RenderPassImplementation* get() const noexcept {
		return implementation.get();
	}

private:
	static void sortBufferHandles(Span<Pair<BufferLayoutReference, SharedPointer<void>>> bufferHandles) {
		sortByAscending<&Pair<BufferLayoutReference, SharedPointer<void>>::first>(bufferHandles);
	}

	SharedPointer<RenderPassImplementation> implementation{};
};

} // namespace grem::graphics

#endif
