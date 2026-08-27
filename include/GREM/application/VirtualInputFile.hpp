// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_VIRTUAL_INPUT_FILE_HPP
#define GREM_APPLICATION_VIRTUAL_INPUT_FILE_HPP

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
 * Unique handle to a readable file in the virtual Filesystem.
 *
 * \warning This file handle type may only be used during the lifetime of a
 *          Filesystem object, which initializes the relevant global context
 *          upon construction. Attempting to use the File API without an active
 *          Filesystem yields undefined behavior.
 */
class VirtualInputFile final : public InputFile {
public:
	/**
	 * Construct a closed file handle without an associated file.
	 */
	VirtualInputFile() noexcept = default;

	GREM_API(application) void close() override;

	[[nodiscard]] bool isOpen() const noexcept override {
		return static_cast<bool>(file);
	}

	[[nodiscard]] GREM_API(application) bool eof() const noexcept override;
	[[nodiscard]] GREM_API(application) size_t size() const noexcept override;

	[[nodiscard]] GREM_API(application) size_t tellg() const noexcept override;

	GREM_API(application) void seekg(size_t position) override;

	GREM_API(application) void skipg(ptrdiff_t offset) override;

	[[nodiscard]] GREM_API(application) Allocation<byte> readBytesIntoAllocation(size_t maxLength = Limits<size_t>::MAX) override;
	[[nodiscard]] GREM_API(application) ArrayList<byte> readBytesIntoArrayList(size_t maxLength = Limits<size_t>::MAX) override;
	[[nodiscard]] GREM_API(application) Buffer<byte> readBytesIntoBuffer(size_t maxLength = Limits<size_t>::MAX) override;
	[[nodiscard]] GREM_API(application) String readBytesIntoString(size_t maxLength = Limits<size_t>::MAX) override;
	[[nodiscard]] GREM_API(application) Allocation<char> readBytesIntoCString(size_t maxLength = Limits<size_t>::MAX) override;

	[[nodiscard]] GREM_API(application) size_t readSome(Span<byte> data) override;
	[[nodiscard]] GREM_API(application) size_t readUntilEOF(Span<byte> data) override;

private:
	friend VirtualFilesystem;

#ifdef GREM_USE_MULTITHREADING
	VirtualInputFile(void* handle, VirtualFilesystem::FileLock lock) noexcept
		: file(handle)
		, lock(std::move(lock)) {}
#else
	explicit VirtualInputFile(void* handle) noexcept
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
