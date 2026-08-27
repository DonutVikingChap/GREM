// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_AUDIO_STATS_HPP
#define GREM_EXAMPLES_FPS_CLIENT_AUDIO_STATS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Array.hpp>

struct ClientAudioStats {
	Array<float, 256> outputFFT{};
	Array<float, 256> outputWave{};
	float leftOutputVolume = 0.0f;
	float rightOutputVolume = 0.0f;
};

#endif
