// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_KITCHEN_SINK_SHADERS_HPP
#define GREM_EXAMPLES_KITCHEN_SINK_SHADERS_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct ExampleShaderParameters {
	float time;
};

using ExampleShaderUniformBuffer = gfx::UniformBuffer<ExampleShaderParameters, "ExampleShaderParameters">;

using ExampleFragmentShader =
	gfx::Model2D::FragmentShaderBase<gfx::Model2D::VertexShaderOutputs, gfx::Model2D::FragmentShaderConstants, gfx::Model2D::FragmentShaderOutputs, ExampleShaderUniformBuffer>;

using ExampleShaderPipeline = gfx::Model2D::ShaderPipeline;

#endif
