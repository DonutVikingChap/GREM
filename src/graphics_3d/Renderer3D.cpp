// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Viewport.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Camera2D.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/graphics_2d/Model2D.hpp>
#include <GREM/graphics_2d/Renderer2D.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/LightProbeVolumes3D.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/ReflectionProbes3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/graphics_3d/Sky3D.hpp>
#include <GREM/resource/Model.hpp>

#include "builtin_shaders_graphics_3d.hpp"

namespace grem::graphics {

namespace {

constexpr mat4 BIAS_MATRIX{
	// clang-format off
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f,
	// clang-format on
};

template <typename T>
struct ArenaDeleter {
	void operator()(T* p) const noexcept {
		if (p) {
			p->~T();
		}
	}
};

} // namespace

constexpr Array<vec3, 8> Renderer3D::CUBE_MODEL_3D_VERTEX_POSITIONS{
	vec3{1.0f, 1.0f, -1.0f},
	vec3{1.0f, -1.0f, -1.0f},
	vec3{1.0f, 1.0f, 1.0f},
	vec3{1.0f, -1.0f, 1.0f},

	vec3{-1.0f, 1.0f, -1.0f},
	vec3{-1.0f, -1.0f, -1.0f},
	vec3{-1.0f, 1.0f, 1.0f},
	vec3{-1.0f, -1.0f, 1.0f},
};

constexpr Array<uint32_t, 36> Renderer3D::CUBE_MODEL_3D_INDICES{
	// clang-format off
	4, 2, 0,
	2, 7, 3,
	6, 5, 7,
	1, 7, 5,
	0, 3, 1,
	4, 1, 5,
	4, 6, 2,
	2, 6, 7,
	6, 4, 5,
	1, 3, 7,
	0, 2, 3,
	4, 0, 1,
	// clang-format on
};

Renderer3D::Renderer3D(Device& device, Renderer2D& renderer2D, const Renderer3DOptions& options)
	: device(device)
	, renderer2D(renderer2D)
	, options(options) {
	GREM_ASSERT(options.tileSize > 0);
	GREM_ASSERT(options.depthBinCount > 0);
	GREM_ASSERT(isPowerOf2(options.specularSplitSumBRDFIntegrationMapResolution));
}

Renderer3D::~Renderer3D() = default;

const Renderer3D::Model2DTransformed3DVertexShader& Renderer3D::getDefault3DTransformedModel2DVertexShader() {
	if (!default3DTransformedModel2DVertexShader) {
		[[unlikely]];
		default3DTransformedModel2DVertexShader.emplace(
			Renderer3D::Model2DTransformed3DVertexShader::create(device, detail::RENDERER_3D_DEFAULT_MODEL_2D_TRANSFORMED_3D_VERTEX_SHADER_CODE));
	}
	return *default3DTransformedModel2DVertexShader;
}

const Renderer3D::Model2DTransformed3DFragmentShader& Renderer3D::getPlain3DTransformedModel2DFragmentShader() {
	if (!plain3DTransformedModel2DFragmentShader) {
		[[unlikely]];
		plain3DTransformedModel2DFragmentShader.emplace(
			Renderer3D::Model2DTransformed3DFragmentShader::create(device, detail::RENDERER_3D_PLAIN_MODEL_2D_TRANSFORMED_3D_FRAGMENT_SHADER_CODE));
	}
	return *plain3DTransformedModel2DFragmentShader;
}

const Renderer3D::Model2DTransformed3DFragmentShader& Renderer3D::getPlain3DTransformedTextFragmentShader() {
	if (!plain3DTransformedTextFragmentShader) {
		[[unlikely]];
		plain3DTransformedTextFragmentShader.emplace(
			Renderer3D::Model2DTransformed3DFragmentShader::create(device, detail::RENDERER_3D_TEXT_MODEL_2D_TRANSFORMED_3D_FRAGMENT_SHADER_CODE));
	}
	return *plain3DTransformedTextFragmentShader;
}

const Model3D::VertexShader& Renderer3D::getDefaultModel3DVertexShader() {
	if (!defaultModel3DVertexShader) {
		[[unlikely]];
		defaultModel3DVertexShader.emplace(Model3D::VertexShader::create(device, detail::RENDERER_3D_DEFAULT_MODEL_3D_VERTEX_SHADER_CODE));
	}
	return *defaultModel3DVertexShader;
}

const Model3D::FragmentShader& Renderer3D::getUnlitModel3DFragmentShader() {
	if (!unlitModel3DFragmentShader) {
		[[unlikely]];
		unlitModel3DFragmentShader.emplace(Model3D::FragmentShader::create(device, detail::RENDERER_3D_UNLIT_MODEL_3D_FRAGMENT_SHADER_CODE));
	}
	return *unlitModel3DFragmentShader;
}

const Renderer3D::PBRModel3DFragmentShader& Renderer3D::getPBRModel3DFragmentShader() {
	if (!pbrModel3DFragmentShader) {
		[[unlikely]];
		pbrModel3DFragmentShader.emplace(Renderer3D::PBRModel3DFragmentShader::create(device, detail::RENDERER_3D_PBR_MODEL_3D_FRAGMENT_SHADER_CODE));
	}
	return *pbrModel3DFragmentShader;
}

Renderer3D::ShadowMapModel3DShaderPipelineSet& Renderer3D::getShadowMapModel3DShaderPipelineSet() {
	if (!shadowMapModel3DShaderPipelineSet) {
		[[unlikely]];
		shadowMapModel3DShaderPipelineSet.emplace(device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
			ShadowMapModel3DFragmentShader::create(device, detail::RENDERER_3D_SHADOW_MAP_MODEL_3D_FRAGMENT_SHADER_CODE), Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
			[shadowMapDepthBiasSlopeFactor = options.shadowMapDepthBiasSlopeFactor,
				shadowMapDepthBiasConstantFactor = options.shadowMapDepthBiasConstantFactor](const Model3D::ShaderConfiguration& shaderConfiguration) -> ShaderPipelineOptions {
				return ShaderPipelineOptions{
					.primitiveType = shaderConfiguration.primitiveType,
					.faceCullingMode =
						((shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_DOUBLE_SIDED) != 0) ? FaceCullingMode::NONE : FaceCullingMode::CULL_BACK_FACES,
					.frontFace = shaderConfiguration.frontFace,
					.depthBiasSlopeFactor = shadowMapDepthBiasSlopeFactor,
					.depthBiasConstantFactor = shadowMapDepthBiasConstantFactor,
				};
			});
	}
	return *shadowMapModel3DShaderPipelineSet;
}

Renderer3D::DistanceModel3DShaderPipelineSet& Renderer3D::getDistanceModel3DShaderPipelineSet() {
	if (!distanceModel3DShaderPipelineSet) {
		[[unlikely]];
		distanceModel3DShaderPipelineSet.emplace(device, getDefaultModel3DVertexShader(), Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
			DistanceModel3DFragmentShader::create(device, detail::RENDERER_3D_DISTANCE_MODEL_3D_FRAGMENT_SHADER_CODE), Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS,
			[](const Model3D::ShaderConfiguration& shaderConfiguration) -> ShaderPipelineOptions {
				return ShaderPipelineOptions{
					.primitiveType = shaderConfiguration.primitiveType,
					.faceCullingMode = FaceCullingMode::NONE,
					.frontFace = shaderConfiguration.frontFace,
				};
			});
	}
	return *distanceModel3DShaderPipelineSet;
}

const Renderer3D::DefaultSky3DVertexShader& Renderer3D::getDefaultSky3DVertexShader() {
	if (!defaultSky3DVertexShader) {
		defaultSky3DVertexShader.emplace(DefaultSky3DVertexShader::create(device, detail::RENDERER_3D_DEFAULT_SKY_3D_VERTEX_SHADER_CODE));
	}
	return *defaultSky3DVertexShader;
}

const Renderer3D::PBRSky3DFragmentShader& Renderer3D::getPBRSky3DFragmentShader() {
	if (!pbrSky3DFragmentShader) {
		pbrSky3DFragmentShader.emplace(PBRSky3DFragmentShader::create(device, detail::RENDERER_3D_PBR_SKY_3D_FRAGMENT_SHADER_CODE));
	}
	return *pbrSky3DFragmentShader;
}

void Renderer3D::renderShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix,
	const mat4& observerViewMatrix, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	lights.flushIndices(device);

