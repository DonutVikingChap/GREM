// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_IMGUI_GRAPHICAL_USER_INTERFACE_HPP
#define GREM_IMGUI_GRAPHICAL_USER_INTERFACE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>

struct ImGuiContext;  // Forward declaration, to avoid including imgui.h.
struct ImGuiViewport; // Forward declaration, to avoid including imgui.h.
struct ImDrawData;    // Forward declaration, to avoid including imgui.h.

namespace grem {
class Filesystem; // Forward declaration, to avoid including Filesystem.hpp.
} // namespace grem

namespace grem::events {
struct Event;    // Forward declaration, to avoid including Event.hpp.
class EventPump; // Forward declaration, to avoid including EventPump.hpp.
} // namespace grem::events

namespace grem::graphics {
class Texture;      // Forward declaration, to avoid including Texture.hpp.
class Window;       // Forward declaration, to avoid including Window.hpp.
class Device;       // Forward declaration, to avoid including Device.hpp.
class Swapchain;    // Forward declaration, to avoid including Swapchain.hpp.
class Renderer2D;   // Forward declaration, to avoid including Renderer2D.hpp.
class RenderPass;   // Forward declaration, to avoid including RenderPass.hpp.
class Bootstrapper; // Forward declaration.
} // namespace grem::graphics

namespace grem::imgui {

/**
 * Configuration options for a GraphicalUserInterface.
 */
struct GraphicalUserInterfaceOptions {};

/**
 * Persistent system that manages a platform+renderer backend for Dear ImGui.
 */
class GraphicalUserInterface {
public:
	/**
	 * Identifier for a texture used by the Dear ImGui backend.
	 */
	using TextureID = uint64_t;

	/**
	 * Data layout for the attributes of a single vertex of a GUI.
	 *
	 * Meets the requirements of the grem::graphics::mesh_vertex concept.
	 */
	struct Vertex {
		vec2 vertexPosition;           ///< Position relative to the top left of the viewport, in pixels.
		vec2 vertexTextureCoordinates; ///< Texture UV coordinates that map to this vertex.
		u8vec4norm vertexColor;        ///< Tint color of this vertex, sRGB-encoded.
	};

	/**
	 * Data type used in the index buffer of a GUI.
	 *
	 * Meets the requirements of the grem::graphics::mesh_index concept.
	 */
	using Index = uint16_t;

	/** Mesh type of the GUI.  */
	using Mesh = graphics::Mesh<Vertex, Index>;

	/** Struct of GUI vertex shader constants. */
	struct VertexShaderConstants {};

	/** Struct of fields output by a GUI vertex shader. */
	struct VertexShaderOutputs {
		vec2 fragmentTextureCoordinates; ///< Texture coordinates of the vertex.
		vec4 fragmentColor;              ///< Linear tint color of the vertex.
	};

	/** Struct of shader parameters representing the view of a GUI. */
	struct ViewParameters {
		vec2 guiOffset; ///< Offset of the view.
		vec2 guiScale;  ///< Scale of the view.
	};

	/** Shader buffer for GUI view parameters. */
	using ViewParameterBuffer = graphics::UniformBuffer<ViewParameters, "GUIViewParameters">;

	/** Default vertex shader for drawing a GUI. */
	using VertexShader = graphics::VertexShader<Mesh, VertexShaderConstants, VertexShaderOutputs, ViewParameterBuffer>;

	/** Struct of GUI fragment shader constants. */
	struct FragmentShaderConstants {};

	/** Struct of fields output by a GUI fragment shader. */
	struct FragmentShaderOutputs {
		vec4 outputColor; ///< Final fragment color.
	};

	/** Struct of shader parameters representing the texture of a GUI. */
	struct TextureParameters {
		graphics::sampler2D mainTexture; ///< Texture to use when rendering the GUI.
	};

	/** Shader buffer for GUI texture parameters. */
	using TextureBuffer = graphics::UniformBuffer<TextureParameters, "GUITexture">;

	/** Default fragment shader for drawing a GUI. */
	using FragmentShader = graphics::FragmentShader<Mesh, VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs, ViewParameterBuffer, TextureBuffer>;

	/** Shader pipeline for drawing a GUI. */
	using ShaderPipeline = graphics::ShaderPipeline<Mesh>;

	/** Default GUI vertex shader constants. */
	static constexpr VertexShaderConstants DEFAULT_VERTEX_SHADER_CONSTANTS{};

