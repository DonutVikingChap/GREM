// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/audio/Error.hpp>
#include <GREM/audio/Listener.hpp>
#include <GREM/audio/Sound.hpp>
#include <GREM/audio/SoundStage.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include <soloud.h> // SoLoud::...

namespace grem::audio {

SoundStage::SoundStage(const SoundStageOptions& options)
	: engine(new SoLoud::Soloud{}) {
	GREM_PROFILE_FUNCTION();

	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	unsigned flags = SoLoud::Soloud::NO_FPU_REGISTER_CHANGE;
	if (options.useRoundoff) {
		flags |= SoLoud::Soloud::CLIP_ROUNDOFF;
	}
	if (options.enableStatistics) {
		flags |= SoLoud::Soloud::ENABLE_VISUALIZATION;
	}
	const unsigned backend = SoLoud::Soloud::AUTO;
	const unsigned sampleRate = SoLoud::Soloud::AUTO;
	const unsigned bufferSize = SoLoud::Soloud::AUTO;
	const unsigned channels = static_cast<unsigned>(options.outputChannelCount);
	{
		GREM_PROFILE_BLOCK("Initialize SDL (audio subsystem) and SoLoud sound engine");
		if (const SoLoud::result errorCode = soloud.init(flags, backend, sampleRate, bufferSize, channels); errorCode != SoLoud::SO_NO_ERROR) {
			throw audio::Error{"Failed to initialize sound system", errorCode};
		}
	}
	setOutputVolume(options.outputVolume);
	setSpeedOfSound(options.speedOfSound);
	setMaxSimultaneousSoundInstanceCount(options.maxSimultaneousSoundInstanceCount);
}

void SoundStage::update(const Listener& listener) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());

	soloud.set3dListenerParameters(listener.position.x, listener.position.y, listener.position.z, listener.forward.x, listener.forward.y, listener.forward.z, listener.up.x,
		listener.up.y, listener.up.z, listener.velocity.x, listener.velocity.y, listener.velocity.z);

	soloud.update3dAudio();
}

SoundInstanceID SoundStage::playSound(const Sound& sound, float volume, Panning panning) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::handle result = soloud.play(*static_cast<SoLoud::AudioSource*>(sound.get()), volume, 0.0f, true);
	soloud.setPanAbsolute(result, panning.leftVolume, panning.rightVolume);
	soloud.setPause(result, false);
	return SoundInstanceID{result};
}

SoundInstanceID SoundStage::play3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return SoundInstanceID{soloud.play3d(*static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, volume)};
}

SoundInstanceID SoundStage::playSoundInBackground(const Sound& sound, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::handle result = soloud.playBackground(*static_cast<SoLoud::AudioSource*>(sound.get()), volume);
	soloud.setProtectVoice(result, true);
	return SoundInstanceID{result};
}

SoundInstanceID SoundStage::createPausedSound(const Sound& sound, float volume, Panning panning) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::handle result = soloud.play(*static_cast<SoLoud::AudioSource*>(sound.get()), volume, 0.0f, true);
	soloud.setPanAbsolute(result, panning.leftVolume, panning.rightVolume);
	return SoundInstanceID{result};
}

SoundInstanceID SoundStage::createPaused3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return SoundInstanceID{soloud.play3d(*static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, volume, true)};
}

SoundInstanceID SoundStage::createPausedSoundInBackground(const Sound& sound, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::handle result = soloud.playBackground(*static_cast<SoLoud::AudioSource*>(sound.get()), volume, true);
	soloud.setProtectVoice(result, true);
	return SoundInstanceID{result};
}

SoundInstanceID SoundStage::playSoundClocked(Duration time, const Sound& sound, float volume, Panning panning) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::handle result = soloud.playClocked(duration_cast<DurationBase<SoLoud::time>>(time).count(), *static_cast<SoLoud::AudioSource*>(sound.get()), volume);
	soloud.setPanAbsolute(result, panning.leftVolume, panning.rightVolume);
	return SoundInstanceID{result};
}

SoundInstanceID SoundStage::play3DSoundClocked(Duration time, const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return SoundInstanceID{soloud.play3dClocked(duration_cast<DurationBase<SoLoud::time>>(time).count(), *static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y,
		position.z, velocity.x, velocity.y, velocity.z, volume)};
}

