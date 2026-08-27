// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_FILTERS_HPP
#define GREM_AUDIO_FILTERS_HPP

#include <GREM/build_config.hpp>

#include <GREM/audio/SoundInstanceID.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>

namespace grem::audio {

class Sound;      // Forward declaration, to avoid a circular include of Sound.hpp.
class SoundStage; // Forward declaration, to avoid a circular include of SoundStage.hpp.

namespace detail {

//==============================================================================

// Example user-defined filter options (see ExampleFilter).
struct ExampleFilterOptions {
	float gain = 1.0f; // See ExampleFilter::Parameters::gain.
};

// Example user-defined filter definition, showing how custom filter types can be defined.
// Also used to test the audio::filter concept.
// This example implements a basic amplification filter with a configurable gain parameter.
class ExampleFilter {
public:
	// Dynamic filter parameters that can be set/faded at any time during playback.
	// If present, this must be a public field with the exact name "parameters" of an aggregate struct type.
	// The struct type can have any name, but "Parameters" is recommended for consistency with the built-in filters.
	// The following parameter types are supported:
	// - bool
	// - int8_t
	// - int16_t
	// - int32_t
	// - uint8_t
	// - uint16_t
	// - uint32_t
	// - float32_t (float)
	struct Parameters {
		float gain; // Example filter parameter. Controls gain in this example, i.e. how much the filter should amplify the sound.
	} parameters;

	// Constructor for initializing the filter parameters, which can have any signature (or be omitted).
	// This pattern of taking a separate "options" struct is not required, but is recommended for extensibility and ease of use.
	explicit ExampleFilter(const ExampleFilterOptions& options)
		: parameters{.gain = options.gain} {}

