// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/audio/SoundInstanceID.hpp>
#include <GREM/audio/SoundStage.hpp>
#include <GREM/audio/filters.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/math.hpp>

#include <soloud.h>                      // SoLoud::...
#include <soloud_biquadresonantfilter.h> // SoLoud::BiquadResonantFilter
#include <soloud_dcremovalfilter.h>      // SoLoud::DCRemovalFilter
#include <soloud_duckfilter.h>           // SoLoud::DuckFilter
#include <soloud_echofilter.h>           // SoLoud::EchoFilter
#include <soloud_eqfilter.h>             // SoLoud::EqFilter
#include <soloud_filter.h>               // SoLoud::Filter, SoLoud::FilterInstance
#include <soloud_flangerfilter.h>        // SoLoud::FlangerFilter
#include <soloud_freeverbfilter.h>       // SoLoud::FreeverbFilter
#include <soloud_lofifilter.h>           // SoLoud::LofiFilter

namespace grem::audio {

namespace {

class CustomFilterInstance final : public SoLoud::FilterInstance {
public:
	CustomFilterInstance(Span<const byte> data, size_t parametersOffset, size_t parameterCount, const detail::FilterParameterDescription* parameterDescriptions,
		detail::FilterFunction doFilter)
		: data(data.begin(), data.end())
		, parametersOffset(parametersOffset)
		, parameterDescriptions(parameterDescriptions)
		, doFilter(doFilter) {
		initParams(static_cast<int>(parameterCount));
		for (size_t i = 0; i < parameterCount; ++i) {
			const detail::FilterParameterDescription& parameterDescription = parameterDescriptions[i];
			float parameterValue = 0.0f;
			switch (parameterDescription.type) {
				case detail::FilterParameterType::BOOL: {
					bool value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::INT8: {
					int8_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::INT16: {
					int16_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::INT32: {
					int32_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::UINT8: {
					uint8_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::UINT16: {
					uint16_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::UINT32: {
					uint32_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
				case detail::FilterParameterType::FLOAT32: {
					float32_t value{};
					memcpy(&value, data.data() + parametersOffset + parameterDescription.byteOffset, sizeof(value));
					parameterValue = static_cast<float>(value);
					break;
				}
			}
			setFilterParameter(static_cast<unsigned>(i), parameterValue);
		}
	}

	void filter(float* aBuffer, unsigned int aSamples, [[maybe_unused]] unsigned int aBufferSize, unsigned int aChannels, float aSamplerate, SoLoud::time aTime) override {
		for (size_t i = 0; i < mNumParams; ++i) {
			const detail::FilterParameterDescription& parameterDescription = parameterDescriptions[i];
			switch (parameterDescription.type) {
				case detail::FilterParameterType::BOOL: {
					const bool value = static_cast<bool>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::INT8: {
					const int8_t value = static_cast<int8_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::INT16: {
					const int16_t value = static_cast<int16_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::INT32: {
					const int32_t value = static_cast<int32_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::UINT8: {
					const uint8_t value = static_cast<uint8_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::UINT16: {
					const uint16_t value = static_cast<uint16_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::UINT32: {
					const uint32_t value = static_cast<uint32_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
				case detail::FilterParameterType::FLOAT32: {
					const float32_t value = static_cast<float32_t>(mParam[i]);
					memcpy(data.data() + parametersOffset + parameterDescription.byteOffset, &value, sizeof(value));
					break;
				}
			}
		}

		doFilter(data.data(), Span{aBuffer, static_cast<size_t>(aSamples) * static_cast<size_t>(aChannels)}, static_cast<size_t>(aChannels), static_cast<size_t>(aSamples),
			aSamplerate, aTime);
	}

private:
	SmallBuffer<byte, 64> data;
	size_t parametersOffset;
	const detail::FilterParameterDescription* parameterDescriptions;
	detail::FilterFunction doFilter;
};

class CustomFilter final : public SoLoud::Filter {
public:
	CustomFilter(Span<const byte> data, size_t parametersOffset, size_t parameterCount, const detail::FilterParameterDescription* parameterDescriptions,
		const detail::FilterParameterInfo* parameterInfos, detail::FilterFunction doFilter)
		: data(data.begin(), data.end())
		, parametersOffset(parametersOffset)
		, parameterCount(parameterCount)
		, parameterDescriptions(parameterDescriptions)
		, parameterInfos(parameterInfos)
		, doFilter(doFilter) {}

	int getParamCount() override {
		return static_cast<int>(parameterCount);
	}

	const char* getParamName(unsigned int aParamIndex) override {
		return parameterInfos[static_cast<size_t>(aParamIndex)].name.c_str();
	}

	unsigned int getParamType(unsigned int aParamIndex) override {
		switch (parameterDescriptions[static_cast<size_t>(aParamIndex)].type) {
			case detail::FilterParameterType::BOOL: return BOOL_PARAM;
			case detail::FilterParameterType::INT8: [[fallthrough]];
			case detail::FilterParameterType::INT16: [[fallthrough]];
			case detail::FilterParameterType::INT32: [[fallthrough]];
			case detail::FilterParameterType::UINT8: [[fallthrough]];
			case detail::FilterParameterType::UINT16: [[fallthrough]];
			case detail::FilterParameterType::UINT32: return INT_PARAM;
			case detail::FilterParameterType::FLOAT32: return FLOAT_PARAM;
		}
		unreachable();
	}

	float getParamMax(unsigned int aParamIndex) override {
		return parameterInfos[static_cast<size_t>(aParamIndex)].maxValue;
	}

	float getParamMin(unsigned int aParamIndex) override {
		return parameterInfos[static_cast<size_t>(aParamIndex)].minValue;
	}

	SoLoud::FilterInstance* createInstance() override {
		return new CustomFilterInstance{data, parametersOffset, parameterCount, parameterDescriptions, doFilter}; // NOLINT(cppcoreguidelines-owning-memory)
	}

private:
	SmallBuffer<byte, 64> data{};
	size_t parametersOffset;
	size_t parameterCount;
	const detail::FilterParameterDescription* parameterDescriptions;
	const detail::FilterParameterInfo* parameterInfos;
	detail::FilterFunction doFilter;
};

} // namespace

namespace detail {

void FilterDeleter::operator()(void* handle) const noexcept {
	delete static_cast<SoLoud::Filter*>(handle); // NOLINT(cppcoreguidelines-owning-memory)
}

void* createCustomFilter(Span<const byte> initialData, size_t parametersOffset, Span<const detail::FilterParameterDescription> parameterDescriptions,
	Span<const detail::FilterParameterInfo> parameterInfos, detail::FilterFunction doFilter) {
	GREM_ASSERT(parameterDescriptions.size() == parameterInfos.size());
	return static_cast<SoLoud::Filter*>(
		new CustomFilter{initialData, parametersOffset, parameterDescriptions.size(), parameterDescriptions.data(), parameterInfos.data(), doFilter});
}

} // namespace detail

void* BiquadResonantFilter::GREM_private_createBuiltinFilter(const void* data) {
	const BiquadResonantFilter& filter = *static_cast<const BiquadResonantFilter*>(data);
	UniquePointer<SoLoud::BiquadResonantFilter> result = UniquePointer<SoLoud::BiquadResonantFilter>::create();
	const float cutoffFrequency =
		clamp(filter.parameters.cutoffFrequency, result->getParamMin(SoLoud::BiquadResonantFilter::FREQUENCY), result->getParamMax(SoLoud::BiquadResonantFilter::FREQUENCY));
	const float resonance =
		clamp(filter.parameters.resonance, result->getParamMin(SoLoud::BiquadResonantFilter::RESONANCE), result->getParamMax(SoLoud::BiquadResonantFilter::RESONANCE));
	result->setParams(static_cast<int>(filter.type), cutoffFrequency, resonance);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* EchoFilter::GREM_private_createBuiltinFilter(const void* data) {
	const EchoFilter& filter = *static_cast<const EchoFilter*>(data);
	UniquePointer<SoLoud::EchoFilter> result = UniquePointer<SoLoud::EchoFilter>::create();
	const float delay = clamp(filter.options.delay, result->getParamMin(SoLoud::EchoFilter::DELAY), result->getParamMax(SoLoud::EchoFilter::DELAY));
	const float decay = clamp(filter.options.decay, result->getParamMin(SoLoud::EchoFilter::DECAY), result->getParamMax(SoLoud::EchoFilter::DECAY));
	const float filter_ = clamp(filter.options.filter, result->getParamMin(SoLoud::EchoFilter::FILTER), result->getParamMax(SoLoud::EchoFilter::FILTER));
	result->setParams(delay, decay, filter_);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* LoFiFilter::GREM_private_createBuiltinFilter(const void* data) {
	const LoFiFilter& filter = *static_cast<const LoFiFilter*>(data);
	UniquePointer<SoLoud::LofiFilter> result = UniquePointer<SoLoud::LofiFilter>::create();
	const float sampleRate = clamp(filter.parameters.sampleRate, result->getParamMin(SoLoud::LofiFilter::SAMPLERATE), result->getParamMax(SoLoud::LofiFilter::SAMPLERATE));
	const float bitDepth = clamp(filter.parameters.bitDepth, result->getParamMin(SoLoud::LofiFilter::BITDEPTH), result->getParamMax(SoLoud::LofiFilter::BITDEPTH));
	result->setParams(sampleRate, bitDepth);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* FlangerFilter::GREM_private_createBuiltinFilter(const void* data) {
	const FlangerFilter& filter = *static_cast<const FlangerFilter*>(data);
	UniquePointer<SoLoud::FlangerFilter> result = UniquePointer<SoLoud::FlangerFilter>::create();
	const float delay = clamp(filter.parameters.delay, result->getParamMin(SoLoud::FlangerFilter::DELAY), result->getParamMax(SoLoud::FlangerFilter::DELAY));
	const float frequency = clamp(filter.parameters.frequency, result->getParamMin(SoLoud::FlangerFilter::FREQ), result->getParamMax(SoLoud::FlangerFilter::FREQ));
	result->setParams(delay, frequency);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* DCRemovalFilter::GREM_private_createBuiltinFilter(const void* data) {
	const DCRemovalFilter& filter = *static_cast<const DCRemovalFilter*>(data);
	UniquePointer<SoLoud::DCRemovalFilter> result = UniquePointer<SoLoud::DCRemovalFilter>::create();
	result->setParams(filter.options.length);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* FreeverbFilter::GREM_private_createBuiltinFilter(const void* data) {
	const FreeverbFilter& filter = *static_cast<const FreeverbFilter*>(data);
	UniquePointer<SoLoud::FreeverbFilter> result = UniquePointer<SoLoud::FreeverbFilter>::create();
	const float roomSize = clamp(filter.parameters.roomSize, result->getParamMin(SoLoud::FreeverbFilter::ROOMSIZE), result->getParamMax(SoLoud::FreeverbFilter::ROOMSIZE));
	const float damping = clamp(filter.parameters.damping, result->getParamMin(SoLoud::FreeverbFilter::DAMP), result->getParamMax(SoLoud::FreeverbFilter::DAMP));
	const float width = clamp(filter.parameters.width, result->getParamMin(SoLoud::FreeverbFilter::WIDTH), result->getParamMax(SoLoud::FreeverbFilter::WIDTH));
	result->setParams(0.0f, roomSize, damping, width);
	return static_cast<SoLoud::Filter*>(result.release());
}

void* EqualizationFilter::GREM_private_createBuiltinFilter(const void* data) {
	const EqualizationFilter& filter = *static_cast<const EqualizationFilter*>(data);
	UniquePointer<SoLoud::EqFilter> result = UniquePointer<SoLoud::EqFilter>::create();
	result->setParam(SoLoud::EqFilter::BAND1, clamp(filter.parameters.band1Volume, result->getParamMin(SoLoud::EqFilter::BAND1), result->getParamMax(SoLoud::EqFilter::BAND1)));
	result->setParam(SoLoud::EqFilter::BAND2, clamp(filter.parameters.band2Volume, result->getParamMin(SoLoud::EqFilter::BAND2), result->getParamMax(SoLoud::EqFilter::BAND2)));
	result->setParam(SoLoud::EqFilter::BAND3, clamp(filter.parameters.band3Volume, result->getParamMin(SoLoud::EqFilter::BAND3), result->getParamMax(SoLoud::EqFilter::BAND3)));
	result->setParam(SoLoud::EqFilter::BAND4, clamp(filter.parameters.band4Volume, result->getParamMin(SoLoud::EqFilter::BAND4), result->getParamMax(SoLoud::EqFilter::BAND4)));
	result->setParam(SoLoud::EqFilter::BAND5, clamp(filter.parameters.band5Volume, result->getParamMin(SoLoud::EqFilter::BAND5), result->getParamMax(SoLoud::EqFilter::BAND5)));
	result->setParam(SoLoud::EqFilter::BAND6, clamp(filter.parameters.band6Volume, result->getParamMin(SoLoud::EqFilter::BAND6), result->getParamMax(SoLoud::EqFilter::BAND6)));
	result->setParam(SoLoud::EqFilter::BAND7, clamp(filter.parameters.band7Volume, result->getParamMin(SoLoud::EqFilter::BAND7), result->getParamMax(SoLoud::EqFilter::BAND7)));
	result->setParam(SoLoud::EqFilter::BAND8, clamp(filter.parameters.band8Volume, result->getParamMin(SoLoud::EqFilter::BAND8), result->getParamMax(SoLoud::EqFilter::BAND8)));
	return static_cast<SoLoud::Filter*>(result.release());
}

void* DuckingFilter::GREM_private_createBuiltinFilter(const void* data) {
	const DuckingFilter& filter = *static_cast<const DuckingFilter*>(data);
	UniquePointer<SoLoud::DuckFilter> result = UniquePointer<SoLoud::DuckFilter>::create();
	const float onRamp = clamp(filter.options.onRamp, result->getParamMin(SoLoud::DuckFilter::ONRAMP), result->getParamMax(SoLoud::DuckFilter::ONRAMP));
	const float offRamp = clamp(filter.options.offRamp, result->getParamMin(SoLoud::DuckFilter::OFFRAMP), result->getParamMax(SoLoud::DuckFilter::OFFRAMP));
	const float level = clamp(filter.options.level, result->getParamMin(SoLoud::DuckFilter::LEVEL), result->getParamMax(SoLoud::DuckFilter::LEVEL));
	result->setParams(static_cast<SoLoud::Soloud*>(filter.soundStage->get()), filter.listenTo.value, onRamp, offRamp, level);
	return static_cast<SoLoud::Filter*>(result.release());
}

} // namespace grem::audio
