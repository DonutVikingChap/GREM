// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_SOUND_HPP
#define GREM_AUDIO_SOUND_HPP

#include <GREM/build_config.hpp>

#include <GREM/audio/filters.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>

#include <new>         // std::launder
#include <type_traits> // std::remove_cvref_t
#include <utility>     // std::move

namespace grem::audio {

/**
 * Distance attenuation/falloff model for 3D positional audio.
 */
enum class SoundAttenuationModel : uint8_t { // NOLINT(performance-enum-size)
	/**
	 * No distance attenuation; sound has the same volume regardless of distance
	 * between the sound instance and the listener.
	 */
	NO_ATTENUATION = 0,

	/**
	 * Attenuate the amplitude of the sound by the inverse distance between the
	 * sound instance and the listener according to the formula:
	 * ```
	 * gain = dmin / (dmin + r * (clamp(d, dmin, dmax) - dmin))
	 * ```
	 * where:
	 * - d is the linear distance between the sound instance and the listener,
	 * - dmin = minDistance,
	 * - dmax = maxDistance,
	 * - r = rolloffFactor.
	 *
	 * \warning When using this attenuation model:
	 *          - minDistance must be less than or equal to maxDistance.
	 *          - minDistance must be greater than 0.
	 *          - rolloffFactor must be greater than 0.
	 */
	INVERSE_DISTANCE = 1,

	/**
	 * Attenuate the amplitude of the sound by the linear distance between the
	 * sound instance and the listener according to the formula:
	 * ```
	 * gain = 1 - r * (clamp(d, dmin, dmax) - dmin) / (dmax - dmin)
	 * ```
	 * where:
	 * - d is the linear distance between the sound instance and the listener,
	 * - dmin = minDistance,
	 * - dmax = maxDistance,
	 * - r = rolloffFactor.
	 *
	 * \warning When using this attenuation model:
	 *          - minDistance must be less than or equal to maxDistance.
	 *          - rolloffFactor must be between 0 and 1 (inclusive).
	 */
	LINEAR_DISTANCE = 2,

	/**
	 * Attenuate the amplitude of the sound by the exponential distance between
	 * the sound instance and the listener according to the formula:
	 * ```
	 * gain = pow(clamp(d, dmin, dmax) / dmin, -r)
	 * ```
	 * where:
	 * - d is the linear distance between the sound instance and the listener,
	 * - dmin = minDistance,
	 * - dmax = maxDistance,
	 * - r = rolloffFactor.
	 *
	 * \warning When using this attenuation model:
	 *          - minDistance must be less than or equal to maxDistance.
	 *          - minDistance must be greater than 0.
	 *          - rolloffFactor must be greater than 0.
	 */
	EXPONENTIAL_DISTANCE = 3,
};

/**
 * Configuration options for a Sound.
 */
struct SoundOptions {
	/**
	 * Which distance attenuation/falloff model to use for 3D positional audio
	 * when playing this sound. See SoundAttenuationModel for the different
	 * alternatives.
	 *
	 * \remark The recommended model for most applications is
	 *         SoundAttenuationModel::INVERSE_DISTANCE.
	 */
	SoundAttenuationModel attenuationModel = SoundAttenuationModel::INVERSE_DISTANCE;

	/**
	 * Default volume of instances of this sound, which is used if no volume
	 * override is specified when the sound is played.
	 *
	 * When used, the amplitude of the playing sound is multiplied by this gain
	 * value, meaning that a value of 1 represents no change, i.e. 100% of the
	 * original volume of the loaded sound file.
	 */
	float volume = 1.0f;

	/**
	 * Minimum distance of the range where the distance between the sound
	 * instance and listener changes the sound attenuation/falloff for this
	 * sound.
	 *
	 * See SoundAttenuationModel for the effect this parameter has when using
	 * the different attenuation models, as well as the restrictions on its
	 * value for well-defined results.
	 *
	 * \warning When using the default attenuation model, this value must be
	 *          greater than 0, and must also be less than or equal to
	 *          maxDistance.
	 */
	float minDistance = 1.0f;

	/**
	 * Maximum distance of the range where the distance between the sound
	 * instance and listener changes sound attenuation/falloff for this sound.
	 *
	 * Beyond this range, the distance between sound instance and listener stops
	 * having an effect on the volume.
	 *
	 * See SoundAttenuationModel for the effect this parameter has when using
	 * the different attenuation models, as well as the restrictions on its
	 * value for well-defined results.
	 *
	 * \warning When using the default attenuation model, this value must be
	 *          greater than or equal to minDistance.
	 */
	float maxDistance = Limits<float>::MAX;

