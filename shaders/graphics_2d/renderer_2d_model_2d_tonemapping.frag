#include <GREM/tonemapping.glsl>

void main() {
	vec4 mainTextureColor = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates);
	float alpha = fragmentTintColor.a * mainTextureColor.a;
	outputColor = vec4(GREM_tonemap(fragmentEmissiveColor * alpha + fragmentTintColor.rgb * mainTextureColor.rgb), alpha);
}
