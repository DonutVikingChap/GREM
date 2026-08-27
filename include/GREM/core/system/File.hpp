// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_FILE_HPP
#define GREM_CORE_SYSTEM_FILE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>

#include <utility> // std::move, std::forward

namespace grem {

/**
 * Base interface for a file in a Filesystem.
 */
class File {
public:
	/**
	 * Exception type for errors that may arise when attempting to access
	 * files through the File API.
	 */
	struct Error : grem::Error {
		using grem::Error::Error;
	};

	/**
	 * File entry type.
	 */
	enum class Kind : uint8_t {
		REGULAR,   ///< Regular file.
		DIRECTORY, ///< Directory/folder.
		SYMLINK,   ///< Symbolic link.
		OTHER,     ///< Something else, such as a network socket or a device.
	};

	/**
	 * Record of metadata for a specific file.
	 */
	struct Metadata {
		size_t size;                  ///< File size, in bytes, or #npos if unavailable.
		int64_t creationTime;         ///< Time when the file was created, in seconds since the Unix epoch (1970-01-01 00:00), or -1 if unavailable.
		int64_t lastAccessTime;       ///< Last time when the file was accessed, in seconds since the Unix epoch (1970-01-01 00:00), or -1 if unavailable.
		int64_t lastModificationTime; ///< Last time when the file was modified, in seconds since the Unix epoch (1970-01-01 00:00), or -1 if unavailable.
		Kind kind;                    ///< Kind of file, such as regular file or directory.
		bool readOnly;                ///< True if the file may only be opened for reading, false if it may also be opened for writing or appending.
	};

	/**
	 * Invalid value for a file offset, used as an end-of-file marker.
	 */
	static constexpr size_t npos = static_cast<size_t>(-1);

	/** Default constructor. */
	File() noexcept = default;

	/**
	 * Virtual destructor.
	 *
	 * Automatically closes the file, ignoring errors.
	 */
	virtual ~File() = default;

	/** Copy constructor. */
	File(const File&) = default;

	/** Move constructor. */
	File(File&&) noexcept = default;

	/** Copy assignment. */
	File& operator=(const File&) = default;

	/** Move assignment. */
	File& operator=(File&&) noexcept = default;

	/**
	 * Check if the file handle has an open file associated with it.
	 *
	 * \return true if there is an associated open file, false otherwise.
	 */
	explicit operator bool() const noexcept {
		return isOpen();
	}

	/**
	 * Close the associated file so that it can no longer be accessed through
	 * this handle, and reset the handle to a closed file handle without an
	 * associated file.
	 *
	 * \throws File::Error on failure to close the file.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function has no effect if the handle has no open file
	 *       associated with it.
	 */
	virtual void close() = 0;

	/**
	 * Check if the file handle has an open file associated with it.
	 *
	 * \return true if there is an associated open file, false otherwise.
	 */
	[[nodiscard]] virtual bool isOpen() const noexcept = 0;
};

/**
 * Base interface for a readable File in a Filesystem.
 */
class InputFile : public File {
public:
	/** Default constructor. */
	InputFile() noexcept = default;

	/**
	 * Get a reader for the file.
	 *
	 * \return a reader for the file.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator Reader() & {
		return Reader{this, [](void* context, Span<byte> data) -> size_t { return static_cast<InputFile*>(context)->readUntilEOF(data); }};
	}

	/**
	 * Check if the end of the file has been reached.
	 *
	 * \return true if the end of the file has been reached, false otherwise.
	 */
	[[nodiscard]] virtual bool eof() const noexcept = 0;

	/**
	 * Get the readable length of the full file contents, in bytes.
	 *
	 * \return the length of the file, or File::npos if the length cannot be
	 *         determined.
	 */
	[[nodiscard]] virtual size_t size() const noexcept = 0;

	/**
	 * Get the current reading position of the file.
	 *
	 * \return the current file reading position, or File::npos if it cannot be
	 *         determined.
	 */
	[[nodiscard]] virtual size_t tellg() const noexcept = 0;

	/**
	 * Set the file reading position to an absolute offset from the beginning of
	 * the file.
	 *
	 * \param position the new reading position to set.
	 *
	 * \throws File::Error on failure to seek to the given position.
	 *
	 * \note Attempting to seek to a position outside the readable length of the
	 *       file will cause the function to fail.
	 */
	virtual void seekg(size_t position) = 0;

