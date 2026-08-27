// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include <ios>     // std::hex, std::dec, std::uppercase, std::nouppercase
#include <sstream> // std::ostringstream
#include <utility> // std::move

namespace {

void processShaderSource(String& output, const char* input) {
	const char* p = input;
	const char* begin = p;
	while (true) {
		switch (*p) {
			case '\0': output.append(StringView{begin, p}); return;
			case '/': {
				const char* const comment = p;
				++p;
				if (*p == '/') {
					output.append(StringView{begin, comment});
					++p;
					while (*p != '\0') {
						if (*p == '\n') {
							++p;
							break;
						}
						++p;
					}
					if (!output.empty() && gfx::detail::isValidNameCharacter(output.back()) && gfx::detail::isValidNameCharacter(*p)) {
						output.push_back(' ');
					}
					begin = p;
				} else if (*p == '*') {
					output.append(StringView{begin, comment});
					++p;
					while (*p != '\0') {
						if (*p == '*' && *(p + 1) == '/') {
							p += 2;
							break;
						}
						++p;
					}
					if (!output.empty() && gfx::detail::isValidNameCharacter(output.back()) && gfx::detail::isValidNameCharacter(*p)) {
						output.push_back(' ');
					}
					begin = p;
				}
				break;
			}
			case ' ': [[fallthrough]];
			case '\t': [[fallthrough]];
			case '\n':
				output.append(StringView{begin, p});
				do {
					++p;
				} while (*p == ' ' || *p == '\t' || *p == '\n');
				if (!output.empty() && gfx::detail::isValidNameCharacter(output.back()) && gfx::detail::isValidNameCharacter(*p)) {
					output.push_back(' ');
				}
				begin = p;
				break;
			case '#':
				output.append(StringView{begin, p});
				++p;
				if (CStringView{p}.starts_with("line") && *(p + 4) == ' ' || *(p + 4) == '\t') {
					p += 5;
					while (*p != '\0') {
						if (*p == '\n') {
							++p;
							break;
						}
						++p;
					}
					if (!output.empty() && gfx::detail::isValidNameCharacter(output.back()) && gfx::detail::isValidNameCharacter(*p)) {
						output.push_back(' ');
					}
				} else {
					if (!output.empty() && output.back() != '\n') {
						output.push_back('\n');
					}
					output.push_back('#');
					while (*p != '\0' && *p != '\n') {
						output.push_back(*p);
						++p;
					}
					output.push_back('\n');
				}
				begin = p;
				break;
			default: ++p; break;
		}
	}
}

struct BootstrapperArguments {
	String inputDirectory;
	String outputDirectory;
};

} // namespace

namespace grem::graphics {

class Bootstrapper {
public:
	explicit Bootstrapper(Filesystem& filesystem, BootstrapperArguments arguments)
		: filesystem(filesystem)
		, arguments(std::move(arguments)) {}

