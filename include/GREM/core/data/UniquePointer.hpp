// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_UNIQUE_POINTER_HPP
#define GREM_CORE_DATA_UNIQUE_POINTER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/UniqueHandle.hpp>

#include <cstddef>     // std::size_t, std::nullptr_t
#include <type_traits> // std::is_same_v, std::is_convertible_v, std::is_void_v, std::remove_reference_t
#include <utility>     // std::move, std::forward

namespace grem {

namespace detail {

template <typename T, typename Deleter>
struct deleter_pointer_type {
	using type = T*;
};

template <typename T, typename Deleter>
requires(requires { typename Deleter::pointer; }) struct deleter_pointer_type<T, Deleter> {
	using type = typename Deleter::pointer;
};

}; // namespace detail

/**
 * Default deleter for UniquePointer<T>.
 *
 * Calls `delete` on the underlying pointer.
 */
template <typename T>
class DefaultDelete {
public:
	constexpr DefaultDelete() noexcept = default;

	template <typename U>
	constexpr DefaultDelete(const DefaultDelete<U>&) noexcept requires(std::is_convertible_v<U*, T*>) {}

	constexpr void operator()(T* handle) const noexcept {
		delete handle; // NOLINT(cppcoreguidelines-owning-memory)
	}
};

/**
 * Default deleter for UniquePointer<T[]>.
 *
 * Calls `delete[]` on the underlying pointer.
 */
template <typename T>
class DefaultDelete<T[]> {
public:
	constexpr DefaultDelete() noexcept = default;

	template <typename U>
	constexpr DefaultDelete(const DefaultDelete<U[]>&) noexcept requires(std::is_convertible_v<U (*)[], T (*)[]>) {}

	template <typename U>
	constexpr void operator()(U* handle) const noexcept requires(std::is_convertible_v<U (*)[], T (*)[]>) {
		delete[] handle; // NOLINT(cppcoreguidelines-owning-memory)
	}
};

/**
 * Nullable RAII data pointer with exclusive ownership of an object that is
 * automatically destroyed on pointer destruction.
 *
 * \tparam T the associated object type.
 * \tparam Deleter function object type that destroys the associated object when
 *         called with a non-null pointer. Passing nullptr must be a no-op.
 */
template <typename T, typename Deleter = DefaultDelete<T>>
class UniquePointer {
public:
	/** Pointer type. */
	using pointer = typename detail::deleter_pointer_type<T, std::remove_reference_t<Deleter>>::type;

	/** Object type. */
	using element_type = T;

	/** Deleter type. */
	using deleter_type = Deleter;

	/**
	 * Allocate and construct an object of the associated type and wrap it in a
	 * pointer with unique ownership.
	 *
	 * \param args arguments to forward to the constructor of the underlying
	 *             object.
	 *
	 * \return a non-null pointer that owns the constructed object.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the object's constructor.
	 */
	template <typename... Args>
	[[nodiscard]] static constexpr UniquePointer create(Args&&... args) {
		return UniquePointer{new T(std::forward<Args>(args)...)};
	}

	/**
	 * Construct a null pointer without an associated object.
	 */
	constexpr UniquePointer() noexcept = default;

	/**
	 * Construct a null pointer without an associated object.
	 *
	 * \param pointer null pointer.
	 */
	constexpr UniquePointer(std::nullptr_t pointer) noexcept {
		(void)pointer;
	}

	/**
	 * Construct a pointer from another pointer of a derived type, taking
	 * ownership of its underlying object.
	 *
	 * \param other other pointer whose object to take owership of.
	 */
	template <typename U, typename OtherDeleter>
	constexpr UniquePointer(UniquePointer<U, OtherDeleter>&& other) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires(!std::is_same_v<T, U> && std::is_convertible_v<typename UniquePointer<U, OtherDeleter>::pointer, pointer>)
		: handle(other.release(), std::move(other.get_deleter())) {}

	/**
	 * Construct a pointer that takes ownership of an existing object pointer.
	 *
	 * \param pointer the underlying pointer to take ownership of, or nullptr to
	 *        construct a null pointer without an associated object.
	 * \param deleter the deleter to use to destroy the object on destruction.
	 */
	constexpr explicit UniquePointer(pointer pointer, Deleter deleter = Deleter()) noexcept
		: handle(pointer, std::move(deleter)) {}

