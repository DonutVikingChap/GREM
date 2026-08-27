// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_APPLICATION_VIRTUAL_FILESYSTEM_HPP
#define GREM_APPLICATION_VIRTUAL_FILESYSTEM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/system/synchronization.hpp>

#include <functional>   // std::hash
#include <system_error> // std::error_code
#include <utility>      // std::move
#endif

namespace grem::application {

class VirtualInputFile;  // Forward declaration, to avoid a circular include of VirtualInputFile.hpp.
class VirtualOutputFile; // Forward declaration, to avoid a circular include of VirtualOutputFile.hpp.

/**
 * Mount priority for a newly mounted archive to a VirtualFilesystem, relative
 * to all previously mounted archives.
 */
enum class MountPriority : uint8_t {
	/**
	 * Mount the archive at a lower priority than any previously mounted
	 * archive, meaning files in already mounted archives will be preferred
	 * when choosing which host file to map a virtual filepath to.
	 */
	LOWER,

	/**
	 * Mount the archive at a higher priority than any previously mounted
	 * archive, meaning files in the new archive will be preferred when
	 * choosing which host file to map a virtual filepath to.
	 */
	HIGHER,
};

/**
 * Persistent system for managing the virtual filesystem.
 *
 * This system supports multiple host filesystem directories being mounted to
 * the same virtual mount point for reading, such that virtual filepaths are
 * mapped to the corresponding host file with the highest mount priority. When
 * creating and writing files, all output is directed to an optional output
 * directory.
 *
 * By default, no input archives are mounted, and no output directory is set,
 * meaning that no files can be opened or created. To be able to read and write
 * files, the recommended initialization procedure for a typical application is
 * as follows:
 * ```cpp
 * // Construct the virtual filesystem.
 * VirtualFilesystem filesystem{argv[0]};
 *
 * // Set the output directory to a standardized location for the current platform.
 * // See VirtualFilesystem::StandardOutputDirectoryOptions for a description of the expected parameters.
 * filesystem.setOutputDirectory(filesystem.createStandardOutputDirectory({
 *     .organizationName = "MyOrg",
 *     .applicationName = "MyApp",
 * }));
 *
 * // Mount the current working directory for reading.
 * // This may be replaced and/or extended with a specific archive in the working directory, such as "data" or "assets.zip".
 * filesystem.mountInputArchive(".");
 * 
 * // Mount any ".zip" archives found under "custom/" in the main archive(s).
 * // This step is optional, but makes the application easy to mod with custom files thanks to the higher mount priority.
 * // If using this, make sure that at least one of the main archives that were mounted above is a regular uncompressed directory,
 * // since it would be very inconvenient for the user to have to add their custom archives to a compressed file.
 * filesystem.mountInputArchivesInMountedDirectory("custom", "zip");
 *
 * // Mount the output directory for reading.
 * // This allows saved files to be re-read again, with a higher priority than all other archives.
 * filesystem.mountInputArchive(filesystem.getOutputDirectory());
 * ```
 */
class VirtualFilesystem : public Filesystem {
public:
	struct StandardOutputDirectoryOptions {
		/**
		 * String that commonly identifies the publisher of the application,
		 * such as an organization name, alias or internet domain. This will be
		 * used to decide the name of the organization folder in the
		 * user/platform-specific preferences directory on applicable platforms.
		 *
		 * \note This option is required.
		 */
		CStringView organizationName;

		/**
		 * String that uniquely identifies the application among all other
		 * applications released by the same organization. This will be used
		 * to decide the name of the application folder (potentially under the
		 * organization folder) in the user/platform-specific preferences
		 * directory on applicable platforms.
		 *
		 * \note This option is required.
		 */
		CStringView applicationName;
	};

	/**
	 * Initialize the virtual filesystem.
	 *
	 * \param programFilepath the first string in the argument vector passed to
	 *        the main function of the program, i.e. argv[0].
	 *
	 * \throws File::Error if initialization failed.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The behavior of passing programFilepath a value other than the
	 *          argv[0] string received from main is undefined.
	 * \warning There can only be one active virtual filesystem in a program at
	 *          a time.
	 *
	 * \sa setOutputDirectory()
	 * \sa mountInputArchivesInMountedDirectory()
	 * \sa mountInputArchive()
	 */
	GREM_API(application) explicit VirtualFilesystem(CStringView programFilepath);

	/** Destructor. */
	GREM_API(application) ~VirtualFilesystem() override;

	/** Copying a filesystem is not allowed, since it manages global state. */
	VirtualFilesystem(const VirtualFilesystem&) = delete;

