// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DEBUG_FORMATTING_HPP
#define GREM_CORE_DEBUG_FORMATTING_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/base16.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>

#include <cstdio>      // stdout, stderr, std::FILE
#include <iterator>    // std::begin, std::end, std::iterator_traits, std::forward_iterator_tag
#include <type_traits> // std::type_identity_t, std::is_empty_v
#include <utility>     // std::index_sequence, std::make_index_sequence, std::declval, std::remove_cvref_t

namespace grem {

template <typename T>
struct DebugFormatter {
	constexpr DebugFormatter(size_t = 0) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const T&) const noexcept {
		return 1;
	}

	void setCompact(bool) {}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const T& value) const {
		constexpr CStringView HEXADECIMAL_DIGITS = "0123456789ABCDEF";
		output.append("<hex");
		for (const byte b : asBytes(Span{&value, 1})) {
			output.append(" ");
			output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(b) >> 4) & 0x0F], 1});
			output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(b) & 0x0F)], 1});
		}
		output.append(">");
	}
};

template <>
struct DebugFormatter<byte> {
	constexpr DebugFormatter(size_t = 0) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const byte&) const noexcept {
		return 1;
	}

	void setCompact(bool) {}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const byte& value) const {
		constexpr CStringView HEXADECIMAL_DIGITS = "0123456789ABCDEF";
		output.append("<hex ");
		output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(value) >> 4) & 0x0F], 1});
		output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(value) & 0x0F)], 1});
		output.append(">");
	}
};

template <>
struct DebugFormatter<StringView> {
	char quote = '\"';

	constexpr DebugFormatter(size_t = 0) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const StringView&) const noexcept {
		return 1;
	}

	void setCompact(bool) {}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, StringView value) const {
		constexpr CStringView HEXADECIMAL_DIGITS = "0123456789ABCDEF";
		output.append(StringView{&quote, 1});
		size_t begin = 0;
		for (size_t i = 0; i < value.size(); ++i) {
			const char ch = value[i];
			if (ch < ' ' || ch > '~' || ch == quote || ch == '\\') {
				output.append(value.substr(begin, i - begin));
				switch (ch) {
					case '\"': output.append("\\\"'"); break;
					case '\'': output.append("\\\''"); break;
					case '\\': output.append("\\\\'"); break;
					case '\b': output.append("\\b'"); break;
					case '\f': output.append("\\f'"); break;
					case '\n': output.append("\\n'"); break;
					case '\r': output.append("\\r'"); break;
					case '\t': output.append("\\t'"); break;
					case '\v': output.append("\\v'"); break;
					case '\0': output.append("\\0'"); break;
					default:
						output.append("\\x");
						output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) >> 4) & 0x0F], 1});
						output.append(StringView{&HEXADECIMAL_DIGITS[(bit_cast<uint8_t>(ch) & 0x0F)], 1});
						break;
				}
				begin = i + 1;
			}
		}
		if (begin < value.size()) {
			output.append(value.substr(begin));
		}
		output.append(StringView{&quote, 1});
	}
};

template <>
struct DebugFormatter<grem::CStringView> : DebugFormatter<StringView> {
	using DebugFormatter<StringView>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, CStringView value) const {
		DebugFormatter<StringView>::formatTo(output, StringView{value});
	}
};

template <>
struct DebugFormatter<grem::String> : DebugFormatter<StringView> {
	using DebugFormatter<StringView>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const String& value) const {
		DebugFormatter<StringView>::formatTo(output, StringView{value});
	}
};

template <grem::size_t Length>
struct DebugFormatter<grem::ConstantString<char, Length>> : DebugFormatter<StringView> {
	using DebugFormatter<StringView>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const ConstantString<char, Length>& value) const {
		DebugFormatter<StringView>::formatTo(output, StringView{value});
	}
};

template <>
struct DebugFormatter<char> : DebugFormatter<StringView> {
	constexpr DebugFormatter(size_t = 0) {
		quote = '\'';
	}

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, char value) const {
		DebugFormatter<StringView>::formatTo(output, StringView{&value, 1});
	}
};

template <>
struct DebugFormatter<const char*> : DebugFormatter<CStringView> {
	using DebugFormatter<CStringView>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const char* value) const {
		DebugFormatter<CStringView>::formatTo(output, CStringView{value});
	}
};

template <>
struct DebugFormatter<char*> : DebugFormatter<const char*> {
	using DebugFormatter<const char*>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, char* value) const {
		DebugFormatter<const char*>::formatTo(output, static_cast<const char*>(value));
	}
};

