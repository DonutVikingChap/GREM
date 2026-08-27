// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_COMMAND_LINE_INTERFACE_HPP
#define GREM_CORE_COMMAND_LINE_INTERFACE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/formats/ascii.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/time.hpp>

#include <charconv>     // std::from_chars_result, std::from_chars
#include <cstdio>       // std::sscanf
#include <iterator>     // std::size
#include <stdexcept>    // std::invalid_argument
#include <system_error> // std::errc
#include <type_traits>  // std::remove_cvref_t, std::false_type, std::true_type, std::bool_constant, std::integral_constant
#include <utility>      // std::move, std::forward, std::declval

namespace grem::cli {

namespace detail {

template <typename T>
concept convertible_to_boolean_reference =
	convertible_to<T&, bool&> || convertible_to<T&, bool8_t&> || convertible_to<T&, bool16_t&> || convertible_to<T&, bool32_t&> || convertible_to<T&, bool64_t&>;

template <typename T>
concept convertible_to_integer_reference =
	convertible_to<T&, signed char&> || convertible_to<T&, short&> || convertible_to<T&, int&> || convertible_to<T&, long&> || convertible_to<T&, long long&> ||
	convertible_to<T&, unsigned char&> || convertible_to<T&, unsigned short&> || convertible_to<T&, unsigned&> || convertible_to<T&, unsigned long&> ||
	convertible_to<T&, unsigned long long&>;

template <typename T>
concept convertible_to_character_reference =
	convertible_to<T&, char&> || convertible_to<T&, wchar_t&> || convertible_to<T&, char8_t&> || convertible_to<T&, char16_t&> || convertible_to<T&, char32_t&>;

template <typename T>
concept optional_argument = requires(T t) {
	t.reset();
	t.emplace();
	t.has_value();
	t.value();
};

template <typename T>
concept quantity_argument = requires(T t, const float value) {
	t = T::reinterpret(value);
	static_cast<float>(t.in(T::UNIT));
} && !optional_argument<T>;

template <typename T>
concept double_argument = requires(T t, const double value) { t = value; } && !convertible_to<T&, float&> && !optional_argument<T> && !quantity_argument<T> &&
                          !convertible_to_character_reference<T> && !convertible_to_integer_reference<T> && !convertible_to_boolean_reference<T> && !convertible_to<T&, String&>;
static_assert(double_argument<double>);

template <typename T>
concept float_argument = requires(T t, const float value) { t = value; } && (!double_argument<T> || convertible_to<T&, float&>) && !optional_argument<T> && !quantity_argument<T> &&
                         !convertible_to_character_reference<T> && !convertible_to_integer_reference<T> && !convertible_to_boolean_reference<T> && !convertible_to<T&, String&>;
static_assert(float_argument<float>);

template <typename T>
concept integer_argument = convertible_to_integer_reference<T> || (requires(T t, const int64_t value) {
	static_cast<int64_t>(t);
	t = static_cast<T>(value);
} && !optional_argument<T> && !quantity_argument<T> && !double_argument<T> && !float_argument<T> && !convertible_to_character_reference<T> && !convertible_to_boolean_reference<T>);
static_assert(integer_argument<int8_t>);
static_assert(integer_argument<int64_t>);
static_assert(integer_argument<uint8_t>);
static_assert(integer_argument<uint64_t>);

template <typename T>
concept character_argument = convertible_to_character_reference<T> || (requires(T t, const char value) {
	static_cast<char>(t);
	t = static_cast<T>(value);
} && !optional_argument<T> && !quantity_argument<T> && !double_argument<T> && !float_argument<T> && !convertible_to_integer_reference<T> && !convertible_to_boolean_reference<T>);
static_assert(character_argument<char>);
static_assert(character_argument<wchar_t>);
static_assert(character_argument<char8_t>);
static_assert(character_argument<char16_t>);
static_assert(character_argument<char32_t>);

template <typename T>
concept string_argument = requires(T t, const char* const string) { t = string; } && !optional_argument<T> && !quantity_argument<T> && !double_argument<T> && !float_argument<T> &&
                          !integer_argument<T> && !character_argument<T> && !convertible_to_boolean_reference<T>;
static_assert(string_argument<String>);
static_assert(string_argument<CStringView>);

template <typename T>
concept boolean_argument =
	requires(T t, const bool value) {
		static_cast<bool>(t);
		t = value;
	} && !optional_argument<T> && !quantity_argument<T> &&
	((!double_argument<T> && !float_argument<T> && !integer_argument<T> && !character_argument<T> && !string_argument<T>) || convertible_to_boolean_reference<T>);
static_assert(boolean_argument<bool>);
static_assert(boolean_argument<bool16_t>);
static_assert(boolean_argument<bool32_t>);
static_assert(boolean_argument<bool64_t>);

template <typename T>
concept list_argument = requires(T t) {
	t.emplace_back();
	t.back();
} && !string_argument<T>;

template <typename T>
concept array_argument = requires(T t, const size_t index) {
	t[index];
	std::size(t);
} && !string_argument<T> && !list_argument<T>;

template <typename T>
concept duration_argument = requires {
	typename T::rep;
	typename T::period;
};

template <typename T>
[[nodiscard]] constexpr auto& getIntegerReference(T& value) noexcept {
	if constexpr (integral<T>) {
		return value;
	} else if constexpr (convertible_to<T&, int64_t&>) {
		return static_cast<int64_t&>(value);
	} else if constexpr (convertible_to<T&, uint64_t&>) {
		return static_cast<uint64_t&>(value);
	} else if constexpr (convertible_to<T&, int32_t&>) {
		return static_cast<int32_t&>(value);
	} else if constexpr (convertible_to<T&, uint32_t&>) {
		return static_cast<uint32_t&>(value);
	} else if constexpr (convertible_to<T&, int16_t&>) {
		return static_cast<int16_t&>(value);
	} else if constexpr (convertible_to<T&, uint16_t&>) {
		return static_cast<uint16_t&>(value);
	} else if constexpr (convertible_to<T&, int8_t&>) {
		return static_cast<int8_t&>(value);
	} else if constexpr (convertible_to<T&, uint8_t&>) {
		return static_cast<uint8_t&>(value);
	} else {
		unreachable();
	}
}

template <typename T>
[[nodiscard]] constexpr auto& getCharacterReference(T& value) noexcept {
	if constexpr (convertible_to<T&, char&>) {
		return static_cast<char&>(value);
	} else if constexpr (convertible_to<T&, wchar_t&>) {
		return static_cast<wchar_t&>(value);
	} else if constexpr (convertible_to<T&, char8_t&>) {
		return static_cast<char8_t&>(value);
	} else if constexpr (convertible_to<T&, char16_t&>) {
		return static_cast<char16_t&>(value);
	} else if constexpr (convertible_to<T&, char32_t&>) {
		return static_cast<char32_t&>(value);
	} else {
		unreachable();
	}
}

template <typename T>
struct is_vector_argument : std::false_type {};

template <size_t N, typename T>
struct is_vector_argument<vec<N, T>> : std::true_type {};

template <quantity_argument Quantity>
struct is_vector_argument<Quantity> : std::bool_constant<Quantity::RANK >= 2> {};

template <typename T>
inline constexpr bool is_vector_argument_v = is_vector_argument<T>::value;

template <typename T>
struct vector_argument_size;

template <size_t N, typename T>
struct vector_argument_size<vec<N, T>> : std::integral_constant<size_t, N> {};

template <quantity_argument Quantity>
struct vector_argument_size<Quantity> : std::integral_constant<size_t, Quantity::RANK> {};

template <typename T>
inline constexpr size_t vector_argument_size_v = vector_argument_size<T>::value;

} // namespace detail

/**
 * Base exception type for errors originating from the command line interface.
 */
struct Error : grem::Error {
	using grem::Error::Error;
};

/**
 * Exception type thrown by the command line interface containing a help string
 * that was requested by the user.
 */
struct Help : cli::Error {
	using cli::Error::Error;
};

/**
 * Exception type thrown by the command line interface when an incorrect number
 * of arguments were provided.
 */
struct InvalidUsage : cli::Error {
	using cli::Error::Error;
};

/**
 * Exception type thrown by the command line interface when an invalid argument
 * value was provided.
 */
struct InvalidArgument : cli::Error {
	using cli::Error::Error;
};

/**
 * Exception type thrown by the command line interface when an unknown option
 * was specified.
 */
struct UnknownOption : cli::Error {
	using cli::Error::Error;
};

/**
 * Exception type thrown by the command line interface when an invalid option
 * value was provided.
 */
struct InvalidOptionValue : cli::Error {
	using cli::Error::Error;
};

/**
 * Proxy type for making command line option fields that reference other values.
 */
template <typename T>
class Proxy {
public:
	constexpr Proxy() noexcept = default;

