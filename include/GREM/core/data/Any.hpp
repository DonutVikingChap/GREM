// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ANY_HPP
#define GREM_CORE_DATA_ANY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>

#include <cstddef>     // std::size_t, std::byte
#include <exception>   // std::exception
#include <memory>      // std::construct_at, std::destroy_at
#include <new>         // std::launder
#include <type_traits> // std::decay_t, std::is_..._v
#include <utility>     // std::move, std::forward, std::in_place_type...

namespace grem {

/**
 * Exception type that is thrown on an attempt to erroneously access the
 * incorrect type of an Any wrapper when using a safe access function such as
 * Any::get().
 */
struct BadAnyAccess : std::exception {
	BadAnyAccess() noexcept = default;

	[[nodiscard]] const char* what() const noexcept override {
		return "Bad any access.";
	}
};

/**
 * Generic type-erased semi-regular value wrapper.
 *
 * \tparam Base generic base type, or void.
 * \tparam SmallCapacity maximum capacity for small object optimization.
 * \tparam SmallAlignment maximum alignment for small object optimization.
 */
template <typename Base, std::size_t SmallCapacity = sizeof(void*) * 3, std::size_t SmallAlignment = alignof(void*)>
class AnyBase {
private:
	struct FunctionTable {
		void (*destroy)(AnyBase& self) noexcept;
		void (*moveConstructOther)(AnyBase& self, AnyBase& other) noexcept;
		void (*copyConstructOther)(const AnyBase& self, AnyBase& other);
		void (*copyAssignOther)(const AnyBase& self, AnyBase& other);
		Base* (*getValuePointer)(AnyBase& self) noexcept;
	};

	template <typename T>
	static constexpr bool IS_SMALL = sizeof(T) <= SmallCapacity && alignof(T) <= SmallAlignment && std::is_nothrow_move_constructible_v<T>;

	template <typename T>
	static const FunctionTable* getFunctionTable() noexcept requires(IS_SMALL<T>) {
		static_assert(std::is_void_v<Base> || std::is_base_of_v<Base, T>);
		static constexpr FunctionTable FUNCTION_TABLE{
			.destroy = [](AnyBase& self) noexcept -> void { std::destroy_at(reinterpret_cast<T*>(self.smallObject)); },
			.moveConstructOther = [](AnyBase& self, AnyBase& other) noexcept -> void {
				std::construct_at(reinterpret_cast<T*>(other.smallObject), std::move(*std::launder(reinterpret_cast<T*>(self.smallObject))));
				other.functionTable = self.functionTable;
			},
			.copyConstructOther = [](const AnyBase& self, AnyBase& other) -> void {
				std::construct_at(reinterpret_cast<T*>(other.smallObject), *std::launder(reinterpret_cast<const T*>(self.smallObject)));
				other.functionTable = self.functionTable;
			},
			.copyAssignOther = [](const AnyBase& self, AnyBase& other) -> void {
				if (other.functionTable == &FUNCTION_TABLE) {
					*std::launder(reinterpret_cast<T*>(other.smallObject)) = *std::launder(reinterpret_cast<const T*>(self.smallObject));
				} else {
					if constexpr (std::is_nothrow_copy_constructible_v<T>) {
						other.functionTable->destroy(other);
						std::construct_at(reinterpret_cast<T*>(other.smallObject), *std::launder(reinterpret_cast<const T*>(self.smallObject)));
						other.functionTable = self.functionTable;
					} else {
						T temporary = *std::launder(reinterpret_cast<const T*>(self.smallObject));
						other.functionTable->destroy(other);
						std::construct_at(reinterpret_cast<T*>(other.smallObject), std::move(temporary));
						other.functionTable = self.functionTable;
					}
				}
			},
			.getValuePointer = [](AnyBase& self) noexcept -> Base* { return std::launder(reinterpret_cast<T*>(self.smallObject)); },
		};
		return &FUNCTION_TABLE;
	}

