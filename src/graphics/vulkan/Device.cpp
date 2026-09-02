// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/Window.hpp>

#include "DeviceImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "ShaderImplementation.hpp"
#include "StagingBuffer.hpp"
#include "TextureImplementation.hpp"
#include "TextureResources.hpp"
#include "VulkanError.hpp"

#include <SDL3/SDL.h> // SDL_..., Uint32
#include <utility>    // std::move, std::exchange
#include <zstd.h>     // ZSTD_...

#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
#include <vector> // std::vector
#endif

namespace grem::graphics {

namespace {

constexpr uint32_t API_VERSION = VK_API_VERSION_1_2;
#define GREM_PRIVATE_GRAPHICS_VULKAN_API_VERSION_STRING "1.2"

constexpr Array REQUIRED_EXTENSIONS{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr Array REQUIRED_FEATURES{
	Pair{"sampler anisotropy", &VkPhysicalDeviceFeatures::samplerAnisotropy},
	Pair{"multi-draw indirect", &VkPhysicalDeviceFeatures::multiDrawIndirect},
	Pair{"first-instance-specified indirect drawing", &VkPhysicalDeviceFeatures::drawIndirectFirstInstance},
	Pair{"cube array images", &VkPhysicalDeviceFeatures::imageCubeArray},
	Pair{"non-solid fill modes", &VkPhysicalDeviceFeatures::fillModeNonSolid},
};

constexpr Array REQUIRED_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES{
	Pair{"separate depth/stencil layouts", &VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures::separateDepthStencilLayouts},
};

constexpr Array REQUIRED_SHADER_DRAW_PARAMETERS_FEATURES{
	Pair{"shader draw parameters", &VkPhysicalDeviceShaderDrawParametersFeatures::shaderDrawParameters},
};

constexpr Array REQUIRED_DESCRIPTOR_INDEXING_FEATURES{
	Pair{"non-uniform indexing of shader-sampled image arrays", &VkPhysicalDeviceDescriptorIndexingFeatures::shaderSampledImageArrayNonUniformIndexing},
	Pair{"runtime descriptor arrays", &VkPhysicalDeviceDescriptorIndexingFeatures::runtimeDescriptorArray},
	Pair{"variable descriptor counts in descriptor bindings", &VkPhysicalDeviceDescriptorIndexingFeatures::descriptorBindingVariableDescriptorCount},
	Pair{"partially bound descriptors", &VkPhysicalDeviceDescriptorIndexingFeatures::descriptorBindingPartiallyBound},
};

constexpr size_t MAX_SIMULTANEOUS_STAGING_BUFFER_COUNT = 32;
constexpr size_t MAX_SIMULTANEOUS_TOTAL_STAGING_BUFFER_SIZE = 1'073'741'824;
constexpr size_t MAX_SIMULTANEOUS_GRAPHICS_QUEUE_SUBMISSION_COUNT = 256;
constexpr size_t MAX_SIMULTANEOUS_RENDER_PASSES_IN_FLIGHT = 128;

[[nodiscard]] VkFormat translateVertexAttributeFormat(VertexAttributeType vertexAttributeType) noexcept {
	switch (vertexAttributeType) {
		case VertexAttributeType::U8NORM: return VK_FORMAT_R8_UNORM;
		case VertexAttributeType::I8NORM: return VK_FORMAT_R8_SNORM;
		case VertexAttributeType::U8: return VK_FORMAT_R8_UINT;
		case VertexAttributeType::I8: return VK_FORMAT_R8_SINT;
		case VertexAttributeType::U8VEC2NORM: return VK_FORMAT_R8G8_UNORM;
		case VertexAttributeType::I8VEC2NORM: return VK_FORMAT_R8G8_SNORM;
		case VertexAttributeType::U8VEC2: return VK_FORMAT_R8G8_UINT;
		case VertexAttributeType::I8VEC2: return VK_FORMAT_R8G8_SINT;
		case VertexAttributeType::U8VEC4NORM: return VK_FORMAT_R8G8B8A8_UNORM;
		case VertexAttributeType::I8VEC4NORM: return VK_FORMAT_R8G8B8A8_SNORM;
		case VertexAttributeType::U8VEC4: return VK_FORMAT_R8G8B8A8_UINT;
		case VertexAttributeType::I8VEC4: return VK_FORMAT_R8G8B8A8_SINT;
		case VertexAttributeType::UA2B10G10R10VEC4NORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case VertexAttributeType::IA2B10G10R10VEC4NORM: return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
		case VertexAttributeType::U16NORM: return VK_FORMAT_R16_UNORM;
		case VertexAttributeType::I16NORM: return VK_FORMAT_R16_SNORM;
		case VertexAttributeType::U16: return VK_FORMAT_R16_UINT;
		case VertexAttributeType::I16: return VK_FORMAT_R16_SINT;
		case VertexAttributeType::F16: return VK_FORMAT_R16_SFLOAT;
		case VertexAttributeType::U16VEC2NORM: return VK_FORMAT_R16G16_UNORM;
		case VertexAttributeType::I16VEC2NORM: return VK_FORMAT_R16G16_SNORM;
		case VertexAttributeType::U16VEC2: return VK_FORMAT_R16G16_UINT;
		case VertexAttributeType::I16VEC2: return VK_FORMAT_R16G16_SINT;
		case VertexAttributeType::F16VEC2: return VK_FORMAT_R16G16_SFLOAT;
		case VertexAttributeType::U16VEC4NORM: return VK_FORMAT_R16G16B16A16_UNORM;
		case VertexAttributeType::I16VEC4NORM: return VK_FORMAT_R16G16B16A16_SNORM;
		case VertexAttributeType::U16VEC4: return VK_FORMAT_R16G16B16A16_UINT;
		case VertexAttributeType::I16VEC4: return VK_FORMAT_R16G16B16A16_SINT;
		case VertexAttributeType::F16VEC4: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case VertexAttributeType::U32: return VK_FORMAT_R32_UINT;
		case VertexAttributeType::I32: return VK_FORMAT_R32_SINT;
		case VertexAttributeType::F32: return VK_FORMAT_R32_SFLOAT;
		case VertexAttributeType::U32VEC2: return VK_FORMAT_R32G32_UINT;
		case VertexAttributeType::I32VEC2: return VK_FORMAT_R32G32_SINT;
		case VertexAttributeType::F32VEC2: return VK_FORMAT_R32G32_SFLOAT;
		case VertexAttributeType::U32VEC3: return VK_FORMAT_R32G32B32_UINT;
		case VertexAttributeType::I32VEC3: return VK_FORMAT_R32G32B32_SINT;
		case VertexAttributeType::F32VEC3: return VK_FORMAT_R32G32B32_SFLOAT;
		case VertexAttributeType::U32VEC4: return VK_FORMAT_R32G32B32A32_UINT;
		case VertexAttributeType::I32VEC4: return VK_FORMAT_R32G32B32A32_SINT;
		case VertexAttributeType::F32VEC4: return VK_FORMAT_R32G32B32A32_SFLOAT;
	}
	unreachable();
}

[[nodiscard]] VkPrimitiveTopology translatePrimitiveType(PrimitiveType primitiveType) noexcept {
	switch (primitiveType) {
		case PrimitiveType::POINTS: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveType::LINES: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveType::LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case PrimitiveType::TRIANGLES: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveType::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	}
	return {};
}

[[nodiscard]] VkPolygonMode translatePolygonMode(PolygonMode polygonMode) noexcept {
	switch (polygonMode) {
		case PolygonMode::POINT: return VK_POLYGON_MODE_POINT;
		case PolygonMode::LINE: return VK_POLYGON_MODE_LINE;
		case PolygonMode::FILL: return VK_POLYGON_MODE_FILL;
	}
	return {};
}

[[nodiscard]] VkCullModeFlags translateFaceCullingMode(FaceCullingMode faceCullingMode) noexcept {
	switch (faceCullingMode) {
		case FaceCullingMode::NONE: return VK_CULL_MODE_NONE;
		case FaceCullingMode::CULL_BACK_FACES: return VK_CULL_MODE_BACK_BIT;
		case FaceCullingMode::CULL_FRONT_FACES: return VK_CULL_MODE_FRONT_BIT;
		case FaceCullingMode::CULL_FRONT_AND_BACK_FACES: return VK_CULL_MODE_FRONT_AND_BACK;
	}
	return {};
}

[[nodiscard]] VkFrontFace translateFrontFace(FrontFace frontFace) noexcept {
	switch (frontFace) {
		case FrontFace::COUNTERCLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case FrontFace::CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
	}
	return {};
}

[[nodiscard]] VkCompareOp translateDepthTestPredicate(DepthTestPredicate depthTestPredicate) noexcept {
	switch (depthTestPredicate) {
		case DepthTestPredicate::NEVER_PASS: return VK_COMPARE_OP_NEVER;
		case DepthTestPredicate::LESS: return VK_COMPARE_OP_LESS;
		case DepthTestPredicate::LESS_OR_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case DepthTestPredicate::GREATER: return VK_COMPARE_OP_GREATER;
		case DepthTestPredicate::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case DepthTestPredicate::EQUAL: return VK_COMPARE_OP_EQUAL;
		case DepthTestPredicate::NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
		case DepthTestPredicate::ALWAYS_PASS: return VK_COMPARE_OP_ALWAYS;
	}
	return {};
}

[[nodiscard]] VkCompareOp translateStencilTestPredicate(StencilTestPredicate stencilTestPredicate) noexcept {
	switch (stencilTestPredicate) {
		case StencilTestPredicate::NEVER_PASS: return VK_COMPARE_OP_NEVER;
		case StencilTestPredicate::LESS: return VK_COMPARE_OP_LESS;
		case StencilTestPredicate::LESS_OR_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case StencilTestPredicate::GREATER: return VK_COMPARE_OP_GREATER;
		case StencilTestPredicate::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case StencilTestPredicate::EQUAL: return VK_COMPARE_OP_EQUAL;
		case StencilTestPredicate::NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
		case StencilTestPredicate::ALWAYS_PASS: return VK_COMPARE_OP_ALWAYS;
	}
	return {};
}

[[nodiscard]] VkStencilOp translateStencilBufferOperation(StencilBufferOperation stencilBufferOperation) noexcept {
	switch (stencilBufferOperation) {
		case StencilBufferOperation::KEEP: return VK_STENCIL_OP_KEEP;
		case StencilBufferOperation::SET_TO_ZERO: return VK_STENCIL_OP_ZERO;
		case StencilBufferOperation::REPLACE: return VK_STENCIL_OP_REPLACE;
		case StencilBufferOperation::INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case StencilBufferOperation::INCREMENT_AND_WRAP: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case StencilBufferOperation::DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case StencilBufferOperation::DECREMENT_AND_WRAP: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
		case StencilBufferOperation::BITWISE_INVERT: return VK_STENCIL_OP_INVERT;
	}
	return {};
}

[[nodiscard]] VkBlendFactor translateBlendFactor(BlendFactor blendFactor) noexcept {
	switch (blendFactor) {
		case BlendFactor::ZERO: return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::ONE: return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SOURCE_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::ONE_MINUS_SOURCE_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::DESTINATION_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::ONE_MINUS_DESTINATION_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::SOURCE_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::ONE_MINUS_SOURCE_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DESTINATION_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::ONE_MINUS_DESTINATION__ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::CONSTANT_COLOR: return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::ONE_MINUS_CONSTANT_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFactor::CONSTANT_ALPHA: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case BlendFactor::ONE_MINUS_CONSTANT_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFactor::SOURCE_ALPHA_SATURATE: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	}
	return {};
}

[[nodiscard]] VkBlendOp translateBlendOperation(BlendOperation blendOperation) noexcept {
	switch (blendOperation) {
		case BlendOperation::ADD: return VK_BLEND_OP_ADD;
		case BlendOperation::SUBTRACT: return VK_BLEND_OP_SUBTRACT;
		case BlendOperation::REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOperation::MIN: return VK_BLEND_OP_MIN;
		case BlendOperation::MAX: return VK_BLEND_OP_MAX;
	}
	return {};
}

[[nodiscard]] String formatAPIVersion(uint32_t version) {
	return formatString("{}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
}

[[nodiscard]] VkInstance getInstance(const Window& window) {
	const SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(window.get()));
	if (properties == 0) {
		throw graphics::Error{String{"Failed to get window properties:\n"} + SDL_GetError()};
	}
	void* const instance = SDL_GetPointerProperty(properties, "GREM.VkInstance", nullptr);
	if (!instance) {
		throw graphics::Error{String{"Failed to get Vulkan instance from window properties:\n"} + SDL_GetError()};
	}
	return static_cast<VkInstance>(instance);
}

[[nodiscard]] DeviceImplementation::PhysicalDevice choosePhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
	GREM_PROFILE_FUNCTION();

	static constexpr auto supportsExtension = [](const auto& extensionProperties, CStringView extensionName) -> bool {
		return anyOf(extensionProperties, [&](const VkExtensionProperties& properties) -> bool { return properties.extensionName == extensionName; });
	};

	static constexpr auto checkExtensions = [](ArrayList<String>& incompatibilityReasons, const auto& extensionProperties, const auto& requiredExtensions) -> void {
		for (const CStringView extensionName : requiredExtensions) {
			if (!supportsExtension(extensionProperties, extensionName)) {
				incompatibilityReasons.push_back(formatString("does not support extension \"{}\"", extensionName));
			}
		}
	};

	static constexpr auto checkFeatures = [](ArrayList<String>& incompatibilityReasons, const auto& features, const auto& requiredFeatures) -> void {
		for (const auto& [featureDescription, feature] : requiredFeatures) {
			if (features.*feature == VK_FALSE) {
				incompatibilityReasons.push_back(formatString("does not support \"{}\"", featureDescription));
			}
		}
	};

	const char* const videoDriverName = SDL_GetCurrentVideoDriver();

	// Enumerate physical devices.
	uint32_t physicalDeviceCount = 0;
	if (const VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkEnumeratePhysicalDevices", result};
	}
	Allocation<VkPhysicalDevice> physicalDevices(static_cast<size_t>(physicalDeviceCount));
	if (const VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkEnumeratePhysicalDevices", result};
	}

	// Determine compatibility and rate the suitability of each physical device.
	struct PhysicalDeviceRating {
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		Array<uint8_t, 16> uuid{};
		ArrayList<String> incompatibilityReasons{};
		Optional<uint32_t> graphicsQueueFamilyIndex{};
		Optional<uint32_t> presentQueueFamilyIndex{};
		Optional<VkSurfaceFormatKHR> surfaceFormat{};
		Optional<VkFormat> depthStencilFormat{};
		FeatureSupport supportedFeatures{
			.graphicsBackendAPIName = "Vulkan",
			.graphicsBackendAPIVersionName = GREM_PRIVATE_GRAPHICS_VULKAN_API_VERSION_STRING,
#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
			.supportsGLSLShaderCode = true,
#else
			.supportsGLSLShaderCode = false,
#endif
			.supportsSPIRVShaderCode = true,
		};
		int64_t suitabilityScore = 0;
	};
	Allocation<PhysicalDeviceRating> physicalDeviceRatings(static_cast<size_t>(physicalDeviceCount));
	for (uint32_t physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount; ++physicalDeviceIndex) {
		const VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
		PhysicalDeviceRating& rating = physicalDeviceRatings[physicalDeviceIndex];
		rating.physicalDevice = physicalDevice;

		if (videoDriverName) {
			rating.supportedFeatures.videoDriverName = videoDriverName;
		}

		// Make sure the device supports all required extensions.
		uint32_t extensionPropertyCount = 0;
		if (const VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionPropertyCount, nullptr); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkEnumerateDeviceExtensionProperties", result};
		}
		Allocation<VkExtensionProperties> extensionProperties(static_cast<size_t>(extensionPropertyCount));
		if (const VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionPropertyCount, extensionProperties.data()); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkEnumerateDeviceExtensionProperties", result};
		}
		checkExtensions(rating.incompatibilityReasons, extensionProperties, REQUIRED_EXTENSIONS);

		// Make sure the device supports all required features.
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		checkFeatures(rating.incompatibilityReasons, features, REQUIRED_FEATURES);

		VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
		descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descriptorIndexingFeatures.pNext = nullptr;
		VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures{};
		shaderDrawParametersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shaderDrawParametersFeatures.pNext = &descriptorIndexingFeatures;
		VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilLayoutsFeatures{};
		separateDepthStencilLayoutsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
		separateDepthStencilLayoutsFeatures.pNext = &shaderDrawParametersFeatures;
		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &separateDepthStencilLayoutsFeatures;
		vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
		checkFeatures(rating.incompatibilityReasons, separateDepthStencilLayoutsFeatures, REQUIRED_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);
		checkFeatures(rating.incompatibilityReasons, shaderDrawParametersFeatures, REQUIRED_SHADER_DRAW_PARAMETERS_FEATURES);
		checkFeatures(rating.incompatibilityReasons, descriptorIndexingFeatures, REQUIRED_DESCRIPTOR_INDEXING_FEATURES);

		// Find suitable graphics/presentation queue families.
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		Allocation<VkQueueFamilyProperties> queueFamilyProperties(static_cast<size_t>(queueFamilyCount));
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
		const uint32_t invalidFamilyIndex = queueFamilyCount;
		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex) {
			const VkQueueFamilyProperties& properties = queueFamilyProperties[queueFamilyIndex];
			if (properties.queueCount > 0) {
				const bool supportsGraphics = (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
				VkBool32 surfaceSupport = VK_FALSE;
				if (const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &surfaceSupport); result != VK_SUCCESS) {
					throw detail::VulkanError{"vkGetPhysicalDeviceSurfaceSupport", result};
				}
				const bool supportsPresentation = surfaceSupport == VK_TRUE;
				if (supportsGraphics && supportsPresentation) {
					rating.graphicsQueueFamilyIndex = queueFamilyIndex;
					rating.presentQueueFamilyIndex = queueFamilyIndex;
					break;
				}
				if (supportsGraphics && rating.graphicsQueueFamilyIndex == invalidFamilyIndex) {
					rating.graphicsQueueFamilyIndex = queueFamilyIndex;
				}
				if (supportsPresentation && rating.presentQueueFamilyIndex == invalidFamilyIndex) {
					rating.presentQueueFamilyIndex = queueFamilyIndex;
				}
			}
		}
		if (!rating.graphicsQueueFamilyIndex || !rating.presentQueueFamilyIndex) {
			rating.incompatibilityReasons.emplace_back("does not have suitable queue families for graphics/presentation");
		}