	constexpr Proxy(T& reference) noexcept
		: pointer(&reference) {}

	constexpr ~Proxy() = default;

	Proxy(const Proxy&) = delete;
	Proxy(Proxy&&) = delete;
	Proxy& operator=(Proxy&&) = delete;
	Proxy& operator=(const Proxy&) = delete;

	template <typename Arg>
	decltype(auto) operator=(Arg&& arg) // NOLINT(cppcoreguidelines-c-copy-assignment-signature, misc-unconventional-assign-operator)
		requires(requires(T t) { t = std::forward<Arg>(arg); }) {
		GREM_ASSERT(pointer);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
		return *pointer = std::forward<Arg>(arg);
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
	}

	explicit operator bool() const noexcept requires(requires(const T t) { static_cast<bool>(t); } && !requires(const T t) { static_cast<const bool&>(t); }) {
		GREM_ASSERT(pointer);
		return static_cast<bool>(*pointer);
	}

	operator T&() noexcept {
		GREM_ASSERT(pointer);
		return *pointer;
	}

	operator const T&() const noexcept {
		GREM_ASSERT(pointer);
		return *pointer;
	}

	void reset() noexcept requires(requires(T t) { t.reset(); }) {
		GREM_ASSERT(pointer);
		pointer->reset();
	}

	decltype(auto) emplace() requires(requires(T t) { t.emplace(); }) {
		GREM_ASSERT(pointer);
		return pointer->emplace();
	}

	[[nodiscard]] bool has_value() const noexcept requires(requires(const T t) { t.has_value(); }) {
		GREM_ASSERT(pointer);
		return pointer->has_value();
	}

	[[nodiscard]] decltype(auto) value() requires(requires(T t) { t.value(); }) {
		GREM_ASSERT(pointer);
		return pointer->value();
	}

	[[nodiscard]] decltype(auto) value() const requires(requires(const T t) { t.value(); }) {
		GREM_ASSERT(pointer);
		return pointer->value();
	}

	[[nodiscard]] decltype(auto) emplace_back() requires(requires(T t) { t.emplace_back(); }) {
		GREM_ASSERT(pointer);
		return pointer->emplace_back();
	}

	[[nodiscard]] decltype(auto) back() requires(requires(T t) { t.back(); }) {
		GREM_ASSERT(pointer);
		return pointer->back();
	}

	[[nodiscard]] decltype(auto) back() const requires(requires(const T t) { t.back(); }) {
		GREM_ASSERT(pointer);
		return pointer->back();
	}

	[[nodiscard]] decltype(auto) size() const noexcept requires(requires(const T t) { std::size(t); }) {
		GREM_ASSERT(pointer);
		return std::size(*pointer);
	}

	[[nodiscard]] decltype(auto) operator[](size_t index) requires(requires(T t, const size_t i) { t[i]; }) {
		GREM_ASSERT(pointer);
		return (*pointer)[index];
	}

	[[nodiscard]] decltype(auto) operator[](size_t index) const requires(requires(const T t, const size_t i) { t[i]; }) {
		GREM_ASSERT(pointer);
		return (*pointer)[index];
	}

private:
	T* pointer = nullptr;
};

/**
 * Configuration options for command line parsing.
 */
struct CommandLineParserOptions {
	/**
	 * Argument prefix that distinguishes short, single-character options from
	 * regular arguments.
	 *
	 * If empty or equal to #longOptionPrefix, short options are disabled.
	 */
	StringView shortOptionPrefix = "-";

	/**
	 * Argument prefix that distinguishes long, multi-character options from
	 * regular arguments.
	 *
	 * Must not be empty.
	 */
	StringView longOptionPrefix = "--";

	/**
	 * Argument that separates the main arguments of the command from any extra
	 * trailing arguments.
	 *
	 * If empty, extra arguments will not be allowed.
	 */
	StringView extraArgumentsSeparator{};

	/**
	 * Don't take explicit values for boolean options whose default values are
	 * `false`, and instead interpret them as being set to `true` whenever the
	 * option is specified.
	 *
	 * \note Boolean options whose default values are `true` always require
	 *       explicit values regardless of this option.
	 */
	bool useImplicitBooleanValues = true;

	/**
	 * Allow short, single-character boolean options to be grouped into a single
	 * argument.
	 *
	 * \note If #shortOptionPrefix is empty or equal to #longOptionPrefix, or if
	 *       #useImplicitBooleanValues is false, this option has no effect.
	 */
	bool allowShortOptionGrouping = true;

	/**
	 * Allow option values to be specified using `option=value` syntax as a
	 * single argument, in addition to the regular `option value` syntax.
	 */
	bool allowEqualsValueSyntax = true;

