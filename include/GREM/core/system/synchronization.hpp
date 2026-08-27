// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_SYNCHRONIZATION_HPP
#define GREM_CORE_SYSTEM_SYNCHRONIZATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <atomic>       // std::memory_order, std::atomic..., std::kill_dependency
#include <mutex>        // std::...mutex, std::...lock, std::...lock_t
#include <shared_mutex> // std::shared_mutex
#include <version>      // __cpp_lib_atomic_ref
#else
#include <GREM/core/data/Tuple.hpp>

#include <system_error> // std::system_error, std::errc, std::make_error_code(std::errc)
#include <utility>      // std::exchange, std::swap, std::...index_sequence
#endif
#include <cstddef>     // std::size_t, std::ptrdiff_t
#include <type_traits> // std::is_..._v, std::remove_..._t, std::underlying_type_t

namespace grem {

enum class MemoryOrder : int { // NOLINT(performance-enum-size)
#ifdef GREM_USE_MULTITHREADING
	RELAXED = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::relaxed)),
	CONSUME = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::consume)),
	ACQUIRE = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::acquire)),
	RELEASE = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::release)),
	ACQUIRE_RELEASE = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::acq_rel)),
	SEQUENTIALLY_CONSISTENT = static_cast<int>(static_cast<std::underlying_type_t<std::memory_order>>(std::memory_order::seq_cst)),
#else
	RELAXED,
	CONSUME,
	ACQUIRE,
	RELEASE,
	ACQUIRE_RELEASE,
	SEQUENTIALLY_CONSISTENT,
#endif
};

namespace detail {

#ifdef GREM_USE_MULTITHREADING
[[nodiscard]] constexpr std::memory_order translateMemoryOrder(MemoryOrder order) noexcept {
	return static_cast<std::memory_order>(static_cast<std::underlying_type_t<std::memory_order>>(static_cast<int>(order)));
}
#if __cpp_lib_atomic_ref < 201806L
[[nodiscard]] constexpr int translateMemoryOrderBuiltin(MemoryOrder order) noexcept {
	switch (order) {
		case MemoryOrder::RELAXED: return __ATOMIC_RELAXED;
		case MemoryOrder::CONSUME: return __ATOMIC_CONSUME;
		case MemoryOrder::ACQUIRE: return __ATOMIC_ACQUIRE;
		case MemoryOrder::RELEASE: return __ATOMIC_RELEASE;
		case MemoryOrder::ACQUIRE_RELEASE: return __ATOMIC_ACQ_REL;
		case MemoryOrder::SEQUENTIALLY_CONSISTENT: return __ATOMIC_SEQ_CST;
	}
	return 0;
}
#endif
#endif

template <typename T>
struct AtomicBase {
	using value_type = T;
};

template <typename T>
struct AtomicBase<T*> {
	using value_type = T*;
	using difference_type = ptrdiff_t;
};

template <typename T>
requires(std::is_integral_v<T> || std::is_floating_point_v<T>) struct AtomicBase<T> {
	using value_type = T;
	using difference_type = value_type;
};

} // namespace detail

template <typename T>
class Atomic : public detail::AtomicBase<T> {
public:
#ifdef GREM_USE_MULTITHREADING
	static constexpr bool is_always_lock_free = std::atomic<T>::is_always_lock_free;
#else
	static constexpr bool is_always_lock_free = true;
#endif

	constexpr Atomic() noexcept(std::is_nothrow_default_constructible_v<T>) = default;

	GREM_ALWAYS_INLINE Atomic(T value) noexcept
		: value(value) {}

	Atomic(const Atomic&) = delete;

	Atomic& operator=(const Atomic&) = delete;
	Atomic& operator=(const Atomic&) volatile = delete;

	[[nodiscard]] GREM_ALWAYS_INLINE bool is_lock_free() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.is_lock_free();
#else
		return true;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool is_lock_free() const volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.is_lock_free();
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void store(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.store(newValue, detail::translateMemoryOrder(order));
#else
		(void)order;
		value = newValue;
#endif
	}

