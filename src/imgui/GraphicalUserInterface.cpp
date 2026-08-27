// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/core/version.hpp>
#include <GREM/events/Error.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/EventPump.hpp>
#include <GREM/events/ExternalApplication.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Display.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/imgui/Error.hpp>
#include <GREM/imgui/GraphicalUserInterface.hpp>

#include "builtin_shaders_imgui.hpp"

#include <imgui.h> // Im...
#include <new>     // std::bad_alloc
#include <utility> // std::move

#ifdef _WIN32

#include <windows.h> // HWND, LONG, GWL_EXSTYLE, WS_EX_APPWINDOW, WS_EX_TOOLWINDOW, SW_HIDE, GetWindowLong, SetWindowLong

#endif

namespace grem::imgui {

namespace {

class ContextPreserver {
public:
	[[nodiscard]] ContextPreserver() noexcept
		: oldContext(ImGui::GetCurrentContext()) {}

	~ContextPreserver() {
		ImGui::SetCurrentContext(oldContext);
	}

	ContextPreserver(const ContextPreserver&) = delete;
	ContextPreserver(ContextPreserver&&) = delete;
	ContextPreserver& operator=(const ContextPreserver&) = delete;
	ContextPreserver& operator=(ContextPreserver&&) = delete;

private:
	ImGuiContext* oldContext;
};

[[nodiscard]] ImGuiViewport* findViewportByWindowID(uint32_t windowID) {
	if (graphics::Window* const window = graphics::Window::findByID(windowID)) {
		if (ImGuiViewport* const viewport = ImGui::FindViewportByPlatformHandle(window)) {
			return viewport;
		}
	}
	return nullptr;
}

[[nodiscard]] ImGuiKey translateScancode(events::Scancode scancode) {
	switch (scancode) {
		case events::Scancode::A: return ImGuiKey_A;
		case events::Scancode::B: return ImGuiKey_B;
		case events::Scancode::C: return ImGuiKey_C;
		case events::Scancode::D: return ImGuiKey_D;
		case events::Scancode::E: return ImGuiKey_E;
		case events::Scancode::F: return ImGuiKey_F;
		case events::Scancode::G: return ImGuiKey_G;
		case events::Scancode::H: return ImGuiKey_H;
		case events::Scancode::I: return ImGuiKey_I;
		case events::Scancode::J: return ImGuiKey_J;
		case events::Scancode::K: return ImGuiKey_K;
		case events::Scancode::L: return ImGuiKey_L;
		case events::Scancode::M: return ImGuiKey_M;
		case events::Scancode::N: return ImGuiKey_N;
		case events::Scancode::O: return ImGuiKey_O;
		case events::Scancode::P: return ImGuiKey_P;
		case events::Scancode::Q: return ImGuiKey_Q;
		case events::Scancode::R: return ImGuiKey_R;
		case events::Scancode::S: return ImGuiKey_S;
		case events::Scancode::T: return ImGuiKey_T;
		case events::Scancode::U: return ImGuiKey_U;
		case events::Scancode::V: return ImGuiKey_V;
		case events::Scancode::W: return ImGuiKey_W;
		case events::Scancode::X: return ImGuiKey_X;
		case events::Scancode::Y: return ImGuiKey_Y;
		case events::Scancode::Z: return ImGuiKey_Z;
		case events::Scancode::ONE: return ImGuiKey_1;
		case events::Scancode::TWO: return ImGuiKey_2;
		case events::Scancode::THREE: return ImGuiKey_3;
		case events::Scancode::FOUR: return ImGuiKey_4;
		case events::Scancode::FIVE: return ImGuiKey_5;
		case events::Scancode::SIX: return ImGuiKey_6;
		case events::Scancode::SEVEN: return ImGuiKey_7;
		case events::Scancode::EIGHT: return ImGuiKey_8;
		case events::Scancode::NINE: return ImGuiKey_9;
		case events::Scancode::ZERO: return ImGuiKey_0;
		case events::Scancode::ESCAPE: return ImGuiKey_Escape;
		case events::Scancode::LEFT_CONTROL: return ImGuiKey_LeftCtrl;
		case events::Scancode::RIGHT_CONTROL: return ImGuiKey_RightCtrl;
		case events::Scancode::LEFT_SHIFT: return ImGuiKey_LeftShift;
		case events::Scancode::RIGHT_SHIFT: return ImGuiKey_RightShift;
		case events::Scancode::LEFT_ALT: return ImGuiKey_LeftAlt;
		case events::Scancode::RIGHT_ALT: return ImGuiKey_RightAlt;
		case events::Scancode::MENU: return ImGuiKey_Menu;
		case events::Scancode::LEFT_BRACKET: return ImGuiKey_LeftBracket;
		case events::Scancode::RIGHT_BRACKET: return ImGuiKey_RightBracket;
		case events::Scancode::SEMICOLON: return ImGuiKey_Semicolon;
		case events::Scancode::COMMA: return ImGuiKey_Comma;
		case events::Scancode::PERIOD: return ImGuiKey_Period;
		case events::Scancode::APOSTROPHE: return ImGuiKey_Apostrophe;
		case events::Scancode::SLASH: return ImGuiKey_Slash;
		case events::Scancode::BACKSLASH: return ImGuiKey_Backslash;
		case events::Scancode::GRAVE_ACCENT: return ImGuiKey_GraveAccent;
		case events::Scancode::NON_US_BACKSLASH: return ImGuiKey_Oem102;
		case events::Scancode::EQUALS: return ImGuiKey_Equal;
		case events::Scancode::MINUS: return ImGuiKey_Minus;
		case events::Scancode::SPACE: return ImGuiKey_Space;
		case events::Scancode::RETURN: return ImGuiKey_Enter;
		case events::Scancode::BACKSPACE: return ImGuiKey_Backspace;
		case events::Scancode::TAB: return ImGuiKey_Tab;
		case events::Scancode::PAGE_UP: return ImGuiKey_PageUp;
		case events::Scancode::PAGE_DOWN: return ImGuiKey_PageDown;
		case events::Scancode::END: return ImGuiKey_End;
		case events::Scancode::HOME: return ImGuiKey_Home;
		case events::Scancode::INSERT: return ImGuiKey_Insert;
		case events::Scancode::DEL: return ImGuiKey_Delete;
		case events::Scancode::UP_ARROW: return ImGuiKey_UpArrow;
		case events::Scancode::DOWN_ARROW: return ImGuiKey_DownArrow;
		case events::Scancode::LEFT_ARROW: return ImGuiKey_LeftArrow;
		case events::Scancode::RIGHT_ARROW: return ImGuiKey_RightArrow;
		case events::Scancode::NUMPAD_PLUS: return ImGuiKey_KeypadAdd;
		case events::Scancode::NUMPAD_MINUS: return ImGuiKey_KeypadSubtract;
		case events::Scancode::NUMPAD_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case events::Scancode::NUMPAD_DIVIDE: return ImGuiKey_KeypadDivide;
		case events::Scancode::NUMPAD_ONE: return ImGuiKey_Keypad1;
		case events::Scancode::NUMPAD_TWO: return ImGuiKey_Keypad2;
		case events::Scancode::NUMPAD_THREE: return ImGuiKey_Keypad3;
		case events::Scancode::NUMPAD_FOUR: return ImGuiKey_Keypad4;
		case events::Scancode::NUMPAD_FIVE: return ImGuiKey_Keypad5;
		case events::Scancode::NUMPAD_SIX: return ImGuiKey_Keypad6;
		case events::Scancode::NUMPAD_SEVEN: return ImGuiKey_Keypad7;
		case events::Scancode::NUMPAD_EIGHT: return ImGuiKey_Keypad8;
		case events::Scancode::NUMPAD_NINE: return ImGuiKey_Keypad9;
		case events::Scancode::NUMPAD_ZERO: return ImGuiKey_Keypad0;
		case events::Scancode::F1: return ImGuiKey_F1;
		case events::Scancode::F2: return ImGuiKey_F2;
		case events::Scancode::F3: return ImGuiKey_F3;
		case events::Scancode::F4: return ImGuiKey_F4;
		case events::Scancode::F5: return ImGuiKey_F5;
		case events::Scancode::F6: return ImGuiKey_F6;
		case events::Scancode::F7: return ImGuiKey_F7;
		case events::Scancode::F8: return ImGuiKey_F8;
		case events::Scancode::F9: return ImGuiKey_F9;
		case events::Scancode::F10: return ImGuiKey_F10;
		case events::Scancode::F11: return ImGuiKey_F11;
		case events::Scancode::F12: return ImGuiKey_F12;
		case events::Scancode::F13: return ImGuiKey_F13;
		case events::Scancode::F14: return ImGuiKey_F14;
		case events::Scancode::F15: return ImGuiKey_F15;
		case events::Scancode::PRINT_SCREEN: return ImGuiKey_PrintScreen;
		case events::Scancode::SCROLL_LOCK: return ImGuiKey_ScrollLock;
		case events::Scancode::PAUSE: return ImGuiKey_Pause;
		default: break;
	}
	return ImGuiKey_None;
}

[[nodiscard]] int translateMouseButton(events::MouseButton mouseButton) {
	switch (mouseButton) {
		case events::MouseButton::LEFT: return 0;
		case events::MouseButton::MIDDLE: return 2;
		case events::MouseButton::RIGHT: return 1;
		case events::MouseButton::BACK: return 3;
		case events::MouseButton::FORWARD: return 4;
		default: break;
	}
	return -1;
}

[[nodiscard]] ImGuiKey translateControllerButton(events::ControllerButton controllerButton) {
	switch (controllerButton) {
		case events::ControllerButton::SOUTH: return ImGuiKey_GamepadFaceDown;
		case events::ControllerButton::EAST: return ImGuiKey_GamepadFaceRight;
		case events::ControllerButton::WEST: return ImGuiKey_GamepadFaceLeft;
		case events::ControllerButton::NORTH: return ImGuiKey_GamepadFaceUp;
		case events::ControllerButton::SELECT: return ImGuiKey_GamepadBack;
		case events::ControllerButton::START: return ImGuiKey_GamepadStart;
		case events::ControllerButton::LEFT_STICK: return ImGuiKey_GamepadL3;
		case events::ControllerButton::RIGHT_STICK: return ImGuiKey_GamepadR3;
		case events::ControllerButton::LEFT_SHOULDER: return ImGuiKey_GamepadL1;
		case events::ControllerButton::RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
		case events::ControllerButton::DPAD_UP: return ImGuiKey_GamepadDpadUp;
		case events::ControllerButton::DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
		case events::ControllerButton::DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
		case events::ControllerButton::DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
		default: break;
	}
	return ImGuiKey_None;
}

void updateMonitors(ImGuiPlatformIO& platformIO) {
	GREM_PROFILE_FUNCTION();

	platformIO.Monitors.resize(0);
	for (const graphics::Display& display : graphics::Display::getAll()) {
		ImGuiPlatformMonitor monitor{};
		try {
			monitor.DpiScale = display.getContentScale();
		} catch (const graphics::Error&) {
			continue;
		}

		const Region2D bounds = display.getBounds();
		const ImVec2 pos{static_cast<float>(bounds.offset.x), static_cast<float>(bounds.offset.y)};
		const ImVec2 size{static_cast<float>(bounds.size.width), static_cast<float>(bounds.size.height)};
		monitor.MainPos = pos;
		monitor.MainSize = size;
		monitor.WorkPos = pos;
		monitor.WorkSize = size;
		try {
			const Region2D usableBounds = display.getUsableBounds();
			if (usableBounds.size.width > 0 && usableBounds.size.height > 0) {
				monitor.WorkPos = ImVec2{static_cast<float>(usableBounds.offset.x), static_cast<float>(usableBounds.offset.y)};
				monitor.WorkSize = ImVec2{static_cast<float>(usableBounds.size.width), static_cast<float>(usableBounds.size.height)};
			}
		} catch (const graphics::Error&) {
		}

		monitor.PlatformHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(display.getID()));
		platformIO.Monitors.push_back(monitor);
	}
}

void updateKeyModifiers(ImGuiIO& io, events::KeyModifiers keyModifiers) {
	io.AddKeyEvent(ImGuiMod_Ctrl, keyModifiers.containsAnyOf(events::KeyModifiers::CONTROL));
	io.AddKeyEvent(ImGuiMod_Shift, keyModifiers.containsAnyOf(events::KeyModifiers::SHIFT));
	io.AddKeyEvent(ImGuiMod_Alt, keyModifiers.containsAnyOf(events::KeyModifiers::ALT));
	io.AddKeyEvent(ImGuiMod_Super, keyModifiers.containsAnyOf(events::KeyModifiers::GUI));
}

} // namespace

