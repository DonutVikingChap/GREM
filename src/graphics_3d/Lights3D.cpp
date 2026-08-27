// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>

namespace grem::graphics {

Lights3D::Lights3D(Device& device, const Lights3DOptions& options)
	: parameterBuffer(device)
	, shadowMatrixBuffer(device)
	, options(options) {
	GREM_ASSERT(isPowerOf2(options.cascadedShadowMapResolution));
	GREM_ASSERT(isPowerOf2(options.pointLightShadowMapResolution));
	GREM_ASSERT(isPowerOf2(options.spotLightShadowMapResolution));
	GREM_ASSERT(!options.shadowCascadeFrustumFarPlaneDistances.empty());
}

LightID Lights3D::createAmbientLight(const AmbientLightOptions3D& options) {
	return lights
	    .insert(Light{
			.color = options.color,
			.type = static_cast<float>(LightType::AMBIENT_LIGHT),
		})
	    ->first;
}

LightID Lights3D::createSunLight(const SunLightOptions3D& options) {
	if (options.shadowMapped) {
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
	}
	return lights
	    .insert(Light{
			.direction = options.direction,
			.color = options.color,
			.type = static_cast<float>(LightType::SUN_LIGHT),
			.shadowMapIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowMatrixIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowMapNormalOffsetBiasConstantFactor = options.shadowMapNormalOffsetBiasConstantFactor,
			.shadowMapNormalOffsetBiasSlopeFactor = options.shadowMapNormalOffsetBiasSlopeFactor,
		})
	    ->first;
}

LightID Lights3D::createDirectionalLight(const DirectionalLightOptions3D& options) {
	if (options.shadowMapped) {
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
	}
	return lights
	    .insert(Light{
			.direction = options.direction,
			.color = options.color,
			.type = static_cast<float>(LightType::DIRECTIONAL_LIGHT),
			.shadowMapIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowMatrixIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowMapNormalOffsetBiasConstantFactor = options.shadowMapNormalOffsetBiasConstantFactor,
			.shadowMapNormalOffsetBiasSlopeFactor = options.shadowMapNormalOffsetBiasSlopeFactor,
		})
	    ->first;
}

LightID Lights3D::createPointLight(const PointLightOptions3D& options) {
	GREM_ASSERT(options.shadowNearZ < options.shadowFarZ);
	if (options.shadowMapped) {
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
	}
	return lights
	    .insert(Light{
			.range = options.range,
			.color = options.color,
			.position = options.position,
			.type = static_cast<float>(LightType::POINT_LIGHT),
			.shadowMapIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowNearZ = options.shadowNearZ,
			.shadowFarZ = options.shadowFarZ,
			.shadowMapNormalOffsetBiasConstantFactor = options.shadowMapNormalOffsetBiasConstantFactor,
			.shadowMapNormalOffsetBiasSlopeFactor = options.shadowMapNormalOffsetBiasSlopeFactor,
		})
	    ->first;
}

LightID Lights3D::createSpotLight(const SpotLightOptions3D& options) {
	GREM_ASSERT(options.innerConeAngle >= 0.0f && options.innerConeAngle <= numbers::PI);
	GREM_ASSERT(options.outerConeAngle >= 0.0f && options.outerConeAngle <= numbers::PI);
	GREM_ASSERT(options.innerConeAngle <= options.outerConeAngle);
	GREM_ASSERT(options.shadowNearZ < options.shadowFarZ);
	if (options.shadowMapped) {
		shadowMapIndicesDirty = true;
		shadowMatrixIndicesDirty = true;
	}
	return lights
	    .insert(Light{
			.direction = options.direction,
			.range = options.range,
			.color = options.color,
			.position = options.position,
			.type = static_cast<float>(LightType::SPOT_LIGHT),
			.innerConeCosine = cos(options.innerConeAngle),
			.outerConeCosine = cos(options.outerConeAngle),
			.shadowMapIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowMatrixIndex = (options.shadowMapped) ? 0.0f : -1.0f,
			.shadowNearZ = options.shadowNearZ,
			.shadowFarZ = options.shadowFarZ,
			.shadowMapNormalOffsetBiasConstantFactor = options.shadowMapNormalOffsetBiasConstantFactor,
			.shadowMapNormalOffsetBiasSlopeFactor = options.shadowMapNormalOffsetBiasSlopeFactor,
		})
	    ->first;
}

bool Lights3D::containsLight(LightID id) const noexcept {
	return lights.contains(id);
}

bool Lights3D::destroyLight(LightID id) {
	if (const auto it = lights.find(id); it != lights.end()) {
		if (it->second.shadowMatrixIndex >= 0.0f) {
			shadowMapIndicesDirty = true;
			shadowMatrixIndicesDirty = true;
		}
		lights.erase(it);
		return true;
	}
	return false;
}

Optional<LightType> Lights3D::getLightType(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		return static_cast<LightType>(light.type);
	}
	return {};
}

