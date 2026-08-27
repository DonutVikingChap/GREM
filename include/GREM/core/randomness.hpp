// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_RANDOMNESS_HPP
#define GREM_CORE_RANDOMNESS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/fundamentals.hpp>

#include <istream> // std::basic_istream
#include <limits>  // std::numeric_limits
#include <ostream> // std::basic_ostream
#include <random>  // std::random_device, std::..._distribution, std::generate_canonical

namespace grem::randomness {

/**
 * Implementation of the SplitMix64 pseudorandom number generator that provides
 * the API required for a standard uniform random bit generator, so that it can
 * be plugged into any of the random number distributions provided by the
 * standard library.
 *
 * This engine is mainly used for seeding the Xoroshiro128PlusPlusEngine, which
 * should be preferred for general use.
 *
 * \warning This engine does not produce cryptographcially secure randomness and
 *          should therefore not be used in situations that require a CSPRNG.
 *
 * \sa Xoroshiro128PlusPlusEngine
 */
class SplitMix64Engine {
public:
	using result_type = uint64_t;

	static constexpr result_type default_seed = 0;

	[[nodiscard]] static constexpr result_type min() {
		return 0;
	}

	[[nodiscard]] static constexpr result_type max() {
		return Limits<result_type>::MAX;
	}

	constexpr SplitMix64Engine() noexcept
		: SplitMix64Engine(default_seed) {}

	constexpr explicit SplitMix64Engine(result_type seedValue) noexcept
		: state(seedValue) {}

	constexpr void seed(result_type seedValue = default_seed) noexcept {
		state = seedValue;
	}

	constexpr result_type operator()() noexcept {
		state += 0x9E3779B97F4A7C15ull;
		uint64_t z = state;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	constexpr void discard(unsigned long long z) noexcept {
		while (z-- > 0) {
			(*this)();
		}
	}

	[[nodiscard]] constexpr bool operator==(const SplitMix64Engine& other) const noexcept {
		return state == other.state;
	}

	template <typename CharT, typename Traits>
	friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& stream, const SplitMix64Engine& engine) {
		return stream << engine.state;
	}

	template <typename CharT, typename Traits>
	friend std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& stream, SplitMix64Engine& engine) {
		return stream >> engine.state;
	}

private:
	uint64_t state;
};

/**
 * Implementation of the xoroshiro128++ pseudorandom number generator that
 * provides the API required for a standard uniform random bit generator, so
 * that it can be plugged into any of the random number distributions provided
 * by the standard library.
 *
 * This engine is small, fast and fairly high quality compared to most of the
 * pseudorandom number generators in the standard library (as of C++20).
 *
 * \warning This engine does not produce cryptographcially secure randomness and
 *          should therefore not be used in situations that require a CSPRNG.
 *
 * \sa https://prng.di.unimi.it/ for more information.
 */
class Xoroshiro128PlusPlusEngine {
public:
	using result_type = uint64_t;
	using State = Array<uint64_t, 2>;

	static constexpr result_type default_seed = SplitMix64Engine::default_seed;

	[[nodiscard]] static constexpr result_type min() {
		return 0;
	}

	[[nodiscard]] static constexpr result_type max() {
		return Limits<result_type>::MAX;
	}

	constexpr Xoroshiro128PlusPlusEngine() noexcept
		: Xoroshiro128PlusPlusEngine(default_seed) {}

	constexpr explicit Xoroshiro128PlusPlusEngine(result_type seedValue) noexcept {
		seed(seedValue);
	}

	constexpr explicit Xoroshiro128PlusPlusEngine(const State& state) noexcept
		: state(state) {}

	constexpr void seed(result_type seedValue = default_seed) noexcept {
		SplitMix64Engine stateGenerator{seedValue};
		setState({stateGenerator(), stateGenerator()});
	}

	constexpr result_type operator()() noexcept {
		const uint64_t s0 = state[0];
		uint64_t s1 = state[1];
		const uint64_t result = rotateBitsLeft(s0 + s1, 17) + s0;
		s1 ^= s0;
		state[0] = rotateBitsLeft(s0, 49) ^ s1 ^ (s1 << 21);
		state[1] = rotateBitsLeft(s1, 28);
		return result;
	}

	constexpr void discard(unsigned long long z) noexcept {
		while (z-- > 0) {
			(*this)();
		}
	}

	/**
	 * Advance the internal state 2^64 times.
	 */
	constexpr void jump() noexcept {
		uint64_t s0 = 0;
		uint64_t s1 = 0;
		for (const uint64_t c : {0x2BD7A6A6E99C2DDCull, 0x0992CCAF6A6FCA05ull}) {
			for (int b = 0; b < 64; ++b) {
				if ((c & (uint64_t{1} << b)) != 0) {
					s0 ^= state[0];
					s1 ^= state[1];
				}
				(*this)();
			}
		}
		state[0] = s0;
		state[1] = s1;
	}

	/**
	 * Advance the internal state 2^96 times.
	 */
	constexpr void longJump() noexcept {
		uint64_t s0 = 0;
		uint64_t s1 = 0;
		for (const uint64_t c : {0x360FD5F2CF8D5D99ull, 0x9C6E6877736C46E3ull}) {
			for (int b = 0; b < 64; ++b) {
				if ((c & (uint64_t{1} << b)) != 0) {
					s0 ^= state[0];
					s1 ^= state[1];
				}
				(*this)();
			}
		}
		state[0] = s0;
		state[1] = s1;
	}

	/**
	 * Set the value representation of the internal state.
	 *
	 * \param newState new state of the engine to set the state to.
	 *
	 * \note This function is mainly intended to be used for deserialization.
	 *
	 * \sa getState()
	 */
	constexpr void setState(const State& newState) noexcept {
		state = newState;
	}

