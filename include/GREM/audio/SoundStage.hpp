// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_SOUND_STAGE_HPP
#define GREM_AUDIO_SOUND_STAGE_HPP

#include <GREM/build_config.hpp>

#include <GREM/audio/Listener.hpp>
#include <GREM/audio/SoundInstanceID.hpp>
#include <GREM/audio/filters.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/Clock.hpp>

#include <new>         // std::launder
#include <type_traits> // std::remove_cvref_t
#include <utility>     // std::move

namespace grem::audio {

class Sound; // Forward declaration, to avoid including Sound.hpp.

/**
 * Panning parameters for a listener-relative sound.
 */
struct Panning {
	/**
	 * Calculate left/right panning based on an offset from the center.
	 *
	 * \param normalizedOffset normalized relative offset from the center of the
	 *        listener.
	 *
	 * \return the new panning parameters.
	 */
	[[nodiscard]] static Panning fromNormalizedCenterOffset(float normalizedOffset) {
		return {
			.leftVolume = cos((1.0f + normalizedOffset) * (numbers::PI * 0.25f)),
			.rightVolume = sin((1.0f + normalizedOffset) * (numbers::PI * 0.25f)),
		};
	}

	float leftVolume = 1.0f;  ///< Left volume.
	float rightVolume = 1.0f; ///< Right volume.
};

/**
 * Configuration options for a SoundStage.
 */
struct SoundStageOptions {
	/**
	 * The number of output channels to produce audio for.
	 *
	 * Must be 1 (mono), 2 (stereo), 4 (quad), 6 (5.1) or 8 (7.1).
	 */
	size_t outputChannelCount = 2;

	/**
	 * Global master output volume.
	 *
	 * The amplitude of all playing sound is multiplied by this gain value,
	 * meaning that a value of 1 represents no change, i.e. 100% of the original
	 * volume.
	 */
	float outputVolume = 1.0f;

	/**
	 * The speed of sound in the sound stage.
	 *
	 * This value is used for doppler effect calculations and distance delay
	 * simulation.
	 *
	 * \note The default value assumes that any coordinates passed to the sound
	 *       stage are expressed in meters, and that the sound stage environment
	 *       is dry air at around 20 degrees Celsius.
	 */
	float speedOfSound = 343.3f;

	/**
	 * The maximum total number of sound instances that can play simultaneously.
	 * If the number of playing sounds exceeds this number, the ones with the
	 * highest volume will be picked to actually play.
	 */
	size_t maxSimultaneousSoundInstanceCount = 128;

	/**
	 * Whether to use roundoff when clipping the audio samples sent to the
	 * output.
	 *
	 * When true, loud audio spikes are rounded off, which prevents clipping
	 * artifacts but may cause a slight quality degradation.
	 *
	 * When false, audio samples are simply clamped to the output range, which
	 * may cause unpleasant clipping artifacts whenever samples go out of range.
	 */
	bool useRoundoff = true;

	/**
	 * Whether to enable statistics gathering or not.
	 *
	 * Statistics gathering must be enabled in order for
	 * calculateOutputFastFourierTransformStatistics(),
	 * getOutputWaveStatistics() and getOutputChannelVolumeStatistics() to work.
	 */
	bool enableStatistics = false;
};

/**
 * Persistent system for playing sound in a simulated 3D arena to the default
 * audio device.
 *
 * The sound stage uses a right-handed coordinate system for 3D calculations,
 * and any coordinates are assumed to be in meters by default. Applications that
 * use a different unit of length should adjust the
 * SoundStageOptions::speedOfSound in the sound stage configuration as well as
 * the SoundOptions::rolloffFactor of each Sound accordingly.
 */
class SoundStage {
public:
	/**
	 * Construct a sound stage.
	 *
	 * \param options initial configuration of the sound stage, see
	 *        SoundStageOptions.
	 *
	 * \throws audio::Error on failure to initialize the underlying audio
	 *         engine.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning Due to having global access to the default audio device, only a
	 *          single SoundManager instance may exist in the program at any
	 *          given time.
	 */
	GREM_API(audio) explicit SoundStage(const SoundStageOptions& options = {});

	/**
	 * Update the 3D parameters of the sound stage.
	 *
	 * \param listener current parameters to use for the sound listener on this
	 *        frame.
	 *
	 * \note This function should typically be called once every frame during
	 *       the application::Application::update() callback.
	 */
	GREM_API(audio) void update(const Listener& listener);