	void run() {
		writeFileHeader("GRAPHICS_2D");
		writeShader<gfx::Renderer2D::DefaultModel2DVertexShader>( //
			"RENDERER_2D_DEFAULT_MODEL_2D_VERTEX_SHADER_CODE", "graphics_2d/renderer_2d_model_2d_default.vert");
		writeShader<gfx::Renderer2D::PlainModel2DFragmentShader>( //
			"RENDERER_2D_PLAIN_MODEL_2D_FRAGMENT_SHADER_CODE", "graphics_2d/renderer_2d_model_2d_plain.frag");
		writeShader<gfx::Renderer2D::TextModel2DFragmentShader>( //
			"RENDERER_2D_TEXT_MODEL_2D_FRAGMENT_SHADER_CODE", "graphics_2d/renderer_2d_model_2d_text.frag");
		writeShader<gfx::Renderer2D::TonemappingModel2DFragmentShader>( //
			"RENDERER_2D_TONEMAPPING_MODEL_2D_FRAGMENT_SHADER_CODE", "graphics_2d/renderer_2d_model_2d_tonemapping.frag");
		writeFileFooter();
		saveOutput("graphics_2d");

		writeFileHeader("GRAPHICS_3D");
		writeShader<gfx::Cubemap3D::VertexShader>( //
			"CUBEMAP_3D_DEFAULT_VERTEX_SHADER_CODE", "graphics_3d/cubemap_3d_default.vert");
		writeShader<gfx::LightBaker3D::CubemapIrradianceFragmentShader>( //
			"LIGHT_BAKER_3D_CUBEMAP_IRRADIANCE_FRAGMENT_SHADER_CODE", "graphics_3d/light_baker_3d_cubemap_irradiance.frag");
		writeShader<gfx::LightBaker3D::CubemapReflectionFragmentShader>( //
			"LIGHT_BAKER_3D_CUBEMAP_REFLECTION_FRAGMENT_SHADER_CODE", "graphics_3d/light_baker_3d_cubemap_reflection.frag");
		writeShader<gfx::LightBaker3D::CubemapFromEquirectangularFragmentShader>( //
			"LIGHT_BAKER_3D_CUBEMAP_FROM_EQUIRECTANGULAR_FRAGMENT_SHADER_CODE", "graphics_3d/light_baker_3d_cubemap_from_equirectangular.frag");
		writeShader<gfx::LightBaker3D::LightProbeAtlasVertexShader>( //
			"LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_DEFAULT_VERTEX_SHADER_CODE", "graphics_3d/light_baker_3d_light_probe_atlas_default.vert");
		writeShader<gfx::LightBaker3D::LightProbeAtlasIrradianceFragmentShader>( //
			"LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_IRRADIANCE_FRAGMENT_SHADER_CODE", "graphics_3d/light_baker_3d_light_probe_atlas_irradiance.frag");
		writeShader<gfx::LightBaker3D::LightProbeAtlasDistanceFragmentShader>( //
			"LIGHT_BAKER_3D_LIGHT_PROBE_ATLAS_DISTANCE_FRAGMENT_SHADER_CODE", "graphics_3d/light_baker_3d_light_probe_atlas_distance.frag");
		writeShader<gfx::Renderer3D::SpecularSplitSumBRDFIntegrationMapFragmentShader>( //
			"RENDERER_3D_SPECULAR_SPLIT_SUM_BRDF_INTEGRATION_MAP_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_specular_split_sum_brdf_integration_map.frag");
		writeShader<gfx::Renderer3D::DefaultModel3DVertexShader>( //
			"RENDERER_3D_DEFAULT_MODEL_3D_VERTEX_SHADER_CODE", "graphics_3d/renderer_3d_model_3d_default.vert");
		writeShader<gfx::Renderer3D::UnlitModel3DFragmentShader>( //
			"RENDERER_3D_UNLIT_MODEL_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_3d_unlit.frag");
		writeShader<gfx::Renderer3D::PBRModel3DFragmentShader>( //
			"RENDERER_3D_PBR_MODEL_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_3d_pbr.frag");
		writeShader<gfx::Renderer3D::DefaultSky3DVertexShader>( //
			"RENDERER_3D_DEFAULT_SKY_3D_VERTEX_SHADER_CODE", "graphics_3d/renderer_3d_sky_3d_default.vert");
		writeShader<gfx::Renderer3D::PBRSky3DFragmentShader>( //
			"RENDERER_3D_PBR_SKY_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_sky_3d_pbr.frag");
		writeShader<gfx::Renderer3D::ShadowMapModel3DFragmentShader>( //
			"RENDERER_3D_SHADOW_MAP_MODEL_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_3d_shadow_map.frag");
		writeShader<gfx::Renderer3D::DistanceModel3DFragmentShader>( //
			"RENDERER_3D_DISTANCE_MODEL_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_3d_distance.frag");
		writeShader<gfx::Renderer3D::Model2DTransformed3DVertexShader>( //
			"RENDERER_3D_DEFAULT_MODEL_2D_TRANSFORMED_3D_VERTEX_SHADER_CODE", "graphics_3d/renderer_3d_model_2d_transformed_3d_default.vert");
		writeShader<gfx::Renderer3D::Model2DTransformed3DFragmentShader>( //
			"RENDERER_3D_PLAIN_MODEL_2D_TRANSFORMED_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_2d_transformed_3d_plain.frag");
		writeShader<gfx::Renderer3D::Model2DTransformed3DFragmentShader>( //
			"RENDERER_3D_TEXT_MODEL_2D_TRANSFORMED_3D_FRAGMENT_SHADER_CODE", "graphics_3d/renderer_3d_model_2d_transformed_3d_text.frag");
		writeFileFooter();
		saveOutput("graphics_3d");

		writeFileHeader("IMGUI");
		writeShader<imgui::GraphicalUserInterface::VertexShader>("GUI_DEFAULT_VERTEX_SHADER_CODE", "imgui/gui_default.vert");
		writeShader<imgui::GraphicalUserInterface::FragmentShader>("GUI_PLAIN_FRAGMENT_SHADER_CODE", "imgui/gui_plain.frag");
		writeFileFooter();
		saveOutput("imgui");
	}

private:
	void writeFileHeader(CStringView uppercaseModuleName) {
		glOutput << "#ifndef GREM_" << uppercaseModuleName << "_OPENGL_BUILTIN_SHADERS_" << uppercaseModuleName
				 << "_GENERATED_HPP\n"
					"#define GREM_"
				 << uppercaseModuleName << "_OPENGL_BUILTIN_SHADERS_" << uppercaseModuleName
				 << "_GENERATED_HPP\n"
					"\n"
					"/**\n"
					" * NOTE: This file is generated from the code in GREM's `shaders/` directory.\n"
					" *\n"
					" * To regenerate it, run this CMake workflow preset:\n"
					" * ```\n"
					" * cmake --workflow --preset bootstrap-builtin-shaders\n"
					" * ```\n"
					" */\n"
					"\n"
					"#include <GREM/build_config.hpp>\n"
					"\n"
					"#include <GREM/core/data/CStringView.hpp>\n"
					"\n"
					"namespace grem::graphics {\n"
					"\n"
					"namespace detail {\n"
					"\n";

		vkOutput << "#ifndef GREM_" << uppercaseModuleName << "_VULKAN_BUILTIN_SHADERS_" << uppercaseModuleName
				 << "_GENERATED_HPP\n"
					"#define GREM_"
				 << uppercaseModuleName << "_VULKAN_BUILTIN_SHADERS_" << uppercaseModuleName
				 << "_GENERATED_HPP\n"
					"\n"
					"/**\n"
					" * NOTE: This file is generated from the code in GREM's `shaders/` directory.\n"
					" *\n"
					" * To regenerate it, run this CMake workflow preset:\n"
					" * ```\n"
					" * cmake --workflow --preset bootstrap-builtin-shaders\n"
					" * ```\n"
					" */\n"
					"\n"
					"#include <GREM/build_config.hpp>\n"
					"\n"
					"#include <GREM/core/fundamentals.hpp>\n"
					"\n"
					"namespace grem::graphics {\n"
					"\n"
					"namespace detail {\n"
					"\n";
	}