	GREM_ALWAYS_INLINE void store(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.store(newValue, detail::translateMemoryOrder(order));
#else
		(void)order;
		value = newValue;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T load(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.load(detail::translateMemoryOrder(order));
#else
		(void)order;
		return value;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T load(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.load(detail::translateMemoryOrder(order));
#else
		(void)order;
		return value;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T exchange(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.exchange(newValue, detail::translateMemoryOrder(order));
#else
		(void)order;
		return std::exchange(value, newValue);
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T exchange(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.exchange(newValue, detail::translateMemoryOrder(order));
#else
		(void)order;
		return std::exchange(value, newValue);
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		(void)success;
		(void)failure;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		(void)success;
		(void)failure;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(order));
#else
		(void)order;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(order));
#else
		(void)order;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		(void)success;
		(void)failure;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder success, MemoryOrder failure) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		(void)success;
		(void)failure;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(order));
#else
		(void)order;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(order));
#else
		(void)order;
		if (memcmp(&value, &expected, sizeof(T)) == 0) {
			value = desired;
			return true;
		}
		expected = value;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE void wait(T old, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.wait(old, detail::translateMemoryOrder(order));
#else
		(void)old;
		(void)order;
		GREM_ASSERT(memcmp(&value, &old, sizeof(T)) != 0);
#endif
	}

	GREM_ALWAYS_INLINE void wait(T old, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.wait(old, detail::translateMemoryOrder(order));
#else
		(void)old;
		(void)order;
		GREM_ASSERT(memcmp(&value, &old, sizeof(T)) != 0);
#endif
	}

	GREM_ALWAYS_INLINE void notify_one() noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_one();
#endif
	}

	GREM_ALWAYS_INLINE void notify_one() volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_one();
#endif
	}

	GREM_ALWAYS_INLINE void notify_all() noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_all();
