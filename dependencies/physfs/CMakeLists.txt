message(STATUS "Configuring physfs...")

FetchContent_Declare(physfs
	GIT_REPOSITORY https://github.com/icculus/physfs
	GIT_TAG 23d0b5696445d86262221519c3db3ae351376da0 # 2026-06-17 (main)
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(physfs)

file(READ "${physfs_SOURCE_DIR}/LICENSE.txt" GREM_PHYSFS_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_PHYSFS_LICENSE_FILE_CONTENTS "${GREM_PHYSFS_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(c\\) 2001-.... Ryan C\\. Gordon <icculus@icculus\\.org> and others\\." GREM_PHYSFS_COPYRIGHT_FILE_CONTENTS "${GREM_PHYSFS_LICENSE_FILE_CONTENTS}")

file(READ "${physfs_SOURCE_DIR}/src/physfs_miniz.h" GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS "${GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "This is free and unencumbered software released into the public domain\\..*<https://unlicense\\.org/>\n" GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS "${GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS}")
string(REPLACE "\n  " "\n" GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS "${GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS}")

set(GREM_PHYSFS_TITLE "PhysicsFS" PARENT_SCOPE)
set(GREM_PHYSFS_USE "Used by GREM." PARENT_SCOPE)
set(GREM_PHYSFS_LINK "https://github.com/icculus/physfs/tree/23d0b5696445d86262221519c3db3ae351376da0" PARENT_SCOPE)
set(GREM_PHYSFS_COPYRIGHT "${GREM_PHYSFS_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_PHYSFS_LICENSE "${GREM_PHYSFS_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_PHYSFS_TINFL_TITLE "tinfl.c (v1.11)" PARENT_SCOPE)
set(GREM_PHYSFS_TINFL_USE "Used by PhysicsFS." PARENT_SCOPE)
set(GREM_PHYSFS_TINFL_LINK "https://github.com/richgel999/miniz/blob/3172e9f7db511aa8cb14d6451074bb8569c675bf/tinfl.c" PARENT_SCOPE)
set(GREM_PHYSFS_TINFL_CREDITS "Rich Geldreich" PARENT_SCOPE)
set(GREM_PHYSFS_TINFL_LICENSE "${GREM_PHYSFS_TINFL_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_LZMA_SDK_TITLE "LZMA SDK" PARENT_SCOPE)
set(GREM_LZMA_SDK_USE "Used by PhysicsFS." PARENT_SCOPE)
set(GREM_LZMA_SDK_LINK "http://www.7-zip.org/sdk.html" PARENT_SCOPE)
set(GREM_LZMA_SDK_CREDITS "Igor Pavlov" PARENT_SCOPE)
set(GREM_LZMA_SDK_LICENSE "\
LZMA SDK is placed in the public domain.\n\
\n\
Anyone is free to copy, modify, publish, use, compile, sell, or distribute the original LZMA SDK code, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.\n\
" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_PHYSFS
	GREM_PHYSFS_TINFL
	GREM_LZMA_SDK
	PARENT_SCOPE)

add_library(physfs EXCLUDE_FROM_ALL)
target_sources(physfs
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${physfs_SOURCE_DIR}/src"
		FILES "${physfs_SOURCE_DIR}/src/physfs.h"
	PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs.c"
		"${physfs_SOURCE_DIR}/src/physfs_byteorder.c"
		"${physfs_SOURCE_DIR}/src/physfs_unicode.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_dir.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_unpacked.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_7z.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_zip.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_wad.c"
		"${physfs_SOURCE_DIR}/src/physfs_archiver_iso9660.c")
target_compile_definitions(physfs PRIVATE
	"PHYSFS_EMSCRIPTEN_STORAGE_PATH=\"/home/web_user/.local/share\""
	PHYSFS_SUPPORTS_DEFAULT=0
	PHYSFS_SUPPORTS_7Z=1
	PHYSFS_SUPPORTS_ZIP=1
	PHYSFS_SUPPORTS_WAD=1
	PHYSFS_SUPPORTS_ISO9660=1
	$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:_DEFAULT_SOURCE>)
set_target_properties(physfs PROPERTIES SYSTEM ON)

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    set(WINRT TRUE)
endif()

if(EMSCRIPTEN)
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_posix.c"
		"${physfs_SOURCE_DIR}/src/physfs_platform_emscripten.c")
elseif(WINRT)
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_winrt.cpp")
	if(NOT BUILD_SHARED_LIBS)
		target_compile_definitions(physfs PRIVATE "PHYSFS_STATIC")
		set_target_properties(physfs PROPERTIES STATIC_LIBRARY_FLAGS "/ignore:4264")
	endif()
	set_target_properties(physfs PROPERTIES VS_WINRT_COMPONENT ON)
elseif(WIN32)
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_windows.c")
	if(NOT BUILD_SHARED_LIBS)
		target_compile_definitions(physfs PRIVATE "PHYSFS_STATIC")
	endif()
	target_link_libraries(physfs PRIVATE advapi32 shell32)
elseif(APPLE)
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_posix.c"
		"${physfs_SOURCE_DIR}/src/physfs_platform_apple.m")
	target_link_options(physfs PRIVATE -framework IOKit -framework Foundation)
elseif(ANDROID)
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_posix.c"
		"${physfs_SOURCE_DIR}/src/physfs_platform_unix.c"
		"${physfs_SOURCE_DIR}/src/physfs_platform_android.c")
else()
	target_sources(physfs PRIVATE
		"${physfs_SOURCE_DIR}/src/physfs_platform_posix.c"
		"${physfs_SOURCE_DIR}/src/physfs_platform_unix.c")
	find_library(PTHREAD_LIBRARY pthread)
	if(PTHREAD_LIBRARY)
		target_link_libraries(physfs PRIVATE ${PTHREAD_LIBRARY})
	endif()
endif()

if(BUILD_SHARED_LIBS)
	set_target_properties(physfs PROPERTIES
		MACOSX_RPATH 1
		VERSION 3.3.0
		SOVERSION 1
		EXPORT_NAME "PhysFS")
endif()

set(GREM_APPLICATION_PRIVATE_DEPENDENCIES ${GREM_APPLICATION_PRIVATE_DEPENDENCIES} physfs PARENT_SCOPE)