Optional<AmbientLightOptions3D> Lights3D::getAmbientLightOptions(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		if (light.type == static_cast<float>(LightType::AMBIENT_LIGHT)) {
			return AmbientLightOptions3D{
				.color = light.color,
			};
		}
	}
	return {};
}

Optional<SunLightOptions3D> Lights3D::getSunLightOptions(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		if (light.type == static_cast<float>(LightType::SUN_LIGHT)) {
			return SunLightOptions3D{
				.direction = light.direction,
				.color = light.color,
				.shadowMapNormalOffsetBiasConstantFactor = light.shadowMapNormalOffsetBiasConstantFactor,
				.shadowMapNormalOffsetBiasSlopeFactor = light.shadowMapNormalOffsetBiasSlopeFactor,
				.shadowMapped = light.shadowMapIndex >= 0.0f,
			};
		}
	}
	return {};
}

Optional<DirectionalLightOptions3D> Lights3D::getDirectionalLightOptions(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		if (light.type == static_cast<float>(LightType::DIRECTIONAL_LIGHT)) {
			return DirectionalLightOptions3D{
				.direction = light.direction,
				.color = light.color,
				.shadowMapNormalOffsetBiasConstantFactor = light.shadowMapNormalOffsetBiasConstantFactor,
				.shadowMapNormalOffsetBiasSlopeFactor = light.shadowMapNormalOffsetBiasSlopeFactor,
				.shadowMapped = light.shadowMapIndex >= 0.0f,
			};
		}
	}
	return {};
}

Optional<PointLightOptions3D> Lights3D::getPointLightOptions(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		if (light.type == static_cast<float>(LightType::POINT_LIGHT)) {
			return PointLightOptions3D{
				.position = light.position,
				.range = light.range,
				.color = light.color,
				.shadowNearZ = light.shadowNearZ,
				.shadowFarZ = light.shadowFarZ,
				.shadowMapNormalOffsetBiasConstantFactor = light.shadowMapNormalOffsetBiasConstantFactor,
				.shadowMapNormalOffsetBiasSlopeFactor = light.shadowMapNormalOffsetBiasSlopeFactor,
				.shadowMapped = light.shadowMapIndex >= 0.0f,
			};
		}
	}
	return {};
}

Optional<SpotLightOptions3D> Lights3D::getSpotLightOptions(LightID id) const {
	if (const auto it = lights.find(id); it != lights.end()) {
		const Light& light = it->second;
		if (light.type == static_cast<float>(LightType::SPOT_LIGHT)) {
			return SpotLightOptions3D{
				.position = light.position,
				.direction = light.direction,
				.range = light.range,
				.innerConeAngle = acos(light.innerConeCosine),
				.outerConeAngle = acos(light.outerConeCosine),
				.color = light.color,
				.shadowNearZ = light.shadowNearZ,
				.shadowFarZ = light.shadowFarZ,
				.shadowMapNormalOffsetBiasConstantFactor = light.shadowMapNormalOffsetBiasConstantFactor,
				.shadowMapNormalOffsetBiasSlopeFactor = light.shadowMapNormalOffsetBiasSlopeFactor,
				.shadowMapped = light.shadowMapIndex >= 0.0f,
			};
		}
	}
	return {};
}

void Lights3D::setAmbientLightOptions(LightID id, const AmbientLightOptions3D& newOptions) {
	if (const auto it = lights.find(id); it != lights.end()) {
		Light& light = it->second;
		if (light.type == static_cast<float>(LightType::AMBIENT_LIGHT)) {
			light.color = newOptions.color;
		}
	}
}