	/**
	 * Throw an error with a help string for the user if the "help" option is
	 * supplied.
	 */
	bool enableHelpOption = true;
};

/**
 * Parse a command line argument into a boolean value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::boolean_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	if (argument == "1" || ascii::caseInsensitiveEqual(argument, "true") || ascii::caseInsensitiveEqual(argument, "on") || ascii::caseInsensitiveEqual(argument, "y") ||
		ascii::caseInsensitiveEqual(argument, "yes")) {
		output = true;
	} else if (argument == "0" || ascii::caseInsensitiveEqual(argument, "false") || ascii::caseInsensitiveEqual(argument, "off") || ascii::caseInsensitiveEqual(argument, "n") ||
			   ascii::caseInsensitiveEqual(argument, "no")) {
		output = false;
	} else {
		throw std::invalid_argument{"Expected a boolean value (true/false)."};
	}
}

/**
 * Parse a command line argument into an integer value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::integer_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	if constexpr (integral<T> || detail::convertible_to_integer_reference<T>) {
		auto& integerReference = detail::getIntegerReference(output);
		const std::from_chars_result parseResult = std::from_chars(argument.data(), argument.data() + argument.size(), integerReference);
		if (parseResult.ec != std::errc{}) {
			if constexpr (unsigned_integral<std::remove_cvref_t<decltype(integerReference)>>) {
				throw std::invalid_argument{"Expected a non-negative integer."};
			} else {
				throw std::invalid_argument{"Expected an integer."};
			}
		}
	} else {
		int64_t value = 0;
		const std::from_chars_result parseResult = std::from_chars(argument.data(), argument.data() + argument.size(), value);
		if (parseResult.ec != std::errc{}) {
			throw std::invalid_argument{"Expected an integer."};
		}
		output = static_cast<T>(value);
	}
}

/**
 * Parse a command line argument into a character value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::character_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	if constexpr (detail::convertible_to_character_reference<T>) {
		auto& characterReference = detail::getCharacterReference(output);
		using X = std::remove_cvref_t<decltype(characterReference)>;
		if (argument.size() != sizeof(X)) {
			throw std::invalid_argument{"Expected a character."};
		}
		memcpy(&characterReference, argument.data(), sizeof(X));
	} else {
		if (argument.size() != 1) {
			throw std::invalid_argument{"Expected a character."};
		}
		output = static_cast<T>(argument.front());
	}
}

/**
 * Parse a command line argument into a single-precision floating-point value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::float_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	float value = 0.0f;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	if (std::sscanf(argument.c_str(), "%f", &value) != 1) {
		throw std::invalid_argument{"Expected a number."};
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	output = value;
}

/**
 * Parse a command line argument into a double-precision floating-point value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::double_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	double value = 0.0;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	if (std::sscanf(argument.c_str(), "%lf", &value) != 1) {
		throw std::invalid_argument{"Expected a number."};
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	output = value;
}

/**
 * Parse a command line argument into a single-precision floating-point
 * quantity.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::quantity_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	float value = 0.0f;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	if (std::sscanf(argument.c_str(), "%f", &value) != 1) {
		throw std::invalid_argument{"Expected a number."};
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
	output = T::reinterpret(value);
}

/**
 * Parse a command line argument into a string value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws any exception thrown by the assignment operator of the output type.
 */
template <detail::string_argument T>
inline void parseArgumentValue(T& output, CStringView argument) {
	output = argument.c_str();
}

/**
 * Parse a command line argument into a duration value.
 *
 * \param output value to parse the argument into.
 * \param argument argument to parse.
 *
 * \throws std::invalid_argument on failure to parse a valid value from the
 *         argument.
 */
template <typename Rep, typename Period>
inline void parseArgumentValue(DurationBase<Rep, Period>& output, CStringView argument) {
	double milliseconds{};
	parseArgumentValue(milliseconds, argument);
	output = duration_cast<DurationBase<Rep, Period>>(DurationBase<double, Ratio<1, 1'000>>{milliseconds});
}

namespace detail {

struct NoArguments {};
struct NoOptions {};

struct ArgumentCounts {
	size_t leadingArgumentCount = 0;
	size_t trailingArgumentCount = 0;
	size_t maxArgumentCount = 0;
};

template <typename Arguments>
[[nodiscard]] inline ArgumentCounts getArgumentCounts(const Arguments& outputArguments) {
	ArgumentCounts result{};
	meta::forEachField(outputArguments, [&]<typename T>(const T& argument) -> void {
		if constexpr (detail::is_vector_argument_v<T>) {
			for (size_t i = 0; i < detail::vector_argument_size_v<T>; ++i) {
				if (result.leadingArgumentCount == result.maxArgumentCount) {
					++result.leadingArgumentCount;
					++result.maxArgumentCount;
				} else {
					++result.trailingArgumentCount;
					if (result.maxArgumentCount != Limits<size_t>::MAX) {
						++result.maxArgumentCount;
					}
				}
			}
		} else if constexpr (aggregate<T>) {
			const ArgumentCounts nestedArgumentCounts = getArgumentCounts(argument);
			if (nestedArgumentCounts.leadingArgumentCount == nestedArgumentCounts.maxArgumentCount) {
				const size_t size = nestedArgumentCounts.maxArgumentCount;
				if (result.leadingArgumentCount == result.maxArgumentCount) {
					result.leadingArgumentCount += size;
					result.maxArgumentCount += size;
				} else {
					result.trailingArgumentCount += size;
					if (result.maxArgumentCount != Limits<size_t>::MAX) {
						result.maxArgumentCount += size;
					}
				}
			} else if (nestedArgumentCounts.maxArgumentCount == Limits<size_t>::MAX) {
				GREM_ASSERT(result.leadingArgumentCount == result.maxArgumentCount && "Command line may only contain one range of optional arguments.");
				result.maxArgumentCount = Limits<size_t>::MAX;
			} else {
				GREM_ASSERT(
					result.trailingArgumentCount == 0 && result.maxArgumentCount != Limits<size_t>::MAX && "Command line may only contain one range of optional arguments.");
				result.leadingArgumentCount += nestedArgumentCounts.leadingArgumentCount;
				result.trailingArgumentCount = nestedArgumentCounts.trailingArgumentCount;
				result.maxArgumentCount += nestedArgumentCounts.maxArgumentCount;
			}
		} else if constexpr (optional_argument<T>) {
			GREM_ASSERT(result.trailingArgumentCount == 0 && result.maxArgumentCount != Limits<size_t>::MAX && "Command line may only contain one range of optional arguments.");
			++result.maxArgumentCount;
		} else if constexpr (list_argument<T>) {
			GREM_ASSERT(result.leadingArgumentCount == result.maxArgumentCount && "Command line may only contain one range of optional arguments.");
			result.maxArgumentCount = Limits<size_t>::MAX;
		} else if constexpr (array_argument<T>) {
			const size_t size = std::size(argument);
			if (result.leadingArgumentCount == result.maxArgumentCount) {
				result.leadingArgumentCount += size;
				result.maxArgumentCount += size;
			} else {
				result.trailingArgumentCount += size;
				if (result.maxArgumentCount != Limits<size_t>::MAX) {
					result.maxArgumentCount += size;
				}
			}
		} else {
			if (result.leadingArgumentCount == result.maxArgumentCount) {
				++result.leadingArgumentCount;
				++result.maxArgumentCount;
			} else {
				++result.trailingArgumentCount;
				if (result.maxArgumentCount != Limits<size_t>::MAX) {
					++result.maxArgumentCount;
				}
			}
		}
	});
	return result;
}

[[nodiscard]] inline String formatArgumentName(StringView namePrefix, StringView fieldName) {
	String result{};
	result.reserve(namePrefix.size() + fieldName.size() + fieldName.size() / 4);
	result.append(namePrefix);
	bool inUppercaseSequence = false;
	for (size_t i = 0; i < fieldName.size(); ++i) {
		if (const char ch = fieldName[i]; ascii::isUppercase(ch)) {
			if (i != 0) {
				if (ascii::isUppercase(fieldName[i - 1])) {
					inUppercaseSequence = true;
				} else if (ascii::isLowercase(fieldName[i - 1])) {
					result.push_back('-');
				}
			}
			result.push_back(ascii::convertUppercaseToLowercaseCharacter(ch));
		} else {
			if (inUppercaseSequence) {
				inUppercaseSequence = false;
				result.push_back('-');
			}
			if ((ch != '-' && ch != '_') || result.empty() || result.back() != '-') {
				if (ch == '_') {
					result.push_back('-');
				} else {
					result.push_back(ch);
				}
			}
		}
	}
	return result;
}

template <typename T>
[[nodiscard]] inline String formatArgumentTypeName() {
	if constexpr (boolean_argument<T>) {
		return "boolean";
	} else if constexpr (character_argument<T>) {
		return "character";
	} else if constexpr (integer_argument<T>) {
		return "integer";
	} else if constexpr (quantity_argument<T>) {
		const auto unitSymbolString = T::Unit::getSymbolString();
		return (unitSymbolString.empty()) ? String{"number"} : String{unitSymbolString};
	} else if constexpr (float_argument<T> || double_argument<T>) {
		return "number";
	} else if constexpr (string_argument<T>) {
		return "string";
	} else if constexpr (optional_argument<T>) {
		return formatString("optional {}", formatArgumentTypeName<std::remove_cvref_t<decltype(std::declval<T>().value())>>());
	} else if constexpr (list_argument<T>) {
		return formatString("{}...", formatArgumentTypeName<std::remove_cvref_t<decltype(std::declval<T>().back())>>());
	} else if constexpr (array_argument<T>) {
		return formatString("{}...", formatArgumentTypeName<std::remove_cvref_t<decltype(std::declval<T>()[0])>>());
	} else if constexpr (duration_argument<T>) {
		return "milliseconds";
	} else {
		const StringView typeName = meta::unqualified_type_name_v<T>;
		String result{};
		result.reserve(typeName.size() + typeName.size() / 4);
		bool inUppercaseSequence = false;
		for (size_t i = 0; i < typeName.size(); ++i) {
			if (const char ch = typeName[i]; ascii::isUppercase(ch)) {
				if (i != 0) {
					if (ascii::isUppercase(typeName[i - 1])) {
						inUppercaseSequence = true;
					} else if (ascii::isLowercase(typeName[i - 1])) {
						result.push_back(' ');
					}
				}
				result.push_back(ascii::convertUppercaseToLowercaseCharacter(ch));
			} else {
				if (inUppercaseSequence) {
					inUppercaseSequence = false;
					result.push_back(' ');
				}
				if ((ch != ' ' && ch != '_') || result.empty() || result.back() != ' ') {
					if (ch == '_') {
						result.push_back(' ');
					} else {
						result.push_back(ch);
					}
				}
			}
		}
		return result;
	}
}

template <typename Arguments>
inline void appendArgumentsUsage(String& output, StringView namePrefix) {
	meta::forEachNamedFieldType<Arguments>([&]<typename T>(StringView name, meta::Type<T>) -> void {
		const String argumentName = formatArgumentName(namePrefix, name);
		if constexpr (detail::is_vector_argument_v<T>) {
			if constexpr (detail::vector_argument_size_v<T> == 1) {
				output.append(formatString(" <{}-x>", argumentName));
			} else if constexpr (detail::vector_argument_size_v<T> == 2) {
				output.append(formatString(" <{0}-x> <{0}-y>", argumentName));
			} else if constexpr (detail::vector_argument_size_v<T> == 3) {
				output.append(formatString(" <{0}-x> <{0}-y> <{0}-z>", argumentName));
			} else if constexpr (detail::vector_argument_size_v<T> == 4) {
				output.append(formatString(" <{0}-x> <{0}-y> <{0}-z> <{0}-w>", argumentName));
			}
		} else if constexpr (aggregate<T>) {
			appendArgumentsUsage<T>(output, argumentName + "-");
		} else if constexpr (optional_argument<T>) {
			output.append(formatString(" [{}]", argumentName));
		} else if constexpr (list_argument<T>) {
			output.append(formatString(" [{}...]", argumentName));
		} else if constexpr (array_argument<T>) {
			output.append(formatString(" <{}...>", argumentName));
		} else {
			output.append(formatString(" <{}>", argumentName));
		}
	});
}

template <typename Arguments, typename Options>
[[nodiscard]] inline String formatUsageString(CStringView commandName, const CommandLineParserOptions& options) {
	String result = formatString("Usage: {}", commandName.substr(commandName.find_last_of("/\\") + 1));
	if (meta::aggregate_size_v<Options> > 0) {
		result.append(" [options...]");
	}
	appendArgumentsUsage<Arguments>(result, {});
	if (!options.extraArgumentsSeparator.empty()) {
		result.append(formatString(" [{} ...]", options.extraArgumentsSeparator));
	}
	return result;
}

template <typename T>
inline decltype(auto) getDefaultArgumentValueForFormatting(const T& value) {
	if constexpr (quantity_argument<T>) {
		return value.in(T::UNIT);
	} else if constexpr (duration_argument<T>) {
		return duration_cast<DurationBase<double, Ratio<1, 1'000>>>(value).count();
	} else {
		return value;
	}
}

template <typename Option>
inline void appendOptionHelp(String& output, const String& optionName, const Option& outputOption, const HashMap<char, String>& shortOptionNameMap,
	const CommandLineParserOptions& options) {
	String argumentString{};
	using T = std::remove_cvref_t<Option>;
	if constexpr (boolean_argument<T>) {
		if (!options.useImplicitBooleanValues || outputOption) {
			argumentString = formatString(" <boolean> (default: {})", getDefaultArgumentValueForFormatting(outputOption));
		}
	} else if constexpr (optional_argument<T>) {
		const String argumentTypeName = formatArgumentTypeName<std::remove_cvref_t<decltype(std::declval<T>().value())>>();
		if (outputOption.has_value()) {
			argumentString = formatString(" [{}] (default: {})", argumentTypeName, getDefaultArgumentValueForFormatting(outputOption.value()));
		} else {
			argumentString = formatString(" [{}] (default: null)", argumentTypeName);
		}
	} else {
		argumentString = formatString(" <{}> (default: {})", formatArgumentTypeName<T>(), getDefaultArgumentValueForFormatting(outputOption));
	}
	const bool enableShortOptions = !options.shortOptionPrefix.empty() && options.shortOptionPrefix != options.longOptionPrefix;
	if (enableShortOptions && !shortOptionNameMap.empty()) {
		const char shortOptionName = optionName.front();
		const char shortOptionNameInOppositeCase = ascii::convertToOppositeCaseCharacter(shortOptionName);
		if (const auto it = shortOptionNameMap.find(shortOptionName); it != shortOptionNameMap.end() && it->second == optionName) {
			output.append(formatString("\n  -{}  {}{}{}", shortOptionName, options.longOptionPrefix, optionName, argumentString));
		} else if (const auto newIt = shortOptionNameMap.find(shortOptionNameInOppositeCase); newIt != shortOptionNameMap.end() && newIt->second == optionName) {
			output.append(formatString("\n  -{}  {}{}{}", shortOptionNameInOppositeCase, options.longOptionPrefix, optionName, argumentString));
		} else {
			output.append(formatString("\n      {}{}{}", options.longOptionPrefix, optionName, argumentString));
		}
	} else {
		output.append(formatString("\n  {}{}{}", options.longOptionPrefix, optionName, argumentString));
	}
}

inline void appendOptionsHelp(String& output, StringView namePrefix, const auto& defaultOptions, const HashMap<char, String>& shortOptionNameMap,
	const CommandLineParserOptions& options) {
	meta::forEachNamedField(defaultOptions, [&](StringView name, const auto& defaultOption) -> void {
		if (!name.empty()) {
			String optionName = formatArgumentName(namePrefix, name);
			using T = std::remove_cvref_t<decltype(defaultOption)>;
			if constexpr (detail::is_vector_argument_v<T>) {
				if constexpr (detail::vector_argument_size_v<T> == 1) {
					appendOptionHelp(output, std::move(optionName) + "-x", defaultOption.x, shortOptionNameMap, options);
				} else if constexpr (detail::vector_argument_size_v<T> == 2) {
					appendOptionHelp(output, optionName + "-x", defaultOption.x, shortOptionNameMap, options);
					appendOptionHelp(output, std::move(optionName) + "-y", defaultOption.y, shortOptionNameMap, options);
				} else if constexpr (detail::vector_argument_size_v<T> == 3) {
					appendOptionHelp(output, optionName + "-x", defaultOption.x, shortOptionNameMap, options);
					appendOptionHelp(output, optionName + "-y", defaultOption.y, shortOptionNameMap, options);
					appendOptionHelp(output, std::move(optionName) + "-z", defaultOption.z, shortOptionNameMap, options);
				} else if constexpr (detail::vector_argument_size_v<T> == 4) {
					appendOptionHelp(output, optionName + "-x", defaultOption.x, shortOptionNameMap, options);
					appendOptionHelp(output, optionName + "-y", defaultOption.y, shortOptionNameMap, options);
					appendOptionHelp(output, optionName + "-z", defaultOption.z, shortOptionNameMap, options);
					appendOptionHelp(output, std::move(optionName) + "-w", defaultOption.w, shortOptionNameMap, options);
				}
			} else if constexpr (aggregate<T>) {
				appendOptionsHelp(output, std::move(optionName) + "-", defaultOption, shortOptionNameMap, options);
			} else {
				appendOptionHelp(output, optionName, defaultOption, shortOptionNameMap, options);
			}
		}
	});
}

template <typename Arguments, typename Options>
[[nodiscard]] inline String formatHelpString(CStringView commandName, const HashMap<char, String>& shortOptionNameMap, const CommandLineParserOptions& options) {
	String result = formatUsageString<Arguments, Options>(commandName, options);
	if constexpr (meta::aggregate_size_v<Options> > 0) {
		const Options defaultOptions{};
		result.append("\nOptions:");
		appendOptionsHelp(result, {}, defaultOptions, shortOptionNameMap, options);
	}
	return result;
}

inline void addShortOptionName(HashMap<char, String>& output, String optionName) {
	const char shortOptionName = optionName.front();
	const auto [it, inserted] = output.try_emplace(shortOptionName, std::move(optionName));
	if (!inserted) {
		const char shortOptionNameInOppositeCase = ascii::convertToOppositeCaseCharacter(shortOptionName);
		const auto [newIt, newInserted] = output.try_emplace(shortOptionNameInOppositeCase, std::move(optionName));
		if (!newInserted) {
			newIt->second = {};
			output[shortOptionName] = {};
		}
	}
}

template <typename Options>
inline void addShortOptionNames(HashMap<char, String>& output, StringView namePrefix) {
	meta::forEachNamedFieldType<Options>([&]<typename T>(StringView name, meta::Type<T>) -> void {
		if (!name.empty()) {
			String optionName = formatArgumentName(namePrefix, name);
			if constexpr (detail::is_vector_argument_v<T>) {
				if constexpr (detail::vector_argument_size_v<T> == 1) {
					addShortOptionName(output, std::move(optionName) + "-x");
				} else if constexpr (detail::vector_argument_size_v<T> == 2) {
					addShortOptionName(output, optionName + "-x");
					addShortOptionName(output, std::move(optionName) + "-y");
				} else if constexpr (detail::vector_argument_size_v<T> == 3) {
					addShortOptionName(output, optionName + "-x");
					addShortOptionName(output, optionName + "-y");
					addShortOptionName(output, std::move(optionName) + "-z");
				} else if constexpr (detail::vector_argument_size_v<T> == 4) {
					addShortOptionName(output, optionName + "-x");
					addShortOptionName(output, optionName + "-y");
					addShortOptionName(output, optionName + "-z");
					addShortOptionName(output, std::move(optionName) + "-w");
				}
			} else if constexpr (aggregate<T>) {
				addShortOptionNames<T>(output, std::move(optionName) + "-");
			} else {
				addShortOptionName(output, std::move(optionName));
			}
		}
	});
}

template <typename Options>
[[nodiscard]] inline HashMap<char, String> getShortOptionNameMap() {
	HashMap<char, String> result{};
	addShortOptionNames<Options>(result, {});
	erase_if(result, [](const auto& kv) -> bool { return kv.second.empty(); });
	return result;
}

template <typename Option>
inline void trySetImplicitBooleanOption(bool& foundSpecifiedOption, StringView specifiedOptionName, Option& outputOption, const String& optionName,
	const CommandLineParserOptions& options) {
	if (foundSpecifiedOption) {
		return;
	}
	if (optionName == specifiedOptionName) {
		using T = std::remove_cvref_t<Option>;
		if constexpr (boolean_argument<T>) {
			outputOption = true;
			foundSpecifiedOption = true;
		} else {
			throw InvalidOptionValue{formatString("Option {}{} requires an argument.", options.longOptionPrefix, optionName)};
		}
	}
}

[[nodiscard]] inline bool findAndSetImplicitBooleanOption(StringView specifiedOptionName, auto& outputOptions, StringView namePrefix, const CommandLineParserOptions& options) {
	bool foundSpecifiedOption = false;
	meta::forEachNamedField(outputOptions, [&](StringView name, auto& outputOption) -> void {
		if (!name.empty()) {
			String optionName = formatArgumentName(namePrefix, name);
			using T = std::remove_cvref_t<decltype(outputOption)>;
			if constexpr (detail::is_vector_argument_v<T>) {
				if constexpr (detail::vector_argument_size_v<T> == 1) {
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.x, std::move(optionName) + "-x", options);
				} else if constexpr (detail::vector_argument_size_v<T> == 2) {
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.x, optionName + "-x", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.y, std::move(optionName) + "-y", options);
				} else if constexpr (detail::vector_argument_size_v<T> == 3) {
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.x, optionName + "-x", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.y, optionName + "-y", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.z, std::move(optionName) + "-z", options);
				} else if constexpr (detail::vector_argument_size_v<T> == 4) {
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.x, optionName + "-x", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.y, optionName + "-y", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.z, optionName + "-z", options);
					trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption.w, std::move(optionName) + "-w", options);
				}
			} else if constexpr (aggregate<T>) {
				foundSpecifiedOption = foundSpecifiedOption || findAndSetImplicitBooleanOption(specifiedOptionName, outputOption, std::move(optionName) + "-", options);
			} else {
				trySetImplicitBooleanOption(foundSpecifiedOption, specifiedOptionName, outputOption, optionName, options);
			}
		}
	});
	return foundSpecifiedOption;
}