	/**
	 * Create a new relative sound instance and start playing it.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 * \param panning panning to apply to the sound when playing.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa createPausedSound()
	 * \sa play3DSound()
	 * \sa playSoundClocked()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID playSound(const Sound& sound, float volume = -1.0f, Panning panning = {});

	/**
	 * Create a new 3D sound instance and start playing it.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param position the position to play the sound at, in sound stage
	 *        coordinates. Used for distance attenuation/falloff according to
	 *        the SoundAttenuationModel set in the sound's SoundOptions.
	 * \param velocity initial linear velocity of the sound. Relevant for
	 *        simulating the doppler effect.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa playSound()
	 * \sa play3DSoundClocked()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID play3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * Create a new sound instance without any panning and start playing it.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa playSound()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID playSoundInBackground(const Sound& sound, float volume = -1.0f);

	/**
	 * Create a new relative sound instance, but don't start playing it yet.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 * \param panning panning to apply to the sound when playing.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa playSound()
	 * \sa play3DSound()
	 * \sa playSoundClocked()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID createPausedSound(const Sound& sound, float volume = -1.0f, Panning panning = {});

	/**
	 * Create a new 3D sound instance, but don't start playing it yet.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param position the position to play the sound at, in sound stage
	 *        coordinates. Used for distance attenuation/falloff according to
	 *        the SoundAttenuationModel set in the sound's SoundOptions.
	 * \param velocity initial linear velocity of the sound. Relevant for
	 *        simulating the doppler effect.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa createPausedSound()
	 * \sa play3DSound()
	 * \sa play3DSoundClocked()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID createPaused3DSound(const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * Create a new sound instance without any panning, but don't start playing
	 * it yet.
	 *
	 * \param sound the sound data to use for this instance.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa createPausedSound()
	 * \sa playSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID createPausedSoundInBackground(const Sound& sound, float volume = -1.0f);

	/**
	 * Create a new 3D sound instance and start playing it at a specific time
	 * relative to other sounds that were also played using playSoundClocked().
	 *
	 * \param time time point to play the sound at, relative to other clocked
	 *        sounds. This is typically the current frame or tick time, plus
	 *        some offset.
	 * \param sound the sound data to use for this instance.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 * \param panning panning to apply to the sound when playing.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa playSound()
	 * \sa createPausedSound()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID playSoundClocked(Duration time, const Sound& sound, float volume = -1.0f, Panning panning = {});

	/**
	 * Create a new 3D sound instance and start playing it at a specific time
	 * relative to other sounds that were also played using playSoundClocked().
	 *
	 * \param time time point to play the sound at, relative to other clocked
	 *        sounds. This is typically the current frame or tick time, plus
	 *        some offset.
	 * \param sound the sound data to use for this instance.
	 * \param position the position to play the sound at, in sound stage
	 *        coordinates. Used for distance attenuation/falloff according to
	 *        the SoundAttenuationModel set in the sound's SoundOptions.
	 * \param velocity initial linear velocity of the sound. Relevant for
	 *        simulating the doppler effect.
	 * \param volume gain to multiply the amplitude of the sound by when
	 *        playing. If negative, the default volume set in the SoundOptions
	 *        is used instead.
	 *
	 * \return a handle to the new sound instance that can be used to refer back
	 *         to it later in order to query its state or make changes to it.
	 *
	 * \sa play3DSound()
	 * \sa createPaused3DSound()
	 * \sa playSoundClocked()
	 * \sa playSoundInBackground()
	 * \sa createPausedSoundInBackground()
	 */
	GREM_API(audio) SoundInstanceID play3DSoundClocked(Duration time, const Sound& sound, vec3 position, vec3 velocity, float volume = -1.0f);

	/**
	 * Check if a specific sound instance is currently paused.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \return true if the sound instance still exists and is currently paused,
	 *         false otherwise.
	 *
	 * \sa isSoundStopped()
	 * \sa pauseSound()
	 * \sa resumeSound()
	 */
	[[nodiscard]] GREM_API(audio) bool isSoundPaused(SoundInstanceID id) const noexcept;

	/**
	 * Check if a specific sound instance has finished playing.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \return true if the sound instance has been stopped or finished playing
	 *         and no longer exists, false otherwise.
	 *
	 * \sa isSoundPaused()
	 * \sa stopSound()
	 */
	[[nodiscard]] GREM_API(audio) bool isSoundStopped(SoundInstanceID id) const noexcept;

