// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_VIRTUAL_OUTPUT_FILE_HPP
#define GREM_APPLICATION_VIRTUAL_OUTPUT_FILE_HPP

#include <GREM/build_config.hpp>

#include <GREM/application/VirtualFilesystem.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/synchronization.hpp>

#include <utility> // std::move
#endif

namespace grem::application {

/**
 * Unique handle to a writable file in the virtual Filesystem.
 *
 * \warning This file handle type may only be used during the lifetime of a
 *          Filesystem object, which initializes the relevant global context
 *          upon construction. Attempting to use the File API without an active
 *          Filesystem yields undefined behavior.
 */
class VirtualOutputFile final : public OutputFile {
public:
	/**
	 * Construct a closed file handle without an associated file.
	 */
	VirtualOutputFile() noexcept = default;

	GREM_API(application) void close() override;

	[[nodiscard]] bool isOpen() const noexcept override {
		return static_cast<bool>(file);
	}

	[[nodiscard]] GREM_API(application) size_t tellp() const noexcept override;

	GREM_API(application) void seekp(size_t position) override;
	GREM_API(application) void skipp(ptrdiff_t offset) override;

	[[nodiscard]] GREM_API(application) size_t writeSome(Span<const byte> data) override;
	GREM_API(application) void flush() override;

private:
	friend VirtualFilesystem;

#ifdef GREM_USE_MULTITHREADING
	VirtualOutputFile(void* handle, VirtualFilesystem::FileLock lock) noexcept
		: file(handle)
		, lock(std::move(lock)) {}
#else
	explicit VirtualOutputFile(void* handle) noexcept
		: file(handle) {}
#endif

	struct FileDeleter {
		GREM_API(application) void operator()(void* handle) const noexcept;
	};

	UniqueHandle<void*, FileDeleter> file{};
#ifdef GREM_USE_MULTITHREADING
	VirtualFilesystem::FileLock lock{};
#endif
};

} // namespace grem::application

#endif