		// Find a suitable surface format.
		uint32_t surfaceFormatCount = 0;
		if (const VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkGetPhysicalDeviceSurfaceFormats", result};
		}
		Allocation<VkSurfaceFormatKHR> surfaceFormats(static_cast<size_t>(surfaceFormatCount));
		if (const VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data()); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkGetPhysicalDeviceSurfaceFormats", result};
		}
		for (const VkSurfaceFormatKHR desiredFormat : {
				 VkSurfaceFormatKHR{.format = VK_FORMAT_R16G16B16A16_SFLOAT, .colorSpace = VK_COLOR_SPACE_BT709_LINEAR_EXT},
				 VkSurfaceFormatKHR{.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32, .colorSpace = VK_COLOR_SPACE_BT709_LINEAR_EXT},
				 VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
				 VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			 }) {
			if (const auto it = findIf(surfaceFormats,
					[&](const VkSurfaceFormatKHR& surfaceFormat) -> bool {
						return surfaceFormat.format == desiredFormat.format && surfaceFormat.colorSpace == desiredFormat.colorSpace;
					});
				it != surfaceFormats.end()) {
				rating.surfaceFormat = *it;
				break;
			}
		}
		if (!rating.surfaceFormat) {
			rating.incompatibilityReasons.emplace_back("does not have a suitable supported surface format");
		}

		// Find a suitable depth/stencil format.
		for (const VkFormat desiredFormat : {
				 VK_FORMAT_D32_SFLOAT_S8_UINT,
				 VK_FORMAT_D24_UNORM_S8_UINT,
				 VK_FORMAT_D16_UNORM_S8_UINT,
			 }) {
			VkFormatProperties formatProperties{};
			vkGetPhysicalDeviceFormatProperties(physicalDevice, desiredFormat, &formatProperties);
			if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
				rating.depthStencilFormat = desiredFormat;
				break;
			}
		}
		if (!rating.depthStencilFormat) {
			rating.incompatibilityReasons.emplace_back("does not have a suitable supported depth/stencil format");
		}

		// Make sure the device supports at least one present mode for the surface.
		uint32_t presentModeCount = 0;
		if (const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkGetPhysicalDeviceSurfacePresentModes", result};
		}
		if (presentModeCount == 0) {
			rating.incompatibilityReasons.emplace_back("does not support any presentation modes for the window surface");
		}

		// Make sure the device supports the surface being used as a transfer source/destination.
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		if (const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkGetPhysicalDeviceSurfaceCapabilities", result};
		}
		if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
			rating.incompatibilityReasons.emplace_back("does not support transfer operations from the window surface");
		}
		if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
			rating.incompatibilityReasons.emplace_back("does not support transfer operations to the window surface");
		}

		// Save properties.
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		memcpy(rating.uuid.data(), properties.pipelineCacheUUID, sizeof(properties.pipelineCacheUUID));
		rating.supportedFeatures.max2DTextureResolution = properties.limits.maxImageDimension2D;
		rating.supportedFeatures.maxCubeTextureResolution = properties.limits.maxImageDimensionCube;
		rating.supportedFeatures.maxTextureLayerCount = properties.limits.maxImageArrayLayers;
		rating.supportedFeatures.maxFramebufferSize = {.width = properties.limits.maxFramebufferWidth, .height = properties.limits.maxFramebufferHeight};
		const VkSampleCountFlags supportedMultisampleCounts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
		if ((supportedMultisampleCounts & (VK_SAMPLE_COUNT_64_BIT | VK_SAMPLE_COUNT_32_BIT | VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT |
											  VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) ==
			(VK_SAMPLE_COUNT_64_BIT | VK_SAMPLE_COUNT_32_BIT | VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT |
				VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 64;
		} else if ((supportedMultisampleCounts &
					   (VK_SAMPLE_COUNT_32_BIT | VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) ==
				   (VK_SAMPLE_COUNT_32_BIT | VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 32;
		} else if ((supportedMultisampleCounts & (VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) ==
				   (VK_SAMPLE_COUNT_16_BIT | VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 16;
		} else if ((supportedMultisampleCounts & (VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) ==
				   (VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 8;
		} else if ((supportedMultisampleCounts & (VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) ==
				   (VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 4;
		} else if ((supportedMultisampleCounts & (VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) == (VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_1_BIT)) {
			rating.supportedFeatures.maxSupportedMultisampleCount = 2;
		}
		rating.supportedFeatures.maxSupportedSamplerAnisotropy = properties.limits.maxSamplerAnisotropy;
		if (features.textureCompressionASTC_LDR == VK_TRUE) {
			rating.supportedFeatures.supportsTextureCompressionASTC_LDR = true;
			rating.supportedFeatures.supportsASTCDecodeMode = supportsExtension(extensionProperties, VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME);
		}
		if (features.textureCompressionBC == VK_TRUE) {
			rating.supportedFeatures.supportsTextureCompressionS3TC = true;
			rating.supportedFeatures.supportsTextureCompressionS3TC_SRGB = true;
			rating.supportedFeatures.supportsTextureCompressionRGTC = true;
			rating.supportedFeatures.supportsTextureCompressionBPTC = true;
		}
		if (features.textureCompressionETC2) {
			rating.supportedFeatures.supportsTextureCompressionETC2 = true;
		}
		if (supportsExtension(extensionProperties, VK_IMG_FORMAT_PVRTC_EXTENSION_NAME)) {
			rating.supportedFeatures.supportsTextureCompressionPVRTC = true;
			rating.supportedFeatures.supportsTextureCompressionPVRTC_SRGB = true;
		}
		if (supportsExtension(extensionProperties, VK_KHR_PRESENT_ID_EXTENSION_NAME) && supportsExtension(extensionProperties, VK_KHR_PRESENT_WAIT_EXTENSION_NAME)) {
			rating.supportedFeatures.supportsAwaitPresentation = true;
		}

		// Determine suitability score.
		if (rating.incompatibilityReasons.empty()) {
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				rating.suitabilityScore += 8192;
			} else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
				rating.suitabilityScore -= 16384;
			}
			rating.suitabilityScore += static_cast<int64_t>(properties.limits.maxImageDimension2D);
		}
	}

	// Move incompatible devices to the end of the container.
	const auto compatiblePhysicalDevicesEnd =
		stablePartition(physicalDeviceRatings, [&](const PhysicalDeviceRating& rating) -> bool { return rating.incompatibilityReasons.empty(); });

	// Make sure that at least one device is compatible.
	if (compatiblePhysicalDevicesEnd == physicalDeviceRatings.begin()) {
#ifndef NDEBUG
		eprintln("Vulkan physical devices:");
		for (size_t i = 0; i < physicalDeviceRatings.size(); ++i) {
			const PhysicalDeviceRating& rating = physicalDeviceRatings[i];
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(rating.physicalDevice, &properties);
			CStringView deviceTypeName = "Unknown";
			switch (properties.deviceType) {
				case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceTypeName = "OTHER"; break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceTypeName = "INTEGRATED_GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceTypeName = "DISCRETE_GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceTypeName = "VIRTUAL_GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceTypeName = "CPU"; break;
				default: break;
			}
			eprintln(
				"  [{}]:\n"
				"    Name: \"{}\"\n"
				"    Type: {}\n"
				"    Vendor ID: 0x{:X}\n"
				"    Device ID: 0x{:X}\n"
				"    API Version: {}\n"
				"    Driver Version: {}\n"
				"    Compatibility: {}\n"
				"    Suitability rating: {}",
				i, static_cast<const char*>(properties.deviceName), deviceTypeName, properties.vendorID, properties.deviceID, formatAPIVersion(properties.apiVersion),
				formatAPIVersion(properties.driverVersion), (rating.incompatibilityReasons.empty()) ? "compatible" : "incompatible", rating.suitabilityScore);
		}
#endif

		String errorMessage{"Failed to find a compatible physical device for Vulkan rendering."};
		for (const PhysicalDeviceRating& rating : physicalDeviceRatings) {
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(rating.physicalDevice, &properties);
			errorMessage.append("\n  Note: Device \"");
			errorMessage.append(static_cast<const char*>(properties.deviceName));
			errorMessage.append("\" (with driver version ");
			errorMessage.append(formatAPIVersion(properties.driverVersion));
			errorMessage.append(") is incompatible because it:");
			for (size_t i = 0; i < rating.incompatibilityReasons.size(); ++i) {
				errorMessage.append("\n  - ");
				errorMessage.append(rating.incompatibilityReasons[i]);
				if (i + 1 == rating.incompatibilityReasons.size()) {
					errorMessage.push_back('.');
				} else if (i + 2 == rating.incompatibilityReasons.size()) {
					errorMessage.append(", and");
				} else {
					errorMessage.push_back(',');
				}
			}
		}
		errorMessage.append("\nIf your device supports Vulkan " GREM_PRIVATE_GRAPHICS_VULKAN_API_VERSION_STRING
							" and should be compatible, please make sure that it has the latest available driver version installed.");
		throw graphics::Error{errorMessage};
	}

	// Sort the remaining devices by decreasing suitability.
	// Note: Stable sort is used to preserve the ordering suggested by the driver in the case of equal suitability.
	stableSortByDescending<&PhysicalDeviceRating::suitabilityScore>(Subrange{physicalDeviceRatings.begin(), compatiblePhysicalDevicesEnd});

	// Choose the most suitable device.
	const PhysicalDeviceRating& mostSuitablePhysicalDevice = physicalDeviceRatings.front();
	return DeviceImplementation::PhysicalDevice{
		.handle = mostSuitablePhysicalDevice.physicalDevice,
		.uuid = mostSuitablePhysicalDevice.uuid,
		.supportedFeatures = mostSuitablePhysicalDevice.supportedFeatures,
		.graphicsQueueFamilyIndex = *mostSuitablePhysicalDevice.graphicsQueueFamilyIndex,
		.presentQueueFamilyIndex = *mostSuitablePhysicalDevice.presentQueueFamilyIndex,
		.surfaceFormat = *mostSuitablePhysicalDevice.surfaceFormat,
		.depthStencilFormat = *mostSuitablePhysicalDevice.depthStencilFormat,
	};
}

[[nodiscard]] detail::VulkanDevice createLogicalDevice(const DeviceImplementation::PhysicalDevice& physicalDevice) {
	GREM_PROFILE_FUNCTION();

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
	descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptorIndexingFeatures.pNext = nullptr;
	for (const auto& [featureDescription, feature] : REQUIRED_DESCRIPTOR_INDEXING_FEATURES) {
		descriptorIndexingFeatures.*feature = VK_TRUE;
	}
	VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures{};
	shaderDrawParametersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shaderDrawParametersFeatures.pNext = &descriptorIndexingFeatures;
	for (const auto& [featureDescription, feature] : REQUIRED_SHADER_DRAW_PARAMETERS_FEATURES) {
		shaderDrawParametersFeatures.*feature = VK_TRUE;
	}
	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilLayoutsFeatures{};
	separateDepthStencilLayoutsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
	separateDepthStencilLayoutsFeatures.pNext = &shaderDrawParametersFeatures;
	for (const auto& [featureDescription, feature] : REQUIRED_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES) {
		separateDepthStencilLayoutsFeatures.*feature = VK_TRUE;
	}
	VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,
		.pNext = &separateDepthStencilLayoutsFeatures,
		.presentId = VK_TRUE,
	};
	VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,
		.pNext = &presentIdFeatures,
		.presentWait = VK_TRUE,
	};
	const void* const pNext =
		(physicalDevice.supportedFeatures.supportsAwaitPresentation) ? static_cast<void*>(&presentWaitFeatures) : static_cast<void*>(&separateDepthStencilLayoutsFeatures);

	const Array<float, 1> queuePriorities{{1.0f}};
	InplaceBuffer<VkDeviceQueueCreateInfo, 2> queueCreateInfos{{
		VkDeviceQueueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDeviceQueueCreateFlags{},
			.queueFamilyIndex = physicalDevice.graphicsQueueFamilyIndex,
			.queueCount = static_cast<uint32_t>(queuePriorities.size()),
			.pQueuePriorities = queuePriorities.data(),
		},
	}};
	if (physicalDevice.presentQueueFamilyIndex != physicalDevice.graphicsQueueFamilyIndex) {
		queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDeviceQueueCreateFlags{},
			.queueFamilyIndex = physicalDevice.presentQueueFamilyIndex,
			.queueCount = static_cast<uint32_t>(queuePriorities.size()),
			.pQueuePriorities = queuePriorities.data(),
		});
	}

	VkPhysicalDeviceFeatures enabledFeatures{};
	for (const auto& [featureDescription, feature] : REQUIRED_FEATURES) {
		enabledFeatures.*feature = VK_TRUE;
	}
	if (physicalDevice.supportedFeatures.supportsTextureCompressionASTC_LDR) {
		enabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
	}
	if (physicalDevice.supportedFeatures.supportsTextureCompressionS3TC || physicalDevice.supportedFeatures.supportsTextureCompressionS3TC_SRGB ||
		physicalDevice.supportedFeatures.supportsTextureCompressionRGTC || physicalDevice.supportedFeatures.supportsTextureCompressionBPTC) {
		enabledFeatures.textureCompressionBC = VK_TRUE;
	}
	if (physicalDevice.supportedFeatures.supportsTextureCompressionETC2) {
		enabledFeatures.textureCompressionETC2 = VK_TRUE;
	}

	InplaceBuffer<const char*, REQUIRED_EXTENSIONS.size() + 4> enabledExtensions{};
	for (const char* const requiredExtension : REQUIRED_EXTENSIONS) {
		enabledExtensions.push_back(requiredExtension);
	}
	if (physicalDevice.supportedFeatures.supportsTextureCompressionPVRTC) {
		enabledExtensions.push_back(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);
	}
	if (physicalDevice.supportedFeatures.supportsASTCDecodeMode) {
		enabledExtensions.push_back(VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME);
	}
	if (physicalDevice.supportedFeatures.supportsAwaitPresentation) {
		enabledExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
		enabledExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
	}

	const VkDeviceCreateInfo deviceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = pNext,
		.flags = VkDeviceCreateFlags{},
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
		.ppEnabledExtensionNames = enabledExtensions.data(),
		.pEnabledFeatures = &enabledFeatures,
	};
	VkDevice deviceHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateDevice(physicalDevice.handle, &deviceCreateInfo, nullptr, &deviceHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateDevice", result};
	}
	detail::VulkanDevice result{deviceHandle, detail::VulkanDeviceDeleter{}};

	volkLoadDevice(deviceHandle);

	return result;
}

[[nodiscard]] VkQueue getQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex) {
	GREM_PROFILE_FUNCTION();

	VkQueue queueHandle = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queueHandle);
	return queueHandle;
}

[[nodiscard]] detail::VulkanAllocator createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
	GREM_PROFILE_FUNCTION();

	const VmaVulkanFunctions vulkanFunctions{
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
		.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
		.vkAllocateMemory = vkAllocateMemory,
		.vkFreeMemory = vkFreeMemory,
		.vkMapMemory = vkMapMemory,
		.vkUnmapMemory = vkUnmapMemory,
		.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
		.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
		.vkBindBufferMemory = vkBindBufferMemory,
		.vkBindImageMemory = vkBindImageMemory,
		.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
		.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
		.vkCreateBuffer = vkCreateBuffer,
		.vkDestroyBuffer = vkDestroyBuffer,
		.vkCreateImage = vkCreateImage,
		.vkDestroyImage = vkDestroyImage,
		.vkCmdCopyBuffer = vkCmdCopyBuffer,
		.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
		.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
		.vkBindBufferMemory2KHR = vkBindBufferMemory2,
		.vkBindImageMemory2KHR = vkBindImageMemory2,
		.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
		.vkGetDeviceBufferMemoryRequirements = nullptr,
		.vkGetDeviceImageMemoryRequirements = nullptr,
		.vkGetMemoryWin32HandleKHR = nullptr,
		.vkGetPhysicalDeviceProperties2KHR = vkGetPhysicalDeviceProperties2,
	};
	const VmaAllocatorCreateInfo allocatorCreateInfo{
		.flags = VmaAllocatorCreateFlags{},
		.physicalDevice = physicalDevice,
		.device = device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &vulkanFunctions,
		.instance = instance,
		.vulkanApiVersion = API_VERSION,
		.pTypeExternalMemoryHandleTypes = nullptr,
	};
	VmaAllocator allocatorHandle = VK_NULL_HANDLE;
	if (const VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &allocatorHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vmaCreateAllocator", result};
	}
	return detail::VulkanAllocator{allocatorHandle, detail::VulkanAllocatorDeleter{}};
}

struct ShaderCacheHeader {
	static constexpr Array<char, 6> FORMAT_IDENTIFIER{'\x67', '\0', '\x69', '\xDD', 'S', 'C'};
	static constexpr Pair<uint8_t> CURRENT_FORMAT_VERSION{uint8_t{0}, uint8_t{0}};
	static constexpr Array<uint8_t, 16> GENERIC_UUID{};

	Array<char, FORMAT_IDENTIFIER.size()> formatIdentifier;
	uint8_t formatBackwardsCompatibilityBreakingVersion;
	uint8_t formatExtensionVersion;
	Array<uint8_t, 16> physicalDeviceUUID;
	uint32_t shaderOffsetInU32sAfterHeader;
	uint32_t shaderCount;
	uint32_t pipelineCacheOffsetInU32sAfterHeader;
	uint8_t pipelineCacheConfiguration;
	uint8_t pipelineCacheCompressionMode;
	uint16_t reserved0 = 0;
	uint64_t pipelineCacheUncompressedSizeInBytes;
	uint64_t pipelineCacheCompressedSizeInBytes;
	uint64_t reserved1 = 0;
};

struct ShaderCacheShaderHeader {
	enum class Configuration : uint8_t {
		DEBUG = 0,
		RELEASE = 1,
	};

	uint32_t codeUncompressedSizeInU32s;
	uint32_t type;
	uint64_t sourceSizeInBytes;
	uint32_t sourceCRC32;
	uint8_t configuration;
	uint8_t compressionMode;
	uint16_t reserved;
	uint64_t codeCompressedSizeInBytes;
};

