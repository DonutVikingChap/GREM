// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATTING_HPP
#define GREM_CORE_FORMATTING_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/fundamentals.hpp>

#include <charconv>     // std::chars_format, std::to_chars_result, std::to_chars
#include <cmath>        // std::isfinite
#include <cstdio>       // EOF, stdout, stderr, std::FILE, std::fwrite, std::fputc
#include <iosfwd>       // std::ostream
#include <iterator>     // std::begin, std::end
#include <system_error> // std::errc
#include <type_traits>  // std::make_unsigned_t, std::type_identity_t, std::is_array_v
#include <utility>      // std::move, std::index_sequence, std::make_index_sequence

namespace grem {

namespace detail {
struct Unformattable {};
} // namespace detail

struct FormatError : Error {
	using Error::Error;
};

class FormatOutput {
public:
	constexpr virtual void append(StringView string) = 0;
};

template <typename T>
struct Formatter {
	using GREM_private_UnformattableTag = void;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const auto& p) {
		static_assert(same_as<T, detail::Unformattable>,
			"Argument type lacks a formatter. Implement one by specializing grem::Formatter<T>. See bottom of <GREM/core/formatting.hpp> for examples.");
		return p;
	}

	void formatTo(FormatOutput& output, const auto& value) const {
		static_assert(same_as<T, detail::Unformattable>,
			"Argument type lacks a formatter. Implement one by specializing grem::Formatter<T>. See bottom of <GREM/core/formatting.hpp> for examples.");
		(void)output;
		(void)value;
	}
};

namespace detail {

struct FormatSizeCounter : FormatOutput {
	size_t output = 0;

	constexpr void append(StringView string) override {
		output += string.size();
	}
};

template <typename It>
struct IteratorFormatWriter : FormatOutput {
	It output;

	constexpr IteratorFormatWriter(It output)
		: output(output) {}

	constexpr void append(StringView string) override {
		for (const char ch : string) {
			*output++ = ch;
		}
	}
};

struct CFileFormatWriter : FormatOutput {
	std::FILE* output;

	constexpr CFileFormatWriter(std::FILE* output)
		: output(output) {}

	void append(StringView string) override {
		if (std::fwrite(string.data(), sizeof(char), string.size(), output) != string.size()) {
			throw FormatError{"Failed to write to file."};
		}
	}
};

struct StreamFormatWriter : FormatOutput {
	std::ostream& output;

	constexpr StreamFormatWriter(std::ostream& output)
		: output(output) {}

	GREM_API(core) void append(StringView string) override;
};

template <typename Output>
struct AppendingFormatWriter : FormatOutput {
	Output& output;

	constexpr AppendingFormatWriter(Output& output)
		: output(output) {}

	constexpr void append(StringView string) override {
		output.append(string);
	}
};

} // namespace detail

template <typename T>
concept formattable = requires(Formatter<T> formatter, const Formatter<T> constFormatter, const char* p, detail::FormatSizeCounter output, const T value) {
	p = formatter.parseFormatSpecification(p);
	constFormatter.formatTo(output, value);
} && !requires { typename Formatter<T>::GREM_private_UnformattableTag; };

class FormatArgument {
public:
	template <typename T, typename CustomFormatter>
	[[nodiscard]] static constexpr FormatArgument createEmpty() noexcept {
		FormatArgument result{};
		result.parseFormatSpecificationAndFormatToImplementation = [](FormatOutput&, const char* p, ValueView) -> const char* {
			CustomFormatter formatter{};
			if (*p != '}') {
				p = formatter.parseFormatSpecification(p);
			}
			return p;
		};
		return result;
	};

	template <typename T, typename CustomFormatter>
	[[nodiscard]] static constexpr FormatArgument create(const T& value) noexcept {
		constexpr bool IS_SMALL = !std::is_array_v<T> && trivially_copyable<T> && sizeof(T) <= sizeof(void*) && alignof(T) <= alignof(void*);
		FormatArgument result{};
		if constexpr (IS_SMALL) {
			result.valueView = {.smallBuffer{}};
			const ValueRepresentation<T> valueRepresentation = bit_cast<ValueRepresentation<T>>(value);
			for (size_t i = 0; i < sizeof(T); ++i) {
				result.valueView.smallBuffer[i] = valueRepresentation.bytes[i];
			}
		} else {
			result.valueView = {.pointer = &value};
		}
		result.parseFormatSpecificationAndFormatToImplementation = [](FormatOutput& output, const char* p, ValueView valueView) -> const char* {
			CustomFormatter formatter{};
			if (*p != '}') {
				p = formatter.parseFormatSpecification(p);
			}
			if constexpr (IS_SMALL) {
				ValueRepresentation<T> valueRepresentation{};
				for (size_t i = 0; i < sizeof(T); ++i) {
					valueRepresentation.bytes[i] = valueView.smallBuffer[i];
				}
				formatter.formatTo(output, bit_cast<T>(valueRepresentation));
			} else {
				formatter.formatTo(output, *static_cast<const T*>(valueView.pointer));
			}
			return p;
		};
		return result;
	};

