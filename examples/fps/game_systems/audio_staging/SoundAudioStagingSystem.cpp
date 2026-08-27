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

#include "../../AssetCache.hpp"
#include "../../Audio.hpp"
#include "../../Graphics.hpp"
#include "../../Schema.hpp"
#include "../../Snapshot.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_events.hpp"

class SoundAudioStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio* audio, Graphics*, exec::Task::ParallelCount) override {
		if (!audio) {
			throw Error{"SoundAudioStagingSystem requires audio."};
		}
		resources.addSharedResource<SoundAssets>(resources.getResource<AssetCache>(), resources.getResource<Schema>());
		resources.addSharedResource<SoundInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<SoundInstances>();
		resources.removeResource<SoundAssets>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics*) override {
		resources.getResource<SoundAssets>() = SoundAssets{resources.getResource<AssetCache>(), resources.getResource<Schema>()};
	}

	void emitEvent(Audio& audio, Graphics&, EntityRegistry&, ResourceRegistry& resources, TickIndex tickIndex, const Event& event) override {
		const SoundAssets& soundAssets = resources.getResource<SoundAssets>();
		SoundInstances& soundInstances = resources.getResource<SoundInstances>();
		SoundType soundType{};
		Optional<phys::Position3D> position{};
		phys::LinearVelocity3D linearVelocity{};
		SynchronizedEntityID emitter{};
		match(event)([&]<typename EventBase>(const EventBase& e) -> void {
			if constexpr (requires { e.soundType; }) {
				soundType = e.soundType;
				if constexpr (requires { e.position; }) {
					position = e.position;
				}
				if constexpr (requires { e.linearVelocity; }) {
					linearVelocity = e.linearVelocity;
				}
				if constexpr (requires { e.emitter; }) {
					emitter = e.emitter;
				}
			}
		});
		if (soundType != SoundType{}) {
			if (const auto it = soundAssets.info.find(soundType); it != soundAssets.info.end()) {
				const SoundAssets::SoundInfo& soundInfo = it->second;
				if (position) {
					for (auto&& [localPlayerID, listenerPerspective] : soundInstances.listenerPerspectives) {
						const aud::SoundInstanceID soundInstanceID = audio.soundStage.createPaused3DSound(*soundInfo.sound,
							listenerPerspective.inverseViewTransformation(*position).in(phys::METERS),
							listenerPerspective.inverseViewTransformation.getRelative(linearVelocity).in(phys::METERS_PER_SECOND));
						audio.soundStage.seekToSoundTime(soundInstanceID, soundInfo.startTimeOffset);
						listenerPerspective.positionalSoundInstances.emplace(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType},
							PositionalSoundInstance{
								.id = soundInstanceID,
								.position = *position,
								.linearVelocity = linearVelocity,
							});
					}
				} else if (emitter) {
					for (auto&& [localPlayerID, listenerPerspective] : soundInstances.listenerPerspectives) {
						const aud::SoundInstanceID soundInstanceID = audio.soundStage.createPaused3DSound(*soundInfo.sound, vec3{}, vec3{});
						audio.soundStage.seekToSoundTime(soundInstanceID, soundInfo.startTimeOffset);
						listenerPerspective.entityParentedSoundInstances.emplace(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType},
							EntityParentedSoundInstance{
								.id = soundInstanceID,
								.emitter = emitter,
							});
					}
				} else {
					const aud::SoundInstanceID soundInstanceID = audio.soundStage.createPausedSoundInBackground(*soundInfo.sound);
					audio.soundStage.seekToSoundTime(soundInstanceID, soundInfo.startTimeOffset);
					soundInstances.backgroundSoundInstances.emplace(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType},
						BackgroundSoundInstance{
							.id = soundInstanceID,
						});
				}
			}
		}
	}

	void cancelEvent(Audio& audio, Graphics&, EntityRegistry&, ResourceRegistry& resources, TickIndex tickIndex, const Event& event) noexcept override {
		if (SoundInstances* const soundInstances = resources.findResource<SoundInstances>()) {
			const SoundType soundType = match(event)([&]<typename EventBase>(const EventBase& e) -> SoundType {
				if constexpr (requires { e.soundType; }) {
					return e.soundType;
				} else {
					return {};
				}
			});
			if (soundType != SoundType{}) {
				for (auto&& [localPlayerID, listenerPerspective] : soundInstances->listenerPerspectives) {
					if (const auto it = listenerPerspective.positionalSoundInstances.find(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType});
						it != listenerPerspective.positionalSoundInstances.end()) {
						audio.soundStage.stopSound(it->second.id);
						listenerPerspective.positionalSoundInstances.erase(it);
					}
					if (const auto it = listenerPerspective.entityParentedSoundInstances.find(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType});
						it != listenerPerspective.entityParentedSoundInstances.end()) {
						audio.soundStage.stopSound(it->second.id);
						listenerPerspective.entityParentedSoundInstances.erase(it);
					}
				}
				if (const auto it = soundInstances->backgroundSoundInstances.find(SoundInstanceKey{.tickIndex = tickIndex, .soundType = soundType});
					it != soundInstances->backgroundSoundInstances.end()) {
					audio.soundStage.stopSound(it->second.id);
					soundInstances->backgroundSoundInstances.erase(it);
				}
			}
		}
	}

	void stageAudio(exec::Executor&, Audio& audio, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		SoundInstances& soundInstances = const_cast<SoundInstances&>(worldView.subtickResources.getResource<SoundInstances>());

		for (auto&& [localPlayerID, listenerPerspective] : soundInstances.listenerPerspectives) {
			listenerPerspective.found = false;
		}
		worldView.subtickResources.getResource<PlayerEntityMap>().forEachPlayerEntity(worldView.playerID, [&](EntityID entityID) -> void {
			const LocalPlayerID* const localPlayerID = worldView.subtickRegistry.findComponent<LocalPlayerID>(entityID);
			const LocalPlayerPerspective* const perspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(entityID);
			if (localPlayerID && perspective) {
				SoundListenerPerspective& listenerPerspective = soundInstances.listenerPerspectives[*localPlayerID];
				listenerPerspective.inverseViewTransformation =
					inverseTranslateRotate(perspective->viewPosition, phys::Orientation3D::fromAngles(phys::PitchYawRoll{perspective->aimAngles, 0}));
				listenerPerspective.found = true;
			}
		});
		for (const auto& [localPlayerID, listenerPerspective] : soundInstances.listenerPerspectives) {
			if (!listenerPerspective.found) {
				for (const auto& [soundInstanceKey, soundInstance] : listenerPerspective.positionalSoundInstances) {
					audio.soundStage.stopSound(soundInstance.id);
				}
				for (const auto& [soundInstanceKey, soundInstance] : listenerPerspective.entityParentedSoundInstances) {
					audio.soundStage.stopSound(soundInstance.id);
				}
			}
		}
		erase_if(soundInstances.listenerPerspectives, [&](const auto& kv) -> bool { return !kv.second.found; });

		if (soundInstances.listenerPerspectives.size() == 1) {
			soundInstances.listenerPerspectives.begin()->second.inverseViewTransformation = phys::InverseTransformation3D{};
		}

		for (const auto& [localPlayerID, listenerPerspective] : soundInstances.listenerPerspectives) {
			for (auto&& [soundInstanceKey, soundInstance] : listenerPerspective.positionalSoundInstances) {
				if (audio.soundStage.isSoundStopped(soundInstance.id)) {
					soundInstance.id = {};
					continue;
				}
				audio.soundStage.setSoundPositionAndVelocity(soundInstance.id, listenerPerspective.inverseViewTransformation(soundInstance.position).in(phys::METERS),
					listenerPerspective.inverseViewTransformation.getRelative(soundInstance.linearVelocity).in(phys::METERS_PER_SECOND));
				audio.soundStage.resumeSound(soundInstance.id);
			}
			erase_if(listenerPerspective.positionalSoundInstances, [&](const auto& kv) -> bool { return !kv.second.id; });

			for (auto&& [soundInstanceKey, soundInstance] : listenerPerspective.entityParentedSoundInstances) {
				if (audio.soundStage.isSoundStopped(soundInstance.id) || !worldView.receivedInterpolation) {
					soundInstance.id = {};
					continue;
				}
				const Optional<InterpolatedEntityView> emitterEntity = worldView.receivedInterpolation->findEntity(soundInstance.emitter);
				if (!emitterEntity || !emitterEntity->hasComponent<phys::Position3D>()) {
					soundInstance.id = {};
					continue;
				}

				const phys::Position3D position = emitterEntity->getInterpolatedComponentWithMargin<phys::Position3D>(SnapshotInterpolationView::TELEPORTATION_MARGIN);
				const phys::LinearVelocity3D linearVelocity =
					(emitterEntity->hasComponent<phys::LinearVelocity3D>()) ? emitterEntity->getInterpolatedComponent<phys::LinearVelocity3D>() : phys::LinearVelocity3D{};

				audio.soundStage.setSoundPositionAndVelocity(soundInstance.id, listenerPerspective.inverseViewTransformation(position).in(phys::METERS),
					listenerPerspective.inverseViewTransformation.getRelative(linearVelocity).in(phys::METERS_PER_SECOND));
				audio.soundStage.resumeSound(soundInstance.id);
			}
			erase_if(listenerPerspective.entityParentedSoundInstances, [&](const auto& kv) -> bool { return !kv.second.id; });
		}

		for (auto&& [soundInstanceKey, soundInstance] : soundInstances.backgroundSoundInstances) {
			if (audio.soundStage.isSoundStopped(soundInstance.id)) {
				soundInstance.id = {};
				continue;
			}
			audio.soundStage.resumeSound(soundInstance.id);
		}
		erase_if(soundInstances.backgroundSoundInstances, [&](const auto& kv) -> bool { return !kv.second.id; });
	}