class GraphicalUserInterface::Implementation {
public:
	Implementation(Filesystem& filesystem, events::EventPump& eventPump, graphics::Window& window, graphics::Device& device, graphics::Swapchain& swapchain,
		graphics::Renderer2D& renderer2D, ImGuiContext* existingContext, const GraphicalUserInterfaceOptions& options)
		: eventPump(eventPump)
		, window(window)
		, device(device)
		, swapchain(swapchain)
		, renderer2D(renderer2D)
		, context(existingContext, ContextDeleter{.isOwned = false}) {
		(void)options;

		const ContextPreserver contextPreserver{};
		if (!context) {
			IMGUI_CHECKVERSION();
			context = ContextHandle{ImGui::CreateContext(), ContextDeleter{.isOwned = true}};
			if (!context) {
				throw std::bad_alloc{};
			}
		}
		ImGui::SetCurrentContext(context.get());

		ImGuiIO& io = ImGui::GetIO();

		GREM_ASSERT(!io.BackendPlatformUserData);
		io.BackendPlatformUserData = this;
		io.BackendPlatformName = backendName.c_str();
#ifdef __EMSCRIPTEN__
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;
#else
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasParentViewport;
#endif

		GREM_ASSERT(!io.BackendRendererUserData);
		io.BackendRendererUserData = &filesystem;
		io.BackendRendererName = backendName.c_str();
#ifdef __EMSCRIPTEN__
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
#else
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasViewports;
#endif

		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

		platformIO.Platform_SetClipboardTextFn = [](ImGuiContext* context, const char* text) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetClipboardTextFn");
			const ContextPreserver contextPreserver{};
			ImGui::SetCurrentContext(context);
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			gui.eventPump.setClipboardText((text) ? CStringView{text} : CStringView{});
		};
		platformIO.Platform_GetClipboardTextFn = [](ImGuiContext* context) -> const char* {
			GREM_PROFILE_BLOCK("ImGui Platform_GetClipboardTextFn");
			const ContextPreserver contextPreserver{};
			ImGui::SetCurrentContext(context);
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			if (Optional<String> text = gui.eventPump.getClipboardText()) {
				gui.storageForGetClipboardText = std::move(*text);
				return gui.storageForGetClipboardText.c_str();
			}
			gui.storageForGetClipboardText.clear();
			return nullptr;
		};
		platformIO.Platform_SetImeDataFn = [](ImGuiContext* context, ImGuiViewport*, ImGuiPlatformImeData* data) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetImeDataFn");
			const ContextPreserver contextPreserver{};
			ImGui::SetCurrentContext(context);
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			gui.imeData = *data;
			gui.imeDirty = true;
			gui.updateIME();
		};
		platformIO.Platform_OpenInShellFn = [](ImGuiContext*, const char* url) -> bool {
			GREM_PROFILE_BLOCK("ImGui Platform_OpenInShellFn");
			if (!url) {
				return false;
			}
			try {
				events::ExternalApplication::openURL(url);
			} catch (const events::Error&) {
				return false;
			}
			return true;
		};
