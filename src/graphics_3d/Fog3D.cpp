// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Fog3D.hpp>

namespace grem::graphics {

Fog3D::Fog3D(Device& device, const Fog3DOptions& options)
	: options(options)
	, parameterBuffer(device) {}

void Fog3D::setFog(const Fog3DOptions& newFogOptions) {
	GREM_ASSERT(newFogOptions.startDistance <= newFogOptions.endDistance);
	options = newFogOptions;
	parameterBufferDirty = true;
}

void Fog3D::flush() const {
	if (parameterBufferDirty) {
		parameterBuffer.upload(Parameters{
			.fogStartAndEndDistancesAndSkyFadeMinAndMaxAngleCosines{options.startDistance, options.endDistance, cos(options.skyFadeMinAngle), cos(options.skyFadeMaxAngle)},
			.fogColorAndMaxDensity = options.color.toLinearRGBA(),
			.fogSkyFadeDirection = options.skyFadeDirection,
		});
		parameterBufferDirty = false;
	}
}

} // namespace grem::graphics
