// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Color.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>

namespace grem::graphics {

Sky3D::Sky3D(Device& device, const Sky3DOptions& options)
	: Sky3D(device, {}, {}, {}, options) {}

Sky3D::Sky3D(Device& device, Texture radianceMap, Texture irradianceMap, Texture reflectionMap, const Sky3DOptions& options)
	: options(options)
	, radianceMap(std::move(radianceMap))
	, irradianceMap(std::move(irradianceMap))
	, reflectionMap(std::move(reflectionMap))
	, parameterBuffer(device) {
	GREM_ASSERT(options.radianceMapResolution == 0 || isPowerOf2(options.radianceMapResolution));
	GREM_ASSERT(options.irradianceMapResolution == 0 || isPowerOf2(options.irradianceMapResolution));
	GREM_ASSERT(options.reflectionMapResolution == 0 || isPowerOf2(options.reflectionMapResolution));
}

void Sky3D::setSky(const Sky3DOptions& newOptions) {
	setSky({}, {}, {}, newOptions);
}

void Sky3D::setSky(Texture newRadianceMap, Texture newIrradianceMap, Texture newReflectionMap, const Sky3DOptions& newOptions) {
	GREM_ASSERT(newOptions.radianceMapResolution == 0 || isPowerOf2(newOptions.radianceMapResolution));
	GREM_ASSERT(newOptions.irradianceMapResolution == 0 || isPowerOf2(newOptions.irradianceMapResolution));
	GREM_ASSERT(newOptions.reflectionMapResolution == 0 || isPowerOf2(newOptions.reflectionMapResolution));

	options = newOptions;
	radianceMap = std::move(newRadianceMap);
	irradianceMap = std::move(newIrradianceMap);
	reflectionMap = std::move(newReflectionMap);

	parameterBufferDirty = true;
}

void Sky3D::flush(Renderer3D& renderer3D) const {
	if (parameterBufferDirty) {
		parameterBuffer.upload(Parameters{
			.skyRadianceMap = (radianceMap) ? radianceMap : renderer3D.getInvisibleTextureCube(),
			.skyIrradianceMap = (irradianceMap) ? irradianceMap : renderer3D.getInvisibleTextureCube(),
			.skyReflectionMap = (reflectionMap) ? reflectionMap : renderer3D.getInvisibleTextureCube(),
			.skyColor = options.color.toLinearRGBA(),
			.skyAmbientColor = options.ambientColor.toLinearRGBA(),
			.skyReflectionColor = options.reflectionColor.toLinearRGBA(),
			.skyReflectionMapDetailLevelScale = (reflectionMap) ? static_cast<float>(resource::Image::getMaxMipLevelCount(Extent2D{reflectionMap.getWidth()}) - 1) : 1.0f,
			.skyReflectionMapDetailLevelMax = (reflectionMap) ? static_cast<float>(reflectionMap.getMipLevelCount() - 1) : 0.0f,
		});
		parameterBufferDirty = false;
	}
}

} // namespace grem::graphics