#ifndef __EMSCRIPTEN__
		platformIO.Platform_CreateWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_CreateWindow");
			graphics::Window* parentWindow = nullptr;
			if (viewport->ParentViewportId != 0) {
				if (ImGuiViewport* const parentViewport = ImGui::FindViewportByID(viewport->ParentViewportId)) {
					parentWindow = &GraphicalUserInterface::getWindow(*parentViewport);
				}
			}
			const graphics::WindowOptions windowOptions{
				.title = "No Title Yet",
				.positionX = static_cast<int32_t>(viewport->Pos.x),
				.positionY = static_cast<int32_t>(viewport->Pos.y),
				.size{static_cast<uint32_t>(viewport->Size.x), static_cast<uint32_t>(viewport->Size.y)},
				.multisampleCount = (parentWindow) ? parentWindow->getMultisampleCount() : 1,
				.opacity = 1.0f,
				.highPixelDensity = (parentWindow) ? parentWindow->isHighPixelDensity() : false,
				.hidden = true,
				.focus = false,
				.minimized = false,
				.resizable = (viewport->Flags & ImGuiViewportFlags_NoDecoration) == 0,
				.fullscreen = false,
				.borderless = (viewport->Flags & ImGuiViewportFlags_NoDecoration) != 0,
				.hideFromTaskbar = (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon) != 0,
				.alwaysOnTop = (viewport->Flags & ImGuiViewportFlags_TopMost) != 0,
				.relativeMouseMode = false,
			};
#ifdef __APPLE__
			graphics::Window* const window = new graphics::Window{windowOptions}; // NOLINT(cppcoreguidelines-owning-memory)
#else
			graphics::Window* const window =
				(parentWindow) ? new graphics::Window{*parentWindow, windowOptions} : new graphics::Window{windowOptions}; // NOLINT(cppcoreguidelines-owning-memory)
#endif
			try {
				viewport->PlatformHandleRaw = window->getNativeHandle();
			} catch (...) {
				delete window; // NOLINT(cppcoreguidelines-owning-memory)
				throw;
			}
			viewport->PlatformUserData = (parentWindow) ? parentWindow : window;
			viewport->PlatformHandle = window;
		};
		platformIO.Platform_DestroyWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_DestroyWindow");
			if (viewport->PlatformHandle && viewport->PlatformUserData) {
				delete static_cast<graphics::Window*>(viewport->PlatformHandle); // NOLINT(cppcoreguidelines-owning-memory)
			}
			viewport->PlatformHandle = nullptr;
			viewport->PlatformUserData = nullptr;
		};
		platformIO.Platform_ShowWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_ShowWindow");
#if defined(_WIN32) && \
	!(defined(WINAPI_FAMILY) && ((defined(WINAPI_FAMILY_APP) && WINAPI_FAMILY == WINAPI_FAMILY_APP) || (defined(WINAPI_FAMILY_GAMES) && WINAPI_FAMILY == WINAPI_FAMILY_GAMES)))
			if ((viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon) == 0) {
				HWND hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);
				LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
				exStyle |= WS_EX_APPWINDOW;
				exStyle &= ~WS_EX_TOOLWINDOW;
				ShowWindow(hwnd, SW_HIDE);
				SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
			}
