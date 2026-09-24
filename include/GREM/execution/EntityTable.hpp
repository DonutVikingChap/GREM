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

/// \cond
template <typename... Components>
struct is_entity_range<Columns<Components...>> : std::true_type {};

template <typename... Components>
struct entity_range_components_and_exclusions<Columns<Components...>> {
	using type = meta::TypeList<Components...>;
};
/// \endcond

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

/**
 * View of the rows in a specific set of columns in an EntityTable.
 *
 * \tparam Components component types of the columns to reference.
 */
template <typename... Components>
class Columns {
public:
	/** List of non-const components required by the range, provided as a meta::TypeList. */
	using MutableComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::MutableComponents...>;

	/** List of const components required by the range, provided as a meta::TypeList. */
	using ImmutableComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::ImmutableComponents...>;

	/** List of components required by the range, provided as a meta::TypeList. */
	using IncludedComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::IncludedComponents...>;

	/** List of components excluded by the range, provided as a meta::TypeList. */
	using ExcludedComponents = meta::type_list_concat_t<typename detail::columns_extract_components<Components>::ExcludedComponents...>;

	using iterator = detail::ColumnsIterator<IncludedComponents>;
	using sentinel = detail::ColumnsSentinel;
	using value_type = typename iterator::value_type;
	using reference = typename iterator::reference;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	/**
	 * Construct an empty entity range.
	 */
	constexpr Columns() noexcept = default;

	/**
	 * Construct an entity subrange from an iterator pair.
	 *
	 * \param first begin iterator of the subrange.
	 * \param last past-the-end iterator of the subrange.
	 *
	 * \warning The subrange `[first, last)` must form a valid range.
	 */
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

	/**
	 * Get the number of rows in the range.
	 *
	 * \return the number of rows spanned by the range.
	 */
	[[nodiscard]] constexpr size_type size() const noexcept {
		return rowsEnd - rowIndex;
	}

	/**
	 * Check if the range is empty.
	 *
	 * \return true if the range spans 0 rows, false otherwise.
	 */
	[[nodiscard]] constexpr bool empty() const noexcept {
		return rowIndex == rowsEnd;
	}

	/**
	 * Get an upper bound estimate on the number of rows in the range, which is
	 * equal to the exact number of rows.
	 *
	 * \return `size()`.
	 *
	 * \note This function is provided for API compatibility with Entities in
	 *       generic code.
	 */
	[[nodiscard]] constexpr size_type getCandidateCount() const noexcept {
		return rowsEnd - rowIndex;
	}

private:
	template <typename Row, typename Allocator>
	friend class EntityTable;

	constexpr Columns(size_type rowIndex, size_type rowsEnd, const Array<void*, meta::type_list_size_v<IncludedComponents>>& componentArrays) noexcept
		: rowIndex(rowIndex)
		, rowsEnd(rowsEnd)
		, componentArrays(componentArrays) {}

	size_type rowIndex = 0;
	size_type rowsEnd = 0;
	Array<void*, meta::type_list_size_v<IncludedComponents>> componentArrays{};
};