	/** Default GUI fragment shader constants. */
	static constexpr FragmentShaderConstants DEFAULT_FRAGMENT_SHADER_CONSTANTS{};

	/** Default GUI graphics pipeline configuration. */
	static constexpr graphics::ShaderPipelineOptions DEFAULT_SHADER_PIPELINE_OPTIONS{
		.depthBufferMode = graphics::DepthBufferMode::NONE,
		.primitiveType = graphics::PrimitiveType::TRIANGLES,
		.faceCullingMode = graphics::FaceCullingMode::NONE,
		.frontFace = graphics::FrontFace::CLOCKWISE,
		.blendState = graphics::BlendState::ALPHA_BLENDING_STRAIGHT,
	};

	/**
	 * Get the window associated with an ImGui viewport.
	 *
	 * \param viewport ImGui viewport to get the window of. Must be a viewport
	 *        created from this GUI's context.
	 *
	 * \return a reference to the window associated with the given viewport.
	 *
	 * \sa getSwapchain()
	 */
	[[nodiscard]] GREM_API(imgui) static graphics::Window& getWindow(ImGuiViewport& viewport);

	/**
	 * Get the swapchain associated with an ImGui viewport.
	 *
	 * \param viewport ImGui viewport to get the swapchain of. Must be a
	 *        viewport created from this GUI's context.
	 *
	 * \return a reference to the swapchain associated with the given viewport.
	 *
	 * \sa getWindow()
	 */
	[[nodiscard]] GREM_API(imgui) static graphics::Swapchain& getSwapchain(ImGuiViewport& viewport);

	/**
	 * Construct a graphical user interface.
	 *
	 * \param filesystem filesystem to create the GUI for. Must outlive the GUI.
	 * \param eventPump event pump to create the GUI for. Must outlive the GUI.
	 * \param window main window to create the GUI for. Must outlive the GUI.
	 * \param device device to create the GUI for. Must be associated with the
	 *        given window. Must outlive the GUI.
	 * \param swapchain swapchain associated with the main window. Must be
	 *        associated with the given window and device. Must outlive the GUI.
	 * \param renderer2D renderer to create the GUI for. Must be associated with
	 *        the given device. Must outlive the GUI.
	 * \param options initial configuration of the graphical user interface, see
	 *        GraphicalUserInterfaceOptions.
	 *
	 * \throws imgui::Error on failure to initialize the GUI.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(imgui)
	GraphicalUserInterface(Filesystem& filesystem, events::EventPump& eventPump, graphics::Window& window, graphics::Device& device, graphics::Swapchain& swapchain,
		graphics::Renderer2D& renderer2D, const GraphicalUserInterfaceOptions& options = {});

	/**
	 * Construct a graphical user interface for an existing Dear ImGui context.
	 *
	 * \param filesystem filesystem to create the GUI for. Must outlive the GUI.
	 * \param eventPump event pump to create the GUI for. Must outlive the GUI.
	 * \param window main window to create the GUI for. Must outlive the GUI.
	 * \param device device to create the GUI for. Must be associated with the
	 *        given window. Must outlive the GUI.
	 * \param swapchain swapchain associated with the main window. Must be
	 *        associated with the given window and device. Must outlive the GUI.
	 * \param renderer2D renderer to create the GUI for. Must be associated with
	 *        the given device. Must outlive the GUI.
	 * \param context Dear ImGui context to use for this GUI. Must outlive the
	 *        GUI.
	 * \param options initial configuration of the graphical user interface, see
	 *        GraphicalUserInterfaceOptions.
	 *
	 * \throws imgui::Error on failure to initialize the GUI.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(imgui)
	GraphicalUserInterface(Filesystem& filesystem, events::EventPump& eventPump, graphics::Window& window, graphics::Device& device, graphics::Swapchain& swapchain,
		graphics::Renderer2D& renderer2D, ImGuiContext& context, const GraphicalUserInterfaceOptions& options = {});

	/** Destructor. */
	GREM_API(imgui) ~GraphicalUserInterface();

	/** Copying a GUI is not allowed. */
	GraphicalUserInterface(const GraphicalUserInterface&) = delete;

	/** Moving a GUI is not allowed. */
	GraphicalUserInterface(GraphicalUserInterface&&) = delete;

	/** Copying a GUI is not allowed. */
	GraphicalUserInterface& operator=(const GraphicalUserInterface&) = delete;

	/** Moving a GUI is not allowed. */
	GraphicalUserInterface& operator=(GraphicalUserInterface&&) = delete;

