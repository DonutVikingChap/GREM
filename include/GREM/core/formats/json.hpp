// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_JSON_HPP
#define GREM_CORE_FORMATS_JSON_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/unicode.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/time.hpp>

#include <charconv>         // std::to_chars_result, std::from_chars_result std::to_chars, std::from_chars
#include <cmath>            // std::isnan, std::isinf, std::signbit
#include <compare>          // std::partial_ordering, std::compare_partial_order_fallback
#include <cstddef>          // std::nullptr_t
#include <cstdlib>          // std::strtoull, std::strtod
#include <initializer_list> // std::initializer_list
#include <ios>              // std::ios_base
#include <istream>          // std::istream
#include <iterator>         // std::begin, std::end, std::...streambuf_iterator
#include <memory>           // std::allocator, std::allocator_traits
#include <memory_resource>  // std::pmr::polymorphic_allocator
#include <new>              // std::launder
#include <ostream>          // std::ostream
#include <sstream>          // std::istringstream, std::basic_istringstream, std::ostringstream, std::basic_ostringstream
#include <stdexcept>        // std::out_of_range
#include <streambuf>        // std::streambuf
#include <string>           // std::...string, std::char_traits
#include <system_error>     // std::errc
#include <tuple>            // std::forward_as_tuple
#include <type_traits>      // std::remove_..._t
#include <utility>          // std::move, std::forward, std::piecewise_construct, std::in_place_type

namespace grem::json {

namespace detail {

struct NoSerializerOverride {};
struct NoDeserializerOverride {};

} // namespace detail

template <template <typename> typename Allocator>
class ObjectBase; // Forward declaration.

template <template <typename> typename Allocator, typename Predicate>
typename ObjectBase<Allocator>::size_type erase_if(ObjectBase<Allocator>& container, Predicate predicate); // Forward declaration.

template <template <typename> typename Allocator>
class ArrayBase; // Forward declaration.

template <template <typename> typename Allocator, typename U>
typename ArrayBase<Allocator>::size_type erase(ArrayBase<Allocator>& container, const U& value); // Forward declaration.

template <template <typename> typename Allocator, typename Predicate>
typename ArrayBase<Allocator>::size_type erase_if(ArrayBase<Allocator>& container, Predicate predicate); // Forward declaration.

/**
 * Line and column numbers of a location in a JSON source string.
 */
struct SourceLocation {
	/**
	 * Line number, starting at 1 for the first line. A value of 0 means no
	 * particular line.
	 */
	size_t lineNumber = 1;

	/**
	 * Column number, starting at 1 for the first column. A value of 0 means no
	 * particular column.
	 */
	size_t columnNumber = 1;

	/**
	 * Compare this source location to another for equality.
	 *
	 * \param other the source location to compare this one to.
	 *
	 * \return true if the source locations are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const SourceLocation& other) const = default;
};

/**
 * Exception type for errors originating from the JSON API.
 */
struct Error : grem::Error {
	/**
	 * Line number, starting at 1 for the first line. A value of 0 means no
	 * particular line.
	 */
	size_t lineNumber;

	/**
	 * Column number, starting at 1 for the first column. A value of 0 means no
	 * particular column.
	 */
	size_t columnNumber;

	Error(const auto& message, const SourceLocation& source)
		: grem::Error(message)
		, lineNumber(source.lineNumber)
		, columnNumber(source.columnNumber) {}

	void writeMessage(String& output) const override {
		if (lineNumber != 0) {
			output.append(toString(lineNumber));
			output.push_back(':');
			if (columnNumber != 0) {
				output.append(toString(columnNumber));
				output.push_back(':');
			}
			output.push_back(' ');
		}
		output.append(what());
	}

	[[nodiscard]] bool messageAttachesToPrecedingFilepath() const noexcept override {
		return lineNumber != 0;
	}

	[[nodiscard]] SourceLocation getSource() const noexcept {
		return {.lineNumber = lineNumber, .columnNumber = columnNumber};
	}
};

/**
 * Options for JSON serialization.
 */
struct SerializationOptions {
	/**
	 * The starting indentation level, expressed as the number of indentation
	 * characters.
	 */
	size_t indentation = 0;

	/**
	 * The number of indentation characters that each new level of indentation
	 * will add.
	 */
	size_t relativeIndentation = 4;

	/**
	 * The character to use when performing indentation.
	 */
	char indentationCharacter = ' ';

	/**
	 * Format the output in a way that is nicely human-readable.
	 *
	 * Disable to use a more compact layout without whitespace or indentation.
	 *
	 * \sa prettyPrintMaxSingleLineObjectPropertyCount
	 * \sa prettyPrintMaxSingleLineArrayItemCount
	 */
	bool prettyPrint = true;

	/**
	 * Maximum size of an object before it is split into multiple lines when
	 * pretty printing.
	 *
	 * When set to a positive value, objects at or below this size will be
	 * written in a single line. Set to 0 to always split non-empty objects into
	 * multiple lines regardless of size.
	 *
	 * \note This option only applies when #prettyPrint is true.
	 *
	 * \sa prettyPrint
	 */
	size_t prettyPrintMaxSingleLineObjectPropertyCount = 4;

	/**
	 * Maximum size of an array before it is split into multiple lines when
	 * pretty printing.
	 *
	 * When set to a positive value, arrays at or below this size will be
	 * written in a single line. Set to 0 to always split non-empty arrays into
	 * multiple lines regardless of size.
	 *
	 * \note This option only applies when #prettyPrint is true.
	 *
	 * \sa prettyPrint
	 */
	size_t prettyPrintMaxSingleLineArrayItemCount = 4;

	/**
	 * Null-terminated ASCII string representing the newline sequence to use
	 * when performing line breaks.
	 *
	 * Defaults to CRLF (carriage return followed by line feed).
	 */
	CStringView newlineString = "\r\n";
};

/**
 * Options for JSON deserialization.
 */
struct DeserializationOptions {};

// Forward declaration.
template <template <typename> typename Allocator>
class VariantBase;

// Forward declaration.
template <template <typename> typename Allocator>
class ValueBase;

/**
 * JSON null type.
 */
using Null = Monostate;

/**
 * JSON boolean type.
 */
using Boolean = bool;

/**
 * JSON string type.
 *
 * \tparam Allocator allocator type template to use.
 */
template <template <typename> typename Allocator>
using StringBase = grem::StringBase<char, std::char_traits<char>, Allocator<char>>;

/**
 * JSON string type using the default allocator.
 */
using String = StringBase<std::allocator>;

/**
 * JSON number type.
 */
using Number = double;

/**
 * JSON object type whose API mimics that of std::map<String, Value>.
 *
 * \tparam Allocator allocator type template to use.
 */
template <template <typename> typename Allocator>
class ObjectBase {
public:
	using key_type = StringBase<Allocator>;
	using mapped_type = ValueBase<Allocator>;
	using value_type = Pair<StringBase<Allocator>, ValueBase<Allocator>>;
	using size_type = typename ArrayList<value_type>::size_type;
	using difference_type = typename ArrayList<value_type>::difference_type;
	using allocator_type = Allocator<value_type>;
	using reference = typename ArrayList<value_type>::reference;
	using const_reference = typename ArrayList<value_type>::const_reference;
	using pointer = typename ArrayList<value_type>::pointer;
	using const_pointer = typename ArrayList<value_type>::const_pointer;
	using iterator = typename ArrayList<value_type>::iterator;
	using const_iterator = typename ArrayList<value_type>::const_iterator;
	using reverse_iterator = typename ArrayList<value_type>::reverse_iterator;
	using const_reverse_iterator = typename ArrayList<value_type>::const_reverse_iterator;

	ObjectBase() noexcept;
	explicit ObjectBase(const allocator_type& allocator) noexcept;
	~ObjectBase();

	template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
	ObjectBase(InputIterator first, Sentinel last, const allocator_type& allocator = allocator_type());
	ObjectBase(std::initializer_list<value_type> ilist, const allocator_type& allocator = allocator_type());

	ObjectBase(const ObjectBase& other, const allocator_type& allocator);
	ObjectBase(const ObjectBase& other);
	ObjectBase(ObjectBase&& other, const allocator_type& allocator) noexcept; // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	ObjectBase(ObjectBase&& other) noexcept;

	ObjectBase& operator=(const ObjectBase& other);
	ObjectBase& operator=(ObjectBase&& other) noexcept(
		std::allocator_traits<
			allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<allocator_type>::is_always_equal::value);
	ObjectBase& operator=(std::initializer_list<value_type> ilist);

	[[nodiscard]] allocator_type get_allocator() const noexcept;

	[[nodiscard]] mapped_type& at(StringView key);
	[[nodiscard]] const mapped_type& at(StringView key) const;
	[[nodiscard]] mapped_type& operator[](const key_type& k);
	[[nodiscard]] mapped_type& operator[](key_type&& k);

	[[nodiscard]] iterator begin() noexcept;
	[[nodiscard]] const_iterator begin() const noexcept;
	[[nodiscard]] const_iterator cbegin() const noexcept;
	[[nodiscard]] iterator end() noexcept;
	[[nodiscard]] const_iterator end() const noexcept;
	[[nodiscard]] const_iterator cend() const noexcept;
	[[nodiscard]] reverse_iterator rbegin() noexcept;
	[[nodiscard]] const_reverse_iterator rbegin() const noexcept;
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept;
	[[nodiscard]] reverse_iterator rend() noexcept;
	[[nodiscard]] const_reverse_iterator rend() const noexcept;
	[[nodiscard]] const_reverse_iterator crend() const noexcept;

	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] size_type size() const noexcept;
	[[nodiscard]] size_type max_size() const noexcept;
	[[nodiscard]] size_type capacity() const noexcept;

	void clear() noexcept;
	void reserve(size_type newCapacity);

	template <typename P>
	Pair<iterator, bool> insert(P&& value);

	template <typename P>
	iterator insert(const_iterator pos, P&& value) requires(!convertible_to<P, const_iterator>);

	template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
	void insert(InputIterator first, Sentinel last);
	void insert(std::initializer_list<value_type> ilist);
	template <typename R>
	void insert_range(R&& r);

	template <typename... Args>
	Pair<iterator, bool> emplace(Args&&... args);

	template <typename... Args>
	iterator emplace_hint(const_iterator hint, Args&&... args);

	template <typename... Args>
	Pair<iterator, bool> try_emplace(const key_type& k, Args&&... args);

	template <typename... Args>
	Pair<iterator, bool> try_emplace(key_type&& k, Args&&... args);

	template <typename... Args>
	iterator try_emplace(const_iterator, const key_type& k, Args&&... args);

	template <typename... Args>
	iterator try_emplace(const_iterator, key_type&& k, Args&&... args);

	iterator erase(const_iterator pos);
	size_type erase(StringView key);

	void swap(ObjectBase& other) noexcept;

	[[nodiscard]] size_type count(StringView key) const noexcept;
	[[nodiscard]] bool contains(StringView key) const noexcept;
	[[nodiscard]] iterator find(StringView key) noexcept;
	[[nodiscard]] const_iterator find(StringView key) const noexcept;
	[[nodiscard]] Pair<iterator, iterator> equal_range(StringView key) noexcept;
	[[nodiscard]] Pair<const_iterator, const_iterator> equal_range(StringView key) const noexcept;
	[[nodiscard]] iterator lower_bound(StringView key) noexcept;
	[[nodiscard]] const_iterator lower_bound(StringView key) const noexcept;
	[[nodiscard]] iterator upper_bound(StringView key) noexcept;
	[[nodiscard]] const_iterator upper_bound(StringView key) const noexcept;

	[[nodiscard]] bool operator==(const ObjectBase& other) const noexcept;
	[[nodiscard]] std::partial_ordering operator<=>(const ObjectBase& other) const noexcept;

	template <typename Predicate>
	friend size_type erase_if(ObjectBase& container, Predicate predicate);

private:
	struct Compare {
		[[nodiscard]] bool operator()(const value_type& a, const value_type& b) const noexcept;
		[[nodiscard]] bool operator()(const value_type& a, StringView b) const noexcept;
		[[nodiscard]] bool operator()(StringView a, const value_type& b) const noexcept;
		[[nodiscard]] bool operator()(StringView a, StringView b) const noexcept;
	};

	ArrayList<value_type, allocator_type> membersSortedByName;
};

/**
 * JSON object type using the default allocator.
 */
using Object = json::ObjectBase<std::allocator>;

/**
 * JSON array type whose API mimics that of std::vector<Value>.
 *
 * \tparam Allocator allocator type template to use.
 */
template <template <typename> typename Allocator>
class ArrayBase {
public:
	using value_type = ValueBase<Allocator>;
	using size_type = typename ArrayList<value_type>::size_type;
	using difference_type = typename ArrayList<value_type>::difference_type;
	using allocator_type = Allocator<value_type>;
	using reference = typename ArrayList<value_type>::reference;
	using const_reference = typename ArrayList<value_type>::const_reference;
	using pointer = typename ArrayList<value_type>::pointer;
	using const_pointer = typename ArrayList<value_type>::const_pointer;
	using iterator = typename ArrayList<value_type>::iterator;
	using const_iterator = typename ArrayList<value_type>::const_iterator;
	using reverse_iterator = typename ArrayList<value_type>::reverse_iterator;
	using const_reverse_iterator = typename ArrayList<value_type>::const_reverse_iterator;

	ArrayBase() noexcept;
	explicit ArrayBase(const allocator_type& allocator) noexcept;
	~ArrayBase();

	template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
	ArrayBase(InputIterator first, Sentinel last, const allocator_type& allocator = allocator_type());
	ArrayBase(size_type count, const value_type& value, const allocator_type& allocator = allocator_type());
	ArrayBase(std::initializer_list<value_type> ilist, const allocator_type& allocator = allocator_type());

	ArrayBase(const ArrayBase& other, const allocator_type& allocator);
	ArrayBase(const ArrayBase& other);
	ArrayBase(ArrayBase&& other, const allocator_type& allocator) noexcept; // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	ArrayBase(ArrayBase&& other) noexcept;

	ArrayBase& operator=(const ArrayBase& other);
	ArrayBase& operator=(ArrayBase&& other) noexcept(
		std::allocator_traits<
			allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<allocator_type>::is_always_equal::value);
	ArrayBase& operator=(std::initializer_list<value_type> ilist);

	void swap(ArrayBase& other) noexcept;

	[[nodiscard]] allocator_type get_allocator() const noexcept;

	[[nodiscard]] pointer data() noexcept;
	[[nodiscard]] const_pointer data() const noexcept;
	[[nodiscard]] size_type size() const noexcept;
	[[nodiscard]] size_type max_size() const noexcept;
	[[nodiscard]] size_type capacity() const noexcept;
	[[nodiscard]] bool empty() const noexcept;

	[[nodiscard]] iterator begin() noexcept;
	[[nodiscard]] const_iterator begin() const noexcept;
	[[nodiscard]] const_iterator cbegin() const noexcept;
	[[nodiscard]] iterator end() noexcept;
	[[nodiscard]] const_iterator end() const noexcept;
	[[nodiscard]] const_iterator cend() const noexcept;
	[[nodiscard]] reverse_iterator rbegin() noexcept;
	[[nodiscard]] const_reverse_iterator rbegin() const noexcept;
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept;
	[[nodiscard]] reverse_iterator rend() noexcept;
	[[nodiscard]] const_reverse_iterator rend() const noexcept;
	[[nodiscard]] const_reverse_iterator crend() const noexcept;

	[[nodiscard]] reference front();
	[[nodiscard]] const_reference front() const;
	[[nodiscard]] reference back();
	[[nodiscard]] const_reference back() const;
	[[nodiscard]] reference at(size_type pos);
	[[nodiscard]] const_reference at(size_type pos) const;
	[[nodiscard]] reference operator[](size_type pos);
	[[nodiscard]] const_reference operator[](size_type pos) const;

	[[nodiscard]] bool operator==(const ArrayBase& other) const;
	[[nodiscard]] std::partial_ordering operator<=>(const ArrayBase& other) const noexcept;

	template <typename U>
	friend size_type erase(ArrayBase& container, const U& value);

	template <typename Predicate>
	friend size_type erase_if(ArrayBase& container, Predicate predicate);

	void clear() noexcept;
	void reserve(size_type newCapacity);
	void shrink_to_fit();

	void assign(size_type count, const value_type& value);
	template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
	void assign(InputIterator first, Sentinel last);
	void assign(std::initializer_list<value_type> ilist);
	template <typename R>
	void assign_range(R&& r);

	iterator insert(const_iterator pos, const value_type& value);
	iterator insert(const_iterator pos, value_type&& value);
	iterator insert(const_iterator pos, size_type count, const value_type& value);

	template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
	iterator insert(const_iterator pos, InputIterator first, Sentinel last);
	iterator insert(const_iterator pos, std::initializer_list<value_type> ilist);
	template <typename R>
	iterator insert_range(const_iterator pos, R&& r);
	template <typename R>
	void append_range(R&& r);

	template <typename... Args>
	iterator emplace(const_iterator pos, Args&&... args);

	iterator erase(const_iterator pos);
	iterator erase(const_iterator first, const_iterator last);

	void push_back(const value_type& value);
	void push_back(value_type&& value);

	template <typename... Args>
	reference emplace_back(Args&&... args);

	void pop_back();

	void resize(size_type count);
	void resize(size_type count, const value_type& value);

private:
	ArrayList<value_type, allocator_type> values;
};

/**
 * JSON array type using the default allocator.
 */
using Array = json::ArrayBase<std::allocator>;

/**
 * JSON value type.
 *
 * Holds a value of one of the following types:
 * - Null
 * - Boolean
 * - String
 * - Number
 * - Object
 * - Array
 *
 * \tparam Allocator allocator type template to use.
 */
template <template <typename> typename Allocator>
class VariantBase : public grem::Variant<Null, Boolean, StringBase<Allocator>, Number, ObjectBase<Allocator>, ArrayBase<Allocator>> {
private:
	using Base = grem::Variant<Null, Boolean, StringBase<Allocator>, Number, ObjectBase<Allocator>, ArrayBase<Allocator>>;

public:
	/** Allocator type used by the value. */
	using allocator_type = Allocator<VariantBase>;

	/**
	 * Parse a value of any JSON type from a UTF-8 JSON string.
	 *
	 * The parser supports JSON5 features such as comments, unquoted identifiers
	 * and trailing commas.
	 *
	 * \param jsonString read-only view over the JSON string to parse a value
	 *        from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on failure to parse a JSON value.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] static ValueBase<Allocator> parse(UTF8StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type());

	/**
	 * Parse a value of any JSON type from a JSON string interpreted as UTF-8.
	 *
	 * The parser supports JSON5 features such as comments, unquoted identifiers
	 * and trailing commas.
	 *
	 * \param jsonString read-only view over the JSON string to parse a value
	 *        from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on failure to parse a JSON value.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] static ValueBase<Allocator> parse(StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type());

	/**
	 * Construct a Null value.
	 */
	VariantBase() noexcept
		: VariantBase(allocator_type()) {}

	/**
	 * Construct a Null value.
	 *
	 * \param allocator allocator to use.
	 */
	explicit VariantBase(const allocator_type& allocator) noexcept
		: allocator(allocator) {}

	/**
	 * Construct a Null value.
	 *
	 * \param allocator allocator to use.
	 */
	VariantBase(Null, const allocator_type& allocator) noexcept
		: VariantBase(allocator) {}

	/**
	 * Construct a Null value.
	 */
	VariantBase(Null) noexcept
		: VariantBase(Null{}, allocator_type()) {}

	/**
	 * Construct a Null value.
	 *
	 * \param allocator allocator to use.
	 */
	VariantBase(std::nullptr_t, const allocator_type& allocator) noexcept
		: VariantBase(allocator) {}

	/**
	 * Construct a Null value.
	 */
	VariantBase(std::nullptr_t) noexcept
		: VariantBase(std::nullptr_t{}, allocator_type()) {}

	/**
	 * Construct a Boolean value with the given underlying value.
	 *
	 * \param value value to copy.
	 * \param allocator allocator to use.
	 */
	VariantBase(Boolean value, const allocator_type& allocator) noexcept
		: Base(std::in_place_type<Boolean>, value)
		, allocator(allocator) {}

