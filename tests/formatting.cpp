// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/core/data/String.hpp>
#include <GREM/core/formatting.hpp>

#include <catch2/catch_test_macros.hpp> // TEST_CASE, SECTION, CHECK

// NOLINTBEGIN(misc-use-anonymous-namespace)

TEST_CASE("Format string", "[format]") {
	SECTION("String only") {
		const grem::String value = grem::formatString("Test string abc 123");
		const grem::String expectedValue{"Test string abc 123"};
		CHECK(value == expectedValue);
	}

	SECTION("Format string") {
		const grem::String value = grem::formatString("This is a string: {}. It's abc123.", "abc123");
		const grem::String expectedValue{"This is a string: abc123. It's abc123."};
		CHECK(value == expectedValue);
	}

	SECTION("Format string with positional argument") {
		const grem::String value = grem::formatString("This is a string: {0}. It's abc123.", "abc123");
		const grem::String expectedValue{"This is a string: abc123. It's abc123."};
		CHECK(value == expectedValue);
	}

	SECTION("Format int") {
		const grem::String value = grem::formatString("This is an int: {}. It's -42.", -42);
		const grem::String expectedValue{"This is an int: -42. It's -42."};
		CHECK(value == expectedValue);
	}

	SECTION("Format float") {
		const grem::String value = grem::formatString("This is a float: {}. It's -42.125.", -42.125f);
		const grem::String expectedValue{"This is a float: -42.125. It's -42.125."};
		CHECK(value == expectedValue);
	}

	SECTION("Format mixed string, int and float") {
		const grem::String value =
			grem::formatString("This is a string: {}. It's abc123. This is an int: {}. It's -42. This is a float: {}. It's -42.125.", "abc123", -42, -42.125f);
		const grem::String expectedValue{"This is a string: abc123. It's abc123. This is an int: -42. It's -42. This is a float: -42.125. It's -42.125."};
		CHECK(value == expectedValue);
	}

	SECTION("Format mixed string, int and float with positional arguments") {
		const grem::String value =
			grem::formatString("This is a string: {0}. It's abc123. This is an int: {1}. It's -42. This is a float: {2}. It's -42.125.", "abc123", -42, -42.125f);
		const grem::String expectedValue{"This is a string: abc123. It's abc123. This is an int: -42. It's -42. This is a float: -42.125. It's -42.125."};
		CHECK(value == expectedValue);
	}

	SECTION("Format mixed string, int and float with positional arguments, out of order") {
		const grem::String value =
			grem::formatString("This is a float: {2}. It's -42.125. This is a string: {0}. It's abc123. This is an int: {1}. It's -42.", "abc123", -42, -42.125f);
		const grem::String expectedValue{"This is a float: -42.125. It's -42.125. This is a string: abc123. It's abc123. This is an int: -42. It's -42."};
		CHECK(value == expectedValue);
	}
}

// NOLINTEND(misc-use-anonymous-namespace)
