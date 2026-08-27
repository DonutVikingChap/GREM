// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_AUDIO_HPP
#define GREM_EXAMPLES_FPS_AUDIO_HPP

#include <GREM/aliases.hpp>
#include <GREM/audio/SoundStage.hpp>

class AssetCache;

struct Audio {
	aud::SoundStage& soundStage;

	Audio(aud::SoundStage& soundStage, AssetCache& assetCache)
		: soundStage(soundStage) {
		(void)assetCache;
	}
};

#endif
