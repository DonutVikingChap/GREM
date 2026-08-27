// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_HASH_MAP_HPP
#define GREM_CORE_DATA_HASH_MAP_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Pair.hpp>

#include <algorithm>        // std::min, std::max, std::equal, std::lexicographical_compare
#include <bit>              // std::countr_zero, std::countl_zero
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <cstdint>          // std::uint32_t, std::uint64_t
#include <cstring>          // std::memset, std::memcpy
#include <functional>       // std::hash, std::equal_to
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::input_iterator, std::sentinel_for, std::iterator_traits, std::..._iterator_tag, std::next, std::begin, std::end
#include <limits>           // std::numeric_limits
#include <memory>           // std::allocator, std::allocator_traits, std::to_address, std::addressof
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::out_of_range, std::length_error
#include <type_traits>      // std::is_..._v, std::remove_..._t, std::conditional_t
#include <utility>          // std::move, std::forward, std::exchange, std::swap

namespace grem {

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>, typename Allocator = std::allocator<Pair<const Key, T>>>
class HashMap {
private:
	static constexpr bool IS_TRANSPARENT = requires {
		typename Hash::is_transparent;
		typename KeyEqual::is_transparent;
	};

public:
	using key_type = Key;
	using mapped_type = T;
	using size_type = std::size_t;
	using hasher = Hash;
	using key_equal = KeyEqual;
	using allocator_type = Allocator;

private:
	template <bool Const>
	class Iterator {
	private:
		using Map = std::conditional_t<Const, const HashMap, HashMap>;

	public:
		using value_type = Pair<const Key, T>;
		using reference = Pair<const Key&, std::conditional_t<Const, const T&, T&>>;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::bidirectional_iterator_tag;

		struct pointer {
			reference ref;

			[[nodiscard]] constexpr reference* operator->() noexcept {
				return &ref;
			}
		};

		Iterator() noexcept = default;

		constexpr Iterator(Map* map, size_type index) noexcept
			: map(map)
			, index(index) {}

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{map, index};
		}

		[[nodiscard]] constexpr reference operator*() const {
			GREM_ASSERT(map);
			GREM_ASSERT(index < map->elementCapacity);
			return reference{map->keys[index], map->values[index]};
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return pointer{**this};
		}

		constexpr Iterator& operator++() {
			GREM_ASSERT(map);
			GREM_ASSERT(index < map->elementCapacity);
			GREM_ASSERT(map->elementCount > 0);
			index = map->findActiveIndexUnbounded(index + 1);
			return *this;
		}

		constexpr Iterator operator++(int) {
			Iterator old = *this;
			++*this;
			return old;
		}

		constexpr Iterator& operator--() {
			GREM_ASSERT(map);
			GREM_ASSERT(index > 0);
			GREM_ASSERT(map->elementCount > 0);
			index = map->findActiveIndexReverseUnbounded(index - 1);
			return *this;
		}

		constexpr Iterator operator--(int) {
			Iterator old = *this;
			--*this;
			return old;
		}

		[[nodiscard]] constexpr bool operator==(const Iterator& other) const {
			GREM_ASSERT(map == other.map);
			return index == other.index;
		}

		[[nodiscard]] constexpr size_type getIndex() const noexcept {
			return index;
		}

	private:
		friend HashMap;

		Map* map = nullptr;
		size_type index = 0;
	};

public:
	using iterator = Iterator<false>;
	using const_iterator = Iterator<true>;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using const_reference = typename const_iterator::reference;
	using pointer = typename iterator::pointer;
	using const_pointer = typename const_iterator::pointer;
	using difference_type = typename iterator::difference_type;

	constexpr HashMap() noexcept = default;

	constexpr explicit HashMap(size_type bucket_count, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(), const Allocator& allocator = Allocator())
		: HashMap(hash, equal, allocator) {
		reserve(bucket_count);
	}

	constexpr HashMap(size_type bucket_count, const Allocator& allocator)
		: HashMap(bucket_count, Hash(), KeyEqual(), allocator) {}

