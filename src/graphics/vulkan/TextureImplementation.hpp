// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_VULKAN_TEXTURE_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_VULKAN_TEXTURE_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/resource/Image.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "DeviceImplementation.hpp"
#include "StagingBuffer.hpp"
#include "TextureResources.hpp"
#include "VulkanError.hpp"
#include "objects.hpp"
#include "vulkan.hpp"

#include <utility> // std::in_place_type, std::move, std::swap

namespace grem::graphics {

[[nodiscard]] constexpr bool operator==(const VkComponentMapping& a, const VkComponentMapping& b) noexcept {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct TextureImplementation : detail::ReusableCopyOnWriteResourceBase<TextureImplementation> {
	struct UninitializedTag {};

	[[nodiscard]] static VkSampleCountFlagBits getSampleCount(uint32_t maxMultisampleCount) {
		if (maxMultisampleCount >= 64) {
			return VK_SAMPLE_COUNT_64_BIT;
		}
		if (maxMultisampleCount >= 32) {
			return VK_SAMPLE_COUNT_32_BIT;
		}
		if (maxMultisampleCount >= 16) {
			return VK_SAMPLE_COUNT_16_BIT;
		}
		if (maxMultisampleCount >= 8) {
			return VK_SAMPLE_COUNT_8_BIT;
		}
		if (maxMultisampleCount >= 4) {
			return VK_SAMPLE_COUNT_4_BIT;
		}
		if (maxMultisampleCount >= 2) {
			return VK_SAMPLE_COUNT_2_BIT;
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	[[nodiscard]] static TextureFormat getInternalFormat(VkFormat format) noexcept {
		switch (format) {
			case VK_FORMAT_R8_UNORM: return TextureFormat::R8_UNORM;
			case VK_FORMAT_R16_SFLOAT: return TextureFormat::R16_FLOAT;
			case VK_FORMAT_R32_SFLOAT: return TextureFormat::R32_FLOAT;
			case VK_FORMAT_R8G8_UNORM: return TextureFormat::R8G8_UNORM;
			case VK_FORMAT_R16G16_SFLOAT: return TextureFormat::R16G16_FLOAT;
			case VK_FORMAT_R32G32_SFLOAT: return TextureFormat::R32G32_FLOAT;
			case VK_FORMAT_R8G8B8A8_UNORM: return TextureFormat::R8G8B8A8_UNORM;
			case VK_FORMAT_R8G8B8A8_SRGB: return TextureFormat::R8G8B8A8_SRGB;
			case VK_FORMAT_R16G16B16A16_SFLOAT: return TextureFormat::R16G16B16A16_FLOAT;
			case VK_FORMAT_R32G32B32A32_SFLOAT: return TextureFormat::R32G32B32A32_FLOAT;
			case VK_FORMAT_D16_UNORM: return TextureFormat::D16_UNORM;
			case VK_FORMAT_D32_SFLOAT: return TextureFormat::D32_FLOAT;
			case VK_FORMAT_D24_UNORM_S8_UINT: return TextureFormat::D24_UNORM_S8_UINT;
			case VK_FORMAT_D32_SFLOAT_S8_UINT: return TextureFormat::D32_FLOAT_S8_UINT;
			case VK_FORMAT_R5G6B5_UNORM_PACK16: return TextureFormat::R5G6B5_UNORM_PACK16;
			case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return TextureFormat::A1R5G5B5_UNORM_PACK16;
			case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return TextureFormat::B10G11R11_UFLOAT_PACK32;
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return TextureFormat::A2B10G10R10_UNORM_PACK32;
			case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK;
			case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK;
			case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return TextureFormat::BC1_RGB_UNORM_BLOCK;
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return TextureFormat::BC1_RGB_SRGB_BLOCK;
			case VK_FORMAT_BC3_UNORM_BLOCK: return TextureFormat::BC3_RGBA_UNORM_BLOCK;
			case VK_FORMAT_BC3_SRGB_BLOCK: return TextureFormat::BC3_RGBA_SRGB_BLOCK;
			case VK_FORMAT_BC4_UNORM_BLOCK: return TextureFormat::BC4_R_UNORM_BLOCK;
			case VK_FORMAT_BC5_UNORM_BLOCK: return TextureFormat::BC5_RG_UNORM_BLOCK;
			case VK_FORMAT_BC6H_UFLOAT_BLOCK: return TextureFormat::BC6H_RGB_UFLOAT_BLOCK;
			case VK_FORMAT_BC6H_SFLOAT_BLOCK: return TextureFormat::BC6H_RGB_FLOAT_BLOCK;
			case VK_FORMAT_BC7_UNORM_BLOCK: return TextureFormat::BC7_RGBA_UNORM_BLOCK;
			case VK_FORMAT_BC7_SRGB_BLOCK: return TextureFormat::BC7_RGBA_SRGB_BLOCK;
			case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return TextureFormat::ETC2_R8G8B8_UNORM_BLOCK;
			case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return TextureFormat::ETC2_R8G8B8_SRGB_BLOCK;
			case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK;
			case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK;
			case VK_FORMAT_EAC_R11_UNORM_BLOCK: return TextureFormat::EAC_R11_UNORM_BLOCK;
			case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return TextureFormat::EAC_R11G11_UNORM_BLOCK;
			case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK;
			case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK;
			default: break;
		}
		return TextureFormat::UNKNOWN;
	}

	[[nodiscard]] static VkImageAspectFlags getAspectMask(VkFormat format) {
		switch (format) {
			case VK_FORMAT_R8_UNORM: [[fallthrough]];
			case VK_FORMAT_R8_SNORM: [[fallthrough]];
			case VK_FORMAT_R8_USCALED: [[fallthrough]];
			case VK_FORMAT_R8_SSCALED: [[fallthrough]];
			case VK_FORMAT_R8_UINT: [[fallthrough]];
			case VK_FORMAT_R8_SINT: [[fallthrough]];
			case VK_FORMAT_R8_SRGB: [[fallthrough]];
			case VK_FORMAT_R8G8_UNORM: [[fallthrough]];
			case VK_FORMAT_R8G8_SNORM: [[fallthrough]];
			case VK_FORMAT_R8G8_USCALED: [[fallthrough]];
			case VK_FORMAT_R8G8_SSCALED: [[fallthrough]];
			case VK_FORMAT_R8G8_UINT: [[fallthrough]];
			case VK_FORMAT_R8G8_SINT: [[fallthrough]];
			case VK_FORMAT_R8G8_SRGB: [[fallthrough]];
			case VK_FORMAT_R8G8B8_UNORM: [[fallthrough]];
			case VK_FORMAT_R8G8B8_SNORM: [[fallthrough]];
			case VK_FORMAT_R8G8B8_USCALED: [[fallthrough]];
			case VK_FORMAT_R8G8B8_SSCALED: [[fallthrough]];
			case VK_FORMAT_R8G8B8_UINT: [[fallthrough]];
			case VK_FORMAT_R8G8B8_SINT: [[fallthrough]];
			case VK_FORMAT_R8G8B8_SRGB: [[fallthrough]];
			case VK_FORMAT_B8G8R8_UNORM: [[fallthrough]];
			case VK_FORMAT_B8G8R8_SNORM: [[fallthrough]];
			case VK_FORMAT_B8G8R8_USCALED: [[fallthrough]];
			case VK_FORMAT_B8G8R8_SSCALED: [[fallthrough]];
			case VK_FORMAT_B8G8R8_UINT: [[fallthrough]];
			case VK_FORMAT_B8G8R8_SINT: [[fallthrough]];
			case VK_FORMAT_B8G8R8_SRGB: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_UNORM: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_SNORM: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_USCALED: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_SSCALED: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_UINT: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_SINT: [[fallthrough]];
			case VK_FORMAT_R8G8B8A8_SRGB: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_UNORM: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_SNORM: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_USCALED: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_SSCALED: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_UINT: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_SINT: [[fallthrough]];
			case VK_FORMAT_B8G8R8A8_SRGB: [[fallthrough]];
			case VK_FORMAT_R16_UNORM: [[fallthrough]];
			case VK_FORMAT_R16_SNORM: [[fallthrough]];
			case VK_FORMAT_R16_USCALED: [[fallthrough]];
			case VK_FORMAT_R16_SSCALED: [[fallthrough]];
			case VK_FORMAT_R16_UINT: [[fallthrough]];
			case VK_FORMAT_R16_SINT: [[fallthrough]];
			case VK_FORMAT_R16_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R16G16_UNORM: [[fallthrough]];
			case VK_FORMAT_R16G16_SNORM: [[fallthrough]];
			case VK_FORMAT_R16G16_USCALED: [[fallthrough]];
			case VK_FORMAT_R16G16_SSCALED: [[fallthrough]];
			case VK_FORMAT_R16G16_UINT: [[fallthrough]];
			case VK_FORMAT_R16G16_SINT: [[fallthrough]];
			case VK_FORMAT_R16G16_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R16G16B16_UNORM: [[fallthrough]];
			case VK_FORMAT_R16G16B16_SNORM: [[fallthrough]];
			case VK_FORMAT_R16G16B16_USCALED: [[fallthrough]];
			case VK_FORMAT_R16G16B16_SSCALED: [[fallthrough]];
			case VK_FORMAT_R16G16B16_UINT: [[fallthrough]];
			case VK_FORMAT_R16G16B16_SINT: [[fallthrough]];
			case VK_FORMAT_R16G16B16_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_UNORM: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_SNORM: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_USCALED: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_SSCALED: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_UINT: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_SINT: [[fallthrough]];
			case VK_FORMAT_R16G16B16A16_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R32_UINT: [[fallthrough]];
			case VK_FORMAT_R32_SINT: [[fallthrough]];
			case VK_FORMAT_R32_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R32G32_UINT: [[fallthrough]];
			case VK_FORMAT_R32G32_SINT: [[fallthrough]];
			case VK_FORMAT_R32G32_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R32G32B32_UINT: [[fallthrough]];
			case VK_FORMAT_R32G32B32_SINT: [[fallthrough]];
			case VK_FORMAT_R32G32B32_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R32G32B32A32_UINT: [[fallthrough]];
			case VK_FORMAT_R32G32B32A32_SINT: [[fallthrough]];
			case VK_FORMAT_R32G32B32A32_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R64_UINT: [[fallthrough]];
			case VK_FORMAT_R64_SINT: [[fallthrough]];
			case VK_FORMAT_R64_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R64G64_UINT: [[fallthrough]];
			case VK_FORMAT_R64G64_SINT: [[fallthrough]];
			case VK_FORMAT_R64G64_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R64G64B64_UINT: [[fallthrough]];
			case VK_FORMAT_R64G64B64_SINT: [[fallthrough]];
			case VK_FORMAT_R64G64B64_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R64G64B64A64_UINT: [[fallthrough]];
			case VK_FORMAT_R64G64B64A64_SINT: [[fallthrough]];
			case VK_FORMAT_R64G64B64A64_SFLOAT: [[fallthrough]];
			case VK_FORMAT_R4G4_UNORM_PACK8: [[fallthrough]];
			case VK_FORMAT_R4G4B4A4_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_B4G4R4A4_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_R5G6B5_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_B5G6R5_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_R5G5B5A1_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_B5G5R5A1_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_A1R5G5B5_UNORM_PACK16: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_UNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_SNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_USCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_UINT_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_SINT_PACK32: [[fallthrough]];
			case VK_FORMAT_A8B8G8R8_SRGB_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_UNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_SNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_USCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_UINT_PACK32: [[fallthrough]];
			case VK_FORMAT_A2R10G10B10_SINT_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_SNORM_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_USCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_UINT_PACK32: [[fallthrough]];
			case VK_FORMAT_A2B10G10R10_SINT_PACK32: [[fallthrough]];
			case VK_FORMAT_B10G11R11_UFLOAT_PACK32: [[fallthrough]];
			case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: [[fallthrough]];
			case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC1_RGB_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC3_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC3_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC4_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC5_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC6H_UFLOAT_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC6H_SFLOAT_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC7_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_BC7_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: [[fallthrough]];
			case VK_FORMAT_EAC_R11_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: [[fallthrough]];
			case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: [[fallthrough]];
			case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return VK_IMAGE_ASPECT_COLOR_BIT;
			case VK_FORMAT_D16_UNORM: [[fallthrough]];
			case VK_FORMAT_D32_SFLOAT: return VK_IMAGE_ASPECT_DEPTH_BIT;
			case VK_FORMAT_S8_UINT: return VK_IMAGE_ASPECT_STENCIL_BIT;
			case VK_FORMAT_D16_UNORM_S8_UINT: [[fallthrough]];
			case VK_FORMAT_D24_UNORM_S8_UINT: [[fallthrough]];
			case VK_FORMAT_D32_SFLOAT_S8_UINT: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			default: break;
		}
		return {};
	}

	[[nodiscard]] static VkOffset2D translateOffset(Offset2D offset) noexcept {
		return {.x = offset.x, .y = offset.y};
	}

	[[nodiscard]] static VkOffset3D translateOffset(Offset3D offset) noexcept {
		return {.x = offset.x, .y = offset.y, .z = offset.z};
	}

	[[nodiscard]] static VkExtent2D translateExtent(Extent2D size) noexcept {
		return {.width = size.width, .height = size.height};
	}

	[[nodiscard]] static VkExtent3D translateExtent(Extent3D size) noexcept {
		return {.width = size.width, .height = size.height, .depth = size.depth};
	}

	[[nodiscard]] static VkRect2D translateRegion(Region2D rectangle, Extent2D framebufferSize) noexcept {
		VkRect2D result{
			.offset = translateOffset(rectangle.offset),
			.extent = translateExtent(rectangle.size),
		};
		result.offset.y = static_cast<int32_t>(framebufferSize.height) - static_cast<int32_t>(result.extent.height) - result.offset.y;
		if (result.offset.x < 0) {
			result.extent.width -= min(static_cast<uint32_t>(-result.offset.x), result.extent.width);
			result.offset.x = 0;
		}
		if (result.offset.y < 0) {
			result.extent.height -= min(static_cast<uint32_t>(-result.offset.y), result.extent.height);
			result.offset.y = 0;
		}
		return result;
	}

	[[nodiscard]] static VkImageAspectFlags translateTextureAspects(TextureAspects aspects) noexcept {
		VkImageAspectFlags result{};
		if (aspects.contains(TextureAspect::COLOR)) {
			result |= VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (aspects.contains(TextureAspect::DEPTH)) {
			result |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (aspects.contains(TextureAspect::STENCIL)) {
			result |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		return result;
	}

	[[nodiscard]] static VkFormat translateInternalFormat(TextureFormat internalFormat) noexcept {
		switch (internalFormat) {
			case TextureFormat::UNKNOWN: return VK_FORMAT_UNDEFINED;
			case TextureFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
			case TextureFormat::R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
			case TextureFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
			case TextureFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
			case TextureFormat::R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
			case TextureFormat::R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
			case TextureFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
			case TextureFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
			case TextureFormat::R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
			case TextureFormat::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case TextureFormat::D16_UNORM: return VK_FORMAT_D16_UNORM;
			case TextureFormat::D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
			case TextureFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
			case TextureFormat::D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
			case TextureFormat::R5G6B5_UNORM_PACK16: return VK_FORMAT_R5G6B5_UNORM_PACK16;
			case TextureFormat::A1R5G5B5_UNORM_PACK16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
			case TextureFormat::B10G11R11_UFLOAT_PACK32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			case TextureFormat::A2B10G10R10_UNORM_PACK32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
			case TextureFormat::ASTC_4x4_RGBA_UNORM_BLOCK: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
			case TextureFormat::ASTC_4x4_RGBA_SRGB_BLOCK: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
			case TextureFormat::BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			case TextureFormat::BC1_RGB_SRGB_BLOCK: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
			case TextureFormat::BC3_RGBA_UNORM_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK;
			case TextureFormat::BC3_RGBA_SRGB_BLOCK: return VK_FORMAT_BC3_SRGB_BLOCK;
			case TextureFormat::BC4_R_UNORM_BLOCK: return VK_FORMAT_BC4_UNORM_BLOCK;
			case TextureFormat::BC5_RG_UNORM_BLOCK: return VK_FORMAT_BC5_UNORM_BLOCK;
			case TextureFormat::BC6H_RGB_UFLOAT_BLOCK: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case TextureFormat::BC6H_RGB_FLOAT_BLOCK: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
			case TextureFormat::BC7_RGBA_UNORM_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK;
			case TextureFormat::BC7_RGBA_SRGB_BLOCK: return VK_FORMAT_BC7_SRGB_BLOCK;
			case TextureFormat::ETC2_R8G8B8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
			case TextureFormat::ETC2_R8G8B8_SRGB_BLOCK: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
			case TextureFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			case TextureFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
			case TextureFormat::EAC_R11_UNORM_BLOCK: return VK_FORMAT_EAC_R11_UNORM_BLOCK;
			case TextureFormat::EAC_R11G11_UNORM_BLOCK: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
			case TextureFormat::PVRTC1_4BPP_RGBA_UNORM_BLOCK: return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
			case TextureFormat::PVRTC1_4BPP_RGBA_SRGB_BLOCK: return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
		}
		return {};
	}

	[[nodiscard]] static VkFilter translateFilter(TextureFilter filter) noexcept {
		switch (filter) {
			case TextureFilter::NEAREST: return VK_FILTER_NEAREST;
			case TextureFilter::LINEAR: return VK_FILTER_LINEAR;
		}
		return {};
	}

	[[nodiscard]] static VkSamplerMipmapMode translateMipmapMode(TextureMipmapMode mipmapMode) noexcept {
		switch (mipmapMode) {
			case TextureMipmapMode::NONE: [[fallthrough]];
			case TextureMipmapMode::NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case TextureMipmapMode::LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		return {};
	}

	[[nodiscard]] static VkSamplerAddressMode translateWrappingMode(TextureWrappingMode wrappingMode) noexcept {
		switch (wrappingMode) {
			case TextureWrappingMode::REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case TextureWrappingMode::MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case TextureWrappingMode::CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		}
		return {};
	}

	[[nodiscard]] static VkCompareOp translateDepthComparisonMode(TextureDepthComparisonMode depthComparisonMode) noexcept {
		switch (depthComparisonMode) {
			case TextureDepthComparisonMode::NEVER_PASS: return VK_COMPARE_OP_NEVER;
			case TextureDepthComparisonMode::LESS: return VK_COMPARE_OP_LESS;
			case TextureDepthComparisonMode::LESS_OR_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
			case TextureDepthComparisonMode::GREATER: return VK_COMPARE_OP_GREATER;
			case TextureDepthComparisonMode::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
			case TextureDepthComparisonMode::EQUAL: return VK_COMPARE_OP_EQUAL;
			case TextureDepthComparisonMode::NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
			case TextureDepthComparisonMode::ALWAYS_PASS: return VK_COMPARE_OP_ALWAYS;
		}
		return {};
	}

	struct SwapchainImplementation {
		struct ImagePresentationSubmission {
			detail::VulkanSemaphore imageAcquiredSemaphore{};
			DeviceImplementation::GraphicsQueueSubmissionGenerationIndex graphicsQueueSubmissionGenerationIndex = 0;
		};

		Device* device;
		Window* window;
		SwapchainOptions options;
		bool isVerticalSynchronizationEnabled = false;
		detail::VulkanSwapchain swapchain{};
		Texture multisampledColorBuffer{};
		Texture depthStencilBuffer{};
		Allocation<Texture> images{};
		Optional<uint32_t> acquiredImageIndex{};
		bool outOfDate = false;
		Allocation<detail::VulkanSemaphore> imagePresentationSubmittedSemaphores{};
		RingBuffer<ImagePresentationSubmission> imagePresentationSubmissions{};

		SwapchainImplementation(Extent3D& outputSize, Device& device, Window& window, const SwapchainOptions& options, VkSwapchainKHR oldSwapchain)
			: device(&device)
			, window(&window)
			, options(options) {
			GREM_PROFILE_FUNCTION();

			if (options.maxBufferedFrameCount > 8) {
				throw graphics::Error{"Invalid maximum buffered frame count."};
			}

			const DeviceImplementation::PhysicalDevice& physicalDevice = device.get()->physicalDevice;
			const VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(window.getSurface());
			const VkDevice deviceHandle = device.get()->logicalDevice.get();

			VkSurfaceCapabilitiesKHR surfaceCapabilities{};
			if (const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.handle, surface, &surfaceCapabilities); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkGetPhysicalDeviceSurfaceCapabilities", result};
			}
			Extent2D size = {.width = surfaceCapabilities.currentExtent.width, .height = surfaceCapabilities.currentExtent.height};
			if (size.width == 0 || size.height == 0 || (size.width == 0xFFFFFFFF && size.height == 0xFFFFFFFF)) {
				const Extent2D screenSize = window.getDrawableSize();
				const uint32_t minWidth = (surfaceCapabilities.minImageExtent.width > 0) ? surfaceCapabilities.minImageExtent.width : 1;
				const uint32_t minHeight = (surfaceCapabilities.minImageExtent.height > 0) ? surfaceCapabilities.minImageExtent.height : 1;
				const uint32_t maxWidth = (surfaceCapabilities.maxImageExtent.width > 0) ? surfaceCapabilities.maxImageExtent.width : Limits<uint32_t>::MAX;
				const uint32_t maxHeight = (surfaceCapabilities.maxImageExtent.height > 0) ? surfaceCapabilities.maxImageExtent.height : Limits<uint32_t>::MAX;
				size.width = clamp(screenSize.width, minWidth, maxWidth);
				size.height = clamp(screenSize.height, minHeight, maxHeight);
			}

			const uint32_t minImageCount =
				(surfaceCapabilities.maxImageCount > 0)
					? clamp(static_cast<uint32_t>(2 + options.maxBufferedFrameCount), surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount)
					: max(static_cast<uint32_t>(2 + options.maxBufferedFrameCount), surfaceCapabilities.minImageCount);

			const VkFormat colorFormat = physicalDevice.surfaceFormat.format;
			const VkColorSpaceKHR colorSpace = physicalDevice.surfaceFormat.colorSpace;
			const VkFormat depthStencilFormat = physicalDevice.depthStencilFormat;
			const uint32_t maxMultisampleCount = clamp(window.getMultisampleCount(), uint32_t{1}, physicalDevice.supportedFeatures.maxSupportedMultisampleCount);
			const VkSampleCountFlagBits sampleCount = getSampleCount(maxMultisampleCount);
			const VkComponentMapping components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			};
			if (sampleCount != VK_SAMPLE_COUNT_1_BIT) {
				multisampledColorBuffer =
					Texture{TextureImplementation::create(device, TextureType::RENDERBUFFER, colorFormat, size, 1, maxMultisampleCount, sampleCount, components, {})};
			}
			depthStencilBuffer =
				Texture{TextureImplementation::create(device, TextureType::RENDERBUFFER, depthStencilFormat, size, 1, maxMultisampleCount, sampleCount, components, {})};

			VkSwapchainKHR swapchainHandle = VK_NULL_HANDLE;
			uint32_t presentModeCount = 0;
			if (const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.handle, surface, &presentModeCount, nullptr); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkGetPhysicalDeviceSurfacePresentModes", result};
			}
			Allocation<VkPresentModeKHR> presentModes(static_cast<size_t>(presentModeCount));
			if (const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.handle, surface, &presentModeCount, presentModes.data()); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkGetPhysicalDeviceSurfacePresentModes", result};
			}
			VkPresentModeKHR presentMode = (options.useVerticalSynchronization) ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
			if (!contains(presentModes, presentMode)) {
				presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
				if (!contains(presentModes, presentMode)) {
					presentMode = VK_PRESENT_MODE_FIFO_KHR;
				}
			}
			isVerticalSynchronizationEnabled = presentMode != VK_PRESENT_MODE_IMMEDIATE_KHR;
			const bool exclusive = physicalDevice.graphicsQueueFamilyIndex == physicalDevice.presentQueueFamilyIndex;
			InplaceBuffer<uint32_t, 2> queueFamilyIndices{};
			if (!exclusive) {
				queueFamilyIndices = {
					physicalDevice.graphicsQueueFamilyIndex,
					physicalDevice.presentQueueFamilyIndex,
				};
			}
			const VkSwapchainCreateInfoKHR swapchainCreateInfo{
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.pNext = nullptr,
				.flags = VkSwapchainCreateFlagsKHR{},
				.surface = surface,
				.minImageCount = minImageCount,
				.imageFormat = colorFormat,
				.imageColorSpace = colorSpace,
				.imageExtent = translateExtent(size),
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.imageSharingMode = (exclusive) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
				.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
				.pQueueFamilyIndices = queueFamilyIndices.data(),
				.preTransform = ((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0)
			                        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
			                        : surfaceCapabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = presentMode,
				.clipped = VK_TRUE,
				.oldSwapchain = oldSwapchain,
			};
			if (const VkResult result = vkCreateSwapchainKHR(deviceHandle, &swapchainCreateInfo, nullptr, &swapchainHandle); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateSwapchain", result};
			}
			swapchain = detail::VulkanSwapchain{swapchainHandle, detail::VulkanSwapchainDeleter{deviceHandle}};

			uint32_t imageCount = 0;
			if (const VkResult result = vkGetSwapchainImagesKHR(deviceHandle, swapchain.get(), &imageCount, nullptr); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkGetSwapchainImages", result};
			}
			Allocation<VkImage> imageHandles(static_cast<size_t>(imageCount));
			if (const VkResult result = vkGetSwapchainImagesKHR(deviceHandle, swapchain.get(), &imageCount, imageHandles.data()); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkGetSwapchainImages", result};
			}
			images.resize(imageHandles.size());
			for (size_t i = 0; i < imageHandles.size(); ++i) {
				images[i] = Texture{createSwapchainImageProxy(device, imageHandles[i], colorFormat, size)};
			}

			imagePresentationSubmittedSemaphores.resize(imageHandles.size());
			for (detail::VulkanSemaphore& semaphore : imagePresentationSubmittedSemaphores) {
				const VkSemaphoreCreateInfo semaphoreCreateInfo{
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
					.pNext = nullptr,
					.flags = VkSemaphoreCreateFlags{},
				};
				VkSemaphore semaphoreHandle = VK_NULL_HANDLE;
				if (const VkResult result = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr, &semaphoreHandle); result != VK_SUCCESS) {
					throw detail::VulkanError{"vkCreateSemaphore", result};
				}
				semaphore = detail::VulkanSemaphore{semaphoreHandle, detail::VulkanSemaphoreDeleter{deviceHandle}};
			}

			outputSize = size;
		}

		~SwapchainImplementation() {
			if (device) {
				device->get()->awaitAllCommandsNoexcept();
			}
		}

		SwapchainImplementation(const SwapchainImplementation&) = delete;
		SwapchainImplementation(SwapchainImplementation&&) = delete;
		SwapchainImplementation& operator=(const SwapchainImplementation&) = delete;

		SwapchainImplementation& operator=(SwapchainImplementation&& other) noexcept {
			if (this == &other) {
				return *this;
			}
			using std::swap;
			swap(device, other.device);
			swap(window, other.window);
			swap(options, other.options);
			swap(isVerticalSynchronizationEnabled, other.isVerticalSynchronizationEnabled);
			swap(swapchain, other.swapchain);
			swap(multisampledColorBuffer, other.multisampledColorBuffer);
			swap(depthStencilBuffer, other.depthStencilBuffer);
			swap(images, other.images);
			swap(acquiredImageIndex, other.acquiredImageIndex);
			swap(outOfDate, other.outOfDate);
			swap(imagePresentationSubmittedSemaphores, other.imagePresentationSubmittedSemaphores);
			swap(imagePresentationSubmissions, other.imagePresentationSubmissions);
			return *this;
		}

		void recreate(Extent3D& outputSize) {
			*this = SwapchainImplementation{outputSize, *device, *window, SwapchainOptions{options}, swapchain.get()};
		}

		void acquireNextImage(Extent3D& outputSize) {
			GREM_PROFILE_FUNCTION();

			const VkDevice deviceHandle = device->get()->logicalDevice.get();
			while (!acquiredImageIndex) {
				ImagePresentationSubmission& imagePresentationSubmission = imagePresentationSubmissions.push_back_unspecified_value();
				if (!imagePresentationSubmission.imageAcquiredSemaphore) {
					const VkSemaphoreCreateInfo semaphoreCreateInfo{
						.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
						.pNext = nullptr,
						.flags = VkSemaphoreCreateFlags{},
					};
					VkSemaphore semaphoreHandle = VK_NULL_HANDLE;
					if (const VkResult result = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr, &semaphoreHandle); result != VK_SUCCESS) {
						imagePresentationSubmissions.pop_back();
						throw detail::VulkanError{"vkCreateSemaphore", result};
					}
					imagePresentationSubmission.imageAcquiredSemaphore = detail::VulkanSemaphore{semaphoreHandle, detail::VulkanSemaphoreDeleter{deviceHandle}};
				}
				imagePresentationSubmission.graphicsQueueSubmissionGenerationIndex = 0;
				uint32_t imageIndex{};
				switch (const VkResult result = vkAcquireNextImageKHR(deviceHandle, swapchain.get(), Limits<uint64_t>::MAX,
							imagePresentationSubmission.imageAcquiredSemaphore.get(), VK_NULL_HANDLE, &imageIndex)) {
					case VK_SUBOPTIMAL_KHR: outOfDate = true; [[fallthrough]];
					case VK_SUCCESS:
						acquiredImageIndex = imageIndex;
						if (*acquiredImageIndex >= images.size()) {
							throw graphics::Error{"Invalid image index."};
						}
						break;
					case VK_ERROR_OUT_OF_DATE_KHR:
						imagePresentationSubmissions.pop_back();
						recreate(outputSize);
						break;
					default: imagePresentationSubmissions.pop_back(); throw detail::VulkanError{"vkAcquireNextImage", result};
				}
			}
		}
	};

	[[nodiscard]] static SharedPointer<TextureImplementation> createSwapchain(Device& device, Window& window, const SwapchainOptions& options, VkSwapchainKHR oldSwapchain) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(device, window, options, oldSwapchain);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> createSwapchainImageProxy(Device& device, VkImage image, VkFormat format, Extent2D size) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(device, image, format, size);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> create(Device& device, TextureType type, VkFormat format, Extent3D size, uint32_t mipLevelCount,
		uint32_t maxMultisampleCount, VkSampleCountFlagBits sampleCount, const VkComponentMapping& components, Optional<TextureSamplerOptions> samplerOptions) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(device, type, format, size, mipLevelCount, maxMultisampleCount, sampleCount, components, samplerOptions);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressed(const TextureImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressedWithSamplerOptions(const TextureImplementation& implementation,
		Optional<TextureSamplerOptions> newSamplerOptions) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation, newSamplerOptions);
	}

	[[nodiscard]] static SharedPointer<TextureImplementation> cloneUncompressedUninitialized(const TextureImplementation& implementation) {
		GREM_PROFILE_FUNCTION();
		return SharedPointer<TextureImplementation>::create(implementation, UninitializedTag{});
	}

	VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	TextureType type;
	VkFormat format;
	Extent3D size;
	uint32_t mipLevelCount;
	uint32_t maxMultisampleCount;
	VkSampleCountFlagBits sampleCount;
	VkComponentMapping components;
	Optional<TextureSamplerOptions> samplerOptions;
	mutable DeviceImplementation::GraphicsQueueSubmissionGenerationIndex latestGraphicsQueueSubmissionUsingThisResource = DeviceImplementation::NOT_IN_USE;
	Variant<detail::TextureResources, SwapchainImplementation> object;

	TextureImplementation(Device& device, Window& window, const SwapchainOptions& options, VkSwapchainKHR oldSwapchain)
		: type(TextureType::SWAPCHAIN)
		, format(VK_FORMAT_UNDEFINED)
		, size{.width = 0, .height = 0}
		, mipLevelCount(1)
		, maxMultisampleCount(clamp(window.getMultisampleCount(), uint32_t{1}, device.get()->physicalDevice.supportedFeatures.maxSupportedMultisampleCount))
		, sampleCount(VK_SAMPLE_COUNT_1_BIT)
		, components{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		}
		, samplerOptions()
		, object(std::in_place_type<SwapchainImplementation>, size, device, window, options, oldSwapchain) {}

	TextureImplementation(Device& device, VkImage image, VkFormat format, Extent2D size)
		: type(TextureType::SWAPCHAIN)
		, format(format)
		, size(size)
		, mipLevelCount(1)
		, maxMultisampleCount(1)
		, sampleCount(VK_SAMPLE_COUNT_1_BIT)
		, components{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		}
		, samplerOptions()
		, object(std::in_place_type<detail::TextureResources>, device) {
		detail::TextureResources& resources = object.as<detail::TextureResources>();

		resources.image = image;
	}

	TextureImplementation(Device& device, TextureType type, VkFormat format, Extent3D size, uint32_t mipLevelCount, uint32_t maxMultisampleCount, VkSampleCountFlagBits sampleCount,
		const VkComponentMapping& components, Optional<TextureSamplerOptions> samplerOptions)
		: type(type)
		, format(format)
		, size(size)
		, maxMultisampleCount(maxMultisampleCount)
		, mipLevelCount(mipLevelCount)
		, sampleCount(sampleCount)
		, components(components)
		, samplerOptions(samplerOptions)
		, object(std::in_place_type<detail::TextureResources>, device) {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(type != TextureType::SWAPCHAIN);

		const VkDevice deviceHandle = device.get()->logicalDevice.get();
		const VmaAllocator allocator = device.get()->allocator.get();
		detail::TextureResources& resources = object.as<detail::TextureResources>();

		VkImageCreateFlags createFlags{};
		switch (type) {
			case TextureType::EMPTY: [[fallthrough]];
			case TextureType::SWAPCHAIN: unreachable();
			case TextureType::TEXTURE_2D: [[fallthrough]];
			case TextureType::TEXTURE_2D_ARRAY: [[fallthrough]];
			case TextureType::RENDERBUFFER: break;
			case TextureType::TEXTURE_CUBE: [[fallthrough]];
			case TextureType::TEXTURE_CUBE_ARRAY: createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
		}

		const TextureFormat internalFormat = getInternalFormat(format);
		const VkImageAspectFlags aspectMask = getAspectMask(format);
		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (!Texture::isCompressedFormat(internalFormat)) {
			usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (Texture::isFramebufferCompatibleFormat(internalFormat) || internalFormat == TextureFormat::UNKNOWN) {
			if (const Extent2D maxFramebufferSize = device.getSupportedFeatures().maxFramebufferSize;
				size.width <= maxFramebufferSize.width && size.height <= maxFramebufferSize.height) {
				if ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
					usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				}
				if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
					usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				}
			}
		}

		if (samplerOptions) {
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		VmaAllocationCreateFlags allocationCreateFlags{};
		if (type == TextureType::RENDERBUFFER) {
			allocationCreateFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		} else {
			allocationCreateFlags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
		}

		const VkImageCreateInfo imageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = createFlags,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent{.width = size.width, .height = size.height, .depth = 1},
			.mipLevels = mipLevelCount,
			.arrayLayers = size.depth,
			.samples = sampleCount,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		const VmaAllocationCreateInfo allocationCreateInfo{
			.flags = allocationCreateFlags,
			.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.preferredFlags = VkMemoryPropertyFlags{},
			.memoryTypeBits = 0,
			.pool = VK_NULL_HANDLE,
			.pUserData = nullptr,
			.priority = 0.0f,
		};
		if (const VkResult result = vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &resources.image, &resources.imageAllocation, nullptr);
			result != VK_SUCCESS) {
			throw detail::VulkanError{"vmaCreateImage", result};
		}
#ifndef NDEBUG
		const char* name = nullptr;
		switch (type) {
			case TextureType::EMPTY: [[fallthrough]];
			case TextureType::SWAPCHAIN: unreachable();
			case TextureType::TEXTURE_2D: name = "Texture (TEXTURE_2D)"; break;
			case TextureType::TEXTURE_2D_ARRAY: name = "Texture (TEXTURE_2D_ARRAY)"; break;
			case TextureType::RENDERBUFFER: name = "Texture (RENDERBUFFER)"; break;
			case TextureType::TEXTURE_CUBE: name = "Texture (TEXTURE_CUBE)"; break;
			case TextureType::TEXTURE_CUBE_ARRAY: name = "Texture (TEXTURE_CUBE_ARRAY)"; break;
		}
		vmaSetAllocationName(allocator, resources.imageAllocation, name);
#endif

		VkImageViewType viewType{};
		switch (type) {
			case TextureType::EMPTY: [[fallthrough]];
			case TextureType::SWAPCHAIN: unreachable();
			case TextureType::TEXTURE_2D: [[fallthrough]];
			case TextureType::RENDERBUFFER: viewType = VK_IMAGE_VIEW_TYPE_2D; break;
			case TextureType::TEXTURE_2D_ARRAY: viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
			case TextureType::TEXTURE_CUBE: viewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
			case TextureType::TEXTURE_CUBE_ARRAY: viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; break;
		}

		if (samplerOptions) {
			Optional<VkImageViewASTCDecodeModeEXT> astcDecodeMode{};
			if ((format == VK_FORMAT_ASTC_4x4_UNORM_BLOCK || format == VK_FORMAT_ASTC_4x4_SRGB_BLOCK) && device.getSupportedFeatures().supportsASTCDecodeMode) {
				astcDecodeMode = VkImageViewASTCDecodeModeEXT{
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT,
					.pNext = nullptr,
					.decodeMode = VK_FORMAT_R8G8B8A8_UNORM,
				};
			}
			const VkImageViewCreateInfo imageViewCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = (astcDecodeMode) ? &*astcDecodeMode : nullptr,
				.flags = VkImageViewCreateFlags{},
				.image = resources.image,
				.viewType = viewType,
				.format = format,
				.components = components,
				.subresourceRange{
					.aspectMask = aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT),
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			};
			if (const VkResult result = vkCreateImageView(deviceHandle, &imageViewCreateInfo, nullptr, &resources.samplerImageView); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateImageView", result};
			}

			const VkSamplerCreateInfo samplerCreateInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.pNext = nullptr,
				.flags = VkSamplerCreateFlags{},
				.magFilter = translateFilter(samplerOptions->magnificationFilter),
				.minFilter = translateFilter(samplerOptions->minificationFilter),
				.mipmapMode = translateMipmapMode(samplerOptions->mipmapMode),
				.addressModeU = translateWrappingMode(samplerOptions->horizontalWrappingMode),
				.addressModeV = translateWrappingMode(samplerOptions->verticalWrappingMode),
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.mipLodBias = 0.0f,
				.anisotropyEnable = (samplerOptions->maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE,
				.maxAnisotropy = samplerOptions->maxAnisotropy,
				.compareEnable = samplerOptions->depthComparisonMode.has_value(),
				.compareOp = (samplerOptions->depthComparisonMode) ? translateDepthComparisonMode(*samplerOptions->depthComparisonMode) : VK_COMPARE_OP_NEVER,
				.minLod = 0.0f,
				.maxLod = (samplerOptions->mipmapMode == TextureMipmapMode::NONE) ? 0.25f : VK_LOD_CLAMP_NONE,
				.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
				.unnormalizedCoordinates = VK_FALSE,
			};
			if (const VkResult result = vkCreateSampler(deviceHandle, &samplerCreateInfo, nullptr, &resources.sampler); result != VK_SUCCESS) {
				throw detail::VulkanError{"vkCreateSampler", result};
			}
		}
	}

