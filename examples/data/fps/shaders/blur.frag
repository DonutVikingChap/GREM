#define BLUR_SAMPLE_COUNT 3u
const float BLUR_SAMPLE_OFFSETS[BLUR_SAMPLE_COUNT] = float[](0.0, 1.3846153846, 3.2307692308);
const float BLUR_SAMPLE_WEIGHTS[BLUR_SAMPLE_COUNT] = float[](0.2270270270, 0.3162162162, 0.0702702703);

void main() {
	outputColor = GREM_textureSample2D(mainTexture, fragmentTextureCoordinates) * BLUR_SAMPLE_WEIGHTS[0];
	for (uint i = 1u; i < BLUR_SAMPLE_COUNT; ++i) {
		if (BLUR_HORIZONTAL) {
			outputColor += GREM_textureSample2D(mainTexture, fragmentTextureCoordinates - vec2(BLUR_SAMPLE_OFFSETS[i] / float(textureSize(mainTexture, 0).x), 0.0)) * BLUR_SAMPLE_WEIGHTS[i];
			outputColor += GREM_textureSample2D(mainTexture, fragmentTextureCoordinates + vec2(BLUR_SAMPLE_OFFSETS[i] / float(textureSize(mainTexture, 0).x), 0.0)) * BLUR_SAMPLE_WEIGHTS[i];
		} else {
			outputColor += GREM_textureSample2D(mainTexture, fragmentTextureCoordinates - vec2(0.0, BLUR_SAMPLE_OFFSETS[i] / float(textureSize(mainTexture, 0).y))) * BLUR_SAMPLE_WEIGHTS[i];
			outputColor += GREM_textureSample2D(mainTexture, fragmentTextureCoordinates + vec2(0.0, BLUR_SAMPLE_OFFSETS[i] / float(textureSize(mainTexture, 0).y))) * BLUR_SAMPLE_WEIGHTS[i];
		}
	}
}
