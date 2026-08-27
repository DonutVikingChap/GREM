// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ORDERED_MULTIMAP_HPP
#define GREM_CORE_DATA_ORDERED_MULTIMAP_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Pair.hpp>

#include <algorithm> // std::min, std::max, std::equal, std::lexicographical_compare, std::stable_sort, std::inplace_merge, std::equal_range, std::upper_bound, std::lower_bound, std::rotate, std::shift_left
#include <cstddef>   // std::size_t, std::ptrdiff_t
#include <functional>       // std::less
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::input_iterator, std::sentinel_for, std::reverse_iterator, std::..._iterator_tag, std::begin, std::end
#include <memory>           // std::allocator, std::allocator_traits, std::to_address
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::length_error, std::logic_error
#include <type_traits>      // std::is_..._v, std::conditional_t
#include <utility>          // std::move, std::forward, std::swap, std::exchange

namespace grem {

template <typename Key, typename T, typename Compare = std::less<Key>, typename Allocator = std::allocator<Pair<Key, T>>>
class OrderedMultimap {
public:
	using key_type = Key;
	using mapped_type = T;
	using key_compare = Compare;
	using size_type = std::size_t;
	using allocator_type = Allocator;

private:
	template <bool Const>
	class Iterator {
	private:
		using V = std::conditional_t<Const, const T, T>;

	public:
		using value_type = Pair<Key, T>;
		using reference = Pair<const Key&, V&>;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::random_access_iterator_tag;

		struct pointer {
			reference ref;

			[[nodiscard]] constexpr reference* operator->() noexcept {
				return &ref;
			}
		};

		Iterator() noexcept = default;

		constexpr explicit Iterator(value_type* element) noexcept
			: element(element) {}

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{element};
		}

		[[nodiscard]] constexpr reference operator*() const {
			GREM_ASSERT(element);
			return reference{element->first, element->second};
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return pointer{**this};
		}

		[[nodiscard]] constexpr reference operator[](difference_type n) const {
			GREM_ASSERT(element);
			return reference{element[n].first, element[n].second};
		}

		constexpr Iterator& operator++() {
			++element;
			return *this;
		}

		constexpr Iterator& operator--() {
			--element;
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
			element += n;
			return *this;
		}

		constexpr Iterator& operator-=(difference_type n) {
			element -= n;
			return *this;
		}

		[[nodiscard]] friend constexpr Iterator operator+(Iterator a, difference_type b) {
			return Iterator{a.element + b};
		}

		[[nodiscard]] friend constexpr Iterator operator+(difference_type a, Iterator b) {
			return Iterator{a + b.element};
		}

		[[nodiscard]] friend constexpr Iterator operator-(Iterator a, difference_type b) {
			return Iterator{a.element - b};
		}

		[[nodiscard]] friend constexpr difference_type operator-(Iterator a, Iterator b) {
			return a.element - b.element;
		}

		[[nodiscard]] friend constexpr bool operator==(Iterator a, Iterator b) {
			return a.element == b.element;
		}

		[[nodiscard]] friend constexpr auto operator<=>(Iterator a, Iterator b) {
			return a.element <=> b.element;
		}

	private:
		value_type* element = nullptr;
	};

public:
	using iterator = Iterator<false>;
	using const_iterator = Iterator<true>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using value_type = typename iterator::value_type;
	using difference_type = typename iterator::difference_type;
	using reference = typename iterator::reference;
	using const_reference = typename const_iterator::reference;
	using pointer = typename iterator::pointer;
	using const_pointer = typename const_iterator::pointer;

	class value_compare {
	public:
		[[nodiscard]] bool operator()(const value_type& a, const value_type& b) const {
			return comp(a.first, b.first);
		}

	protected:
		friend OrderedMultimap;

		value_compare(Compare c)
			: comp(std::move(c)) {}

		Compare comp;
	};

	OrderedMultimap()
		: OrderedMultimap(Compare()) {}

