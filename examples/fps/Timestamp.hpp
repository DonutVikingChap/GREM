// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_TIMESTAMP_HPP
#define GREM_EXAMPLES_FPS_TIMESTAMP_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include "serialization.hpp"

using TickCount = uint64_t;
using TickDifference = int64_t;

class TickIndex {
public:
	struct TriviallySerializableTag {};

	constexpr TickIndex() noexcept = default;

	[[nodiscard]] constexpr bool operator==(const TickIndex&) const = default;
	[[nodiscard]] constexpr auto operator<=>(const TickIndex&) const = default;

	[[nodiscard]] constexpr TickIndex getPrevious(TickCount reverseOffset = 1) const {
		return TickIndex{value - min(reverseOffset, value)};
	}

	[[nodiscard]] constexpr TickIndex getNext(TickCount offset = 1) const {
		return TickIndex{value + offset};
	}

	constexpr TickIndex& operator++() {
		++value;
		return *this;
	}

	constexpr TickIndex operator++(int) {
		return TickIndex{value++};
	}

	constexpr TickIndex& operator--() {
		--value;
		return *this;
	}

	constexpr TickIndex operator--(int) {
		return TickIndex{value--};
	}

	constexpr TickIndex& operator+=(TickDifference offset) {
		value = static_cast<TickCount>(static_cast<TickDifference>(value) + offset);
		return *this;
	}

	constexpr TickIndex& operator-=(TickDifference offset) {
		value = static_cast<TickCount>(static_cast<TickDifference>(value) - offset);
		return *this;
	}

	[[nodiscard]] friend constexpr TickIndex operator+(const TickIndex& a, TickDifference b) {
		return TickIndex{static_cast<TickCount>(static_cast<TickDifference>(a.value) + b)};
	}

	[[nodiscard]] friend constexpr TickIndex operator+(TickDifference a, const TickIndex& b) {
		return TickIndex{static_cast<TickCount>(a + static_cast<TickDifference>(b.value))};
	}

	[[nodiscard]] friend constexpr TickIndex operator-(const TickIndex& a, TickDifference b) {
		return TickIndex{static_cast<TickCount>(static_cast<TickDifference>(a.value) - b)};
	}

	[[nodiscard]] friend constexpr TickIndex operator-(TickDifference a, const TickIndex& b) {
		return TickIndex{static_cast<TickCount>(a - static_cast<TickDifference>(b.value))};
	}

	[[nodiscard]] friend constexpr TickDifference operator-(const TickIndex& a, const TickIndex& b) {
		return static_cast<TickDifference>(a.value) - static_cast<TickDifference>(b.value);
	}

	void serializeTo(Writer output) const {
		serialize(value, output);
	}

	[[nodiscard]] bool deserializeFrom(SpanReader input) {
		return deserialize(value, input);
	}

	void parseValueFrom(const json::Value& jsonValue) {
		if (const json::Number* const number = jsonValue.get_if<json::Number>()) {
			if (isinf(*number)) {
				value = (signbit(*number)) ? Limits<TickCount>::MIN : Limits<TickCount>::MAX;
				return;
			}
			if (trunc(*number) == *number && *number >= 0) {
				value = static_cast<TickCount>(*number);
				return;
			}
		}
		throw json::Error{"Expected a non-negative integer.", jsonValue.getSource()};
	}

	[[nodiscard]] json::Variant toJSON() const {
		return static_cast<json::Number>(value);
	}

private:
	constexpr explicit TickIndex(TickCount value)
		: value(value) {}

	TickCount value = 0;
};

class Timestamp {
public:
	struct TriviallySerializableTag {};

	constexpr Timestamp() noexcept = default;

	constexpr Timestamp(TickIndex tickIndex)
		: Timestamp(tickIndex, Duration{}) {}

	constexpr Timestamp(TickIndex tickIndex, Duration timeOffset, Duration tickInterval)
		: Timestamp(tickIndex, Duration{}) {
		addTime(timeOffset, tickInterval);
	}

	[[nodiscard]] constexpr bool operator==(const Timestamp&) const = default;
	[[nodiscard]] constexpr auto operator<=>(const Timestamp&) const = default;

	constexpr void addTicks(TickDifference ticks) {
		tickIndex += max(ticks, TickIndex{} - tickIndex);
	}

	constexpr void addTime(Duration deltaTime, Duration tickInterval) {
		timeOffset += deltaTime;
		const Duration::rep ticks = timeOffset / tickInterval;
		addTicks(static_cast<TickDifference>(ticks));
		timeOffset -= ticks * tickInterval;
		if (timeOffset < Duration{}) {
			if (tickIndex > TickIndex{}) {
				--tickIndex;
				timeOffset += tickInterval;
			} else {
				timeOffset = {};
			}
		}
	}

	[[nodiscard]] constexpr Timestamp withTicksAdded(TickDifference ticks) const {
		Timestamp result = *this;
		result.addTicks(ticks);
		return result;
	}

	[[nodiscard]] constexpr Timestamp withTimeAdded(Duration deltaTime, Duration tickInterval) const {
		Timestamp result = *this;
		result.addTime(deltaTime, tickInterval);
		return result;
	}

	[[nodiscard]] constexpr TickIndex getTickIndex() const noexcept {
		return tickIndex;
	}

	[[nodiscard]] constexpr Duration getTimeOffset() const noexcept {
		return timeOffset;
	}

	void serializeTo(Writer output) const {
		serialize(tickIndex, output);
		serialize(static_cast<uint32_t>(duration_cast<Nanoseconds>(timeOffset).count()), output);
	}

	[[nodiscard]] bool deserializeFrom(SpanReader input) {
		if (!deserialize(tickIndex, input)) {
			return false;
		}
		uint32_t nanoseconds{};
		if (!deserialize(nanoseconds, input)) {
			return false;
		}
		timeOffset = duration_cast<Duration>(Nanoseconds{static_cast<Nanoseconds::rep>(nanoseconds)});
		return true;
	}

	void parseValueFrom(const json::Value& jsonValue) {
		const Nanoseconds::rep timeOffsetNumber = jsonValue.getNumberProperty<Nanoseconds::rep>("timeOffset");
		if (timeOffsetNumber < 0) {
			throw json::Error{"Expected a non-negative integer.", jsonValue.getSource()};
		}
		tickIndex.parseValueFrom(jsonValue.getProperty("tickIndex"));
		timeOffset = duration_cast<Duration>(Nanoseconds{timeOffsetNumber});
	}

	[[nodiscard]] json::Variant toJSON() const {
		return json::Object{
			{"tickIndex", tickIndex.toJSON()},
			{"timeOffset", static_cast<json::Number>(duration_cast<Nanoseconds>(timeOffset).count())},
		};
	}

	[[nodiscard]] friend constexpr Duration getTimeBetween(const Timestamp& a, const Timestamp& b, Duration tickInterval) {
		return static_cast<Duration::rep>(b.getTickIndex() - a.getTickIndex()) * tickInterval + (b.getTimeOffset() - a.getTimeOffset());
	}

private:
	constexpr Timestamp(TickIndex tickIndex, Duration timeOffset)
		: tickIndex(tickIndex)
		, timeOffset(timeOffset) {}

	TickIndex tickIndex{};
	Duration timeOffset{};
};

#endif