	/**
	 * Check if a specific sound instance is set to repeat instead of stopping.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \return true if the sound instance still exists and is currently set to
	 *         play on repeat, false otherwise.
	 *
	 * \sa isSoundStopped()
	 * \sa setSoundLooping()
	 */
	[[nodiscard]] GREM_API(audio) bool isSoundLooping(SoundInstanceID id) const noexcept;

	/**
	 * Get the current playback time point of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \return the current time point, measured from the beginning of the sound,
	 *         or an empty optional if the sound has stopped or does not exist.
	 *
	 * \sa seekToSoundTime()
	 */
	[[nodiscard]] GREM_API(audio) Optional<Duration> getSoundTime(SoundInstanceID id) const;

	/**
	 * Stop a specific sound instance and remove it.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa isSoundStopped()
	 * \sa scheduleSoundStop()
	 */
	GREM_API(audio) void stopSound(SoundInstanceID id) noexcept;

	/**
	 * Pause a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \note If the given sound instance doesn't exist, or if it is already
	 *       paused, this function has no effect.
	 *
	 * \sa isSoundPaused()
	 * \sa resumeSound()
	 * \sa scheduleSoundPause()
	 */
	GREM_API(audio) void pauseSound(SoundInstanceID id);

	/**
	 * Unpause and resume a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 *
	 * \note If the given sound instance doesn't exist, or if it is not paused,
	 *       this function has no effect.
	 *
	 * \sa isSoundPaused()
	 * \sa pauseSound()
	 */
	GREM_API(audio) void resumeSound(SoundInstanceID id);

	/**
	 * Schedule for a specific sound instance to stop playing and remove itself
	 * when the playback reaches a specific time point.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param timePointInSound the time point, measured from the beginning of
	 *        the sound, where the sound instance will stop itself.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa isSoundStopped()
	 * \sa stopSound()
	 */
	GREM_API(audio) void scheduleSoundStop(SoundInstanceID id, Duration timePointInSound);

	/**
	 * Schedule for a specific sound instance to pause itself when the playback
	 * reaches a specific time point.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param timePointInSound the time point, measured from the beginning of
	 *        the sound, where the sound instance will pause itself.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa isSoundPaused()
	 * \sa pauseSound()
	 */
	GREM_API(audio) void scheduleSoundPause(SoundInstanceID id, Duration timePointInSound);

	/**
	 * Set whether a specific sound instance should repeat or stop when it ends.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newLooping true to set the sound to play on repeat, false to
	 *        disable looping and stop the sound when it ends.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa isSoundLooping()
	 * \sa stopSound()
	 */
	GREM_API(audio) void setSoundLooping(SoundInstanceID id, bool newLooping);

	/**
	 * Set the current playback time point of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param timePointInSound the time point, measured from the beginning of
	 *        the sound, to seek to.
	 *
	 * \note If the given sound instance exists, it will continue playing after
	 *       seeking unless it was paused.
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa getSoundTime()
	 */
	GREM_API(audio) void seekToSoundTime(SoundInstanceID id, Duration timePointInSound);

	/**
	 * Set the current 3D position of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newPosition new position of the sound instance, in sound stage
	 *        coordinates.
	 *
	 * \note The effect of this function will only apply after the next call to
	 *       update().
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundVelocity()
	 * \sa setSoundPositionAndVelocity()
	 */
	GREM_API(audio) void setSoundPosition(SoundInstanceID id, vec3 newPosition);

	/**
	 * Set the current 3D velocity of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newVelocity new linear velocity of the sound instance.
	 *
	 * \note The effect of this function will only apply after the next call to
	 *       update().
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundPosition()
	 * \sa setSoundPositionAndVelocity()
	 */
	GREM_API(audio) void setSoundVelocity(SoundInstanceID id, vec3 newVelocity);

	/**
	 * Set both the 3D position and 3D velocity of a specific sound instance at
	 * the same time.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newPosition new position of the sound instance, in sound stage
	 *        coordinates.
	 * \param newVelocity new linear velocity of the sound instance.
	 *
	 * \note The effect of this function will only apply after the next call to
	 *       update().
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundPosition()
	 * \sa setSoundVelocity()
	 */
	GREM_API(audio) void setSoundPositionAndVelocity(SoundInstanceID id, vec3 newPosition, vec3 newVelocity);

