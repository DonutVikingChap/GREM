// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/formats/unicode.hpp>
#include <GREM/core/formats/xml.hpp>
#include <GREM/core/fundamentals.hpp>

#include <charconv>     // std::from_chars_result, std::from_chars
#include <memory>       // std::to_address
#include <new>          // std::launder
#include <system_error> // std::errc
#include <utility>      // std::move

namespace grem::xml {

namespace {

class Parser {
public:
	[[nodiscard]] static constexpr bool isWhitespace(char ch) noexcept {
		return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
	}

	[[nodiscard]] static constexpr bool isNameStartCharacter(char ch) noexcept {
		return ch == ':' || (ch >= 'A' && ch <= 'Z') || ch == '_' || (ch >= 'a' && ch <= 'z');
	}

	[[nodiscard]] static constexpr bool isNameCharacter(char ch) noexcept {
		return isNameStartCharacter(ch) || ch == '-' || ch == '.' || (ch >= '0' && ch <= '9');
	}

	Parser(StringView string, size_t lineNumber) noexcept
		: it(string.begin())
		, end(string.end())
		, lineNumber(lineNumber) {}

	[[nodiscard]] UniquePointer<Element> parseXMLDeclarationIfPresent() {
		if (const StringView tag = (read("<?xml")) ? "?xml" : (read("<?XML")) ? "?XML" : ""; !tag.empty()) {
			Element declaration{.tag = String{tag}};
			parseAttributes(declaration);
			if (!read("?>")) {
				throw xml::Error{"Invalid XML declaration end.", it, lineNumber};
			}
			return UniquePointer<Element>::create(std::move(declaration));
		}
		return nullptr;
	}

	[[nodiscard]] UniquePointer<Element> parseElement() {
		if (!read('<')) {
			throw xml::Error{"Missing element.", it, lineNumber};
		}
		Element result{};
		result.tag = parseName();
		parseAttributes(result);
		if (!read("/>")) {
			if (read("?>")) {
				throw xml::Error{"Invalid element tag end.", it, lineNumber};
			}
			if (!read('>')) {
				throw xml::Error{"Invalid tag end.", it, lineNumber};
			}
			parseContent(result);
		}
		return UniquePointer<Element>::create(std::move(result));
	}

	void skipWhitespace() {
		while (it != end) {
			if (*it == '\n') {
				++it;
				++lineNumber;
			} else if (*it == '\r') {
				++it;
				if (it != end && *it == '\n') {
					++it;
				}
				++lineNumber;
			} else if (*it == ' ' || *it == '\t') {
				++it;
			} else {
				break;
			}
		}
	}

private:
	[[nodiscard]] bool read(char ch) {
		if (it != end && *it == ch) {
			++it;
			return true;
		}
		return false;
	}

	[[nodiscard]] bool read(StringView str) {
		StringView::iterator p = it;
		for (const char ch : str) {
			if (*p != ch) {
				return false;
			}
			++p;
		}
		it = p;
		return true;
	}

	[[nodiscard]] String parseName() {
		const StringView::iterator begin = it;
		while (it != end && isNameCharacter(*it)) {
			++it;
		}
		if (it == begin) {
			throw xml::Error{"Missing name.", it, lineNumber};
		}
		return String{begin, it};
	}

	void parseReference(String& output) {
		if (it == end || *it != '&') {
			throw xml::Error{"Missing reference.", it, lineNumber};
		}
		++it;
		if (read('#')) {
			const StringView::iterator codePointStringBegin = it;
			int radix = 10;
			if (read('x')) {
				radix = 16;
			}
			const char* const codePointBegin = std::to_address(it);
			while (it != end && *it != ';') {
				if (*it == '\n') {
					++it;
					++lineNumber;
				} else if (*it == '\r') {
					++it;
					if (it != end && *it == '\n') {
						++it;
					}
					++lineNumber;
				} else {
					++it;
				}
			}
			const char* const codePointEnd = std::to_address(it);
			uint32_t codePointValue = 0;
			if (const std::from_chars_result parseResult = std::from_chars(codePointBegin, codePointEnd, codePointValue, radix);
				parseResult.ec != std::errc{} || parseResult.ptr != codePointEnd || !unicode::isValidCodePoint(static_cast<char32_t>(codePointValue))) {
				throw xml::Error{"Invalid code point.", codePointStringBegin, lineNumber};
			}
			const InplaceBuffer<char8_t, 4> codePointUTF8 = unicode::encodeUTF8FromCodePoint(static_cast<char32_t>(codePointValue));
			output.append(StringView{std::launder(reinterpret_cast<const char*>(codePointUTF8.data())), codePointUTF8.size()});
		} else {
			char character{};
			if (read("amp;")) {
				character = '&';
			} else if (read("lt;")) {
				character = '<';
			} else if (read("gt;")) {
				character = '>';
			} else if (read("apos;")) {
				character = '\'';
			} else if (read("quot;")) {
				character = '\"';
			} else {
				throw xml::Error{"Unknown reference.", it, lineNumber};
			}
			output.push_back(character);
		}
	}

