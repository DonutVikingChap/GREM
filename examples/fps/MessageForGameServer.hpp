// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_MESSAGE_TO_GAME_SERVER_HPP
#define GREM_EXAMPLES_FPS_MESSAGE_TO_GAME_SERVER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/CRC32.hpp>

#include "PlayerEntityMap.hpp"
#include "Timestamp.hpp"
#include "game_commands.hpp"

inline constexpr size_t MAX_CHAT_MESSAGE_SIZE = 255;

struct JoinGameRequestMessageForGameServer {
	LocalPlayerID localPlayerID;
};

struct LeaveGameRequestMessageForGameServer {
	LocalPlayerID localPlayerID;
};

struct RequestSchemaExtensionMessageForGameServer {
	CRC32 previousSchemaCRC32;
	String schemaExtension;
};

struct AcknowledgeSnapshotMessageForGameServer {
	TickIndex latestReceivedSnapshotTickIndex{};
};

struct LatestCommandsMessageForGameServer {
	TickIndex firstCommandTickIndex{};
	RingBuffer<TickCommand> tickCommands{};
};

struct ChatMessageForGameServer {
	String message;
};

using MessageForGameServer = Variant<           //
	JoinGameRequestMessageForGameServer,        //
	LeaveGameRequestMessageForGameServer,       //
	RequestSchemaExtensionMessageForGameServer, //
	AcknowledgeSnapshotMessageForGameServer,    //
	LatestCommandsMessageForGameServer,         //
	ChatMessageForGameServer>;

#endif
