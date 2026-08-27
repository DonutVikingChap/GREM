#include "Tileset.glsl"

void main() {
	outputColor = Tileset_sampleTile(fragmentTile, fragmentSubTileCoordinates);
}
