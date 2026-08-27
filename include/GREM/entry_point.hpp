// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_ENTRY_POINT_HPP
#define GREM_ENTRY_POINT_HPP

#include <GREM/build_config.hpp>

#ifdef main
#undef main
#endif

#ifdef _WIN32
#include <windows.h> // LPWSTR, CP_UTF8, LocalFree, GetCommandLineW, CommandLineToArgvW, WideCharToMultiByte
//
#include <shellapi.h> // CommandLineToArgvW
//
#include <cstddef> // std::size_t
#include <memory>  // std::unique_ptr, std::make_unique
#endif

int GREM_private_main(int argc, char* argv[]);

#ifdef _WIN32

namespace {

char GREM_private_defaultProgramName[] = "app";

constexpr int GREM_private_argcDefault = 1;
char* GREM_private_argvDefault[]{GREM_private_defaultProgramName, nullptr};

int GREM_private_windowsMain() { // NOLINT(misc-definitions-in-headers)
	int argc = 0;
	if (LPWSTR* const arguments = CommandLineToArgvW(GetCommandLineW(), &argc); arguments && argc >= 1) {
		std::unique_ptr<char*[]> argv = std::make_unique<char*[]>(static_cast<std::size_t>(argc + 1));
		for (int i = 0; i < argc; ++i) {
			const std::size_t argumentIndex = static_cast<std::size_t>(i);
			try {
				const int sizeWithNullTerminator = WideCharToMultiByte(CP_UTF8, 0, arguments[argumentIndex], -1, nullptr, 0, nullptr, nullptr);
				if (sizeWithNullTerminator <= 1) {
					argv[argumentIndex] = new char[1]{'\0'}; // NOLINT(cppcoreguidelines-owning-memory)
				} else {
					argv[argumentIndex] = new char[static_cast<std::size_t>(sizeWithNullTerminator)]; // NOLINT(cppcoreguidelines-owning-memory)
					if (WideCharToMultiByte(CP_UTF8, 0, arguments[i], -1, argv[argumentIndex], sizeWithNullTerminator, nullptr, nullptr) != sizeWithNullTerminator) {
						argv[argumentIndex][0] = '\0';
					}
				}
			} catch (...) {
				while (i-- > 0) {
					delete[] argv[static_cast<std::size_t>(i)]; // NOLINT(cppcoreguidelines-owning-memory)
				}
				LocalFree(arguments);
				throw;
			}
		}
		argv[static_cast<std::size_t>(argc)] = nullptr;
		LocalFree(arguments);

		try {
			const int argcCopy = argc;
			const int result = GREM_private_main(argcCopy, argv.get());
			while (argc-- > 0) {
				delete[] argv[static_cast<std::size_t>(argc)]; // NOLINT(cppcoreguidelines-owning-memory)
			}
			return result;
		} catch (...) {
			while (argc-- > 0) {
				delete[] argv[static_cast<std::size_t>(argc)]; // NOLINT(cppcoreguidelines-owning-memory)
			}
			throw;
		}
	}
	return GREM_private_main(GREM_private_argcDefault, GREM_private_argvDefault);
}

} // namespace

#ifdef _MSC_VER
#if UNICODE
int wmain(int, wchar_t*[], wchar_t*) { // NOLINT(misc-definitions-in-headers)
	return GREM_private_windowsMain();
}
#else
int main(int, char*[]) { // NOLINT(misc-definitions-in-headers)
	return GREM_private_windowsMain();
}
#endif
#endif

#if UNICODE
extern "C" int __stdcall wWinMain(struct HINSTANCE__*, struct HINSTANCE__*, wchar_t*, int) { // NOLINT(misc-definitions-in-headers)
	return GREM_private_windowsMain();
}
#else
extern "C" int __stdcall WinMain(struct HINSTANCE__*, struct HINSTANCE__*, char*, int) { // NOLINT(misc-definitions-in-headers)
	return GREM_private_windowsMain();
}
#endif

#else

int main(int argc, char* argv[]) { // NOLINT(misc-definitions-in-headers)
	return GREM_private_main(argc, argv);
}

#endif

#define main GREM_private_main

#endif
