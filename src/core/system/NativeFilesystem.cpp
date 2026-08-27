// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/NativeFilesystem.hpp>
#include <GREM/core/system/NativeInputFile.hpp>
#include <GREM/core/system/NativeOutputFile.hpp>
#include <GREM/core/time.hpp>

#include <filesystem> // std::filesystem::...
#include <fstream>    // std::ifstream, std::ofstream
#include <utility>    // std::move

namespace grem {

namespace {

String formatPath(const std::filesystem::path& path) {
	const std::filesystem::path full = std::filesystem::weakly_canonical(path);
	const std::filesystem::path base = std::filesystem::weakly_canonical(std::filesystem::current_path());
	auto pathIt = full.begin();
	auto baseIt = base.begin();
	const auto pathEnd = full.end();
	const auto baseEnd = base.end();
	while (baseIt != baseEnd) {
		if (pathIt == pathEnd || *pathIt != *baseIt) {
			return path.filename().generic_string();
		}
		++pathIt;
		++baseIt;
	}
	return full.lexically_relative(base).generic_string();
}

} // namespace

CStringView NativeFilesystem::getOutputDirectory() const noexcept {
	return ".";
}

CStringView NativeFilesystem::findInputArchiveOfFile(CStringView filepath) const noexcept {
	if (inputFileExists(filepath)) {
		return ".";
	}
	return {};
}

void NativeFilesystem::createOutputDirectory(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	std::error_code errorCode{};
	std::filesystem::create_directories(path, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to create directory \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
}

void NativeFilesystem::createParentOutputDirectories(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	if (path.has_parent_path()) {
		std::error_code errorCode{};
		std::filesystem::create_directories(path.parent_path(), errorCode);
		if (errorCode) {
			throw File::Error{String{"Failed to create parent directories for file \""} + formatPath(path) + "\":\n" + errorCode.message()};
		}
	}
}

void NativeFilesystem::deleteOutputFile(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	std::error_code errorCode{};
	const bool result = std::filesystem::remove(path, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to delete file \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
	if (!result) {
		throw File::Error{String{"Failed to delete file \""} + formatPath(path) + "\":\n" + "The specified file does not exist."};
	}
}

bool NativeFilesystem::inputFileExists(CStringView filepath) const {
	const std::filesystem::path path{filepath.c_str()};
	std::error_code errorCode{};
	const bool result = std::filesystem::exists(path, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to check existence of file \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
	return result;
}

File::Metadata NativeFilesystem::getInputFileMetadata(CStringView filepath) const {
	const std::filesystem::path path{filepath.c_str()};
	std::error_code errorCode{};
	const std::filesystem::file_status status = std::filesystem::status(path, errorCode);
	if (errorCode) {
		throw File::Error{String{"Failed to get status of file \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
	File::Kind kind{};
	switch (status.type()) {
		case std::filesystem::file_type::regular: kind = File::Kind::REGULAR; break;
		case std::filesystem::file_type::directory: kind = File::Kind::DIRECTORY; break;
		case std::filesystem::file_type::symlink: kind = File::Kind::SYMLINK; break;
		default: kind = File::Kind::OTHER; break;
	}
	size_t size = File::npos;
	if (kind == File::Kind::REGULAR) {
		size = static_cast<size_t>(std::filesystem::file_size(path, errorCode));
		if (errorCode) {
			throw File::Error{String{"Failed to get size of file \""} + formatPath(path) + "\":\n" + errorCode.message()};
		}
	}
	const int64_t lastModificationTime = duration_cast<DurationBase<int64_t, Ratio<1, 1>>>(std::filesystem::last_write_time(path, errorCode).time_since_epoch()).count();
	if (errorCode) {
		throw File::Error{String{"Failed to get modification time of file \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
	return {
		.size = size,
		.creationTime = -1,
		.lastAccessTime = -1,
		.lastModificationTime = lastModificationTime,
		.kind = kind,
		.readOnly = (status.permissions() & (std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write)) ==
	                std::filesystem::perms::none,
	};
}

void NativeFilesystem::forEachInputFilenameInDirectory(CStringView filepath, FunctionView<bool(CStringView filename)> callback) const {
	const std::filesystem::path path{filepath.c_str()};
	std::error_code errorCode{};
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{path, errorCode}) {
		if (errorCode) {
			throw File::Error{String{"Failed to enumerate directory \""} + formatPath(path) + ":\n" + errorCode.message()};
		}
		if (callback(entry.path().generic_string().c_str())) {
			return;
		}
	}
	if (errorCode) {
		throw File::Error{String{"Failed to enumerate directory \""} + formatPath(path) + ":\n" + errorCode.message()};
	}
}

InputFileHandle NativeFilesystem::openInputFile(CStringView filepath) const {
	const std::filesystem::path path{filepath.c_str()};
	std::ifstream stream{path, std::ifstream::binary | std::ifstream::in};
	if (!stream.good()) {
		throw File::Error{String{"Failed to open file \""} + formatPath(path) + "\" for reading."};
	}
	std::error_code errorCode{};
	const size_t fileSize = static_cast<size_t>(std::filesystem::file_size(path, errorCode));
	if (errorCode) {
		throw File::Error{String{"Failed to get size of file \""} + formatPath(path) + "\":\n" + errorCode.message()};
	}
	return InputFileHandle::create<NativeInputFile>(NativeInputFile{std::move(stream), fileSize});
}

InputFileHandle NativeFilesystem::tryOpenInputFile(CStringView filepath) const {
	const std::filesystem::path path{filepath.c_str()};
	std::ifstream stream{path, std::ifstream::binary | std::ifstream::in};
	if (!stream.good()) {
		return {};
	}
	std::error_code errorCode{};
	const size_t fileSize = static_cast<size_t>(std::filesystem::file_size(path, errorCode));
	if (errorCode) {
		return {};
	}
	return InputFileHandle::create<NativeInputFile>(NativeInputFile{std::move(stream), fileSize});
}

OutputFileHandle NativeFilesystem::openEmptyOutputFile(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	std::ofstream stream{path, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc};
	if (!stream.good()) {
		throw File::Error{String{"Failed to create file \""} + formatPath(path) + "\" for writing."};
	}
	return OutputFileHandle::create<NativeOutputFile>(NativeOutputFile{std::move(stream)});
}

void NativeFilesystem::createEmptyOutputFile(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	std::ofstream stream{path, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc};
	if (!stream.good()) {
		throw File::Error{String{"Failed to create file \""} + formatPath(path) + "\" for writing."};
	}
}

OutputFileHandle NativeFilesystem::openOutputFileForAppending(CStringView filepath) {
	const std::filesystem::path path{filepath.c_str()};
	std::ofstream stream{path, std::ofstream::binary | std::ofstream::out | std::ofstream::app};
	if (!stream.good()) {
		throw File::Error{String{"Failed to create file \""} + formatPath(path) + "\" for appending."};
	}
	return OutputFileHandle::create<NativeOutputFile>(NativeOutputFile{std::move(stream)});
}

} // namespace grem