#endif
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.show((viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing) == 0);
		};
		platformIO.Platform_UpdateWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_UpdateWindow");
			graphics::Window* newParentWindow = nullptr;
			if (viewport->ParentViewportId != 0) {
				if (ImGuiViewport* const parentViewport = ImGui::FindViewportByID(viewport->ParentViewportId)) {
					newParentWindow = &GraphicalUserInterface::getWindow(*parentViewport);
				}
			}
			if (viewport->PlatformUserData && newParentWindow != static_cast<graphics::Window*>(viewport->PlatformUserData)) {
#ifndef __APPLE__
				GraphicalUserInterface::getWindow(*viewport).setParent(newParentWindow);
#endif
				viewport->PlatformUserData = (newParentWindow) ? newParentWindow : viewport->PlatformHandle;
			}
		};
		platformIO.Platform_SetWindowPos = [](ImGuiViewport* viewport, ImVec2 pos) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetWindowPos");
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.setPosition(Offset2D{.x = static_cast<int32_t>(pos.x), .y = static_cast<int32_t>(pos.y)});
		};
		platformIO.Platform_GetWindowPos = [](ImGuiViewport* viewport) -> ImVec2 {
			GREM_PROFILE_BLOCK("ImGui Platform_GetWindowPos");
			const graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			const Offset2D position = window.getPosition();
			return ImVec2{static_cast<float>(position.x), static_cast<float>(position.y)};
		};
		platformIO.Platform_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetWindowSize");
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.setSize(Extent2D{.width = static_cast<uint32_t>(size.x), .height = static_cast<uint32_t>(size.y)});
		};
		platformIO.Platform_GetWindowSize = [](ImGuiViewport* viewport) -> ImVec2 {
			GREM_PROFILE_BLOCK("ImGui Platform_GetWindowSize");
			const graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			const Extent2D size = window.getSize();
			return ImVec2{static_cast<float>(size.width), static_cast<float>(size.height)};
		};
		platformIO.Platform_GetWindowFramebufferScale = [](ImGuiViewport* viewport) -> ImVec2 {
			GREM_PROFILE_BLOCK("ImGui Platform_GetWindowFramebufferScale");
			const graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
#ifdef __APPLE__
			const vec2 framebufferScale{window.getDisplayScale()};
			return ImVec2{framebufferScale.x, framebufferScale.y};
#else
			const Extent2D size = window.getSize();
			const Extent2D drawableSize = window.getDrawableSize();
			return ImVec2{
				(size.width > 0) ? static_cast<float>(drawableSize.width) / static_cast<float>(size.width) : 1.0f,
				(size.height > 0) ? static_cast<float>(drawableSize.height) / static_cast<float>(size.height) : 1.0f,
			};
#endif
		};
		platformIO.Platform_SetWindowFocus = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetWindowFocus");
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.raise();
		};
		platformIO.Platform_GetWindowFocus = [](ImGuiViewport* viewport) -> bool {
			GREM_PROFILE_BLOCK("ImGui Platform_GetWindowFocus");
			const graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			return window.hasFocus();
		};
		platformIO.Platform_GetWindowMinimized = [](ImGuiViewport* viewport) -> bool {
			GREM_PROFILE_BLOCK("ImGui Platform_GetWindowMinimized");
			const graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			return window.isMinimized();
		};
		platformIO.Platform_SetWindowTitle = [](ImGuiViewport* viewport, const char* title) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetWindowTitle");
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.setTitle((title) ? CStringView{title} : CStringView{});
		};
		platformIO.Platform_RenderWindow = [](ImGuiViewport*, void*) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_RenderWindow");
		};
		platformIO.Platform_SwapBuffers = [](ImGuiViewport*, void*) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SwapBuffers");
		};
		platformIO.Platform_SetWindowAlpha = [](ImGuiViewport* viewport, float alpha) -> void {
			GREM_PROFILE_BLOCK("ImGui Platform_SetWindowAlpha");
			graphics::Window& window = GraphicalUserInterface::getWindow(*viewport);
			window.setOpacity(alpha);
		};
		platformIO.Platform_CreateVkSurface = [](ImGuiViewport*, ImU64, const void*, ImU64*) -> int {
			GREM_PROFILE_BLOCK("ImGui Platform_CreateVkSurface");
			return 0;
		};

		platformIO.DrawCallback_ResetRenderState = [](const ImDrawList*, const ImDrawCmd*) -> void {
			GREM_PROFILE_BLOCK("ImGui DrawCallback_ResetRenderState");
		};
		platformIO.DrawCallback_SetSamplerLinear = [](const ImDrawList*, const ImDrawCmd*) -> void {
			GREM_PROFILE_BLOCK("ImGui DrawCallback_SetSamplerLinear");
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			gui.useNearestTextureFilter = false;
		};
		platformIO.DrawCallback_SetSamplerNearest = [](const ImDrawList*, const ImDrawCmd*) -> void {
			GREM_PROFILE_BLOCK("ImGui DrawCallback_SetSamplerNearest");
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			gui.useNearestTextureFilter = true;
		};

		platformIO.Renderer_RenderWindow = [](ImGuiViewport* viewport, void* arg) -> void {
			GREM_PROFILE_BLOCK("ImGui Renderer_RenderWindow");
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			graphics::Swapchain& swapchain = GraphicalUserInterface::getSwapchain(*viewport);
			graphics::RenderPass renderPass{gui.device, swapchain, graphics::ClearValues{}};
			SharedPointer<graphics::ShaderPipelineImplementation> shaderPipelineOverrideHandle{};
			if (arg) {
				shaderPipelineOverrideHandle = *static_cast<const SharedPointer<graphics::ShaderPipelineImplementation>*>(arg);
			}
			gui.drawFrame(renderPass, *viewport->DrawData, std::move(shaderPipelineOverrideHandle));
			gui.device.render(renderPass);
			gui.device.present(swapchain);
		};
		platformIO.Renderer_CreateWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Renderer_CreateWindow");
			Implementation& gui = *static_cast<Implementation*>(ImGui::GetIO().BackendPlatformUserData);
			graphics::Swapchain* const swapchain = new graphics::Swapchain{gui.device, GraphicalUserInterface::getWindow(*viewport),
				graphics::SwapchainOptions{
					.maxBufferedFrameCount = gui.swapchain.getMaxBufferedFrameCount(),
					.useVerticalSynchronization = gui.swapchain.isVerticalSynchronizationEnabled(),
				}};
			viewport->RendererUserData = swapchain;
		};
		platformIO.Renderer_DestroyWindow = [](ImGuiViewport* viewport) -> void {
			GREM_PROFILE_BLOCK("ImGui Renderer_DestroyWindow");
			if (viewport->RendererUserData && viewport->PlatformUserData) {
				delete static_cast<graphics::Swapchain*>(viewport->RendererUserData); // NOLINT(cppcoreguidelines-owning-memory)
			}
			viewport->RendererUserData = nullptr;
		};
