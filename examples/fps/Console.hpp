// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CONSOLE_HPP
#define GREM_EXAMPLES_FPS_CONSOLE_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>

#include <utility> // std::move

class Game;

class Console {
public:
	using Command = String (*)(Game& game, CStringView arguments);

	void addCommand(String name, Command command) {
		commands.emplace(std::move(name), command);
	}

	String executeCommand(Game& game, StringView name, CStringView arguments) const {
		if (const auto it = commands.find(name); it != commands.end()) {
			return it->second(game, arguments);
		}
		return formatString("Unknown command \"/{}\". Use /help to list available commands.", name);
	}

	[[nodiscard]] Optional<CStringView> previousCommandInHistory() {
		if (commandHistoryIndex > 0) {
			--commandHistoryIndex;
			return commandHistory[commandHistoryIndex];
		}
		return {};
	}

	[[nodiscard]] Optional<CStringView> nextCommandInHistory() {
		if (commandHistoryIndex < commandHistory.size()) {
			++commandHistoryIndex;
			if (commandHistoryIndex < commandHistory.size()) {
				return commandHistory[commandHistoryIndex];
			}
			return CStringView{};
		}
		return {};
	}

	void saveCommandToHistory(CStringView inputBuffer) {
		if (!inputBuffer.empty() && (commandHistory.empty() || commandHistory.back() != inputBuffer)) {
			commandHistory.emplace_back(inputBuffer);
		}
		commandHistoryIndex = commandHistory.size();
	}

	[[nodiscard]] const OrderedMap<String, Command, LessThan>& getCommands() const noexcept {
		return commands;
	}

private:
	OrderedMap<String, Command, LessThan> commands{};
	ArrayList<String> commandHistory{};
	size_t commandHistoryIndex = 0;
};

#endif