void loadShaderCache([[maybe_unused]] DeviceImplementation::ShaderCache& shaderCache, Allocation<byte>& pipelineCacheInitialData, [[maybe_unused]] bool willSaveShaderCache,
	[[maybe_unused]] VkDevice device, Span<const uint8_t, 16> physicalDeviceUUID, const Filesystem& filesystem, CStringView inputFilepath) {
	const auto printWarning = [&](StringView message) -> void {
		eprintln("Warning: Failed to read shader cache: {}", message);
	};

	InputFileHandle file{};
	try {
		file = filesystem.openInputFile(inputFilepath);
	} catch (const File::Error&) {
		return;
	}

	try {
		ShaderCacheHeader header{};
		try {
			file.read(asWritableBytes(Span{&header, 1}));
		} catch (const File::Error&) {
			printWarning("Invalid file size.");
			return;
		}
		if (memcmp(header.formatIdentifier.data(), ShaderCacheHeader::FORMAT_IDENTIFIER.data(), ShaderCacheHeader::FORMAT_IDENTIFIER.size()) != 0) {
			printWarning("Incorrect format identifier.");
			return;
		}
		if (convertLittleEndianToHostEndian(header.formatBackwardsCompatibilityBreakingVersion) > ShaderCacheHeader::CURRENT_FORMAT_VERSION.first) {
			printWarning("Incompatible format version.");
			return;
		}
		if (memcmp(header.physicalDeviceUUID.data(), ShaderCacheHeader::GENERIC_UUID.data(), ShaderCacheHeader::GENERIC_UUID.size()) != 0 &&
			memcmp(header.physicalDeviceUUID.data(), physicalDeviceUUID.data(), physicalDeviceUUID.size()) != 0) {
			printWarning("Wrong physical device UUID.");
			return;
		}

		const uint32_t shaderOffsetInU32sAfterHeader = convertLittleEndianToHostEndian(header.shaderOffsetInU32sAfterHeader);
		const uint32_t shaderCount = convertLittleEndianToHostEndian(header.shaderCount);
		const uint32_t pipelineCacheOffsetInU32sAfterHeader = convertLittleEndianToHostEndian(header.pipelineCacheOffsetInU32sAfterHeader);
		const uint8_t pipelineCacheConfiguration = convertLittleEndianToHostEndian(header.pipelineCacheConfiguration);
		const uint8_t pipelineCacheCompressionMode = convertLittleEndianToHostEndian(header.pipelineCacheCompressionMode);
		const uint64_t pipelineCacheUncompressedSizeInBytes = convertLittleEndianToHostEndian(header.pipelineCacheUncompressedSizeInBytes);
		const uint64_t pipelineCacheCompressedSizeInBytes = convertLittleEndianToHostEndian(header.pipelineCacheCompressedSizeInBytes);

		if (shaderCount > 0) {
			size_t fileOffset = sizeof(ShaderCacheHeader) + static_cast<size_t>(shaderOffsetInU32sAfterHeader) * sizeof(uint32_t);
			file.seekg(fileOffset);
			for (uint32_t shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex) {
				ShaderCacheShaderHeader shaderHeader{};
				try {
					file.read(asWritableBytes(Span{&shaderHeader, 1}));
				} catch (const File::Error&) {
					printWarning("Invalid shader offset.");
					return;
				}

				const uint64_t codeCompressedSizeInBytes = convertLittleEndianToHostEndian(shaderHeader.codeCompressedSizeInBytes);
				if (codeCompressedSizeInBytes > max(uint64_t{Limits<ptrdiff_t>::MAX}, uint64_t{Limits<size_t>::MAX})) {
					printWarning("Invalid shader size.");
					return;
				}

				fileOffset += sizeof(ShaderCacheHeader);
				const size_t codeBegin = fileOffset;
				fileOffset += codeCompressedSizeInBytes;
				const size_t codeEnd = fileOffset;
				fileOffset = roundUpToMultiple(fileOffset, alignof(uint32_t));
				const size_t alignmentBytes = fileOffset - codeEnd;

#ifdef GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION
				const uint32_t codeUncompressedSizeInU32s = convertLittleEndianToHostEndian(shaderHeader.codeUncompressedSizeInU32s);
				const uint32_t type = convertLittleEndianToHostEndian(shaderHeader.type);
				const uint64_t sourceSizeInBytes = convertLittleEndianToHostEndian(shaderHeader.sourceSizeInBytes);
				const uint32_t sourceCRC32 = convertLittleEndianToHostEndian(shaderHeader.sourceCRC32);
				const uint8_t configuration = convertLittleEndianToHostEndian(shaderHeader.configuration);
				const uint8_t compressionMode = convertLittleEndianToHostEndian(shaderHeader.compressionMode);

				bool isApplicable = true;
				switch (type) {
					case static_cast<uint32_t>(DeviceImplementation::ShaderType::SPIRV_VERTEX): [[fallthrough]];
					case static_cast<uint32_t>(DeviceImplementation::ShaderType::SPIRV_FRAGMENT): break;
					default: isApplicable = false; break;
				}
				switch (configuration) {
					case static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::DEBUG):
#ifdef NDEBUG
						isApplicable = false;
#endif
						break;
					case static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::RELEASE):
#ifndef NDEBUG
						isApplicable = false;
#endif
						break;
					default: isApplicable = false; break;
				}
				switch (compressionMode) {
					case static_cast<uint8_t>(DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED): [[fallthrough]];
					case static_cast<uint8_t>(DeviceImplementation::ShaderCompressionMode::ZSTANDARD): break;
					default: isApplicable = false; break;
				}
				if (!isApplicable) {
					eprintln("Warning: Found non-applicable shader in shader cache.");
					file.skipg(static_cast<ptrdiff_t>(fileOffset - codeBegin));
					continue;
				}

				Allocation<byte> compressedCode{};
				std::vector<uint32_t> code(static_cast<size_t>(codeUncompressedSizeInU32s));
				const size_t uncompressedCodeSizeInBytes = code.size() * sizeof(uint32_t);
				switch (static_cast<DeviceImplementation::ShaderCompressionMode>(compressionMode)) {
					case DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED: {
						if (static_cast<size_t>(codeCompressedSizeInBytes) != uncompressedCodeSizeInBytes) {
							printWarning("Invalid shader size.");
							return;
						}
						try {
							file.read(asWritableBytes(Span{code}));
						} catch (const File::Error&) {
							printWarning("Invalid file size.");
							return;
						}
						if (willSaveShaderCache && uncompressedCodeSizeInBytes > 0) {
							compressedCode.resize(ZSTD_compressBound(uncompressedCodeSizeInBytes));
							const size_t codeCompressedSizeInBytes =
								ZSTD_compress(compressedCode.data(), compressedCode.size(), code.data(), uncompressedCodeSizeInBytes, ZSTD_CLEVEL_DEFAULT);
							if (ZSTD_isError(codeCompressedSizeInBytes)) {
								printWarning(formatString("Failed to compress shader code: {}", ZSTD_getErrorName(codeCompressedSizeInBytes)));
								return;
							}
							compressedCode.resize(codeCompressedSizeInBytes);
						}
						break;
					}
					case DeviceImplementation::ShaderCompressionMode::ZSTANDARD: {
						compressedCode.resize(static_cast<size_t>(codeCompressedSizeInBytes));
						try {
							file.read(asWritableBytes(Span{compressedCode}));
						} catch (const File::Error&) {
							printWarning("Invalid file size.");
							return;
						}
						const size_t expectedDecompressedSizeInBytes = code.size() * sizeof(uint32_t);
						const size_t decompressedBytes = ZSTD_decompress(code.data(), expectedDecompressedSizeInBytes, compressedCode.data(), compressedCode.size());
						if (ZSTD_isError(decompressedBytes)) {
							printWarning(formatString("Failed to decompress shader code: {}", ZSTD_getErrorName(decompressedBytes)));
							return;
						}
						if (decompressedBytes != expectedDecompressedSizeInBytes) {
							printWarning("Invalid decompressed size.");
							return;
						}
						if (!willSaveShaderCache) {
							compressedCode.clear();
						}
						break;
					}
				}

				file.skipg(static_cast<ptrdiff_t>(alignmentBytes));

				const VkShaderModuleCreateInfo shaderModuleCreateInfo{
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.pNext = nullptr,
					.flags = VkShaderModuleCreateFlags{},
					.codeSize = uncompressedCodeSizeInBytes,
					.pCode = code.data(),
				};
				VkShaderModule shaderModuleHandle = VK_NULL_HANDLE;
				if (const VkResult result = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModuleHandle); result != VK_SUCCESS) {
					throw detail::VulkanError{"vkCreateShaderModule", result};
				}
				detail::VulkanShaderModule shaderModule{shaderModuleHandle, detail::VulkanShaderModuleDeleter{device}};

				const DeviceImplementation::ShaderKey shaderKey{
					.type = static_cast<DeviceImplementation::ShaderType>(type),
					.sourceSizeInBytes = sourceSizeInBytes,
					.sourceCRC32{sourceCRC32},
				};
				shaderCache.emplace(shaderKey,
					DeviceImplementation::Shader{
						.codeUncompressedSizeInU32s = codeUncompressedSizeInU32s,
						.compressionMode = DeviceImplementation::ShaderCompressionMode::ZSTANDARD,
						.compressedCode = std::move(compressedCode),
						.shaderModule = std::move(shaderModule),
					});
#else
				file.skipg(static_cast<ptrdiff_t>(fileOffset - codeBegin));
#endif
			}
		}

		switch (pipelineCacheConfiguration) {
			case static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::DEBUG):
#ifdef NDEBUG
				eprintln("Warning: Ignoring pipeline cache in shader cache that was created for a different configuration.");
				return;
#else
				break;
#endif
			case static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::RELEASE):
#ifndef NDEBUG
				eprintln("Warning: Ignoring pipeline cache in shader cache that was created for a different configuration.");
				return;
#else
				break;
#endif
			default: return;
		}

		if (pipelineCacheUncompressedSizeInBytes > 0) {
			if (pipelineCacheUncompressedSizeInBytes > Limits<size_t>::MAX || pipelineCacheCompressedSizeInBytes > Limits<size_t>::MAX) {
				printWarning("Invalid pipeline cache size.");
				return;
			}

			file.seekg(sizeof(ShaderCacheHeader) + static_cast<size_t>(pipelineCacheOffsetInU32sAfterHeader) * sizeof(uint32_t));

			pipelineCacheInitialData.resize(static_cast<size_t>(pipelineCacheCompressedSizeInBytes));
			try {
				file.read(asWritableBytes(Span{pipelineCacheInitialData}));
			} catch (const File::Error&) {
				printWarning("Invalid file size.");
				return;
			}

			switch (pipelineCacheCompressionMode) {
				case static_cast<uint8_t>(DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED): break;
				case static_cast<uint8_t>(DeviceImplementation::ShaderCompressionMode::ZSTANDARD): {
					Allocation<byte> uncompressedPipelineCache(static_cast<size_t>(pipelineCacheUncompressedSizeInBytes));
					const size_t decompressedBytes =
						ZSTD_decompress(uncompressedPipelineCache.data(), uncompressedPipelineCache.size(), pipelineCacheInitialData.data(), pipelineCacheInitialData.size());
					if (ZSTD_isError(decompressedBytes)) {
						printWarning(formatString("Failed to decompress pipeline cache: {}", ZSTD_getErrorName(decompressedBytes)));
						return;
					}
					pipelineCacheInitialData = std::move(uncompressedPipelineCache);
					break;
				}
				default: printWarning("Invalid pipeline cache compression mode."); return;
			}
		}
	} catch (const File::Error& e) {
		printWarning(e.what());
	}
}

void saveShaderCache(Filesystem& filesystem, CStringView outputFilepath, VkDevice device, VkPipelineCache pipelineCache, const DeviceImplementation::ShaderCache& shaderCache,
	Span<const uint8_t, 16> physicalDeviceUUID) {
	GREM_PROFILE_FUNCTION();

	Allocation<byte> pipelineCacheData{};
	size_t pipelineCacheDataUncompressedSize = 0;
	DeviceImplementation::ShaderCompressionMode pipelineCacheCompressionMode = DeviceImplementation::ShaderCompressionMode::UNCOMPRESSED;
	if (vkGetPipelineCacheData(device, pipelineCache, &pipelineCacheDataUncompressedSize, nullptr) == VK_SUCCESS && pipelineCacheDataUncompressedSize > 0) {
		pipelineCacheData.resize(pipelineCacheDataUncompressedSize);
		if (vkGetPipelineCacheData(device, pipelineCache, &pipelineCacheDataUncompressedSize, pipelineCacheData.data()) != VK_SUCCESS) {
			pipelineCacheData.clear();
		}

		Allocation<byte> compressedPipelineCacheData(ZSTD_compressBound(pipelineCacheData.size()));
		const size_t pipelineCacheCompressedSizeInBytes =
			ZSTD_compress(compressedPipelineCacheData.data(), compressedPipelineCacheData.size(), pipelineCacheData.data(), pipelineCacheData.size(), ZSTD_CLEVEL_DEFAULT);
		if (ZSTD_isError(pipelineCacheCompressedSizeInBytes)) {
			pipelineCacheData.clear();
			pipelineCacheDataUncompressedSize = 0;
		} else {
			compressedPipelineCacheData.resize(pipelineCacheCompressedSizeInBytes);
			pipelineCacheData = std::move(compressedPipelineCacheData);
			pipelineCacheCompressionMode = DeviceImplementation::ShaderCompressionMode::ZSTANDARD;
		}
	}

	filesystem.createParentOutputDirectories(outputFilepath);
	OutputFileHandle file = filesystem.openEmptyOutputFile(outputFilepath);

	const size_t shaderOffsetInBytesAfterHeader = 0;
	size_t pipelineCacheOffsetInBytesAfterHeader = 0;
	for (const auto& [key, shader] : shaderCache) {
		pipelineCacheOffsetInBytesAfterHeader += sizeof(ShaderCacheShaderHeader) + shader.compressedCode.size();
		pipelineCacheOffsetInBytesAfterHeader = roundUpToMultiple(pipelineCacheOffsetInBytesAfterHeader, alignof(uint32_t));
	}
	static_assert(sizeof(ShaderCacheHeader) % sizeof(uint32_t) == 0);
	GREM_ASSERT(pipelineCacheOffsetInBytesAfterHeader % sizeof(uint32_t) == 0);

	ShaderCacheHeader header{
		.formatIdentifier = ShaderCacheHeader::FORMAT_IDENTIFIER,
		.formatBackwardsCompatibilityBreakingVersion = convertHostEndianToLittleEndian(ShaderCacheHeader::CURRENT_FORMAT_VERSION.first),
		.formatExtensionVersion = convertHostEndianToLittleEndian(ShaderCacheHeader::CURRENT_FORMAT_VERSION.second),
		.physicalDeviceUUID{},
		.shaderOffsetInU32sAfterHeader = convertHostEndianToLittleEndian(static_cast<uint32_t>(shaderOffsetInBytesAfterHeader / sizeof(uint32_t))),
		.shaderCount = convertHostEndianToLittleEndian(static_cast<uint32_t>(shaderCache.size())),
		.pipelineCacheOffsetInU32sAfterHeader = convertHostEndianToLittleEndian(static_cast<uint32_t>(pipelineCacheOffsetInBytesAfterHeader / sizeof(uint32_t))),
#ifdef NDEBUG
		.pipelineCacheConfiguration = convertHostEndianToLittleEndian(static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::RELEASE)),
#else
		.pipelineCacheConfiguration = convertHostEndianToLittleEndian(static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::DEBUG)),
#endif
		.pipelineCacheCompressionMode = convertHostEndianToLittleEndian(static_cast<uint8_t>(pipelineCacheCompressionMode)),
		.pipelineCacheUncompressedSizeInBytes = convertHostEndianToLittleEndian(static_cast<uint64_t>(pipelineCacheDataUncompressedSize)),
		.pipelineCacheCompressedSizeInBytes = convertHostEndianToLittleEndian(static_cast<uint64_t>(pipelineCacheData.size())),
	};
	memcpy(header.physicalDeviceUUID.data(), physicalDeviceUUID.data(), physicalDeviceUUID.size());
	file.write(asBytes(Span{&header, 1}));

	const Array<byte, alignof(uint32_t) - 1> padding{};

	GREM_ASSERT(file.tellp() == sizeof(ShaderCacheHeader) + shaderOffsetInBytesAfterHeader);
	size_t fileOffset = sizeof(ShaderCacheHeader) + shaderOffsetInBytesAfterHeader;
	for (const auto& [key, shader] : shaderCache) {
		const ShaderCacheShaderHeader shaderHeader{
			.codeUncompressedSizeInU32s = convertHostEndianToLittleEndian(shader.codeUncompressedSizeInU32s),
			.type = convertHostEndianToLittleEndian(static_cast<uint32_t>(key.type)),
			.sourceSizeInBytes = convertHostEndianToLittleEndian(static_cast<uint64_t>(key.sourceSizeInBytes)),
			.sourceCRC32 = convertHostEndianToLittleEndian(static_cast<uint32_t>(key.sourceCRC32)),
#ifdef NDEBUG
			.configuration = convertHostEndianToLittleEndian(static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::RELEASE)),
#else
			.configuration = convertHostEndianToLittleEndian(static_cast<uint8_t>(ShaderCacheShaderHeader::Configuration::DEBUG)),
#endif
			.compressionMode = convertHostEndianToLittleEndian(static_cast<uint8_t>(shader.compressionMode)),
			.reserved = 0,
			.codeCompressedSizeInBytes = convertHostEndianToLittleEndian(static_cast<uint64_t>(shader.compressedCode.size())),
		};
		file.write(asBytes(Span{&shaderHeader, 1}));
		file.write(asBytes(Span{shader.compressedCode}));
		fileOffset += sizeof(ShaderCacheShaderHeader) + shader.compressedCode.size();
		const size_t codeEnd = fileOffset;
		fileOffset = roundUpToMultiple(fileOffset, alignof(uint32_t));
		const size_t alignmentBytes = fileOffset - codeEnd;
		file.write(Span{padding}.first(alignmentBytes));
	}

	GREM_ASSERT(fileOffset - sizeof(ShaderCacheHeader) == pipelineCacheOffsetInBytesAfterHeader);
	GREM_ASSERT(file.tellp() == fileOffset);
	file.write(asBytes(Span{pipelineCacheData}));
	fileOffset += pipelineCacheData.size();
	const size_t pipelineCacheEnd = fileOffset;
	fileOffset = roundUpToMultiple(fileOffset, alignof(uint32_t));
	const size_t alignmentBytes = fileOffset - pipelineCacheEnd;
	file.write(Span{padding}.first(alignmentBytes));
}

[[nodiscard]] DeviceImplementation::ShaderCache createShaderCache(detail::VulkanPipelineCache& pipelineCache, bool willSaveShaderCache, VkDevice device,
	Span<const uint8_t, 16> physicalDeviceUUID, const Filesystem* filesystem, CStringView inputFilepath) {
	GREM_PROFILE_FUNCTION();

	DeviceImplementation::ShaderCache result{};
	Allocation<byte> pipelineCacheInitialData{};
	if (filesystem && !inputFilepath.empty()) {
		loadShaderCache(result, pipelineCacheInitialData, willSaveShaderCache, device, physicalDeviceUUID, *filesystem, inputFilepath);
	}
	const VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineCacheCreateFlags{},
		.initialDataSize = pipelineCacheInitialData.size(),
		.pInitialData = pipelineCacheInitialData.data(),
	};
	VkPipelineCache pipelineCacheHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCacheHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreatePipelineCache", result};
	}
	pipelineCache = detail::VulkanPipelineCache{pipelineCacheHandle, detail::VulkanPipelineCacheDeleter{device}};
	return result;
}

[[nodiscard]] detail::VulkanDescriptorSetLayout createInstanceOrDrawCommandBufferDescriptorSetLayout(VkDevice device) {
	GREM_PROFILE_FUNCTION();

	const Array bindings{VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	}};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkDescriptorSetLayoutCreateFlags{},
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.data()),
	};
	VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateDescriptorSetLayout", result};
	}
	return detail::VulkanDescriptorSetLayout{descriptorSetLayoutHandle, detail::VulkanDescriptorSetLayoutDeleter{device}};
}

[[nodiscard]] RangeAllocation<uint32_t> acquireElementRange(RangeAllocator<uint32_t>& rangeAllocator, HashMap<uint32_t, size_t>& rangeReferenceCounts, uint32_t elementCount) {
	GREM_PROFILE_FUNCTION();

	if (elementCount == 0) {
		return {};
	}
	const Optional<RangeAllocation<uint32_t>> allocation = rangeAllocator.allocateRange(elementCount);
	if (!allocation) {
		throw std::bad_alloc{};
	}
	[[maybe_unused]] const auto [it, inserted] = rangeReferenceCounts.emplace(allocation->begin, size_t{1});
	GREM_ASSERT(inserted);
	return *allocation;
}

void reacquireElementRange(HashMap<uint32_t, size_t>& rangeReferenceCounts, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	const auto it = rangeReferenceCounts.find(allocation.begin);
	GREM_ASSERT(it != rangeReferenceCounts.end());
	++it->second;
}

