#include <GREM/Model3D/fragment.glsl>

void main() {
	if (FRAGMENT_ALPHA_MASKED) {
		vec4 baseColor = GREM_Model3D_getMaterialBaseColor();
		if (baseColor.a < GREM_Model3D_getMaterialAlphaCutoff()) {
			discard;
		}
	}
}