	/**
	 * Set the panning of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newPanning new panning value.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundVolume()
	 */
	GREM_API(audio) void setSoundPanning(SoundInstanceID id, Panning newPanning);

	/**
	 * Set the volume of a specific sound instance.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newVolume new volume, see SoundOptions::volume.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setOutputVolume()
	 * \sa fadeSoundVolume()
	 */
	GREM_API(audio) void setSoundVolume(SoundInstanceID id, float newVolume);

	/**
	 * Fade the volume of a specific sound instance towards a target volume over
	 * a given duration.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param targetVolume new volume to fade towards, see SoundOptions::volume.
	 * \param fadeDuration duration of time to fade over.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundVolume()
	 */
	GREM_API(audio) void fadeSoundVolume(SoundInstanceID id, float targetVolume, Duration fadeDuration);

	/**
	 * Set the relative playback speed of a specific sound instance.
	 *
	 * The effective sample rate of the playing sound is adjusted by this
	 * factor, meaning that a value of 1 represents no change, i.e. 100% of the
	 * original playback speed of the loaded sound file.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param newPlaybackSpeed new relative playback speed. Must be greater than
	 *        0.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa fadeSoundPlaybackSpeed()
	 */
	GREM_API(audio) void setSoundPlaybackSpeed(SoundInstanceID id, float newPlaybackSpeed);

	/**
	 * Fade the relative playback speed of a specific sound instance towards a
	 * target relative playback speed over a given duration.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param targetPlaybackSpeed new relative playback speed to fade towards.
	 *        Must be greater than 0.
	 * \param fadeDuration duration of time to fade over.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \sa setSoundPlaybackSpeed()
	 */
	GREM_API(audio) void fadeSoundPlaybackSpeed(SoundInstanceID id, float targetPlaybackSpeed, Duration fadeDuration);

	/**
	 * Set the listener-relative speaker position of an output audio channel.
	 *
	 * \param outputChannelIndex index of the output channel to set the speaker
	 *        position of. Must be less than getOutputChannelCount().
	 * \param newPosition new speaker position to set.
	 */
	GREM_API(audio) void setOutputChannelSpeakerPosition(size_t outputChannelIndex, vec3 newPosition);

	/**
	 * Set the global master volume of the sound stage.
	 *
	 * \param newVolume new volume, see SoundStageOptions::volume.
	 *
	 * \sa setSoundVolume()
	 * \sa fadeOutputVolume()
	 */
	GREM_API(audio) void setOutputVolume(float newVolume);

	/**
	 * Fade the global master volume of the sound stage towards a target volume
	 * over a given duration.
	 *
	 * \param targetVolume new volume to fade towards, see
	 *        SoundStageOptions::outputVolume.
	 * \param fadeDuration duration of time to fade over.
	 *
	 * \sa setOutputVolume()
	 */
	GREM_API(audio) void fadeOutputVolume(float targetVolume, Duration fadeDuration);

	/**
	 * Set the speed of sound in the sound stage.
	 *
	 * \param newSpeedOfSound new speed of sound, see
	 *        SoundStageOptions::speedOfSound.
	 */
	GREM_API(audio) void setSpeedOfSound(float newSpeedOfSound);

	/**
	 * Set the maximum total number of sound instances that can play
	 * simultaneously.
	 *
	 * \param newMaxSimultaneousSoundInstanceCount new number of sound
	 *        instances, see
	 *        SoundStageOptions::maxSimultaneousSoundInstanceCount.
	 */
	GREM_API(audio) void setMaxSimultaneousSoundInstanceCount(size_t newMaxSimultaneousSoundInstanceCount);

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
	 * Get the number of output audio channels.
	 *
	 * \return the number of channels.
	 */
	[[nodiscard]] GREM_API(audio) size_t getOutputChannelCount() const noexcept;

	/**
	 * Get the listener-relative speaker position of an output audio channel.
	 *
	 * \param outputChannelIndex index of the output channel to get the speaker
	 *        position of.
	 *
	 * \return the speaker position of the given channel, or (0, 0, 0) if the
	 *         channel index was out of range.
	 */
	[[nodiscard]] GREM_API(audio) vec3 getOutputChannelSpeakerPosition(size_t outputChannelIndex) const noexcept;