	template <typename T>
	static const FunctionTable* getFunctionTable() noexcept requires(!IS_SMALL<T>) {
		static_assert(std::is_void_v<Base> || std::is_base_of_v<Base, T>);
		static constexpr FunctionTable FUNCTION_TABLE{
			.destroy = [](AnyBase& self) noexcept -> void {
				delete static_cast<T*>(self.largeObject); // NOLINT(cppcoreguidelines-owning-memory)
			},
			.moveConstructOther = [](AnyBase& self, AnyBase& other) noexcept -> void {
				other.largeObject = self.largeObject;
				other.functionTable = self.functionTable;
				self.functionTable = &NULL_FUNCTION_TABLE;
			},
			.copyConstructOther = [](const AnyBase& self, AnyBase& other) -> void {
				other.largeObject = new T{*static_cast<const T*>(self.largeObject)}; // NOLINT(cppcoreguidelines-owning-memory)
				other.functionTable = self.functionTable;
			},
			.copyAssignOther = [](const AnyBase& self, AnyBase& other) -> void {
				if (other.functionTable == &FUNCTION_TABLE) {
					*static_cast<T*>(other.largeObject) = *static_cast<const T*>(self.largeObject);
				} else {
					T* const objectCopy = new T{*static_cast<const T*>(self.largeObject)}; // NOLINT(cppcoreguidelines-owning-memory)
					other.functionTable->destroy(other);
					other.largeObject = objectCopy;
					other.functionTable = self.functionTable;
				}
			},
			.getValuePointer = [](AnyBase& self) noexcept -> Base* { return self.largeObject; },
		};
		return &FUNCTION_TABLE;
	}

public:
	/**
	 * Create a wrapper of a new value.
	 *
	 * \tparam T type of the concrete value to create.
	 *
	 * \param args arguments to forward to the constructor of the new value.
	 *
	 * \return a wrapper around the newly created value.
	 *
	 * \throws any exception thrown by the value type's constructor.
	 * \throws std::bad_alloc on allocation failure, if the value type does not
	 *         qualify for small object optimization.
	 *
	 * \note The stored value's concrete type will be std::decay_t<T>.
	 */
	template <typename T, typename... Args>
	[[nodiscard]] static AnyBase create(Args&&... args) {
		return AnyBase{InplaceTag{}, std::in_place_type<T>, std::forward<Args>(args)...};
	}

	/**
	 * Construct an empty wrapper.
	 */
	constexpr AnyBase() noexcept
		: functionTable(&NULL_FUNCTION_TABLE) {}

	/**
	 * Construct a wrapper from a value.
	 *
	 * \param value value to forward to the stored value's constructor.
	 *
	 * \throws any exception thrown by the value type's constructor.
	 * \throws std::bad_alloc on allocation failure, if the value type does not
	 *         qualify for small object optimization.
	 *
	 * \note The stored value's concrete type will be std::decay_t<T>.
	 */
	template <typename T>
	AnyBase(T&& value) requires(!std::is_same_v<std::decay_t<T>, AnyBase>)
		: AnyBase(InplaceTag{}, std::in_place_type<T>, std::forward<T>(value)) {}

	/** Destructor. */
	~AnyBase() {
		functionTable->destroy(*this);
	}

	/** Copy constructor. */
	AnyBase(const AnyBase& other) {
		other.functionTable->copyConstructOther(other, *this);
	}

	/** Move constructor. */
	AnyBase(AnyBase&& other) noexcept {
		other.functionTable->moveConstructOther(other, *this);
	}

	/** Copy assignment */
	AnyBase& operator=(const AnyBase& other) {
		if (this == &other) {
			return *this;
		}
		other.functionTable->copyAssignOther(other, *this);
		return *this;
	}

	/** Move assignment. */
	AnyBase& operator=(AnyBase&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		functionTable->destroy(*this);
		other.functionTable->moveConstructOther(other, *this);
		return *this;
	}

	/**
	 * Destroy the wrapped object if one exists, and reset the wrapper to empty.
	 */
	void reset() noexcept {
		functionTable->destroy(*this);
		functionTable = &NULL_FUNCTION_TABLE;
	}