	/**
	 * Handle an event from an events::EventPump in the GUI.
	 *
	 * \param event event to handle.
	 *
	 * \return true if the event was consumed and should be discarded, false if
	 *         the event should be propagated to the rest of the application.
	 *
	 * \sa update()
	 */
	GREM_API(imgui)
	bool handleEvent(const events::Event& event);

	/**
	 * Prepare for a new GUI frame.
	 *
	 * \param deltaTime time elapsed between the beginning of the previous frame
	 *        and the beginning of the new frame.
	 *
	 * \throws imgui::Error on failure to prepare a new frame.
	 * \throws graphics::Error if graphics resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function should be called once before each call to
	 *       ImGui::NewFrame(), ideally after handling the events for the
	 *       current frame.
	 *
	 * \sa handleEvent()
	 */
	GREM_API(imgui)
	void update(Duration deltaTime);

	/**
	 * Push the draw commands of a GUI frame to a render pass.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param drawData GUI frame to draw, obtained using ImGui::GetDrawData()
	 *        after calling ImGui::Render(). Must have been rendered with the
	 *        context of this GUI.
	 * \param shaderPipelineOverrideHandle shader pipeline override to use, or
	 *        nullptr to not use a shader override.
	 *
	 * \throws imgui::Error on failure to draw the frame.
	 * \throws graphics::Error if graphics resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Additional platform windows may be rendered by calling
	 *       ImGui::RenderPlatformWindowsDefault(), or by manually calling this
	 *       function for each window's swapchain, which can be obtained using
	 *       getSwapchain().
	 */
	GREM_API(imgui)
	void drawFrame(graphics::RenderPass& renderPass, const ImDrawData& drawData, SharedPointer<graphics::ShaderPipelineImplementation> shaderPipelineOverrideHandle = {});

	/**
	 * Push the draw commands of a GUI frame to a render pass.
	 *
	 * \param renderPass render pass to push the draw commands to.
	 * \param drawData GUI frame to draw, obtained using ImGui::GetDrawData()
	 *        after calling ImGui::Render(). Must have been rendered with the
	 *        context of this GUI.
	 * \param shaderPipelineOverride shader pipeline override to use.
	 *
	 * \throws imgui::Error on failure to draw the frame.
	 * \throws graphics::Error if graphics resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note Additional platform windows may be rendered by calling
	 *       ImGui::RenderPlatformWindowsDefault(), or by manually calling this
	 *       function for each window's swapchain, which can be obtained using
	 *       getSwapchain().
	 */
	void drawFrame(graphics::RenderPass& renderPass, const ImDrawData& drawData, const ShaderPipeline& shaderPipelineOverride) {
		drawFrame(renderPass, drawData, shaderPipelineOverride.lock());
	}

	/**
	 * Create a texture ID for a texture that will be valid for rendering in the
	 * GUI until at least the next call to update() after the texture has been
	 * destroyed, by extending the lifetime of the texture.
	 *
	 * \param texture texture to create an ID for.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \return an ImTextureID for the specified texture, suitable for passing to
	 *         functions like ImGui::Image() for the current frame.
	 */
	[[nodiscard]] GREM_API(imgui) TextureID getTextureID(const graphics::Texture& texture);

	/**
	 * Get the underlying Dear ImGui context.
	 *
	 * \return a non-null non-owning pointer to the Dear ImGui context of this
	 *         GUI.
	 */
	[[nodiscard]] GREM_API(imgui) ImGuiContext* getContext() const noexcept;

	/**
	 * Get the default GUI vertex shader.
	 *
	 * \return a read-only reference to the default GUI vertex shader.
	 */
	[[nodiscard]] GREM_API(imgui) const VertexShader& getDefaultVertexShader();

	/**
	 * Get the default plain GUI fragment shader.
	 *
	 * \return a read-only reference to the plain GUI fragment shader.
	 */
	[[nodiscard]] GREM_API(imgui) const FragmentShader& getPlainFragmentShader();

	/**
	 * Get the default plain GUI shader pipeline.
	 *
	 * \return a read-only reference to the plain GUI shader pipeline.
	 */
	[[nodiscard]] GREM_API(imgui) const ShaderPipeline& getPlainShaderPipeline();

private:
	friend graphics::Bootstrapper;

	class Implementation;

	UniquePointer<Implementation> implementation;
};

} // namespace grem::imgui

#endif
