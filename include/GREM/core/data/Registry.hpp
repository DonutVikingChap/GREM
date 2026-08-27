// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_REGISTRY_HPP
#define GREM_CORE_DATA_REGISTRY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>

#include <compare>     // std::strong_ordering
#include <cstddef>     // std::size_t
#include <iterator>    // std::reverse_iterator, std::random_access_iterator_tag
#include <stdexcept>   // std::length_error, std::out_of_range
#include <type_traits> // std::conditional_t
#include <utility>     // std::move, std::forward, std::swap, std::exchange

namespace grem {

template <typename T, typename ElementID, typename Container>
class Registry; // Forward declaration.

/**
 * Base class for an opaque handle to a specific element in a Registry.
 *
 * \tparam Self CRTP argument for the concrete derived type.
 */
template <typename Self>
class RegistryElementIDBase {
public:
	using GREM_private_DerivedFromRegistryElementIDBaseTag = void;

	using Identifier = uint32_t; ///< Underlying value type.
	using Index = uint16_t;      ///< Index type.
	using Generation = uint16_t; ///< Generation counter type.
	using Flags = uint8_t;       ///< User-defined flags index type.

	static constexpr Index INVALID_INDEX = Limits<Index>::MAX;  ///< Reserved index value for an invalid element handle.
	static constexpr Index MAX_INDEX = Limits<Index>::MAX - 1;  ///< Maximum valid index value.
	static constexpr Generation MAX_GENERATION = (1 << 14) - 1; ///< Maximum valid generation counter value.
	static constexpr Flags ALL_FLAGS = 0b11;                    ///< Maximum valid set of user-defined flags.

	/**
     * Construct an invalid element handle.
     */
	constexpr RegistryElementIDBase() noexcept = default;

	/**
	 * Construct an element handle with a specific set of parameters.
	 *
	 * \param index index of the element's generation slot within the registry.
	 *        Must be less than or equal to MAX_INDEX.
	 * \param generation generation counter value of the element. Must be less
	 *        than or equal to MAX_GENERATION.
	 * \param flags user-defined flags. Must be less than or equal to ALL_FLAGS.
	 */
	constexpr RegistryElementIDBase(Index index, Generation generation, Flags flags)
		: value((uint32_t{flags} << 30) | ((uint32_t{generation} & 0x3FFF) << 16) | uint32_t{index}) {}

	/**
	 * Check if this handle is potentially valid.
	 *
	 * \return true if this handle is potentially valid, false if it is equal to
	 *         a default-constructed invalid handle.
	 */
	constexpr explicit operator bool() const noexcept {
		return *this != RegistryElementIDBase{};
	}

	/**
	 * Get a unique identifier for the underlying representation of the element
	 * handle.
	 *
	 * \return the underlying value of the handle.
	 */
	[[nodiscard]] constexpr Identifier getIdentifier() const noexcept {
		return value;
	}

	/**
	 * Get the index of the element's generation slot.
	 *
	 * \return the element index.
	 */
	[[nodiscard]] constexpr Index getIndex() const noexcept {
		return static_cast<Index>(value & uint32_t{Limits<Index>::MAX});
	}

	/**
	 * Get the generation counter value of the element.
	 *
	 * \return the element generation.
	 */
	[[nodiscard]] constexpr Generation getGeneration() const noexcept {
		return static_cast<Generation>((value >> 16) & MAX_GENERATION);
	}

	/**
	 * Get the user-defined flags of the element.
	 *
	 * \return the element flags.
	 */
	[[nodiscard]] constexpr Flags getFlags() const noexcept {
		return static_cast<Flags>(value >> 30);
	}

	/**
	 * Compare this handle to another for equality.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return true if the handles are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const RegistryElementIDBase& other) const noexcept = default;

	/**
	 * Compare this handle to another.
	 *
	 * \param other the handle to compare this one to.
	 *
	 * \return a strong ordering between the two handles.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const RegistryElementIDBase& other) const noexcept = default;

	/**
	 * Compare two handles for equality.
	 *
	 * \param a first handle.
	 * \param b second handle.
	 *
	 * \return true if the handles are equal, false otherwise.
	 */
	[[nodiscard]] friend constexpr bool operator==(const Self& a, const Self& b) noexcept {
		return *static_cast<const RegistryElementIDBase*>(&a) == *static_cast<const RegistryElementIDBase*>(&b);
	}

