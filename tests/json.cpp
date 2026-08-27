// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>

#include <catch2/catch_test_macros.hpp> // TEST_CASE, SECTION, CHECK
#include <cmath>                        // std::isinf, std::isnan, std::signbit
#include <limits>                       // std::numeric_limits
#include <sstream>                      // std::istringstream, std::ostringstream
#include <stdexcept>                    // std::runtime_error
#include <utility>                      // std::move

namespace json = grem::json;

using grem::formatString;
using grem::Optional;
using grem::String;
using grem::StringView;
using grem::UTF8StringView;

namespace {

[[nodiscard]] json::Value parseJSONValueFromString(UTF8StringView jsonString) {
	try {
		return json::Value::parse(jsonString);
	} catch (const json::Error& e) {
		throw std::runtime_error{formatString("Line {}, Column {}: {}", e.lineNumber, e.columnNumber, e.what())};
	}
}

[[nodiscard]] json::Value parseJSONValueFromInputStream(String jsonString) {
	jsonString.append("Can't-touch-this");
	std::istringstream stream{jsonString};
	stream.exceptions(std::istringstream::failbit | std::istringstream::badbit);
	try {
		json::Value result{};
		String subsequentString{};
		stream >> result >> subsequentString;
		CHECK(subsequentString == "Can't-touch-this");
		return result;
	} catch (const json::Error& e) {
		throw std::runtime_error{formatString("Line {}, Column {}: {}", e.lineNumber, e.columnNumber, e.what())};
	}
}

} // namespace

// NOLINTBEGIN(misc-use-anonymous-namespace)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc2y-extensions"
#endif