	/**
	 * Get the global master volume of the sound stage.
	 *
	 * \return the current global master volume, see
	 *         SoundStageOptions::outputVolume.
	 */
	[[nodiscard]] GREM_API(audio) float getOutputVolume() const noexcept;

	/**
	 * Get the speed of sound in the sound stage.
	 *
	 * \return the current speed of sound.
	 */
	[[nodiscard]] GREM_API(audio) float getSpeedOfSound() const noexcept;

	/**
	 * Get the maximum total number of sound instances that can play
	 * simultaneously.
	 *
	 * \return the maximum number of simultaneous sound instances.
	 */
	[[nodiscard]] GREM_API(audio) size_t getMaxSimultaneousSounds() const noexcept;

	/**
	 * Calculate the fast Fourier transform of the audio currently playing
	 * through the sound stage.
	 *
	 * \return A sequence of 256 values representing the discrete Fourier
	 *         transform of the currently playing audio, from low to high
	 *         frequencies.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound stage in order to produce meaningful results.
	 *
	 * \sa SoundStageOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) Array<float, 256> calculateOutputFastFourierTransformStatistics() const;

	/**
	 * Get the sound wave currently playing through the sound stage.
	 *
	 * \return a sequence of the 256 latest audio samples played through the
	 *         sound stage.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound stage in order to produce meaningful results.
	 *
	 * \sa SoundStageOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) Array<float, 256> getOutputWaveStatistics() const;

	/**
	 * Get an approximation of the volume of the audio currently playing through
	 * a specific channel of the sound stage.
	 *
	 * \param outputChannelIndex index of the channel to get the volume of.
	 *
	 * \return an approximation of the volume of the current audio playing
	 *         through the given sound stage channel, or 0.0f if the channel
	 *         index was out of range.
	 *
	 * \note This function requires statistics gathering to be enabled on the
	 *       sound stage in order to produce meaningful results.
	 *
	 * \sa SoundMixOptions::enableStatistics
	 * \sa setStatisticsEnabled()
	 */
	[[nodiscard]] GREM_API(audio) float getOutputChannelVolumeStatistics(size_t outputChannelIndex) const;

	/**
	 * Set the filter at a particular global filter slot of the sound stage.
	 *
	 * This replaces any existing filter at the given slot.
	 *
	 * \param filterSlotIndex filter slot index to set the filter of. Must be
	 *        less than Sound::MAX_FILTER_COUNT.
	 * \param newFilter new filter to insert.
	 *
	 * \throws audio::Error on failure to create the filter.
	 * \throws std::length_error if the maximum filter size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \remark A custom filter is any trivially copyable and trivially
	 *         destructible type that provides the following public member
	 *         function:
	 *         ```cpp
	 *         void filter(Span<float> samples, size_t channelCount, size_t sampleCountPerChannel, float sampleRate, double time);
	 *         ```
	 *         Filters may also provide a field named `parameters` of an
	 *         aggregate type containing a scalar field for each filter
	 *         parameter that should be adjustable on-the-fly.
	 */
	template <filter F>
	void setGlobalFilter(size_t filterSlotIndex, const F& newFilter) {
		if constexpr (requires(const void* data) { F::GREM_private_createBuiltinFilter(data); }) {
			detail::Filter filter{F::GREM_private_createBuiltinFilter(static_cast<const void*>(static_cast<const F*>(&newFilter)))};
			setGlobalFilterImplementation(filterSlotIndex, std::move(filter));
		} else {
			static_assert(trivially_copyable<F>);
			static constexpr detail::FilterFunction doFilter =
				[](byte* data, Span<float> samples, size_t channelCount, size_t sampleCountPerChannel, float sampleRate, double time) -> void {
				std::launder(reinterpret_cast<F*>(data))->filter(samples, channelCount, sampleCountPerChannel, sampleRate, time);
			};
			if constexpr (requires { newFilter.parameters; }) {
				using Parameters = std::remove_cvref_t<decltype(newFilter.parameters)>;
				const size_t parametersOffset = std::launder(reinterpret_cast<const byte*>(&newFilter.parameters)) - std::launder(reinterpret_cast<const byte*>(&newFilter));
				detail::Filter filter{detail::createCustomFilter(asBytes(Span{static_cast<const F*>(&newFilter), 1}), parametersOffset,
					detail::FILTER_PARAMETER_DESCRIPTIONS<Parameters>, detail::FILTER_PARAMETER_INFOS<Parameters>, doFilter)};
				setGlobalFilterImplementation(filterSlotIndex, std::move(filter));
			} else {
				detail::Filter filter{detail::createCustomFilter(asBytes(Span{static_cast<const F*>(&newFilter), 1}), 0, {}, doFilter)};
				setGlobalFilterImplementation(filterSlotIndex, std::move(filter));
			}
		}
	}

