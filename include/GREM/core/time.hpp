// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_TIME_HPP
#define GREM_CORE_TIME_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>

#include <chrono> // std::chrono::...
#include <ratio>  // std::ratio

namespace grem {

template <intmax_t Num, intmax_t Den = 1>
using Ratio = std::ratio<Num, Den>;

using std::ratio_add;    // NOLINT(misc-unused-alias-decls)
using std::ratio_divide; // NOLINT(misc-unused-alias-decls)
using std::ratio_equal;  // NOLINT(misc-unused-alias-decls)
using std::ratio_equal_v;
using std::ratio_greater;       // NOLINT(misc-unused-alias-decls)
using std::ratio_greater_equal; // NOLINT(misc-unused-alias-decls)
using std::ratio_greater_equal_v;
using std::ratio_greater_v;
using std::ratio_less;       // NOLINT(misc-unused-alias-decls)
using std::ratio_less_equal; // NOLINT(misc-unused-alias-decls)
using std::ratio_less_equal_v;
using std::ratio_less_v;
using std::ratio_multiply;  // NOLINT(misc-unused-alias-decls)
using std::ratio_not_equal; // NOLINT(misc-unused-alias-decls)
using std::ratio_not_equal_v;
using std::ratio_subtract; // NOLINT(misc-unused-alias-decls)

template <typename Rep, typename Period = Ratio<1, 1>>
using DurationBase = std::chrono::duration<Rep, Period>;

template <typename Clock, typename Duration = typename Clock::duration>
using TimePointBase = std::chrono::time_point<Clock, Duration>;

using std::chrono::abs;
using std::chrono::ceil;
using std::chrono::duration_cast;
using std::chrono::floor;
using std::chrono::round;
using std::chrono::time_point_cast;

using Hours = DurationBase<int64_t, Ratio<60 * 60, 1>>;
using Minutes = DurationBase<int64_t, Ratio<60, 1>>;
using Seconds = DurationBase<int64_t, Ratio<1, 1>>;
using Milliseconds = DurationBase<int64_t, Ratio<1, 1'000>>;
using Microseconds = DurationBase<int64_t, Ratio<1, 1'000'000>>;
using Nanoseconds = DurationBase<int64_t, Ratio<1, 1'000'000'000>>;

using FloatHours = DurationBase<float, Ratio<60 * 60, 1>>;
using FloatMinutes = DurationBase<float, Ratio<60, 1>>;
using FloatSeconds = DurationBase<float, Ratio<1, 1>>;
using FloatMilliseconds = DurationBase<float, Ratio<1, 1'000>>;
using FloatMicroseconds = DurationBase<float, Ratio<1, 1'000'000>>;
using FloatNanoseconds = DurationBase<float, Ratio<1, 1'000'000'000>>;

/**
 * Subtract time from a time value and then check if it reached 0.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to subtract from the timer.
 *
 * \return true if the time value reached below or equal to 0, false otherwise.
 *
 * \note The time value is clamped to 0 when it is reached.
 *
 * \sa countup()
 * \sa countdownLoop()
 */
template <typename Timer, typename DeltaTime>
constexpr bool countdown(Timer& timer, DeltaTime deltaTime) {
	timer -= deltaTime;
	if (timer <= Timer{}) {
		timer = Timer{};
		return true;
	}
	return false;
}

/**
 * Subtract time from a time value and then check if it reached a given target
 * time value.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to subtract from the timer.
 * \param targetTime target time to count down towards.
 *
 * \return true if the time value reached below or equal to the target time,
 *         false otherwise.
 *
 * \note The time value is clamped to the target time when the target is
 *       reached.
 *
 * \sa countup()
 * \sa countdownLoop()
 */
template <typename Timer, typename DeltaTime, typename TargetTime>
constexpr bool countdown(Timer& timer, DeltaTime deltaTime, TargetTime targetTime) {
	timer -= deltaTime;
	if (timer <= targetTime) {
		timer = targetTime;
		return true;
	}
	return false;
}

/**
 * Add time to a time value and then check if it reached a given target time
 * value.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to add to the timer.
 * \param targetTime target time to count up towards.
 *
 * \return true if the time value reached above or equal to the target time,
 *         false otherwise.
 *
 * \note The time value is clamped to the target time when the target is
 *       reached.
 *
 * \sa countdown()
 * \sa countupLoop()
 */
template <typename Timer, typename DeltaTime, typename TargetTime>
constexpr bool countup(Timer& timer, DeltaTime deltaTime, TargetTime targetTime) {
	timer += deltaTime;
	if (timer >= targetTime) {
		timer = targetTime;
		return true;
	}
	return false;
}

/**
 * Subtract time from a time value and then check how many times it reached 0
 * while looping back to a given time interval.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to subtract from the timer.
 * \param interval loop interval duration.
 *
 * \return the number of times that the time value reached below or equal to 0
 *         and looped back around. This may be any non-negative integer,
 *         including 0.
 *
 * \note An interval duration of 0 results in the number 1 being returned every
 *       time.
 *
 * \sa countdown()
 * \sa countupLoop()
 */
template <typename Timer, typename DeltaTime, typename Interval>
[[nodiscard]] constexpr size_t countdownLoop(Timer& timer, DeltaTime deltaTime, Interval interval) {
	if (interval <= Interval{}) {
		timer = Timer{};
		return 1;
	}
	size_t ticks = 0;
	timer -= deltaTime;
	while (timer <= Timer{}) {
		timer += interval;
		++ticks;
	}
	return ticks;
}

/**
 * Add time to a time value and then check how many times it reached a given
 * time interval while looping back to 0.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to add to the timer.
 * \param interval loop interval duration.
 *
 * \return the number of times that the time value reached above or equal to the
 *         interval time and looped back around. This may be any non-negative
 *         integer, including 0.
 *
 * \note An interval duration of 0 results in the number 1 being returned every
 *       time.
 *
 * \sa countup()
 * \sa countdownLoop()
 */
template <typename Timer, typename DeltaTime, typename Interval>
[[nodiscard]] constexpr size_t countupLoop(Timer& timer, DeltaTime deltaTime, Interval interval) {
	if (interval <= Interval{}) {
		timer = Timer{};
		return 1;
	}
	size_t ticks = 0;
	timer += deltaTime;
	while (timer >= interval) {
		timer -= interval;
		++ticks;
	}
	return ticks;
}

/**
 * Update a countdown loop with a boolean trigger that determines whether the
 * loop is active or not.
 *
 * An inactive loop will continue counting down to 0 but will not loop back
 * around to the interval time and will always return 0.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to subtract from the timer.
 * \param interval loop interval duration.
 * \param active whether the loop is currently active or not.
 *
 * \return the number of times that the time value reached below or equal to 0
 *         while active. This may be any non-negative integer, including 0.
 *
 * \note An interval duration of 0 results in the number 1 being returned every
 *       time when the loop is active.
 *
 * \remark This function can be used to simulate something like the trigger
 *         mechanism of a fully automatic firearm firing from a closed bolt,
 *         since it will fire once as soon as it is activated and then keep
 *         firing at a fixed cyclic rate until the trigger is released, at which
 *         point the mechanism will continue to cycle into the closed position
 *         even if the trigger is not held, where it will then stop without
 *         firing the next round, and be ready to fire immediately when the
 *         trigger is activated again. Reactivating the trigger before the
 *         mechanism has fully cycled does not make it fire more quickly.
 *
 * \sa countdownLoop()
 */
template <typename Timer, typename DeltaTime, typename Interval>
[[nodiscard]] constexpr size_t countdownLoop(Timer& timer, DeltaTime deltaTime, Interval interval, bool active) {
	if (active) {
		return countdownLoop(timer, deltaTime, interval);
	}
	countdown(timer, deltaTime);
	return 0;
}

/**
 * Update a countup loop with a boolean trigger that determines whether the
 * loop is active or not.
 *
 * An inactive loop will reset itself to 0 and will always return 0.
 *
 * \param timer time value to update.
 * \param deltaTime time delta to add to the timer.
 * \param interval loop interval duration.
 * \param active whether the loop is currently active or not.
 *
 * \return the number of times that the time value reached above or equal to the
 *         interval time while active. This may be any non-negative integer,
 *         including 0.
 *
 * \note An interval duration of 0 results in the number 1 being returned every
 *       time when the loop is active.
 *
 * \remark This function can be used to simulate something like the trigger
 *         mechanism of a hypothetical "railgun" that needs to be fully charged
 *         before it can fire. Activating the trigger starts the charging
 *         process, which can be canceled at any time by deactivating the
 *         trigger, which immediately resets the charge back to 0. The mechanism
 *         fires as soon as it is fully charged and then immediately starts
 *         charging the next round if the trigger is still held.
 *
 * \sa countupLoop()
 */
template <typename Timer, typename DeltaTime, typename Interval>
[[nodiscard]] constexpr size_t countupLoop(Timer& timer, DeltaTime deltaTime, Interval interval, bool active) {
	if (active) {
		return countupLoop(timer, deltaTime, interval);
	}
	timer = Timer{};
	return 0;
}

namespace time_literals {

[[nodiscard]] consteval Seconds operator""_seconds(unsigned long long value) {
	return Seconds{static_cast<Seconds::rep>(value)};
}

[[nodiscard]] consteval Seconds operator""_second(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_second\". For values other than 1, use the plural form \"_seconds\" instead."};
	}
	return operator""_seconds(value);
}