	constexpr HashMap(size_type bucket_count, const Hash& hash, const Allocator& allocator)
		: HashMap(bucket_count, hash, KeyEqual(), allocator) {}

	constexpr explicit HashMap(const Allocator& allocator)
		: HashMap(Hash(), KeyEqual(), allocator) {}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr HashMap(InputIterator first, Sentinel last, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
		const Allocator& allocator = Allocator())
		: HashMap(bucket_count, hash, equal, allocator) {
		insert(first, last);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr HashMap(InputIterator first, Sentinel last, size_type bucket_count, const Allocator& allocator)
		: HashMap(first, last, bucket_count, Hash(), KeyEqual(), allocator) {}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr HashMap(InputIterator first, Sentinel last, size_type bucket_count, const Hash& hash, const Allocator& allocator)
		: HashMap(first, last, bucket_count, hash, KeyEqual(), allocator) {}

	constexpr HashMap(std::initializer_list<value_type> ilist, size_type bucket_count = 0, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(),
		const Allocator& allocator = Allocator())
		: HashMap(ilist.begin(), ilist.end(), bucket_count, hash, equal, allocator) {}

	constexpr HashMap(std::initializer_list<value_type> ilist, size_type bucket_count, const Allocator& allocator = Allocator())
		: HashMap(ilist, bucket_count, Hash(), KeyEqual(), allocator) {}

	constexpr HashMap(std::initializer_list<value_type> ilist, size_type bucket_count, const Hash& hash = Hash(), const Allocator& allocator = Allocator())
		: HashMap(ilist, bucket_count, hash, KeyEqual(), allocator) {}

	constexpr HashMap(const HashMap& other)
		: HashMap(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr HashMap(const HashMap& other, const Allocator& allocator)
		: HashMap(allocator) {
		*this = other;
	}

	constexpr HashMap(HashMap&& other) noexcept
		: activeBits(std::exchange(other.activeBits, nullptr))
		, bucketExtents(std::exchange(other.bucketExtents, nullptr))
		, keys(std::exchange(other.keys, nullptr))
		, values(std::exchange(other.values, nullptr))
		, elementCount(std::exchange(other.elementCount, size_type{0}))
		, elementCapacity(std::exchange(other.elementCapacity, size_type{0}))
		, hash(std::move(other.hash))
		, equal(std::move(other.equal))
		, allocator(other.get_allocator()) {}

	constexpr HashMap(HashMap&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: HashMap(allocator) {
		if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.get_allocator()) {
				reserve(other.size());
				for (reference element : other) {
					if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
						emplace(std::move(const_cast<Key&>(element.first)), std::move(element.second));
					} else {
						emplace(element.first, element.second);
					}
				}
				other.clear();
				return;
			}
		}
		activeBits = std::exchange(other.activeBits, nullptr);
		bucketExtents = std::exchange(other.bucketExtents, nullptr);
		keys = std::exchange(other.keys, nullptr);
		values = std::exchange(other.values, nullptr);
		elementCount = std::exchange(other.elementCount, size_type{0});
		elementCapacity = std::exchange(other.elementCapacity, size_type{0});
		hash = std::move(other.hash);
		equal = std::move(other.equal);
	}

	constexpr ~HashMap() {
		reset();
	}

