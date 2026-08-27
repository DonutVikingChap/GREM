// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_THREAD_HPP
#define GREM_CORE_SYSTEM_THREAD_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include <functional>  // std::hash
#include <iosfwd>      // std::basic_ostream
#include <type_traits> // std::is_..._v, std::remove_..._t

#ifdef GREM_USE_MULTITHREADING
#include <thread>  // std::thread, std::this_thread, std::hash<std::thread::id>
#include <utility> // std::forward
#elif defined(_WIN32)
#include <windows.h> // DWORD, Sleep, YieldProcessor
#elif defined(__linux__)
#include <cerrno>  // errno, EINTR
#include <sched.h> // sched_yield
#include <time.h>  // nanosleep // NOLINT(modernize-deprecated-headers)
#endif

#ifndef GREM_USE_MULTITHREADING
#include <GREM/core/assertions.hpp>
#endif

namespace grem {

namespace detail {

struct InvalidNativeThreadHandle {};

template <typename Thread>
struct native_thread_handle {
	using type = InvalidNativeThreadHandle;
};

template <typename Thread>
requires(requires { typename Thread::native_handle_type; }) struct native_thread_handle<Thread> {
	using type = typename Thread::native_handle_type;
};

} // namespace detail

class Thread; // Forward declaration.

class ThreadID {
public:
	[[nodiscard]] GREM_ALWAYS_INLINE static ThreadID getCurrent() noexcept {
#ifdef GREM_USE_MULTITHREADING
		return std::this_thread::get_id();
#else
		return {};
#endif
	}

	ThreadID() noexcept = default;

#ifdef GREM_USE_MULTITHREADING
	ThreadID(std::thread::id value) noexcept
		: value(value) {}
#endif

	[[nodiscard]] bool operator==(const ThreadID&) const = default;
	[[nodiscard]] auto operator<=>(const ThreadID&) const = default;

	GREM_ALWAYS_INLINE explicit operator bool() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value != std::thread::id{};
#else
		return false;
#endif
	}

#ifdef GREM_USE_MULTITHREADING
	GREM_ALWAYS_INLINE operator std::thread::id() const noexcept {
		return value;
	}
#endif

	template <typename CharT, typename Traits>
	friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& stream, ThreadID id) {
#ifdef GREM_USE_MULTITHREADING
		return stream << id.value;
#else
		(void)id;
		return stream;
#endif
	}

private:
	friend Thread;
	friend std::hash<ThreadID>;

#ifdef GREM_USE_MULTITHREADING
	std::thread::id value{};
#endif
};

class Thread {
public:
#ifdef GREM_USE_MULTITHREADING
	using native_handle_type = typename detail::native_thread_handle<std::thread>::type;
#else
	using native_handle_type = detail::InvalidNativeThreadHandle;
#endif

	[[nodiscard]] GREM_ALWAYS_INLINE static unsigned int hardware_concurrency() noexcept {
#ifdef GREM_USE_MULTITHREADING
		return std::thread::hardware_concurrency();
#else
		return 1;
#endif
	}

	Thread() noexcept = default;
	~Thread() = default;

	template <typename F, typename... Args>
	GREM_ALWAYS_INLINE Thread([[maybe_unused]] F&& f, [[maybe_unused]] Args&&... args) requires(!std::is_same_v<std::remove_cvref_t<F>, Thread>)
#ifdef GREM_USE_MULTITHREADING
		: value(std::forward<F>(f), std::forward<Args>(args)...)
#endif
	{
#ifndef GREM_USE_MULTITHREADING
		unreachable();
#endif
	}

	Thread(Thread&&) noexcept = default;
	Thread(const Thread&) = delete;
	Thread& operator=(Thread&&) noexcept = default;
	Thread& operator=(const Thread&) = delete;

	[[nodiscard]] GREM_ALWAYS_INLINE bool joinable() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.joinable();
#else
		return false;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ThreadID get_id() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.get_id();
#else
		return {};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE native_handle_type native_handle() {
#ifdef GREM_USE_MULTITHREADING
		if constexpr (requires { value.native_handle(); }) {
			return value.native_handle();
		} else {
			return {};
		}
#else
		return {};
#endif
	}