void Lights3D::setSunLightOptions(LightID id, const SunLightOptions3D& newOptions) {
	if (const auto it = lights.find(id); it != lights.end()) {
		Light& light = it->second;
		if (light.type == static_cast<float>(LightType::SUN_LIGHT)) {
			light.direction = newOptions.direction;
			light.color = newOptions.color;
			light.shadowMapNormalOffsetBiasConstantFactor = newOptions.shadowMapNormalOffsetBiasConstantFactor;
			light.shadowMapNormalOffsetBiasSlopeFactor = newOptions.shadowMapNormalOffsetBiasSlopeFactor;
			if (newOptions.shadowMapped) {
				if (light.shadowMapIndex < 0.0f) {
					light.shadowMapIndex = 0.0f;
					light.shadowMatrixIndex = 0.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			} else {
				if (light.shadowMapIndex >= 0.0f) {
					light.shadowMapIndex = -1.0f;
					light.shadowMatrixIndex = -1.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			}
		}
	}
}

void Lights3D::setDirectionalLightOptions(LightID id, const DirectionalLightOptions3D& newOptions) {
	if (const auto it = lights.find(id); it != lights.end()) {
		Light& light = it->second;
		if (light.type == static_cast<float>(LightType::DIRECTIONAL_LIGHT)) {
			light.direction = newOptions.direction;
			light.color = newOptions.color;
			light.shadowMapNormalOffsetBiasConstantFactor = newOptions.shadowMapNormalOffsetBiasConstantFactor;
			light.shadowMapNormalOffsetBiasSlopeFactor = newOptions.shadowMapNormalOffsetBiasSlopeFactor;
			if (newOptions.shadowMapped) {
				if (light.shadowMapIndex < 0.0f) {
					light.shadowMapIndex = 0.0f;
					light.shadowMatrixIndex = 0.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			} else {
				if (light.shadowMapIndex >= 0.0f) {
					light.shadowMapIndex = -1.0f;
					light.shadowMatrixIndex = -1.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			}
		}
	}
}

void Lights3D::setPointLightOptions(LightID id, const PointLightOptions3D& newOptions) {
	if (const auto it = lights.find(id); it != lights.end()) {
		Light& light = it->second;
		if (light.type == static_cast<float>(LightType::POINT_LIGHT)) {
			light.position = newOptions.position;
			light.range = newOptions.range;
			light.color = newOptions.color;
			light.shadowNearZ = newOptions.shadowNearZ;
			light.shadowFarZ = newOptions.shadowFarZ;
			light.shadowMapNormalOffsetBiasConstantFactor = newOptions.shadowMapNormalOffsetBiasConstantFactor;
			light.shadowMapNormalOffsetBiasSlopeFactor = newOptions.shadowMapNormalOffsetBiasSlopeFactor;
			if (newOptions.shadowMapped) {
				if (light.shadowMapIndex < 0.0f) {
					light.shadowMapIndex = 0.0f;
					shadowMapIndicesDirty = true;
				}
			} else {
				if (light.shadowMapIndex >= 0.0f) {
					light.shadowMapIndex = -1.0f;
					shadowMapIndicesDirty = true;
				}
			}
		}
	}
}

void Lights3D::setSpotLightOptions(LightID id, const SpotLightOptions3D& newOptions) {
	if (const auto it = lights.find(id); it != lights.end()) {
		Light& light = it->second;
		if (light.type == static_cast<float>(LightType::SPOT_LIGHT)) {
			light.position = newOptions.position;
			light.direction = newOptions.direction;
			light.range = newOptions.range;
			light.innerConeCosine = cos(newOptions.innerConeAngle);
			light.outerConeCosine = cos(newOptions.outerConeAngle);
			light.color = newOptions.color;
			light.shadowNearZ = newOptions.shadowNearZ;
			light.shadowFarZ = newOptions.shadowFarZ;
			light.shadowMapNormalOffsetBiasConstantFactor = newOptions.shadowMapNormalOffsetBiasConstantFactor;
			light.shadowMapNormalOffsetBiasSlopeFactor = newOptions.shadowMapNormalOffsetBiasSlopeFactor;
			if (newOptions.shadowMapped) {
				if (light.shadowMapIndex < 0.0f) {
					light.shadowMapIndex = 0.0f;
					light.shadowMatrixIndex = 0.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			} else {
				if (light.shadowMapIndex >= 0.0f) {
					light.shadowMapIndex = -1.0f;
					light.shadowMatrixIndex = -1.0f;
					shadowMapIndicesDirty = true;
					shadowMatrixIndicesDirty = true;
				}
			}
		}
	}
}

void Lights3D::setLightPosition(LightID id, vec3 newPosition) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.position = newPosition;
	}
}

void Lights3D::setLightDirection(LightID id, vec3 newDirection) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.direction = newDirection;
	}
}

void Lights3D::setLightRange(LightID id, float newRange) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.range = newRange;
	}
}

