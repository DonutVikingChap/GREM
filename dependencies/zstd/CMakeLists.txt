message(STATUS "Configuring zstd...")

FetchContent_Declare(zstd
	GIT_REPOSITORY https://github.com/facebook/zstd
	GIT_TAG f8745da6ff1ad1e7bab384bd1f9d742439278e99 # v1.5.7
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(zstd)

file(READ "${zstd_SOURCE_DIR}/LICENSE" GREM_ZSTD_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_ZSTD_LICENSE_FILE_CONTENTS "${GREM_ZSTD_LICENSE_FILE_CONTENTS}")

set(GREM_ZSTD_TITLE "Zstandard (v1.5.7)" PARENT_SCOPE)
set(GREM_ZSTD_USE "Used by GREM and basis_universal transcoder." PARENT_SCOPE)
set(GREM_ZSTD_LINK "https://github.com/facebook/zstd/tree/f8745da6ff1ad1e7bab384bd1f9d742439278e99" PARENT_SCOPE)
set(GREM_ZSTD_COPYRIGHT "Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved." PARENT_SCOPE)
set(GREM_ZSTD_LICENSE "${GREM_ZSTD_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_ZSTD
	PARENT_SCOPE)

add_library(zstd EXCLUDE_FROM_ALL)
target_sources(zstd
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${zstd_SOURCE_DIR}/lib"
		FILES
			"${zstd_SOURCE_DIR}/lib/zstd_errors.h"
			"${zstd_SOURCE_DIR}/lib/zstd.h"
	PRIVATE
		"${zstd_SOURCE_DIR}/lib/common/debug.c"
		"${zstd_SOURCE_DIR}/lib/common/entropy_common.c"
		"${zstd_SOURCE_DIR}/lib/common/error_private.c"
		"${zstd_SOURCE_DIR}/lib/common/fse_decompress.c"
		"${zstd_SOURCE_DIR}/lib/common/pool.c"
		"${zstd_SOURCE_DIR}/lib/common/threading.c"
		"${zstd_SOURCE_DIR}/lib/common/xxhash.c"
		"${zstd_SOURCE_DIR}/lib/common/zstd_common.c"

		"${zstd_SOURCE_DIR}/lib/compress/fse_compress.c"
		"${zstd_SOURCE_DIR}/lib/compress/hist.c"
		"${zstd_SOURCE_DIR}/lib/compress/huf_compress.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_compress_literals.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_compress_sequences.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_compress_superblock.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_compress.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_double_fast.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_fast.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_lazy.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_ldm.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_opt.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstd_preSplit.c"
		"${zstd_SOURCE_DIR}/lib/compress/zstdmt_compress.c"

		"${zstd_SOURCE_DIR}/lib/decompress/huf_decompress.c"
		"${zstd_SOURCE_DIR}/lib/decompress/zstd_ddict.c"
		"${zstd_SOURCE_DIR}/lib/decompress/zstd_decompress_block.c"
		"${zstd_SOURCE_DIR}/lib/decompress/zstd_decompress.c")
target_compile_options(zstd PRIVATE
	$<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
	$<$<C_COMPILER_ID:Clang>:-Wno-sign-conversion>)
target_compile_definitions(zstd PRIVATE
	ZSTD_DISABLE_ASM
	$<$<C_COMPILER_ID:MSVC>:ZSTD_HEAPMODE=0 _CRT_SECURE_NO_WARNINGS>)
if(BUILD_SHARED_LIBS AND WIN32)
	target_compile_definitions(zstd
		INTERFACE ZSTD_DLL_IMPORT=1
		PRIVATE ZSTD_DLL_EXPORT=1)
endif()

set_target_properties(zstd PROPERTIES SYSTEM ON)

set(GREM_GRAPHICS_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES} zstd PARENT_SCOPE)
set(GREM_RESOURCE_PRIVATE_DEPENDENCIES ${GREM_RESOURCE_PRIVATE_DEPENDENCIES} zstd PARENT_SCOPE)
