#include <GREM/tonemapping.glsl>

void main() {
	vec4 mainColor = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates);
	vec4 bloomColor = GREM_textureSample2D(bloomTexture, fragmentTextureCoordinates);
	vec3 color = mix(mainColor.rgb, mainColor.rgb + bloomColor.rgb, BLOOM_STRENGTH);
    outputColor = vec4(GREM_tonemap(color), mainColor.a);
}