#endif

		updateMonitors(platformIO);

		if (ImGuiViewport* const mainViewport = ImGui::GetMainViewport()) {
			mainViewport->PlatformHandle = &window;
			mainViewport->PlatformHandleRaw = window.getNativeHandle();
			mainViewport->PlatformUserData = nullptr;
			mainViewport->RendererUserData = &swapchain;
		}
	}

	~Implementation() {
		if (context) {
			const ContextPreserver contextPreserver{};
			ImGui::SetCurrentContext(context.get());

			if (ImGuiViewport* const mainViewport = ImGui::GetMainViewport()) {
				mainViewport->PlatformHandle = nullptr;
				mainViewport->PlatformHandleRaw = nullptr;
				mainViewport->PlatformUserData = nullptr;
				mainViewport->RendererUserData = nullptr;
			}

			ImGui::DestroyPlatformWindows();

			ImGuiIO& io = ImGui::GetIO();
			ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

			io.BackendPlatformName = nullptr;
			io.BackendPlatformUserData = nullptr;
			io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_PlatformHasViewports |
								 ImGuiBackendFlags_HasParentViewport);
			platformIO.ClearPlatformHandlers();

			io.BackendRendererName = nullptr;
			io.BackendRendererUserData = nullptr;
			io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasViewports);
			platformIO.ClearRendererHandlers();

			if (cursorStyle != events::CursorStyle::DEFAULT) {
				eventPump.setCursorStyle(events::CursorStyle::DEFAULT);
			}
		}
	}

	Implementation(const Implementation&) = delete;
	Implementation(Implementation&&) = delete;
	Implementation& operator=(const Implementation&) = delete;
	Implementation& operator=(Implementation&&) = delete;

	bool handleEvent(const events::Event& event) {
		const ContextPreserver contextPreserver{};
		ImGui::SetCurrentContext(context.get());
		ImGuiIO& io = ImGui::GetIO();
		GREM_MATCH(event) {
			GREM_CASE(const events::ApplicationQuitRequestedEvent& quitRequested) break;
			GREM_CASE(const events::DisplayOrientationChangedEvent& displayOrientationChanged) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayAddedEvent& displayAdded) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayRemovedEvent& displayRemoved) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayMovedEvent& displayMoved) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayDesktopModeChangedEvent& displayDesktopModeChanged) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayCurrentModeChangedEvent& displayCurrentModeChanged) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayContentScaleChangedEvent& displayContentScaleChanged) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::DisplayUsableBoundsChangedEvent& displayUsableBoundsChanged) {
				shouldUpdateMonitors = true;
				break;
			}
			GREM_CASE(const events::WindowShownEvent& windowShown) break;
			GREM_CASE(const events::WindowHiddenEvent& windowHidden) break;
			GREM_CASE(const events::WindowExposedEvent& windowExposed) break;
			GREM_CASE(const events::WindowMovedEvent& windowMoved) {
				if (ImGuiViewport* const viewport = findViewportByWindowID(windowMoved.windowID)) {
					viewport->PlatformRequestMove = true;
				}
				break;
			}
			GREM_CASE(const events::WindowResizedEvent& windowResized) {
				if (ImGuiViewport* const viewport = findViewportByWindowID(windowResized.windowID)) {
					viewport->PlatformRequestResize = true;
				}
				break;
			}
			GREM_CASE(const events::WindowDrawableSizeChangedEvent& windowDrawableSizeChanged) {
				if (ImGuiViewport* const viewport = findViewportByWindowID(windowDrawableSizeChanged.windowID)) {
					viewport->PlatformRequestResize = true;
				}
				break;
			}
			GREM_CASE(const events::WindowMinimizedEvent& windowMinimized) break;
			GREM_CASE(const events::WindowMaximizedEvent& windowMaximized) break;
			GREM_CASE(const events::WindowRestoredEvent& windowRestored) break;
			GREM_CASE(const events::WindowMouseFocusGainedEvent& windowMouseFocusGained) {
				if (findViewportByWindowID(windowMouseFocusGained.windowID)) {
					mouseWindowID = windowMouseFocusGained.windowID;
					mouseLeaveFrameIndex = 0;
				}
				break;
			}
			GREM_CASE(const events::WindowMouseFocusLostEvent& windowMouseFocusLost) {
				if (findViewportByWindowID(windowMouseFocusLost.windowID)) {
					if (windowMouseFocusLost.windowID == mouseWindowID) {
						mouseLeaveFrameIndex = frameIndex + 1;
					}
				}
				break;
			}
			GREM_CASE(const events::WindowKeyboardFocusGainedEvent& windowKeyboardFocusGained) {
				if (findViewportByWindowID(windowKeyboardFocusGained.windowID)) {
					io.AddFocusEvent(true);
				}
				break;
			}
			GREM_CASE(const events::WindowKeyboardFocusLostEvent& windowKeyboardFocusLost) {
				if (findViewportByWindowID(windowKeyboardFocusLost.windowID)) {
					io.AddFocusEvent(false);
				}
				break;
			}
			GREM_CASE(const events::WindowCloseRequestedEvent& windowCloseRequested) {
				if (ImGuiViewport* const viewport = findViewportByWindowID(windowCloseRequested.windowID)) {
					viewport->PlatformRequestClose = true;
				}
				break;
			}
			GREM_CASE(const events::WindowDisplayChangedEvent& windowDisplayChanged) break;
			GREM_CASE(const events::WindowDisplayScaleChangedEvent& windowDisplayScaleChanged) break;
			GREM_CASE(const events::KeyPressedEvent& keyPressed) {
				if (findViewportByWindowID(keyPressed.windowID)) {
					updateKeyModifiers(io, keyPressed.keyModifiers);
					io.AddKeyEvent(translateScancode(keyPressed.scancode), true);
					if (io.WantCaptureKeyboard && (keyPressed.scancode != events::Scancode::ESCAPE || ImGui::IsAnyItemActive())) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::KeyPressRepeatedEvent& keyPressRepeated) {
				if (findViewportByWindowID(keyPressRepeated.windowID)) {
					updateKeyModifiers(io, keyPressRepeated.keyModifiers);
					io.AddKeyEvent(translateScancode(keyPressRepeated.scancode), true);
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::KeyReleasedEvent& keyReleased) {
				if (findViewportByWindowID(keyReleased.windowID)) {
					updateKeyModifiers(io, keyReleased.keyModifiers);
					io.AddKeyEvent(translateScancode(keyReleased.scancode), false);
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::TextInputEditedEvent& textInputEdited) {
				if (findViewportByWindowID(textInputEdited.windowID)) {
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::TextInputSubmittedEvent& textInputSubmitted) {
				if (findViewportByWindowID(textInputSubmitted.windowID)) {
					io.AddInputCharactersUTF8(textInputSubmitted.inputText.c_str());
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::MouseMovedEvent& mouseMoved) {
				if (graphics::Window* const window = graphics::Window::findByID(mouseMoved.windowID)) {
					if (ImGui::FindViewportByPlatformHandle(window)) {
						vec2 globalMousePosition = mouseMoved.mousePosition;
						if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
							globalMousePosition += vec2{window->getPosition()};
						}
						io.AddMouseSourceEvent((mouseMoved.mouseID == 0xFFFFFFFF)   ? ImGuiMouseSource_TouchScreen
											   : (mouseMoved.mouseID == 0xFFFFFFFE) ? ImGuiMouseSource_Pen
																					: ImGuiMouseSource_Mouse);
						io.AddMousePosEvent(globalMousePosition.x, globalMousePosition.y);
					}
					if (io.WantCaptureMouse) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::MouseButtonPressedEvent& mouseButtonPressed) {
				if (findViewportByWindowID(mouseButtonPressed.windowID)) {
					const int mouseButton = translateMouseButton(mouseButtonPressed.mouseButton);
					if (mouseButton != -1) {
						io.AddMouseSourceEvent((mouseButtonPressed.mouseID == 0xFFFFFFFF)   ? ImGuiMouseSource_TouchScreen
											   : (mouseButtonPressed.mouseID == 0xFFFFFFFE) ? ImGuiMouseSource_Pen
																							: ImGuiMouseSource_Mouse);
						io.AddMouseButtonEvent(mouseButton, true);
						mouseButtonsDown |= 1 << mouseButton;
					}
					if (io.WantCaptureMouse) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::MouseButtonReleasedEvent& mouseButtonReleased) {
				if (findViewportByWindowID(mouseButtonReleased.windowID)) {
					const int mouseButton = translateMouseButton(mouseButtonReleased.mouseButton);
					if (mouseButton != -1) {
						io.AddMouseSourceEvent((mouseButtonReleased.mouseID == 0xFFFFFFFF)   ? ImGuiMouseSource_TouchScreen
											   : (mouseButtonReleased.mouseID == 0xFFFFFFFE) ? ImGuiMouseSource_Pen
																							 : ImGuiMouseSource_Mouse);
						io.AddMouseButtonEvent(mouseButton, false);
						mouseButtonsDown &= ~(1 << mouseButton);
					}
					if (io.WantCaptureMouse) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::MouseWheelScrolledEvent& mouseWheelScrolled) {
				if (findViewportByWindowID(mouseWheelScrolled.windowID)) {
					io.AddMouseSourceEvent((mouseWheelScrolled.mouseID == 0xFFFFFFFF)   ? ImGuiMouseSource_TouchScreen
										   : (mouseWheelScrolled.mouseID == 0xFFFFFFFE) ? ImGuiMouseSource_Pen
																						: ImGuiMouseSource_Mouse);
					io.AddMouseWheelEvent(-mouseWheelScrolled.scrollAmount.x, mouseWheelScrolled.scrollAmount.y);
					if (io.WantCaptureMouse) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::ControllerAddedEvent& controllerAdded) break;
			GREM_CASE(const events::ControllerRemovedEvent& controllerRemoved) break;
			GREM_CASE(const events::ControllerRemappedEvent& controllerRemapped) break;
			GREM_CASE(const events::ControllerAxisMovedEvent& controllerAxisMoved) {
				if (findViewportByWindowID(controllerAxisMoved.windowID)) {
					constexpr float DEADZONE = 8000.0f;
					constexpr float MIN = -32768.0f;
					constexpr float MAX = 32767.0f;
					const float negativeAmount = clamp((static_cast<float>(controllerAxisMoved.axisValue) + DEADZONE) / (MIN + DEADZONE), 0.0f, 1.0f);
					const float positiveAmount = clamp((static_cast<float>(controllerAxisMoved.axisValue) - DEADZONE) / (MAX - DEADZONE), 0.0f, 1.0f);
					switch (controllerAxisMoved.axis) {
						case events::ControllerAxis::LEFT_STICK_X:
							io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, negativeAmount > 0.1f, negativeAmount);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, positiveAmount > 0.1f, positiveAmount);
							break;
						case events::ControllerAxis::LEFT_STICK_Y:
							io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, negativeAmount > 0.1f, negativeAmount);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, positiveAmount > 0.1f, positiveAmount);
							break;
						case events::ControllerAxis::RIGHT_STICK_X:
							io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, negativeAmount > 0.1f, negativeAmount);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, positiveAmount > 0.1f, positiveAmount);
							break;
						case events::ControllerAxis::RIGHT_STICK_Y:
							io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, negativeAmount > 0.1f, negativeAmount);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, positiveAmount > 0.1f, positiveAmount);
							break;
						case events::ControllerAxis::LEFT_TRIGGER: io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, positiveAmount > 0.1f, positiveAmount); break;
						case events::ControllerAxis::RIGHT_TRIGGER: io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, positiveAmount > 0.1f, positiveAmount); break;
						default: break;
					}
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::ControllerButtonPressedEvent& controllerButtonPressed) {
				if (findViewportByWindowID(controllerButtonPressed.windowID)) {
					io.AddKeyEvent(translateControllerButton(controllerButtonPressed.controllerButton), true);
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::ControllerButtonReleasedEvent& controllerButtonReleased) {
				if (findViewportByWindowID(controllerButtonReleased.windowID)) {
					io.AddKeyEvent(translateControllerButton(controllerButtonReleased.controllerButton), false);
					if (io.WantCaptureKeyboard) {
						return true;
					}
				}
				break;
			}
			GREM_CASE(const events::TouchMovedEvent& touchMoved) break;
			GREM_CASE(const events::TouchPressedEvent& touchPressed) break;
			GREM_CASE(const events::TouchReleasedEvent& touchReleased) break;
			GREM_CASE(const events::KeymapChangedEvent& keymapChanged) break;
			GREM_CASE(const events::ClipboardUpdatedEvent& clipboardUpdated) break;
			GREM_CASE(const events::DroppedFileEvent& droppedFile) break;
			GREM_CASE(const events::DroppedTextEvent& droppedText) break;
			GREM_CASE(const events::DropStartedEvent& dropStarted) break;
			GREM_CASE(const events::DropCompletedEvent& dropCompleted) break;
		}
		return false;
	}

	void update(Duration deltaTime) {
		erase_if(textures, [](const auto& kv) -> bool { return kv.second.isExternal(); });

		const ContextPreserver contextPreserver{};
		ImGui::SetCurrentContext(context.get());
		ImGuiIO& io = ImGui::GetIO();

		const Extent2D size = window.getSize();
		const Extent2D drawableSize = window.getDrawableSize();
		io.DisplaySize = ImVec2{static_cast<float>(size.width), static_cast<float>(size.height)};
		if (size.width > 0 && size.height > 0) {
			const vec2 framebufferScale = vec2{drawableSize} / vec2{size};
			io.DisplayFramebufferScale = ImVec2{framebufferScale.x, framebufferScale.y};
		} else {
			io.DisplayFramebufferScale = ImVec2{1.0f, 1.0f};
		}

		if (shouldUpdateMonitors) {
			updateMonitors(ImGui::GetPlatformIO());
			shouldUpdateMonitors = false;
		}

		io.DeltaTime = duration_cast<FloatSeconds>(deltaTime).count();

		if (frameIndex == mouseLeaveFrameIndex && mouseButtonsDown == 0) {
			mouseWindowID = 0;
			io.AddMousePosEvent(-Limits<float>::MAX, -Limits<float>::MAX);
		}

		if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0) {
			const ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
			if (io.MouseDrawCursor || imguiCursor == ImGuiMouseCursor_None) {
				eventPump.hideCursor();
			} else {
				events::CursorStyle newCursorStyle = events::CursorStyle::DEFAULT;
				switch (imguiCursor) {
					case ImGuiMouseCursor_Arrow: newCursorStyle = events::CursorStyle::DEFAULT; break;
					case ImGuiMouseCursor_TextInput: newCursorStyle = events::CursorStyle::TEXT; break;
					case ImGuiMouseCursor_ResizeAll: newCursorStyle = events::CursorStyle::MOVE; break;
					case ImGuiMouseCursor_ResizeNS: newCursorStyle = events::CursorStyle::RESIZE_VERTICAL; break;
					case ImGuiMouseCursor_ResizeEW: newCursorStyle = events::CursorStyle::RESIZE_HORIZONTAL; break;
					case ImGuiMouseCursor_ResizeNESW: newCursorStyle = events::CursorStyle::RESIZE_DIAGONAL_NE_SW; break;
					case ImGuiMouseCursor_ResizeNWSE: newCursorStyle = events::CursorStyle::RESIZE_DIAGONAL_NW_SE; break;
					case ImGuiMouseCursor_Hand: newCursorStyle = events::CursorStyle::POINTER; break;
					case ImGuiMouseCursor_Wait: newCursorStyle = events::CursorStyle::WAIT; break;
					case ImGuiMouseCursor_Progress: newCursorStyle = events::CursorStyle::PROGRESS; break;
					case ImGuiMouseCursor_NotAllowed: newCursorStyle = events::CursorStyle::NOT_ALLOWED; break;
					default: break;
				}

				if (newCursorStyle != cursorStyle) {
					eventPump.setCursorStyle(newCursorStyle);
					cursorStyle = newCursorStyle;
				}
				eventPump.showCursor();
			}
		}

		if (eventPump.getConnectedControllerIDs().empty()) {
			io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
		} else {
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
		}

		bool captureMouse = false;
		for (int button = 0; button < ImGuiMouseButton_COUNT; ++button) {
			if (ImGui::IsMouseDragging(button, 1.0f)) {
				captureMouse = true;
				break;
			}
		}

		if (captureMouse != mouseCaptured) {
			try {
				if (captureMouse) {
					eventPump.captureMouse();
					mouseCaptured = true;
				} else {
					eventPump.uncaptureMouse();
					mouseCaptured = false;
				}
			} catch (const events::Error&) {
			}
		}

		updateIME();

		++frameIndex;
	}

	void drawFrame(graphics::RenderPass& renderPass, const ImDrawData& drawData, SharedPointer<graphics::ShaderPipelineImplementation> shaderPipelineOverrideHandle) {
		GREM_ASSERT(drawData.Valid);

		if (!shaderPipelineOverrideHandle) {
			shaderPipelineOverrideHandle = getPlainShaderPipeline().lock();
		}

		const ContextPreserver contextPreserver{};
		ImGui::SetCurrentContext(context.get());
		ImGuiIO& io = ImGui::GetIO();
		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

		if (drawData.Textures) {
			for (ImTextureData* const textureData : *drawData.Textures) {
				GREM_ASSERT(textureData);
				if (textureData->Status == ImTextureStatus_WantCreate) {
					GREM_ASSERT(textureData->TexID == ImTextureID_Invalid);
					if (textureData->Format != ImTextureFormat_RGBA32 || textureData->BytesPerPixel != 4) {
						throw imgui::Error{"Unsupported texture format requested from the ImGui backend."};
					}
					const ImTextureID textureID{nextTextureID};
					textures.emplace(nextTextureID,
						ManagedTexture{
							.linear{graphics::Texture::create(device, graphics::TextureType::TEXTURE_2D, graphics::TextureFormat::R8G8B8A8_SRGB,
								Extent2D{.width = static_cast<uint32_t>(textureData->Width), .height = static_cast<uint32_t>(textureData->Height)}, 1, textureData->GetPixels(),
								graphics::TextureSamplerOptions{
									.minificationFilter = graphics::TextureFilter::LINEAR,
									.magnificationFilter = graphics::TextureFilter::LINEAR,
									.mipmapMode = graphics::TextureMipmapMode::NONE,
									.horizontalWrappingMode = graphics::TextureWrappingMode::CLAMP_TO_EDGE,
									.verticalWrappingMode = graphics::TextureWrappingMode::CLAMP_TO_EDGE,
									.maxAnisotropy = 1.0f,
									.depthComparisonMode{},
								})},
							.nearest{},
						});
					++nextTextureID;
					textureData->SetTexID(textureID);
					textureData->SetStatus(ImTextureStatus_OK);
				} else if (textureData->Status == ImTextureStatus_WantUpdates) {
					GREM_ASSERT(textureData->Format == ImTextureFormat_RGBA32);
					GREM_ASSERT(textureData->BytesPerPixel == 4);
					const Extent2D imageSize{.width = static_cast<uint32_t>(textureData->Width), .height = static_cast<uint32_t>(textureData->Height)};
					const auto it = textures.find(textureData->GetTexID());
					if (it == textures.end()) {
						throw imgui::Error{"Invalid texture ID provided to the ImGui backend."};
					}
					if (it->second.isExternal()) {
						throw imgui::Error{"The ImGui backend tried to update an external texture."};
					}
					if (graphics::Texture* const texture = it->second.linear.get_if<graphics::Texture>()) {
						GREM_ASSERT(texture->getType() == graphics::TextureType::TEXTURE_2D);
						texture->pasteImage(imageSize, textureData->GetPixels());
					}
					if (graphics::Texture* const texture = it->second.nearest.get_if<graphics::Texture>()) {
						GREM_ASSERT(texture->getType() == graphics::TextureType::TEXTURE_2D);
						texture->pasteImage(imageSize, textureData->GetPixels());
					}
					textureData->SetStatus(ImTextureStatus_OK);
				} else if (textureData->Status == ImTextureStatus_WantDestroy && textureData->UnusedFrames > 0) {
					textures.erase(textureData->GetTexID());
					textureData->SetTexID(ImTextureID_Invalid);
					textureData->SetStatus(ImTextureStatus_Destroyed);
				}
			}
		}

		const int framebufferWidth = static_cast<int>(drawData.DisplaySize.x * drawData.FramebufferScale.x);
		const int framebufferHeight = static_cast<int>(drawData.DisplaySize.y * drawData.FramebufferScale.y);
		if (framebufferWidth <= 0 || framebufferHeight <= 0 || drawData.TotalVtxCount <= 0 || drawData.TotalIdxCount <= 0) {
			return;
		}

		const Extent2D framebufferSize{.width = static_cast<uint32_t>(framebufferWidth), .height = static_cast<uint32_t>(framebufferHeight)};
		const graphics::Viewport viewport{.region{.size = framebufferSize}};
		renderPass.setViewport(viewport);
		useNearestTextureFilter = false;

		const vec2 guiScale{2.0f / drawData.DisplaySize.x, 2.0f / drawData.DisplaySize.y};
		const vec2 guiOffset = vec2{-1.0f} - vec2{drawData.DisplayPos.x, drawData.DisplayPos.y} * guiScale;
		viewParameterBuffer.upload(ViewParameters{.guiOffset = guiOffset, .guiScale = guiScale});

		const vec2 scissorOffset{drawData.DisplayPos.x, drawData.DisplayPos.y};
		const vec2 scissorScale{drawData.FramebufferScale.x, drawData.FramebufferScale.y};
		for (const ImDrawList* const drawList : drawData.CmdLists) {
			static_assert(sizeof(Vertex) == sizeof(ImDrawVert));
			static_assert(sizeof(Index) == sizeof(ImDrawIdx));
			const Span<const Vertex> vertices{reinterpret_cast<const Vertex*>(drawList->VtxBuffer.Data), static_cast<size_t>(drawList->VtxBuffer.Size)};
			const Span<const Index> indices{reinterpret_cast<const Index*>(drawList->IdxBuffer.Data), static_cast<size_t>(drawList->IdxBuffer.Size)};

			mesh.setVertices(vertices);

			for (const ImDrawCmd& drawCommand : drawList->CmdBuffer) {
				if (drawCommand.UserCallback) {
					if (drawCommand.UserCallback == platformIO.DrawCallback_ResetRenderState) {
						renderPass.setViewport(viewport);
						useNearestTextureFilter = false;
					} else {
						drawCommand.UserCallback(drawList, &drawCommand);
					}
				} else {
					const auto it = textures.find(drawCommand.GetTexID());
					if (it == textures.end()) {
						throw imgui::Error{"Invalid texture ID provided to the ImGui backend."};
					}

					if (useNearestTextureFilter && it->second.nearest.is<Monostate>()) {
						it->second.nearest = it->second.linear.as<graphics::Texture>().copyWithSamplerOptions(graphics::TextureSamplerOptions{
							.minificationFilter = graphics::TextureFilter::NEAREST,
							.magnificationFilter = graphics::TextureFilter::NEAREST,
							.mipmapMode = graphics::TextureMipmapMode::NONE,
							.horizontalWrappingMode = graphics::TextureWrappingMode::CLAMP_TO_EDGE,
							.verticalWrappingMode = graphics::TextureWrappingMode::CLAMP_TO_EDGE,
							.maxAnisotropy = 1.0f,
							.depthComparisonMode{},
						});
					}

					textureBuffer.upload(TextureParameters{
						.mainTexture = match((useNearestTextureFilter) ? it->second.nearest : it->second.linear)( //                                                //
							[&](Monostate) -> SharedPointer<graphics::TextureImplementation> { unreachable(); },  //
							[&](const graphics::Texture& texture) -> SharedPointer<graphics::TextureImplementation> { return texture.lock(); }, //
							[&](const SharedPointer<graphics::TextureImplementation>& handle) -> SharedPointer<graphics::TextureImplementation> { return handle; }),
					});

					GREM_ASSERT(drawCommand.VtxOffset == 0);
					const Span<const Index> drawCommandIndices = indices.subspan(drawCommand.IdxOffset, drawCommand.ElemCount);
					mesh.setIndices(drawCommandIndices);

					drawCommandBuffer.clear();
					drawCommandBuffer.push(shaderPipelineOverrideHandle, mesh.lock());

					const vec2 scissorMin = clamp((vec2{drawCommand.ClipRect.x, drawCommand.ClipRect.y} - scissorOffset) * scissorScale, vec2{}, vec2{framebufferSize});
					const vec2 scissorMax = clamp((vec2{drawCommand.ClipRect.z, drawCommand.ClipRect.w} - scissorOffset) * scissorScale, vec2{}, vec2{framebufferSize});
					if (all(lessThan(scissorMin, scissorMax))) {
						renderPass.setViewportScissor(Region2D{
							.offset{.x = static_cast<int32_t>(scissorMin.x), .y = static_cast<int32_t>(framebufferSize.height) - static_cast<int32_t>(scissorMax.y)},
							.size{.width = static_cast<uint32_t>(scissorMax.x - scissorMin.x), .height = static_cast<uint32_t>(scissorMax.y - scissorMin.y)},
						});
						renderPass.draw(drawCommandBuffer, viewParameterBuffer, textureBuffer);
					}
				}
			}
		}
	}

	[[nodiscard]] GraphicalUserInterface::TextureID getTextureID(const graphics::Texture& texture) {
		if (texture.getType() != graphics::TextureType::TEXTURE_2D) {
			throw imgui::Error{"Invalid texture type provided to the ImGui backend."};
		}
		SharedPointer<graphics::TextureImplementation> handle = texture.lock();
		const GraphicalUserInterface::TextureID textureID{nextTextureID};
		textures.emplace(nextTextureID, ManagedTexture{.linear = handle, .nearest = std::move(handle)});
		++nextTextureID;
		return textureID;
	}

	[[nodiscard]] ImGuiContext* getContext() const noexcept {
		return context.get();
	}

	[[nodiscard]] const VertexShader& getDefaultVertexShader() {
		if (!defaultVertexShader) {
			defaultVertexShader.emplace(VertexShader::create(device, graphics::detail::GUI_DEFAULT_VERTEX_SHADER_CODE));
		}
		return *defaultVertexShader;
	}

	[[nodiscard]] const FragmentShader& getPlainFragmentShader() {
		if (!plainFragmentShader) {
			plainFragmentShader.emplace(FragmentShader::create(device, graphics::detail::GUI_PLAIN_FRAGMENT_SHADER_CODE));
		}
		return *plainFragmentShader;
	}

	[[nodiscard]] const ShaderPipeline& getPlainShaderPipeline() {
		if (!plainShaderPipeline) {
			[[unlikely]];
			plainShaderPipeline.emplace(device, getDefaultVertexShader(), GraphicalUserInterface::DEFAULT_VERTEX_SHADER_CONSTANTS, getPlainFragmentShader(),
				GraphicalUserInterface::DEFAULT_FRAGMENT_SHADER_CONSTANTS, GraphicalUserInterface::DEFAULT_SHADER_PIPELINE_OPTIONS);
		}
		return *plainShaderPipeline;
	}

