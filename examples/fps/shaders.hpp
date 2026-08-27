// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SHADERS_HPP
#define GREM_EXAMPLES_FPS_SHADERS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/buffers.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_3d/Camera3D.hpp>

struct FullscreenVertex {
	vec2 vertexPosition;
};

using FullscreenMesh = gfx::Mesh<FullscreenVertex>;

struct FullscreenVertexShaderConstants {};

struct FullscreenVertexShaderOutputs {
	vec2 fragmentTextureCoordinates;
};

using FullscreenVertexShader = gfx::VertexShader<FullscreenMesh, FullscreenVertexShaderConstants, FullscreenVertexShaderOutputs>;

struct FullscreenFragmentShaderConstants {};

struct FullscreenFragmentShaderOutputs {
	vec4 outputColor;
};

struct FullscreenTextureParameters {
	gfx::sampler2D mainTexture;
};

using FullscreenTextureBuffer = gfx::UniformBuffer<FullscreenTextureParameters, "FullscreenTexture">;

struct BloomDownsampleFragmentShaderConstants {
	float32_t BLOOM_THRESHOLD;
	bool32_t BLOOM_DOWNSAMPLE_FIRST_LEVEL;
};

using BloomDownsampleFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, BloomDownsampleFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

struct BloomUpsampleFragmentShaderConstants {
	float32_t BLOOM_FILTER_RADIUS;
};

using BloomUpsampleFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, BloomUpsampleFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

struct BloomComposeFragmentShaderConstants {
	float32_t BLOOM_STRENGTH;
};

struct BloomComposeParameters {
	gfx::sampler2D mainTexture;
	gfx::sampler2D bloomTexture;
};

using BloomComposeParameterBuffer = gfx::UniformBuffer<BloomComposeParameters, "BloomComposeParameters">;

using BloomComposeFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, BloomComposeFragmentShaderConstants, FullscreenFragmentShaderOutputs, BloomComposeParameterBuffer>;

struct BlurFragmentShaderConstants {
	bool32_t BLUR_HORIZONTAL;
};

using BlurFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, BlurFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

using TonemapFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, FullscreenFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

using DownscaleFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, FullscreenFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

using UpscaleFragmentShader =
	gfx::FragmentShader<FullscreenMesh, FullscreenVertexShaderOutputs, FullscreenFragmentShaderConstants, FullscreenFragmentShaderOutputs, FullscreenTextureBuffer>;

struct HullVertex {
	vec3 vertexPosition;
};

struct HullInstance {
	mat4 instanceTransformation;
};

using HullMesh = gfx::Mesh<HullVertex, gfx::NoIndex, gfx::NoParameters, HullInstance>;

struct HullVertexShaderConstants {};

struct HullVertexOutputs {};

using HullVertexShader = gfx::VertexShader<HullMesh, HullVertexShaderConstants, HullVertexOutputs, gfx::Camera3D::ParameterBuffer>;

struct HullFragmentShaderConstants {};

struct HullFragmentOutputs {
	vec4 outputColor;
};

using HullFragmentShader = gfx::FragmentShader<HullMesh, HullVertexOutputs, HullFragmentShaderConstants, HullFragmentOutputs>;

#endif
