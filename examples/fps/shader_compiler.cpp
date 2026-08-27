// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "shaders.hpp"

#include <vector> // std::vector

namespace {

template <typename Shader>
void compileShader(Filesystem& filesystem, CStringView filepath, const gfx::ShaderCompilationOptions& compilationOptions) {
	try {
		const std::vector<uint32_t> code = Shader::compileGLSLToVulkanSPIRV(filesystem.readInputFileString(filepath), filesystem, filepath, compilationOptions);
		filesystem.openEmptyOutputFile(formatString("{}.spv", filepath)).write(asBytes(Span{code}));
	} catch (...) {
		Error::throwWithNestedFilepath(filepath);
	}
}

} // namespace

int main(int argc, char* argv[]) {
	try {
		struct {
			String inputDirectory{};
			Optional<String> outputDirectory{};
		} arguments{};
		gfx::ShaderCompilationOptions compilationOptions{};
		cli::parseCommandLine(arguments, compilationOptions, argc, argv);

		app::VirtualFilesystem filesystem{argv[0]};
		filesystem.setOutputDirectory((arguments.outputDirectory) ? *arguments.outputDirectory : arguments.inputDirectory);
		filesystem.mountInputArchive(arguments.inputDirectory);

		compileShader<FullscreenVertexShader>(filesystem, "fullscreen.vert", compilationOptions);
		compileShader<BloomDownsampleFragmentShader>(filesystem, "bloom_downsample.frag", compilationOptions);
		compileShader<BloomUpsampleFragmentShader>(filesystem, "bloom_upsample.frag", compilationOptions);
		compileShader<BloomComposeFragmentShader>(filesystem, "bloom_compose.frag", compilationOptions);
		compileShader<BlurFragmentShader>(filesystem, "blur.frag", compilationOptions);
		compileShader<DownscaleFragmentShader>(filesystem, "downscale.frag", compilationOptions);
		compileShader<HullVertexShader>(filesystem, "hull.vert", compilationOptions);
		compileShader<HullFragmentShader>(filesystem, "hull.frag", compilationOptions);
		compileShader<gfx::Renderer3D::PBRModel3DFragmentShader>(filesystem, "probe.frag", compilationOptions);
		compileShader<TonemapFragmentShader>(filesystem, "tonemap.frag", compilationOptions);
		compileShader<UpscaleFragmentShader>(filesystem, "upscale.frag", compilationOptions);
	} catch (...) {
		eprintln("{}", Error::formatCurrentExceptionMessage());
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
