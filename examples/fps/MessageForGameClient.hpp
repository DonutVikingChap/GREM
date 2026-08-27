// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_MESSAGE_TO_GAME_CLIENT_HPP
#define GREM_EXAMPLES_FPS_MESSAGE_TO_GAME_CLIENT_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/system/Clock.hpp>

#include "PlayerEntityMap.hpp"
#include "Snapshot.hpp"
#include "Timestamp.hpp"
#include "game_events.hpp"

struct LoadMapMessageForGameClient {
	String schemaFilepath;
	String mapFilepath;
	Duration tickInterval;
};

struct JoinedGameMessageForGameClient {
	PlayerID playerID;
	LocalPlayerID localPlayerID;
	TickIndex tickIndex;
};

struct LeftGameMessageForGameClient {
	LocalPlayerID localPlayerID;
};

struct ExtendSchemaMessageForGameClient {
	String schemaExtension;
};

struct UpdateGameStateMessageForGameClient {
	struct ConfirmEvent {
		TickIndex tickIndex;
		Event event;
	};

	CRC32 schemaCRC32{};
	SnapshotDelta delta{};
	Buffer<ConfirmEvent> confirmEvents{};
	TickIndex latestReceivedCommandTickIndex{};
	float recentPacketLossFraction = 0.0f;
};

struct ChatMessageForGameClient {
	String senderName;
	String message;
};

using MessageForGameClient = Variant<    //
	LoadMapMessageForGameClient,         //
	JoinedGameMessageForGameClient,      //
	LeftGameMessageForGameClient,        //
	ExtendSchemaMessageForGameClient,    //
	UpdateGameStateMessageForGameClient, //
	ChatMessageForGameClient>;

#endif