[[nodiscard]] consteval Milliseconds operator""_milliseconds(unsigned long long value) {
	return Milliseconds{static_cast<Milliseconds::rep>(value)};
}

[[nodiscard]] consteval Milliseconds operator""_millisecond(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_millisecond\". For values other than 1, use the plural form \"_milliseconds\" instead."};
	}
	return operator""_milliseconds(value);
}

[[nodiscard]] consteval Microseconds operator""_microseconds(unsigned long long value) {
	return Microseconds{static_cast<Microseconds::rep>(value)};
}

[[nodiscard]] consteval Microseconds operator""_microsecond(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_microsecond\". For values other than 1, use the plural form \"_microseconds\" instead."};
	}
	return operator""_microseconds(value);
}

[[nodiscard]] consteval Nanoseconds operator""_nanoseconds(unsigned long long value) {
	return Nanoseconds{static_cast<Nanoseconds::rep>(value)};
}

[[nodiscard]] consteval Nanoseconds operator""_nanosecond(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_nanosecond\". For values other than 1, use the plural form \"_nanoseconds\" instead."};
	}
	return operator""_nanoseconds(value);
}

[[nodiscard]] consteval Minutes operator""_minutes(unsigned long long value) {
	return Minutes{static_cast<Minutes::rep>(value)};
}