	// Main filter callback. Must be public and have this exact function signature.
	void filter(Span<float> samples, size_t channelCount, size_t sampleCountPerChannel, float sampleRate, double time) const {
		// Arguments provided by the sound engine:
		// - samples:               Writable view over the sound samples to apply the filter to, consisting of `sampleCountPerChannel` contiguous samples for each channel in `channelCount`.
		// - channelCount:          Number of separate channels that the `samples` buffer is split into.
		// - sampleCountPerChannel: Number of samples per channel in `samples`. Always equal to `samples.size() / channelCount`.
		// - sampleRate:            Sample rate of the sound, in Hertz (samples per second).
		// - time:                  Current playback time point in the sound stream, in seconds.

		// This basic filter doesn't need to use any arguments besides `samples` since it just applies uniformly to all samples across all channels.
		(void)channelCount;
		(void)sampleCountPerChannel;
		(void)sampleRate;
		(void)time;

		// Example filter behavior: Amplify each sample by the specified gain parameter.
		for (float& sample : samples) {
			sample *= parameters.gain;
		}
	}
};

//==============================================================================

enum class FilterParameterType : uint32_t { // NOLINT(performance-enum-size)
	BOOL,
	INT8,
	INT16,
	INT32,
	UINT8,
	UINT16,
	UINT32,
	FLOAT32,
};

struct FilterParameterDescription {
	FilterParameterType type;
	uint32_t byteOffset;
};

struct FilterParameterInfo {
	CStringView name;
	float minValue;
	float maxValue;
};

template <typename FilterParameterStruct>
[[nodiscard]] constexpr auto getFilterParameterDescriptions() {
	static_assert(aggregate<FilterParameterStruct>);
	static_assert(standard_layout<FilterParameterStruct>);
	Array<FilterParameterDescription, meta::aggregate_size_v<FilterParameterStruct>> result{};
	size_t byteOffset = 0;
	meta::forEachIndexedFieldType<FilterParameterStruct>([&]<typename T>(auto index, meta::Type<T>) -> void {
		FilterParameterType type{};
		if constexpr (same_as<T, bool>) {
			type = FilterParameterType::BOOL;
		} else if constexpr (enumeration<T>) {
			if constexpr (sizeof(T) == 1) {
				type = FilterParameterType::UINT8;
			} else if constexpr (sizeof(T) == 2) {
				type = FilterParameterType::UINT16;
			} else if constexpr (sizeof(T) == 4) {
				type = FilterParameterType::UINT32;
			} else {
				unreachable();
			}
		} else if constexpr (integral<T>) {
			if constexpr (signed_integral<T>) {
				if constexpr (sizeof(T) == 1) {
					type = FilterParameterType::INT8;
				} else if constexpr (sizeof(T) == 2) {
					type = FilterParameterType::INT16;
				} else if constexpr (sizeof(T) == 4) {
					type = FilterParameterType::INT32;
				} else {
					unreachable();
				}
			} else {
				if constexpr (sizeof(T) == 1) {
					type = FilterParameterType::UINT8;
				} else if constexpr (sizeof(T) == 2) {
					type = FilterParameterType::UINT16;
				} else if constexpr (sizeof(T) == 4) {
					type = FilterParameterType::UINT32;
				} else {
					unreachable();
				}
			}
		} else if constexpr (floating_point<T>) {
			if constexpr (sizeof(T) == 4) {
				type = FilterParameterType::FLOAT32;
			} else {
				unreachable();
			}
		} else {
			unreachable();
		}

		byteOffset = roundUpToMultiple(byteOffset, alignof(T));

		GREM_ASSERT(byteOffset <= size_t{Limits<uint32_t>::MAX});

		result[index] = {
			.type = type,
			.byteOffset = static_cast<uint32_t>(byteOffset),
		};
		byteOffset += sizeof(T);
	});
	return result;
}

template <typename FilterParameterStruct>
[[nodiscard]] constexpr auto getFilterParameterInfos() {
	static_assert(aggregate<FilterParameterStruct>);
	static_assert(standard_layout<FilterParameterStruct>);
	Array<FilterParameterInfo, meta::aggregate_size_v<FilterParameterStruct>> result{};
	meta::forEachIndexedFieldType<FilterParameterStruct>([&]<typename T>(auto index, meta::Type<T>) -> void {
		float minValue = 0.0f;
		float maxValue = 0.0f;
		if constexpr (same_as<T, bool>) {
			minValue = 0.0f;
			maxValue = 1.0f;
		} else if constexpr (enumeration<T>) {
			if constexpr (sizeof(T) == 1) {
				minValue = 0.0f;
				maxValue = float{Limits<uint8_t>::MAX};
			} else if constexpr (sizeof(T) == 2) {
				minValue = 0.0f;
				maxValue = float{Limits<uint16_t>::MAX};
			} else if constexpr (sizeof(T) == 4) {
				minValue = 0.0f;
				maxValue = Limits<float>::MAX;
			} else {
				unreachable();
			}
		} else if constexpr (floating_point<T>) {
			minValue = Limits<float>::MIN;
			maxValue = Limits<float>::MAX;
		} else if constexpr (integral<T>) {
			if constexpr (int64_t{Limits<T>::MIN} <= int64_t{Limits<int32_t>::MIN}) {
				minValue = Limits<float>::MIN;
			} else {
				minValue = float{Limits<T>::MIN};
			}
			if constexpr (uint64_t{Limits<T>::MAX} >= uint64_t{Limits<int32_t>::MAX}) {
				maxValue = Limits<float>::MAX;
			} else {
				maxValue = float{Limits<T>::MAX};
			}
		} else {
			unreachable();
		}

		result[index] = {
			.name = meta::aggregate_field_name_v<index, FilterParameterStruct>,
			.minValue = minValue,
			.maxValue = maxValue,
		};
	});
	return result;
}

template <typename FilterParameterStruct>
inline constexpr auto FILTER_PARAMETER_DESCRIPTIONS = getFilterParameterDescriptions<FilterParameterStruct>();

template <typename FilterParameterStruct>
inline constexpr auto FILTER_PARAMETER_INFOS = getFilterParameterInfos<FilterParameterStruct>();

using FilterFunction = void (*)(byte* data, Span<float> samples, size_t channelCount, size_t sampleCountPerChannel, float sampleRate, double time);

struct FilterDeleter {
	GREM_API(audio) void operator()(void* handle) const noexcept;
};

using Filter = UniqueHandle<void*, FilterDeleter>;

[[nodiscard]] GREM_API(audio) void* createCustomFilter(Span<const byte> initialData, size_t parametersOffset, Span<const FilterParameterDescription> parameterDescriptions,
	Span<const FilterParameterInfo> parameterInfos, FilterFunction doFilter);

} // namespace detail

/**
 * Concept that checks if a type is a valid sound filter type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept filter = trivially_copyable<T> && ((requires(const void* data) {
	T::GREM_private_createBuiltinFilter(data);
}) || (requires(T t, const Span<float> samples, const size_t channelCount, const size_t sampleCountPerChannel, const float sampleRate, const double time) {
	t.filter(samples, channelCount, sampleCountPerChannel, sampleRate, time);
}));

static_assert(filter<detail::ExampleFilter>); // Make sure the example filter is considered valid.

/**
 * Filter type of a BiquadResonantFilter.
 */
enum class BiquadResonantFilterType : int { // NOLINT(performance-enum-size)
	LOWPASS = 0,                            ///< Preserve the sound frequencies below the cutoff frequency, filter those above.
	HIGHPASS = 1,                           ///< Preserve the sound frequencies above the cutoff frequency, filter those below.
	BANDPASS = 2,                           ///< Preserve the sound frequencies near the cutoff frequency, filter those above and below.
};

/**
 * Configuration options for a BiquadResonantFilter.
 */
struct BiquadResonantFilterOptions {
	/**
	 * Filter type that determines which sound frequencies to preserve/filter.
	 */
	BiquadResonantFilterType type = BiquadResonantFilterType::LOWPASS;

