// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Texture.hpp>

#include "../../Graphics.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"

class LightBakingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"LightBakingSystem requires graphics."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void renderGraphics(Graphics& graphics, const WorldView&, gfx::Texture*) override {
		GREM_PROFILE_FUNCTION();

		if (!graphics.baking) {
			throw Error{"LightBakingSystem active while not baking."};
		}

		const Span<const gfx::LightProbeVolumeOptions3D> lightProbeVolumesVolumeOptions = graphics.lightProbeVolumes.getVolumeOptions();
		const Span<const gfx::ReflectionProbeOptions3D> reflectionProbesProbeOptions = graphics.reflectionProbes.getProbeOptions();

		static constexpr size_t PROGRESS_PER_FRAME = 32;
		for (size_t progressIndex = 0; progressIndex < PROGRESS_PER_FRAME && graphics.baking->state != Graphics::Baking::State::DONE; ++progressIndex) {
			switch (graphics.baking->state) {
				case Graphics::Baking::State::BAKING_LIGHT_PROBES_DISTANCE: {
					const gfx::LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumesVolumeOptions[graphics.baking->lightProbeVolumeIndex];
					const size_t lightProbeCount = static_cast<size_t>(volumeOptions.probeCounts.x * volumeOptions.probeCounts.y * volumeOptions.probeCounts.z);

					++graphics.baking->totalProgressIndex;

					const size_t i = graphics.baking->lightProbeIndex;
					const size_t strideY = static_cast<size_t>(volumeOptions.probeCounts.x);
					const size_t strideZ = strideY * static_cast<size_t>(volumeOptions.probeCounts.y);
					const u32vec3 lightProbeGridIndices{
						static_cast<uint32_t>(i % strideY),
						static_cast<uint32_t>((i % strideZ) / strideY),
						static_cast<uint32_t>(i / strideZ),
					};
					graphics.baking->lightBaker.bakeLightProbeDistanceMap(graphics.lightProbeVolumes, static_cast<uint32_t>(graphics.baking->lightProbeVolumeIndex),
						lightProbeGridIndices, [&](gfx::RenderPass& renderPass, const gfx::Camera3D& camera) -> void {
							graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
								{{graphics.baking->lightProbeOccluderInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}}}, camera);
						});

					if (++graphics.baking->lightProbeIndex >= lightProbeCount) {
						graphics.baking->lightProbeIndex = 0;
						if (++graphics.baking->lightProbeVolumeIndex >= lightProbeVolumesVolumeOptions.size()) {
							graphics.baking->lightProbeVolumeIndex = 0;
							graphics.baking->state = Graphics::Baking::State::BAKING_LIGHT_PROBES_IRRADIANCE;
						}
					}
					break;
				}
				case Graphics::Baking::State::BAKING_LIGHT_PROBES_IRRADIANCE: {
					const gfx::LightProbeVolumeOptions3D& volumeOptions = lightProbeVolumesVolumeOptions[graphics.baking->lightProbeVolumeIndex];
					const size_t lightProbeCount = static_cast<size_t>(volumeOptions.probeCounts.x * volumeOptions.probeCounts.y * volumeOptions.probeCounts.z);

					++graphics.baking->totalProgressIndex;

					const size_t i = graphics.baking->lightProbeIndex;
					const size_t strideY = static_cast<size_t>(volumeOptions.probeCounts.x);
					const size_t strideZ = strideY * static_cast<size_t>(volumeOptions.probeCounts.y);
					const u32vec3 lightProbeGridIndices{
						static_cast<uint32_t>(i % strideY),
						static_cast<uint32_t>((i % strideZ) / strideY),
						static_cast<uint32_t>(i / strideZ),
					};
					graphics.baking->lightBaker.bakeLightProbeIrradianceMap(graphics.lightProbeVolumes, static_cast<uint32_t>(graphics.baking->lightProbeVolumeIndex),
						lightProbeGridIndices, [&](gfx::RenderPass& renderPass, const gfx::Camera3D& camera) -> void {
							graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
								{{graphics.depthPrepassInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}}}, camera);
							graphics.renderer3D.drawHDRPBRFrame(renderPass, {graphics.visibleInstances3D}, camera, graphics.fog, graphics.baking->radianceOnlySky, graphics.decals,
								graphics.lights, graphics.baking->previousBounceLightProbeVolumes, graphics.baking->previousBounceReflectionProbes);
						});

					if (++graphics.baking->lightProbeIndex >= lightProbeCount) {
						graphics.baking->lightProbeIndex = 0;
						if (++graphics.baking->lightProbeVolumeIndex >= lightProbeVolumesVolumeOptions.size()) {
							graphics.baking->lightProbeVolumeIndex = 0;
							graphics.baking->previousBounceLightProbeVolumes = graphics.lightProbeVolumes;
							if (!reflectionProbesProbeOptions.empty()) {
								graphics.baking->state = Graphics::Baking::State::BAKING_REFLECTION_PROBES;
							} else {
								if (++graphics.baking->bounceIndex >= graphics.baking->bounceCount) {
									graphics.baking->state = Graphics::Baking::State::DONE;
								}
							}
						}
					}
					break;
				}
				case Graphics::Baking::State::BAKING_REFLECTION_PROBES: {
					++graphics.baking->totalProgressIndex;
					progressIndex += PROGRESS_PER_FRAME - 1;

					graphics.baking->lightBaker.bakeReflectionProbeReflectionMap(graphics.reflectionProbes, static_cast<uint32_t>(graphics.baking->reflectionProbeIndex),
						[&](gfx::RenderPass& renderPass, const gfx::Camera3D& camera) -> void {
							graphics.renderer3D.drawUnlitUnorderedFrame(renderPass,
								{{graphics.depthPrepassInstances3D, {.skipAlphaBlendedModelMeshInstances = true, .skipAll2DInstances = true}}}, camera);
							graphics.renderer3D.drawHDRPBRFrame(renderPass, {graphics.visibleInstances3D}, camera, graphics.fog, graphics.sky, graphics.decals, graphics.lights,
								graphics.baking->previousBounceLightProbeVolumes, graphics.baking->previousBounceReflectionProbes);
						});

					if (++graphics.baking->reflectionProbeIndex >= reflectionProbesProbeOptions.size()) {
						graphics.baking->reflectionProbeIndex = 0;
						graphics.baking->previousBounceReflectionProbes = graphics.reflectionProbes;
						if (++graphics.baking->bounceIndex >= graphics.baking->bounceCount) {
							graphics.baking->state = Graphics::Baking::State::DONE;
						} else if (!lightProbeVolumesVolumeOptions.empty()) {
							graphics.baking->state = Graphics::Baking::State::BAKING_LIGHT_PROBES_IRRADIANCE;
						}
					}
					break;
				}
				case Graphics::Baking::State::DONE: break;
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createLightBakingSystem() { // NOLINT(misc-use-internal-linkage)
	return new LightBakingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