	template <typename T>
	constexpr FormatArgument(const T& value) requires(!same_as<T, FormatArgument>) {
		*this = create<T, Formatter<T>>(value);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecificationAndFormatTo(FormatOutput& output, const char* p) const {
		return parseFormatSpecificationAndFormatToImplementation(output, p, valueView);
	}

private:
	union ValueView {
		const void* pointer;
		alignas(void*) byte smallBuffer[sizeof(void*)];
	};

	ValueView valueView{.pointer = nullptr};
	const char* (*parseFormatSpecificationAndFormatToImplementation)(FormatOutput& output, const char* p, ValueView valueView) = nullptr;

	constexpr FormatArgument() noexcept = default;
};

using FormatArguments = Span<const FormatArgument>;

constexpr void formatToV(FormatOutput& output, CStringView format, FormatArguments arguments) {
	size_t nextImplicitArgumentIndex = 0;

	const char* p = format.c_str();
	const char* begin = p;
	while (true) {
		switch (*p) {
			case '\0': output.append(StringView{begin, p}); return;
			case '}':
				++p;
				if (*p == '}') {
					output.append(StringView{begin, p});
					++p;
					begin = p;
					continue;
				}
				throw FormatError{"Misplaced '}' in format string."};
			case '{': {
				const char* const openCurly = p;
				++p;
				if (*p == '{') {
					output.append(StringView{begin, p});
					++p;
					begin = p;
					continue;
				}
				output.append(StringView{begin, openCurly});
				size_t argumentIndex = 0;
				if (*p >= '0' && *p <= '9') {
					if (nextImplicitArgumentIndex != 0 && nextImplicitArgumentIndex != Limits<size_t>::MAX) {
						throw FormatError{"Mixed explicit/implicit argument indices in format string."};
					}
					do {
						argumentIndex *= 10;
						argumentIndex += static_cast<size_t>(*p - '0');
						++p;
					} while (*p >= '0' && *p <= '9');
					nextImplicitArgumentIndex = Limits<size_t>::MAX;
				} else {
					if (nextImplicitArgumentIndex == Limits<size_t>::MAX) {
						throw FormatError{"Mixed explicit/implicit argument indices in format string."};
					}
					argumentIndex = nextImplicitArgumentIndex++;
				}
				if (*p == ':') {
					++p;
				} else if (*p != '}') {
					throw FormatError{"Invalid format argument specifier."};
				}
				if (argumentIndex >= arguments.size()) {
					throw FormatError{"Missing format argument."};
				}
				const FormatArgument argument = arguments.begin()[argumentIndex];
				p = argument.parseFormatSpecificationAndFormatTo(output, p);
				if (*p != '}') {
					throw FormatError{"Invalid format argument specifier."};
				}
				++p;
				begin = p;
				break;
			}
			default: ++p; break;
		}
	}
}

struct RuntimeFormat : CStringView {
	constexpr explicit RuntimeFormat(CStringView format) noexcept
		: CStringView(format) {}
};

template <typename... Args>
class FormatStringBase {
public:
	template <typename T>
	consteval FormatStringBase(const T& format) requires(convertible_to<T, CStringView>)
		: format(format) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, get(), {FormatArgument::createEmpty<Args, Formatter<Args>>()...});
	}

	constexpr FormatStringBase(RuntimeFormat format) noexcept
		: format(format) {}

	[[nodiscard]] constexpr CStringView get() const noexcept {
		return format;
	}

private:
	CStringView format;
};

template <typename... Args>
using FormatString = FormatStringBase<std::type_identity_t<Args>...>;

template <typename... Args>
[[nodiscard]] constexpr size_t getFormattedSize(FormatString<Args...> format, const Args&... args) {
	detail::FormatSizeCounter sizeCounter{};
	formatToV(sizeCounter, format.get(), {args...});
	return sizeCounter.output;
}

