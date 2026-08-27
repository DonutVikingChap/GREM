// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_FILESYSTEM_HPP
#define GREM_CORE_SYSTEM_FILESYSTEM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/system/File.hpp>

#include <type_traits> // std::is_void_v

namespace grem {

/**
 * Base interface for a filesystem.
 */
class Filesystem {
public:
	/** Default constructor. */
	constexpr Filesystem() noexcept = default;

	/** Virtual destructor. */
	virtual ~Filesystem() = default;

	/** Copy constructor. */
	constexpr Filesystem(const Filesystem&) = default;

	/** Move constructor. */
	constexpr Filesystem(Filesystem&&) noexcept = default;

	/** Copy assignment. */
	Filesystem& operator=(const Filesystem&) = default;

	/** Move assignment. */
	Filesystem& operator=(Filesystem&&) noexcept = default;

	/**
	 * Get the current output directory of the filesystem.
	 *
	 * \return the host filepath corresponding to the current output directory,
	 *         or an empty string if no output directory is currently set.
	 */
	[[nodiscard]] virtual CStringView getOutputDirectory() const noexcept = 0;

	/**
	 * Get the host filepath of the mounted input archive on the host filesystem
	 * that contains a given virtual file.
	 *
	 * \param filepath virtual filepath of the file to get the archive of.
	 *
	 * \return the host filepath of the directory or archive that was previously
	 *         passed to mountInputArchive(), in which the corresponding virtual
	 *         file was found, or an empty string if the virtual file was not
	 *         found at any active mount point.
	 */
	[[nodiscard]] virtual CStringView findInputArchiveOfFile(CStringView filepath) const noexcept = 0;

	/**
	 * Create a new host directory relative to the current output directory,
	 * while also creating any missing parent directories along the way as
	 * needed.
	 *
	 * \param filepath virtual filepath, relative to the current output
	 *        directory, of the new directory to be created.
	 *
	 * \throws File::Error on failure to create the directories.
	 * \throws std::bad_alloc on allocation failure.
	 */
	virtual void createOutputDirectory(CStringView filepath) = 0;

	/**
	 * Create any missing parent directories above a host directory relative to
	 * the current output directory.
	 *
	 * \param filepath virtual filepath, relative to the current output
	 *        directory, of the directory whose missing parent directories to
	 *        create.
	 *
	 * \throws File::Error on failure to create the directories.
	 * \throws std::bad_alloc on allocation failure.
	 */
	virtual void createParentOutputDirectories(CStringView filepath) = 0;

	/**
	 * Delete a host file or directory relative to the current output directory.
	 *
	 * \param filepath virtual filepath, relative to the current output
	 *        directory, of the file or directory to delete.
	 *
	 * \throws File::Error on failure to delete the given file or directory.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning If successful, this will delete the actual file that
	 *          corresponds to the given virtual filepath on the host
	 *          filesystem; not just the virtual file entry.
	 * \warning Although deleting a file will prevent it from being read again
	 *          through conventional means, the physical data that was contained
	 *          in the file may or may not remain untouched on disk, meaning
	 *          that this function cannot be relied upon to securly erase
	 *          sensitive data.
	 *
	 * \note Directories must be empty before they can be successfully deleted
	 *       using this function.
	 */
	virtual void deleteOutputFile(CStringView filepath) = 0;

	/**
	 * Check if a given virtual filepath has a corresponding host file mounted.
	 *
	 * \param filepath virtual filepath to check for a mounted file.
	 *
	 * \return true if a file is mounted at the given filepath, false otherwise.
	 *
	 * \sa getInputFileMetadata()
	 * \sa forEachInputFilenameInDirectory()
	 */
	[[nodiscard]] virtual bool inputFileExists(CStringView filepath) const = 0;

	/**
	 * Get the metadata of an input file that is mounted at a given virtual
	 * filepath.
	 *
	 * \param filepath virtual filepath of the file to get the metadata of.
	 *
	 * \return the file metadata, see File::Metadata.
	 *
	 * \throws File::Error on failure to get the metadata, such as if no file is
	 *         mounted at the given filepath.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual File::Metadata getInputFileMetadata(CStringView filepath) const = 0;

	/**
	 * Iterate the filenames of all readable virtual filepaths that are direct
	 * children of a given directory.
	 *
	 * \param filepath virtual filepath of the input directory to enumerate.
	 * \param callback function to execute for each file, which should accept
	 *        the current filename as a parameter and return a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 *
	 * \throws File::Error on failure to enumerate the directory.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback function.
	 *
	 * \note This function is not recursive, and only yields the filename
	 *       component of the direct descendants of the given directory, without
	 *       the leading directory path. The full virtual filepath of each
	 *       filepath can be constructed using
	 *       `formatString("{}/{}", filepath, filename)`, where `filepath` is
	 *       the directory filepath that was passed to the function, and
	 *       `filename` is the current filename. Note that this path may refer
	 *       to any kind of file, including a subdirectory. Use
	 *       getInputFileMetadata() to find out which kind of file it refers to.
	 *
	 * \sa inputFileExists()
	 * \sa getInputFileMetadata()
	 */
	virtual void forEachInputFilenameInDirectory(CStringView filepath, FunctionView<bool(CStringView filename)> callback) const = 0; // NOLINT(modernize-use-nodiscard)