	/**
	 * Compare two handles.
	 *
	 * \param a first handle.
	 * \param b second handle.
	 *
	 * \return a strong ordering between the two handles.
	 */
	[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const Self& a, const Self& b) noexcept {
		return *static_cast<const RegistryElementIDBase*>(&a) <=> *static_cast<const RegistryElementIDBase*>(&b);
	}

private:
	uint32_t value = Limits<uint32_t>::MAX;
};

/**
 * Opaque handle to a specific element in a Registry.
 */
struct RegistryElementID : RegistryElementIDBase<RegistryElementID> {
	using RegistryElementIDBase::RegistryElementIDBase;
};

/**
 * Tightly packed set of elements with stable IDs, supporting fast iteration as
 * well as amortized O(1) insertion, removal and lookup.
 *
 * \tparam T element type.
 * \tparam ElementID element handle type. Must be derived from a specialization
 *         of RegistryElementIDBase with itself as the template argument.
 * \tparam Container element container type. Must provide an API similar to
 *         std::vector.
 */
template <typename T, typename ElementID = RegistryElementID, typename Container = ArrayList<T>>
class Registry {
private:
	using Index = typename RegistryElementID::Index;
	using Generation = typename RegistryElementID::Generation;
	using Flags = typename RegistryElementID::Flags;

	static constexpr Index MAX_INDEX = RegistryElementID::MAX_INDEX;
	static constexpr Flags ALL_FLAGS = RegistryElementID::ALL_FLAGS;

	template <bool Const>
	class Iterator {
	private:
		using UnderlyingIterator = std::conditional_t<Const, typename Container::const_iterator, typename Container::iterator>;

	public:
		using difference_type = ptrdiff_t;
		using value_type = Pair<const ElementID, typename Container::value_type>;
		using reference = Pair<const ElementID&, std::conditional_t<Const, typename Container::const_reference, typename Container::reference>>;
		using iterator_category = std::random_access_iterator_tag;

		struct pointer {
			reference ref;

			[[nodiscard]] constexpr reference* operator->() noexcept {
				return &ref;
			}
		};

		Iterator() noexcept = default;

		constexpr Iterator(const ElementID* elementID, UnderlyingIterator it) noexcept
			: elementID(elementID)
			, it(std::move(it)) {}

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{elementID, it};
		}

		[[nodiscard]] constexpr reference operator*() const {
			return reference{*elementID, *it};
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return pointer{**this};
		}

		[[nodiscard]] constexpr reference operator[](difference_type n) const {
			return *(*this + n);
		}

		constexpr Iterator& operator++() {
			++elementID;
			++it;
			return *this;
		}

		constexpr Iterator& operator--() {
			--elementID;
			--it;
			return *this;
		}

		constexpr Iterator operator++(int) {
			Iterator old = *this;
			++*this;
			return old;
		}

		constexpr Iterator operator--(int) {
			Iterator old = *this;
			--*this;
			return old;
		}

		constexpr Iterator& operator+=(difference_type n) {
			elementID += n;
			it += n;
			return *this;
		}

		constexpr Iterator& operator-=(difference_type n) {
			elementID -= n;
			it -= n;
			return *this;
		}

		[[nodiscard]] friend constexpr Iterator operator+(Iterator a, difference_type b) {
			return a += b;
		}

		[[nodiscard]] friend constexpr Iterator operator+(difference_type a, Iterator b) {
			return b += a;
		}

		[[nodiscard]] friend constexpr Iterator operator-(Iterator a, difference_type b) {
			return a -= b;
		}

		[[nodiscard]] friend constexpr difference_type operator-(Iterator a, Iterator b) {
			GREM_ASSERT(a.elementID - b.elementID == a.it - b.it);
			return static_cast<difference_type>(a.it - b.it);
		}

		[[nodiscard]] friend constexpr bool operator==(Iterator a, Iterator b) {
			GREM_ASSERT(a.elementID - b.elementID == a.it - b.it);
			return a.it == b.it;
		}

		[[nodiscard]] friend constexpr auto operator<=>(Iterator a, Iterator b) {
			GREM_ASSERT(a.elementID - b.elementID == a.it - b.it);
			return a.it <=> b.it;
		}

	private:
		const ElementID* elementID;
		UnderlyingIterator it;
	};

public:
	using iterator = Iterator<false>;
	using const_iterator = Iterator<true>;
	using value_type = typename iterator::value_type;
	using allocator_type = typename Container::allocator_type;
	using size_type = typename Container::size_type;
	using difference_type = typename iterator::difference_type;
	using reference = typename iterator::reference;
	using const_reference = typename const_iterator::reference;
	using pointer = typename iterator::pointer;
	using const_pointer = typename const_iterator::pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr Registry() noexcept(noexcept(allocator_type()))
		: Registry(allocator_type()) {}

