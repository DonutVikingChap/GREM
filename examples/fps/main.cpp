// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # FPS Example
 *
 * This example shows how GREM can be used in a larger project. It uses the
 * physics engine and networking support provided by GREM to implement a basic
 * online multiplayer first-person shooter game with a server-authoritative
 * networking architecture, complete with client-side prediction, subticked
 * player commands, lag compensation and reconciliation.
 *
 * The game uses an Entity Component System (ECS) architecture, where the
 * components are basic structs and data types defined in `game_components.hpp`,
 * and the systems are dynamically loaded as plugins (defined in the
 * `game_systems/` directory) that define a set of tasks to be executed for each
 * tick, player update and/or frame of the game world. The tick and player
 * update tasks operate directly on the components of the entities in the
 * simulation, and are scheduled to be run in parallel automatically based on
 * which components and resources they have mutable and/or read-only access to.
 *
 * Systems are loaded by GameSystems and managed by GameState. If
 * BUILD_SHARED_LIBS is OFF, the systems are linked statically instead of
 * dynamically by including their definitions directly into `GameSystems.cpp`.
 * This example project uses this manual approach, of listing all the files, as
 * a simple solution to also support compiling for the web platform, which
 * doesn't support shared libraries. Note that ideally, a real project should
 * probably choose just one of the two approaches based on the primary target
 * platform, in order to maximize either moddability (when targeting desktop) or
 * simplicity (when targeting web/both), though it would also be possible to set
 * up a CMake command to generate the includes automatically.
 *
 * When played offline, the game runs a "listen server" on a background thread,
 * which the local client connects to through a loopback socket. Running the
 * server on a separate thread ensures that any potential lag spikes caused by
 * the server-side physics calculations are not directly felt by the client,
 * which greatly improves the smoothness and framerate stability of the game.
 * Thanks to client-side prediction, this separation does not add any perceived
 * lag to the client's inputs, but causes some "sponginess" when interacting
 * with unpredicted non-static physics objects, and some potential for jerky
 * movements in the case of prediction errors (e.g. when an object is pushed by
 * two different players at the same time). When playing over a network, this
 * sort of compromise is unavoidable anyway, so sharing the same architecture
 * for single player helps ensure a uniform experience.
 *
 * The game uses the included `examples/data/shared_2d/`,
 * `examples/data/shared_3d/` and `examples/data/fps/` folders as its resource
 * archives for any asset files loaded at runtime.
 */

#include <GREM/aliases.hpp>
#include <GREM/application/VirtualFilesystem.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/command_line_interface.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/events/SimpleMessageBox.hpp>

#include "Game.hpp"

// Define cross-platform entry point that makes main() more consistent.
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
			.applicationName = "ExampleFPS",
		}));
		filesystem.mountInputArchive("data");
		filesystem.mountInputArchive("data/shared_2d");
		filesystem.mountInputArchive("data/shared_3d");
		filesystem.mountInputArchive("data/fps");
		filesystem.mountInputArchivesInMountedDirectory("custom", "zip");
		filesystem.mountInputArchive(filesystem.getOutputDirectory());

		GameArguments gameArguments{};
		GameOptions gameOptions{};
		try {
			cli::parseCommandLine(gameArguments, gameOptions, argc, argv, {.shortOptionPrefix{}});
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}
		if (!gameOptions.settingsFilepath.empty()) {
			gameOptions.settings = GameSettings::load(filesystem, gameOptions.settingsFilepath);
		}
		if (!gameOptions.cl.settingsFilepath.empty()) {
			gameOptions.cl.settings = ClientSettings::load(filesystem, gameOptions.cl.settingsFilepath);
		}

#ifdef GREM_USE_PROFILING
		if (gameOptions.captureStartupTimeProfile) {
			GREM_PROFILER_SAVE_NEXT_FRAME("fps_profiler_startup_trace_", ProfileFormat::TRACE_EVENT_FORMAT);
		}
#endif

		Game game{filesystem, gameArguments, gameOptions};
		game.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