	/**
	 * Iterate the filenames of all readable virtual filepaths that are direct
	 * children of a given directory.
	 *
	 * \param filepath virtual filepath of the input directory to enumerate.
	 * \param callback function to execute for each file, which should accept
	 *        the current filename as a parameter.
	 *
	 * \throws File::Error on failure to enumerate the directory.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback function.
	 *
	 * \note This function is not recursive, and only yields the filename
	 *       component of the direct descendants of the given directory, without
	 *       the leading directory path. The full virtual filepath of each
	 *       filepath can be constructed using
	 *       `formatString("{}/{}", filepath, filename)`, where `filepath` is
	 *       the directory filepath that was passed to the function, and
	 *       `filename` is the current filename. Note that this path may refer
	 *       to any kind of file, including a subdirectory. Use
	 *       getInputFileMetadata() to find out which kind of file it refers to.
	 *
	 * \sa inputFileExists()
	 * \sa getInputFileMetadata()
	 */
	void forEachInputFilenameInDirectory(CStringView filepath, auto callback) const requires(std::is_void_v<decltype(callback(CStringView{}))>) {
		forEachInputFilenameInDirectory(filepath, [&](CStringView filename) -> bool {
			callback(filename);
			return false;
		});
	}

	/**
	 * Open an input file in the virtual filesystem for reading.
	 *
	 * \param filepath virtual filepath of the mounted input file to open.
	 *
	 * \return a new file handle with an input stream set up to read the opened
	 *         file starting at file position 0.
	 *
	 * \throws File::Error on failure to open the file for reading.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa tryOpenInputFile()
	 * \sa openEmptyOutputFile()
	 * \sa openOutputFileForAppending()
	 */
	[[nodiscard]] virtual InputFileHandle openInputFile(CStringView filepath) const = 0;

	/**
	 * Try to open an input file in the virtual filesystem for reading.
	 *
	 * \param filepath virtual filepath of the mounted input file to open.
	 *
	 * \return a new file handle with an input stream set up to read the opened
	 *         file starting at file position 0, or a closed file handle if the
	 *         file could not be opened.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa openEmptyOutputFile()
	 * \sa openOutputFileForAppending()
	 */
	[[nodiscard]] virtual InputFileHandle tryOpenInputFile(CStringView filepath) const = 0;

	/**
	 * Open an input file in the virtual filesystem for reading and read its
	 * full contents into an array of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an array containing a copy of the full contents of the file.
	 *
	 * \throws File::Error on failure to open the file for reading or read the
	 *         file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa tryReadInputFile()
	 */
	[[nodiscard]] Allocation<byte> readInputFile(CStringView filepath) const {
		return openInputFile(filepath).readBytesIntoAllocation();
	}

	/**
	 * Try to open an input file in the virtual filesystem for reading and read
	 * its full contents into an array of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an array containing a copy of the full contents of the file, or
	 *         an empty optional if the file could not be opened.
	 *
	 * \throws File::Error on failure to read the opened file's contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa readInputFile()
	 */
	[[nodiscard]] Optional<Allocation<byte>> tryReadInputFile(CStringView filepath) const {
		if (InputFileHandle file = tryOpenInputFile(filepath)) {
			return file.readBytesIntoAllocation();
		}
		return {};
	}

	/**
	 * Open an input file in the virtual filesystem for reading and read its
	 * full contents into an array list of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an array list containing a copy of the full contents of the file.
	 *
	 * \throws File::Error on failure to open the file for reading or read the
	 *         file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa tryReadInputFileArrayList()
	 */
	[[nodiscard]] ArrayList<byte> readInputFileArrayList(CStringView filepath) const {
		return openInputFile(filepath).readBytesIntoArrayList();
	}

	/**
	 * Try to open an input file in the virtual filesystem for reading and read
	 * its full contents into an array list of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an array list containing a copy of the full contents of the file,
	 *         or an empty optional if the file could not be opened.
	 *
	 * \throws File::Error on failure to read the opened file's contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa readInputFileArrayList()
	 */
	[[nodiscard]] Optional<ArrayList<byte>> tryReadInputFileArrayList(CStringView filepath) const {
		if (InputFileHandle file = tryOpenInputFile(filepath)) {
			return file.readBytesIntoArrayList();
		}
		return {};
	}

	/**
	 * Open an input file in the virtual filesystem for reading and read its
	 * full contents into a buffer of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return a buffer containing a copy of the full contents of the file.
	 *
	 * \throws File::Error on failure to open the file for reading or read the
	 *         file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa tryReadInputFileBuffer()
	 */
	[[nodiscard]] Buffer<byte> readInputFileBuffer(CStringView filepath) const {
		return openInputFile(filepath).readBytesIntoBuffer();
	}

