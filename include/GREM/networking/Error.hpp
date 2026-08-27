// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_NETWORKING_ERROR_HPP
#define GREM_NETWORKING_ERROR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/networking/platform.hpp>

#include <string>       // std::string
#include <system_error> // std::error_code, std::error_condition, std::error_category, std::is_error_code_enum, std::is_error_condition_enum
#include <type_traits>  // std::true_type

namespace grem::networking {

/**
 * Exception type for domain-specific errors originating from the
 * grem::networking module.
 */
struct Error : grem::Error {
	[[nodiscard]] GREM_API(networking) static std::error_code getLastErrorCode() noexcept;

	using grem::Error::Error;

	explicit Error(std::error_code errorCode)
		: Error(errorCode.message()) {}
};

} // namespace grem::networking

#ifdef _WIN32

namespace grem::networking {

struct WSAError {
	int value{};
};

} // namespace grem::networking

template <>
struct std::is_error_code_enum<grem::networking::WSAError> : std::true_type {};

namespace grem::networking {

struct WSAErrorCategory : std::error_category {
	[[nodiscard]] GREM_API(networking) const char* name() const noexcept override;
	[[nodiscard]] GREM_API(networking) std::string message(int condition) const override;

	[[nodiscard]] static const WSAErrorCategory& instance() noexcept {
		static const WSAErrorCategory category{};
		return category;
	}
};

[[nodiscard]] inline std::error_code make_error_code(WSAError err) {
	return std::error_code{static_cast<int>(err.value), WSAErrorCategory::instance()};
}

} // namespace grem::networking

#endif

namespace grem::networking {

enum class EndpointError : int { // NOLINT(performance-enum-size)
	AGAIN = EAI_AGAIN,
	BADFLAGS = EAI_BADFLAGS,
	FAIL = EAI_FAIL,
	FAMILY = EAI_FAMILY,
	MEMORY = EAI_MEMORY,
	NODATA = EAI_NODATA,
	NONAME = EAI_NONAME,
	SERVICE = EAI_SERVICE,
	SOCKTYPE = EAI_SOCKTYPE,
#ifndef _WIN32
	ADDRFAMILY = EAI_ADDRFAMILY,
	SYSTEM = EAI_SYSTEM,
	INPROGRESS = EAI_INPROGRESS,
	CANCELED = EAI_CANCELED,
	NOTCANCELED = EAI_NOTCANCELED,
	ALLDONE = EAI_ALLDONE,
	INTR = EAI_INTR,
	IDN_ENCODE = EAI_IDN_ENCODE,
#endif
};

} // namespace grem::networking

template <>
struct std::is_error_code_enum<grem::networking::EndpointError> : std::true_type {};

namespace grem::networking {

struct EndpointErrorCategory : std::error_category {
	[[nodiscard]] GREM_API(networking) const char* name() const noexcept override;
	[[nodiscard]] GREM_API(networking) std::string message(int condition) const override;

	[[nodiscard]] static const EndpointErrorCategory& instance() noexcept {
		static const EndpointErrorCategory category{};
		return category;
	}
};

[[nodiscard]] inline std::error_code make_error_code(EndpointError error) {
	return std::error_code{static_cast<int>(error), EndpointErrorCategory::instance()};
}

enum class SocketError : int { // NOLINT(performance-enum-size)
	// The value 0 is reserved for success.
	WAIT = 1,     ///< Try again later.
	PARTIAL,      ///< The data was partially transmitted.
	DISCONNECTED, ///< The socket disconnected.
	FAILED,       ///< The operation failed.
};

} // namespace grem::networking

template <>
struct std::is_error_condition_enum<grem::networking::SocketError> : std::true_type {};

namespace grem::networking {

struct SocketErrorCategory : std::error_category {
	[[nodiscard]] GREM_API(networking) const char* name() const noexcept override;
	[[nodiscard]] GREM_API(networking) std::string message(int condition) const override;
	[[nodiscard]] GREM_API(networking) bool equivalent(const std::error_code& code, int condition) const noexcept override;

	[[nodiscard]] static const SocketErrorCategory& instance() noexcept {
		static const SocketErrorCategory category{};
		return category;
	}
};

[[nodiscard]] inline std::error_code make_error_code(SocketError error) {
	return std::error_code{static_cast<int>(error), SocketErrorCategory::instance()};
}

[[nodiscard]] inline std::error_condition make_error_condition(SocketError error) {
	return std::error_condition{static_cast<int>(error), SocketErrorCategory::instance()};
}

} // namespace grem::networking

#endif
