void main() {
	vec3 r = normalize(fragmentTextureCoordinates);
	vec2 uv = vec2(0.5) + vec2(atan(r.z, r.x) * 0.1591, asin(r.y) * 0.3183);
	outputColor = GREM_textureSample2D(equirectangularTexture, uv);
}
