// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/audio/Sound.hpp>
#include <GREM/audio/SoundMix.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include <soloud.h>     // SoLoud::...
#include <soloud_bus.h> // SoLoud::Bus

namespace grem::audio {

SoundMix::SoundMix(const SoundMixOptions& options, const SoundOptions& soundOptions)
	: Sound(static_cast<SoLoud::AudioSource*>(new SoLoud::Bus{}), soundOptions) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	bus.setChannels(static_cast<unsigned>(options.channelCount));
	bus.setVolume(options.volume);
	bus.setVisualizationEnable(options.enableStatistics);
}

SoundInstanceID SoundMix::playSound(const Sound& sound, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.play(*static_cast<SoLoud::AudioSource*>(sound.get()), volume)};
}

SoundInstanceID SoundMix::play3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.play3d(*static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, volume)};
}

SoundInstanceID SoundMix::createPausedSound(const Sound& sound, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.play(*static_cast<SoLoud::AudioSource*>(sound.get()), volume, 0.0f, true)};
}

SoundInstanceID SoundMix::createPaused3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.play3d(*static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, volume, true)};
}

SoundInstanceID SoundMix::playSoundClocked(Duration time, const Sound& sound, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.playClocked(duration_cast<DurationBase<SoLoud::time>>(time).count(), *static_cast<SoLoud::AudioSource*>(sound.get()), volume)};
}

SoundInstanceID SoundMix::play3DSoundClocked(Duration time, const Sound& sound, vec3 position, vec3 velocity, float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return SoundInstanceID{bus.play3dClocked(duration_cast<DurationBase<SoLoud::time>>(time).count(), *static_cast<SoLoud::AudioSource*>(sound.get()), position.x, position.y,
		position.z, velocity.x, velocity.y, velocity.z, volume)};
}

void SoundMix::setVolume(float volume) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	bus.setVolume(volume);
}

void SoundMix::setStatisticsEnabled(bool newEnableStatistics) {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	bus.setVisualizationEnable(newEnableStatistics);
}

Array<float, 256> SoundMix::calculateOutputFastFourierTransformStatistics() const {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	const float* const fft = bus.calcFFT();
	Array<float, 256> result;
	copy(Span{fft, result.size()}, result.begin());
	return result;
}

Array<float, 256> SoundMix::getOutputWaveStatistics() const {
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	const float* const wave = bus.getWave();
	Array<float, 256> result;
	copy(Span{wave, result.size()}, result.begin());
	return result;
}

float SoundMix::getOutputChannelVolumeStatistics(uint32_t channelIndex) const {
	if (channelIndex > size_t{Limits<unsigned>::MAX}) {
		return 0.0f;
	}
	SoLoud::Bus& bus = *static_cast<SoLoud::Bus*>(get());
	return bus.getApproximateVolume(static_cast<unsigned>(channelIndex));
}

} // namespace grem::audio
