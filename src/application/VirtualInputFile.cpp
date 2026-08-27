// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/application/VirtualInputFile.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>

#include <physfs.h> // PHYSFS_...

namespace grem::application {

void VirtualInputFile::close() {
	if (file) {
		if (PHYSFS_close(static_cast<PHYSFS_File*>(file.get())) == 0) {
			throw File::Error{String{"Failed to close file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
		}
		file.release();
	}
}

bool VirtualInputFile::eof() const noexcept {
	return !file || PHYSFS_eof(static_cast<PHYSFS_File*>(file.get())) != 0;
}

size_t VirtualInputFile::size() const noexcept {
	if (file) {
		const PHYSFS_sint64 length = PHYSFS_fileLength(static_cast<PHYSFS_File*>(file.get()));
		return (length < 0) ? File::npos : static_cast<size_t>(length);
	}
	return 0;
}

size_t VirtualInputFile::tellg() const noexcept {
	if (file) {
		const PHYSFS_sint64 position = PHYSFS_tell(static_cast<PHYSFS_File*>(file.get()));
		return (position < 0) ? File::npos : static_cast<size_t>(position);
	}
	return 0;
}

void VirtualInputFile::seekg(size_t position) {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	if (PHYSFS_seek(static_cast<PHYSFS_File*>(file.get()), static_cast<PHYSFS_uint64>(position)) == 0) {
		throw File::Error{String{"Failed to seek in file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

void VirtualInputFile::skipg(ptrdiff_t offset) {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	if (const PHYSFS_sint64 position = PHYSFS_tell(static_cast<PHYSFS_File*>(file.get())); position > 0) {
		if (PHYSFS_seek(static_cast<PHYSFS_File*>(file.get()), static_cast<PHYSFS_uint64>(position + offset)) == 0) {
			throw File::Error{String{"Failed to seek in file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
		}
	} else {
		throw File::Error{String{"Failed to get position in file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

Allocation<byte> VirtualInputFile::readBytesIntoAllocation(size_t maxLength) {
	Allocation<byte> result{};
	const size_t fileOffset = tellg();
	const size_t fileSize = size();
	if (fileOffset < fileSize) {
		result.resize(min(fileSize - fileOffset, maxLength));
		read(result);
	}
	return result;
}

ArrayList<byte> VirtualInputFile::readBytesIntoArrayList(size_t maxLength) {
	ArrayList<byte> result{};
	const size_t fileOffset = tellg();
	const size_t fileSize = size();
	if (fileOffset < fileSize) {
		result.resize(min(fileSize - fileOffset, maxLength));
		read(result);
	}
	return result;
}

Buffer<byte> VirtualInputFile::readBytesIntoBuffer(size_t maxLength) {
	Buffer<byte> result{};
	const size_t fileOffset = tellg();
	const size_t fileSize = size();
	if (fileOffset < fileSize) {
		result.resize(min(fileSize - fileOffset, maxLength));
		read(result);
	}
	return result;
}

String VirtualInputFile::readBytesIntoString(size_t maxLength) {
	String result{};
	const size_t fileOffset = tellg();
	const size_t fileSize = size();
	if (fileOffset < fileSize) {
		result.resize(min(fileSize - fileOffset, maxLength));
		read(asWritableBytes(Span<char>{result}));
	}
	return result;
}

Allocation<char> VirtualInputFile::readBytesIntoCString(size_t maxLength) {
	Allocation<char> result{};
	const size_t fileOffset = tellg();
	const size_t fileSize = size();
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

size_t VirtualInputFile::readSome(Span<byte> data) {
	return VirtualInputFile::readUntilEOF(data);
}

size_t VirtualInputFile::readUntilEOF(Span<byte> data) {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	const PHYSFS_sint64 bytesRead = PHYSFS_readBytes(static_cast<PHYSFS_File*>(file.get()), data.data(), static_cast<PHYSFS_uint64>(data.size()));
	if (bytesRead < 0 || (bytesRead == 0 && PHYSFS_eof(static_cast<PHYSFS_File*>(file.get())) == 0)) {
		throw File::Error{String{"Failed to read from file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	return static_cast<size_t>(bytesRead);
}

void VirtualInputFile::FileDeleter::operator()(void* handle) const noexcept {
	if (handle) {
		PHYSFS_close(static_cast<PHYSFS_File*>(handle));
	}
}

} // namespace grem::application