	template <typename Shader>
	void writeShader(CStringView name, StringView relativeFilepath) {
		const String filepath = formatString("{}/{}", arguments.inputDirectory, relativeFilepath);
		try {
			const String fileContents = filesystem.readInputFileString(filepath);
			gfx::detail::AllocatedStringBuffer allocatedStrings{};
			gfx::detail::ExpandedStringBuffer sourceStrings{fileContents.c_str()};
			sourceStrings = gfx::detail::expandIncludes(allocatedStrings, sourceStrings, &filesystem, filepath);

			String processedSourceCode{};
			for (const char* sourceString : sourceStrings) {
				processShaderSource(processedSourceCode, sourceString);
			}
			glOutput << "inline constexpr CStringView " << name << " = R\"GLSL(" << processedSourceCode << ")GLSL\";\n";

			vkOutput << "inline constexpr uint32_t " << name
					 << "[]{\n"
						"#ifdef GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS\n";
			if (const std::vector<uint32_t> unoptimizedSPIRV = Shader::compileGLSLToVulkanSPIRV(processedSourceCode, {.optimize = false}); !unoptimizedSPIRV.empty()) {
				vkOutput << "\t0x" << std::uppercase << std::hex << unoptimizedSPIRV.front();
				for (const uint32_t word : Span{unoptimizedSPIRV}.subspan(1)) {
					vkOutput << ", 0x" << std::uppercase << std::hex << word;
				}
				vkOutput << '\n';
			}
			vkOutput << "#else\n";
			if (const std::vector<uint32_t> optimizedSPIRV = Shader::compileGLSLToVulkanSPIRV(processedSourceCode, {.optimize = true}); !optimizedSPIRV.empty()) {
				vkOutput << "\t0x" << std::uppercase << std::hex << optimizedSPIRV.front();
				for (const uint32_t word : Span{optimizedSPIRV}.subspan(1)) {
					vkOutput << ", 0x" << std::uppercase << std::hex << word;
				}
				vkOutput << '\n';
			}
			vkOutput << "#endif\n"
						"};\n"
					 << std::nouppercase << std::dec;
		} catch (...) {
			Error::throwWithNestedFilepath(filepath);
		}
	}

	void writeFileFooter() {
		glOutput << "\n"
					"} // namespace detail\n"
					"\n"
					"} // namespace grem::graphics\n"
					"\n"
					"#endif\n";

		vkOutput << "\n"
					"} // namespace detail\n"
					"\n"
					"} // namespace grem::graphics\n"
					"\n"
					"#endif\n";
	}

	void saveOutput(CStringView lowercaseModuleName) {
		const String glOutputFilepath = formatString("{0}/{1}/opengl/builtin_shaders_{1}_generated.hpp", arguments.outputDirectory, lowercaseModuleName);
		const String vkOutputFilepath = formatString("{0}/{1}/vulkan/builtin_shaders_{1}_generated.hpp", arguments.outputDirectory, lowercaseModuleName);

		filesystem.createParentOutputDirectories(glOutputFilepath);
		filesystem.openEmptyOutputFile(glOutputFilepath).write(std::move(glOutput).str());

		filesystem.createParentOutputDirectories(vkOutputFilepath);
		filesystem.openEmptyOutputFile(vkOutputFilepath).write(std::move(vkOutput).str());

		glOutput = {};
		vkOutput = {};
	}

	Filesystem& filesystem;
	BootstrapperArguments arguments;
	std::ostringstream glOutput{};
	std::ostringstream vkOutput{};
};

} // namespace grem::graphics

int main(int argc, char* argv[]) {
	try {
		NativeFilesystem filesystem{};

		BootstrapperArguments arguments{};
		cli::parseCommandLineArguments(arguments, argc, argv);

		grem::graphics::Bootstrapper bootstrapper{filesystem, std::move(arguments)};
		bootstrapper.run();
	} catch (...) {
		eprintln("{}", Error::formatCurrentExceptionMessage());
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
