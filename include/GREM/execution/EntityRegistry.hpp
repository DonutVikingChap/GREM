// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_ENTITY_REGISTRY_HPP
#define GREM_EXECUTION_ENTITY_REGISTRY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/execution/component.hpp>
#include <GREM/execution/component_pool.hpp>
#include <GREM/execution/entity_range.hpp>
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
#include <GREM/core/system/synchronization.hpp>
#endif

#include <algorithm>   // std::fill, std::copy_n
#include <compare>     // std::strong_ordering
#include <cstddef>     // std::size_t
#include <functional>  // std::hash
#include <iterator>    // std::forward_iterator_tag, std::random_access_iterator_tag
#include <memory>      // std::allocator, std::uninitialized_..., std::destroy_n, std::destroy_at, std::construct_at
#include <stdexcept>   // std::invalid_argument, std::out_of_range, std::length_error, std::logic_error
#include <type_traits> // std::is_const_v, std::is_empty_v, std::remove_..._t, std::conditional_t, std::true_type
#include <typeindex>   // std::type_index
#include <typeinfo>    // IWYU pragma: keep // typeid
#include <utility>     // std::move, std::forward, std::exchange, std::...index_sequence, std::in_place_type...

namespace grem::execution {

template <typename... KnownComponents>
class EntityRegistry; // Forward declaration.

template <typename... ComponentsAndExclusions>
class Entities; // Forward declaration.

// Mark Entities as an entity range type.
template <typename... ComponentsAndExclusions>
struct is_entity_range<Entities<ComponentsAndExclusions...>> : std::true_type {};

// Define how to extract the components and exclusions of an Entities type.
template <typename... ComponentsAndExclusions>
struct entity_range_components_and_exclusions<Entities<ComponentsAndExclusions...>> {
	using type = meta::TypeList<ComponentsAndExclusions...>;
};

template <typename Component>
class ComponentPool; // Forward declaration.

// Mark ComponentPool as a component pool type.
template <typename Component>
struct is_component_pool<ComponentPool<Component>> : std::true_type {};

/**
 * Handle to a specific entity in a registry.
 */
class EntityID {
public:
	using Identifier = uint64_t; ///< Underlying value type.
	using Index = uint32_t;      ///< Index type.
	using Generation = uint16_t; ///< Generation counter type.
	using Flags = uint16_t;      ///< User-defined flags type.

	static constexpr Index INVALID_INDEX = Limits<Index>::MAX;            ///< Reserved index value for an invalid entity handle.
	static constexpr Index MAX_INDEX = Limits<Index>::MAX - 1;            ///< Maximum valid index value.
	static constexpr Generation MAX_GENERATION = Limits<Generation>::MAX; ///< Maximum valid generation counter value.
	static constexpr Flags ALL_FLAGS = Limits<Flags>::MAX;                ///< Maximum valid set of user-defined flags.

	/**
     * Construct an invalid entity handle.
     */
	GREM_ALWAYS_INLINE constexpr EntityID() noexcept = default;

	/**
	 * Construct an entity handle with a specific set of parameters.
	 *
	 * \param index index of the entity's generation slot within the registry.
	 *        Must be less than or equal to MAX_INDEX.
	 * \param generation generation counter value of the entity. Must be less
	 *        than or equal to MAX_GENERATION.
	 * \param flags user-defined flags. Must be less than or equal to ALL_FLAGS.
	 */
	GREM_ALWAYS_INLINE constexpr EntityID(Index index, Generation generation, Flags flags)
		: value((uint64_t{flags} << 48) | (uint64_t{generation} << 32) | uint64_t{index}) {}

	/**
	 * Check if this handle is potentially valid.
	 *
	 * \return true if this handle is potentially valid, false if it is equal to
	 *         a default-constructed invalid handle.
	 */
	GREM_ALWAYS_INLINE constexpr explicit operator bool() const noexcept {
		return *this != EntityID{};
	}