inline void parseOptionValueOrThrow(auto& output, CStringView argument, StringView name) {
	try {
		parseArgumentValue(output, argument);
	} catch (const std::invalid_argument& e) {
		throw InvalidArgument{formatString("Invalid {} value \"{}\": {}", name, argument, e.what())};
	}
}

template <typename Option>
inline void tryParseOptionValue(bool& foundSpecifiedOption, StringView specifiedOptionName, CStringView specifiedOptionValue, Option& outputOption, Span<const CStringView> command,
	size_t& argumentIndex, const String& optionName, const CommandLineParserOptions& options) {
	if (foundSpecifiedOption) {
		return;
	}
	if (optionName == specifiedOptionName) {
		using T = std::remove_cvref_t<Option>;
		if constexpr (boolean_argument<T>) {
			if (!specifiedOptionValue.empty()) {
				parseOptionValueOrThrow(outputOption, specifiedOptionValue, optionName);
			} else if (!options.useImplicitBooleanValues || outputOption) {
				if (argumentIndex + 1 >= command.size()) {
					throw InvalidOptionValue{formatString("Option {}{} requires an argument.", options.longOptionPrefix, optionName)};
				}
				++argumentIndex;
				parseOptionValueOrThrow(outputOption, command[argumentIndex], optionName);
			} else {
				outputOption = true;
			}
		} else if constexpr (optional_argument<T>) {
			if (!specifiedOptionValue.empty()) {
				parseOptionValueOrThrow(outputOption.emplace(), specifiedOptionValue, optionName);
			} else {
				if (argumentIndex + 1 >= command.size()) {
					throw InvalidOptionValue{formatString("Option {}{} requires an argument.", options.longOptionPrefix, optionName)};
				}
				++argumentIndex;
				parseOptionValueOrThrow(outputOption.emplace(), command[argumentIndex], optionName);
			}
		} else {
			if (!specifiedOptionValue.empty()) {
				parseOptionValueOrThrow(outputOption, specifiedOptionValue, optionName);
			} else {
				if (argumentIndex + 1 >= command.size()) {
					throw InvalidOptionValue{formatString("Option {}{} requires an argument.", options.longOptionPrefix, optionName)};
				}
				++argumentIndex;
				parseOptionValueOrThrow(outputOption, command[argumentIndex], optionName);
			}
		}
		foundSpecifiedOption = true;
	}
}