template <grem::size_t N>
struct DebugFormatter<char[N]> : DebugFormatter<const char*> {
	using DebugFormatter<const char*>::DebugFormatter;

	GREM_ALWAYS_INLINE void formatTo(FormatOutput& output, const char (&value)[N]) const {
		DebugFormatter<const char*>::formatTo(output, static_cast<const char*>(value));
	}
};

template <formattable T>
requires(!requires { typename Formatter<T>::GREM_private_DefaultRangeFormatterTag; }) struct DebugFormatter<T> : Formatter<T> {
	constexpr DebugFormatter(size_t = 0) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const T&) const noexcept {
		return 1;
	}

	void setCompact(bool) {}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			++p;
		}
		return p;
	}
};

template <typename T>
struct DebugRangeFormatter {
	DebugFormatter<T> underlying{};
	size_t indentation = 0;
	bool compact = false;

	constexpr DebugRangeFormatter(size_t indentation = 0)
		: underlying(indentation + 1)
		, indentation(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const auto& value) const noexcept {
		if constexpr (convertible_to<typename std::iterator_traits<decltype(std::begin(value))>::iterator_category, std::forward_iterator_tag>) {
			size_t result = 1;
			for (const auto& element : value) {
				result += underlying.getRecursiveSize(element);
			}
			return result;
		} else {
			return 1;
		}
	}

	void setCompact(bool newCompact) {
		compact = newCompact;
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			setCompact(true);
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const auto& value) const {
		if (compact || getRecursiveSize(value) - 1 <= 4) {
			output.append("[");
			auto it = std::begin(value);
			auto end = std::end(value);
			if (it != end) {
				underlying.formatTo(output, *it++);
				while (it != end) {
					output.append(", ");
					underlying.formatTo(output, *it++);
				}
			}
			output.append("]");
		} else {
			output.append("[\n");
			auto it = std::begin(value);
			auto end = std::end(value);
			if (it != end) {
				for (size_t i = 0; i < indentation + 1; ++i) {
					output.append("    ");
				}
				underlying.formatTo(output, *it++);
				while (it != end) {
					output.append(",\n");
					for (size_t i = 0; i < indentation + 1; ++i) {
						output.append("    ");
					}
					underlying.formatTo(output, *it++);
				}
			}
			output.append("\n");
			for (size_t i = 0; i < indentation; ++i) {
				output.append("    ");
			}
			output.append("]");
		}
	}
};

template <input_range Range>
requires(!formattable<Range> || requires { typename Formatter<Range>::GREM_private_DefaultRangeFormatterTag; })
struct DebugFormatter<Range> : DebugRangeFormatter<range_value_t<Range>> {
	using DebugRangeFormatter<range_value_t<Range>>::DebugRangeFormatter;
};