private:
	struct ContextDeleter {
		bool isOwned;

		void operator()(ImGuiContext* handle) const noexcept {
			if (handle && isOwned) {
				ImGui::DestroyContext(handle);
			}
		}
	};

	using ContextHandle = UniqueHandle<ImGuiContext*, ContextDeleter, nullptr>;

	struct ManagedTexture {
		Variant<Monostate, graphics::Texture, SharedPointer<graphics::TextureImplementation>> linear;
		Variant<Monostate, graphics::Texture, SharedPointer<graphics::TextureImplementation>> nearest;

		[[nodiscard]] bool isExternal() const noexcept {
			return linear.is<SharedPointer<graphics::TextureImplementation>>() || nearest.is<SharedPointer<graphics::TextureImplementation>>();
		}
	};

	void updateIME() {
		graphics::Window* const focusedWindow = graphics::Window::getFocused();
		if (imeWindow && ((!imeData.WantVisible && !imeData.WantTextInput) || imeWindow != focusedWindow)) {
			imeWindow->stopTextInput();
			imeWindow = nullptr;
		}
		if (!focusedWindow || (!imeDirty && imeWindow == focusedWindow)) {
			return;
		}

		imeDirty = false;
		if (imeData.WantVisible) {
			ImVec2 viewportPosition{};
			if (ImGuiViewport* const focusedViewport = findViewportByWindowID(focusedWindow->getID())) {
				viewportPosition = focusedViewport->Pos;
			}
			focusedWindow->setTextInputArea(
				Region2D{
					.offset{
						.x = static_cast<int32_t>(imeData.InputPos.x - viewportPosition.x),
						.y = static_cast<int32_t>(imeData.InputPos.y - viewportPosition.y),
					},
					.size{.width = 1, .height = static_cast<uint32_t>(imeData.InputLineHeight)},
				},
				0);
			imeWindow = focusedWindow;
		}
		if (!focusedWindow->isTextInputActive() && (imeData.WantVisible || imeData.WantTextInput)) {
			focusedWindow->startTextInput();
		}
	}

	events::EventPump& eventPump;
	graphics::Window& window;
	graphics::Device& device;
	graphics::Swapchain& swapchain;
	graphics::Renderer2D& renderer2D;
	HashMap<ImTextureID, ManagedTexture> textures{};
	ImTextureID nextTextureID = ImTextureID_Invalid + 1;
	Mesh mesh{device};
	ViewParameterBuffer viewParameterBuffer{device};
	TextureBuffer textureBuffer{device};
	graphics::DrawCommandBuffer<Mesh> drawCommandBuffer{device};
	Optional<VertexShader> defaultVertexShader{};
	Optional<FragmentShader> plainFragmentShader{};
	Optional<ShaderPipeline> plainShaderPipeline{};
	ContextHandle context;
	const String backendName = String{"GREM ("} + getVersionName() + ")";
	String storageForGetClipboardText{};
	graphics::Window* imeWindow = nullptr;
	ImGuiPlatformImeData imeData{};
	bool imeDirty = false;
	size_t frameIndex = 1;
	size_t mouseLeaveFrameIndex = 0;
	int mouseButtonsDown = 0;
	uint32_t mouseWindowID = 0;
	Optional<events::CursorStyle> cursorStyle{};
	bool shouldUpdateMonitors = false;
	bool mouseCaptured = false;
	bool useNearestTextureFilter = false;
};