	~TextureImplementation() {
		if (detail::TextureResources* const resources = object.get_if<detail::TextureResources>()) {
			if (latestGraphicsQueueSubmissionUsingThisResource != DeviceImplementation::NOT_IN_USE) {
				DeviceImplementation& device = *resources->device.get();
				device.adoptTextureResources(latestGraphicsQueueSubmissionUsingThisResource, std::move(*resources));
			}
		}
	}

	TextureImplementation(const TextureImplementation& other)
		: TextureImplementation(other, other.samplerOptions) {}

	TextureImplementation(const TextureImplementation& other, Optional<TextureSamplerOptions> newSamplerOptions)
		: TextureImplementation(other.object.get<detail::TextureResources>().device, other.type, other.format, other.size, other.mipLevelCount, other.maxMultisampleCount,
			  other.sampleCount, other.components, newSamplerOptions) {
		assignFromOtherUncompressedTextureOfSameShape(other);
	}

	TextureImplementation(const TextureImplementation& other, UninitializedTag)
		: TextureImplementation(other.object.get<detail::TextureResources>().device, other.type, other.format, other.size, other.mipLevelCount, other.maxMultisampleCount,
			  other.sampleCount, other.components, other.samplerOptions) {}

	TextureImplementation(TextureImplementation&&) = delete;