	/**
	 * Advance the file reading position by a relative offset, which may be
	 * negative in order to go backwards.
	 *
	 * \param offset the relative offset to advance the reading position by.
	 *
	 * \throws File::Error on failure to skip by the given offset.
	 *
	 * \note Attempting to seek to a position outside the readable length of the
	 *       file will cause the function to fail.
	 */
	virtual void skipg(ptrdiff_t offset) = 0;

	/**
	 * Read from an open file into an allocation of bytes.
	 *
	 * \param maxLength maximum number of bytes to read.
	 *
	 * \return an allocation containing the file contents read from the current
	 *         read position until the end of the file, or until maxLength
	 *         bytes, whichever comes first.
	 *
	 * \throws File::Error on failure to read the file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual Allocation<byte> readBytesIntoAllocation(size_t maxLength = Limits<size_t>::MAX) = 0;

	/**
	 * Read from an open file into an array list of bytes.
	 *
	 * \param maxLength maximum number of bytes to read.
	 *
	 * \return an array list of the file contents read from the current read
	 *         position until the end of the file, or until maxLength bytes,
	 *         whichever comes first.
	 *
	 * \throws File::Error on failure to read the file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual ArrayList<byte> readBytesIntoArrayList(size_t maxLength = Limits<size_t>::MAX) = 0;

	/**
	 * Read from an open file into a buffer of bytes.
	 *
	 * \param maxLength maximum number of bytes to read.
	 *
	 * \return a buffer of the file contents read from the current read position
	 *         until the end of the file, or until maxLength bytes, whichever
	 *         comes first.
	 *
	 * \throws File::Error on failure to read the file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual Buffer<byte> readBytesIntoBuffer(size_t maxLength = Limits<size_t>::MAX) = 0;

	/**
	 * Read from an open file into a string of bytes.
	 *
	 * \param maxLength maximum number of bytes to read.
	 *
	 * \return a string of the file contents read from the current read position
	 *         until the end of the file, or until maxLength bytes, whichever
	 *         comes first.
	 *
	 * \throws File::Error on failure to read the file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual String readBytesIntoString(size_t maxLength = Limits<size_t>::MAX) = 0;

	/**
	 * Read from an open file into an allocated null-terminated string of bytes.
	 *
	 * \param maxLength maximum number of bytes to read.
	 *
	 * \return an allocated null-terminated string of the file contents read
	 *         from the current read position until the end of the file, or
	 *         until maxLength bytes, whichever comes first.
	 *
	 * \throws File::Error on failure to read the file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual Allocation<char> readBytesIntoCString(size_t maxLength = Limits<size_t>::MAX) = 0;

	/**
	 * Read some amount of data from an open file, starting at the current
	 * reading position.
	 *
	 * The reading position is advanced to the end of the bytes that were read.
	 *
	 * \param data writable span over the buffer to read file data into. The
	 *        size of the span determines the number of bytes that are attempted
	 *        to be read.
	 *
	 * \return the number of bytes that were read, which may be any non-negative
	 *         integer less than or equal to the size of the given span,
	 *         including 0 if the reading position was already at the end of the
	 *         file, though always a positive value otherwise.
	 *
	 * \note The number of bytes read could be smaller than requested, even if
	 *       more bytes are available. To read as many bytes as possible, use
	 *       readUntilEOF() instead.
	 *
	 * \throws File::Error on failure to read from the file.
	 *
	 * \sa readUntilEOF()
	 * \sa read()
	 * \sa tryRead()
	 */
	[[nodiscard]] virtual size_t readSome(Span<byte> data) = 0;

	/**
	 * Read data from an open file into a buffer, starting at the current
	 * reading position.
	 *
	 * The reading position is advanced to the end of the bytes that were read.
	 *
	 * \param data writable span over the buffer to read file data into. The
	 *        size of the span determines the number of bytes that are attempted
	 *        to be read.
	 *
	 * \return the number of bytes that were successfully read, i.e.
	 *         `data.size()` or `(fileSize - previousReadingPosition)`,
	 *         whichever is smaller. This includes 0 if and only if the reading
	 *         position was already at the end of the file.
	 *
	 * \note If the end of the file is reached before the entire span's worth of
	 *       data could be read, this will be reflected in the returned number
	 *       of read bytes. To make sure all requested data was read, use read()
	 *       instead.
	 *
	 * \throws File::Error on failure to read from the file.
	 *
	 * \sa readSome()
	 * \sa read()
	 * \sa tryRead()
	 */
	[[nodiscard]] virtual size_t readUntilEOF(Span<byte> data) = 0;

