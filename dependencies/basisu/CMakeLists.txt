message(STATUS "Configuring basisu...")

FetchContent_Declare(basisu
	GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal
	GIT_TAG 9bebe16726b3a61c8c213eeee3b7cffb462ef34e # v2.50
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(basisu)

file(READ "${basisu_SOURCE_DIR}/NOTICE" GREM_BASISU_TRANSCODER_COPYRIGHT_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_BASISU_TRANSCODER_COPYRIGHT_FILE_CONTENTS "${GREM_BASISU_TRANSCODER_COPYRIGHT_FILE_CONTENTS}")

file(READ "${basisu_SOURCE_DIR}/LICENSE" GREM_BASISU_TRANSCODER_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_BASISU_TRANSCODER_LICENSE_FILE_CONTENTS "${GREM_BASISU_TRANSCODER_LICENSE_FILE_CONTENTS}")

set(GREM_BASISU_TRANSCODER_TITLE "basis_universal transcoder (v2.50)" PARENT_SCOPE)
set(GREM_BASISU_TRANSCODER_USE "Used by GREM." PARENT_SCOPE)
set(GREM_BASISU_TRANSCODER_LINK "https://github.com/BinomialLLC/basis_universal/tree/9bebe16726b3a61c8c213eeee3b7cffb462ef34e" PARENT_SCOPE)
set(GREM_BASISU_TRANSCODER_COPYRIGHT "${GREM_BASISU_TRANSCODER_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_BASISU_TRANSCODER_LICENSE "${GREM_BASISU_TRANSCODER_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_BASISU_TRANSCODER
	PARENT_SCOPE)

add_library(basisu_transcoder STATIC EXCLUDE_FROM_ALL)
target_sources(basisu_transcoder
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${basisu_SOURCE_DIR}/transcoder"
		FILES
			"${basisu_SOURCE_DIR}/transcoder/basisu_astc_hdr_core.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_astc_helpers.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_containers_impl.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_containers.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_file_headers.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_transcoder_internal.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_transcoder_uastc.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu_transcoder.h"
			"${basisu_SOURCE_DIR}/transcoder/basisu.h"
	PRIVATE
		"${basisu_SOURCE_DIR}/transcoder/basisu_transcoder.cpp")
target_compile_definitions(basisu_transcoder PRIVATE
	BASISD_SUPPORT_KTX2_ZSTD=1
	BASISD_SUPPORT_FXT1=0
	BASISD_SUPPORT_PVRTC2=0)
target_compile_options(basisu_transcoder PRIVATE
	$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:        -fvisibility=hidden -fPIC -fno-strict-aliasing -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wno-unused-local-typedefs -Wno-unused-value -Wno-unused-parameter -Wno-unused-variable -Wno-reorder -Wno-misleading-indentation -Wno-class-memaccess -Wno-deprecated-copy -Wno-maybe-uninitialized -Wno-unused-function -Wno-stringop-overflow -Wno-unknown-warning-option -Wno-nontrivial-memcall -Wno-deprecated-enum-enum-conversion -Wno-deprecated-anon-enum-enum-conversion $<$<EQUAL:4,${CMAKE_SIZEOF_VOID_P}>:-m32>>
	$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:    -fvisibility=hidden -fPIC -fno-strict-aliasing -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wno-unused-local-typedefs -Wno-unused-value -Wno-unused-parameter -Wno-unused-variable -Wno-reorder -Wno-misleading-indentation -Wno-class-memaccess -Wno-deprecated-copy -Wno-maybe-uninitialized -Wno-unused-function -Wno-stringop-overflow -Wno-unknown-warning-option -Wno-nontrivial-memcall -Wno-deprecated-enum-enum-conversion -Wno-deprecated-anon-enum-enum-conversion $<$<EQUAL:4,${CMAKE_SIZEOF_VOID_P}>:-m32>>
	$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:      /wd4828>)
target_link_libraries(basisu_transcoder PRIVATE zstd)
set_target_properties(basisu_transcoder PROPERTIES SYSTEM ON)

set(GREM_RESOURCE_PRIVATE_DEPENDENCIES ${GREM_RESOURCE_PRIVATE_DEPENDENCIES} basisu_transcoder PARENT_SCOPE)
