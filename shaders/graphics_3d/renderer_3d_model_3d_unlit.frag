#include <GREM/tonemapping.glsl>
#include <GREM/Model3D/fragment.glsl>

void main() {
	vec4 baseColor = GREM_Model3D_getMaterialBaseColor();

	if (FRAGMENT_ALPHA_MASKED) {
		if (baseColor.a < GREM_Model3D_getMaterialAlphaCutoff()) {
			discard;
		}
	}

	vec3 color = baseColor.rgb;

	if (!FRAGMENT_HDR) {
		color = GREM_tonemap(color);
	}

	float coverage = 1.0;
	if (FRAGMENT_ALPHA_BLENDED) {
		coverage = baseColor.a;
	}
	outputColor = vec4(color, coverage);
}