	/**
	 * Construct a Boolean value with the given underlying value.
	 *
	 * \param value value to copy.
	 */
	VariantBase(Boolean value) noexcept
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value value to copy.
	 * \param allocator allocator to use.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const StringBase<Allocator>& value, const allocator_type& allocator)
		: Base(std::in_place_type<StringBase<Allocator>>, value, allocator)
		, allocator(this->template as<StringBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value value to copy.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const StringBase<Allocator>& value)
		: Base(std::in_place_type<StringBase<Allocator>>, value)
		, allocator(this->template as<StringBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct a String value from the given underlying value.
	 *
	 * \param value value to take.
	 * \param allocator allocator to use.
	 */
	VariantBase(StringBase<Allocator>&& value, const allocator_type& allocator) noexcept
		: Base(std::in_place_type<StringBase<Allocator>>, std::move(value), allocator)
		, allocator(this->template as<StringBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct a String value from the given underlying value.
	 *
	 * \param value value to take.
	 */
	VariantBase(StringBase<Allocator>&& value) noexcept
		: Base(std::in_place_type<StringBase<Allocator>>, std::move(value))
		, allocator(this->template as<StringBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only pointer to the null-terminated string to copy the
	 *        contents of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const char* value, const allocator_type& allocator)
		: VariantBase(StringBase<Allocator>{value, allocator}, allocator) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only pointer to the null-terminated string to copy the
	 *        contents of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const char* value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the string to copy the contents of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(StringView value, const allocator_type& allocator)
		: VariantBase(StringBase<Allocator>{value, allocator}, allocator) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the string to copy the contents of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(StringView value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the string to copy the contents of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(CStringView value, const allocator_type& allocator)
		: VariantBase(value.c_str(), allocator) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the string to copy the contents of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(CStringView value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only pointer to the null-terminated UTF-8 string to
	 *        copy the contents of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const char8_t* value, const allocator_type& allocator)
		: VariantBase(std::launder(reinterpret_cast<const char*>(value)), allocator) {
		static_assert(sizeof(char) == sizeof(char8_t));
		static_assert(alignof(char) == alignof(char8_t));
	}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only pointer to the null-terminated UTF-8 string to
	 *        copy the contents of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const char8_t* value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the UTF-8 string to copy the contents
	 *        of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(UTF8StringView value, const allocator_type& allocator)
		: VariantBase(StringBase<Allocator>{value.begin(), value.end(), allocator}, allocator) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the UTF-8 string to copy the contents
	 *        of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(UTF8StringView value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the UTF-8 string to copy the contents
	 *        of.
	 * \param allocator allocator to use.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(UTF8CStringView value, const allocator_type& allocator)
		: VariantBase(value.c_str(), allocator) {}

	/**
	 * Construct a String value with the given underlying value.
	 *
	 * \param value read-only view over the UTF-8 string to copy the contents
	 *        of.
	 *
	 * \throws std::length_error if the maximum string length was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(UTF8CStringView value)
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct a Number value with the given underlying value.
	 *
	 * \param value value to copy. May be of any fundamental arithmetic type
	 *        except for bool and character types.
	 * \param allocator allocator to use.
	 */
	VariantBase(strict_arithmetic auto value, const allocator_type& allocator) noexcept
		: Base(std::in_place_type<Number>, static_cast<Number>(value))
		, allocator(allocator) {}

	/**
	 * Construct a Number value with the given underlying value.
	 *
	 * \param value value to copy. May be of any fundamental arithmetic type
	 *        except for bool and character types.
	 */
	VariantBase(strict_arithmetic auto value) noexcept
		: VariantBase(value, allocator_type()) {}

	/**
	 * Construct an Object value with the given underlying value.
	 *
	 * \param value value to copy.
	 * \param allocator allocator to use.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const ObjectBase<Allocator>& value, const allocator_type& allocator)
		: Base(std::in_place_type<ObjectBase<Allocator>>, value, allocator)
		, allocator(this->template as<ObjectBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Object value with the given underlying value.
	 *
	 * \param value value to copy.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const ObjectBase<Allocator>& value)
		: Base(std::in_place_type<ObjectBase<Allocator>>, value)
		, allocator(this->template as<ObjectBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Object value from the given underlying value.
	 *
	 * \param value value to take.
	 * \param allocator allocator to use.
	 */
	VariantBase(ObjectBase<Allocator>&& value, const allocator_type& allocator) noexcept
		: Base(std::in_place_type<ObjectBase<Allocator>>, std::move(value), allocator)
		, allocator(this->template as<ObjectBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Object value from the given underlying value.
	 *
	 * \param value value to take.
	 */
	VariantBase(ObjectBase<Allocator>&& value) noexcept
		: Base(std::in_place_type<ObjectBase<Allocator>>, std::move(value))
		, allocator(this->template as<ObjectBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Array value with the given underlying value.
	 *
	 * \param value value to copy.
	 * \param allocator allocator to use.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const ArrayBase<Allocator>& value, const allocator_type& allocator)
		: Base(std::in_place_type<ArrayBase<Allocator>>, value, allocator)
		, allocator(this->template as<ArrayBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Array value with the given underlying value.
	 *
	 * \param value value to copy.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	VariantBase(const ArrayBase<Allocator>& value)
		: Base(std::in_place_type<ArrayBase<Allocator>>, value)
		, allocator(this->template as<ArrayBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Array value from the given underlying value.
	 *
	 * \param value value to take.
	 * \param allocator allocator to use.
	 */
	VariantBase(ArrayBase<Allocator>&& value, const allocator_type& allocator) noexcept
		: Base(std::in_place_type<ArrayBase<Allocator>>, std::move(value), allocator)
		, allocator(this->template as<ArrayBase<Allocator>>().get_allocator()) {}

	/**
	 * Construct an Array value from the given underlying value.
	 *
	 * \param value value to take.
	 */
	VariantBase(ArrayBase<Allocator>&& value) noexcept
		: Base(std::in_place_type<ArrayBase<Allocator>>, std::move(value))
		, allocator(this->template as<ArrayBase<Allocator>>().get_allocator()) {}

	/** Destructor. */
	~VariantBase() = default;

	/** Copy constructor. */
	VariantBase(const VariantBase& other, const allocator_type& allocator) noexcept
		: Base(static_cast<const Base&>(other))
		, allocator((this->valueless_by_exception())
						? allocator
						: match(static_cast<Base&>(*this))(                                                          //
							  [&](Null&) -> allocator_type { return allocator; },                                    //
							  [&](Boolean&) -> allocator_type { return allocator; },                                 //
							  [&](StringBase<Allocator>& value) -> allocator_type { return value.get_allocator(); }, //
							  [&](Number&) -> allocator_type { return allocator; },                                  //
							  [&](ObjectBase<Allocator>& value) -> allocator_type { return value.get_allocator(); }, //
							  [&](ArrayBase<Allocator>& value) -> allocator_type { return value.get_allocator(); })) {}

	/** Copy constructor. */
	VariantBase(const VariantBase& other)
		: VariantBase(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

	/** Move constructor. */
	VariantBase(VariantBase&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: Base(std::move(static_cast<Base&>(other)))
		, allocator((this->valueless_by_exception())
						? allocator
						: match(static_cast<Base&>(*this))(                                                          //
							  [&](Null&) -> allocator_type { return allocator; },                                    //
							  [&](Boolean&) -> allocator_type { return allocator; },                                 //
							  [&](StringBase<Allocator>& value) -> allocator_type { return value.get_allocator(); }, //
							  [&](Number&) -> allocator_type { return allocator; },                                  //
							  [&](ObjectBase<Allocator>& value) -> allocator_type { return value.get_allocator(); }, //
							  [&](ArrayBase<Allocator>& value) -> allocator_type { return value.get_allocator(); })) {}

	/** Move constructor. */
	VariantBase(VariantBase&& other) noexcept
		: VariantBase(std::move(other), other.get_allocator()) {}

	/** Copy assignment. */
	VariantBase& operator=(const VariantBase& other) {
		if (this == &other) {
			return *this;
		}
		if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
			if constexpr (!std::allocator_traits<allocator_type>::is_always_equal::value) {
				if (allocator != other.get_allocator()) {
					this->template emplace<Null>();
				}
			}
			allocator = other.get_allocator();
		}
		static_cast<Base&>(*this) = static_cast<const Base&>(other);
		if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
			match(static_cast<Base&>(*this))([&](auto& value) -> void {
				if constexpr (requires { value.get_allocator(); }) {
					allocator = value.get_allocator();
				}
			});
		}
		return *this;
	}

	/** Move assignment. */
	VariantBase& operator=(VariantBase&& other) noexcept(
		std::allocator_traits<
			allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<allocator_type>::is_always_equal::value) {
		if (this == &other) {
			return *this;
		}
		this->template emplace<Null>();
		if constexpr (!std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value && !std::allocator_traits<allocator_type>::is_always_equal::value) {
			if (allocator != other.allocator) {
				match(other)(                                                                                                             //
					[&](const Null&) -> void {},                                                                                          //
					[&](const Boolean& value) -> void { this->template emplace<Boolean>(value); },                                        //
					[&](const StringBase<Allocator>& value) -> void { this->template emplace<StringBase<Allocator>>(value, allocator); }, //
					[&](const Number& value) -> void { this->template emplace<Number>(value); },                                          //
					[&](const ObjectBase<Allocator>& value) -> void { this->template emplace<ObjectBase<Allocator>>(value, allocator); }, //
					[&](const ArrayBase<Allocator>& value) -> void { this->template emplace<ArrayBase<Allocator>>(value, allocator); });
				other.template emplace<Null>();
				return *this;
			}
		}
		static_cast<Base&>(*this) = std::move(static_cast<Base&>(other));
		if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value) {
			match(static_cast<Base&>(*this))([&](auto& value) -> void {
				if constexpr (requires { value.get_allocator(); }) {
					allocator = value.get_allocator();
				} else {
					allocator = other.allocator;
				}
			});
		}
		return *this;
	}

	/**
	 * Get the allocator used by the value.
	 *
	 * \return a copy of the value's allocator.
	 */
	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	/**
	 * Get a JSON string representation of the value.
	 *
	 * \param options options for JSON serialization, see SerializationOptions.
	 * \param stringAllocator allocator to use.
	 *
	 * \return a JSON string containing a representation of the value as it
	 *         would be if it had been serialized to an output stream with the
	 *         given options.
	 *
	 * \throws json::Error on failure to serialize the value.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] StringBase<Allocator> toString(const SerializationOptions& options = {}, const allocator_type& stringAllocator = allocator_type()) const;

	/**
	 * Compare this value to another for equality.
	 *
	 * \param other the value to compare this value to.
	 *
	 * \return true if the values are equal, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool operator==(const VariantBase& other) const {
		return static_cast<const Base&>(*this) == static_cast<const Base&>(other);
	}

	/**
	 * Compare this value to another.
	 *
	 * \param other the value to compare this value to.
	 *
	 * \return a partial ordering between the two values.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE std::partial_ordering operator<=>(const VariantBase& other) const {
		return std::compare_partial_order_fallback(static_cast<const Base&>(*this), static_cast<const Base&>(other));
	}

private:
	[[no_unique_address]] allocator_type allocator;
};

/**
 * JSON value type using the default allocator.
 */
using Variant = json::VariantBase<std::allocator>;

/**
 * JSON value with an associated SourceLocation.
 *
 * \tparam Allocator allocator type template to use.
 */
template <template <typename> typename Allocator>
class ValueBase : public VariantBase<Allocator> {
public:
	using typename VariantBase<Allocator>::allocator_type;

	/**
	 * Parse a value of any JSON type from a UTF-8 JSON string.
	 *
	 * The parser supports JSON5 features such as comments, unquoted identifiers
	 * and trailing commas.
	 *
	 * \param jsonString read-only view over the JSON string to parse a value
	 *        from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on failure to parse a JSON value.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] static ValueBase parse(UTF8StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type());

	/**
	 * Parse a value of any JSON type from a JSON string interpreted as UTF-8.
	 *
	 * The parser supports JSON5 features such as comments, unquoted identifiers
	 * and trailing commas.
	 *
	 * \param jsonString read-only view over the JSON string to parse a value
	 *        from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on failure to parse a JSON value.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] static ValueBase parse(StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type());

	using VariantBase<Allocator>::VariantBase; // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)

	/**
	 * Construct a JSON value.
	 *
	 * \param value value to construct this value from.
	 * \param allocator allocator to use.
	 */
	ValueBase(VariantBase<Allocator> value, const allocator_type& allocator)
		: VariantBase<Allocator>(std::move(value), allocator) {}

	/**
	 * Construct a JSON value.
	 *
	 * \param value value to construct this value from.
	 */
	ValueBase(VariantBase<Allocator> value)
		: VariantBase<Allocator>(std::move(value)) {}

	/**
	 * Construct a JSON value with an associated source location.
	 *
	 * \param value value to construct this value from.
	 * \param source source location of the value.
	 */
	ValueBase(VariantBase<Allocator> value, const SourceLocation& source)
		: VariantBase<Allocator>(std::move(value))
		, source(source) {}

	/**
	 * Construct a JSON value with an associated source location.
	 *
	 * \param value value to construct this value from.
	 * \param source source location of the value.
	 * \param allocator allocator to use.
	 */
	ValueBase(VariantBase<Allocator> value, const SourceLocation& source, const allocator_type& allocator)
		: VariantBase<Allocator>(std::move(value), allocator)
		, source(source) {}

	/** Destructor. */
	~ValueBase() = default;

	/** Copy constructor. */
	ValueBase(const ValueBase& other, const allocator_type& allocator) noexcept
		: VariantBase<Allocator>(static_cast<const VariantBase<Allocator>&>(other), allocator)
		, source(other.source) {}

	/** Copy constructor. */
	ValueBase(const ValueBase& other) = default;

	/** Move constructor. */
	ValueBase(ValueBase&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: VariantBase<Allocator>(std::move(static_cast<VariantBase<Allocator>&>(other)), allocator)
		, source(other.source) {}

	/** Move constructor. */
	ValueBase(ValueBase&& other) noexcept = default;

	/** Copy assignment. */
	ValueBase& operator=(const ValueBase& other) = default;

	/** Move assignment. */
	ValueBase& operator=(ValueBase&& other) noexcept(
		std::allocator_traits<
			allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		std::allocator_traits<allocator_type>::is_always_equal::value) = default;

	/**
	 * Check if this value is of type Null.
	 *
	 * \return true if this value is null, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isNull() const noexcept {
		return this->template is<Null>();
	}

	/**
	 * Get this value as Null, throwing an exception if it is not actually null.
	 *
	 * \return a reference to the null value.
	 *
	 * \throw json::Error if this value is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null& getNull() & {
		if (!this->template is<Null>()) {
			throw json::Error{"Expected null.", source};
		}
		return this->template as<Null>();
	}

	/**
	 * Get this value as Null, throwing an exception if it is not actually null.
	 *
	 * \return a read-only reference to the null value.
	 *
	 * \throw json::Error if this value is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNull() const& {
		if (!this->template is<Null>()) {
			throw json::Error{"Expected null.", source};
		}
		return this->template as<Null>();
	}

	/**
	 * Get this value as Null, throwing an exception if it is not actually null.
	 *
	 * \return an rvalue reference to the null value.
	 *
	 * \throw json::Error if this value is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null&& getNull() && {
		if (!this->template is<Null>()) {
			throw json::Error{"Expected null.", source};
		}
		return std::move(*this).template as<Null>();
	}

	/**
	 * Get this value as Null, throwing an exception if it is not actually null.
	 *
	 * \return a read-only rvalue reference to the null value.
	 *
	 * \throw json::Error if this value is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNull() const&& {
		if (!this->template is<Null>()) {
			throw json::Error{"Expected null.", source};
		}
		return std::move(*this).template as<Null>();
	}

	/**
	 * Check if this value is of type Boolean.
	 *
	 * \return true if this value is a boolean, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isBoolean() const noexcept {
		return this->template is<Boolean>();
	}

	/**
	 * Get this value as a Boolean, throwing an exception if it is not actually
	 * a boolean.
	 *
	 * \return a reference to the boolean value.
	 *
	 * \throw json::Error if this value is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean& getBoolean() & {
		if (!this->template is<Boolean>()) {
			throw json::Error{"Expected a boolean.", source};
		}
		return this->template as<Boolean>();
	}

	/**
	 * Get this value as a Boolean, throwing an exception if it is not actually
	 * a boolean.
	 *
	 * \return a read-only reference to the boolean value.
	 *
	 * \throw json::Error if this value is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean& getBoolean() const& {
		if (!this->template is<Boolean>()) {
			throw json::Error{"Expected a boolean.", source};
		}
		return this->template as<Boolean>();
	}

	/**
	 * Get this value as a Boolean, throwing an exception if it is not actually
	 * a boolean.
	 *
	 * \return an rvalue reference to the boolean value.
	 *
	 * \throw json::Error if this value is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean&& getBoolean() && {
		if (!this->template is<Boolean>()) {
			throw json::Error{"Expected a boolean.", source};
		}
		return std::move(*this).template as<Boolean>();
	}

	/**
	 * Get this value as a Boolean, throwing an exception if it is not actually
	 * a boolean.
	 *
	 * \return a read-only rvalue reference to the boolean value.
	 *
	 * \throw json::Error if this value is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean&& getBoolean() const&& {
		if (!this->template is<Boolean>()) {
			throw json::Error{"Expected a boolean.", source};
		}
		return std::move(*this).template as<Boolean>();
	}

	/**
	 * Check if this value is of type String.
	 *
	 * \return true if this value is a string, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isString() const noexcept {
		return this->template is<String>();
	}

	/**
	 * Get this value as a String, throwing an exception if it is not actually a
	 * string.
	 *
	 * \return a read-only reference to the the string value.
	 *
	 * \throw json::Error if this value is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const StringBase<Allocator>& getString() const& {
		if (!this->template is<StringBase<Allocator>>()) {
			throw json::Error{"Expected a string.", source};
		}
		return this->template as<StringBase<Allocator>>();
	}

	/**
	 * Get this value as a String, throwing an exception if it is not actually a
	 * string.
	 *
	 * \return a reference to the string value.
	 *
	 * \throw json::Error if this value is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE StringBase<Allocator>& getString() & {
		if (!this->template is<StringBase<Allocator>>()) {
			throw json::Error{"Expected a string.", source};
		}
		return this->template as<StringBase<Allocator>>();
	}

	/**
	 * Get this value as a String, throwing an exception if it is not actually a
	 * string.
	 *
	 * \return a read-only rvalue reference to the string value.
	 *
	 * \throw json::Error if this value is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE StringBase<Allocator>&& getString() && {
		if (!this->template is<StringBase<Allocator>>()) {
			throw json::Error{"Expected a string.", source};
		}
		return std::move(*this).template as<StringBase<Allocator>>();
	}

	/**
	 * Get this value as a String, throwing an exception if it is not actually a
	 * string.
	 *
	 * \return an rvalue reference to the string value.
	 *
	 * \throw json::Error if this value is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const StringBase<Allocator>&& getString() const&& {
		if (!this->template is<StringBase<Allocator>>()) {
			throw json::Error{"Expected a string.", source};
		}
		return std::move(*this).template as<StringBase<Allocator>>();
	}

	/**
	 * Check if this value is of type Number.
	 *
	 * \return true if this value is a number, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isNumber() const noexcept {
		return this->template is<Number>();
	}

	/**
	 * Get this value as a Number, throwing an exception if it is not actually a
	 * number.
	 *
	 * \return a reference to the number value.
	 *
	 * \throw json::Error if this value is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number& getNumber() & {
		if (!this->template is<Number>()) {
			throw json::Error{"Expected a number.", source};
		}
		return this->template as<Number>();
	}

	/**
	 * Get this value as a Number, throwing an exception if it is not actually a
	 * number.
	 *
	 * \return a read-only reference to the number value.
	 *
	 * \throw json::Error if this value is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number& getNumber() const& {
		if (!this->template is<Number>()) {
			throw json::Error{"Expected a number.", source};
		}
		return this->template as<Number>();
	}

	/**
	 * Get this value as a Number, throwing an exception if it is not actually a
	 * number.
	 *
	 * \return an rvalue reference to the number value.
	 *
	 * \throw json::Error if this value is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number&& getNumber() && {
		if (!this->template is<Number>()) {
			throw json::Error{"Expected a number.", source};
		}
		return std::move(*this).template as<Number>();
	}

	/**
	 * Get this value as a Number, throwing an exception if it is not actually a
	 * number.
	 *
	 * \return a read-only rvalue reference to the number value.
	 *
	 * \throw json::Error if this value is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number&& getNumber() const&& {
		if (!this->template is<Number>()) {
			throw json::Error{"Expected a number.", source};
		}
		return std::move(*this).template as<Number>();
	}

	/**
	 * Get this value as a Number, throwing an exception if it is not actually a
	 * number, or not the correct kind of number.
	 *
	 * \tparam T number type to get.
	 *
	 * \return the number value, converted to the given type.
	 *
	 * \throw json::Error if this value is not a valid number of the given type.
	 */
	template <typename T>
	[[nodiscard]] GREM_ALWAYS_INLINE T getNumber() const {
		const json::Number number = getNumber();
		if constexpr (integral<T>) {
			if (trunc(number) != number) {
				throw json::Error{"Expected an integer.", source};
			}
			if (number < static_cast<json::Number>(Limits<T>::MIN) || number > static_cast<json::Number>(Limits<T>::MAX)) {
				throw json::Error{"Value out of range.", source};
			}
		}
		return static_cast<T>(number);
	}

	/**
	 * Check if this value is of type Object.
	 *
	 * \return true if this value is an object, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isObject() const noexcept {
		return this->template is<Object>();
	}

	/**
	 * Get this value as an Object, throwing an exception if it is not actually
	 * an object.
	 *
	 * \return a reference to the object value.
	 *
	 * \throw json::Error if this value is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ObjectBase<Allocator>& getObject() & {
		if (!this->template is<ObjectBase<Allocator>>()) {
			throw json::Error{"Expected an object.", source};
		}
		return this->template as<ObjectBase<Allocator>>();
	}

	/**
	 * Get this value as an Object, throwing an exception if it is not actually
	 * an object.
	 *
	 * \return a read-only reference to the object value.
	 *
	 * \throw json::Error if this value is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ObjectBase<Allocator>& getObject() const& {
		if (!this->template is<ObjectBase<Allocator>>()) {
			throw json::Error{"Expected an object.", source};
		}
		return this->template as<ObjectBase<Allocator>>();
	}

	/**
	 * Get this value as an Object, throwing an exception if it is not actually
	 * an object.
	 *
	 * \return an rvalue reference to the object value.
	 *
	 * \throw json::Error if this value is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ObjectBase<Allocator>&& getObject() && {
		if (!this->template is<ObjectBase<Allocator>>()) {
			throw json::Error{"Expected an object.", source};
		}
		return std::move(*this).template as<ObjectBase<Allocator>>();
	}

	/**
	 * Get this value as an Object, throwing an exception if it is not actually
	 * an object.
	 *
	 * \return a read-only rvalue reference to the object value.
	 *
	 * \throw json::Error if this value is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ObjectBase<Allocator>&& getObject() const&& {
		if (!this->template is<ObjectBase<Allocator>>()) {
			throw json::Error{"Expected an object.", source};
		}
		return std::move(*this).template as<ObjectBase<Allocator>>();
	}

	/**
	 * Check if this value is of type Array.
	 *
	 * \return true if this value is an array, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isArray() const noexcept {
		return this->template is<Array>();
	}

	/**
	 * Get the size of the Array in this value, throwing an exception if it is
	 * not actually an array.
	 *
	 * \return the number of elements in the array.
	 *
	 * \throw json::Error if this value is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t getArraySize() const {
		if (!this->template is<ArrayBase<Allocator>>()) {
			throw json::Error{"Expected an array.", source};
		}
		return this->template as<ArrayBase<Allocator>>().size();
	}

	/**
	 * Get this value as an Array, throwing an exception if it is not actually
	 * an array.
	 *
	 * \return a reference to the array value.
	 *
	 * \throw json::Error if this value is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ArrayBase<Allocator>& getArray() & {
		if (!this->template is<ArrayBase<Allocator>>()) {
			throw json::Error{"Expected an array.", source};
		}
		return this->template as<ArrayBase<Allocator>>();
	}

	/**
	 * Get this value as an Array, throwing an exception if it is not actually
	 * an array.
	 *
	 * \return a read-only reference to the array value.
	 *
	 * \throw json::Error if this value is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ArrayBase<Allocator>& getArray() const& {
		if (!this->template is<ArrayBase<Allocator>>()) {
			throw json::Error{"Expected an array.", source};
		}
		return this->template as<ArrayBase<Allocator>>();
	}

	/**
	 * Get this value as an Array, throwing an exception if it is not actually
	 * an array.
	 *
	 * \return an rvalue reference to the array value.
	 *
	 * \throw json::Error if this value is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ArrayBase<Allocator>&& getArray() && {
		if (!this->template is<ArrayBase<Allocator>>()) {
			throw json::Error{"Expected an array.", source};
		}
		return std::move(*this).template as<ArrayBase<Allocator>>();
	}

	/**
	 * Get this value as an Array, throwing an exception if it is not actually
	 * an array.
	 *
	 * \return a read-only rvalue reference to the array value.
	 *
	 * \throw json::Error if this value is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ArrayBase<Allocator>&& getArray() const&& {
		if (!this->template is<ArrayBase<Allocator>>()) {
			throw json::Error{"Expected an array.", source};
		}
		return std::move(*this).template as<ArrayBase<Allocator>>();
	}

	/**
	 * Check if this value is of type Object and has a property with a given
	 * key.
	 *
	 * \param key key of the property to check for.
	 *
	 * \return true if this value is an object that has the given property,
	 *         false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasProperty(StringView key) const noexcept {
		return isObject() && this->template as<ObjectBase<Allocator>>().contains(key);
	}

	/**
	 * Get a property of the Object in this value, returning nullptr if it is
	 * not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a non-owning pointer to the specified property value, or nullptr
	 *         if this value is not an object, or if the specified property
	 *         doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>* findProperty(StringView key) noexcept {
		if (isObject()) {
			ObjectBase<Allocator>& object = this->template as<ObjectBase<Allocator>>();
			if (const auto it = object.find(key); it != object.end()) {
				return &it->second;
			}
		}
		return nullptr;
	}

	/**
	 * Get a property of the Object in this value, returning nullptr if it is
	 * not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a non-owning read-only pointer to the specified property value,
	 *         or nullptr if this value is not an object, or if the specified
	 *         property doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>* findProperty(StringView key) const noexcept {
		if (isObject()) {
			const ObjectBase<Allocator>& object = this->template as<ObjectBase<Allocator>>();
			if (const auto it = object.find(key); it != object.end()) {
				return &it->second;
			}
		}
		return nullptr;
	}

	/**
	 * Get a property of the Object in this value, throwing an exception if it
	 * is not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>& getProperty(StringView key) & {
		ObjectBase<Allocator>& object = getObject();
		const auto it = object.find(key);
		if (it == object.end()) {
			throw json::Error{formatString("Missing property \"{}\".", key), source};
		}
		return it->second;
	}

	/**
	 * Get a property of the Object in this value, throwing an exception if it
	 * is not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>& getProperty(StringView key) const& {
		const ObjectBase<Allocator>& object = getObject();
		const auto it = object.find(key);
		if (it == object.end()) {
			throw json::Error{formatString("Missing property \"{}\".", key), source};
		}
		return it->second;
	}

	/**
	 * Get a property of the Object in this value, throwing an exception if it
	 * is not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>&& getProperty(StringView key) && {
		ObjectBase<Allocator>&& object = std::move(*this).getObject();
		const auto it = object.find(key);
		if (it == object.end()) {
			throw json::Error{formatString("Missing property \"{}\".", key), source};
		}
		return std::move(it->second);
	}

	/**
	 * Get a property of the Object in this value, throwing an exception if it
	 * is not actually an object or if the property doesn't exist.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>&& getProperty(StringView key) const&& {
		const ObjectBase<Allocator>&& object = std::move(*this).getObject();
		const auto it = object.find(key);
		if (it == object.end()) {
			throw json::Error{formatString("Missing property \"{}\".", key), source};
		}
		return std::move(it->second);
	}

	/**
	 * Check if this value is of type Array and has an item at the given index.
	 *
	 * \param item array index of the item to check for.
	 *
	 * \return true if this value is an array that has an item at the given
	 *         index, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasItem(size_t index) const noexcept {
		return isArray() && index < this->template as<ArrayBase<Allocator>>().size();
	}

	/**
	 * Get an item of the Array in this value, returning nullptr if it is not
	 * actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a non-owning pointer to the specified item value, or nullptr if
	 *         this value is not an array, or if the specified index is out of
	 *         range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>* findItem(size_t index) noexcept {
		if (isArray()) {
			ArrayBase<Allocator>& array = this->template as<ArrayBase<Allocator>>();
			if (index < array.size()) {
				return &array[index];
			}
		}
		return nullptr;
	}

	/**
	 * Get an item of the Array in this value, returning nullptr if it is not
	 * actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a non-owning read-only pointer to the specified item value, or
	 *         nullptr if this value is not an array, or if the specified index
	 *         is out of range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>* findItem(size_t index) const noexcept {
		if (isArray()) {
			const ArrayBase<Allocator>& array = this->template as<ArrayBase<Allocator>>();
			if (index < array.size()) {
				return &array[index];
			}
		}
		return nullptr;
	}

	/**
	 * Get an item of the Array in this value, throwing an exception if it is
	 * not actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>& getItem(size_t index) & {
		ArrayBase<Allocator>& array = getArray();
		if (index >= array.size()) {
			throw json::Error{formatString("Missing item \"[{}]\".", index), source};
		}
		return array[index];
	}

	/**
	 * Get an item of the Array in this value, throwing an exception if it is
	 * not actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>& getItem(size_t index) const& {
		const ArrayBase<Allocator>& array = getArray();
		if (index >= array.size()) {
			throw json::Error{formatString("Missing item \"[{}]\".", index), source};
		}
		return array[index];
	}

	/**
	 * Get an item of the Array in this value, throwing an exception if it is
	 * not actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ValueBase<Allocator>&& getItem(size_t index) && {
		ArrayBase<Allocator>&& array = std::move(*this).getArray();
		if (index >= array.size()) {
			throw json::Error{formatString("Missing item \"[{}]\".", index), source};
		}
		return std::move(array[index]);
	}

	/**
	 * Get an item of the Array in this value, throwing an exception if it is
	 * not actually an array or if the index is out of range.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const ValueBase<Allocator>&& getItem(size_t index) const&& {
		const ArrayBase<Allocator>&& array = std::move(*this).getArray();
		if (index >= array.size()) {
			throw json::Error{formatString("Missing item \"[{}]\".", index), source};
		}
		return std::move(array[index]);
	}

	/**
	 * Get a property of the Object in this value as Null, throwing an exception
	 * if this value is not actually an object, if the property doesn't exist,
	 * or if the property is not actually null.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified null property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null& getNullProperty(StringView key) & {
		return getProperty(key).getNull();
	}

	/**
	 * Get a property of the Object in this value as Null, throwing an exception
	 * if this value is not actually an object, if the property doesn't exist,
	 * or if the property is not actually null.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified null property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNullProperty(StringView key) const& {
		return getProperty(key).getNull();
	}

	/**
	 * Get a property of the Object in this value as Null, throwing an exception
	 * if this value is not actually an object, if the property doesn't exist,
	 * or if the property is not actually null.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified null property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null&& getNullProperty(StringView key) && {
		return std::move(*this).getProperty(key).getNull();
	}

	/**
	 * Get a property of the Object in this value as Null, throwing an exception
	 * if this value is not actually an object, if the property doesn't exist,
	 * or if the property is not actually null.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified null property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNullProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getNull();
	}

	/**
	 * Get an item of the Array in this value as Null, throwing an exception if
	 * this value is not actually an array, if the index is out of range, or if
	 * the item is not actually null.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified null item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null& getNullItem(size_t index) & {
		return getItem(index).getNull();
	}

	/**
	 * Get an item of the Array in this value as Null, throwing an exception if
	 * this value is not actually an array, if the index is out of range, or if
	 * the item is not actually null.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified null item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNullItem(size_t index) const& {
		return getItem(index).getNull();
	}

	/**
	 * Get an item of the Array in this value as Null, throwing an exception if
	 * this value is not actually an array, if the index is out of range, or if
	 * the item is not actually null.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified null item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Null&& getNullItem(size_t index) && {
		return std::move(*this).getItem(index).getNull();
	}

	/**
	 * Get an item of the Array in this value as Null, throwing an exception if
	 * this value is not actually an array, if the index is out of range, or if
	 * the item is not actually null.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified null item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Null& getNullItem(size_t index) const&& {
		return std::move(*this).getItem(index).getNull();
	}

	/**
	 * Get a property of the Object in this value as a Boolean, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a boolean.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified boolean property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean& getBooleanProperty(StringView key) & {
		return getProperty(key).getBoolean();
	}

	/**
	 * Get a property of the Object in this value as a Boolean, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a boolean.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified boolean property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean& getBooleanProperty(StringView key) const& {
		return getProperty(key).getBoolean();
	}

	/**
	 * Get a property of the Object in this value as a Boolean, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a boolean.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified boolean property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean&& getBooleanProperty(StringView key) && {
		return std::move(*this).getProperty(key).getBoolean();
	}

	/**
	 * Get a property of the Object in this value as a Boolean, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a boolean.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified boolean property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean& getBooleanProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getBoolean();
	}

	/**
	 * Get an item of the Array in this value as a Boolean, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a boolean.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified boolean item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean& getBooleanItem(size_t index) & {
		return getItem(index).getBoolean();
	}

	/**
	 * Get an item of the Array in this value as a Boolean, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a boolean.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified boolean item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean& getBooleanItem(size_t index) const& {
		return getItem(index).getBoolean();
	}

	/**
	 * Get an item of the Array in this value as a Boolean, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a boolean.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified boolean item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Boolean&& getBooleanItem(size_t index) && {
		return std::move(*this).getItem(index).getBoolean();
	}

	/**
	 * Get an item of the Array in this value as a Boolean, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a boolean.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified boolean item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a boolean.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Boolean& getBooleanItem(size_t index) const&& {
		return std::move(*this).getItem(index).getBoolean();
	}

	/**
	 * Get a property of the Object in this value as a String, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a string.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified string property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE String& getStringProperty(StringView key) & {
		return getProperty(key).getString();
	}

	/**
	 * Get a property of the Object in this value as a String, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a string.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified string property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const String& getStringProperty(StringView key) const& {
		return getProperty(key).getString();
	}

	/**
	 * Get a property of the Object in this value as a String, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a string.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified string property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE String&& getStringProperty(StringView key) && {
		return std::move(*this).getProperty(key).getString();
	}

	/**
	 * Get a property of the Object in this value as a String, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a string.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified string property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const String& getStringProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getString();
	}

	/**
	 * Get an item of the Array in this value as a String, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a string.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified string item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE String& getStringItem(size_t index) & {
		return getItem(index).getString();
	}

	/**
	 * Get an item of the Array in this value as a String, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a string.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified string item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const String& getStringItem(size_t index) const& {
		return getItem(index).getString();
	}

	/**
	 * Get an item of the Array in this value as a String, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a string.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified string item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE String&& getStringItem(size_t index) && {
		return std::move(*this).getItem(index).getString();
	}

	/**
	 * Get an item of the Array in this value as a String, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a string.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified string item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a string.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const String& getStringItem(size_t index) const&& {
		return std::move(*this).getItem(index).getString();
	}

	/**
	 * Get a property of the Object in this value as a Number, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a number.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified number property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number& getNumberProperty(StringView key) & {
		return getProperty(key).getNumber();
	}

	/**
	 * Get a property of the Object in this value as a Number, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a number.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified number property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number& getNumberProperty(StringView key) const& {
		return getProperty(key).getNumber();
	}

	/**
	 * Get a property of the Object in this value as a Number, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a number.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified number property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number&& getNumberProperty(StringView key) && {
		return std::move(*this).getProperty(key).getNumber();
	}

	/**
	 * Get a property of the Object in this value as a Number, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a number.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified number property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number& getNumberProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getNumber();
	}

	/**
	 * Get a property of the Object in this value as a Number, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually a number, or not the
	 * correct kind of number.
	 *
	 * \tparam T number type to get.
	 *
	 * \param key key of the property to get.
	 *
	 * \return the number value of the specified property, converted to the
	 *         given type.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not a
	 *        number of the given type.
	 */
	template <typename T>
	[[nodiscard]] GREM_ALWAYS_INLINE T getNumberProperty(StringView key) const {
		return getProperty(key).template getNumber<T>();
	}

	/**
	 * Get an item of the Array in this value as a Number, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a number.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified number item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number& getNumberItem(size_t index) & {
		return getItem(index).getNumber();
	}

	/**
	 * Get an item of the Array in this value as a Number, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a number.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified number item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number& getNumberItem(size_t index) const& {
		return getItem(index).getNumber();
	}

	/**
	 * Get an item of the Array in this value as a Number, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a number.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified number item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Number&& getNumberItem(size_t index) && {
		return std::move(*this).getItem(index).getNumber();
	}

	/**
	 * Get an item of the Array in this value as a Number, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a number.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified number item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a number.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Number& getNumberItem(size_t index) const&& {
		return std::move(*this).getItem(index).getNumber();
	}

	/**
	 * Get an item of the Array in this value as a Number, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually a number, or not the correct kind
	 * of number.
	 *
	 * \tparam T number type to get.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return the number value of the specified item, converted to the given
	 *         type.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not a number of
	 *        the given type.
	 */
	template <typename T>
	[[nodiscard]] GREM_ALWAYS_INLINE T getNumberItem(size_t index) const {
		return getItem(index).template getNumber<T>();
	}

	/**
	 * Get a property of the Object in this value as an Object, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an object.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified object property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Object& getObjectProperty(StringView key) & {
		return getProperty(key).getObject();
	}

	/**
	 * Get a property of the Object in this value as an Object, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an object.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified object property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Object& getObjectProperty(StringView key) const& {
		return getProperty(key).getObject();
	}

	/**
	 * Get a property of the Object in this value as an Object, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an object.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified object property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Object&& getObjectProperty(StringView key) && {
		return std::move(*this).getProperty(key).getObject();
	}

	/**
	 * Get a property of the Object in this value as an Object, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an object.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified object property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Object& getObjectProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getObject();
	}

	/**
	 * Get an item of the Array in this value as an Object, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an object.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified object item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Object& getObjectItem(size_t index) & {
		return getItem(index).getObject();
	}

	/**
	 * Get an item of the Array in this value as an Object, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an object.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified object item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Object& getObjectItem(size_t index) const& {
		return getItem(index).getObject();
	}

	/**
	 * Get an item of the Array in this value as an Object, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an object.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified object item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Object&& getObjectItem(size_t index) && {
		return std::move(*this).getItem(index).getObject();
	}

	/**
	 * Get an item of the Array in this value as an Object, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an object.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified object item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an object.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Object& getObjectItem(size_t index) const&& {
		return std::move(*this).getItem(index).getObject();
	}

	/**
	 * Get a property of the Object in this value as an Array, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an array.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a reference to the specified array property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Array& getArrayProperty(StringView key) & {
		return getProperty(key).getArray();
	}

	/**
	 * Get a property of the Object in this value as an Array, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an array.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only reference to the specified array property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Array& getArrayProperty(StringView key) const& {
		return getProperty(key).getArray();
	}

	/**
	 * Get a property of the Object in this value as an Array, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an array.
	 *
	 * \param key key of the property to get.
	 *
	 * \return an rvalue reference to the specified array property value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Array&& getArrayProperty(StringView key) && {
		return std::move(*this).getProperty(key).getArray();
	}

	/**
	 * Get a property of the Object in this value as an Array, throwing an
	 * exception if this value is not actually an object, if the property
	 * doesn't exist, or if the property is not actually an array.
	 *
	 * \param key key of the property to get.
	 *
	 * \return a read-only rvalue reference to the specified array property
	 *         value.
	 *
	 * \throw json::Error if this value is not an object, or if the specified
	 *        property doesn't exist, or if the specified property is not an
	 *        array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Array& getArrayProperty(StringView key) const&& {
		return std::move(*this).getProperty(key).getArray();
	}

	/**
	 * Get an item of the Array in this value as an Array, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an array.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a reference to the specified array item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Array& getArrayItem(size_t index) & {
		return getItem(index).getArray();
	}

	/**
	 * Get an item of the Array in this value as an Array, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an array.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only reference to the specified array item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Array& getArrayItem(size_t index) const& {
		return getItem(index).getArray();
	}

	/**
	 * Get an item of the Array in this value as an Array, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an array.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return an rvalue reference to the specified array item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Array&& getArrayItem(size_t index) && {
		return std::move(*this).getItem(index).getArray();
	}

	/**
	 * Get an item of the Array in this value as an Array, throwing an
	 * exception if this value is not actually an array, if the index is out of
	 * range, or if the item is not actually an array.
	 *
	 * \param index array index of the item to get.
	 *
	 * \return a read-only rvalue reference to the specified array item value.
	 *
	 * \throw json::Error if this value is not an array, or if the specified
	 *        index is out of range, or if the specified item is not an array.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const Array& getArrayItem(size_t index) const&& {
		return std::move(*this).getItem(index).getArray();
	}

	/**
	 * Get the source location of this value.
	 *
	 * \return the associated source location.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE SourceLocation getSource() const noexcept {
		return source;
	}

	/**
	 * Compare this value to another for equality.
	 *
	 * \param other the value to compare this value to.
	 *
	 * \return true if the values are equal (ignoring source location), false
	 *         otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool operator==(const ValueBase& other) const {
		return static_cast<const VariantBase<Allocator>&>(*this) == static_cast<const VariantBase<Allocator>&>(other);
	}

private:
	SourceLocation source{.lineNumber = 0, .columnNumber = 0};
};

/**
 * JSON value type with an associated SourceLocation using the default
 * allocator.
 */
using Value = json::ValueBase<std::allocator>;

/**
 * Check if a Unicode code point is considered to be whitespace in JSON5.
 *
 * \param codePoint code point value to check.
 *
 * \return true if the code point is considered whitespace, false otherwise.
 */
[[nodiscard]] constexpr bool isWhitespace(char32_t codePoint) noexcept {
	return codePoint == '\t' || codePoint == '\n' || codePoint == '\v' || codePoint == '\f' || codePoint == '\r' || codePoint == ' ' || codePoint == 0x00A0 ||
	       codePoint == 0x1680 || (codePoint >= 0x2000 && codePoint <= 0x200A) || codePoint == 0x2028 || codePoint == 0x2029 || codePoint == 0x202F || codePoint == 0x205F ||
	       codePoint == 0x3000 || codePoint == 0xFEFF;
}

/**
 * Check if a Unicode code point is considered to be punctuation in JSON5.
 *
 * \param codePoint code point value to check.
 *
 * \return true if the code point is considered punctuation, false otherwise.
 */
[[nodiscard]] constexpr bool isPunctuationCharacter(char32_t codePoint) noexcept {
	return codePoint == ',' || codePoint == ':' || codePoint == '[' || codePoint == ']' || codePoint == '{' || codePoint == '}';
}

/**
 * Check if a Unicode code point marks the beginning of a line terminator
 * sequence in JSON5.
 *
 * \param codePoint code point value to check.
 *
 * \return true if the code point is considered a line terminator, false
 *         otherwise.
 */
[[nodiscard]] constexpr bool isLineTerminatorCharacter(char32_t codePoint) noexcept {
	return codePoint == '\n' || codePoint == '\r' || codePoint == 0x2028 || codePoint == 0x2029;
}

/**
 * Type of a scanned JSON5 token.
 */
enum class TokenType : uint8_t {
	END_OF_FILE,                     ///< End-of-file marker.
	IDENTIFIER_NULL,                 ///< Keyword null.
	IDENTIFIER_FALSE,                ///< Keyword false.
	IDENTIFIER_TRUE,                 ///< Keyword true.
	IDENTIFIER_NAME,                 ///< Unquoted identifier, e.g. abc.
	PUNCTUATOR_COMMA,                ///< Comma ',' symbol.
	PUNCTUATOR_COLON,                ///< Colon ':' symbol.
	PUNCTUATOR_OPEN_SQUARE_BRACKET,  ///< Open square bracket '[' symbol.
	PUNCTUATOR_CLOSE_SQUARE_BRACKET, ///< Closing square bracket ']' symbol.
	PUNCTUATOR_OPEN_CURLY_BRACE,     ///< Open curly brace '{' symbol.
	PUNCTUATOR_CLOSE_CURLY_BRACE,    ///< Closing curly brace '}' symbol.
	STRING,                          ///< Quoted string literal, e.g. "abc".
	NUMBER_BINARY,                   ///< Binary number literal, e.g. 0b0000000111111111.
	NUMBER_OCTAL,                    ///< Octal number literal, e.g. 0777.
	NUMBER_DECIMAL,                  ///< Decimal number literal, e.g. 511.
	NUMBER_HEXADECIMAL,              ///< Hexadecimal number literal, e.g. 0x01FF.
	NUMBER_POSITIVE_INFINITY,        ///< Keyword Infinity.
	NUMBER_NEGATIVE_INFINITY,        ///< Keyword -Infinity.
	NUMBER_POSITIVE_NAN,             ///< Keyword NaN.
	NUMBER_NEGATIVE_NAN,             ///< Keyword -NaN.
};

/**
 * Token data scanned from JSON.
 */
template <template <typename> typename Allocator>
struct TokenBase {
	StringBase<Allocator> string; ///< Scanned string.
	SourceLocation source;        ///< Location of the scanned string in the JSON source string.
	TokenType type;               ///< Scanned token type.
};

/**
 * Lexical analyzer for scanning and tokenizing input in the JSON5 format.
 *
 * \tparam It iterator type of the underlying input source. Must be an input
 *         iterator.
 */
template <typename It, template <typename> typename Allocator = std::allocator>
class Lexer {
public:
	/** Allocator type used by the lexer. */
	using allocator_type = Allocator<VariantBase<Allocator>>;

	/** Token type of the lexer. */
	using Token = TokenBase<Allocator>;

	/**
	 * Construct a lexer with a Unicode iterator pair as input.
	 *
	 * \param it iterator to the beginning of the JSON input to scan.
	 * \param end sentinel that marks the end of the JSON input to scan.
	 * \param source initial source location corresponding to the current
	 *               position of the iterator.
	 * \param allocator allocator to use.
	 *
	 * \warning The iterator pair [it, end) must form a valid forward range.
	 */
	Lexer(unicode::UTF8Iterator<It> it, unicode::UTF8Sentinel end, const SourceLocation& source, const allocator_type& allocator = allocator_type())
		: it(std::move(it))
		, end(end)
		, source(source)
		, allocator(allocator) {}

	/** Copy constructor. */
	Lexer(const Lexer& other, const allocator_type& allocator)
		: it(other.it)
		, end(other.end)
		, source(other.source)
		, currentCodePoint(other.currentCodePoint)
		, allocator(allocator) {}

	/** Move constructor. */
	Lexer(Lexer&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: it(other.it)
		, end(other.end)
		, source(other.source)
		, currentCodePoint(other.currentCodePoint)
		, allocator(allocator) {}

	/** Destructor. */
	~Lexer() = default;

	/** Copy constructor. */
	Lexer(const Lexer& other)
		: Lexer(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

	/** Move constructor. */
	Lexer(Lexer&& other) noexcept
		: Lexer(std::move(other), other.get_allocator()) {}

	/** Copy assignment. */
	Lexer& operator=(const Lexer& other) {
		if (this == &other) {
			return *this;
		}
		it = other.it;
		end = other.end;
		source = other.source;
		currentCodePoint = other.currentCodePoint;
		if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	/** Move assignment. */
	Lexer& operator=(Lexer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		it = other.it;
		end = other.end;
		source = other.source;
		currentCodePoint = other.currentCodePoint;
		if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value) {
			allocator = other.allocator;
		}
		return *this;
	}

	/**
	 * Get the allocator used by the lexer.
	 *
	 * \return a copy of the lexer's allocator.
	 */
	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return allocator;
	}

	/**
	 * Scan and consume the next token from the input.
	 *
	 * This advances the internal state of the lexer.
	 *
	 * \return the scanned token.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 */
	Token scan() {
		skipWhitespace();
		if (hasReachedEnd()) {
			return {.string{allocator}, .source = source, .type = TokenType::END_OF_FILE};
		}
		switch (peek()) {
			case '{': [[fallthrough]];
			case '}': [[fallthrough]];
			case '[': [[fallthrough]];
			case ']': [[fallthrough]];
			case ':': [[fallthrough]];
			case ',': return scanPunctuator();
			case '\"': [[fallthrough]];
			case '\'': return scanString();
			case '0': [[fallthrough]];
			case '1': [[fallthrough]];
			case '2': [[fallthrough]];
			case '3': [[fallthrough]];
			case '4': [[fallthrough]];
			case '5': [[fallthrough]];
			case '6': [[fallthrough]];
			case '7': [[fallthrough]];
			case '8': [[fallthrough]];
			case '9': [[fallthrough]];
			case '+': [[fallthrough]];
			case '-': [[fallthrough]];
			case '.': return scanNumber();
			default: return scanIdentifier();
		}
	}

private:
	void skipWhitespace() {
		while (!hasReachedEnd()) {
			if (isWhitespace(peek())) {
				if (isLineTerminatorCharacter(peek())) {
					skipLineTerminatorSequence();
				} else {
					advance();
				}
			} else if (peek() == '/') {
				advance();
				if (hasReachedEnd()) {
					throw json::Error{"Invalid token.", source};
				}
				if (peek() == '/') {
					advance();
					while (!hasReachedEnd()) {
						if (isLineTerminatorCharacter(peek())) {
							skipLineTerminatorSequence();
							break;
						}
						advance();
					}
				} else if (peek() == '*') {
					advance();
					while (!hasReachedEnd()) {
						if (isLineTerminatorCharacter(peek())) {
							skipLineTerminatorSequence();
						} else if (peek() == '*') {
							advance();
							if (!hasReachedEnd() && peek() == '/') {
								advance();
								break;
							}
						} else {
							advance();
						}
					}
				} else {
					throw json::Error{"Invalid token.", source};
				}
			} else {
				break;
			}
		}
	}

	void skipLineTerminatorSequence() {
		if (peek() == '\r') {
			advance();
			if (!hasReachedEnd() && peek() == '\n') {
				advance();
			}
		} else {
			advance();
		}
		++source.lineNumber;
		source.columnNumber = 1;
	}

	void advance() {
		if (!currentCodePoint) {
			++it;
		}
		currentCodePoint.reset();
		++source.columnNumber;
	}

	[[nodiscard]] bool hasReachedEnd() const noexcept {
		return it == end && !currentCodePoint;
	}

	[[nodiscard]] char32_t peek() const {
		if (!currentCodePoint) {
			currentCodePoint = *it++;
		}
		return *currentCodePoint;
	}

	[[nodiscard]] Optional<char32_t> lookahead() const {
		if (!currentCodePoint) {
			currentCodePoint = *it++;
		}
		if (it != end) {
			return *it;
		}
		return {};
	}

	[[nodiscard]] Token scanPunctuator() {
		StringBase<Allocator> string{1, static_cast<char>(peek()), allocator};
		const SourceLocation punctuatorSource = source;
		TokenType type{};
		switch (peek()) {
			case ',': type = TokenType::PUNCTUATOR_COMMA; break;
			case ':': type = TokenType::PUNCTUATOR_COLON; break;
			case '[': type = TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET; break;
			case ']': type = TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET; break;
			case '{': type = TokenType::PUNCTUATOR_OPEN_CURLY_BRACE; break;
			case '}': type = TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE; break;
			default: break;
		}
		advance();
		return {.string = std::move(string), .source = punctuatorSource, .type = type};
	}

	[[nodiscard]] Token scanString() {
		const char32_t quoteCharacter = peek();
		StringBase<Allocator> string{allocator};
		const SourceLocation stringSource = source;
		advance();
		while (!hasReachedEnd()) {
			if (!unicode::isValidCodePoint(peek())) {
				throw json::Error{"Invalid UTF-8.", source};
			}
			if (peek() == quoteCharacter) {
				advance();
				return {.string = std::move(string), .source = stringSource, .type = TokenType::STRING};
			}
			if (isLineTerminatorCharacter(peek())) {
				throw json::Error{"Unexpected line terminator in string.", source};
			}
			if (peek() != '\\') {
				const InplaceBuffer<char8_t, 4> codePointUTF8 = unicode::encodeUTF8FromCodePoint(peek());
				string.append(StringView{std::launder(reinterpret_cast<const char*>(codePointUTF8.data())), codePointUTF8.size()});
				advance();
				continue;
			}
			advance();
			if (hasReachedEnd()) {
				throw json::Error{"Empty escape sequence.", source};
			}
			if (isLineTerminatorCharacter(peek())) {
				skipLineTerminatorSequence();
				continue;
			}
			switch (peek()) {
				case '\"': string.push_back('\"'); break;
				case '\'': string.push_back('\''); break;
				case '\\': string.push_back('\\'); break;
				case 'b': string.push_back('\b'); break;
				case 'f': string.push_back('\f'); break;
				case 'n': string.push_back('\n'); break;
				case 'r': string.push_back('\r'); break;
				case 't': string.push_back('\t'); break;
				case 'v': string.push_back('\v'); break;
				case '0': [[fallthrough]];
				case '1': [[fallthrough]];
				case '2': [[fallthrough]];
				case '3': [[fallthrough]];
				case '4': [[fallthrough]];
				case '5': [[fallthrough]];
				case '6': [[fallthrough]];
				case '7': [[fallthrough]];
				case '8': [[fallthrough]];
				case '9': scanNumericEscapeSequence(string, 1, 3, 8, [](char32_t codePoint) noexcept -> bool { return (codePoint >= '0' && codePoint <= '7'); }); continue;
				case 'x':
					scanNumericEscapeSequence(string, 2, 2, 16, [](char32_t codePoint) noexcept -> bool {
						return (codePoint >= '0' && codePoint <= '9') || (codePoint >= 'a' && codePoint <= 'f') || (codePoint >= 'A' && codePoint <= 'F');
					});
					continue;
				case 'u':
					scanNumericEscapeSequence(string, 4, 4, 16, [](char32_t codePoint) noexcept -> bool {
						return (codePoint >= '0' && codePoint <= '9') || (codePoint >= 'a' && codePoint <= 'f') || (codePoint >= 'A' && codePoint <= 'F');
					});
					continue;
				case 'U':
					scanNumericEscapeSequence(string, 8, 8, 16, [](char32_t codePoint) noexcept -> bool {
						return (codePoint >= '0' && codePoint <= '9') || (codePoint >= 'a' && codePoint <= 'f') || (codePoint >= 'A' && codePoint <= 'F');
					});
					continue;
				default: {
					const InplaceBuffer<char8_t, 4> codePointUTF8 = unicode::encodeUTF8FromCodePoint(peek());
					string.append(StringView{std::launder(reinterpret_cast<const char*>(codePointUTF8.data())), codePointUTF8.size()});
					break;
				}
			}
			advance();
		}
		throw json::Error{"Missing end of string quote character.", source};
	}

	[[nodiscard]] Token scanNumber() {
		StringBase<Allocator> string{allocator};
		const SourceLocation numberSource = source;
		bool negative = false;
		if (peek() == '+') {
			advance();
		} else if (peek() == '-') {
			string.push_back('-');
			advance();
			negative = true;
		}
		if (hasReachedEnd()) {
			throw json::Error{"Missing number.", source};
		}
		if (peek() == 'I') {
			if (scanIdentifier().type == TokenType::NUMBER_POSITIVE_INFINITY) {
				return {.string{allocator}, .source = numberSource, .type = (negative) ? TokenType::NUMBER_NEGATIVE_INFINITY : TokenType::NUMBER_POSITIVE_INFINITY};
			}
			throw json::Error{"Invalid number.", numberSource};
		}
		if (peek() == 'N') {
			if (scanIdentifier().type == TokenType::NUMBER_POSITIVE_NAN) {
				return {.string{allocator}, .source = numberSource, .type = (negative) ? TokenType::NUMBER_NEGATIVE_NAN : TokenType::NUMBER_POSITIVE_NAN};
			}
			throw json::Error{"Invalid number.", numberSource};
		}
		TokenType type = TokenType::NUMBER_DECIMAL;
		if (!hasReachedEnd() && peek() == '0') {
			string.push_back('0');
			advance();
			if (!hasReachedEnd() && (peek() == 'b' || peek() == 'B')) {
				string.push_back('b');
				advance();
				type = TokenType::NUMBER_BINARY;
			} else if (!hasReachedEnd() && (peek() == 'x' || peek() == 'X')) {
				string.push_back('x');
				advance();
				type = TokenType::NUMBER_HEXADECIMAL;
			} else if (hasReachedEnd() || peek() != '.') {
				type = TokenType::NUMBER_OCTAL;
			}
		}
		bool eNotation = false;
		bool fraction = false;
		while (!hasReachedEnd()) {
			if (peek() == '.') {
				if (lookahead() == '.') {
					break;
				}
				if (type != TokenType::NUMBER_DECIMAL) {
					break;
				}
				if (eNotation) {
					throw json::Error{"Decimal point in E notation exponent.", source};
				}
				if (fraction) {
					throw json::Error{"Multiple decimal points in number.", source};
				}
				string.push_back('.');
				advance();
				fraction = true;
			} else if ((peek() == 'e' || peek() == 'E') && type != TokenType::NUMBER_HEXADECIMAL) {
				if (type != TokenType::NUMBER_DECIMAL) {
					break;
				}
				if (eNotation) {
					throw json::Error{"Multiple exponent symbols in E notation.", source};
				}
				string.push_back('e');
				advance();
				eNotation = true;
				fraction = true;
				if (hasReachedEnd()) {
					throw json::Error{"Missing exponent in E notation.", source};
				}
				if (peek() >= '0' && peek() <= '9') {
					string.push_back(static_cast<char>(peek()));
					advance();
				} else if ((peek() == '+' || peek() == '-')) {
					string.push_back(static_cast<char>(peek()));
					advance();
					if (!hasReachedEnd() && peek() >= '0' && peek() <= '9') {
						string.push_back(static_cast<char>(peek()));
						advance();
					} else {
						throw json::Error{"Missing exponent in E notation.", source};
					}
				}
			} else if (                                                                    //
				(type == TokenType::NUMBER_BINARY && (peek() == '0' || peek() == '1')) ||  //
				(type == TokenType::NUMBER_OCTAL && (peek() >= '0' && peek() <= '7')) ||   //
				(type == TokenType::NUMBER_DECIMAL && (peek() >= '0' && peek() <= '9')) || //
				(type == TokenType::NUMBER_HEXADECIMAL && ((peek() >= '0' && peek() <= '9') || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F')))) {
				string.push_back(static_cast<char>(peek()));
				advance();
			} else if (peek() == '_') {
				advance();
			} else {
				break;
			}
		}
		if (!hasReachedEnd()) {
			if (!isWhitespace(peek()) && !isPunctuationCharacter(peek()) && peek() != '\"' && peek() != '\'' && peek() != '/') {
				throw json::Error{"Invalid character after number.", source};
			}
		}
		return {.string = std::move(string), .source = numberSource, .type = type};
	}

	[[nodiscard]] Token scanIdentifier() {
		StringBase<Allocator> string{allocator};
		const SourceLocation identifierSource = source;
		do {
			if (!unicode::isValidCodePoint(peek())) {
				throw json::Error{"Invalid UTF-8.", source};
			}
			const InplaceBuffer<char8_t, 4> codePointUTF8 = unicode::encodeUTF8FromCodePoint(peek());
			string.append(StringView{std::launder(reinterpret_cast<const char*>(codePointUTF8.data())), codePointUTF8.size()});
			advance();
		} while (!hasReachedEnd() && !isWhitespace(peek()) && !isPunctuationCharacter(peek()) && peek() != '\"' && peek() != '\'' && peek() != '/');
		TokenType type = TokenType::IDENTIFIER_NAME;
		if (string == "null") {
			string = {};
			type = TokenType::IDENTIFIER_NULL;
		} else if (string == "false") {
			string = {};
			type = TokenType::IDENTIFIER_FALSE;
		} else if (string == "true") {
			string = {};
			type = TokenType::IDENTIFIER_TRUE;
		} else if (string == "Infinity") {
			string = {};
			type = TokenType::NUMBER_POSITIVE_INFINITY;
		} else if (string == "NaN") {
			string = {};
			type = TokenType::NUMBER_POSITIVE_NAN;
		}
		return {.string = std::move(string), .source = identifierSource, .type = type};
	}

	void scanNumericEscapeSequence(StringBase<Allocator>& output, size_t minDigitCount, size_t maxDigitCount, int radix, bool (*isDigit)(char32_t) noexcept) {
		const SourceLocation escapeSequenceSource = source;
		StringBase<Allocator> digits{allocator};
		digits.reserve(maxDigitCount);
		while (digits.size() < maxDigitCount && !hasReachedEnd() && isDigit(peek())) {
			digits.push_back(static_cast<char>(peek()));
			advance();
		}
		if (digits.size() < minDigitCount) {
			throw json::Error{"Invalid escape sequence length.", escapeSequenceSource};
		}
		const char* const digitsBegin = digits.data();
		const char* const digitsEnd = digitsBegin + digits.size();
		uint32_t codePointValue = 0;
		if (const std::from_chars_result parseResult = std::from_chars(digitsBegin, digitsEnd, codePointValue, radix);
			parseResult.ec != std::errc{} || parseResult.ptr != digitsEnd || !unicode::isValidCodePoint(static_cast<char32_t>(codePointValue))) {
			throw json::Error{"Invalid code point value.", escapeSequenceSource};
		}
		const InplaceBuffer<char8_t, 4> codePointUTF8 = unicode::encodeUTF8FromCodePoint(static_cast<char32_t>(codePointValue));
		output.append(StringView{std::launder(reinterpret_cast<const char*>(codePointUTF8.data())), codePointUTF8.size()});
	}

	mutable unicode::UTF8Iterator<It> it;
	unicode::UTF8Sentinel end;
	SourceLocation source;
	mutable Optional<char32_t> currentCodePoint{};
	[[no_unique_address]] allocator_type allocator;
};

/**
 * Syntactic analyzer for parsing input in the JSON5 format obtained from a
 * json::Lexer.
 *
 * \tparam It iterator type of the underlying input source. Must be an input
 *         iterator.
 * \tparam Allocator allocator type template to use.
 */
template <typename It, template <typename> typename Allocator = std::allocator>
class Parser {
public:
	/** Allocator type used by the parser. */
	using allocator_type = Allocator<ValueBase<Allocator>>;

	/** Token type of the parser. */
	using Token = TokenBase<Allocator>;

	/**
	 * Polymorphic interface for visitation-based parsing of JSON values.
	 */
	class ValueVisitor {
	public:
		/**
		 * Callback for values of type Null.
		 *
		 * \param source location of the parsed value.
		 * \param value parsed value.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitNull(const SourceLocation& source, Null value) {
			(void)value;
			throw json::Error{"Unexpected null.", source};
		}

		/**
		 * Callback for values of type Boolean.
		 *
		 * \param source location of the parsed value.
		 * \param value parsed value.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitBoolean(const SourceLocation& source, Boolean value) {
			(void)value;
			throw json::Error{"Unexpected boolean.", source};
		}

		/**
		 * Callback for values of type String.
		 *
		 * \param source location of the parsed value.
		 * \param value parsed value.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitString(const SourceLocation& source, StringBase<Allocator>&& value) {
			(void)std::move(value);
			throw json::Error{"Unexpected string.", source};
		}

		/**
		 * Callback for values of type Number.
		 *
		 * \param source location of the parsed value.
		 * \param value parsed value.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitNumber(const SourceLocation& source, Number value) {
			(void)value;
			throw json::Error{"Unexpected number.", source};
		}

		/**
		 * Callback for objects.
		 *
		 * \param source location of the beginning of the encountered object.
		 * \param parser parser that should be used to parse the object.
		 *
		 * \warning Implementations must advance the parser to the end of the
		 *          encountered object, past the last closing curly brace.
		 * \warning Implementations must not advance the parser past the end of
		 *          the encountered object.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitObject(const SourceLocation& source, Parser& parser) {
			(void)parser;
			throw json::Error{"Unexpected object.", source};
		}

		/**
		 * Callback for arrays.
		 *
		 * \param source location of the beginning of the encountered array.
		 * \param parser parser that should be used to parse the array.
		 *
		 * \warning Implementations must advance the parser to the end of the
		 *          encountered array, past the last closing square bracket.
		 * \warning Implementations must not advance the parser past the end of
		 *          the encountered array.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitArray(const SourceLocation& source, Parser& parser) {
			(void)parser;
			throw json::Error{"Unexpected array.", source};
		}

	protected:
		~ValueVisitor() = default;
	};

	/**
	 * Implementation of ValueVisitor for freestanding classes that implement
	 * all or parts of its interface without directly inheriting from it.
	 *
	 * \tparam Visitor freestanding value visitor type to adapt.
	 */
	template <typename Visitor>
	struct ConcreteValueVisitor final : ValueVisitor {
		Visitor visitor;

		ConcreteValueVisitor(Visitor visitor)
			: visitor(std::move(visitor)) {}

		void visitNull(const SourceLocation& source, Null value) override {
			if constexpr (requires { visitor.visitNull(source, value); }) {
				visitor.visitNull(source, value);
			} else {
				ValueVisitor::visitNull(source, value);
			}
		}

		void visitBoolean(const SourceLocation& source, Boolean value) override {
			if constexpr (requires { visitor.visitBoolean(source, value); }) {
				visitor.visitBoolean(source, value);
			} else {
				ValueVisitor::visitBoolean(source, value);
			}
		}

		void visitString(const SourceLocation& source, StringBase<Allocator>&& value) override {
			if constexpr (requires { visitor.visitString(source, std::move(value)); }) {
				visitor.visitString(source, std::move(value));
			} else {
				ValueVisitor::visitString(source, std::move(value));
			}
		}

		void visitNumber(const SourceLocation& source, Number value) override {
			if constexpr (requires { visitor.visitNumber(source, value); }) {
				visitor.visitNumber(source, value);
			} else {
				ValueVisitor::visitNumber(source, value);
			}
		}

		void visitObject(const SourceLocation& source, Parser& parser) override {
			if constexpr (requires { visitor.visitObject(source, parser); }) {
				visitor.visitObject(source, parser);
			} else {
				ValueVisitor::visitObject(source, parser);
			}
		}

		void visitArray(const SourceLocation& source, Parser& parser) override {
			if constexpr (requires { visitor.visitArray(source, parser); }) {
				visitor.visitArray(source, parser);
			} else {
				ValueVisitor::visitArray(source, parser);
			}
		}
	};

	/**
	 * Polymorphic interface for visitation-based parsing of JSON object
	 * properties.
	 */
	class PropertyVisitor {
	public:
		/**
		 * Callback for each object property.
		 *
		 * \param source location of the beginning of the property's value.
		 * \param key the property's name string.
		 * \param parser parser that should be used to parse the property's
		 *        value.
		 *
		 * \warning Implementations must advance the parser to the end of the
		 *          encountered value.
		 * \warning Implementations must not advance the parser past the end of
		 *          the property's value.
		 *
		 * \throws json::Error on invalid input.
		 * \throws any exception thrown by the concrete implementation.
		 */
		virtual void visitProperty(const SourceLocation& source, StringBase<Allocator>&& key, Parser& parser) = 0;

	protected:
		~PropertyVisitor() = default;
	};

	/**
	 * Implementation of PropertyVisitor for freestanding classes that implement
	 * all or parts of its interface without directly inheriting from it.
	 *
	 * \tparam Visitor freestanding property visitor type to adapt.
	 */
	template <typename Visitor>
	struct ConcretePropertyVisitor final : PropertyVisitor {
		Visitor visitor;

		ConcretePropertyVisitor(Visitor visitor)
			: visitor(std::move(visitor)) {}

		void visitProperty(const SourceLocation& source, StringBase<Allocator>&& key, Parser& parser) override {
			if constexpr (requires { visitor.visitProperty(source, std::move(key), parser); }) {
				visitor.visitProperty(source, std::move(key), parser);
			}
		}
	};

	/**
	 * Implementation of ValueVisitor that skips over the parsed value and
	 * discards the result.
	 */
	struct SkipValueVisitor final : ValueVisitor {
		// clang-format off
		void visitNull(const SourceLocation& source, Null value) override { (void)source; (void)value; }
		void visitBoolean(const SourceLocation& source, Boolean value) override { (void)source; (void)value; }
		void visitString(const SourceLocation& source, StringBase<Allocator>&& value) override { (void)source; (void)std::move(value); }
		void visitNumber(const SourceLocation& source, Number value) override { (void)source; (void)value; }
		void visitObject(const SourceLocation& source, Parser& parser) override { (void)source; parser.parseObject(SkipPropertyVisitor{}); }
		void visitArray(const SourceLocation& source, Parser& parser) override { (void)source; parser.parseArray(SkipValueVisitor{}); }
		// clang-format on
	};

	/**
	 * Implementation of PropertyVisitor that skips over the parsed property and
	 * discards the result.
	 */
	struct SkipPropertyVisitor final : PropertyVisitor {
		// clang-format off
		void visitProperty(const SourceLocation& source, StringBase<Allocator>&& key, Parser& parser) override { (void)source; (void)std::move(key); parser.parseValue(SkipValueVisitor{}); }
		// clang-format on
	};

	/**
	 * Construct a parser with an existing lexer as input.
	 *
	 * \param lexer lexer to scan JSON tokens from.
	 * \param allocator allocator to use.
	 */
	explicit Parser(Lexer<It, Allocator> lexer, const allocator_type& allocator = allocator_type())
		: lexer(std::move(lexer), allocator) {}

	/**
	 * Construct a parser with a contiguous UTF-8 view as input.
	 *
	 * \param codePoints non-owning read-only view over the UTF-8 string to
	 *        parse JSON tokens from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 */
	explicit Parser(unicode::UTF8View codePoints, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type()) requires(same_as<It, const char8_t*>)
		: Parser(Lexer<It, Allocator>{codePoints.begin(), codePoints.end(), source, allocator}) {}

	/**
	 * Construct a parser with a contiguous UTF-8 string as input.
	 *
	 * \param jsonString non-owning read-only view over the UTF-8 string to
	 *        parse JSON tokens from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 */
	explicit Parser(UTF8StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type()) requires(same_as<It, const char8_t*>)
		: Parser(unicode::UTF8View{jsonString}, source, allocator) {}

	/**
	 * Construct a parser with a contiguous string, interpreted as UTF-8, as
	 * input.
	 *
	 * \param jsonString non-owning read-only view over the string to parse JSON
	 *        tokens from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 */
	explicit Parser(StringView jsonString, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type()) requires(same_as<It, const char8_t*>)
		: Parser(unicode::UTF8View{jsonString}, source, allocator) {}

	/**
	 * Construct a parser with an input stream as input.
	 *
	 * \param stream input stream to parse JSON tokens from.
	 * \param source source location corresponding to the start of the given
	 *        stream.
	 * \param allocator allocator to use.
	 */
	explicit Parser(std::istream& stream, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type())
		requires(same_as<It, std::istreambuf_iterator<char>>)
		: Parser(Lexer<It, Allocator>{unicode::UTF8Iterator<It>{It{stream}, It{}}, unicode::UTF8Sentinel{}, source, allocator}) {}

	/**
	 * Construct a parser with an input stream buffer as input.
	 *
	 * \param streambuf input stream buffer to parse JSON tokens from.
	 * \param source source location corresponding to the start of the given
	 *        JSON string.
	 * \param allocator allocator to use.
	 */
	explicit Parser(std::streambuf* streambuf, const SourceLocation& source = {}, const allocator_type& allocator = allocator_type())
		requires(same_as<It, std::istreambuf_iterator<char>>)
		: Parser(Lexer<It, Allocator>{unicode::UTF8Iterator<It>{It{streambuf}, It{}}, unicode::UTF8Sentinel{}, source, allocator}) {}

	/** Copy constructor. */
	Parser(const Parser& other, const allocator_type& allocator)
		: lexer(other.lexer, allocator) {
		if (other.currentToken) {
			currentToken = Token{
				.string{other.currentToken->string, allocator},
				.source = other.currentToken->source,
				.type = other.currentToken->type,
			};
		}
	}

	/** Move constructor. */
	Parser(Parser&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		: lexer(std::move(other.lexer), allocator) {
		if (other.currentToken) {
			currentToken = Token{
				.string{std::move(other.currentToken->string), allocator},
				.source = other.currentToken->source,
				.type = other.currentToken->type,
			};
		}
	}

	/** Destructor. */
	~Parser() = default;

	/** Copy constructor. */
	Parser(const Parser& other)
		: Parser(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

	/** Move constructor. */
	Parser(Parser&& other) noexcept
		: Parser(std::move(other), other.get_allocator()) {}

	/** Copy assignment. */
	Parser& operator=(const Parser&) = default;

	/** Move assignment. */
	Parser& operator=(Parser&&) noexcept = default;

	/**
	 * Get the allocator used by the parser.
	 *
	 * \return a copy of the parser's allocator.
	 */
	[[nodiscard]] allocator_type get_allocator() const noexcept {
		return lexer.get_allocator();
	}

	/**
	 * Read a single JSON value from the input and visit it, then make sure the
	 * rest of the input only consists of whitespace.
	 *
	 * \param visitor visitor to give the parsed value to.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the visitor.
	 *
	 * \sa json::onNull()
	 * \sa json::onBoolean()
	 * \sa json::onString()
	 * \sa json::onNumber()
	 * \sa json::onObject()
	 * \sa json::onArray()
	 * \sa parseFile()
	 * \sa parseValue(ValueVisitor&)
	 */
	void parseFile(ValueVisitor& visitor) {
		parseValue(visitor);
		if (const Token& token = peek(); token.type != TokenType::END_OF_FILE) {
			throw json::Error{"Multiple top-level values.", token.source};
		}
	}

	/**
	 * Read a single JSON value from the input and visit it.
	 *
	 * \param visitor visitor to give the parsed value to.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the visitor.
	 *
	 * \sa json::onNull()
	 * \sa json::onBoolean()
	 * \sa json::onString()
	 * \sa json::onNumber()
	 * \sa json::onObject()
	 * \sa json::onArray()
	 * \sa parseValue()
	 * \sa parseFile(ValueVisitor&)
	 */
	void parseValue(ValueVisitor& visitor) {
		switch (const Token& token = peek(); token.type) {
			case TokenType::END_OF_FILE: throw json::Error{"Expected a value.", token.source};
			case TokenType::IDENTIFIER_NULL:
				advance();
				visitor.visitNull(token.source, Null{});
				break;
			case TokenType::IDENTIFIER_FALSE:
				advance();
				visitor.visitBoolean(token.source, Boolean{false});
				break;
			case TokenType::IDENTIFIER_TRUE:
				advance();
				visitor.visitBoolean(token.source, Boolean{true});
				break;
			case TokenType::IDENTIFIER_NAME: throw json::Error{"Unexpected name identifier.", token.source};
			case TokenType::PUNCTUATOR_COMMA: throw json::Error{"Unexpected comma.", token.source};
			case TokenType::PUNCTUATOR_COLON: throw json::Error{"Unexpected colon.", token.source};
			case TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET: {
				const SourceLocation source = token.source;
				visitor.visitArray(source, *this);
				break;
			}
			case TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET: throw json::Error{"Unexpected closing bracket.", token.source};
			case TokenType::PUNCTUATOR_OPEN_CURLY_BRACE: {
				const SourceLocation source = token.source;
				visitor.visitObject(source, *this);
				break;
			}
			case TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE: throw json::Error{"Unexpected closing brace.", token.source};
			case TokenType::STRING: visitor.visitString(token.source, std::move(eat().string)); break;
			case TokenType::NUMBER_BINARY: visitor.visitNumber(token.source, parseNumberContents(eat(), 2)); break;
			case TokenType::NUMBER_OCTAL: visitor.visitNumber(token.source, parseNumberContents(eat(), 8)); break;
			case TokenType::NUMBER_DECIMAL: visitor.visitNumber(token.source, parseNumberContents(eat(), 10)); break;
			case TokenType::NUMBER_HEXADECIMAL: visitor.visitNumber(token.source, parseNumberContents(eat(), 16)); break;
			case TokenType::NUMBER_POSITIVE_INFINITY:
				advance();
				visitor.visitNumber(token.source, Number{Limits<Number>::INF});
				break;
			case TokenType::NUMBER_NEGATIVE_INFINITY:
				advance();
				visitor.visitNumber(token.source, Number{-Limits<Number>::INF});
				break;
			case TokenType::NUMBER_POSITIVE_NAN:
				advance();
				visitor.visitNumber(token.source, Number{Limits<Number>::QUIET_NAN});
				break;
			case TokenType::NUMBER_NEGATIVE_NAN:
				advance();
				visitor.visitNumber(token.source, Number{-Limits<Number>::QUIET_NAN});
				break;
		}
	}

	/**
	 * Read a single JSON object from the input and visit each of its
	 * properties.
	 *
	 * \param visitor visitor to give each parsed property of the object to.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the visitor.
	 *
	 * \sa json::onProperty()
	 * \sa parseObject()
	 */
	void parseObject(PropertyVisitor& visitor) {
		if (const Token& token = peek(); token.type != TokenType::PUNCTUATOR_OPEN_CURLY_BRACE) {
			throw json::Error{"Expected an object.", token.source};
		}
		advance();
		while (true) {
			StringBase<Allocator> key{get_allocator()};
			switch (const Token& token = peek(); token.type) {
				case TokenType::END_OF_FILE: throw json::Error{"Missing end of object.", token.source};
				case TokenType::IDENTIFIER_NULL: throw json::Error{"Unexpected null.", token.source};
				case TokenType::IDENTIFIER_FALSE: throw json::Error{"Unexpected false.", token.source};
				case TokenType::IDENTIFIER_TRUE: throw json::Error{"Unexpected true.", token.source};
				case TokenType::IDENTIFIER_NAME: [[fallthrough]];
				case TokenType::STRING: key = std::move(eat().string); break;
				case TokenType::PUNCTUATOR_COMMA: [[fallthrough]];
				case TokenType::PUNCTUATOR_COLON: [[fallthrough]];
				case TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET: [[fallthrough]];
				case TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET: [[fallthrough]];
				case TokenType::PUNCTUATOR_OPEN_CURLY_BRACE: throw json::Error{"Unexpected punctuator.", token.source};
				case TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE: advance(); return;
				case TokenType::NUMBER_BINARY: [[fallthrough]];
				case TokenType::NUMBER_OCTAL: [[fallthrough]];
				case TokenType::NUMBER_DECIMAL: [[fallthrough]];
				case TokenType::NUMBER_HEXADECIMAL: [[fallthrough]];
				case TokenType::NUMBER_POSITIVE_INFINITY: [[fallthrough]];
				case TokenType::NUMBER_NEGATIVE_INFINITY: [[fallthrough]];
				case TokenType::NUMBER_POSITIVE_NAN: [[fallthrough]];
				case TokenType::NUMBER_NEGATIVE_NAN: throw json::Error{"Unexpected number.", token.source};
			}
			if (const Token token = eat(); token.type != TokenType::PUNCTUATOR_COLON) {
				throw json::Error{"Expected a colon.", token.source};
			}
			const SourceLocation source = peek().source;
			visitor.visitProperty(source, std::move(key), *this);
			if (peek().source == source) {
				skipValue();
			}
			if (const Token& token = peek(); token.type == TokenType::PUNCTUATOR_COMMA) {
				advance();
			} else if (token.type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
				advance();
				break;
			} else {
				throw json::Error{"Expected a comma or closing brace.", token.source};
			}
		}
	}

	/**
	 * Read a single JSON array from the input and visit each of its values.
	 *
	 * \param visitor visitor to give each parsed value of the array to.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the visitor.
	 *
	 * \sa json::onNull()
	 * \sa json::onBoolean()
	 * \sa json::onString()
	 * \sa json::onNumber()
	 * \sa json::onObject()
	 * \sa json::onArray()
	 * \sa parseArray()
	 */
	void parseArray(ValueVisitor& visitor) {
		if (const Token& token = peek(); token.type != TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET) {
			throw json::Error{"Expected an array.", token.source};
		}
		advance();
		while (true) {
			if (peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
				advance();
				return;
			}
			parseValue(visitor);
			if (const Token& token = peek(); token.type == TokenType::PUNCTUATOR_COMMA) {
				advance();
			} else if (token.type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
				advance();
				break;
			} else {
				throw json::Error{"Expected a comma or closing bracket.", token.source};
			}
		}
	}

	/**
	 * \sa parseFile(ValueVisitor&)
	 */
	void parseFile(ValueVisitor&& visitor) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		parseFile(visitor);
	}

	/**
	 * \sa parseValue(ValueVisitor&)
	 */
	void parseValue(ValueVisitor&& visitor) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		parseValue(visitor);
	}

	/**
	 * \sa parseObject(PropertyVisitor&)
	 */
	void parseObject(PropertyVisitor&& visitor) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		parseObject(visitor);
	}

	/**
	 * \sa parseArray(ValueVisitor&)
	 */
	void parseArray(ValueVisitor&& visitor) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
		parseArray(visitor);
	}

	/**
	 * \sa parseFile(ValueVisitor&)
	 */
	template <typename Visitor>
	void parseFile(Visitor visitor) {
		parseFile(static_cast<ValueVisitor&&>(ConcreteValueVisitor<Visitor>{std::move(visitor)}));
	}

	/**
	 * \sa parseValue(ValueVisitor&)
	 */
	template <typename Visitor>
	void parseValue(Visitor visitor) {
		parseValue(static_cast<ValueVisitor&&>(ConcreteValueVisitor<Visitor>{std::move(visitor)}));
	}

	/**
	 * \sa parseObject(PropertyVisitor&)
	 */
	template <typename Visitor>
	void parseObject(Visitor visitor) {
		parseObject(static_cast<PropertyVisitor&&>(ConcretePropertyVisitor<Visitor>{std::move(visitor)}));
	}

	/**
	 * \sa parseArray(ValueVisitor&)
	 */
	template <typename Visitor>
	void parseArray(Visitor visitor) {
		parseArray(static_cast<ValueVisitor&&>(ConcreteValueVisitor<Visitor>{std::move(visitor)}));
	}

	/**
	 * Parse a single JSON value from the input and discard the result, then
	 * make sure the rest of the input only consists of whitespace.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseFile()
	 * \sa skipValue()
	 */
	void skipFile() {
		parseFile(SkipValueVisitor{});
	}

	/**
	 * Parse a single JSON value from the input and discard the result.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue()
	 * \sa skipFile()
	 */
	void skipValue() {
		parseValue(SkipValueVisitor{});
	}

	/**
	 * Read a single JSON value from the input and make sure the rest of the
	 * input only consists of whitespace.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseFile(ValueVisitor&)
	 * \sa parseValue()
	 * \sa skipFile()
	 */
	ValueBase<Allocator> parseFile() {
		ValueBase<Allocator> result = parseValue();
		if (const Token& token = peek(); token.type != TokenType::END_OF_FILE) {
			throw json::Error{"Multiple top-level values.", token.source};
		}
		return result;
	}

	/**
	 * Read a single JSON value from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue(ValueVisitor&)
	 * \sa parseFile()
	 * \sa parseNull()
	 * \sa parseBoolean()
	 * \sa parseString()
	 * \sa parseNumber()
	 * \sa parseObject()
	 * \sa parseArray()
	 * \sa skipValue()
	 */
	ValueBase<Allocator> parseValue() {
		struct Visitor final : ValueVisitor {
			Optional<ValueBase<Allocator>>& result;
			allocator_type allocator;

			Visitor(Optional<ValueBase<Allocator>>& result, const allocator_type& allocator)
				: result(result)
				, allocator(allocator) {}

			// clang-format off
			void visitNull(const SourceLocation& source, Null value) override { result.emplace(VariantBase<Allocator>{value, allocator}, source); }
			void visitBoolean(const SourceLocation& source, Boolean value) override { result.emplace(VariantBase<Allocator>{value, allocator}, source); }
			void visitString(const SourceLocation& source, StringBase<Allocator>&& value) override { result.emplace(VariantBase<Allocator>{std::move(value), allocator}, source); }
			void visitNumber(const SourceLocation& source, Number value) override { result.emplace(VariantBase<Allocator>{value, allocator}, source); }
			void visitObject(const SourceLocation& source, Parser& parser) override { result.emplace(VariantBase<Allocator>{parser.parseObject(), allocator}, source); }
			void visitArray(const SourceLocation& source, Parser& parser) override { result.emplace(VariantBase<Allocator>{parser.parseArray(), allocator}, source); }
			// clang-format on
		};
		Optional<ValueBase<Allocator>> result{};
		parseValue(Visitor{result, get_allocator()});
		return std::move(*result);
	}

	/**
	 * Read a single JSON value of type Null from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue()
	 * \sa peekIsNull()
	 */
	Null parseNull() {
		const Token token = eat();
		switch (token.type) {
			case TokenType::IDENTIFIER_NULL: return Null{};
			default: break;
		}
		throw json::Error{"Expected null.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type Null.
	 *
	 * \return true if the parser is at a value of type Null, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseNull()
	 */
	[[nodiscard]] bool peekIsNull() const {
		return peek().type == TokenType::IDENTIFIER_NULL;
	}

	/**
	 * Read a single JSON value of type Boolean from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue()
	 * \sa peekIsBoolean()
	 */
	Boolean parseBoolean() {
		const Token token = eat();
		switch (token.type) {
			case TokenType::IDENTIFIER_FALSE: return Boolean{false};
			case TokenType::IDENTIFIER_TRUE: return Boolean{true};
			default: break;
		}
		throw json::Error{"Expected a boolean.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type Boolean.
	 *
	 * \return true if the parser is at a value of type Boolean, false
	 *         otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseBoolean()
	 */
	[[nodiscard]] bool peekIsBoolean() const {
		switch (peek().type) {
			case TokenType::IDENTIFIER_FALSE: [[fallthrough]];
			case TokenType::IDENTIFIER_TRUE: return true;
			default: break;
		}
		return false;
	}

	/**
	 * Read a single JSON value of type String from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue()
	 * \sa peekIsString()
	 */
	StringBase<Allocator> parseString() {
		Token token = eat();
		switch (token.type) {
			case TokenType::STRING: return std::move(token.string);
			default: break;
		}
		throw json::Error{"Expected a string.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type String.
	 *
	 * \return true if the parser is at a value of type String, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseString()
	 */
	[[nodiscard]] bool peekIsString() const {
		return peek().type == TokenType::STRING;
	}

	/**
	 * Read a single JSON value of type Number from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseValue()
	 * \sa peekIsNumber()
	 */
	Number parseNumber() {
		const Token token = eat();
		switch (token.type) {
			case TokenType::NUMBER_BINARY: return parseNumberContents(token, 2);
			case TokenType::NUMBER_OCTAL: return parseNumberContents(token, 8);
			case TokenType::NUMBER_DECIMAL: return parseNumberContents(token, 10);
			case TokenType::NUMBER_HEXADECIMAL: return parseNumberContents(token, 16);
			case TokenType::NUMBER_POSITIVE_INFINITY: return Limits<Number>::INF;
			case TokenType::NUMBER_NEGATIVE_INFINITY: return -Limits<Number>::INF;
			case TokenType::NUMBER_POSITIVE_NAN: return Limits<Number>::QUIET_NAN;
			case TokenType::NUMBER_NEGATIVE_NAN: return -Limits<Number>::QUIET_NAN;
			default: break;
		}
		throw json::Error{"Expected a number.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type Number.
	 *
	 * \return true if the parser is at a value of type Number, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseNumber()
	 */
	[[nodiscard]] bool peekIsNumber() const {
		switch (peek().type) {
			case TokenType::NUMBER_BINARY: [[fallthrough]];
			case TokenType::NUMBER_OCTAL: [[fallthrough]];
			case TokenType::NUMBER_DECIMAL: [[fallthrough]];
			case TokenType::NUMBER_HEXADECIMAL: [[fallthrough]];
			case TokenType::NUMBER_POSITIVE_INFINITY: [[fallthrough]];
			case TokenType::NUMBER_NEGATIVE_INFINITY: [[fallthrough]];
			case TokenType::NUMBER_POSITIVE_NAN: [[fallthrough]];
			case TokenType::NUMBER_NEGATIVE_NAN: return true;
			default: break;
		}
		return false;
	}

	/**
	 * Read a single JSON value of type Object from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseObject(PropertyVisitor&)
	 * \sa parseValue()
	 * \sa peekIsObject()
	 */
	ObjectBase<Allocator> parseObject() {
		const Token& token = peek();
		switch (token.type) {
			case TokenType::PUNCTUATOR_OPEN_CURLY_BRACE: {
				struct Visitor final : PropertyVisitor {
					ObjectBase<Allocator>& result;

					explicit Visitor(ObjectBase<Allocator>& result) noexcept
						: result(result) {}

					void visitProperty(const SourceLocation&, StringBase<Allocator>&& key, Parser& parser) override {
						result.emplace(std::move(key), parser.parseValue());
					}
				};
				ObjectBase<Allocator> result{get_allocator()};
				parseObject(Visitor{result});
				return result;
			}
			default: break;
		}
		throw json::Error{"Expected an object.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type Object.
	 *
	 * \return true if the parser is at a value of type Object, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseObject()
	 */
	[[nodiscard]] bool peekIsObject() const {
		return peek().type == TokenType::PUNCTUATOR_OPEN_CURLY_BRACE;
	}

	/**
	 * Read a single JSON value of type Array from the input.
	 *
	 * \return the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseArray(ValueVisitor&)
	 * \sa parseValue()
	 * \sa peekIsArray()
	 */
	ArrayBase<Allocator> parseArray() {
		const Token& token = peek();
		switch (token.type) {
			case TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET: {
				struct Visitor final : ValueVisitor {
					ArrayBase<Allocator>& result;

					explicit Visitor(ArrayBase<Allocator>& result)
						: result(result) {}

					// clang-format off
					void visitNull(const SourceLocation& source, Null value) override { result.emplace_back(VariantBase<Allocator>{value, result.get_allocator()}, source); }
					void visitBoolean(const SourceLocation& source, Boolean value) override { result.emplace_back(VariantBase<Allocator>{value, result.get_allocator()}, source); }
					void visitString(const SourceLocation& source, StringBase<Allocator>&& value) override { result.emplace_back(VariantBase<Allocator>{std::move(value)}, source); }
					void visitNumber(const SourceLocation& source, Number value) override { result.emplace_back(VariantBase<Allocator>{value, result.get_allocator()}, source); }
					void visitObject(const SourceLocation& source, Parser& parser) override { result.emplace_back(VariantBase<Allocator>{parser.parseObject()}, source); }
					void visitArray(const SourceLocation& source, Parser& parser) override { result.emplace_back(VariantBase<Allocator>{parser.parseArray()}, source); }
					// clang-format on
				};
				ArrayBase<Allocator> result{get_allocator()};
				parseArray(Visitor{result});
				return result;
			}
			default: break;
		}
		throw json::Error{"Expected an array.", token.source};
	}

	/**
	 * Check if the next immediate input is a JSON value of type Array.
	 *
	 * \return true if the parser is at a value of type Array, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \sa parseArray()
	 */
	[[nodiscard]] bool peekIsArray() const {
		return peek().type == TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET;
	}

	/**
	 * Advance the internal state of the underlying lexer by one token.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 */
	void advance() {
		if (!currentToken) {
			lexer.scan();
		}
		currentToken.reset();
	}

	/**
	 * Peek the next token without advancing the internal state of the
	 * underlying lexer.
	 *
	 * \return a read-only reference to the next token to be read that is valid
	 *         until the next call to advance() or eat(), or until the parser is
	 *         moved from or destroyed, whichever happens first.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 *
	 * \warning Although it is semantically const, this function is not
	 *          thread-safe since it mutates an internal lookahead buffer.
	 *          Exclusive access is therefore required for safety.
	 */
	[[nodiscard]] const Token& peek() const {
		if (!currentToken) {
			currentToken = lexer.scan();
		}
		return *currentToken;
	}

	/**
	 * Scan and consume the next token from the input.
	 *
	 * This advances the internal state of the underlying lexer.
	 *
	 * \return the scanned token.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 */
	[[nodiscard]] Token eat() {
		if (!currentToken) {
			currentToken = lexer.scan();
		}
		Token result = std::move(*currentToken);
		currentToken.reset();
		return result;
	}

private:
	[[nodiscard]] static Number parseNumberContents(const Token& token, int radix) {
		const char* numberStringBegin = token.string.c_str();
		const char* const numberStringEnd = token.string.data() + token.string.size();
		char* endPointer = const_cast<char*>(numberStringEnd);
		if (radix == 10) {
			const double numberValue = std::strtod(numberStringBegin, &endPointer);
			if (endPointer != numberStringEnd) {
				throw json::Error{"Invalid number.", token.source};
			}
			return Number{numberValue};
		}
		bool negative = false;
		if (!token.string.empty() && token.string.front() == '-') {
			negative = true;
			++numberStringBegin;
		}
		const unsigned long long integerNumberValue = std::strtoull(numberStringBegin, &endPointer, radix);
		if (endPointer != numberStringEnd) {
			throw json::Error{"Invalid number.", token.source};
		}
		const double numberValue = static_cast<double>(integerNumberValue);
		return Number{(negative) ? -numberValue : numberValue};
	}

	mutable Lexer<It, Allocator> lexer;
	mutable Optional<Token> currentToken{};
};

/**
 * Parser for reading contiguous UTF-8-encoded JSON strings.
 */
template <template <typename> typename Allocator>
using StringParserBase = Parser<const char8_t*, Allocator>;

/**
 * Parser for reading contiguous UTF-8-encoded JSON strings using the default
 * allocator.
 */
using StringParser = StringParserBase<std::allocator>;

/**
 * Parser for reading UTF-8-encoded JSON input stream buffers.
 */
template <template <typename> typename Allocator>
using StreamParserBase = Parser<std::istreambuf_iterator<char>, Allocator>;

/**
 * Parser for reading UTF-8-encoded JSON input stream buffers using the default
 * allocator.
 */
using StreamParser = StreamParserBase<std::allocator>;

namespace detail {

template <typename T>
[[nodiscard]] size_t getRecursiveSize(const T& value);

} // namespace detail

/**
 * Base template to specialize in order to implement JSON serialization for a
 * specific type.
 *
 * The specialization should have a member function with one of the following
 * signatures:
 * ```
 * void serializeTo(Writer& writer, const T& value)
 * void serializeTo(Writer& writer, T value)
 * ```
 * where T is the specialized template type parameter.
 *
 * The implementation should write the output using the provided Writer.
 *
 * \tparam T the type that this serializer serializes.
 */
template <typename T>
struct Serializer;

/**
 * Base template to specialize in order to implement JSON deserialization for a
 * specific type.
 *
 * The specialization should have a member function with the following
 * signature:
 * ```
 * void deserializeFrom(Reader& reader, T& value)
 * ```
 * where T is the specialized template type parameter.
 *
 * The implementation should read the input using the provided Reader.
 *
 * \tparam T the type that this deserializer deserializes.
 */
template <typename T>
struct Deserializer;

/**
 * Serialize a value of any JSON-serializable type to an output stream.
 *
 * \param stream stream to write the output to.
 * \param value value to serialize.
 * \param options serialization options, see SerializationOptions.
 * \param serializerOverride overloaded function object that overrides the
 *        default serialization behavior for the value or its nested
 *        elements/fields whenever a call to `serializerOverride(writer, value)`
 *        yields a valid matching overload.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by a user-defined implementation of Serializer
 *         or SerializerOverride, if one is used in the serialization of the
 *         given value type.
 *
 * \note Serialization of user-defined types can be defined by implementing a
 *       specialization of the Serializer template.
 */
template <typename T, typename SerializerOverride = detail::NoSerializerOverride>
void serialize(std::ostream& stream, const T& value, const SerializationOptions& options = {}, SerializerOverride serializerOverride = {});

/**
 * Deserialize a value of any JSON-serializable type from an input stream.
 *
 * \param stream stream to read the input from.
 * \param value value to deserialize to.
 * \param options deserialization options, see DeserializationOptions.
 * \param deserializerOverride overloaded function object that overrides the
 *        default deserialization behavior for the value or its nested
 *        elements/fields whenever a call to
 *        `deserializerOverride(reader, value)` yields a valid matching
 *        overload.
 * \param source source location corresponding to the start of the given stream.
 *
 * \throws json::Error on failure to parse the value from the stream.
 * \throws std::ios_base::failure if thrown by the input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by a user-defined implementation of
 *         Deserializer or DeserializerOverride, if one is used in the
 *         deserialization of the given value type.
 *
 * \note Deserialization of user-defined types can be defined by implementing a
 *       specialization of the Deserializer template.
 */
template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride>
void deserialize(std::istream& stream, T& value, const DeserializationOptions& options = {}, DeserializerOverride deserializerOverride = {}, const SourceLocation& source = {});

/**
 * Write a JSON object to an output stream using the default serialization
 * options.
 *
 * \param stream stream to write the output to.
 * \param value the JSON value to serialize.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::ostream& operator<<(std::ostream& stream, const ObjectBase<Allocator>& value) {
	json::serialize(stream, value);
	return stream;
}

/**
 * Read a JSON object from an input stream using the default deserialization
 * options.
 *
 * If the function fails to parse a JSON value from the stream,
 * std::ios_base::failbit is set on the stream, which may or may not
 * cause an exception to be thrown.
 *
 * \param stream stream to read the input from.
 * \param value the JSON value to deserialize to.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws std::ios_base::failure if thrown by the input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::istream& operator>>(std::istream& stream, ObjectBase<Allocator>& value) {
	try {
		json::deserialize(stream, value);
	} catch (const json::Error&) {
		stream.setstate(std::istream::failbit);
	}
	return stream;
}

/**
 * Write a JSON array to an output stream using the default serialization
 * options.
 *
 * \param stream stream to write the output to.
 * \param value the JSON value to serialize.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::ostream& operator<<(std::ostream& stream, const ArrayBase<Allocator>& value) {
	json::serialize(stream, value);
	return stream;
}

/**
 * Read a JSON array from an input stream using the default deserialization
 * options.
 *
 * If the function fails to parse a JSON value from the stream,
 * std::ios_base::failbit is set on the stream, which may or may not
 * cause an exception to be thrown.
 *
 * \param stream stream to read the input from.
 * \param value the JSON value to deserialize to.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws std::ios_base::failure if thrown by the input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::istream& operator>>(std::istream& stream, ArrayBase<Allocator>& value) {
	try {
		json::deserialize(stream, value);
	} catch (const json::Error&) {
		stream.setstate(std::istream::failbit);
	}
	return stream;
}

/**
 * Write a JSON value to an output stream using the default serialization
 * options.
 *
 * \param stream stream to write the output to.
 * \param value the JSON value to serialize.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::ostream& operator<<(std::ostream& stream, const VariantBase<Allocator>& value) {
	json::serialize(stream, value);
	return stream;
}

/**
 * Read a JSON value from an input stream using the default deserialization
 * options.
 *
 * If the function fails to parse a JSON value from the stream,
 * std::ios_base::failbit is set on the stream, which may or may not
 * cause an exception to be thrown.
 *
 * \param stream stream to read the input from.
 * \param value the JSON value to deserialize to.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws std::ios_base::failure if thrown by the input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::istream& operator>>(std::istream& stream, VariantBase<Allocator>& value) {
	try {
		json::deserialize(stream, value);
	} catch (const json::Error&) {
		stream.setstate(std::istream::failbit);
	}
	return stream;
}

/**
 * Write a JSON value to an output stream using the default serialization
 * options.
 *
 * \param stream stream to write the output to.
 * \param value the JSON value to serialize.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::ostream& operator<<(std::ostream& stream, const ValueBase<Allocator>& value) {
	json::serialize(stream, static_cast<const VariantBase<Allocator>&>(value));
	return stream;
}

/**
 * Read a JSON value from an input stream using the default deserialization
 * options.
 *
 * If the function fails to parse a JSON value from the stream,
 * std::ios_base::failbit is set on the stream, which may or may not
 * cause an exception to be thrown.
 *
 * \param stream stream to read the input from.
 * \param value the JSON value to deserialize to.
 *
 * \return a reference to the stream parameter, for chaining.
 *
 * \throws std::ios_base::failure if thrown by the input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
template <template <typename> typename Allocator>
inline std::istream& operator>>(std::istream& stream, ValueBase<Allocator>& value) {
	try {
		json::deserialize(stream, value);
	} catch (const json::Error&) {
		stream.setstate(std::istream::failbit);
	}
	return stream;
}

/**
 * Serialize a value of any JSON-serializable type to a string.
 *
 * \param value value to serialize.
 * \param options serialization options, see SerializationOptions.
 * \param serializerOverride overloaded function object that overrides the
 *        default serialization behavior for the value or its nested
 *        elements/fields whenever a call to `serializerOverride(writer, value)`
 *        yields a valid matching overload.
 * \param allocator allocator to use for the string.
 *
 * \return a string containing the serialized value.
 *
 * \throws json::Error on failure to serialize the value.
 * \throws std::ios_base::failure if thrown by the internal output stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by a user-defined implementation of Serializer
 *         or SerializerOverride, if one is used in the serialization of the
 *         given value type.
 *
 * \note Serialization of user-defined types can be defined by implementing a
 *       specialization of the Serializer template.
 */
template <typename T, typename SerializerOverride = detail::NoSerializerOverride, typename Allocator = std::allocator<char>>
inline grem::StringBase<char, std::char_traits<char>, Allocator> serializeToString(const T& value, const SerializationOptions& options = {},
	SerializerOverride serializerOverride = {}, const Allocator& allocator = Allocator()) {
	std::basic_ostringstream<char, std::char_traits<char>, Allocator> stream{std::ios_base::out, allocator};
	json::serialize(stream, value, options, serializerOverride);
	return std::move(stream).str();
}

/**
 * Deserialize a value of any JSON-serializable type from a string.
 *
 * \param string string to read from.
 * \param value value to deserialize to.
 * \param options deserialization options, see DeserializationOptions.
 * \param deserializerOverride overloaded function object that overrides the
 *        default deserialization behavior for the value or its nested
 *        elements/fields whenever a call to
 *        `deserializerOverride(reader, value)` yields a valid matching
 *        overload.
 * \param source source location corresponding to the start of the given JSON
 *        string.
 *
 * \throws json::Error on failure to deserialize the value.
 * \throws std::ios_base::failure if thrown by the internal input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by a user-defined implementation of
 *         Deserializer or DeserializerOverride, if one is used in the
 *         serialization of the given value type.
 *
 * \note Deserialization of user-defined types can be defined by implementing a
 *       specialization of the Deserializer template.
 */
template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride, typename Allocator = std::allocator<char>>
inline void deserializeFromString(grem::StringBase<char, std::char_traits<char>, Allocator> string, T& value, const DeserializationOptions& options = {},
	DeserializerOverride deserializerOverride = {}, const SourceLocation& source = {}) {
	std::basic_istringstream<char, std::char_traits<char>, Allocator> stream{std::move(string)}; // NOLINT(performance-move-const-arg)
	json::deserialize(stream, value, options, deserializerOverride, source);
}

/**
 * Deserialize a value of any JSON-serializable type from a string.
 *
 * \tparam T JSON-deserializable value type to deserialize.
 *
 * \param string string to read from.
 * \param options deserialization options, see DeserializationOptions.
 * \param deserializerOverride overloaded function object that overrides the
 *        default deserialization behavior for the value or its nested
 *        elements/fields whenever a call to
 *        `deserializerOverride(reader, value)` yields a valid matching
 *        overload.
 * \param source source location corresponding to the start of the given JSON
 *        string.
 *
 * \throws json::Error on failure to deserialize the value.
 * \throws std::ios_base::failure if thrown by the internal input stream.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by a user-defined implementation of
 *         Deserializer or DeserializerOverride, if one is used in the
 *         serialization of the given value type.
 *
 * \note Deserialization of user-defined types can be defined by implementing a
 *       specialization of the Deserializer template.
 */
template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride, typename Allocator = std::allocator<char>>
[[nodiscard]] inline T deserializeFromString(grem::StringBase<char, std::char_traits<char>, Allocator> string, const DeserializationOptions& options = {},
	DeserializerOverride deserializerOverride = {}, const SourceLocation& source = {}) {
	T value{};
	json::deserializeFromString(std::move(string), value, options, deserializerOverride, source);
	return value;
}

/**
 * Stateful wrapper object of an output stream for JSON serialization.
 */
struct Writer {
private:
	std::ostream& stream;

public:
	/**
	 * The current options of the serialization process.
	 */
	SerializationOptions options;

	/**
	 * Construct a writer with an output stream as output.
	 *
	 * \param stream output stream to write to.
	 * \param options output options, see SerializationOptions.
	 */
	explicit Writer(std::ostream& stream, const SerializationOptions& options = {})
		: stream(stream)
		, options(options) {}

	/**
	 * Write a single raw char to the output without any extra formatting.
	 *
	 * \param ch value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void write(char ch) {
		stream << ch;
	}

	/**
	 * Write a raw sequence of chars to the output without any extra formatting.
	 *
	 * \param raw values to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void write(StringView raw) {
		stream << raw;
	}

	/**
	 * Write a sequence of indentation characters to the output.
	 *
	 * The length of the sequence matches the current indentation level
	 * specified in the options.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 *
	 * \sa SerializationOptions::indentation
	 * \sa SerializationOptions::indentationCharacter
	 */
	void writeIndentation() {
		for (size_t i = 0; i < options.indentation; ++i) {
			write(options.indentationCharacter);
		}
	}

	/**
	 * Write a newline sequence to the output.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeNewline() {
		stream << options.newlineString;
	}

	/**
	 * Write a single JSON value of type Null to the output.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeNull() {
		write("null");
	}

	/**
	 * Write a single JSON value of type Boolean to the output.
	 *
	 * \param value value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeBoolean(Boolean value) {
		write((value) ? "true" : "false");
	}

	/**
	 * Write a single JSON value of type String to the output from a raw string.
	 *
	 * \param value raw string value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeString(StringView value) {
		constexpr CStringView HEXADECIMAL_DIGITS = "0123456789ABCDEF";
		write('\"');
		for (const char ch : value) {
			if (ch >= ' ' && ch <= '~' && ch != '\"' && ch != '\\') {
				write(ch);
			} else {
				write('\\');
				switch (ch) {
					case '\"': write('\"'); break;
					case '\\': write('\\'); break;
					case '\b': write('b'); break;
					case '\f': write('f'); break;
					case '\n': write('n'); break;
					case '\r': write('r'); break;
					case '\t': write('t'); break;
					case '\v': write('v'); break;
					case '\0': write('0'); break;
					default:
						write('x');
						write(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) >> 4) & 0x0F]);
						write(HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) & 0x0F)]);
						break;
				}
			}
		}
		write('\"');
	}

	/**
	 * Write a single JSON value of type String to the output from a raw UTF8
	 * string.
	 *
	 * \param value raw string value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeString(UTF8StringView value) {
		static_assert(sizeof(char8_t) == sizeof(char));
		writeString(StringView{std::launder(reinterpret_cast<const char*>(value.data())), value.size()});
	}

	/**
	 * Write a single JSON value of type String to the output from any
	 * string-view-like or JSON-serializable value.
	 *
	 * \param value value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the underlying string view conversion or
	 *         Serializer implementation of the given value type.
	 */
	void writeString(const auto& value) {
		if constexpr (requires { StringView{value}; }) {
			writeString(StringView{value});
		} else if constexpr (requires { UTF8StringView{value}; }) {
			writeString(UTF8StringView{value});
		} else {
			std::ostringstream stringStream{};
			json::serialize(stringStream, value, {.prettyPrint = false});
			writeString(std::move(stringStream).str());
		}
	}

	/**
	 * Write a single JSON value of type Number to the output.
	 *
	 * \param value value to write.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 */
	void writeNumber(Number value) {
		if (std::isnan(value)) {
			if (std::signbit(value)) {
				stream << "-NaN";
			} else {
				stream << "NaN";
			}
		} else if (std::isinf(value)) {
			if (std::signbit(value)) {
				stream << "-Infinity";
			} else {
				stream << "Infinity";
			}
		} else [[likely]] {
			char chars[32];
			const std::to_chars_result result = std::to_chars(chars, chars + sizeof(chars) - 1, value);
			GREM_ASSERT(result.ec == std::errc{});
			*result.ptr = '\0';
			stream << static_cast<const char*>(chars);
		}
	}

	/**
	 * Write a single JSON object to the output, using a callback function for
	 * writing out the properties.
	 *
	 * \param propertyCountHint estimated number of properties contained by the
	 *        object, which will be used to determine how to format the object
	 *        when pretty printing is enabled.
	 * \param callback function object for writing the object contents, which
	 *        should return void and accept the following generic parameter:
	 *        - `auto writeProperty`: a function object for writing each
	 *          property of the object, which accepts the following parameters:
	 *          - `const auto& key`: property key, writable as string.
	 *          - `const auto& value`: property value to write.
	 *          - `auto writeValue` (optional): a function object that accepts
	 *            the property value as an argument and writes its value to the
	 *            writer. Defaults to calling `writer.serialize(value)` if not
	 *            provided.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the callback function.
	 */
	void writeCustomObject(size_t propertyCountHint, auto callback) {
		write('{');
		if (options.prettyPrint) {
			if (propertyCountHint <= options.prettyPrintMaxSingleLineObjectPropertyCount) {
				bool empty = true;
				callback(WriteSmallPrettyObjectProperty{*this, empty});
				if (!empty) {
					write(' ');
				}
			} else {
				bool empty = true;
				callback(WriteLargePrettyObjectProperty{*this, empty});
				if (!empty) {
					writeNewline();
					options.indentation -= options.relativeIndentation;
					writeIndentation();
				}
			}
		} else {
			bool successor = false;
			callback(WriteCompactObjectProperty{*this, successor});
		}
		write('}');
	}

	/**
	 * Write a single JSON object to the output, using a callback function for
	 * writing out the properties.
	 *
	 * \param callback function object for writing the object contents, which
	 *        should return void and accept the following generic parameter:
	 *        - `auto writeProperty`: a function object for writing each
	 *          property of the object, which accepts the following parameters:
	 *          - `const auto& key`: property key, writable as string.
	 *          - `const auto& value`: property value to write.
	 *          - `auto writeValue` (optional): a function object that accepts
	 *            the property value as an argument and writes its value to the
	 *            writer. Defaults to calling `writer.serialize(value)` if not
	 *            provided.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the callback function.
	 */
	void writeCustomObject(auto callback) {
		writeCustomObject(Limits<size_t>::MAX, callback);
	}

	/**
	 * Write a single JSON object to the output from any range of
	 * JSON-serializable key-value pairs.
	 *
	 * \param value range to write as an object.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for nested values whenever a call
	 *        to `serializerOverride(writer, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer/SerializerOverride
	 *         implementations of the given key/value types.
	 */
	template <typename SerializerOverride = detail::NoSerializerOverride>
	void writeObject(const auto& value, SerializerOverride serializerOverride = {}) {
		const size_t propertyCountHint = detail::getRecursiveSize(value) - 1;
		writeCustomObject(propertyCountHint, [&](auto writeProperty) -> void {
			for (const auto& element : value) {
				writeProperty(element.first, element.second, [&](const auto& v) -> void { serialize(v, serializerOverride); });
			}
		});
	}

	/**
	 * Write a single JSON array to the output, using a callback function for
	 * writing out the items.
	 *
	 * \param itemCountHint estimated number of items contained by the array,
	 *        which will be used to determine how to format the array when
	 *        pretty printing is enabled.
	 * \param callback function object for writing the array contents, which
	 *        should return void and accept the following generic parameter:
	 *        - `auto writeItem`: a function object for writing each item of the
	 *          array, which accepts the following parameters:
	 *          - `const auto& value`: item value to write.
	 *          - `auto writeValue` (optional): a function object that accepts
	 *            the item value as an argument and writes its value to the
	 *            writer. Defaults to calling `writer.serialize(value)` if not
	 *            provided.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the callback function.
	 */
	void writeCustomArray(size_t itemCountHint, auto callback) {
		write('[');
		if (options.prettyPrint) {
			if (itemCountHint <= options.prettyPrintMaxSingleLineArrayItemCount) {
				bool successor = false;
				callback(WriteSmallPrettyArrayItem{*this, successor});
			} else {
				bool empty = true;
				callback(WriteLargePrettyArrayItem{*this, empty});
				if (!empty) {
					writeNewline();
					options.indentation -= options.relativeIndentation;
					writeIndentation();
				}
			}
		} else {
			bool successor = false;
			callback(WriteCompactArrayItem{*this, successor});
		}
		write(']');
	}

	/**
	 * Write a single JSON array to the output, using a callback function for
	 * writing out the items.
	 *
	 * \param callback function object for writing the array contents, which
	 *        should return void and accept the following generic parameter:
	 *        - `auto writeItem`: a function object for writing each item of the
	 *          array, which accepts the following parameters:
	 *          - `const auto& value`: item value to write.
	 *          - `auto writeValue` (optional): a function object that accepts
	 *            the item value as an argument and writes its value to the
	 *            writer. Defaults to calling `writer.serialize(value)` if not
	 *            provided.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the callback function.
	 */
	void writeCustomArray(auto callback) {
		writeCustomArray(Limits<size_t>::MAX, callback);
	}

	/**
	 * Write a single JSON array to the output from any range of
	 * JSON-serializable values.
	 *
	 * \param value range to write as an array.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for nested items whenever a call to
	 *        `serializerOverride(writer, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer/SerializerOverride
	 *         implementation of the element type of the given value type.
	 */
	template <typename SerializerOverride = detail::NoSerializerOverride>
	void writeArray(const auto& value, SerializerOverride serializerOverride = {}) {
		const size_t itemCountHint = detail::getRecursiveSize(value) - 1;
		writeCustomArray(itemCountHint, [&](auto writeItem) -> void {
			if constexpr (requires {
							  std::begin(value);
							  std::end(value);
						  }) {
				for (const auto& element : value) {
					writeItem(element, [&](const auto& v) -> void { serialize(v, serializerOverride); });
				}
			} else {
				const size_t n = std::size(value);
				for (size_t i = 0; i < n; ++i) {
					const auto& element = value[i];
					writeItem(element, [&](const auto& v) -> void { serialize(v, serializerOverride); });
				}
			}
		});
	}

	/**
	 * Write a single JSON value to the output from any value that supports
	 * conversion to bool and the dereference operator.
	 *
	 * If the value evaluates to true when converted to bool, the value is
	 * dereferenced and serialized normally. Otherwise, null is written.
	 *
	 * \param value value to write.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for the nested value whenever a
	 *        call to `serializerOverride(writer, value)` yields a valid
	 *        matching overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer implementation of the
	 *         dereferenced type of the given value type.
	 */
	template <typename SerializerOverride = detail::NoSerializerOverride>
	void writeOptional(const auto& value, SerializerOverride serializerOverride = {}) {
		if (value) {
			serialize(*value, serializerOverride);
		} else {
			writeNull();
		}
	}

	/**
	 * Write a single JSON value to the output from any value of aggregate type.
	 *
	 * The output JSON format is an object with properties whose keys match the
	 * field names of the aggregate.
	 *
	 * \param value aggregate whose fields to write.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for nested fields whenever a call
	 *        to `serializerOverride(writer, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer/SerializerOverride
	 *         implementations of the given field types.
	 */
	template <typename Aggregate, typename SerializerOverride = detail::NoSerializerOverride>
	void writeAggregateObject(const Aggregate& value, SerializerOverride serializerOverride = {}) {
		const size_t propertyCountHint = detail::getRecursiveSize(value) - 1;
		writeCustomObject(propertyCountHint, [&](auto writeProperty) -> void {
			meta::forEachNamedField(value, [&](StringView name, const auto& field) -> void { //
				writeProperty(name, field, [&](const auto& f) -> void { serialize(f, serializerOverride); });
			});
		});
	}

	/**
	 * Write a single JSON value to the output from any value of aggregate type.
	 *
	 * The output JSON format is an array with all fields as items.
	 *
	 * \param value aggregate whose fields to write.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for nested fields whenever a call
	 *        to `serializerOverride(writer, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer/SerializerOverride
	 *         implementations of the given field types.
	 */
	template <typename Aggregate, typename SerializerOverride = detail::NoSerializerOverride>
	void writeAggregateArray(const Aggregate& value, SerializerOverride serializerOverride = {}) {
		const size_t itemCountHint = detail::getRecursiveSize(value) - 1;
		writeCustomArray(itemCountHint, [&](auto writeItem) -> void {
			meta::forEachField(value, [&](const auto& field) -> void { //
				writeItem(field, [&](const auto& f) -> void { serialize(f, serializerOverride); });
			});
		});
	}

	/**
	 * Write any JSON-serializable value to the output using its corresponding
	 * implementation of Serializer.
	 *
	 * \param value value to write.
	 * \param serializerOverride overloaded function object that overrides the
	 *        default serialization behavior for the value or its nested
	 *        elements/fields whenever a call to
	 *        `serializerOverride(writer, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying output stream.
	 * \throws any exception thrown by the Serializer/SerializerOverride
	 *         implementation of T.
	 */
	template <typename T, typename SerializerOverride = detail::NoSerializerOverride>
	void serialize(const T& value, SerializerOverride serializerOverride = {});

private:
	struct WriteSmallPrettyObjectProperty {
		Writer& writer;
		bool& empty;

		void operator()(const auto& key, const auto& value, auto writeValue) {
			if (empty) {
				writer.write(' ');
				empty = false;
			} else {
				writer.write(", ");
			}
			writer.writeString(key);
			writer.write(": ");
			writeValue(value);
		}

		void operator()(const auto& key, const auto& value) {
			(*this)(key, value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};

	struct WriteLargePrettyObjectProperty {
		Writer& writer;
		bool& empty;

		void operator()(const auto& key, const auto& value, auto writeValue) {
			if (empty) {
				writer.writeNewline();
				writer.options.indentation += writer.options.relativeIndentation;
				writer.writeIndentation();
				empty = false;
			} else {
				writer.write(',');
				writer.writeNewline();
				writer.writeIndentation();
			}
			writer.writeString(key);
			writer.write(": ");
			writeValue(value);
		}

		void operator()(const auto& key, const auto& value) {
			(*this)(key, value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};

	struct WriteCompactObjectProperty {
		Writer& writer;
		bool& successor;

		void operator()(const auto& key, const auto& value, auto writeValue) {
			if (successor) {
				writer.write(',');
			}
			successor = true;
			writer.writeString(key);
			writer.write(':');
			writeValue(value);
		}

		void operator()(const auto& key, const auto& value) {
			(*this)(key, value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};

	struct WriteSmallPrettyArrayItem {
		Writer& writer;
		bool& successor;

		void operator()(const auto& value, auto writeValue) {
			if (successor) {
				writer.write(", ");
			}
			successor = true;
			writeValue(value);
		}

		void operator()(const auto& value) {
			(*this)(value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};

	struct WriteLargePrettyArrayItem {
		Writer& writer;
		bool& empty;

		void operator()(const auto& value, auto writeValue) {
			if (empty) {
				writer.writeNewline();
				writer.options.indentation += writer.options.relativeIndentation;
				writer.writeIndentation();
				empty = false;
			} else {
				writer.write(',');
				writer.writeNewline();
				writer.writeIndentation();
			}
			writeValue(value);
		}

		void operator()(const auto& value) {
			(*this)(value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};

	struct WriteCompactArrayItem {
		Writer& writer;
		bool& successor;

		void operator()(const auto& value, auto writeValue) {
			if (successor) {
				writer.write(',');
			}
			successor = true;
			writeValue(value);
		}

		void operator()(const auto& value) {
			(*this)(value, [&](const auto& value) -> void { writer.serialize(value); });
		}
	};
};

/**
 * Stateful wrapper object of an input stream for JSON deserialization.
 */
struct Reader {
private:
	using It = std::istreambuf_iterator<char>;

	Parser<It> parser;

public:
	/** Token type of the reader's parser. */
	using Token = typename Parser<It>::Token;

	/**
	 * The current options of the deserialization process.
	 */
	DeserializationOptions options;

	/**
	 * Construct a reader with an input stream as input.
	 *
	 * \param stream input stream to write to.
	 * \param source source location corresponding to the start of the given
	 *        stream.
	 * \param options input options, see DeserializationOptions.
	 */
	explicit Reader(std::istream& stream, const SourceLocation& source = {}, const DeserializationOptions& options = {})
		: parser(stream, source)
		, options(options) {}

	/**
	 * Read a single JSON value of type Null from the input.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseNull()
	 * \sa readValue()
	 */
	SourceLocation readNull() {
		const SourceLocation source = parser.peek().source;
		parser.parseNull();
		return source;
	}

	/**
	 * Check if the next value of the input is a JSON value of type Null.
	 *
	 * \return true if the next value is of type Null, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsNull()
	 * \sa readNull()
	 */
	[[nodiscard]] bool nextIsNull() const {
		return parser.peekIsNull();
	}

	/**
	 * Read a single JSON value of type Boolean from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::readBoolean()
	 * \sa readValue()
	 */
	SourceLocation readBoolean(Boolean& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseBoolean();
		return source;
	}

	/**
	 * Read a single JSON value of type Boolean from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::readBoolean()
	 * \sa readValue()
	 */
	[[nodiscard]] Boolean readBoolean() {
		Boolean value{};
		readBoolean(value);
		return value;
	}

	/**
	 * Check if the next value of the input is a JSON value of type Boolean.
	 *
	 * \return true if the next value is a of type Boolean, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsBoolean()
	 * \sa readBoolean()
	 */
	[[nodiscard]] bool nextIsBoolean() const {
		return parser.peekIsBoolean();
	}

	/**
	 * Read a single nullable JSON value of type Boolean from the input.
	 *
	 * \return the read value, or an empty optional if the value was null.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 */
	[[nodiscard]] Optional<Boolean> readBooleanOrNull() {
		Optional<Boolean> value{};
		readOptional(value);
		return value;
	}

	/**
	 * Read a single JSON value of type String from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseString()
	 * \sa readValue()
	 */
	SourceLocation readString(String& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseString();
		return source;
	}

	/**
	 * Read a single JSON value of type String from the input into any value
	 * that can be assigned from a standard string or deserialized from JSON.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the underlying assignment operator or
	 *         Deserializer implementation of the given value type.
	 */
	SourceLocation readString(auto& value) {
		String string{};
		const SourceLocation source = readString(string);
		convertString(value, std::move(string), source);
		return source;
	}

	/**
	 * Read a single JSON value of type String from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseString()
	 * \sa readValue()
	 */
	[[nodiscard]] String readString() {
		String value{};
		readString(value);
		return value;
	}

	/**
	 * Check if the next value of the input is a JSON value of type String.
	 *
	 * \return true if the next value is of type String, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsString()
	 * \sa readString()
	 */
	[[nodiscard]] bool nextIsString() const {
		return parser.peekIsString();
	}

	/**
	 * Read a single nullable JSON value of type String from the input.
	 *
	 * \return the read value, or an empty optional if the value was null.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 */
	[[nodiscard]] Optional<String> readStringOrNull() {
		Optional<String> value{};
		readOptional(value);
		return value;
	}

	/**
	 * Read a single JSON value of type Number from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseNumber()
	 * \sa readValue()
	 */
	SourceLocation readNumber(Number& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseNumber();
		return source;
	}

	/**
	 * Read a single JSON value of type Number from the input into any value
	 * that Number can be explicitly converted to.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the underlying conversion or assignment
	 *         operator of the given value type.
	 */
	template <typename T>
	SourceLocation readNumber(T& value) {
		Number number{};
		const SourceLocation source = readNumber(number);
		if constexpr (integral<T>) {
			if (trunc(number) != number) {
				throw json::Error{"Expected an integer.", source};
			}
			if (number < static_cast<json::Number>(Limits<T>::MIN) || number > static_cast<json::Number>(Limits<T>::MAX)) {
				throw json::Error{"Value out of range.", source};
			}
		}
		value = static_cast<T>(number);
		return source;
	}

	/**
	 * Read a single JSON value of type Number from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseNumber()
	 * \sa readValue()
	 */
	[[nodiscard]] Number readNumber() {
		Number value{};
		readNumber(value);
		return value;
	}

	/**
	 * Check if the next value of the input is a JSON value of type Number.
	 *
	 * \return true if the next value is of type Number, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsNumber()
	 * \sa readNumber()
	 */
	[[nodiscard]] bool nextIsNumber() const {
		return parser.peekIsNumber();
	}

	/**
	 * Read a single JSON value of type Number from the input.
	 *
	 * \tparam T number type to read.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseNumber()
	 * \sa readValue()
	 */
	template <typename T>
	[[nodiscard]] T readNumber() {
		T value{};
		readNumber(value);
		return value;
	}

	/**
	 * Read a single nullable JSON value of type Number from the input.
	 *
	 * \return the read value, or an empty optional if the value was null.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 */
	[[nodiscard]] Optional<Number> readNumberOrNull() {
		Optional<Number> value{};
		readOptional(value);
		return value;
	}

	/**
	 * Read a single JSON object from the input, using a callback function for
	 * reading its properties.
	 *
	 * \param readProperty function object for reading each property of the
	 *        object, which should return void and accept the following
	 *        parameters:
	 *        - `const SourceLocation& source`: source location of the upcoming
	 *          value to be read.
	 *        - `json::String&& key`: the key of the read property.
	 *        The function should read the value of the property using this
	 *        reader. If the function does nothing, the property value will be
	 *        skipped automatically.
	 *
	 * \return the location of the beginning of the parsed object.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the callback functions.
	 */
	SourceLocation readCustomObject(auto readProperty) {
		const SourceLocation source = parser.peek().source;
		if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_OPEN_CURLY_BRACE) {
			throw json::Error{"Expected an object.", token.source};
		}
		if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
			parser.advance();
		} else {
			while (true) {
				String propertyKey{};
				if (Token token = parser.eat(); token.type == TokenType::STRING || token.type == TokenType::IDENTIFIER_NAME) {
					propertyKey = std::move(token.string);
				} else {
					throw json::Error{"Expected a string or a name identifier.", token.source};
				}
				if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_COLON) {
					throw json::Error{"Expected a colon.", token.source};
				}
				const SourceLocation propertySource = parser.peek().source;
				readProperty(propertySource, propertyKey);
				if (parser.peek().source == propertySource) {
					parser.skipValue();
				}
				const Token token = parser.eat();
				if (token.type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
					break;
				}
				if (token.type == TokenType::PUNCTUATOR_COMMA) {
					if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
						parser.advance();
						break;
					}
				} else {
					throw json::Error{"Expected a comma or closing brace.", token.source};
				}
			}
		}
		return source;
	}

	/**
	 * Read a single JSON value of type Object from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseObject()
	 * \sa readValue()
	 */
	SourceLocation readObject(Object& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseObject();
		return source;
	}

	/**
	 * Read a single JSON object from the input into any container of key-value
	 * pairs where the key and value types are default-constructible and
	 * deserializable from JSON, and where the container supports `clear()` and
	 * `emplace(std::move(key), std::move(value))`.
	 *
	 * \param value reference to the output container to write the parsed results
	 *        into.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested values whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the location of the beginning of the parsed object.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the default constructors of the given
	 *         key/value types.
	 * \throws any exception thrown by the Deserializer implementations of the
	 *         given key/value types.
	 * \throws any exception thrown by `clear()` or
	 *         `emplace(std::move(key), std::move(value))`.
	 *
	 * \warning If an exception is thrown, the output value may be left empty or
	 *          with some successfully parsed properties added to it, since they
	 *          are not removed automatically if a later operation fails.
	 */
	template <typename DeserializerOverride = detail::NoDeserializerOverride>
	SourceLocation readObject(auto& value, DeserializerOverride deserializerOverride = {}) {
		const SourceLocation source = parser.peek().source;
		if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_OPEN_CURLY_BRACE) {
			throw json::Error{"Expected an object.", token.source};
		}
		value.clear();
		if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
			parser.advance();
		} else {
			while (true) {
				std::remove_cvref_t<decltype(std::begin(value)->first)> propertyKey{};
				std::remove_cvref_t<decltype(std::begin(value)->second)> propertyValue{};
				if (Token token = parser.eat(); token.type == TokenType::STRING || token.type == TokenType::IDENTIFIER_NAME) {
					convertString(propertyKey, std::move(token.string), token.source);
				} else {
					throw json::Error{"Expected a string or a name identifier.", token.source};
				}
				if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_COLON) {
					throw json::Error{"Expected a colon.", token.source};
				}
				deserialize(propertyValue, deserializerOverride);
				value.emplace(std::move(propertyKey), std::move(propertyValue));
				const Token token = parser.eat();
				if (token.type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
					break;
				}
				if (token.type == TokenType::PUNCTUATOR_COMMA) {
					if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_CURLY_BRACE) {
						parser.advance();
						break;
					}
				} else {
					throw json::Error{"Expected a comma or closing brace.", token.source};
				}
			}
		}
		return source;
	}

	/**
	 * Read a single JSON value of type Object from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseObject()
	 * \sa readValue()
	 */
	[[nodiscard]] Object readObject() {
		Object value{};
		readObject(value);
		return value;
	}

	/**
	 * Read a single JSON object from the input into any container of key-value
	 * pairs where the key and value types are default-constructible and
	 * deserializable from JSON, and where the container supports `clear()` and
	 * `emplace(std::move(key), std::move(value))`.
	 *
	 * \tparam Object object type to read.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the default constructors of the given
	 *         key/value types.
	 * \throws any exception thrown by the Deserializer implementations of the
	 *         given key/value types.
	 * \throws any exception thrown by `clear()` or
	 *         `emplace(std::move(key), std::move(value))`.
	 *
	 * \sa Parser::parseObject()
	 * \sa readValue()
	 */
	template <typename Object>
	[[nodiscard]] Object readObject() {
		Object value{};
		readObject(value);
		return value;
	}

	/**
	 * Check if the next value of the input is a JSON value of type Object.
	 *
	 * \return true if the next value is of type Object, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsObject()
	 * \sa readObject()
	 */
	[[nodiscard]] bool nextIsObject() const {
		return parser.peekIsObject();
	}

	/**
	 * Read a single nullable JSON value of type Object from the input.
	 *
	 * \return the read value, or an empty optional if the value was null.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 */
	[[nodiscard]] Optional<Object> readObjectOrNull() {
		Optional<Object> value{};
		readOptional(value);
		return value;
	}

	/**
	 * Read a single JSON array from the input, using a callback function for
	 * reading its items.
	 *
	 * \param readItem function object for reading each item of the array, which
	 *        should return void and accept the following parameter:
	 *        - `const SourceLocation& source`: source location of the upcoming
	 *          value to be read.
	 *        The function should read the value of the item using this reader.
	 *        If the function does nothing, the item value will be skipped
	 *        automatically.
	 *
	 * \return the location of the beginning of the parsed array.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the callback functions.
	 */
	SourceLocation readCustomArray(auto readItem) {
		const SourceLocation source = parser.peek().source;
		if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET) {
			throw json::Error{"Expected an array.", token.source};
		}
		if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
			parser.advance();
		} else {
			while (true) {
				const SourceLocation itemSource = parser.peek().source;
				readItem(itemSource);
				if (parser.peek().source == itemSource) {
					parser.skipValue();
				}
				const Token token = parser.eat();
				if (token.type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
					break;
				}
				if (token.type == TokenType::PUNCTUATOR_COMMA) {
					if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
						parser.advance();
						break;
					}
				} else {
					throw json::Error{"Expected a comma or closing bracket.", token.source};
				}
			}
		}
		return source;
	}

	/**
	 * Read a single JSON value of type Array from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseArray()
	 * \sa readValue()
	 */
	SourceLocation readArray(Array& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseArray();
		return source;
	}

	/**
	 * Read a single JSON array from the input into any container of elements
	 * that are default-constructible and deserializable from JSON, where the
	 * container supports `clear()` and `push_back(std::move(element))`.
	 *
	 * \param value reference to the output container to write the parsed results
	 *        into.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested items whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the location of the beginning of the parsed array.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the default constructor of the given
	 *         element type.
	 * \throws any exception thrown by the Deserializer implementation of the
	 *         given element type.
	 * \throws any exception thrown by `clear()` or `push_back(std::move(element))`.
	 *
	 * \warning If an exception is thrown, the output value may be left empty or
	 *          with some successfully parsed items added to it, since they are
	 *          not removed automatically if a later operation fails.
	 */
	template <typename DeserializerOverride = detail::NoDeserializerOverride>
	SourceLocation readArray(auto& value, DeserializerOverride deserializerOverride = {}) {
		const SourceLocation source = parser.peek().source;
		if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET) {
			throw json::Error{"Expected an array.", token.source};
		}
		value.clear();
		if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
			parser.advance();
		} else {
			while (true) {
				std::remove_cvref_t<decltype(*std::begin(value))> item{};
				deserialize(item, deserializerOverride);
				value.push_back(std::move(item));
				const Token token = parser.eat();
				if (token.type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
					break;
				}
				if (token.type == TokenType::PUNCTUATOR_COMMA) {
					if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
						parser.advance();
						break;
					}
				} else {
					throw json::Error{"Expected a comma or closing bracket.", token.source};
				}
			}
		}
		return source;
	}

	/**
	 * Read a single JSON value of type Array from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::parseArray()
	 * \sa readValue()
	 */
	[[nodiscard]] Array readArray() {
		Array value{};
		readArray(value);
		return value;
	}

	/**
	 * Read a single JSON array from the input into any container of elements
	 * that are default-constructible and deserializable from JSON, where the
	 * container supports `clear()` and `push_back(std::move(element))`.
	 *
	 * \tparam Array array type to read.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the default constructor of the given
	 *         element type.
	 * \throws any exception thrown by the Deserializer implementation of the
	 *         given element type.
	 * \throws any exception thrown by `clear()` or `push_back(std::move(element))`.
	 *
	 * \sa Parser::parseArray()
	 * \sa readValue()
	 */
	template <typename Array>
	[[nodiscard]] Array readArray() {
		Array value{};
		readArray(value);
		return value;
	}

	/**
	 * Check if the next value of the input is a JSON value of type Array.
	 *
	 * \return true if the next value is of type Array, false otherwise.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::peekIsArray()
	 * \sa readArray()
	 */
	[[nodiscard]] bool nextIsArray() const {
		return parser.peekIsArray();
	}

	/**
	 * Read a single nullable JSON value of type Array from the input.
	 *
	 * \return the read value, or an empty optional if the value was null.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 */
	[[nodiscard]] Optional<Array> readArrayOrNull() {
		Optional<Array> value{};
		readOptional(value);
		return value;
	}

	/**
	 * Read a single JSON value from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseValue()
	 */
	SourceLocation readValue(Variant& value) {
		const SourceLocation source = parser.peek().source;
		value = static_cast<Variant&&>(parser.parseValue());
		return source;
	}

	/**
	 * Read a single JSON value from the input.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 *
	 * \sa Parser::parseValue()
	 */
	SourceLocation readValue(Value& value) {
		const SourceLocation source = parser.peek().source;
		value = parser.parseValue();
		return source;
	}

	/**
	 * Read a single JSON value from the input.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input stream.
	 *
	 * \sa Parser::readValue()
	 */
	[[nodiscard]] Value readValue() {
		Value value{};
		readValue(value);
		return value;
	}

	/**
	 * Read a single nullable JSON value from the input into any value that is
	 * default-constructible, move-assignable and dereferencable, and where the
	 * dereferenced value type is default-constructible, deserializable from
	 * JSON and can be move-assigned into the value.
	 *
	 * If a null value is read, the output is assigned a default-constructed
	 * value of its own type.
	 *
	 * \param value reference to the output value to write the result to.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for the nested value whenever a
	 *        call to `deserializerOverride(reader, value)` yields a valid
	 *        matching overload.
	 *
	 * \return the location of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the default constructor of the given
	 *         value type or its dereferenced type.
	 * \throws any exception thrown by the Deserializer implementation of the
	 *         given value's dereferenced type.
	 * \throws any exception thrown by the assignment operator of the given
	 *         value type.
	 */
	template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride>
	SourceLocation readOptional(T& value, DeserializerOverride deserializerOverride = {}) {
		const SourceLocation source = parser.peek().source;
		if (parser.peek().type == TokenType::IDENTIFIER_NULL) {
			parser.advance();
			value = T{};
		} else {
			std::remove_cvref_t<decltype(*value)> result{};
			deserialize(result, deserializerOverride);
			value = std::move(result);
		}
		return source;
	}

	/**
	 * Read a single JSON value from the input into any value into a specific
	 * named field of an aggregate whose fields are deserializable from JSON.
	 *
	 * \param fieldName name of the field to read a value for.
	 * \param aggregate reference to the output aggregate whose field to write
	 *        the parsed result into.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested fields whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the location of the beginning of the parsed value, or an empty
	 *         optional if a field with the given name could not be found.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementations of the given field types.
	 *
	 * \note If any exception is thrown, the output value is left unmodified.
	 */
	template <typename Aggregate, typename DeserializerOverride = detail::NoDeserializerOverride>
	Optional<SourceLocation> readAggregateField(StringView fieldName, Aggregate& aggregate, DeserializerOverride deserializerOverride = {}) {
		const SourceLocation source = parser.peek().source;
		meta::forEachNamedField(aggregate, [&](StringView name, auto& field) -> void {
			if (fieldName == name) {
				deserialize(field, deserializerOverride);
			}
		});
		if (parser.peek().source == source) {
			return {};
		}
		return source;
	}

	/**
	 * Read a single JSON value from the input into any value of aggregate type
	 * whose fields are deserializable from JSON.
	 *
	 * The expected JSON format is an object with properties whose keys match
	 * the field names of the aggregate. Properties whose keys don't match any
	 * field name are ignored. Fields without a corresponding property are left
	 * unmodified.
	 *
	 * \param value reference to the output value whose fields to write the
	 *        parsed results into.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested fields whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the location of the beginning of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementations of the given field types.
	 *
	 * \warning If an exception is thrown, the output value may be left with
	 *          some successfully parsed fields, since they are
	 *          not reset automatically if a later operation fails.
	 */
	template <typename Aggregate, typename DeserializerOverride = detail::NoDeserializerOverride>
	SourceLocation readAggregateObject(Aggregate& value, DeserializerOverride deserializerOverride = {}) {
		return readCustomObject([&](const SourceLocation&, const String& key) -> void { readAggregateField(key, value, deserializerOverride); });
	}

	/**
	 * Read a single JSON value from the input into any value of aggregate type
	 * whose fields are deserializable from JSON.
	 *
	 * The expected JSON format is an object with properties whose keys match
	 * the field names of the aggregate. Properties whose keys don't match any
	 * field name are ignored. Fields without a corresponding property are left
	 * default-initialized.
	 *
	 * \tparam Aggregate aggregate type to read.
	 *
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested fields whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementations of the given field types.
	 */
	template <typename Aggregate, typename DeserializerOverride = detail::NoDeserializerOverride>
	[[nodiscard]] Aggregate readAggregateObject(DeserializerOverride deserializerOverride = {}) {
		Aggregate result{};
		readAggregateObject(result, deserializerOverride);
		return result;
	}

	/**
	 * Read a single JSON value from the input into any value of aggregate type
	 * whose fields are deserializable from JSON.
	 *
	 * The expected JSON format is an array with the same number of items as the
	 * number of fields in the aggregate.
	 *
	 * \param value reference to the output value whose fields to write the
	 *        parsed results into.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested fields whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the location of the beginning of the parsed value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementations of the given field types.
	 *
	 * \warning If an exception is thrown, the output value may be left with
	 *          some successfully parsed fields, since they are
	 *          not reset automatically if a later operation fails.
	 */
	template <typename Aggregate, typename DeserializerOverride = detail::NoDeserializerOverride>
	SourceLocation readAggregateArray(Aggregate& value, DeserializerOverride deserializerOverride = {}) {
		const SourceLocation source = parser.peek().source;
		if (const Token token = parser.eat(); token.type != TokenType::PUNCTUATOR_OPEN_SQUARE_BRACKET) {
			throw json::Error{"Expected an array.", token.source};
		}
		if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
			parser.advance();
		} else {
			meta::forEachField(value, [&](auto& field) -> void {
				const SourceLocation source = parser.peek().source;
				deserialize(field, deserializerOverride);
				if (parser.peek().source == source) {
					parser.skipValue();
				}
				const Token nextToken = parser.peek();
				if (nextToken.type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
					throw json::Error{"Expected " + toString(meta::aggregate_size_v<Aggregate>) + " items.", nextToken.source};
				}
				if (nextToken.type == TokenType::PUNCTUATOR_COMMA) {
					parser.advance();
					if (parser.peek().type == TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
						throw json::Error{"Expected " + toString(meta::aggregate_size_v<Aggregate>) + " items.", nextToken.source};
					}
				} else {
					throw json::Error{"Expected a comma or closing bracket.", nextToken.source};
				}
			});
			const Token token = parser.eat();
			if (token.type != TokenType::PUNCTUATOR_CLOSE_SQUARE_BRACKET) {
				throw json::Error{"Expected only " + toString(meta::aggregate_size_v<Aggregate>) + " items.", token.source};
			}
		}
		return source;
	}

	/**
	 * Read a single JSON value from the input into any value of aggregate type
	 * whose fields are deserializable from JSON.
	 *
	 * The expected JSON format is an array with the same number of items as the
	 * number of fields in the aggregate.
	 *
	 * \tparam Aggregate aggregate type to read.
	 *
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for nested fields whenever a call
	 *        to `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the read value.
	 *
	 * \throws json::Error on invalid input.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the underlying input iterator.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementations of the given field types.
	 */
	template <typename Aggregate, typename DeserializerOverride = detail::NoDeserializerOverride>
	[[nodiscard]] Aggregate readAggregateArray(DeserializerOverride deserializerOverride = {}) {
		Aggregate result{};
		readAggregateArray(result, deserializerOverride);
		return result;
	}

	/**
	 * Read a JSON value from the input into any value that is deserializable
	 * from JSON using its corresponding implementation of Deserializer.
	 *
	 * \param value reference to the output value to write the parsed result to.
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for the value or its nested
	 *        elements/fields whenever a call to
	 *        `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the Deserializer/DeserializerOverride
	 *         implementation of T.
	 */
	template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride>
	void deserialize(T& value, DeserializerOverride deserializerOverride = {});

	/**
	 * Read a JSON value from the input into any value that is deserializable
	 * from JSON using its corresponding implementation of Deserializer.
	 *
	 * \tparam T value type to read.
	 *
	 * \param deserializerOverride overloaded function object that overrides the
	 *        default deserialization behavior for the value or its nested
	 *        elements/fields whenever a call to
	 *        `deserializerOverride(reader, value)` yields a valid matching
	 *        overload.
	 *
	 * \return the read value.
	 *
	 * \throws any exception thrown by the underlying input stream.
	 * \throws any exception thrown by the Deserializer implementation of T.
	 */
	template <typename T, typename DeserializerOverride = detail::NoDeserializerOverride>
	[[nodiscard]] T deserialize(DeserializerOverride deserializerOverride = {}) {
		T result{};
		deserialize(result, deserializerOverride);
		return result;
	}

private:
	static void convertString(auto& value, String&& string, const SourceLocation& source) {
		if constexpr (requires { value = std::move(string); }) {
			value = std::move(string);
		} else if constexpr (requires { value = String{}; }) {
			value = String{std::move(string)};
		} else if constexpr (requires { value = UTF8String{}; }) {
			value = UTF8String{string.begin(), string.end()};
		} else {
			json::deserializeFromString(std::move(string), value, {}, {}, source);
		}
	}
};

template <typename T, typename SerializerOverride>
inline void serialize(std::ostream& stream, const T& value, const SerializationOptions& options, SerializerOverride serializerOverride) {
	Writer{stream, options}.serialize(value, serializerOverride);
}

template <typename T, typename DeserializerOverride>
inline void deserialize(std::istream& stream, T& value, const DeserializationOptions& options, DeserializerOverride deserializerOverride, const SourceLocation& source) {
	Reader{stream, source, options}.deserialize(value, deserializerOverride);
}

namespace detail {

struct NoVisitor {};

template <typename Base, typename Callback>
struct VisitNull : Base {
	[[no_unique_address]] Callback callback;

	VisitNull(Callback callback)
		: callback(std::move(callback)) {}

	VisitNull(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitNull(const SourceLocation& source, Null value) {
		callback(source, value);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitNull<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitBoolean : Base {
	[[no_unique_address]] Callback callback;

	VisitBoolean(Callback callback)
		: callback(std::move(callback)) {}

	VisitBoolean(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitBoolean(const SourceLocation& source, Boolean value) {
		callback(source, value);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitBoolean<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitString : Base {
	[[no_unique_address]] Callback callback;

	VisitString(Callback callback)
		: callback(std::move(callback)) {}

	VisitString(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitString(const SourceLocation& source, String&& value) {
		callback(source, std::move(value));
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitString<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitNumber : Base {
	[[no_unique_address]] Callback callback;

	VisitNumber(Callback callback)
		: callback(std::move(callback)) {}

	VisitNumber(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitNumber(const SourceLocation& source, Number value) {
		callback(source, value);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitNumber<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitObject : Base {
	[[no_unique_address]] Callback callback;

	VisitObject(Callback callback)
		: callback(std::move(callback)) {}

	VisitObject(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitObject(const SourceLocation& source, auto& parser) {
		callback(source, parser);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitObject<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitArray : Base {
	[[no_unique_address]] Callback callback;

	VisitArray(Callback callback)
		: callback(std::move(callback)) {}

	VisitArray(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	void visitArray(const SourceLocation& source, auto& parser) {
		callback(source, parser);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitArray<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

template <typename Base, typename Callback>
struct VisitProperty : Base {
	[[no_unique_address]] Callback callback;

	VisitProperty(Callback callback)
		: callback(std::move(callback)) {}

	VisitProperty(Base base, Callback callback)
		: Base(std::move(base))
		, callback(std::move(callback)) {}

	template <template <typename> typename Allocator>
	void visitProperty(const SourceLocation& source, StringBase<Allocator>&& key, auto& parser) {
		callback(source, std::move(key), parser);
	}

	template <typename NewBase>
	[[nodiscard]] auto operator|(NewBase other) && {
		return VisitProperty<NewBase, Callback>{std::move(other), std::move(callback)};
	}
};

} // namespace detail

/**
 * Build a Parser::ValueVisitor that handles Null values with a given callback
 * function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitNull() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting values of type
 *         Null. This visitor can be combined with other related visitors using
 *         the pipe operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onBoolean()
 * \sa json::onString()
 * \sa json::onNumber()
 * \sa json::onObject()
 * \sa json::onArray()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onNull(Callback callback) {
	return detail::VisitNull<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::ValueVisitor that handles Boolean values with a given
 * callback function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitBoolean() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting values of type
 *         Boolean. This visitor can be combined with other related visitors
 *         using the pipe operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onNull()
 * \sa json::onString()
 * \sa json::onNumber()
 * \sa json::onObject()
 * \sa json::onArray()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onBoolean(Callback callback) {
	return detail::VisitBoolean<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::ValueVisitor that handles String values with a given
 * callback function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitString() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting values of type
 *         String. This visitor can be combined with other related visitors using the
 *         pipe operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onNull()
 * \sa json::onBoolean()
 * \sa json::onNumber()
 * \sa json::onObject()
 * \sa json::onArray()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onString(Callback callback) {
	return detail::VisitString<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::ValueVisitor that handles Number values with a given
 * callback function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitNumber() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting values of type
 *         Number. This visitor can be combined with other related visitors
 *         using the pipe operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onNull()
 * \sa json::onBoolean()
 * \sa json::onString()
 * \sa json::onObject()
 * \sa json::onArray()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onNumber(Callback callback) {
	return detail::VisitNumber<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::ValueVisitor that handles objects with a given callback
 * function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitObject() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting objects. This
 *         visitor can be combined with other related visitors using the pipe
 *         operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onNull()
 * \sa json::onBoolean()
 * \sa json::onString()
 * \sa json::onNumber()
 * \sa json::onArray()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onObject(Callback callback) {
	return detail::VisitObject<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::ValueVisitor that handles arrays with a given callback
 * function.
 *
 * \param callback function object that is callable with the same signature as
 *        ValueVisitor::visitArray() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting arrays. This
 *         visitor can be combined with other related visitors using the pipe
 *         operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa json::onNull()
 * \sa json::onBoolean()
 * \sa json::onString()
 * \sa json::onNumber()
 * \sa json::onObject()
 * \sa Parser::parseFile(Parser::ValueVisitor&)
 * \sa Parser::parseValue(Parser::ValueVisitor&)
 * \sa Parser::parseArray(Parser::ValueVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onArray(Callback callback) {
	return detail::VisitArray<detail::NoVisitor, Callback>{std::move(callback)};
}

/**
 * Build a Parser::PropertyVisitor that handles object properties with a given
 * callback function.
 *
 * \param callback function object that is callable with the same signature as
 *        PropertyVisitor::visitProperty() and has the same semantics.
 *
 * \return a visitor that uses the given callback for visiting arrays. This
 *         visitor can be combined with other related visitors using the pipe
 *         operator '|'.
 *
 * \throws any exception thrown by the move constructor of the callback type.
 *
 * \sa Parser::parseObject(Parser::PropertyVisitor&)
 */
template <typename Callback>
[[nodiscard]] inline auto onProperty(Callback callback) {
	return detail::VisitProperty<detail::NoVisitor, Callback>{std::move(callback)};
}

namespace detail {

template <template <typename> typename Allocator>
consteval void derivedFromStringBaseTest(const StringBase<Allocator>&);

template <template <typename> typename Allocator>
consteval void derivedFromObjectBaseTest(const ObjectBase<Allocator>&);

template <template <typename> typename Allocator>
consteval void derivedFromArrayBaseTest(const ArrayBase<Allocator>&);

template <template <typename> typename Allocator>
consteval void derivedFromVariantBaseTest(const VariantBase<Allocator>&);

template <typename T>
concept derived_from_string_base = requires(const T t) { detail::derivedFromStringBaseTest(t); };

template <typename T>
concept derived_from_object_base = requires(const T t) { detail::derivedFromObjectBaseTest(t); };

template <typename T>
concept derived_from_array_base = requires(const T t) { detail::derivedFromArrayBaseTest(t); };

template <typename T>
concept derived_from_raw_value_base = requires(const T t) { detail::derivedFromVariantBaseTest(t); };

template <typename T>
concept nullable =    //
	!arithmetic<T> && //
	requires(const T value) {
		static_cast<bool>(value);
		static_cast<bool>(!value);
		T{};
	};

template <typename T>
concept serializable_as_string =   //
	derived_from_string_base<T> || //
	requires(const T input, T output) {
		StringView{input};
		output = String{};
	} || //
	requires(const T input, T output) {
		UTF8StringView{input};
		output = UTF8String{};
	};

template <typename T>
concept serializable_as_string_view =                 //
	requires(const T input) { StringView{input}; } || //
	requires(const T input) { UTF8StringView{input}; };

template <typename T>
concept serializable_as_duration = //
	requires(const T input, T output, const DurationBase<Number, Ratio<1, 1>> seconds) {
		duration_cast<DurationBase<Number, Ratio<1, 1>>>(input);
		output = duration_cast<T>(seconds);
	};

template <typename T>
concept serializable_as_array = //
	requires(Reader& reader, Writer& writer, const T input, T output, const size_t i) {
		std::size(input);
		writer.serialize(input[i]);
		reader.deserialize(output[i]);
	};

template <typename T>
concept serializable_as_map =      //
	derived_from_object_base<T> || //
	requires(Reader& reader, Writer& writer, const T input, T output) {
		writer.writeString(std::begin(input)->first);
		writer.serialize(std::begin(input)->second);

		output.clear();
		std::remove_cvref_t<decltype(std::begin(output)->first)>{};
		std::remove_cvref_t<decltype(std::begin(output)->second)>{};
		reader.readString(std::begin(output)->first);
		reader.deserialize(std::begin(output)->second);
		output.emplace(std::remove_cvref_t<decltype(std::begin(output)->first)>{}, std::remove_cvref_t<decltype(std::begin(output)->second)>{});
	};

template <typename T>
concept serializable_as_list =    //
	derived_from_array_base<T> || //
	requires(Reader& reader, Writer& writer, const T t) {
		writer.serialize(*std::begin(t));

		t.clear();
		std::remove_cvref_t<decltype(*std::begin(t))>{};
		reader.deserialize(*std::begin(t));
		t.push_back(std::remove_cvref_t<decltype(*std::begin(t))>{});
	};

template <typename T>
concept serializable_as_optional = //
	!pointer<T> &&                 //
	requires(Reader& reader, Writer& writer, const T input, T output, std::remove_cvref_t<decltype(*output)> result) {
		static_cast<bool>(input);
		writer.serialize(*input);

		output = T{};
		std::remove_cvref_t<decltype(*output)>{};
		reader.deserialize(result);
		output = std::move(result);
	};

template <typename T>
inline size_t getRecursiveSize(const T& value) {
	if constexpr (derived_from_raw_value_base<T>) {
		return match(value)([&](const auto& v) -> size_t { return getRecursiveSize(v); });
	} else if constexpr (serializable_as_string<T> || serializable_as_string_view<T> || serializable_as_duration<T>) {
		return 1;
	} else if constexpr (serializable_as_map<T>) {
		if constexpr (nullable<T>) {
			if (!value) {
				return 1;
			}
		}
		size_t result = 1;
		for (const auto& kv : value) {
			result += getRecursiveSize(kv.second);
		}
		return result;
	} else if constexpr (serializable_as_list<T> || serializable_as_array<T>) {
		if constexpr (nullable<T>) {
			if (!value) {
				return 1;
			}
		}
		size_t result = 1;
		if constexpr (requires {
						  std::begin(value);
						  std::end(value);
					  }) {
			for (const auto& v : value) {
				result += getRecursiveSize(v);
			}
		} else {
			const size_t n = std::size(value);
			for (size_t i = 0; i < n; ++i) {
				const auto& v = value[i];
				result += getRecursiveSize(v);
			}
		}
		return result;
	} else if constexpr (serializable_as_optional<T>) {
		return 1;
	} else if constexpr (aggregate<T>) {
		if constexpr (nullable<T>) {
			if (!value) {
				return 1;
			}
		}
		size_t result = 1;
		meta::forEachField(value, [&](const auto& v) -> void { result += getRecursiveSize(v); });
		return result;
	} else {
		return 1;
	}
}

} // namespace detail

/// \cond
template <>
struct Serializer<Null> {
	void serializeTo(Writer& writer, Null) {
		writer.writeNull();
	}
};

template <>
struct Serializer<std::nullptr_t> {
	void serializeTo(Writer& writer, std::nullptr_t) {
		writer.writeNull();
	}
};

template <>
struct Serializer<Boolean> {
	void serializeTo(Writer& writer, Boolean value) {
		writer.writeBoolean(value);
	}
};

template <strict_arithmetic Arithmetic>
struct Serializer<Arithmetic> {
	void serializeTo(Writer& writer, Arithmetic value) {
		writer.writeNumber(static_cast<Number>(value));
	}
};

template <>
struct Serializer<char> {
	void serializeTo(Writer& writer, char value) {
		writer.writeString(StringView{&value, 1});
	}
};

template <>
struct Serializer<char8_t> {
	void serializeTo(Writer& writer, char8_t value) {
		writer.writeString(UTF8StringView{&value, 1});
	}
};

template <template <typename> typename Allocator>
struct Serializer<StringBase<Allocator>> {
	void serializeTo(Writer& writer, const StringBase<Allocator>& value) {
		writer.writeString(value);
	}
};

template <template <typename> typename Allocator>
struct Serializer<ObjectBase<Allocator>> {
	void serializeTo(Writer& writer, const ObjectBase<Allocator>& value) {
		writer.writeObject(value);
	}
};

template <template <typename> typename Allocator>
struct Serializer<ArrayBase<Allocator>> {
	void serializeTo(Writer& writer, const ArrayBase<Allocator>& value) {
		writer.writeArray(value);
	}
};

template <template <typename> typename Allocator>
struct Serializer<VariantBase<Allocator>> {
	void serializeTo(Writer& writer, const VariantBase<Allocator>& value) {
		match(value)([&](const auto& v) -> void { writer.serialize(v); });
	}
};

template <template <typename> typename Allocator>
struct Serializer<ValueBase<Allocator>> : Serializer<VariantBase<Allocator>> {};

template <>
struct Deserializer<Null> {
	void deserializeFrom(Reader& reader, Null&) {
		reader.readNull();
	}
};

template <>
struct Deserializer<std::nullptr_t> {
	void deserializeFrom(Reader& reader, std::nullptr_t&) {
		reader.readNull();
	}
};

template <>
struct Deserializer<Boolean> {
	void deserializeFrom(Reader& reader, Boolean& value) {
		reader.readBoolean(value);
	}
};

template <strict_arithmetic Arithmetic>
struct Deserializer<Arithmetic> {
	void deserializeFrom(Reader& reader, Arithmetic& value) {
		reader.readNumber(value);
	}
};

template <>
struct Deserializer<char> {
	void deserializeFrom(Reader& reader, char& value) {
		String string{};
		const SourceLocation source = reader.readString(string);
		if (string.size() != 1) {
			throw json::Error{"Expected only a single character.", source};
		}
		value = string.front();
	}
};

template <>
struct Deserializer<char8_t> {
	void deserializeFrom(Reader& reader, char8_t& value) {
		String string{};
		const SourceLocation source = reader.readString(string);
		if (string.size() != sizeof(char8_t)) {
			throw json::Error{"Expected only a single UTF-8 code unit.", source};
		}
		memcpy(&value, string.data(), sizeof(char8_t));
	}
};

template <>
struct Deserializer<String> {
	void deserializeFrom(Reader& reader, String& value) {
		reader.readString(value);
	}
};

template <>
struct Deserializer<Object> {
	void deserializeFrom(Reader& reader, Object& value) {
		reader.readObject(value);
	}
};

template <>
struct Deserializer<Array> {
	void deserializeFrom(Reader& reader, Array& value) {
		reader.readArray(value);
	}
};

template <>
struct Deserializer<Variant> {
	void deserializeFrom(Reader& reader, Variant& value) {
		reader.readValue(value);
	}
};

template <>
struct Deserializer<Value> {
	void deserializeFrom(Reader& reader, Value& value) {
		reader.readValue(value);
	}
};
/// \endcond

template <typename T, typename SerializerOverride>
inline void Writer::serialize(const T& value, SerializerOverride serializerOverride) {
	using X = std::remove_cvref_t<T>;
	if constexpr (requires { serializerOverride(*this, value); }) {
		serializerOverride(*this, value);
	} else if constexpr (requires { json::Serializer<X>{}; }) {
		json::Serializer<X>{}.serializeTo(*this, value);
	} else {
		if constexpr (detail::nullable<X>) {
			if (!value) {
				writeNull();
				return;
			}
		}
		if constexpr (detail::serializable_as_string<X> || detail::serializable_as_string_view<X>) {
			writeString(value);
		} else if constexpr (detail::serializable_as_duration<X>) {
			writeNumber(duration_cast<DurationBase<Number, Ratio<1, 1>>>(value).count());
		} else if constexpr (detail::serializable_as_map<X>) {
			writeObject(value, serializerOverride);
		} else if constexpr (detail::serializable_as_list<X> || detail::serializable_as_array<X>) {
			if constexpr (requires(const T t, const size_t i) {
							  X::RANK;
							  t[i] == t[i];
						  } && !detail::serializable_as_array<std::remove_cvref_t<decltype(value[size_t{0}])>>) {
				const auto& element = value[size_t{0}];
				if (all(equal(value, X{element}))) {
					serialize(element, serializerOverride);
					return;
				}
			}
			writeArray(value, serializerOverride);
		} else if constexpr (detail::serializable_as_optional<X>) {
			writeOptional(value, serializerOverride);
		} else if constexpr (aggregate<X>) {
			writeAggregateObject(value, serializerOverride);
		} else if constexpr (enumeration<X>) {
			bool found = false;
			meta::forEachNamedEnumerand<X>([&](StringView name, auto type) -> void {
				if (!found && value == type) {
					found = true;
					writeString(name);
				}
			});
		} else {
			static_assert(meta::always_false_v<X>, "JSON serialization is not implemented for the given type.");
		}
	}
}

template <typename T, typename DeserializerOverride>
inline void Reader::deserialize(T& value, DeserializerOverride deserializerOverride) {
	using X = std::remove_cvref_t<T>;
	if constexpr (requires { deserializerOverride(*this, value); }) {
		deserializerOverride(*this, value);
	} else if constexpr (requires { json::Deserializer<X>{}; }) {
		json::Deserializer<X>{}.deserializeFrom(*this, value);
	} else if constexpr (detail::serializable_as_string<X>) {
		readString(value);
	} else if constexpr (detail::serializable_as_duration<X>) {
		Number seconds{};
		readNumber(seconds);
		value = duration_cast<X>(DurationBase<Number, Ratio<1, 1>>{seconds});
	} else if constexpr (detail::serializable_as_map<X>) {
		readObject(value, deserializerOverride);
	} else if constexpr (detail::serializable_as_list<X>) {
		readArray(value, deserializerOverride);
	} else if constexpr (detail::serializable_as_array<X>) {
		size_t index = 0;
		const size_t size = std::size(value);
		if constexpr (requires(const T t, const size_t i) {
						  X::RANK;
						  t[i] == t[i];
					  } && !detail::serializable_as_array<std::remove_cvref_t<decltype(value[index])>>) {
			if (nextIsArray()) {
				const SourceLocation source = readCustomArray([&](const json::SourceLocation& source) -> void {
					if (index >= size) {
						throw json::Error{"Expected " + toString(size) + " array items.", source};
					}
					deserialize(value[index], deserializerOverride);
					++index;
				});
				if (index != size) {
					throw json::Error{"Expected " + toString(size) + " array items.", source};
				}
			} else {
				value = X{deserialize<std::remove_cvref_t<decltype(value[index])>>(deserializerOverride)};
			}
		} else {
			const SourceLocation source = readCustomArray([&](const json::SourceLocation& source) -> void {
				if (index >= size) {
					throw json::Error{"Expected " + toString(size) + " array items.", source};
				}
				deserialize(value[index], deserializerOverride);
				++index;
			});
			if (index != size) {
				throw json::Error{"Expected " + toString(size) + " array items.", source};
			}
		}
	} else if constexpr (detail::serializable_as_optional<X>) {
		readOptional(value, deserializerOverride);
	} else if constexpr (aggregate<X>) {
		readAggregateObject(value, deserializerOverride);
	} else if constexpr (enumeration<X>) {
		json::String string{};
		const SourceLocation source = readString(string);
		Optional<X> result{};
		meta::forEachNamedEnumerand<X>([&](StringView name, auto type) -> void {
			if (!result && string == name) {
				result = type();
			}
		});
		if (!result) {
			throw json::Error{"Invalid type \"" + string + "\".", source};
		}
		value = *result;
	} else {
		static_assert(meta::always_false_v<X>, "JSON deserialization is not implemented for the given type.");
	}
}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase() noexcept
	: ObjectBase(allocator_type()) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(const allocator_type& allocator) noexcept
	: membersSortedByName(allocator) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::~ObjectBase() = default;

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(const ObjectBase& other, const allocator_type& allocator)
	: membersSortedByName(other.membersSortedByName, allocator) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(const ObjectBase& other)
	: ObjectBase(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(ObjectBase&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	: membersSortedByName(std::move(other.membersSortedByName), allocator) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(ObjectBase&& other) noexcept
	: ObjectBase(std::move(other), other.get_allocator()) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>& ObjectBase<Allocator>::operator=(const ObjectBase& other) = default;

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>& ObjectBase<Allocator>::operator=(ObjectBase&& other) noexcept(
	std::allocator_traits<
		allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
	std::allocator_traits<allocator_type>::is_always_equal::value) = default;

template <template <typename> typename Allocator>
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
inline ObjectBase<Allocator>::ObjectBase(InputIterator first, Sentinel last, const allocator_type& allocator)
	: membersSortedByName(first, last, allocator) {
	sort(membersSortedByName, Compare{});
	const auto newEnd = unique(membersSortedByName, [](const value_type& a, const value_type& b) -> bool { return a.first == b.first; });
	membersSortedByName.erase(newEnd, membersSortedByName.end());
}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>::ObjectBase(std::initializer_list<value_type> ilist, const allocator_type& allocator)
	: ObjectBase(ilist.begin(), ilist.end(), allocator) {}

template <template <typename> typename Allocator>
inline ObjectBase<Allocator>& ObjectBase<Allocator>::operator=(std::initializer_list<value_type> ilist) {
	membersSortedByName = ilist;
	sort(membersSortedByName, Compare{});
	const auto newEnd = unique(membersSortedByName, [](const value_type& a, const value_type& b) -> bool { return a.first == b.first; });
	membersSortedByName.erase(newEnd, membersSortedByName.end());
	return *this;
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::allocator_type ObjectBase<Allocator>::get_allocator() const noexcept {
	return membersSortedByName.get_allocator();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::mapped_type& ObjectBase<Allocator>::at(StringView key) {
	if (const auto it = find(key); it != end()) {
		return it->second;
	}
	throw std::out_of_range{"JSON object does not contain key \"" + grem::String{key} + "\"."};
}

template <template <typename> typename Allocator>
inline const typename ObjectBase<Allocator>::mapped_type& ObjectBase<Allocator>::at(StringView key) const {
	if (const auto it = find(key); it != end()) {
		return it->second;
	}
	throw std::out_of_range{"JSON object does not contain key \"" + grem::String{key} + "\"."};
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::mapped_type& ObjectBase<Allocator>::operator[](const key_type& k) {
	return try_emplace(k).first->second;
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::mapped_type& ObjectBase<Allocator>::operator[](key_type&& k) {
	return try_emplace(std::move(k)).first->second;
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::begin() noexcept {
	return membersSortedByName.begin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::begin() const noexcept {
	return membersSortedByName.begin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::cbegin() const noexcept {
	return membersSortedByName.cbegin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::end() noexcept {
	return membersSortedByName.end();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::end() const noexcept {
	return membersSortedByName.end();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::cend() const noexcept {
	return membersSortedByName.cend();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::reverse_iterator ObjectBase<Allocator>::rbegin() noexcept {
	return membersSortedByName.rbegin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_reverse_iterator ObjectBase<Allocator>::rbegin() const noexcept {
	return membersSortedByName.rbegin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_reverse_iterator ObjectBase<Allocator>::crbegin() const noexcept {
	return membersSortedByName.crbegin();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::reverse_iterator ObjectBase<Allocator>::rend() noexcept {
	return membersSortedByName.rend();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_reverse_iterator ObjectBase<Allocator>::rend() const noexcept {
	return membersSortedByName.rend();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_reverse_iterator ObjectBase<Allocator>::crend() const noexcept {
	return membersSortedByName.crend();
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::empty() const noexcept {
	return membersSortedByName.empty();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::size_type ObjectBase<Allocator>::size() const noexcept {
	return membersSortedByName.size();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::size_type ObjectBase<Allocator>::max_size() const noexcept {
	return membersSortedByName.max_size();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::size_type ObjectBase<Allocator>::capacity() const noexcept {
	return membersSortedByName.capacity();
}

template <template <typename> typename Allocator>
inline void ObjectBase<Allocator>::clear() noexcept {
	membersSortedByName.clear();
}

template <template <typename> typename Allocator>
inline void ObjectBase<Allocator>::reserve(size_type newCapacity) {
	membersSortedByName.reserve(newCapacity);
}

template <template <typename> typename Allocator>
template <typename P>
inline Pair<typename ObjectBase<Allocator>::iterator, bool> ObjectBase<Allocator>::insert(P&& value) {
	return emplace(std::forward<P>(value));
}

template <template <typename> typename Allocator>
template <typename P>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::insert(const_iterator pos, P&& value) requires(!convertible_to<P, const_iterator>) {
	return emplace_hint(pos, std::forward<P>(value));
}

template <template <typename> typename Allocator>
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
inline void ObjectBase<Allocator>::insert(InputIterator first, Sentinel last) {
	const size_t offset = membersSortedByName.size();
	try {
		while (first != last) {
			membersSortedByName.push_back(*first++);
		}
		const auto newMembersBegin = membersSortedByName.begin() + static_cast<ptrdiff_t>(offset);
		sort(Subrange{newMembersBegin, membersSortedByName.end()}, Compare{});
		inplaceMerge(membersSortedByName, newMembersBegin);
	} catch (...) {
		membersSortedByName.resize(offset);
		throw;
	}
}

template <template <typename> typename Allocator>
inline void ObjectBase<Allocator>::insert(std::initializer_list<value_type> ilist) {
	insert(ilist.begin(), ilist.end());
}

template <template <typename> typename Allocator>
template <typename R>
inline void ObjectBase<Allocator>::insert_range(R&& r) { // NOLINT(cppcoreguidelines-missing-std-forward)
	insert(std::begin(r), std::end(r));
}

template <template <typename> typename Allocator>
template <typename... Args>
inline Pair<typename ObjectBase<Allocator>::iterator, bool> ObjectBase<Allocator>::emplace(Args&&... args) {
	value_type value{std::forward<Args>(args)...};
	const auto [first, last] = equal_range(value.first);
	if (first != last) {
		return {first, false};
	}
	const auto it = membersSortedByName.insert(last, std::move(value));
	return {it, true};
}

template <template <typename> typename Allocator>
template <typename... Args>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::emplace_hint(const_iterator, Args&&... args) {
	return emplace(std::forward<Args>(args)...).first;
}

template <template <typename> typename Allocator>
template <typename... Args>
inline Pair<typename ObjectBase<Allocator>::iterator, bool> ObjectBase<Allocator>::try_emplace(const key_type& k, Args&&... args) {
	const auto [first, last] = equal_range(k);
	if (first != last) {
		return {first, false};
	}
	const auto it = membersSortedByName.emplace(last, std::piecewise_construct, std::forward_as_tuple(k), std::forward_as_tuple(std::forward<Args>(args)...));
	return {it, true};
}

template <template <typename> typename Allocator>
template <typename... Args>
inline Pair<typename ObjectBase<Allocator>::iterator, bool> ObjectBase<Allocator>::try_emplace(key_type&& k, Args&&... args) {
	const auto [first, last] = equal_range(k);
	if (first != last) {
		return {first, false};
	}
	const auto it = membersSortedByName.emplace(last, std::piecewise_construct, std::forward_as_tuple(std::move(k)), std::forward_as_tuple(std::forward<Args>(args)...));
	return {it, true};
}

template <template <typename> typename Allocator>
template <typename... Args>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::try_emplace(const_iterator, const key_type& k, Args&&... args) {
	return try_emplace(k, std::forward<Args>(args)...);
}

template <template <typename> typename Allocator>
template <typename... Args>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::try_emplace(const_iterator, key_type&& k, Args&&... args) {
	return try_emplace(std::move(k), std::forward<Args>(args)...);
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::erase(const_iterator pos) {
	return membersSortedByName.erase(pos);
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::size_type ObjectBase<Allocator>::erase(StringView key) {
	const auto [first, last] = equal_range(key);
	const size_type count = static_cast<size_type>(last - first);
	membersSortedByName.erase(first, last);
	return count;
}

template <template <typename> typename Allocator>
inline void ObjectBase<Allocator>::swap(ObjectBase& other) noexcept {
	membersSortedByName.swap(other.membersSortedByName);
}

template <template <typename> typename Allocator>
inline void swap(ObjectBase<Allocator>& a, ObjectBase<Allocator>& b) noexcept {
	a.swap(b);
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::size_type ObjectBase<Allocator>::count(StringView key) const noexcept {
	const auto [first, last] = equal_range(key);
	return static_cast<size_type>(last - first);
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::contains(StringView key) const noexcept {
	return count(key) > 0;
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::find(StringView key) noexcept {
	if (const auto [first, last] = equal_range(key); first != last) {
		return first;
	}
	return end();
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::find(StringView key) const noexcept {
	if (const auto [first, last] = equal_range(key); first != last) {
		return first;
	}
	return end();
}

template <template <typename> typename Allocator>
inline Pair<typename ObjectBase<Allocator>::iterator, typename ObjectBase<Allocator>::iterator> ObjectBase<Allocator>::equal_range(StringView key) noexcept {
	const auto [first, last] = equalRange(membersSortedByName, key, Compare{});
	return {first, last};
}

template <template <typename> typename Allocator>
inline Pair<typename ObjectBase<Allocator>::const_iterator, typename ObjectBase<Allocator>::const_iterator> ObjectBase<Allocator>::equal_range(StringView key) const noexcept {
	const auto [first, last] = equalRange(membersSortedByName, key, Compare{});
	return {first, last};
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::lower_bound(StringView key) noexcept {
	return lowerBound(membersSortedByName, key, Compare{});
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::lower_bound(StringView key) const noexcept {
	return lowerBound(membersSortedByName, key, Compare{});
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::iterator ObjectBase<Allocator>::upper_bound(StringView key) noexcept {
	return upperBound(membersSortedByName, key, Compare{});
}

template <template <typename> typename Allocator>
inline typename ObjectBase<Allocator>::const_iterator ObjectBase<Allocator>::upper_bound(StringView key) const noexcept {
	return upperBound(membersSortedByName, key, Compare{});
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::operator==(const ObjectBase& other) const noexcept {
	return membersSortedByName == other.membersSortedByName;
}

template <template <typename> typename Allocator>
inline std::partial_ordering ObjectBase<Allocator>::operator<=>(const ObjectBase& other) const noexcept {
	return std::compare_partial_order_fallback(membersSortedByName, other.membersSortedByName);
}

template <template <typename> typename Allocator, typename Predicate>
inline typename ObjectBase<Allocator>::size_type erase_if(ObjectBase<Allocator>& container, Predicate predicate) {
	return erase_if(container.membersSortedByName, predicate);
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::Compare::operator()(const value_type& a, const value_type& b) const noexcept {
	return a.first < b.first;
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::Compare::operator()(const value_type& a, StringView b) const noexcept {
	return a.first < b;
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::Compare::operator()(StringView a, const value_type& b) const noexcept {
	return a < b.first;
}

template <template <typename> typename Allocator>
inline bool ObjectBase<Allocator>::Compare::operator()(StringView a, StringView b) const noexcept {
	return a < b;
}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase() noexcept
	: ArrayBase(allocator_type()) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(const allocator_type& allocator) noexcept
	: values(allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::~ArrayBase() = default;

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(const ArrayBase& other, const allocator_type& allocator)
	: values(other.values, allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(const ArrayBase& other)
	: ArrayBase(other, std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator())) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(ArrayBase&& other, const allocator_type& allocator) noexcept // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	: values(std::move(other.values), allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(ArrayBase&& other) noexcept
	: ArrayBase(std::move(other), other.get_allocator()) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>& ArrayBase<Allocator>::operator=(const ArrayBase& other) = default;

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>& ArrayBase<Allocator>::operator=(ArrayBase&& other) noexcept(
	std::allocator_traits<
		allocator_type>::propagate_on_container_move_assignment::value || // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
	std::allocator_traits<allocator_type>::is_always_equal::value) = default;

template <template <typename> typename Allocator>
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
inline ArrayBase<Allocator>::ArrayBase(InputIterator first, Sentinel last, const allocator_type& allocator)
	: values(first, last, allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(size_type count, const value_type& value, const allocator_type& allocator)
	: values(count, value, allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>::ArrayBase(std::initializer_list<value_type> ilist, const allocator_type& allocator)
	: values(ilist, allocator) {}

template <template <typename> typename Allocator>
inline ArrayBase<Allocator>& ArrayBase<Allocator>::operator=(std::initializer_list<value_type> ilist) {
	values = ilist;
	return *this;
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::swap(ArrayBase& other) noexcept {
	values.swap(other.values);
}

template <template <typename> typename Allocator>
inline void swap(ArrayBase<Allocator>& a, ArrayBase<Allocator>& b) noexcept {
	a.swap(b);
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::allocator_type ArrayBase<Allocator>::get_allocator() const noexcept {
	return values.get_allocator();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::pointer ArrayBase<Allocator>::data() noexcept {
	return values.data();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_pointer ArrayBase<Allocator>::data() const noexcept {
	return values.data();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::size_type ArrayBase<Allocator>::size() const noexcept {
	return values.size();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::size_type ArrayBase<Allocator>::max_size() const noexcept {
	return values.max_size();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::size_type ArrayBase<Allocator>::capacity() const noexcept {
	return values.capacity();
}

template <template <typename> typename Allocator>
inline bool ArrayBase<Allocator>::empty() const noexcept {
	return values.empty();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::begin() noexcept {
	return values.begin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_iterator ArrayBase<Allocator>::begin() const noexcept {
	return values.begin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_iterator ArrayBase<Allocator>::cbegin() const noexcept {
	return values.cbegin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::end() noexcept {
	return values.end();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_iterator ArrayBase<Allocator>::end() const noexcept {
	return values.end();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_iterator ArrayBase<Allocator>::cend() const noexcept {
	return values.cend();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reverse_iterator ArrayBase<Allocator>::rbegin() noexcept {
	return values.rbegin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reverse_iterator ArrayBase<Allocator>::rbegin() const noexcept {
	return values.rbegin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reverse_iterator ArrayBase<Allocator>::crbegin() const noexcept {
	return values.crbegin();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reverse_iterator ArrayBase<Allocator>::rend() noexcept {
	return values.rend();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reverse_iterator ArrayBase<Allocator>::rend() const noexcept {
	return values.rend();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reverse_iterator ArrayBase<Allocator>::crend() const noexcept {
	return values.crend();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reference ArrayBase<Allocator>::front() {
	return values.front();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reference ArrayBase<Allocator>::front() const {
	return values.front();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reference ArrayBase<Allocator>::back() {
	return values.back();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reference ArrayBase<Allocator>::back() const {
	return values.back();
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reference ArrayBase<Allocator>::at(size_type pos) {
	if (pos < values.size()) {
		return values[pos];
	}
	throw std::out_of_range{"JSON array does not contain item [" + toString(pos) + "]."};
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reference ArrayBase<Allocator>::at(size_type pos) const {
	if (pos < values.size()) {
		return values[pos];
	}
	throw std::out_of_range{"JSON array does not contain item [" + toString(pos) + "]."};
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::reference ArrayBase<Allocator>::operator[](size_type pos) {
	return values[pos];
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::const_reference ArrayBase<Allocator>::operator[](size_type pos) const {
	return values[pos];
}

template <template <typename> typename Allocator>
inline bool ArrayBase<Allocator>::operator==(const ArrayBase& other) const {
	return values == other.values;
}

template <template <typename> typename Allocator>
inline std::partial_ordering ArrayBase<Allocator>::operator<=>(const ArrayBase& other) const noexcept {
	return std::compare_partial_order_fallback(values, other.values);
}

template <template <typename> typename Allocator, typename U>
inline typename ArrayBase<Allocator>::size_type erase(ArrayBase<Allocator>& container, const U& value) {
	return erase(container.values, value);
}

template <template <typename> typename Allocator, typename Predicate>
inline typename ArrayBase<Allocator>::size_type erase_if(ArrayBase<Allocator>& container, Predicate predicate) {
	return erase_if(container.values, predicate);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::clear() noexcept {
	values.clear();
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::reserve(size_type newCapacity) {
	values.reserve(newCapacity);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::shrink_to_fit() {
	values.shrink_to_fit();
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::assign(size_type count, const value_type& value) {
	values.assign(count, value);
}

template <template <typename> typename Allocator>
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
inline void ArrayBase<Allocator>::assign(InputIterator first, Sentinel last) {
	values.assign(first, last);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::assign(std::initializer_list<value_type> ilist) {
	values.assign(ilist);
}

template <template <typename> typename Allocator>
template <typename R>
inline void ArrayBase<Allocator>::assign_range(R&& r) {
	values.assign_range(std::forward<R>(r));
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert(const_iterator pos, const value_type& value) {
	return values.insert(pos, value);
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert(const_iterator pos, value_type&& value) {
	return values.insert(pos, std::move(value));
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert(const_iterator pos, size_type count, const value_type& value) {
	return values.insert(pos, count, value);
}

template <template <typename> typename Allocator>
template <input_iterator InputIterator, sentinel_for<InputIterator> Sentinel>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert(const_iterator pos, InputIterator first, Sentinel last) {
	return values.insert(pos, first, last);
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert(const_iterator pos, std::initializer_list<value_type> ilist) {
	return values.insert(pos, ilist);
}

template <template <typename> typename Allocator>
template <typename R>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::insert_range(const_iterator pos, R&& r) {
	return values.insert_range(pos, std::forward<R>(r));
}

template <template <typename> typename Allocator>
template <typename R>
inline void ArrayBase<Allocator>::append_range(R&& r) {
	values.append_range(std::forward<R>(r));
}

template <template <typename> typename Allocator>
template <typename... Args>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::emplace(const_iterator pos, Args&&... args) {
	return values.emplace(pos, std::forward<Args>(args)...);
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::erase(const_iterator pos) {
	return values.erase(pos);
}

template <template <typename> typename Allocator>
inline typename ArrayBase<Allocator>::iterator ArrayBase<Allocator>::erase(const_iterator first, const_iterator last) {
	return values.erase(first, last);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::push_back(const value_type& value) {
	values.push_back(value);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::push_back(value_type&& value) {
	values.push_back(std::move(value));
}

template <template <typename> typename Allocator>
template <typename... Args>
inline typename ArrayBase<Allocator>::reference ArrayBase<Allocator>::emplace_back(Args&&... args) {
	return values.emplace_back(std::forward<Args>(args)...);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::pop_back() {
	values.pop_back();
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::resize(size_type count) {
	values.resize(count);
}

template <template <typename> typename Allocator>
inline void ArrayBase<Allocator>::resize(size_type count, const value_type& value) {
	values.resize(count, value);
}

template <template <typename> typename Allocator>
inline ValueBase<Allocator> VariantBase<Allocator>::parse(UTF8StringView jsonString, const SourceLocation& source, const allocator_type& allocator) {
	return ValueBase<Allocator>::parse(jsonString, source, allocator);
}

template <template <typename> typename Allocator>
inline ValueBase<Allocator> VariantBase<Allocator>::parse(StringView jsonString, const SourceLocation& source, const allocator_type& allocator) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	return parse(UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(jsonString.data())), jsonString.size()}, source, allocator);
}

template <template <typename> typename Allocator>
inline StringBase<Allocator> VariantBase<Allocator>::toString(const SerializationOptions& options, const allocator_type& stringAllocator) const {
	return json::serializeToString(*this, options, {}, Allocator<char>{stringAllocator});
}

template <template <typename> typename Allocator>
inline ValueBase<Allocator> ValueBase<Allocator>::parse(UTF8StringView jsonString, const SourceLocation& source, const allocator_type& allocator) {
	unicode::UTF8View codePoints{jsonString};
	return Parser<const char8_t*, Allocator>{Lexer<const char8_t*, Allocator>{codePoints.begin(), codePoints.end(), source, allocator}, allocator}.parseFile();
}

template <template <typename> typename Allocator>
inline ValueBase<Allocator> ValueBase<Allocator>::parse(StringView jsonString, const SourceLocation& source, const allocator_type& allocator) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	return parse(UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(jsonString.data())), jsonString.size()}, source, allocator);
}

} // namespace grem::json

namespace grem::pmr {

namespace json {

using String = grem::json::StringBase<std::pmr::polymorphic_allocator>;
using Object = grem::json::ObjectBase<std::pmr::polymorphic_allocator>;
using Array = grem::json::ArrayBase<std::pmr::polymorphic_allocator>;
using Variant = grem::json::VariantBase<std::pmr::polymorphic_allocator>;
using Value = grem::json::ValueBase<std::pmr::polymorphic_allocator>;
using StringParser = grem::json::StringParserBase<std::pmr::polymorphic_allocator>;
using StreamParser = grem::json::StreamParserBase<std::pmr::polymorphic_allocator>;

} // namespace json

} // namespace grem::pmr

#endif
