// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_SHARED_LIBRARY_HPP
#define GREM_CORE_SYSTEM_SHARED_LIBRARY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/UniqueHandle.hpp>

namespace grem {

class Filesystem; // Forward declaration, to avoid including Filesystem.hpp

/**
 * Library of dynamically loaded symbols.
 *
 * On Windows, this corresponds to a Dynamically Linked Library (.dll file).
 * On Linux, this corresponds to a Dynamic Shared Object (.so file).
 * On Apple platforms, this corresponds to a Dynamic Library (.dylib file).
 * On other platforms, shared libraries are unsupported, and attempting to open
 * one always fails.
 */
class SharedLibrary {
public:
	/**
	 * Exception type for errors that may arise from a SharedLibrary.
	 */
	struct Error : grem::Error {
		using grem::Error::Error;
	};

	/**
	 * Get the proper filename extension for shared libraries on the current
	 * platform.
	 *
	 * \return the filename extension for shared libraries on the current
	 *         platform without a leading dot, or an empty string if the current
	 *         platform does not support shared libraries.
	 */
	[[nodiscard]] GREM_API(core) static CStringView getLibraryFilenameExtension();

	/**
	 * Construct a closed shared library.
	 */
	SharedLibrary() noexcept = default;

	/**
	 * Construct a shared library opened from a file.
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the shared library file to open.
	 *
	 * \throws File::Error on failure to locate the file.
	 * \throws SharedLibrary::Error on failure to open the library.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit SharedLibrary(const Filesystem& filesystem, CStringView filepath) {
		open(filesystem, filepath);
	}

	/**
	 * Check if the shared library is currently open.
	 *
	 * \return true if the library is open, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return is_open();
	}

	/**
	 * Check if the shared library is currently open.
	 *
	 * \return true if the library is open, false otherwise.
	 */
	[[nodiscard]] bool is_open() const noexcept {
		return static_cast<bool>(library);
	}

	/**
	 * Close the shared library if it is currently open, invalidating all
	 * current pointers to its symbols from this program.
	 *
	 * \throws SharedLibrary::Error on failure to close the library.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(core) void close();

	/**
	 * Open the shared library from a file.
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the shared library file to open.
	 *
	 * \throws File::Error on failure to locate the file.
	 * \throws SharedLibrary::Error on failure to open the library.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(core) void open(const Filesystem& filesystem, CStringView filepath);

	/**
	 * Get a reference to the symbol with a specific name in the library.
	 *
	 * \tparam T type of the symbol to get. Must exactly match the type of the
	 *         specified symbol in the library.
	 *
	 * \param name exact, potentially mangled, name of the symbol to search for.
	 *
	 * \return a reference to the requested symbol.
	 *
	 * \throws SharedLibrary::Error on failure to find or retrieve the symbol
	 *         address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename T>
	T& getSymbol(CStringView name) {
		return *(T*)getSymbolAddress(name);
	}

private:
	struct LibraryDeleter {
		GREM_API(core) void operator()(void* handle) const noexcept;
	};

	[[nodiscard]] GREM_API(core) void* getSymbolAddress(CStringView name) const;

	UniqueHandle<void*, LibraryDeleter> library{};
};

} // namespace grem

#endif