	/**
	 * Cutoff frequency of the filter, in Hertz.
	 *
	 * \note Range: [10, 8000]
	 *
	 * \sa BiquadResonantFilterOptions::type
	 */
	float cutoffFrequency = 4000.0f;

	/**
	 * Resonance/sharpness parameter of the filter.
	 *
	 * Higher values yield a sharper cutoff.
	 *
	 * \note Range: [0.1, 20]
	 */
	float resonance = 1.0f;
};

/**
 * Biquadratic resonant filter for filtering out frequencies above/below a
 * threshold.
 */
class BiquadResonantFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Dynamic filter parameters that can be set/faded on-the-fly.
	 */
	struct Parameters {
		float wet = 1.0f;      ///< Filter active amount (0 for unfiltered, 1 for fully filtered).
		float cutoffFrequency; ///< See BiquadResonantFilterOptions::cutoffFrequency.
		float resonance;       ///< See BiquadResonantFilterOptions::resonance.
	} parameters;

	/**
	 * Construct a biquadratic resonant filter.
	 *
	 * \param options filter options, see BiquadResonantFilterOptions.
	 */
	explicit BiquadResonantFilter(const BiquadResonantFilterOptions& options)
		: parameters{
			  .cutoffFrequency = options.cutoffFrequency,
			  .resonance = options.resonance,
		  }
		, type(options.type) {}

private:
	BiquadResonantFilterType type = BiquadResonantFilterType::LOWPASS;
};

/**
 * Configuration options for an EchoFilter.
 */
struct EchoFilterOptions {
	/**
	 * Echo delay in seconds.
	 *
	 * \note Range: (0, inf)
	 */
	float delay = 0.5f; // (0, inf)

	/**
	 * Echo decay multiplier.
	 *
	 * \note Range: (0, 1)
	 */
	float decay = 0.7f;

	/**
	 * Filter parameter.
	 *
	 * \note Range: [0, 1)
	 */
	float filter = 0.0f;
};

/**
 * Echo filter that repeats the sound at lower and lower volumes until it fades
 * out.
 */
class EchoFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Construct an echo filter.
	 *
	 * \param options filter options, see EchoFilterOptions.
	 */
	explicit EchoFilter(const EchoFilterOptions& options)
		: options(options) {}

private:
	EchoFilterOptions options;
};

/**
 * Configuration options for a LoFiFilter.
 */
