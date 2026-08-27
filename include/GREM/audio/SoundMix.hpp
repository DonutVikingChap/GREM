// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_SOUND_MIX_HPP
#define GREM_AUDIO_SOUND_MIX_HPP

#include <GREM/build_config.hpp>

#include <GREM/audio/Sound.hpp>
#include <GREM/audio/SoundInstanceID.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>

namespace grem::audio {

/**
 * Configuration options for a SoundMix.
 */
struct SoundMixOptions {
	/**
	 * The number of channels to produce audio for.
	 *
	 * Must be 1 (mono), 2 (stereo), 4 (quad), 6 (5.1) or 8 (7.1).
	 */
	size_t channelCount = 2;

	/**
	 * Default volume of the sound mix.
	 */
	float volume = 1.0f;

	/**
	 * Whether to enable statistics gathering or not.
	 *
	 * Statistics gathering must be enabled in order for
	 * calculateOutputFastFourierTransformStatistics(),
	 * getOutputWaveStatistics() and getOutputChannelVolumeStatistics() to work.
	 */
	bool enableStatistics = false;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SoundMixOptions& other) const = default;
};

/**
 * Container for a mix of sound waves that can be played in a SoundStage.
 *
 * \note Only one instance of a sound mix can be played at a time.
 */
class SoundMix : public Sound {
public:
	/**
	 * Construct a stereo sound mix with the default options.
	 *
	 * \throws audio::Error on failure to create the sound mix.
	 * \throws std::bad_alloc on allocation failure.
	 */
	SoundMix()
		: SoundMix(SoundMixOptions{}, SoundOptions{}) {}

	/**
	 * Construct a sound mix.
	 *
	 * \param options sound mix options, see SoundMixOptions.
	 * \param soundOptions sound options, see SoundOptions.
	 *
	 * \throws audio::Error on failure to create the sound mix.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(audio) explicit SoundMix(const SoundMixOptions& options, const SoundOptions& soundOptions = {});

	/**
	 * \copydoc SoundStage::playSound(const Sound&, float, Panning)
	 */
	GREM_API(audio) SoundInstanceID playSound(const Sound& sound, float volume = -1.0f);

	/**
	 * \copydoc SoundStage::play3DSound(const Sound&, vec3, vec3, float)
	 */
	GREM_API(audio) SoundInstanceID play3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * \copydoc SoundStage::createPausedSound(const Sound&, float, Panning)
	 */
	GREM_API(audio) SoundInstanceID createPausedSound(const Sound& sound, float volume = -1.0f);

	/**
	 * \copydoc SoundStage::createPaused3DSound(const Sound&, vec3, vec3, float)
	 */
	GREM_API(audio) SoundInstanceID createPaused3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * \copydoc SoundStage::playSoundClocked(Duration, const Sound&, float, Panning)
	 */
	GREM_API(audio) SoundInstanceID playSoundClocked(Duration time, const Sound& sound, float volume = -1.0f);

	/**
	 * \copydoc SoundStage::play3DSoundClocked(Duration, const Sound&, vec3, vec3, float)
	 */
	GREM_API(audio) SoundInstanceID play3DSoundClocked(Duration time, const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * Set the volume of the sound mix.
	 *
	 * \param volume new volume, see SoundMixOptions::volume.
	 */
	GREM_API(audio) void setVolume(float volume);

	/**
	 * Set whether to enable statistics gathering or not.
	 *
	 * Statistics gathering must be enabled in order for
	 * calculateOutputFastFourierTransformStatistics(),
	 * getOutputWaveStatistics() and getOutputChannelVolumeStatistics() to work.
	 *
	 * \param newEnableStatistics new mode of statistics gathering to set.
	 */
	GREM_API(audio) void setStatisticsEnabled(bool newEnableStatistics);

	/**
	 * Calculate the fast Fourier transform of the audio currently playing
	 * through the sound mix.
	 *
	 * \return a sequence of 256 values representing the discrete Fourier
	 *         transform of the currently playing audio, from low to high
	 *         frequencies.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound mix in order to produce meaningful results.
	 *
	 * \sa SoundMixOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) Array<float, 256> calculateOutputFastFourierTransformStatistics() const;

	/**
	 * Get the sound wave currently playing through the sound mix.
	 *
	 * \return a sequence of the 256 latest audio samples played through the
	 *         sound mix.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound mix in order to produce meaningful results.
	 *
	 * \sa SoundMixOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) Array<float, 256> getOutputWaveStatistics() const;

	/**
	 * Get an approximation of the volume of the audio currently playing through
	 * a specific channel of the sound mix.
	 *
	 * \param channelIndex index of the channel to get the volume of.
	 *
	 * \return an approximation of the volume of the current audio playing
	 *         through the given sound mix channel, or 0.0f if the channel index
	 *         was out of range.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound mix in order to produce meaningful results.
	 *
	 * \sa SoundMixOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) float getOutputChannelVolumeStatistics(uint32_t channelIndex) const;
};

} // namespace grem::audio

#endif
