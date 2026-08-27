// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_TABLE_HPP
#define GREM_CORE_DATA_TABLE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/metaprogramming.hpp>

#include <algorithm>       // std::min, std::copy, std::move, std::equal, std::lexicographical_compare
#include <cstddef>         // std::size_t, std::ptrdiff_t
#include <iterator>        // std::iterator_traits, std::random_access_iterator_tag, std::reverse_iterator
#include <limits>          // std::numeric_limits
#include <memory>          // std::allocator, std::allocator_traits, std::to_address
#include <memory_resource> // std::pmr::polymorphic_allocator
#include <stdexcept>       // std::out_of_range, std::length_error
#include <type_traits>     // std::is_..._v, std::remove_..._t
#include <utility>         // std::move, std::forward, std::exchange, std::swap, std::tuple_element_t, std::as_const

namespace grem {

template <typename Row, typename Allocator = std::allocator<Row>>
class Table;

namespace detail {

template <typename Row, typename Allocator>
class TableIterator;

template <typename... Ts, typename Allocator>
class TableIterator<Tuple<Ts...>, Allocator> {
public:
	static_assert(sizeof...(Ts) > 0);

	using difference_type = std::ptrdiff_t;
	using value_type = Tuple<Ts...>;
	using reference = Tuple<Ts&...>;
	using iterator_category = std::random_access_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	TableIterator() = default;

	constexpr TableIterator(Table<value_type, Allocator>* table, std::size_t rowIndex) noexcept
		: table(table)
		, rowIndex(rowIndex) {}

	constexpr operator TableIterator<const value_type, Allocator>() const noexcept {
		return TableIterator<const value_type, Allocator>{table, rowIndex};
	}

	[[nodiscard]] constexpr reference operator*() const {
		return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
			return grem::tie(*(get<Indices>(table->columns) + rowIndex)...);
		}(std::make_index_sequence<sizeof...(Ts)>{});
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return pointer{**this};
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	constexpr TableIterator& operator++() {
		++rowIndex;
		return *this;
	}

	constexpr TableIterator& operator--() {
		--rowIndex;
		return *this;
	}

	constexpr TableIterator operator++(int) {
		return TableIterator{table, rowIndex++};
	}

	constexpr TableIterator operator--(int) {
		return TableIterator{table, rowIndex--};
	}

	constexpr TableIterator& operator+=(difference_type n) {
		rowIndex += static_cast<std::size_t>(n);
		return *this;
	}

	constexpr TableIterator& operator-=(difference_type n) {
		rowIndex -= static_cast<std::size_t>(n);
		return *this;
	}