bool releaseElementRange(RangeAllocator<uint32_t>& rangeAllocator, HashMap<uint32_t, size_t>& rangeReferenceCounts, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin == allocation.end) {
		return false;
	}
	const auto it = rangeReferenceCounts.find(allocation.begin);
	GREM_ASSERT(it != rangeReferenceCounts.end());
	if (it->second-- == 1) {
		rangeAllocator.deallocateRange(allocation);
		rangeReferenceCounts.erase(it);
		return true;
	}
	return false;
}

void popGraphicsQueueSubmission(DeviceImplementation& device) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(device.graphicsQueueSubmissions.size() >= 2);
	DeviceImplementation::GraphicsQueueSubmission& submission = device.graphicsQueueSubmissions.front();
	GREM_ASSERT(device.totalRenderPassesInFlight >= submission.usedRenderPasses.size());
	device.totalRenderPassesInFlight -= submission.usedRenderPasses.size();
	submission.usedRenderPasses.clear();
	for (detail::StagingBuffer& stagingBuffer : submission.ownedStagingBuffers) {
		device.totalStagingBufferBytesInFlight -= stagingBuffer.size();
		device.stagingBuffersForReuse.push_back(std::move(stagingBuffer));
	}
	device.totalStagingBuffersInFlight -= submission.ownedStagingBuffers.size();
	submission.ownedStagingBuffers.clear();
	submission.ownedTargetedTextureResources.clear();
	submission.ownedTargetedFramebufferContexts.clear();
	device.graphicsQueueSubmissions.pop_front();
}

void pushRenderPassCommands(DeviceImplementation& device, VkCommandBuffer commandBuffer, const RenderPassImplementation& renderPass) {
	GREM_PROFILE_FUNCTION();

	VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;
	renderPass.commands->visit(Overloaded{
		[&](const RenderPassImplementation::CommandSetViewport& command) -> void { //
			const Array viewports{command.viewport};
			vkCmdSetViewport(commandBuffer, 0, static_cast<uint32_t>(viewports.size()), viewports.data());
		},
		[&](const RenderPassImplementation::CommandSetScissor& command) -> void { //
			const Array scissors{command.scissor};
			vkCmdSetScissor(commandBuffer, 0, static_cast<uint32_t>(scissors.size()), scissors.data());
		},
		[&](const RenderPassImplementation::CommandFill& command) -> void { //
			InplaceBuffer<VkClearAttachment, 2> clearAttachments{};
			if (command.values.aspects.contains(TextureAspect::COLOR)) {
				const vec4 clearColor = command.values.color.toLinearRGBA();
				clearAttachments.push_back(VkClearAttachment{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.colorAttachment = 0,
					.clearValue{.color{.float32{clearColor.x, clearColor.y, clearColor.z, clearColor.w}}},
				});
			}
			if (command.values.aspects.containsAnyOf(TextureAspects::DEPTH_STENCIL)) {
				clearAttachments.push_back(VkClearAttachment{
					.aspectMask = TextureImplementation::translateTextureAspects(command.values.aspects) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
					.colorAttachment = 0,
					.clearValue{.depthStencil{.depth = command.values.depth, .stencil = command.values.stencil}},
				});
			}
			const Array rects{VkClearRect{
				.rect = command.targetRegion,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}};
			vkCmdClearAttachments(commandBuffer, static_cast<uint32_t>(clearAttachments.size()), clearAttachments.data(), static_cast<uint32_t>(rects.size()), rects.data());
		},
		[&](const RenderPassImplementation::CommandUsePipeline& command) -> void { //
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command.pipeline);
			currentPipelineLayout = command.pipelineLayout;
		},
		[&](const RenderPassImplementation::CommandUseMesh& command) -> void { //
			const DeviceImplementation::MeshContext& meshContext = device.getMeshContext(command.meshTypeIndex);
			const DeviceImplementation::MeshContext::VertexBuffers& vertexBuffers = meshContext.vertexBufferMap.at(command.activeVertexAttributes);
			vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(vertexBuffers.vertexBufferHandles.size()), vertexBuffers.vertexBufferHandles.data(),
				vertexBuffers.vertexBufferOffsets.data());
			if (command.indexType) {
				vkCmdBindIndexBuffer(commandBuffer, meshContext.indexBuffer.get(), 0, RenderPassImplementation::translateMeshIndexType(*command.indexType));
			}
		},
		[&](Span<const VkDescriptorSet> descriptorSets) -> void { //
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
				0, nullptr);
		},
		[&](const RenderPassImplementation::CommandDraw& command) -> void { //
			vkCmdDraw(commandBuffer, command.vertexCount, command.instanceCount, command.firstVertex, command.firstInstance);
		},
		[&](const RenderPassImplementation::CommandDrawIndexed& command) -> void { //
			vkCmdDrawIndexed(commandBuffer, command.indexCount, command.instanceCount, command.firstIndex, command.vertexOffset, command.firstInstance);
		},
		[&](const RenderPassImplementation::CommandDrawIndirect& command) -> void { //
			vkCmdDrawIndexedIndirect(commandBuffer, command.buffer, 0, command.drawCount, command.stride);
		},
		[&](const RenderPassImplementation::CommandDrawIndexedIndirect& command) -> void { //
			vkCmdDrawIndexedIndirect(commandBuffer, command.buffer, 0, command.drawCount, command.stride);
		},
	});
}

void renderRenderPass(DeviceImplementation& device, SharedPointer<RenderPassImplementation> renderPassHandle) {
	GREM_PROFILE_FUNCTION();

	RenderPassImplementation& renderPass = *renderPassHandle;
	DeviceImplementation::RenderPassContext& renderPassContext = device.getRenderPassContext(renderPass.renderTargets.getRenderPassContextKey());
	auto&& [key, framebufferContext] = device.getFramebufferContext(renderPassContext, renderPass.renderTargets.acquireFramebufferContextKey());
	framebufferContext.latestGraphicsQueueSubmissionUsingThisResource = device.nextGraphicsQueueSubmissionGenerationIndex;

	VkPipelineStageFlags preRenderPassImageMemoryStageMask = VK_PIPELINE_STAGE_NONE;
	InplaceBuffer<VkImageMemoryBarrier, 3> preRenderPassImageMemoryBarriers{};
	InplaceBuffer<VkClearValue, 2> clearValues{};

	if (const SharedPointer<TextureImplementation> colorTargetHandle = key.colorTargetHandle.lock()) {
		if (colorTargetHandle->imageLayout != renderPassContext.initialColorLayout) {
			preRenderPassImageMemoryStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			preRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = colorTargetHandle->imageLayout,
				.newLayout = renderPassContext.initialColorLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = colorTargetHandle->object.get<detail::TextureResources>().image,
				.subresourceRange{
					.aspectMask = TextureImplementation::getAspectMask(colorTargetHandle->format),
					.baseMipLevel = key.colorTargetMipLevel,
					.levelCount = 1,
					.baseArrayLayer = key.colorTargetLayer,
					.layerCount = 1,
				},
			});
		}

		if (const ClearValues* const values = renderPass.renderTargets.clearMode.get_if<ClearValues>()) {
			const vec4 clearColor = values->color.toLinearRGBA();
			clearValues.push_back(VkClearValue{.color{.float32{clearColor.x, clearColor.y, clearColor.z, clearColor.w}}});
		} else {
			clearValues.push_back(VkClearValue{});
		}

		colorTargetHandle->latestGraphicsQueueSubmissionUsingThisResource = device.nextGraphicsQueueSubmissionGenerationIndex;
	}

	if (const SharedPointer<TextureImplementation> depthStencilTargetHandle = key.depthStencilTargetHandle.lock()) {
		if (depthStencilTargetHandle->imageLayout != renderPassContext.initialDepthStencilLayout) {
			preRenderPassImageMemoryStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			preRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = depthStencilTargetHandle->imageLayout,
				.newLayout = renderPassContext.initialDepthStencilLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = depthStencilTargetHandle->object.get<detail::TextureResources>().image,
				.subresourceRange{
					.aspectMask = TextureImplementation::getAspectMask(depthStencilTargetHandle->format),
					.baseMipLevel = key.depthStencilTargetMipLevel,
					.levelCount = 1,
					.baseArrayLayer = key.depthStencilTargetLayer,
					.layerCount = 1,
				},
			});
		}

		if (const ClearValues* const values = renderPass.renderTargets.clearMode.get_if<ClearValues>()) {
			clearValues.push_back(VkClearValue{.depthStencil{.depth = values->depth, .stencil = static_cast<uint32_t>(values->stencil)}});
		} else {
			clearValues.push_back(VkClearValue{});
		}

		depthStencilTargetHandle->latestGraphicsQueueSubmissionUsingThisResource = device.nextGraphicsQueueSubmissionGenerationIndex;
	}

	if (const SharedPointer<TextureImplementation> resolveTargetHandle = key.resolveTargetHandle.lock()) {
		if (resolveTargetHandle->imageLayout != renderPassContext.initialResolveLayout) {
			preRenderPassImageMemoryStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			preRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = resolveTargetHandle->imageLayout,
				.newLayout = renderPassContext.initialResolveLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resolveTargetHandle->object.get<detail::TextureResources>().image,
				.subresourceRange{
					.aspectMask = TextureImplementation::getAspectMask(resolveTargetHandle->format),
					.baseMipLevel = key.resolveTargetMipLevel,
					.levelCount = 1,
					.baseArrayLayer = key.resolveTargetLayer,
					.layerCount = 1,
				},
			});
		}

		resolveTargetHandle->latestGraphicsQueueSubmissionUsingThisResource = device.nextGraphicsQueueSubmissionGenerationIndex;
	}

	if (!preRenderPassImageMemoryBarriers.empty()) {
		vkCmdPipelineBarrier(device.getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, preRenderPassImageMemoryStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preRenderPassImageMemoryBarriers.size()), preRenderPassImageMemoryBarriers.data());
	}

	const VkCommandBuffer commandBuffer = device.getGraphicsCommandBuffer();
	const VkRenderPassBeginInfo renderPassBeginInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = renderPassContext.renderPass.get(),
		.framebuffer = framebufferContext.framebuffer.get(),
		.renderArea{
			.offset{.x = 0, .y = 0},
			.extent = TextureImplementation::translateExtent(framebufferContext.size),
		},
		.clearValueCount = (renderPass.renderTargets.clearMode.is<ClearValues>()) ? static_cast<uint32_t>(clearValues.size()) : 0,
		.pClearValues = (renderPass.renderTargets.clearMode.is<ClearValues>()) ? clearValues.data() : nullptr,
	};
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	pushRenderPassCommands(device, commandBuffer, *renderPassHandle);
	vkCmdEndRenderPass(commandBuffer);

	InplaceBuffer<VkImageMemoryBarrier, 3> postRenderPassImageMemoryBarriers{};

	const auto transitionAllRemainingLayers =
		[&postRenderPassImageMemoryBarriers](TextureImplementation& texture, VkImageLayout newLayout, uint32_t renderedLayer, uint32_t renderedMipLevel) -> void {
		if (texture.imageLayout != newLayout) {
			const VkImage image = texture.object.get<detail::TextureResources>().image;
			const VkImageAspectFlags aspectMask = TextureImplementation::getAspectMask(texture.format);
			const uint32_t layerCount = texture.size.depth;
			const uint32_t mipLevelCount = texture.mipLevelCount;
			GREM_ASSERT(layerCount > 0);
			GREM_ASSERT(mipLevelCount > 0);
			if (renderedLayer > 0) {
				postRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.oldLayout = texture.imageLayout,
					.newLayout = newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = 0,
						.levelCount = mipLevelCount,
						.baseArrayLayer = 0,
						.layerCount = renderedLayer,
					},
				});
			}
			if (renderedMipLevel > 0) {
				postRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.oldLayout = texture.imageLayout,
					.newLayout = newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = 0,
						.levelCount = renderedMipLevel,
						.baseArrayLayer = renderedLayer,
						.layerCount = 1,
					},
				});
			}
			if (renderedMipLevel + 1 < mipLevelCount) {
				postRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.oldLayout = texture.imageLayout,
					.newLayout = newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = renderedMipLevel + 1,
						.levelCount = mipLevelCount - (renderedMipLevel + 1),
						.baseArrayLayer = renderedLayer,
						.layerCount = 1,
					},
				});
			}
			if (renderedLayer + 1 < layerCount) {
				postRenderPassImageMemoryBarriers.push_back(VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
					.oldLayout = texture.imageLayout,
					.newLayout = newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = 0,
						.levelCount = mipLevelCount,
						.baseArrayLayer = renderedLayer + 1,
						.layerCount = layerCount - (renderedLayer + 1),
					},
				});
			}
			texture.imageLayout = newLayout;
		}
	};

	if (const SharedPointer<TextureImplementation> colorTargetHandle = key.colorTargetHandle.lock()) {
		transitionAllRemainingLayers(*colorTargetHandle, renderPassContext.finalColorLayout, key.colorTargetLayer, key.colorTargetMipLevel);
	}

	if (const SharedPointer<TextureImplementation> depthStencilTargetHandle = key.depthStencilTargetHandle.lock()) {
		transitionAllRemainingLayers(*depthStencilTargetHandle, renderPassContext.finalDepthStencilLayout, key.depthStencilTargetLayer, key.depthStencilTargetMipLevel);
	}

	if (const SharedPointer<TextureImplementation> resolveTargetHandle = key.resolveTargetHandle.lock()) {
		transitionAllRemainingLayers(*resolveTargetHandle, renderPassContext.finalResolveLayout, key.resolveTargetLayer, key.resolveTargetMipLevel);
	}

	if (!postRenderPassImageMemoryBarriers.empty()) {
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postRenderPassImageMemoryBarriers.size()), postRenderPassImageMemoryBarriers.data());
	}

	if (device.totalRenderPassesInFlight >= MAX_SIMULTANEOUS_RENDER_PASSES_IN_FLIGHT) {
		device.submitAndAwaitGraphicsCommands();
	}
	device.graphicsQueueSubmissions.back().usedRenderPasses.push_back(std::move(renderPassHandle));
	++device.totalRenderPassesInFlight;
}

} // namespace

DeviceImplementation::RenderPassContext::FramebufferContext::FramebufferContext(DeviceImplementation& device, const FramebufferContextKey& key,
	const RenderPassContext& renderPassContext) {
	GREM_PROFILE_FUNCTION();

	const VkDevice deviceHandle = device.logicalDevice.get();

	InplaceBuffer<VkImageView, 3> attachments{};
	if (const SharedPointer<TextureImplementation> colorTargetHandle = key.colorTargetHandle.lock()) {
		const VkImageViewCreateInfo imageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkImageViewCreateFlags{},
			.image = colorTargetHandle->object.get<detail::TextureResources>().image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = colorTargetHandle->format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange{
				.aspectMask = TextureImplementation::getAspectMask(colorTargetHandle->format),
				.baseMipLevel = key.colorTargetMipLevel,
				.levelCount = 1,
				.baseArrayLayer = key.colorTargetLayer,
				.layerCount = 1,
			},
		};
		VkImageView imageViewHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateImageView(deviceHandle, &imageViewCreateInfo, nullptr, &imageViewHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateImageView", result};
		}
		colorTargetImageView = detail::VulkanImageView{imageViewHandle, detail::VulkanImageViewDeleter{deviceHandle}};
		attachments.push_back(imageViewHandle);

		const Extent2D colorTargetSize{
			.width = max(colorTargetHandle->size.width >> key.colorTargetMipLevel, uint32_t{1}),
			.height = max(colorTargetHandle->size.height >> key.colorTargetMipLevel, uint32_t{1}),
		};
		GREM_ASSERT(size == Extent2D{} || colorTargetSize == size);
		size = colorTargetSize;
	}

	if (const SharedPointer<TextureImplementation> depthStencilTargetHandle = key.depthStencilTargetHandle.lock()) {
		const VkImageViewCreateInfo imageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkImageViewCreateFlags{},
			.image = depthStencilTargetHandle->object.get<detail::TextureResources>().image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depthStencilTargetHandle->format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange{
				.aspectMask = TextureImplementation::getAspectMask(depthStencilTargetHandle->format),
				.baseMipLevel = key.depthStencilTargetMipLevel,
				.levelCount = 1,
				.baseArrayLayer = key.depthStencilTargetLayer,
				.layerCount = 1,
			},
		};
		VkImageView imageViewHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateImageView(deviceHandle, &imageViewCreateInfo, nullptr, &imageViewHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateImageView", result};
		}
		depthStencilTargetImageView = detail::VulkanImageView{imageViewHandle, detail::VulkanImageViewDeleter{deviceHandle}};
		attachments.push_back(imageViewHandle);

		const Extent2D depthStencilTargetSize{
			.width = max(depthStencilTargetHandle->size.width >> key.depthStencilTargetMipLevel, uint32_t{1}),
			.height = max(depthStencilTargetHandle->size.height >> key.depthStencilTargetMipLevel, uint32_t{1}),
		};
		GREM_ASSERT(size == Extent2D{} || depthStencilTargetSize == size);
		size = depthStencilTargetSize;
	}

	if (const SharedPointer<TextureImplementation> resolveTargetHandle = key.resolveTargetHandle.lock()) {
		const VkImageViewCreateInfo imageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkImageViewCreateFlags{},
			.image = resolveTargetHandle->object.get<detail::TextureResources>().image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = resolveTargetHandle->format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange{
				.aspectMask = TextureImplementation::getAspectMask(resolveTargetHandle->format),
				.baseMipLevel = key.resolveTargetMipLevel,
				.levelCount = 1,
				.baseArrayLayer = key.resolveTargetLayer,
				.layerCount = 1,
			},
		};
		VkImageView imageViewHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateImageView(deviceHandle, &imageViewCreateInfo, nullptr, &imageViewHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateImageView", result};
		}
		resolveTargetImageView = detail::VulkanImageView{imageViewHandle, detail::VulkanImageViewDeleter{deviceHandle}};
		attachments.push_back(imageViewHandle);

		const Extent2D resolveTargetSize{
			.width = max(resolveTargetHandle->size.width >> key.resolveTargetMipLevel, uint32_t{1}),
			.height = max(resolveTargetHandle->size.height >> key.resolveTargetMipLevel, uint32_t{1}),
		};
		GREM_ASSERT(size == Extent2D{} || resolveTargetSize == size);
		size = resolveTargetSize;
	}

	GREM_ASSERT(size != Extent2D{});

	const VkFramebufferCreateInfo framebufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkFramebufferCreateFlags{},
		.renderPass = renderPassContext.renderPass.get(),
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.width = size.width,
		.height = size.height,
		.layers = 1,
	};
	VkFramebuffer framebufferHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateFramebuffer(deviceHandle, &framebufferCreateInfo, nullptr, &framebufferHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateFramebuffer", result};
	}
	framebuffer = detail::VulkanFramebuffer{framebufferHandle, detail::VulkanFramebufferDeleter{deviceHandle}};
}