	/**
	 * Check if this pointer has an associated object, i.e. if it is not null.
	 *
	 * \return true if the pointer has an associated object, false if the
	 *         pointer is null.
	 */
	constexpr explicit operator bool() const noexcept {
		return static_cast<bool>(handle);
	}

	/**
	 * Compare this pointer against another for equality of the underlying
	 * address.
	 *
	 * \param other the pointer to compare this pointer to.
	 *
	 * \return true if the pointers are equal, false otherwise.
	 *
	 * \note This does not compare the values of any associated objects. It only
	 *       compares the values of the pointers themselves.
	 */
	[[nodiscard]] constexpr bool operator==(const UniquePointer& other) const noexcept = default;

	/**
	 * Check if this pointer is null and has no associated object.
	 *
	 * \param other the null pointer to compare this pointer to.
	 *
	 * \return true if the pointers are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(std::nullptr_t other) const noexcept {
		(void)other;
		return !handle;
	}

	/**
	 * Destroy the object associated with this pointer, if any, and take
	 * ownership of a new object pointer, which may be null.
	 *
	 * \param newPointer the new underlying pointer to take ownership of, or
	 *        nullptr to reset to a null pointer without an associated object.
	 *
	 * \sa release()
	 * \sa get()
	 */
	constexpr void reset(pointer newPointer = nullptr) noexcept {
		handle.reset(newPointer);
	}

	/**
	 * Relinquish ownership of the associated object.
	 *
	 * This pointer will be reset to null, without destroying the associated
	 * object.
	 *
	 * \return the pointer to the associated object that was released, or
	 *         nullptr if the pointer did not have an associated object.
	 *
	 * \warning After calling this function, the associated object will no
	 *          longer be destroyed automatically along with the pointer. It
	 *          instead becomes the responsibility of the caller to ensure that
	 *          the object is properly destroyed. If the intent is to reset the
	 *          pointer to null while destroying the associated object in the
	 *          process, use reset() instead.
	 *
	 * \sa reset()
	 * \sa get()
	 */
	constexpr pointer release() noexcept {
		return handle.release();
	}

	/**
	 * Get the underlying pointer.
	 *
	 * \return a non-owning copy of the underlying pointer.
	 *
	 * \sa reset()
	 * \sa release()
	 */
	[[nodiscard]] constexpr pointer get() const noexcept {
		return handle.get();
	}

	/**
	 * Get the current deleter.
	 *
	 * \return a reference to the current deleter.
	 */
	[[nodiscard]] constexpr Deleter& get_deleter() noexcept {
		return handle.get_deleter();
	}

	/**
	 * Get the current deleter.
	 *
	 * \return a read-only reference to the current deleter.
	 */
	[[nodiscard]] constexpr const Deleter& get_deleter() const noexcept {
		return handle.get_deleter();
	}

	/**
	 * Access the associated object by pointer.
	 *
	 * \return a non-owning pointer to the associated object.
	 *
	 * \warning If the pointer is null and has no associated object, the
	 *          behavior is undefined.
	 *
	 * \sa get()
	 */
	[[nodiscard]] constexpr pointer operator->() const noexcept {
		return handle.get();
	}

	/**
	 * Access the associated object.
	 *
	 * \return a reference to the associated object.
	 *
	 * \warning If the pointer is null and has no associated object, the
	 *          behavior is undefined.
	 *
	 * \sa get()
	 */
	[[nodiscard]] constexpr decltype(auto) operator*() const noexcept requires(!std::is_void_v<T>) {
		return *handle.get();
	}

private:
	UniqueHandle<pointer, Deleter, nullptr> handle{};
};

/**
 * Nullable RAII data pointer with exclusive ownership of an array that is
 * automatically destroyed on pointer destruction.
 *
 * \tparam T the element type of the associated array.
 * \tparam Deleter function object type that destroys the associated array when
 *         called with a non-null pointer. Passing nullptr must be a no-op.
 */
template <typename T, typename Deleter>
class UniquePointer<T[], Deleter> {
public:
	/** Pointer type. */
	using pointer = typename detail::deleter_pointer_type<T, std::remove_reference_t<Deleter>>::type;

	/** Object type. */
	using element_type = T;

	/** Deleter type. */
	using deleter_type = Deleter;

	/**
	 * Allocate an array of default-initialized elements of the associated type
	 * and wrap it in a pointer with unique ownership.
	 *
	 * \param size number of elements to allocate.
	 *
	 * \return a non-null pointer that owns the allocated array.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 * \throws std::bad_array_new_length if the given size is invalid.
	 * \throws any exception thrown by the element's default constructor.
	 */
	[[nodiscard]] static constexpr UniquePointer create(std::size_t size) {
		return UniquePointer{new T[size]};
	}