[[nodiscard]] inline bool findOptionAndParseValue(StringView specifiedOptionName, CStringView specifiedOptionValue, auto& outputOptions, Span<const CStringView> command,
	size_t& argumentIndex, StringView namePrefix, const CommandLineParserOptions& options) {
	bool foundSpecifiedOption = false;
	meta::forEachNamedField(outputOptions, [&](StringView name, auto& outputOption) -> void {
		String optionName = formatArgumentName(namePrefix, name);
		using T = std::remove_cvref_t<decltype(outputOption)>;
		if constexpr (detail::is_vector_argument_v<T>) {
			if constexpr (detail::vector_argument_size_v<T> == 1) {
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, specifiedOptionValue, outputOption.x, command, argumentIndex,
					std::move(optionName) + "-x", options);
			} else if constexpr (detail::vector_argument_size_v<T> == 2) {
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.x, command, argumentIndex, optionName + "-x", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.y, command, argumentIndex, std::move(optionName) + "-y", options);
			} else if constexpr (detail::vector_argument_size_v<T> == 3) {
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.x, command, argumentIndex, optionName + "-x", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.y, command, argumentIndex, optionName + "-y", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.z, command, argumentIndex, std::move(optionName) + "-z", options);
			} else if constexpr (detail::vector_argument_size_v<T> == 4) {
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.x, command, argumentIndex, optionName + "-x", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.y, command, argumentIndex, optionName + "-y", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.z, command, argumentIndex, optionName + "-z", options);
				tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption.w, command, argumentIndex, std::move(optionName) + "-w", options);
			}
		} else if constexpr (aggregate<T>) {
			foundSpecifiedOption = foundSpecifiedOption ||
			                       findOptionAndParseValue(specifiedOptionName, specifiedOptionValue, outputOption, command, argumentIndex, std::move(optionName) + "-", options);
		} else {
			tryParseOptionValue(foundSpecifiedOption, specifiedOptionName, specifiedOptionValue, outputOption, command, argumentIndex, optionName, options);
		}
	});
	return foundSpecifiedOption;
}

