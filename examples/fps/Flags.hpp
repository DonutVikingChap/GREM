// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_FLAGS_HPP
#define GREM_EXAMPLES_FPS_FLAGS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/formats/json.hpp>

#include "serialization.hpp"

#include <cstddef>     // std::size_t
#include <functional>  // std::hash
#include <stdexcept>   // std::invalid_argument
#include <type_traits> // std::underlying_type_t

template <enumeration Flag>
class Flags {
public:
	struct TriviallySerializableTag {};

	using flag_type = Flag;
	using value_type = std::underlying_type_t<Flag>;

	constexpr Flags() noexcept = default;

	constexpr Flags(value_type bits) noexcept
		: bits(bits) {}

	constexpr Flags(int bits) noexcept requires(!same_as<value_type, int>)
		: bits(static_cast<value_type>(bits)) {}

	[[nodiscard]] constexpr bool operator==(const Flags&) const noexcept = default;
	[[nodiscard]] constexpr auto operator<=>(const Flags&) const noexcept = default;

	void serializeTo(Writer output) const {
		serialize(bits, output);
	}

	[[nodiscard]] bool deserializeFrom(SpanReader input) {
		return deserialize(bits, input);
	}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
	void parseValueFrom(const json::Value& jsonValue) {
		if (jsonValue.isArray()) {
			for (const json::Value& item : jsonValue.getArray()) {
				Flag flag{};
				parseValue(item, flag);
				bits |= flag;
			}
		} else if (jsonValue.isString()) {
			Flag flag{};
			parseValue(jsonValue, flag);
			bits |= flag;
		} else if (const json::Number* const number = jsonValue.get_if<json::Number>();
			number && trunc(*number) == *number && *number >= json::Number{Limits<value_type>::MIN} && *number <= json::Number{Limits<value_type>::MAX}) {
			bits = static_cast<value_type>(*number);
		} else {
			throw std::invalid_argument{"Expected an array, a string or a non-negative integer."};
		}
	}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(bits);
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return bits == 0;
	}

	[[nodiscard]] constexpr bool contains(Flag flag) const noexcept {
		return (bits & Flags{flag}.bits) != 0;
	}

	[[nodiscard]] constexpr bool containsAnyOf(Flags flags) const noexcept {
		return (bits & flags.bits) != 0;
	}

	[[nodiscard]] constexpr bool containsAllOf(Flags flags) const noexcept {
		return static_cast<value_type>(bits & flags.bits) == flags.bits;
	}

	[[nodiscard]] friend constexpr Flags operator~(Flags a) noexcept {
		return Flags{static_cast<value_type>(~a.bits)};
	}

	[[nodiscard]] friend constexpr Flags operator&(Flags a, Flags b) noexcept {
		return Flags{static_cast<value_type>(a.bits & b.bits)};
	}

	[[nodiscard]] friend constexpr Flags operator|(Flags a, Flags b) noexcept {
		return Flags{static_cast<value_type>(a.bits | b.bits)};
	}

	[[nodiscard]] friend constexpr Flags operator^(Flags a, Flags b) noexcept {
		return Flags{static_cast<value_type>(a.bits ^ b.bits)};
	}

	friend constexpr Flags& operator&=(Flags& a, Flags b) noexcept {
		return a = a & b;
	}

	friend constexpr Flags& operator|=(Flags& a, Flags b) noexcept {
		return a = a | b;
	}

	friend constexpr Flags& operator^=(Flags& a, Flags b) noexcept {
		return a = a ^ b;
	}

private:
	template <typename T>
	friend struct std::hash;

	value_type bits{};
};

template <derived_from_template_specialization_of<Flags> T>
struct std::hash<T> {
	[[nodiscard]] std::size_t operator()(const T& type) const {
		return hasher(type.bits);
	}

private:
	[[no_unique_address]] std::hash<typename T::value_type> hasher;
};

#endif
