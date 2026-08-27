#include <GREM/gamma_correction.glsl>

float getKarisAverageWeight(vec3 color) {
	float luminance = dot(GREM_convertLinearToSRGB(color), vec3(0.2126, 0.7152, 0.0722));
	return 1.0 / (1.0 + luminance);
}

vec3 thresholdBloom(vec3 color) {
	float maxBrightness = max(max(color.r, color.g), color.b);
	return max(color * max(maxBrightness - BLOOM_THRESHOLD, 0.0) / max(maxBrightness, 0.0001), vec3(0.0));
}

vec3 downsampleBloom(vec2 uv) {
	// Reference: Jorge Jimenez (Activision Blizzard), Sledgehammer Games:
	//            "Next Generation Post Processing in Call of Duty Advanced Warfare":
	//            SIGGRAPH 2014 Advances in Real-Time Rendering in Games course:
	//            http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare

	vec2 texelSize = 1.0 / vec2(textureSize(mainTexture, 0));

    // . . . . . . .
    // . a . b . c .
    // . . d . e . .
    // . f . g . h .
    // . . i . j . .
    // . k . l . m .
    // . . . . . . .
    vec3 a = GREM_textureSample2D(mainTexture, uv + texelSize * vec2(-2.0,  2.0)).rgb;
    vec3 b = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 0.0,  2.0)).rgb;
    vec3 c = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 2.0,  2.0)).rgb;
    vec3 d = GREM_textureSample2D(mainTexture, uv + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 e = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 1.0,  1.0)).rgb;
    vec3 f = GREM_textureSample2D(mainTexture, uv + texelSize * vec2(-2.0,  0.0)).rgb;
    vec3 g = GREM_textureSample2D(mainTexture, uv).rgb;
    vec3 h = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 2.0,  0.0)).rgb;
    vec3 i = GREM_textureSample2D(mainTexture, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 j = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 1.0, -1.0)).rgb;
    vec3 k = GREM_textureSample2D(mainTexture, uv + texelSize * vec2(-2.0, -2.0)).rgb;
    vec3 l = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 0.0, -2.0)).rgb;
    vec3 m = GREM_textureSample2D(mainTexture, uv + texelSize * vec2( 2.0, -2.0)).rgb;

	if (BLOOM_DOWNSAMPLE_FIRST_LEVEL) {
		vec3 group0 = (a + b + f + g) * (0.125 / 4.0);
		vec3 group1 = (b + c + g + h) * (0.125 / 4.0);
		vec3 group2 = (f + g + k + l) * (0.125 / 4.0);
		vec3 group3 = (g + h + l + m) * (0.125 / 4.0);
		vec3 group4 = (d + e + i + j) * (0.5 / 4.0);
		return thresholdBloom(
			group0 * getKarisAverageWeight(group0) +
			group1 * getKarisAverageWeight(group1) +
			group2 * getKarisAverageWeight(group2) +
			group3 * getKarisAverageWeight(group3) +
			group4 * getKarisAverageWeight(group4));
	} else {
		return
			(d + e + g + i + j) * 0.125 +
			(b + f + h + l) * 0.0625 +
			(a + c + k + m) * 0.03125;
	}
}

void main() {
    outputColor = vec4(downsampleBloom(fragmentTextureCoordinates), 1.0);
}
