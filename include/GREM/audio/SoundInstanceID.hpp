// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_AUDIO_SOUND_INSTANCE_ID_HPP
#define GREM_AUDIO_SOUND_INSTANCE_ID_HPP

#include <GREM/build_config.hpp>

#include <compare>    // std::strong_ordering
#include <cstddef>    // std::size_t
#include <functional> // std::hash

namespace grem::audio {

class SoundStage;    // Forward declaration, to avoid a circular include of SoundStage.hpp.
class SoundMix;      // Forward declaration, to avoid a circular include of SoundMix.hpp.
class DuckingFilter; // Forward declaration, to avoid a circular include of filters.hpp.

/**
 * Opaque handle to a specific instance of a sound or sound mix playing in a
 * sound stage or sound mix.
 */
struct SoundInstanceID {
	/**
     * Construct an invalid sound instance handle.
     */
	constexpr SoundInstanceID() noexcept = default;

	/**
	 * Check if this handle is potentially valid.
	 *
	 * \return true if this handle is potentially valid, false if it is equal to
	 *         a default-constructed invalid handle.
	 */
	constexpr explicit operator bool() const noexcept {
		return *this != SoundInstanceID{};
	}

	/**
	 * Compare this handle to another for equality.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return true if the handles are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SoundInstanceID& other) const noexcept = default;

	/**
	 * Compare this handle to another.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return a strong ordering between the two handles.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const SoundInstanceID& other) const noexcept = default;

private:
	friend SoundStage;
	friend SoundMix;
	friend DuckingFilter;
	friend std::hash<SoundInstanceID>;

	constexpr explicit SoundInstanceID(unsigned value) noexcept
		: value(value) {}

	unsigned value = 0;
};

} // namespace grem::audio

template <>
struct std::hash<grem::audio::SoundInstanceID> {
	[[nodiscard]] std::size_t operator()(const grem::audio::SoundInstanceID& soundInstanceID) const {
		return hasher(soundInstanceID.value);
	}

private:
	[[no_unique_address]] std::hash<unsigned> hasher;
};

#endif
