#include <GREM/blending.glsl>
#include <GREM/gamma_correction.glsl>

void main() {
	fragmentTextureCoordinates = vec2(vertexTextureCoordinates.x, 1.0 - vertexTextureCoordinates.y);
	fragmentColor = GREM_convertPremultipliedToStraightAlpha(GREM_convertSRGBToLinear(GREM_convertStraightToPremultipliedAlpha(vertexColor)));

	gl_Position = vec4(guiOffset + vertexPosition * guiScale, 0.0, 1.0);
	gl_Position.y = -gl_Position.y;
}
