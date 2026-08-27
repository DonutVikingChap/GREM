message(STATUS "Configuring SPIRV-Tools...")

set(ENABLE_RTTI ON)
set(SKIP_SPIRV_TOOLS_INSTALL ON)
set(SPIRV_WERROR OFF)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_TOOLS_USE_MIMALLOC OFF)
set(SPIRV_TOOLS_BUILD_STATIC ON)

add_compile_options($<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>:-Wno-array-bounds>)

FetchContent_Declare(SPIRV-Tools
	GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools
	GIT_TAG 9a49b0883b9b635689a85b5647dbfcb223268151 # vulkan-sdk-1.4.357.0
	EXCLUDE_FROM_ALL
	SYSTEM
	OVERRIDE_FIND_PACKAGE)

FetchContent_MakeAvailable(SPIRV-Tools)

file(READ "${spirv-tools_SOURCE_DIR}/LICENSE" GREM_SPIRV_TOOLS_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SPIRV_TOOLS_LICENSE_FILE_CONTENTS "${GREM_SPIRV_TOOLS_LICENSE_FILE_CONTENTS}")

set(GREM_SPIRV_TOOLS_TITLE "SPIRV-Tools (vulkan-sdk-1.4.357.0)" PARENT_SCOPE)
set(GREM_SPIRV_TOOLS_USE "Used by GREM and glslang." PARENT_SCOPE)
set(GREM_SPIRV_TOOLS_LINK "https://github.com/KhronosGroup/SPIRV-Tools/tree/9a49b0883b9b635689a85b5647dbfcb223268151" PARENT_SCOPE)
set(GREM_SPIRV_TOOLS_COPYRIGHT "\
Copyright (c) 2015-2016 The Khronos Group Inc.\n\
\n\
Licensed under the Apache License, Version 2.0 (the \"License\");\n\
you may not use this file except in compliance with the License.\n\
You may obtain a copy of the License at\n\
\n\
    http://www.apache.org/licenses/LICENSE-2.0\n\
\n\
Unless required by applicable law or agreed to in writing, software\n\
distributed under the License is distributed on an \"AS IS\" BASIS,\n\
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
See the License for the specific language governing permissions and\n\
limitations under the License.\n\
" PARENT_SCOPE)
set(GREM_SPIRV_TOOLS_LICENSE "${GREM_SPIRV_TOOLS_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_SPIRV_TOOLS
	PARENT_SCOPE)
