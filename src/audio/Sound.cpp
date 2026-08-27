// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/audio/Error.hpp>
#include <GREM/audio/Sound.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>

#include <new>                // std::launder
#include <soloud.h>           // SoLoud::...
#include <soloud_file.h>      // SoLoud::File
#include <soloud_wav.h>       // SoLoud::Wav
#include <soloud_wavstream.h> // SoLoud::WavStream
#include <stdexcept>          // std::length_error

namespace grem::audio {

static_assert(Sound::MAX_FILTER_COUNT <= FILTERS_PER_STREAM);

namespace {

class SoundFile final : public SoLoud::File {
public:
	explicit SoundFile(const Filesystem& filesystem, CStringView filepath)
		: file(filesystem.openInputFile(filepath)) {
		if (file.size() > size_t{Limits<unsigned>::MAX}) {
			throw std::length_error{"Sound file size overflow."};
		}
	}

	~SoundFile() override = default;

	SoundFile(const SoundFile&) = delete;
	SoundFile(SoundFile&&) = delete;
	SoundFile& operator=(const SoundFile&) = delete;
	SoundFile& operator=(SoundFile&&) = delete;

	int eof() override {
		return (file.eof()) ? 1 : 0;
	}

	unsigned read(unsigned char* aDst, unsigned aBytes) override {
		return static_cast<unsigned>(file.readUntilEOF(asWritableBytes(Span<unsigned char>{aDst, static_cast<size_t>(aBytes)})));
	}

	unsigned length() override {
		return static_cast<unsigned>(file.size());
	}

	void seek(int aOffset) override {
		file.seekg(static_cast<size_t>(aOffset));
	}