	const auto it = lights.lights.find(lightID);
	if (it == lights.lights.end()) {
		[[unlikely]];
		return;
	}

	const Lights3D::Light& light = it->second;
	if (light.shadowMapIndex < 0.0f) {
		return;
	}

	switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
		case LightType::AMBIENT_LIGHT: unreachable();
		case LightType::SUN_LIGHT: [[fallthrough]];
		case LightType::DIRECTIONAL_LIGHT: {
			renderViewDependentShadowMap(lights, lightID, totalShadowCasterBoundingBox, observerProjectionMatrix, observerViewMatrix, drawShadowCasters);
			break;
		}
		case LightType::POINT_LIGHT: [[fallthrough]];
		case LightType::SPOT_LIGHT:
			if (light.range > 0.0f) {
				const vec4 positionInView = observerViewMatrix * vec4{light.position, 1.0f};
				if (length2(vec3{positionInView}) > length2(light.range)) {
					const vec4 positionInClipSpace = observerProjectionMatrix * positionInView;
					const vec2 radiiInClipSpace = vec2{light.range} * vec2{observerProjectionMatrix[0][0], observerProjectionMatrix[1][1]};
					if (positionInClipSpace.z < 0.0f || any(lessThan(vec2{positionInClipSpace} + radiiInClipSpace, vec2{-positionInClipSpace.w}) |
															greaterThan(vec2{positionInClipSpace} - radiiInClipSpace, vec2{positionInClipSpace.w}))) {
						break;
					}
				}
			}
			renderViewIndependentShadowMap(lights, lightID, drawShadowCasters);
			break;
	}
}

void Renderer3D::renderAllShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix, const mat4& observerViewMatrix,
	FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [lightID, light] : lights.lights) {
		renderShadowMap(lights, lightID, totalShadowCasterBoundingBox, observerProjectionMatrix, observerViewMatrix, drawShadowCasters);
	}
}

void Renderer3D::renderViewIndependentShadowMap(Lights3D& lights, LightID lightID, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	lights.flushIndices(device);

	const auto it = lights.lights.find(lightID);
	if (it == lights.lights.end()) {
		[[unlikely]];
		return;
	}

	const Lights3D::Light& light = it->second;
	if (light.shadowMapIndex < 0.0f) {
		return;
	}

	const uint32_t shadowMapIndex = static_cast<uint32_t>(light.shadowMapIndex);
	switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
		case LightType::AMBIENT_LIGHT: unreachable();
		case LightType::SUN_LIGHT: [[fallthrough]];
		case LightType::DIRECTIONAL_LIGHT: break;
		case LightType::POINT_LIGHT: {
			const float farZ = (light.range > 0.0f) ? min(light.shadowFarZ, light.range) : light.shadowFarZ;
			const float nearZ = min(light.shadowNearZ, farZ);
			const mat4 shadowProjectionMatrix = perspective(convertDegreesToRadians(90.0f), 1.0f, nearZ, farZ);
			const Array<mat4, 6> shadowViewMatrices = Cubemap3D::getSideViewMatrices(light.position);

			for (uint32_t side = 0; side < 6; ++side) {
				shadowMapCamera3D.setProjectionAndView(shadowProjectionMatrix, shadowViewMatrices[side]);
				RenderPass renderPass{device, lights.pointLightShadowMaps.getSubresource({.layer = shadowMapIndex * 6 + side}), ClearValues{}};
				drawShadowCasters(renderPass, shadowMapCamera3D);
				device.render(renderPass);
			}
			break;
		}
		case LightType::SPOT_LIGHT: {
			const uint32_t shadowMatrixIndex = static_cast<uint32_t>(light.shadowMatrixIndex);
			const vec3 up = (light.direction.x == 0.0f && light.direction.z == 0.0f) ? vec3{0.0f, 0.0f, 1.0f} : vec3{0.0f, 1.0f, 0.0f};
			const float farZ = (light.range > 0.0f) ? min(light.shadowFarZ, light.range) : light.shadowFarZ;
			const float nearZ = min(light.shadowNearZ, farZ);
			const mat4 shadowProjectionMatrix = perspective(2.0f * acos(light.outerConeCosine), 1.0f, nearZ, farZ);
			const mat4 shadowViewMatrix = lookAt(light.position, light.position + light.direction, up);

			shadowMapCamera3D.setProjectionAndView(shadowProjectionMatrix, shadowViewMatrix);
			RenderPass renderPass{device, lights.spotLightShadowMaps.getSubresource({.layer = shadowMapIndex}), ClearValues{}};
			drawShadowCasters(renderPass, shadowMapCamera3D);
			device.render(renderPass);

			lights.shadowMatrices[shadowMatrixIndex].shadowMatrix = BIAS_MATRIX * shadowProjectionMatrix * shadowViewMatrix;
			lights.shadowMatrixBufferDirty = true;
			break;
		}
	}
}

void Renderer3D::renderAllViewIndependentShadowMaps(Lights3D& lights, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [lightID, light] : lights.lights) {
		renderViewIndependentShadowMap(lights, lightID, drawShadowCasters);
	}
}

