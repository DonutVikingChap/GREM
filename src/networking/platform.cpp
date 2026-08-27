// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/networking/Error.hpp>
#include <GREM/networking/platform.hpp>

#include <system_error> // std::error_code

namespace grem::networking {

void ensurePlatformInitialized(std::error_code& errorCode) {
#ifdef _WIN32
	static const struct WSAInitializer {
		int status = 0;

		[[nodiscard]] WSAInitializer() noexcept {
			WSADATA wsaData{};
			status = WSAStartup(MAKEWORD(2, 2), &wsaData);
		}

		~WSAInitializer() {
			WSACleanup();
		}

		WSAInitializer(const WSAInitializer&) = delete;
		WSAInitializer(WSAInitializer&&) = delete;
		WSAInitializer& operator=(const WSAInitializer&) = delete;
		WSAInitializer& operator=(WSAInitializer&&) = delete;
	} wsaInitializer{};

	if (wsaInitializer.status == 0) {
		errorCode.clear();
	} else {
		errorCode = make_error_code(WSAError{.value = wsaInitializer.status});
	}
#else
	errorCode.clear();
#endif
}

} // namespace grem::networking