DeviceImplementation::RenderPassContext::RenderPassContext(DeviceImplementation& device, const RenderPassContextKey& key) {
	GREM_PROFILE_FUNCTION();

	InplaceBuffer<VkAttachmentDescription, 3> attachments{};
	InplaceBuffer<VkAttachmentReference, 1> colorAttachments{};
	InplaceBuffer<VkAttachmentReference, 1> resolveAttachments{};
	Optional<VkAttachmentReference> depthStencilAttachment{};
	VkSubpassDependency externalToSubpass0Dependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_NONE,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.dependencyFlags = VkDependencyFlags{},
	};
	VkSubpassDependency subpass0ToExternalDependency{
		.srcSubpass = 0,
		.dstSubpass = VK_SUBPASS_EXTERNAL,
		.srcStageMask = VK_PIPELINE_STAGE_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_NONE,
		.srcAccessMask = VK_ACCESS_NONE,
		.dstAccessMask = VK_ACCESS_NONE,
		.dependencyFlags = VkDependencyFlags{},
	};

	if (key.colorFormat != VK_FORMAT_UNDEFINED) {
		initialColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		finalColorLayout = ((key.flags & RENDER_PASS_RENDER_TARGET_IS_SWAPCHAIN) != 0) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		                   : ((key.flags & RENDER_PASS_COLOR_TARGET_IS_SAMPLED) != 0)
		                       ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		                       : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments.push_back(VkAttachmentReference{
			.attachment = static_cast<uint32_t>(attachments.size()),
			.layout = initialColorLayout,
		});
		attachments.push_back(VkAttachmentDescription{
			.flags = VkAttachmentDescriptionFlags{},
			.format = key.colorFormat,
			.samples = static_cast<VkSampleCountFlagBits>(key.sampleCount),
			.loadOp = ((key.flags & RENDER_PASS_CLEAR_COLOR) == 0)              ? VK_ATTACHMENT_LOAD_OP_LOAD
		              : ((key.flags & RENDER_PASS_CLEAR_UNDEFINED_VALUES) != 0) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
		                                                                        : VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = ((key.flags & (RENDER_PASS_HAS_RESOLVE_TARGET | RENDER_PASS_STORE_INTERMEDIATE_COLOR)) != RENDER_PASS_HAS_RESOLVE_TARGET)
		                   ? VK_ATTACHMENT_STORE_OP_STORE
		                   : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = initialColorLayout,
			.finalLayout = finalColorLayout,
		});
		externalToSubpass0Dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		externalToSubpass0Dependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		externalToSubpass0Dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		externalToSubpass0Dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpass0ToExternalDependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass0ToExternalDependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpass0ToExternalDependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass0ToExternalDependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		if ((key.flags & RENDER_PASS_CLEAR_COLOR) == 0) {
			externalToSubpass0Dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			subpass0ToExternalDependency.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		}
	}

	if (key.depthStencilFormat != VK_FORMAT_UNDEFINED) {
		const VkImageAspectFlags aspectMask = TextureImplementation::getAspectMask(key.depthStencilFormat);
		const bool hasDepth = (aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
		const bool hasStencil = (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;
		initialDepthStencilLayout = (hasDepth && hasStencil) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		                            : (hasStencil)           ? VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
		                                                     : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		finalDepthStencilLayout = ((key.flags & RENDER_PASS_DEPTH_STENCIL_TARGET_IS_SAMPLED) != 0) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : initialDepthStencilLayout;
		depthStencilAttachment = VkAttachmentReference{
			.attachment = static_cast<uint32_t>(attachments.size()),
			.layout = initialDepthStencilLayout,
		};
		attachments.push_back(VkAttachmentDescription{
			.flags = VkAttachmentDescriptionFlags{},
			.format = key.depthStencilFormat,
			.samples = static_cast<VkSampleCountFlagBits>(key.sampleCount),
			.loadOp = (hasDepth) ? (((key.flags & RENDER_PASS_CLEAR_DEPTH) == 0)                 ? VK_ATTACHMENT_LOAD_OP_LOAD
									   : ((key.flags & RENDER_PASS_CLEAR_UNDEFINED_VALUES) != 0) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
																								 : VK_ATTACHMENT_LOAD_OP_CLEAR)
		                         : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = (hasDepth && ((key.flags & (RENDER_PASS_HAS_RESOLVE_TARGET | RENDER_PASS_STORE_INTERMEDIATE_DEPTH)) != RENDER_PASS_HAS_RESOLVE_TARGET))
		                   ? VK_ATTACHMENT_STORE_OP_STORE
		                   : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = (hasStencil) ? (((key.flags & RENDER_PASS_CLEAR_STENCIL) == 0)               ? VK_ATTACHMENT_LOAD_OP_LOAD
												: ((key.flags & RENDER_PASS_CLEAR_UNDEFINED_VALUES) != 0) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
																										  : VK_ATTACHMENT_LOAD_OP_CLEAR)
		                                  : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = (hasStencil && ((key.flags & (RENDER_PASS_HAS_RESOLVE_TARGET | RENDER_PASS_STORE_INTERMEDIATE_STENCIL)) != RENDER_PASS_HAS_RESOLVE_TARGET))
		                          ? VK_ATTACHMENT_STORE_OP_STORE
		                          : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = initialDepthStencilLayout,
			.finalLayout = finalDepthStencilLayout,
		});
		externalToSubpass0Dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		externalToSubpass0Dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		externalToSubpass0Dependency.dstStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		externalToSubpass0Dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		subpass0ToExternalDependency.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		subpass0ToExternalDependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		subpass0ToExternalDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		subpass0ToExternalDependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		if ((hasDepth && (key.flags & RENDER_PASS_CLEAR_DEPTH) == 0) || (hasStencil && (key.flags & RENDER_PASS_CLEAR_STENCIL) == 0)) {
			externalToSubpass0Dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			externalToSubpass0Dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			subpass0ToExternalDependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			subpass0ToExternalDependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
	}

	if ((key.flags & RENDER_PASS_HAS_RESOLVE_TARGET) != 0) {
		GREM_ASSERT(key.colorFormat != VK_FORMAT_UNDEFINED);
		initialResolveLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		finalResolveLayout =
			((key.flags & RENDER_PASS_RESOLVE_TARGET_IS_SWAPCHAIN) != 0) ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			: ((key.flags & RENDER_PASS_RESOLVE_TARGET_IS_SAMPLED) != 0)
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		resolveAttachments.push_back(VkAttachmentReference{
			.attachment = static_cast<uint32_t>(attachments.size()),
			.layout = initialResolveLayout,
		});
		attachments.push_back(VkAttachmentDescription{
			.flags = VkAttachmentDescriptionFlags{},
			.format = key.colorFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = initialResolveLayout,
			.finalLayout = finalResolveLayout,
		});
	}

	if ((key.flags & (RENDER_PASS_COLOR_TARGET_IS_SAMPLED | RENDER_PASS_DEPTH_STENCIL_TARGET_IS_SAMPLED | RENDER_PASS_RESOLVE_TARGET_IS_SAMPLED)) != 0) {
		externalToSubpass0Dependency.srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		externalToSubpass0Dependency.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
		subpass0ToExternalDependency.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		subpass0ToExternalDependency.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
	}

	if ((key.flags & (RENDER_PASS_RENDER_TARGET_IS_SWAPCHAIN | RENDER_PASS_RESOLVE_TARGET_IS_SWAPCHAIN)) == 0) {
		externalToSubpass0Dependency.srcStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		externalToSubpass0Dependency.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		subpass0ToExternalDependency.dstStageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		subpass0ToExternalDependency.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	const Array subpasses{VkSubpassDescription{
		.flags = VkSubpassDescriptionFlags{},
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
		.pColorAttachments = colorAttachments.data(),
		.pResolveAttachments = (resolveAttachments.empty()) ? nullptr : resolveAttachments.data(),
		.pDepthStencilAttachment = (depthStencilAttachment) ? &*depthStencilAttachment : nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	}};
	const Array dependencies{externalToSubpass0Dependency, subpass0ToExternalDependency};
	GREM_ASSERT(externalToSubpass0Dependency.srcStageMask != VK_PIPELINE_STAGE_NONE && externalToSubpass0Dependency.dstStageMask != VK_PIPELINE_STAGE_NONE);
	GREM_ASSERT(externalToSubpass0Dependency.srcAccessMask != VK_ACCESS_NONE && externalToSubpass0Dependency.dstAccessMask != VK_ACCESS_NONE);
	GREM_ASSERT(subpass0ToExternalDependency.srcStageMask != VK_PIPELINE_STAGE_NONE && subpass0ToExternalDependency.dstStageMask != VK_PIPELINE_STAGE_NONE);
	GREM_ASSERT(subpass0ToExternalDependency.srcAccessMask != VK_ACCESS_NONE && subpass0ToExternalDependency.dstAccessMask != VK_ACCESS_NONE);
	const VkRenderPassCreateInfo renderPassCreateInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkRenderPassCreateFlags{},
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = static_cast<uint32_t>(subpasses.size()),
		.pSubpasses = subpasses.data(),
		.dependencyCount = static_cast<uint32_t>(dependencies.size()),
		.pDependencies = dependencies.data(),
	};
	VkRenderPass renderPassHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateRenderPass(device.logicalDevice.get(), &renderPassCreateInfo, nullptr, &renderPassHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateRenderPass", result};
	}
	renderPass = detail::VulkanRenderPass{renderPassHandle, detail::VulkanRenderPassDeleter{device.logicalDevice.get()}};
}

DeviceImplementation::MeshContext::VertexBuffers::VertexBuffers(VmaAllocator allocator, VertexAttributeMask activeVertexAttributes,
	Span<const VertexAttributeDescription> vertexAttributeDescriptions) {
	GREM_PROFILE_FUNCTION();

	constexpr size_t INITIAL_VERTEX_CAPACITY = 8;

	GREM_ASSERT(vertexAttributeDescriptions.size() <= activeVertexAttributes.size());
	while (firstActiveVertexAttributeIndex < vertexAttributeDescriptions.size()) {
		if (activeVertexAttributes[firstActiveVertexAttributeIndex]) {
			break;
		}
		++firstActiveVertexAttributeIndex;
	}
	for (size_t i = 0; i < vertexAttributeDescriptions.size(); ++i) {
		const size_t vertexAttributeStride = detail::getVertexAttributeSizeInBytes(vertexAttributeDescriptions[i]);
		if (activeVertexAttributes[i]) {
			vertexBuffers.emplace_back(allocator, INITIAL_VERTEX_CAPACITY * vertexAttributeStride, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		} else {
			vertexBuffers.emplace_back(allocator, 0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			largestInactiveVertexAttributeStride = max(largestInactiveVertexAttributeStride, vertexAttributeStride);
		}
	}
	vertexBufferHandles.resize(vertexBuffers.size(), VK_NULL_HANDLE);
	vertexBufferOffsets.resize(vertexBuffers.size(), VkDeviceSize{0});
}

void DeviceImplementation::MeshContext::VertexBuffers::flush(VertexAttributeMask activeVertexAttributes) {
	GREM_PROFILE_FUNCTION();

	for (size_t i = 0; i < vertexBuffers.size(); ++i) {
		if (activeVertexAttributes[i]) {
			vertexBufferHandles[i] = vertexBuffers[i].get();
		} else {
			vertexBufferHandles[i] = vertexBuffers[firstActiveVertexAttributeIndex].get();
		}
	}
}

DeviceImplementation::MeshContext::MeshContext(VkDevice device, VmaAllocator allocator, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t indexStride,
	size_t parameterStride, size_t textureParameterCount)
	: indexBuffer(allocator, 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
	, uniformBuffer(allocator, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
	, textureParameterCount(textureParameterCount) {
	GREM_PROFILE_FUNCTION();

	constexpr size_t INITIAL_INDEX_CAPACITY = 8;

	for (const VertexAttributeDescription& vertexAttributeDescription : vertexAttributeDescriptions) {
		const uint32_t binding = static_cast<uint32_t>(vertexBindings.size());
		vertexBindings.push_back(VkVertexInputBindingDescription{
			.binding = binding,
			.stride = static_cast<uint32_t>(detail::getVertexAttributeSizeInBytes(vertexAttributeDescription)),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		});
		vertexAttributes.push_back(VkVertexInputAttributeDescription{
			.location = static_cast<uint32_t>(vertexAttributes.size()),
			.binding = binding,
			.format = translateVertexAttributeFormat(vertexAttributeDescription.type),
			.offset = 0,
		});
	}

	if (indexStride > 0) {
		indexBuffer = detail::ResourceBuffer{allocator, INITIAL_INDEX_CAPACITY * indexStride, VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
	}

	if (parameterStride > 0 || textureParameterCount > 0) {
		InplaceBuffer<VkDescriptorSetLayoutBinding, 2> bindings{};
		InplaceBuffer<VkDescriptorBindingFlags, 2> bindingFlags{};
		if (parameterStride > 0) {
			uniformBuffer = detail::ResourceBuffer{allocator, parameterStride, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
			bindings.push_back(VkDescriptorSetLayoutBinding{
				.binding = static_cast<uint32_t>(bindings.size()),
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			});
			bindingFlags.push_back(VkDescriptorBindingFlags{});
		}
		if (textureParameterCount > 0) {
			bindings.push_back(VkDescriptorSetLayoutBinding{
				.binding = static_cast<uint32_t>(bindings.size()),
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 65536,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			});
			bindingFlags.push_back(VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT});
		}
		const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.pNext = nullptr,
			.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
			.pBindingFlags = reinterpret_cast<const VkDescriptorBindingFlags*>(bindingFlags.data()),
		};
		const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &descriptorSetLayoutBindingFlagsCreateInfo,
			.flags = VkDescriptorSetLayoutCreateFlags{},
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.data()),
		};
		VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayoutHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorSetLayout", result};
		}
		parametersDescriptorSetLayout = detail::VulkanDescriptorSetLayout{descriptorSetLayoutHandle, detail::VulkanDescriptorSetLayoutDeleter{device}};
	}
}

void DeviceImplementation::MeshContext::flush(DeviceImplementation& device) {
	if (flushed) {
		return;
	}

	for (auto&& [activeVertexAttributes, vertexBuffers] : vertexBufferMap) {
		vertexBuffers.flush(activeVertexAttributes);
	}

	if (parametersDescriptorSetLayout) {
		device.submitAndAwaitGraphicsCommands();

		const TextureImplementation* validTexture = nullptr;
		for (const TextureImplementation* const maybeTexture : textures) {
			if (maybeTexture) {
				validTexture = maybeTexture;
				break;
			}
		}

		textureSamplers.clear();
		if (validTexture) {
			textureSamplers.reserve(textures.size());
			for (const TextureImplementation* const maybeTexture : textures) {
				const TextureImplementation* const texture = (maybeTexture) ? maybeTexture : validTexture;
				textureSamplers.push_back({
					.sampler = texture->object.get<detail::TextureResources>().sampler,
					.imageView = texture->object.get<detail::TextureResources>().samplerImageView,
					.imageLayout = texture->imageLayout,
				});
			}
		}

		parametersDescriptorSet = VK_NULL_HANDLE;
		parametersDescriptorPool.reset();

		InplaceBuffer<VkDescriptorPoolSize, 2> poolSizes{};
		if (uniformBuffer.capacity() > 0) {
			poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1});
		}
		if (textureParameterCount > 0) {
			poolSizes.push_back(VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(textureSamplers.size())});
		}
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkDescriptorPoolCreateFlags{},
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data(),
		};
		VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateDescriptorPool(device.logicalDevice.get(), &descriptorPoolCreateInfo, nullptr, &descriptorPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateDescriptorPool", result};
		}
		parametersDescriptorPool = detail::VulkanDescriptorPool{descriptorPoolHandle, detail::VulkanDescriptorPoolDeleter{device.logicalDevice.get()}};

		const Array setLayouts{parametersDescriptorSetLayout.get()};
		const Array descriptorCounts{static_cast<uint32_t>(textureSamplers.size())};
		const VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorSetVariableDescriptorCountAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorSetCount = static_cast<uint32_t>(descriptorCounts.size()),
			.pDescriptorCounts = descriptorCounts.data(),
		};
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = (textureParameterCount > 0) ? &descriptorSetVariableDescriptorCountAllocateInfo : nullptr,
			.descriptorPool = descriptorPoolHandle,
			.descriptorSetCount = static_cast<uint32_t>(setLayouts.size()),
			.pSetLayouts = setLayouts.data(),
		};
		if (const VkResult result = vkAllocateDescriptorSets(device.logicalDevice.get(), &descriptorSetAllocateInfo, &parametersDescriptorSet); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateDescriptorSets", result};
		}

		const VkDescriptorBufferInfo uniformBufferInfo{
			.buffer = uniformBuffer.get(),
			.offset = 0,
			.range = uniformBuffer.capacity(),
		};
		InplaceBuffer<VkWriteDescriptorSet, 2> descriptorWrites{};
		if (uniformBuffer.capacity() > 0) {
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = parametersDescriptorSet,
				.dstBinding = static_cast<uint32_t>(descriptorWrites.size()),
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &uniformBufferInfo,
				.pTexelBufferView = nullptr,
			});
		}
		if (textureParameterCount > 0) {
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = parametersDescriptorSet,
				.dstBinding = static_cast<uint32_t>(descriptorWrites.size()),
				.dstArrayElement = 0,
				.descriptorCount = static_cast<uint32_t>(textureSamplers.size()),
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = textureSamplers.data(),
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			});
		}
		GREM_ASSERT(!descriptorWrites.empty());
		vkUpdateDescriptorSets(device.logicalDevice.get(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	flushed = true;
}

DeviceImplementation::Pipeline::Pipeline(DeviceImplementation& device, const PipelineKey& key, const MeshContext& meshContext) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(&meshContext == &device.meshContextMap.at(key.shaderPipelineHandle->meshTypeIndex));

	const VkDevice deviceHandle = device.logicalDevice.get();
	const VkPipelineCache pipelineCache = device.pipelineCache.get();

	const VertexShaderImplementation& vertexShader = *key.shaderPipelineHandle->vertexShaderHandle;
	const FragmentShaderImplementation& fragmentShader = *key.shaderPipelineHandle->fragmentShaderHandle;
	const Span<const ConstantDescription> vertexShaderConstantDescriptions = key.shaderPipelineHandle->vertexShaderConstantDescriptions;
	const Span<const ConstantDescription> fragmentShaderConstantDescriptions = key.shaderPipelineHandle->fragmentShaderConstantDescriptions;
	const Span<const byte> vertexShaderConstantData = key.shaderPipelineHandle->vertexShaderConstantData;
	const Span<const byte> fragmentShaderConstantData = key.shaderPipelineHandle->fragmentShaderConstantData;
	const ShaderPipelineOptions shaderPipelineOptions = key.shaderPipelineHandle->shaderPipelineOptions;
	RenderPassContext& renderPassContext = device.getRenderPassContext(key.renderPassContextKey);

	SmallBuffer<VkDescriptorSetLayout, 8> descriptorSetLayouts{};
	if (!vertexShader.instanceAttributeDescriptions.empty() || !vertexShader.parameterDescriptions.empty()) {
		descriptorSetLayouts.push_back(device.instanceOrDrawCommandBufferDescriptorSetLayout.get());
	}
	if (!vertexShader.instanceAttributeDescriptions.empty()) {
		descriptorSetLayouts.push_back(device.instanceOrDrawCommandBufferDescriptorSetLayout.get());
	}
	if (!vertexShader.parameterDescriptions.empty()) {
		descriptorSetLayouts.push_back(meshContext.parametersDescriptorSetLayout.get());
	}
	for (const BufferLayoutReference& bufferLayout : vertexShader.bufferLayouts) {
		descriptorSetLayouts.push_back(device.bufferDescriptorSetLayoutMap.at(bufferLayout.nameCRC32).get());
	}
	if (!fragmentShader.bufferLayouts.empty()) {
		if (vertexShader.bufferLayouts.size() > fragmentShader.bufferLayouts.size()) {
			throw graphics::Error{
				"Failed to link shader program:\n"
				"Incompatible shaders: The vertex shader uses more buffers than the fragment shader."};
		}
		auto it = fragmentShader.bufferLayouts.begin();
		for (const BufferLayoutReference& bufferLayout : vertexShader.bufferLayouts) {
			if (*it++ != bufferLayout) {
				throw graphics::Error{
					"Failed to link shader program:\n"
					"Incompatible shaders: The fragment shader does not use all of the vertex shader's buffers."};
			}
		}
		while (it != fragmentShader.bufferLayouts.end()) {
			descriptorSetLayouts.push_back(device.bufferDescriptorSetLayoutMap.at(it->nameCRC32).get());
			++it;
		}
	}

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineLayoutCreateFlags{},
		.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
		.pSetLayouts = descriptorSetLayouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};
	VkPipelineLayout pipelineLayoutHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCreateInfo, nullptr, &pipelineLayoutHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreatePipelineLayout", result};
	}
	pipelineLayout = detail::VulkanPipelineLayout{pipelineLayoutHandle, detail::VulkanPipelineLayoutDeleter{deviceHandle}};

	SmallBuffer<VkSpecializationMapEntry, 16> vertexShaderSpecializationMapEntries{};
	{
		uint32_t constantID = 0;
		uint32_t offset = 0;
		for (const ConstantDescription& constantDescription : vertexShaderConstantDescriptions) {
			(void)constantDescription;
			vertexShaderSpecializationMapEntries.push_back(VkSpecializationMapEntry{
				.constantID = constantID,
				.offset = offset,
				.size = sizeof(float),
			});
			++constantID;
			offset += sizeof(float);
		}
	}

	SmallBuffer<VkSpecializationMapEntry, 16> fragmentShaderSpecializationMapEntries{};
	{
		uint32_t constantID = 0;
		uint32_t offset = 0;
		for (const ConstantDescription& constantDescription : fragmentShaderConstantDescriptions) {
			(void)constantDescription;
			fragmentShaderSpecializationMapEntries.push_back(VkSpecializationMapEntry{
				.constantID = constantID,
				.offset = offset,
				.size = sizeof(float),
			});
			++constantID;
			offset += sizeof(float);
		}
	}

	const VkSpecializationInfo vertexShaderSpecializationInfo{
		.mapEntryCount = static_cast<uint32_t>(vertexShaderSpecializationMapEntries.size()),
		.pMapEntries = vertexShaderSpecializationMapEntries.data(),
		.dataSize = vertexShaderConstantData.size(),
		.pData = vertexShaderConstantData.data(),
	};

	const VkSpecializationInfo fragmentShaderSpecializationInfo{
		.mapEntryCount = static_cast<uint32_t>(fragmentShaderSpecializationMapEntries.size()),
		.pMapEntries = fragmentShaderSpecializationMapEntries.data(),
		.dataSize = fragmentShaderConstantData.size(),
		.pData = fragmentShaderConstantData.data(),
	};

	const Array stages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkPipelineShaderStageCreateFlags{},
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = match(vertexShader.shaderModule)(                                                              //
				[](const detail::VulkanShaderModule& shaderModule) -> VkShaderModule { return shaderModule.get(); }, //
				[](VkShaderModule shaderModule) -> VkShaderModule { return shaderModule; }),
			.pName = "main",
			.pSpecializationInfo = (vertexShaderSpecializationMapEntries.empty()) ? nullptr : &vertexShaderSpecializationInfo,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkPipelineShaderStageCreateFlags{},
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = match(fragmentShader.shaderModule)(                                                            //
				[](const detail::VulkanShaderModule& shaderModule) -> VkShaderModule { return shaderModule.get(); }, //
				[](VkShaderModule shaderModule) -> VkShaderModule { return shaderModule; }),
			.pName = "main",
			.pSpecializationInfo = (fragmentShaderSpecializationMapEntries.empty()) ? nullptr : &fragmentShaderSpecializationInfo,
		},
	};
	const VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineVertexInputStateCreateFlags{},
		.vertexBindingDescriptionCount = static_cast<uint32_t>(meshContext.vertexBindings.size()),
		.pVertexBindingDescriptions = reinterpret_cast<const VkVertexInputBindingDescription*>(meshContext.vertexBindings.data()),
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(meshContext.vertexAttributes.size()),
		.pVertexAttributeDescriptions = reinterpret_cast<const VkVertexInputAttributeDescription*>(meshContext.vertexAttributes.data()),
	};
	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineInputAssemblyStateCreateFlags{},
		.topology = translatePrimitiveType(shaderPipelineOptions.primitiveType),
		.primitiveRestartEnable = VK_FALSE,
	};
	const VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineViewportStateCreateFlags{},
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};
	const VkPipelineRasterizationStateCreateInfo rasterizationState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineRasterizationStateCreateFlags{},
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = translatePolygonMode(shaderPipelineOptions.polygonMode),
		.cullMode = translateFaceCullingMode(shaderPipelineOptions.faceCullingMode),
		.frontFace = translateFrontFace(shaderPipelineOptions.frontFace),
		.depthBiasEnable = (shaderPipelineOptions.depthBiasConstantFactor != 0.0f || shaderPipelineOptions.depthBiasSlopeFactor != 0.0f),
		.depthBiasConstantFactor = shaderPipelineOptions.depthBiasConstantFactor,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = shaderPipelineOptions.depthBiasSlopeFactor,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisampleState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineMultisampleStateCreateFlags{},
		.rasterizationSamples = static_cast<VkSampleCountFlagBits>(key.renderPassContextKey.sampleCount),
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};
	const VkPipelineDepthStencilStateCreateInfo depthStencilState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineDepthStencilStateCreateFlags{},
		.depthTestEnable = (shaderPipelineOptions.depthBufferMode == DepthBufferMode::NONE) ? VK_FALSE : VK_TRUE,
		.depthWriteEnable = (shaderPipelineOptions.depthBufferMode == DepthBufferMode::USE_DEPTH_TEST) ? VK_TRUE : VK_FALSE,
		.depthCompareOp = translateDepthTestPredicate(shaderPipelineOptions.depthTestPredicate),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = (shaderPipelineOptions.stencilBufferMode == StencilBufferMode::USE_STENCIL_TEST) ? VK_TRUE : VK_FALSE,
		.front{
			.failOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnFrontFaceStencilTestFail),
			.passOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnFrontFacePass),
			.depthFailOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnFrontFaceDepthTestFail),
			.compareOp = translateStencilTestPredicate(shaderPipelineOptions.stencilTestFrontFacePredicate),
			.compareMask = static_cast<uint32_t>(shaderPipelineOptions.stencilTestFrontFaceMask),
			.writeMask = static_cast<uint32_t>(shaderPipelineOptions.stencilTestFrontFaceMask),
			.reference = static_cast<uint32_t>(static_cast<uint8_t>(shaderPipelineOptions.stencilTestFrontFaceReferenceValue)),
		},
		.back{
			.failOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnBackFaceStencilTestFail),
			.passOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnBackFacePass),
			.depthFailOp = translateStencilBufferOperation(shaderPipelineOptions.stencilBufferOperationOnBackFaceDepthTestFail),
			.compareOp = translateStencilTestPredicate(shaderPipelineOptions.stencilTestBackFacePredicate),
			.compareMask = static_cast<uint32_t>(shaderPipelineOptions.stencilTestBackFaceMask),
			.writeMask = static_cast<uint32_t>(shaderPipelineOptions.stencilTestBackFaceMask),
			.reference = static_cast<uint32_t>(static_cast<uint8_t>(shaderPipelineOptions.stencilTestBackFaceReferenceValue)),
		},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f,
	};
	const Array attachments{VkPipelineColorBlendAttachmentState{
		.blendEnable = (shaderPipelineOptions.blendState) ? VK_TRUE : VK_FALSE,
		.srcColorBlendFactor = (shaderPipelineOptions.blendState) ? translateBlendFactor(shaderPipelineOptions.blendState->sourceColorBlendFactor) : VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = (shaderPipelineOptions.blendState) ? translateBlendFactor(shaderPipelineOptions.blendState->destinationColorBlendFactor) : VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = (shaderPipelineOptions.blendState) ? translateBlendOperation(shaderPipelineOptions.blendState->colorBlendOperation) : VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = (shaderPipelineOptions.blendState) ? translateBlendFactor(shaderPipelineOptions.blendState->sourceAlphaBlendFactor) : VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = (shaderPipelineOptions.blendState) ? translateBlendFactor(shaderPipelineOptions.blendState->destinationAlphaBlendFactor) : VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = (shaderPipelineOptions.blendState) ? translateBlendOperation(shaderPipelineOptions.blendState->alphaBlendOperation) : VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	}};
	const vec4 blendConstants = (shaderPipelineOptions.blendState) ? shaderPipelineOptions.blendState->blendConstants.toLinearRGBA() : vec4{1.0f, 1.0f, 1.0f, 1.0f};
	const VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineColorBlendStateCreateFlags{},
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_NO_OP,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.blendConstants{blendConstants.x, blendConstants.y, blendConstants.z, blendConstants.w},
	};
	const Array dynamicStates{
		VkDynamicState{VK_DYNAMIC_STATE_VIEWPORT},
		VkDynamicState{VK_DYNAMIC_STATE_SCISSOR},
	};
	const VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineDynamicStateCreateFlags{},
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data(),
	};
	const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VkPipelineCreateFlags{},
		.stageCount = static_cast<uint32_t>(stages.size()),
		.pStages = stages.data(),
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pTessellationState = nullptr,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = pipelineLayoutHandle,
		.renderPass = renderPassContext.renderPass.get(),
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};
	VkPipeline pipelineHandle = VK_NULL_HANDLE;
	if (const VkResult result = vkCreateGraphicsPipelines(deviceHandle, pipelineCache, 1, &graphicsPipelineCreateInfo, nullptr, &pipelineHandle); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkCreateGraphicsPipelines", result};
	}
	pipeline = detail::VulkanPipeline{pipelineHandle, detail::VulkanPipelineDeleter{deviceHandle}};
}