[[nodiscard]] consteval Minutes operator""_minute(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_minute\". For values other than 1, use the plural form \"_minutes\" instead."};
	}
	return operator""_minutes(value);
}

[[nodiscard]] consteval Hours operator""_hours(unsigned long long value) {
	return Hours{static_cast<Hours::rep>(value)};
}

[[nodiscard]] consteval Hours operator""_hour(unsigned long long value) {
	if (value != 1ull) {
		throw Error{"Syntax error: Expected \"1_hour\". For values other than 1, use the plural form \"_hours\" instead."};
	}
	return operator""_hours(value);
}

}; // namespace time_literals

} // namespace grem

template <typename Rep, typename Period>
struct grem::Formatter<grem::DurationBase<Rep, Period>> : Formatter<Rep> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<Rep>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const grem::DurationBase<Rep, Period>& value) const {
		Formatter<Rep>::formatTo(output, value.count());
		output.append(symbolSeparator);
		if constexpr (ratio_equal_v<Period, Ratio<1, 1>>) {
			output.append("s");
		} else if constexpr (ratio_equal_v<Period, Ratio<1, 1'000>>) {
			output.append("ms");
		} else if constexpr (ratio_equal_v<Period, Ratio<1, 1'000'000>>) {
			output.append("us");
		} else if constexpr (ratio_equal_v<Period, Ratio<1, 1'000'000'000>>) {
			output.append("ns");
		} else if constexpr (ratio_equal_v<Period, Ratio<60, 1>>) {
			output.append("min");
		} else if constexpr (ratio_equal_v<Period, Ratio<60 * 60, 1>>) {
			output.append("h");
		} else {
			formatTo(output, "({}/{})s", Period::num, Period::den);
		}
	}
};

#endif