	/**
	 * Try to open an input file in the virtual filesystem for reading and read
	 * its full contents into a buffer of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return a buffer containing a copy of the full contents of the file, or
	 *         an empty optional if the file could not be opened.
	 *
	 * \throws File::Error on failure to read the opened file's contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa readInputFileBuffer()
	 */
	[[nodiscard]] Optional<Buffer<byte>> tryReadInputFileBuffer(CStringView filepath) const {
		if (InputFileHandle file = tryOpenInputFile(filepath)) {
			return file.readBytesIntoBuffer();
		}
		return {};
	}

	/**
	 * Open an input file in the virtual filesystem for reading and read its
	 * full contents into a string of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return a string containing a copy of the full contents of the file.
	 *
	 * \throws File::Error on failure to open the file for reading or read the
	 *         file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa tryReadInputFileString()
	 */
	[[nodiscard]] String readInputFileString(CStringView filepath) const {
		return openInputFile(filepath).readBytesIntoString();
	}

	/**
	 * Try to open an input file in the virtual filesystem for reading and read
	 * its full contents into a string of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return a string containing a copy of the full contents of the file, or
	 *         an empty optional if the file could not be opened.
	 *
	 * \throws File::Error on failure to read the opened file's contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa readInputFileString()
	 */
	[[nodiscard]] Optional<String> tryReadInputFileString(CStringView filepath) const {
		if (InputFileHandle file = tryOpenInputFile(filepath)) {
			return file.readBytesIntoString();
		}
		return {};
	}

	/**
	 * Open an input file in the virtual filesystem for reading and read its
	 * full contents into an allocated null-terminated string of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an allocated null-terminated string containing a copy of the full
	 *         contents of the file.
	 *
	 * \throws File::Error on failure to open the file for reading or read the
	 *         file contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa openInputFile()
	 * \sa tryReadInputFileCString()
	 */
	[[nodiscard]] Allocation<char> readInputFileCString(CStringView filepath) const {
		return openInputFile(filepath).readBytesIntoCString();
	}

	/**
	 * Try to open an input file in the virtual filesystem for reading and read
	 * its full contents into an allocated null-terminated string of bytes.
	 *
	 * \param filepath virtual filepath of the mounted input file to read.
	 *
	 * \return an allocated null-terminated string containing a copy of the full
	 *         contents of the file, or nullptr if the file could not be opened.
	 *
	 * \throws File::Error on failure to read the opened file's contents.
	 * \throws std::length_error if the maximum file size was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa readInputFileCString()
	 */
	[[nodiscard]] Allocation<char> tryReadInputFileCString(CStringView filepath) const {
		if (InputFileHandle file = tryOpenInputFile(filepath)) {
			return file.readBytesIntoCString();
		}
		return {};
	}

	/**
	 * Create a file relative to the current output directory and open it for
	 * writing, overwriting any existing file at the same filepath.
	 *
	 * \param filepath virtual filepath of the new file to be created, relative
	 *        to the current output directory.
	 *
	 * \return a new file handle with an output stream set up to write to the
	 *         new empty file that was opened.
	 *
	 * \throws File::Error on failure to delete the existing file, create the
	 *         new file, or open the new file for writing.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning If successful, any existing host file that corresponds to the
	 *          given virtual filepath in the output directory on the host
	 *          filesystem will be deleted on the host filesystem; not just its
	 *          virtual file entry. All modifications made through the virtual
	 *          file handle will also be reflected in the physical file on the
	 *          host file system.
	 *
	 * \sa openInputFile()
	 * \sa createEmptyOutputFile()
	 * \sa openOutputFileForAppending()
	 */
	[[nodiscard]] virtual OutputFileHandle openEmptyOutputFile(CStringView filepath) = 0;

	/**
	 * Create an empty file relative to the current output directory,
	 * overwriting any existing file at the same filepath.
	 *
	 * \param filepath virtual filepath of the new file to be created, relative
	 *        to the current output directory.
	 *
	 * \throws File::Error on failure to delete the existing file or create the
	 *         new file.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning If successful, any existing host file that corresponds to the
	 *          given virtual filepath in the output directory on the host
	 *          filesystem will be deleted on the host filesystem; not just its
	 *          virtual file entry. All modifications made through the virtual
	 *          file handle will also be reflected in the physical file on the
	 *          host file system.
	 *
	 * \sa openEmptyOutputFile()
	 */
	virtual void createEmptyOutputFile(CStringView filepath) = 0;

	/**
	 * Open a file relative to the current output directory for appended
	 * writing, or create an empty file and open it if one didn't already exist.
	 *
	 * \param filepath virtual filepath of the file to be opened, relative to
	 *        the current output directory.
	 *
	 * \return a new file handle with an output stream set up to write to the
	 *         end of the opened file.
	 *
	 * \throws File::Error on failure to open the file for writing.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning If successful, the existing host file that corresponds to the
	 *          given virtual filepath in the output directory on the host
	 *          filesystem will receive all modifications made through the
	 *          virtual file handle.
	 *
	 * \note A new, empty file will be created if the specified file doesn't
	 *       already exist.
	 *
	 * \sa openInputFile()
	 * \sa openEmptyOutputFile()
	 */
	[[nodiscard]] virtual OutputFileHandle openOutputFileForAppending(CStringView filepath) = 0;
};

} // namespace grem

#endif