inline void parseArgumentValueOrThrow(auto& output, CStringView argument, StringView namePrefix, StringView fieldName) {
	try {
		parseArgumentValue(output, argument);
	} catch (const std::invalid_argument& e) {
		throw InvalidArgument{formatString("Invalid {} argument \"{}\": {}", formatArgumentName(namePrefix, fieldName), argument, e.what())};
	}
}

template <typename Argument>
inline void parseArgument(Argument& outputArgument, Span<const CStringView> arguments, size_t& argumentIndex, StringView namePrefix, StringView fieldName,
	size_t trailingArgumentCount) {
	using T = std::remove_cvref_t<Argument>;
	if constexpr (optional_argument<T>) {
		if (arguments.size() - argumentIndex > trailingArgumentCount) {
			parseArgumentValueOrThrow(outputArgument.emplace(), arguments[argumentIndex++], namePrefix, fieldName);
		}
	} else if constexpr (list_argument<T>) {
		while (arguments.size() - argumentIndex > trailingArgumentCount) {
			parseArgumentValueOrThrow(outputArgument.emplace_back(), arguments[argumentIndex++], namePrefix, fieldName);
		}
	} else if constexpr (array_argument<T>) {
		const size_t size = std::size(outputArgument);
		GREM_ASSERT(arguments.size() - argumentIndex >= size);
		for (size_t i = 0; i < size; ++i) {
			parseArgumentValueOrThrow(outputArgument[i], arguments[argumentIndex++], namePrefix, fieldName);
		}
	} else {
		GREM_ASSERT(arguments.size() - argumentIndex > 0);
		parseArgumentValueOrThrow(outputArgument, arguments[argumentIndex++], namePrefix, fieldName);
	}
}

