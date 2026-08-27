// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_NAMED_TYPE_HPP
#define GREM_EXAMPLES_FPS_NAMED_TYPE_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstddef>    // std::size_t
#include <functional> // std::hash

class NamedType {
public:
	struct TriviallySerializableTag {};

	constexpr NamedType() noexcept = default;

	consteval explicit NamedType(StringView name) noexcept
		: nameCRC32(name) {}

	constexpr explicit NamedType(CRC32 nameCRC32) noexcept
		: nameCRC32(nameCRC32) {}

	[[nodiscard]] constexpr bool operator==(const NamedType&) const noexcept = default;

	[[nodiscard]] constexpr auto operator<=>(const NamedType& other) const noexcept {
		return static_cast<uint32_t>(nameCRC32) <=> static_cast<uint32_t>(other.nameCRC32);
	}

	void serializeTo(Writer output) const {
		output.writeUInt32LE(static_cast<uint32_t>(nameCRC32));
	}

	[[nodiscard]] bool deserializeFrom(SpanReader input) {
		const Optional<uint32_t> value = input.tryReadUInt32LE();
		if (!value) {
			return false;
		}
		nameCRC32 = CRC32{*value};
		return true;
	}

	void parseValueFrom(const json::Value& jsonValue) {
		if (jsonValue.isNull()) {
			nameCRC32 = {};
		} else if (jsonValue.isString()) {
			nameCRC32 = CRC32{jsonValue.getString()};
		} else if (const json::Number* const number = jsonValue.get_if<json::Number>();
			number && trunc(*number) == *number && *number >= 0 && *number <= json::Number{Limits<uint32_t>::MAX}) {
			nameCRC32 = CRC32{static_cast<uint32_t>(*number)};
		} else {
			throw json::Error{"Expected null, a string or a non-negative integer.", jsonValue.getSource()};
		}
	}

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(static_cast<uint32_t>(nameCRC32));
	}

private:
	template <typename T>
	friend struct std::hash;

	template <typename T>
	friend struct grem::Formatter;

	CRC32 nameCRC32;
};

template <derived_from<NamedType> T>
struct std::hash<T> {
	[[nodiscard]] std::size_t operator()(const T& type) const {
		return hasher(type.nameCRC32);
	}

private:
	[[no_unique_address]] std::hash<CRC32> hasher;
};

template <derived_from<NamedType> T>
struct grem::Formatter<T> : Formatter<StringView> {
	void formatTo(FormatOutput& output, const T& value) const {
		Formatter<StringView>::formatTo(output, formatString("0x{:04X}", static_cast<uint32_t>(value.nameCRC32)));
	}
};

#endif
