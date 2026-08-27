// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXECUTION_ENTITY_TABLE_HPP
#define GREM_EXECUTION_ENTITY_TABLE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Table.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/execution/component.hpp>
#include <GREM/execution/entity_range.hpp>

#include <iterator>    // std::random_access_iterator_tag
#include <memory>      // std::allocator
#include <stdexcept>   // std::out_of_range
#include <type_traits> // std::true_type
#include <utility>     // std::...index_sequence

namespace grem::execution {

template <typename Row, typename Allocator = std::allocator<Row>>
class EntityTable; // Forward declaration.

template <typename... Components>
class Columns; // Forward declaration.

template <typename... Components>
struct is_entity_range<Columns<Components...>> : std::true_type {};

template <typename... Components>
struct entity_range_components_and_exclusions<Columns<Components...>> {
	using type = meta::TypeList<Components...>;
};

namespace detail {

template <typename Component>
struct columns_extract_components;

template <component Component>
struct columns_extract_components<Component> {
	using MutableComponents = meta::TypeList<Component>;
	using ImmutableComponents = meta::TypeList<>;
	using IncludedComponents = meta::TypeList<Component>;
	using ExcludedComponents = meta::TypeList<>;
};

template <component Component>
struct columns_extract_components<const Component> {
	using MutableComponents = meta::TypeList<>;
	using ImmutableComponents = meta::TypeList<Component>;
	using IncludedComponents = meta::TypeList<const Component>;
	using ExcludedComponents = meta::TypeList<>;
};

struct ColumnsSentinel {
	size_t rowsEnd;
};

template <typename IncludedComponentsList>
class ColumnsIterator;

template <typename... IncludedComponents>
class ColumnsIterator<meta::TypeList<IncludedComponents...>> {
public:
	using difference_type = ptrdiff_t;
	using value_type = Tuple<const size_t, IncludedComponents...>;
	using reference = Tuple<const size_t&, IncludedComponents&...>;
	using iterator_category = std::random_access_iterator_tag;

	struct pointer {
		reference ref;

		[[nodiscard]] constexpr reference* operator->() noexcept {
			return &ref;
		}
	};

	ColumnsIterator() noexcept = default;

	constexpr ColumnsIterator(size_t rowIndex, const Array<void*, sizeof...(IncludedComponents)>& componentArrays) noexcept
		: rowIndex(rowIndex)
		, componentArrays(componentArrays) {}

	[[nodiscard]] constexpr reference operator*() const {
		return [&]<size_t... Indices>(std::index_sequence<Indices...>) -> reference {
			return reference{rowIndex, static_cast<IncludedComponents*>(componentArrays[Indices])[rowIndex]...};
		}(std::make_index_sequence<sizeof...(IncludedComponents)>{});
	}

	[[nodiscard]] constexpr pointer operator->() const {
		return pointer{**this};
	}

	[[nodiscard]] constexpr reference operator[](difference_type n) const {
		return *(*this + n);
	}

	constexpr ColumnsIterator& operator++() {
		++rowIndex;
		return *this;
	}

	constexpr ColumnsIterator& operator--() {
		--rowIndex;
		return *this;
	}

	constexpr ColumnsIterator operator++(int) {
		return ColumnsIterator{rowIndex++, componentArrays};
	}

	constexpr ColumnsIterator operator--(int) {
		return ColumnsIterator{rowIndex--, componentArrays};
	}

	constexpr ColumnsIterator& operator+=(difference_type n) {
		rowIndex += n;
		return *this;
	}

	constexpr ColumnsIterator& operator-=(difference_type n) {
		rowIndex -= n;
		return *this;
	}

	[[nodiscard]] constexpr bool operator==(ColumnsSentinel other) const {
		return rowIndex == other.rowsEnd;
	}

	[[nodiscard]] friend constexpr ColumnsIterator operator+(const ColumnsIterator& a, difference_type b) {
		return ColumnsIterator{a.rowIndex + b, a.componentArrays};
	}

	[[nodiscard]] friend constexpr ColumnsIterator operator+(difference_type a, const ColumnsIterator& b) {
		return ColumnsIterator{a + b.rowIndex, b.componentArrays};
	}

	[[nodiscard]] friend constexpr ColumnsIterator operator-(const ColumnsIterator& a, difference_type b) {
		return ColumnsIterator{a.rowIndex - b, a.componentArrays};
	}

