// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_ORDERED_MAP_HPP
#define GREM_CORE_DATA_ORDERED_MAP_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/OrderedMultimap.hpp>
#include <GREM/core/data/Pair.hpp>

#include <algorithm>        // std::unique
#include <functional>       // std::less
#include <initializer_list> // std::initializer_list
#include <iterator>         // std::input_iterator, std::sentinel_for, std::begin, std::end
#include <memory>           // std::allocator
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <stdexcept>        // std::out_of_range
#include <tuple>            // std::forward_as_tuple
#include <type_traits>      // std::is_..._v
#include <utility>          // std::move, std::forward, std::piecewise_construct

namespace grem {

template <typename Key, typename T, typename Compare = std::less<Key>, typename Allocator = std::allocator<Pair<Key, T>>>
class OrderedMap : private OrderedMultimap<Key, T, Compare, Allocator> {
private:
	using Base = OrderedMultimap<Key, T, Compare, Allocator>;

public:
	using typename Base::allocator_type;
	using typename Base::const_iterator;
	using typename Base::const_pointer;
	using typename Base::const_reference;
	using typename Base::const_reverse_iterator;
	using typename Base::difference_type;
	using typename Base::iterator;
	using typename Base::key_compare;
	using typename Base::key_type;
	using typename Base::mapped_type;
	using typename Base::pointer;
	using typename Base::reference;
	using typename Base::reverse_iterator;
	using typename Base::size_type;
	using typename Base::value_compare;
	using typename Base::value_type;

	OrderedMap()
		: OrderedMap(Compare()) {}

	explicit OrderedMap(const Compare& compare, const Allocator& allocator = Allocator())
		: Base(compare, allocator) {
		eraseDuplicates();
	}

	explicit OrderedMap(const Allocator& allocator)
		: OrderedMap(Compare(), allocator) {}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	OrderedMap(InputIterator first, Sentinel last, const Compare& compare = Compare(), const Allocator& allocator = Allocator())
		: Base(first, last, compare, allocator) {
		eraseDuplicates();
	}

	template <std::input_iterator InputIterator, std::sentinel_for<InputIterator> Sentinel>
	OrderedMap(InputIterator first, Sentinel last, const Allocator& allocator)
		: OrderedMap(first, last, Compare(), allocator) {}

	OrderedMap(std::initializer_list<value_type> ilist, const Compare& compare = Compare(), const Allocator& allocator = Allocator())
		: OrderedMap(ilist.begin(), ilist.end(), compare, allocator) {}

	OrderedMap(std::initializer_list<value_type> ilist, const Allocator& allocator)
		: OrderedMap(ilist, Compare(), allocator) {}

	OrderedMap& operator=(std::initializer_list<value_type> ilist) {
		Base::operator=(ilist);
		eraseDuplicates();
		return *this;
	}

	using Base::begin;
	using Base::cbegin;
	using Base::cend;
	using Base::clear;
	using Base::crbegin;
	using Base::crend;
	using Base::empty;
	using Base::end;
	using Base::get_allocator;
	using Base::reserve;

