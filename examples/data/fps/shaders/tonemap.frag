#include <GREM/tonemapping.glsl>

void main() {
	vec4 color = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates);
    outputColor = vec4(GREM_tonemap(color.rgb), color.a);
}
