// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/version.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Window.hpp>

#include "VulkanError.hpp"
#include "vulkan.hpp"

#include <SDL3/SDL.h>        // SDL..., Sint64
#include <SDL3/SDL_vulkan.h> // SDL_Vulkan_...

namespace grem::graphics {

namespace {

size_t windowCount = 0;
VkInstance instance = VK_NULL_HANDLE;

struct VideoSubsystemInitializer {
	[[nodiscard]] VideoSubsystemInitializer() {
		if (windowCount == 0) {
			GREM_PROFILE_BLOCK("Initialize SDL (video subsystem)");

			if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
				throw graphics::Error{String{"Failed to initialize SDL video subsystem:\n"} + SDL_GetError()};
			}
		}
	}

	~VideoSubsystemInitializer() {
		if (windowCount == 0) {
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
		}
	}

	VideoSubsystemInitializer(const VideoSubsystemInitializer&) = delete;
	VideoSubsystemInitializer(VideoSubsystemInitializer&&) = delete;
	VideoSubsystemInitializer& operator=(const VideoSubsystemInitializer&) = delete;
	VideoSubsystemInitializer& operator=(VideoSubsystemInitializer&&) = delete;
};

struct WindowDeleter {
	void operator()(SDL_Window* handle) const noexcept {
		if (handle) {
			SDL_DestroyWindow(handle);
		}
	}
};

using WindowHandle = UniqueHandle<SDL_Window*, WindowDeleter, nullptr>;

struct VolkInitializer {
	[[nodiscard]] VolkInitializer() {
		if (windowCount == 0) {
			GREM_PROFILE_BLOCK("Load Vulkan functions");

			const SDL_FunctionPointer getInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
			if (!getInstanceProcAddr) {
				throw graphics::Error{String{"Failed to get the Vulkan vkGetInstanceProcAddr function from SDL:\n"} + SDL_GetError()};
			}
			volkInitializeCustom((PFN_vkGetInstanceProcAddr)getInstanceProcAddr); // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
		}
	}

	~VolkInitializer() {
		if (windowCount == 0) {
			volkFinalize();
		}
	}

	VolkInitializer(const VolkInitializer&) = delete;
	VolkInitializer(VolkInitializer&&) = delete;
	VolkInitializer& operator=(const VolkInitializer&) = delete;
	VolkInitializer& operator=(VolkInitializer&&) = delete;
};

struct InstanceInitializer {
	[[nodiscard]] InstanceInitializer(SDL_Window* window) {
		if (windowCount == 0) {
			GREM_PROFILE_BLOCK("Create Vulkan instance");

			Uint32 extensionCount = 0;
			const char* const* const extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
			if (!extensionNames) {
				throw graphics::Error{String{"Failed to get the names of the required Vulkan instance extensions for SDL:\n"} + SDL_GetError()};
			}

			Allocation<const char*> enabledExtensions(static_cast<size_t>(extensionCount) + 1);
			for (size_t i = 0; i < extensionCount; ++i) {
				enabledExtensions[i] = extensionNames[i];
			}
			enabledExtensions.back() = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;

			const VkApplicationInfo applicationInfo{
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pNext = nullptr,
				.pApplicationName = SDL_GetWindowTitle(window),
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "GREM",
				.engineVersion = VK_MAKE_VERSION(getMajorVersion(), getMinorVersion(), getPatchVersion()),
				.apiVersion = VK_API_VERSION_1_2,
			};
			VkInstanceCreateInfo instanceCreateInfo{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkInstanceCreateFlags{},
				.pApplicationInfo = &applicationInfo,
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,
				.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
				.ppEnabledExtensionNames = enabledExtensions.data(),
			};
			VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
			if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
				--instanceCreateInfo.enabledExtensionCount;
				result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
			}
			if (result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateInstance", result};
			}
		}
	}

	~InstanceInitializer() {
		if (windowCount == 0) {
			vkDestroyInstance(instance, nullptr);
			instance = VK_NULL_HANDLE;
		}
	}

	InstanceInitializer(const InstanceInitializer&) = delete;
	InstanceInitializer(InstanceInitializer&&) = delete;
	InstanceInitializer& operator=(const InstanceInitializer&) = delete;
	InstanceInitializer& operator=(InstanceInitializer&&) = delete;
};

} // namespace