void Renderer3D::renderViewDependentShadowMap(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix,
	const mat4& observerViewMatrix, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	lights.flushIndices(device);

	const auto it = lights.lights.find(lightID);
	if (it == lights.lights.end()) {
		[[unlikely]];
		return;
	}

	const Lights3D::Light& light = it->second;
	if (light.shadowMapIndex < 0.0f) {
		return;
	}

	const uint32_t shadowMapIndex = static_cast<uint32_t>(light.shadowMapIndex);
	switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
		case LightType::AMBIENT_LIGHT: unreachable();
		case LightType::SUN_LIGHT: [[fallthrough]];
		case LightType::DIRECTIONAL_LIGHT: {
			const uint32_t shadowMatrixIndex = static_cast<uint32_t>(light.shadowMatrixIndex);

			// Fit shadow near/far plane to scene AABB.
			const Box<3, float> sceneAABB = totalShadowCasterBoundingBox;
			const Array sceneAABBCorners{
				vec3{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.min.z},
				vec3{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.max.z},
				vec3{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.min.z},
				vec3{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.max.z},
				vec3{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.min.z},
				vec3{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.max.z},
				vec3{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.min.z},
				vec3{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.max.z},
			};

			float sceneNearZ = Limits<float>::MAX;
			float sceneFarZ = Limits<float>::MIN;
			for (const vec3& sceneAABBCorner : sceneAABBCorners) {
				const float z = dot(sceneAABBCorner, light.direction);
				sceneNearZ = min(sceneNearZ, z);
				sceneFarZ = max(sceneFarZ, z);
			}

			// Calculate the side slopes of the observer's projection matrix.
			const mat4 inverseObserverProjectionMatrix = inverse(observerProjectionMatrix);
			const mat4 inverseObserverViewMatrix = inverse(observerViewMatrix);

			const vec3 observerFrustumLeft = vec3{inverseObserverProjectionMatrix * vec4{-1.0f, 0.0f, 1.0f, 1.0f}};
			const vec3 observerFrustumRight = vec3{inverseObserverProjectionMatrix * vec4{1.0f, 0.0f, 1.0f, 1.0f}};
			const vec3 observerFrustumBottom = vec3{inverseObserverProjectionMatrix * vec4{0.0f, -1.0f, 1.0f, 1.0f}};
			const vec3 observerFrustumTop = vec3{inverseObserverProjectionMatrix * vec4{0.0f, 1.0f, 1.0f, 1.0f}};

			const float observerFrustumLeftSlope = observerFrustumLeft.x / observerFrustumLeft.z;
			const float observerFrustumRightSlope = observerFrustumRight.x / observerFrustumRight.z;
			const float observerFrustumBottomSlope = observerFrustumBottom.y / observerFrustumBottom.z;
			const float observerFrustumTopSlope = observerFrustumTop.y / observerFrustumTop.z;

			const vec3 up = (light.direction.x == 0.0f && light.direction.z == 0.0f) ? vec3{0.0f, 0.0f, 1.0f} : vec3{0.0f, 1.0f, 0.0f};
			const mat4 shadowViewMatrix = lookAt(vec3{0.0f, 0.0f, 0.0f}, light.direction, up);

			const float texelSize = 1.0f / static_cast<float>(lights.options.cascadedShadowMapResolution);

			for (uint32_t cascadeLevel = 0; cascadeLevel < lights.options.shadowCascadeFrustumFarPlaneDistances.size(); ++cascadeLevel) {
				// Fit shadow area to observer frustum.
				const float nearZ =
					(cascadeLevel == 0) ? 0.0f : max(lights.options.shadowCascadeFrustumFarPlaneDistances[cascadeLevel - 1] - lights.options.shadowCascadeBlendSize, 0.0f);
				const float farZ = lights.options.shadowCascadeFrustumFarPlaneDistances[cascadeLevel];

				const float nearLeft = observerFrustumLeftSlope * nearZ;
				const float nearRight = observerFrustumRightSlope * nearZ;
				const float nearBottom = observerFrustumBottomSlope * nearZ;
				const float nearTop = observerFrustumTopSlope * nearZ;

				const float farLeft = observerFrustumLeftSlope * farZ;
				const float farRight = observerFrustumRightSlope * farZ;
				const float farBottom = observerFrustumBottomSlope * farZ;
				const float farTop = observerFrustumTopSlope * farZ;

				const Array corners{
					vec3{inverseObserverViewMatrix * vec4{nearRight, nearTop, -nearZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{nearLeft, nearTop, -nearZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{nearLeft, nearBottom, -nearZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{nearRight, nearBottom, -nearZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{farRight, farTop, -farZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{farLeft, farTop, -farZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{farLeft, farBottom, -farZ, 1.0f}},
					vec3{inverseObserverViewMatrix * vec4{farRight, farBottom, -farZ, 1.0f}},
				};

				const vec3 center = accumulate(corners, vec3{}) * (1.0f / static_cast<float>(corners.size()));
				const float boundingRadius = 1.05f * sqrt(max(distance2(center, corners[2]), distance2(center, corners[4])));

				// Snap the center to 1 pixel increments so that moving the camera does not cause shadows to jitter.
				const float unitsPerTexel = texelSize * (boundingRadius * 2.0f);
				const vec2 centerXY = floor(vec2{shadowViewMatrix * vec4{center, 1.0f}} / unitsPerTexel) * unitsPerTexel;
				const vec2 minXY = centerXY - vec2{boundingRadius};
				const vec2 maxXY = centerXY + vec2{boundingRadius};
				const mat4 shadowProjectionMatrix = ortho(minXY.x, maxXY.x, minXY.y, maxXY.y, sceneNearZ, sceneFarZ);

				shadowMapCamera3D.setProjectionAndView(shadowProjectionMatrix, shadowViewMatrix);

				RenderPass renderPass{device, lights.cascadedShadowMaps.getSubresource({.layer = shadowMapIndex + cascadeLevel}), ClearValues{}};
				drawShadowCasters(renderPass, shadowMapCamera3D);
				device.render(renderPass);

				lights.shadowMatrices[shadowMatrixIndex + cascadeLevel].shadowMatrix = BIAS_MATRIX * shadowProjectionMatrix * shadowViewMatrix;
			}
			lights.shadowMatrixBufferDirty = true;
			break;
		}
		case LightType::POINT_LIGHT: [[fallthrough]];
		case LightType::SPOT_LIGHT: break;
	}
}

void Renderer3D::renderAllViewDependentShadowMaps(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox, const mat4& observerProjectionMatrix,
	const mat4& observerViewMatrix, FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [lightID, light] : lights.lights) {
		renderViewDependentShadowMap(lights, lightID, totalShadowCasterBoundingBox, observerProjectionMatrix, observerViewMatrix, drawShadowCasters);
	}
}

void Renderer3D::renderViewDependentShadowMapForFullZoomedOutView(Lights3D& lights, LightID lightID, const Box<3, float>& totalShadowCasterBoundingBox,
	FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	lights.flushIndices(device);

	const auto it = lights.lights.find(lightID);
	if (it == lights.lights.end()) {
		[[unlikely]];
		return;
	}

	const Lights3D::Light& light = it->second;
	if (light.shadowMapIndex < 0.0f) {
		return;
	}

	const uint32_t shadowMapIndex = static_cast<uint32_t>(light.shadowMapIndex);
	switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
		case LightType::AMBIENT_LIGHT: unreachable();
		case LightType::SUN_LIGHT: [[fallthrough]];
		case LightType::DIRECTIONAL_LIGHT: {
			const uint32_t shadowMatrixIndex = static_cast<uint32_t>(light.shadowMatrixIndex);

			// Fit shadow area to scene AABB.
			const Box<3, float> sceneAABB = totalShadowCasterBoundingBox;
			const Array sceneAABBCorners{
				vec3{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.min.z},
				vec3{sceneAABB.min.x, sceneAABB.min.y, sceneAABB.max.z},
				vec3{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.min.z},
				vec3{sceneAABB.min.x, sceneAABB.max.y, sceneAABB.max.z},
				vec3{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.min.z},
				vec3{sceneAABB.max.x, sceneAABB.min.y, sceneAABB.max.z},
				vec3{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.min.z},
				vec3{sceneAABB.max.x, sceneAABB.max.y, sceneAABB.max.z},
			};

			const vec3 up = (light.direction.x == 0.0f && light.direction.z == 0.0f) ? vec3{0.0f, 0.0f, 1.0f} : vec3{0.0f, 1.0f, 0.0f};
			const mat4 shadowViewMatrix = lookAt(vec3{0.0f, 0.0f, 0.0f}, light.direction, up);

			vec3 shadowViewMin{Limits<float>::MAX};
			vec3 shadowViewMax{Limits<float>::MIN};
			for (const vec3& sceneAABBCorner : sceneAABBCorners) {
				const vec3 sceneAABBCornerInShadowView{shadowViewMatrix * vec4{sceneAABBCorner, 1.0f}};
				shadowViewMin = min(shadowViewMin, sceneAABBCornerInShadowView);
				shadowViewMax = max(shadowViewMax, sceneAABBCornerInShadowView);
			}

			const mat4 shadowProjectionMatrix = ortho(shadowViewMin.x, shadowViewMax.x, shadowViewMin.y, shadowViewMax.y, shadowViewMin.z, shadowViewMax.z);
			for (uint32_t cascadeLevel = 0; cascadeLevel < lights.options.shadowCascadeFrustumFarPlaneDistances.size(); ++cascadeLevel) {
				shadowMapCamera3D.setProjectionAndView(shadowProjectionMatrix, shadowViewMatrix);

				RenderPass renderPass{device, lights.cascadedShadowMaps.getSubresource({.layer = shadowMapIndex + cascadeLevel}), ClearValues{}};
				drawShadowCasters(renderPass, shadowMapCamera3D);
				device.render(renderPass);

				lights.shadowMatrices[shadowMatrixIndex + cascadeLevel].shadowMatrix = BIAS_MATRIX * shadowProjectionMatrix * shadowViewMatrix;
			}
			lights.shadowMatrixBufferDirty = true;
			break;
		}
		case LightType::POINT_LIGHT: [[fallthrough]];
		case LightType::SPOT_LIGHT: break;
	}
}

void Renderer3D::renderAllViewDependentShadowMapsForFullZoomedOutView(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox,
	FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [lightID, light] : lights.lights) {
		renderViewDependentShadowMapForFullZoomedOutView(lights, lightID, totalShadowCasterBoundingBox, drawShadowCasters);
	}
}

void Renderer3D::renderAllShadowMapsForFullZoomedOutView(Lights3D& lights, const Box<3, float>& totalShadowCasterBoundingBox,
	FunctionView<void(RenderPass&, const Camera3D&)> drawShadowCasters) {
	GREM_PROFILE_FUNCTION();

	for (const auto& [lightID, light] : lights.lights) {
		switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
			case LightType::AMBIENT_LIGHT: break;
			case LightType::SUN_LIGHT: [[fallthrough]];
			case LightType::DIRECTIONAL_LIGHT: renderViewDependentShadowMapForFullZoomedOutView(lights, lightID, totalShadowCasterBoundingBox, drawShadowCasters); break;
			case LightType::POINT_LIGHT: [[fallthrough]];
			case LightType::SPOT_LIGHT: renderViewIndependentShadowMap(lights, lightID, drawShadowCasters); break;
		}
	}
}

Texture Renderer3D::generateSpecularSplitSumBRDFIntegrationMap(Device& device, Renderer2D& renderer2D, uint32_t resolution, uint32_t sampleCount) {
	Texture result = Texture::create(device, TextureType::TEXTURE_2D, TextureFormat::R16G16_FLOAT, Extent2D{resolution}, 1, UndefinedClearValues{},
		TextureSamplerOptions{
			.minificationFilter = TextureFilter::LINEAR,
			.magnificationFilter = TextureFilter::LINEAR,
			.mipmapMode = TextureMipmapMode::NONE,
			.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
			.maxAnisotropy = 1.0f,
		});

	Instances2D instances{device, renderer2D};
	instances.putShadedRectangleInstance(
		Model2D::ShaderPipeline{
			device,
			renderer2D.getDefaultModel2DVertexShader(),
			Model2D::DEFAULT_VERTEX_SHADER_CONSTANTS,
			SpecularSplitSumBRDFIntegrationMapFragmentShader::create(device, detail::RENDERER_3D_SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_FRAGMENT_SHADER_CODE),
			SpecularSplitSumBRDFIntegrationMapFragmentShaderConstants{.SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_SAMPLE_COUNT = sampleCount},
			Model2D::DEFAULT_SHADER_PIPELINE_OPTIONS,
		},
		{.size = result.getSize2D()});

	RenderPass renderPass{device, result, UndefinedClearValues{}};
	renderer2D.drawFrame(renderPass, {instances}, Camera2D{device, OrthographicProjection2D{.size = result.getSize2D()}});
	device.render(renderPass);
	return result;
}

void Renderer3D::drawFrameImplementation(RenderPass& renderPass, StridedSpan<const Instances3DView> instanceBatches, const Camera3D& camera,
	Span<const Pair<BufferLayoutReference, SharedPointer<void>>> extraBufferHandles, FrameOptions frameOptions) {
	GREM_PROFILE_FUNCTION();

	transparentDrawCommandBuffer3D.clear();
	transparentDrawCommandBuffer2D.clear();
	combined2DAnd3DTransparentDrawCommands.clear();

	const auto pushTransparent2DDrawCommand =
		[&](const SharedPointer<ShaderPipelineImplementation>& shaderPipelineOverride2DHandle, const InstanceBuffer<Model2D::Instance>& instanceBuffer,
			Span<const Instances3D::Transparent2DDrawCommand> drawCommands, uint32_t drawCommandIndex) -> void {
		const Instances3D::Transparent2DDrawCommand& drawCommand = drawCommands[drawCommandIndex];
		textureBuffer2D.upload(Model2D::TextureParameters{.mainTexture = drawCommand.textureHandle});
		transformation3DBuffer2D.upload(Model2D::Transformation3DParameters{.transformation3DTransformation = drawCommand.transformation});
		const uint32_t instancesEnd =
			(drawCommandIndex + 1 < static_cast<uint32_t>(drawCommands.size())) ? drawCommands[drawCommandIndex + 1].instanceOffset : instanceBuffer.size();
		transparentDrawCommandBuffer2D.append(drawCommand.shaderPipelineHandle, drawCommand.meshHandle, drawCommand.instanceOffset, instancesEnd - drawCommand.instanceOffset);
		setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer(), textureBuffer2D, transformation3DBuffer2D);
		renderPass.drawShaded(shaderPipelineOverride2DHandle, transparentDrawCommandBuffer2D.lock(), instanceBuffer.lock(), temporaryCombinedBufferHandles);
		transparentDrawCommandBuffer2D.clear();
	};

	struct TransparentDrawCommandInfo {
		vec3 position;
		float boundingRadius;
		float distanceOrderingBias;
	};

	const auto getTransparentDrawCommandInfo = [&](const TransparentDrawCommandReference& drawCommandReference) -> TransparentDrawCommandInfo {
		const Instances3D& instances = *instanceBatches[drawCommandReference.instanceBatchIndex].instances;
		if (drawCommandReference.is3D) {
			const Instances3D::Transparent3DDrawCommand& drawCommand = instances.transparent3DDrawCommands[drawCommandReference.drawCommandIndex];
			return {drawCommand.position, drawCommand.boundingRadius, drawCommand.distanceOrderingBias};
		}
		const Instances3D::Transparent2DDrawCommand& drawCommand = instances.transparent2DDrawCommands[drawCommandReference.drawCommandIndex];
		return {vec3{drawCommand.transformation[3]}, drawCommand.boundingRadius, drawCommand.distanceOrderingBias};
	};

	const uint16_t instanceBatchCount = static_cast<uint16_t>(instanceBatches.size());
	for (uint16_t instanceBatchIndex = 0; instanceBatchIndex < instanceBatchCount; ++instanceBatchIndex) {
		const Instances3DView& instanceBatch = instanceBatches[instanceBatchIndex];
		GREM_ASSERT(instanceBatch.instances);
		const Instances3D& instances = *instanceBatch.instances;

		if (!instanceBatch.filter.skipAll3DInstances) {
			if (instances.jointsDirty) {
				instances.buffers.upload<Model3D::JointBuffer>(instances.joints);
				instances.jointsDirty = false;
			}

			if (instances.morphTargetWeightsDirty) {
				instances.buffers.upload<Model3D::MorphTargetWeightBuffer>(instances.morphTargetWeights);
				instances.morphTargetWeightsDirty = false;
			}

			setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer(), model3DDataBuffers, instances.buffers);

			if (!instanceBatch.filter.skipOpaqueModelMeshInstances) {
				renderPass.drawShadedUnordered(instanceBatch.shaderPipelineOverride3DHandle, instances.opaqueModel3DDrawCommandBuffer.lock(),
					instances.model3DInstanceBuffer.lock(), temporaryCombinedBufferHandles);
			}

			if (!instanceBatch.filter.skipAlphaBlendedModelMeshInstances) {
				if (frameOptions.unordered) {
					for (const Instances3D::Transparent3DDrawCommand& drawCommand : instances.transparent3DDrawCommands) {
						transparentDrawCommandBuffer3D.push(drawCommand.shaderPipelineHandle, drawCommand.meshHandle, drawCommand.instanceIndex);
					}
					renderPass.drawShaded(instanceBatch.shaderPipelineOverride3DHandle, transparentDrawCommandBuffer3D.lock(), instances.model3DInstanceBuffer.lock(),
						temporaryCombinedBufferHandles);
					transparentDrawCommandBuffer3D.clear();
				} else {
					const uint32_t drawCommandCount = static_cast<uint32_t>(instances.transparent3DDrawCommands.size());
					for (uint32_t drawCommandIndex = 0; drawCommandIndex < drawCommandCount; ++drawCommandIndex) {
						const TransparentDrawCommandReference drawCommandReference{
							.is3D = true,
							.instanceBatchIndex = instanceBatchIndex,
							.drawCommandIndex = drawCommandIndex,
						};
						const TransparentDrawCommandInfo info = getTransparentDrawCommandInfo(drawCommandReference);
						if (instanceBatch.filter.transparentInstanceFilter(info.position, info.boundingRadius, info.distanceOrderingBias)) {
							combined2DAnd3DTransparentDrawCommands.push_back(drawCommandReference);
						}
					}
				}
			}
		}

		if (!instanceBatch.filter.skipAll2DInstances) {
			const uint32_t drawCommandCount = static_cast<uint32_t>(instances.transparent2DDrawCommands.size());
			if (frameOptions.unordered) {
				for (uint32_t drawCommandIndex = 0; drawCommandIndex < drawCommandCount; ++drawCommandIndex) {
					pushTransparent2DDrawCommand(instanceBatch.shaderPipelineOverride2DHandle, instances.model2DInstanceBuffer, instances.transparent2DDrawCommands,
						drawCommandIndex);
				}
			} else {
				for (uint32_t drawCommandIndex = 0; drawCommandIndex < drawCommandCount; ++drawCommandIndex) {
					const TransparentDrawCommandReference drawCommandReference{
						.is3D = false,
						.instanceBatchIndex = instanceBatchIndex,
						.drawCommandIndex = drawCommandIndex,
					};
					const TransparentDrawCommandInfo info = getTransparentDrawCommandInfo(drawCommandReference);
					if (instanceBatch.filter.transparentInstanceFilter(info.position, info.boundingRadius, info.distanceOrderingBias)) {
						combined2DAnd3DTransparentDrawCommands.push_back(drawCommandReference);
					}
				}
			}
		}
	}

	if (frameOptions.drawSky) {
		// Draw the sky AFTER opaque instances to reduce overdraw.
		Optional<DrawCommandBuffer<Cubemap3D::Mesh>>& drawCommandBuffer = (frameOptions.hdr) ? hdrSkyDrawCommandBuffer : skyDrawCommandBuffer;
		if (!drawCommandBuffer) {
			const Cubemap3D::ShaderPipeline& shaderPipeline = (frameOptions.hdr) ? getHDRPBRSky3DShaderPipeline() : getPBRSky3DShaderPipeline();
			drawCommandBuffer.emplace(device);
			drawCommandBuffer->push(shaderPipeline, getCubemap3D().getMesh());
		}
		setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer());
		renderPass.drawShaded(std::move(frameOptions.skyShaderPipelineOverrideHandle), drawCommandBuffer->lock(), {}, temporaryCombinedBufferHandles);
	}

	if (combined2DAnd3DTransparentDrawCommands.empty()) {
		return;
	}

	const vec3 cameraPosition = transpose(mat3{camera.getViewMatrix()}) * -vec3{camera.getViewMatrix()[3]};
	sort(combined2DAnd3DTransparentDrawCommands, [&](const TransparentDrawCommandReference& a, const TransparentDrawCommandReference& b) -> bool {
		const TransparentDrawCommandInfo infoA = getTransparentDrawCommandInfo(a);
		const TransparentDrawCommandInfo infoB = getTransparentDrawCommandInfo(b);
		return distance(infoA.position, cameraPosition) - infoA.distanceOrderingBias > distance(infoB.position, cameraPosition) - infoB.distanceOrderingBias;
	});

	bool is3D = combined2DAnd3DTransparentDrawCommands.front().is3D;
	uint16_t instanceBatchIndex = combined2DAnd3DTransparentDrawCommands.front().instanceBatchIndex;

	const auto commit3DDrawCommands = [&]() -> void {
		GREM_ASSERT(is3D);
		const Instances3DView& instanceBatch = instanceBatches[instanceBatchIndex];
		GREM_ASSERT(instanceBatch.instances);
		const Instances3D& instances = *instanceBatch.instances;
		setTemporaryCombinedBufferHandles(extraBufferHandles, camera.getParameterBuffer(), model3DDataBuffers, instances.buffers);
		renderPass.drawShaded(instanceBatch.shaderPipelineOverride3DHandle, transparentDrawCommandBuffer3D.lock(), instances.model3DInstanceBuffer.lock(),
			temporaryCombinedBufferHandles);
		transparentDrawCommandBuffer3D.clear();
	};

	for (const TransparentDrawCommandReference& drawCommandReference : combined2DAnd3DTransparentDrawCommands) {
		if (is3D && (!drawCommandReference.is3D || drawCommandReference.instanceBatchIndex != instanceBatchIndex)) {
			commit3DDrawCommands();
		}
		const Instances3DView& instanceBatch = instanceBatches[drawCommandReference.instanceBatchIndex];
		GREM_ASSERT(instanceBatch.instances);
		const Instances3D& instances = *instanceBatch.instances;
		if (drawCommandReference.is3D) {
			const Instances3D::Transparent3DDrawCommand& drawCommand = instances.transparent3DDrawCommands[drawCommandReference.drawCommandIndex];
			transparentDrawCommandBuffer3D.push(drawCommand.shaderPipelineHandle, drawCommand.meshHandle, drawCommand.instanceIndex);
		} else {
			pushTransparent2DDrawCommand(instanceBatch.shaderPipelineOverride2DHandle, instances.model2DInstanceBuffer, instances.transparent2DDrawCommands,
				drawCommandReference.drawCommandIndex);
		}
		is3D = drawCommandReference.is3D;
		instanceBatchIndex = drawCommandReference.instanceBatchIndex;
	}
	if (is3D) {
		commit3DDrawCommands();
	}
}

