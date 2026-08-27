// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_TIME_SAMPLE_BUFFER_HPP
#define GREM_EXAMPLES_FPS_TIME_SAMPLE_BUFFER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/InplaceDoubleEndedQueue.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include <ratio> // std::ratio_multiply

struct TimeSampleBufferStatistics {
	Duration mean;
	Duration standardDeviation;
};

template <size_t SampleCount>
class TimeSampleBuffer {
public:
	void update(Duration newSample) {
		if (sampleBuffer.size() >= SampleCount) {
			const Duration expiredSample = sampleBuffer.front();
			const Microseconds expiredSampleMicroseconds = round<Microseconds>(expiredSample);
			sampleSum -= expiredSample;
			sampleSumMicroseconds -= expiredSampleMicroseconds;
			sampleSumOfMicrosecondsSquares -= SquaredMicroseconds{expiredSampleMicroseconds.count() * expiredSampleMicroseconds.count()};
			sampleBuffer.pop_front();
		}
		const Microseconds newSampleMicroseconds = round<Microseconds>(newSample);
		sampleBuffer.push_back(newSample);
		sampleSum += newSample;
		sampleSumMicroseconds += newSampleMicroseconds;
		sampleSumOfMicrosecondsSquares += SquaredMicroseconds{newSampleMicroseconds.count() * newSampleMicroseconds.count()};
	}

	[[nodiscard]] TimeSampleBufferStatistics getStatistics() const noexcept {
		const Duration::rep sampleCount = static_cast<Duration::rep>(sampleBuffer.size());
		const Duration mean = (sampleCount <= 1) ? sampleSum : sampleSum / sampleCount;
		const SquaredMicroseconds variance =
			(sampleCount <= 1)
				? SquaredMicroseconds{}
				: SquaredMicroseconds{
					  (sampleSumOfMicrosecondsSquares - SquaredMicroseconds{sampleSumMicroseconds.count() * sampleSumMicroseconds.count()} / sampleCount) / (sampleCount - 1)};
		const Duration standardDeviation = duration_cast<Duration>(DurationBase<float, Microseconds::period>{sqrt(static_cast<float>(variance.count()))});
		return {.mean = mean, .standardDeviation = standardDeviation};
	}

	[[nodiscard]] const auto& getSamples() const noexcept {
		return sampleBuffer;
	}

private:
	using SquaredMicroseconds = DurationBase<Microseconds::rep, std::ratio_multiply<Microseconds::period, Microseconds::period>>;

	InplaceDoubleEndedQueue<Duration, SampleCount> sampleBuffer{};
	Duration sampleSum{};
	Microseconds sampleSumMicroseconds{};
	SquaredMicroseconds sampleSumOfMicrosecondsSquares{};
};

#endif