	constexpr explicit Registry(const allocator_type& allocator) noexcept
		: slots(allocator)
		, elementIDs(allocator)
		, elements(allocator) {}

	constexpr ~Registry() = default;

	constexpr Registry(const Registry& other)
		: Registry(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr Registry(Registry&& other) noexcept
		: Registry(std::move(other), other.get_allocator()) {}

	constexpr Registry(const Registry& other, const allocator_type& allocator)
		: nextAvailableSlotIndex(other.nextAvailableSlotIndex)
		, slots(other.slots, allocator)
		, elementIDs(other.elementIDs, allocator)
		, elements(other.elements, allocator) {}

	constexpr Registry(Registry&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: nextAvailableSlotIndex(std::exchange(other.nextAvailableSlotIndex, Index{0}))
		, slots(std::move(other.slots), allocator)
		, elementIDs(std::move(other.elementIDs), allocator)
		, elements(std::move(other.elements), allocator) {}

	constexpr Registry& operator=(const Registry& other) = default;
	constexpr Registry& operator=(Registry&&) noexcept(
		std::allocator_traits<
			allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<allocator_type>::is_always_equal::value) = default;

	constexpr ElementID setFlags(ElementID key, ElementID::Flags newFlags) {
		const Index slotIndex = key.getIndex();
		if (slotIndex < slots.size() && (key.getGeneration() & 1) != 0) {
			ElementID& slot = slots[slotIndex];
			if (slot.getGeneration() == key.getGeneration()) {
				const Index idIndex = slot.getIndex();
				slot = ElementID{idIndex, key.getGeneration(), newFlags};
				const ElementID newElementID{slotIndex, key.getGeneration(), newFlags};
				GREM_ASSERT(elementIDs[idIndex].getGeneration() == key.getGeneration());
				elementIDs[idIndex] = newElementID;
				return newElementID;
			}
		}
		return {};
	}

	[[nodiscard]] constexpr const Container& getElements() const noexcept {
		return elements;
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return elements.get_allocator();
	}

	[[nodiscard]] constexpr T* data() noexcept requires(requires(Container c) { c.data(); }) {
		return elements.data();
	}

	[[nodiscard]] constexpr const T* data() const noexcept requires(requires(const Container c) { c.data(); }) {
		return elements.data();
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{elementIDs.data(), elements.begin()};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{elementIDs.data(), elements.begin()};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{elementIDs.data() + size(), elements.end()};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{elementIDs.data() + size(), elements.end()};
	}

	[[nodiscard]] constexpr const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
		return reverse_iterator{end()};
	}

	[[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator{end()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	[[nodiscard]] constexpr reverse_iterator rend() noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
		return rend();
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return elements.empty();
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elements.size();
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return min(min(slots.max_size(), elements.max_size()), static_cast<size_type>(static_cast<size_type>(MAX_INDEX) + 1));
	}

	[[nodiscard]] constexpr bool contains(ElementID key) const noexcept {
		const Index slotIndex = key.getIndex();
		return slotIndex < slots.size() && (key.getGeneration() & 1) != 0 && slots[slotIndex].getGeneration() == key.getGeneration();
	}

	[[nodiscard]] constexpr iterator find(ElementID key) noexcept {
		if (contains(key)) {
			return begin() + static_cast<difference_type>(slots[key.getIndex()].getIndex());
		}
		return end();
	}

	[[nodiscard]] constexpr const_iterator find(ElementID key) const noexcept {
		if (contains(key)) {
			return begin() + static_cast<difference_type>(slots[key.getIndex()].getIndex());
		}
		return end();
	}

	[[nodiscard]] constexpr ElementID id(size_type pos) const {
		GREM_ASSERT(pos < elementIDs.size());
		return elementIDs[pos];
	}

	[[nodiscard]] constexpr ElementID id(const_iterator pos) const {
		const Index idIndex = static_cast<Index>(pos - cbegin());
		GREM_ASSERT(idIndex < elementIDs.size());
		return elementIDs[idIndex];
	}

	[[nodiscard]] constexpr T& at(ElementID key) {
		if (const auto it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] constexpr const T& at(ElementID key) const {
		if (const auto it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] constexpr T& operator[](ElementID key) {
		const Index slotIndex = key.getIndex();
		GREM_ASSERT(slotIndex < slots.size());
		GREM_ASSERT(slots[slotIndex].getGeneration() == key.getGeneration());
		return elements[slots[slotIndex].getIndex()];
	}

	[[nodiscard]] constexpr const T& operator[](ElementID key) const {
		const Index slotIndex = key.getIndex();
		GREM_ASSERT(slotIndex < slots.size());
		GREM_ASSERT(slots[slotIndex].getGeneration() == key.getGeneration());
		return elements[slots[slotIndex].getIndex()];
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return *(begin() + static_cast<difference_type>(pos));
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return *(begin() + static_cast<difference_type>(pos));
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		return *(begin() + static_cast<difference_type>(pos));
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		return *(begin() + static_cast<difference_type>(pos));
	}

	template <typename ColumnType>
	[[nodiscard]] constexpr decltype(auto) column() requires(requires(Container c) { c.template column<ColumnType>(); }) {
		return elements.template column<ColumnType>();
	}

	template <typename ColumnType>
	[[nodiscard]] constexpr decltype(auto) column() const requires(requires(const Container c) { c.template column<ColumnType>(); }) {
		return elements.template column<ColumnType>();
	}

	template <size_t ColumnIndex>
	[[nodiscard]] constexpr decltype(auto) column() requires(requires(Container c) { c.template column<ColumnIndex>(); }) {
		return elements.template column<ColumnIndex>();
	}

	template <size_t ColumnIndex>
	[[nodiscard]] constexpr decltype(auto) column() const requires(requires(const Container c) { c.template column<ColumnIndex>(); }) {
		return elements.template column<ColumnIndex>();
	}

	constexpr void reserve(size_type newCapacity) {
		if (newCapacity > max_size()) {
			throw std::length_error{"newCap > max_size()"};
		}
		slots.reserve(newCapacity);
		elementIDs.reserve(newCapacity);
		elements.reserve(newCapacity);
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return slots.capacity();
	}

	constexpr void shrink_to_fit() {
		slots.shrink_to_fit();
		elementIDs.shrink_to_fit();
		elements.shrink_to_fit();
	}

	constexpr void clear() noexcept {
		elements.clear();
		elementIDs.clear();

		// Make all generation entries available for reuse.
		for (Index slotIndex = 0; slotIndex < slots.size(); ++slotIndex) {
			const Generation generation = slots[slotIndex].getGeneration();
			const Generation destroyedGeneration = generation + (generation & 1); // Make sure that the generation counter is even (marking invalid).
			slots[slotIndex] = ElementID{static_cast<Index>(slotIndex + 1), destroyedGeneration, Flags{}};
		}
		nextAvailableSlotIndex = 0;
	}

	constexpr iterator insert(const T& value) {
		return emplace(value);
	}

	constexpr iterator insert(T&& value) {
		return emplace(std::move(value));
	}

	template <typename... Args>
	constexpr iterator emplace(Args&&... args) {
		return emplaceWithFlags(Flags{}, std::forward<Args>(args)...);
	}

	template <typename... Args>
	constexpr iterator emplaceWithFlags(Flags flags, Args&&... args) {
		GREM_ASSERT(elements.size() == elementIDs.size());

		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}

		const Index idIndex = static_cast<Index>(size());
		ElementID& elementID = elementIDs.push_back_unspecified_value();

		Index slotIndex = static_cast<Index>(slots.size());
		Generation generation = 1;
		if (nextAvailableSlotIndex >= slotIndex) {
			GREM_ASSERT(nextAvailableSlotIndex == slotIndex);
			GREM_ASSERT(slotIndex == idIndex);
			try {
				slots.push_back(ElementID{idIndex, generation, flags});
				try {
					elements.emplace_back(std::forward<Args>(args)...);
				} catch (...) {
					slots.pop_back();
					throw;
				}
				++nextAvailableSlotIndex;
			} catch (...) {
				elementIDs.pop_back();
				throw;
			}
		} else {
			try {
				elements.emplace_back(std::forward<Args>(args)...);
			} catch (...) {
				elementIDs.pop_back();
				throw;
			}
			slotIndex = nextAvailableSlotIndex;
			nextAvailableSlotIndex = slots[slotIndex].getIndex();
			// Note: The generation counter is incremented both on insertion and on removal.
			// This means that you can identify which generation slots are invalid based on whether the generation is even (invalid) or odd (valid),
			// and it also solves the problem of newly created elements' generations comparing equal to deleted elements' generations in previous copies of the registry.
			// The downside is that it effectively cuts the range of generation indices in half, but since we have a whole 14 bits to work with, that should be fine for most use cases.
			generation = static_cast<Generation>(slots[slotIndex].getGeneration() + 1);
			slots[slotIndex] = ElementID{idIndex, generation, flags};
		}

		elementID = ElementID{slotIndex, generation, flags};
		return begin() + static_cast<difference_type>(idIndex);
	}

	constexpr size_type erase(ElementID key) {
		if (!contains(key)) {
			return 0;
		}
		eraseImplementation(key.getIndex());
		return 1;
	}

	constexpr iterator erase(const_iterator pos) {
		return eraseImplementation(id(pos).getIndex());
	}

	constexpr void swap(Registry& other) noexcept(
		noexcept(elements.swap(other.elements)) && noexcept(elementIDs.swap(other.elementIDs))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		using std::swap;
		swap(nextAvailableSlotIndex, other.nextAvailableSlotIndex);
		swap(slots, other.slots);
		swap(elementIDs, other.elementIDs);
		swap(elements, other.elements);
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(Registry& c, Predicate predicate) {
		const size_type oldSize = c.size();
		iterator it = c.begin();
		while (it != c.end()) {
			if (predicate(*it)) {
				it = c.erase(it);
			} else {
				++it;
			}
		}
		return oldSize - c.size();
	}

private:
	using ElementIDAllocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<ElementID>;

	iterator eraseImplementation(Index slotIndex) {
		GREM_ASSERT(!empty());
		GREM_ASSERT(elements.size() == elementIDs.size());

		// Move the last element's data to the destroyed element's index.
		const Index idIndex = slots[slotIndex].getIndex();
		const Generation generation = slots[slotIndex].getGeneration();
		const Index lastIDIndex = static_cast<Index>(size() - 1);
		const ElementID lastElementID = elementIDs[lastIDIndex];
		const Index lastSlotIndex = lastElementID.getIndex();
		if (lastSlotIndex != slotIndex) {
			elements[idIndex] = std::move(elements[lastIDIndex]);
			elementIDs[idIndex] = lastElementID;
			slots[lastSlotIndex] = ElementID{idIndex, lastElementID.getGeneration(), lastElementID.getFlags()}; // Repoint the last element's slot to its new index.
		}
		elements.pop_back();
		elementIDs.pop_back();

		// Make the destroyed element's slot available for reuse.
		slots[slotIndex] = ElementID{nextAvailableSlotIndex, static_cast<Generation>(generation + 1), Flags{}};
		nextAvailableSlotIndex = slotIndex;

		return begin() + static_cast<difference_type>(idIndex);
	}

	Index nextAvailableSlotIndex = 0;
	Buffer<ElementID, ElementIDAllocator> slots;
	Buffer<ElementID, ElementIDAllocator> elementIDs;
	Container elements;
};

template <typename ColumnType, typename T, typename ElementID, typename Container>
[[nodiscard]] constexpr decltype(auto) column(Registry<T, ElementID, Container>& r) requires(requires(Container c) { c.template column<ColumnType>(); }) {
	return r.template column<ColumnType>();
}

template <typename ColumnType, typename T, typename ElementID, typename Container>
[[nodiscard]] constexpr decltype(auto) column(const Registry<T, ElementID, Container>& r) requires(requires(const Container c) { c.template column<ColumnType>(); }) {
	return r.template column<ColumnType>();
}

template <size_t ColumnIndex, typename T, typename ElementID, typename Container>
[[nodiscard]] constexpr decltype(auto) column(Registry<T, ElementID, Container>& r) requires(requires(Container c) { c.template column<ColumnIndex>(); }) {
	return r.template column<ColumnIndex>();
}

template <size_t ColumnIndex, typename T, typename ElementID, typename Container>
[[nodiscard]] constexpr decltype(auto) column(const Registry<T, ElementID, Container>& r) requires(requires(const Container c) { c.template column<ColumnIndex>(); }) {
	return r.template column<ColumnIndex>();
}

} // namespace grem

namespace grem::pmr {

template <typename T, typename ElementID>
using Registry = grem::Registry<T, ElementID, pmr::ArrayList<T>>;

} // namespace grem::pmr

template <typename ElementID>
requires(requires { typename ElementID::GREM_private_DerivedFromRegistryElementIDBaseTag; }) struct std::hash<ElementID> {
	[[nodiscard]] std::size_t operator()(const ElementID& elementID) const {
		return hasher(elementID.getIdentifier());
	}

private:
	[[no_unique_address]] std::hash<typename grem::RegistryElementID::Identifier> hasher;
};

#endif