template <typename Output, typename... Args>
constexpr void formatTo(Output& output, FormatString<Args...> format, const Args&... args) {
	if constexpr (requires(Output out, const size_t n) {
					  out.data();
					  out.size();
					  out.resize(n);
				  }) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, format.get(), {args...});
		const size_t formattedSize = sizeCounter.output;
		const size_t offset = output.size();
		output.resize(offset + formattedSize);
		try {
			char* p = output.data() + offset;
			formatTo(p, format, args...);
		} catch (...) {
			output.resize(offset);
			throw;
		}
	} else if constexpr (requires(Output out, const size_t n) {
							 out.size();
							 out.reserve(n);
						 }) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, format.get(), {args...});
		const size_t formattedSize = sizeCounter.output;
		output.reserve(output.size() + formattedSize);
		detail::AppendingFormatWriter writer{output};
		formatToV(writer, format.get(), {args...});
	} else if constexpr (requires { *output++ = '\0'; }) {
		detail::IteratorFormatWriter writer{output};
		formatToV(writer, format.get(), {args...});
		output = writer.output;
	} else {
		detail::AppendingFormatWriter writer{output};
		formatToV(writer, format.get(), {args...});
	}
}

template <typename... Args>
GREM_ALWAYS_INLINE void print(std::FILE* output, FormatString<Args...> format, const Args&... args) {
	detail::CFileFormatWriter writer{output};
	formatTo(writer, format, args...);
}

template <typename... Args>
GREM_ALWAYS_INLINE void print(std::ostream& output, FormatString<Args...> format, const Args&... args) {
	detail::StreamFormatWriter writer{output};
	formatTo(writer, format, args...);
}

GREM_ALWAYS_INLINE void println(std::FILE* output) {
	if (std::fputc('\n', output) == EOF) {
		throw FormatError{"Failed to write to file."};
	}
}

GREM_ALWAYS_INLINE void println(std::ostream& output) {
	print(output, "\n");
}

template <typename... Args>
GREM_ALWAYS_INLINE void println(std::FILE* output, FormatString<Args...> format, const Args&... args) {
	print(output, format, args...);
	println(output);
}

template <typename... Args>
GREM_ALWAYS_INLINE void println(std::ostream& output, FormatString<Args...> format, const Args&... args) {
	print(output, format, args...);
	println(output);
}

template <typename... Args>
GREM_ALWAYS_INLINE void print(FormatString<Args...> format, const Args&... args) {
	print(stdout, format, args...);
}

GREM_ALWAYS_INLINE void println() {
	println(stdout);
}

template <typename... Args>
GREM_ALWAYS_INLINE void println(FormatString<Args...> format, const Args&... args) {
	println(stdout, format, args...);
}

template <typename... Args>
GREM_ALWAYS_INLINE void eprint(FormatString<Args...> format, const Args&... args) {
	print(stderr, format, args...);
}

GREM_ALWAYS_INLINE void eprintln() {
	println(stderr);
}

template <typename... Args>
GREM_ALWAYS_INLINE void eprintln(FormatString<Args...> format, const Args&... args) {
	println(stderr, format, args...);
}

/**
 * Format specification syntax examples:
 *
 * Given integer 42:
 *
 * | Format    | Output     | Comment             |
 * | --------- | ---------- | ------------------- |
 * | `{}`      | `42`       |                     |
 * | `{:b}`    | `101010`   | Binary              |
 * | `{:#b}`   | `0b101010` |                     |
 * | `{:o}`    | `52`       | Octal               |
 * | `{:#o}`   | `052`      |                     |
 * | `{:x}`    | `2a`       | Hexadecimal         |
 * | `{:#x}`   | `0x2a`     |                     |
 * | `{:X}`    | `2A`       | Uppercase           |
 * | `{:#X}`   | `0X2A`     |                     |
 * | `{:#B}`   | `0B101010` |                     |
 * | `{:>6}`   | `    42`   | Right-align         |
 * | `{:*>6}`  | `****42`   | Right-align with *  |
 * | `{:*^6}`  | `**42**`   | Center-align with * |
 * | `{:+}`    | `+42`      | Explicit + sign     |
 * | `{:_>+4}` | `_+42`     |                     |
 * | `{:_<+6}` | `+42___`   |                     |
 * | `{:04}`   | `0042`     | Zero-pad            |
 * | `{:06}`   | `000042`   |                     |
 * | `{:08X}`  | `0000002A` |                     |
 *
 * Given floating-point 42.125f:
 *
 * | Format    | Output         | Comment                |
 * | --------- | -------------- | ---------------------- |
 * | `{}`      | `42.125`       |                        |
 * | `{:.4}`   | `42.12`        | Precision 4            |
 * | `{:.7}`   | `42.125`       |                        |
 * | `{:f}`    | `42.125000`    | Fixed                  |
 * | `{:.4f}`  | `42.1250`      |                        |
 * | `{:e}`    | `4.212500e+01` | Scientific             |
 * | `{:.3e}`  | `4.212e+01`    |                        |
 * | `{:a}`    | `1.51p+5`      | Hexfloat               |
 * | `{:.4a}`  | `1.5100p+5`    |                        |
 * | `{:E}`    | `4.212500E+01` | Uppercase              |
 * | `{:A}`    | `1.51P+5`      |                        |
 * | `{:07}`   | `042.125`      | Zero-pad               |
 */