DeviceImplementation::DeviceImplementation(Filesystem* filesystem, Window& window, Device& device, const DeviceOptions& options)
	: filesystem(filesystem)
	, shaderCacheOutputFilepath(options.shaderCacheOutputFilepath)
	, device(device)
	, instance(getInstance(window))
	, physicalDevice(choosePhysicalDevice(instance, static_cast<VkSurfaceKHR>(window.getSurface())))
	, logicalDevice(createLogicalDevice(physicalDevice))
	, graphicsQueue(getQueue(logicalDevice.get(), physicalDevice.graphicsQueueFamilyIndex, 0))
	, presentQueue(getQueue(logicalDevice.get(), physicalDevice.presentQueueFamilyIndex, 0))
	, allocator(createAllocator(instance, physicalDevice.handle, logicalDevice.get()))
	, shaderCache(createShaderCache(pipelineCache, filesystem && !shaderCacheOutputFilepath.empty(), logicalDevice.get(), physicalDevice.uuid, filesystem,
		  options.shaderCacheInputFilepath))
	, instanceOrDrawCommandBufferDescriptorSetLayout(createInstanceOrDrawCommandBufferDescriptorSetLayout(logicalDevice.get())) {
	beginGraphicsQueueSubmission();
	GREM_PROFILE_CONSTRUCTOR_END();
}

DeviceImplementation::~DeviceImplementation() {
	awaitAllCommandsNoexcept();
	if (filesystem && !shaderCacheOutputFilepath.empty()) {
		try {
			saveShaderCache(*filesystem, shaderCacheOutputFilepath, logicalDevice.get(), pipelineCache.get(), shaderCache, physicalDevice.uuid);
		} catch (...) {
		}
	}
}

void DeviceImplementation::ensureExclusiveUncompressedTextureAccess(Texture& texture, bool uninitialized) {
	if (texture.implementation.use_count() >= 2) {
		const TextureFormat internalFormat = texture.getInternalFormat();
		if (Texture::getFormatAspects(internalFormat) == TextureAspect::COLOR) {
			const size_t sizeInBytes = resource::Image::getSizeInBytes(Texture::getImageFormat(internalFormat), texture.getSize3D(), texture.getMipLevelCount());
			if (sizeInBytes >= size_t{1073741824}) {
				if (detail::TextureResources* const resources = texture.implementation->object.get_if<detail::TextureResources>()) {
					resources->device.get()->submitAndAwaitGraphicsCommands();
				}
			}
		}
	}

	detail::ensureExclusiveResourceAccess(
		texture.implementation,
		[&]() -> SharedPointer<TextureImplementation> {
			return (uninitialized) ? TextureImplementation::cloneUncompressedUninitialized(*texture.implementation)
		                           : TextureImplementation::cloneUncompressed(*texture.implementation);
		},
		[&](TextureImplementation& oldTexture) -> void {
			if (!uninitialized) {
				oldTexture.assignFromOtherUncompressedTextureOfSameShape(*texture.implementation);
			}
		});
}

void DeviceImplementation::awaitAllCommands() { // NOLINT(readability-make-member-function-const)
	GREM_PROFILE_FUNCTION();
	if (const VkResult result = awaitAllCommandsNoexcept(); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkQueueWaitIdle", result};
	}
	submitAndAwaitGraphicsCommands();
}

VkResult DeviceImplementation::awaitAllCommandsNoexcept() noexcept { // NOLINT(readability-make-member-function-const)
	const TimePoint waitStartTime = Clock::now();
	if (const VkResult result = vkQueueWaitIdle(graphicsQueue); result != VK_SUCCESS) {
		return result;
	}
	if (const VkResult result = vkQueueWaitIdle(presentQueue); result != VK_SUCCESS) {
		return result;
	}
	const TimePoint waitEndTime = Clock::now();
	currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
	return VK_SUCCESS;
}

void DeviceImplementation::submitGraphicsCommands(VkSemaphore signalSemaphore) {
	GREM_PROFILE_FUNCTION();

	endGraphicsQueueSubmission();
	GraphicsQueueSubmission& submission = graphicsQueueSubmissions.back();
	const Array submits{VkSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitDestinationPipelineStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &submission.commandBuffer,
		.signalSemaphoreCount = (signalSemaphore) ? uint32_t{1} : uint32_t{0},
		.pSignalSemaphores = &signalSemaphore,
	}};
	if (const VkResult result = vkQueueSubmit(graphicsQueue, static_cast<uint32_t>(submits.size()), submits.data(), submission.fence.get()); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkQueueSubmit", result};
	}
	waitSemaphores.clear();
	waitDestinationPipelineStages.clear();
	beginGraphicsQueueSubmission();
}

void DeviceImplementation::submitAndAwaitGraphicsCommands() {
	GREM_PROFILE_FUNCTION();

	endGraphicsQueueSubmission();
	GraphicsQueueSubmission& submission = graphicsQueueSubmissions.back();
	const Array submits{VkSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitDestinationPipelineStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &submission.commandBuffer,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr,
	}};
	if (const VkResult result = vkQueueSubmit(graphicsQueue, static_cast<uint32_t>(submits.size()), submits.data(), submission.fence.get()); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkQueueSubmit", result};
	}
	waitSemaphores.clear();
	waitDestinationPipelineStages.clear();

	const Array fences{submission.fence.get()};
	const TimePoint waitStartTime = Clock::now();
	switch (const VkResult result = vkWaitForFences(logicalDevice.get(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, Limits<uint64_t>::MAX)) {
		case VK_SUCCESS: [[fallthrough]];
		case VK_TIMEOUT: break;
		default: throw detail::VulkanError{"vkWaitForFences", result};
	}
	const TimePoint waitEndTime = Clock::now();
	currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
	beginGraphicsQueueSubmission();
	cleanupRenderPassesAvailableForReuse();
}

void DeviceImplementation::cleanupRenderPassesAvailableForReuse() {
	for (const SharedPointer<RenderPassImplementation>& renderPass : renderPassesForReuse) {
		if (renderPass.use_count() == 1) {
			renderPass->reset();
		}
	}
}

void DeviceImplementation::cleanupExpiredFramebufferContexts() {
	GREM_PROFILE_FUNCTION();

	for (auto&& [key, renderPassContext] : renderPassContextMap) {
		erase_if(renderPassContext.framebufferContextMap, [](const auto& kv) -> bool { return kv.first.isExpired(); });
	}
}

void DeviceImplementation::adoptTextureResources(GraphicsQueueSubmissionGenerationIndex latestGraphicsQueueSubmissionUsingThisResource, detail::TextureResources&& resources) {
	GREM_PROFILE_FUNCTION();

	if (const auto it = upperBound(graphicsQueueSubmissions, latestGraphicsQueueSubmissionUsingThisResource,
			[](GraphicsQueueSubmissionGenerationIndex generationIndex, const GraphicsQueueSubmission& submission) -> bool { return generationIndex < submission.generationIndex; });
		it != graphicsQueueSubmissions.begin()) {
		(it - 1)->ownedTargetedTextureResources.push_back(std::move(resources));
	}
}

void DeviceImplementation::adoptFramebufferContext(RenderPassContext::FramebufferContext&& framebufferContext) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(framebufferContext.latestGraphicsQueueSubmissionUsingThisResource != NOT_IN_USE);
	if (const auto it = upperBound(graphicsQueueSubmissions, framebufferContext.latestGraphicsQueueSubmissionUsingThisResource,
			[](GraphicsQueueSubmissionGenerationIndex generationIndex, const GraphicsQueueSubmission& submission) -> bool { return generationIndex < submission.generationIndex; });
		it != graphicsQueueSubmissions.begin()) {
		(it - 1)->ownedTargetedFramebufferContexts.push_back(std::move(framebufferContext));
	}
}

