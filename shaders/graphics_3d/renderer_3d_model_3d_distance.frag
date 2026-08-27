#include <GREM/Model3D/fragment.glsl>

void main() {
	if (FRAGMENT_ALPHA_MASKED) {
		vec4 baseColor = GREM_Model3D_getMaterialBaseColor();
		if (baseColor.a < GREM_Model3D_getMaterialAlphaCutoff()) {
			discard;
		}
	}

	float distance = length(cameraPosition - fragmentPosition);
	if (!FRAGMENT_DOUBLE_SIDED && !gl_FrontFacing) {
		distance *= 0.8;
	}

	outputColor = vec4(distance, 0.0, 0.0, 1.0);
}