template <typename... Args>
[[nodiscard]] inline String formatString(FormatString<Args...> format, const Args&... args) {
	String result{};
	formatTo(result, format, args...);
	return result;
}

[[nodiscard]] inline String formatStringV(CStringView format, FormatArguments arguments) {
	String result{};
	detail::AppendingFormatWriter writer{result};
	formatToV(writer, format, arguments);
	return result;
}

namespace detail {

template <size_t SmallCapacity>
class SmallFormattedString {
public:
	SmallFormattedString() noexcept = default;

	~SmallFormattedString() {
		if (length > SmallCapacity) {
			delete[] characters;
		}
	}

	SmallFormattedString(const SmallFormattedString& other)
		: SmallFormattedString() {
		*this = other;
	}

	SmallFormattedString(SmallFormattedString&& other) noexcept
		: SmallFormattedString() {
		*this = std::move(other);
	}

	SmallFormattedString& operator=(const SmallFormattedString& other) {
		if (this == &other) {
			return *this;
		}
		resize(other.size());
		char* p = data();
		for (const char ch : other) {
			*p++ = ch;
		}
		return *this;
	}

	SmallFormattedString& operator=(SmallFormattedString&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		if (length > SmallCapacity) {
			delete[] characters;
		}
		if (other.length > SmallCapacity) {
			characters = other.characters;
			other.characters = other.smallBuffer;
		} else {
			char* p = smallBuffer;
			for (const char ch : other) {
				*p++ = ch;
			}
			characters = smallBuffer;
		}
		length = other.length;
		other.length = 0;
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE char* data() noexcept {
		return characters;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const char* data() const noexcept {
		return characters;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t size() const noexcept {
		return length;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE char* begin() noexcept {
		return data();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const char* begin() const noexcept {
		return data();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE char* end() noexcept {
		return data() + size();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE const char* end() const noexcept {
		return data() + size();
	}

	void resize(size_t newLength) {
		if (newLength > length) {
			if (newLength > SmallCapacity) {
				char* const newCharacters = new char[newLength]; // NOLINT(cppcoreguidelines-owning-memory)
				char* p = newCharacters;
				for (const char ch : *this) {
					*p++ = ch;
				}
				if (length > SmallCapacity) {
					delete[] characters;
				}
				characters = newCharacters;
			} else {
				if (length > SmallCapacity) {
					char* p = smallBuffer;
					for (const char ch : *this) {
						*p++ = ch;
					}
					delete[] characters;
					characters = smallBuffer;
				}
			}
			length = newLength;
		} else if (newLength < length) {
			if (newLength <= SmallCapacity && length > SmallCapacity) {
				char* p = smallBuffer;
				for (const char ch : *this) {
					*p++ = ch;
				}
				delete[] characters;
				characters = smallBuffer;
			}
			length = newLength;
		}
	}

	constexpr operator StringView() const noexcept {
		return StringView{characters, length};
	}

private:
	char* characters = smallBuffer;
	size_t length = 0;
	char smallBuffer[SmallCapacity];
};

} // namespace detail

template <size_t N, typename... Args>
GREM_ALWAYS_INLINE detail::SmallFormattedString<N> formatSmallString(FormatString<Args...> format, const Args&... args) {
	detail::SmallFormattedString<N> result{};
	formatTo(result, format, args...);
	return result;
}

template <typename T>
struct RangeFormatter {
	Formatter<T> underlying{};
	StringView separator = ", ";
	StringView openingBracket = "[";
	StringView closingBracket = "]";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'n') {
			openingBracket = {};
			closingBracket = {};
			++p;
		}
		if constexpr (requires {
						  underlying.separator = ": ";
						  underlying.openingBracket = {};
						  underlying.closingBracket = {};
					  }) {
			if (*p == 'm') {
				if (!openingBracket.empty()) {
					openingBracket = "{";
					closingBracket = "}";
				}
				underlying.separator = ": ";
				underlying.openingBracket = {};
				underlying.closingBracket = {};
				++p;
			}
		}
		if constexpr (same_as<T, char>) {
			if (*p == 's') {
				separator = {};
				openingBracket = {};
				closingBracket = {};
				++p;
			}
		}
		if (*p == ':') {
			++p;
			p = underlying.parseFormatSpecification(p);
		}
		return p;
	}

	void formatTo(FormatOutput& output, const auto& value) const {
		output.append(openingBracket);
		auto it = std::begin(value);
		auto end = std::end(value);
		if (it != end) {
			underlying.formatTo(output, *it++);
			while (it != end) {
				output.append(separator);
				underlying.formatTo(output, *it++);
			}
		}
		output.append(closingBracket);
	}
};

} // namespace grem

template <>
struct grem::Formatter<grem::StringView> {
	size_t minWidth = 0;
	char fill = ' ';
	char alignment = '<';

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		switch (*p) {
			case '\0': break;
			case '<': [[fallthrough]];
			case '>': [[fallthrough]];
			case '^': alignment = *p++; break;
			default:
				switch (*(p + 1)) {
					case '<': [[fallthrough]];
					case '>': [[fallthrough]];
					case '^':
						fill = *p++;
						alignment = *p++;
						break;
					default: break;
				}
				break;
		}
		while (*p >= '0' && *p <= '9') {
			minWidth *= 10;
			minWidth += static_cast<size_t>(*p - '0');
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, StringView value) const {
		switch (alignment) {
			case '<':
				output.append(value);
				for (size_t i = value.size(); i < minWidth; ++i) {
					output.append(StringView{&fill, 1});
				}
				break;
			case '>':
				for (size_t i = value.size(); i < minWidth; ++i) {
					output.append(StringView{&fill, 1});
				}
				output.append(value);
				break;
			case '^':
				if (value.size() < minWidth) {
					const size_t n = minWidth - value.size();
					const size_t halfRoundedDown = n / 2;
					const size_t halfRoundedUp = n - halfRoundedDown;
					for (size_t i = 0; i < halfRoundedDown; ++i) {
						output.append(StringView{&fill, 1});
					}
					output.append(value);
					for (size_t i = 0; i < halfRoundedUp; ++i) {
						output.append(StringView{&fill, 1});
					}
				} else {
					output.append(value);
				}
				break;
			default: unreachable();
		}
	}
};

template <>
struct grem::Formatter<grem::CStringView> : Formatter<StringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, CStringView value) const {
		Formatter<StringView>::formatTo(output, StringView{value});
	}
};

template <>
struct grem::Formatter<grem::String> : Formatter<StringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const String& value) const {
		Formatter<StringView>::formatTo(output, StringView{value});
	}
};

