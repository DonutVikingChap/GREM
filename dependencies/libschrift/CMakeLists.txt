message(STATUS "Configuring libschrift...")

FetchContent_Declare(libschrift
	GIT_REPOSITORY https://github.com/tomolt/libschrift
	GIT_TAG 8e533fd07acc2f8ae4cffe7f95d2c3392773e2b5 # 0.10.2
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(libschrift)

file(READ "${libschrift_SOURCE_DIR}/LICENSE" GREM_LIBSCHRIFT_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_LIBSCHRIFT_LICENSE_FILE_CONTENTS "${GREM_LIBSCHRIFT_LICENSE_FILE_CONTENTS}")

set(GREM_LIBSCHRIFT_TITLE "libschrift (0.10.2)" PARENT_SCOPE)
set(GREM_LIBSCHRIFT_USE "Used by GREM." PARENT_SCOPE)
set(GREM_LIBSCHRIFT_LINK "https://github.com/tomolt/libschrift/tree/8e533fd07acc2f8ae4cffe7f95d2c3392773e2b5" PARENT_SCOPE)
set(GREM_LIBSCHRIFT_COPYRIGHT "© 2019-2022 Thomas Oltmann and contributors" PARENT_SCOPE)
set(GREM_LIBSCHRIFT_LICENSE "${GREM_LIBSCHRIFT_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_LIBSCHRIFT
	PARENT_SCOPE)

add_library(schrift STATIC EXCLUDE_FROM_ALL)
target_sources(schrift
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${libschrift_SOURCE_DIR}"
		FILES "${libschrift_SOURCE_DIR}/schrift.h"
	PRIVATE
		"${libschrift_SOURCE_DIR}/schrift.c")
target_compile_options(schrift PRIVATE $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:-Wno-discarded-qualifiers>)
target_compile_definitions(schrift PRIVATE $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:__STRICT_ANSI__>)
set_target_properties(schrift PROPERTIES SYSTEM ON)

set(GREM_GRAPHICS_2D_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_2D_PRIVATE_DEPENDENCIES} schrift PARENT_SCOPE)
