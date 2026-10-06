// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Platformer Example
 *
 * This example is a small/medium-sized 3D platformer game prototype that
 * showcases some of GREM's more advanced APIs while keeping the program
 * structure relatively simple.
 *
 * The game supports local play only, so its architecture is designed to be more
 * straightforward and gameplay-code-focused compared to an online multiplayer
 * game like the FPS example. Unlike the FPS, this game does not have a separate
 * server thread, and there is only one world state that is both operated on
 * directly by the simulation and also interpolated for display to the user. In
 * a local/single-player game like this, there is no need to keep a buffer of
 * more than 2 world states or do any clientside prediction, since the
 * simulation ticks happen at a consistent timing (128 Hz by default in this
 * example) that can be interpolated between without the risk of jitter or skips
 * due to varying network conditions. The relative simplicity of the game logic
 * in a platformer game also contributes to making sure most CPUs can keep up
 * with the 128 Hz tick rate without too much trouble, and not have the
 * simulation ticks cause frame rate stutters, despite the tick stepping being
 * completely synchronized with the main render thread.
 *
 * To create the example game level, a basic Python script was written that
 * exports models created in Blender to a `.glb` file for the world geometry and
 * a `.json` file for all the game entity placements, which are defined in
 * Blender through a combination of "Empty" nodes and object instances with
 * custom properties (see `examples/datasrc/scripts/export_level.py`). The
 * corresponding game-side level loader is defined in
 * `examples/platformer/systems/level.cpp`.
 *
 * Another bonus feature that this example demonstrates is one possible setup
 * for a custom 3D shader pipeline, using tiled Forward+ rendering with custom
 * shader buffers and culling logic, that doesn't use the built-in PBR shader
 * provided with `graphics_3d`. This might especially make sense to do in a
 * platformer game like this, since they often want to use a more stylistic or
 * toon-like art style than what PBR shaders are mainly designed for. In this
 * example, the custom shader uses the classic Blinn-Phong shading model with a
 * half-Lambertian diffuse term as a baseline
 * (see `examples/data/platformer/shaders/model.frag`), but this can easily be
 * changed to suit whatever style the game needs. The example shader supports
 * direct ambient, directional, point and spot lights as well as basic "blob
 * shadows" in a retro-ish style that, while completely unrealistic, is fast to
 * render on lower-end hardware and also helps the player tell where they're
 * about to land when jumping.
 *
 * The game uses the included `examples/data/shared_2d/`,
 * `examples/data/shared_3d/`, `data/shared_3d_advanced` and
 * `examples/data/platformer/` folders as its resource archives for any asset
 * files loaded at runtime. The included `examples/datasrc` folder also contains
 * the source .blend file and export script that were used to create the game
 * level. Note that these source files aren't needed by the final game
 * distribution, which is why they're kept in a separate folder that doesn't get
 * bundled in when the application is packaged with `cmake --install`.
 */

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "Game.hpp"

// Entry point.
#include <GREM/entry_point.hpp>

extern "C" {
GREM_EXPORT uint32_t NvOptimusEnablement = 1;                  // https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
GREM_EXPORT uint32_t AmdPowerXpressRequestHighPerformance = 1; // https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
}

int main(int argc, char* argv[]) {
	try {
		app::VirtualFilesystem filesystem{argv[0]};
		filesystem.setOutputDirectory(filesystem.createStandardOutputDirectory({
			.organizationName = "GREM",
			.applicationName = "ExamplePlatformer",
		}));
		filesystem.mountInputArchive("data");
		filesystem.mountInputArchive("data/shared_2d");
		filesystem.mountInputArchive("data/shared_3d");
		filesystem.mountInputArchive("data/shared_3d_advanced");
		filesystem.mountInputArchive("data/platformer");
		filesystem.mountInputArchivesInMountedDirectory("custom", "zip");
		filesystem.mountInputArchive(filesystem.getOutputDirectory());

		GameOptions gameOptions{};
		try {
			json::deserializeFromString(filesystem.readInputFileString(Game::GAME_CONFIGURATION_FILEPATH), gameOptions);
		} catch (...) {
			Error::throwWithNestedFilepath(Game::GAME_CONFIGURATION_FILEPATH);
		}
		try {
			cli::parseCommandLineOptions(gameOptions, argc, argv);
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}

		Game game{filesystem, gameOptions};
		game.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