void Renderer3D::flushPBRBuffers(Extent2D framebufferSize, const Viewport& viewport, const Camera3D& camera, const Fog3D& fog, const Sky3D& sky, const Decals3D& decals,
	const Lights3D& lights, const LightProbeVolumes3D& lightProbeVolumes, const ReflectionProbes3D& reflectionProbes) {
	GREM_PROFILE_FUNCTION();

	GREM_ASSERT(options.tileSize > 0);

	if (pbrParameterBufferDirty) {
		pbrParameterBuffer.upload(PBRParameters{
			.pbrSpecularSplitSumBRDFIntegrationMap = getSpecularSplitSumBRDFIntegrationMap(),
		});
		pbrParameterBufferDirty = false;
	}

	uint32_t tileExtent = options.tileSize;
	uint32_t tileCountX = 0;
	uint32_t tileCountY = 0;
	while (true) {
		tileCountX = (viewport.region.size.width + tileExtent - 1) / tileExtent;
		tileCountY = (viewport.region.size.height + tileExtent - 1) / tileExtent;
		if (static_cast<size_t>(tileCountX) * static_cast<size_t>(tileCountY) <= ScreenTileParameters::MAX_TILE_COUNT) {
			break;
		}
		tileExtent *= 2;
	}

	Arena<0>& arena = flushPBRBuffersArena;
	arena.release();

	fog.flush();
	sky.flush(*this);
	decals.flush(*this);
	lights.flush(device);
	lightProbeVolumes.flush(device, *this);
	reflectionProbes.flush(device, *this);

	const mat4 projectionMatrix = camera.getProjectionMatrix();
	const mat4 viewMatrix = camera.getViewMatrix();
	const float nearZ = projectionMatrix[3][2] / projectionMatrix[2][2];
	const float farZ = (projectionMatrix[2][2] == -1.0f) ? Limits<float>::MAX : projectionMatrix[3][2] / (projectionMatrix[2][2] + 1.0f);
	const float tileSize = static_cast<float>(tileExtent);
	const vec2 viewportSize{viewport.region.size};
	const float framebufferHeight = static_cast<float>(framebufferSize.height + (framebufferSize.height & 1));
	const uint32_t depthBinCount = min(options.depthBinCount, uint32_t{ScreenDepthBinParameters::MAX_DEPTH_BIN_COUNT});
	const float depthBinCountFloat = static_cast<float>(depthBinCount);
	const float depthRangeBegin = nearZ;
	const float depthRangeLength = farZ - depthRangeBegin;
	const float inverseDepthBinLength = depthBinCountFloat / depthRangeLength;

	struct DepthBinnedRectangularScreenItem {
		Box<2, float> shapeInScreenSpace;
		uint32_t depthBinsBegin;
		uint32_t depthBinsEnd;
		const void* data;

		[[nodiscard]] bool intersectsTile(uint32_t x, uint32_t y, float tileSize) const {
			return intersects(shapeInScreenSpace, Rectangle<float>{.position = vec2{static_cast<float>(x), static_cast<float>(y)} * tileSize, .size{tileSize, tileSize}});
		}
	};

	const auto createDepthBinnedRectangularScreenItem = [&](vec3 position, float radius, const void* data) -> Optional<DepthBinnedRectangularScreenItem> {
		const vec4 centerInViewSpace = viewMatrix * vec4{position, 1.0f};

		const float depth = -centerInViewSpace.z - depthRangeBegin;
		const float minDepthIndex = floor((depth - radius) * inverseDepthBinLength);
		const float maxDepthIndex = ceil((depth + radius) * inverseDepthBinLength);
		if (maxDepthIndex <= 0.0f || minDepthIndex >= depthBinCountFloat) {
			return {};
		}

		const auto getAxisBoundsInViewSpace = [&](vec2 axis) -> Pair<vec3> {
			const vec2 projectedCenter{dot(axis, vec2{centerInViewSpace.x, centerInViewSpace.y}), centerInViewSpace.z};
			const float distanceSquared = length2(projectedCenter);
			const float differenceOfSquares = distanceSquared - length2(radius);
			const bool containingCamera = differenceOfSquares <= 0.0f;
			const bool behindCamera = projectedCenter.y + radius >= -nearZ;
			const vec2 direction = (containingCamera) ? vec2{} : vec2{sqrt(differenceOfSquares), radius} / sqrt(distanceSquared);
			const float discriminantSqrt = sqrt(length2(radius) - length2(-nearZ - projectedCenter.y));
			vec2 lowerBound{};
			vec2 upperBound{};
			if (!containingCamera) {
				lowerBound = mat2{direction.x, -direction.y, direction.y, direction.x} * projectedCenter * direction.x;
			}
			if (behindCamera && (containingCamera || lowerBound.y > -nearZ)) {
				lowerBound = vec2{projectedCenter.x - discriminantSqrt, -nearZ};
			}
			if (!containingCamera) {
				upperBound = mat2{direction.x, direction.y, -direction.y, direction.x} * projectedCenter * direction.x;
			}
			if (behindCamera && (containingCamera || upperBound.y > -nearZ)) {
				upperBound = vec2{projectedCenter.x + discriminantSqrt, -nearZ};
			}
			return {vec3{lowerBound.x * axis, lowerBound.y}, vec3{upperBound.x * axis, upperBound.y}};
		};

		const auto screenFromViewSpace = [&](vec3 pointInViewSpace) -> vec2 {
			const vec4 pointInClipSpace = projectionMatrix * vec4{pointInViewSpace, 1.0f};
			const vec2 pointInNDCSpace = vec2{pointInClipSpace} / pointInClipSpace.w;
			const vec2 pointInScreenSpace = (vec2{0.5f} + (pointInNDCSpace * 0.5f)) * viewportSize;
			return pointInScreenSpace;
		};

		const auto [left, right] = getAxisBoundsInViewSpace(vec2{1.0f, 0.0f});
		const auto [bottom, top] = getAxisBoundsInViewSpace(vec2{0.0f, 1.0f});
		const Box<2, float> shapeInScreenSpace{
			.min{screenFromViewSpace(left).x, screenFromViewSpace(bottom).y},
			.max{screenFromViewSpace(right).x, screenFromViewSpace(top).y},
		};

		if (!intersects(shapeInScreenSpace, Rectangle<float>{.position{0.0f, 0.0f}, .size = viewportSize})) {
			return {};
		}

		return DepthBinnedRectangularScreenItem{
			.shapeInScreenSpace = shapeInScreenSpace,
			.depthBinsBegin = (minDepthIndex <= 0.0f) ? 0 : min(static_cast<uint32_t>(minDepthIndex), depthBinCount),
			.depthBinsEnd = min(static_cast<uint32_t>(maxDepthIndex), depthBinCount),
			.data = data,
		};
	};

	struct RectangularScreenItem {
		Box<2, float> shapeInScreenSpace;
		const void* data;

		[[nodiscard]] bool intersectsTile(uint32_t x, uint32_t y, float tileSize) const {
			return intersects(shapeInScreenSpace, Rectangle<float>{.position = vec2{static_cast<float>(x), static_cast<float>(y)} * tileSize, .size{tileSize, tileSize}});
		}
	};

	const auto createRectangularScreenItem = [&](Span<const vec3, 8> corners, const void* data) -> Optional<RectangularScreenItem> {
		Box<2, float> shapeInScreenSpace{.min{Limits<float>::MAX}, .max{Limits<float>::MIN}};
		bool withinLeft = false;
		bool withinRight = false;
		bool withinBottom = false;
		bool withinTop = false;
		bool withinNear = false;
		bool withinFar = false;
		for (size_t i = 0; i < 8; ++i) {
			const vec4 cornerPositionInViewSpace = viewMatrix * vec4{corners[i], 1.0f};
			const vec4 cornerPositionInClipSpace = projectionMatrix * cornerPositionInViewSpace;
			vec3 cornerPositionInNDCSpace = vec3{cornerPositionInClipSpace} / cornerPositionInClipSpace.w;
			if (cornerPositionInClipSpace.x >= -cornerPositionInClipSpace.w) {
				withinLeft = true;
			} else {
				cornerPositionInNDCSpace.x = -1.0f;
			}
			if (cornerPositionInClipSpace.x <= cornerPositionInClipSpace.w) {
				withinRight = true;
			} else {
				cornerPositionInNDCSpace.x = 1.0f;
			}
			if (cornerPositionInClipSpace.y >= -cornerPositionInClipSpace.w) {
				withinBottom = true;
			} else {
				cornerPositionInNDCSpace.y = -1.0f;
			}
			if (cornerPositionInClipSpace.y <= cornerPositionInClipSpace.w) {
				withinTop = true;
			} else {
				cornerPositionInNDCSpace.y = 1.0f;
			}
			if (cornerPositionInClipSpace.z >= 0.0f) {
				withinNear = true;
				const vec2 cornerPositionInScreenSpace = (vec2{0.5f} + vec2{cornerPositionInNDCSpace} * 0.5f) * viewportSize;
				shapeInScreenSpace.min = min(shapeInScreenSpace.min, cornerPositionInScreenSpace);
				shapeInScreenSpace.max = max(shapeInScreenSpace.max, cornerPositionInScreenSpace);
			} else {
				shapeInScreenSpace.min = {};
				shapeInScreenSpace.max = viewportSize;
			}
			withinFar |= cornerPositionInClipSpace.z <= cornerPositionInClipSpace.w;
		}

		if (withinLeft & withinRight & withinBottom & withinTop & withinNear & withinFar) {
			return RectangularScreenItem{.shapeInScreenSpace = shapeInScreenSpace, .data = data};
		}
		return {};
	};

	Buffer<DepthBinnedRectangularScreenItem, ArenaAllocator<DepthBinnedRectangularScreenItem>> idOrderedDecals{&arena};
	idOrderedDecals.reserve(decals.decals.size());
	for (const auto& [decalID, decal] : decals.decals) {
		const vec2 centerRelativeToOrigin = (vec2{0.5f} - decal.origin) * decal.size;
		const float decalRadius = max(length(centerRelativeToOrigin - decal.size * 0.5f), length(centerRelativeToOrigin + decal.size * 0.5f));
		if (const Optional<DepthBinnedRectangularScreenItem> item = createDepthBinnedRectangularScreenItem(decal.position, length(vec2{decal.range, decalRadius}), &decal)) {
			idOrderedDecals.push_back(*item);
		}
	}

	Buffer<const Lights3D::Light*, ArenaAllocator<const Lights3D::Light*>> globalLights{&arena};
	Buffer<DepthBinnedRectangularScreenItem, ArenaAllocator<DepthBinnedRectangularScreenItem>> depthOrderedLights{&arena};
	globalLights.reserve(2);
	depthOrderedLights.reserve(lights.lights.size());
	for (const auto& [lightID, light] : lights.lights) {
		if (light.range <= 0.0f) {
			globalLights.push_back(&light);
		} else if (const Optional<DepthBinnedRectangularScreenItem> item = createDepthBinnedRectangularScreenItem(light.position, light.range, &light)) {
			depthOrderedLights.push_back(*item);
		}
	}
	const uint32_t globalLightCount = static_cast<uint32_t>(globalLights.size());
	sortByAscending<&DepthBinnedRectangularScreenItem::depthBinsBegin>(depthOrderedLights);

	Buffer<RectangularScreenItem, ArenaAllocator<RectangularScreenItem>> volumeOrderedLightProbeVolumes{&arena};
	volumeOrderedLightProbeVolumes.reserve(lightProbeVolumes.volumeOptions.size());
	for (const LightProbeVolumeOptions3D& volumeOptions : lightProbeVolumes.volumeOptions) {
		const vec3 halfExtentsWithBlendMargins = vec3{volumeOptions.probeCounts + u32vec3{2}} * volumeOptions.probeSpacing * 0.5f;
		const Array<vec3, 8> corners{
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{-halfExtentsWithBlendMargins.x, -halfExtentsWithBlendMargins.y, -halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{halfExtentsWithBlendMargins.x, -halfExtentsWithBlendMargins.y, -halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{-halfExtentsWithBlendMargins.x, halfExtentsWithBlendMargins.y, -halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{halfExtentsWithBlendMargins.x, halfExtentsWithBlendMargins.y, -halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{-halfExtentsWithBlendMargins.x, -halfExtentsWithBlendMargins.y, halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{halfExtentsWithBlendMargins.x, -halfExtentsWithBlendMargins.y, halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{-halfExtentsWithBlendMargins.x, halfExtentsWithBlendMargins.y, halfExtentsWithBlendMargins.z}},
			vec3{volumeOptions.center + volumeOptions.orientation * vec3{halfExtentsWithBlendMargins.x, halfExtentsWithBlendMargins.y, halfExtentsWithBlendMargins.z}},
		};
		if (const Optional<RectangularScreenItem> item = createRectangularScreenItem(corners, &volumeOptions)) {
			volumeOrderedLightProbeVolumes.push_back(*item);
		}
	}
	sort(volumeOrderedLightProbeVolumes, [&](const RectangularScreenItem& a, const RectangularScreenItem& b) -> bool {
		const size_t lightProbeVolumeIndexA = static_cast<size_t>(static_cast<const LightProbeVolumeOptions3D*>(a.data) - lightProbeVolumes.volumeOptions.data());
		const size_t lightProbeVolumeIndexB = static_cast<size_t>(static_cast<const LightProbeVolumeOptions3D*>(b.data) - lightProbeVolumes.volumeOptions.data());
		return lightProbeVolumes.worldSpaceVolumes[lightProbeVolumeIndexA] < lightProbeVolumes.worldSpaceVolumes[lightProbeVolumeIndexB];
	});

	Buffer<RectangularScreenItem, ArenaAllocator<RectangularScreenItem>> volumeOrderedReflectionProbes{&arena};
	volumeOrderedReflectionProbes.reserve(reflectionProbes.probeOptions.size());
	for (const ReflectionProbeOptions3D& probeOptions : reflectionProbes.probeOptions) {
		const vec3 affectedRegionCenter = probeOptions.center + probeOptions.orientation * probeOptions.localAffectedRegionOffset;
		const vec3 affectedRegionHalfExtents = probeOptions.localAffectedRegionSize * 0.5f;
		const Array<vec3, 8> corners{
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{-affectedRegionHalfExtents.x, -affectedRegionHalfExtents.y, -affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{affectedRegionHalfExtents.x, -affectedRegionHalfExtents.y, -affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{-affectedRegionHalfExtents.x, affectedRegionHalfExtents.y, -affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{affectedRegionHalfExtents.x, affectedRegionHalfExtents.y, -affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{-affectedRegionHalfExtents.x, -affectedRegionHalfExtents.y, affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{affectedRegionHalfExtents.x, -affectedRegionHalfExtents.y, affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{-affectedRegionHalfExtents.x, affectedRegionHalfExtents.y, affectedRegionHalfExtents.z}},
			vec3{affectedRegionCenter + probeOptions.orientation * vec3{affectedRegionHalfExtents.x, affectedRegionHalfExtents.y, affectedRegionHalfExtents.z}},
		};
		if (const Optional<RectangularScreenItem> item = createRectangularScreenItem(corners, &probeOptions)) {
			volumeOrderedReflectionProbes.push_back(*item);
		}
	}
	sort(volumeOrderedReflectionProbes, [&](const RectangularScreenItem& a, const RectangularScreenItem& b) -> bool {
		const size_t reflectionProbeIndexA = static_cast<size_t>(static_cast<const ReflectionProbeOptions3D*>(a.data) - reflectionProbes.probeOptions.data());
		const size_t reflectionProbeIndexB = static_cast<size_t>(static_cast<const ReflectionProbeOptions3D*>(b.data) - reflectionProbes.probeOptions.data());
		return reflectionProbes.worldSpaceBoxVolumes[reflectionProbeIndexA] < reflectionProbes.worldSpaceBoxVolumes[reflectionProbeIndexB];
	});

	UniquePointer<ScreenTileParameters, ArenaDeleter<ScreenTileParameters>> screenTiles{};
	UniquePointer<ScreenDepthBinParameters, ArenaDeleter<ScreenDepthBinParameters>> screenDepthBins{};
	Buffer<ScreenItemFields, ArenaAllocator<ScreenItemFields>> screenItems{&arena};
	Buffer<ScreenDecalFields, ArenaAllocator<ScreenDecalFields>> screenDecals{&arena};
	Buffer<ScreenLightFields, ArenaAllocator<ScreenLightFields>> screenLights{&arena};

	screenTiles.reset(new (ArenaAllocator<ScreenTileParameters>{&arena}.allocate(1)) ScreenTileParameters);             // NOLINT(cppcoreguidelines-owning-memory)
	screenDepthBins.reset(new (ArenaAllocator<ScreenDepthBinParameters>{&arena}.allocate(1)) ScreenDepthBinParameters); // NOLINT(cppcoreguidelines-owning-memory)
	fill(Span{screenDepthBins->depthBins}.first(depthBinCount), u32vec4{Limits<uint32_t>::MAX, 0, Limits<uint32_t>::MAX, 0});
	screenItems.reserve(min(idOrderedDecals.size(), size_t{0xFF}) +                //
						min(depthOrderedLights.size(), size_t{0xFF}) +             //
						min(volumeOrderedLightProbeVolumes.size(), size_t{0xFF}) + //
						min(volumeOrderedReflectionProbes.size(), size_t{0xFF}));
	screenDecals.reserve(idOrderedDecals.size());
	screenLights.reserve(globalLights.size() + depthOrderedLights.size());

	for (uint32_t itemIndex = 0; itemIndex < idOrderedDecals.size(); ++itemIndex) {
		const DepthBinnedRectangularScreenItem& item = idOrderedDecals[itemIndex];
		for (uint32_t z = item.depthBinsBegin; z < item.depthBinsEnd; ++z) {
			u32vec4& depthBin = screenDepthBins->depthBins[z];
			uint32_t& depthBinDecalsBegin = depthBin.x;
			uint32_t& depthBinDecalsEnd = depthBin.y;
			depthBinDecalsBegin = min(depthBinDecalsBegin, itemIndex);
			depthBinDecalsEnd = max(depthBinDecalsEnd, itemIndex + 1);
		}
	}
	for (uint32_t itemIndex = 0; itemIndex < depthOrderedLights.size(); ++itemIndex) {
		const DepthBinnedRectangularScreenItem& item = depthOrderedLights[itemIndex];
		for (uint32_t z = item.depthBinsBegin; z < item.depthBinsEnd; ++z) {
			u32vec4& depthBin = screenDepthBins->depthBins[z];
			uint32_t& depthBinLightsBegin = depthBin.z;
			uint32_t& depthBinLightsEnd = depthBin.w;
			depthBinLightsBegin = min(depthBinLightsBegin, globalLightCount + itemIndex);
			depthBinLightsEnd = max(depthBinLightsEnd, globalLightCount + itemIndex + 1);
		}
	}

	for (uint32_t y = 0; y < tileCountY; ++y) {
		for (uint32_t x = 0; x < tileCountX; ++x) {
			const uint32_t itemOffset = static_cast<uint32_t>(screenItems.size());
			uint32_t decalCount = 0;
			uint32_t lightCount = 0;
			uint32_t lightProbeVolumeCount = 0;
			uint32_t reflectionProbeCount = 0;

			for (size_t i = 0; i < idOrderedDecals.size() && decalCount < 0xFF; ++i) {
				const DepthBinnedRectangularScreenItem& item = idOrderedDecals[i];
				if (item.intersectsTile(x, y, tileSize)) {
					const uint32_t itemIndex = static_cast<uint32_t>(i);
					screenItems.push_back(ScreenItemFields{.itemIndex = itemIndex});
					++decalCount;
				}
			}
			for (size_t i = 0; i < depthOrderedLights.size() && lightCount < 0xFF; ++i) {
				const DepthBinnedRectangularScreenItem& item = depthOrderedLights[i];
				if (item.intersectsTile(x, y, tileSize)) {
					const uint32_t itemIndex = globalLightCount + static_cast<uint32_t>(i);
					screenItems.push_back(ScreenItemFields{.itemIndex = itemIndex});
					++lightCount;
				}
			}
			for (size_t i = 0; i < volumeOrderedLightProbeVolumes.size() && lightProbeVolumeCount < 0xFF; ++i) {
				const RectangularScreenItem& item = volumeOrderedLightProbeVolumes[i];
				if (item.intersectsTile(x, y, tileSize)) {
					const uint32_t itemIndex = static_cast<uint32_t>(static_cast<const LightProbeVolumeOptions3D*>(item.data) - lightProbeVolumes.volumeOptions.data());
					screenItems.push_back(ScreenItemFields{.itemIndex = itemIndex});
					++lightProbeVolumeCount;
				}
			}
			for (size_t i = 0; i < volumeOrderedReflectionProbes.size() && reflectionProbeCount < 0xFF; ++i) {
				const RectangularScreenItem& item = volumeOrderedReflectionProbes[i];
				if (item.intersectsTile(x, y, tileSize)) {
					const uint32_t itemIndex = static_cast<uint32_t>(static_cast<const ReflectionProbeOptions3D*>(item.data) - reflectionProbes.probeOptions.data());
					screenItems.push_back(ScreenItemFields{.itemIndex = itemIndex});
					++reflectionProbeCount;
				}
			}

			const size_t tileOffset = (static_cast<size_t>(y) * static_cast<size_t>(tileCountX) + static_cast<size_t>(x)) * 2;
			screenTiles->tileItemOffsetsAndCountsBy4s[(tileOffset + 0) / 4][(tileOffset + 0) % 4] = itemOffset;
			screenTiles->tileItemOffsetsAndCountsBy4s[(tileOffset + 1) / 4][(tileOffset + 1) % 4] =
				(decalCount << 24) | (lightCount << 16) | (lightProbeVolumeCount << 8) | reflectionProbeCount;
		}
	}

	for (const DepthBinnedRectangularScreenItem& item : idOrderedDecals) {
		const Decals3D::Decal& decal = *static_cast<const Decals3D::Decal*>(item.data);
		const vec2 origin = decal.origin * decal.size;
		const mat4 projectionMatrix = ortho(-origin.x, decal.size.x - origin.x, -origin.y, decal.size.y - origin.y, -decal.range, decal.range);
		const vec3 direction = decal.orientation * vec3{0.0f, 0.0f, -1.0f};
		const vec3 up = decal.orientation * vec3{0.0f, 1.0f, 0.0f};
		const mat4 viewMatrix = lookAt(decal.position, decal.position + direction, up);
		const vec2 inverseAtlasTextureSize = 1.0f / vec2{decals.atlasTexture.getSize2D()};
		const Decals3D::DecalMaterial& decalMaterial = decals.decalMaterials[decal.materialID.index];
		const vec4 baseColorTextureOffsetAndScale = decalMaterial.baseColorMapPositionAndSize * vec4{inverseAtlasTextureSize, inverseAtlasTextureSize};
		const vec4 normalTextureOffsetAndScale = decalMaterial.normalMapPositionAndSize * vec4{inverseAtlasTextureSize, inverseAtlasTextureSize};
		const vec4 occlusionRoughnessMetallicTextureOffsetAndScale =
			decalMaterial.occlusionRoughnessMetallicMapPositionAndSize * vec4{inverseAtlasTextureSize, inverseAtlasTextureSize};
		const vec4 emissiveTextureOffsetAndScale = decalMaterial.emissiveMapPositionAndSize * vec4{inverseAtlasTextureSize, inverseAtlasTextureSize};
		screenDecals.push_back(ScreenDecalFields{
			.decalMatrix = BIAS_MATRIX * projectionMatrix * viewMatrix,
			.decalDirectionAndRange = vec4{direction, decal.range},
			.decalBaseColorTextureOffsetAndScale = baseColorTextureOffsetAndScale,
			.decalNormalTextureOffsetAndScale = normalTextureOffsetAndScale,
			.decalOcclusionRoughnessMetallicTextureOffsetAndScale = occlusionRoughnessMetallicTextureOffsetAndScale,
			.decalEmissiveTextureOffsetAndScale = emissiveTextureOffsetAndScale,
			.decalBaseColorFactor = decalMaterial.baseColorFactor * decal.color.toLinearRGBA(),
			.decalOcclusionRoughnessMetallicFactorAndNormalScale{decalMaterial.occlusionRoughnessMetallicFactor, decalMaterial.normalScale},
			.decalEmissiveFactor = decalMaterial.emissiveFactor * decal.emissiveFactor,
			.decalModelInstanceIdentifier = decal.modelInstanceIdentifier,
		});
	}

	for (const Lights3D::Light* const light : globalLights) {
		screenLights.push_back(ScreenLightFields{
			.lightDirectionAndRange{light->direction, light->range},
			.lightColorAndIntensity = light->color.toLinearRGBA(),
			.lightPositionAndType{light->position, light->type},
			.lightConeCosinesAndShadowMapIndexAndShadowMatrixIndex{
				light->innerConeCosine,
				light->outerConeCosine,
				light->shadowMapIndex,
				light->shadowMatrixIndex,
			},
			.lightShadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors{
				light->shadowNearZ,
				light->shadowFarZ,
				light->shadowMapNormalOffsetBiasConstantFactor,
				light->shadowMapNormalOffsetBiasSlopeFactor,
			},
		});
	}
	for (const DepthBinnedRectangularScreenItem& item : depthOrderedLights) {
		const Lights3D::Light& light = *static_cast<const Lights3D::Light*>(item.data);
		screenLights.push_back(ScreenLightFields{
			.lightDirectionAndRange{light.direction, light.range},
			.lightColorAndIntensity = light.color.toLinearRGBA(),
			.lightPositionAndType{light.position, light.type},
			.lightConeCosinesAndShadowMapIndexAndShadowMatrixIndex{
				light.innerConeCosine,
				light.outerConeCosine,
				light.shadowMapIndex,
				light.shadowMatrixIndex,
			},
			.lightShadowNearAndFarPlaneDistancesAndShadowMapNormalOffsetBiasConstantAndSlopeFactors{
				light.shadowNearZ,
				light.shadowFarZ,
				light.shadowMapNormalOffsetBiasConstantFactor,
				light.shadowMapNormalOffsetBiasSlopeFactor,
			},
		});
	}

	screenBuffers.upload<ScreenTileBuffer>(*screenTiles);
	screenBuffers.upload<ScreenDepthBinBuffer>(*screenDepthBins);
	screenBuffers.upload<ScreenItemBuffer>(screenItems);
	screenBuffers.upload<ScreenDecalBuffer>(screenDecals);
	screenBuffers.upload<ScreenLightBuffer>(screenLights);
	screenBuffers.upload<ScreenParameterBuffer>(ScreenParameters{
		.screenViewportOffset{viewport.region.offset},
		.screenFramebufferHeight = framebufferHeight,
		.screenInverseTileSize = 1.0f / tileSize,
		.screenTileCounts{tileCountX, tileCountY},
		.screenDepthBinCount = depthBinCount,
		.screenCascadedShadowMaps = (lights.cascadedShadowMaps) ? lights.cascadedShadowMaps : getDefaultDepthTexture2DArray(),
		.screenPointLightShadowMaps = (lights.pointLightShadowMaps) ? lights.pointLightShadowMaps : getDefaultDepthTextureCubeArray(),
		.screenSpotLightShadowMaps = (lights.spotLightShadowMaps) ? lights.spotLightShadowMaps : getDefaultDepthTexture2DArray(),
		.screenGlobalLightCount = globalLightCount,
	});

	environmentBuffers.setBuffers(pbrParameterBuffer, fog.parameterBuffer, sky.parameterBuffer, decals.parameterBuffer, lights.parameterBuffer, lights.shadowMatrixBuffer,
		lightProbeVolumes.atlasBuffer, lightProbeVolumes.volumeBuffer, reflectionProbes.atlasBuffer, reflectionProbes.probeBuffer);
}

} // namespace grem::graphics
