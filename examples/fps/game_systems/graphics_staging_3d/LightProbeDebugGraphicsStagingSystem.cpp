// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/Model.hpp>

#include "../../AssetCache.hpp"
#include "../../ClientSettings.hpp"
#include "../../Graphics.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"

#include <memory> // std::uninitialized_...

class LightProbeDebugGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"LightProbeDebugGraphicsStagingSystem requires graphics."};
		}
		resources.addSharedResource<ProbeAssets>(graphics->device, graphics->renderer3D, resources.getResource<AssetCache>());
		resources.addSharedResource<ProbeInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<ProbeInstances>();
		resources.removeResource<ProbeAssets>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics* graphics) override {
		GREM_ASSERT(graphics);
		resources.getResource<ProbeAssets>() = ProbeAssets{graphics->device, graphics->renderer3D, resources.getResource<AssetCache>()};
	}

	void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor&, Graphics& graphics, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		const ClientSettings& settings = worldView.subtickResources.getResource<ClientSettings>();

		if (settings.graphics.showLightProbeVolumesDebugVisualization) {
			ProbeAssets& probeAssets = const_cast<ProbeAssets&>(worldView.subtickResources.getResource<ProbeAssets>());
			ProbeInstances& probeInstances = const_cast<ProbeInstances&>(worldView.subtickResources.getResource<ProbeInstances>());

			static_assert(trivially_destructible<mat4>);
			static_assert(trivially_destructible<float>);
			probeInstances.probeTransformationArena.release();
			probeInstances.probeTransformations.clear();
			const gfx::Model3D& model = *probeAssets.sphereModel;
			const res::Model::JointCount jointCount = model.getJointCount();
			const res::Model::MorphTargetWeightCount morphTargetWeightCount = model.getMorphTargetWeightCount();
			for (const gfx::LightProbeVolumeOptions3D& volumeOptions : graphics.lightProbeVolumes.getVolumeOptions()) {
				const u32vec3 probeCounts = volumeOptions.probeCounts;
				const vec3 probeSpacing = volumeOptions.probeSpacing;
				const vec3 halfExtents = vec3{probeCounts} * probeSpacing * 0.5f;
				for (uint32_t y = 0; y < probeCounts.y; ++y) {
					for (uint32_t z = 0; z < probeCounts.z; ++z) {
						for (uint32_t x = 0; x < probeCounts.x; ++x) {
							const u32vec3 gridIndices{x, y, z};
							const vec3 localOffset = (vec3{gridIndices} + vec3{0.5f}) * probeSpacing - halfExtents;
							const vec3 position = volumeOptions.center + volumeOptions.orientation * localOffset;
							const mat4 rootTransformation = translateScale(position, vec3{0.1f});
							const res::Model::Pose& pose = model.getBindPose();
							res::Model::TransformationReference transformation{};
							transformation.jointMatrices = ArenaAllocator<mat4>{&probeInstances.probeTransformationArena}.allocate(jointCount);
							std::uninitialized_default_construct_n(transformation.jointMatrices, jointCount);
							transformation.jointMatrices[0] = rootTransformation;
							transformation.jointsVisible = ArenaAllocator<bool>{&probeInstances.probeTransformationArena}.allocate(jointCount);
							std::uninitialized_default_construct_n(transformation.jointsVisible, jointCount);
							transformation.jointsVisible[0] = true;
							if (morphTargetWeightCount > 0) {
								transformation.morphTargetWeights = ArenaAllocator<float>{&probeInstances.probeTransformationArena}.allocate(morphTargetWeightCount);
								std::uninitialized_fill_n(transformation.morphTargetWeights, morphTargetWeightCount, 0.0f);
							}
							transformation.pose(pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
							probeInstances.probeTransformations.push_back(transformation);
						}
					}
				}
			}
			graphics.visibleInstances3D.putShadedModelInstances(probeAssets.shaderPipelineSet, *probeAssets.sphereModel, probeInstances.probeTransformations);
		}
	}

private:
	struct ProbeAssets {
		explicit ProbeAssets(gfx::Device& device, gfx::Renderer3D& renderer3D, AssetCache& assetCache)
			: sphereModel(assetCache.getModel3D(device, renderer3D, "models/sphere.obj"))
			, shaderPipelineSet(device, renderer3D.getDefaultModel3DVertexShader(), gfx::Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
				  assetCache.getFragmentShader<gfx::Renderer3D::PBRModel3DFragmentShader>(device, "shaders/probe.frag"), gfx::Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
				  gfx::Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS) {}

		SharedPointer<gfx::Model3D> sphereModel;
		gfx::Renderer3D::PBRModel3DShaderPipelineSet shaderPipelineSet;
	};

	struct ProbeInstances {
		Arena<0> probeTransformationArena{};
		ArrayList<res::Model::TransformationView> probeTransformations{};
	};
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createLightProbeDebugGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new LightProbeDebugGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
