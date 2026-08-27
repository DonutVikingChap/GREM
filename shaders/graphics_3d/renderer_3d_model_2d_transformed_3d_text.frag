#include <GREM/blending.glsl>

void main() {
	float mainTextureValue = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates).r;
	outputColor = GREM_convertStraightToPremultipliedAlpha(vec4(fragmentEmissiveColor + fragmentTintColor.rgb, fragmentTintColor.a * mainTextureValue));
}
