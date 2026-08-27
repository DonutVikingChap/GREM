// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_SHARED_POINTER_HPP
#define GREM_CORE_DATA_SHARED_POINTER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/system/synchronization.hpp>

#include <algorithm>   // std::max
#include <cstddef>     // std::size_t, std::ptrdiff_t, std::byte, std::nullptr_t
#include <functional>  // std::hash
#include <memory>      // std::construct_at, std::destroy_at, std::destroy_n, std::uninitialized_default_construct_n
#include <new>         // std::launder, std::align_val_t
#include <type_traits> // std::is_..._v, std::remove_..._t
#include <utility>     // std::move, std::forward, std::exchange, std::swap

namespace grem {

namespace detail {

struct alignas(std::max(std::size_t{__STDCPP_DEFAULT_NEW_ALIGNMENT__}, std::size_t{alignof(std::max_align_t)})) ControlBlockHeader {
	Atomic<std::size_t> strongReferenceCount = 1;
	Atomic<std::size_t> totalReferenceCount = 1;
	std::size_t elementCount;
	void (*destroyObject)(void* object, std::size_t elementCount) noexcept;

	explicit ControlBlockHeader(std::size_t elementCount, void (*destroyObject)(void* object, std::size_t elementCount) noexcept) noexcept
		: elementCount(elementCount)
		, destroyObject(destroyObject) {}

	void incrementStrongReferenceCount() {
		strongReferenceCount.fetch_add(1, MemoryOrder::RELAXED);
	}

	void incrementTotalReferenceCount() {
		totalReferenceCount.fetch_add(1, MemoryOrder::RELAXED);
	}

	[[nodiscard]] bool decrementStrongReferenceCount() {
		return strongReferenceCount.fetch_sub(1, MemoryOrder::ACQUIRE_RELEASE) == 1;
	}

	[[nodiscard]] bool decrementTotalReferenceCount() {
		return totalReferenceCount.fetch_sub(1, MemoryOrder::ACQUIRE_RELEASE) == 1;
	}

	[[nodiscard]] bool acquireStrongReference() {
		return strongReferenceCount.fetch_add(1, MemoryOrder::ACQUIRE) > 0;
	}

	[[nodiscard]] std::size_t getApproximateStrongReferenceCount() const {
		return strongReferenceCount.load(MemoryOrder::RELAXED);
	}
};

} // namespace detail

template <typename T>
class WeakPointer;

template <typename T>
class SharedPointer {
public:
	using element_type = T;
	using weak_type = WeakPointer<T>;

