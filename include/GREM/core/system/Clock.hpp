// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_CLOCK_HPP
#define GREM_CORE_SYSTEM_CLOCK_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/time.hpp>

#include <chrono>       // std::chrono::...
#include <ctime>        // std::time_t, std::tm, std::time, std::gmtime, std::strftime
#include <system_error> // std::system_error, std::generic_category
#ifdef __linux__
#include <time.h> // gmtime_r // NOLINT(modernize-deprecated-headers)
#endif

namespace grem {

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
using TimePoint = Clock::time_point;

struct ISO8601FormattingOptions {
	char dateTimeDelimiter = 'T';
	char timeSeparator = ':';
	bool timeZoneDesignator = true;
};

struct UTCTimestamp {
	[[nodiscard]] static UTCTimestamp now() {
		const std::time_t currentTime = std::time(nullptr);
		if (currentTime == static_cast<std::time_t>(-1)) {
			throw std::system_error{errno, std::generic_category()};
		}
		std::tm currentTimeUTC{};
#ifdef __linux__
		if (!gmtime_r(&currentTime, &currentTimeUTC)) {
			throw std::system_error{errno, std::generic_category()};
		}
#else
		if (const std::tm* const gmtimeResult = std::gmtime(&currentTime)) {
			currentTimeUTC = *gmtimeResult;
		} else {
			throw std::system_error{errno, std::generic_category()};
		}
#endif
		return {
			.year = static_cast<uint16_t>(1900 + currentTimeUTC.tm_year),
			.month = static_cast<uint8_t>(currentTimeUTC.tm_mon),
			.day = static_cast<uint8_t>(currentTimeUTC.tm_mday),
			.hour = static_cast<uint8_t>(currentTimeUTC.tm_hour),
			.minute = static_cast<uint8_t>(currentTimeUTC.tm_min),
			.second = static_cast<uint8_t>(currentTimeUTC.tm_sec),
		};
	}

	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;

	void formatTo(Span<char> output, CStringView format) const {
		std::tm tm{};
		tm.tm_year = static_cast<int>(year) - 1900;
		tm.tm_mon = static_cast<int>(month);
		tm.tm_mday = static_cast<int>(day);
		tm.tm_hour = static_cast<int>(hour);
		tm.tm_min = static_cast<int>(minute);
		tm.tm_sec = static_cast<int>(second);
		if (std::strftime(output.data(), output.size(), format.c_str(), &tm) == 0) {
			throw std::length_error{"Maximum timestamp string length exceeded."};
		}
	}

	[[nodiscard]] String toISO8601(const ISO8601FormattingOptions& options = {}) const {
		char formatStringBuffer[] = "%Y-%m-%dT%H:%M:%SZ";
		formatStringBuffer[8] = options.dateTimeDelimiter;
		formatStringBuffer[11] = options.timeSeparator;
		formatStringBuffer[14] = options.timeSeparator;
		if (!options.timeZoneDesignator) {
			formatStringBuffer[17] = '\0';
		}
		char filepathTimestampStringBuffer[32]{};
		formatTo(filepathTimestampStringBuffer, formatStringBuffer);
		return String{static_cast<const char*>(filepathTimestampStringBuffer)};
	}

	[[nodiscard]] String toISO8601Date() const {
		return toISO8601({.dateTimeDelimiter = '\0'});
	}

	[[nodiscard]] String toISO8601Time(char timeSeparator = ':', char timeZoneDesignator = '\0') const {
		char formatStringBuffer[] = "%H:%M:%SZ";
		formatStringBuffer[2] = timeSeparator;
		formatStringBuffer[5] = timeSeparator;
		formatStringBuffer[8] = timeZoneDesignator;
		char filepathTimestampStringBuffer[16]{};
		formatTo(filepathTimestampStringBuffer, formatStringBuffer);
		return String{static_cast<const char*>(filepathTimestampStringBuffer)};
	}
};

namespace time_literals {

[[nodiscard]] consteval Duration operator""_seconds(long double value) {
	return duration_cast<Duration>(DurationBase<long double, Ratio<1, 1>>{value});
}

[[nodiscard]] consteval Duration operator""_second(long double value) {
	if (value != 1.0l) {
		throw Error{"Syntax error: Expected \"1.0_second\". For values other than 1, use the plural form \"_seconds\" instead."};
	}
	return operator""_seconds(value);
}

[[nodiscard]] consteval Duration operator""_milliseconds(long double value) {
	return duration_cast<Duration>(DurationBase<long double, Ratio<1, 1'000>>{value});
}

[[nodiscard]] consteval Duration operator""_millisecond(long double value) {
	if (value != 1.0l) {
		throw Error{"Syntax error: Expected \"1.0_millisecond\". For values other than 1, use the plural form \"_milliseconds\" instead."};
	}
	return operator""_milliseconds(value);
}

[[nodiscard]] consteval Duration operator""_microseconds(long double value) {
	return duration_cast<Duration>(DurationBase<long double, Ratio<1, 1'000'000>>{value});
}

[[nodiscard]] consteval Duration operator""_microsecond(long double value) {
	if (value != 1.0l) {
		throw Error{"Syntax error: Expected \"1.0_microsecond\". For values other than 1, use the plural form \"_microseconds\" instead."};
	}
	return operator""_microseconds(value);
}

[[nodiscard]] consteval Duration operator""_minutes(long double value) {
	return duration_cast<Duration>(DurationBase<long double, Ratio<60, 1>>{value});
}

[[nodiscard]] consteval Duration operator""_minute(long double value) {
	if (value != 1.0l) {
		throw Error{"Syntax error: Expected \"1.0_minute\". For values other than 1, use the plural form \"_minutes\" instead."};
	}
	return operator""_minutes(value);
}

[[nodiscard]] consteval Duration operator""_hours(long double value) {
	return duration_cast<Duration>(DurationBase<long double, Ratio<60 * 60, 1>>{value});
}

[[nodiscard]] consteval Duration operator""_hour(long double value) {
	if (value != 1.0l) {
		throw Error{"Syntax error: Expected \"1.0_hour\". For values other than 1, use the plural form \"_hours\" instead."};
	}
	return operator""_hours(value);
}

} // namespace time_literals

} // namespace grem

#endif
