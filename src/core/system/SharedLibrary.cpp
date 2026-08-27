// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/system/SharedLibrary.hpp>

#include <filesystem> // std::filesystem::path

#ifdef _WIN32

#include <GREM/core/data/Array.hpp>

#include <windows.h> // DWORD, HMODULE, CP_UTF8, FORMAT_MESSAGE_FROM_SYSTEM, MAKEWORD, MAKELANGID, LANG_NEUTRAL, SUBLANG_DEFAULT, GetLastError, WideCharToMultiByte, FormatMessageW, LoadLibraryW, GetProcAddress

#elif defined(__APPLE__) || defined(__linux__)

#include <dlfcn.h> // dlopen, dlclose, dlsym, dlerror, RTLD_...

#endif

namespace grem {

namespace {

[[nodiscard]] String getLastErrorMessage() {
#ifdef _WIN32
	Array<wchar_t, 256> buffer{};
	DWORD size =
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
	while (size > 0 && (buffer[size - 1] == '\r' || buffer[size - 1] == '\n')) {
		--size;
	}
	if (size == 0) {
		return "Unknown SharedLibrary error.";
	}
	String result(size_t{512}, '\0');
	const int length = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), static_cast<int>(size), result.data(), static_cast<int>(result.size()), nullptr, nullptr);
	if (length >= 0) {
		result.resize(static_cast<size_t>(length));
	} else {
		result.clear();
	}
	return result;
#elif defined(__APPLE__) || defined(__linux__)
	if (const char* const message = dlerror()) {
		return message;
	}
	return "Unknown SharedLibrary error.";
#else
	return "Unsupported platform.";
#endif
}

} // namespace

CStringView SharedLibrary::getLibraryFilenameExtension() {
#ifdef _WIN32
	return "dll";
#elif defined(__APPLE__)
	return "dylib";
#elif defined(__linux__)
	return "so";
#else
	return {};
#endif
}

void SharedLibrary::close() {
#ifdef _WIN32
	if (library) {
		if (FreeLibrary(static_cast<HMODULE>(library.get())) == 0) {
			throw SharedLibrary::Error{String{"Failed to close shared library:\n"} + getLastErrorMessage()};
		}
		library.release();
	}
#elif defined(__APPLE__) || defined(__linux__)
	if (library) {
		if (dlclose(library.get()) != 0) {
			throw SharedLibrary::Error{String{"Failed to close shared library:\n"} + getLastErrorMessage()};
		}
		library.release();
	}
#endif
}

void SharedLibrary::open(const Filesystem& filesystem, CStringView filepath) {
	close();

	const CStringView directoryFilepath = filesystem.findInputArchiveOfFile(filepath);
	if (directoryFilepath.empty()) {
		throw File::Error{String{"Failed to open shared library \""} + filepath.c_str() + "\":\n" + "File not found."};
	}
	const std::filesystem::path fullFilepath = std::filesystem::path{directoryFilepath.c_str()} / filepath.c_str();

#ifdef _WIN32
	library.reset(LoadLibraryW(fullFilepath.wstring().c_str()));
#elif defined(__APPLE__) || defined(__linux__)
	library.reset(dlopen(fullFilepath.generic_string().c_str(), RTLD_NOW | RTLD_LOCAL));
#endif
	if (!library) {
		throw SharedLibrary::Error{String{"Failed to open shared library \""} + filepath.c_str() + "\":\n" + getLastErrorMessage()};
	}
}

void SharedLibrary::LibraryDeleter::operator()(void* handle) const noexcept {
#ifdef _WIN32
	if (handle) {
		FreeLibrary(static_cast<HMODULE>(handle));
	}
#elif defined(__APPLE__) || defined(__linux__)
	if (handle) {
		dlclose(handle);
	}
#endif
}

void* SharedLibrary::getSymbolAddress(CStringView name) const {
#ifdef _WIN32
	void* const address = (void*)GetProcAddress(static_cast<HMODULE>(library.get()), name.c_str()); // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
#elif defined(__APPLE__) || defined(__linux__)
	void* const address = dlsym(library.get(), name.c_str());
#else
	void* const address = nullptr;
#endif
	if (!address) {
		throw SharedLibrary::Error{String{"Failed to get address of shared library symbol \""} + name.c_str() + "\":\n" + getLastErrorMessage()};
	}
	return address;
}

} // namespace grem
