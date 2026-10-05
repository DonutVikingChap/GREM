// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SHADERS_HPP
#define GREM_EXAMPLES_PLATFORMER_SHADERS_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct PlatformerModelShaderParameters {
	vec2 modelShaderViewportOffset;
	float modelShaderFramebufferHeight;
	u32vec2 modelShaderTileCounts;
	float modelShaderInverseTileSize;
};
using PlatformerModelShaderParameterBuffer = gfx::UniformBuffer<PlatformerModelShaderParameters, "PlatformerModelShaderParameters">;

struct PlatformerModelShaderLight {
	static constexpr float LIGHT_TYPE_DIRECTIONAL = 0.0f;
	static constexpr float LIGHT_TYPE_POINT = 1.0f;
	static constexpr float LIGHT_TYPE_SPOT = 2.0f;

	float lightType;
	float lightRange;
	vec2 lightConeCosines;
	vec3 lightPosition;
	vec3 lightDirection;
	vec3 lightIntensity;
};
using PlatformerModelShaderLightBuffer = gfx::StorageBuffer<PlatformerModelShaderLight, "PlatformerModelShaderLights">;

struct PlatformerModelShaderBlobShadow {
	vec3 blobShadowPosition;
	float blobShadowRadius;
	uint32_t blobShadowInstanceIdentifier;

	[[nodiscard]] Box<2, float> getHorizontalBoundingBox() const {
		return {
			.min{blobShadowPosition.x - blobShadowRadius, blobShadowPosition.z - blobShadowRadius},
			.max{blobShadowPosition.x + blobShadowRadius, blobShadowPosition.z + blobShadowRadius},
		};
	}

	[[nodiscard]] Box<3, float> getBoundingBox(const Box<3, float>& visibleBounds) const {
		const Box<2, float> horizontalBox = getHorizontalBoundingBox();
		return {
			.min{horizontalBox.min.x, visibleBounds.min.y, horizontalBox.min.y},
			.max{horizontalBox.max.x, blobShadowPosition.y + blobShadowRadius * 2.0f, horizontalBox.max.y},
		};
	}
};
using PlatformerModelShaderBlobShadowBuffer = gfx::StorageBuffer<PlatformerModelShaderBlobShadow, "PlatformerModelShaderBlobShadows">;

struct PlatformerModelShaderItem {
	uint32_t itemIndex;
};
using PlatformerModelShaderItemBuffer = gfx::StorageBuffer<PlatformerModelShaderItem, "PlatformerModelShaderItems">;

struct PlatformerModelShaderTile {
	static constexpr uint32_t TILE_SIZE = 128;

	uint32_t itemOffset;
	uint32_t itemCounts; // 8 bits: lightCount | 8 bits: blobShadowCount | 16 bits: reserved
};
using PlatformerModelShaderTileBuffer = gfx::StorageBuffer<PlatformerModelShaderTile, "PlatformerModelShaderTiles">;

using PlatformerModelShaderBuffers = gfx::BufferSet< //
	PlatformerModelShaderParameterBuffer,            //
	PlatformerModelShaderLightBuffer,                //
	PlatformerModelShaderBlobShadowBuffer,           //
	PlatformerModelShaderItemBuffer,                 //
	PlatformerModelShaderTileBuffer>;

struct PlatformerModelFragmentShaderConstants {
	bool32_t FRAGMENT_HDR;
	bool32_t FRAGMENT_ALPHA_MASKED;
	bool32_t FRAGMENT_ALPHA_BLENDED;
	bool32_t FRAGMENT_DOUBLE_SIDED;
	bool32_t FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
	bool32_t FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1;
	bool32_t FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
	bool32_t FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1;
	bool32_t FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
	bool32_t FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1;
	bool32_t FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
	bool32_t FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1;
	bool32_t FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
	bool32_t FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1;
	float LIGHT_TYPE_DIRECTIONAL;
	float LIGHT_TYPE_POINT;
	float LIGHT_TYPE_SPOT;
};

using PlatformerModelFragmentShader = gfx::Model3D::FragmentShaderBase< //
	gfx::Model3D::VertexShaderOutputs,                                  //
	PlatformerModelFragmentShaderConstants,                             //
	gfx::Model3D::FragmentShaderOutputs,                                //
	gfx::Sky3D::ParameterBuffer,                                        //
	PlatformerModelShaderBuffers>;

using PlatformerModelShaderPipelineSet = gfx::Model3D::ShaderPipelineSetBase< //
	gfx::Model3D::VertexShaderConstants,                                      //
	gfx::Model3D::VertexShaderOutputs,                                        //
	meta::TypeList<>,                                                         //
	PlatformerModelFragmentShaderConstants,                                   //
	gfx::Model3D::FragmentShaderOutputs,                                      //
	meta::TypeList<gfx::Sky3D::ParameterBuffer, PlatformerModelShaderBuffers>>;

#endif