	[[nodiscard]] friend constexpr difference_type operator-(const ColumnsIterator& a, const ColumnsIterator& b) {
		return static_cast<difference_type>(a.rowIndex - b.rowIndex);
	}

	[[nodiscard]] friend constexpr bool operator==(const ColumnsIterator& a, const ColumnsIterator& b) {
		return a.rowIndex == b.rowIndex;
	}

	[[nodiscard]] friend constexpr auto operator<=>(const ColumnsIterator& a, const ColumnsIterator& b) {
		return a.rowIndex <=> b.rowIndex;
	}

private:
	size_t rowIndex;
	Array<void*, sizeof...(IncludedComponents)> componentArrays;
};

} // namespace detail

template <typename... Components>
class Columns {
public:
	using MutableComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::MutableComponents...>;
	using ImmutableComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::ImmutableComponents...>;
	using IncludedComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::IncludedComponents...>;
	using ExcludedComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::ExcludedComponents...>;

	using iterator = detail::ColumnsIterator<IncludedComponents>;
	using sentinel = detail::ColumnsSentinel;

	constexpr Columns() noexcept = default;

	constexpr Columns(const iterator& first, const iterator& last) noexcept
		: rowIndex(first.rowIndex)
		, rowsEnd(last.rowIndex)
		, componentArrays(first.componentArrays) {}

	[[nodiscard]] constexpr iterator begin() const noexcept {
		return iterator{rowIndex, componentArrays};
	}

	[[nodiscard]] constexpr sentinel end() const noexcept {
		return sentinel{rowsEnd};
	}

	[[nodiscard]] constexpr size_t getCandidateCount() const noexcept {
		return rowsEnd - rowIndex;
	}

private:
	template <typename Row, typename Allocator>
	friend class EntityTable;

	constexpr Columns(size_t rowIndex, size_t rowsEnd, const Array<void*, meta::type_list_size_v<IncludedComponents>>& componentArrays) noexcept
		: rowIndex(rowIndex)
		, rowsEnd(rowsEnd)
		, componentArrays(componentArrays) {}

	size_t rowIndex = 0;
	size_t rowsEnd = 0;
	Array<void*, meta::type_list_size_v<IncludedComponents>> componentArrays{};
};

template <typename... Components, typename Allocator>
class EntityTable<Tuple<Components...>, Allocator> : private Table<Tuple<Components...>> {
private:
	using ComponentTable = Table<Tuple<Components...>>;

public:
	static_assert((component<Components> && ...));

	using typename ComponentTable::allocator_type;
	using typename ComponentTable::const_iterator;
	using typename ComponentTable::const_pointer;
	using typename ComponentTable::const_reference;
	using typename ComponentTable::const_reverse_iterator;
	using typename ComponentTable::difference_type;
	using typename ComponentTable::iterator;
	using typename ComponentTable::pointer;
	using typename ComponentTable::reference;
	using typename ComponentTable::reverse_iterator;
	using typename ComponentTable::size_type;
	using typename ComponentTable::value_type;

	using ComponentTable::at;
	using ComponentTable::column;
	using ComponentTable::get;
	using ComponentTable::Table;
	using ComponentTable::operator[];
	using ComponentTable::back;
	using ComponentTable::begin;
	using ComponentTable::capacity;
	using ComponentTable::cbegin;
	using ComponentTable::cend;
	using ComponentTable::clear;
	using ComponentTable::crbegin;
	using ComponentTable::crend;
	using ComponentTable::emplace_back;
	using ComponentTable::empty;
	using ComponentTable::end;
	using ComponentTable::front;
	using ComponentTable::max_size;
	using ComponentTable::pop_back;
	using ComponentTable::push_back;
	using ComponentTable::rbegin;
	using ComponentTable::rend;
	using ComponentTable::reserve;
	using ComponentTable::resize;
	using ComponentTable::shrink_to_fit;
	using ComponentTable::size;
	using ComponentTable::swap;

	template <component T>
	[[nodiscard]] T& getComponent(size_t rowIndex) {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return this->template get<T>(rowIndex);
	}

	template <component T>
	[[nodiscard]] const T& getComponent(size_t rowIndex) const {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return this->template get<T>(rowIndex);
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return const_cast<EntityTable*>(this)->getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>)
	{
		using EntityRange = Columns<Cs...>;
		return const_cast<EntityTable*>(this)->getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{});
	}

private:
	template <typename EntityRange, typename... IncludedComponents>
	[[nodiscard]] EntityRange getEntitiesImplementation(meta::TypeList<IncludedComponents...>) noexcept {
		return EntityRange{0, size(), Array<void*, sizeof...(IncludedComponents)>{this->template column<IncludedComponents>().data()...}};
	}

