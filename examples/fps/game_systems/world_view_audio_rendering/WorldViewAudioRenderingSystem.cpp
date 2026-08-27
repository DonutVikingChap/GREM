// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/audio/Sound.hpp>
#include <GREM/audio/SoundInstanceID.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../Audio.hpp"
#include "../../Graphics.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"

class WorldViewAudioRenderingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry&, Audio* audio, Graphics*, exec::Task::ParallelCount) override {
		if (!audio) {
			throw Error{"WorldViewAudioRenderingSystem requires audio."};
		}
	}

	void removeResources(ResourceRegistry&, Audio*, Graphics*) noexcept override {}

	void renderAudio(Audio& audio, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		LocalPlayerPerspective localPlayerPerspective{};
		size_t localPlayerPerspectiveCount = 0;
		worldView.subtickResources.getResource<PlayerEntityMap>().forEachPlayerEntity(worldView.playerID, [&](EntityID entityID) -> bool {
			if (worldView.subtickRegistry.hasComponent<LocalPlayerID>(entityID)) {
				if (const LocalPlayerPerspective* const perspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(entityID)) {
					if (++localPlayerPerspectiveCount >= 2) {
						return true;
					}
					localPlayerPerspective = *perspective;
				}
			}
			return false;
		});

		if (localPlayerPerspectiveCount >= 2) {
			audio.soundStage.update({});
		} else {
			const phys::OrthonormalBasis3D cameraBasis = rotate(phys::PitchYawRoll{localPlayerPerspective.aimAngles, 0_radians});
			audio.soundStage.update({
				.position = localPlayerPerspective.viewPosition.in(phys::METERS),
				.velocity = localPlayerPerspective.linearVelocity.in(phys::METERS_PER_SECOND),
				.forward = -cameraBasis[phys::Z],
				.up = cameraBasis[phys::Y],
			});
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createWorldViewAudioRenderingSystem() { // NOLINT(misc-use-internal-linkage)
	return new WorldViewAudioRenderingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