Texture& DeviceImplementation::acquireSwapchainImage(TextureImplementation& swapchainTexture) {
	GREM_PROFILE_FUNCTION();

	TextureImplementation::SwapchainImplementation& swapchainImplementation = swapchainTexture.object.get<TextureImplementation::SwapchainImplementation>();
	if (!swapchainImplementation.acquiredImageIndex) {
		const size_t maxFramesInFlight = min(static_cast<size_t>(1 + swapchainImplementation.options.maxBufferedFrameCount), swapchainImplementation.images.size());
		GREM_ASSERT(maxFramesInFlight > 0);
		GREM_ASSERT(!graphicsQueueSubmissions.empty());
		while (swapchainImplementation.imagePresentationSubmissions.size() >= maxFramesInFlight) {
			if (graphicsQueueSubmissions.size() >= 2) {
				DeviceImplementation::GraphicsQueueSubmission& submission = graphicsQueueSubmissions.front();
				const GraphicsQueueSubmissionGenerationIndex generationIndex = submission.generationIndex;
				const Array fences{submission.fence.get()};
				const TimePoint waitStartTime = Clock::now();
				if (const VkResult result = vkWaitForFences(logicalDevice.get(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, Limits<uint64_t>::MAX);
					result != VK_SUCCESS) {
					throw detail::VulkanError{"vkWaitForFences", result};
				}
				const TimePoint waitEndTime = Clock::now();
				currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
				popGraphicsQueueSubmission(*this);
				while (generationIndex >= swapchainImplementation.imagePresentationSubmissions.front().graphicsQueueSubmissionGenerationIndex) {
					swapchainImplementation.imagePresentationSubmissions.pop_front();
					if (swapchainImplementation.imagePresentationSubmissions.empty()) {
						break;
					}
				}
			} else {
				swapchainImplementation.imagePresentationSubmissions.clear();
			}
		}

		const TimePoint waitStartTime = Clock::now();
		swapchainImplementation.acquireNextImage(swapchainTexture.size);
		const TimePoint waitEndTime = Clock::now();
		currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;

		waitSemaphores.push_back(swapchainImplementation.imagePresentationSubmissions.back().imageAcquiredSemaphore.get());
		waitDestinationPipelineStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	return swapchainImplementation.images[*swapchainImplementation.acquiredImageIndex];
}

detail::StagingBuffer DeviceImplementation::acquireStagingBuffer(size_t size) {
	GREM_PROFILE_FUNCTION();

	if (totalStagingBuffersInFlight >= MAX_SIMULTANEOUS_STAGING_BUFFER_COUNT ||
		(totalStagingBuffersInFlight > 0 && totalStagingBufferBytesInFlight + size > MAX_SIMULTANEOUS_TOTAL_STAGING_BUFFER_SIZE)) {
		submitAndAwaitGraphicsCommands();
	}

	detail::StagingBuffer stagingBuffer{};
	if (!stagingBuffersForReuse.empty()) {
		stagingBuffer = std::move(stagingBuffersForReuse.front());
		stagingBuffersForReuse.pop_front();
	}
	if (stagingBuffer.size() < size || (stagingBuffer.size() > size && stagingBuffer.size() > MAX_SIMULTANEOUS_TOTAL_STAGING_BUFFER_SIZE)) {
		stagingBuffer = detail::StagingBuffer{allocator.get(), size};
	}
	return stagingBuffer;
}

void DeviceImplementation::submitStagingBuffer(detail::StagingBuffer stagingBuffer) {
	GraphicsQueueSubmission& submission = graphicsQueueSubmissions.back();
	submission.ownedStagingBuffers.push_back(std::move(stagingBuffer));
	++totalStagingBuffersInFlight;
	totalStagingBufferBytesInFlight += submission.ownedStagingBuffers.back().size();
}

void DeviceImplementation::declareMeshType(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes,
	Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t indexStride, size_t parameterStride, size_t textureParameterCount) {
	GREM_PROFILE_FUNCTION();

	meshContextMap.try_emplace(meshTypeIndex, logicalDevice.get(), allocator.get(), vertexAttributeDescriptions, indexStride, parameterStride, textureParameterCount)
		.first->second.vertexBufferMap.try_emplace(activeVertexAttributes, allocator.get(), activeVertexAttributes, vertexAttributeDescriptions);
}

RangeAllocation<uint32_t> DeviceImplementation::uploadIndices(std::type_index meshTypeIndex, Span<const byte> indexData, uint32_t indexCount, size_t indexStride) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(indexData.size_bytes() == static_cast<size_t>(indexCount) * indexStride);
	if (indexCount == 0) {
		[[unlikely]];
		return {};
	}

	GREM_ASSERT(indexStride > 0);
	MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
	const size_t oldBufferRangeBegin = static_cast<size_t>(meshContext.indexRangeAllocator.getUsedRangeBegin()) * indexStride;
	const size_t oldBufferRangeEnd = static_cast<size_t>(meshContext.indexRangeAllocator.getUsedRangeEnd()) * indexStride;
	const RangeAllocation<uint32_t> allocation = acquireElementRange(meshContext.indexRangeAllocator, meshContext.indexRangeReferenceCounts, indexCount);
	try {
		const size_t byteOffset = static_cast<size_t>(allocation.begin) * indexStride;
		const size_t sizeBytes = static_cast<size_t>(allocation.size()) * indexStride;
		GREM_ASSERT(indexData.size_bytes() == sizeBytes);

		detail::StagingBuffer stagingBuffer = acquireStagingBuffer(sizeBytes);
		memcpy(stagingBuffer.data(), indexData.data(), sizeBytes);
		stagingBuffer.flush(0, sizeBytes);

		if (meshContext.indexBuffer.upload(getGraphicsCommandBuffer(), oldBufferRangeBegin, oldBufferRangeEnd, byteOffset + sizeBytes, byteOffset, sizeBytes, stagingBuffer.get(),
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, [&]() -> VkCommandBuffer {
					submitAndAwaitGraphicsCommands();
					return getGraphicsCommandBuffer();
				})) {
			meshContext.flushed = false;
		}

		submitStagingBuffer(std::move(stagingBuffer));
	} catch (...) {
		releaseElementRange(meshContext.indexRangeAllocator, meshContext.indexRangeReferenceCounts, allocation);
		throw;
	}
	return allocation;
}

RangeAllocation<uint32_t> DeviceImplementation::uploadVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const byte> vertexData,
	uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions, size_t vertexStride) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(vertexData.size_bytes() == static_cast<size_t>(vertexCount) * vertexStride);
	if (vertexCount == 0) {
		[[unlikely]];
		return {};
	}

	MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
	MeshContext::VertexBuffers& vertexBuffers = meshContext.vertexBufferMap.at(activeVertexAttributes);
	GREM_ASSERT(vertexBuffers.vertexBuffers.size() == vertexAttributeDescriptions.size());

	const uint32_t oldRangeBegin = vertexBuffers.vertexRangeAllocator.getUsedRangeBegin();
	const uint32_t oldRangeEnd = vertexBuffers.vertexRangeAllocator.getUsedRangeEnd();
	const RangeAllocation<uint32_t> allocation = acquireElementRange(vertexBuffers.vertexRangeAllocator, vertexBuffers.vertexRangeReferenceCounts, vertexCount);
	try {
		size_t vertexAttributeOffset = 0;
		for (size_t i = 0; i < vertexAttributeDescriptions.size(); ++i) {
			const size_t vertexAttributeStride = detail::getVertexAttributeSizeInBytes(vertexAttributeDescriptions[i]);
			GREM_ASSERT(vertexAttributeStride > 0);
			if (activeVertexAttributes[i]) {
				const size_t oldBufferRangeBegin = static_cast<size_t>(oldRangeBegin) * vertexAttributeStride;
				const size_t oldBufferRangeEnd = static_cast<size_t>(oldRangeEnd) * vertexAttributeStride;
				const size_t byteOffset = static_cast<size_t>(allocation.begin) * vertexAttributeStride;
				const size_t sizeBytes = static_cast<size_t>(vertexCount) * vertexAttributeStride;

				detail::StagingBuffer stagingBuffer = acquireStagingBuffer(sizeBytes);
				if (vertexAttributeStride == vertexStride) {
					GREM_ASSERT(vertexData.size_bytes() == sizeBytes);
					memcpy(stagingBuffer.data(), vertexData.data(), sizeBytes);
				} else {
					for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
						memcpy(stagingBuffer.data() + static_cast<size_t>(vertexIndex) * vertexAttributeStride,
							vertexData.data() + vertexAttributeOffset + static_cast<size_t>(vertexIndex) * vertexStride, vertexAttributeStride);
					}
				}
				stagingBuffer.flush(0, sizeBytes);

				if (vertexBuffers.vertexBuffers[i].upload(getGraphicsCommandBuffer(), oldBufferRangeBegin, oldBufferRangeEnd, byteOffset + sizeBytes, byteOffset, sizeBytes,
						stagingBuffer.get(), VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, [&]() -> VkCommandBuffer {
							submitAndAwaitGraphicsCommands();
							return getGraphicsCommandBuffer();
						})) {
					meshContext.flushed = false;
				}
				submitStagingBuffer(std::move(stagingBuffer));
			}
			vertexAttributeOffset += vertexAttributeStride;
		}
	} catch (...) {
		releaseElementRange(vertexBuffers.vertexRangeAllocator, vertexBuffers.vertexRangeReferenceCounts, allocation);
		throw;
	}
	return allocation;
}

RangeAllocation<uint32_t> DeviceImplementation::uploadVertexAttributes(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes,
	Span<const Span<const byte>> vertexAttributeData, uint32_t vertexCount, Span<const VertexAttributeDescription> vertexAttributeDescriptions) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(vertexAttributeData.size() == vertexAttributeDescriptions.size());
	if (vertexCount == 0) {
		[[unlikely]];
		return {};
	}

	MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
	MeshContext::VertexBuffers& vertexBuffers = meshContext.vertexBufferMap.at(activeVertexAttributes);
	GREM_ASSERT(vertexBuffers.vertexBuffers.size() == vertexAttributeDescriptions.size());

	const size_t firstActiveVertexAttributeIndex = vertexBuffers.firstActiveVertexAttributeIndex;
	const size_t firstActiveVertexAttributeStride = detail::getVertexAttributeSizeInBytes(vertexAttributeDescriptions[firstActiveVertexAttributeIndex]);
	const size_t largestInactiveVertexAttributeStride = vertexBuffers.largestInactiveVertexAttributeStride;
	const size_t largestInactiveVertexAttributeRequiredSizeInBytes = largestInactiveVertexAttributeStride * static_cast<size_t>(vertexCount);
	const uint32_t paddedVertexCount =
		max(vertexCount, static_cast<uint32_t>((largestInactiveVertexAttributeRequiredSizeInBytes + firstActiveVertexAttributeStride - 1) / firstActiveVertexAttributeStride));

	const uint32_t oldRangeBegin = vertexBuffers.vertexRangeAllocator.getUsedRangeBegin();
	const uint32_t oldRangeEnd = vertexBuffers.vertexRangeAllocator.getUsedRangeEnd();
	const RangeAllocation<uint32_t> allocation = acquireElementRange(vertexBuffers.vertexRangeAllocator, vertexBuffers.vertexRangeReferenceCounts, paddedVertexCount);
	try {
		for (size_t i = 0; i < vertexAttributeDescriptions.size(); ++i) {
			if (activeVertexAttributes[i]) {
				const size_t vertexAttributeStride = detail::getVertexAttributeSizeInBytes(vertexAttributeDescriptions[i]);
				const Span<const byte> data = vertexAttributeData[i];
				GREM_ASSERT(!data.empty());
				GREM_ASSERT(data.size_bytes() == static_cast<size_t>(vertexCount) * vertexAttributeStride);
				const size_t oldBufferRangeBegin = static_cast<size_t>(oldRangeBegin) * vertexAttributeStride;
				const size_t oldBufferRangeEnd = static_cast<size_t>(oldRangeEnd) * vertexAttributeStride;
				const size_t byteOffset = static_cast<size_t>(allocation.begin) * vertexAttributeStride;
				const size_t paddedSizeBytes = static_cast<size_t>(paddedVertexCount) * vertexAttributeStride;
				const size_t sizeBytes = (i == firstActiveVertexAttributeIndex) ? paddedSizeBytes : static_cast<size_t>(vertexCount) * vertexAttributeStride;
				GREM_ASSERT(sizeBytes >= data.size_bytes());

				detail::StagingBuffer stagingBuffer = acquireStagingBuffer(sizeBytes);
				memcpy(stagingBuffer.data(), data.data(), data.size_bytes());
				stagingBuffer.flush(0, sizeBytes);

				if (vertexBuffers.vertexBuffers[i].upload(getGraphicsCommandBuffer(), oldBufferRangeBegin, oldBufferRangeEnd, byteOffset + paddedSizeBytes, byteOffset, sizeBytes,
						stagingBuffer.get(), VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, [&]() -> VkCommandBuffer {
							submitAndAwaitGraphicsCommands();
							return getGraphicsCommandBuffer();
						})) {
					meshContext.flushed = false;
				}
				submitStagingBuffer(std::move(stagingBuffer));
			}
		}
	} catch (...) {
		releaseElementRange(vertexBuffers.vertexRangeAllocator, vertexBuffers.vertexRangeReferenceCounts, allocation);
		throw;
	}
	return allocation;
}

RangeAllocation<uint32_t> DeviceImplementation::uploadParameters(std::type_index meshTypeIndex, Span<const byte> parameterValuesBytes,
	Span<const SharedPointer<TextureImplementation>> textures) {
	GREM_PROFILE_FUNCTION();

	MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
	meshContext.flushed = false;
	GREM_ASSERT(textures.size() == meshContext.textureParameterCount);

	const size_t parameterStride = parameterValuesBytes.size_bytes();
	const size_t oldBufferRangeBegin = static_cast<size_t>(meshContext.parameterRangeAllocator.getUsedRangeBegin()) * parameterStride;
	const size_t oldBufferRangeEnd = static_cast<size_t>(meshContext.parameterRangeAllocator.getUsedRangeEnd()) * parameterStride;
	const RangeAllocation<uint32_t> allocation = acquireElementRange(meshContext.parameterRangeAllocator, meshContext.parameterRangeReferenceCounts, 1);
	GREM_ASSERT(allocation.end == allocation.begin + 1);
	try {
		const size_t textureSamplersBegin = allocation.begin * meshContext.textureParameterCount;
		const size_t textureSamplersEnd = textureSamplersBegin + meshContext.textureParameterCount;
		if (textureSamplersEnd > meshContext.textureSamplers.size()) {
			meshContext.textures.resize(textureSamplersEnd, nullptr);
		}
		for (size_t i = 0; i < textures.size(); ++i) {
			GREM_ASSERT(textures[i]);
			meshContext.textures[textureSamplersBegin + i] = textures[i].get();
		}

		const size_t byteOffset = static_cast<size_t>(allocation.begin) * parameterStride;

		detail::StagingBuffer stagingBuffer = acquireStagingBuffer(parameterStride);
		memcpy(stagingBuffer.data(), parameterValuesBytes.data(), parameterStride);
		stagingBuffer.flush(0, parameterStride);

		meshContext.uniformBuffer.upload(getGraphicsCommandBuffer(), oldBufferRangeBegin, oldBufferRangeEnd, byteOffset + parameterStride, byteOffset, parameterStride,
			stagingBuffer.get(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, [&]() -> VkCommandBuffer {
				submitAndAwaitGraphicsCommands();
				return getGraphicsCommandBuffer();
			});
		submitStagingBuffer(std::move(stagingBuffer));
	} catch (...) {
		releaseElementRange(meshContext.parameterRangeAllocator, meshContext.parameterRangeReferenceCounts, allocation);
		throw;
	}
	return allocation;
}

void DeviceImplementation::reacquireIndices(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		reacquireElementRange(meshContext.indexRangeReferenceCounts, allocation);
	}
}

void DeviceImplementation::reacquireVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		MeshContext::VertexBuffers& vertexBuffers = meshContext.vertexBufferMap.at(activeVertexAttributes);
		reacquireElementRange(vertexBuffers.vertexRangeReferenceCounts, allocation);
	}
}

void DeviceImplementation::reacquireParameters(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		reacquireElementRange(meshContext.parameterRangeReferenceCounts, allocation);
	}
}

void DeviceImplementation::releaseIndices(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		releaseElementRange(meshContext.indexRangeAllocator, meshContext.indexRangeReferenceCounts, allocation);
	}
}

void DeviceImplementation::releaseVertices(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		MeshContext::VertexBuffers& vertexBuffers = meshContext.vertexBufferMap.at(activeVertexAttributes);
		releaseElementRange(vertexBuffers.vertexRangeAllocator, vertexBuffers.vertexRangeReferenceCounts, allocation);
	}
}

void DeviceImplementation::releaseParameters(std::type_index meshTypeIndex, RangeAllocation<uint32_t> allocation) noexcept {
	GREM_PROFILE_FUNCTION();

	if (allocation.begin != allocation.end) {
		MeshContext& meshContext = meshContextMap.at(meshTypeIndex);
		if (releaseElementRange(meshContext.parameterRangeAllocator, meshContext.parameterRangeReferenceCounts, allocation)) {
			meshContext.flushed = false;
			const size_t textureSamplersBegin = allocation.begin * meshContext.textureParameterCount;
			const size_t textureSamplersEnd = textureSamplersBegin + meshContext.textureParameterCount;
			for (size_t i = textureSamplersBegin; i < textureSamplersEnd; ++i) {
				meshContext.textures[i] = nullptr;
			}
		}
	}
}

void DeviceImplementation::beginGraphicsQueueSubmission() {
	GREM_PROFILE_FUNCTION();

	while (graphicsQueueSubmissions.size() >= 2) {
		GraphicsQueueSubmission& submission = graphicsQueueSubmissions.front();
		bool fenceIsSignaled = false;
		const Array fences{submission.fence.get()};
		const uint64_t timeout = (graphicsQueueSubmissions.size() >= MAX_SIMULTANEOUS_GRAPHICS_QUEUE_SUBMISSION_COUNT) ? Limits<uint64_t>::MAX : 0;
		const TimePoint waitStartTime = Clock::now();
		switch (const VkResult result = vkWaitForFences(logicalDevice.get(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, timeout)) {
			case VK_SUCCESS: fenceIsSignaled = true; break;
			case VK_TIMEOUT:
				if (timeout == 0) {
					break;
				}
				[[fallthrough]];
			default: throw detail::VulkanError{"vkWaitForFences", result};
		}
		const TimePoint waitEndTime = Clock::now();
		currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
		if (!fenceIsSignaled) {
			break;
		}
		popGraphicsQueueSubmission(*this);
	}

	GraphicsQueueSubmission& submission = graphicsQueueSubmissions.push_back_unspecified_value();
	GREM_ASSERT(submission.usedRenderPasses.empty());
	GREM_ASSERT(submission.ownedStagingBuffers.empty());
	GREM_ASSERT(submission.ownedTargetedTextureResources.empty());
	GREM_ASSERT(submission.ownedTargetedFramebufferContexts.empty());
	submission.generationIndex = nextGraphicsQueueSubmissionGenerationIndex;
	if (submission.commandBuffer) {
		const Array fences{submission.fence.get()};
		if (const VkResult result = vkResetFences(logicalDevice.get(), static_cast<uint32_t>(fences.size()), fences.data()); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkResetFences", result};
		}

		if (const VkResult result = vkResetCommandBuffer(submission.commandBuffer, VkCommandBufferResetFlags{}); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkResetCommandBuffer", result};
		}
	} else {
		const VkFenceCreateInfo fenceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkFenceCreateFlags{},
		};
		VkFence fenceHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateFence(logicalDevice.get(), &fenceCreateInfo, nullptr, &fenceHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateFence", result};
		}
		submission.fence = detail::VulkanFence{fenceHandle, detail::VulkanFenceDeleter{logicalDevice.get()}};

		const VkCommandPoolCreateInfo commandPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = physicalDevice.graphicsQueueFamilyIndex,
		};
		VkCommandPool commandPoolHandle = VK_NULL_HANDLE;
		if (const VkResult result = vkCreateCommandPool(logicalDevice.get(), &commandPoolCreateInfo, nullptr, &commandPoolHandle); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkCreateCommandPool", result};
		}
		submission.commandPool = detail::VulkanCommandPool{commandPoolHandle, detail::VulkanCommandPoolDeleter{logicalDevice.get()}};

		const VkCommandBufferAllocateInfo commandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = submission.commandPool.get(),
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		if (const VkResult result = vkAllocateCommandBuffers(logicalDevice.get(), &commandBufferAllocateInfo, &submission.commandBuffer); result != VK_SUCCESS) {
			throw detail::VulkanError{"vkAllocateCommandBuffers", result};
		}
	}
	const VkCommandBufferBeginInfo commandBufferBeginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	if (const VkResult result = vkBeginCommandBuffer(submission.commandBuffer, &commandBufferBeginInfo); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkBeginCommandBuffer", result};
	}
}

