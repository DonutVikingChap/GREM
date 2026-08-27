// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_NATIVE_INPUT_FILE_HPP
#define GREM_CORE_SYSTEM_NATIVE_INPUT_FILE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>

#include <fstream> // std::ifstream, std::streamoff, std::streamsize
#include <utility> // std::move, std::exchange

namespace grem {

class NativeFilesystem; // Forward declaration, to avoid a circular include of NativeFilesystem.hpp.

/**
 * Unique handle to a readable file in the native Filesystem.
 */
class NativeInputFile final
	: public InputFile
	, public std::ifstream {
public:
	/**
	 * Construct a closed file handle without an associated file.
	 */
	NativeInputFile() noexcept = default;

	/** Destructor */
	~NativeInputFile() override = default;

	/** Copying a file handle is not allowed. */
	NativeInputFile(const NativeInputFile&) = delete;

	/** Move constructor. */
	NativeInputFile(NativeInputFile&& other) // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)
		: InputFile(std::move(static_cast<InputFile&>(other)))
		, std::ifstream(std::move(static_cast<std::ifstream&>(other)))
		, fileSize(std::exchange(other.fileSize, size_t{0})) {}

	/** Copying a file handle is not allowed. */
	NativeInputFile& operator=(const NativeInputFile&) = delete;

	/** Move assignment. */
	NativeInputFile& operator=(NativeInputFile&&) = default; // NOLINT(cppcoreguidelines-noexcept-move-operations, performance-noexcept-move-constructor)

	void close() override {
		std::ifstream::close();
	}

	[[nodiscard]] bool isOpen() const noexcept override {
		return std::ifstream::is_open();
	}

	[[nodiscard]] bool eof() const noexcept override {
		return !isOpen() || std::ifstream::eof();
	}

	[[nodiscard]] size_t size() const noexcept override {
		return fileSize;
	}

	[[nodiscard]] size_t tellg() const noexcept override {
		return static_cast<size_t>(const_cast<std::ifstream*>(static_cast<const std::ifstream*>(this))->tellg());
	}

	void seekg(size_t position) override {
		if (std::ifstream::seekg(static_cast<std::ifstream::pos_type>(static_cast<std::streamoff>(position)), std::ifstream::beg).fail()) {
			throw File::Error{"Failed to seek in file."};
		}
	}

	void skipg(ptrdiff_t offset) override {
		if (std::ifstream::seekg(static_cast<std::ifstream::pos_type>(static_cast<std::streamoff>(offset)), std::ifstream::cur).fail()) {
			throw File::Error{"Failed to seek in file."};
		}
	}

	[[nodiscard]] Allocation<byte> readBytesIntoAllocation(size_t maxLength = Limits<size_t>::MAX) override {
		return readInto<Allocation<byte>>(maxLength);
	}

	[[nodiscard]] ArrayList<byte> readBytesIntoArrayList(size_t maxLength = Limits<size_t>::MAX) override {
		return readInto<ArrayList<byte>>(maxLength);
	}

	[[nodiscard]] Buffer<byte> readBytesIntoBuffer(size_t maxLength = Limits<size_t>::MAX) override {
		return readInto<Buffer<byte>>(maxLength);
	}

	[[nodiscard]] String readBytesIntoString(size_t maxLength = Limits<size_t>::MAX) override {
		return readInto<String>(maxLength);
	}

	[[nodiscard]] Allocation<char> readBytesIntoCString(size_t maxLength = Limits<size_t>::MAX) override {
		Allocation<char> result{};
		const size_t fileOffset = tellg();
		if (fileOffset < fileSize) {
			const size_t length = min(fileSize - fileOffset, maxLength);
			result.resize(length + 1);
			read(asWritableBytes(Span{result.data(), length}));
			result[length] = '\0';
		} else {
			result.resize(1);
			result[0] = '\0';
		}
		return result;
	}

	[[nodiscard]] size_t readSome(Span<byte> data) override {
		return NativeInputFile::readUntilEOF(data);
	}

	[[nodiscard]] size_t readUntilEOF(Span<byte> data) override {
		const size_t size = min(data.size(), size_t{Limits<std::streamsize>::MAX});
		if (std::ifstream::read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)).bad()) {
			throw File::Error{"Failed to read from file."};
		}
		const std::streamsize bytesRead = std::ifstream::gcount();
		if (bytesRead <= 0) {
			if (bytesRead < 0 || !std::ifstream::eof()) {
				throw File::Error{"Failed to read from file."};
			}
			return 0;
		}
		return static_cast<size_t>(bytesRead);
	}

	using InputFile::read;

private:
	friend NativeFilesystem;

	NativeInputFile(std::ifstream&& stream, size_t fileSize) noexcept
		: std::ifstream(std::move(stream))
		, fileSize(fileSize) {}

	template <typename Container>
	[[nodiscard]] Container readInto(size_t maxLength) {
		Container result{};
		const size_t fileOffset = tellg();
		if (fileOffset < fileSize) {
			result.resize(min(fileSize - fileOffset, maxLength));
			read(asWritableBytes(Span{result}));
		}
		return result;
	}

	size_t fileSize{};
};

} // namespace grem

#endif