graphics::Window& GraphicalUserInterface::getWindow(ImGuiViewport& viewport) {
	return *static_cast<graphics::Window*>(viewport.PlatformHandle);
}

graphics::Swapchain& GraphicalUserInterface::getSwapchain(ImGuiViewport& viewport) {
	return *static_cast<graphics::Swapchain*>(viewport.RendererUserData);
}

GraphicalUserInterface::GraphicalUserInterface(Filesystem& filesystem, events::EventPump& eventPump, graphics::Window& window, graphics::Device& device,
	graphics::Swapchain& swapchain, graphics::Renderer2D& renderer2D, const GraphicalUserInterfaceOptions& options)
	: implementation(UniquePointer<Implementation>::create(filesystem, eventPump, window, device, swapchain, renderer2D, nullptr, options)) {}

GraphicalUserInterface::GraphicalUserInterface(Filesystem& filesystem, events::EventPump& eventPump, graphics::Window& window, graphics::Device& device,
	graphics::Swapchain& swapchain, graphics::Renderer2D& renderer2D, ImGuiContext& context, const GraphicalUserInterfaceOptions& options)
	: implementation(UniquePointer<Implementation>::create(filesystem, eventPump, window, device, swapchain, renderer2D, &context, options)) {}

GraphicalUserInterface::~GraphicalUserInterface() = default;

