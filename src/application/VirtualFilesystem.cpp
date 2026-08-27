// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/application/VirtualFilesystem.hpp>
#include <GREM/application/VirtualInputFile.hpp>
#include <GREM/application/VirtualOutputFile.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/synchronization.hpp>

#include <filesystem>   // std::filesystem::...
#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)
#endif

#include <exception> // std::exception, std::current_exception, std::rethrow_exception
#include <physfs.h>  // PHYSFS_...
#include <utility>   // std::move

namespace grem::application {

VirtualFilesystem::VirtualFilesystem(CStringView programFilepath) {
	if (PHYSFS_init(programFilepath.c_str()) == 0) {
		throw File::Error{String{"Failed to initialize filesystem:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

VirtualFilesystem::~VirtualFilesystem() {
	PHYSFS_deinit();
}

String VirtualFilesystem::createStandardOutputDirectory(const StandardOutputDirectoryOptions& standardOutputDirectoryOptions) {
	if (const char* const prefDir = PHYSFS_getPrefDir(standardOutputDirectoryOptions.organizationName.c_str(), standardOutputDirectoryOptions.applicationName.c_str())) {
		String result{prefDir};
		const StringView dirSeparator = PHYSFS_getDirSeparator();
		if (result.ends_with(dirSeparator)) {
			result.resize(result.size() - dirSeparator.size());
			if (result.empty()) {
				result = ".";
			}
		}
		return result;
	}
	return {};
}

CStringView VirtualFilesystem::getOutputDirectory() const noexcept {
	return outputDirectory;
}

void VirtualFilesystem::setOutputDirectory(CStringView path) {
	if (PHYSFS_setWriteDir((path.empty()) ? nullptr : path.c_str()) == 0) {
		throw File::Error{String{"Failed to set output directory:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	outputDirectory = String{path};
}

void VirtualFilesystem::mountInputArchive(CStringView path, MountPriority priority) {
	if (path.empty()) {
		return;
	}
	if (PHYSFS_mount(path.c_str(), nullptr, (priority == MountPriority::LOWER) ? 1 : 0) == 0) {
		throw File::Error{String{"Failed to mount archive \""} + path.c_str() + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) + "\n"};
	}
}

void VirtualFilesystem::unmountInputArchive(CStringView path) {
	if (path.empty()) {
		return;
	}
	if (PHYSFS_unmount(path.c_str()) == 0) {
		throw File::Error{String{"Failed to unmount archive \""} + path.c_str() + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) + "\n"};
	}
}

void VirtualFilesystem::mountInputArchivesInMountedDirectory(CStringView filepath, CStringView archiveFileExtension, MountPriority priority,
	FunctionView<void(CStringView path)> callback) {
	struct Context {
		VirtualFilesystem& filesystem;
		CStringView directoryFilepath;
		CStringView archiveFileExtension;
		MountPriority priority;
		const FunctionView<void(CStringView path)>& callback;
	} context{
		.filesystem = *this,
		.directoryFilepath = filepath,
		.archiveFileExtension = archiveFileExtension,
		.priority = priority,
		.callback = callback,
	};
	PHYSFS_enumerate(
		filepath.c_str(),
		[](void* data, const char*, const char* fname) -> PHYSFS_EnumerateCallbackResult {
			Context& context = *static_cast<Context*>(data);
			const StringView filename{fname};
			if (!context.archiveFileExtension.empty()) {
				if (const StringView extension{context.archiveFileExtension};
					filename.size() <= extension.size() || (context.archiveFileExtension[0] != '.' && filename[filename.size() - extension.size() - 1] != '.') ||
					!filename.ends_with(extension)) {
					return PHYSFS_ENUM_OK;
				}
			}
			const String virtualArchiveFilepath = String{context.directoryFilepath} + "/" + fname;
			const char* realDirectory = PHYSFS_getRealDir(virtualArchiveFilepath.c_str());
			if (!realDirectory) {
				throw File::Error{
					String{"Failed to get the real directory of archive \""} + virtualArchiveFilepath + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) + "\n"};
			}
			String archiveFilepath = String{realDirectory} + PHYSFS_getDirSeparator() + context.directoryFilepath.c_str() + PHYSFS_getDirSeparator() + fname;
			context.filesystem.mountInputArchive(archiveFilepath.c_str(), context.priority);
			context.callback(archiveFilepath);
			return PHYSFS_ENUM_OK;
		},
		&context);
}

CStringView VirtualFilesystem::findInputArchiveOfFile(CStringView filepath) const noexcept {
	if (const char* const result = PHYSFS_getRealDir(filepath.c_str())) {
		return result;
	}
	return {};
}

void VirtualFilesystem::createOutputDirectory(CStringView filepath) {
	if (PHYSFS_mkdir(filepath.c_str()) == 0) {
		throw File::Error{String{"Failed to create directory \""} + filepath.c_str() + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

void VirtualFilesystem::createParentOutputDirectories(CStringView filepath) {
	if (const size_t lastSlashPosition = filepath.find_last_of("/\\"); lastSlashPosition != CStringView::npos) {
		if (PHYSFS_mkdir(String{filepath.substr(0, lastSlashPosition)}.c_str()) == 0) {
			throw File::Error{String{"Failed to create parent directories for file \""} + filepath.c_str() + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
		}
	}
}

void VirtualFilesystem::deleteOutputFile(CStringView filepath) {
	if (PHYSFS_delete(filepath.c_str()) == 0) {
		throw File::Error{String{"Failed to delete file \""} + filepath.c_str() + "\":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

bool VirtualFilesystem::inputFileExists(CStringView filepath) const {
	return PHYSFS_exists(filepath.c_str()) != 0;
}

File::Metadata VirtualFilesystem::getInputFileMetadata(CStringView filepath) const {
	PHYSFS_Stat metadata{};
	if (PHYSFS_stat(filepath.c_str(), &metadata) == 0) {
		throw File::Error{String{"Failed to get file metadata:\n"} + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	File::Kind kind{};
	switch (metadata.filetype) {
		case PHYSFS_FILETYPE_REGULAR: kind = File::Kind::REGULAR; break;
		case PHYSFS_FILETYPE_DIRECTORY: kind = File::Kind::DIRECTORY; break;
		case PHYSFS_FILETYPE_SYMLINK: kind = File::Kind::SYMLINK; break;
		case PHYSFS_FILETYPE_OTHER: kind = File::Kind::OTHER; break;
	}
	return {
		.size = (metadata.filesize < 0) ? File::npos : static_cast<size_t>(metadata.filesize),
		.creationTime = metadata.createtime,
		.lastAccessTime = metadata.accesstime,
		.lastModificationTime = metadata.modtime,
		.kind = kind,
		.readOnly = metadata.readonly != 0,
	};
}

void VirtualFilesystem::forEachInputFilenameInDirectory(CStringView filepath, FunctionView<bool(CStringView filename)> callback) const {
	struct Context {
		std::exception_ptr error{};
		const FunctionView<bool(CStringView filename)>& callback;
	};

	Context context{.callback = callback};
	if (PHYSFS_enumerate(
			filepath.c_str(),
			[](void* data, const char*, const char* fname) -> PHYSFS_EnumerateCallbackResult {
				Context& context = *static_cast<Context*>(data);
				try {
					if (context.callback(fname)) {
						return PHYSFS_ENUM_STOP;
					}
				} catch (...) {
					context.error = std::current_exception();
					return PHYSFS_ENUM_ERROR;
				}
				return PHYSFS_ENUM_OK;
			},
			&context) == 0) {
		if (context.error) {
			std::rethrow_exception(std::move(context.error)); // NOLINT(performance-move-const-arg)
		}
		throw File::Error{String{"Failed to enumerate directory \""} + filepath.c_str() + ":\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

InputFileHandle VirtualFilesystem::openInputFile(CStringView filepath) const {
#ifdef GREM_USE_MULTITHREADING
	std::error_code errorCode{};
	FileLock lock = lockInputFile(filepath, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to open file \""} + filepath.c_str() + "\" for reading:\n" + errorCode.message()};
	}
	VirtualInputFile result{PHYSFS_openRead(filepath.c_str()), std::move(lock)};
#else
	VirtualInputFile result{PHYSFS_openRead(filepath.c_str())};
#endif
	if (!result) {
		throw File::Error{String{"Failed to open file \""} + filepath.c_str() + "\" for reading:\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	return InputFileHandle::create<VirtualInputFile>(std::move(result));
}

InputFileHandle VirtualFilesystem::tryOpenInputFile(CStringView filepath) const {
#ifdef GREM_USE_MULTITHREADING
	std::error_code errorCode{};
	FileLock lock = lockInputFile(filepath, errorCode);
	if (errorCode) {
		return {};
	}
	VirtualInputFile result{PHYSFS_openRead(filepath.c_str()), std::move(lock)};
#else
	VirtualInputFile result{PHYSFS_openRead(filepath.c_str())};
#endif
	if (!result) {
		return {};
	}
	return InputFileHandle::create<VirtualInputFile>(std::move(result));
}

OutputFileHandle VirtualFilesystem::openEmptyOutputFile(CStringView filepath) {
#ifdef GREM_USE_MULTITHREADING
	std::error_code errorCode{};
	FileLock lock = lockOutputFile(filepath, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to create file \""} + filepath.c_str() + "\" for writing:\n" + errorCode.message()};
	}
	VirtualOutputFile result{PHYSFS_openWrite(filepath.c_str()), std::move(lock)};
#else
	VirtualOutputFile result{PHYSFS_openWrite(filepath.c_str())};
#endif
	if (!result) {
		throw File::Error{String{"Failed to create file \""} + filepath.c_str() + "\" for writing:\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	return OutputFileHandle::create<VirtualOutputFile>(std::move(result));
}

void VirtualFilesystem::createEmptyOutputFile(CStringView filepath) {
#ifdef GREM_USE_MULTITHREADING
	std::error_code errorCode{};
	FileLock lock = lockOutputFile(filepath, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to create file \""} + filepath.c_str() + "\" for writing:\n" + errorCode.message()};
	}
	VirtualOutputFile result{PHYSFS_openWrite(filepath.c_str()), std::move(lock)};
#else
	VirtualOutputFile result{PHYSFS_openWrite(filepath.c_str())};
#endif
	if (!result) {
		throw File::Error{String{"Failed to create file \""} + filepath.c_str() + "\" for writing:\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
}

OutputFileHandle VirtualFilesystem::openOutputFileForAppending(CStringView filepath) {
#ifdef GREM_USE_MULTITHREADING
	std::error_code errorCode{};
	FileLock lock = lockOutputFile(filepath, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to open file \""} + filepath.c_str() + "\" for appending:\n" + errorCode.message()};
	}
	VirtualOutputFile result{PHYSFS_openAppend(filepath.c_str()), std::move(lock)};
#else
	VirtualOutputFile result{PHYSFS_openAppend(filepath.c_str())};
#endif
	if (!result) {
		throw File::Error{String{"Failed to open file \""} + filepath.c_str() + "\" for appending:\n" + PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())};
	}
	return OutputFileHandle::create<VirtualOutputFile>(std::move(result));
}

#ifdef GREM_USE_MULTITHREADING
VirtualFilesystem::FileLock::~FileLock() {
	if (lock.owns_lock()) {
		lock.unlock();
		ScopedLock fileMutexesSetLock{filesystem->fileMutexesSetMutex};
		if (fileMutex.value.use_count() <= 2) {
			filesystem->fileMutexes.erase(fileMutex.value->canonicalFilepath);
		}
	}
}

VirtualFilesystem::FileLock VirtualFilesystem::lockInputFile(CStringView filepath, std::error_code& errorCode) const {
	errorCode.clear();
	String canonicalFilepath{};
	if (const CStringView inputDirectory = VirtualFilesystem::findInputArchiveOfFile(filepath); !inputDirectory.empty()) {
		const std::filesystem::path directory{inputDirectory.c_str()};
		if (std::filesystem::is_directory(directory, errorCode)) {
			canonicalFilepath = std::filesystem::canonical(directory / filepath.c_str(), errorCode).generic_string();
		} else {
			if (errorCode) {
				return {};
			}
			canonicalFilepath = (std::filesystem::canonical(directory, errorCode) / filepath.c_str()).generic_string();
		}
		if (errorCode) {
			return {};
		}
	} else {
		errorCode = make_error_code(std::errc::no_such_file_or_directory);
		return {};
	}
	SharedFileMutex fileMutex{};
	{
		ScopedLock fileMutexesSetLock{fileMutexesSetMutex};
		fileMutex = *fileMutexes.emplace(std::move(canonicalFilepath)).first;
	}
	return FileLock{*this, std::move(fileMutex)};
}

VirtualFilesystem::FileLock VirtualFilesystem::lockOutputFile(CStringView filepath, std::error_code& errorCode) const {
	errorCode.clear();
	const CStringView outputDirectory = VirtualFilesystem::getOutputDirectory();
	if (outputDirectory.empty()) {
		errorCode = make_error_code(std::errc::read_only_file_system);
		return {};
	}
	String canonicalFilepath = std::filesystem::weakly_canonical(std::filesystem::path{outputDirectory.c_str()} / filepath.c_str(), errorCode).generic_string();
	if (errorCode) {
		return {};
	}
	SharedFileMutex fileMutex{};
	{
		ScopedLock fileMutexesSetLock{fileMutexesSetMutex};
		fileMutex = *fileMutexes.emplace(std::move(canonicalFilepath)).first;
	}
	return FileLock{*this, std::move(fileMutex)};
}
#endif

} // namespace grem::application