	/**
	 * Remove the filter at a particular global filter slot of the sound stage.
	 *
	 * \param filterSlotIndex filter slot index to clear the filter of. Must be
	 *        less than Sound::MAX_FILTER_COUNT.
	 */
	GREM_API(audio) void clearGlobalFilter(size_t filterSlotIndex) noexcept;

	/**
	 * Set a live parameter of an active filter at a particular filter slot of a
	 * sound instance in the sound stage.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to set the value of. If the given sound
	 *         instance exists, its sound source must have a filter at the given
	 *         slot whose `parameters` member is an aggregate type that is the
	 *         same as this member pointer's object type.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        set. Must be less than Sound::MAX_FILTER_COUNT.
	 * \param newValue new value to set the parameter to.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         sound.setFilter(filterSlotIndex, MyFilter{});
	 *         const SoundInstanceID id = soundStage.playSound(sound);
	 *         soundStage.setSoundFilterParameter<&MyFilter::Parameters::wet>(id, filterSlotIndex, 0.5f);
	 *         ```
	 */
	template <auto Member>
	void setSoundFilterParameter(SoundInstanceID id, size_t filterSlotIndex, meta::member_pointer_target_type_t<Member> newValue) {
		setSoundFilterParameterImplementation(id, filterSlotIndex, meta::member_pointer_index_v<Member>, static_cast<float>(newValue));
	}

	/**
	 * Set a live parameter of an active filter at a particular global filter
	 * slot of the sound stage.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to set the value of. The sound stage must
	 *         have a global filter at the given slot whose `parameters` member
	 *         is an aggregate type that is the same as this member pointer's
	 *         object type.
	 *
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        set. Must be less than Sound::MAX_FILTER_COUNT.
	 * \param newValue new value to set the parameter to.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         soundStage.setGlobalFilter(filterSlotIndex, MyFilter{});
	 *         soundStage.setGlobalFilterParameter<&MyFilter::Parameters::wet>(filterSlotIndex, 0.5f);
	 *         ```
	 */
	template <auto Member>
	void setGlobalFilterParameter(size_t filterSlotIndex, meta::member_pointer_target_type_t<Member> newValue) {
		setGlobalFilterParameterImplementation(filterSlotIndex, meta::member_pointer_index_v<Member>, static_cast<float>(newValue));
	}

	/**
	 * Fade a live parameter of an active filter at a particular filter slot of a
	 * sound instance in the sound stage towards a target value over a given
	 * duration.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to fade the value of. If the given sound
	 *         instance exists, its sound source must have a filter at the given
	 *         slot whose `parameters` member is an aggregate type that is the
	 *         same as this member pointer's object type.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        set. Must be less than Sound::MAX_FILTER_COUNT.
	 * \param targetValue new value to fade the parameter towards.
	 * \param fadeDuration duration of time to fade over.
	 *
	 * \note If the given sound instance doesn't exist, this function has no
	 *       effect.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         sound.setFilter(filterSlotIndex, MyFilter{});
	 *         const SoundInstanceID id = soundStage.playSound(sound);
	 *         soundStage.fadeSoundFilterParameter<&MyFilter::Parameters::wet>(id, filterSlotIndex, 0.5f, 2_seconds);
	 *         ```
	 */
	template <auto Member>
	void fadeSoundFilterParameter(SoundInstanceID id, size_t filterSlotIndex, meta::member_pointer_target_type_t<Member> targetValue, Duration fadeDuration) {
		fadeSoundFilterParameterImplementation(id, filterSlotIndex, meta::member_pointer_index_v<Member>, static_cast<float>(targetValue), fadeDuration);
	}