template <grem::size_t Length>
struct grem::Formatter<grem::ConstantString<char, Length>> : Formatter<StringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const ConstantString<char, Length>& value) const {
		Formatter<StringView>::formatTo(output, StringView{value});
	}
};

template <>
struct grem::Formatter<char> : Formatter<StringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, char value) const {
		Formatter<StringView>::formatTo(output, StringView{&value, 1});
	}
};

template <>
struct grem::Formatter<const char*> : Formatter<CStringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const char* value) const {
		Formatter<CStringView>::formatTo(output, CStringView{value});
	}
};

template <>
struct grem::Formatter<char*> : Formatter<const char*> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, char* value) const {
		Formatter<const char*>::formatTo(output, static_cast<const char*>(value));
	}
};

template <grem::size_t N>
struct grem::Formatter<char[N]> : Formatter<const char*> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const char (&value)[N]) const {
		Formatter<const char*>::formatTo(output, static_cast<const char*>(value));
	}
};

template <>
struct grem::Formatter<bool> : Formatter<StringView> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, bool value) const {
		Formatter<StringView>::formatTo(output, (value) ? StringView{"true"} : StringView{"false"});
	}
};

template <grem::strict_integral Integer>
struct grem::Formatter<Integer> {
	size_t minWidth = 0;
	int radix = 10;
	char fill = ' ';
	char alignment = '\0';
	char sign = '-';
	bool addRadixPrefix = false;
	bool makeUppercase = false;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		switch (*p) {
			case '\0': break;
			case '<': [[fallthrough]];
			case '>': [[fallthrough]];
			case '^': alignment = *p++; break;
			default:
				switch (*(p + 1)) {
					case '<': [[fallthrough]];
					case '>': [[fallthrough]];
					case '^':
						fill = *p++;
						alignment = *p++;
						break;
					default: break;
				}
				break;
		}