template <typename... Ts>
requires(!formattable<Ts> || ...) struct DebugFormatter<Tuple<Ts...>> {
	size_t indentation = 0;
	bool compact = false;

	constexpr DebugFormatter(size_t indentation = 0)
		: indentation(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const auto& value) const noexcept {
		size_t result = 1;
		meta::forEach(value, [&]<typename T>(const T& element) -> void { result += DebugFormatter<std::remove_cvref_t<T>>{}.getRecursiveSize(element); });
		return result;
	}

	void setCompact(bool newCompact) {
		compact = newCompact;
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			setCompact(true);
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const auto& value) const {
		if constexpr (sizeof...(Ts) > 0) {
			if (compact || getRecursiveSize(value) - 1 <= 4) {
				output.append("(");
				meta::forEachIndex<sizeof...(Ts)>([&](auto index) -> void {
					if constexpr (index != 0) {
						output.append(", ");
					}
					const auto& field = get<index>(value);
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append(")");
			} else {
				output.append("(\n");
				meta::forEachIndex<sizeof...(Ts)>([&](auto index) -> void {
					if constexpr (index != 0) {
						output.append(",\n");
					}
					for (size_t i = 0; i < indentation + 1; ++i) {
						output.append("    ");
					}
					const auto& field = get<index>(value);
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append("\n");
				for (size_t i = 0; i < indentation; ++i) {
					output.append("    ");
				}
				output.append(")");
			}
		} else {
			output.append("()");
		}
	}
};

template <typename T1, typename T2>
requires(!formattable<T1> || !formattable<T2>) struct DebugFormatter<Pair<T1, T2>> : DebugFormatter<Tuple<T1, T2>> {
	using DebugFormatter<Tuple<T1, T2>>::DebugFormatter;
};

template <typename T>
requires(!formattable<Optional<T>>) struct DebugFormatter<Optional<T>> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const Optional<T>& value) const noexcept {
		return (value) ? underlying.getRecursiveSize(*value) : 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const Optional<T>& value) const {
		if (value) {
			underlying.formatTo(output, *value);
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!std::is_void_v<T>) struct DebugFormatter<T*> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(T* value) const noexcept {
		return (value) ? underlying.getRecursiveSize(*value) : 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, T* value) const {
		if (value) {
			underlying.formatTo(output, *value);
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<UniquePointer<T>>) struct DebugFormatter<UniquePointer<T>> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const UniquePointer<T>& value) const noexcept {
		return (value) ? underlying.getRecursiveSize(*value) : 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const UniquePointer<T>& value) const {
		if (value) {
			underlying.formatTo(output, *value);
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<UniquePointer<T[]>>) struct DebugFormatter<UniquePointer<T[]>> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const UniquePointer<T>& value) const noexcept {
		return (value) ? underlying.getRecursiveSize(*value) : 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const UniquePointer<T>& value) const {
		if (value) {
			output.append("[");
			underlying.formatTo(output, *value);
			output.append(", ... (unknown size)]");
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<SharedPointer<T>>) struct DebugFormatter<SharedPointer<T>> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const SharedPointer<T>& value) const noexcept {
		return (value) ? underlying.getRecursiveSize(*value) : 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const SharedPointer<T>& value) const {
		if (value) {
			underlying.formatTo(output, *value);
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<SharedPointer<T[]>>) struct DebugFormatter<SharedPointer<T[]>> {
	DebugRangeFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const SharedPointer<T[]>& value) const noexcept {
		if (value) {
			return underlying.getRecursiveSize(Span{value.get(), value.size()});
		}
		return 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const SharedPointer<T[]>& value) const {
		if (value) {
			underlying.formatTo(output, Span{value.get(), value.size()});
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<WeakPointer<T>>) struct DebugFormatter<WeakPointer<T>> {
	DebugFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const WeakPointer<T>& value) const noexcept {
		if (value) {
			if (const SharedPointer<T> p = value.lock()) {
				return underlying.getRecursiveSize(*p);
			}
		}
		return 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const WeakPointer<T>& value) const {
		if (value) {
			if (const SharedPointer<T> p = value.lock()) {
				underlying.formatTo(output, *p);
			} else {
				output.append("<expired>");
			}
		} else {
			output.append("null");
		}
	}
};

template <typename T>
requires(!formattable<WeakPointer<T[]>>) struct DebugFormatter<WeakPointer<T[]>> {
	DebugRangeFormatter<std::remove_cvref_t<T>> underlying{};

	constexpr DebugFormatter(size_t indentation = 0)
		: underlying(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const WeakPointer<T[]>& value) const noexcept {
		if (value) {
			if (const SharedPointer<T> p = value.lock()) {
				return underlying.getRecursiveSize(Span{p.get(), p.size()});
			}
		}
		return 1;
	}

	void setCompact(bool newCompact) {
		underlying.setCompact(newCompact);
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return underlying.parseFormatSpecification(p);
	}

	void formatTo(FormatOutput& output, const WeakPointer<T>& value) const {
		if (value) {
			if (const SharedPointer<T> p = value.lock()) {
				underlying.formatTo(output, Span{p.get(), p.size()});
			} else {
				output.append("<expired>");
			}
		} else {
			output.append("null");
		}
	}
};

template <typename... Ts>
requires(!formattable<Variant<Ts...>>) struct DebugFormatter<Variant<Ts...>> {
	size_t indentation = 0;
	bool compact = false;

	constexpr DebugFormatter(size_t indentation = 0)
		: indentation(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const Variant<Ts...>& value) const noexcept {
		if (value.valueless_by_exception()) {
			return 1;
		}
		return match(value)([&]<typename T>(const T& v) -> size_t { return DebugFormatter<std::remove_cvref_t<T>>{indentation}.getRecursiveSize(v); });
	}

	void setCompact(bool newCompact) {
		compact = newCompact;
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			setCompact(true);
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const Variant<Ts...>& value) const {
		if (value.valueless_by_exception()) {
			output.append("<valueless by exception>");
		} else {
			match(value)([&]<typename T>(const T& v) -> void {
				output.append(meta::unqualified_type_name_v<T>);
				output.append("(");
				DebugFormatter<std::remove_cvref_t<T>> underlying{indentation};
				underlying.setCompact(compact);
				underlying.formatTo(output, v);
				output.append(")");
			});
		}
	}
};

template <enumeration Enum>
requires(!formattable<Enum>) struct DebugFormatter<Enum> {
	constexpr DebugFormatter(size_t = 0) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const Enum&) const noexcept {
		return 1;
	}

	void setCompact(bool) {}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		return p;
	}

	void formatTo(FormatOutput& output, const Enum& value) const {
		bool found = false;
		meta::forEachNamedEnumerand<Enum>([&](StringView name, auto type) -> void {
			if (!found && value == type) {
				output.append(meta::unqualified_type_name_v<Enum>);
				output.append("::");
				output.append(name);
				found = true;
			}
		});
	}
};

namespace detail {

template <typename... Ts>
consteval Variant<Ts...> getVariant(const Variant<Ts...>&);

} // namespace detail

template <aggregate Aggregate>
requires(!formattable<Aggregate> && !requires(const Aggregate t) { detail::getVariant(t); }) struct DebugFormatter<Aggregate> {
	size_t indentation = 0;
	bool compact = false;

	constexpr DebugFormatter(size_t indentation = 0)
		: indentation(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const Aggregate& value) const noexcept {
		size_t result = 1;
		meta::forEachField(value,
			[&]<typename T>(const T& field) -> void { result += DebugFormatter<std::remove_cvref_t<decltype(field)>>{indentation + 1}.getRecursiveSize(field); });
		return result;
	}

	void setCompact(bool newCompact) {
		compact = newCompact;
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			setCompact(true);
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const Aggregate& value) const {
		if constexpr (meta::aggregate_size_v<Aggregate> > 0) {
			if (compact || getRecursiveSize(value) - 1 <= 4) {
				output.append("{ ");
				meta::forEachIndexedNamedField(value, [&](auto index, StringView name, const auto& field) -> void {
					if constexpr (index != 0) {
						output.append(", ");
					}
					output.append(name);
					output.append(": ");
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append(" }");
			} else {
				output.append("{\n");
				meta::forEachIndexedNamedField(value, [&](auto index, StringView name, const auto& field) -> void {
					if constexpr (index != 0) {
						output.append(",\n");
					}
					for (size_t i = 0; i < indentation + 1; ++i) {
						output.append("    ");
					}
					output.append(name);
					output.append(": ");
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append("\n");
				for (size_t i = 0; i < indentation; ++i) {
					output.append("    ");
				}
				output.append("}");
			}
		} else {
			output.append("{}");
		}
	}
};

template <typename VariantDerived>
requires(!formattable<VariantDerived> && !aggregate<VariantDerived> && requires(const VariantDerived t) { detail::getVariant(t); })
struct DebugFormatter<VariantDerived> : DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerived&>()))> {
	using DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerived&>()))>::DebugFormatter;
};

template <aggregate VariantDerivedAggregate>
requires(!formattable<VariantDerivedAggregate> && requires(const VariantDerivedAggregate t) { detail::getVariant(t); })
struct DebugFormatter<VariantDerivedAggregate> : DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))> {
	size_t indentation = 0;
	bool compact = false;

	constexpr DebugFormatter(size_t indentation = 0)
		: DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>(indentation + 1)
		, indentation(indentation) {}

	[[nodiscard]] constexpr size_t getRecursiveSize(const VariantDerivedAggregate& value) const noexcept {
		size_t result = DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>::getRecursiveSize(value);
		meta::forEachField(value,
			[&]<typename T>(const T& field) -> void { result += DebugFormatter<std::remove_cvref_t<decltype(field)>>{indentation + 1}.getRecursiveSize(field); });
		return result;
	}

	void setCompact(bool newCompact) {
		DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>::setCompact(newCompact);
		compact = newCompact;
	}

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'c') {
			setCompact(true);
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const VariantDerivedAggregate& value) const {
		if constexpr (meta::aggregate_size_v<VariantDerivedAggregate> > 0) {
			if (compact || getRecursiveSize(value) - 1 <= 4) {
				output.append("{ ");
				DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>::formatTo(output, value);
				meta::forEachNamedField(value, [&](StringView name, const auto& field) -> void {
					output.append(", ");
					output.append(name);
					output.append(": ");
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append(" }");
			} else {
				output.append("{\n");
				DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>::formatTo(output, value);
				meta::forEachNamedField(value, [&](StringView name, const auto& field) -> void {
					output.append(",\n");
					for (size_t i = 0; i < indentation + 1; ++i) {
						output.append("    ");
					}
					output.append(name);
					output.append(": ");
					DebugFormatter<std::remove_cvref_t<decltype(field)>> underlying{indentation + 1};
					underlying.setCompact(compact);
					underlying.formatTo(output, field);
				});
				output.append("\n");
				for (size_t i = 0; i < indentation; ++i) {
					output.append("    ");
				}
				output.append("}");
			}
		} else {
			DebugFormatter<decltype(detail::getVariant(std::declval<const VariantDerivedAggregate&>()))>::formatTo(output, value);
		}
	}
};

template <typename... Args>
class DebugFormatStringBase {
public:
	template <typename T>
	consteval DebugFormatStringBase(const T& format) requires(convertible_to<T, CStringView>)
		: format(format) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, get(), {FormatArgument::createEmpty<Args, DebugFormatter<Args>>()...});
	}

	constexpr DebugFormatStringBase(RuntimeFormat format) noexcept
		: format(format) {}

	[[nodiscard]] constexpr CStringView get() const noexcept {
		return format;
	}

private:
	CStringView format;
};