void DeviceImplementation::endGraphicsQueueSubmission() {
	GREM_PROFILE_FUNCTION();

	GraphicsQueueSubmission& submission = graphicsQueueSubmissions.back();
	if (const VkResult result = vkEndCommandBuffer(submission.commandBuffer); result != VK_SUCCESS) {
		throw detail::VulkanError{"vkEndCommandBuffer", result};
	}
	++nextGraphicsQueueSubmissionGenerationIndex;
}

Device::Device(Window& window, const DeviceOptions& options)
	: implementation(UniquePointer<DeviceImplementation>::create(nullptr, window, *this, options)) {}

Device::Device(Filesystem& filesystem, Window& window, const DeviceOptions& options)
	: implementation(UniquePointer<DeviceImplementation>::create(&filesystem, window, *this, options)) {}

Device::~Device() = default;

void Device::blit(TextureRegion2DReference renderTarget, TextureRegion2DConstReference renderSource, TextureFilter filter) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
	GREM_ASSERT(renderSource.texture && *renderSource.texture);
	GREM_ASSERT(renderTarget.region.offset.x >= 0 && renderTarget.region.offset.y >= 0);
	GREM_ASSERT(renderSource.region.offset.x >= 0 && renderSource.region.offset.y >= 0);

	TextureImplementation* targetTexture = renderTarget.texture->get();
	if (targetTexture->type == TextureType::SWAPCHAIN) {
		targetTexture = implementation->acquireSwapchainImage(*targetTexture).get();
		GREM_ASSERT(targetTexture);
		const Extent3D size = targetTexture->size;
		renderTarget.region.offset.x = min(renderTarget.region.offset.x, static_cast<int32_t>(size.width));
		renderTarget.region.offset.y = min(renderTarget.region.offset.y, static_cast<int32_t>(size.height));
		if (static_cast<uint32_t>(renderTarget.region.offset.x) + renderTarget.region.size.width > size.width) {
			renderTarget.region.size.width = size.width - min(static_cast<uint32_t>(renderTarget.region.offset.x), size.width);
		}
		if (static_cast<uint32_t>(renderTarget.region.offset.y) + renderTarget.region.size.height > size.height) {
			renderTarget.region.size.height = size.height - min(static_cast<uint32_t>(renderTarget.region.offset.y), size.height);
		}
	}

	TextureImplementation* sourceTexture = renderSource.texture->get();
	if (sourceTexture->type == TextureType::SWAPCHAIN) {
		sourceTexture = implementation->acquireSwapchainImage(*sourceTexture).get();
		GREM_ASSERT(sourceTexture);
		const Extent3D size = sourceTexture->size;
		renderSource.region.offset.x = min(renderSource.region.offset.x, static_cast<int32_t>(size.width));
		renderSource.region.offset.y = min(renderSource.region.offset.y, static_cast<int32_t>(size.height));
		if (static_cast<uint32_t>(renderSource.region.offset.x) + renderSource.region.size.width > size.width) {
			renderSource.region.size.width = size.width - min(static_cast<uint32_t>(renderSource.region.offset.x), size.width);
		}
		if (static_cast<uint32_t>(renderSource.region.offset.y) + renderSource.region.size.height > size.height) {
			renderSource.region.size.height = size.height - min(static_cast<uint32_t>(renderSource.region.offset.y), size.height);
		}
	}

	GREM_ASSERT(targetTexture->sampleCount == VK_SAMPLE_COUNT_1_BIT);
	GREM_ASSERT(sourceTexture->sampleCount == VK_SAMPLE_COUNT_1_BIT);
	GREM_ASSERT(static_cast<uint32_t>(renderTarget.region.offset.x) + renderTarget.region.size.width <= targetTexture->size.width);
	GREM_ASSERT(static_cast<uint32_t>(renderTarget.region.offset.y) + renderTarget.region.size.height <= targetTexture->size.height);
	GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.x) + renderSource.region.size.width <= sourceTexture->size.width);
	GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.y) + renderSource.region.size.height <= sourceTexture->size.height);
	const VkImageAspectFlags aspectMask =
		TextureImplementation::translateTextureAspects(renderTarget.region.aspects) & TextureImplementation::getAspectMask(targetTexture->format) &
		TextureImplementation::translateTextureAspects(renderSource.region.aspects) & TextureImplementation::getAspectMask(sourceTexture->format);
	if (aspectMask == 0) {
		return;
	}
	targetTexture->transitionToTransferDestinationLayout();
	sourceTexture->transitionToTransferSourceLayout();
	const Array regions{VkImageBlit{
		.srcSubresource{
			.aspectMask = aspectMask,
			.mipLevel = renderSource.region.mipLevel,
			.baseArrayLayer = static_cast<uint32_t>(renderSource.region.offset.z),
			.layerCount = 1,
		},
		.srcOffsets{
			TextureImplementation::translateOffset(Offset3D{.x = renderSource.region.offset.x,
				.y = static_cast<int32_t>(sourceTexture->size.height) - renderSource.region.offset.y - static_cast<int32_t>(renderSource.region.size.height),
				.z = 0}),
			TextureImplementation::translateOffset(Offset3D{
				.x = renderSource.region.offset.x + static_cast<int32_t>(renderSource.region.size.width),
				.y = static_cast<int32_t>(sourceTexture->size.height) - renderSource.region.offset.y,
				.z = 1,
			}),
		},
		.dstSubresource{
			.aspectMask = aspectMask,
			.mipLevel = renderTarget.region.mipLevel,
			.baseArrayLayer = static_cast<uint32_t>(renderTarget.region.offset.z),
			.layerCount = 1,
		},
		.dstOffsets{
			TextureImplementation::translateOffset(Offset3D{.x = renderTarget.region.offset.x,
				.y = static_cast<int32_t>(targetTexture->size.height) - renderTarget.region.offset.y - static_cast<int32_t>(renderTarget.region.size.height),
				.z = 0}),
			TextureImplementation::translateOffset(Offset3D{
				.x = renderTarget.region.offset.x + static_cast<int32_t>(renderTarget.region.size.width),
				.y = static_cast<int32_t>(targetTexture->size.height) - renderTarget.region.offset.y,
				.z = 1,
			}),
		},
	}};
	vkCmdBlitImage(implementation->getGraphicsCommandBuffer(), sourceTexture->object.get<detail::TextureResources>().image, sourceTexture->imageLayout,
		targetTexture->object.get<detail::TextureResources>().image, targetTexture->imageLayout, static_cast<uint32_t>(regions.size()), regions.data(),
		TextureImplementation::translateFilter(filter));
	targetTexture->transitionToPreferredLayout();
	sourceTexture->transitionToPreferredLayout();

	++implementation->currentPresentationSubmission.totalBlitCount;
}

void Device::blit(TextureSubresourceReference renderTarget, Offset2D targetOffset, TextureRegion2DConstReference renderSource) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
	GREM_ASSERT(renderSource.texture && *renderSource.texture);
	GREM_ASSERT(targetOffset.x >= 0 && targetOffset.y >= 0);
	GREM_ASSERT(renderSource.region.offset.x >= 0 && renderSource.region.offset.y >= 0);

	TextureImplementation* targetTexture = renderTarget.texture->get();
	if (targetTexture->type == TextureType::SWAPCHAIN) {
		targetTexture = implementation->acquireSwapchainImage(*targetTexture).get();
		GREM_ASSERT(targetTexture);
		const Extent3D size = targetTexture->size;
		targetOffset.x = min(targetOffset.x, static_cast<int32_t>(size.width));
		targetOffset.y = min(targetOffset.y, static_cast<int32_t>(size.height));
		if (static_cast<uint32_t>(targetOffset.x) + renderSource.region.size.width > size.width) {
			renderSource.region.size.width = size.width - min(static_cast<uint32_t>(targetOffset.x), size.width);
		}
		if (static_cast<uint32_t>(targetOffset.y) + renderSource.region.size.height > size.height) {
			renderSource.region.size.height = size.height - min(static_cast<uint32_t>(targetOffset.y), size.height);
		}
	}

	TextureImplementation* sourceTexture = renderSource.texture->get();
	if (sourceTexture->type == TextureType::SWAPCHAIN) {
		sourceTexture = implementation->acquireSwapchainImage(*sourceTexture).get();
		GREM_ASSERT(sourceTexture);
		GREM_ASSERT(renderSource.region.offset.x >= 0 && renderSource.region.offset.y >= 0);
		const Extent3D size = sourceTexture->size;
		renderSource.region.offset.x = min(renderSource.region.offset.x, static_cast<int32_t>(size.width));
		renderSource.region.offset.y = min(renderSource.region.offset.y, static_cast<int32_t>(size.height));
		if (static_cast<uint32_t>(renderSource.region.offset.x) + renderSource.region.size.width > size.width) {
			renderSource.region.size.width = size.width - min(static_cast<uint32_t>(renderSource.region.offset.x), size.width);
		}
		if (static_cast<uint32_t>(renderSource.region.offset.y) + renderSource.region.size.height > size.height) {
			renderSource.region.size.height = size.height - min(static_cast<uint32_t>(renderSource.region.offset.y), size.height);
		}
	}

	GREM_ASSERT(targetTexture->sampleCount == VK_SAMPLE_COUNT_1_BIT || targetTexture->sampleCount == sourceTexture->sampleCount);
	GREM_ASSERT(static_cast<uint32_t>(targetOffset.x) + renderSource.region.size.width <= targetTexture->size.width);
	GREM_ASSERT(static_cast<uint32_t>(targetOffset.y) + renderSource.region.size.height <= targetTexture->size.height);
	GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.x) + renderSource.region.size.width <= sourceTexture->size.width);
	GREM_ASSERT(static_cast<uint32_t>(renderSource.region.offset.y) + renderSource.region.size.height <= sourceTexture->size.height);
	const VkImageAspectFlags aspectMask =
		TextureImplementation::translateTextureAspects(renderTarget.subresource.aspects) & TextureImplementation::getAspectMask(targetTexture->format) &
		TextureImplementation::translateTextureAspects(renderSource.region.aspects) & TextureImplementation::getAspectMask(sourceTexture->format);
	if (aspectMask == 0) {
		return;
	}
	targetTexture->transitionToTransferDestinationLayout();
	sourceTexture->transitionToTransferSourceLayout();
	if (targetTexture->sampleCount == VK_SAMPLE_COUNT_1_BIT && sourceTexture->sampleCount != VK_SAMPLE_COUNT_1_BIT) {
		const Array regions{VkImageResolve{
			.srcSubresource{
				.aspectMask = aspectMask,
				.mipLevel = renderSource.region.mipLevel,
				.baseArrayLayer = static_cast<uint32_t>(renderSource.region.offset.z),
				.layerCount = 1,
			},
			.srcOffset = TextureImplementation::translateOffset(Offset3D{.x = renderSource.region.offset.x,
				.y = static_cast<int32_t>(sourceTexture->size.height) - renderSource.region.offset.y - static_cast<int32_t>(renderSource.region.size.height),
				.z = 0}),
			.dstSubresource{
				.aspectMask = aspectMask,
				.mipLevel = renderTarget.subresource.mipLevel,
				.baseArrayLayer = renderTarget.subresource.layer,
				.layerCount = 1,
			},
			.dstOffset = TextureImplementation::translateOffset(Offset3D{.x = targetOffset.x,
				.y = static_cast<int32_t>(targetTexture->size.height) - targetOffset.y - static_cast<int32_t>(renderSource.region.size.height),
				.z = 0}),
			.extent = TextureImplementation::translateExtent(static_cast<Extent3D>(renderSource.region.size)),
		}};
		vkCmdResolveImage(implementation->getGraphicsCommandBuffer(), sourceTexture->object.get<detail::TextureResources>().image, sourceTexture->imageLayout,
			targetTexture->object.get<detail::TextureResources>().image, targetTexture->imageLayout, static_cast<uint32_t>(regions.size()), regions.data());
	} else {
		const Array regions{VkImageBlit{
			.srcSubresource{
				.aspectMask = aspectMask,
				.mipLevel = renderSource.region.mipLevel,
				.baseArrayLayer = static_cast<uint32_t>(renderSource.region.offset.z),
				.layerCount = 1,
			},
			.srcOffsets{
				TextureImplementation::translateOffset(Offset3D{.x = renderSource.region.offset.x,
					.y = static_cast<int32_t>(sourceTexture->size.height) - renderSource.region.offset.y - static_cast<int32_t>(renderSource.region.size.height),
					.z = 0}),
				TextureImplementation::translateOffset(Offset3D{
					.x = renderSource.region.offset.x + static_cast<int32_t>(renderSource.region.size.width),
					.y = static_cast<int32_t>(sourceTexture->size.height) - renderSource.region.offset.y,
					.z = 1,
				}),
			},
			.dstSubresource{
				.aspectMask = aspectMask,
				.mipLevel = renderTarget.subresource.mipLevel,
				.baseArrayLayer = renderTarget.subresource.layer,
				.layerCount = 1,
			},
			.dstOffsets{
				TextureImplementation::translateOffset(Offset3D{.x = targetOffset.x,
					.y = static_cast<int32_t>(targetTexture->size.height) - targetOffset.y - static_cast<int32_t>(renderSource.region.size.height),
					.z = 0}),
				TextureImplementation::translateOffset(Offset3D{
					.x = targetOffset.x + static_cast<int32_t>(renderSource.region.size.width),
					.y = static_cast<int32_t>(targetTexture->size.height) - targetOffset.y,
					.z = 1,
				}),
			},
		}};
		GREM_ASSERT(targetTexture->sampleCount == sourceTexture->sampleCount);
		vkCmdBlitImage(implementation->getGraphicsCommandBuffer(), sourceTexture->object.get<detail::TextureResources>().image, sourceTexture->imageLayout,
			targetTexture->object.get<detail::TextureResources>().image, targetTexture->imageLayout, static_cast<uint32_t>(regions.size()), regions.data(), VK_FILTER_NEAREST);
	}
	targetTexture->transitionToPreferredLayout();
	sourceTexture->transitionToPreferredLayout();

	++implementation->currentPresentationSubmission.totalBlitCount;
}

void Device::render(const RenderPass& renderPass) {
	GREM_PROFILE_FUNCTION();

	implementation->currentPresentationSubmission.totalRenderPassStatistics += renderPass.getStatistics();
	++implementation->currentPresentationSubmission.totalRenderPassCount;
	renderRenderPass(*implementation, renderPass.lock());
}

void Device::await() noexcept {
	GREM_PROFILE_FUNCTION();

	implementation->awaitAllCommands();
	implementation->cleanupRenderPassesAvailableForReuse();
	implementation->cleanupExpiredFramebufferContexts();
}

bool Device::awaitPresentation(const Swapchain& swapchain, PresentationSubmissionID presentationSubmissionID, Duration timeout) noexcept {
	GREM_PROFILE_FUNCTION();

	if (!implementation->physicalDevice.supportedFeatures.supportsAwaitPresentation) {
		return false;
	}

	const TextureImplementation::SwapchainImplementation& swapchainImplementation = swapchain.get()->object.get<TextureImplementation::SwapchainImplementation>();
	const uint64_t timeoutNanoseconds = (timeout <= Duration{}) ? Limits<uint64_t>::MAX : static_cast<uint64_t>(duration_cast<Nanoseconds>(timeout).count());
	const TimePoint waitStartTime = Clock::now();
	const bool result = vkWaitForPresentKHR(implementation->logicalDevice.get(), swapchainImplementation.swapchain.get(), static_cast<uint64_t>(presentationSubmissionID),
							timeoutNanoseconds) == VK_SUCCESS;
	const TimePoint waitEndTime = Clock::now();
	implementation->currentPresentationSubmission.totalWaitTime += waitEndTime - waitStartTime;
	return result;
}

Device::PresentationSubmission Device::present(Swapchain& swapchain) {
	GREM_PROFILE_FUNCTION();

	Texture& targetTexture = implementation->acquireSwapchainImage(*swapchain.get());
	targetTexture.get()->transitionToPresentSourceLayout();
	swapchain.get()->latestGraphicsQueueSubmissionUsingThisResource = implementation->nextGraphicsQueueSubmissionGenerationIndex;
	TextureImplementation::SwapchainImplementation& swapchainImplementation = swapchain.get()->object.get<TextureImplementation::SwapchainImplementation>();
	swapchainImplementation.imagePresentationSubmissions.back().graphicsQueueSubmissionGenerationIndex = implementation->nextGraphicsQueueSubmissionGenerationIndex;
	const VkSemaphore imagePresentationSubmittedSemaphore = swapchainImplementation.imagePresentationSubmittedSemaphores[*swapchainImplementation.acquiredImageIndex].get();
	implementation->submitGraphicsCommands(imagePresentationSubmittedSemaphore);

	GREM_ASSERT(implementation->waitSemaphores.empty());
	GREM_ASSERT(implementation->waitDestinationPipelineStages.empty());

	implementation->currentPresentationSubmission.id = static_cast<PresentationSubmissionID>(static_cast<uint64_t>(implementation->currentPresentationSubmission.id) + 1);

	const Array swapchains{swapchainImplementation.swapchain.get()};
	Array presentIDs{static_cast<uint64_t>(implementation->currentPresentationSubmission.id)};
	VkPresentIdKHR presentID{
		.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR,
		.pNext = nullptr,
		.swapchainCount = static_cast<uint32_t>(swapchains.size()),
		.pPresentIds = presentIDs.data(),
	};
	const VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = (implementation->physicalDevice.supportedFeatures.supportsAwaitPresentation) ? &presentID : nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &imagePresentationSubmittedSemaphore,
		.swapchainCount = static_cast<uint32_t>(swapchains.size()),
		.pSwapchains = swapchains.data(),
		.pImageIndices = &*swapchainImplementation.acquiredImageIndex,
		.pResults = nullptr,
	};
	switch (const VkResult result = vkQueuePresentKHR(implementation->presentQueue, &presentInfo)) {
		case VK_SUCCESS:
			if (swapchainImplementation.outOfDate) {
				swapchainImplementation.recreate(swapchain.get()->size);
			} else {
				swapchainImplementation.acquiredImageIndex.reset();
			}
			break;
		case VK_SUBOPTIMAL_KHR: [[fallthrough]];
		case VK_ERROR_OUT_OF_DATE_KHR: swapchainImplementation.recreate(swapchain.get()->size); break;
		default: throw detail::VulkanError{"vkQueuePresent", result};
	}

	implementation->cleanupExpiredFramebufferContexts();
	return std::exchange(implementation->currentPresentationSubmission, Device::PresentationSubmission{.id = implementation->currentPresentationSubmission.id});
}

const FeatureSupport& Device::getSupportedFeatures() const noexcept {
	return implementation->physicalDevice.supportedFeatures;
}

} // namespace grem::graphics
