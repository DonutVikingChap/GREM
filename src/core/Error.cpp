// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>

#include <exception> // std::exception, std::exception_ptr, std::current_exception, std::rethrow_if_nested

namespace grem {

namespace {

void writeNestedExceptionSeparator(String& output, StringView separator) {
	if (output.ends_with('.') || output.ends_with('!')) {
		output.pop_back();
	}
	output.append(separator);
}

void writeNestedExceptionMessage(String& output, const std::exception& outerException);

void writeNestedFilepathExceptionMessage(String& output, const std::exception& outerException) {
	try {
		std::rethrow_if_nested(outerException);
	} catch (const detail::FilepathError& nestedException) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append(nestedException.filepath);
		writeNestedFilepathExceptionMessage(output, nestedException);
	} catch (const Error& nestedException) {
		writeNestedExceptionSeparator(output, (nestedException.messageAttachesToPrecedingFilepath()) ? ":" : ": ");
		nestedException.writeMessage(output);
		writeNestedExceptionMessage(output, nestedException);
	} catch (const std::exception& nestedException) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append(nestedException.what());
		writeNestedExceptionMessage(output, nestedException);
	} catch (...) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append("Unknown error.");
	}
}

void writeNestedExceptionMessage(String& output, const std::exception& outerException) {
	try {
		std::rethrow_if_nested(outerException);
	} catch (const detail::FilepathError& nestedException) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append(nestedException.filepath);
		writeNestedFilepathExceptionMessage(output, nestedException);
	} catch (const Error& nestedException) {
		writeNestedExceptionSeparator(output, ":\n");
		nestedException.writeMessage(output);
		writeNestedExceptionMessage(output, nestedException);
	} catch (const std::exception& nestedException) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append(nestedException.what());
		writeNestedExceptionMessage(output, nestedException);
	} catch (...) {
		writeNestedExceptionSeparator(output, ":\n");
		output.append("Unknown error.");
	}
}

} // namespace

String Error::formatCurrentExceptionMessage() {
	String result{};
	if (const std::exception_ptr exceptionPointer = std::current_exception()) {
		try {
			std::rethrow_exception(exceptionPointer);
		} catch (const detail::FilepathError& nestedException) {
			result.append(nestedException.filepath);
			writeNestedFilepathExceptionMessage(result, nestedException);
		} catch (const Error& outermostException) {
			outermostException.writeMessage(result);
			writeNestedExceptionMessage(result, outermostException);
		} catch (const std::exception& outermostException) {
			result.append(outermostException.what());
			writeNestedExceptionMessage(result, outermostException);
		} catch (...) {
			result.append("Unknown error.");
		}
	}
	return result;
}

} // namespace grem