	/**
	 * Get the value representation of the internal state.
	 *
	 * \return the internal state of the engine.
	 *
	 * \note This function is mainly intended to be used for serialization.
	 *
	 * \sa setState()
	 */
	[[nodiscard]] constexpr State getState() const noexcept {
		return state;
	}

	[[nodiscard]] constexpr bool operator==(const Xoroshiro128PlusPlusEngine& other) const noexcept {
		return state == other.state;
	}

	template <typename CharT, typename Traits>
	friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& stream, const Xoroshiro128PlusPlusEngine& engine) {
		return stream << engine.state[0] << ' ' << engine.state[1];
	}

	template <typename CharT, typename Traits>
	friend std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& stream, Xoroshiro128PlusPlusEngine& engine) {
		return stream >> engine.state[0] >> engine.state[1];
	}

private:
	State state{};
};

/**
 * Uniformly-destributed random number generator that provides non-deterministic
 * random numbers if supported by the host system.
 *
 * \warning The performance of this engine may sharply degrade once the
 *          available entropy is exhausted. Therefore, it should mainly be used
 *          to seed a pseudorandom number generator such as DefaultRandomEngine.
 */
using NonDeterministicRandomEngine = std::random_device;

/**
 * Default pseudorandom number generator.
 *
 * \note This type may be different from std::default_random_engine.
 *
 * \warning This engine is not guaranteed to produce cryptographcially secure
 *          randomness and should therefore not be used in situations that
 *          require a CSPRNG.
 */
using DefaultRandomEngine = Xoroshiro128PlusPlusEngine;

/**
 * Random number distribution that produces integer values uniformly distributed
 * on the closed interval [`a`, `b`].
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using UniformIntegerDistribution = std::uniform_int_distribution<Integer>;

/**
 * Random number distribution that produces floating-point values uniformly
 * distributed on the half-open interval [`a`, `b`).
 *
 * \tparam Float floating-point type to generate. Must be one of the following
 *         types:
 *         - float
 *         - double
 *         - long double
 *
 * \warning Most existing implementations have a bug where they may occasionally
 *          return b, despite the interval being defined as half-open.
 */
template <typename Float>
using UniformRealDistribution = std::uniform_real_distribution<Float>;

/**
 * Random number distribution that produces boolean values with probability `p`
 * of being true.
 */
using BernoulliDistribution = std::bernoulli_distribution;

/**
 * Random number distribution that produces non-negative integer values that
 * correspond to the number of true values in a sequence of `t` boolean tests,
 * each with probability `p` of being true.
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using BinomialDistribution = std::binomial_distribution<Integer>;

/**
 * Random number distribution that produces non-negative integer values that
 * correspond to the number of false values before exactly `k` true values occur
 * in a sequence of boolean tests, each with probability `p` of being true.
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using NegativeBinomialDistribution = std::negative_binomial_distribution<Integer>;

/**
 * Random number distribution that produces non-negative integer values that
 * correspond to the number of false values before a true value occurs in a
 * sequence of boolean tests, each with probability `p` of being true.
 *
 * This distribution is equivalent to NegativeBinomialDistribution with `k = 1`,
 * and is the integer counterpart of ExponentialDistribution.
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using GeometricDistribution = std::geometric_distribution<Integer>;

/**
 * Random number distribution that produces non-negative integer values that
 * correspond to the number of occurences of a random event where the expected
 * average number of occurences is `μ` in the same interval under the same
 * conditions.
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using PoissonDistribution = std::poisson_distribution<Integer>;

/**
 * Random number distribution that produces non-negative floating-point values
 * that correspond to the time/distance until the next random event if random
 * events occur at a constant average rate `λ` per unit of time/distance.
 *
 * This distribution is the floating-point counterpart of GeometricDistribution.
 *
 * \tparam Float floating-point type to generate. Must be one of the following
 *         types:
 *         - float
 *         - double
 *         - long double
 */
template <typename Float>
using ExponentialDistribution = std::exponential_distribution<Float>;

/**
 * Random number distribution that produces floating-point values according to a
 * Normal/Gaussian distribution with mean `μ` and standard deviation `σ`.
 *
 * \tparam Float floating-point type to generate. Must be one of the following
 *         types:
 *         - float
 *         - double
 *         - long double
 */
template <typename Float>
using NormalDistribution = std::normal_distribution<Float>;

/**
 * Random number distribution that, given a list of weights `w`, produces
 * integer values in the interval `[0, w.size() - 1]` with each integer `i`
 * having probability `w[i] / sum(w)` of being selected.
 *
 * \tparam Integer integer type to generate. Must be one of the following types:
 *         - short
 *         - int
 *         - long
 *         - long long
 *         - unsigned short
 *         - unsigned
 *         - unsigned long
 *         - unsigned long long
 */
template <typename Integer>
using DiscreteDistribution = std::discrete_distribution<Integer>;

/**
 * Generate a random floating-point number uniformly distributed on the
 * half-open interval [0, 1).
 *
 * \tparam T floating-point number type to generate.
 * \tparam EntropyBits number of bits of entropy to acquire when generating the
 *         result.
 *
 * \param generator uniform random bit generator to acquire entropy from.
 *
 * \return a value in the range [0, 1).
 *
 * \warning Most existing implementations have a bug where they may occasionally
 *          return 1, despite the interval being defined as half-open.
 */
template <floating_point T, size_t EntropyBits = std::numeric_limits<T>::digits>
[[nodiscard]] inline T generateCanonical(auto& generator) {
	return std::generate_canonical<T, EntropyBits>(generator);
}

} // namespace grem::randomness

#endif
