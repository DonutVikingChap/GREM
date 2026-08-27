void main() {
	vec4 mainTextureColor = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates);
	mainTextureColor.rgb *= vec3(0.5 + 0.5 * cos(time), 0.5 + 0.5 * sin(time), 0.5 + 0.5 * sin(time + 1.5));
	float alpha = fragmentTintColor.a * mainTextureColor.a;
	outputColor = vec4(fragmentEmissiveColor * alpha + fragmentTintColor.rgb * mainTextureColor.rgb, alpha);
}