	/**
	 * Fade a live parameter of an active filter at a particular global filter
	 * slot of the sound stage towards a target value over a given duration.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to fade the value of. The sound stage must
	 *         have a global filter at the given slot whose `parameters` member
	 *         is an aggregate type that is the same as this member pointer's
	 *         object type.
	 *
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        set. Must be less than Sound::MAX_FILTER_COUNT.
	 * \param targetValue new value to fade the parameter towards.
	 * \param fadeDuration duration of time to fade over.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         sound.setGlobalFilter(filterSlotIndex, MyFilter{});
	 *         soundStage.fadeGlobalFilterParameter<&MyFilter::Parameters::wet>(filterSlotIndex, 0.5f, 2_seconds);
	 *         ```
	 */
	template <auto Member>
	void fadeGlobalFilterParameter(size_t filterSlotIndex, meta::member_pointer_target_type_t<Member> targetValue, Duration fadeDuration) {
		fadeGlobalFilterParameterImplementation(filterSlotIndex, meta::member_pointer_index_v<Member>, static_cast<float>(targetValue), fadeDuration);
	}

	/**
	 * Get a live parameter of an active filter at a particular filter slot of a
	 * sound instance in the sound stage.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to get the value of. If the given sound
	 *         instance exists, its sound source must have a filter at the given
	 *         slot whose `parameters` member is an aggregate type that is the
	 *         same as this member pointer's object type.
	 *
	 * \param id handle to the sound instance, acquired from when the sound
	 *        instance was created.
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        get. Must be less than Sound::MAX_FILTER_COUNT.
	 *
	 * \note If the given sound instance doesn't exist, this function returns an
	 *       unspecified value.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         sound.setFilter(filterSlotIndex, MyFilter{});
	 *         const SoundInstanceID id = soundStage.playSound(sound);
	 *         const float wet = soundStage.getSoundFilterParameter<&MyFilter::Parameters::wet>(id, filterSlotIndex);
	 *         ```
	 */
	template <auto Member>
	[[nodiscard]] meta::member_pointer_target_type_t<Member> getSoundFilterParameter(SoundInstanceID id, size_t filterSlotIndex) const {
		return getSoundFilterParameterImplementation(id, filterSlotIndex, meta::member_pointer_index_v<Member>);
	}

	/**
	 * Get a live parameter of an active filter at a particular global filter
	 * slot of the sound stage.
	 *
	 * \tparam Member pointer-to-member of the parameter field in the filter's
	 *         `parameters` struct to get the value of. The sound stage must
	 *         have a global filter at the given slot whose `parameters` member
	 *         is an aggregate type that is the same as this member pointer's
	 *         object type.
	 *
	 * \param filterSlotIndex filter slot index of the filter whose parameter to
	 *        get. Must be less than Sound::MAX_FILTER_COUNT.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         const size_t filterSlotIndex = 3;
	 *         soundStage.setGlobalFilter(filterSlotIndex, MyFilter{});
	 *         const float wet = soundStage.getGlobalFilterParameter<&MyFilter::Parameters::wet>(filterSlotIndex);
	 *         ```
	 */
	template <auto Member>
	[[nodiscard]] meta::member_pointer_target_type_t<Member> getGlobalFilterParameter(size_t filterSlotIndex) const {
		return getGlobalFilterParameterImplementation(filterSlotIndex, meta::member_pointer_index_v<Member>);
	}

	/**
	 * Get an opaque handle to the internal representation of the sound stage.
	 *
	 * \return an untyped non-owning pointer to the internal representation of
	 *         the sound stage.
	 *
	 * \note This function is used internally by the implementation of the audio
	 *       module and is not intended to be used outside of it. The returned
	 *       handle has no meaning to application code.
	 */
	[[nodiscard]] void* get() const noexcept {
		return engine.get();
	}

private:
	struct EngineDeleter {
		GREM_API(audio) void operator()(void* handle) const noexcept;
	};

	GREM_API(audio) void setGlobalFilterImplementation(size_t filterSlotIndex, detail::Filter filter);

	GREM_API(audio) void setSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex, float newValue);
	GREM_API(audio) void setGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex, float newValue);

	GREM_API(audio) void fadeSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex, float targetValue, Duration fadeDuration);
	GREM_API(audio) void fadeGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex, float targetValue, Duration fadeDuration);

	[[nodiscard]] GREM_API(audio) float getSoundFilterParameterImplementation(SoundInstanceID id, size_t filterSlotIndex, size_t attributeIndex) const;
	[[nodiscard]] GREM_API(audio) float getGlobalFilterParameterImplementation(size_t filterSlotIndex, size_t attributeIndex) const;

	UniqueHandle<void*, EngineDeleter> engine{};
	UniquePointer<detail::Filter[]> globalFilters{};
};

} // namespace grem::audio

#endif
