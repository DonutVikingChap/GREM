// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_FUNCTIONS_HPP
#define GREM_EXAMPLES_FPS_GAME_FUNCTIONS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/physics/quantities.hpp>

#include "Schema.hpp"
#include "Timestamp.hpp"

inline auto fastForward(Timestamp startTimestamp, Timestamp endTimestamp, Duration tickInterval, auto updateCallback) {
	constexpr bool CALLBACK_RETURNS_BOOL = convertible_to<decltype(updateCallback(startTimestamp, tickInterval)), bool>;

	Timestamp timestamp = startTimestamp;
	while (true) {
		const Timestamp nextTimestamp = min(Timestamp{timestamp.getTickIndex().getNext()}, endTimestamp);
		const Duration deltaTime = getTimeBetween(timestamp, nextTimestamp, tickInterval);
		if (deltaTime <= Duration{}) {
			break;
		}

		if constexpr (CALLBACK_RETURNS_BOOL) {
			if (updateCallback(timestamp, deltaTime)) {
				return true;
			}
		} else {
			updateCallback(timestamp, deltaTime);
		}

		timestamp = nextTimestamp;
	}

	if constexpr (CALLBACK_RETURNS_BOOL) {
		return false;
	}
}

[[nodiscard]] inline phys::PitchYawRates calculateRecoilAngularRates(phys::Time timeSinceFire, phys::PitchYawRates recoilStrengthOfLatestShot,
	const WeaponDescription& weaponDescription) {
	if (timeSinceFire >= weaponDescription.recoilDuration) {
		return {};
	}
	const phys::Coefficient recoilProgress = timeSinceFire / weaponDescription.recoilDuration;
	return recoilStrengthOfLatestShot * (clamp(weaponDescription.recoil.linearInitial + weaponDescription.recoil.linearSlope * recoilProgress, 0_x, 1_x)) *
	       cos(phys::PI * weaponDescription.recoil.cosineCutoff * recoilProgress);
}

#endif