	/**
	 * Read an exact amount of data from an open file, starting at the current
	 * reading position.
	 *
	 * The reading position is advanced to the end of the bytes that were read.
	 *
	 * \param data writable span to read file data into. The size of the span
	 *        determines the number of bytes that are read.
	 *
	 * \throws File::Error on failure to read from the file, or if not all
	 *         requested data was read.
	 *
	 * \sa readUntilEOF()
	 * \sa tryRead()
	 */
	void read(Span<byte> data) {
		if (!tryRead(data)) {
			throw File::Error{"Unexpected end of file."};
		}
	}

	/**
	 * Try to read an exact amount of data from an open file, starting at the
	 * current reading position.
	 *
	 * The reading position is advanced to the end of the bytes that were read.
	 *
	 * \param data writable span to read file data into. The size of the span
	 *        determines the number of bytes that are attempted to be read.
	 *
	 * \return true if all bytes were successfully read without hitting the end
	 *         of the file, false if the end of the file was reached before all
	 *         data could be read.
	 *
	 * \throws File::Error on failure to read from the file.
	 *
	 * \sa readUntilEOF()
	 * \sa read()
	 */
	[[nodiscard]] bool tryRead(Span<byte> data) {
		return readUntilEOF(data) == data.size();
	}
};

/**
 * Generic boxed unique handle to a readable file in any filesystem.
 */
class InputFileHandle {
public:
	/**
	 * Create a boxed file with a specific implementation.
	 *
	 * \tparam Implementation concrete file type.
	 *
	 * \param args arguments to forward to the concrete file constructor.
	 *
	 * \return the new boxed file.
	 *
	 * \throws any exception thrown by the concrete file's constructor.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Implementation, typename... Args>
	[[nodiscard]] GREM_ALWAYS_INLINE static InputFileHandle create(Args&&... args) {
		return InputFileHandle{UniquePointer<Implementation>::create(std::forward<Args>(args)...)};
	}

	/**
	 * Construct a null file handle without an associated file.
	 */
	InputFileHandle() noexcept = default;

	/**
	 * Get a pointer to the underlying file interface.
	 *
	 * \return a non-owning pointer to the underlying file interface.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE InputFile* get() noexcept {
		return file.get();
	}

	/**
	 * Get a pointer to the underlying file interface.
	 *
	 * \return a non-owning read-only pointer to the underlying file interface.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const InputFile* get() const noexcept {
		return file.get();
	}

	/**
	 * Get a reference to the underlying file interface.
	 *
	 * \return a reference to the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator InputFile&() {
		return *file;
	}

	/**
	 * Get a reference to the underlying file interface.
	 *
	 * \return a read-only reference to the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator const InputFile&() const {
		return *file;
	}

	/**
	 * Get a reader for the underlying file interface.
	 *
	 * \return a reader for the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator Reader() & {
		return *file;
	}

	/** \copydoc File::operator bool() */
	GREM_ALWAYS_INLINE explicit operator bool() const noexcept {
		return isOpen();
	}

	/** \copydoc File::close() */
	GREM_ALWAYS_INLINE void close() {
		if (file) {
			file->close();
			file = {};
		}
	}

	/** \copydoc File::isOpen() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isOpen() const noexcept {
		return file && file->isOpen();
	}

	/** \copydoc InputFile::eof() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool eof() const noexcept {
		return !file || file->eof();
	}

	/** \copydoc InputFile::size() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t size() const noexcept {
		return (file) ? file->size() : 0;
	}

	/** \copydoc InputFile::tellg() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t tellg() const noexcept {
		return (file) ? file->tellg() : 0;
	}

	/** \copydoc InputFile::seekg() */
	GREM_ALWAYS_INLINE void seekg(size_t position) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->seekg(position);
	}