	[[nodiscard]] friend constexpr TableIterator operator+(TableIterator a, difference_type b) {
		return TableIterator{a.table, a.rowIndex + static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr TableIterator operator+(difference_type a, TableIterator b) {
		return TableIterator{b.table, static_cast<std::size_t>(a) + b.rowIndex};
	}

	[[nodiscard]] friend constexpr TableIterator operator-(TableIterator a, difference_type b) {
		return TableIterator{a.table, a.rowIndex - static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr difference_type operator-(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return static_cast<difference_type>(a.rowIndex - b.rowIndex);
	}

	[[nodiscard]] friend constexpr bool operator==(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return a.rowIndex == b.rowIndex;
	}

	[[nodiscard]] friend constexpr auto operator<=>(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return a.rowIndex <=> b.rowIndex;
	}

private:
	Table<value_type, Allocator>* table;
	std::size_t rowIndex;
};

template <typename... Ts, typename Allocator>
class TableIterator<const Tuple<Ts...>, Allocator> {
public:
	using difference_type = std::ptrdiff_t;
	using value_type = Tuple<Ts...>;
	using reference = Tuple<const Ts&...>;
	using iterator_category = std::random_access_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	TableIterator() = default;

	constexpr TableIterator(const Table<value_type, Allocator>* table, std::size_t rowIndex) noexcept
		: table(table)
		, rowIndex(rowIndex) {}

	[[nodiscard]] constexpr reference operator*() const {
		return [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
			return grem::tie(std::as_const(*(get<Indices>(table->columns) + rowIndex))...);
		}(std::make_index_sequence<sizeof...(Ts)>{});
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return pointer{**this};
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	constexpr TableIterator& operator++() {
		++rowIndex;
		return *this;
	}

	constexpr TableIterator& operator--() {
		--rowIndex;
		return *this;
	}

	constexpr TableIterator operator++(int) {
		return TableIterator{table, rowIndex++};
	}

	constexpr TableIterator operator--(int) {
		return TableIterator{table, rowIndex--};
	}

	constexpr TableIterator& operator+=(difference_type n) {
		rowIndex += static_cast<std::size_t>(n);
		return *this;
	}

	constexpr TableIterator& operator-=(difference_type n) {
		rowIndex -= static_cast<std::size_t>(n);
		return *this;
	}

	[[nodiscard]] friend constexpr TableIterator operator+(TableIterator a, difference_type b) {
		return TableIterator{a.table, a.rowIndex + static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr TableIterator operator+(difference_type a, TableIterator b) {
		return TableIterator{b.table, static_cast<std::size_t>(a) + b.rowIndex};
	}

	[[nodiscard]] friend constexpr TableIterator operator-(TableIterator a, difference_type b) {
		return TableIterator{a.table, a.rowIndex - static_cast<std::size_t>(b)};
	}

	[[nodiscard]] friend constexpr difference_type operator-(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return static_cast<difference_type>(a.rowIndex - b.rowIndex);
	}

	[[nodiscard]] friend constexpr bool operator==(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return a.rowIndex == b.rowIndex;
	}

	[[nodiscard]] friend constexpr auto operator<=>(TableIterator a, TableIterator b) {
		GREM_ASSERT(a.table == b.table);
		return a.rowIndex <=> b.rowIndex;
	}

private:
	const Table<value_type, Allocator>* table;
	std::size_t rowIndex;
};

} // namespace detail

template <typename... Ts, typename Allocator>
class Table<Tuple<Ts...>, Allocator> {
public:
	using value_type = Tuple<Ts...>;
	using allocator_type = Allocator;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = Tuple<Ts&...>;
	using const_reference = Tuple<const Ts&...>;
	using iterator = detail::TableIterator<value_type, Allocator>;
	using const_iterator = detail::TableIterator<const value_type, Allocator>;
	using pointer = typename iterator::pointer;
	using const_pointer = typename const_iterator::pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr Table() = default;

	constexpr explicit Table(const Allocator& allocator) noexcept
		: allocator(allocator) {}

	constexpr Table(const Table& other)
		: Table(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr Table(const Table& other, const Allocator& allocator)
		: Table(allocator) {
		assign(other);
	}

	constexpr Table(Table&& other) noexcept
		: rowCount(std::exchange(other.rowCount, size_type{0}))
		, rowCapacity(std::exchange(other.rowCapacity, size_type{0}))
		, columns(std::exchange(other.columns, {}))
		, allocator(other.get_allocator()) {}

	constexpr Table(Table&& other, const Allocator& allocator) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: Table(allocator) {
		if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			if (allocator != other.get_allocator()) {
				assign(std::move(other));
				other.clear();
				return;
			}
		}
		rowCount = std::exchange(other.rowCount, size_type{0});
		rowCapacity = std::exchange(other.rowCapacity, size_type{0});
		columns = std::exchange(other.columns, {});
	}

	constexpr ~Table() {
		reset();
	}

	constexpr Table& operator=(const Table& other) {
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
		assign(other);
		return *this;
	}

	constexpr Table& operator=(Table&& other) noexcept(
		std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		if (this == &other) {
			return *this;
		}
		if constexpr (!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					assign(std::move(other));
					other.clear();
					return *this;
				}
			}
		}
		reset();
		rowCount = std::exchange(other.rowCount, size_type{0});
		rowCapacity = std::exchange(other.rowCapacity, size_type{0});
		columns = std::exchange(other.columns, {});
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			allocator = other.get_allocator();
		}
		return *this;
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return allocator;
	}

	template <typename T>
	[[nodiscard]] constexpr T& get(size_type rowIndex) {
		using ColumnPointer = typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>::pointer;
		return columns.template get<ColumnPointer>()[rowIndex];
	}

	template <typename T>
	[[nodiscard]] constexpr const T& get(size_type rowIndex) const {
		using ColumnPointer = typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>::pointer;
		return columns.template get<ColumnPointer>()[rowIndex];
	}

	template <std::size_t ColumnIndex>
	[[nodiscard]] constexpr decltype(auto) get(size_type rowIndex) {
		return columns.template get<ColumnIndex>()[rowIndex];
	}

	template <std::size_t ColumnIndex>
	[[nodiscard]] constexpr decltype(auto) get(size_type rowIndex) const {
		return columns.template get<ColumnIndex>()[rowIndex];
	}

	template <typename T>
	[[nodiscard]] constexpr Span<T> column() {
		using ColumnPointer = typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>::pointer;
		return Span<T>{columns.template get<ColumnPointer>(), rowCount};
	}

	template <typename T>
	[[nodiscard]] constexpr Span<const T> column() const {
		using ColumnPointer = typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>::pointer;
		return Span<const T>{columns.template get<ColumnPointer>(), rowCount};
	}

	template <std::size_t ColumnIndex>
	[[nodiscard]] constexpr auto column() {
		using T = std::remove_cvref_t<decltype(*columns.template get<ColumnIndex>())>;
		return Span<T>{columns.template get<ColumnIndex>(), rowCount};
	}

	template <std::size_t ColumnIndex>
	[[nodiscard]] constexpr auto column() const {
		using T = std::remove_cvref_t<decltype(*columns.template get<ColumnIndex>())>;
		return Span<const T>{columns.template get<ColumnIndex>(), rowCount};
	}

	[[nodiscard]] constexpr reference at(size_type pos) {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
	}

	[[nodiscard]] constexpr const_reference at(size_type pos) const {
		if (pos >= size()) {
			throw std::out_of_range{"pos >= size()"};
		}
		return (*this)[pos];
	}

	[[nodiscard]] constexpr reference operator[](size_type pos) {
		GREM_ASSERT(pos < size());
		return *(begin() + static_cast<difference_type>(pos));
	}

	[[nodiscard]] constexpr const_reference operator[](size_type pos) const {
		GREM_ASSERT(pos < size());
		return *(begin() + static_cast<difference_type>(pos));
	}

	[[nodiscard]] constexpr reference front() {
		GREM_ASSERT(!empty());
		return *begin();
	}

	[[nodiscard]] constexpr const_reference front() const {
		GREM_ASSERT(!empty());
		return *begin();
	}

	[[nodiscard]] constexpr reference back() {
		GREM_ASSERT(!empty());
		return *(end() - 1);
	}

	[[nodiscard]] constexpr const_reference back() const {
		GREM_ASSERT(!empty());
		return *(end() - 1);
	}

	[[nodiscard]] constexpr iterator begin() noexcept {
		return iterator{this, 0};
	}

	[[nodiscard]] constexpr const_iterator begin() const noexcept {
		return const_iterator{this, 0};
	}

	[[nodiscard]] constexpr const_iterator cbegin() const noexcept {
		return begin();
	}

	[[nodiscard]] constexpr iterator end() noexcept {
		return iterator{this, rowCount};
	}

	[[nodiscard]] constexpr const_iterator end() const noexcept {
		return const_iterator{this, rowCount};
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
		return size() == 0;
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return rowCount;
	}

	[[nodiscard]] constexpr size_type max_size() const noexcept {
		return std::min({
			static_cast<size_type>(std::numeric_limits<difference_type>::max()),
			static_cast<size_type>(std::numeric_limits<size_type>::max() / std::max({sizeof(Ts)...})),
			static_cast<size_type>(std::allocator_traits<Allocator>::max_size(allocator)),
		});
	}

	constexpr void reserve(size_type newRowCapacity) {
		if (newRowCapacity <= capacity()) {
			return;
		}
		if (newRowCapacity > max_size()) {
			throw std::length_error{"Maximum capacity exceeded."};
		}
		Table newTable{allocator};
		newTable.rowCapacity = newRowCapacity;
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
			ColumnAllocator columnAllocator = allocator;
			newTable.columns.template get<columnIndex>() = std::allocator_traits<ColumnAllocator>::allocate(columnAllocator, newRowCapacity);
		});
		newTable.assign(std::move(*this));
		*this = std::move(newTable);
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return rowCapacity;
	}

	constexpr void shrink_to_fit() {
		if (size() < capacity()) {
			*this = Table{*this};
		}
	}

	constexpr void clear() noexcept {
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			if constexpr (!std::is_trivially_destructible_v<T>) {
				using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
				ColumnAllocator columnAllocator = allocator;
				for (size_type rowIndex = rowCount; rowIndex-- > 0;) {
					std::allocator_traits<ColumnAllocator>::destroy(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex);
				}
			}
		});
		rowCount = 0;
	}