void Lights3D::setLightInnerConeAngle(LightID id, float newInnerConeAngle) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.innerConeCosine = cos(newInnerConeAngle);
	}
}

void Lights3D::setLightOuterConeAngle(LightID id, float newOuterConeAngle) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.outerConeCosine = cos(newOuterConeAngle);
	}
}

void Lights3D::setLightColor(LightID id, Color newColor) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.color = newColor;
	}
}

void Lights3D::setLightShadowNearZ(LightID id, float newShadowNearZ) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.shadowNearZ = newShadowNearZ;
	}
}

void Lights3D::setLightShadowFarZ(LightID id, float newShadowFarZ) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.shadowFarZ = newShadowFarZ;
	}
}

void Lights3D::setLightShadowMapNormalOffsetBiasConstantFactor(LightID id, float newShadowMapNormalOffsetBiasConstantFactor) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.shadowMapNormalOffsetBiasConstantFactor = newShadowMapNormalOffsetBiasConstantFactor;
	}
}

void Lights3D::setLightShadowMapNormalOffsetBiasSlopeFactor(LightID id, float newShadowMapNormalOffsetBiasSlopeFactor) {
	if (const auto it = lights.find(id); it != lights.end()) {
		it->second.shadowMapNormalOffsetBiasSlopeFactor = newShadowMapNormalOffsetBiasSlopeFactor;
	}
}

void Lights3D::setLightShadowMapped(LightID id, bool newShadowMapped) {
	if (const auto it = lights.find(id); it != lights.end()) {
		if (it->second.type == static_cast<float>(LightType::AMBIENT_LIGHT)) {
			return;
		}
		if (newShadowMapped) {
			if (it->second.shadowMapIndex < 0.0f) {
				it->second.shadowMapIndex = 0.0f;
				shadowMapIndicesDirty = true;
				switch (static_cast<LightType>(static_cast<uint32_t>(it->second.type))) {
					case LightType::AMBIENT_LIGHT: unreachable();
					case LightType::SUN_LIGHT: [[fallthrough]];
					case LightType::DIRECTIONAL_LIGHT: [[fallthrough]];
					case LightType::SPOT_LIGHT:
						it->second.shadowMatrixIndex = 0.0f;
						shadowMatrixIndicesDirty = true;
						break;
					case LightType::POINT_LIGHT: break;
				}
			}
		} else {
			if (it->second.shadowMapIndex >= 0.0f) {
				it->second.shadowMapIndex = -1.0f;
				shadowMapIndicesDirty = true;
				switch (static_cast<LightType>(static_cast<uint32_t>(it->second.type))) {
					case LightType::AMBIENT_LIGHT: unreachable();
					case LightType::SUN_LIGHT: [[fallthrough]];
					case LightType::DIRECTIONAL_LIGHT: [[fallthrough]];
					case LightType::SPOT_LIGHT:
						it->second.shadowMatrixIndex = -1.0f;
						shadowMatrixIndicesDirty = true;
						break;
					case LightType::POINT_LIGHT: break;
				}
			}
		}
	}
}