inline void parseArguments(auto& outputArguments, Span<const CStringView> arguments, size_t& argumentIndex, StringView namePrefix, size_t trailingArgumentCount) {
	meta::forEachNamedField(outputArguments, [&](StringView name, auto& outputArgument) -> void {
		using T = std::remove_cvref_t<decltype(outputArgument)>;
		if constexpr (detail::is_vector_argument_v<T>) {
			const String innerNamePrefix = formatArgumentName(namePrefix, name) + "-";
			if constexpr (detail::vector_argument_size_v<T> == 1) {
				parseArgument(outputArgument.x, arguments, argumentIndex, innerNamePrefix, "x");
			} else if constexpr (detail::vector_argument_size_v<T> == 2) {
				parseArgument(outputArgument.x, arguments, argumentIndex, innerNamePrefix, "x");
				parseArgument(outputArgument.y, arguments, argumentIndex, innerNamePrefix, "y");
			} else if constexpr (detail::vector_argument_size_v<T> == 3) {
				parseArgument(outputArgument.x, arguments, argumentIndex, innerNamePrefix, "x");
				parseArgument(outputArgument.y, arguments, argumentIndex, innerNamePrefix, "y");
				parseArgument(outputArgument.z, arguments, argumentIndex, innerNamePrefix, "z");
			} else if constexpr (detail::vector_argument_size_v<T> == 4) {
				parseArgument(outputArgument.x, arguments, argumentIndex, innerNamePrefix, "x");
				parseArgument(outputArgument.y, arguments, argumentIndex, innerNamePrefix, "y");
				parseArgument(outputArgument.z, arguments, argumentIndex, innerNamePrefix, "z");
				parseArgument(outputArgument.w, arguments, argumentIndex, innerNamePrefix, "w");
			}
		} else if constexpr (aggregate<T>) {
			parseArguments(outputArgument, arguments, argumentIndex, formatArgumentName(namePrefix, name) + "-", trailingArgumentCount);
		} else {
			parseArgument(outputArgument, arguments, argumentIndex, namePrefix, name, trailingArgumentCount);
		}
	});
}

} // namespace detail

/**
 * Parse a command line into arguments and options.
 *
 * \param outputArguments user-defined aggregate of fields to parse as arguments
 *        from the command line.
 * \param outputOptions user-defined aggregate of fields to parse as options
 *        from the command line.
 * \param command view over the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given arguments and/or options.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given
 *         argument or option fields.
 */
template <aggregate Arguments, aggregate Options>
ArrayList<CStringView> parseCommandLine(Arguments& outputArguments, Options& outputOptions, Span<const CStringView> command, const CommandLineParserOptions& options = {})
	requires(default_initializable<Arguments> && default_initializable<Options>) {
	GREM_ASSERT(!options.longOptionPrefix.empty());
	if (command.empty()) {
		throw InvalidUsage{detail::formatUsageString<Arguments, Options>("command", options)};
	}

	const bool enableShortOptions = !options.shortOptionPrefix.empty() && options.shortOptionPrefix != options.longOptionPrefix;
	const HashMap<char, String> shortOptionNameMap = (enableShortOptions) ? detail::getShortOptionNameMap<Options>() : HashMap<char, String>{};

	if (options.enableHelpOption) {
		for (const CStringView argument : command.subspan(1)) {
			if ((argument.starts_with(options.longOptionPrefix) && argument.substr(options.longOptionPrefix.size()) == "help") ||
				(enableShortOptions && argument.starts_with(options.shortOptionPrefix) && argument.substr(options.shortOptionPrefix.size()) == "?") ||
				(!enableShortOptions && options.longOptionPrefix == "-" && argument == "--help")) {
				throw Help{detail::formatHelpString<Arguments, Options>(command.front(), shortOptionNameMap, options)};
			}
		}
	}

	ArrayList<CStringView> arguments{};
	ArrayList<CStringView> extraArguments{};
	arguments.reserve(command.size() - 1);

	if constexpr (meta::aggregate_size_v<Options> > 0) {
		HashSet<String> optionsSpecified{};
		for (size_t argumentIndex = 1; argumentIndex < command.size(); ++argumentIndex) {
			const CStringView argument = command[argumentIndex];
			if (!options.extraArgumentsSeparator.empty() && argument == options.extraArgumentsSeparator) {
				extraArguments.assign(command.begin() + argumentIndex + 1, command.end());
				break;
			}

			const auto throwUnknownOptionError = [&]() -> void {
				if (options.enableHelpOption) {
					throw UnknownOption{formatString("Unknown option {}. Try {}help.", argument, options.longOptionPrefix)};
				}
				throw UnknownOption{formatString("Unknown option {}.", argument)};
			};

			StringView specifiedOptionName{};
			CStringView specifiedOptionValue{};
			if (argument.starts_with(options.longOptionPrefix)) {
				const CStringView argumentWithoutPrefix = argument.substr(options.longOptionPrefix.size());
				specifiedOptionName = argumentWithoutPrefix;
				if (options.allowEqualsValueSyntax) {
					if (const size_t equalsPosition = specifiedOptionName.find('='); equalsPosition != StringView::npos) {
						if (equalsPosition + 1 == specifiedOptionName.size()) {
							throw InvalidOptionValue{"Expected a value."};
						}
						specifiedOptionValue = argumentWithoutPrefix.substr(equalsPosition + 1);
						specifiedOptionName = specifiedOptionName.substr(0, equalsPosition);
					}
				}
				if (specifiedOptionName.empty()) {
					throwUnknownOptionError();
				}
			} else if (enableShortOptions && argument.starts_with(options.shortOptionPrefix)) {
				const CStringView argumentWithoutPrefix = argument.substr(options.shortOptionPrefix.size());
				specifiedOptionName = argumentWithoutPrefix;
				if (options.allowEqualsValueSyntax) {
					if (const size_t equalsPosition = specifiedOptionName.find('='); equalsPosition != StringView::npos) {
						if (equalsPosition + 1 == specifiedOptionName.size()) {
							throw InvalidOptionValue{"Expected a value."};
						}
						specifiedOptionValue = argumentWithoutPrefix.substr(equalsPosition + 1);
						specifiedOptionName = specifiedOptionName.substr(0, equalsPosition);
					}
				}
				if (specifiedOptionName.empty()) {
					throwUnknownOptionError();
				}
				if (specifiedOptionName.size() == 1) {
					const char shortOptionName = specifiedOptionName.front();
					const auto it = shortOptionNameMap.find(shortOptionName);
					if (it == shortOptionNameMap.end()) {
						throwUnknownOptionError();
					}
					specifiedOptionName = it->second;
				} else {
					const bool allowShortOptionGrouping = enableShortOptions && options.useImplicitBooleanValues && options.allowShortOptionGrouping;
					if (!allowShortOptionGrouping) {
						throwUnknownOptionError();
					}
					for (const char shortOptionName : specifiedOptionName) {
						const auto it = shortOptionNameMap.find(shortOptionName);
						if (it == shortOptionNameMap.end()) {
							throwUnknownOptionError();
						}
						if (!optionsSpecified.emplace(it->second).second) {
							throw InvalidOptionValue{formatString("Option {}{} was specified multiple times.", options.longOptionPrefix, it->second)};
						}
						if (detail::findAndSetImplicitBooleanOption(it->second, outputOptions, {}, options)) {
							if (!specifiedOptionValue.empty()) {
								throw InvalidOptionValue{"Cannot specify value for a group of boolean options."};
							}
						} else {
							throwUnknownOptionError();
						}
					}
					continue;
				}
			}

			if (specifiedOptionName.empty()) {
				arguments.push_back(argument);
			} else {
				if (!optionsSpecified.emplace(specifiedOptionName).second) {
					throw InvalidOptionValue{formatString("Option {}{} was specified multiple times.", options.longOptionPrefix, specifiedOptionName)};
				}
				if (!detail::findOptionAndParseValue(specifiedOptionName, specifiedOptionValue, outputOptions, command, argumentIndex, {}, options)) {
					throwUnknownOptionError();
				}
			}
		}
	} else {
		for (size_t argumentIndex = 1; argumentIndex < command.size(); ++argumentIndex) {
			const CStringView argument = command[argumentIndex];
			if (!options.extraArgumentsSeparator.empty() && argument == options.extraArgumentsSeparator) {
				extraArguments.assign(command.begin() + argumentIndex + 1, command.end());
				break;
			}
			arguments.push_back(argument);
		}
	}

	if constexpr (meta::aggregate_size_v<Arguments> > 0) {
		const detail::ArgumentCounts argumentCounts = detail::getArgumentCounts(outputArguments);
		if (arguments.size() < argumentCounts.leadingArgumentCount + argumentCounts.trailingArgumentCount || arguments.size() > argumentCounts.maxArgumentCount) {
			throw InvalidUsage{detail::formatUsageString<Arguments, Options>(command.front(), options)};
		}

		size_t argumentIndex = 0;
		detail::parseArguments(outputArguments, arguments, argumentIndex, {}, argumentCounts.trailingArgumentCount);
	} else {
		if (!arguments.empty()) {
			throw InvalidUsage{detail::formatUsageString<Arguments, Options>(command.front(), options)};
		}
	}

	return extraArguments;
}