	/** \copydoc InputFile::skipg() */
	GREM_ALWAYS_INLINE void skipg(ptrdiff_t offset) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->skipg(offset);
	}

	/** \copydoc InputFile::readBytesIntoAllocation() */
	[[nodiscard]] GREM_ALWAYS_INLINE Allocation<byte> readBytesIntoAllocation(size_t maxLength = Limits<size_t>::MAX) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readBytesIntoAllocation(maxLength);
	}

	/** \copydoc InputFile::readBytesIntoArrayList() */
	[[nodiscard]] GREM_ALWAYS_INLINE ArrayList<byte> readBytesIntoArrayList(size_t maxLength = Limits<size_t>::MAX) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readBytesIntoArrayList(maxLength);
	}

	/** \copydoc InputFile::readBytesIntoBuffer() */
	[[nodiscard]] GREM_ALWAYS_INLINE Buffer<byte> readBytesIntoBuffer(size_t maxLength = Limits<size_t>::MAX) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readBytesIntoBuffer(maxLength);
	}

	/** \copydoc InputFile::readBytesIntoString() */
	[[nodiscard]] GREM_ALWAYS_INLINE String readBytesIntoString(size_t maxLength = Limits<size_t>::MAX) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readBytesIntoString(maxLength);
	}

	/** \copydoc InputFile::readBytesIntoCString() */
	[[nodiscard]] GREM_ALWAYS_INLINE Allocation<char> readBytesIntoCString(size_t maxLength = Limits<size_t>::MAX) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readBytesIntoCString(maxLength);
	}

	/** \copydoc InputFile::readSome() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t readSome(Span<byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readSome(data);
	}

	/** \copydoc InputFile::readUntilEOF() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t readUntilEOF(Span<byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->readUntilEOF(data);
	}

	/** \copydoc InputFile::read() */
	GREM_ALWAYS_INLINE void read(Span<byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->read(data);
	}

	/** \copydoc InputFile::tryRead() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool tryRead(Span<byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->tryRead(data);
	}

private:
	explicit InputFileHandle(UniquePointer<InputFile> file) noexcept
		: file(std::move(file)) {}

	UniquePointer<InputFile> file{};
};

/**
 * Base interface for a writable file in a Filesystem.
 */
class OutputFile : public File {
public:
	/** Default constructor. */
	OutputFile() noexcept = default;

	/**
	 * Get a writer for the file.
	 *
	 * \return a writer for the file.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator Writer() & {
		return Writer{this, [](void* context, Span<const byte> data, bool thenFlush) -> size_t {
						  OutputFile& output = *static_cast<OutputFile*>(context);
						  const size_t bytesWritten = output.writeSome(data);
						  if (thenFlush) {
							  output.flush();
						  }
						  return bytesWritten;
					  }};
	}

	/**
	 * Get the current writing position of the file.
	 *
	 * \return the current file writing position, or File::npos if it cannot be
	 *         determined.
	 */
	[[nodiscard]] virtual size_t tellp() const noexcept = 0;

	/**
	 * Set the file writing position to an absolute offset from the beginning of
	 * the file.
	 *
	 * \param position the new writing position to set.
	 *
	 * \throws File::Error on failure to seek to the given position.
	 *
	 * \note Attempting to seek to a position outside the readable length of the
	 *       file will cause the function to fail.
	 */
	virtual void seekp(size_t position) = 0;

	/**
	 * Advance the file writing position by a relative offset, which may be
	 * negative in order to go backwards.
	 *
	 * \param offset the relative offset to advance the writing position by.
	 *
	 * \throws File::Error on failure to skip by the given offset.
	 *
	 * \note Attempting to seek to a position outside the readable length of the
	 *       file will cause the function to fail.
	 */
	virtual void skipp(ptrdiff_t offset) = 0;

	/**
	 * Write data to the end of an open file from a buffer.
	 *
	 * The writing position is advanced to the new end of the file.
	 *
	 * \param data read-only view over the buffer to copy the data from. The
	 *        size of the buffer determines the number of bytes that are
	 *        attempted to be written.
	 *
	 * \return the number of bytes that were successfully written, which may be
	 *         any non-negative integer less than or equal to the size of the
	 *         given view, including 0.
	 *
	 * \throws File::Error on failure to write to the file.
	 */
	[[nodiscard]] virtual size_t writeSome(Span<const byte> data) = 0;

