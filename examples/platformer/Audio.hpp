// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_AUDIO_HPP
#define GREM_EXAMPLES_PLATFORMER_AUDIO_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct Audio {
	aud::SoundStage soundStage;

	explicit Audio(const aud::SoundStageOptions& soundStageOptions)
		: soundStage(soundStageOptions) {}
};

#endif