	[[nodiscard]] String parseQuotedString() {
		if (it == end || (*it != '\'' && *it != '\"')) {
			throw xml::Error{"Missing quote.", it, lineNumber};
		}
		const char quoteCharacter = *it;
		++it;
		String result{};
		while (it != end && !read(quoteCharacter)) {
			if (*it == '&') {
				parseReference(result);
			} else if (*it == '\n') {
				result.push_back(*it++);
				++lineNumber;
			} else if (*it == '\r') {
				result.push_back(*it++);
				if (it != end && *it == '\n') {
					result.push_back(*it++);
				}
				++lineNumber;
			} else {
				result.push_back(*it++);
			}
		}
		return result;
	}

	[[nodiscard]] UniquePointer<Attribute> parseAttribute() {
		Attribute result{};
		result.name = parseName();
		skipWhitespace();
		if (read('=')) {
			skipWhitespace();
			result.value = parseQuotedString();
		}
		return UniquePointer<Attribute>::create(std::move(result));
	}

	void parseAttributes(Element& element) {
		UniquePointer<Attribute>* nextAttribute = &element.attributes;
		while (true) {
			skipWhitespace();
			if (it == end || *it == '>') {
				break;
			}
			if (it + 1 != end) {
				if (*it == '/' && *(it + 1) == '>') {
					break;
				}
				if (*it == '?' && *(it + 1) == '>') {
					break;
				}
			}
			*nextAttribute = parseAttribute();
			nextAttribute = &(*nextAttribute)->next;
		}
	}

	void commitContent(String& content, Element& element, UniquePointer<Element>*& nextChild) {
		while (!content.empty() && isWhitespace(content.back())) {
			content.pop_back();
		}
		if (!content.empty()) {
			if (element.children) {
				*nextChild = UniquePointer<Element>::create(Element{
					.tag{},
					.content = std::move(content),
					.attributes{},
					.children{},
					.next{},
				});
				nextChild = &(*nextChild)->next;
			} else {
				element.content = std::move(content);
			}
			content.clear();
		}
	}

	void parseContent(Element& element) {
		String content{};
		skipWhitespace();
		UniquePointer<Element>* nextChild = &element.children;
		while (true) {
			if (it == end) {
				if (!element.tag.empty()) {
					throw xml::Error{"Missing end tag.", it, lineNumber};
				}
				commitContent(content, element, nextChild);
				break;
			}
			if (read('<')) {
				commitContent(content, element, nextChild);
				if (read('/')) {
					const String tag = parseName();
					skipWhitespace();
					if (!read('>')) {
						throw xml::Error{"Invalid end tag.", it, lineNumber};
					}
					if (tag != element.tag) {
						throw xml::Error{"Incorrect end tag.", it, lineNumber};
					}
					break;
				}

				if (read('?')) {
					throw xml::Error{"Unknown processing instruction.", it, lineNumber};
				}

				if (read('!')) {
					if (read("--")) {
						while (it != end && !read("-->")) {
							++it;
						}
						skipWhitespace();
						continue;
					}
					throw xml::Error{"Unknown declaration.", it, lineNumber};
				}

				--it;
				*nextChild = parseElement();
				nextChild = &(*nextChild)->next;
				skipWhitespace();
			} else if (*it == '&') {
				parseReference(content);
			} else {
				content.push_back(*it);
				++it;
			}
		}
	}

	StringView::iterator it;
	StringView::iterator end;
	size_t lineNumber;
};

} // namespace

Document Document::parse(StringView xmlString) {
	Document result{};
	Parser parser{xmlString, 1};
	parser.skipWhitespace();
	result.declaration = parser.parseXMLDeclarationIfPresent();
	parser.skipWhitespace();
	result.root = parser.parseElement();
	return result;
}

} // namespace grem::xml