	template <typename... Args>
	[[nodiscard]] static SharedPointer create(Args&&... args) {
		if constexpr (alignof(T) <= alignof(detail::ControlBlockHeader)) {
			void* const storage = operator new[](sizeof(detail::ControlBlockHeader) + sizeof(T), static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			detail::ControlBlockHeader* const controlBlock =
				std::construct_at(static_cast<detail::ControlBlockHeader*>(storage), 1, [](void* object, std::size_t) noexcept -> void { //
					std::destroy_at(static_cast<T*>(object));
				});
			try {
				T* const object = std::construct_at(reinterpret_cast<T*>(static_cast<std::byte*>(storage) + sizeof(detail::ControlBlockHeader)), std::forward<Args>(args)...);
				return SharedPointer{controlBlock, object};
			} catch (...) {
				std::destroy_at(controlBlock);
				operator delete[](storage, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
				throw;
			}
		} else {
			void* const storage = operator new[](sizeof(detail::ControlBlockHeader), static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			detail::ControlBlockHeader* const controlBlock =
				std::construct_at(static_cast<detail::ControlBlockHeader*>(storage), 1, [](void* object, std::size_t) noexcept -> void {
					delete static_cast<T*>(object); // NOLINT(cppcoreguidelines-owning-memory)
				});
			try {
				T* const object = new T(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
				return SharedPointer{controlBlock, object};
			} catch (...) {
				std::destroy_at(controlBlock);
				operator delete[](storage, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
				throw;
			}
		}
	}

	SharedPointer() noexcept = default;

	SharedPointer(std::nullptr_t) noexcept
		: SharedPointer() {}

	SharedPointer(const SharedPointer& other) noexcept
		: SharedPointer(other, other.object) {}

	template <typename U>
	SharedPointer(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: SharedPointer(other, other.object) {}

	SharedPointer(SharedPointer&& other) noexcept
		: SharedPointer(std::move(other), std::exchange(other.object, nullptr)) {}

	template <typename U>
	SharedPointer(SharedPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: SharedPointer(std::move(other), std::exchange(other.object, nullptr)) {}

	~SharedPointer() {
		reset();
	}

	SharedPointer& operator=(const SharedPointer& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	SharedPointer& operator=(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	SharedPointer& operator=(SharedPointer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	template <typename U>
	SharedPointer& operator=(SharedPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	void swap(SharedPointer& other) noexcept {
		std::swap(controlBlock, other.controlBlock);
		std::swap(object, other.object);
	}

	friend void swap(SharedPointer& a, SharedPointer& b) noexcept {
		a.swap(b);
	}

	void reset() noexcept {
		if (controlBlock) {
			if (controlBlock->decrementStrongReferenceCount()) {
				controlBlock->destroyObject(object, controlBlock->elementCount);
			}
			if (controlBlock->decrementTotalReferenceCount()) {
				GREM_ASSERT(controlBlock->getApproximateStrongReferenceCount() == 0);
				std::destroy_at(controlBlock);
				operator delete[](controlBlock, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			}
			controlBlock = nullptr;
			object = nullptr;
		}
	}

	[[nodiscard]] std::size_t use_count() const noexcept {
		return (controlBlock) ? controlBlock->getApproximateStrongReferenceCount() : 0;
	}

	[[nodiscard]] T* get() const noexcept {
		return object;
	}

	explicit operator bool() const noexcept {
		return object != nullptr;
	}

	[[nodiscard]] decltype(auto) operator*() const noexcept requires(!std::is_void_v<T>) {
		GREM_ASSERT(object);
		return *object;
	}

	[[nodiscard]] T* operator->() const noexcept {
		GREM_ASSERT(object);
		return object;
	}

	[[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
		return object == nullptr;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const SharedPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const SharedPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> static_pointer_cast(SharedPointer&& p) noexcept {
		return staticPointerCastImplementation<U>(std::move(p));
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> static_pointer_cast(const SharedPointer& p) noexcept {
		return staticPointerCastImplementation<U>(p);
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> dynamic_pointer_cast(SharedPointer&& p) noexcept {
		return dynamicPointerCastImplementation<U>(std::move(p));
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> dynamic_pointer_cast(const SharedPointer& p) noexcept {
		return dynamicPointerCastImplementation<U>(p);
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> const_pointer_cast(SharedPointer&& p) noexcept {
		return constPointerCastImplementation<U>(std::move(p));
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> const_pointer_cast(const SharedPointer& p) noexcept {
		return constPointerCastImplementation<U>(p);
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> reinterpret_pointer_cast(SharedPointer&& p) noexcept {
		return reinterpretPointerCastImplementation<U>(std::move(p));
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> reinterpret_pointer_cast(const SharedPointer& p) noexcept {
		return reinterpretPointerCastImplementation<U>(p);
	}

private:
	template <typename U>
	friend class SharedPointer;

	template <typename U>
	friend class WeakPointer;

	template <typename U>
	[[nodiscard]] static SharedPointer<U> staticPointerCastImplementation(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), static_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> staticPointerCastImplementation(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, static_cast<U*>(p.object)};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> dynamicPointerCastImplementation(SharedPointer&& p) noexcept {
		if (U* const u = dynamic_cast<U*>(std::exchange(p.object, nullptr))) {
			return SharedPointer<U>{std::move(p), u};
		}
		return {};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> dynamicPointerCastImplementation(const SharedPointer& p) noexcept {
		if (U* const u = dynamic_cast<U*>(p.object)) {
			return SharedPointer<U>{p, u};
		}
		return {};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> constPointerCastImplementation(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), const_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> constPointerCastImplementation(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, const_cast<U*>(p.object)};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> reinterpretPointerCastImplementation(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), reinterpret_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] static SharedPointer<U> reinterpretPointerCastImplementation(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, reinterpret_cast<U*>(p.object)};
	}

	SharedPointer(detail::ControlBlockHeader* controlBlock, T* object) noexcept
		: controlBlock(controlBlock)
		, object(object) {}

	template <typename U>
	SharedPointer(const SharedPointer<U>& other, T* object) noexcept
		: SharedPointer(other.controlBlock, object) {
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
	}

	template <typename U>
	SharedPointer(SharedPointer<U>&& other, T* object) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: SharedPointer(std::exchange(other.controlBlock, nullptr), object) {}

	detail::ControlBlockHeader* controlBlock = nullptr;
	T* object = nullptr;
};

template <typename T>
class SharedPointer<T[]> {
public:
	using element_type = T;
	using weak_type = WeakPointer<T[]>;

	[[nodiscard]] static SharedPointer create(std::size_t size) {
		if constexpr (alignof(T) <= alignof(detail::ControlBlockHeader)) {
			void* const storage = operator new[](sizeof(detail::ControlBlockHeader) + sizeof(T) * size, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			detail::ControlBlockHeader* const controlBlock =
				std::construct_at(static_cast<detail::ControlBlockHeader*>(storage), size, [](void* object, std::size_t elementCount) noexcept -> void { //
					std::destroy_n(static_cast<T*>(object), elementCount);
				});
			try {
				T* const object = reinterpret_cast<T*>(static_cast<std::byte*>(storage) + sizeof(detail::ControlBlockHeader));
				std::uninitialized_default_construct_n(object, size);
				return SharedPointer{controlBlock, std::launder(object)};
			} catch (...) {
				std::destroy_at(controlBlock);
				operator delete[](storage, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
				throw;
			}
		} else {
			void* const storage = operator new[](sizeof(detail::ControlBlockHeader), static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			detail::ControlBlockHeader* const controlBlock =
				std::construct_at(static_cast<detail::ControlBlockHeader*>(storage), size, [](void* object, std::size_t) noexcept -> void {
					delete[] static_cast<T*>(object); // NOLINT(cppcoreguidelines-owning-memory)
				});
			try {
				T* const object = new T[size]; // NOLINT(cppcoreguidelines-owning-memory)
				return SharedPointer{controlBlock, object};
			} catch (...) {
				std::destroy_at(controlBlock);
				operator delete[](storage, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
				throw;
			}
		}
	}

	SharedPointer() noexcept = default;

	SharedPointer(std::nullptr_t) noexcept
		: SharedPointer() {}

	SharedPointer(const SharedPointer& other) noexcept
		: SharedPointer(other, other.object) {}

	template <typename U>
	SharedPointer(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: SharedPointer(other, other.object) {}

	SharedPointer(SharedPointer&& other) noexcept
		: SharedPointer(std::move(other), std::exchange(other.object, nullptr)) {}

	template <typename U>
	SharedPointer(SharedPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: SharedPointer(std::move(other), std::exchange(other.object, nullptr)) {}

	~SharedPointer() {
		reset();
	}

	SharedPointer& operator=(const SharedPointer& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	SharedPointer& operator=(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	SharedPointer& operator=(SharedPointer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	template <typename U>
	SharedPointer& operator=(SharedPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	void swap(SharedPointer& other) noexcept {
		std::swap(controlBlock, other.controlBlock);
		std::swap(object, other.object);
	}

	friend void swap(SharedPointer& a, SharedPointer& b) noexcept {
		a.swap(b);
	}

	void reset() noexcept {
		if (controlBlock) {
			if (controlBlock->decrementStrongReferenceCount()) {
				controlBlock->destroyObject(object, controlBlock->elementCount);
			}
			if (controlBlock->decrementTotalReferenceCount()) {
				GREM_ASSERT(controlBlock->getApproximateStrongReferenceCount() == 0);
				std::destroy_at(controlBlock);
				operator delete[](controlBlock, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			}
			controlBlock = nullptr;
			object = nullptr;
		}
	}

	[[nodiscard]] std::size_t use_count() const noexcept {
		return (controlBlock) ? controlBlock->getApproximateStrongReferenceCount() : 0;
	}

	[[nodiscard]] T* get() const noexcept {
		return object;
	}

	explicit operator bool() const noexcept {
		return object != nullptr;
	}

	[[nodiscard]] decltype(auto) operator[](std::ptrdiff_t index) const noexcept requires(!std::is_void_v<T>) {
		GREM_ASSERT(object);
		return object[index];
	}

	[[nodiscard]] std::size_t size() const noexcept {
		return (controlBlock) ? controlBlock->elementCount : 0;
	}

	[[nodiscard]] bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
		return object == nullptr;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const SharedPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const SharedPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> static_pointer_cast(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), static_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> static_pointer_cast(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, static_cast<U*>(p.object)};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> dynamic_pointer_cast(SharedPointer&& p) noexcept {
		if (U* const u = dynamic_cast<U*>(std::exchange(p.object, nullptr))) {
			return SharedPointer<U>{std::move(p), u};
		}
		return {};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> dynamic_pointer_cast(const SharedPointer& p) noexcept {
		if (U* const u = dynamic_cast<U*>(p.object)) {
			return SharedPointer<U>{p, u};
		}
		return {};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> const_pointer_cast(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), const_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> const_pointer_cast(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, const_cast<U*>(p.object)};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> reinterpret_pointer_cast(SharedPointer&& p) noexcept {
		return SharedPointer<U>{std::move(p), reinterpret_cast<U*>(std::exchange(p.object, nullptr))};
	}

	template <typename U>
	[[nodiscard]] friend SharedPointer<U> reinterpret_pointer_cast(const SharedPointer& p) noexcept {
		return SharedPointer<U>{p, reinterpret_cast<U*>(p.object)};
	}

private:
	template <typename U>
	friend class SharedPointer;

	template <typename U>
	friend class WeakPointer;

	SharedPointer(detail::ControlBlockHeader* controlBlock, T* object) noexcept
		: controlBlock(controlBlock)
		, object(object) {}

	template <typename U>
	SharedPointer(const SharedPointer<U>& other, T* object) noexcept
		: SharedPointer(other.controlBlock, object) {
		if (controlBlock) {
			controlBlock->incrementStrongReferenceCount();
			controlBlock->incrementTotalReferenceCount();
		}
	}

	template <typename U>
	SharedPointer(SharedPointer<U>&& other, T* object) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: SharedPointer(std::exchange(other.controlBlock, nullptr), object) {}

	detail::ControlBlockHeader* controlBlock = nullptr;
	T* object = nullptr;
};

template <typename T>
class WeakPointer {
public:
	using element_type = T;

	WeakPointer() noexcept = default;

	template <typename U>
	WeakPointer(const SharedPointer<U>& shared) noexcept requires(std::is_convertible_v<U*, T*>)
		: controlBlock(shared.controlBlock)
		, object(shared.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	WeakPointer(const WeakPointer& other) noexcept
		: controlBlock(other.controlBlock)
		, object(other.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	template <typename U>
	WeakPointer(const WeakPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: controlBlock(other.controlBlock)
		, object(other.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	WeakPointer(WeakPointer&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: controlBlock(std::exchange(other.controlBlock, nullptr))
		, object(std::exchange(other.object, nullptr)) {}

	template <typename U>
	WeakPointer(WeakPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: controlBlock(std::exchange(other.controlBlock, nullptr))
		, object(std::exchange(other.object, nullptr)) {}

	~WeakPointer() {
		reset();
	}

	WeakPointer& operator=(const WeakPointer& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(const WeakPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	WeakPointer& operator=(WeakPointer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(WeakPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	void swap(WeakPointer& other) noexcept {
		std::swap(controlBlock, other.controlBlock);
		std::swap(object, other.object);
	}

	friend void swap(WeakPointer& a, WeakPointer& b) noexcept {
		a.swap(b);
	}

	void reset() noexcept {
		if (controlBlock) {
			if (controlBlock->decrementTotalReferenceCount()) {
				GREM_ASSERT(controlBlock->getApproximateStrongReferenceCount() == 0);
				std::destroy_at(controlBlock);
				operator delete[](controlBlock, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			}
			controlBlock = nullptr;
			object = nullptr;
		}
	}

	[[nodiscard]] std::size_t use_count() const noexcept {
		return (controlBlock) ? controlBlock->getApproximateStrongReferenceCount() : 0;
	}

	[[nodiscard]] bool expired() const noexcept {
		return use_count() == 0;
	}

	[[nodiscard]] SharedPointer<T> lock() const noexcept {
		if (controlBlock && controlBlock->acquireStrongReference()) {
			controlBlock->incrementTotalReferenceCount();
			return SharedPointer<T>{controlBlock, object};
		}
		return {};
	}

	[[nodiscard]] T* get() const noexcept {
		return object;
	}

	explicit operator bool() const noexcept {
		return object != nullptr;
	}

	[[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
		return object == nullptr;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const WeakPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const SharedPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const WeakPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const SharedPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

private:
	template <typename U>
	friend class WeakPointer;

	detail::ControlBlockHeader* controlBlock = nullptr;
	T* object = nullptr;
};

template <typename T>
class WeakPointer<T[]> {
public:
	using element_type = T;

	WeakPointer() noexcept = default;

	template <typename U>
	WeakPointer(const SharedPointer<U>& shared) noexcept requires(std::is_convertible_v<U*, T*>)
		: controlBlock(shared.controlBlock)
		, object(shared.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	WeakPointer(const WeakPointer& other) noexcept
		: controlBlock(other.controlBlock)
		, object(other.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	template <typename U>
	WeakPointer(const WeakPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>)
		: controlBlock(other.controlBlock)
		, object(other.object) {
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
	}

	WeakPointer(WeakPointer&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: controlBlock(std::exchange(other.controlBlock, nullptr))
		, object(std::exchange(other.object, nullptr)) {}

	template <typename U>
	WeakPointer(WeakPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: controlBlock(std::exchange(other.controlBlock, nullptr))
		, object(std::exchange(other.object, nullptr)) {}

	~WeakPointer() {
		reset();
	}

	WeakPointer& operator=(const WeakPointer& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(const SharedPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(const WeakPointer<U>& other) noexcept requires(std::is_convertible_v<U*, T*>) {
		reset();
		controlBlock = other.controlBlock;
		object = other.object;
		if (controlBlock) {
			controlBlock->incrementTotalReferenceCount();
		}
		return *this;
	}

	WeakPointer& operator=(WeakPointer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	template <typename U>
	WeakPointer& operator=(WeakPointer<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		reset();
		controlBlock = std::exchange(other.controlBlock, nullptr);
		object = std::exchange(other.object, nullptr);
		return *this;
	}

	void swap(WeakPointer& other) noexcept {
		std::swap(controlBlock, other.controlBlock);
		std::swap(object, other.object);
	}

	friend void swap(WeakPointer& a, WeakPointer& b) noexcept {
		a.swap(b);
	}

	void reset() noexcept {
		if (controlBlock) {
			if (controlBlock->decrementTotalReferenceCount()) {
				GREM_ASSERT(controlBlock->getApproximateStrongReferenceCount() == 0);
				std::destroy_at(controlBlock);
				operator delete[](controlBlock, static_cast<std::align_val_t>(alignof(detail::ControlBlockHeader)));
			}
			controlBlock = nullptr;
			object = nullptr;
		}
	}

	[[nodiscard]] std::size_t use_count() const noexcept {
		return (controlBlock) ? controlBlock->getApproximateStrongReferenceCount() : 0;
	}

	[[nodiscard]] bool expired() const noexcept {
		return use_count() == 0;
	}

	[[nodiscard]] SharedPointer<T> lock() const noexcept {
		if (controlBlock && controlBlock->acquireStrongReference()) {
			controlBlock->incrementTotalReferenceCount();
			return SharedPointer<T>{controlBlock, object};
		}
		return {};
	}

	[[nodiscard]] T* get() const noexcept {
		return object;
	}

	explicit operator bool() const noexcept {
		return object != nullptr;
	}

	[[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
		return object == nullptr;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const WeakPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] bool operator==(const SharedPointer<U>& other) const noexcept {
		return object == other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const WeakPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

	template <typename U>
	[[nodiscard]] auto operator<=>(const SharedPointer<U>& other) const noexcept {
		return object <=> other.object;
	}

private:
	template <typename U>
	friend class WeakPointer;

	detail::ControlBlockHeader* controlBlock = nullptr;
	T* object = nullptr;
};

} // namespace grem

template <typename T>
struct std::hash<grem::SharedPointer<T>> {
	[[nodiscard]] std::size_t operator()(const grem::SharedPointer<T>& p) const {
		return hasher(p.get());
	}

private:
	[[no_unique_address]] std::hash<std::remove_const_t<T>*> hasher;
};

template <typename T>
struct std::hash<grem::WeakPointer<T>> {
	[[nodiscard]] std::size_t operator()(const grem::WeakPointer<T>& p) const {
		return hasher(p.get());
	}

private:
	[[no_unique_address]] std::hash<std::remove_const_t<T>*> hasher;
};

#endif
