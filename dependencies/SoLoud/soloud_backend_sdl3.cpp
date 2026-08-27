// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <SDL3/SDL.h> // SDL_...
#include <algorithm>  // std::min
#include <array>      // std::array
#include <cstdio>     // std::snprintf
#include <memory>     // std::unique_ptr, std::make_unique
#include <soloud.h>   // SoLoud::... (includes windows.h on _WIN32, hence the #defines above)

namespace SoLoud {

namespace {

static_assert(sizeof(float) == 4);

struct AudioOutputDevice {
	struct InitializationError {};

	static void mixAudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
		(void)total_amount;

		constexpr int SAMPLE_SIZE = static_cast<int>(sizeof(float));

		Soloud& soloud = *static_cast<Soloud*>(userdata);
		AudioOutputDevice& device = *static_cast<AudioOutputDevice*>(soloud.mBackendData);

		int samplesRemaining = additional_amount / SAMPLE_SIZE;
		while (samplesRemaining > 0) {
			std::array<float, 4096> sampleScratchBuffer{};
			const int sampleCount = std::min(samplesRemaining, static_cast<int>(sampleScratchBuffer.size()));
			soloud.mix(sampleScratchBuffer.data(), static_cast<unsigned>(sampleCount / device.channelCount));
			SDL_PutAudioStreamData(stream, sampleScratchBuffer.data(), sampleCount * SAMPLE_SIZE);
			samplesRemaining -= sampleCount;
		}
	}

	static void cleanupCallback(Soloud* aSoloud) {
		delete static_cast<AudioOutputDevice*>(aSoloud->mBackendData); // NOLINT(cppcoreguidelines-owning-memory)
		aSoloud->mBackendData = nullptr;
	}

	int channelCount = 0;
	SDL_AudioStream* stream = nullptr;

	AudioOutputDevice() {
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			throw InitializationError{};
		}
	}

	AudioOutputDevice(Soloud* aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels)
		: AudioOutputDevice() {
		char sampleCountString[16];
		const int sampleCountLength = std::snprintf(sampleCountString, (sizeof(sampleCountString) / sizeof(char)) - 1, "%u", aBuffer);
		if (sampleCountLength > 0) {
			sampleCountString[sampleCountLength] = '\0';
			SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, sampleCountString);
		}

		SDL_AudioSpec recommendedDeviceSpec{};
		if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &recommendedDeviceSpec, nullptr)) {
			aSamplerate = static_cast<unsigned>(recommendedDeviceSpec.freq);
		}

		const SDL_AudioSpec spec{
			.format = SDL_AUDIO_F32,
			.channels = static_cast<int>(aChannels),
			.freq = static_cast<int>(aSamplerate),
		};
		channelCount = spec.channels;
		stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mixAudioCallback, aSoloud);
		if (!stream) {
			throw InitializationError{};
		}

		SDL_AudioSpec deviceSpec{};
		int deviceSampleCount{};
		if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &deviceSpec, &deviceSampleCount)) {
			throw InitializationError{};
		}

		aSoloud->postinit_internal(aSamplerate, static_cast<unsigned>(deviceSampleCount), aFlags, aChannels);

		SDL_ResumeAudioStreamDevice(stream);
	}

	~AudioOutputDevice() {
		SDL_DestroyAudioStream(stream);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	AudioOutputDevice(const AudioOutputDevice&) = delete;
	AudioOutputDevice(AudioOutputDevice&&) = delete;
	AudioOutputDevice& operator=(const AudioOutputDevice&) = delete;
	AudioOutputDevice& operator=(AudioOutputDevice&&) = delete;
};

} // namespace

// Hijack the sdl2static_init symbol for our custom SDL3 backend to avoid having to change Soloud's source code.
result sdl2static_init( // NOLINT(misc-use-internal-linkage)
	Soloud* aSoloud, unsigned int aFlags, unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels) {
	try {
		std::unique_ptr<AudioOutputDevice> device = std::make_unique<AudioOutputDevice>(aSoloud, aFlags, aSamplerate, aBuffer, aChannels);
		aSoloud->mBackendCleanupFunc = AudioOutputDevice::cleanupCallback;
		aSoloud->mBackendData = device.release();
		aSoloud->mBackendString = "SDL3";
	} catch (...) {
		return UNKNOWN_ERROR;
	}
	return 0;
}

} // namespace SoLoud