	/**
	 * Rolloff factor to use in the attenuation/falloff calculation for this
	 * sound.
	 *
	 * In general, a larger rolloff factor causes the sound volume to drop more
	 * steeply with the distance between the sound instance and listener.
	 *
	 * See SoundAttenuationModel for the effect this parameter has when using
	 * the different attenuation models, as well as the restrictions on its
	 * value for well-defined results.
	 *
	 * \warning When using the default attenuation model, this value must be
	 *          greater than 0.
	 */
	float rolloffFactor = 1.0f;

	/**
	 * Strength of the doppler effect for this sound.
	 *
	 * The doppler effect depends on the velocity of the sound instance and the
	 * listener as well as the speed of sound that is set in the SoundStage.
	 * When both velocities are 0, there is no doppler effect, and in that case
	 * this parameter makes no difference to the sound.
	 */
	float dopplerFactor = 1.0f;

	/**
	 * Simulate the delay due to the speed of sound between the sound being
	 * played and the sound being heard.
	 *
	 * When enabled, the delay depends on the distance between the sound
	 * instance and the listener as well as the sound speed set in the
	 * SoundStage.
	 */
	bool useDistanceDelay = false;

	/**
	 * Don't take the listener's sound stage position into account when playing
	 * this sound in 3D.
	 *
	 * When enabled, the position of the sound instance is treated as being
	 * relative to the listener, as if the listener's position is (0, 0, 0).
	 */
	bool listenerRelative = false;

	/**
	 * Default to playing instances of this sound on repeat instead of just
	 * playing them once.
	 */
	bool looping = false;

	/**
	 * Override any instances of this sound that are already playing when a new
	 * instance is played.
	 *
	 * Useful for making sure a certain sound effect never overlaps with itself
	 * when played multiple times.
	 */
	bool singleInstance = false;

	/**
	 * Stream in the contents of the sound file gradually as the sound plays
	 * instead of loading the whole file at once.
	 *
	 * \note This option has no effect when constructing a sound from a raw
	 *       array of samples.
	 */
	bool streamed = false;

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SoundOptions& other) const = default;
};

/**
 * Container for a particular sound wave that can be played in a SoundStage.
 *
 * A single loaded sound can be used to spawn multiple sound instances that play
 * the same sound at different times or in parallel, and with potentially
 * varying volumes, positions and velocities.
 */
class Sound {
public:
	/**
	 * Maximum number of filters that can be applied simultaneously to a sound.
	 */
	static constexpr size_t MAX_FILTER_COUNT = 8;

	/**
	 * Construct an invalid sound that must be reassigned before use.
	 */
	Sound() noexcept = default;

	/**
	 * Load a sound from a virtual file.
	 *
	 * The supported file formats are:
	 * - Vorbis (.ogg)
	 * - RIFF (.wav)
	 * - FLAC (.flac)
	 * - MP3 (.mp3)
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath virtual filepath of the sound file to load.
	 * \param options sound options, see SoundOptions.
	 *
	 * \throws File::Error on failure to open the file.
	 * \throws audio::Error on failure to load a sound from the file.
	 * \throws std::length_error if the maximum sound file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 */
	GREM_API(audio) Sound(const Filesystem& filesystem, CStringView filepath, const SoundOptions& options = {});