bool SoundStage::isSoundPaused(SoundInstanceID id) const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getPause(id.value);
}

bool SoundStage::isSoundStopped(SoundInstanceID id) const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return !soloud.isValidVoiceHandle(id.value);
}

bool SoundStage::isSoundLooping(SoundInstanceID id) const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getLooping(id.value);
}

Optional<Duration> SoundStage::getSoundTime(SoundInstanceID id) const {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const SoLoud::time result = soloud.getStreamPosition(id.value);
	if (!soloud.isValidVoiceHandle(id.value)) { // Check after calling getStreamPosition(), to make sure the sound didn't stop after checking.
		return {};
	}
	return duration_cast<Duration>(DurationBase<SoLoud::time>{result});
}

void SoundStage::stopSound(SoundInstanceID id) noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.stop(id.value);
}

void SoundStage::pauseSound(SoundInstanceID id) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setPause(id.value, true);
}

void SoundStage::resumeSound(SoundInstanceID id) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setPause(id.value, false);
}

void SoundStage::scheduleSoundStop(SoundInstanceID id, Duration timePointInSound) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.scheduleStop(id.value, duration_cast<DurationBase<SoLoud::time>>(timePointInSound).count());
}

void SoundStage::scheduleSoundPause(SoundInstanceID id, Duration timePointInSound) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.schedulePause(id.value, duration_cast<DurationBase<SoLoud::time>>(timePointInSound).count());
}

void SoundStage::setSoundLooping(SoundInstanceID id, bool newLooping) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setLooping(id.value, newLooping);
}

void SoundStage::seekToSoundTime(SoundInstanceID id, Duration timePointInSound) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.seek(id.value, duration_cast<DurationBase<SoLoud::time>>(timePointInSound).count());
}

void SoundStage::setSoundPosition(SoundInstanceID id, vec3 newPosition) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.set3dSourcePosition(id.value, newPosition.x, newPosition.y, newPosition.z);
}

void SoundStage::setSoundVelocity(SoundInstanceID id, vec3 newVelocity) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.set3dSourceVelocity(id.value, newVelocity.x, newVelocity.y, newVelocity.z);
}

void SoundStage::setSoundPositionAndVelocity(SoundInstanceID id, vec3 newPosition, vec3 newVelocity) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.set3dSourceParameters(id.value, newPosition.x, newPosition.y, newPosition.z, newVelocity.x, newVelocity.y, newVelocity.z);
}

void SoundStage::setSoundPanning(SoundInstanceID id, Panning newPanning) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setPanAbsolute(id.value, newPanning.leftVolume, newPanning.rightVolume);
}

void SoundStage::setSoundVolume(SoundInstanceID id, float volume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setVolume(id.value, volume);
}

void SoundStage::fadeSoundVolume(SoundInstanceID id, float targetVolume, Duration fadeDuration) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.fadeVolume(id.value, targetVolume, duration_cast<DurationBase<SoLoud::time>>(fadeDuration).count());
}

void SoundStage::setSoundPlaybackSpeed(SoundInstanceID id, float playbackSpeed) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setRelativePlaySpeed(id.value, playbackSpeed);
}

void SoundStage::fadeSoundPlaybackSpeed(SoundInstanceID id, float targetPlaybackSpeed, Duration fadeDuration) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.fadeRelativePlaySpeed(id.value, targetPlaybackSpeed, duration_cast<DurationBase<SoLoud::time>>(fadeDuration).count());
}

void SoundStage::setOutputChannelSpeakerPosition(size_t outputChannelIndex, vec3 newPosition) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setSpeakerPosition(static_cast<unsigned>(outputChannelIndex), newPosition.x, newPosition.y, newPosition.z);
}

void SoundStage::setOutputVolume(float newVolume) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setGlobalVolume(newVolume);
}

void SoundStage::fadeOutputVolume(float targetVolume, Duration fadeDuration) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.fadeGlobalVolume(targetVolume, duration_cast<DurationBase<SoLoud::time>>(fadeDuration).count());
}

void SoundStage::setSpeedOfSound(float newSpeedOfSound) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.set3dSoundSpeed(newSpeedOfSound);
}