	TextureImplementation& operator=(const TextureImplementation& other) = delete;
	TextureImplementation& operator=(TextureImplementation&&) = delete;

	void assignFromOtherUncompressedTextureOfSameShape(const TextureImplementation& other) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const detail::TextureResources& otherResources = other.object.get<detail::TextureResources>();

		GREM_ASSERT(&resources.device == &otherResources.device);
		GREM_ASSERT(type == other.type && format == other.format && size == other.size && mipLevelCount == other.mipLevelCount && sampleCount == other.sampleCount &&
					components == other.components);
		if (other.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
			return;
		}

		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = other.imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		SmallBuffer<VkImageCopy, 15> regions{};
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
			regions.push_back(VkImageCopy{
				.srcSubresource{
					.aspectMask = aspectMask,
					.mipLevel = mipLevel,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.srcOffset{.x = 0, .y = 0, .z = 0},
				.dstSubresource{
					.aspectMask = aspectMask,
					.mipLevel = mipLevel,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.dstOffset{.x = 0, .y = 0, .z = 0},
				.extent{.width = mipLevelSize.width, .height = mipLevelSize.height, .depth = 1},
			});
		}
		vkCmdCopyImage(commandBuffer, otherResources.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(regions.size()), regions.data());

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const Array postCopyImageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.newLayout = preferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = preferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postCopyImageMemoryBarriers.size()), postCopyImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
		other.latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void assignFirstMipLevelFromOtherUncompressedTextureOfSameShapeAndGenerateMipmap(const TextureImplementation& other) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const detail::TextureResources& otherResources = other.object.get<detail::TextureResources>();