	/**
	 * Load a sound from an in-memory file.
	 *
	 * The supported file formats are:
	 * - Vorbis (.ogg)
	 * - RIFF (.wav)
	 * - FLAC (.flac)
	 * - MP3 (.mp3)
	 *
	 * \param fileContents contents of the sound file to load.
	 * \param options sound options, see SoundOptions.
	 *
	 * \throws audio::Error on failure to load a sound from the file.
	 * \throws std::length_error if the maximum sound file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(audio) explicit Sound(Span<const byte> fileContents, const SoundOptions& options = {});

	/**
	 * Construct a sound wave from a raw array of samples.
	 *
	 * \param samples array of unsigned 8-bit samples.
	 * \param sampleRate sample rate of the sound, in Hertz. For example:
	 *        44100.0f, 48000.0f or 96000.0f.
	 * \param channelCount number of sound channels. The number of samples must
	 *        be divisible by this number.
	 * \param options sound options, see SoundOptions.
	 *
	 * \throws audio::Error on failure to create the sound.
	 * \throws std::length_error if the maximum sound wave size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(audio) Sound(Span<const uint8_t> samples, float sampleRate, size_t channelCount = 1, const SoundOptions& options = {});

	/**
	 * Construct a sound wave from a raw array of samples.
	 *
	 * \param samples array of signed 16-bit samples.
	 * \param sampleRate sample rate of the sound, in Hertz. For example:
	 *        44100.0f, 48000.0f or 96000.0f.
	 * \param channelCount number of sound channels. The number of samples must
	 *        be divisible by this number.
	 * \param options sound options, see SoundOptions.
	 *
	 * \throws audio::Error on failure to create the sound.
	 * \throws std::length_error if the maximum sound wave size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(audio) Sound(Span<const int16_t> samples, float sampleRate, size_t channelCount = 1, const SoundOptions& options = {});

	/**
	 * Construct a sound wave from a raw array of samples.
	 *
	 * \param samples array of floating-point 32-bit samples.
	 * \param sampleRate sample rate of the sound, in Hertz. For example:
	 *        44100.0f, 48000.0f or 96000.0f.
	 * \param channelCount number of sound channels. The number of samples must
	 *        be divisible by this number.
	 * \param options sound options, see SoundOptions.
	 *
	 * \throws audio::Error on failure to create the sound.
	 * \throws std::length_error if the maximum sound wave size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(audio) Sound(Span<const float32_t> samples, float sampleRate, size_t channelCount = 1, const SoundOptions& options = {});

	/**
	 * Check if the sound has a value.
	 *
	 * \return true if the sound has a value, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return static_cast<bool>(sourceHandle);
	}

	/**
	 * Set the filter at a particular filter slot of the sound.
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
	void setFilter(size_t filterSlotIndex, const F& newFilter) {
		if constexpr (requires(const void* data) { F::GREM_private_createBuiltinFilter(data); }) {
			detail::Filter filter{F::GREM_private_createBuiltinFilter(static_cast<const void*>(static_cast<const F*>(&newFilter)))};
			setFilterImplementation(filterSlotIndex, std::move(filter));
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
				setFilterImplementation(filterSlotIndex, std::move(filter));
			} else {
				detail::Filter filter{detail::createCustomFilter(asBytes(Span{static_cast<const F*>(&newFilter), 1}), 0, {}, doFilter)};
				setFilterImplementation(filterSlotIndex, std::move(filter));
			}
		}
	}

	/**
	 * Remove the filter at a particular filter slot of the sound.
	 *
	 * \param filterSlotIndex filter slot index to clear the filter of. Must be
	 *        less than Sound::MAX_FILTER_COUNT.
	 */
	GREM_API(audio) void removeFilter(size_t filterSlotIndex) noexcept;

	/**
	 * Get the duration of the sound.
	 *
	 * \return the time duration between the start and end of the sound when
	 *         played at 1x speed.
	 */
	[[nodiscard]] GREM_API(audio) Duration getDuration() const noexcept;

	/**
	 * Get an opaque handle to the internal representation of the sound.
	 *
	 * \return an untyped non-owning pointer to the internal representation of
	 *         the sound.
	 *
	 * \note This function is used internally by the SoundStage implementation
	 *       and is not intended to be used outside of it. The returned handle
	 *       has no meaning to application code.
	 */
	[[nodiscard]] void* get() const noexcept {
		return sourceHandle.get();
	}

protected:
	GREM_API(audio) Sound(void* handle, const SoundOptions& options);

private:
	struct SoundFileDeleter {
		GREM_API(audio) void operator()(void* handle) const noexcept;
	};

	using SoundFileHandle = UniqueHandle<void*, SoundFileDeleter>;

	struct SourceDeleter {
		GREM_API(audio) void operator()(void* handle) const noexcept;
	};

	using SourceHandle = UniqueHandle<void*, SourceDeleter>;

	GREM_API(audio) void setFilterImplementation(size_t filterSlotIndex, detail::Filter filter);

	SoundFileHandle soundFileHandle{};
	SourceHandle sourceHandle{};
	UniquePointer<detail::Filter[]> filters{};
	bool streamed;
};

} // namespace grem::audio

#endif
