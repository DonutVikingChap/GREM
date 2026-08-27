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

		compileShader<TileVertexShader>(filesystem, "tile.vert", compilationOptions);
		compileShader<TileFragmentShader>(filesystem, "tile.frag", compilationOptions);
		compileShader<TilemapVertexShader>(filesystem, "tilemap.vert", compilationOptions);
		compileShader<TilemapFragmentShader>(filesystem, "tilemap.frag", compilationOptions);
	} catch (...) {
		eprintln("{}", Error::formatCurrentExceptionMessage());
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
