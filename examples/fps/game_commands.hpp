// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_COMMANDS_HPP
#define GREM_EXAMPLES_FPS_GAME_COMMANDS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/physics/quantities.hpp>

#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "Snapshot.hpp"
#include "Timestamp.hpp"
#include "build_config.hpp"
#include "serialization.hpp"

class GameState;

struct RotateAimCommand {
	phys::PitchYawRotations aimRotations;

	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StartJumpingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StopJumpingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StartCrouchingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StopCrouchingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StartSprintingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StopSprintingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ToggleFlyingCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ToggleSimulationPausedCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct SingleStepPausedSimulationCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StartPrimaryFireCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StopPrimaryFireCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StartAimingDownSightsCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct StopAimingDownSightsCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ReloadWeaponCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ChangeFireModeLeftCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ChangeFireModeRightCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct CycleFireModeCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct ToggleFlashlightCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct PlaceDecalCommand {
	DecalMaterialType decalMaterialType;
	phys::Length2D decalSize;
	phys::Distance decalRange;

	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct SpawnPrefabCommand {
	String prefabFilepath;

	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct SpawnModelObjectCommand {
	ModelType modelType;

	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct RemoveModelObjectCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct RainBoxesCommand {
	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

using SubtickCommandBase = Variant<    //
	RotateAimCommand,                  //
	StartJumpingCommand,               //
	StopJumpingCommand,                //
	StartCrouchingCommand,             //
	StopCrouchingCommand,              //
	StartSprintingCommand,             //
	StopSprintingCommand,              //
	ToggleFlyingCommand,               //
	ToggleSimulationPausedCommand,     //
	SingleStepPausedSimulationCommand, //
	StartPrimaryFireCommand,           //
	StopPrimaryFireCommand,            //
	StartAimingDownSightsCommand,      //
	StopAimingDownSightsCommand,       //
	ReloadWeaponCommand,               //
	ChangeFireModeLeftCommand,         //
	ChangeFireModeRightCommand,        //
	CycleFireModeCommand,              //
	ToggleFlashlightCommand,           //
	PlaceDecalCommand,                 //
	SpawnPrefabCommand,                //
	SpawnModelObjectCommand,           //
	RemoveModelObjectCommand,          //
	RainBoxesCommand>;
struct SubtickCommand : SubtickCommandBase {
	Duration timeOffset;

	[[nodiscard]] bool operator==(const SubtickCommand& other) const noexcept {
		return timeOffset == other.timeOffset;
	}

	[[nodiscard]] auto operator<=>(const SubtickCommand& other) const noexcept {
		return timeOffset <=> other.timeOffset;
	}

	[[nodiscard]] bool operator==(Duration otherTimeOffset) const noexcept {
		return timeOffset == otherTimeOffset;
	}

	[[nodiscard]] auto operator<=>(Duration otherTimeOffset) const noexcept {
		return timeOffset <=> otherTimeOffset;
	}

	void serializeTo(Writer output) const {
		serialize(static_cast<const SubtickCommandBase&>(*this), output);
		serialize(static_cast<uint32_t>(duration_cast<Nanoseconds>(timeOffset).count()), output);
	}

	[[nodiscard]] bool deserializeFrom(SpanReader input) {
		if (!deserialize(static_cast<SubtickCommandBase&>(*this), input)) {
			return false;
		}
		uint32_t nanoseconds{};
		if (!deserialize(nanoseconds, input)) {
			return false;
		}
		timeOffset = duration_cast<Duration>(Nanoseconds{static_cast<Nanoseconds::rep>(nanoseconds)});
		return true;
	}

	FPS_SHARED_API void execute(GameState& gameState, PlayerID playerID, LocalPlayerID localPlayerID) const;
};

struct TickCommand {
	struct LocalPlayerCommand {
		LocalPlayerID localPlayerID;
		phys::Scale2D desiredDirectionScale{};
		phys::PitchYawRates aimRotationRates{};
		SmallBuffer<SubtickCommand, 2> subtickCommands{};

		[[nodiscard]] bool operator==(const LocalPlayerCommand& other) const noexcept {
			return localPlayerID == other.localPlayerID;
		}

		[[nodiscard]] auto operator<=>(const LocalPlayerCommand& other) const noexcept {
			return localPlayerID <=> other.localPlayerID;
		}

		[[nodiscard]] bool operator==(LocalPlayerID otherLocalPlayerID) const noexcept {
			return localPlayerID == otherLocalPlayerID;
		}

		[[nodiscard]] auto operator<=>(LocalPlayerID otherLocalPlayerID) const noexcept {
			return localPlayerID <=> otherLocalPlayerID;
		}

		void insertSubtickCommand(Duration timeOffset, const SubtickCommandBase& command) {
			const auto it = upperBound(subtickCommands, timeOffset);
			subtickCommands.insert(it, SubtickCommand{command, timeOffset});
		}
	};

	Timestamp receivedInterpolationTimestampAtTickBegin{};
	SmallBuffer<LocalPlayerCommand, 1> localPlayerCommands{};

	FPS_SHARED_API void beginTick(GameState& gameState, PlayerID playerID) const;

	FPS_SHARED_API void runSubtick(GameState& gameState, PlayerID playerID, SnapshotBufferView receivedSnapshots, SnapshotBufferView predictionSnapshots, Duration oldTimeOffset,
		Duration newTimeOffset) const;

	FPS_SHARED_API void endTick(GameState& gameState, PlayerID playerID) const;
};

#endif