	/**
	 * Destroy the wrapped object if one exists, then replace it with a new
	 * object.
	 *
	 * \tparam T type of the concrete value to create.
	 *
	 * \param args arguments to forward to the constructor of the new value.
	 *
	 * \return a reference to the newly created value.
	 *
	 * \throws any exception thrown by the value type's constructor.
	 * \throws std::bad_alloc on allocation failure, if the value type does not
	 *         qualify for small object optimization.
	 *
	 * \note The stored value's concrete type will be std::decay_t<T>.
	 */
	template <typename T, typename... Args>
	std::decay_t<T>& emplace(Args&&... args) {
		reset();
		using X = std::decay_t<T>;
		X* result = nullptr;
		if constexpr (IS_SMALL<X>) {
			result = std::construct_at(reinterpret_cast<X*>(smallObject), std::forward<Args>(args)...);
		} else {
			result = new X(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
			largeObject = result;
		}
		functionTable = getFunctionTable<X>(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
		return *result;
	}

	/**
	 * Get a base pointer to the wrapped value.
	 *
	 * \return a non-owning pointer to the value held by the wrapper, or nullptr
	 *         if the wrapper does not currently hold a value.
	 *
	 * \sa get()
	 */
	[[nodiscard]] Base* base() noexcept {
		return functionTable->getValuePointer(*this);
	}

	/**
	 * Get a base pointer to the wrapped value.
	 *
	 * \return a non-owning read-only pointer to the value held by the wrapper,
	 *         or nullptr if the wrapper does not currently hold a value.
	 *
	 * \sa get()
	 */
	[[nodiscard]] const Base* base() const noexcept {
		return functionTable->getValuePointer(*const_cast<AnyBase*>(this));
	}

	/**
	 * Get a base pointer to the wrapped value, under the assumption that the
	 * value currently held by the wrapper fits into the small object
	 * optimization (in both size and alignment), and also that its address can
	 * be safely cast to the base pointer type without applying any offset
	 * (which can only be true if the concrete value type only has one base
	 * class, and the base class is not virtual).
	 *
	 * \return a non-owning pointer to the value held by the wrapper.
	 *
	 * \warning Failure to meet the requirements described above results in
	 *          undefined behavior.
	 * 
	 * \sa getUnsafeLargeObjectBasePointer()
	 */
	[[nodiscard]] Base* getUnsafeSmallObjectBasePointer() {
#if defined(_WIN32) || defined(__linux__)
		Base* const result = std::launder(reinterpret_cast<Base*>(smallObject));
		GREM_ASSERT(result == base());
		return result;
#else
		return base();
#endif
	}

	/**
	 * Get a base pointer to the wrapped value, under the assumption that the
	 * value currently held by the wrapper fits into the small object
	 * optimization (in both size and alignment), and also that its address can
	 * be safely cast to the base pointer type without applying any offset
	 * (which can only be true if the concrete value type only has one base
	 * class, and the base class is not virtual).
	 *
	 * \return a non-owning read-only pointer to the value held by the wrapper.
	 *
	 * \warning Failure to meet the requirements described above results in
	 *          undefined behavior.
	 * 
	 * \sa getUnsafeLargeObjectBasePointer()
	 */
	[[nodiscard]] const Base* getUnsafeSmallObjectBasePointer() const {
#if defined(_WIN32) || defined(__linux__)
		const Base* const result = std::launder(reinterpret_cast<const Base*>(smallObject));
		GREM_ASSERT(result == base());
		return result;
#else
		return base();
#endif
	}

	/**
	 * Get a base pointer to the wrapped value, under the assumption that the
	 * value currently held by the wrapper does not fit into the small object
	 * optimization.
	 *
	 * \return a non-owning pointer to the value held by the wrapper.
	 *
	 * \warning Failure to meet the requirements described above results in
	 *          undefined behavior.
	 * 
	 * \sa getUnsafeSmallObjectBasePointer()
	 */
	[[nodiscard]] Base* getUnsafeLargeObjectBasePointer() {
		GREM_ASSERT(largeObject == base());
		return largeObject;
	}

	/**
	 * Get a base pointer to the wrapped value, under the assumption that the
	 * value currently held by the wrapper does not fit into the small object
	 * optimization.
	 *
	 * \return a non-owning read-only pointer to the value held by the wrapper.
	 *
	 * \warning Failure to meet the requirements described above results in
	 *          undefined behavior.
	 * 
	 * \sa getUnsafeSmallObjectBasePointer()
	 */
	[[nodiscard]] const Base* getUnsafeLargeObjectBasePointer() const {
		GREM_ASSERT(largeObject == base());
		return largeObject;
	}

	/**
	 * Get a base reference to the wrapped value.
	 *
	 * \return a non-owning reference to the value held by the wrapper.
	 *
	 * \sa base()
	 */
	[[nodiscard]] auto& operator*() requires(!std::is_void_v<Base>) {
		Base* const result = base();
		GREM_ASSERT(result);
		return *result;
	}

	/**
	 * Get a base reference to the wrapped value.
	 *
	 * \return a non-owning read-only reference to the value held by the
	 *         wrapper.
	 *
	 * \sa base()
	 */
	[[nodiscard]] const auto& operator*() const requires(!std::is_void_v<Base>) {
		const Base* const result = base();
		GREM_ASSERT(result);
		return *result;
	}

	/**
	 * Get a base pointer to the wrapped value.
	 *
	 * \return a non-owning pointer to the value held by the wrapper.
	 *
	 * \sa base()
	 */
	[[nodiscard]] Base* operator->() requires(!std::is_void_v<Base>) {
		Base* const result = base();
		GREM_ASSERT(result);
		return result;
	}

	/**
	 * Get a base pointer to the wrapped value.
	 *
	 * \return a non-owning read-only pointer to the value held by the wrapper.
	 *
	 * \sa base()
	 */
	[[nodiscard]] const Base* operator->() const requires(!std::is_void_v<Base>) {
		const Base* const result = base();
		GREM_ASSERT(result);
		return result;
	}

	/**
	 * Try to get a pointer to the wrapped value.
	 *
	 * \tparam T type of the wrapped value to get.
	 *
	 * \return a non-owning pointer to the value held by the wrapper, or nullptr
	 *         if the wrapper does not currently hold a value of the given type.
	 *
	 * \sa get()
	 */
	template <typename T>
	[[nodiscard]] std::decay_t<T>* get_if() noexcept {
		using X = std::decay_t<T>;
		if (functionTable != getFunctionTable<X>()) {
			return nullptr;
		}
		if constexpr (IS_SMALL<X>) {
			return std::launder(reinterpret_cast<X*>(smallObject));
		} else {
			return largeObject;
		}
	}

	/**
	 * Try to get a pointer to the wrapped value.
	 *
	 * \tparam T type of the wrapped value to get.
	 *
	 * \return a non-owning read-only pointer to the value held by the wrapper,
	 *         or nullptr if the wrapper does not currently hold a value of the
	 *         given type.
	 *
	 * \sa get()
	 */
	template <typename T>
	[[nodiscard]] const std::decay_t<T>* get_if() const noexcept {
		using X = std::decay_t<T>;
		if (functionTable != getFunctionTable<X>()) {
			return nullptr;
		}
		if constexpr (IS_SMALL<X>) {
			return std::launder(reinterpret_cast<const X*>(smallObject));
		} else {
			return largeObject;
		}
	}

	/**
	 * Access the wrapped value.
	 *
	 * \tparam T type of the wrapped value to get.
	 *
	 * \return a reference to the value held by the wrapper.
	 *
	 * \throws BadAnyAccess if the wrapper does not currently hold a value of
	 *         the given type.
	 *
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] std::decay_t<T>& get() {
		std::decay_t<T>* const result = get_if<T>();
		if (!result) {
			throw BadAnyAccess{};
		}
		return *result;
	}

	/**
	 * Access the wrapped value.
	 *
	 * \tparam T type of the wrapped value to get.
	 *
	 * \return a read-only reference to the value held by the wrapper.
	 *
	 * \throws BadAnyAccess if the wrapper does not currently hold a value of
	 *         the given type.
	 *
	 * \sa get_if()
	 */
	template <typename T>
	[[nodiscard]] const std::decay_t<T>& get() const {
		const std::decay_t<T>* const result = get_if<T>();
		if (!result) {
			throw BadAnyAccess{};
		}
		return *result;
	}

private:
	struct InplaceTag {};

	template <typename T, typename... Args>
	AnyBase(InplaceTag, std::in_place_type_t<T>, Args&&... args) {
		using X = std::decay_t<T>;
		if constexpr (IS_SMALL<X>) {
			std::construct_at(reinterpret_cast<X*>(smallObject), std::forward<Args>(args)...);
		} else {
			largeObject = new X(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
		}
		functionTable = getFunctionTable<X>(); // NOLINT(cppcoreguidelines-prefer-member-initializer)
	}

	static void nullDestroy(AnyBase&) noexcept {}
	static void nullMoveConstructOther(AnyBase&, AnyBase& other) noexcept {
		other.functionTable = &NULL_FUNCTION_TABLE;
	}
	static void nullCopyConstructOther(const AnyBase&, AnyBase& other) {
		other.functionTable = &NULL_FUNCTION_TABLE;
	}
	static void nullCopyAssignOther(const AnyBase&, AnyBase& other) {
		other.functionTable->destroy(other);
		other.functionTable = &NULL_FUNCTION_TABLE;
	}
	static Base* nullGetValuePointer(AnyBase&) noexcept {
		return nullptr;
	}

	static constexpr FunctionTable NULL_FUNCTION_TABLE{
		.destroy = nullDestroy,
		.moveConstructOther = nullMoveConstructOther,
		.copyConstructOther = nullCopyConstructOther,
		.copyAssignOther = nullCopyAssignOther,
		.getValuePointer = nullGetValuePointer,
	};

	const FunctionTable* functionTable = &NULL_FUNCTION_TABLE;
	union {
		Base* largeObject;
		alignas(SmallAlignment) std::byte smallObject[SmallCapacity];
	};
};

/**
 * Type-erased semi-regular value wrapper.
 */
using Any = AnyBase<void>;

} // namespace grem

#endif
