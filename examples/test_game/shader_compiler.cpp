// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Shader Compiler Example
 *
 * To add offline Vulkan shader compilation to your own application, create a
 * compiler program similar to this file (without this stuff at the top), and
 * then add the commands required to compile/invoke it to your CMakeLists.txt.
 *
 * Note that the shader compiler will need access to your concrete shader type
 * definitions (mesh type, inputs, outputs, buffers, etc.) in order to generate
 * the appropriate layout code, which is why this example defines them all in a
 * shared "shaders.hpp" header that both this shader compiler and the actual
 * application can include to ensure consistency.
 */
//==============================================================================
// Here is some example CMake code you can use in your CMakeLists.txt, assuming
// your compiler program is "src/shader_compiler.cpp" and your shaders are
// located in "data/shaders" (if not, change add_executable and SHADER_DIR to
// match your project structure):
[[maybe_unused]] static constexpr auto EXAMPLE_CMAKE_CODE = R"CMAKE(

# If we're on Vulkan and shader compilation is enabled...
if(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan" AND GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	# Add a target named shader-compiler for the shader compiler executable.
	add_executable(shader-compiler "src/shader_compiler.cpp")
	target_link_libraries(shader-compiler PRIVATE GREM::GREM)
	set_target_properties(shader-compiler PROPERTIES LINKER_LANGUAGE CXX CXX_SCAN_FOR_MODULES OFF)
	GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" shader-compiler)

	# Find the shader input files to compile (expecting a ".vert" or ".frag" extension).
	set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/shaders")
	file(GLOB SHADER_INPUTS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")

	# Make a list of corresponding shader output files with an added ".spv" (SPIR-V) suffix.
	set(SHADER_OUTPUTS "")
	foreach(SHADER_FILEPATH ${SHADER_INPUTS})
		list(APPEND SHADER_OUTPUTS "${SHADER_FILEPATH}.spv")
	endforeach()

	# Add a custom command that generates the shader output files by invoking
	# our shader-compiler on the shader directory, with the shader compiler and
	# input files specified as dependencies to trigger regeneration if the input
	# files have changed.
	add_custom_command(
		OUTPUT ${SHADER_OUTPUTS}
		COMMAND shader-compiler "${SHADER_DIR}"
		DEPENDS shader-compiler ${SHADER_INPUTS}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		VERBATIM)

	# Add a custom target named compile-shaders that depends on the generated
	# shader output files, and make it part of ALL. This makes it so that
	# building the default ALL target (or the compile-shaders target directly)
	# checks to make sure the generated shaders are up-to-date, triggering our
	# custom command to regenerate them if not.
	add_custom_target(compile-shaders ALL DEPENDS ${SHADER_OUTPUTS})
endif()

)CMAKE";
//==============================================================================

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

		compileShader<ExampleFragmentShader>(filesystem, "example.frag", compilationOptions);
		// Add other shaders to compile here, such as:
		//compileShader<MyAwesomeVertexShader>(filesystem, "my_awesome_shader.vert", compilationOptions);
		//compileShader<MyAwesomeFragmentShader>(filesystem, "my_awesome_shader.frag", compilationOptions);
		// Some more concrete examples of this can be found in:
		// - examples/tiles/shader_compiler.cpp
		// - examples/tiles/shaders.hpp
		// - examples/fps/shader_compiler.cpp
		// - examples/fps/shaders.hpp
	} catch (...) {
		eprintln("{}", Error::formatCurrentExceptionMessage());
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
