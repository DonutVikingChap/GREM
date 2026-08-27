// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_RECEIVED_CHAT_MESSAGES_HPP
#define GREM_EXAMPLES_FPS_CLIENT_RECEIVED_CHAT_MESSAGES_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/system/Clock.hpp>

struct ClientReceivedChatMessages {
	struct Message {
		struct Compare {
			[[nodiscard]] bool operator()(const Message& a, const Message& b) const {
				return a.receiveTimestamp < b.receiveTimestamp;
			}

			[[nodiscard]] bool operator()(const Message& a, TimePoint b) const {
				return a.receiveTimestamp < b;
			}

			[[nodiscard]] bool operator()(TimePoint a, const Message& b) const {
				return a < b.receiveTimestamp;
			}
		};

		TimePoint receiveTimestamp{};
		String senderName{};
		String message{};
	};

	ArrayList<Message> messages{};
};

#endif
