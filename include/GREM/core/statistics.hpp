// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_STATISTICS_HPP
#define GREM_CORE_STATISTICS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/InplaceDoubleEndedQueue.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/math.hpp>

#include <cstddef>     // std::size_t
#include <type_traits> // std::remove_cvref_t

namespace grem {

/**
 * Exponential moving average of a discrete-time input signal.
 *
 * \tparam T signal value type. Must be a type that behaves like a number.
 */
template <typename T>
class ExponentialMovingAverage {
public:
	/**
	 * Default-construct an exponential moving average.
	 */
	constexpr ExponentialMovingAverage() = default;

	/**
	 * Construct an exponential moving average with a specific initial value.
	 *
	 * \param value initial value to set the exponential moving average to.
	 */
	constexpr explicit ExponentialMovingAverage(T value)
		: value(value) {}

	/**
	 * Reset the exponential moving average to a specific value.
	 *
	 * \param newValue new value to set the exponential moving average to.
	 */
	constexpr void reset(T newValue = {}) {
		value = newValue;
	}

	/**
	 * Update the exponential moving average given the next discrete input
	 * signal sample value.
	 *
	 * \param newSignalValue new sample value of the input signal.
	 * \param alpha amount of the new value that is contributed to the average.
	 *        Must be between 0 and 1 (inclusive).
	 *
	 * \return the new exponential moving average of the input signal.
	 */
	template <typename Coefficient>
	constexpr T update(T newSignalValue, Coefficient alpha) {
		return value = newSignalValue * alpha + value * (Coefficient{1} - alpha);
	}

	/**
	 * Get the current exponential moving average value.
	 *
	 * \return the current exponential moving average of the input signal.
	 */
	[[nodiscard]] constexpr T get() const {
		return value;
	}

private:
	T value{};
};

/**
 * Sliding window of a discrete-time input signal.
 *
 * \tparam T signal value type. Must be a type that behaves like a number.
 * \tparam N number of samples in the sliding window. Must be positive.
 */
template <typename T, std::size_t N>
class SlidingWindow {
public:
	/**
	 * Statistics of the sliding window.
	 */
	struct Statistics {
		T mean;              ///< Mean (average) of all samples in the window.
		T standardDeviation; ///< Standard deviation of all samples in the window.
	};

	/**
	 * Clear the sliding window of all samples.
	 */
	void clear() noexcept {
		sampleBuffer.clear();
		sampleSum = T{};
		sampleSumOfSquares = SquaredT{};
	}

	/**
	 * Push a new sample into the sliding window, popping the oldest sample
	 * first if the window is at its maximum capacity.
	 *
	 * \param newSample new sample to add.
	 */
	void update(T newSample) noexcept {
		if (sampleBuffer.size() >= N) {
			const T expiredSample = sampleBuffer.front();
			sampleSum -= expiredSample;
			sampleSumOfSquares -= expiredSample * expiredSample;
			sampleBuffer.pop_front();
		}
		sampleBuffer.push_back(newSample);
		sampleSum += newSample;
		sampleSumOfSquares += newSample * newSample;
	}

	/**
	 * Calculate the statistics of the current samples in the sliding window.
	 *
	 * \return the current statistics of the sliding window.
	 */
	[[nodiscard]] Statistics getStatistics() const noexcept {
		const std::size_t sampleCount = sampleBuffer.size();
		const T mean = (sampleCount <= 1) ? sampleSum : sampleSum / sampleCount;
		const SquaredT variance = (sampleCount <= 1) ? SquaredT{} : (sampleSumOfSquares - (sampleSum * sampleSum) / sampleCount) / (sampleCount - 1);
		const T standardDeviation = sqrt(variance);
		return {.mean = mean, .standardDeviation = standardDeviation};
	}

	/**
	 * Get the samples in the sliding window.
	 *
	 * \return a read-only view over the current samples in the sliding window,
	 *         ordered from oldest to newest, valid until the next time the
	 *         window is modified or destroyed.
	 */
	[[nodiscard]] auto getSamples() const noexcept {
		return Subrange{sampleBuffer};
	}

private:
	using SquaredT = std::remove_cvref_t<decltype(T{} * T{})>;

	InplaceDoubleEndedQueue<T, N> sampleBuffer{};
	T sampleSum{};
	SquaredT sampleSumOfSquares{};
};

} // namespace grem

#endif