template <typename... Args>
using DebugFormatString = DebugFormatStringBase<std::type_identity_t<Args>...>;

template <typename... Args>
[[nodiscard]] constexpr size_t getFormattedSizeDebug(DebugFormatString<Args...> format, const Args&... args) {
	detail::FormatSizeCounter sizeCounter{};
	formatToV(sizeCounter, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
	return sizeCounter.output;
}

template <typename Output, typename... Args>
constexpr void formatToDebug(Output& output, DebugFormatString<Args...> format, const Args&... args) {
	if constexpr (requires(Output out, const size_t n) {
					  out.data();
					  out.size();
					  out.resize(n);
				  }) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
		const size_t formattedSize = sizeCounter.output;
		const size_t offset = output.size();
		output.resize(offset + formattedSize);
		try {
			char* p = output.data() + offset;
			formatToDebug(p, format, args...);
		} catch (...) {
			output.resize(offset);
			throw;
		}
	} else if constexpr (requires(Output out, const size_t n) {
							 out.size();
							 out.reserve(n);
						 }) {
		detail::FormatSizeCounter sizeCounter{};
		formatToV(sizeCounter, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
		const size_t formattedSize = sizeCounter.output;
		output.reserve(output.size() + formattedSize);
		detail::AppendingFormatWriter writer{output};
		formatToV(writer, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
	} else if constexpr (requires { *output++ = '\0'; }) {
		detail::IteratorFormatWriter writer{output};
		formatToV(writer, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
		output = writer.output;
	} else {
		detail::AppendingFormatWriter writer{output};
		formatToV(writer, format.get(), {FormatArgument::create<Args, DebugFormatter<Args>>(args)...});
	}
}

template <typename... Args>
GREM_ALWAYS_INLINE void printDebug(std::FILE* output, DebugFormatString<Args...> format, const Args&... args) {
	detail::CFileFormatWriter writer{output};
	formatToDebug(writer, format, args...);
}

GREM_ALWAYS_INLINE void printlnDebug(std::FILE* output) {
	println(output);
}

template <typename... Args>
GREM_ALWAYS_INLINE void printlnDebug(std::FILE* output, DebugFormatString<Args...> format, const Args&... args) {
	printDebug(output, format, args...);
	printlnDebug(output);
}

template <typename... Args>
GREM_ALWAYS_INLINE void printDebug(DebugFormatString<Args...> format, const Args&... args) {
	printDebug(stdout, format, args...);
}

GREM_ALWAYS_INLINE void printlnDebug() {
	printlnDebug(stdout);
}

template <typename... Args>
GREM_ALWAYS_INLINE void printlnDebug(DebugFormatString<Args...> format, const Args&... args) {
	printlnDebug(stdout, format, args...);
}

template <typename... Args>
GREM_ALWAYS_INLINE void eprintDebug(DebugFormatString<Args...> format, const Args&... args) {
	printDebug(stderr, format, args...);
}

GREM_ALWAYS_INLINE void eprintlnDebug() {
	printlnDebug(stderr);
}

template <typename... Args>
GREM_ALWAYS_INLINE void eprintlnDebug(DebugFormatString<Args...> format, const Args&... args) {
	printlnDebug(stderr, format, args...);
}

template <typename... Args>
[[nodiscard]] inline String formatStringDebug(DebugFormatString<Args...> format, const Args&... args) {
	String result{};
	formatToDebug(result, format, args...);
	return result;
}

template <size_t N, typename... Args>
inline detail::SmallFormattedString<N> formatSmallStringDebug(DebugFormatString<Args...> format, const Args&... args) {
	detail::SmallFormattedString<N> result{};
	formatToDebug(result, format, args...);
	return result;
}

} // namespace grem

#endif
