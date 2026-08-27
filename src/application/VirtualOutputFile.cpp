// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/application/VirtualOutputFile.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/fundamentals.hpp>

#include <physfs.h> // PHYSFS_...

namespace grem::application {

void VirtualOutputFile::close() {
	if (file) {
		if (PHYSFS_close(static_cast<PHYSFS_File*>(file.get())) == 0) {
			throw File::Error{String{"Failed to close file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
		}
		file.release();
	}
}

size_t VirtualOutputFile::tellp() const noexcept {
	if (file) {
		const PHYSFS_sint64 position = PHYSFS_tell(static_cast<PHYSFS_File*>(file.get()));
		return (position < 0) ? File::npos : static_cast<size_t>(position);
	}
	return 0;
}

void VirtualOutputFile::seekp(size_t position) {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	if (PHYSFS_seek(static_cast<PHYSFS_File*>(file.get()), static_cast<PHYSFS_uint64>(position)) == 0) {
		throw File::Error{String{"Failed to seek in file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

void VirtualOutputFile::skipp(ptrdiff_t offset) {
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

size_t VirtualOutputFile::writeSome(Span<const byte> data) {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	const PHYSFS_sint64 bytesWritten = PHYSFS_writeBytes(static_cast<PHYSFS_File*>(file.get()), data.data(), static_cast<PHYSFS_uint64>(data.size()));
	if (bytesWritten < 0) {
		throw File::Error{String{"Failed to write to file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	return static_cast<size_t>(bytesWritten);
}

void VirtualOutputFile::flush() {
	if (!file) {
		throw File::Error{"Invalid file handle."};
	}
	if (PHYSFS_flush(static_cast<PHYSFS_File*>(file.get())) == 0) {
		throw File::Error{String{"Failed to flush file:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

void VirtualOutputFile::FileDeleter::operator()(void* handle) const noexcept {
	if (handle) {
		PHYSFS_close(static_cast<PHYSFS_File*>(handle));
	}
}

} // namespace grem::application