TEST_CASE("Parse UTF-8 string", "[json]") {
	SECTION("Valid input - Kitchen sink") {
		const json::Value value = parseJSONValueFromString(
			u8"\n"
			"// This is a single line comment.\n"
			"\n"
			"/* This is a multi-\n"
			"line comment. */\n"
			"\n"
			"{\n"
			"  // comments\n"
			"  unquoted: 'and you can quote me on that',\n"
			"  singleQuotes: 'I can use \"double quotes\" here',\n"
			"  lineBreaks: \"Look, Mom! \\\n"
			"No \\\\n's!\",\n"
			"  hexadecimal: 0xdecaf,\n"
			"  leadingDecimalPoint: .8675309, andTrailing: 8675309.,\n"
			"  positiveSign: +1,\n"
			"  trailingComma: 'in objects', andIn: ['arrays',],\n"
			"  \"backwardsCompatible\": \"with JSON\",\n"
			"}\n");
		const json::Value expectedValue = json::Object{
			{"unquoted", "and you can quote me on that"},
			{"singleQuotes", "I can use \"double quotes\" here"},
			{"lineBreaks", "Look, Mom! No \\n's!"},
			{"hexadecimal", 0xdecaf},
			{"leadingDecimalPoint", .8675309},
			{"andTrailing", 8675309.},
			{"positiveSign", +1},
			{"trailingComma", "in objects"},
			{"andIn", json::Array{"arrays"}},
			{"backwardsCompatible", "with JSON"},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Empty object") {
		const json::Value value = parseJSONValueFromString(u8"{}");
		const json::Value expectedValue = json::Object{};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Object with two properties and a trailing comma") {
		const json::Value value = parseJSONValueFromString(
			u8"{\n"
			"    width: 1920,\n"
			"    height: 1080,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"width", 1920},
			{"height", 1080},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Nested objects") {
		const json::Value value = parseJSONValueFromString(
			u8"{\n"
			"    image: {\n"
			"        width: 1920,\n"
			"        height: 1080,\n"
			"        'aspect-ratio': '16:9',\n"
			"    }\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"image", json::Object{{"width", 1920}, {"height", 1080}, {"aspect-ratio", "16:9"}}},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Array of objects") {
		const json::Value value = parseJSONValueFromString(
			u8"[\n"
			"    { name: 'Joe', age: 27 },\n"
			"    { name: 'Jane', age: 32 },\n"
			"]");
		const json::Value expectedValue = json::Array{
			json::Object{{"name", "Joe"}, {"age", 27}},
			json::Object{{"name", "Jane"}, {"age", 32}},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Empty array") {
		const json::Value value = parseJSONValueFromString(u8"[]");
		const json::Value expectedValue = json::Array{};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Array with three elements and a trailing comma") {
		const json::Value value = parseJSONValueFromString(
			u8"[\n"
			"    1,\n"
			"    true,\n"
			"    'three',\n"
			"]");
		const json::Value expectedValue = json::Array{
			1,
			true,
			"three",
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Nested arrays") {
		const json::Value value = parseJSONValueFromString(
			u8"[\n"
			"    [1, true, 'three'],\n"
			"    [4, \"five\", 0x6],\n"
			"]");
		const json::Value expectedValue = json::Array{
			json::Array{1, true, "three"},
			json::Array{4, "five", 0x6},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Line terminator in string") {
		const json::Value value = parseJSONValueFromString(
			u8"'Lorem ipsum dolor sit amet, \\\n"
			"consectetur adipiscing elit.'");
		const json::Value expectedValue = json::String{"Lorem ipsum dolor sit amet, consectetur adipiscing elit."};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Arbitrary escaped characters") {
		const json::Value value = parseJSONValueFromString(u8"'\\A\\C\\/\\D\\C'");
		const json::Value expectedValue = json::String{"AC/DC"};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Numbers") {
		const json::Value value = parseJSONValueFromString(
			u8"{\n"
			"    integer: 123,\n"
			"    withFractionPart: 123.45,\n"
			"    onlyFractionPart: .45,\n"
			"    withExponent: 123e-45,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"integer", 123},
			{"withFractionPart", 123.45},
			{"onlyFractionPart", .45},
			{"withExponent", 123e-45},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Hexadecimal numbers") {
		const json::Value value = parseJSONValueFromString(
			u8"{\n"
			"    positiveHex: 0xdecaf,\n"
			"    negativeHex: -0xC0FFEE,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"positiveHex", 0xdecaf},
			{"negativeHex", -0xC0FFEE},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Infinity and NaN") {
		const json::Value value = parseJSONValueFromString(
			u8"{\n"
			"    positiveInfinity: Infinity,\n"
			"    negativeInfinity: -Infinity,\n"
			"    notANumber: NaN,\n"
			"}");
		CHECK(std::isinf(value.getNumberProperty("positiveInfinity")));
		CHECK(!std::signbit(value.getNumberProperty("positiveInfinity")));
		CHECK(std::isinf(value.getNumberProperty("negativeInfinity")));
		CHECK(std::signbit(value.getNumberProperty("negativeInfinity")));
		CHECK(std::isnan(value.getNumberProperty("notANumber")));
		CHECK(!std::signbit(value.getNumberProperty("notANumber")));
	}
}

TEST_CASE("Parse ASCII input stream", "[json]") {
	SECTION("Valid input - Kitchen sink") {
		const json::Value value = parseJSONValueFromInputStream(
			"\n"
			"// This is a single line comment.\n"
			"\n"
			"/* This is a multi-\n"
			"line comment. */\n"
			"\n"
			"{\n"
			"  // comments\n"
			"  unquoted: 'and you can quote me on that',\n"
			"  singleQuotes: 'I can use \"double quotes\" here',\n"
			"  lineBreaks: \"Look, Mom! \\\n"
			"No \\\\n's!\",\n"
			"  hexadecimal: 0xdecaf,\n"
			"  leadingDecimalPoint: .8675309, andTrailing: 8675309.,\n"
			"  positiveSign: +1,\n"
			"  trailingComma: 'in objects', andIn: ['arrays',],\n"
			"  \"backwardsCompatible\": \"with JSON\",\n"
			"}\n");
		const json::Value expectedValue = json::Object{
			{"unquoted", "and you can quote me on that"},
			{"singleQuotes", "I can use \"double quotes\" here"},
			{"lineBreaks", "Look, Mom! No \\n's!"},
			{"hexadecimal", 0xdecaf},
			{"leadingDecimalPoint", .8675309},
			{"andTrailing", 8675309.},
			{"positiveSign", +1},
			{"trailingComma", "in objects"},
			{"andIn", json::Array{"arrays"}},
			{"backwardsCompatible", "with JSON"},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Empty object") {
		const json::Value value = parseJSONValueFromInputStream("{}");
		const json::Value expectedValue = json::Object{};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Object with two properties and a trailing comma") {
		const json::Value value = parseJSONValueFromInputStream(
			"{\n"
			"    width: 1920,\n"
			"    height: 1080,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"width", 1920},
			{"height", 1080},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Nested objects") {
		const json::Value value = parseJSONValueFromInputStream(
			"{\n"
			"    image: {\n"
			"        width: 1920,\n"
			"        height: 1080,\n"
			"        'aspect-ratio': '16:9',\n"
			"    }\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"image", json::Object{{"width", 1920}, {"height", 1080}, {"aspect-ratio", "16:9"}}},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Array of objects") {
		const json::Value value = parseJSONValueFromInputStream(
			"[\n"
			"    { name: 'Joe', age: 27 },\n"
			"    { name: 'Jane', age: 32 },\n"
			"]");
		const json::Value expectedValue = json::Array{
			json::Object{{"name", "Joe"}, {"age", 27}},
			json::Object{{"name", "Jane"}, {"age", 32}},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Empty array") {
		const json::Value value = parseJSONValueFromInputStream("[]");
		const json::Value expectedValue = json::Array{};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Array with three elements and a trailing comma") {
		const json::Value value = parseJSONValueFromInputStream(
			"[\n"
			"    1,\n"
			"    true,\n"
			"    'three',\n"
			"]");
		const json::Value expectedValue = json::Array{
			1,
			true,
			"three",
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Nested arrays") {
		const json::Value value = parseJSONValueFromInputStream(
			"[\n"
			"    [1, true, 'three'],\n"
			"    [4, \"five\", 0x6],\n"
			"]");
		const json::Value expectedValue = json::Array{
			json::Array{1, true, "three"},
			json::Array{4, "five", 0x6},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Line terminator in string") {
		const json::Value value = parseJSONValueFromInputStream(
			"'Lorem ipsum dolor sit amet, \\\n"
			"consectetur adipiscing elit.'");
		const json::Value expectedValue = json::String{"Lorem ipsum dolor sit amet, consectetur adipiscing elit."};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Arbitrary escaped characters") {
		const json::Value value = parseJSONValueFromInputStream("'\\A\\C\\/\\D\\C'");
		const json::Value expectedValue = json::String{"AC/DC"};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Numbers") {
		const json::Value value = parseJSONValueFromInputStream(
			"{\n"
			"    integer: 123,\n"
			"    withFractionPart: 123.45,\n"
			"    onlyFractionPart: .45,\n"
			"    withExponent: 123e-45,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"integer", 123},
			{"withFractionPart", 123.45},
			{"onlyFractionPart", .45},
			{"withExponent", 123e-45},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Hexadecimal numbers") {
		const json::Value value = parseJSONValueFromInputStream(
			"{\n"
			"    positiveHex: 0xdecaf,\n"
			"    negativeHex: -0xC0FFEE,\n"
			"}");
		const json::Value expectedValue = json::Object{
			{"positiveHex", 0xdecaf},
			{"negativeHex", -0xC0FFEE},
		};
		CHECK(value == expectedValue);
	}

	SECTION("Valid input - Infinity and NaN") {
		const json::Value value = parseJSONValueFromInputStream(
			"{\n"
			"    positiveInfinity: Infinity,\n"
			"    negativeInfinity: -Infinity,\n"
			"    notANumber: NaN,\n"
			"}");
		CHECK(std::isinf(value.getNumberProperty("positiveInfinity")));
		CHECK(!std::signbit(value.getNumberProperty("positiveInfinity")));
		CHECK(std::isinf(value.getNumberProperty("negativeInfinity")));
		CHECK(std::signbit(value.getNumberProperty("negativeInfinity")));
		CHECK(std::isnan(value.getNumberProperty("notANumber")));
		CHECK(!std::signbit(value.getNumberProperty("notANumber")));
	}
}

TEST_CASE("Output to ASCII string", "[json]") {
	SECTION("Kitchen sink") {
		const String string = json::Value{
			json::Object{
				{"unquoted", "and you can quote me on that"},
				{"singleQuotes", "I can use \"double quotes\" here"},
				{"lineBreaks", "Look, Mom! No \\n's!"},
				{"hexadecimal", 0xdecaf},
				{"leadingDecimalPoint", .8675309},
				{"andTrailing", 8675309.},
				{"positiveSign", +1},
				{"trailingComma", "in objects"},
				{"andIn", json::Array{"arrays"}},
				{"backwardsCompatible", "with JSON"},
			}}.toString();
		constexpr StringView EXPECTED_STRING =
			"{\r\n"
			"    \"andIn\": [\"arrays\"],\r\n"
			"    \"andTrailing\": 8675309,\r\n"
			"    \"backwardsCompatible\": \"with JSON\",\r\n"
			"    \"hexadecimal\": 912559,\r\n"
			"    \"leadingDecimalPoint\": 0.8675309,\r\n"
			"    \"lineBreaks\": \"Look, Mom! No \\\\n's!\",\r\n"
			"    \"positiveSign\": 1,\r\n"
			"    \"singleQuotes\": \"I can use \\\"double quotes\\\" here\",\r\n"
			"    \"trailingComma\": \"in objects\",\r\n"
			"    \"unquoted\": \"and you can quote me on that\"\r\n"
			"}";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Empty object") {
		const String string = json::Value{json::Object{}}.toString();
		constexpr StringView EXPECTED_STRING = "{}";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Object with two properties") {
		const String string = json::Value{
			json::Object{
				{"width", 1920},
				{"height", 1080},
			}}.toString();
		constexpr StringView EXPECTED_STRING = "{ \"height\": 1080, \"width\": 1920 }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Nested objects") {
		const String string = json::Value{
			json::Object{
				{"image", json::Object{{"width", 1920}, {"height", 1080}, {"aspect-ratio", "16:9"}}},
			}}.toString();
		constexpr StringView EXPECTED_STRING = "{ \"image\": { \"aspect-ratio\": \"16:9\", \"height\": 1080, \"width\": 1920 } }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Array of objects") {
		const String string = json::Value{
			json::Array{
				json::Object{{"name", "Joe"}, {"age", 27}},
				json::Object{{"name", "Jane"}, {"age", 32}},
			}}.toString();
		constexpr StringView EXPECTED_STRING =
			"[\r\n"
			"    { \"age\": 27, \"name\": \"Joe\" },\r\n"
			"    { \"age\": 32, \"name\": \"Jane\" }\r\n"
			"]";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Empty array") {
		const String string = json::Value{json::Array{}}.toString();
		constexpr StringView EXPECTED_STRING = "[]";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Array with three elements") {
		String string = json::Value{json::Array{1, true, "three"}}.toString();
		constexpr StringView EXPECTED_STRING = "[1, true, \"three\"]";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Nested arrays") {
		const String string = json::Value{
			json::Array{
				json::Array{1, true, "three"},
				json::Array{4, "five", 0x6},
			}}.toString();
		constexpr StringView EXPECTED_STRING =
			"[\r\n"
			"    [1, true, \"three\"],\r\n"
			"    [4, \"five\", 6]\r\n"
			"]";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Line terminator in string") {
		const String string = json::Value{"Lorem ipsum dolor sit amet, \nconsectetur adipiscing elit."}.toString();
		constexpr StringView EXPECTED_STRING = "\"Lorem ipsum dolor sit amet, \\nconsectetur adipiscing elit.\"";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Arbitrary escaped characters") {
		const String string = json::Value{"'\\A\\C\\/\\D\\C'"}.toString();
		constexpr StringView EXPECTED_STRING = "\"'\\\\A\\\\C\\\\/\\\\D\\\\C'\"";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Numbers") {
		const String string = json::Value{
			json::Object{
				{"integer", 123},
				{"withFractionPart", 123.45},
				{"onlyFractionPart", .45},
				{"withExponent", 123e-45},
			}}.toString();
		constexpr StringView EXPECTED_STRING = "{ \"integer\": 123, \"onlyFractionPart\": 0.45, \"withExponent\": 1.23e-43, \"withFractionPart\": 123.45 }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Hexadecimal numbers") {
		const String string = json::Value{
			json::Object{
				{"positiveHex", 0xdecaf},
				{"negativeHex", -0xC0FFEE},
			}}.toString();
		constexpr StringView EXPECTED_STRING = "{ \"negativeHex\": -12648430, \"positiveHex\": 912559 }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Infinity and NaN") {
		const String string = json::Value{
			json::Object{
				{"positiveInfinity", std::numeric_limits<json::Number>::infinity()},
				{"negativeInfinity", -std::numeric_limits<json::Number>::infinity()},
				{"notANumber", std::numeric_limits<json::Number>::quiet_NaN()},
			}}.toString();
		constexpr StringView EXPECTED_STRING = "{ \"negativeInfinity\": -Infinity, \"notANumber\": NaN, \"positiveInfinity\": Infinity }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Small object with large nested objects") {
		const String string = json::Value{
			json::Object{
				{"nested",
					json::Object{
						{"a", 1},
						{"b", 2},
						{"c", 3},
						{"d", 4},
						{"e", 5},
					}},
			}}.toString();
		constexpr StringView EXPECTED_STRING =
			"{\r\n"
			"    \"nested\": {\r\n"
			"        \"a\": 1,\r\n"
			"        \"b\": 2,\r\n"
			"        \"c\": 3,\r\n"
			"        \"d\": 4,\r\n"
			"        \"e\": 5\r\n"
			"    }\r\n"
			"}";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Large object with small nested objects") {
		const String string = json::Value{
			json::Object{
				{"nested1", json::Object{{"a", 1}, {"b", 2}}},
				{"nested2", json::Object{{"a", 1}, {"b", 2}}},
				{"nested3", json::Object{{"a", 1}, {"b", 2}}},
				{"nested4", json::Object{{"a", 1}, {"b", 2}}},
				{"nested5", json::Object{{"a", 1}, {"b", 2}}},
			}}.toString();
		constexpr StringView EXPECTED_STRING =
			"{\r\n"
			"    \"nested1\": { \"a\": 1, \"b\": 2 },\r\n"
			"    \"nested2\": { \"a\": 1, \"b\": 2 },\r\n"
			"    \"nested3\": { \"a\": 1, \"b\": 2 },\r\n"
			"    \"nested4\": { \"a\": 1, \"b\": 2 },\r\n"
			"    \"nested5\": { \"a\": 1, \"b\": 2 }\r\n"
			"}";
		CHECK(string == EXPECTED_STRING);
	}
}

TEST_CASE("Serialize to ASCII stream", "[json]") {
	SECTION("Non-null string") {
		const char* const input = "Some\ntext";
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "\"Some\\ntext\"";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Null string") {
		const char* const input = nullptr;
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "null";
		CHECK(string == EXPECTED_STRING);
	}

	struct Aggregate {
		int x = 123;
		double y = -5.3;
		String z = "abc";
	};

	SECTION("Aggregate") {
		const Aggregate input{};
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "{ \"x\": 123, \"y\": -5.3, \"z\": \"abc\" }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Non-null optional aggregate") {
		const Optional<Aggregate> input = Aggregate{};
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "{ \"x\": 123, \"y\": -5.3, \"z\": \"abc\" }";
		CHECK(string == EXPECTED_STRING);
	}

	SECTION("Null optional aggregate") {
		const Optional<Aggregate> input{};
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "null";
		CHECK(string == EXPECTED_STRING);
	}

	struct SingleFieldAggregate {
		int x = 123;
	};

	SECTION("Single-field aggregate") {
		const SingleFieldAggregate input{};
		std::ostringstream stream{};
		json::serialize(stream, input, {});
		const String string = std::move(stream).str();
		constexpr StringView EXPECTED_STRING = "{ \"x\": 123 }";
		CHECK(string == EXPECTED_STRING);
	}
}

TEST_CASE("Deserialize from ASCII stream - pretty printed", "[json]") {
	SECTION("Kitchen sink") {
		const json::Value valueA{json::Object{
			{"unquoted", "and you can quote me on that"},
			{"singleQuotes", "I can use \"double quotes\" here"},
			{"lineBreaks", "Look, Mom! No \\n's!"},
			{"hexadecimal", 0xdecaf},
			{"leadingDecimalPoint", .8675309},
			{"andTrailing", 8675309.},
			{"positiveSign", +1},
			{"trailingComma", "in objects"},
			{"andIn", json::Array{"arrays"}},
			{"backwardsCompatible", "with JSON"},
		}};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Value valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Empty object") {
		const json::Object valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Object with two properties") {
		const json::Object valueA{
			{"width", 1920},
			{"height", 1080},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Nested objects") {
		const json::Object valueA{
			{"image", json::Object{{"width", 1920}, {"height", 1080}, {"aspect-ratio", "16:9"}}},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Array of objects") {
		const json::Array valueA{
			json::Object{{"name", "Joe"}, {"age", 27}},
			json::Object{{"name", "Jane"}, {"age", 32}},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Empty array") {
		const json::Array valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Array with three elements") {
		const json::Array valueA{1, true, "three"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Nested arrays") {
		const json::Array valueA{
			json::Array{1, true, "three"},
			json::Array{4, "five", 0x6},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Line terminator in string") {
		const String valueA{"Lorem ipsum dolor sit amet, \nconsectetur adipiscing elit."};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		String valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Arbitrary escaped characters") {
		const String valueA{"'\\A\\C\\/\\D\\C'"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		String valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Numbers") {
		const json::Object valueA{
			{"integer", 123},
			{"withFractionPart", 123.45},
			{"onlyFractionPart", .45},
			{"withExponent", 123e-45},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Hexadecimal numbers") {
		const json::Object valueA{
			{"positiveHex", 0xdecaf},
			{"negativeHex", -0xC0FFEE},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Infinity and NaN") {
		const json::Object valueA{
			{"positiveInfinity", std::numeric_limits<json::Number>::infinity()},
			{"negativeInfinity", -std::numeric_limits<json::Number>::infinity()},
			{"positiveNotANumber", std::numeric_limits<json::Number>::quiet_NaN()},
			{"negativeNotANumber", -std::numeric_limits<json::Number>::quiet_NaN()},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(std::isinf(valueB.at("positiveInfinity").getNumber()));
		CHECK(!std::signbit(valueB.at("positiveInfinity").getNumber()));
		CHECK(std::isinf(valueB.at("negativeInfinity").getNumber()));
		CHECK(std::signbit(valueB.at("negativeInfinity").getNumber()));
		CHECK(std::isnan(valueB.at("positiveNotANumber").getNumber()));
		CHECK(!std::signbit(valueB.at("positiveNotANumber").getNumber()));
		CHECK(std::isnan(valueB.at("negativeNotANumber").getNumber()));
		CHECK(std::signbit(valueB.at("negativeNotANumber").getNumber()));
	}

	struct Aggregate {
		int x = 0;
		double y = 0.0;
		String z{};

		[[nodiscard]] bool operator==(const Aggregate& other) const = default;
	};

	SECTION("Aggregate") {
		const Aggregate valueA{.x = 123, .y = -5.3, .z = "abc"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		Aggregate valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Non-null optional aggregate") {
		const Optional<Aggregate> valueA = Aggregate{.x = 123, .y = -5.3, .z = "abc"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		Optional<Aggregate> valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Null optional aggregate") {
		const Optional<Aggregate> valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		Optional<Aggregate> valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	struct SingleFieldAggregate {
		int x = 0;

		[[nodiscard]] bool operator==(const SingleFieldAggregate& other) const = default;
	};

	SECTION("Single-field aggregate") {
		const SingleFieldAggregate valueA{.x = 123};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = true});
		std::istringstream streamB{std::move(streamA).str()};
		SingleFieldAggregate valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}
}

TEST_CASE("Deserialize from ASCII stream - not pretty printed", "[json]") {
	SECTION("Kitchen sink") {
		const json::Value valueA{json::Object{
			{"unquoted", "and you can quote me on that"},
			{"singleQuotes", "I can use \"double quotes\" here"},
			{"lineBreaks", "Look, Mom! No \\n's!"},
			{"hexadecimal", 0xdecaf},
			{"leadingDecimalPoint", .8675309},
			{"andTrailing", 8675309.},
			{"positiveSign", +1},
			{"trailingComma", "in objects"},
			{"andIn", json::Array{"arrays"}},
			{"backwardsCompatible", "with JSON"},
		}};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Value valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Empty object") {
		const json::Object valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Object with two properties") {
		const json::Object valueA{
			{"width", 1920},
			{"height", 1080},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Nested objects") {
		const json::Object valueA{
			{"image", json::Object{{"width", 1920}, {"height", 1080}, {"aspect-ratio", "16:9"}}},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Array of objects") {
		const json::Array valueA{
			json::Object{{"name", "Joe"}, {"age", 27}},
			json::Object{{"name", "Jane"}, {"age", 32}},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Empty array") {
		const json::Array valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Array with three elements") {
		const json::Array valueA{1, true, "three"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Nested arrays") {
		const json::Array valueA{
			json::Array{1, true, "three"},
			json::Array{4, "five", 0x6},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Array valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Line terminator in string") {
		const String valueA{"Lorem ipsum dolor sit amet, \nconsectetur adipiscing elit."};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		String valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Arbitrary escaped characters") {
		const String valueA{"'\\A\\C\\/\\D\\C'"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		String valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Numbers") {
		const json::Object valueA{
			{"integer", 123},
			{"withFractionPart", 123.45},
			{"onlyFractionPart", .45},
			{"withExponent", 123e-45},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Hexadecimal numbers") {
		const json::Object valueA{
			{"positiveHex", 0xdecaf},
			{"negativeHex", -0xC0FFEE},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Infinity and NaN") {
		const json::Object valueA{
			{"positiveInfinity", std::numeric_limits<json::Number>::infinity()},
			{"negativeInfinity", -std::numeric_limits<json::Number>::infinity()},
			{"positiveNotANumber", std::numeric_limits<json::Number>::quiet_NaN()},
			{"negativeNotANumber", -std::numeric_limits<json::Number>::quiet_NaN()},
		};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		json::Object valueB{};
		json::deserialize(streamB, valueB);
		CHECK(std::isinf(valueB.at("positiveInfinity").getNumber()));
		CHECK(!std::signbit(valueB.at("positiveInfinity").getNumber()));
		CHECK(std::isinf(valueB.at("negativeInfinity").getNumber()));
		CHECK(std::signbit(valueB.at("negativeInfinity").getNumber()));
		CHECK(std::isnan(valueB.at("positiveNotANumber").getNumber()));
		CHECK(!std::signbit(valueB.at("positiveNotANumber").getNumber()));
		CHECK(std::isnan(valueB.at("negativeNotANumber").getNumber()));
		CHECK(std::signbit(valueB.at("negativeNotANumber").getNumber()));
	}

	struct Aggregate {
		int x = 0;
		double y = 0.0;
		String z{};

		[[nodiscard]] bool operator==(const Aggregate& other) const = default;
	};

	SECTION("Aggregate") {
		const Aggregate valueA{.x = 123, .y = -5.3, .z = "abc"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		Aggregate valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Non-null optional aggregate") {
		const Optional<Aggregate> valueA = Aggregate{.x = 123, .y = -5.3, .z = "abc"};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		Optional<Aggregate> valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	SECTION("Null optional aggregate") {
		const Optional<Aggregate> valueA{};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		Optional<Aggregate> valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}

	struct SingleFieldAggregate {
		int x = 0;

		[[nodiscard]] bool operator==(const SingleFieldAggregate& other) const = default;
	};

	SECTION("Single-field aggregate") {
		const SingleFieldAggregate valueA{.x = 123};
		std::ostringstream streamA{};
		json::serialize(streamA, valueA, {.prettyPrint = false});
		std::istringstream streamB{std::move(streamA).str()};
		SingleFieldAggregate valueB{};
		json::deserialize(streamB, valueB);
		CHECK(valueA == valueB);
	}
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// NOLINTEND(misc-use-anonymous-namespace)

/*
NOTE:
  Some of the test cases above are based on examples from the
  JSON5 specification version 1.0.0 at https://spec.json5.org/
  which was published under the following license:

The MIT License (MIT)

Copyright (c) 2017 Aseem Kishore, Jordan Tucker

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*/