		GREM_ASSERT(&resources.device == &otherResources.device);
		GREM_ASSERT(type == other.type && format == other.format && size == other.size && sampleCount == other.sampleCount && components == other.components);
		if (other.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
			return;
		}

		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = other.imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		const Array regions{
			VkImageCopy{
				.srcSubresource{
					.aspectMask = aspectMask,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.srcOffset{.x = 0, .y = 0, .z = 0},
				.dstSubresource{
					.aspectMask = aspectMask,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.dstOffset{.x = 0, .y = 0, .z = 0},
				.extent{.width = size.width, .height = size.height, .depth = 1},
			},
		};
		vkCmdCopyImage(commandBuffer, otherResources.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(regions.size()), regions.data());

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const Array postCopyImageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.newLayout = preferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postCopyImageMemoryBarriers.size()), postCopyImageMemoryBarriers.data());

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
		other.latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

		generateMipChainAndTransitionFromTransferDestinationToPreferredLayout();
	}

	void transitionToTransferDestinationLayout() {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		if (imageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			return;
		}
		const Array imageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = getAspectMask(format),
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(resources.device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0,
			nullptr, 0, nullptr, static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
		imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void transitionToTransferSourceLayout() {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		if (imageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			return;
		}
		const Array imageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = getAspectMask(format),
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(resources.device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0,
			nullptr, 0, nullptr, static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
		imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	struct PreferredLayoutInfo {
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_NONE;
		VkAccessFlags accessMask = VK_ACCESS_NONE;
	};

	[[nodiscard]] PreferredLayoutInfo getPreferredLayoutInfo() const {
		const VkImageAspectFlags aspectMask = getAspectMask(format);
		PreferredLayoutInfo result{};

		if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
			result.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			result.stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			result.accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		} else if ((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) {
			result.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			result.stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			result.accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		} else if ((aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) {
			result.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
			result.stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			result.accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		} else {
			result.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			result.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			result.accessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (samplerOptions) {
			result.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			result.stageMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			result.accessMask |= VK_ACCESS_SHADER_READ_BIT;
		}

		return result;
	}

	void transitionToPreferredLayout() {
		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		if (imageLayout == preferredImageLayout) {
			return;
		}
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const Array imageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = imageLayout,
				.newLayout = preferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = getAspectMask(format),
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(resources.device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void transitionToPresentSourceLayout() {
		if (imageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			return;
		}
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		const VkAccessFlags dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		const Array imageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = getAspectMask(format),
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(resources.device.get()->getGraphicsCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
		imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void generateMipChainAndTransitionFromTransferDestinationToPreferredLayout() {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);
		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();

		for (uint32_t mipLevel = 1; mipLevel < mipLevelCount; ++mipLevel) {
			const uint32_t previousMipLevel = mipLevel - 1;
			const Extent2D previousMipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, previousMipLevel);
			const int32_t previousMipWidth = static_cast<int32_t>(previousMipLevelSize.width);
			const int32_t previousMipHeight = static_cast<int32_t>(previousMipLevelSize.height);
			const Extent2D mipLevelSize = resource::Image::getMipLevelSize2D(Extent2D{size.width, size.height}, mipLevel);
			const int32_t mipWidth = static_cast<int32_t>(mipLevelSize.width);
			const int32_t mipHeight = static_cast<int32_t>(mipLevelSize.height);

			const Array preBlitImageMemoryBarriers{VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = previousMipLevel,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
				static_cast<uint32_t>(preBlitImageMemoryBarriers.size()), preBlitImageMemoryBarriers.data());

			const Array imageBlitRegions{VkImageBlit{
				.srcSubresource{
					.aspectMask = aspectMask,
					.mipLevel = previousMipLevel,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.srcOffsets{
					{.x = 0, .y = 0, .z = 0},
					{.x = previousMipWidth, .y = previousMipHeight, .z = 1},
				},
				.dstSubresource{
					.aspectMask = aspectMask,
					.mipLevel = mipLevel,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
				.dstOffsets{
					{.x = 0, .y = 0, .z = 0},
					{.x = mipWidth, .y = mipHeight, .z = 1},
				},
			}};
			vkCmdBlitImage(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(imageBlitRegions.size()), imageBlitRegions.data(), VK_FILTER_LINEAR);

			const Array postBlitImageMemoryBarriers{VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = dstAccessMask,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.newLayout = preferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = previousMipLevel,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
				static_cast<uint32_t>(postBlitImageMemoryBarriers.size()), postBlitImageMemoryBarriers.data());
		}

		const Array postBlitImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = dstAccessMask,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = preferredImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = mipLevelCount - 1,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postBlitImageMemoryBarriers.size()), postBlitImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void uploadFirstMipLevelAndGenerateMipmap(const void* pixels) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		DeviceImplementation& device = *resources.device.get();

		const size_t sizeInBytes = resource::Image::getMipLevelStride(Texture::getImageFormat(getInternalFormat(format)), size);
		detail::StagingBuffer stagingBuffer = device.acquireStagingBuffer(sizeInBytes);
		if (sizeInBytes > 0) {
			memcpy(stagingBuffer.data(), pixels, sizeInBytes);
		}
		stagingBuffer.flush(0, sizeInBytes);

		const VkCommandBuffer commandBuffer = device.getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		const Array imageCopyRegions{VkBufferImageCopy{
			.bufferOffset = 0,
			.bufferRowLength = size.width,
			.bufferImageHeight = size.height,
			.imageSubresource{
				.aspectMask = aspectMask,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
			.imageOffset{.x = 0, .y = 0, .z = 0},
			.imageExtent{.width = size.width, .height = size.height, .depth = 1},
		}};
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.get(), resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(imageCopyRegions.size()),
			imageCopyRegions.data());

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

		generateMipChainAndTransitionFromTransferDestinationToPreferredLayout();

		device.submitStagingBuffer(std::move(stagingBuffer));
	}

	void uploadAllMipLevels(const void* pixels) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		DeviceImplementation& device = *resources.device.get();

		const resource::ImageFormat imageFormat = Texture::getImageFormat(getInternalFormat(format));
		const size_t sizeInBytes = resource::Image::getSizeInBytes(imageFormat, size, mipLevelCount);
		detail::StagingBuffer stagingBuffer = device.acquireStagingBuffer(sizeInBytes);
		if (sizeInBytes > 0) {
			memcpy(stagingBuffer.data(), pixels, sizeInBytes);
		}
		stagingBuffer.flush(0, sizeInBytes);

		const VkCommandBuffer commandBuffer = device.getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		VkDeviceSize bufferOffset = 0;
		SmallBuffer<VkBufferImageCopy, 15> regions{};
		for (uint32_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
			const Extent3D mipLevelSize = resource::Image::getMipLevelSize3D(size, mipLevel);
			const size_t mipLevelStride = resource::Image::getMipLevelStride(imageFormat, mipLevelSize);
			regions.push_back(VkBufferImageCopy{
				.bufferOffset = bufferOffset,
				.bufferRowLength = mipLevelSize.width,
				.bufferImageHeight = mipLevelSize.height,
				.imageSubresource{
					.aspectMask = aspectMask,
					.mipLevel = mipLevel,
					.baseArrayLayer = 0,
					.layerCount = mipLevelSize.depth,
				},
				.imageOffset{.x = 0, .y = 0, .z = 0},
				.imageExtent{.width = mipLevelSize.width, .height = mipLevelSize.height, .depth = 1},
			});
			bufferOffset += mipLevelStride;
		}
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.get(), resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const Array postCopyImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = dstAccessMask,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = preferredImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postCopyImageMemoryBarriers.size()), postCopyImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

		device.submitStagingBuffer(std::move(stagingBuffer));
	}

	void pasteImageOntoFirstMipLevelAndGenerateMipmap(Offset3D destinationOffset, Extent3D sourceExtent, FunctionView<void(byte* output, size_t sizeBytes)> writeData) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		DeviceImplementation& device = *resources.device.get();

		const size_t sizeInBytes = resource::Image::getMipLevelStride(Texture::getImageFormat(getInternalFormat(format)), sourceExtent);
		detail::StagingBuffer stagingBuffer = device.acquireStagingBuffer(sizeInBytes);
		writeData(stagingBuffer.data(), sizeInBytes);
		stagingBuffer.flush(0, sizeInBytes);

		const VkCommandBuffer commandBuffer = device.getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		const Array imageCopyRegions{VkBufferImageCopy{
			.bufferOffset = 0,
			.bufferRowLength = sourceExtent.width,
			.bufferImageHeight = sourceExtent.height,
			.imageSubresource{
				.aspectMask = aspectMask,
				.mipLevel = 0,
				.baseArrayLayer = static_cast<uint32_t>(destinationOffset.z),
				.layerCount = sourceExtent.depth,
			},
			.imageOffset{.x = destinationOffset.x, .y = static_cast<int32_t>(size.height) - destinationOffset.y - static_cast<int32_t>(sourceExtent.height), .z = 0},
			.imageExtent{.width = sourceExtent.width, .height = sourceExtent.height, .depth = 1},
		}};
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.get(), resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(imageCopyRegions.size()),
			imageCopyRegions.data());

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

		generateMipChainAndTransitionFromTransferDestinationToPreferredLayout();

		device.submitStagingBuffer(std::move(stagingBuffer));
	}

	void pasteImageOntoFirstMipLevelAndGenerateMipmap(Extent3D sourceSize, const void* pixels, Offset3D destinationOffset, Region3D sourceRegion) {
		pasteImageOntoFirstMipLevelAndGenerateMipmap(destinationOffset, sourceRegion.size, [&](byte* output, size_t sizeBytes) -> void {
			if (sizeBytes > 0) {
				if (sourceRegion.offset == Offset3D{} && sourceRegion.size == sourceSize) {
					memcpy(output, pixels, sizeBytes);
				} else {
					const size_t pixelStride = resource::Image::getPixelStride(Texture::getImageFormat(getInternalFormat(format)));
					resource::Image::copyPixels(sourceRegion.size, output, Offset3D{}, sourceSize, static_cast<const byte*>(pixels), sourceRegion, pixelStride);
				}
			}
		});
	}

	void pasteTextureOntoFirstMipLevelAndGenerateMipmap(TextureImplementation& source, Offset3D destinationOffset, Region3D sourceRegion) {
		GREM_ASSERT(format == source.format);
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const detail::TextureResources& otherResources = source.object.get<detail::TextureResources>();
		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();
		const VkImageAspectFlags aspectMask = getAspectMask(format);

		const Array preCopyImageMemoryBarriers{
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = source.imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = static_cast<uint32_t>(sourceRegion.offset.z),
					.layerCount = sourceRegion.size.depth,
				},
			},
			VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = imageLayout,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = resources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = mipLevelCount,
					.baseArrayLayer = 0,
					.layerCount = size.depth,
				},
			},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preCopyImageMemoryBarriers.size()), preCopyImageMemoryBarriers.data());

		const Array imageBlitRegions{VkImageBlit{
			.srcSubresource{
				.aspectMask = aspectMask,
				.mipLevel = 0,
				.baseArrayLayer = static_cast<uint32_t>(sourceRegion.offset.z),
				.layerCount = sourceRegion.size.depth,
			},
			.srcOffsets{
				{.x = sourceRegion.offset.x, .y = static_cast<int32_t>(source.size.height) - sourceRegion.offset.y - static_cast<int32_t>(sourceRegion.size.height), .z = 0},
				{.x = sourceRegion.offset.x + static_cast<int32_t>(sourceRegion.size.width), .y = static_cast<int32_t>(source.size.height) - sourceRegion.offset.y, .z = 1},
			},
			.dstSubresource{
				.aspectMask = aspectMask,
				.mipLevel = 0,
				.baseArrayLayer = static_cast<uint32_t>(destinationOffset.z),
				.layerCount = sourceRegion.size.depth,
			},
			.dstOffsets{
				{.x = destinationOffset.x, .y = static_cast<int32_t>(size.height) - destinationOffset.y - static_cast<int32_t>(sourceRegion.size.height), .z = 0},
				{.x = destinationOffset.x + static_cast<int32_t>(sourceRegion.size.width), .y = static_cast<int32_t>(size.height) - destinationOffset.y, .z = 1},
			},
		}};
		vkCmdBlitImage(commandBuffer, otherResources.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(imageBlitRegions.size()), imageBlitRegions.data(), VK_FILTER_NEAREST);

		const auto [otherPreferredImageLayout, otherDstStageMask, otherDstAccessMask] = source.getPreferredLayoutInfo();
		if (mipLevelCount > 1) {
			const Array postCopyImageMemoryBarriers{VkImageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.dstAccessMask = otherDstAccessMask,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.newLayout = otherPreferredImageLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = otherResources.image,
				.subresourceRange{
					.aspectMask = aspectMask,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = static_cast<uint32_t>(sourceRegion.offset.z),
					.layerCount = sourceRegion.size.depth,
				},
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, otherDstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
				static_cast<uint32_t>(postCopyImageMemoryBarriers.size()), postCopyImageMemoryBarriers.data());
			source.imageLayout = otherPreferredImageLayout;

			latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
			source.latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

			generateMipChainAndTransitionFromTransferDestinationToPreferredLayout();
		} else {
			const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
			const Array postCopyImageMemoryBarriers{
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.dstAccessMask = dstAccessMask,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.newLayout = otherPreferredImageLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = otherResources.image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = static_cast<uint32_t>(sourceRegion.offset.z),
						.layerCount = sourceRegion.size.depth,
					},
				},
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = otherDstAccessMask,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = preferredImageLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = resources.image,
					.subresourceRange{
						.aspectMask = aspectMask,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = size.depth,
					},
				},
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, otherDstStageMask | dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
				static_cast<uint32_t>(postCopyImageMemoryBarriers.size()), postCopyImageMemoryBarriers.data());
			source.imageLayout = otherPreferredImageLayout;
			imageLayout = preferredImageLayout;

			latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
			source.latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
		}
	}

	void fillTexture(const ClearValues& values) {
		const VkImageAspectFlags aspectMask = getAspectMask(format) & translateTextureAspects(values.aspects);
		if (aspectMask == VkImageAspectFlags{}) {
			return;
		}

		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();

		const Array preFillImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preFillImageMemoryBarriers.size()), preFillImageMemoryBarriers.data());

		if ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
			const vec4 clearColor = values.color.toLinearRGBA();
			const VkClearColorValue clearColorValue{.float32{clearColor.x, clearColor.y, clearColor.z, clearColor.w}};
			const Array clearRanges{VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			}};
			vkCmdClearColorImage(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, static_cast<uint32_t>(clearRanges.size()),
				clearRanges.data());
		}
		if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
			const VkClearDepthStencilValue clearDepthStencilValue{
				.depth = values.depth,
				.stencil = static_cast<uint32_t>(values.stencil),
			};
			const Array clearRanges{VkImageSubresourceRange{
				.aspectMask = aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			}};
			vkCmdClearDepthStencilImage(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepthStencilValue, static_cast<uint32_t>(clearRanges.size()),
				clearRanges.data());
		}

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const Array postFillImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = dstAccessMask,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = preferredImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = mipLevelCount,
				.baseArrayLayer = 0,
				.layerCount = size.depth,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postFillImageMemoryBarriers.size()), postFillImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void fillTextureSubresource(TextureSubresource subresource, const ClearValues& values) {
		const VkImageAspectFlags aspectMask = getAspectMask(format) & translateTextureAspects(values.aspects) & translateTextureAspects(subresource.aspects);
		if (aspectMask == VkImageAspectFlags{}) {
			return;
		}

		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();

		const Array preFillImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = subresource.layer,
				.layerCount = 1,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preFillImageMemoryBarriers.size()), preFillImageMemoryBarriers.data());

		if ((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
			const vec4 clearColor = values.color.toLinearRGBA();
			const VkClearColorValue clearColorValue{.float32{clearColor.x, clearColor.y, clearColor.z, clearColor.w}};
			const Array clearRanges{VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = subresource.layer,
				.layerCount = 1,
			}};
			vkCmdClearColorImage(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, static_cast<uint32_t>(clearRanges.size()),
				clearRanges.data());
		}
		if ((aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
			const VkClearDepthStencilValue clearDepthStencilValue{
				.depth = values.depth,
				.stencil = static_cast<uint32_t>(values.stencil),
			};
			const Array clearRanges{VkImageSubresourceRange{
				.aspectMask = aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
				.baseMipLevel = subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = subresource.layer,
				.layerCount = 1,
			}};
			vkCmdClearDepthStencilImage(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepthStencilValue, static_cast<uint32_t>(clearRanges.size()),
				clearRanges.data());
		}

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const Array postFillImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = dstAccessMask,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = preferredImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange{
				.aspectMask = aspectMask,
				.baseMipLevel = subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = subresource.layer,
				.layerCount = 1,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postFillImageMemoryBarriers.size()), postFillImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;
	}

	void downloadColor(uint32_t layer, uint32_t mipLevel, void* pixels) {
		detail::TextureResources& resources = object.get<detail::TextureResources>();
		const VmaAllocator allocator = resources.device.get()->allocator.get();
		const Extent2D layerSize = resource::Image::getMipLevelSize2D(Extent2D{.width = size.width, .height = size.height}, mipLevel);
		const size_t layerStride = resource::Image::getLayerStride(Texture::getImageFormat(getInternalFormat(format)), layerSize);
		if (layerStride == 0) {
			return;
		}

		const VkBufferCreateInfo bufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkBufferCreateFlags{},
			.size = static_cast<VkDeviceSize>(layerStride),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		const VmaAllocationCreateInfo allocationCreateInfo{
			.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
			.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			.preferredFlags = VkMemoryPropertyFlags{},
			.memoryTypeBits = 0,
			.pool = VK_NULL_HANDLE,
			.pUserData = nullptr,
			.priority = 0.0f,
		};
		VkBuffer bufferHandle = VK_NULL_HANDLE;
		VmaAllocation allocationHandle = VK_NULL_HANDLE;
		VmaAllocationInfo allocationInfo{};
		if (const VkResult result = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &bufferHandle, &allocationHandle, &allocationInfo); result != VK_SUCCESS) {
			throw detail::VulkanError{"vmaCreateBuffer", result};
		}