struct LoFiFilterOptions {
	/**
	 * Sample rate of the filter, in Hertz.
	 *
	 * \note Range: [100, 22000]
	 */
	float sampleRate = 11000.0f;

	/**
	 * Bit depth of the filter.
	 *
	 * \note Range: [0.5, 16]
	 */
	float bitDepth = 4.0f;
};

/**
 * Signal degrading lo-fi filter for reducing the sound's bit depth and sample
 * rate.
 */
class LoFiFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Dynamic filter parameters that can be set/faded on-the-fly.
	 */
	struct Parameters {
		float wet = 1.0f; ///< Filter active amount (0 for unfiltered, 1 for fully filtered).
		float sampleRate; ///< See LoFiFilterOptions::sampleRate.
		float bitDepth;   ///< See LoFiFilterOptions::bitDepth.
	} parameters;

	/**
	 * Construct a lo-fi filter.
	 *
	 * \param options filter options, see LoFiFilterOptions.
	 */
	explicit LoFiFilter(const LoFiFilterOptions& options)
		: parameters{
			  .sampleRate = options.sampleRate,
			  .bitDepth = options.bitDepth,
		  } {}
};

/**
 * Configuration options for a FlangerFilter.
 */
struct FlangerFilterOptions {
	/**
	 * Delay of the flanger effect, in seconds.
	 *
	 * \note Range: [0.001, 0.1]
	 */
	float delay = 0.05f;

	/**
	 * Frequency of the flanger effect, in Hertz.
	 *
	 * \note: Range: [0.001, 100]
	 */
	float frequency = 10.0f;
};

/**
 * Flanger effect filter.
 *
 * Can be used to make voices sound robotic, for example.
 */
class FlangerFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Dynamic filter parameters that can be set/faded on-the-fly.
	 */
	struct Parameters {
		float wet = 1.0f; ///< Filter active amount (0 for unfiltered, 1 for fully filtered).
		float delay;      ///< See FlangerFilterOptions::delay.
		float frequency;  ///< See FlangerFilterOptions::frequency.
	} parameters;

	/**
	 * Construct a flanger filter.
	 *
	 * \param options filter options, see FlangerFilterOptions.
	 */
	explicit FlangerFilter(const FlangerFilterOptions& options)
		: parameters{
			  .delay = options.delay,
			  .frequency = options.frequency,
		  } {}
};

/**
 * Configuration options for a DCRemovalFilter.
 */
struct DCRemovalFilterOptions {
	/**
	 * Length of the averaging buffer, in seconds.
	 *
	 * \note Range: [0, 1]
	 */
	float length = 0.1f;
};

/**
 * Filter for removing DC signal from the sound, i.e. centering the sound
 * waveform around 0.
 *
 * The filter works by calculating the average sample value over a relatively
 * long period of time and subtracting it from the output.
 */
class DCRemovalFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Construct a DC removal filter.
	 *
	 * \param options filter options, see DCRemovalFilterOptions.
	 */
	explicit DCRemovalFilter(const DCRemovalFilterOptions& options)
		: options(options) {}

private:
	DCRemovalFilterOptions options;
};

/**
 * Configuration options for a FreeverbFilter.
 */
struct FreeverbFilterOptions {
	/**
	 * Freeverb mode parameter.
	 *
	 * Set to true to freeze the audio currently flowing through the filter.
	 */
	bool freeze = false;

	/**
	 * Freeverb room size parameter.
	 *
	 * \note Range: (0, 1]
	 */
	float roomSize = 0.5f;

	/**
	 * Freeverb damping parameter.
	 *
	 * \note Range: [0, 1]
	 */
	float damping = 0.5f;

	/**
	 * Freeverb width parameter.
	 *
	 * \note Range: (0, 1]
	 */
	float width = 1.0f;
};

/**
 * Reverb filter based on Freeverb (high quality, but relatively heavy).
 */
class FreeverbFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Dynamic filter parameters that can be set/faded on-the-fly.
	 */
	struct Parameters {
		float wet = 1.0f; ///< Filter active amount (0 for unfiltered, 1 for fully filtered).
		bool freeze;      ///< See FreeverbFilterOptions::freeze.
		float roomSize;   ///< See FreeverbFilterOptions::roomSize.
		float damping;    ///< See FreeverbFilterOptions::damping.
		float width;      ///< See FreeverbFilterOptions::width.
	} parameters;

	/**
	 * Construct a Freeverb filter.
	 *
	 * \param options filter options, see FreeverbFilterOptions.
	 */
	explicit FreeverbFilter(const FreeverbFilterOptions& options)
		: parameters{
			  .freeze = options.freeze,
			  .roomSize = options.roomSize,
			  .damping = options.damping,
			  .width = options.width,
		  } {}
};

/**
 * Configuration options for an EqualizationFilter.
 */
struct EqualizationFilterOptions {
	/**
	 * Volume/gain of frequency band 1 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band1Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 2 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band2Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 3 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band3Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 4 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band4Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 5 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band5Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 6 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band6Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 7 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band7Volume = 1.0f;

	/**
	 * Volume/gain of frequency band 8 of 8.
	 *
	 * \note Range: [0, 4]
	 */
	float band8Volume = 1.0f;
};

/**
 * Equalization (EQ) filter for adjusting the volume levels of 8 different
 * frequency bands in the sound.
 */
class EqualizationFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	struct Parameters {
		float wet = 1.0f;  ///< Filter active amount (0 for unfiltered, 1 for fully filtered).
		float band1Volume; ///< See EqualizationFilterOptions::band1Volume.
		float band2Volume; ///< See EqualizationFilterOptions::band2Volume.
		float band3Volume; ///< See EqualizationFilterOptions::band3Volume.
		float band4Volume; ///< See EqualizationFilterOptions::band4Volume.
		float band5Volume; ///< See EqualizationFilterOptions::band5Volume.
		float band6Volume; ///< See EqualizationFilterOptions::band6Volume.
		float band7Volume; ///< See EqualizationFilterOptions::band7Volume.
		float band8Volume; ///< See EqualizationFilterOptions::band8Volume.
	} parameters;

	/**
	 * Construct an equalization filter.
	 *
	 * \param options filter options, see EqualizationFilterOptions.
	 */
	explicit EqualizationFilter(const EqualizationFilterOptions& options)
		: parameters{
			  .band1Volume = options.band1Volume,
			  .band2Volume = options.band2Volume,
			  .band3Volume = options.band3Volume,
			  .band4Volume = options.band4Volume,
			  .band5Volume = options.band5Volume,
			  .band6Volume = options.band6Volume,
			  .band7Volume = options.band7Volume,
			  .band8Volume = options.band8Volume,
		  } {}
};

/**
 * Configuration options for a DuckingFilter.
 */
struct DuckingFilterOptions {
	/**
	 * On-ramp parameter.
	 *
	 * \note Range: [0, 1]
	 */
	float onRamp = 0.05f;

	/**
	 * Off-ramp parameter.
	 *
	 * \note Range: [0, 1]
	 */
	float offRamp = 0.5f;

	/**
	 * Level parameter.
	 *
	 * \note Range: [0, 1]
	 */
	float level = 0.5f;
};

/**
 * Ducking filter for reducing the volume of a sound when another sound is
 * active.
 */
class DuckingFilter {
public:
	[[nodiscard]] GREM_API(audio) static void* GREM_private_createBuiltinFilter(const void* data); // Internal function. Do not call in user code.

	/**
	 * Construct a ducking filter.
	 *
	 * \param soundStage sound stage containing the sound instance to listen to.
	 * \param listenTo handle to the sound instance to listen to.
	 * \param options filter options, see DuckingFilterOptions.
	 */
	explicit DuckingFilter(SoundStage& soundStage, SoundInstanceID listenTo, const DuckingFilterOptions& options)
		: soundStage(&soundStage)
		, listenTo(listenTo)
		, options(options) {}

private:
	SoundStage* soundStage;
	SoundInstanceID listenTo;
	DuckingFilterOptions options;
};

} // namespace grem::audio

#endif
