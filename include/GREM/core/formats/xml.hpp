// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_XML_HPP
#define GREM_CORE_FORMATS_XML_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>

namespace grem::xml {

/**
 * Exception type for errors originating from the XML API.
 */
struct Error : grem::Error {
	/**
	 * Iterator into the source XML string where the error originated from.
	 */
	StringView::iterator position;

	/**
	 * Line number, starting at 1 for the first line, where the error occured.
	 */
	size_t lineNumber;

	Error(const auto& message, StringView::iterator position, size_t lineNumber)
		: grem::Error(message)
		, position(position)
		, lineNumber(lineNumber) {}
};

/**
 * Named attribute of an Element with an optional value.
 */
struct Attribute {
	String name{};                   ///< Name of the attribute.
	String value{};                  ///< Attribute value, or empty for no value.
	UniquePointer<Attribute> next{}; ///< Next neighboring attribute in the list that this attribute is part of.
};

/**
 * Node in a Document.
 */
struct Element {
	String tag{};                          ///< Element tag name.
	String content{};                      ///< Raw non-element text content of the element.
	UniquePointer<Attribute> attributes{}; ///< Linked list of element attributes.
	UniquePointer<Element> children{};     ///< Linked list of children of this element.
	UniquePointer<Element> next{};         ///< Next neighboring element in the list that this element is part of.
};

/**
 * Tree of Element nodes defined by an XML file.
 */
struct Document {
	/**
	 * Parse a document from an XML string.
	 *
	 * \param xmlString read-only view over the XML string to parse.
	 *
	 * \return the parsed document.
	 *
	 * \throws xml::Error on failure to parse any element of the document.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(core) static Document parse(StringView xmlString);

	UniquePointer<Element> declaration{}; ///< Optional XML declaration.
	UniquePointer<Element> root{};        ///< Root element of the document tree.
};

} // namespace grem::xml

#endif