	explicit OrderedMultimap(const Compare& compare, const Allocator& allocator = Allocator())
		: compare(compare)
		, allocator(allocator) {}

	explicit OrderedMultimap(const Allocator& allocator)
		: OrderedMultimap(Compare(), allocator) {}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	OrderedMultimap(InputIterator first, Sentinel last, const Compare& compare = Compare(), const Allocator& allocator = Allocator())
		: OrderedMultimap(compare, allocator) {
		insert(first, last);
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	OrderedMultimap(InputIterator first, Sentinel last, const Allocator& allocator)
		: OrderedMultimap(first, last, Compare(), allocator) {}

	OrderedMultimap(std::initializer_list<value_type> ilist, const Compare& compare = Compare(), const Allocator& allocator = Allocator())
		: OrderedMultimap(ilist.begin(), ilist.end(), compare, allocator) {}

	OrderedMultimap(std::initializer_list<value_type> ilist, const Allocator& allocator)
		: OrderedMultimap(ilist, Compare(), allocator) {}

	OrderedMultimap(const OrderedMultimap& other)
		: OrderedMultimap(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	OrderedMultimap(const OrderedMultimap& other, const Allocator& allocator)
		: OrderedMultimap(allocator) {
		*this = other;
	}

	OrderedMultimap(OrderedMultimap&& other) noexcept
		: elements(std::exchange(other.elements, nullptr))
		, elementCount(std::exchange(other.elementCount, size_type{0}))
		, elementCapacity(std::exchange(other.elementCapacity, size_type{0}))
		, compare(std::move(other.compare))
		, allocator(other.get_allocator()) {}

	OrderedMultimap(OrderedMultimap&& other, const Allocator& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: OrderedMultimap(allocator) {
		if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.get_allocator()) {
				reserve(other.size());
				for (size_type index = 0; index < other.size(); ++index) {
					if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
						std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + index, std::move(other.elements[index]));
					} else {
						std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + index, other.elements[index]);
					}
					++elementCount;
				}
				other.clear();
				return;
			}
		}
		elements = std::exchange(other.elements, nullptr);
		elementCount = std::exchange(other.elementCount, size_type{0});
		elementCapacity = std::exchange(other.elementCapacity, size_type{0});
		compare = std::move(other.compare);
	}

	~OrderedMultimap() {
		reset();
	}

	OrderedMultimap& operator=(const OrderedMultimap& other) {
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
		while (elementCount > other.elementCount) {
			--elementCount;
			std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + elementCount);
		}
		reserve(other.size());
		for (size_type index = 0; index < size(); ++index) {
			elements[index] = other.elements[index];
		}
		for (size_type index = size(); index < other.size(); ++index) {
			std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + index, other.elements[index]);
			++elementCount;
		}
		return *this;
	}

	OrderedMultimap& operator=(OrderedMultimap&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					clear();
					reserve(other.size());
					for (size_type index = 0; index < other.size(); ++index) {
						if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
							std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + index, std::move(other.elements[index]));
						} else {
							std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + index, other.elements[index]);
						}
						++elementCount;
					}
					other.clear();
					return *this;
				}
			}
		}
		reset();
		elements = std::exchange(other.elements, nullptr);
		elementCount = std::exchange(other.elementCount, size_type{0});
		elementCapacity = std::exchange(other.elementCapacity, size_type{0});
		compare = std::move(other.compare);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.get_allocator();
		}
		return *this;
	}

	OrderedMultimap& operator=(std::initializer_list<value_type> ilist) {
		clear();
		insert(ilist);
		return *this;
	}

	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	[[nodiscard]] iterator begin() noexcept {
		return iterator{std::to_address(elements)};
	}

	[[nodiscard]] const_iterator begin() const noexcept {
		return const_iterator{std::to_address(elements)};
	}

	[[nodiscard]] const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] iterator end() noexcept {
		return iterator{std::to_address(elements) + elementCount};
	}

	[[nodiscard]] const_iterator end() const noexcept {
		return const_iterator{std::to_address(elements) + elementCount};
	}

	[[nodiscard]] const_iterator cend() const noexcept {
		return end();
	}

	[[nodiscard]] reverse_iterator rbegin() noexcept {
		return reverse_iterator{end()};
	}

	[[nodiscard]] const_reverse_iterator rbegin() const noexcept {
		return const_reverse_iterator{end()};
	}

	[[nodiscard]] const_reverse_iterator crbegin() const noexcept {
		return rbegin();
	}

	[[nodiscard]] reverse_iterator rend() noexcept {
		return reverse_iterator{begin()};
	}

	[[nodiscard]] const_reverse_iterator rend() const noexcept {
		return const_reverse_iterator{begin()};
	}

	[[nodiscard]] const_reverse_iterator crend() const noexcept {
		return rend();
	}

	[[nodiscard]] bool empty() const noexcept {
		return elementCount == 0;
	}

	[[nodiscard]] size_type size() const noexcept {
		return elementCount;
	}

	[[nodiscard]] size_type capacity() const noexcept {
		return elementCapacity;
	}

	[[nodiscard]] size_type max_size() const noexcept {
		return static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator));
	}

	void clear() noexcept {
		for (size_type index = 0; index < elementCount; ++index) {
			std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + index);
		}
		elementCount = 0;
	}

	void reserve(size_type newCapacity) {
		if (newCapacity > max_size()) {
			throw std::length_error{"newCapacity > max_size()"};
		}

		const size_type cap = capacity();
		if (newCapacity <= cap) {
			return;
		}

		OrderedMultimap newMap{compare.comp, allocator};
		newMap.elements = std::allocator_traits<Allocator>::allocate(allocator, newCapacity);
		newMap.elementCapacity = newCapacity;

		for (size_type index = 0; index < elementCount; ++index) {
			if constexpr (std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_move_constructible_v<T>) {
				std::allocator_traits<Allocator>::construct(allocator, std::to_address(newMap.elements) + index, std::move(elements[index]));
			} else {
				std::allocator_traits<Allocator>::construct(allocator, std::to_address(newMap.elements) + index, elements[index]);
			}
			++newMap.elementCount;
		}
		GREM_ASSERT(allocator == newMap.get_allocator());
		*this = std::move(newMap);
	}

	template <typename P>
	iterator insert(P&& value) {
		return emplace(std::forward<P>(value));
	}

	template <typename P>
	iterator insert(const_iterator pos, P&& value) requires(!std::is_convertible_v<P, const_iterator>) {
		return emplace_hint(pos, std::forward<P>(value));
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	void insert(InputIterator first, Sentinel last) {
		const size_type oldSize = size();
		try {
			while (first != last) {
				auto [key, value] = *first++;
				pushElement(std::move(key), std::move(value));
			}
			value_type* const oldElementsBegin = getMutableBegin();
			value_type* const newElementsBegin = std::to_address(elements) + oldSize;
			value_type* const elementsEnd = getMutableEnd();
			std::stable_sort(newElementsBegin, elementsEnd, CompareWrapper{key_comp()});
			std::inplace_merge(oldElementsBegin, newElementsBegin, elementsEnd, CompareWrapper{key_comp()});
		} catch (...) {
			while (size() > oldSize) {
				popElement();
			}
			throw;
		}
	}

	void insert(std::initializer_list<value_type> ilist) {
		insert(ilist.begin(), ilist.end());
	}

	template <typename R>
	void insert_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
		insert(std::begin(r), std::end(r));
	}

	template <typename... Args>
	iterator emplace(Args&&... args) {
		Pair<Key, T> value{std::forward<Args>(args)...};
		const auto last = upper_bound(value.first);
		return insertElement(last, std::move(value.first), std::move(value.second));
	}

	template <typename... Args>
	iterator emplace_hint(const_iterator hint, Args&&... args) {
		(void)hint;
		return emplace(std::forward<Args>(args)...);
	}

	iterator erase(const_iterator pos) {
		GREM_ASSERT(!empty());
		const difference_type offset = pos - cbegin();
		std::shift_left(getMutableBegin() + offset, getMutableEnd(), 1);
		popElement();
		return begin() + offset;
	}

	size_type erase(const Key& key) {
		const auto [first, last] = equal_range(key);
		return eraseN(static_cast<size_type>(first - cbegin()), static_cast<size_type>(last - first));
	}

	template <typename K>
	size_type erase(K&& x) requires(requires { typename Compare::is_transparent; }) {
		const auto [first, last] = equal_range(std::forward<K>(x));
		return eraseN(static_cast<size_type>(first - cbegin()), static_cast<size_type>(last - first));
	}

	void swap(OrderedMultimap& other) noexcept(
		std::allocator_traits<Allocator>::propagate_on_container_swap::value || // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		(std::allocator_traits<Allocator>::is_always_equal::value && std::is_nothrow_swappable_v<T>)) {
		if (this == &other) {
			return;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_swap::value && !std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.allocator) {
				throw std::logic_error{"get_allocator() != other.get_allocator()"};
			}
		}
		using std::swap;
		swap(elements, other.elements);
		swap(elementCount, other.elementCount);
		swap(elementCapacity, other.elementCapacity);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		}
	}

	friend void swap(OrderedMultimap& a, OrderedMultimap& b) noexcept {
		a.swap(b);
	}

	[[nodiscard]] size_type count(const Key& key) const noexcept {
		const auto [first, last] = equal_range(key);
		return static_cast<size_type>(last - first);
	}

	template <typename K>
	[[nodiscard]] size_type count(K&& x) const noexcept requires(requires { typename Compare::is_transparent; }) {
		const auto [first, last] = equal_range(std::forward<K>(x));
		return static_cast<size_type>(last - first);
	}

	[[nodiscard]] bool contains(const Key& key) const noexcept {
		return count(key) > 0;
	}

	template <typename K>
	[[nodiscard]] bool contains(K&& x) const noexcept {
		return count(std::forward<K>(x)) > 0;
	}

	[[nodiscard]] Pair<iterator, iterator> equal_range(const Key& key) noexcept {
		const auto [first, last] = std::equal_range(begin(), end(), key, CompareWrapper{key_comp()});
		return {first, last};
	}

	template <typename K>
	[[nodiscard]] Pair<iterator, iterator> equal_range(K&& x) noexcept {
		const auto [first, last] = std::equal_range(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
		return {first, last};
	}

	[[nodiscard]] Pair<const_iterator, const_iterator> equal_range(const Key& key) const noexcept {
		const auto [first, last] = std::equal_range(begin(), end(), key, CompareWrapper{key_comp()});
		return {first, last};
	}

	template <typename K>
	[[nodiscard]] Pair<const_iterator, const_iterator> equal_range(K&& x) const noexcept {
		const auto [first, last] = std::equal_range(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
		return {first, last};
	}

	[[nodiscard]] iterator lower_bound(const Key& key) noexcept {
		return std::lower_bound(begin(), end(), key, CompareWrapper{key_comp()});
	}

	template <typename K>
	[[nodiscard]] iterator lower_bound(K&& x) noexcept {
		return std::lower_bound(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
	}

	[[nodiscard]] const_iterator lower_bound(const Key& key) const noexcept {
		return std::lower_bound(begin(), end(), key, CompareWrapper{key_comp()});
	}

	template <typename K>
	[[nodiscard]] const_iterator lower_bound(K&& x) const noexcept {
		return std::lower_bound(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
	}

	[[nodiscard]] iterator upper_bound(const Key& key) noexcept {
		return std::upper_bound(begin(), end(), key, CompareWrapper{key_comp()});
	}

	template <typename K>
	[[nodiscard]] iterator upper_bound(K&& x) noexcept {
		return std::upper_bound(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
	}

	[[nodiscard]] const_iterator upper_bound(const Key& key) const noexcept {
		return std::upper_bound(begin(), end(), key, CompareWrapper{key_comp()});
	}

	template <typename K>
	[[nodiscard]] const_iterator upper_bound(K&& x) const noexcept {
		return std::upper_bound(begin(), end(), std::forward<K>(x), CompareWrapper{key_comp()});
	}

	[[nodiscard]] key_compare key_comp() const {
		return compare.comp;
	}

	[[nodiscard]] value_compare value_comp() const {
		return compare;
	}

	[[nodiscard]] constexpr bool operator==(const OrderedMultimap& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const OrderedMultimap& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const OrderedMultimap& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const OrderedMultimap& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const OrderedMultimap& other) const {
		return !(*this < other);
	}

	template <typename Predicate>
	friend size_type erase_if(OrderedMultimap& container, Predicate predicate) {
		const auto it = std::remove_if(container.getMutableBegin(), container.getMutableEnd(), predicate);
		const size_type offset = static_cast<size_type>(it - container.getMutableBegin());
		const size_type count = static_cast<size_type>(container.getMutableEnd() - it);
		container.eraseN(offset, count);
		return count;
	}

protected:
	struct CompareWrapper {
		[[nodiscard]] bool operator()(const auto& a, const auto& b) const {
			constexpr auto getKey = []<typename K>(const K& x) -> const auto& {
				if constexpr (std::is_convertible_v<K, value_type>) {
					return x.first;
				} else {
					return x;
				}
			};
			return comp(getKey(a), getKey(b));
		}

		Compare comp;
	};

	[[nodiscard]] value_type* getMutableBegin() {
		return std::to_address(elements);
	}

	[[nodiscard]] value_type* getMutableEnd() {
		return std::to_address(elements) + elementCount;
	}

	void reset() noexcept {
		clear();
		if (elementCapacity > 0) {
			std::allocator_traits<Allocator>::deallocate(allocator, elements, elementCapacity);
			elements = nullptr;
			elementCapacity = 0;
		}
	}

	template <typename... Args>
	void pushElement(Args&&... args) {
		const size_type newSize = size() + 1;
		if (newSize > elementCapacity) {
			reserve(std::max(newSize, elementCapacity + elementCapacity / 2));
		}
		std::allocator_traits<Allocator>::construct(allocator, std::to_address(elements) + elementCount, std::forward<Args>(args)...);
		elementCount = newSize;
	}

	void popElement() {
		--elementCount;
		std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + elementCount);
	}

	template <typename... Args>
	iterator insertElement(const_iterator pos, Args&&... args) {
		const difference_type offset = pos - cbegin();
		pushElement(std::forward<Args>(args)...);
		try {
			std::rotate(getMutableBegin() + offset, getMutableEnd() - 1, getMutableEnd());
		} catch (...) {
			popElement();
			throw;
		}
		return begin() + offset;
	}

	size_type eraseN(size_type offset, size_type count) {
		if (count > 0) {
			GREM_ASSERT(count <= size());
			const auto newEnd = std::shift_left(getMutableBegin() + static_cast<difference_type>(offset), getMutableEnd(), static_cast<difference_type>(count));
			for (size_type i = static_cast<size_type>(newEnd - getMutableBegin()); i < elementCount; ++i) {
				std::allocator_traits<Allocator>::destroy(allocator, std::to_address(elements) + i);
			}
			elementCount -= count;
		}
		return count;
	}

	typename std::allocator_traits<Allocator>::pointer elements{};
	size_type elementCount = 0;
	size_type elementCapacity = 0;
	[[no_unique_address]] value_compare compare;
	[[no_unique_address]] Allocator allocator;
};

} // namespace grem

namespace grem::pmr {

template <typename Key, typename T, typename Compare = std::less<Key>>
using OrderedMultimap = grem::OrderedMultimap<Key, T, Compare, std::pmr::polymorphic_allocator<Pair<Key, T>>>;

} // namespace grem::pmr

#endif
