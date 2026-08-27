// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>

#include <cstddef> // std::size_t
#include <imgui.h> // Im...
#include <utility> // std::move

namespace grem::imgui {

namespace detail {

struct FileHandle {
	grem::Variant<grem::InputFileHandle, grem::OutputFileHandle> file;
};

} // namespace detail

} // namespace grem::imgui

ImFileHandle ImFileOpen(const char* filename, const char* mode) {
	if (!filename || !mode) {
		return nullptr;
	}

	if (!ImGui::GetCurrentContext()) {
		return nullptr;
	}

	ImGuiIO& io = ImGui::GetIO();
	if (!io.BackendRendererName || !grem::CStringView{io.BackendRendererName}.starts_with("GREM")) {
		return nullptr;
	}

	grem::Filesystem* const filesystem = static_cast<grem::Filesystem*>(io.BackendRendererUserData);
	if (!filesystem) {
		return nullptr;
	}

	const grem::CStringView modeString{mode};
	if (modeString.starts_with('r')) {
		grem::InputFileHandle file = filesystem->tryOpenInputFile(filename);
		if (!file) {
			return nullptr;
		}
		return new grem::imgui::detail::FileHandle{.file = std::move(file)}; // NOLINT(cppcoreguidelines-owning-memory)
	}
	if (modeString.starts_with('w')) {
		grem::OutputFileHandle file{};
		try {
			file = filesystem->openEmptyOutputFile(filename);
		} catch (...) {
			return nullptr;
		}
		return new grem::imgui::detail::FileHandle{.file = std::move(file)}; // NOLINT(cppcoreguidelines-owning-memory)
	}
	if (modeString.starts_with('a')) {
		grem::OutputFileHandle file{};
		try {
			file = filesystem->openOutputFileForAppending(filename);
		} catch (...) {
			return nullptr;
		}
		return new grem::imgui::detail::FileHandle{.file = std::move(file)}; // NOLINT(cppcoreguidelines-owning-memory)
	}
	return nullptr;
}

bool ImFileClose(ImFileHandle file) {
	if (file) {
		delete static_cast<grem::imgui::detail::FileHandle*>(file); // NOLINT(cppcoreguidelines-owning-memory)
		return true;
	}
	return false;
}

std::size_t ImFileGetSize(ImFileHandle file) {
	if (!file) {
		return grem::Limits<std::size_t>::MAX;
	}

	GREM_MATCH(file->file) {
		GREM_CASE(const grem::InputFileHandle& inputFileHandle) {
			return inputFileHandle.size();
		}
		GREM_CASE(const grem::OutputFileHandle& outputFileHandle) break;
	}
	return grem::Limits<std::size_t>::MAX;
}

std::size_t ImFileRead(void* data, std::size_t size, std::size_t count, ImFileHandle file) {
	if (!file) {
		return 0;
	}

	GREM_MATCH(file->file) {
		GREM_CASE(grem::InputFileHandle & inputFileHandle) {
			if (count > grem::Limits<std::size_t>::MAX / size) {
				return 0;
			}
			try {
				return inputFileHandle.readUntilEOF(grem::Span{reinterpret_cast<grem::byte*>(data), size * count});
			} catch (...) {
			}
			break;
		}
		GREM_CASE(grem::OutputFileHandle & outputFileHandle) break;
	}
	return 0;
}

std::size_t ImFileWrite(const void* data, std::size_t size, std::size_t count, ImFileHandle file) {
	if (!file) {
		return 0;
	}

	GREM_MATCH(file->file) {
		GREM_CASE(grem::InputFileHandle & inputFileHandle) break;
		GREM_CASE(grem::OutputFileHandle & outputFileHandle) { //
			if (count > grem::Limits<std::size_t>::MAX / size) {
				return 0;
			}
			try {
				return outputFileHandle.writeSome(grem::Span{reinterpret_cast<const grem::byte*>(data), size * count});
			} catch (...) {
			}
			break;
		}
	}
	return 0;
}
