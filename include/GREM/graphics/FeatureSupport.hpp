// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_FEATURE_SUPPORT_HPP
#define GREM_GRAPHICS_FEATURE_SUPPORT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::graphics {

/**
 * Declarations of special features supported by a Device.
 */
struct FeatureSupport {
	/** Name of the active video driver, e.g. "windows", "x11" or "wayland". */
	CStringView videoDriverName{};

	/** Name of the active graphics backend API, e.g. "OpenGL", "WebGL" or "Vulkan". */
	CStringView graphicsBackendAPIName{};

	/** Name of the active graphics backend API version, e.g. "3.3 Core", "ES 3.0", "2.0" or "1.2". */
	CStringView graphicsBackendAPIVersionName{};

	/** Maximum allowed 2D texture width or height, in texels. */
	uint32_t max2DTextureResolution = 16384;

	/** Maximum allowed cube texture width or height, in texels. */
	uint32_t maxCubeTextureResolution = 16384;

	/** Maximum allowed number of array texture layers. */
	uint32_t maxTextureLayerCount = 2048;

	/** Maximum allowed framebuffer size, in texels. */
	Extent2D maxFramebufferSize{16384, 16384};

	/** Maximum supported level of multisampling of renderbuffer textures. */
	uint32_t maxSupportedMultisampleCount = 1;

	/** Maximum supported level of anisotropic filtering of sampled textures. */
	float maxSupportedSamplerAnisotropy = 1.0f;

	/** Whether ASTC LDR texture compression is supported or not. */
	bool supportsTextureCompressionASTC_LDR = false;

	/** Whether specifying the decode mode is supported for ASTC texture compression or not. */
	bool supportsASTCDecodeMode = false;

	/** Whether DXT1/DXT5 texture compression is supported or not. */
	bool supportsTextureCompressionS3TC = false;

	/** Whether DXT1/DXT5 SRGB texture compression is supported or not. */
	bool supportsTextureCompressionS3TC_SRGB = false;

	/** Whether RGTC texture compression is supported or not. */
	bool supportsTextureCompressionRGTC = false;

	/** Whether BPTC texture compression is supported or not. */
	bool supportsTextureCompressionBPTC = false;

	/** Whether ETC2 texture compression is supported or not. */
	bool supportsTextureCompressionETC2 = false;

	/** Whether PVRTC texture compression is supported or not. */
	bool supportsTextureCompressionPVRTC = false;

	/** Whether PVRTC SRGB texture compression is supported or not. */
	bool supportsTextureCompressionPVRTC_SRGB = false;

	/** Whether shaders can be compiled directly from GLSL source code or not. */
	bool supportsGLSLShaderCode = false;

	/** Whether shaders can be created directly from SPIR-V code or not. */
	bool supportsSPIRVShaderCode = false;

	/** Whether Device::awaitPresentation() is supported or not. */
	bool supportsAwaitPresentation = false;
};

} // namespace grem::graphics

#endif