	/** Moving a filesystem is not allowed, since it manages global state. */
	VirtualFilesystem(VirtualFilesystem&&) = delete;

	/** Copying a filesystem is not allowed, since it manages global state. */
	VirtualFilesystem& operator=(const VirtualFilesystem&) = delete;

	/** Moving a filesystem is not allowed, since it manages global state. */
	VirtualFilesystem& operator=(VirtualFilesystem&&) = delete;

	/**
	 * Get a suitable output directory for configuration files and other save
	 * data on the host platform, which is usually located somwhere within the
	 * user's home directory in a sub-folder tailored to this specific
	 * application.
	 *
	 * \param standardOutputDirectoryOptions creation options, see
	 *        StandardOutputDirectoryOptions.
	 *
	 * \return a directory that can be passed to setOutputDirectory(), or an
	 *         empty string if a suitable output directory could not be
	 *         determined.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa setOutputDirectory()
	 * \sa getOutputDirectory()
	 */
	[[nodiscard]] GREM_API(application) String createStandardOutputDirectory(const StandardOutputDirectoryOptions& standardOutputDirectoryOptions);

	/**
	 * Get the current output directory of the virtual filesystem.
	 *
	 * \return the host filepath corresponding to the current output directory,
	 *         or an empty string if no output directory is currently set.
	 *
	 * \sa createStandardOutputDirectory()
	 * \sa setOutputDirectory()
	 */
	[[nodiscard]] GREM_API(application) CStringView getOutputDirectory() const noexcept override;

	/**
	 * Set the output directory of the virtual filesystem, where all output
	 * files will be written.
	 *
	 * The specified directory will be created if it doesn't already exist.
	 *
	 * \param path host filepath of the new output directory to set, or an empty
	 *        string to revert to having no output directory.
	 *
	 * \throws File::Error on failure to set the output directory to the given
	 *         path.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To be able to read files from the output directory, it must also be
	 *       mounted using mountInputArchive().
	 *
	 * \sa createStandardOutputDirectory()
	 * \sa getOutputDirectory()
	 */
	GREM_API(application) void setOutputDirectory(CStringView path);

	/**
	 * Mount a directory or archive on the host filesystem to the root input
	 * directory of the virtual filesystem.
	 *
	 * The supported archive types, besides regular directories, are:
	 * - ZIP archive (.zip)
	 * - 7-Zip archive (.7z)
	 * - ISO 9660 CD-ROM image (.iso)
	 * - Quake I/II archive (.pak)
	 * - DOOM engine archive (.wad)
	 *
	 * \param path host filepath of the directory or archive to mount.
	 * \param priority mount priority of the archive to be mounted, see
	 *        FilesystemMountPriority.
	 *
	 * \throws File::Error on failure to mount the given filepath.
	 *
	 * \note If the given path is empty, this function has no effect.
	 * \note If the given path is already mounted, no change will occur.
	 *
	 * \sa mountInputArchivesInMountedDirectory()
	 * \sa unmountInputArchive()
	 */
	GREM_API(application) void mountInputArchive(CStringView path, MountPriority priority = MountPriority::HIGHER);

	/**
	 * Unmount a previously mounted input directory or archive on the host
	 * filesystem from the virtual filesystem.
	 *
	 * \param path host filepath of the directory or archive to unmount, which
	 *        was previously mounted by mountInputArchive() or
	 *        mountInputArchivesInMountedDirectory().
	 *
	 * \throws File::Error on failure to unmount the given filepath.
	 *
	 * \note If the given path is empty, this function has no effect.
	 *
	 * \sa mountInputArchive()
	 */
	GREM_API(application) void unmountInputArchive(CStringView path);

	/**
	 * Mount all archives in a given directory on the host filesystem to the
	 * root input directory of the virtual filesystem. This is useful for
	 * allowing users to easily create and share modifications or plugins that
	 * add or override application resources by simply adding the mod archive to
	 * the given directory.
	 *
	 * The newly mounted archives will have a higher priority than any
	 * previously mounted archives when choosing which host file to map a
	 * virtual filepath to, meaning more recently mounted files are preferred.
	 *
	 * \param filepath virtual filepath of the mounted directory to search for
	 *        archives in.
	 * \param archiveFileExtension non-owning pointer to a null-terminated UTF-8
	 *        string of the filename extension of the archive files to mount. If
	 *        empty, all found archives will be mounted regardless of extension.
	 * \param priority mount priority of the archives to be mounted, see
	 *        FilesystemMountPriority.
	 * \param callback callback to execute for each host filepath of the
	 *        archives that were mounted, in no specific order.
	 *
	 * \throws File::Error on failure to search for archives or mount an
	 *         archive.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The found archives are mounted in no particular order.
	 *
	 * \sa mountInputArchive()
	 * \sa unmountInputArchive()
	 */
	GREM_API(application)
	void mountInputArchivesInMountedDirectory(
		CStringView filepath, CStringView archiveFileExtension, MountPriority priority = MountPriority::HIGHER,
		FunctionView<void(CStringView path)> callback = [](CStringView) -> void {});

