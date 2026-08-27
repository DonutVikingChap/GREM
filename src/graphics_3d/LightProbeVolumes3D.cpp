// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/ArrayAtlasPacker.hpp>

#include <utility> // std::move

namespace grem::graphics {

LightProbeVolumes3D::LightProbeVolumes3D(Device& device, const LightProbeVolumes3DOptions& options)
	: LightProbeVolumes3D(device, {}, {}, {}, options) {}

LightProbeVolumes3D::LightProbeVolumes3D(Device& device, Texture irradianceAtlasTexture, Texture distanceAtlasTexture, ArrayList<LightProbeVolumeOptions3D> volumeOptions,
	const LightProbeVolumes3DOptions& options)
	: options(options)
	, volumeOptions(std::move(volumeOptions))
	, irradianceAtlasTexture(std::move(irradianceAtlasTexture))
	, distanceAtlasTexture(std::move(distanceAtlasTexture))
	, irradianceAtlasResolution(this->irradianceAtlasTexture.getWidth())
	, irradianceAtlasDepth(this->irradianceAtlasTexture.getDepth())
	, distanceAtlasResolution(this->distanceAtlasTexture.getWidth())
	, distanceAtlasDepth(this->distanceAtlasTexture.getDepth())
	, atlasBuffer(device)
	, volumeBuffer(device)
	, volumesDirty(!this->volumeOptions.empty())
	, texturesDirty(!this->volumeOptions.empty() && (!this->irradianceAtlasTexture || !this->distanceAtlasTexture)) {
	GREM_ASSERT(!this->irradianceAtlasTexture || (this->irradianceAtlasTexture.getType() == TextureType::TEXTURE_2D_ARRAY && this->irradianceAtlasTexture.getSamplerOptions()));
	GREM_ASSERT(!this->distanceAtlasTexture || (this->distanceAtlasTexture.getType() == TextureType::TEXTURE_2D_ARRAY && this->distanceAtlasTexture.getSamplerOptions()));
	GREM_ASSERT(this->irradianceAtlasTexture.getWidth() == this->irradianceAtlasTexture.getHeight());
	GREM_ASSERT(this->distanceAtlasTexture.getWidth() == this->distanceAtlasTexture.getHeight());

	if (volumeOptions.size() > size_t{Limits<uint32_t>::MAX}) {
		throw std::length_error{"Maximum light probe volume count exceeded."};
	}
}

void LightProbeVolumes3D::clearLightProbeVolumes() noexcept {
	irradianceAtlasTexture = {};
	distanceAtlasTexture = {};
	irradianceAtlasResolution = 0;
	irradianceAtlasDepth = 0;
	distanceAtlasResolution = 0;
	distanceAtlasDepth = 0;
	volumes.clear();
	worldSpaceVolumes.clear();
	volumesDirty = false;
	texturesDirty = false;
	buffersDirty = true;

	volumeOptions.clear();
}

void LightProbeVolumes3D::addLightProbeVolume(const LightProbeVolumeOptions3D& options) {
	if (volumeOptions.size() >= size_t{Limits<uint32_t>::MAX}) {
		throw std::length_error{"Maximum light probe volume count exceeded."};
	}

	irradianceAtlasTexture = {};
	distanceAtlasTexture = {};
	irradianceAtlasResolution = 0;
	irradianceAtlasDepth = 0;
	distanceAtlasResolution = 0;
	distanceAtlasDepth = 0;
	volumes.clear();
	worldSpaceVolumes.clear();
	volumesDirty = true;
	texturesDirty = true;
	buffersDirty = true;

	volumeOptions.push_back(options);
}

void LightProbeVolumes3D::setLightProbeVolumes(const LightProbeVolumes3DOptions& newOptions) {
	setLightProbeVolumes({}, {}, {}, newOptions);
}

void LightProbeVolumes3D::setLightProbeVolumes(Texture newIrradianceAtlasTexture, Texture newDistanceAtlasTexture, ArrayList<LightProbeVolumeOptions3D> newVolumeOptions,
	const LightProbeVolumes3DOptions& newOptions) {
	GREM_ASSERT(!newIrradianceAtlasTexture || newIrradianceAtlasTexture.getType() == TextureType::TEXTURE_2D_ARRAY);
	GREM_ASSERT(!newDistanceAtlasTexture || newDistanceAtlasTexture.getType() == TextureType::TEXTURE_2D_ARRAY);
	GREM_ASSERT(newIrradianceAtlasTexture.getWidth() == newIrradianceAtlasTexture.getHeight());
	GREM_ASSERT(newDistanceAtlasTexture.getWidth() == newDistanceAtlasTexture.getHeight());

	volumesDirty = !newVolumeOptions.empty() || !volumes.empty();
	texturesDirty = volumesDirty && (!newIrradianceAtlasTexture || !newDistanceAtlasTexture);
	buffersDirty = true;

	volumes.clear();
	worldSpaceVolumes.clear();

	options = newOptions;
	volumeOptions = std::move(newVolumeOptions);
	irradianceAtlasTexture = std::move(newIrradianceAtlasTexture);
	distanceAtlasTexture = std::move(newDistanceAtlasTexture);
	irradianceAtlasResolution = this->irradianceAtlasTexture.getWidth();
	irradianceAtlasDepth = this->irradianceAtlasTexture.getDepth();
	distanceAtlasResolution = this->distanceAtlasTexture.getWidth();
	distanceAtlasDepth = this->distanceAtlasTexture.getDepth();
}

void LightProbeVolumes3D::flushVolumesAndTextures(Device& device) const {
	if (volumesDirty) {
		volumes.clear();
		worldSpaceVolumes.clear();

		irradianceAtlasResolution = 0;
		irradianceAtlasDepth = 0;
		distanceAtlasResolution = 0;
		distanceAtlasDepth = 0;

		if (!volumeOptions.empty()) {
			uint32_t maxHorizontalIrradianceResolution = 1;
			uint32_t maxHorizontalDistanceResolution = 1;
			uint32_t maxVerticalProbeCount = 1;
			for (const LightProbeVolumeOptions3D& volumeOptions : this->volumeOptions) {
				const uint32_t horizontalProbeCount = max(volumeOptions.probeCounts.x, volumeOptions.probeCounts.z);
				maxHorizontalIrradianceResolution = max(maxHorizontalIrradianceResolution, horizontalProbeCount * volumeOptions.irradianceMapResolution);
				maxHorizontalDistanceResolution = max(maxHorizontalDistanceResolution, horizontalProbeCount * volumeOptions.distanceMapResolution);
				maxVerticalProbeCount = max(maxVerticalProbeCount, volumeOptions.probeCounts.y);
			}

			resource::ArrayAtlasPacker irradianceAtlasPacker{{
				.initialResolution = roundUpToPowerOf2(maxHorizontalIrradianceResolution),
				.initialDepth = maxVerticalProbeCount,
				.padding = 0,
				.alignment = 1,
			}};
			resource::ArrayAtlasPacker distanceAtlasPacker{{
				.initialResolution = roundUpToPowerOf2(maxHorizontalDistanceResolution),
				.initialDepth = maxVerticalProbeCount,
				.padding = 0,
				.alignment = 1,
			}};
			for (const LightProbeVolumeOptions3D& volumeOptions : this->volumeOptions) {
				GREM_ASSERT(isPowerOf2(volumeOptions.irradianceMapResolution) && volumeOptions.irradianceMapResolution >= 4);
				GREM_ASSERT(isPowerOf2(volumeOptions.distanceMapResolution) && volumeOptions.distanceMapResolution >= 4);

				const auto [irradianceAtlasOffset, irradianceAtlasResized] = irradianceAtlasPacker.insertBox(Extent3D{
					volumeOptions.probeCounts.x * volumeOptions.irradianceMapResolution,
					volumeOptions.probeCounts.z * volumeOptions.irradianceMapResolution,
					volumeOptions.probeCounts.y,
				});
				const auto [distanceAtlasOffset, distanceAtlasResized] = distanceAtlasPacker.insertBox(Extent3D{
					volumeOptions.probeCounts.x * volumeOptions.distanceMapResolution,
					volumeOptions.probeCounts.z * volumeOptions.distanceMapResolution,
					volumeOptions.probeCounts.y,
				});
				volumes.push_back(VolumeFields{
					.lightProbeVolumeCenter = volumeOptions.center,
					.lightProbeVolumeOrientation{volumeOptions.orientation.x, volumeOptions.orientation.y, volumeOptions.orientation.z, volumeOptions.orientation.w},
					.lightProbeVolumeProbeSpacing = volumeOptions.probeSpacing,
					.lightProbeVolumeProbeCounts = vec3{volumeOptions.probeCounts},
					.lightProbeVolumeIrradianceAtlasOffset{irradianceAtlasOffset},
					.lightProbeVolumeDistanceAtlasOffset{distanceAtlasOffset},
					.lightProbeVolumeIrradianceAtlasPaddedProbeSizeAndTexelSize{},
					.lightProbeVolumeDistanceAtlasPaddedProbeSizeAndTexelSize{},
				});
				const float worldSpaceVolume = product(volumeOptions.probeSpacing * vec3{volumeOptions.probeCounts});
				worldSpaceVolumes.push_back(worldSpaceVolume);
			}

			irradianceAtlasResolution = irradianceAtlasPacker.getResolution();
			irradianceAtlasDepth = irradianceAtlasPacker.getDepth();

			distanceAtlasResolution = distanceAtlasPacker.getResolution();
			distanceAtlasDepth = distanceAtlasPacker.getDepth();

			if (!texturesDirty) {
				if (irradianceAtlasResolution != irradianceAtlasTexture.getWidth() || irradianceAtlasResolution != irradianceAtlasTexture.getHeight() ||
					irradianceAtlasDepth != irradianceAtlasTexture.getDepth()) {
					throw graphics::Error{"Incorrect size of irradiance atlas texture provided for light probe volumes."};
				}
				if (distanceAtlasResolution != distanceAtlasTexture.getWidth() || distanceAtlasResolution != distanceAtlasTexture.getHeight() ||
					distanceAtlasDepth != distanceAtlasTexture.getDepth()) {
					throw graphics::Error{"Incorrect size of distance atlas texture provided for light probe volumes."};
				}
			}

			const float irradianceAtlasTexelSize = 1.0f / static_cast<float>(irradianceAtlasResolution);
			const float distanceAtlasTexelSize = 1.0f / static_cast<float>(distanceAtlasResolution);
			for (size_t i = 0; i < volumes.size(); ++i) {
				VolumeFields& volume = volumes[i];
				const LightProbeVolumeOptions3D& volumeOptions = this->volumeOptions[i];

				volume.lightProbeVolumeIrradianceAtlasOffset.x *= irradianceAtlasTexelSize;
				volume.lightProbeVolumeIrradianceAtlasOffset.y *= irradianceAtlasTexelSize;
				volume.lightProbeVolumeDistanceAtlasOffset.x *= distanceAtlasTexelSize;
				volume.lightProbeVolumeDistanceAtlasOffset.y *= distanceAtlasTexelSize;
				volume.lightProbeVolumeIrradianceAtlasPaddedProbeSizeAndTexelSize = {
					static_cast<float>(volumeOptions.irradianceMapResolution) * irradianceAtlasTexelSize,
					irradianceAtlasTexelSize,
				};
				volume.lightProbeVolumeDistanceAtlasPaddedProbeSizeAndTexelSize = {
					static_cast<float>(volumeOptions.distanceMapResolution) * distanceAtlasTexelSize,
					distanceAtlasTexelSize,
				};
			}
		}

		volumesDirty = false;
	}

	if (texturesDirty) {
		irradianceAtlasTexture = {};
		distanceAtlasTexture = {};

		if (irradianceAtlasResolution > 0) {
			irradianceAtlasTexture = Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::B10G11R11_UFLOAT_PACK32,
				Extent3D{irradianceAtlasResolution, irradianceAtlasResolution, irradianceAtlasDepth}, 1, ClearValues{}, IRRADIANCE_ATLAS_SAMPLER_OPTIONS);
			distanceAtlasTexture = Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::R16G16_FLOAT,
				Extent3D{distanceAtlasResolution, distanceAtlasResolution, distanceAtlasDepth}, 1, ClearValues{}, DISTANCE_ATLAS_SAMPLER_OPTIONS);
		}

		texturesDirty = false;
	}
}

void LightProbeVolumes3D::flush(Device& device, Renderer3D& renderer3D) const {
	flushVolumesAndTextures(device);

	if (buffersDirty) {
		atlasBuffer.upload(AtlasParameters{
			.lightProbeVolumesIrradianceAtlasTexture = (irradianceAtlasTexture) ? irradianceAtlasTexture : renderer3D.getInvisibleTexture2DArray(),
			.lightProbeVolumesDistanceAtlasTexture = (distanceAtlasTexture) ? distanceAtlasTexture : renderer3D.getInvisibleTexture2DArray(),
		});
		volumeBuffer.upload(volumes);
		buffersDirty = false;
	}
}

} // namespace grem::graphics