#ifndef NDEBUG
		vmaSetAllocationName(allocator, allocationHandle, "DownloadBuffer");
#endif
		const detail::VulkanBuffer buffer{bufferHandle, detail::VulkanBufferDeleter{allocator, allocationHandle}};
		void* const mappedData = allocationInfo.pMappedData;
		GREM_ASSERT(mappedData);

		const VkCommandBuffer commandBuffer = resources.device.get()->getGraphicsCommandBuffer();

		const auto [preferredImageLayout, dstStageMask, dstAccessMask] = getPreferredLayoutInfo();
		const VkImageSubresourceRange layoutTransitionSubresourceRange = (imageLayout == preferredImageLayout) ? VkImageSubresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = mipLevel,
			.levelCount = 1,
			.baseArrayLayer = layer,
			.layerCount = 1,
		} : VkImageSubresourceRange{
			.aspectMask = getAspectMask(format),
			.baseMipLevel = 0,
			.levelCount = mipLevelCount,
			.baseArrayLayer = 0,
			.layerCount = size.depth,
		};

		const Array preReadImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = imageLayout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange = layoutTransitionSubresourceRange,
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(preReadImageMemoryBarriers.size()), preReadImageMemoryBarriers.data());

		const Array imageCopyRegions{VkBufferImageCopy{
			.bufferOffset = 0,
			.bufferRowLength = layerSize.width,
			.bufferImageHeight = layerSize.height,
			.imageSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = mipLevel,
				.baseArrayLayer = layer,
				.layerCount = 1,
			},
			.imageOffset{.x = 0, .y = 0, .z = 0},
			.imageExtent{.width = layerSize.width, .height = layerSize.height, .depth = 1},
		}};
		vkCmdCopyImageToBuffer(commandBuffer, resources.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.get(), static_cast<uint32_t>(imageCopyRegions.size()),
			imageCopyRegions.data());

		const Array postReadImageMemoryBarriers{VkImageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = dstAccessMask,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout = preferredImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = resources.image,
			.subresourceRange = layoutTransitionSubresourceRange,
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, VkDependencyFlags{}, 0, nullptr, 0, nullptr,
			static_cast<uint32_t>(postReadImageMemoryBarriers.size()), postReadImageMemoryBarriers.data());
		imageLayout = preferredImageLayout;

		latestGraphicsQueueSubmissionUsingThisResource = resources.device.get()->nextGraphicsQueueSubmissionGenerationIndex;

		resources.device.get()->submitAndAwaitGraphicsCommands();

		memcpy(pixels, mappedData, layerStride);
	}
};

} // namespace grem::graphics

#endif
