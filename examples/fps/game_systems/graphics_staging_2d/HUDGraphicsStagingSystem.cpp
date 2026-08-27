// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>

#include "../../ClientReceivedChatMessages.hpp"
#include "../../ClientSettings.hpp"
#include "../../Graphics.hpp"
#include "../../PlayerEntityMap.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../Timestamp.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"

class HUDGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"HUDGraphicsStagingSystem requires graphics."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void stageLocalPlayer2DGraphics(exec::Executor&, Graphics& graphics, const WorldView& worldView, const LocalPlayerID& localPlayerID, const Region2D& viewRegion) override {
		GREM_PROFILE_FUNCTION();

		const ClientSettings& settings = worldView.subtickResources.getResource<ClientSettings>();
		if (!settings.world.showHUD) {
			return;
		}

		const ClientReceivedChatMessages& receivedChatMessages = worldView.subtickResources.getResource<ClientReceivedChatMessages>();

		const TimePoint currentTime = Clock::now();
		const auto begin = (settings.chat.displayAll)
		                       ? receivedChatMessages.messages.begin()
		                       : lowerBound(receivedChatMessages.messages, currentTime - settings.chat.displayDuration, ClientReceivedChatMessages::Message::Compare{});
		vec2 position{
			static_cast<float>(viewRegion.offset.x) + 15.0f,
			static_cast<float>(viewRegion.offset.y) + static_cast<float>(viewRegion.size.height / 4),
		};
		for (auto it = receivedChatMessages.messages.end(); it != begin;) {
			--it;
			const float nextLineOffset =
				graphics.put2DText(position, Color::WHITE, formatString("{}: {}", it->senderName, it->message), 2.0f, gfx::TextAlign::LAST_LINE_START_BASE);
			position.y += 8.0f - nextLineOffset;
		}

		if (worldView.predictionInterpolation) {
			worldView.predictionInterpolation->snapshotB.resources.getResource<PlayerEntityMap>().forEachPlayerEntity(worldView.playerID, localPlayerID,
				[&](EntityID entityIDB) -> bool {
					const SynchronizedEntityID* const synchronizedEntityIDB = worldView.predictionInterpolation->snapshotB.registry.findComponent<SynchronizedEntityID>(entityIDB);
					if (!synchronizedEntityIDB) {
						return false;
					}

					const Optional<InterpolatedEntityView> entity = worldView.predictionInterpolation->findEntity(*synchronizedEntityIDB);
					if (!entity || !entity->hasComponent<phys::Position3D>() || !entity->hasComponent<Aim>()) {
						return false;
					}

					if (entity->hasComponent<PlayerRespawnCountdown>()) {
						const Timestamp timestamp = worldView.getEntityDisplayTimestamp(entity->entityIDs.second.getFlags());
						const Duration respawnTimeRemaining =
							getTimeBetween(timestamp, entity->getNewAttribute<&PlayerRespawnCountdown::respawnOnTickIndex>().getNext(), worldView.tickInterval);
						const vec2 messagePosition = viewRegion.offset + viewRegion.size / 2;
						graphics.put2DText(messagePosition, Color::WHITE, "YOU ARE DEAD", 4.0f, gfx::TextAlign::CENTER);
						graphics.put2DText(messagePosition - vec2{0.0f, 40.0f}, Color::LIGHT_GRAY, "(not big soup rice)", 2.0f, gfx::TextAlign::CENTER);
						graphics.put2DText(messagePosition - vec2{0.0f, 40.0f + 26.0f * 2.0f}, Color::LIGHT_GRAY,
							formatSmallString<32>("   Respawning in {} seconds...", ceil<Seconds>(respawnTimeRemaining).count()), 2.0f, gfx::TextAlign::CENTER);
						return true;
					}

					if (entity->hasComponent<Health>()) {
						const float health = entity->getNewAttribute<&Health::health>();
						const Color color = (health >= 75.0f) ? Color::LIME : (health >= 35.0f) ? Color::YELLOW : Color::RED;
						graphics.put2DText(viewRegion.offset + Offset2D{64, 64}, color, formatSmallString<16>("HP: {}", static_cast<int32_t>(ceil(health))), 3.0f);
						return true;
					}

					return false;
				});
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createHUDGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new HUDGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
