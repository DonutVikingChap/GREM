// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_STATE_HPP
#define GREM_EXAMPLES_FPS_CLIENT_STATE_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/fundamentals.hpp>

enum class ClientState : uint8_t {
	IDLE,
	CONNECTING,
	LOADING_MAP,
	LIGHT_BAKING,
	JOINING_GAME,
	JOINED_GAME_AWAITING_FIRST_SNAPSHOT,
	PLAYING_GAME,
};

#endif
