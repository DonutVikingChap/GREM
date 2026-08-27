message(STATUS "Configuring libjpeg-turbo...")

FetchContent_Declare(libjpeg-turbo
	GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo
	GIT_TAG c85e6b905bf237038faa936dab160ebfc5da0344 # 3.2.0
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(libjpeg-turbo)

file(READ "${libjpeg-turbo_SOURCE_DIR}/README.ijg" GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS "${GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "The authors make.*assumed by the product vendor\\.\n" GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS "${GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS}")

set(GREM_LIBJPEG_TURBO_TITLE "libjpeg-turbo (3.2.0)" PARENT_SCOPE)
set(GREM_LIBJPEG_TURBO_USE "This software is based in part on the work of the Independent JPEG Group.\n\nUsed by GREM." PARENT_SCOPE)
set(GREM_LIBJPEG_TURBO_LINK "https://github.com/libjpeg-turbo/libjpeg-turbo/tree/c85e6b905bf237038faa936dab160ebfc5da0344" PARENT_SCOPE)
set(GREM_LIBJPEG_TURBO_COPYRIGHT "\
Copyright (C) 1991-2020, Thomas G. Lane, Guido Vollbeding.\n\
\n\
Copyright (C) 2009-2026 D. R. Commander." PARENT_SCOPE)
set(GREM_LIBJPEG_TURBO_LICENSE "${GREM_LIBJPEG_IJG_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_LIBJPEG_TURBO
	PARENT_SCOPE)

set(JPEG_LIB_VERSION 62)
set(VERSION 3.2.0)
set(LIBJPEG_TURBO_VERSION_NUMBER 3002000)
set(BUILD "20260630")
if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
	set(HIDDEN "__attribute__((visibility(\"hidden\")))")
endif()
set(INLINE "inline")
set(CMAKE_PROJECT_NAME "libjpeg-turbo")
set(SIMD_ARCHITECTURE NONE)
include(CheckTypeSize)
check_type_size("size_t" SIZE_T)
configure_file("${libjpeg-turbo_SOURCE_DIR}/src/jversion.h.in" jversion.h)
configure_file("${libjpeg-turbo_SOURCE_DIR}/src/jconfig.h.in" jconfig.h)
configure_file("${libjpeg-turbo_SOURCE_DIR}/src/jconfigint.h.in" jconfigint.h)

set(JPEG16_SOURCES
	"${libjpeg-turbo_SOURCE_DIR}/src/jcapistd.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jccolor.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcdiffct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jclossls.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcmainct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcprepct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcsample.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdapistd.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdcolor.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jddiffct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdlossls.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdmainct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdpostct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdsample.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jutils.c")

set(JPEG12_SOURCES
	${JPEG16_SOURCES}
	"${libjpeg-turbo_SOURCE_DIR}/src/jccoefct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcdctmgr.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdcoefct.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jddctmgr.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdmerge.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jfdctfst.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jfdctint.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jidctflt.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jidctfst.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jidctint.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jidctred.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jquant1.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jquant2.c")

set(JPEG_SOURCES
	${JPEG12_SOURCES}
	"${libjpeg-turbo_SOURCE_DIR}/src/jcapimin.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jchuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcicc.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcinit.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jclhuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcmarker.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcmaster.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcomapi.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcparam.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jcphuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jctrans.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdapimin.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdatadst.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdatasrc.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdhuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdicc.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdinput.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdlhuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdmarker.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdmaster.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdphuff.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jdtrans.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jerror.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jfdctflt.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jmemmgr.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jmemnobs.c"
	"${libjpeg-turbo_SOURCE_DIR}/src/jpeg_nbits.c")

add_library(jpeg12-static OBJECT ${JPEG12_SOURCES})
target_include_directories(jpeg12-static SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${libjpeg-turbo_SOURCE_DIR}/src")
target_compile_definitions(jpeg12-static PRIVATE NO_GETENV NO_PUTENV BITS_IN_JSAMPLE=12)
target_compile_options(jpeg12-static PRIVATE $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:-Wno-stringop-overflow -Wno-attributes>)
add_library(jpeg16-static OBJECT ${JPEG16_SOURCES})
target_include_directories(jpeg16-static SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${libjpeg-turbo_SOURCE_DIR}/src")
target_compile_definitions(jpeg16-static PRIVATE NO_GETENV NO_PUTENV BITS_IN_JSAMPLE=16)
target_compile_options(jpeg16-static PRIVATE $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:-Wno-stringop-overflow -Wno-attributes>)
add_library(jpeg-static STATIC EXCLUDE_FROM_ALL ${JPEG_SOURCES} $<TARGET_OBJECTS:jpeg12-static> $<TARGET_OBJECTS:jpeg16-static>)
target_include_directories(jpeg-static SYSTEM PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${libjpeg-turbo_SOURCE_DIR}/src")
target_compile_definitions(jpeg-static PRIVATE NO_GETENV NO_PUTENV)
target_compile_options(jpeg-static PRIVATE $<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:-Wno-stringop-overflow -Wno-attributes>)

set(GREM_RESOURCE_PRIVATE_DEPENDENCIES ${GREM_RESOURCE_PRIVATE_DEPENDENCIES} jpeg-static PARENT_SCOPE)