	constexpr HashMap& operator=(const HashMap& other) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					reset();
				}
			}
			allocator = other.get_allocator();
		}
		clear();
		if (elementCapacity == other.elementCapacity) {
			if (other.elementCount > 0) {
				GREM_ASSERT(elementCapacity > 0);
				GREM_ASSERT(elementCapacity % 64 == 0);
				KeyAllocator keyAllocator = allocator;
				ValueAllocator valueAllocator = allocator;
				const size_type begin = other.findActiveIndexUnbounded(0);
				for (size_type index = begin; index < elementCapacity; index = other.findActiveIndexUnbounded(index + 1)) {
					try {
						std::allocator_traits<KeyAllocator>::construct(keyAllocator, std::to_address(keys) + index, other.keys[index]);
						try {
							std::allocator_traits<ValueAllocator>::construct(valueAllocator, std::to_address(values) + index, other.values[index]);
						} catch (...) {
							std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
							throw;
						}
					} catch (...) {
						while (index != begin) {
							index = other.findActiveIndexReverseUnbounded(index - 1);
							std::allocator_traits<ValueAllocator>::destroy(valueAllocator, std::to_address(values) + index);
							std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
						}
						throw;
					}
				}
				std::memcpy(std::to_address(activeBits), std::to_address(other.activeBits), (elementCapacity / 64) * sizeof(std::uint64_t));
				std::memcpy(std::to_address(bucketExtents), std::to_address(other.bucketExtents), elementCapacity * sizeof(std::uint32_t));
				elementCount = other.elementCount;
			}
		} else {
			for (const_reference element : other) {
				insert(element);
			}
		}
		return *this;
	}

	constexpr HashMap& operator=(HashMap&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					clear();
					reserve(other.size());
					for (reference element : other) {
						if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
							emplace(std::move(const_cast<Key&>(element.first)), std::move(element.second));
						} else {
							emplace(element.first, element.second);
						}
					}
					other.clear();
					return *this;
				}
			}
		}
		reset();
		activeBits = std::exchange(other.activeBits, nullptr);
		bucketExtents = std::exchange(other.bucketExtents, nullptr);
		keys = std::exchange(other.keys, nullptr);
		values = std::exchange(other.values, nullptr);
		elementCount = std::exchange(other.elementCount, size_type{0});
		elementCapacity = std::exchange(other.elementCapacity, size_type{0});
		hash = std::move(other.hash);
		equal = std::move(other.equal);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.get_allocator();
		}
		return *this;
	}

	constexpr HashMap& operator=(std::initializer_list<value_type> ilist) {
		clear();
		insert(ilist);
		return *this;
	}

	constexpr void swap(HashMap& other) noexcept(std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		using std::swap;
		swap(activeBits, other.activeBits);
		swap(bucketExtents, other.bucketExtents);
		swap(keys, other.keys);
		swap(values, other.values);
		swap(elementCount, other.elementCount);
		swap(elementCapacity, other.elementCapacity);
		swap(hash, other.hash);
		swap(equal, other.equal);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		} else if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			GREM_ASSERT(allocator == other.allocator);
		}
	}

	friend constexpr void swap(HashMap& a, HashMap& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

	constexpr void clear() noexcept {
		if (elementCount > 0) {
			GREM_ASSERT(elementCapacity > 0);
			GREM_ASSERT(elementCapacity % 64 == 0);
			KeyAllocator keyAllocator = allocator;
			ValueAllocator valueAllocator = allocator;
			for (size_type index = findActiveIndexUnbounded(0); index < elementCapacity; index = findActiveIndexUnbounded(index + 1)) {
				std::allocator_traits<ValueAllocator>::destroy(valueAllocator, std::to_address(values) + index);
				std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
			}
			std::memset(std::to_address(activeBits), 0, (elementCapacity / 64) * sizeof(std::uint64_t));
			std::memset(std::to_address(bucketExtents), 0, elementCapacity * sizeof(std::uint32_t));
			elementCount = 0;
		}
	}

	constexpr void reserve(size_type capacity) {
		size_type newCapacity = std::max(elementCapacity, size_type{64});
		while (static_cast<float>(capacity) > static_cast<float>(newCapacity) * MAX_SIZE_PER_CAPACITY) {
			if (newCapacity >= std::numeric_limits<size_type>::max() / 2) {
				throw std::length_error{"Maximum capacity exceeded."};
			}
			newCapacity *= 2;
		}
		if (newCapacity <= elementCapacity) {
			return;
		}

		GREM_ASSERT(newCapacity % 64 == 0);
		GREM_ASSERT((newCapacity & (newCapacity - 1)) == 0);

		HashMap newMap{hash, equal, allocator};
		UInt64Allocator uInt64Allocator = allocator;
		UInt32Allocator uInt32Allocator = allocator;
		KeyAllocator keyAllocator = allocator;
		ValueAllocator valueAllocator = allocator;
		newMap.activeBits = std::allocator_traits<UInt64Allocator>::allocate(uInt64Allocator, newCapacity / 64 + 1);
		try {
			newMap.bucketExtents = std::allocator_traits<UInt32Allocator>::allocate(uInt32Allocator, newCapacity);
			try {
				newMap.keys = std::allocator_traits<KeyAllocator>::allocate(keyAllocator, newCapacity);
				try {
					newMap.values = std::allocator_traits<ValueAllocator>::allocate(valueAllocator, newCapacity);
				} catch (...) {
					std::allocator_traits<KeyAllocator>::deallocate(keyAllocator, newMap.keys, newCapacity);
					throw;
				}
			} catch (...) {
				std::allocator_traits<UInt32Allocator>::deallocate(uInt32Allocator, newMap.bucketExtents, newCapacity);
				throw;
			}
		} catch (...) {
			std::allocator_traits<UInt64Allocator>::deallocate(uInt64Allocator, newMap.activeBits, newCapacity / 64 + 1);
			throw;
		}
		std::memset(std::to_address(newMap.activeBits), 0, (newCapacity / 64) * sizeof(std::uint64_t));
		newMap.activeBits[newCapacity / 64] = 1;
		std::memset(std::to_address(newMap.bucketExtents), 0, newCapacity * sizeof(std::uint32_t));
		newMap.elementCapacity = newCapacity;

		for (reference element : *this) {
			if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
				newMap.emplace(std::move(const_cast<Key&>(element.first)), std::move(element.second));
			} else {
				newMap.emplace(element.first, element.second);
			}
		}
		GREM_ASSERT(allocator == newMap.get_allocator());
		*this = std::move(newMap);
	}

	constexpr Pair<iterator, bool> insert(const value_type& value) {
		return try_emplace(value.first, value.second);
	}

	constexpr Pair<iterator, bool> insert(value_type&& value) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		return try_emplace(std::move(value.first), std::move(value.second));
	}

	template <typename P>
	constexpr Pair<iterator, bool> insert(P&& value) requires(std::is_constructible_v<value_type, P &&>) {
		return emplace(std::forward<P>(value));
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	constexpr void insert(InputIterator first, Sentinel last) {
		if constexpr (std::is_convertible_v<typename std::iterator_traits<InputIterator>::iterator_category, std::random_access_iterator_tag>) {
			reserve(size() + static_cast<size_type>(last - first));
		}
		while (first != last) {
			insert(*first++);
		}
	}

	constexpr void insert(std::initializer_list<value_type> ilist) {
		insert(ilist.begin(), ilist.end());
	}

	template <typename R>
	constexpr void insert_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		insert(std::begin(r), std::end(r));
	}

	template <typename... Args>
	constexpr Pair<iterator, bool> emplace(Args&&... args) {
		return insert(value_type(std::forward<Args>(args)...));
	}

	template <typename... Args>
	constexpr Pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
		return try_emplace(Key(k), std::forward<Args>(args)...);
	}

	template <typename... Args>
	constexpr Pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
		return insertAndConstruct(std::move(k), std::forward<Args>(args)...);
	}

	template <typename K, typename... Args>
	constexpr Pair<iterator, bool> try_emplace(K&& k, Args&&... args)
		requires(!std::is_convertible_v<K &&, const_iterator> && !std::is_convertible_v<K &&, iterator> && IS_TRANSPARENT) {
		return insertAndConstruct(std::forward<K>(k), std::forward<Args>(args)...);
	}

	template <typename M>
	constexpr Pair<iterator, bool> insert_or_assign(const Key& k, M&& obj) {
		const auto result = try_emplace(k, std::forward<M>(obj));
		if (!result.second) {
			result.first->second = std::forward<M>(obj);
		}
		return result;
	}

	template <typename M>
	constexpr Pair<iterator, bool> insert_or_assign(Key&& k, M&& obj) {
		const auto result = try_emplace(std::move(k), std::forward<M>(obj));
		if (!result.second) {
			result.first->second = std::forward<M>(obj);
		}
		return result;
	}

	template <typename K, typename M>
	constexpr Pair<iterator, bool> insert_or_assign(K&& k, M&& obj) requires(IS_TRANSPARENT) {
		const auto result = try_emplace(std::forward<K>(k), std::forward<M>(obj));
		if (!result.second) {
			result.first->second = std::forward<M>(obj);
		}
		return result;
	}

	constexpr iterator erase(const_iterator pos) {
		return iterator{this, eraseAtIndex(pos.index)};
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		while (first != last) {
			first = erase(first);
		}
		return iterator{this, first.index};
	}

	constexpr size_type erase(const Key& key) {
		return eraseKey(key);
	}

	template <typename K>
	constexpr size_type erase(K&& x) requires(IS_TRANSPARENT && !std::is_convertible_v<K, iterator> && !std::is_convertible_v<K, const_iterator>) {
		return eraseKey(std::forward<K>(x));
	}

	template <typename Predicate>
	friend constexpr size_type erase_if(HashMap& c, Predicate predicate) {
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

	[[nodiscard]] constexpr hasher hash_function() const {
		return hash;
	}

	[[nodiscard]] constexpr key_equal key_eq() const {
		return equal;
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return elementCount == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return elementCount;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		static_assert(MAX_SIZE_PER_CAPACITY <= 1.0f);
		return static_cast<size_type>(
			static_cast<float>(std::min({
				static_cast<size_type>(std::numeric_limits<difference_type>::max()),
				static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(std::uint32_t)),
				static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(Key)),
				static_cast<size_type>(std::numeric_limits<size_type>::max() / sizeof(T)),
				static_cast<size_type>(std::allocator_traits<UInt32Allocator>::max_size(allocator)),
				static_cast<size_type>(std::allocator_traits<KeyAllocator>::max_size(allocator)),
				static_cast<size_type>(std::allocator_traits<ValueAllocator>::max_size(allocator)),
			})) *
			MAX_SIZE_PER_CAPACITY);
	}

	[[nodiscard]] constexpr T& at(const Key& key) {
		if (const iterator it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] constexpr const T& at(const Key& key) const {
		if (const const_iterator it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	template <typename K>
	[[nodiscard]] constexpr T& at(const K& key) requires(IS_TRANSPARENT) {
		if (const iterator it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	template <typename K>
	[[nodiscard]] constexpr const T& at(const K& key) const requires(IS_TRANSPARENT) {
		if (const const_iterator it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] constexpr T& operator[](const Key& key) {
		return try_emplace(key).first->second;
	}

	[[nodiscard]] constexpr T& operator[](Key&& key) {
		return try_emplace(std::move(key)).first->second;
	}

	template <typename K>
	[[nodiscard]] constexpr T& operator[](K&& x) requires(IS_TRANSPARENT) {
		return try_emplace(std::forward<K>(x)).first->second;
	}

	[[nodiscard]] constexpr bool contains(const Key& key) const {
		return find(key) != end();
	}

	template <typename K>
	[[nodiscard]] constexpr bool contains(const K& x) const requires(IS_TRANSPARENT) {
		return find(x) != end();
	}

	[[nodiscard]] constexpr size_type count(const Key& key) const {
		return (contains(key)) ? 1 : 0;
	}

	template <typename K>
	[[nodiscard]] constexpr size_type count(const K& x) const requires(IS_TRANSPARENT) {
		return (contains(x)) ? 1 : 0;
	}

	[[nodiscard]] constexpr iterator find(const Key& key) {
		return iterator{this, findIndex(key)};
	}

	[[nodiscard]] constexpr const_iterator find(const Key& key) const {
		return const_iterator{this, findIndex(key)};
	}

	template <typename K>
	[[nodiscard]] constexpr iterator find(const K& x) requires(IS_TRANSPARENT) {
		return iterator{this, findIndex(x)};
	}

	template <typename K>
	[[nodiscard]] constexpr const_iterator find(const K& x) const requires(IS_TRANSPARENT) {
		return const_iterator{this, findIndex(x)};
	}

	[[nodiscard]] constexpr iterator getIteratorAtIndex(size_type index) {
		GREM_ASSERT(index <= elementCapacity);
		return iterator{this, index};
	}

	[[nodiscard]] constexpr const_iterator getIteratorAtIndex(size_type index) const {
		GREM_ASSERT(index <= elementCapacity);
		return const_iterator{this, index};
	}

	[[nodiscard]] constexpr reference getAtIndex(size_type index) {
		GREM_ASSERT(index <= elementCapacity);
		GREM_ASSERT(isActive(index));
		return *iterator{this, index};
	}

	[[nodiscard]] constexpr const_reference getAtIndex(size_type index) const {
		GREM_ASSERT(index <= elementCapacity);
		GREM_ASSERT(isActive(index));
		return *const_iterator{this, index};
	}

	[[nodiscard]] constexpr Pair<iterator, iterator> equal_range(const Key& key) {
		const iterator it = find(key);
		if (it != end()) {
			return {it, std::next(it)};
		}
		return {it, it};
	}

	[[nodiscard]] constexpr Pair<const_iterator, const_iterator> equal_range(const Key& key) const {
		const const_iterator it = find(key);
		if (it != end()) {
			return {it, std::next(it)};
		}
		return {it, it};
	}

	template <typename K>
	[[nodiscard]] constexpr Pair<iterator, iterator> equal_range(const K& x) requires(IS_TRANSPARENT) {
		const iterator it = find(x);
		if (it != end()) {
			return {it, std::next(it)};
		}
		return {it, it};
	}

	template <typename K>
	[[nodiscard]] constexpr Pair<const_iterator, const_iterator> equal_range(const K& x) const requires(IS_TRANSPARENT) {
		const const_iterator it = find(x);
		if (it != end()) {
			return {it, std::next(it)};
		}
		return {it, it};
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{this, (elementCount > 0) ? findActiveIndexUnbounded(0) : elementCapacity};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{this, (elementCount > 0) ? findActiveIndexUnbounded(0) : elementCapacity};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{this, elementCapacity};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{this, elementCapacity};
	}

	[[nodiscard]] constexpr const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] constexpr bool operator==(const HashMap& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const HashMap& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const HashMap& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const HashMap& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const HashMap& other) const {
		return !(*this < other);
	}

private:
	static constexpr float MAX_SIZE_PER_CAPACITY = 0.7f;

	using UInt64Allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint64_t>;
	using UInt32Allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<std::uint32_t>;
	using KeyAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Key>;
	using ValueAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

	constexpr HashMap(const Hash& hash, const KeyEqual& equal, const Allocator& allocator)
		: hash(hash)
		, equal(equal)
		, allocator(allocator) {}

	[[nodiscard]] constexpr size_type getBucketIndexOfHash(std::size_t hashCode) const noexcept {
		GREM_ASSERT((elementCapacity & (elementCapacity - 1)) == 0);
		return static_cast<size_type>(hashCode) & static_cast<size_type>(elementCapacity - 1);
	}

	[[nodiscard]] constexpr size_type getWrappedIndex(size_type index) const noexcept {
		GREM_ASSERT(elementCapacity > 0);
		GREM_ASSERT((elementCapacity & (elementCapacity - 1)) == 0);
		return index & static_cast<size_type>(elementCapacity - 1);
	}

	[[nodiscard]] constexpr bool isActive(size_type index) const {
		return (activeBits[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
	}

	constexpr void setActive(size_type index) {
		activeBits[index / 64] |= std::uint64_t{1} << (index % 64);
	}

	constexpr void clearActive(size_type index) {
		activeBits[index / 64] &= ~(std::uint64_t{1} << (index % 64));
	}

	[[nodiscard]] constexpr size_type findActiveIndexUnbounded(size_type index) const {
		while (true) {
			const std::uint64_t bitsIndex = index / 64;
			const std::uint64_t bitOffset = index % 64;
			const std::uint64_t bits = activeBits[bitsIndex] >> bitOffset;
			if (bits != 0) {
				index += static_cast<size_type>(std::countr_zero(bits));
				break;
			}
			index += 64 - bitOffset;
		}
		return index;
	}

	[[nodiscard]] constexpr size_type findActiveIndexReverseUnbounded(size_type index) const {
		while (true) {
			const std::uint64_t bitsIndex = index / 64;
			const std::uint64_t bitOffset = index % 64;
			const std::uint64_t bits = activeBits[bitsIndex] << (63 - bitOffset);
			if (bits != 0) {
				index -= static_cast<size_type>(std::countl_zero(bits));
				break;
			}
			index -= bitOffset + 1;
		}
		return index;
	}

	constexpr void reset() noexcept {
		clear();
		if (elementCapacity > 0) {
			GREM_ASSERT(elementCapacity % 64 == 0);
			UInt64Allocator uInt64Allocator = allocator;
			UInt32Allocator uInt32Allocator = allocator;
			KeyAllocator keyAllocator = allocator;
			ValueAllocator valueAllocator = allocator;
			std::allocator_traits<UInt64Allocator>::deallocate(uInt64Allocator, activeBits, elementCapacity / 64 + 1);
			std::allocator_traits<UInt32Allocator>::deallocate(uInt32Allocator, bucketExtents, elementCapacity);
			std::allocator_traits<KeyAllocator>::deallocate(keyAllocator, keys, elementCapacity);
			std::allocator_traits<ValueAllocator>::deallocate(valueAllocator, values, elementCapacity);
			activeBits = nullptr;
			bucketExtents = nullptr;
			keys = nullptr;
			values = nullptr;
			elementCapacity = 0;
		}
	}

	template <typename K, typename... Args>
	[[nodiscard]] constexpr Pair<iterator, bool> insertAndConstruct(K&& k, Args&&... args) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}

		reserve(elementCount + 1);

		const size_type bucketBegin = getBucketIndexOfHash(hash(k));
		const size_type bucketExtent = bucketExtents[bucketBegin];
		const size_type bucketEnd = getWrappedIndex(bucketBegin + bucketExtent);
		for (size_type index = bucketBegin; index != bucketEnd; index = getWrappedIndex(index + 1)) {
			if (isActive(index) && equal(keys[index], k)) {
				return {iterator{this, index}, false};
			}
		}
		size_type index = bucketBegin;
		size_type newBucketExtent = 1;
		while (isActive(index)) {
			index = getWrappedIndex(index + 1);
			if (newBucketExtent >= size_type{std::numeric_limits<std::uint32_t>::max()}) {
				throw std::length_error{"Maximum bucket size exceeded."};
			}
			++newBucketExtent;
		}

		KeyAllocator keyAllocator = allocator;
		ValueAllocator valueAllocator = allocator;
		std::allocator_traits<KeyAllocator>::construct(keyAllocator, std::to_address(keys) + index, std::forward<K>(k));
		try {
			std::allocator_traits<ValueAllocator>::construct(valueAllocator, std::to_address(values) + index, std::forward<Args>(args)...);
		} catch (...) {
			std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
			throw;
		}

		if (newBucketExtent > bucketExtent) {
			bucketExtents[bucketBegin] = static_cast<std::uint32_t>(newBucketExtent);
		}
		setActive(index);
		++elementCount;

		return {iterator{this, index}, true};
	}

	[[nodiscard]] size_type eraseAtIndex(size_type index) {
		GREM_ASSERT(index < elementCapacity);
		GREM_ASSERT(isActive(index));

		const size_type bucketBegin = getBucketIndexOfHash(hash(keys[index]));
		size_type bucketExtent = bucketExtents[bucketBegin];
		size_type bucketEnd = getWrappedIndex(bucketBegin + bucketExtent);
		GREM_ASSERT(bucketExtent > 0);
		GREM_ASSERT(bucketBegin != bucketEnd);
		GREM_ASSERT(index != bucketEnd);

		KeyAllocator keyAllocator = allocator;
		ValueAllocator valueAllocator = allocator;
		std::allocator_traits<ValueAllocator>::destroy(valueAllocator, std::to_address(values) + index);
		std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
		clearActive(index);
		--elementCount;

		if (getWrappedIndex(index + 1) == bucketEnd) {
			--bucketExtent;
			bucketEnd = index;
			while (bucketExtent > 0) {
				const size_type previousIndex = getWrappedIndex(bucketEnd - 1);
				if (isActive(previousIndex)) {
					break;
				}
				--bucketExtent;
				bucketEnd = previousIndex;
			}
			bucketExtents[bucketBegin] = static_cast<std::uint32_t>(bucketExtent);
		}

		return findActiveIndexUnbounded(index + 1);
	}

	template <typename K>
	[[nodiscard]] size_type eraseKey(const K& k) {
		if (empty()) {
			return 0;
		}

		const size_type bucketBegin = getBucketIndexOfHash(hash(k));
		size_type bucketExtent = bucketExtents[bucketBegin];
		size_type bucketEnd = getWrappedIndex(bucketBegin + bucketExtent);
		size_type index = bucketBegin;
		while (index != bucketEnd && (!isActive(index) || !equal(keys[index], k))) {
			index = getWrappedIndex(index + 1);
		}
		if (index == bucketEnd) {
			return 0;
		}

		GREM_ASSERT(bucketExtent > 0);
		GREM_ASSERT(bucketBegin != bucketEnd);

		KeyAllocator keyAllocator = allocator;
		ValueAllocator valueAllocator = allocator;
		std::allocator_traits<ValueAllocator>::destroy(valueAllocator, std::to_address(values) + index);
		std::allocator_traits<KeyAllocator>::destroy(keyAllocator, std::to_address(keys) + index);
		clearActive(index);
		--elementCount;

		if (getWrappedIndex(index + 1) == bucketEnd) {
			--bucketExtent;
			bucketEnd = index;
			while (bucketExtent > 0) {
				const size_type previousIndex = getWrappedIndex(bucketEnd - 1);
				if (isActive(previousIndex)) {
					break;
				}
				--bucketExtent;
				bucketEnd = previousIndex;
			}
			bucketExtents[bucketBegin] = static_cast<std::uint32_t>(bucketExtent);
		}

		return 1;
	}

	template <typename K>
	[[nodiscard]] size_type findIndex(const K& k) const {
		if (!empty()) {
			const size_type bucketBegin = getBucketIndexOfHash(hash(k));
			const size_type bucketExtent = bucketExtents[bucketBegin];
			const size_type bucketEnd = getWrappedIndex(bucketBegin + bucketExtent);
			size_type index = bucketBegin;
			while (index != bucketEnd && (!isActive(index) || !equal(keys[index], k))) {
				index = getWrappedIndex(index + 1);
			}
			if (index != bucketEnd) {
				return index;
			}
		}
		return elementCapacity;
	}

	typename std::allocator_traits<UInt64Allocator>::pointer activeBits{};
	typename std::allocator_traits<UInt32Allocator>::pointer bucketExtents{};
	typename std::allocator_traits<KeyAllocator>::pointer keys{};
	typename std::allocator_traits<ValueAllocator>::pointer values{};
	size_type elementCount = 0;
	size_type elementCapacity = 0;
	[[no_unique_address]] hasher hash;
	[[no_unique_address]] key_equal equal;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename Key, typename T, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using HashMap = grem::HashMap<Key, T, Hash, KeyEqual, std::pmr::polymorphic_allocator<Pair<const Key, T>>>;

} // namespace grem::pmr

#endif