Window::Implementation::Implementation(Window* parent, const WindowOptions& options)
	: multisampleCount(options.multisampleCount) {
	GREM_PROFILE_FUNCTION();

	const VideoSubsystemInitializer videoSubsystemInitializer{};

	WindowHandle windowHandle{};
	{
		GREM_PROFILE_BLOCK("Create SDL window");

		SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;
		windowFlags |= (options.highPixelDensity) ? SDL_WINDOW_HIGH_PIXEL_DENSITY : SDL_WindowFlags{};
		windowFlags |= (options.hidden) ? SDL_WINDOW_HIDDEN : SDL_WindowFlags{};
		windowFlags |= (options.focus) ? SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS : SDL_WindowFlags{};
		windowFlags |= (options.minimized) ? SDL_WINDOW_MINIMIZED : SDL_WindowFlags{};
		windowFlags |= (options.resizable) ? SDL_WINDOW_RESIZABLE : SDL_WindowFlags{};
		windowFlags |= (options.fullscreen && options.resizable) ? SDL_WINDOW_FULLSCREEN : SDL_WindowFlags{};
		windowFlags |= (options.borderless) ? SDL_WINDOW_BORDERLESS : SDL_WindowFlags{};
		windowFlags |= (options.hideFromTaskbar) ? SDL_WINDOW_UTILITY : SDL_WindowFlags{};
		windowFlags |= (options.alwaysOnTop) ? SDL_WINDOW_ALWAYS_ON_TOP : SDL_WindowFlags{};

		const SDL_PropertiesID windowProperties = SDL_CreateProperties();
		if (windowProperties == 0) {
			throw graphics::Error{String{"Failed to create window properties:\n"} + SDL_GetError()};
		}
		if (parent) {
			SDL_SetPointerProperty(windowProperties, SDL_PROP_WINDOW_CREATE_PARENT_POINTER, parent->get());
		}
		if (!options.title.empty()) {
			SDL_SetStringProperty(windowProperties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, options.title.c_str());
		}
		if (options.positionX) {
			SDL_SetNumberProperty(windowProperties, SDL_PROP_WINDOW_CREATE_X_NUMBER, static_cast<Sint64>(*options.positionX));
		}
		if (options.positionY) {
			SDL_SetNumberProperty(windowProperties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, static_cast<Sint64>(*options.positionY));
		}
		SDL_SetNumberProperty(windowProperties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<Sint64>(options.size.width));
		SDL_SetNumberProperty(windowProperties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<Sint64>(options.size.height));
		SDL_SetNumberProperty(windowProperties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<Sint64>(windowFlags));

		windowHandle.reset(SDL_CreateWindowWithProperties(windowProperties));
	}
	if (!windowHandle) {
		throw graphics::Error{String{"Failed to create window:\n"} + SDL_GetError()};
	}

	const VolkInitializer volkInitializer{};
	const InstanceInitializer instanceInitializer{windowHandle.get()};
	if (windowCount == 0) {
		volkLoadInstanceOnly(instance);
	}

	const SDL_PropertiesID properties = SDL_GetWindowProperties(windowHandle.get());
	if (properties != 0) {
		SDL_SetPointerProperty(properties, "GREM.VkInstance", instance);
	}

	VkSurfaceKHR surfaceHandle = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(windowHandle.get(), instance, nullptr, &surfaceHandle)) {
		throw graphics::Error{String{"Failed to create SDL Vulkan window surface:\n"} + SDL_GetError()};
	}

	window = windowHandle.release();
	surface = surfaceHandle;
	++windowCount;
}

Window::Implementation::~Implementation() {
	vkDestroySurfaceKHR(static_cast<VkInstance>(instance), static_cast<VkSurfaceKHR>(surface), nullptr);
	if (--windowCount == 0) {
		vkDestroyInstance(static_cast<VkInstance>(instance), nullptr);
		SDL_DestroyWindow(static_cast<SDL_Window*>(window));
		volkFinalize();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	} else {
		SDL_DestroyWindow(static_cast<SDL_Window*>(window));
	}
}

} // namespace grem::graphics
