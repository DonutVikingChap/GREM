vec3 upsampleBloom(vec2 uv) {
	// Reference: Jorge Jimenez (Activision Blizzard), Sledgehammer Games:
	//            "Next Generation Post Processing in Call of Duty Advanced Warfare":
	//            SIGGRAPH 2014 Advances in Real-Time Rendering in Games course:
	//            http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare

	vec2 texelSize = 1.0 / vec2(textureSize(mainTexture, 0));
    float x = texelSize.x * BLOOM_FILTER_RADIUS;
    float y = texelSize.y * BLOOM_FILTER_RADIUS;

    // . . . . . . .
    // . a . b . c .
    // . . . . . . .
    // . d . e . f .
    // . . . . . . .
    // . g . h . i .
    // . . . . . . .
    vec3 a = GREM_textureSample2D(mainTexture, uv + vec2(-x, -y)).rgb;
    vec3 b = GREM_textureSample2D(mainTexture, uv + vec2( 0, -y)).rgb;
    vec3 c = GREM_textureSample2D(mainTexture, uv + vec2( x, -y)).rgb;
    vec3 d = GREM_textureSample2D(mainTexture, uv + vec2(-y,  0)).rgb;
    vec3 e = GREM_textureSample2D(mainTexture, uv).rgb;
    vec3 f = GREM_textureSample2D(mainTexture, uv + vec2( x,  0)).rgb;
    vec3 g = GREM_textureSample2D(mainTexture, uv + vec2(-x,  y)).rgb;
    vec3 h = GREM_textureSample2D(mainTexture, uv + vec2( 0,  y)).rgb;
    vec3 i = GREM_textureSample2D(mainTexture, uv + vec2( x,  y)).rgb;

    return
        e * 0.25 +
        (b + d + f + h) * 0.125 +
        (a + c + g + i) * 0.0625;
}

void main() {
    outputColor = vec4(upsampleBloom(fragmentTextureCoordinates), 1.0);
}