	[[nodiscard]] GREM_API(application) CStringView findInputArchiveOfFile(CStringView filepath) const noexcept override;

	GREM_API(application) void createOutputDirectory(CStringView filepath) override;
	GREM_API(application) void createParentOutputDirectories(CStringView filepath) override;
	GREM_API(application) void deleteOutputFile(CStringView filepath) override;

	[[nodiscard]] GREM_API(application) bool inputFileExists(CStringView filepath) const override;
	[[nodiscard]] GREM_API(application) File::Metadata getInputFileMetadata(CStringView filepath) const override;

	GREM_API(application)
	void forEachInputFilenameInDirectory(CStringView filepath, FunctionView<bool(CStringView filename)> callback) const override; // NOLINT(modernize-use-nodiscard)

	[[nodiscard]] GREM_API(application) InputFileHandle openInputFile(CStringView filepath) const override;
	[[nodiscard]] GREM_API(application) InputFileHandle tryOpenInputFile(CStringView filepath) const override;
	[[nodiscard]] GREM_API(application) OutputFileHandle openEmptyOutputFile(CStringView filepath) override;
	GREM_API(application) void createEmptyOutputFile(CStringView filepath) override;
	[[nodiscard]] GREM_API(application) OutputFileHandle openOutputFileForAppending(CStringView filepath) override;

private:
	friend VirtualInputFile;
	friend VirtualOutputFile;

	String outputDirectory{};
#ifdef GREM_USE_MULTITHREADING
	struct SharedFileMutex {
		struct Value {
			explicit Value(String canonicalFilepath)
				: canonicalFilepath(std::move(canonicalFilepath)) {}

			String canonicalFilepath;
			RecursiveMutex mutex{};
		};

		struct Hash {
			using is_transparent = void;

			[[nodiscard]] size_t operator()(const SharedFileMutex& a) const {
				return stringHasher(a.value->canonicalFilepath);
			}

			[[nodiscard]] size_t operator()(const String& a) const {
				return stringHasher(a);
			}

		private:
			[[no_unique_address]] std::hash<String> stringHasher;
		};

		struct Equal {
			using is_transparent = void;

			[[nodiscard]] bool operator()(const SharedFileMutex& a, const SharedFileMutex& b) const {
				return a.value->canonicalFilepath == b.value->canonicalFilepath;
			}

			[[nodiscard]] bool operator()(const SharedFileMutex& a, const String& b) const {
				return a.value->canonicalFilepath == b;
			}

			[[nodiscard]] bool operator()(const String& a, const SharedFileMutex& b) const {
				return a == b.value->canonicalFilepath;
			}

			[[nodiscard]] bool operator()(const String& a, const String& b) const {
				return a == b;
			}
		};

		SharedPointer<Value> value{};

		SharedFileMutex() noexcept = default;

		explicit SharedFileMutex(String canonicalFilepath)
			: value(SharedPointer<Value>::create(std::move(canonicalFilepath))) {}
	};

	mutable HashSet<SharedFileMutex, SharedFileMutex::Hash, SharedFileMutex::Equal> fileMutexes{};
	mutable Mutex fileMutexesSetMutex{};

	struct FileLock {
		const VirtualFilesystem* filesystem = nullptr;
		SharedFileMutex fileMutex{};
		UniqueLock<RecursiveMutex> lock{};

		FileLock() noexcept = default;

		FileLock(const VirtualFilesystem& filesystem, SharedFileMutex fileMutex)
			: filesystem(&filesystem)
			, fileMutex(std::move(fileMutex))
			, lock(this->fileMutex.value->mutex) {}

		GREM_API(application) ~FileLock();

		FileLock(const FileLock&) = delete;
		FileLock(FileLock&&) noexcept = default;
		FileLock& operator=(const FileLock&) = delete;
		FileLock& operator=(FileLock&&) noexcept = default;
	};

	[[nodiscard]] GREM_API(application) FileLock lockInputFile(CStringView filepath, std::error_code& errorCode) const;
	[[nodiscard]] GREM_API(application) FileLock lockOutputFile(CStringView filepath, std::error_code& errorCode) const;
#endif
};

} // namespace grem::application

#endif
