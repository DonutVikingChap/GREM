#include <GREM/tonemapping.glsl>
#include <GREM/Sky3D/fragment.glsl>

void main() {
	vec3 direction = normalize(fragmentTextureCoordinates);

	vec4 radiance = GREM_Sky3D_getRadiance(direction);
	vec3 color = radiance.rgb;

	color *= cameraExposure;
	if (!SKY_HDR) {
		color = GREM_tonemap(color);
	}

	outputColor = vec4(color, radiance.a);
}