/**
 * Parse the command line received by main() into arguments and options.
 *
 * \param outputArguments user-defined aggregate of fields to parse as arguments
 *        from the command line.
 * \param outputOptions user-defined aggregate of fields to parse as options
 *        from the command line.
 * \param argc number of arguments in the given command line.
 * \param argv pointer to a null-terminated array of pointers to null-terminated
 *        strings holding the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given arguments and/or options.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given
 *         argument or option fields.
 */
template <aggregate Arguments, aggregate Options>
ArrayList<CStringView> parseCommandLine(Arguments& outputArguments, Options& outputOptions, int argc, char* argv[], const CommandLineParserOptions& options = {})
	requires(default_initializable<Arguments> && default_initializable<Options>) {
	return parseCommandLine(outputArguments, outputOptions, Span<const CStringView>{reinterpret_cast<const CStringView*>(argv), static_cast<size_t>(argc)}, options);
}

/**
 * Parse a command line into arguments.
 *
 * \param outputArguments user-defined aggregate of fields to parse as arguments
 *        from the command line.
 * \param command view over the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given arguments.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given
 *         argument fields.
 */
template <aggregate Arguments>
ArrayList<CStringView> parseCommandLineArguments(Arguments& outputArguments, Span<const CStringView> command, const CommandLineParserOptions& options = {})
	requires(default_initializable<Arguments>) {
	detail::NoOptions outputOptions{};
	return parseCommandLine(outputArguments, outputOptions, command, options);
}

/**
 * Parse the command line received by main() into arguments.
 *
 * \param outputArguments user-defined aggregate of fields to parse as arguments
 *        from the command line.
 * \param argc number of arguments in the given command line.
 * \param argv pointer to a null-terminated array of pointers to null-terminated
 *        strings holding the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given arguments.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given
 *         argument fields.
 */
template <aggregate Arguments>
ArrayList<CStringView> parseCommandLineArguments(Arguments& outputArguments, int argc, char* argv[], const CommandLineParserOptions& options = {})
	requires(default_initializable<Arguments>) {
	detail::NoOptions outputOptions{};
	return parseCommandLine(outputArguments, outputOptions, argc, argv, options);
}

/**
 * Parse a command line into options.
 *
 * \param outputOptions user-defined aggregate of fields to parse as options
 *        from the command line.
 * \param command view over the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given options.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given option
 *         fields.
 */
template <aggregate Options>
ArrayList<CStringView> parseCommandLineOptions(Options& outputOptions, Span<const CStringView> command, const CommandLineParserOptions& options = {})
	requires(default_initializable<Options>) {
	detail::NoArguments outputArguments{};
	return parseCommandLine(outputArguments, outputOptions, command, options);
}

/**
 * Parse the command line received by main() into options.
 *
 * \param outputOptions user-defined aggregate of fields to parse as options
 *        from the command line.
 * \param argc number of arguments in the given command line.
 * \param argv pointer to a null-terminated array of pointers to null-terminated
 *        strings holding the given command line arguments.
 * \param options parser options, see CommandLineParserOptions.
 *
 * \return extra arguments that were provided after the
 *         CommandLineParserOptions::extraArgumentsSeparator, if any.
 *
 * \throws cli::Error on failure to parse the given options.
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the assignment operators of the given option
 *         fields.
 */
template <aggregate Options>
ArrayList<CStringView> parseCommandLineOptions(Options& outputOptions, int argc, char* argv[], const CommandLineParserOptions& options = {})
	requires(default_initializable<Options>) {
	detail::NoArguments outputArguments{};
	return parseCommandLine(outputArguments, outputOptions, argc, argv, options);
}

} // namespace grem::cli

#endif
