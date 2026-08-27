// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_ERROR_HPP
#define GREM_CORE_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/concepts.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

#include <exception> // std::exception, std::throw_with_nested
#include <stdexcept> // std::runtime_error
#include <utility>   // std::move, std::forward

namespace grem {

namespace detail {

struct FilepathError : std::exception {
	String filepath;

	explicit FilepathError(String filepath)
		: filepath(std::move(filepath)) {}

	[[nodiscard]] const char* what() const noexcept override {
		return filepath.c_str();
	}
};

} // namespace detail

/**
 * Base exception type for runtime errors.
 */
class Error : public std::runtime_error {
public:
	template <typename OuterException>
	[[noreturn]] static void throwWithNested(OuterException&& error) {
		std::throw_with_nested(std::forward<OuterException>(error));
	}

	[[noreturn]] static void throwWithNestedFilepath(String filepath) {
		std::throw_with_nested(detail::FilepathError{std::move(filepath)});
	}

	[[noreturn]] static void throwWithNestedFilepath(CStringView filepath) {
		std::throw_with_nested(detail::FilepathError{String{filepath}});
	}

	[[nodiscard]] GREM_API(core) static String formatCurrentExceptionMessage();

	explicit Error(CStringView message)
		: runtime_error(message.c_str()) {}

	template <convertible_to<StringView> StringViewConvertible>
	explicit Error(const StringViewConvertible& message) requires(!convertible_to<StringViewConvertible, CStringView>)
		: runtime_error(String{StringView{message}}) {}

	virtual void writeMessage(String& output) const {
		output.append(what());
	}

	[[nodiscard]] virtual bool messageAttachesToPrecedingFilepath() const noexcept {
		return false;
	}
};

} // namespace grem

#endif