#endif
	}

	GREM_ALWAYS_INLINE void notify_all() volatile noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_all();
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_integral_v<T> || std::is_floating_point_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
	{
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_integral_v<T> || std::is_floating_point_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
	{
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_and(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_and(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value &= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_and(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_and(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value &= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_or(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_or(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value |= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_or(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_or(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value |= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_xor(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_xor(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value ^= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_xor(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) volatile noexcept requires(std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
		return value.fetch_xor(arg, detail::translateMemoryOrder(order));
#else
		(void)order;
		const T old = value;
		value ^= arg;
		return old;
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::atomic<T> value;
#else
	T value;
#endif
};

class AtomicFlag {
public:
	constexpr AtomicFlag() noexcept = default;

	AtomicFlag(const AtomicFlag&) = delete;

	AtomicFlag& operator=(const AtomicFlag&) = delete;

	GREM_ALWAYS_INLINE void clear(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.clear(detail::translateMemoryOrder(order));
#else
		(void)order;
		value = false;
#endif
	}

	GREM_ALWAYS_INLINE bool test_and_set(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.test_and_set(detail::translateMemoryOrder(order));
#else
		(void)order;
		const bool old = value;
		value = true;
		return old;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool test(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return value.test(detail::translateMemoryOrder(order));
#else
		(void)order;
		return value;
#endif
	}

	GREM_ALWAYS_INLINE void wait(bool old, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.wait(old, detail::translateMemoryOrder(order));
#else
		(void)old;
		(void)order;
		GREM_ASSERT(value != old);
#endif
	}

	GREM_ALWAYS_INLINE void notify_one() noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_one();
#endif
	}

	GREM_ALWAYS_INLINE void notify_all() noexcept {
#ifdef GREM_USE_MULTITHREADING
		value.notify_all();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::atomic_flag value{};
#else
	bool value = false;
#endif
};

GREM_ALWAYS_INLINE void atomicThreadFence(MemoryOrder order) noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::atomic_thread_fence(detail::translateMemoryOrder(order));
#else
	(void)order;
#endif
}

GREM_ALWAYS_INLINE void atomicSignalFence(MemoryOrder order) noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::atomic_signal_fence(detail::translateMemoryOrder(order));
#else
	(void)order;
#endif
}

template <typename T>
GREM_ALWAYS_INLINE T killDependency(T y) noexcept {
#ifdef GREM_USE_MULTITHREADING
	std::kill_dependency(y);
#else
	return y;
#endif
}

template <typename T>
class AtomicRef : public detail::AtomicBase<std::remove_cv_t<T>> {
public:
	using typename detail::AtomicBase<std::remove_cv_t<T>>::value_type;

#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
	static constexpr bool is_always_lock_free = std::atomic_ref<T>::is_always_lock_free;
	static constexpr std::size_t required_alignment = std::atomic_ref<T>::required_alignment;

	GREM_ALWAYS_INLINE explicit AtomicRef(T& obj)
		: ref(obj) {}

	GREM_ALWAYS_INLINE AtomicRef(const AtomicRef& other) noexcept
		: ref(other.ref) {}
#else
	static constexpr bool is_always_lock_free = true;
	static constexpr std::size_t required_alignment = alignof(T);

	GREM_ALWAYS_INLINE explicit AtomicRef(T& obj)
		: ptr(&obj) {}

	GREM_ALWAYS_INLINE AtomicRef(const AtomicRef& other) noexcept
		: ptr(other.ptr) {}
#endif
#else
	static constexpr bool is_always_lock_free = true;
	static constexpr std::size_t required_alignment = alignof(T);

	GREM_ALWAYS_INLINE explicit AtomicRef(T& obj)
		: ptr(&obj) {}

	GREM_ALWAYS_INLINE AtomicRef(const AtomicRef& other) noexcept
		: ptr(other.ptr) {}
#endif

	GREM_ALWAYS_INLINE value_type operator=(value_type desired) const noexcept // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
		requires(!std::is_const_v<T>) {
		store(desired);
		return desired;
	}

	AtomicRef& operator=(const AtomicRef&) = delete;

	[[nodiscard]] GREM_ALWAYS_INLINE bool is_lock_free() const noexcept {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.is_lock_free();
#else
		return true;
#endif
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void store(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		ref.store(newValue, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		__atomic_store(ptr, &newValue, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		*ptr = newValue;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T load(MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.load(detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		T result{};
		__atomic_load(ptr, &result, detail::translateMemoryOrderBuiltin(order));
		return result;
#endif
#else
		(void)order;
		return *ptr;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE T exchange(T newValue, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.exchange(newValue, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		T result{};
		__atomic_exchange(ptr, &newValue, &result, detail::translateMemoryOrderBuiltin(order));
		return result;
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		return std::exchange(*ptr, newValue);
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		GREM_ASSERT(ptr);
		return __atomic_compare_exchange(ptr, &expected, &desired, true, detail::translateMemoryOrderBuiltin(success), detail::translateMemoryOrderBuiltin(failure));
#endif
#else
		GREM_ASSERT(ptr);
		(void)success;
		(void)failure;
		if (memcmp(ptr, &expected, sizeof(T)) == 0) {
			*ptr = desired;
			return true;
		}
		expected = *ptr;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_weak(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.compare_exchange_weak(expected, desired, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_compare_exchange(ptr, &expected, &desired, true, detail::translateMemoryOrderBuiltin(order),
			(order == MemoryOrder::ACQUIRE_RELEASE) ? __ATOMIC_ACQUIRE
			: (order == MemoryOrder::RELEASE)       ? __ATOMIC_RELAXED
													: detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		if (memcmp(ptr, &expected, sizeof(T)) == 0) {
			*ptr = desired;
			return true;
		}
		expected = *ptr;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder success, MemoryOrder failure) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(success), detail::translateMemoryOrder(failure));
#else
		GREM_ASSERT(ptr);
		return __atomic_compare_exchange(ptr, &expected, &desired, false, detail::translateMemoryOrderBuiltin(success), detail::translateMemoryOrderBuiltin(failure));
#endif
#else
		GREM_ASSERT(ptr);
		(void)success;
		(void)failure;
		if (memcmp(ptr, &expected, sizeof(T)) == 0) {
			*ptr = desired;
			return true;
		}
		expected = *ptr;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE bool compare_exchange_strong(T& expected, T desired, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.compare_exchange_strong(expected, desired, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_compare_exchange(ptr, &expected, &desired, false, detail::translateMemoryOrderBuiltin(order),
			(order == MemoryOrder::ACQUIRE_RELEASE) ? __ATOMIC_ACQUIRE
			: (order == MemoryOrder::RELEASE)       ? __ATOMIC_RELAXED
													: detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		if (memcmp(ptr, &expected, sizeof(T)) == 0) {
			*ptr = desired;
			return true;
		}
		expected = *ptr;
		return false;
#endif
	}

	GREM_ALWAYS_INLINE void wait(T old, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		ref.wait(old, detail::translateMemoryOrder(order));
#else
		while (load(order) == old) {
		}
#endif
#else
		GREM_ASSERT(ptr);
		GREM_ASSERT(memcmp(ptr, &old, sizeof(T)) != 0);
		(void)old;
		(void)order;
#endif
	}

	GREM_ALWAYS_INLINE void notify_one() noexcept {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		ref.notify_one();
#endif
#endif
	}

	GREM_ALWAYS_INLINE void notify_all() noexcept {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		ref.notify_all();
#endif
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept
		requires(!std::is_const_v<T> && (std::is_integral_v<T> || std::is_floating_point_v<T>)) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_add(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_add(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept
		requires(!std::is_const_v<T> && std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_add(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_add(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr += arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept
		requires(!std::is_const_v<T> && (std::is_integral_v<T> || std::is_floating_point_v<T>)) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_sub(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_sub(std::ptrdiff_t arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept
		requires(!std::is_const_v<T> && std::is_pointer_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_sub(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_sub(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr -= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_and(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T> && std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_and(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_and(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr &= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_or(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T> && std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_or(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_or(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr |= arg;
		return old;
#endif
	}

	GREM_ALWAYS_INLINE T fetch_xor(T arg, MemoryOrder order = MemoryOrder::SEQUENTIALLY_CONSISTENT) const noexcept requires(!std::is_const_v<T> && std::is_integral_v<T>) {
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
		return ref.fetch_xor(arg, detail::translateMemoryOrder(order));
#else
		GREM_ASSERT(ptr);
		return __atomic_fetch_xor(ptr, arg, detail::translateMemoryOrderBuiltin(order));
#endif
#else
		GREM_ASSERT(ptr);
		(void)order;
		const T old = *ptr;
		*ptr ^= arg;
		return old;
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
#if __cpp_lib_atomic_ref >= 201806L
	std::atomic_ref<T> ref;
#else
	T* ptr;
#endif
#else
	T* ptr;
#endif
};

class Mutex {
public:
	constexpr Mutex() noexcept = default;

	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::mutex mutex{};
#endif
};

class RecursiveMutex {
public:
	constexpr RecursiveMutex() noexcept = default;

	RecursiveMutex(const RecursiveMutex&) = delete;
	RecursiveMutex& operator=(const RecursiveMutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::recursive_mutex mutex{};
#endif
};

class SharedMutex {
public:
	constexpr SharedMutex() noexcept = default;

	SharedMutex(const SharedMutex&) = delete;
	SharedMutex& operator=(const SharedMutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

	GREM_ALWAYS_INLINE void lock_shared() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock_shared();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_shared() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_shared();
#else
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock_shared() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock_shared();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::shared_mutex mutex{};
#endif
};

class TimedMutex {
public:
	constexpr TimedMutex() noexcept = default;

	TimedMutex(const TimedMutex&) = delete;
	TimedMutex& operator=(const TimedMutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_for(timeoutDuration);
#else
		(void)timeoutDuration;
		return true;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(const TimePoint& timeoutTime) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_until(timeoutTime);
#else
		(void)timeoutTime;
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::timed_mutex mutex{};
#endif
};

class RecursiveTimedMutex {
public:
	constexpr RecursiveTimedMutex() noexcept = default;

	RecursiveTimedMutex(const RecursiveTimedMutex&) = delete;
	RecursiveTimedMutex& operator=(const RecursiveTimedMutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_for(timeoutDuration);
#else
		(void)timeoutDuration;
		return true;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(const TimePoint& timeoutTime) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_until(timeoutTime);
#else
		(void)timeoutTime;
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::recursive_timed_mutex mutex{};
#endif
};

class SharedTimedMutex {
public:
	constexpr SharedTimedMutex() noexcept = default;

	SharedTimedMutex(const SharedTimedMutex&) = delete;
	SharedTimedMutex& operator=(const SharedTimedMutex&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock();
#else
		return true;
#endif
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_for(timeoutDuration);
#else
		(void)timeoutDuration;
		return true;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(const TimePoint& timeoutTime) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_until(timeoutTime);
#else
		(void)timeoutTime;
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock();
#endif
	}

	GREM_ALWAYS_INLINE void lock_shared() {
#ifdef GREM_USE_MULTITHREADING
		mutex.lock_shared();
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_shared() {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_shared();
#else
		return true;
#endif
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_shared_for(const DurationBase<Rep, Period>& timeoutDuration) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_shared_for(timeoutDuration);
#else
		(void)timeoutDuration;
		return true;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_shared_until(const TimePoint& timeoutTime) {
#ifdef GREM_USE_MULTITHREADING
		return mutex.try_lock_shared_until(timeoutTime);
#else
		(void)timeoutTime;
		return true;
#endif
	}

	GREM_ALWAYS_INLINE void unlock_shared() {
#ifdef GREM_USE_MULTITHREADING
		mutex.unlock_shared();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::shared_timed_mutex mutex{};
#endif
};

template <typename Lockable1, typename Lockable2, typename... LockableN>
GREM_ALWAYS_INLINE void lock(Lockable1& lock1, Lockable2& lock2, LockableN&... lockN) {
#ifdef GREM_USE_MULTITHREADING
	std::lock(lock1, lock2, lockN...);
#else
	bool locked[2 + sizeof...(LockableN)]{};
	try {
		lock1.lock();
		locked[0] = true;
		lock2.lock();
		locked[1] = true;
		std::size_t i = 2;
		((lockN.lock(), (locked[i++] = true)), ...);
	} catch (...) {
		if (locked[0]) {
			lock1.unlock();
		}
		if (locked[1]) {
			lock2.unlock();
		}
		std::size_t i = 2;
		((locked[i++] && (lockN.unlock(), false)), ...);
		throw;
	}
#endif
}

struct DeferLock {
	explicit DeferLock() = default;
};
inline constexpr DeferLock DEFER_LOCK{};

struct TryToLock {
	explicit TryToLock() = default;
};
inline constexpr TryToLock TRY_TO_LOCK{};

struct AdoptLock {
	explicit AdoptLock() = default;
};
inline constexpr AdoptLock ADOPT_LOCK{};

template <typename... MutexTypes>
class ScopedLock {
public:
#ifdef GREM_USE_MULTITHREADING
	GREM_ALWAYS_INLINE explicit ScopedLock(MutexTypes&... mutexes)
		: implementation(mutexes...) {}

	GREM_ALWAYS_INLINE ScopedLock(AdoptLock, MutexTypes&... mutexes)
		: implementation(std::adopt_lock, mutexes...) {}
#else
	GREM_ALWAYS_INLINE explicit ScopedLock(MutexTypes&... mutexes)
		: mutexes(mutexes...) {
		lock(mutexes...);
	}

	GREM_ALWAYS_INLINE ScopedLock(AdoptLock, MutexTypes&... mutexes)
		: mutexes(mutexes...) {}

	GREM_ALWAYS_INLINE ~ScopedLock() {
		[&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
			(get<Indices>(mutexes).unlock(), ...);
		}(std::make_index_sequence<sizeof...(MutexTypes)>{});
	}
#endif

	ScopedLock(const ScopedLock&) = delete;
	ScopedLock& operator=(const ScopedLock&) = delete;

private:
#ifdef GREM_USE_MULTITHREADING
	std::scoped_lock<MutexTypes...> implementation;
#else
	Tuple<MutexTypes&...> mutexes;
#endif
};

template <typename MutexType>
class ScopedLock<MutexType> {
public:
	using mutex_type = MutexType;

#ifdef GREM_USE_MULTITHREADING
	GREM_ALWAYS_INLINE explicit ScopedLock(mutex_type& mutex)
		: implementation(mutex) {}

	GREM_ALWAYS_INLINE ScopedLock(AdoptLock, mutex_type& mutex)
		: implementation(std::adopt_lock, mutex) {}
#else
	GREM_ALWAYS_INLINE explicit ScopedLock(mutex_type& mutex)
		: mutex(mutex) {
		mutex.lock();
	}

	GREM_ALWAYS_INLINE ScopedLock(AdoptLock, mutex_type& mutex)
		: mutex(mutex) {}

	GREM_ALWAYS_INLINE ~ScopedLock() {
		mutex.unlock();
	}
#endif

	ScopedLock(const ScopedLock&) = delete;
	ScopedLock& operator=(const ScopedLock&) = delete;

private:
#ifdef GREM_USE_MULTITHREADING
	std::scoped_lock<MutexType> implementation;
#else
	mutex_type& mutex;
#endif
};

template <typename MutexType>
class UniqueLock {
public:
	using mutex_type = MutexType;

	UniqueLock() noexcept = default;
	UniqueLock(const UniqueLock&) = delete;

#ifdef GREM_USE_MULTITHREADING
	UniqueLock(UniqueLock&& other) noexcept = default;

	GREM_ALWAYS_INLINE explicit UniqueLock(mutex_type& mutex)
		: implementation(mutex) {}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, DeferLock) noexcept
		: implementation(mutex, std::defer_lock) {}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, TryToLock)
		: implementation(mutex, std::try_to_lock) {}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, AdoptLock)
		: implementation(mutex, std::adopt_lock) {}

	template <typename Rep, typename Period>
	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, const DurationBase<Rep, Period>& timeoutDuration)
		: implementation(mutex, timeoutDuration) {}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, const TimePoint& timeoutTime)
		: implementation(mutex, timeoutTime) {}

	UniqueLock& operator=(UniqueLock&& other) = default;
#else
	GREM_ALWAYS_INLINE UniqueLock(UniqueLock&& other) noexcept
		: associatedMutex(std::exchange(other.associatedMutex, nullptr))
		, locked(std::exchange(other.locked, false)) {}

	GREM_ALWAYS_INLINE explicit UniqueLock(mutex_type& mutex)
		: associatedMutex(&mutex) {
		mutex.lock();
		locked = true; // NOLINT(cppcoreguidelines-prefer-member-initializer)
	}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, DeferLock) noexcept
		: associatedMutex(&mutex) {}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, TryToLock)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock(); }) {
			locked = mutex.try_lock();
		}
	}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, AdoptLock)
		: associatedMutex(&mutex)
		, locked(true) {}

	template <typename Rep, typename Period>
	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, const DurationBase<Rep, Period>& timeoutDuration)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock_for(timeoutDuration); }) {
			locked = mutex.try_lock_for(timeoutDuration);
		} else if constexpr (requires { mutex.try_lock(); }) {
			locked = mutex.try_lock();
		}
	}

	GREM_ALWAYS_INLINE UniqueLock(mutex_type& mutex, const TimePoint& timeoutTime)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock_until(timeoutTime); }) {
			locked = mutex.try_lock_until(timeoutTime);
		} else if constexpr (requires { mutex.try_lock(); }) {
			locked = mutex.try_lock();
		}
	}

	GREM_ALWAYS_INLINE ~UniqueLock() {
		if (locked) {
			associatedMutex->unlock();
		}
	}

	GREM_ALWAYS_INLINE UniqueLock& operator=(UniqueLock&& other) { // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		if (this == &other) {
			return *this;
		}
		associatedMutex = std::exchange(other.associatedMutex, nullptr);
		locked = std::exchange(other.locked, false);
		return *this;
	}
#endif

	UniqueLock& operator=(const UniqueLock&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		implementation.lock();
#else
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		associatedMutex->lock();
		locked = true;
#endif
	}

#ifdef GREM_USE_MULTITHREADING
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() requires(requires(std::unique_lock<MutexType> l) { l.try_lock(); }) {
		return implementation.try_lock();
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration)
		requires(requires(std::unique_lock<MutexType> l) { l.try_lock_for(timeoutDuration); }) {
		return implementation.try_lock_for(timeoutDuration);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(TimePoint timeoutTime) requires(requires(std::unique_lock<MutexType> l) { l.try_lock_until(timeoutTime); }) {
		return implementation.try_lock_until(timeoutTime);
	}
#else
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() requires(requires(mutex_type m) { m.try_lock(); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock();
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration) requires(requires(mutex_type m) { m.try_lock_for(timeoutDuration); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock_for(timeoutDuration);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(TimePoint timeoutTime) requires(requires(mutex_type m) { m.try_lock_until(timeoutTime); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock_until(timeoutTime);
	}
#endif

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		implementation.unlock();
#else
		if (!associatedMutex || !locked) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		associatedMutex->unlock();
		locked = false;
#endif
	}

	GREM_ALWAYS_INLINE void swap(UniqueLock& other) noexcept {
#ifdef GREM_USE_MULTITHREADING
		implementation.swap(other.implementation);
#else
		std::swap(associatedMutex, other.associatedMutex);
		std::swap(locked, other.locked);
#endif
	}

	GREM_ALWAYS_INLINE friend void swap(UniqueLock& a, UniqueLock& b) noexcept {
		a.swap(b);
	}

	GREM_ALWAYS_INLINE mutex_type* release() noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.release();
#else
		locked = false;
		return std::exchange(associatedMutex, nullptr);
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE mutex_type* mutex() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.mutex();
#else
		return associatedMutex;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool owns_lock() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.owns_lock();
#else
		return associatedMutex && locked;
#endif
	}

	GREM_ALWAYS_INLINE explicit operator bool() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return static_cast<bool>(implementation);
#else
		return owns_lock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::unique_lock<MutexType> implementation;
#else
	mutex_type* associatedMutex = nullptr;
	bool locked = false;
#endif
};

template <typename MutexType>
class SharedLock {
public:
	using mutex_type = MutexType;

	SharedLock() noexcept = default;
	SharedLock(const SharedLock&) = delete;

#ifdef GREM_USE_MULTITHREADING
	SharedLock(SharedLock&& other) noexcept = default;

	GREM_ALWAYS_INLINE explicit SharedLock(mutex_type& mutex)
		: implementation(mutex) {}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, DeferLock) noexcept
		: implementation(mutex, std::defer_lock) {}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, TryToLock)
		: implementation(mutex, std::try_to_lock) {}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, AdoptLock)
		: implementation(mutex, std::adopt_lock) {}

	template <typename Rep, typename Period>
	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, const DurationBase<Rep, Period>& timeoutDuration)
		: implementation(mutex, timeoutDuration) {}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, const TimePoint& timeoutTime)
		: implementation(mutex, timeoutTime) {}

	SharedLock& operator=(SharedLock&& other) = default;
#else
	GREM_ALWAYS_INLINE SharedLock(SharedLock&& other) noexcept
		: associatedMutex(std::exchange(other.associatedMutex, nullptr))
		, locked(std::exchange(other.locked, false)) {}

	GREM_ALWAYS_INLINE explicit SharedLock(mutex_type& mutex)
		: associatedMutex(&mutex) {
		mutex.lock_shared();
		locked = true; // NOLINT(cppcoreguidelines-prefer-member-initializer)
	}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, DeferLock) noexcept
		: associatedMutex(&mutex) {}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, TryToLock)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock_shared(); }) {
			locked = mutex.try_lock_shared();
		}
	}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, AdoptLock)
		: associatedMutex(&mutex)
		, locked(true) {}

	template <typename Rep, typename Period>
	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, const DurationBase<Rep, Period>& timeoutDuration)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock_shared_for(timeoutDuration); }) {
			locked = mutex.try_lock_shared_for(timeoutDuration);
		} else if constexpr (requires { mutex.try_lock_shared(); }) {
			locked = mutex.try_lock_shared();
		}
	}

	GREM_ALWAYS_INLINE SharedLock(mutex_type& mutex, const TimePoint& timeoutTime)
		: associatedMutex(&mutex) {
		if constexpr (requires { mutex.try_lock_shared_until(timeoutTime); }) {
			locked = mutex.try_lock_shared_until(timeoutTime);
		} else if constexpr (requires { mutex.try_lock_shared(); }) {
			locked = mutex.try_lock_shared();
		}
	}

	GREM_ALWAYS_INLINE ~SharedLock() {
		if (locked) {
			associatedMutex->unlock_shared();
		}
	}

	GREM_ALWAYS_INLINE SharedLock& operator=(SharedLock&& other) { // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		if (this == &other) {
			return *this;
		}
		associatedMutex = std::exchange(other.associatedMutex, nullptr);
		locked = std::exchange(other.locked, false);
		return *this;
	}
#endif

	SharedLock& operator=(const SharedLock&) = delete;

	GREM_ALWAYS_INLINE void lock() {
#ifdef GREM_USE_MULTITHREADING
		implementation.lock();
#else
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		associatedMutex->lock_shared();
		locked = true;
#endif
	}

#ifdef GREM_USE_MULTITHREADING
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() requires(requires(std::shared_lock<MutexType> l) { l.try_lock(); }) {
		return implementation.try_lock();
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration)
		requires(requires(std::shared_lock<MutexType> l) { l.try_lock_for(timeoutDuration); }) {
		return implementation.try_lock_for(timeoutDuration);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(TimePoint timeoutTime) requires(requires(std::shared_lock<MutexType> l) { l.try_lock_until(timeoutTime); }) {
		return implementation.try_lock_until(timeoutTime);
	}
#else
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock() requires(requires(mutex_type m) { m.try_lock_shared(); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock_shared();
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_for(const DurationBase<Rep, Period>& timeoutDuration)
		requires(requires(mutex_type m) { m.try_lock_shared_for(timeoutDuration); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock_shared_for(timeoutDuration);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool try_lock_until(TimePoint timeoutTime) requires(requires(mutex_type m) { m.try_lock_shared_until(timeoutTime); }) {
		if (!associatedMutex) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		if (locked) {
			throw std::system_error{make_error_code(std::errc::resource_deadlock_would_occur)};
		}
		return locked = associatedMutex->try_lock_shared_until(timeoutTime);
	}
#endif

	GREM_ALWAYS_INLINE void unlock() {
#ifdef GREM_USE_MULTITHREADING
		implementation.unlock();
#else
		if (!associatedMutex || !locked) {
			throw std::system_error{make_error_code(std::errc::operation_not_permitted)};
		}
		associatedMutex->unlock_shared();
		locked = false;
#endif
	}

	GREM_ALWAYS_INLINE void swap(SharedLock& other) noexcept {
#ifdef GREM_USE_MULTITHREADING
		implementation.swap(other.implementation);
#else
		std::swap(associatedMutex, other.associatedMutex);
		std::swap(locked, other.locked);
#endif
	}

	GREM_ALWAYS_INLINE friend void swap(SharedLock& a, SharedLock& b) noexcept {
		a.swap(b);
	}

	GREM_ALWAYS_INLINE mutex_type* release() noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.release();
#else
		locked = false;
		return std::exchange(associatedMutex, nullptr);
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE mutex_type* mutex() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.mutex();
#else
		return associatedMutex;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool owns_lock() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return implementation.owns_lock();
#else
		return associatedMutex && locked;
#endif
	}

	GREM_ALWAYS_INLINE explicit operator bool() const noexcept {
#ifdef GREM_USE_MULTITHREADING
		return static_cast<bool>(implementation);
#else
		return owns_lock();
#endif
	}

private:
#ifdef GREM_USE_MULTITHREADING
	std::shared_lock<MutexType> implementation;
#else
	mutex_type* associatedMutex = nullptr;
	bool locked = false;
#endif
};

} // namespace grem

#endif