	template <typename EntityRange, typename... IncludedComponents>
	[[nodiscard]] EntityRange getEntitiesChunkImplementation(size_t chunkIndex, size_t chunkCount, meta::TypeList<IncludedComponents...>) noexcept {
		GREM_ASSERT(chunkIndex < chunkCount);
		const size_t chunkSize = (size() + chunkCount - 1) / chunkCount;
		const size_t chunkBegin = chunkIndex * chunkSize;
		GREM_ASSERT(chunkBegin <= size());
		const size_t chunkEnd = min(chunkBegin + chunkSize, size());
		return EntityRange{chunkBegin, chunkEnd, Array<void*, sizeof...(IncludedComponents)>{this->template column<IncludedComponents>().data()...}};
	}
};

template <std::size_t Index, typename Row, typename Allocator>
[[nodiscard]] constexpr decltype(auto) get(EntityTable<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<Index>(rowIndex);
}

template <std::size_t Index, typename Row, typename Allocator>
[[nodiscard]] constexpr decltype(auto) get(const EntityTable<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<Index>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr T& get(EntityTable<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<T>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr const T& get(const EntityTable<Row, Allocator>& t, std::size_t rowIndex) {
	return t.template get<T>(rowIndex);
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr Span<T> column(EntityTable<Row, Allocator>& t) {
	return t.template column<T>();
}

template <typename T, typename Row, typename Allocator>
[[nodiscard]] constexpr Span<const T> column(const EntityTable<Row, Allocator>& t) {
	return t.template column<T>();
}

template <std::size_t ColumnIndex, typename Row, typename Allocator>
[[nodiscard]] constexpr auto column(EntityTable<Row, Allocator>& t) {
	return t.template column<ColumnIndex>();
}

template <std::size_t ColumnIndex, typename Row, typename Allocator>
[[nodiscard]] constexpr auto column(const EntityTable<Row, Allocator>& t) {
	return t.template column<ColumnIndex>();
}

template <typename Row>
class EntityTableReference;

template <typename... Components>
class EntityTableReference<Tuple<Components...>> {
public:
	static_assert((component<Components> && ...));

	constexpr explicit EntityTableReference(size_t rowCount, Components*... columns)
		: columns(columns...)
		, rowCount(rowCount) {}

	template <typename Allocator>
	constexpr EntityTableReference(EntityTable<Tuple<Components...>, Allocator>& table)
		: columns(column<Components>(table).data()...)
		, rowCount(table.size()) {}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return rowCount == 0;
	}

	[[nodiscard]] constexpr size_t size() const noexcept {
		return rowCount;
	}

	template <component T>
	[[nodiscard]] constexpr T& getComponent(size_t rowIndex) {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return get<T*>(columns)[rowIndex];
	}

	template <component T>
	[[nodiscard]] const T& getComponent(size_t rowIndex) const {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return get<T*>(columns)[rowIndex];
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return const_cast<EntityTableReference*>(this)->getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{});
	}

	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>)
	{
		using EntityRange = Columns<Cs...>;
		return const_cast<EntityTableReference*>(this)->getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{});
	}

private:
	template <typename EntityRange, typename... IncludedComponents>
	[[nodiscard]] EntityRange getEntitiesImplementation(meta::TypeList<IncludedComponents...>) noexcept {
		return EntityRange{0, rowCount, Array<void*, sizeof...(IncludedComponents)>{get<IncludedComponents*>(columns)...}};
	}

	template <typename EntityRange, typename... IncludedComponents>
	[[nodiscard]] EntityRange getEntitiesChunkImplementation(size_t chunkIndex, size_t chunkCount, meta::TypeList<IncludedComponents...>) noexcept {
		GREM_ASSERT(chunkIndex < chunkCount);
		const size_t chunkSize = (size() + chunkCount - 1) / chunkCount;
		const size_t chunkBegin = min(chunkIndex * chunkSize, size());
		const size_t chunkEnd = min(chunkBegin + chunkSize, size());
		return EntityRange{chunkBegin, chunkEnd, Array<void*, sizeof...(IncludedComponents)>{get<IncludedComponents*>(columns)...}};
	}

	Tuple<Components*...> columns;
	size_t rowCount;
};

} // namespace grem::execution

#endif