		switch (*p) {
			case '-': ++p; break;
			case '+': [[fallthrough]];
			case ' ': sign = *p++; break;
			default: break;
		}

		if (*p == '#') {
			addRadixPrefix = true;
			++p;
		}

		if (*p == '0') {
			if (alignment == '\0') {
				alignment = '0';
			}
			++p;
		}

		while (*p >= '0' && *p <= '9') {
			minWidth *= 10;
			minWidth += static_cast<size_t>(*p - '0');
			++p;
		}

		switch (*p) {
			case 'B': makeUppercase = true; [[fallthrough]];
			case 'b':
				radix = 2;
				++p;
				break;
			case 'o':
				radix = 8;
				++p;
				break;
			case 'd':
				radix = 10;
				++p;
				break;
			case 'X': makeUppercase = true; [[fallthrough]];
			case 'x':
				radix = 16;
				++p;
				break;
			default: break;
		}
		return p;
	}

	void formatTo(FormatOutput& output, Integer value) const {
		char buffer[128];

		char* p = buffer;

		using UnsignedInteger = std::make_unsigned_t<Integer>;
		UnsignedInteger unsignedValue = static_cast<UnsignedInteger>(value);

		if constexpr (signed_integral<Integer>) {
			if (value < 0) {
				*p++ = '-';
				unsignedValue = static_cast<UnsignedInteger>(UnsignedInteger{0} - unsignedValue);
			} else if (value != 0 && sign != '-') {
				*p++ = sign;
			}
		} else if (value != 0 && sign != '-') {
			*p++ = sign;
		}

		if (addRadixPrefix) {
			switch (radix) {
				case 2:
					*p++ = '0';
					*p++ = 'b';
					break;
				case 8:
					if (value != 0) {
						*p++ = '0';
					}
					break;
				case 10: break;
				case 16:
					*p++ = '0';
					*p++ = 'x';
					break;
				default: unreachable();
			}
		}

		char* const numberBegin = p;
		const std::to_chars_result result = std::to_chars(p, buffer + sizeof buffer, unsignedValue, radix);
		if (result.ec != std::errc{}) {
			throw FormatError{"Maximum number string length exceeded."};
		}

		char* const begin = buffer;
		char* const end = result.ptr;

		if (makeUppercase) {
			for (p = begin; p != end; ++p) {
				if (*p >= 'a' && *p <= 'z') {
					*p = static_cast<char>(*p - ('a' - 'A'));
				}
			}
		}

		switch (alignment) {
			case '<':
				output.append(StringView{begin, end});
				for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
					output.append(StringView{&fill, 1});
				}
				break;
			case '\0': [[fallthrough]];
			case '>':
				for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
					output.append(StringView{&fill, 1});
				}
				output.append(StringView{begin, end});
				break;
			case '^':
				if (const size_t width = static_cast<size_t>(end - begin); width < minWidth) {
					const size_t n = minWidth - width;
					const size_t halfRoundedDown = n / 2;
					const size_t halfRoundedUp = n - halfRoundedDown;
					for (size_t i = 0; i < halfRoundedDown; ++i) {
						output.append(StringView{&fill, 1});
					}
					output.append(StringView{begin, end});
					for (size_t i = 0; i < halfRoundedUp; ++i) {
						output.append(StringView{&fill, 1});
					}
				} else {
					output.append(StringView{begin, end});
				}
				break;
			case '0':
				output.append(StringView{begin, numberBegin});
				for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
					output.append(StringView{&alignment, 1});
				}
				output.append(StringView{numberBegin, end});
				break;
			default: unreachable();
		}
	}
};

