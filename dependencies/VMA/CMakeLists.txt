message(STATUS "Configuring VMA...")

FetchContent_Declare(VMA
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
	GIT_TAG 3aa921224c154a0d2c43912bc88e1c42ce1f7607 # v3.4.0
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(VMA)

add_library(VMA STATIC EXCLUDE_FROM_ALL)
target_sources(VMA
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${vma_SOURCE_DIR}/include"
		FILES "${vma_SOURCE_DIR}/include/vk_mem_alloc.h"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/vk_mem_alloc.cpp")
target_link_libraries(VMA PRIVATE volk_headers)
target_compile_options(VMA PUBLIC $<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wno-nullability-completeness -Wno-unknown-warning-option>)
set_target_properties(VMA PROPERTIES SYSTEM ON)

set(GREM_GRAPHICS_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES} VMA PARENT_SCOPE)

file(READ "${vma_SOURCE_DIR}/LICENSE.txt" GREM_VMA_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_VMA_LICENSE_FILE_CONTENTS "${GREM_VMA_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(c\\) 2017-.... Advanced Micro Devices, Inc\\. All rights reserved\\." GREM_VMA_COPYRIGHT_FILE_CONTENTS "${GREM_VMA_LICENSE_FILE_CONTENTS}")

set(GREM_VMA_TITLE "Vulkan Memory Allocator (v3.4.0)" PARENT_SCOPE)
set(GREM_VMA_USE "Used by GREM." PARENT_SCOPE)
set(GREM_VMA_LINK "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/tree/3aa921224c154a0d2c43912bc88e1c42ce1f7607" PARENT_SCOPE)
set(GREM_VMA_COPYRIGHT "${GREM_VMA_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_VMA_LICENSE "${GREM_VMA_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_VMA
	PARENT_SCOPE)
