// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>

#include <utility> // std::move

namespace grem::graphics {

ReflectionProbes3D::ReflectionProbes3D(Device& device, const ReflectionProbes3DOptions& options)
	: ReflectionProbes3D(device, {}, {}, options) {}

ReflectionProbes3D::ReflectionProbes3D(Device& device, Texture reflectionMaps, ArrayList<ReflectionProbeOptions3D> probeOptions, const ReflectionProbes3DOptions& options)
	: options(options)
	, probeOptions(std::move(probeOptions))
	, reflectionMaps(std::move(reflectionMaps))
	, atlasBuffer(device)
	, probeBuffer(device)
	, probesDirty(!this->probeOptions.empty())
	, texturesDirty(!this->probeOptions.empty() && !this->reflectionMaps) {
	GREM_ASSERT(!this->reflectionMaps || (this->reflectionMaps.getType() == TextureType::TEXTURE_CUBE_ARRAY && this->reflectionMaps.getSamplerOptions()));
	GREM_ASSERT(isPowerOf2(options.reflectionMapResolution));

	if (probeOptions.size() > size_t{Limits<uint32_t>::MAX}) {
		throw std::length_error{"Maximum reflection probe count exceeded."};
	}
}

void ReflectionProbes3D::clearReflectionProbes() noexcept {
	reflectionMaps = {};
	probes.clear();
	worldSpaceBoxVolumes.clear();
	probesDirty = false;
	texturesDirty = false;
	buffersDirty = true;

	probeOptions.clear();
}

void ReflectionProbes3D::addReflectionProbe(const ReflectionProbeOptions3D& options) {
	if (probeOptions.size() >= size_t{Limits<uint32_t>::MAX}) {
		throw std::length_error{"Maximum reflection probe count exceeded."};
	}

	reflectionMaps = {};
	probes.clear();
	worldSpaceBoxVolumes.clear();
	probesDirty = true;
	texturesDirty = true;
	buffersDirty = true;

	probeOptions.push_back(options);
}

void ReflectionProbes3D::setReflectionProbes(const ReflectionProbes3DOptions& newOptions) {
	setReflectionProbes({}, {}, newOptions);
}

void ReflectionProbes3D::setReflectionProbes(Texture newReflectionMaps, ArrayList<ReflectionProbeOptions3D> newProbeOptions, const ReflectionProbes3DOptions& newOptions) {
	GREM_ASSERT(!newReflectionMaps || newReflectionMaps.getType() == TextureType::TEXTURE_CUBE_ARRAY);
	GREM_ASSERT(isPowerOf2(newOptions.reflectionMapResolution));

	probesDirty = !newProbeOptions.empty() || !probes.empty();
	texturesDirty = probesDirty && !newReflectionMaps;
	buffersDirty = true;

	probes.clear();
	worldSpaceBoxVolumes.clear();

	options = newOptions;
	probeOptions = std::move(newProbeOptions);
	reflectionMaps = std::move(newReflectionMaps);
}

void ReflectionProbes3D::flushProbesAndTextures(Device& device) const {
	if (probesDirty) {
		probes.clear();
		worldSpaceBoxVolumes.clear();

		if (!probeOptions.empty()) {
			for (const ReflectionProbeOptions3D& probeOptions : this->probeOptions) {
				probes.push_back(ProbeFields{
					.reflectionProbeCenter = probeOptions.center,
					.reflectionProbeOrientation{probeOptions.orientation.x, probeOptions.orientation.y, probeOptions.orientation.z, probeOptions.orientation.w},
					.reflectionProbeSize = probeOptions.size,
					.reflectionProbeLocalAffectedRegionOffset = probeOptions.localAffectedRegionOffset,
					.reflectionProbeLocalAffectedRegionSize = probeOptions.localAffectedRegionSize,
					.reflectionProbeBlendWidthsOnNegativeSides = probeOptions.blendWidthsOnNegativeSides,
					.reflectionProbeBlendWidthsOnPositiveSides = probeOptions.blendWidthsOnPositiveSides,
					.reflectionProbeCaptureOffset = probeOptions.captureOffset,
				});
				worldSpaceBoxVolumes.push_back(product(probeOptions.localAffectedRegionSize));
			}
		}

		if (!texturesDirty) {
			if (reflectionMaps.getWidth() != reflectionMaps.getHeight() || static_cast<size_t>(reflectionMaps.getDepth()) != probeOptions.size() * 6) {
				throw graphics::Error{"Incorrect size of reflection map texture provided for reflection probes."};
			}
		}

		probesDirty = false;
	}

	if (texturesDirty) {
		reflectionMaps = {};

		if (!probeOptions.empty()) {
			const uint32_t resolution = options.reflectionMapResolution;
			const uint32_t maxMipLevelCount = resource::Image::getMaxMipLevelCount(Extent2D{resolution});
			const uint32_t mipLevelCount = maxMipLevelCount - min(maxMipLevelCount - 1, size_t{2}); // Limit to 4x4 pixels as minimum mip resolution.
			reflectionMaps = Texture::create(device, TextureType::TEXTURE_CUBE_ARRAY, TextureFormat::R16G16B16A16_FLOAT,
				Extent3D{resolution, resolution, static_cast<uint32_t>(probeOptions.size() * 6)}, mipLevelCount, ClearValues{}, REFLECTION_MAPS_SAMPLER_OPTIONS);
		}

		texturesDirty = false;
	}
}

void ReflectionProbes3D::flush(Device& device, Renderer3D& renderer3D) const {
	flushProbesAndTextures(device);

	if (buffersDirty) {
		atlasBuffer.upload(AtlasParameters{
			.reflectionProbesReflectionMaps = (reflectionMaps) ? reflectionMaps : renderer3D.getInvisibleTextureCubeArray(),
			.reflectionProbesReflectionMapDetailLevelScale =
				(reflectionMaps) ? static_cast<float>(resource::Image::getMaxMipLevelCount(Extent2D{reflectionMaps.getWidth()}) - 1) : 1.0f,
			.reflectionProbesReflectionMapDetailLevelMax = (reflectionMaps) ? static_cast<float>(reflectionMaps.getMipLevelCount() - 1) : 0.0f,
		});
		probeBuffer.upload(probes);
		buffersDirty = false;
	}
}

} // namespace grem::graphics