bool GraphicalUserInterface::handleEvent(const events::Event& event) {
	GREM_PROFILE_FUNCTION();
	return implementation->handleEvent(event);
}

void GraphicalUserInterface::update(Duration deltaTime) {
	GREM_PROFILE_FUNCTION();
	implementation->update(deltaTime);
}

void GraphicalUserInterface::drawFrame(graphics::RenderPass& renderPass, const ImDrawData& drawData,
	SharedPointer<graphics::ShaderPipelineImplementation> shaderPipelineOverrideHandle) {
	GREM_PROFILE_FUNCTION();
	implementation->drawFrame(renderPass, drawData, std::move(shaderPipelineOverrideHandle));
}

GraphicalUserInterface::TextureID GraphicalUserInterface::getTextureID(const graphics::Texture& texture) {
	return implementation->getTextureID(texture);
}

ImGuiContext* GraphicalUserInterface::getContext() const noexcept {
	return implementation->getContext();
}

const GraphicalUserInterface::VertexShader& GraphicalUserInterface::getDefaultVertexShader() {
	return implementation->getDefaultVertexShader();
}

const GraphicalUserInterface::FragmentShader& GraphicalUserInterface::getPlainFragmentShader() {
	return implementation->getPlainFragmentShader();
}

const GraphicalUserInterface::ShaderPipeline& GraphicalUserInterface::getPlainShaderPipeline() {
	return implementation->getPlainShaderPipeline();
}

} // namespace grem::imgui
