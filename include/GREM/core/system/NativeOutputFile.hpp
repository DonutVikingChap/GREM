// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_NATIVE_OUTPUT_FILE_HPP
#define GREM_CORE_SYSTEM_NATIVE_OUTPUT_FILE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>

#include <fstream> // std::ofstream, std::streamoff, std::streamsize
#include <utility> // std::move

namespace grem {

class NativeFilesystem; // Forward declaration, to avoid a circular include of NativeFilesystem.hpp.

/**
 * Unique handle to a writable file in the native Filesystem.
 */
class NativeOutputFile final
	: public OutputFile
	, public std::ofstream {
public:
	/**
	 * Construct a closed file handle without an associated file.
	 */
	NativeOutputFile() noexcept = default;

	/** Destructor */
	~NativeOutputFile() override = default;

	/** Copying a file handle is not allowed. */
	NativeOutputFile(const NativeOutputFile&) = delete;

	/** Move constructor. */
	NativeOutputFile(NativeOutputFile&& other) // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		: OutputFile(std::move(static_cast<OutputFile&>(other)))
		, std::ofstream(std::move(static_cast<std::ofstream&>(other))) {}

	/** Copying a file handle is not allowed. */
	NativeOutputFile& operator=(const NativeOutputFile&) = delete;

	/** Move assignment. */
	NativeOutputFile& operator=(NativeOutputFile&&) = default; // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)

	void close() override {
		std::ofstream::close();
	}

	[[nodiscard]] bool isOpen() const noexcept override {
		return std::ofstream::is_open();
	}

	[[nodiscard]] size_t tellp() const noexcept override {
		return static_cast<size_t>(const_cast<std::ofstream*>(static_cast<const std::ofstream*>(this))->tellp());
	}

	void seekp(size_t position) override {
		if (std::ofstream::seekp(static_cast<std::ofstream::pos_type>(static_cast<std::streamoff>(position)), std::ofstream::beg).fail()) {
			throw File::Error{"Failed to seek in file."};
		}
	}

	void skipp(ptrdiff_t offset) override {
		if (std::ofstream::seekp(static_cast<std::ofstream::pos_type>(static_cast<std::streamoff>(offset)), std::ofstream::cur).fail()) {
			throw File::Error{"Failed to seek in file."};
		}
	}

	size_t writeSome(Span<const byte> data) override {
		const size_t size = min(data.size(), size_t{Limits<std::streamsize>::MAX});
		if (std::ofstream::write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size)).fail()) {
			throw File::Error{"Failed to write to file."};
		}
		return size;
	}

	void flush() override {
		if (std::ofstream::flush().fail()) {
			throw File::Error{"Failed to flush file."};
		}
	}

private:
	friend NativeFilesystem;

	explicit NativeOutputFile(std::ofstream&& stream) noexcept
		: std::ofstream(std::move(stream)) {}
};

} // namespace grem

#endif