template <grem::floating_point Float>
struct grem::Formatter<Float> {
	size_t minWidth = 0;
	int precision = -1;
	char fill = ' ';
	char alignment = '\0';
	char sign = '-';
	char presentationType = '\0';
	bool makeUppercase = false;
	bool ensureDecimalPoint = false;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		switch (*p) {
			case '\0': break;
			case '<': [[fallthrough]];
			case '>': [[fallthrough]];
			case '^': alignment = *p++; break;
			default:
				switch (*(p + 1)) {
					case '<': [[fallthrough]];
					case '>': [[fallthrough]];
					case '^':
						fill = *p++;
						alignment = *p++;
						break;
					default: break;
				}
				break;
		}

		switch (*p) {
			case '-': ++p; break;
			case '+': [[fallthrough]];
			case ' ': sign = *p++; break;
			default: break;
		}

		if (*p == '#') {
			ensureDecimalPoint = true;
			++p;
		}

		if (*p == '0') {
			if (alignment == '\0') {
				alignment = '0';
			}
			++p;
		}

		while (*p >= '0' && *p <= '9') {
			minWidth *= 10;
			minWidth += static_cast<size_t>(*p - '0');
			++p;
		}

		if (*p == '.') {
			++p;
			if (*p < '0' || *p > '9') {
				throw FormatError{"Invalid precision specifier."};
			}
			precision = 0;
			do {
				precision *= 10;
				precision += static_cast<size_t>(*p - '0');
				++p;
			} while (*p >= '0' && *p <= '9');
		}

		switch (*p) {
			case 'A': [[fallthrough]];
			case 'E': [[fallthrough]];
			case 'F': [[fallthrough]];
			case 'G': makeUppercase = true; [[fallthrough]];
			case 'a': [[fallthrough]];
			case 'e': [[fallthrough]];
			case 'f': [[fallthrough]];
			case 'g': presentationType = *p++; break;
			default: break;
		}
		return p;
	}

	void formatTo(FormatOutput& output, Float value) const {
		char buffer[256];

		char* p = buffer;
		const char* numberBegin = p;
		if (value < Float{}) {
			++numberBegin;
		} else if (value != 0 && sign != '-') {
			++numberBegin;
			*p++ = sign;
		}

		std::to_chars_result result{};
		switch (presentationType) {
			case '\0':
				if (precision < 0) {
					result = std::to_chars(p, buffer + sizeof buffer, value);
				} else {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::general, precision);
				}
				break;
			case 'A': [[fallthrough]];
			case 'a':
				if (precision < 0) {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::hex);
				} else {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::hex, precision);
				}
				break;
			case 'E': [[fallthrough]];
			case 'e':
				if (precision < 0) {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::scientific);
				} else {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::scientific, precision);
				}
				break;
			case 'F': [[fallthrough]];
			case 'f':
				if (precision < 0) {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::fixed);
				} else {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::fixed, precision);
				}
				break;
			case 'G': [[fallthrough]];
			case 'g':
				if (precision < 0) {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::general);
				} else {
					result = std::to_chars(p, buffer + sizeof buffer, value, std::chars_format::general, precision);
				}
				break;
			default: unreachable();
		}

		if (result.ec != std::errc{}) {
			throw FormatError{"Maximum number string length exceeded."};
		}

		char* const begin = buffer;
		char* end = result.ptr;

		if (makeUppercase) {
			for (p = begin; p != end; ++p) {
				if (*p >= 'a' && *p <= 'z') {
					*p = static_cast<char>(*p - ('a' - 'A'));
				}
			}
		}

		if (ensureDecimalPoint) {
			if (std::isfinite(value) && StringView{begin, end}.find('.') == StringView::npos) {
				if (static_cast<size_t>(end - begin) >= sizeof buffer) {
					throw FormatError{"Maximum number string length exceeded."};
				}
				*end++ = '.';
			}
		}

		switch (alignment) {
			case '<':
				output.append(StringView{begin, end});
				for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
					output.append(StringView{&fill, 1});
				}
				break;
			case '0':
				if (std::isfinite(value)) {
					output.append(StringView{begin, numberBegin});
					for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
						output.append(StringView{&alignment, 1});
					}
					output.append(StringView{numberBegin, end});
					break;
				}
				[[fallthrough]];
			case '\0': [[fallthrough]];
			case '>':
				for (size_t width = static_cast<size_t>(end - begin); width < minWidth; ++width) {
					output.append(StringView{&fill, 1});
				}
				output.append(StringView{begin, end});
				break;
			case '^':
				if (const size_t width = static_cast<size_t>(end - begin); width < minWidth) {
					const size_t n = minWidth - width;
					const size_t halfRoundedDown = n / 2;
					const size_t halfRoundedUp = n - halfRoundedDown;
					for (size_t i = 0; i < halfRoundedDown; ++i) {
						output.append(StringView{&fill, 1});
					}
					output.append(StringView{begin, end});
					for (size_t i = 0; i < halfRoundedUp; ++i) {
						output.append(StringView{&fill, 1});
					}
				} else {
					output.append(StringView{begin, end});
				}
				break;
			default: unreachable();
		}
	}
};