	unsigned pos() override {
		return static_cast<unsigned>(file.tellg());
	}

private:
	grem::InputFileHandle file;
};

[[nodiscard]] constexpr unsigned getSoLoudAttenuationModel(SoundAttenuationModel attenuationModel) noexcept {
	switch (attenuationModel) {
		case SoundAttenuationModel::NO_ATTENUATION: return SoLoud::AudioSource::NO_ATTENUATION;
		case SoundAttenuationModel::INVERSE_DISTANCE: return SoLoud::AudioSource::INVERSE_DISTANCE;
		case SoundAttenuationModel::LINEAR_DISTANCE: return SoLoud::AudioSource::LINEAR_DISTANCE;
		case SoundAttenuationModel::EXPONENTIAL_DISTANCE: return SoLoud::AudioSource::EXPONENTIAL_DISTANCE;
	}
	return 0u;
}

[[nodiscard]] SoundOptions getOptionsUnstreamed(SoundOptions options) {
	options.streamed = false;
	return options;
}

} // namespace

Sound::Sound(const Filesystem& filesystem, CStringView filepath, const SoundOptions& options)
	: Sound((options.streamed) ? static_cast<SoLoud::AudioSource*>(new SoLoud::WavStream{}) : static_cast<SoLoud::AudioSource*>(new SoLoud::Wav{}), options) {
	if (options.streamed) {
		GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load streamed sound {}", filepath));

		SoLoud::WavStream& wavStream =
			*static_cast<SoLoud::WavStream*>(static_cast<SoLoud::AudioSource*>(sourceHandle.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		soundFileHandle = SoundFileHandle{new SoundFile(filesystem, filepath)};
		if (const SoLoud::result errorCode = wavStream.loadFile(static_cast<SoundFile*>(soundFileHandle.get())); errorCode != SoLoud::SO_NO_ERROR) {
			throw audio::Error{String{"Failed to load sound file \""} + filepath.c_str() + "\"", errorCode};
		}
	} else {
		GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load sound {}", filepath));

		SoLoud::Wav& wav = *static_cast<SoLoud::Wav*>(static_cast<SoLoud::AudioSource*>(sourceHandle.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		SoundFile soundFile{filesystem, filepath};
		if (const SoLoud::result errorCode = wav.loadFile(&soundFile); errorCode != SoLoud::SO_NO_ERROR) {
			throw audio::Error{String{"Failed to load sound file \""} + filepath.c_str() + "\"", errorCode};
		}
	}
}

Sound::Sound(Span<const byte> fileContents, const SoundOptions& options)
	: Sound((options.streamed) ? static_cast<SoLoud::AudioSource*>(new SoLoud::WavStream{}) : static_cast<SoLoud::AudioSource*>(new SoLoud::Wav{}), options) {
	if (fileContents.size() > size_t{Limits<unsigned>::MAX}) {
		throw std::length_error{"Sound file size overflow."};
	}
	if (options.streamed) {
		GREM_PROFILE_BLOCK("Load streamed sound");

		SoLoud::WavStream& wavStream =
			*static_cast<SoLoud::WavStream*>(static_cast<SoLoud::AudioSource*>(sourceHandle.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		if (const SoLoud::result errorCode =
				wavStream.loadMem(std::launder(reinterpret_cast<const unsigned char*>(fileContents.data())), static_cast<unsigned>(fileContents.size()), true, true);
			errorCode != SoLoud::SO_NO_ERROR) {
			throw audio::Error{"Failed to load sound file", errorCode};
		}
	} else {
		GREM_PROFILE_BLOCK("Load sound");

		SoLoud::Wav& wav = *static_cast<SoLoud::Wav*>(static_cast<SoLoud::AudioSource*>(sourceHandle.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		if (const SoLoud::result errorCode =
				wav.loadMem(std::launder(reinterpret_cast<const unsigned char*>(fileContents.data())), static_cast<unsigned>(fileContents.size()), true, true);
			errorCode != SoLoud::SO_NO_ERROR) {
			throw audio::Error{"Failed to load sound file", errorCode};
		}
	}
}

Sound::Sound(Span<const uint8_t> samples, float sampleRate, size_t channelCount, const SoundOptions& options)
	: Sound(static_cast<SoLoud::AudioSource*>(new SoLoud::Wav{}), getOptionsUnstreamed(options)) {
	GREM_PROFILE_BLOCK("Load sound from samples");

	GREM_ASSERT(samples.size() % channelCount == 0);
	SoLoud::Wav& wav = *static_cast<SoLoud::Wav*>(static_cast<SoLoud::AudioSource*>(sourceHandle.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
	if (samples.size() > size_t{Limits<unsigned>::MAX} || channelCount > size_t{Limits<unsigned>::MAX}) {
		throw std::length_error{"Sound wave size overflow."};
	}
	if (const SoLoud::result errorCode =
			wav.loadRawWave8(const_cast<uint8_t*>(samples.data()), static_cast<unsigned>(samples.size()), sampleRate, static_cast<unsigned>(channelCount));
		errorCode != SoLoud::SO_NO_ERROR) {
		throw audio::Error{"Failed to load sound file", errorCode};
	}
}

Sound::Sound(Span<const int16_t> samples, float sampleRate, size_t channelCount, const SoundOptions& options)
	: Sound(static_cast<SoLoud::AudioSource*>(new SoLoud::Wav{}), getOptionsUnstreamed(options)) {
	GREM_PROFILE_BLOCK("Load sound from samples");

	GREM_ASSERT(samples.size() % channelCount == 0);
	SoLoud::Wav& wav = *static_cast<SoLoud::Wav*>(sourceHandle.get());
	if (samples.size() > size_t{Limits<unsigned>::MAX} || channelCount > size_t{Limits<unsigned>::MAX}) {
		throw std::length_error{"Sound wave size overflow."};
	}
	if (const SoLoud::result errorCode =
			wav.loadRawWave16(const_cast<int16_t*>(samples.data()), static_cast<unsigned>(samples.size()), sampleRate, static_cast<unsigned>(channelCount));
		errorCode != SoLoud::SO_NO_ERROR) {
		throw audio::Error{"Failed to load sound file", errorCode};
	}
}

Sound::Sound(Span<const float32_t> samples, float sampleRate, size_t channelCount, const SoundOptions& options)
	: Sound(static_cast<SoLoud::AudioSource*>(new SoLoud::Wav{}), getOptionsUnstreamed(options)) {
	GREM_PROFILE_BLOCK("Load sound from samples");

	GREM_ASSERT(samples.size() % channelCount == 0);
	SoLoud::Wav& wav = *static_cast<SoLoud::Wav*>(sourceHandle.get());
	if (samples.size() > size_t{Limits<unsigned>::MAX} || channelCount > size_t{Limits<unsigned>::MAX}) {
		throw std::length_error{"Sound wave size overflow."};
	}
	if (const SoLoud::result errorCode =
			wav.loadRawWave(const_cast<float32_t*>(samples.data()), static_cast<unsigned>(samples.size()), sampleRate, static_cast<unsigned>(channelCount));
		errorCode != SoLoud::SO_NO_ERROR) {
		throw audio::Error{"Failed to load sound file", errorCode};
	}
}

Sound::Sound(void* handle, const SoundOptions& options)
	: sourceHandle(handle)
	, streamed(options.streamed) {
	SoLoud::AudioSource& source = *static_cast<SoLoud::AudioSource*>(sourceHandle.get());
	source.setVolume(options.volume);
	// Note: Setting aMustTick to true for all sounds would probably be preferable,
	// but it causes SoLoud to segfault if the number of inaudible sounds exceeds the maximum simultaneous sound count,
	// so we unfortunately have to keep the default behavior of pausing inaudible sounds.
	source.setInaudibleBehavior(false, false);
	source.set3dMinMaxDistance(options.minDistance, options.maxDistance);
	source.set3dAttenuation(getSoLoudAttenuationModel(options.attenuationModel), options.rolloffFactor);
	source.set3dDopplerFactor(options.dopplerFactor);
	source.set3dDistanceDelay(options.useDistanceDelay);
	source.set3dListenerRelative(options.listenerRelative);
	source.setLooping(options.looping);
	source.setSingleInstance(options.singleInstance);
}

void Sound::removeFilter(size_t filterSlotIndex) noexcept {
	GREM_ASSERT(filterSlotIndex < MAX_FILTER_COUNT);
	if (filters) {
		SoLoud::AudioSource& source = *static_cast<SoLoud::AudioSource*>(sourceHandle.get());
		source.setFilter(static_cast<unsigned>(filterSlotIndex), nullptr);
		filters[filterSlotIndex].reset();
	}
}

Duration Sound::getDuration() const noexcept {
	SoLoud::AudioSource* const source = static_cast<SoLoud::AudioSource*>(sourceHandle.get());
	return duration_cast<Duration>(DurationBase<SoLoud::time>{(streamed) ? static_cast<SoLoud::WavStream*>(source)->getLength() : static_cast<SoLoud::Wav*>(source)->getLength()});
}

void Sound::SoundFileDeleter::operator()(void* handle) const noexcept {
	delete static_cast<SoundFile*>(handle); // NOLINT(cppcoreguidelines-owning-memory)
}

void Sound::SourceDeleter::operator()(void* handle) const noexcept {
	delete static_cast<SoLoud::AudioSource*>(handle); // NOLINT(cppcoreguidelines-owning-memory)
}

void Sound::setFilterImplementation(size_t filterSlotIndex, detail::Filter filter) {
	GREM_ASSERT(filterSlotIndex < MAX_FILTER_COUNT);
	if (!filters) {
		filters = UniquePointer<detail::Filter[]>::create(MAX_FILTER_COUNT);
	}
	SoLoud::AudioSource& source = *static_cast<SoLoud::AudioSource*>(sourceHandle.get());
	source.setFilter(static_cast<unsigned>(filterSlotIndex), static_cast<SoLoud::Filter*>(filter.get()));
	filters[filterSlotIndex] = std::move(filter);
}

} // namespace grem::audio