void Lights3D::flushIndices(Device& device) const {
	if (shadowMapIndicesDirty) {
		uint32_t cascadedShadowMapCount = 0;
		uint32_t pointLightShadowMapCount = 0;
		uint32_t spotLightShadowMapCount = 0;
		for (auto&& [lightID, light] : lights) {
			if (light.shadowMapIndex >= 0.0f) {
				uint32_t shadowMapIndex{};
				switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
					case LightType::AMBIENT_LIGHT: unreachable();
					case LightType::SUN_LIGHT:
					case LightType::DIRECTIONAL_LIGHT:
						shadowMapIndex = cascadedShadowMapCount;
						cascadedShadowMapCount += static_cast<uint32_t>(options.shadowCascadeFrustumFarPlaneDistances.size());
						break;
					case LightType::POINT_LIGHT: shadowMapIndex = pointLightShadowMapCount++; break;
					case LightType::SPOT_LIGHT: shadowMapIndex = spotLightShadowMapCount++; break;
				}
				light.shadowMapIndex = static_cast<float>(shadowMapIndex);
			}
		}

		if (cascadedShadowMapCount > cascadedShadowMapCapacity) {
			cascadedShadowMapCapacity = cascadedShadowMapCount;
			cascadedShadowMaps = Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::D16_UNORM,
				Extent3D{
					.width = options.cascadedShadowMapResolution,
					.height = options.cascadedShadowMapResolution,
					.depth = cascadedShadowMapCapacity,
				},
				1, ClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
					.depthComparisonMode = TextureDepthComparisonMode::LESS_OR_EQUAL,
				});
		}

		if (pointLightShadowMapCount > pointLightShadowMapCapacity) {
			pointLightShadowMapCapacity = pointLightShadowMapCount;
			pointLightShadowMaps = Texture::create(device, TextureType::TEXTURE_CUBE_ARRAY, TextureFormat::D16_UNORM,
				Extent3D{options.pointLightShadowMapResolution, options.pointLightShadowMapResolution, pointLightShadowMapCapacity * 6}, 1, ClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
					.depthComparisonMode = TextureDepthComparisonMode::LESS_OR_EQUAL,
				});
		}

		if (spotLightShadowMapCount > spotLightShadowMapCapacity) {
			spotLightShadowMapCapacity = spotLightShadowMapCount;
			spotLightShadowMaps = Texture::create(device, TextureType::TEXTURE_2D_ARRAY, TextureFormat::D16_UNORM,
				Extent3D{
					.width = options.spotLightShadowMapResolution,
					.height = options.spotLightShadowMapResolution,
					.depth = spotLightShadowMapCapacity,
				},
				1, ClearValues{},
				TextureSamplerOptions{
					.minificationFilter = TextureFilter::LINEAR,
					.magnificationFilter = TextureFilter::LINEAR,
					.mipmapMode = TextureMipmapMode::NONE,
					.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE,
					.maxAnisotropy = 1.0f,
					.depthComparisonMode = TextureDepthComparisonMode::LESS_OR_EQUAL,
				});
		}

		shadowMapIndicesDirty = false;
	}

	if (shadowMatrixIndicesDirty) {
		uint32_t shadowMatrixCount = 0;
		for (auto&& [lightID, light] : lights) {
			if (light.shadowMatrixIndex >= 0.0f) {
				switch (static_cast<LightType>(static_cast<uint32_t>(light.type))) {
					case LightType::AMBIENT_LIGHT: [[fallthrough]];
					case LightType::POINT_LIGHT: unreachable();
					case LightType::SUN_LIGHT: [[fallthrough]];
					case LightType::DIRECTIONAL_LIGHT:
						light.shadowMatrixIndex = static_cast<float>(shadowMatrixCount);
						shadowMatrixCount += static_cast<uint32_t>(options.shadowCascadeFrustumFarPlaneDistances.size());
						break;
					case LightType::SPOT_LIGHT:
						light.shadowMatrixIndex = static_cast<float>(shadowMatrixCount);
						++shadowMatrixCount;
						break;
				}
			}
		}

		shadowMatrices.assign(static_cast<size_t>(shadowMatrixCount), ShadowMatrixFields{.shadowMatrix{1.0f}});
		shadowMatrixBufferDirty = true;

		shadowMatrixIndicesDirty = false;
	}
}

void Lights3D::flush(Device& device) const {
	flushIndices(device);

	if (parameterBufferDirty) {
		Array<float, MAX_SHADOW_CASCADE_COUNT> shadowCascadeFrustumFarPlaneDistances{};
		copy(options.shadowCascadeFrustumFarPlaneDistances, shadowCascadeFrustumFarPlaneDistances.begin());

		parameterBuffer.upload(Parameters{
			.lightsShadowCascadeFrustumFarPlaneDistances = shadowCascadeFrustumFarPlaneDistances,
			.lightsShadowCascadeCount = static_cast<uint32_t>(options.shadowCascadeFrustumFarPlaneDistances.size()),
			.lightsShadowCascadeBlendSize = options.shadowCascadeBlendSize,
		});
		parameterBufferDirty = false;
	}

	if (shadowMatrixBufferDirty) {
		shadowMatrixBuffer.upload(shadowMatrices);
		shadowMatrixBufferDirty = false;
	}
}

} // namespace grem::graphics