template <>
struct grem::Formatter<const void*> : Formatter<uintptr_t> {
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr const char* parseFormatSpecification(const char* p) {
		radix = 16;
		return Formatter<uintptr_t>::parseFormatSpecification(p);
	}

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const void* value) const {
		Formatter<uintptr_t>::formatTo(output, reinterpret_cast<uintptr_t>(value));
	}
};

template <>
struct grem::Formatter<void*> : Formatter<const void*> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, void* value) const {
		Formatter<const void*>::formatTo(output, static_cast<const void*>(value));
	}
};

template <>
struct grem::Formatter<std::nullptr_t> : Formatter<const void*> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, std::nullptr_t value) const {
		Formatter<const void*>::formatTo(output, static_cast<const void*>(value));
	}
};

template <>
struct grem::Formatter<grem::bool16_t> : Formatter<bool> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, bool16_t value) const {
		Formatter<bool>::formatTo(output, static_cast<bool>(value));
	}
};

template <>
struct grem::Formatter<grem::bool32_t> : Formatter<bool> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, bool32_t value) const {
		Formatter<bool>::formatTo(output, static_cast<bool>(value));
	}
};

template <>
struct grem::Formatter<grem::bool64_t> : Formatter<bool> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, bool64_t value) const {
		Formatter<bool>::formatTo(output, static_cast<bool>(value));
	}
};

template <>
struct grem::Formatter<grem::float16_t> : Formatter<float> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, float16_t value) const {
		Formatter<float>::formatTo(output, static_cast<float>(value));
	}
};

template <>
struct grem::Formatter<grem::i8norm> : Formatter<float> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, i8norm value) const {
		Formatter<float>::formatTo(output, static_cast<float>(value));
	}
};

template <>
struct grem::Formatter<grem::u8norm> : Formatter<float> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, u8norm value) const {
		Formatter<float>::formatTo(output, static_cast<float>(value));
	}
};

template <>
struct grem::Formatter<grem::i16norm> : Formatter<float> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, i16norm value) const {
		Formatter<float>::formatTo(output, static_cast<float>(value));
	}
};

template <>
struct grem::Formatter<grem::u16norm> : Formatter<float> {
	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, u16norm value) const {
		Formatter<float>::formatTo(output, static_cast<float>(value));
	}
};

template <grem::input_range Range>
requires(grem::formattable<grem::range_value_t<Range>>) struct grem::Formatter<Range> : RangeFormatter<grem::range_value_t<Range>> {
	using GREM_private_DefaultRangeFormatterTag = void;
};

template <grem::formattable... Ts>
struct grem::Formatter<grem::Tuple<Ts...>> {
	Tuple<Formatter<std::remove_cvref_t<Ts>>...> underlying{};
	StringView separator = ", ";
	StringView openingBracket = "(";
	StringView closingBracket = ")";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		switch (*p) {
			case 'm':
				if constexpr (sizeof...(Ts) == 2) {
					separator = ": ";
					openingBracket = {};
					closingBracket = {};
					++p;
				} else {
					throw FormatError{"Invalid format specification."};
				}
				break;
			case 'n':
				separator = {};
				openingBracket = {};
				closingBracket = {};
				break;
			default: break;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const auto& value) const {
		output.append(openingBracket);
		if (sizeof...(Ts) > 0) {
			get<0>(underlying).formatTo(output, get<0>(value));
			[&]<size_t... Indices>(std::index_sequence<Indices...>) -> void {
				((output.append(separator), get<1 + Indices>(underlying).formatTo(output, get<1 + Indices>(value))), ...);
			}(std::make_index_sequence<sizeof...(Ts) - 1>{});
		}
		output.append(closingBracket);
	}
};

template <grem::formattable T1, grem::formattable T2>
struct grem::Formatter<grem::Pair<T1, T2>> : Formatter<grem::Tuple<T1, T2>> {};

#endif