	/**
	 * Write data to the end of an open file.
	 *
	 * The writing position is advanced to the new end of the file.
	 *
	 * \param data data to write.
	 *
	 * \throws File::Error on failure to write to the file, or if not all of the
	 *         data was written.
	 */
	void write(Span<const byte> data) {
		while (!data.empty()) {
			data = data.subspan(writeSome(data));
		}
	}

	/**
	 * Write data from a string to the end of an open file.
	 *
	 * The writing position is advanced to the new end of the file.
	 *
	 * \param string string containing the data to write.
	 *
	 * \throws File::Error on failure to write to the file, or if not all of the
	 *         data was written.
	 */
	void write(StringView string) {
		write(asBytes(Span{string}));
	}

	/**
	 * Synchronize with the underlying file to make sure that all buffered data
	 * that has been written so far is flushed into the actual file.
	 *
	 * \throws File::Error on failure to synchronize with the file.
	 */
	virtual void flush() = 0;
};

/**
 * Generic boxed unique handle to a writable file in any filesystem.
 */
class OutputFileHandle {
public:
	/**
	 * Create a boxed file with a specific implementation.
	 *
	 * \tparam Implementation concrete file type.
	 *
	 * \param args arguments to forward to the concrete file constructor.
	 *
	 * \return the new boxed file.
	 *
	 * \throws any exception thrown by the concrete file's constructor.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Implementation, typename... Args>
	[[nodiscard]] GREM_ALWAYS_INLINE static OutputFileHandle create(Args&&... args) {
		return OutputFileHandle{UniquePointer<Implementation>::create(std::forward<Args>(args)...)};
	}

	/**
	 * Construct a null file handle without an associated file.
	 */
	OutputFileHandle() noexcept = default;

	/**
	 * Get a writer for the underlying file interface.
	 *
	 * \return a writer for the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator Writer() & {
		return *file;
	}

	/**
	 * Get a pointer to the underlying file interface.
	 *
	 * \return a non-owning pointer to the underlying file interface.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE OutputFile* get() noexcept {
		return file.get();
	}

	/**
	 * Get a pointer to the underlying file interface.
	 *
	 * \return a non-owning read-only pointer to the underlying file interface.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const OutputFile* get() const noexcept {
		return file.get();
	}

	/**
	 * Get a reference to the underlying file interface.
	 *
	 * \return a reference to the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator OutputFile&() {
		return *file;
	}

	/**
	 * Get a reference to the underlying file interface.
	 *
	 * \return a read-only reference to the underlying file interface.
	 *
	 * \warning The file handle must not be null.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE operator const OutputFile&() const {
		return *file;
	}

	/** \copydoc File::operator bool() */
	GREM_ALWAYS_INLINE explicit operator bool() const noexcept {
		return isOpen();
	}

	/** \copydoc File::close() */
	GREM_ALWAYS_INLINE void close() {
		if (file) {
			file->close();
			file = {};
		}
	}

	/** \copydoc File::isOpen() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isOpen() const noexcept {
		return file && file->isOpen();
	}

	/** \copydoc OutputFile::tellp() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t tellp() const noexcept {
		return (file) ? file->tellp() : 0;
	}

	/** \copydoc OutputFile::seekp() */
	GREM_ALWAYS_INLINE void seekp(size_t position) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->seekp(position);
	}

	/** \copydoc OutputFile::skipp() */
	GREM_ALWAYS_INLINE void skipp(ptrdiff_t offset) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->skipp(offset);
	}

	/** \copydoc OutputFile::writeSome() */
	[[nodiscard]] GREM_ALWAYS_INLINE size_t writeSome(Span<const byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		return file->writeSome(data);
	}

	/** \copydoc OutputFile::write() */
	GREM_ALWAYS_INLINE void write(Span<const byte> data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->write(data);
	}

	/** \copydoc OutputFile::write() */
	GREM_ALWAYS_INLINE void write(StringView data) {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->write(data);
	}

	/** \copydoc OutputFile::flush() */
	GREM_ALWAYS_INLINE void flush() {
		if (!file) {
			throw File::Error{"Invalid file handle."};
		}
		file->flush();
	}

private:
	explicit OutputFileHandle(UniquePointer<OutputFile> file) noexcept
		: file(std::move(file)) {}

	UniquePointer<OutputFile> file{};
};

} // namespace grem

#endif