	[[nodiscard]] T& at(const Key& key) {
		if (const auto it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	template <typename K>
	[[nodiscard]] T& at(const K& x) requires(requires { typename Compare::is_tranparent; }) {
		if (const auto it = find(x); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] const T& at(const Key& key) const {
		if (const auto it = find(key); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	template <typename K>
	[[nodiscard]] const T& at(const K& x) const requires(requires { typename Compare::is_tranparent; }) {
		if (const auto it = find(x); it != end()) {
			return it->second;
		}
		throw std::out_of_range{"!contains(key)"};
	}

	[[nodiscard]] T& operator[](const Key& key) {
		return try_emplace(key).first->second;
	}

	[[nodiscard]] T& operator[](Key&& key) {
		return try_emplace(std::move(key)).first->second;
	}

	template <typename K>
	[[nodiscard]] T& operator[](K&& x) requires(requires { typename Compare::is_transparent; }) {
		return try_emplace(std::forward<K>(x)).first->second;
	}

	using Base::capacity;
	using Base::max_size;
	using Base::rbegin;
	using Base::rend;
	using Base::size;

	template <typename P>
	Pair<iterator, bool> insert(P&& value) {
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
				Base::pushElement(std::move(key), std::move(value));
			}
			Base::mergeElements(oldSize);
			eraseDuplicates();
		} catch (...) {
			while (size() > oldSize) {
				Base::popElement();
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
	Pair<iterator, bool> emplace(Args&&... args) {
		value_type value{std::forward<Args>(args)...};
		const auto [first, last] = equal_range(value.first);
		if (first != last) {
			return {first, false};
		}
		const auto it = Base::insertElement(last, std::move(value));
		return {it, true};
	}

	template <typename... Args>
	iterator emplace_hint(const_iterator hint, Args&&... args) {
		(void)hint;
		return emplace(std::forward<Args>(args)...).first;
	}

	template <typename... Args>
	Pair<iterator, bool> try_emplace(const Key& k, Args&&... args) {
		const auto [first, last] = equal_range(k);
		if (first != last) {
			return {first, false};
		}
		const auto it = Base::insertElement(last, std::piecewise_construct, std::forward_as_tuple(k), std::forward_as_tuple(std::forward<Args>(args)...));
		return {it, true};
	}

	template <typename... Args>
	Pair<iterator, bool> try_emplace(Key&& k, Args&&... args) {
		const auto [first, last] = equal_range(k);
		if (first != last) {
			return {first, false};
		}
		const auto it = Base::insertElement(last, std::piecewise_construct, std::forward_as_tuple(std::move(k)), std::forward_as_tuple(std::forward<Args>(args)...));
		return {it, true};
	}

	template <typename K, typename... Args>
	Pair<iterator, bool> try_emplace(K&& k, Args&&... args)
		requires(!std::is_convertible_v<K &&, const_iterator> && !std::is_convertible_v<K &&, iterator> && requires { typename Compare::is_transparent; }) {
		const auto [first, last] = equal_range(k);
		if (first != last) {
			return {first, false};
		}
		const auto it = Base::insertElement(last, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)), std::forward_as_tuple(std::forward<Args>(args)...));
		return {it, true};
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, const Key& k, Args&&... args) {
		(void)hint;
		return try_emplace(k, std::forward<Args>(args)...);
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, Key&& k, Args&&... args) {
		(void)hint;
		return try_emplace(std::move(k), std::forward<Args>(args)...);
	}

	template <typename K, typename... Args>
	iterator try_emplace(const_iterator hint, K&& k, Args&&... args) {
		(void)hint;
		return try_emplace(std::forward<K>(k), std::forward<Args>(args)...);
	}

	using Base::erase;
	using Base::swap;

	friend void swap(OrderedMap& a, OrderedMap& b) noexcept {
		a.swap(b);
	}

	using Base::contains;
	using Base::count;

	[[nodiscard]] iterator find(const Key& key) noexcept {
		if (const auto [first, last] = equal_range(key); first != last) {
			return first;
		}
		return end();
	}

	template <typename K>
	[[nodiscard]] iterator find(K&& x) noexcept {
		if (const auto [first, last] = equal_range(std::forward<K>(x)); first != last) {
			return first;
		}
		return end();
	}

	[[nodiscard]] const_iterator find(const Key& key) const noexcept {
		if (const auto [first, last] = equal_range(key); first != last) {
			return first;
		}
		return end();
	}

	template <typename K>
	[[nodiscard]] const_iterator find(K&& x) const noexcept {
		if (const auto [first, last] = equal_range(std::forward<K>(x)); first != last) {
			return first;
		}
		return end();
	}

	using Base::equal_range;
	using Base::key_comp;
	using Base::lower_bound;
	using Base::upper_bound;
	using Base::value_comp;

	[[nodiscard]] bool operator==(const OrderedMap& other) const = default;

	[[nodiscard]] bool operator<(const OrderedMap& other) const {
		return static_cast<const Base&>(*this) < static_cast<const Base&>(other);
	}

	[[nodiscard]] bool operator<=(const OrderedMap& other) const {
		return static_cast<const Base&>(*this) <= static_cast<const Base&>(other);
	}

	[[nodiscard]] bool operator>(const OrderedMap& other) const {
		return static_cast<const Base&>(*this) > static_cast<const Base&>(other);
	}

	[[nodiscard]] bool operator>=(const OrderedMap& other) const {
		return static_cast<const Base&>(*this) >= static_cast<const Base&>(other);
	}

	template <typename Predicate>
	friend size_type erase_if(OrderedMap& container, Predicate predicate) {
		return erase_if(static_cast<Base&>(container), predicate);
	}

private:
	void eraseDuplicates() {
		try {
			const auto newEnd = std::unique(Base::getMutableBegin(), Base::getMutableEnd(),
				[comp = key_comp()](const auto& a, const auto& b) -> bool { return !comp(a.first, b.first) && !comp(b.first, a.first); });
			Base::eraseN(static_cast<size_type>(newEnd - Base::getMutableBegin()), static_cast<size_type>(Base::getMutableEnd() - newEnd));
		} catch (...) {
			clear();
			throw;
		}
	}
};

} // namespace grem

namespace grem::pmr {

template <typename Key, typename T, typename Compare = std::less<Key>>
using OrderedMap = grem::OrderedMap<Key, T, Compare, std::pmr::polymorphic_allocator<Pair<Key, T>>>;

} // namespace grem::pmr

#endif