	template <typename... Us>
	constexpr void push_back(const Tuple<Us...>& value) requires(sizeof...(Us) == sizeof...(Ts)) {
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		if (size() >= capacity()) {
			GREM_ASSERT(size() == capacity());
			reserve(std::max(capacity() + capacity() / 2, size_type{1}));
		}
		GREM_ASSERT(size() < capacity());
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
			ColumnAllocator columnAllocator = allocator;
			try {
				std::allocator_traits<ColumnAllocator>::construct(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowCount,
					value.template get<columnIndex>());
			} catch (...) {
				meta::forEachIndex<columnIndex>([&](auto otherColumnIndex) -> void {
					using OtherT = std::tuple_element_t<otherColumnIndex, value_type>;
					if constexpr (!std::is_trivially_destructible_v<OtherT>) {
						using OtherColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<OtherT>;
						OtherColumnAllocator otherColumnAllocator = allocator;
						std::allocator_traits<OtherColumnAllocator>::destroy(otherColumnAllocator, std::to_address(columns.template get<otherColumnIndex>()) + rowCount);
					}
				});
				throw;
			}
		});
		++rowCount;
	}

	template <typename... Us>
	constexpr void push_back(Tuple<Us...>&& value) requires(sizeof...(Us) == sizeof...(Ts)) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		if (size() >= max_size()) {
			throw std::length_error{"size() >= max_size()"};
		}
		if (size() >= capacity()) {
			GREM_ASSERT(size() == capacity());
			reserve(std::max(capacity() + capacity() / 2, size_type{32}));
		}
		GREM_ASSERT(size() < capacity());
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
			ColumnAllocator columnAllocator = allocator;
			try {
				std::allocator_traits<ColumnAllocator>::construct(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowCount,
					std::move(value.template get<columnIndex>()));
			} catch (...) {
				meta::forEachIndex<columnIndex>([&](auto otherColumnIndex) -> void {
					using OtherT = std::tuple_element_t<otherColumnIndex, value_type>;
					if constexpr (!std::is_trivially_destructible_v<OtherT>) {
						using OtherColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<OtherT>;
						OtherColumnAllocator otherColumnAllocator = allocator;
						std::allocator_traits<OtherColumnAllocator>::destroy(otherColumnAllocator, std::to_address(columns.template get<otherColumnIndex>()) + rowCount);
					}
				});
				throw;
			}
		});
		++rowCount;
	}

	template <typename... Args>
	constexpr reference emplace_back(Args&&... args) {
		push_back(Tuple<Ts...>(std::forward<Args>(args)...));
		return back();
	}

	constexpr void pop_back() {
		GREM_ASSERT(!empty());
		--rowCount;
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			if constexpr (!std::is_trivially_destructible_v<T>) {
				using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
				ColumnAllocator columnAllocator = allocator;
				std::allocator_traits<ColumnAllocator>::destroy(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowCount);
			}
		});
	}

	constexpr void resize(size_type newRowCount) {
		if (newRowCount < size()) {
			for (size_type i = size() - newRowCount; i-- > 0;) {
				pop_back();
			}
		} else {
			reserve(newRowCount);
			meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
				using T = std::tuple_element_t<columnIndex, value_type>;
				if constexpr (!std::is_trivially_default_constructible_v<T>) {
					using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
					ColumnAllocator columnAllocator = allocator;
					size_type rowIndex = rowCount;
					try {
						while (rowIndex < newRowCount) {
							std::allocator_traits<ColumnAllocator>::construct(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex);
							++rowIndex;
						}
					} catch (...) {
						while (rowIndex-- > rowCount) {
							std::allocator_traits<ColumnAllocator>::destroy(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex);
						}
						meta::forEachIndex<columnIndex>([&](auto otherColumnIndex) -> void {
							using OtherT = std::tuple_element_t<otherColumnIndex, value_type>;
							if constexpr (!std::is_trivially_destructible_v<OtherT>) {
								using OtherColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<OtherT>;
								OtherColumnAllocator otherColumnAllocator = allocator;
								for (size_type rowIndex = newRowCount; rowIndex-- > rowCount;) {
									std::allocator_traits<OtherColumnAllocator>::destroy(otherColumnAllocator,
										std::to_address(columns.template get<otherColumnIndex>()) + rowIndex);
								}
							}
						});
						throw;
					}
				}
			});
			rowCount = newRowCount;
		}
	}

	constexpr void swap(Table& other) noexcept(std::allocator_traits<Allocator>::is_always_equal::value) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		if (this == &other) {
			return;
		}
		using std::swap;
		swap(rowCount, other.rowCount);
		swap(rowCapacity, other.rowCapacity);
		swap(columns, other.columns);
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			swap(allocator, other.allocator);
		} else if constexpr (!std::allocator_traits<Allocator>::is_always_equal::value) {
			GREM_ASSERT(allocator == other.allocator);
		}
	}

	[[nodiscard]] constexpr bool operator==(const Table& other) const {
		return std::equal(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<(const Table& other) const {
		return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
	}

	[[nodiscard]] constexpr bool operator<=(const Table& other) const {
		return !(other < *this);
	}

	[[nodiscard]] constexpr bool operator>(const Table& other) const {
		return other < *this;
	}

	[[nodiscard]] constexpr bool operator>=(const Table& other) const {
		return !(*this < other);
	}

	friend constexpr void swap(Table& a, Table& b) noexcept(noexcept(a.swap(b))) { // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap)
		a.swap(b);
	}

private:
	friend iterator;
	friend const_iterator;

	constexpr void reset() noexcept {
		clear();
		meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
			using T = std::tuple_element_t<columnIndex, value_type>;
			using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
			ColumnAllocator columnAllocator = allocator;
			if (columns.template get<columnIndex>()) {
				std::allocator_traits<ColumnAllocator>::deallocate(columnAllocator, columns.template get<columnIndex>(), rowCapacity);
				columns.template get<columnIndex>() = nullptr;
			}
		});
		rowCapacity = 0;
	}

	constexpr void assign(const Table& other) {
		const size_type newRowCount = other.rowCount;
		if constexpr ((std::is_default_constructible_v<Ts> && ...) && (std::is_copy_assignable_v<Ts> && ...)) {
			resize(newRowCount);
			meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
				const auto* const column = other.columns.template get<columnIndex>();
				std::copy(column, column + newRowCount, columns.template get<columnIndex>());
			});
		} else {
			if constexpr ((std::is_copy_assignable_v<Ts> && ...)) {
				if (newRowCount <= rowCount) {
					while (rowCount > newRowCount) {
						pop_back();
					}
					meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
						const auto* const column = other.columns.template get<columnIndex>();
						std::copy(column, column + newRowCount, columns.template get<columnIndex>());
					});
					return;
				}
			}
			clear();
			reserve(newRowCount);
			meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
				using T = std::tuple_element_t<columnIndex, value_type>;
				using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
				ColumnAllocator columnAllocator = allocator;
				size_type rowIndex = 0;
				try {
					while (rowIndex < newRowCount) {
						std::allocator_traits<ColumnAllocator>::construct(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex,
							*(other.columns.template get<columnIndex>() + rowIndex));
						++rowIndex;
					}
				} catch (...) {
					while (rowIndex-- > 0) {
						std::allocator_traits<ColumnAllocator>::destroy(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex);
					}
					meta::forEachIndex<columnIndex>([&](auto otherColumnIndex) -> void {
						using OtherT = std::tuple_element_t<otherColumnIndex, value_type>;
						if constexpr (!std::is_trivially_destructible_v<OtherT>) {
							using OtherColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<OtherT>;
							OtherColumnAllocator otherColumnAllocator = allocator;
							for (size_type rowIndex = newRowCount; rowIndex-- > 0;) {
								std::allocator_traits<OtherColumnAllocator>::destroy(otherColumnAllocator, std::to_address(columns.template get<otherColumnIndex>()) + rowIndex);
							}
						}
					});
					throw;
				}
			});
			rowCount = newRowCount;
		}
	}

	constexpr void assign(Table&& other) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		const size_type newRowCount = other.rowCount;
		if constexpr ((std::is_default_constructible_v<Ts> && ...) && (std::is_move_assignable_v<Ts> && ...)) {
			resize(newRowCount);
			meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
				const auto* const column = other.columns.template get<columnIndex>();
				std::move(column, column + newRowCount, columns.template get<columnIndex>());
			});
		} else {
			if constexpr ((std::is_move_assignable_v<Ts> && ...)) {
				if (newRowCount <= rowCount) {
					while (rowCount > newRowCount) {
						pop_back();
					}
					meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
						const auto* const column = other.columns.template get<columnIndex>();
						std::move(column, column + newRowCount, columns.template get<columnIndex>());
					});
					return;
				}
			}
			clear();
			reserve(newRowCount);
			meta::forEachIndex<sizeof...(Ts)>([&](auto columnIndex) -> void {
				using T = std::tuple_element_t<columnIndex, value_type>;
				using ColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
				ColumnAllocator columnAllocator = allocator;
				size_type rowIndex = 0;
				try {
					while (rowIndex < newRowCount) {
						std::allocator_traits<ColumnAllocator>::construct(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex,
							std::move(*(other.columns.template get<columnIndex>() + rowIndex)));
						++rowIndex;
					}
				} catch (...) {
					while (rowIndex-- > 0) {
						std::allocator_traits<ColumnAllocator>::destroy(columnAllocator, std::to_address(columns.template get<columnIndex>()) + rowIndex);
					}
					meta::forEachIndex<columnIndex>([&](auto otherColumnIndex) -> void {
						using OtherT = std::tuple_element_t<otherColumnIndex, value_type>;
						if constexpr (!std::is_trivially_destructible_v<OtherT>) {
							using OtherColumnAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<OtherT>;
							OtherColumnAllocator otherColumnAllocator = allocator;
							for (size_type rowIndex = newRowCount; rowIndex-- > 0;) {
								std::allocator_traits<OtherColumnAllocator>::destroy(otherColumnAllocator, std::to_address(columns.template get<otherColumnIndex>()) + rowIndex);
							}
						}
					});
					throw;
				}
			});
			rowCount = newRowCount;
		}
	}

	size_type rowCount = 0;
	size_type rowCapacity = 0;
	Tuple<typename std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<Ts>>::pointer...> columns{};
	[[no_unique_address]] Allocator allocator;
};

template <std::size_t Index, typename Row, typename Allocator>
[[nodiscard]] constexpr decltype(auto) get(Table<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<Index>(rowIndex);
}

template <std::size_t Index, typename Row, typename Allocator>
[[nodiscard]] constexpr decltype(auto) get(const Table<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<Index>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr T& get(Table<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<T>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr const T& get(const Table<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<T>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr Span<T> column(Table<Row, Allocator>& t) {
	return t.template column<T>();
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr Span<const T> column(const Table<Row, Allocator>& t) {
	return t.template column<T>();
}

template <std::size_t ColumnIndex, typename Row, typename Allocator>
[[nodiscard]] constexpr auto column(Table<Row, Allocator>& t) {
	return t.template column<ColumnIndex>();
}

template <std::size_t ColumnIndex, typename Row, typename Allocator>
[[nodiscard]] constexpr auto column(const Table<Row, Allocator>& t) {
	return t.template column<ColumnIndex>();
}

} // namespace grem

namespace grem::pmr {

template <typename Row>
using Table = grem::Table<Row, std::pmr::polymorphic_allocator<Row>>;

} // namespace grem::pmr

#endif