private:
	struct SoundAssets {
		struct SoundInfo {
			SharedPointer<aud::Sound> sound;
			phys::Time startTimeOffset;
		};

		HashMap<SoundType, SoundInfo> info{};

		SoundAssets(AssetCache& assetCache, const Schema& schema) {
			info.reserve(schema.getSoundDescriptions().size());
			for (const auto& [soundType, soundDescription] : schema.getSoundDescriptions()) {
				info.emplace(soundType, SoundInfo{
											.sound = assetCache.getSound(soundDescription.filepath, soundDescription.options),
											.startTimeOffset = soundDescription.startTimeOffset,
										});
			}
		}
	};

	struct SoundInstanceKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const SoundInstanceKey& key) const {
				return getHash(key.tickIndex - TickIndex{}, key.soundType);
			}
		};

		struct Equal {
			[[nodiscard]] bool operator()(const SoundInstanceKey& a, const SoundInstanceKey& b) const {
				return a.tickIndex == b.tickIndex && a.soundType == b.soundType;
			}
		};

		TickIndex tickIndex;
		SoundType soundType;
	};

	struct PositionalSoundInstance {
		aud::SoundInstanceID id;
		phys::Position3D position;
		phys::LinearVelocity3D linearVelocity;
	};

	struct EntityParentedSoundInstance {
		aud::SoundInstanceID id;
		SynchronizedEntityID emitter;
	};

	struct BackgroundSoundInstance {
		aud::SoundInstanceID id;
	};

	struct SoundListenerPerspective {
		phys::InverseTransformation3D inverseViewTransformation{};
		HashMap<SoundInstanceKey, PositionalSoundInstance, SoundInstanceKey::Hash, SoundInstanceKey::Equal> positionalSoundInstances{};
		HashMap<SoundInstanceKey, EntityParentedSoundInstance, SoundInstanceKey::Hash, SoundInstanceKey::Equal> entityParentedSoundInstances{};
		bool found = false;
	};

	struct SoundInstances {
		HashMap<LocalPlayerID, SoundListenerPerspective> listenerPerspectives{};
		HashMap<SoundInstanceKey, BackgroundSoundInstance, SoundInstanceKey::Hash, SoundInstanceKey::Equal> backgroundSoundInstances{};
	};
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createSoundAudioStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new SoundAudioStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