	/**
	 * Construct a null pointer without an associated array.
	 */
	constexpr UniquePointer() noexcept = default;

	/**
	 * Construct a null pointer without an associated array.
	 *
	 * \param pointer null pointer.
	 */
	constexpr UniquePointer(std::nullptr_t pointer) noexcept {
		(void)pointer;
	}

	/**
	 * Construct a pointer that takes ownership of an existing array.
	 *
	 * \param pointer the underlying array pointer to take ownership of, or
	 *        nullptr to construct a null pointer without an associated array.
	 * \param deleter the deleter to use to destroy the array on destruction.
	 */
	constexpr explicit UniquePointer(pointer pointer, Deleter deleter = Deleter()) noexcept
		: handle(pointer, std::move(deleter)) {}

	/**
	 * Check if this pointer has an associated array, i.e. if it is not null.
	 *
	 * \return true if the pointer has an associated array, false if the
	 *         pointer is null.
	 */
	constexpr explicit operator bool() const noexcept {
		return static_cast<bool>(handle);
	}

	/**
	 * Compare this pointer against another for equality of the underlying
	 * address.
	 *
	 * \param other the pointer to compare this pointer to.
	 *
	 * \return true if the pointers are equal, false otherwise.
	 *
	 * \note This does not compare the values of any associated arrays. It only
	 *       compares the values of the pointers themselves.
	 */
	[[nodiscard]] constexpr bool operator==(const UniquePointer& other) const noexcept = default;

	/**
	 * Check if this pointer is null and has no associated array.
	 *
	 * \param other the null pointer to compare this pointer to.
	 *
	 * \return true if the pointers are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(std::nullptr_t other) const noexcept {
		(void)other;
		return !handle;
	}

	/**
	 * Destroy the array associated with this pointer, if any, and take
	 * ownership of a new array, which may be null.
	 *
	 * \param newPointer the new underlying array pointer to take ownership of,
	 *        or nullptr to reset to a null pointer without an associated array.
	 *
	 * \sa release()
	 * \sa get()
	 */
	constexpr void reset(pointer newPointer = nullptr) noexcept {
		handle.reset(newPointer);
	}

	/**
	 * Relinquish ownership of the associated array.
	 *
	 * This pointer will be reset to null, without destroying the associated
	 * array.
	 *
	 * \return the pointer to the associated array that was released, or
	 *         nullptr if the pointer did not have an associated array.
	 *
	 * \warning After calling this function, the associated array will no
	 *          longer be destroyed automatically along with the pointer. It
	 *          instead becomes the responsibility of the caller to ensure that
	 *          the array is properly destroyed. If the intent is to reset the
	 *          pointer to null while destroying the associated array in the
	 *          process, use reset() instead.
	 *
	 * \sa reset()
	 * \sa get()
	 */
	constexpr pointer release() noexcept {
		return handle.release();
	}

	/**
	 * Get the underlying array pointer.
	 *
	 * \return a non-owning copy of the underlying array pointer.
	 *
	 * \sa reset()
	 * \sa release()
	 * \sa operator[]()
	 */
	[[nodiscard]] constexpr pointer get() const noexcept {
		return handle.get();
	}

	/**
	 * Get the current deleter.
	 *
	 * \return a reference to the current deleter.
	 */
	[[nodiscard]] constexpr Deleter& get_deleter() noexcept {
		return handle.get_deleter();
	}

	/**
	 * Get the current deleter.
	 *
	 * \return a read-only reference to the current deleter.
	 */
	[[nodiscard]] constexpr const Deleter& get_deleter() const noexcept {
		return handle.get_deleter();
	}

	/**
	 * Access an element of the associated array.
	 *
	 * \param index index of the array element to access.
	 *
	 * \return a reference to the given element of the associated array.
	 *
	 * \warning If the pointer is null and has no associated array, the
	 *          behavior is undefined.
	 *
	 * \sa get()
	 */
	[[nodiscard]] constexpr decltype(auto) operator[](std::size_t index) const noexcept requires(!std::is_void_v<T>) {
		return handle.get()[index];
	}

private:
	UniqueHandle<pointer, Deleter, nullptr> handle{};
};

} // namespace grem

#endif
