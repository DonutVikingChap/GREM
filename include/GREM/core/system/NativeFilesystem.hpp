// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_SYSTEM_NATIVE_FILESYSTEM_HPP
#define GREM_CORE_SYSTEM_NATIVE_FILESYSTEM_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>

namespace grem {

/**
 * Wrapper over the native host filesystem.
 */
class NativeFilesystem : public Filesystem {
public:
	/** Default constructor. */
	constexpr NativeFilesystem() noexcept = default;

	/** Destructor. */
	~NativeFilesystem() override = default;

	/** Copying a filesystem is not allowed, since it manages global state. */
	NativeFilesystem(const NativeFilesystem&) = delete;

	/** Moving a filesystem is not allowed, since it manages global state. */
	NativeFilesystem(NativeFilesystem&&) = delete;

	/** Copying a filesystem is not allowed, since it manages global state. */
	NativeFilesystem& operator=(const NativeFilesystem&) = delete;

	/** Moving a filesystem is not allowed, since it manages global state. */
	NativeFilesystem& operator=(NativeFilesystem&&) = delete;

	[[nodiscard]] GREM_API(core) CStringView getOutputDirectory() const noexcept override;

	[[nodiscard]] GREM_API(core) CStringView findInputArchiveOfFile(CStringView filepath) const noexcept override;

	GREM_API(core) void createOutputDirectory(CStringView filepath) override;
	GREM_API(core) void createParentOutputDirectories(CStringView filepath) override;
	GREM_API(core) void deleteOutputFile(CStringView filepath) override;

	[[nodiscard]] GREM_API(core) bool inputFileExists(CStringView filepath) const override;
	[[nodiscard]] GREM_API(core) File::Metadata getInputFileMetadata(CStringView filepath) const override;
	GREM_API(core) void forEachInputFilenameInDirectory(CStringView filepath, FunctionView<bool(CStringView filename)> callback) const override;

	[[nodiscard]] GREM_API(core) InputFileHandle openInputFile(CStringView filepath) const override;
	[[nodiscard]] GREM_API(core) InputFileHandle tryOpenInputFile(CStringView filepath) const override;
	[[nodiscard]] GREM_API(core) OutputFileHandle openEmptyOutputFile(CStringView filepath) override;
	GREM_API(core) void createEmptyOutputFile(CStringView filepath) override;
	[[nodiscard]] GREM_API(core) OutputFileHandle openOutputFileForAppending(CStringView filepath) override;
};

} // namespace grem

#endif