	/**
	 * Get a unique identifier for the underlying representation of the entity
	 * handle.
	 *
	 * \return the underlying value of the handle.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Identifier getIdentifier() const noexcept {
		return value;
	}

	/**
	 * Get the index of the entity's generation slot.
	 *
	 * \return the entity index.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Index getIndex() const noexcept {
		return static_cast<Index>(value & uint64_t{Limits<Index>::MAX});
	}

	/**
	 * Get the generation counter value of the entity.
	 *
	 * \return the entity generation.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Generation getGeneration() const noexcept {
		return static_cast<Generation>((value >> 32) & uint64_t{Limits<Generation>::MAX});
	}

	/**
	 * Get the user-defined flags of the entity.
	 *
	 * \return the entity flags.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Flags getFlags() const noexcept {
		return static_cast<Flags>((value >> 48) & uint64_t{Limits<Flags>::MAX});
	}

	/**
	 * Compare this handle to another for equality.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return true if the handles are equal, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const EntityID& other) const noexcept = default;

	/**
	 * Compare this handle to another.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return a strong ordering between the two handles.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr std::strong_ordering operator<=>(const EntityID& other) const noexcept = default;

private:
	uint64_t value = Limits<uint64_t>::MAX;
};

namespace detail {

struct EntitySlot {
	struct Active {
		EntityID::Index idIndex;
		EntityID::Flags flags;
	};

	struct Inactive {
		EntityID::Index previousAvailableSlotIndex;
		EntityID::Index nextAvailableSlotIndex;
	};

	EntityID::Generation generation;
	union {
		Active active;
		Inactive inactive;
	};
};

template <typename ComponentOrExclusion>
struct entities_extract_components {
	static_assert(component<ComponentOrExclusion>,
		"The template arguments to grem::execution::Entities may only contain valid component types and/or exclusions of valid component types. See the definition of the "
		"grem::execution::component concept for what constitutes a valid component type.");
};

template <component Component>
struct entities_extract_components<Component> {
	using MutableComponents = meta::TypeList<Component>;
	using ImmutableComponents = meta::TypeList<>;
	using IncludedComponents = meta::TypeList<Component>;
	using ExcludedComponents = meta::TypeList<>;
};

template <component Component>
struct entities_extract_components<const Component> {
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<Component>;
	using IncludedComponents = meta::TypeList<const Component>;
	using ExcludedComponents = meta::TypeList<>;
};

template <component Component>
struct entities_extract_components<Exclude<Component>> {
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<>;
	using IncludedComponents = meta::TypeList<>;
	using ExcludedComponents = meta::TypeList<Component>;
};

template <typename T>
concept empty_component = std::is_empty_v<std::remove_cv_t<T>>;

template <empty_component Component>
inline constinit Component DUMMY_COMPONENT{};

template <typename... Components>
inline constexpr size_t NON_EMPTY_COMPONENT_COUNT = (static_cast<size_t>(!empty_component<Components>) + ... + 0);

template <typename... Components>
static constexpr auto NON_EMPTY_COMPONENT_INDEX_MAP = [] {
	Array<size_t, sizeof...(Components)> result{};
	[&]<size_t... Indices>(std::index_sequence<Indices...>) {
		size_t i = 0;
		((result[Indices] = (empty_component<Components>) ? size_t{} : i++), ...);
	}(std::make_index_sequence<sizeof...(Components)>{});
	return result;
}();

template <typename FirstComponent, typename... OtherComponents>
struct first_non_empty_component : first_non_empty_component<OtherComponents...> {};

template <typename FirstComponent>
struct first_non_empty_component<FirstComponent> {
	using type = FirstComponent;
};

template <typename FirstComponent, typename... OtherComponents>
requires(!empty_component<FirstComponent>) struct first_non_empty_component<FirstComponent, OtherComponents...> {
	using type = FirstComponent;
};

template <typename List>
struct non_empty_component_count_of_type_list;

template <typename... Ts>
struct non_empty_component_count_of_type_list<meta::TypeList<Ts...>> : std::integral_constant<size_t, NON_EMPTY_COMPONENT_COUNT<Ts...>> {};

template <typename List>
struct first_non_empty_component_of_type_list;

template <typename... Ts>
struct first_non_empty_component_of_type_list<meta::TypeList<Ts...>> : first_non_empty_component<Ts...> {};

template <typename... ComponentsAndExclusions>
concept entities_is_single_component =
	(meta::type_list_size_v<typename detail::entities_extract_components<ComponentsAndExclusions>::IncludedComponents> + ...) == 1 &&
	(meta::type_list_empty_v<typename detail::entities_extract_components<ComponentsAndExclusions>::ExcludedComponents> && ...);

struct EmptyComponentPointer {};

#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
struct IteratorValidity {
	SharedPointer<Atomic<size_t>> validGeneration{};
	size_t iteratorGeneration = Limits<size_t>::MAX;

	constexpr IteratorValidity() noexcept = default;

	IteratorValidity(SharedPointer<Atomic<size_t>> validGeneration) noexcept
		: validGeneration(std::move(validGeneration)) {
		if (this->validGeneration) {
			iteratorGeneration = this->validGeneration->load();
		}
	}

	[[nodiscard]] bool operator==(const IteratorValidity&) const noexcept = default;

	void assertValid() const {
		[[maybe_unused]] const bool iteratorValidity = validGeneration && iteratorGeneration == validGeneration->load();
		GREM_ASSERT(iteratorValidity && R"(

Error: Detected use of invalidated iterator to grem::execution::Entities.
Note: Iterators may be invalidated by calls to createEntity(), destroyEntity(), clear(), addComponent(), etc. on the EntityRegistry.
Tip: If entities need to be destroyed as a result of iteration, either use erase_if(), or build a list of EntityIDs while iterating and destroy them afterwards.
Tip: If new entities need to be created as a result of iteration, build a list of configurations while iterating, and create entities from them afterwards.
Tip: If only one entity needs to be created/destroyed as a result of searching the entities, create/destroy it and then immediately break from the loop.

)");
	}
};
#endif

struct MultiComponentSentinel {};

template <typename IncludedComponentsList, typename ExcludedComponentsList>
class MultiComponentIterator;

template <typename... IncludedComponents, typename... ExcludedComponents>
class MultiComponentIterator<meta::TypeList<IncludedComponents...>, meta::TypeList<ExcludedComponents...>> {
private:
	static constexpr size_t EXCLUDED_COMPONENT_COUNT = sizeof...(ExcludedComponents);
	static constexpr size_t INCLUDED_COMPONENT_COUNT = sizeof...(IncludedComponents);
	static constexpr size_t NON_EMPTY_INCLUDED_COMPONENT_COUNT = NON_EMPTY_COMPONENT_COUNT<IncludedComponents...>;
	static constexpr auto NON_EMPTY_INCLUDED_COMPONENT_INDEX_MAP = NON_EMPTY_COMPONENT_INDEX_MAP<IncludedComponents...>;

public:
	using difference_type = ptrdiff_t;
	using value_type = Tuple<const EntityID, IncludedComponents...>;
	using reference = Tuple<const EntityID, IncludedComponents&...>;
	using iterator_category = std::forward_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	GREM_ALWAYS_INLINE MultiComponentIterator() noexcept = default;

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
	GREM_ALWAYS_INLINE constexpr MultiComponentIterator(const EntityID* candidateEntity, const EntityID* candidateEntitiesEnd,
		const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays, const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays,
		const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays, Span<const EntitySlot> slots) noexcept
		: candidateEntity(candidateEntity)
		, candidateEntitiesEnd(candidateEntitiesEnd)
		, excludedIndexArrays(excludedIndexArrays)
		, componentIndexArrays(componentIndexArrays)
		, componentArrays(componentArrays)
		, slots(slots) {
		findNextEntity();
	}
#else
	GREM_ALWAYS_INLINE constexpr MultiComponentIterator(const EntityID* candidateEntity, const EntityID* candidateEntitiesEnd,
		const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays, const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays,
		const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays, Span<const EntitySlot> slots,
		const Array<IteratorValidity, EXCLUDED_COMPONENT_COUNT>& excludedIteratorValidity,
		const Array<IteratorValidity, max(INCLUDED_COMPONENT_COUNT, size_t{1})>& componentIteratorValidity) noexcept
		: candidateEntity(candidateEntity)
		, candidateEntitiesEnd(candidateEntitiesEnd)
		, excludedIndexArrays(excludedIndexArrays)
		, componentIndexArrays(componentIndexArrays)
		, componentArrays(componentArrays)
		, slots(slots)
		, excludedIteratorValidity(excludedIteratorValidity)
		, componentIteratorValidity(componentIteratorValidity) {
		findNextEntity();
	}
#endif

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator*() const {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		GREM_ASSERT(containsEntity(excludedIndexArrays, componentIndexArrays, slots, *candidateEntity));
		return dereference(componentIndexArrays, componentArrays, *candidateEntity);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr pointer operator->() const {
		return pointer{**this};
	}

	GREM_ALWAYS_INLINE constexpr MultiComponentIterator& operator++() {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		++candidateEntity;
		findNextEntity();
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr MultiComponentIterator operator++(int) {
		MultiComponentIterator old = *this;
		++*this;
		return old;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const MultiComponentIterator& other) const {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(excludedIteratorValidity == other.excludedIteratorValidity);
		GREM_ASSERT(componentIteratorValidity == other.componentIteratorValidity);
#endif
		return candidateEntity == other.candidateEntity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const MultiComponentSentinel&) const {
		return candidateEntity == candidateEntitiesEnd;
	}

private:
	template <typename... ComponentsAndExclusions>
	friend class grem::execution::Entities;

	template <typename Component, size_t I>
	[[nodiscard]] GREM_ALWAYS_INLINE static Component& getComponentUnsafe(const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays,
		const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays, size_t slotIndex) {
		if constexpr (empty_component<Component>) {
			return DUMMY_COMPONENT<Component>;
		} else {
			return static_cast<Component*>(componentArrays[NON_EMPTY_INCLUDED_COMPONENT_INDEX_MAP[I]])[componentIndexArrays[I][slotIndex]];
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static bool passesFilter(const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays,
		const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays, EntityID id) {
		const size_t slotIndex = id.getIndex();
		const bool notExcluded = [&]<size_t... Indices>(std::index_sequence<Indices...>) -> bool {
			return ((!excludedIndexArrays[Indices] || excludedIndexArrays[Indices][slotIndex] == EntityID::INVALID_INDEX) && ... && true);
		}(std::make_index_sequence<EXCLUDED_COMPONENT_COUNT>{});
		if (!notExcluded) {
			return false;
		}
		const bool included = [&]<size_t... Indices>(std::index_sequence<Indices...>) -> bool {
			return ((componentIndexArrays[Indices][slotIndex] != EntityID::INVALID_INDEX) && ... && true);
		}(std::make_index_sequence<INCLUDED_COMPONENT_COUNT>{});
		return included;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static bool containsEntity(const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays,
		const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays, Span<const EntitySlot> slots, EntityID id) {
		return id.getIndex() < slots.size() && (id.getGeneration() & 1) != 0 && slots[id.getIndex()].generation == id.getGeneration() &&
		       passesFilter(excludedIndexArrays, componentIndexArrays, id);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static reference dereference(const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays,
		const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays, EntityID id) {
		return [&]<size_t... Indices>(std::index_sequence<Indices...>) -> reference {
			const size_t slotIndex = id.getIndex();
			return reference{id, getComponentUnsafe<IncludedComponents, Indices>(componentIndexArrays, componentArrays, slotIndex)...};
		}(std::make_index_sequence<INCLUDED_COMPONENT_COUNT>{});
	}

	GREM_ALWAYS_INLINE void findNextEntity() {
		while (candidateEntity != candidateEntitiesEnd && !passesFilter(excludedIndexArrays, componentIndexArrays, *candidateEntity)) {
			++candidateEntity;
		}
	}

	const EntityID* candidateEntity;
	const EntityID* candidateEntitiesEnd;
	[[no_unique_address]] Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT> excludedIndexArrays;
	[[no_unique_address]] Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT> componentIndexArrays;
	[[no_unique_address]] Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT> componentArrays;
	Span<const EntitySlot> slots;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	Array<IteratorValidity, EXCLUDED_COMPONENT_COUNT> excludedIteratorValidity{};
	Array<IteratorValidity, max(INCLUDED_COMPONENT_COUNT, size_t{1})> componentIteratorValidity{};

	void assertValid() const {
		for (size_t i = 0; i < EXCLUDED_COMPONENT_COUNT; ++i) {
			if (excludedIndexArrays[i]) {
				excludedIteratorValidity[i].assertValid();
			}
		}
		for (const IteratorValidity& validity : componentIteratorValidity) {
			validity.assertValid();
		}
	}
#endif
};

struct SingleComponentSentinel {
	const EntityID* entitiesEnd;
};

template <typename Component>
class SingleComponentIterator {
public:
	using difference_type = ptrdiff_t;
	using value_type = Tuple<const EntityID, Component>;
	using reference = Tuple<const EntityID, Component&>;
	using iterator_category = std::random_access_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	GREM_ALWAYS_INLINE SingleComponentIterator() noexcept = default;

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
	GREM_ALWAYS_INLINE constexpr SingleComponentIterator(const EntityID* entity, Component* component, const EntityID::Index* componentIndexArray, void* componentArray,
		Span<const EntitySlot> slots) noexcept
		: entity(entity)
		, component(component)
		, componentIndexArray(componentIndexArray)
		, componentArray(componentArray)
		, slots(slots) {}
#else
	GREM_ALWAYS_INLINE constexpr SingleComponentIterator(const EntityID* entity, Component* component, const EntityID::Index* componentIndexArray, void* componentArray,
		Span<const EntitySlot> slots,
		const IteratorValidity& validity) noexcept // NOLINT(modernize-pass-by-value)
		: entity(entity)
		, component(component)
		, componentIndexArray(componentIndexArray)
		, componentArray(componentArray)
		, slots(slots)
		, validity(validity) {}
#endif

	GREM_ALWAYS_INLINE constexpr operator SingleComponentIterator<const Component>() const noexcept {
#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return SingleComponentIterator<const Component>{entity, component, componentIndexArray, componentArray, slots};
#else
		return SingleComponentIterator<const Component>{entity, component, componentIndexArray, componentArray, slots, validity};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator*() const {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		return reference{*entity, *component};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr pointer operator->() const {
		return pointer{**this};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator++() {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		++entity;
		++component;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator--() {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		--entity;
		--component;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator operator++(int) {
		SingleComponentIterator old = *this;
		++*this;
		return old;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator operator--(int) {
		SingleComponentIterator old = *this;
		--*this;
		return old;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator+=(difference_type n) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		if (n != 0) {
			assertValid();
		}
#endif
		entity += n;
		component += n;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator-=(difference_type n) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		if (n != 0) {
			assertValid();
		}
#endif
		entity -= n;
		component -= n;
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const SingleComponentSentinel& sentinel) const {
		return entity == sentinel.entitiesEnd;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator+(SingleComponentIterator a, difference_type b) {
		return a += b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator+(difference_type a, SingleComponentIterator b) {
		return b += a;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator-(SingleComponentIterator a, difference_type b) {
		return a -= b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr difference_type operator-(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		GREM_ASSERT(a.entity - b.entity == a.component - b.component);
		return static_cast<difference_type>(a.entity - b.entity);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		GREM_ASSERT(a.entity - b.entity == a.component - b.component);
		return a.entity == b.entity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		GREM_ASSERT(a.entity - b.entity == a.component - b.component);
		return a.entity <=> b.entity;
	}

private:
	template <typename... ComponentsAndExclusions>
	friend class Entities;

	const EntityID* entity;
	Component* component;
	const EntityID::Index* componentIndexArray;
	void* componentArray;
	Span<const EntitySlot> slots;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	IteratorValidity validity{};

	void assertValid() const {
		validity.assertValid();
	}
#endif
};

template <empty_component Component>
class SingleComponentIterator<Component> {
public:
	using difference_type = ptrdiff_t;
	using value_type = Tuple<const EntityID, Component>;
	using reference = Tuple<const EntityID, Component&>;
	using iterator_category = std::random_access_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	GREM_ALWAYS_INLINE SingleComponentIterator() noexcept = default;

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
	GREM_ALWAYS_INLINE constexpr SingleComponentIterator(const EntityID* entity, Component*, const EntityID::Index* componentIndexArray, void*,
		Span<const EntitySlot> slots) noexcept
		: entity(entity)
		, componentIndexArray(componentIndexArray)
		, slots(slots) {}
#else
	GREM_ALWAYS_INLINE constexpr SingleComponentIterator(const EntityID* entity, Component*, const EntityID::Index* componentIndexArray, void*, Span<const EntitySlot> slots,
		const IteratorValidity& validity) noexcept // NOLINT(modernize-pass-by-value)
		: entity(entity)
		, componentIndexArray(componentIndexArray)
		, slots(slots)
		, validity(validity) {}
#endif

	GREM_ALWAYS_INLINE constexpr operator SingleComponentIterator<const Component>() const noexcept {
#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return SingleComponentIterator<const Component>{entity, nullptr, componentIndexArray, nullptr, slots};
#else
		return SingleComponentIterator<const Component>{entity, nullptr, componentIndexArray, nullptr, slots, validity};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator*() const {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		return reference{*entity, DUMMY_COMPONENT<Component>};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr pointer operator->() const {
		return pointer{**this};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator++() {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		++entity;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator--() {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		--entity;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator operator++(int) {
		SingleComponentIterator old = *this;
		++*this;
		return old;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator operator--(int) {
		SingleComponentIterator old = *this;
		--*this;
		return old;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator+=(difference_type n) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		if (n != 0) {
			assertValid();
		}
#endif
		entity += n;
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr SingleComponentIterator& operator-=(difference_type n) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		if (n != 0) {
			assertValid();
		}
#endif
		entity -= n;
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const SingleComponentSentinel& sentinel) const {
		return entity == sentinel.entitiesEnd;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator+(SingleComponentIterator a, difference_type b) {
		return a += b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator+(difference_type a, SingleComponentIterator b) {
		return b += a;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr SingleComponentIterator operator-(SingleComponentIterator a, difference_type b) {
		return a -= b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr difference_type operator-(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		return static_cast<difference_type>(a.entity - b.entity);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool operator==(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		return a.entity == b.entity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr auto operator<=>(SingleComponentIterator a, SingleComponentIterator b) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(a.validity == b.validity);
#endif
		return a.entity <=> b.entity;
	}

private:
	template <typename... ComponentsAndExclusions>
	friend class Entities;

	const EntityID* entity;
	const EntityID::Index* componentIndexArray;
	Span<const EntitySlot> slots;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	IteratorValidity validity{};

	void assertValid() const {
		validity.assertValid();
	}
#endif
};

} // namespace detail

template <typename... ComponentsAndExclusions>
class Entities {
public:
	using MutableComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::MutableComponents...>;
	using ImmutableComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::ImmutableComponents...>;
	using IncludedComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::IncludedComponents...>;
	using ExcludedComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::ExcludedComponents...>;

private:
	static constexpr size_t EXCLUDED_COMPONENT_COUNT = meta::type_list_size_v<ExcludedComponents>;
	static constexpr size_t INCLUDED_COMPONENT_COUNT = meta::type_list_size_v<IncludedComponents>;
	static constexpr size_t NON_EMPTY_INCLUDED_COMPONENT_COUNT = detail::non_empty_component_count_of_type_list<IncludedComponents>::value;

public:
	using iterator = detail::MultiComponentIterator<IncludedComponents, ExcludedComponents>;
	using sentinel = detail::MultiComponentSentinel;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	GREM_ALWAYS_INLINE constexpr Entities() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Entities(const iterator& first, const iterator& last) noexcept
		: candidateEntities(first.candidateEntities.data(), static_cast<size_t>(last.candidateEntities.data() - first.candidateEntities.data()))
		, excludedIndexArrays(first.excludedIndexArrays)
		, componentIndexArrays(first.componentIndexArrays)
		, componentArrays(first.componentArrays)
		, slots(first.slots) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(first.excludedIteratorValidity == last.excludedIteratorValidity);
		GREM_ASSERT(first.componentIteratorValidity == last.componentIteratorValidity);
		excludedIteratorValidity = first.excludedIteratorValidity;
		componentIteratorValidity = first.componentIteratorValidity;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr iterator begin() const noexcept {
#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return iterator{candidateEntities.data(), candidateEntities.data() + candidateEntities.size(), excludedIndexArrays, componentIndexArrays, componentArrays, slots};
#else
		return iterator{candidateEntities.data(), candidateEntities.data() + candidateEntities.size(), excludedIndexArrays, componentIndexArrays, componentArrays, slots,
			excludedIteratorValidity, componentIteratorValidity};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr sentinel end() const noexcept {
		return sentinel{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type getCandidateCount() const noexcept {
		return candidateEntities.size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool containsEntity(EntityID id) const {
		for (const EntityID::Index* const componentIndexArray : componentIndexArrays) {
			if (!componentIndexArray) {
				return false;
			}
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		return iterator::containsEntity(excludedIndexArrays, componentIndexArrays, slots, id);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator[](EntityID id) const {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		GREM_ASSERT(iterator::containsEntity(excludedIndexArrays, componentIndexArrays, slots, id));
		return iterator::dereference(componentIndexArrays, componentArrays, id);
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasComponent(EntityID id) const noexcept {
		return findComponent<T>(id) != nullptr;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto& getComponent(EntityID id) const {
		auto* const result = findComponent<T>(id);
		if (!result) {
			throw std::out_of_range{("Component \"" + meta::unqualified_type_name_v<T> + "\" not found for the specified entity.").c_str()};
		}
		return *result;
	}

	template <component T, typename U>
	[[nodiscard]] GREM_ALWAYS_INLINE auto getComponentOr(EntityID id, U&& defaultValue) const {
		auto* const result = findComponent<T>(id);
		if (!result) {
			return std::forward<U>(defaultValue);
		}
		return *result;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto* findComponent(EntityID id) const noexcept {
		if (containsEntity(id)) {
			return &getComponentAtEntityIndexUnsafe<T>(id.getIndex());
		}
		return decltype(&getComponentAtEntityIndexUnsafe<T>(id.getIndex())){nullptr};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE EntityID getEntityIDAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		GREM_ASSERT((slots[slotIndex].generation & 1) != 0);
		return EntityID{slotIndex, slots[slotIndex].generation, slots[slotIndex].active.flags};
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto& getComponentAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		constexpr bool CONTAINS_CONST_COMPONENT = meta::type_list_contains_v<IncludedComponents, const T>;
		constexpr bool CONTAINS_NON_CONST_COMPONENT = meta::type_list_contains_v<IncludedComponents, T>;
		static_assert(CONTAINS_CONST_COMPONENT || CONTAINS_NON_CONST_COMPONENT, "The specified component type must be included in the entity range type.");
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		if constexpr (CONTAINS_CONST_COMPONENT) {
			return iterator::template getComponentUnsafe<const T, meta::type_list_index_v<IncludedComponents, const T>>(componentIndexArrays, componentArrays, slotIndex);
		} else {
			return iterator::template getComponentUnsafe<T, meta::type_list_index_v<IncludedComponents, T>>(componentIndexArrays, componentArrays, slotIndex);
		}
	}

private:
	template <typename... KnownComponents>
	friend class EntityRegistry;

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays,
		const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays, const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays,
		Span<const detail::EntitySlot> slots) noexcept
		: candidateEntities(candidateEntities)
		, excludedIndexArrays(excludedIndexArrays)
		, componentIndexArrays(componentIndexArrays)
		, componentArrays(componentArrays)
		, slots(slots) {}
#else
	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT>& excludedIndexArrays,
		const Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT>& componentIndexArrays, const Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT>& componentArrays,
		Span<const detail::EntitySlot> slots, const Array<detail::IteratorValidity, EXCLUDED_COMPONENT_COUNT>& excludedIteratorValidity,
		const Array<detail::IteratorValidity, max(INCLUDED_COMPONENT_COUNT, size_t{1})>& componentIteratorValidity) noexcept
		: candidateEntities(candidateEntities)
		, excludedIndexArrays(excludedIndexArrays)
		, componentIndexArrays(componentIndexArrays)
		, componentArrays(componentArrays)
		, slots(slots)
		, excludedIteratorValidity(excludedIteratorValidity)
		, componentIteratorValidity(componentIteratorValidity) {}
#endif

	GREM_ALWAYS_INLINE void chunkCandidates(size_t begin, size_t end) noexcept {
		candidateEntities = candidateEntities.subspan(begin, end - begin);
	}

	Span<const EntityID> candidateEntities{};
	[[no_unique_address]] Array<const EntityID::Index*, EXCLUDED_COMPONENT_COUNT> excludedIndexArrays{};
	[[no_unique_address]] Array<const EntityID::Index*, INCLUDED_COMPONENT_COUNT> componentIndexArrays{};
	[[no_unique_address]] Array<void*, NON_EMPTY_INCLUDED_COMPONENT_COUNT> componentArrays{};
	Span<const detail::EntitySlot> slots{};
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	Array<detail::IteratorValidity, EXCLUDED_COMPONENT_COUNT> excludedIteratorValidity{};
	Array<detail::IteratorValidity, max(INCLUDED_COMPONENT_COUNT, size_t{1})> componentIteratorValidity{};

	void assertValid() const {
		for (size_t i = 0; i < EXCLUDED_COMPONENT_COUNT; ++i) {
			if (excludedIndexArrays[i]) {
				excludedIteratorValidity[i].assertValid();
			}
		}
		for (const detail::IteratorValidity& validity : componentIteratorValidity) {
			validity.assertValid();
		}
	}
#endif
};

template <typename... ComponentsAndExclusions>
requires(detail::entities_is_single_component<ComponentsAndExclusions...>) class Entities<ComponentsAndExclusions...> {
public:
	using MutableComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::MutableComponents...>;
	using ImmutableComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::ImmutableComponents...>;
	using IncludedComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::IncludedComponents...>;
	using ExcludedComponents = meta::type_list_concat_t<typename detail::entities_extract_components<ComponentsAndExclusions>::ExcludedComponents...>;

private:
	using Component = typename detail::first_non_empty_component_of_type_list<IncludedComponents>::type;

	static constexpr bool IS_COMPONENT_EMPTY = detail::empty_component<Component>;

public:
	using iterator = detail::SingleComponentIterator<Component>;
	using sentinel = detail::SingleComponentSentinel;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	GREM_ALWAYS_INLINE constexpr Entities() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Entities(const iterator& first, const iterator& last) noexcept requires(IS_COMPONENT_EMPTY)
		: candidateEntities(first.entity, static_cast<size_t>(last.entity - first.entity))
		, componentIndexArray(first.componentIndexArray)
		, slots(first.slots) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(first.validity == last.validity);
		validity = first.validity;
#endif
	}

	GREM_ALWAYS_INLINE constexpr Entities(const iterator& first, const iterator& last) noexcept requires(!IS_COMPONENT_EMPTY)
		: candidateEntities(first.entity, static_cast<size_t>(last.entity - first.entity))
		, candidateComponents(first.component)
		, componentIndexArray(first.componentIndexArray)
		, componentArray(first.componentArray)
		, slots(first.slots) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		GREM_ASSERT(first.validity == last.validity);
		validity = first.validity;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr iterator begin() const noexcept {
		Component* component = nullptr;
		void* componentArrayElement = nullptr;
		if constexpr (!IS_COMPONENT_EMPTY) {
			component = candidateComponents;
			componentArrayElement = componentArray;
		}
#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return iterator{candidateEntities.data(), component, componentIndexArray, componentArrayElement, slots};
#else
		return iterator{candidateEntities.data(), component, componentIndexArray, componentArrayElement, slots, validity};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr sentinel end() const noexcept {
		return sentinel{candidateEntities.data() + candidateEntities.size()};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Component* data() const noexcept requires(!IS_COMPONENT_EMPTY) {
		return candidateComponents;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_type size() const noexcept {
		return candidateEntities.size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool empty() const noexcept {
		return candidateEntities.empty();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t getCandidateCount() const noexcept {
		return candidateEntities.size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool containsEntity(EntityID id) const {
		if (!componentIndexArray) {
			return false;
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		return id.getIndex() < slots.size() && (id.getGeneration() & 1) != 0 && slots[id.getIndex()].generation == id.getGeneration() &&
		       componentIndexArray[id.getIndex()] != EntityID::INVALID_INDEX;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator[](EntityID id) const {
		GREM_ASSERT(containsEntity(id));
		return {id, getComponentAtEntityIndexUnsafe<std::remove_const_t<Component>>(id.getIndex())};
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasComponent(EntityID id) const noexcept {
		return findComponent<T>(id) != nullptr;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto& getComponent(EntityID id) const {
		auto* const result = findComponent<T>(id);
		if (!result) {
			throw std::out_of_range{("Component \"" + meta::unqualified_type_name_v<T> + "\" not found for the specified entity.").c_str()};
		}
		return *result;
	}

	template <component T, typename U>
	[[nodiscard]] GREM_ALWAYS_INLINE auto getComponentOr(EntityID id, U&& defaultValue) const {
		auto* const result = findComponent<T>(id);
		if (!result) {
			return std::forward<U>(defaultValue);
		}
		return *result;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto* findComponent(EntityID id) const noexcept {
		if (containsEntity(id)) {
			return &getComponentAtEntityIndexUnsafe<T>(id.getIndex());
		}
		return decltype(&getComponentAtEntityIndexUnsafe<T>(id.getIndex())){nullptr};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE EntityID getEntityIDAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		GREM_ASSERT((slots[slotIndex].generation & 1) != 0);
		return EntityID{slotIndex, slots[slotIndex].generation, slots[slotIndex].active.flags};
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE auto& getComponentAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		static_assert(same_as<T, std::remove_const_t<Component>>, "The specified component type must be included in the entity range type.");
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		assertValid();
#endif
		if constexpr (IS_COMPONENT_EMPTY) {
			return detail::DUMMY_COMPONENT<Component>;
		} else {
			return static_cast<Component*>(componentArray)[componentIndexArray[slotIndex]];
		}
	}

private:
	template <typename... KnownComponents>
	friend class EntityRegistry;

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, 0>&,
		const Array<const EntityID::Index*, 1>& componentIndexArrays, const Array<void*, 0>&, Span<const detail::EntitySlot> slots) noexcept requires(IS_COMPONENT_EMPTY)
		: candidateEntities(candidateEntities)
		, componentIndexArray(componentIndexArrays[0])
		, slots(slots) {}

	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, 0>&,
		const Array<const EntityID::Index*, 1>& componentIndexArrays, const Array<void*, 1>& componentArrays, Span<const detail::EntitySlot> slots) noexcept
		requires(!IS_COMPONENT_EMPTY)
		: candidateEntities(candidateEntities)
		, candidateComponents(static_cast<Component*>(componentArrays[0]))
		, componentIndexArray(componentIndexArrays[0])
		, componentArray(componentArrays[0])
		, slots(slots) {}
#else
	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, 0>&,
		const Array<const EntityID::Index*, 1>& componentIndexArrays, const Array<void*, 0>&, Span<const detail::EntitySlot> slots, const Array<detail::IteratorValidity, 0>&,
		const Array<detail::IteratorValidity, 1>& componentIteratorValidity) noexcept requires(IS_COMPONENT_EMPTY)
		: candidateEntities(candidateEntities)
		, componentIndexArray(componentIndexArrays[0])
		, slots(slots)
		, validity(componentIteratorValidity[0]) {
		if (!candidateEntities.empty()) {
			validity.assertValid();
		}
	}

	GREM_ALWAYS_INLINE constexpr Entities(Span<const EntityID> candidateEntities, const Array<const EntityID::Index*, 0>&,
		const Array<const EntityID::Index*, 1>& componentIndexArrays, const Array<void*, 1>& componentArrays, Span<const detail::EntitySlot> slots,
		const Array<detail::IteratorValidity, 0>&, const Array<detail::IteratorValidity, 1>& componentIteratorValidity) noexcept requires(!IS_COMPONENT_EMPTY)
		: candidateEntities(candidateEntities)
		, candidateComponents(static_cast<Component*>(componentArrays[0]))
		, componentIndexArray(componentIndexArrays[0])
		, componentArray(componentArrays[0])
		, slots(slots)
		, validity(componentIteratorValidity[0]) {
		if (!candidateEntities.empty()) {
			validity.assertValid();
		}
	}
#endif

	GREM_ALWAYS_INLINE void chunkCandidates(size_t begin, size_t end) noexcept {
		candidateEntities = candidateEntities.subspan(begin, end - begin);
		if constexpr (!IS_COMPONENT_EMPTY) {
			candidateComponents += begin;
		}
	}

	Span<const EntityID> candidateEntities{};
	[[no_unique_address]] std::conditional_t<IS_COMPONENT_EMPTY, detail::EmptyComponentPointer, Component*> candidateComponents{};
	[[no_unique_address]] const EntityID::Index* componentIndexArray = nullptr;
	[[no_unique_address]] std::conditional_t<IS_COMPONENT_EMPTY, detail::EmptyComponentPointer, void*> componentArray{};
	Span<const detail::EntitySlot> slots{};
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	detail::IteratorValidity validity{};

	void assertValid() const {
		validity.assertValid();
	}
#endif
};

namespace detail {

struct ComponentStorage {
	using Destroy = void (*)(void* array, EntityID::Index count, EntityID::Index capacity) noexcept;
	using DestructiveMove = void (*)(void* array, EntityID::Index destinationIndex, EntityID::Index sourceIndex) noexcept;
	using CopyOrClone = void* (*)(void* output, EntityID::Index outputCount, EntityID::Index outputCapacity, const void* input, EntityID::Index inputCount,
		EntityID::Index inputCapacity);

	template <component T>
	static void destroyImplementation(void* array, EntityID::Index count, EntityID::Index capacity) noexcept {
		if constexpr (!detail::empty_component<T>) {
			T* const components = static_cast<T*>(array);
			std::destroy_n(components, static_cast<size_t>(count));
			if (capacity > 0) {
				std::allocator<T>{}.deallocate(components, static_cast<size_t>(capacity));
			}
		}
	}

	template <component T>
	static void destructiveMoveImplementation(void* array, EntityID::Index destinationIndex, EntityID::Index sourceIndex) noexcept {
		if constexpr (!detail::empty_component<T>) {
			T* const components = static_cast<T*>(array);
			if (destinationIndex != sourceIndex) {
				components[destinationIndex] = std::move(components[sourceIndex]);
			}
			std::destroy_at(components + sourceIndex);
		}
	}

	template <component T>
	static void* copyOrCloneImplementation(void* output, EntityID::Index outputCount, EntityID::Index outputCapacity, const void* input, EntityID::Index inputCount,
		EntityID::Index inputCapacity) {
		if constexpr (!copyable<T>) {
			if (inputCount > 0) {
				throw std::logic_error{("Attempted to copy uncopyable entity component \"" + meta::unqualified_type_name_v<T> + "\".").c_str()};
			}
			T* const outputComponents = static_cast<T*>(output);
			std::destroy_n(outputComponents, static_cast<size_t>(outputCount));
			return nullptr;
		} else if constexpr (!detail::empty_component<T>) {
			T* const outputComponents = static_cast<T*>(output);
			const T* const inputComponents = static_cast<const T*>(input);
			if (outputCount >= inputCount) {
				std::copy_n(inputComponents, static_cast<size_t>(inputCount), outputComponents);
				std::destroy_n(outputComponents + inputCount, static_cast<size_t>(outputCount - inputCount));
			} else if (outputCapacity >= inputCount) {
				std::copy_n(inputComponents, static_cast<size_t>(outputCount), outputComponents);
				std::uninitialized_copy_n(inputComponents + outputCount, static_cast<size_t>(inputCount - outputCount), outputComponents + outputCount);
			} else {
				T* const newArray = std::allocator<T>{}.allocate(static_cast<size_t>(inputCapacity));
				try {
					std::uninitialized_copy_n(inputComponents, static_cast<size_t>(inputCount), newArray);
				} catch (...) {
					std::allocator<T>{}.deallocate(newArray, static_cast<size_t>(inputCapacity));
					throw;
				}
				return newArray;
			}
			return nullptr;
		} else {
			return nullptr;
		}
	}

	Buffer<EntityID::Index> indices{};
	Buffer<EntityID> entityIDs{};
	EntityID::Index componentCapacity = 0;
	void* array = nullptr;
	Destroy destroy = nullptr;
	DestructiveMove destructiveMove = nullptr;
	CopyOrClone copyOrClone = nullptr;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	SharedPointer<Atomic<size_t>> currentIteratorGeneration = SharedPointer<Atomic<size_t>>::create();
#endif

	GREM_ALWAYS_INLINE ComponentStorage() = default;

	template <component T>
	GREM_ALWAYS_INLINE ComponentStorage(size_t slotCount, std::in_place_type_t<T>)
		: indices(slotCount, EntityID::INVALID_INDEX)
		, destroy(destroyImplementation<T>)
		, destructiveMove(destructiveMoveImplementation<T>)
		, copyOrClone(copyOrCloneImplementation<T>) {}

	GREM_ALWAYS_INLINE ComponentStorage(size_t slotCount, const ComponentStorage& other)
		: indices(slotCount, EntityID::INVALID_INDEX)
		, destroy(other.destroy)
		, destructiveMove(other.destructiveMove)
		, copyOrClone(other.copyOrClone) {}

	GREM_ALWAYS_INLINE ~ComponentStorage() {
		if (array) {
			destroy(array, static_cast<EntityID::Index>(entityIDs.size()), componentCapacity);
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
	}

	GREM_ALWAYS_INLINE ComponentStorage(const ComponentStorage& other) {
		*this = other;
	}

	GREM_ALWAYS_INLINE ComponentStorage(ComponentStorage&& other) noexcept {
		*this = std::move(other);
	}

	ComponentStorage& operator=(const ComponentStorage& other) {
		if (this == &other) {
			return *this;
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
		if (destroy != other.destroy) {
			if (array) {
				destroy(array, static_cast<EntityID::Index>(entityIDs.size()), componentCapacity);
			}
			indices.clear();
			entityIDs.clear();
			componentCapacity = 0;
			array = nullptr;
			destroy = other.destroy;
			destructiveMove = other.destructiveMove;
			copyOrClone = other.copyOrClone;
		}
		if (other.copyOrClone) {
			const EntityID::Index count = static_cast<EntityID::Index>(entityIDs.size());
			const EntityID::Index capacity = componentCapacity;
			const EntityID::Index otherCount = static_cast<EntityID::Index>(other.entityIDs.size());
			const EntityID::Index otherCapacity = other.componentCapacity;
			void* const newArray = other.copyOrClone(array, count, capacity, other.array, otherCount, otherCapacity);
			if (newArray) {
				if (array) {
					destroy(array, count, capacity);
				}
				array = newArray;
				componentCapacity = other.componentCapacity;
			}
			try {
				entityIDs = other.entityIDs;
				indices = other.indices;
			} catch (...) {
				destroy(array, otherCount, 0);
				entityIDs.clear();
				throw;
			}
		}
		return *this;
	}

	ComponentStorage& operator=(ComponentStorage&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		if (array) {
			destroy(array, static_cast<EntityID::Index>(entityIDs.size()), componentCapacity);
		}
		indices = std::move(other.indices);
		entityIDs = std::move(other.entityIDs);
		componentCapacity = std::exchange(other.componentCapacity, EntityID::Index{0});
		array = std::exchange(other.array, nullptr);
		destroy = other.destroy;
		destructiveMove = other.destructiveMove;
		copyOrClone = other.copyOrClone;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
		std::swap(currentIteratorGeneration, other.currentIteratorGeneration);
#endif
		return *this;
	}

	void clear() noexcept {
		destroy(array, static_cast<EntityID::Index>(entityIDs.size()), 0);
		entityIDs.clear();
		std::fill(indices.begin(), indices.end(), EntityID::INVALID_INDEX);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
	}

	template <component T, typename... Args>
	T& add(EntityID id, Args&&... args) {
		T* const result = addIfMissing<T>(id, std::forward<Args>(args)...);
		if (!result) {
			throw std::logic_error{"Component was already added to entity."};
		}
		return *result;
	}

	template <component T, typename... Args>
	T* addIfMissing(EntityID id, Args&&... args) {
		const EntityID::Index slotIndex = id.getIndex();
		if (indices[slotIndex] != EntityID::INVALID_INDEX) {
			return nullptr;
		}
		const size_t oldCount = entityIDs.size();
		if constexpr (detail::empty_component<T>) {
			entityIDs.push_back(id);
			indices[slotIndex] = static_cast<EntityID::Index>(oldCount);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
			currentIteratorGeneration->fetch_add(1);
#endif
			return &detail::DUMMY_COMPONENT<T>;
		} else {
			const size_t oldCapacity = static_cast<size_t>(componentCapacity);
			if (oldCount >= oldCapacity) {
				GREM_ASSERT(oldCount == oldCapacity);
				const size_t newCapacity = clamp(oldCapacity + oldCapacity / 2, size_t{8}, static_cast<size_t>(static_cast<size_t>(EntityID::MAX_INDEX) + 1));
				GREM_ASSERT(newCapacity > oldCapacity);
				const T* const oldArray = static_cast<const T*>(array);
				T* const newArray = std::allocator<T>{}.allocate(newCapacity);
				try {
					std::uninitialized_move_n(oldArray, oldCount, newArray);
				} catch (...) {
					std::allocator<T>{}.deallocate(newArray, newCapacity);
					throw;
				}
				if (array) {
					destroy(array, static_cast<EntityID::Index>(oldCount), static_cast<EntityID::Index>(oldCapacity));
				}
				array = newArray;
				componentCapacity = static_cast<EntityID::Index>(newCapacity);
			}
			T* const newComponent = std::construct_at(static_cast<T*>(array) + oldCount, std::forward<Args>(args)...);
			try {
				entityIDs.push_back(id);
			} catch (...) {
				std::destroy_at(newComponent);
				throw;
			}
			indices[slotIndex] = static_cast<EntityID::Index>(oldCount);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
			currentIteratorGeneration->fetch_add(1);
#endif
			return newComponent;
		}
	}

	void remove(EntityID::Index slotIndex) noexcept {
		const EntityID::Index componentIndex = indices[slotIndex];
		if (componentIndex != EntityID::INVALID_INDEX) {
			const EntityID::Index lastComponentIndex = static_cast<EntityID::Index>(entityIDs.size() - 1);
			const EntityID lastComponentEntityID = entityIDs[lastComponentIndex];
			indices[lastComponentEntityID.getIndex()] = componentIndex;
			entityIDs[componentIndex] = lastComponentEntityID;
			destructiveMove(array, componentIndex, lastComponentIndex);
			entityIDs.pop_back();
			indices[slotIndex] = EntityID::INVALID_INDEX;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
			currentIteratorGeneration->fetch_add(1);
#endif
		}
	}
};

} // namespace detail

template <typename Component>
class ComponentPool {
public:
	using iterator = detail::SingleComponentIterator<Component>;
	using sentinel = detail::SingleComponentSentinel;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using size_type = size_t;
	using difference_type = typename iterator::difference_type;
	using component_type = Component;

	GREM_ALWAYS_INLINE constexpr ComponentPool() noexcept = default;

	GREM_ALWAYS_INLINE operator ComponentPool<const Component>() const noexcept requires(!std::is_const_v<Component>) {
		return ComponentPool<const Component>{storage, slots};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr iterator begin() const noexcept {
#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return (storage) ? iterator{storage->entityIDs.data(), static_cast<Component*>(storage->array), storage->indices.data(), storage->array, storage->indices.size()}
		                 : iterator{nullptr, nullptr, nullptr, nullptr, 0};
#else
		return (storage)
		           ? iterator{storage->entityIDs.data(), static_cast<Component*>(storage->array), storage->indices.data(), storage->array, storage->indices.size(), validGeneration}
		           : iterator{nullptr, nullptr, nullptr, nullptr, 0, validGeneration};
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr sentinel end() const noexcept {
		return (storage) ? sentinel{storage->entityIDs.data() + storage->entityIDs.size()} : sentinel{nullptr};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool containsEntity(EntityID id) const {
		if (!storage) {
			return false;
		}
		GREM_ASSERT(storage->indices.size() == slots.size());
		return isValidEntity(id) && storage->indices[id.getIndex()] != EntityID::INVALID_INDEX;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr reference operator[](EntityID id) const {
		GREM_ASSERT(storage);
		GREM_ASSERT(containsEntity(id));
		if constexpr (detail::empty_component<Component>) {
			return {id, detail::DUMMY_COMPONENT<Component>};
		} else {
			return {id, static_cast<Component*>(storage->array)[storage->indices[id.getIndex()]]};
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component* find(EntityID id) const {
		if (!storage) {
			return nullptr;
		}
		GREM_ASSERT(storage->indices.size() == slots.size());
		if (isValidEntity(id)) {
			const EntityID::Index componentIndex = storage->indices[id.getIndex()];
			if (componentIndex != EntityID::INVALID_INDEX) {
				if constexpr (detail::empty_component<Component>) {
					return &detail::DUMMY_COMPONENT<Component>;
				} else {
					return static_cast<Component*>(storage->array) + componentIndex;
				}
			}
		}
		return nullptr;
	}

	template <typename... Args>
	GREM_ALWAYS_INLINE Component& add(EntityID id, Args&&... args) requires(!std::is_const_v<Component>) {
		if (!storage) {
			throw std::logic_error{("The component pool required to add component \"" + meta::unqualified_type_name_v<Component> + "\" does not exist.").c_str()};
		}
		if (!isValidEntity(id)) {
			throw std::out_of_range{"Invalid entity handle."};
		}
		return storage->template add<Component>(id, std::forward<Args>(args)...);
	}

	template <typename... Args>
	GREM_ALWAYS_INLINE Component* addIfMissing(EntityID id, Args&&... args) requires(!std::is_const_v<Component>) {
		if (!storage) {
			throw std::logic_error{("The component pool required to add component \"" + meta::unqualified_type_name_v<Component> + "\" does not exist.").c_str()};
		}
		if (!isValidEntity(id)) {
			throw std::out_of_range{"Invalid entity handle."};
		}
		return storage->template addIfMissing<Component>(id, std::forward<Args>(args)...);
	}

	GREM_ALWAYS_INLINE Component& addOrAssign(EntityID id, Component&& value) requires(!std::is_const_v<Component>) {
		if (Component* const result = find(id)) {
			return *result = std::move(value);
		}
		return add(id, std::move(value));
	}

	GREM_ALWAYS_INLINE Component& addOrAssign(EntityID id, const Component& value) requires(!std::is_const_v<Component>) {
		if (Component* const result = find(id)) {
			return *result = value;
		}
		return add(id, value);
	}

	GREM_ALWAYS_INLINE Component& get(EntityID id) const {
		Component* const result = find(id);
		if (!result) {
			throw std::out_of_range{("Component \"" + meta::unqualified_type_name_v<std::remove_const_t<Component>> + "\" not found for the specified entity.").c_str()};
		}
		return *result;
	}

	GREM_ALWAYS_INLINE bool remove(EntityID id) requires(!std::is_const_v<Component>) {
		if (!containsEntity(id)) {
			return false;
		}
		storage->remove(id.getIndex());
		return true;
	}

	GREM_ALWAYS_INLINE size_type removeFromAllEntities() requires(!std::is_const_v<Component>) {
		if (storage) {
			const size_type result = storage->entityIDs.size();
			storage->clear();
			return result;
		}
		return 0;
	}

private:
	template <typename... KnownComponents>
	friend class EntityRegistry;

	template <typename U>
	friend class ComponentPool;

	GREM_ALWAYS_INLINE constexpr ComponentPool(std::conditional_t<std::is_const_v<Component>, const detail::ComponentStorage, detail::ComponentStorage>* storage,
		Span<const detail::EntitySlot> slots) noexcept
		: storage(storage)
		, slots(slots) {
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		if (storage) {
			validGeneration = storage->currentIteratorGeneration;
		}
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool isValidEntity(EntityID id) const {
		return id.getIndex() < slots.size() && (id.getGeneration() & 1) != 0 && slots[id.getIndex()].generation == id.getGeneration();
	}

	std::conditional_t<std::is_const_v<Component>, const detail::ComponentStorage, detail::ComponentStorage>* storage = nullptr;
	Span<const detail::EntitySlot> slots{};
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	SharedPointer<Atomic<size_t>> validGeneration{};
#endif
};

template <typename EntReg>
class EntityBuilder {
public:
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr EntityBuilder(EntReg& registry, EntityID entityID) noexcept
		: registry(registry)
		, entityID(entityID) {}

	~EntityBuilder();

	EntityBuilder(const EntityBuilder&) = delete;
	EntityBuilder(EntityBuilder&&) = default;

	EntityBuilder& operator=(const EntityBuilder&) = delete;
	EntityBuilder& operator=(EntityBuilder&&) = delete;

	EntityID setEntityFlags(EntityID::Flags newFlags);

	[[nodiscard]] GREM_ALWAYS_INLINE EntityID getEntityID() const noexcept {
		return entityID;
	}

	template <component T, typename... Args>
	T& addComponent(Args&&... args);

	template <component T, typename... Args>
	T* addComponentIfMissing(Args&&... args);

	template <component T>
	T& addOrAssignComponent(T&& value);

	template <component T>
	T& addOrAssignComponent(const T& value);

	template <component T>
	bool removeComponent() noexcept;

	bool removeAllComponents() noexcept;

	template <component T>
	[[nodiscard]] bool hasComponent() const noexcept;

	template <component T>
	[[nodiscard]] T& getComponent();

	template <component T>
	[[nodiscard]] const T& getComponent() const;

	template <component T, typename U>
	[[nodiscard]] T getComponentOr(U&& defaultValue) const;

	template <component T>
	[[nodiscard]] T* findComponent() noexcept;

	template <component T>
	[[nodiscard]] const T* findComponent() const noexcept;

	template <typename Callback, typename... Args>
	GREM_ALWAYS_INLINE EntityBuilder& extend(Callback&& callback, Args&&... args) { // NOLINT(cppcoreguidelines-missing-std-forward)
		callback(registry, EntityID{entityID}, std::forward<Args>(args)...);
		return *this;
	}

	GREM_ALWAYS_INLINE EntityID build() {
		return std::exchange(entityID, EntityID{});
	}

private:
	EntReg& registry;
	EntityID entityID;
};

template <typename Registry>
EntityBuilder(Registry&, EntityID) -> EntityBuilder<Registry>;

template <typename... KnownComponents>
class EntityRegistry {
private:
	using KnownComponentTypeList = meta::TypeList<KnownComponents...>;

public:
	static_assert((component<KnownComponents> && ...));

	using value_type = EntityID;
	using reference = EntityID&;
	using const_reference = const EntityID&;
	using pointer = EntityID*;
	using const_pointer = const EntityID*;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using iterator = typename Buffer<EntityID>::iterator;
	using const_iterator = typename Buffer<EntityID>::const_iterator;

	EntityRegistry() noexcept = default;

#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	~EntityRegistry() {
		currentIteratorGeneration->fetch_add(1);
	}
#else
	~EntityRegistry() = default;
#endif

	EntityRegistry(const EntityRegistry& other) {
		*this = other;
	}

	EntityRegistry(EntityRegistry&& other) noexcept {
		*this = std::move(other);
	}

	EntityRegistry& operator=(const EntityRegistry& other) {
		if (this == &other) {
			return *this;
		}
		try {
			slots = other.slots;
			entityIDs = other.entityIDs;
			firstAvailableSlotIndex = other.firstAvailableSlotIndex;
			for (auto&& [typeIndex, storage] : componentPools) {
				if (!other.componentPools.contains(typeIndex)) {
					storage.clear();
					storage.indices.resize(slots.size(), EntityID::INVALID_INDEX);
				}
			}
			for (const auto& [typeIndex, otherStorage] : other.componentPools) {
				componentPools[typeIndex] = otherStorage;
			}
			for (size_t i = 0; i < sizeof...(KnownComponents); ++i) {
				knownComponentPools[i] = other.knownComponentPools[i];
			}
		} catch (...) {
			slots.clear();
			entityIDs.clear();
			firstAvailableSlotIndex = EntityID::INVALID_INDEX;
			componentPools.clear();
			for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
				if (storage) {
					storage->clear();
				}
			}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
			currentIteratorGeneration->fetch_add(1);
#endif
			throw;
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
		return *this;
	}

	EntityRegistry& operator=(EntityRegistry&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		slots = std::exchange(other.slots, {});
		entityIDs = std::exchange(other.entityIDs, {});
		firstAvailableSlotIndex = std::exchange(other.firstAvailableSlotIndex, EntityID::Index{0});
		componentPools = std::exchange(other.componentPools, {});
		knownComponentPools = std::exchange(other.knownComponentPools, {});
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
		std::swap(currentIteratorGeneration, other.currentIteratorGeneration);
#endif
		return *this;
	}

	void clear() noexcept {
		entityIDs.clear();

		// Make all generation entries available for reuse.
		for (EntityID::Index slotIndex = 0; slotIndex < slots.size(); ++slotIndex) {
			const EntityID::Generation generation = slots[slotIndex].generation;
			const EntityID::Generation destroyedGeneration = generation + (generation & 1); // Make sure that the generation counter is even (marking invalid).
			const EntityID::Index previousAvailableSlotIndex = (slotIndex == 0) ? EntityID::INVALID_INDEX : static_cast<EntityID::Index>(slotIndex - 1);
			const EntityID::Index nextAvailableSlotIndex = (slotIndex == slots.size() - 1) ? EntityID::INVALID_INDEX : static_cast<EntityID::Index>(slotIndex + 1);
			slots[slotIndex] = detail::EntitySlot{
				.generation = destroyedGeneration,
				.inactive{.previousAvailableSlotIndex = previousAvailableSlotIndex, .nextAvailableSlotIndex = nextAvailableSlotIndex},
			};
		}
		firstAvailableSlotIndex = (slots.empty()) ? EntityID::INVALID_INDEX : EntityID::Index{0};

		for (auto&& [typeIndex, storage] : componentPools) {
			storage.clear();
		}
		for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
			if (storage) {
				storage->clear();
			}
		}

#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
	}

	[[nodiscard]] EntityBuilder<EntityRegistry> createEntity(EntityID::Flags flags = {}) {
		if (size() >= max_size()) {
			throw std::length_error{"Maximum entity count exceeded."};
		}

		const EntityID::Index idIndex = static_cast<EntityID::Index>(entityIDs.size());
		EntityID& id = entityIDs.push_back_unspecified_value();

		if (firstAvailableSlotIndex == EntityID::INVALID_INDEX) {
			const EntityID::Index slotIndex = static_cast<EntityID::Index>(slots.size());
			GREM_ASSERT(slotIndex == idIndex);
			try {
				const size_t oldSlotCount = slots.size();
				slots.push_back(detail::EntitySlot{.generation = 1, .active{.idIndex = idIndex, .flags = flags}});
				try {
					for (auto&& [typeIndex, storage] : componentPools) {
						storage.indices.push_back(EntityID::INVALID_INDEX);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
						storage.currentIteratorGeneration->fetch_add(1);
#endif
					}
					for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
						if (storage) {
							storage->indices.push_back(EntityID::INVALID_INDEX);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
							storage->currentIteratorGeneration->fetch_add(1);
#endif
						}
					}
				} catch (...) {
					for (auto&& [typeIndex, storage] : componentPools) {
						storage.indices.resize(oldSlotCount);
					}
					for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
						if (storage) {
							storage->indices.resize(oldSlotCount);
						}
					}
					slots.pop_back();
					throw;
				}
			} catch (...) {
				entityIDs.pop_back();
				throw;
			}
			id = EntityID{slotIndex, 1, flags};
		} else {
			const EntityID::Index slotIndex = firstAvailableSlotIndex;
			const EntityID::Index nextAvailableSlotIndex = slots[slotIndex].inactive.nextAvailableSlotIndex;
			if (nextAvailableSlotIndex != EntityID::INVALID_INDEX) {
				slots[nextAvailableSlotIndex].inactive.previousAvailableSlotIndex = slotIndex;
			}
			firstAvailableSlotIndex = nextAvailableSlotIndex;
			const EntityID::Generation generation = slots[slotIndex].generation + 1;
			slots[slotIndex] = detail::EntitySlot{.generation = generation, .active{.idIndex = idIndex, .flags = flags}};
			id = EntityID{slotIndex, generation, flags};
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
		return EntityBuilder{*this, id};
	}

	[[nodiscard]] EntityBuilder<EntityRegistry> createEntityAtID(EntityID id) {
		if (id.getIndex() == EntityID::INVALID_INDEX || (id.getGeneration() & 1) == 0) {
			throw std::invalid_argument{"Invalid entity handle."};
		}

		if (containsEntityAtIndex(id.getIndex())) {
			throw std::logic_error{"An entity already exists at the index of the given ID."};
		}

		if (size() >= max_size()) {
			throw std::length_error{"Maximum entity count exceeded."};
		}

		const EntityID::Index idIndex = static_cast<EntityID::Index>(entityIDs.size());
		entityIDs.push_back(id);

		const EntityID::Index slotIndex = id.getIndex();
		const EntityID::Generation generation = id.getGeneration();
		const EntityID::Flags flags = id.getFlags();
		if (slotIndex >= slots.size()) {
			try {
				const size_t oldSlotCount = slots.size();
				const size_t newSlotCount = static_cast<size_t>(slotIndex + 1);
				slots.reserve(newSlotCount);
				for (size_t newSlotIndex = oldSlotCount; newSlotIndex < newSlotCount - 1; ++newSlotIndex) {
					const EntityID::Index previousAvailableSlotIndex = (newSlotIndex == oldSlotCount) ? EntityID::INVALID_INDEX : static_cast<EntityID::Index>(newSlotIndex - 1);
					const EntityID::Index nextAvailableSlotIndex = (newSlotIndex == newSlotCount - 2) ? EntityID::INVALID_INDEX : static_cast<EntityID::Index>(newSlotIndex + 1);
					slots.push_back(detail::EntitySlot{
						.generation = 0,
						.inactive{.previousAvailableSlotIndex = previousAvailableSlotIndex, .nextAvailableSlotIndex = nextAvailableSlotIndex},
					});
				}
				if (newSlotCount - oldSlotCount >= 2) {
					if (firstAvailableSlotIndex != EntityID::INVALID_INDEX) {
						slots.back().inactive.nextAvailableSlotIndex = firstAvailableSlotIndex;
					}
					firstAvailableSlotIndex = static_cast<EntityID::Index>(oldSlotCount);
				}
				slots.push_back(detail::EntitySlot{.generation = generation, .active{.idIndex = idIndex, .flags = flags}});
				try {
					for (auto&& [typeIndex, storage] : componentPools) {
						storage.indices.resize(newSlotCount, EntityID::INVALID_INDEX);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
						storage.currentIteratorGeneration->fetch_add(1);
#endif
					}
					for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
						if (storage) {
							storage->indices.resize(newSlotCount, EntityID::INVALID_INDEX);
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
							storage->currentIteratorGeneration->fetch_add(1);
#endif
						}
					}
				} catch (...) {
					for (auto&& [typeIndex, storage] : componentPools) {
						storage.indices.resize(oldSlotCount);
					}
					for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
						if (storage) {
							storage->indices.resize(oldSlotCount);
						}
					}
					slots.resize(oldSlotCount);
					throw;
				}
			} catch (...) {
				entityIDs.pop_back();
				throw;
			}
		} else {
			const EntityID::Index previousAvailableSlotIndex = slots[slotIndex].inactive.previousAvailableSlotIndex;
			const EntityID::Index nextAvailableSlotIndex = slots[slotIndex].inactive.nextAvailableSlotIndex;
			if (previousAvailableSlotIndex == EntityID::INVALID_INDEX) {
				firstAvailableSlotIndex = nextAvailableSlotIndex;
			} else {
				slots[previousAvailableSlotIndex].inactive.nextAvailableSlotIndex = nextAvailableSlotIndex;
			}
			if (nextAvailableSlotIndex != EntityID::INVALID_INDEX) {
				slots[nextAvailableSlotIndex].inactive.previousAvailableSlotIndex = previousAvailableSlotIndex;
			}
			slots[slotIndex] = detail::EntitySlot{.generation = generation, .active{.idIndex = idIndex, .flags = flags}};
		}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
		return EntityBuilder{*this, id};
	}

	EntityID setEntityFlags(EntityID id, EntityID::Flags newFlags) {
		const EntityID::Index slotIndex = id.getIndex();
		if (slotIndex < slots.size() && (id.getGeneration() & 1) != 0) {
			detail::EntitySlot& slot = slots[slotIndex];
			if (slot.generation == id.getGeneration()) {
				const EntityID::Index idIndex = slot.active.idIndex;
				slot.active.flags = newFlags;
				const EntityID newEntityID{slotIndex, id.getGeneration(), newFlags};
				GREM_ASSERT(entityIDs[idIndex].getGeneration() == id.getGeneration());
				entityIDs[idIndex] = newEntityID;
				for (auto&& [typeIndex, storage] : componentPools) {
					const EntityID::Index componentIndex = storage.indices[slotIndex];
					if (componentIndex != EntityID::INVALID_INDEX) {
						GREM_ASSERT(storage.entityIDs[componentIndex].getGeneration() == id.getGeneration());
						storage.entityIDs[componentIndex] = newEntityID;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
						storage.currentIteratorGeneration->fetch_add(1);
#endif
					}
				}
				for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
					if (storage) {
						const EntityID::Index componentIndex = storage->indices[slotIndex];
						if (componentIndex != EntityID::INVALID_INDEX) {
							GREM_ASSERT(storage->entityIDs[componentIndex].getGeneration() == id.getGeneration());
							storage->entityIDs[componentIndex] = newEntityID;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
							storage->currentIteratorGeneration->fetch_add(1);
#endif
						}
					}
				}
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
				currentIteratorGeneration->fetch_add(1);
#endif
				return newEntityID;
			}
		}
		return {};
	}

	GREM_ALWAYS_INLINE bool destroyEntity(EntityID id) noexcept {
		if (!containsEntity(id)) {
			return false;
		}
		const bool result = destroyEntityAtIndex(id.getIndex());
		GREM_ASSERT(result);
		return result;
	}

	bool destroyEntityAtIndex(EntityID::Index slotIndex) noexcept {
		if (!containsEntityAtIndex(slotIndex)) {
			return false;
		}

		GREM_ASSERT(!entityIDs.empty());

		// Move the last entity's data to the destroyed entity's index.
		const EntityID::Index idIndex = slots[slotIndex].active.idIndex;
		const EntityID::Generation generation = slots[slotIndex].generation;
		const EntityID lastEntityID = entityIDs.back();
		const EntityID::Index lastSlotIndex = lastEntityID.getIndex();
		if (lastSlotIndex != slotIndex) {
			slots[lastSlotIndex] = detail::EntitySlot{.generation = lastEntityID.getGeneration(),
				.active{.idIndex = idIndex, .flags = lastEntityID.getFlags()}}; // Repoint the last entity's slot to its new index.
			entityIDs[idIndex] = lastEntityID;
		}
		for (auto&& [typeIndex, storage] : componentPools) {
			storage.remove(slotIndex);
		}
		for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
			if (storage) {
				storage->remove(slotIndex);
			}
		}
		entityIDs.pop_back();

		// Make the destroyed entity's slot available for reuse.
		slots[slotIndex] = detail::EntitySlot{.generation = static_cast<EntityID::Generation>(generation + 1),
			.inactive{.previousAvailableSlotIndex = EntityID::INVALID_INDEX, .nextAvailableSlotIndex = firstAvailableSlotIndex}};
		firstAvailableSlotIndex = slotIndex;

#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		currentIteratorGeneration->fetch_add(1);
#endif
		return true;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE iterator begin() noexcept {
		return entityIDs.begin();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const_iterator begin() const noexcept {
		return entityIDs.begin();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE iterator end() noexcept {
		return entityIDs.end();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const_iterator end() const noexcept {
		return entityIDs.end();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool containsEntity(EntityID id) const noexcept {
		const EntityID::Index slotIndex = id.getIndex();
		return slotIndex < slots.size() && (id.getGeneration() & 1) != 0 && slots[slotIndex].generation == id.getGeneration();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool containsEntityAtIndex(EntityID::Index slotIndex) const {
		return slotIndex < slots.size() && (slots[slotIndex].generation & 1) != 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_type size() const noexcept {
		return entityIDs.size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool empty() const noexcept {
		return entityIDs.empty();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_type max_size() const noexcept {
		return static_cast<size_type>(EntityID::MAX_INDEX) + 1;
	}

	template <component T, typename... Args>
	GREM_ALWAYS_INLINE T& addComponent(EntityID id, Args&&... args) {
		return createComponentPool<T>().add(id, std::forward<Args>(args)...);
	}

	template <component T, typename... Args>
	GREM_ALWAYS_INLINE T* addComponentIfMissing(EntityID id, Args&&... args) {
		return createComponentPool<T>().addIfMissing(id, std::forward<Args>(args)...);
	}

	template <component T>
	GREM_ALWAYS_INLINE T& addOrAssignComponent(EntityID id, T&& value) {
		return createComponentPool<std::decay_t<T>>().addOrAssign(id, std::forward<T>(value));
	}

	template <component T>
	GREM_ALWAYS_INLINE T& addOrAssignComponent(EntityID id, const T& value) {
		return createComponentPool<T>().addOrAssign(id, value);
	}

	template <component T>
	GREM_ALWAYS_INLINE bool removeComponent(EntityID id) noexcept {
		return getComponentPool<T>().remove(id);
	}

	GREM_ALWAYS_INLINE bool removeAllComponents(EntityID id) noexcept {
		if (!containsEntity(id)) {
			return false;
		}
		for (auto&& [typeIndex, storage] : componentPools) {
			storage.remove(id.getIndex());
		}
		for (Optional<detail::ComponentStorage>& storage : knownComponentPools) {
			if (storage) {
				storage->remove(id.getIndex());
			}
		}
		return true;
	}

	template <component T>
	size_type GREM_ALWAYS_INLINE removeComponentFromAllEntities() noexcept {
		return getComponentPool<T>().removeFromAllEntities();
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasComponent(EntityID id) const noexcept {
		return findComponent<T>(id) != nullptr;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE T& getComponent(EntityID id) {
		T* const result = findComponent<T>(id);
		if (!result) {
			throw std::out_of_range{("Component \"" + meta::unqualified_type_name_v<T> + "\" not found for the specified entity.").c_str()};
		}
		return *result;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE const T& getComponent(EntityID id) const {
		const T* const result = findComponent<T>(id);
		if (!result) {
			throw std::out_of_range{("Component \"" + meta::unqualified_type_name_v<T> + "\" not found for the specified entity.").c_str()};
		}
		return *result;
	}

	template <component T, typename U>
	[[nodiscard]] GREM_ALWAYS_INLINE T getComponentOr(EntityID id, U&& defaultValue) const {
		const T* const result = findComponent<T>(id);
		if (!result) {
			return std::forward<U>(defaultValue);
		}
		return *result;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE T* findComponent(EntityID id) noexcept {
		if (containsEntity(id)) {
			if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
				if (Optional<detail::ComponentStorage>& storage = knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>]) {
					GREM_ASSERT(storage->indices.size() == slots.size());
					const EntityID::Index componentIndex = storage->indices[id.getIndex()];
					if (componentIndex != EntityID::INVALID_INDEX) {
						if constexpr (detail::empty_component<T>) {
							return &detail::DUMMY_COMPONENT<T>;
						} else {
							return static_cast<T*>(storage->array) + componentIndex;
						}
					}
				}
			} else {
				if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
					detail::ComponentStorage& storage = it->second;
					GREM_ASSERT(storage.indices.size() == slots.size());
					const EntityID::Index componentIndex = storage.indices[id.getIndex()];
					if (componentIndex != EntityID::INVALID_INDEX) {
						if constexpr (detail::empty_component<T>) {
							return &detail::DUMMY_COMPONENT<T>;
						} else {
							return static_cast<T*>(storage.array) + componentIndex;
						}
					}
				}
			}
		}
		return nullptr;
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE const T* findComponent(EntityID id) const noexcept {
		if (containsEntity(id)) {
			if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
				if (const Optional<detail::ComponentStorage>& storage = knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>]) {
					GREM_ASSERT(storage->indices.size() == slots.size());
					const EntityID::Index componentIndex = storage->indices[id.getIndex()];
					if (componentIndex != EntityID::INVALID_INDEX) {
						if constexpr (detail::empty_component<T>) {
							return &detail::DUMMY_COMPONENT<const T>;
						} else {
							return static_cast<const T*>(storage->array) + componentIndex;
						}
					}
				}
			} else {
				if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
					const detail::ComponentStorage& storage = it->second;
					GREM_ASSERT(storage.indices.size() == slots.size());
					const EntityID::Index componentIndex = storage.indices[id.getIndex()];
					if (componentIndex != EntityID::INVALID_INDEX) {
						if constexpr (detail::empty_component<T>) {
							return &detail::DUMMY_COMPONENT<const T>;
						} else {
							return static_cast<const T*>(storage.array) + componentIndex;
						}
					}
				}
			}
		}
		return nullptr;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE EntityID getEntityIDAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		GREM_ASSERT((slots[slotIndex].generation & 1) != 0);
		return EntityID{slotIndex, slots[slotIndex].generation, slots[slotIndex].active.flags};
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE T& getComponentAtEntityIndexUnsafe(EntityID::Index slotIndex) {
		if constexpr (detail::empty_component<T>) {
			return detail::DUMMY_COMPONENT<T>;
		} else if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
			detail::ComponentStorage& storage = *knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>];
			GREM_ASSERT(slotIndex < storage.indices.size());
			const EntityID::Index componentIndex = storage.indices[slotIndex];
			GREM_ASSERT(componentIndex != EntityID::INVALID_INDEX);
			return static_cast<T*>(storage.array)[componentIndex];
		} else {
			const auto it = componentPools.find(typeid(T));
			GREM_ASSERT(it != componentPools.end());
			detail::ComponentStorage& storage = it->second;
			GREM_ASSERT(slotIndex < storage.indices.size());
			const EntityID::Index componentIndex = storage.indices[slotIndex];
			GREM_ASSERT(componentIndex != EntityID::INVALID_INDEX);
			return static_cast<T*>(storage.array)[componentIndex];
		}
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE const T& getComponentAtEntityIndexUnsafe(EntityID::Index slotIndex) const {
		if constexpr (detail::empty_component<T>) {
			return detail::DUMMY_COMPONENT<const T>;
		} else if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
			const detail::ComponentStorage& storage = *knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>];
			GREM_ASSERT(slotIndex < storage.indices.size());
			const EntityID::Index componentIndex = storage.indices[slotIndex];
			GREM_ASSERT(componentIndex != EntityID::INVALID_INDEX);
			return static_cast<const T*>(storage.array)[componentIndex];
		} else {
			const auto it = componentPools.find(typeid(T));
			GREM_ASSERT(it != componentPools.end());
			const detail::ComponentStorage& storage = it->second;
			GREM_ASSERT(slotIndex < storage.indices.size());
			const EntityID::Index componentIndex = storage.indices[slotIndex];
			GREM_ASSERT(componentIndex != EntityID::INVALID_INDEX);
			return static_cast<const T*>(storage.array)[componentIndex];
		}
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] GREM_ALWAYS_INLINE Entities<ComponentsAndExclusions...> getEntities() noexcept
		requires(!meta::type_list_empty_v<typename Entities<ComponentsAndExclusions...>::MutableComponents>) {
		using EntityRange = Entities<ComponentsAndExclusions...>;
		return getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{}, typename EntityRange::ExcludedComponents{});
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] GREM_ALWAYS_INLINE Entities<ComponentsAndExclusions...> getEntities() const noexcept
		requires(meta::type_list_empty_v<typename Entities<ComponentsAndExclusions...>::MutableComponents>) {
		using EntityRange = Entities<ComponentsAndExclusions...>;
		return const_cast<EntityRegistry*>(this)->getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{}, typename EntityRange::ExcludedComponents{});
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] GREM_ALWAYS_INLINE Entities<ComponentsAndExclusions...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) noexcept
		requires(!meta::type_list_empty_v<typename Entities<ComponentsAndExclusions...>::MutableComponents>) {
		using EntityRange = Entities<ComponentsAndExclusions...>;
		return getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{}, typename EntityRange::ExcludedComponents{});
	}

	template <typename... ComponentsAndExclusions>
	[[nodiscard]] GREM_ALWAYS_INLINE Entities<ComponentsAndExclusions...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) const noexcept
		requires(meta::type_list_empty_v<typename Entities<ComponentsAndExclusions...>::MutableComponents>) {
		using EntityRange = Entities<ComponentsAndExclusions...>;
		return const_cast<EntityRegistry*>(this)->getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{},
			typename EntityRange::ExcludedComponents{});
	}

	template <component T>
	GREM_ALWAYS_INLINE ComponentPool<T> createComponentPool() {
		if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
			Optional<detail::ComponentStorage>& storage = get<meta::type_list_index_v<KnownComponentTypeList, T>>(knownComponentPools);
			if (!storage) {
				storage.emplace(slots.size(), std::in_place_type<T>);
			}
			return ComponentPool<T>{&*storage, slots};
		} else {
			detail::ComponentStorage& storage = componentPools.try_emplace(typeid(T), slots.size(), std::in_place_type<T>).first->second;
			return ComponentPool<T>{&storage, slots};
		}
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentPool<T> getComponentPool() noexcept {
		if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
			if (Optional<detail::ComponentStorage>& storage = get<meta::type_list_index_v<KnownComponentTypeList, T>>(knownComponentPools)) {
				return ComponentPool<T>{&*storage, slots};
			}
			return ComponentPool<T>{};
		} else {
			if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
				return ComponentPool<T>{&it->second, slots};
			}
			return ComponentPool<T>{};
		}
	}

	template <component T>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentPool<const T> getComponentPool() const noexcept {
		if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
			if (const Optional<detail::ComponentStorage>& storage = get<meta::type_list_index_v<KnownComponentTypeList, T>>(knownComponentPools)) {
				return ComponentPool<const T>{&*storage, slots};
			}
			return ComponentPool<const T>{};
		} else {
			if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
				return ComponentPool<const T>{&it->second, slots};
			}
			return ComponentPool<const T>{};
		}
	}

	template <typename... ComponentsAndExclusions>
	size_type destroyEntities() noexcept {
		size_type result = 0;
		while (true) {
			const auto entities = getEntities<ComponentsAndExclusions...>();
			const auto first = entities.begin();
			const auto last = entities.end();
			if (first == last) {
				break;
			}
			destroyEntity(get<const EntityID>(*first));
			++result;
		}
		return result;
	}

	template <typename Predicate>
	friend size_type erase_if(EntityRegistry& c, Predicate predicate) {
		size_type result = 0;
		size_type i = 0;
		while (i < c.entityIDs.size()) {
			const EntityID id = c.entityIDs[i];
			if (predicate(id)) {
				c.destroyEntity(id);
				++result;
			} else {
				++i;
			}
		}
		return result;
	}

private:
	template <typename EntityRange, typename... IncludedComponents, typename... ExcludedComponents>
	[[nodiscard]] EntityRange getEntitiesImplementation(meta::TypeList<IncludedComponents...>, meta::TypeList<ExcludedComponents...>) noexcept {
		Optional<Span<const EntityID>> candidateEntities{};
		Array<const EntityID::Index*, sizeof...(ExcludedComponents)> excludedIndexArrays{};
		Array<const EntityID::Index*, sizeof...(IncludedComponents)> componentIndexArrays{};
		Array<void*, detail::NON_EMPTY_COMPONENT_COUNT<IncludedComponents...>> componentArrays{};
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
		Array<detail::IteratorValidity, sizeof...(ExcludedComponents)> excludedIteratorValidity{};
		Array<detail::IteratorValidity, max(sizeof...(IncludedComponents), size_t{1})> componentIteratorValidity{};
#endif

		[[maybe_unused]] size_t excludedComponentIndex = 0;
		(([&]<typename Component>(meta::Type<Component>) -> void {
			static_assert(!std::is_const_v<Component>);
			using T = Component;
			if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
				if (Optional<detail::ComponentStorage>& storage = knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>]) {
					excludedIndexArrays[excludedComponentIndex] = storage->indices.data();
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
					excludedIteratorValidity[excludedComponentIndex] = storage->currentIteratorGeneration;
#endif
				}
			} else {
				if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
					excludedIndexArrays[excludedComponentIndex] = it->second.indices.data();
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
					excludedIteratorValidity[excludedComponentIndex] = it->second.currentIteratorGeneration;
#endif
				}
			}
			++excludedComponentIndex;
		}(meta::TYPE<ExcludedComponents>)),
			...);

		[[maybe_unused]] size_t includedComponentIndex = 0;
		[[maybe_unused]] size_t nonEmptyIncludedComponentIndex = 0;
		if constexpr (sizeof...(IncludedComponents) == 0) {
			candidateEntities = entityIDs;
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
			componentIteratorValidity.front() = currentIteratorGeneration;
#endif
		} else {
			(([&]<typename Component>(meta::Type<Component>) -> void {
				using T = std::remove_const_t<Component>;
				if constexpr (meta::type_list_contains_v<KnownComponentTypeList, T>) {
					if (Optional<detail::ComponentStorage>& storage = knownComponentPools[meta::type_list_index_v<KnownComponentTypeList, T>]) {
						componentIndexArrays[includedComponentIndex] = storage->indices.data();
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
						componentIteratorValidity[includedComponentIndex] = storage->currentIteratorGeneration;
#endif
						if constexpr (!detail::empty_component<Component>) {
							componentArrays[nonEmptyIncludedComponentIndex] = storage->array;
						}
						const Span<const EntityID> componentEntityIDs = storage->entityIDs;
						if (!candidateEntities || componentEntityIDs.size() < candidateEntities->size()) {
							candidateEntities = componentEntityIDs;
						}
					} else {
						candidateEntities.emplace();
					}
				} else {
					if (const auto it = componentPools.find(typeid(T)); it != componentPools.end()) {
						componentIndexArrays[includedComponentIndex] = it->second.indices.data();
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
						componentIteratorValidity[includedComponentIndex] = it->second.currentIteratorGeneration;
#endif
						if constexpr (!detail::empty_component<Component>) {
							componentArrays[nonEmptyIncludedComponentIndex] = it->second.array;
						}
						const Span<const EntityID> componentEntityIDs = it->second.entityIDs;
						if (!candidateEntities || componentEntityIDs.size() < candidateEntities->size()) {
							candidateEntities = componentEntityIDs;
						}
					} else {
						candidateEntities.emplace();
					}
				}
				++includedComponentIndex;
				if constexpr (!detail::empty_component<Component>) {
					++nonEmptyIncludedComponentIndex;
				}
			}(meta::TYPE<IncludedComponents>)),
				...);
		}

#if defined(NDEBUG) && !defined(GREM_USE_RELEASE_ASSERTIONS)
		return EntityRange{candidateEntities.value_or(Span<const EntityID>{}), excludedIndexArrays, componentIndexArrays, componentArrays, slots};
#else
		return EntityRange{candidateEntities.value_or(Span<const EntityID>{}), excludedIndexArrays, componentIndexArrays, componentArrays, slots, excludedIteratorValidity,
			componentIteratorValidity};
#endif
	}

	template <typename EntityRange, typename... IncludedComponents, typename... ExcludedComponents>
	[[nodiscard]] EntityRange getEntitiesChunkImplementation(size_t chunkIndex, size_t chunkCount, meta::TypeList<IncludedComponents...>,
		meta::TypeList<ExcludedComponents...>) noexcept {
		EntityRange result = getEntitiesImplementation<EntityRange>(meta::TYPE_LIST<IncludedComponents...>, meta::TYPE_LIST<ExcludedComponents...>);
		const size_t candidateCount = result.getCandidateCount();
		GREM_ASSERT(chunkIndex < chunkCount);
		const size_t chunkSize = (candidateCount + chunkCount - 1) / chunkCount;
		const size_t chunkBegin = min(chunkIndex * chunkSize, candidateCount);
		const size_t chunkEnd = min(chunkBegin + chunkSize, candidateCount);
		result.chunkCandidates(chunkBegin, chunkEnd);
		return result;
	}

	Buffer<detail::EntitySlot> slots{};
	Buffer<EntityID> entityIDs{};
	EntityID::Index firstAvailableSlotIndex = EntityID::INVALID_INDEX;
	HashMap<std::type_index, detail::ComponentStorage> componentPools{};
	Array<Optional<detail::ComponentStorage>, sizeof...(KnownComponents)> knownComponentPools{};
#if !defined(NDEBUG) || defined(GREM_USE_RELEASE_ASSERTIONS)
	SharedPointer<Atomic<size_t>> currentIteratorGeneration = SharedPointer<Atomic<size_t>>::create();
#endif
};

template <typename EntReg>
inline EntityBuilder<EntReg>::~EntityBuilder() {
	if (entityID) {
		registry.destroyEntity(entityID);
	}
}

template <typename EntReg>
inline EntityID EntityBuilder<EntReg>::setEntityFlags(EntityID::Flags newFlags) {
	entityID = registry.setEntityFlags(entityID, newFlags);
	return entityID;
}

template <typename EntReg>
template <component T, typename... Args>
inline T& EntityBuilder<EntReg>::addComponent(Args&&... args) {
	return registry.template addComponent<T>(entityID, std::forward<Args>(args)...);
}

template <typename EntReg>
template <component T, typename... Args>
inline T* EntityBuilder<EntReg>::addComponentIfMissing(Args&&... args) {
	return registry.template addComponentIfMissing<T>(entityID, std::forward<Args>(args)...);
}

template <typename EntReg>
template <component T>
inline T& EntityBuilder<EntReg>::addOrAssignComponent(T&& value) {
	return registry.template addOrAssignComponent<std::decay_t<T>>(entityID, std::forward<T>(value));
}

template <typename EntReg>
template <component T>
inline T& EntityBuilder<EntReg>::addOrAssignComponent(const T& value) {
	return registry.template addOrAssignComponent<T>(entityID, value);
}

template <typename EntReg>
template <component T>
inline bool EntityBuilder<EntReg>::removeComponent() noexcept {
	return registry.template removeComponent<T>(entityID);
}

template <typename EntReg>
inline bool EntityBuilder<EntReg>::removeAllComponents() noexcept {
	return registry.removeAllComponents(entityID);
}

template <typename EntReg>
template <component T>
inline bool EntityBuilder<EntReg>::hasComponent() const noexcept {
	return registry.template hasComponent<T>(entityID);
}

template <typename EntReg>
template <component T>
inline T& EntityBuilder<EntReg>::getComponent() {
	return registry.template getComponent<T>(entityID);
}

template <typename EntReg>
template <component T>
inline const T& EntityBuilder<EntReg>::getComponent() const {
	return registry.template getComponent<T>(entityID);
}

template <typename EntReg>
template <component T, typename U>
inline T EntityBuilder<EntReg>::getComponentOr(U&& defaultValue) const {
	return registry.template getComponentOr<T>(entityID, std::forward<U>(defaultValue));
}

template <typename EntReg>
template <component T>
inline T* EntityBuilder<EntReg>::findComponent() noexcept {
	return registry.template findComponent<T>(entityID);
}

template <typename EntReg>
template <component T>
inline const T* EntityBuilder<EntReg>::findComponent() const noexcept {
	return registry.template findComponent<T>(entityID);
}

} // namespace grem::execution

template <>
struct std::hash<grem::execution::EntityID> {
	[[nodiscard]] std::size_t operator()(const grem::execution::EntityID& entityID) const {
		return hasher(entityID.getIdentifier());
	}

private:
	[[no_unique_address]] std::hash<typename grem::execution::EntityID::Identifier> hasher;
};

#endif