void SoundStage::setMaxSimultaneousSoundInstanceCount(size_t newMaxSimultaneousSoundInstanceCount) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setMaxActiveVoiceCount(static_cast<unsigned>(min(newMaxSimultaneousSoundInstanceCount, size_t{Limits<unsigned>::MAX})));
}

void SoundStage::setStatisticsEnabled(bool newEnableStatistics) {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setVisualizationEnable(newEnableStatistics);
}

size_t SoundStage::getOutputChannelCount() const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return static_cast<size_t>(soloud.getBackendChannels());
}

vec3 SoundStage::getOutputChannelSpeakerPosition(size_t outputChannelIndex) const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	vec3 result{};
	if (outputChannelIndex > size_t{Limits<unsigned>::MAX} ||
		soloud.getSpeakerPosition(static_cast<unsigned>(outputChannelIndex), result.x, result.y, result.z) != SoLoud::SO_NO_ERROR) {
		return vec3{};
	}
	return result;
}

float SoundStage::getOutputVolume() const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getGlobalVolume();
}

float SoundStage::getSpeedOfSound() const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.get3dSoundSpeed();
}

size_t SoundStage::getMaxSimultaneousSounds() const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return static_cast<size_t>(soloud.getMaxActiveVoiceCount());
}

Array<float, 256> SoundStage::calculateOutputFastFourierTransformStatistics() const {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const float* const fft = soloud.calcFFT();
	Array<float, 256> result;
	copy(Span{fft, result.size()}, result.begin());
	return result;
}

Array<float, 256> SoundStage::getOutputWaveStatistics() const {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	const float* const wave = soloud.getWave();
	Array<float, 256> result;
	copy(Span{wave, result.size()}, result.begin());
	return result;
}

float SoundStage::getOutputChannelVolumeStatistics(size_t channelIndex) const {
	if (channelIndex > size_t{Limits<unsigned>::MAX}) {
		return 0.0f;
	}
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getApproximateVolume(static_cast<unsigned>(channelIndex));
}

void SoundStage::clearGlobalFilter(size_t filterSlotIndex) noexcept {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	if (globalFilters) {
		SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
		soloud.setGlobalFilter(static_cast<unsigned>(filterSlotIndex), nullptr);
		globalFilters[filterSlotIndex].reset();
	}
}

void SoundStage::EngineDeleter::operator()(void* handle) const noexcept {
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(handle);
	soloud.deinit();
	delete static_cast<SoLoud::Soloud*>(handle); // NOLINT(cppcoreguidelines-owning-memory)
}

void SoundStage::setGlobalFilterImplementation(size_t filterSlotIndex, detail::Filter filter) {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	if (!globalFilters) {
		globalFilters = UniquePointer<detail::Filter[]>::create(Sound::MAX_FILTER_COUNT);
	}
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setGlobalFilter(static_cast<unsigned>(filterSlotIndex), static_cast<SoLoud::Filter*>(filter.get()));
	globalFilters[filterSlotIndex] = std::move(filter);
}

void SoundStage::setSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex, float newValue) {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setFilterParameter(id.value, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex), newValue);
}

void SoundStage::setGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex, float newValue) {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.setFilterParameter(0, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex), newValue);
}

void SoundStage::fadeSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex, float targetValue, Duration fadeDuration) {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.fadeFilterParameter(id.value, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex), targetValue,
		duration_cast<DurationBase<SoLoud::time>>(fadeDuration).count());
}

void SoundStage::fadeGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex, float targetValue, Duration fadeDuration) {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	soloud.fadeFilterParameter(0, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex), targetValue,
		duration_cast<DurationBase<SoLoud::time>>(fadeDuration).count());
}

float SoundStage::getSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex) const {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getFilterParameter(id.value, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex));
}

float SoundStage::getGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex) const {
	GREM_ASSERT(filterSlotIndex < Sound::MAX_FILTER_COUNT);
	SoLoud::Soloud& soloud = *static_cast<SoLoud::Soloud*>(engine.get());
	return soloud.getFilterParameter(0, static_cast<unsigned>(filterSlotIndex), static_cast<unsigned>(attributeIndex));
}

} // namespace grem::audio
