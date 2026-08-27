// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/platform.hpp>

#include <cerrno>       // errno
#include <string>       // std::string, std::to_string
#include <system_error> // std::error_code, std::error_category, std::generic_category

#ifdef _WIN32

#include <windows.h> // DWORD, CP_UTF8, FORMAT_MESSAGE_FROM_SYSTEM, MAKEWORD, MAKELANGID, LANG_NEUTRAL, SUBLANG_DEFAULT, WideCharToMultiByte, FormatMessageW

#endif

namespace grem::networking {

std::error_code Error::getLastErrorCode() noexcept {
#ifdef _WIN32
	return make_error_code(WSAError{static_cast<int>(WSAGetLastError())});
#else
	return std::error_code{errno, std::generic_category()};
#endif
}

} // namespace grem::networking

#ifdef _WIN32

namespace grem::networking {

const char* WSAErrorCategory::name() const noexcept {
	return "WSA";
}

std::string WSAErrorCategory::message(int condition) const {
	Array<wchar_t, 256> buffer{};
	DWORD size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, static_cast<DWORD>(condition), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer.data(),
		static_cast<DWORD>(buffer.size()), nullptr);
	while (size > 0 && (buffer[size - 1] == '\r' || buffer[size - 1] == '\n')) {
		--size;
	}
	if (size == 0) {
		return "WSA Error " + std::to_string(condition);
	}
	std::string result(size_t{512}, '\0');
	const int length = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(size), result.data(), static_cast<int>(result.size()), nullptr, nullptr);
	if (length >= 0) {
		result.resize(static_cast<size_t>(length));
	} else {
		result.clear();
	}
	return result;
}

} // namespace grem::networking

#endif

namespace grem::networking {

const char* EndpointErrorCategory::name() const noexcept {
	return "addrinfo";
}

std::string EndpointErrorCategory::message(int condition) const {
#ifdef _WIN32
	return WSAErrorCategory::instance().message(condition);
#else
	return gai_strerror(condition);
#endif
}

const char* SocketErrorCategory::name() const noexcept {
	return "socket";
}

std::string SocketErrorCategory::message(int condition) const {
	switch (static_cast<SocketError>(condition)) {
		case SocketError::WAIT: return "Wait";
		case SocketError::PARTIAL: return "Partial";
		case SocketError::DISCONNECTED: return "Disconnected";
		case SocketError::FAILED: return "Failed";
	}
	return "Unknown";
}

bool SocketErrorCategory::equivalent(const std::error_code& code, int condition) const noexcept {
	if (code.category() == *this && code.value() == condition) {
		return true;
	}

#ifdef _WIN32
	if (code.category() == WSAErrorCategory::instance()) {
		switch (code.value()) {
			case WSAEWOULDBLOCK: return condition == static_cast<int>(SocketError::WAIT);
			case WSAEALREADY: return condition == static_cast<int>(SocketError::WAIT);
			case WSAECONNABORTED: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case WSAECONNRESET: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case WSAETIMEDOUT: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case WSAENETRESET: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case WSAENOTCONN: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case WSAEISCONN: return condition == 0;
			default: break;
		}
		return condition == static_cast<int>(SocketError::FAILED);
	}
#endif

	if (code.category() == std::generic_category()) {
		if ((code.value() == EAGAIN) || (code.value() == EINPROGRESS)) {
			return condition == static_cast<int>(SocketError::WAIT);
		}
		switch (code.value()) {
			case EWOULDBLOCK: return condition == static_cast<int>(SocketError::WAIT);
			case ECONNABORTED: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case ECONNRESET: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case ETIMEDOUT: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case ENETRESET: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case ENOTCONN: return condition == static_cast<int>(SocketError::DISCONNECTED);
			case EPIPE: return condition == static_cast<int>(SocketError::DISCONNECTED);
			default: break;
		}
		return condition == static_cast<int>(SocketError::FAILED);
	}
	return false;
}

} // namespace grem::networking
