// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Tiles Example
 *
 * This example shows how GREM can be used to render a large 2D tilemap using a
 * sparse GPU-resident approach that keeps the frame rate fairly consistent even
 * when zoomed out very far (but uses quite a bit of VRAM for large maps).
 *
 * The tilemap is split into 512x512-tile chunks and has a main CPU-side copy
 * that is fully dynamic and synchronizes with the GPU tilemap on each frame by
 * marking chunks as "dirty" on edit, and writing only those edited chunks to
 * the GPU before rendering.
 *
 * Note that the tilemap chunk textures only store tile info (tileset location,
 * animation parameters, etc.) packed into 32 bits per tile, and not the actual
 * pixels from the tileset texture. When rendering, the final pixel colors are
 * determined by drawing a fullscreen rectangle with a custom fragment shader
 * (`examples/data/tiles/shaders/tilemap.frag`) that first reads the sparse
 * tilemap texture index for the current chunk, then reads the chunk's tilemap
 * texture to determine which tile to draw, and finally samples the
 * corresponding tileset texture at the specified tile location plus the
 * sub-tile offset of the current pixel.
 *
 * Since the game world is very large and uses a pixel-perfect art style, all
 * absolute world positions are expressed using a custom fixed-point Coordinate
 * type that is transformed to view-local space before being converted to 32-bit
 * floating-point for rendering. This avoids the common problem of
 * floating-point precision error increasing when rendering further away from
 * the world origin, which can cause visible gaps between tiles, misaligned
 * sprites, color bleeding on tile edges and other artifacts in a naive
 * implementation. Fixed-point coordinates also help make the collision code,
 * movement, etc. more consistent across the whole map.
 *
 * The game uses the included `examples/data/shared_2d/` and
 * `examples/data/tiles/` folders as its resource archives for any asset files
 * loaded at runtime.
 */

#include <GREM/aliases.hpp>
#include <GREM/application.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>

#include "Game.hpp"

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

int main(int argc, char* argv[]) {
	try {
		app::VirtualFilesystem filesystem{argv[0]};
		filesystem.setOutputDirectory(filesystem.createStandardOutputDirectory({
			.organizationName = "GREM",
			.applicationName = "ExampleTiles",
		}));
		filesystem.mountInputArchive("data");
		filesystem.mountInputArchive("data/shared_2d");
		filesystem.mountInputArchive("data/tiles");
		filesystem.mountInputArchivesInMountedDirectory("custom", "zip");
		filesystem.mountInputArchive(filesystem.getOutputDirectory());

		GameOptions gameOptions = json::deserializeFromString<GameOptions>(filesystem.readInputFileString("configuration/game.json"));
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