/**
 * Column-major Structure-of-Arrays-style table container of rows of a specific
 * tuple of component types, available to tasks defined through a Scheduler.
 *
 * This container can be used as a simpler and more performant alternative to
 * EntityRegistry when the full set of component types is known at compile time
 * and fixed to be the same for all entities, and stable EntityID handles are
 * not needed.
 *
 * \tparam Components component types of the columns in the table.
 *
 * \sa EntityRegistry
 */
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

	/**
	 * Get a specific component of a row in the table.
	 *
	 * \tparam T component type to get.
	 *
	 * \param rowIndex index of the row to get the component from.
	 *
	 * \return a reference to the specified component.
	 *
	 * \throws std::out_of_range if `rowIndex >= size()`.
	 */
	template <component T>
	[[nodiscard]] T& getComponent(size_t rowIndex) {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return this->template get<T>(rowIndex);
	}

	/**
	 * Get a specific component of a row in the table.
	 *
	 * \tparam T component type to get.
	 *
	 * \param rowIndex index of the row to get the component from.
	 *
	 * \return a read-only reference to the specified component.
	 *
	 * \throws std::out_of_range if `rowIndex >= size()`.
	 */
	template <component T>
	[[nodiscard]] const T& getComponent(size_t rowIndex) const {
		if (rowIndex >= size()) {
			throw std::out_of_range{"Component not found for entity."};
		}
		return this->template get<T>(rowIndex);
	}

	/**
	 * Get a view of all rows of a specific set of columns in the table.
	 *
	 * \tparam Cs component types to get the columns of.
	 *
	 * \return the specified entity range, whose reference type can be
	 *         destructured into a structured binding containing a read-only
	 *         reference to the entity handle followed by potentially read-only
	 *         references to all of the specified components.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         for (auto&& [entityID, a, b] : registry.getEntities<A, const B>()) {
	 *             // ...
	 *         }
	 *         ```
	 */
	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	/**
	 * Get a view of all rows of a specific set of columns in the table.
	 *
	 * \tparam Cs component types to get the columns of.
	 *
	 * \return the specified entity range, whose reference type can be
	 *         destructured into a structured binding containing a read-only
	 *         reference to the entity handle followed by read-only references
	 *         to all of the specified components.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         for (auto&& [entityID, a, b] : registry.getEntities<const A, const B>()) {
	 *             // ...
	 *         }
	 *         ```
	 */
	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntities() const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return const_cast<EntityTable*>(this)->getEntitiesImplementation<EntityRange>(typename EntityRange::IncludedComponents{});
	}

	/**
	 * Implicitly convert the table to an entity range.
	 *
	 * \return the entity range.
	 *
	 * \sa getEntities()
	 */
	template <typename... Cs>
	[[nodiscard]] operator Columns<Cs...>() noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		return getEntities<Cs...>();
	}

	/**
	 * Implicitly convert the table to an entity range.
	 *
	 * \return the entity range.
	 *
	 * \sa getEntities()
	 */
	template <typename... Cs>
	[[nodiscard]] operator Columns<Cs...>() const noexcept requires(meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		return getEntities<Cs...>();
	}

	/**
	 * Get a specific chunk of the rows of a specific set of columns in the
	 * table.
	 *
	 * \tparam Cs component types to get the columns of.
	 *
	 * \param chunkIndex index of the specific chunk to get. Must be less than
	 *        `chunkCount`.
	 * \param chunkCount number of chunks that the entity range is divided into.
	 *        Must be positive, and greater than chunkIndex.
	 *
	 * \return the specified entity range chunk, whose reference type can be
	 *         destructured into a structured binding containing a read-only
	 *         reference to the entity handle followed by potentially read-only
	 *         references to all of the specified components.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         myParallelFor(chunkCount, [&](size_t chunkIndex) {
	 *             for (auto&& [entityID, a, b] : registry.getEntitiesChunk<A, const B, Exclude<C>>(chunkIndex, chunkCount)) {
	 *                 // ...
	 *             }
	 *         });
	 *         ```
	 */
	template <typename... Cs>
	[[nodiscard]] Columns<Cs...> getEntitiesChunk(size_t chunkIndex, size_t chunkCount) noexcept requires(!meta::type_list_empty_v<typename Columns<Cs...>::MutableComponents>) {
		using EntityRange = Columns<Cs...>;
		return getEntitiesChunkImplementation<EntityRange>(chunkIndex, chunkCount, typename EntityRange::IncludedComponents{});
	}

	/**
	 * Get a specific chunk of the rows of a specific set of columns in the
	 * table.
	 *
	 * \tparam Cs component types to get the columns of.
	 *
	 * \param chunkIndex index of the specific chunk to get. Must be less than
	 *        `chunkCount`.
	 * \param chunkCount number of chunks that the entity range is divided into.
	 *        Must be positive, and greater than chunkIndex.
	 *
	 * \return the specified entity range chunk, whose reference type can be
	 *         destructured into a structured binding containing a read-only
	 *         reference to the entity handle followed by read-only references
	 *         to all of the specified components.
	 *
	 * \remark Example usage:
	 *         ```cpp
	 *         myParallelFor(chunkCount, [&](size_t chunkIndex) {
	 *             for (auto&& [entityID, a, b] : registry.getEntitiesChunk<const A, const B, Exclude<C>>(chunkIndex, chunkCount)) {
	 *                 // ...
	 *             }
	 *         });
	 *         ```
	 */
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

} // namespace grem::execution

#endif