	GREM_ALWAYS_INLINE void join() {
#ifdef GREM_USE_MULTITHREADING
		value.join();
#endif
	}

	GREM_ALWAYS_INLINE void detach() {
#ifdef GREM_USE_MULTITHREADING
		value.detach();
#endif
	}

	GREM_ALWAYS_INLINE void swap([[maybe_unused]] Thread& other) noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.swap(other.value);
#endif
	}

	GREM_ALWAYS_INLINE friend void swap(Thread& a, Thread& b) noexcept {
		a.swap(b);
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::thread value{};
#endif
};

GREM_ALWAYS_INLINE void yield() noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::this_thread::yield();
#elif defined(_WIN32)
	YieldProcessor();
#elif defined(__linux__)
	sched_yield();
#endif
}

template <typename Rep, typename Period>
GREM_ALWAYS_INLINE void sleepFor(const DurationBase<Rep, Period>& duration) noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::this_thread::sleep_for(duration);
#elif defined(_WIN32)
	if (duration <= DurationBase<Rep, Period>{}) {
		return;
	}
	const TimePoint endTime = Clock::now() + duration_cast<Duration>(duration);
	using DWORDMilliseconds = DurationBase<DWORD, Ratio<1, 1'000>>;
	const DWORD milliseconds = floor<DWORDMilliseconds>(duration).count();
	if (milliseconds > 0) {
		Sleep(milliseconds);
	}
	TimePoint currentTime = Clock::now();
	while (currentTime < endTime) {
		currentTime = Clock::now();
	}
#elif defined(__linux__)
	if (duration <= DurationBase<Rep, Period>{}) {
		return;
	}
	struct timespec ts = {};
	using TimespecSeconds = DurationBase<decltype(ts.tv_sec), Ratio<1, 1>>;
	using TimespecNanoseconds = DurationBase<decltype(ts.tv_nsec), Ratio<1, 1'000'000'000>>;
	const TimespecSeconds seconds = floor<TimespecSeconds>(duration);
	const TimespecNanoseconds nanoseconds = duration_cast<TimespecNanoseconds>(duration - seconds);
	ts.tv_sec = seconds.count();
	ts.tv_nsec = nanoseconds.count();
	struct timespec rem = {};
	while (nanosleep(&ts, &rem) != 0) {
		if (errno == EINTR) {
			ts = rem;
			rem = {};
		} else {
			TimePoint currentTime = Clock::now();
			const TimePoint endTime = currentTime + duration_cast<Duration>(duration);
			while (currentTime < endTime) {
				currentTime = Clock::now();
			}
			break;
		}
	}
#else
	TimePoint currentTime = Clock::now();
	const TimePoint endTime = currentTime + duration_cast<Duration>(duration);
	while (currentTime < endTime) {
		currentTime = Clock::now();
	}
#endif
}

template <typename Clock, typename Duration>
GREM_ALWAYS_INLINE void sleepUntil(const TimePointBase<Clock, Duration>& time) noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::this_thread::sleep_until(time);
#elif defined(_WIN32) || defined(__linux__)
	typename Clock::time_point currentTime = Clock::now();
	if constexpr (Clock::is_steady) {
		if (currentTime < time) {
			sleepFor(time - currentTime);
		}
	} else {
		while (currentTime < time) {
			sleepFor(time - currentTime);
			currentTime = Clock::now();
		}
	}
#else
	typename Clock::time_point currentTime = Clock::now();
	while (currentTime < time) {
		currentTime = Clock::now();
	}
#endif
}

} // namespace grem

template <>
struct std::hash<grem::ThreadID> {
	[[nodiscard]] std::size_t operator()(const grem::ThreadID& id) const {
#ifdef GREM_USE_MULTITHREADING
		return hasher(id.value);
#else
		(void)id;
		return 0;
#endif
	}

#ifdef GREM_USE_MULTITHREADING
private:
	std::hash<std::thread::id> hasher;
#endif
};

#endif
