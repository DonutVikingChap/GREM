message(STATUS "Configuring Dear ImGui...")

FetchContent_Declare(imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG b48d1afbe8ee8b238e2961dc363a949dd7304e23 # v1.92.9b-docking
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(imgui)

file(READ "${imgui_SOURCE_DIR}/LICENSE.txt" GREM_IMGUI_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_IMGUI_LICENSE_FILE_CONTENTS "${GREM_IMGUI_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(c\\) 2014-.... Omar Cornut" GREM_IMGUI_COPYRIGHT_FILE_CONTENTS "${GREM_IMGUI_LICENSE_FILE_CONTENTS}")

set(GREM_IMGUI_TITLE "Dear ImGui (v1.92.9b-docking)" PARENT_SCOPE)
set(GREM_IMGUI_USE "Used by GREM." PARENT_SCOPE)
set(GREM_IMGUI_LINK "https://github.com/ocornut/imgui/tree/b48d1afbe8ee8b238e2961dc363a949dd7304e23" PARENT_SCOPE)
set(GREM_IMGUI_COPYRIGHT "${GREM_IMGUI_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_IMGUI_LICENSE "${GREM_IMGUI_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_IMGUI_PROGGY_CLEAN_TITLE "ProggyClean" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_CLEAN_USE "Used by Dear ImGui." PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_CLEAN_LINK "https://github.com/bluescan/proggyfonts" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_CLEAN_COPYRIGHT "Copyright (c) 2004, 2005 Tristan Grimmer" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_CLEAN_LICENSE "\
MIT License\n\
\n\
Copyright (c) 2004, 2005 Tristan Grimmer\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_IMGUI_PROGGY_FOREVER_TITLE "ProggyForever" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_FOREVER_USE "Used by Dear ImGui." PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_FOREVER_LINK "https://github.com/ocornut/proggyforever" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_FOREVER_COPYRIGHT "\
Copyright (c) 2026 Disco Hello\n\
Copyright (c) 2019,2023 Tristan Grimmer" PARENT_SCOPE)
set(GREM_IMGUI_PROGGY_FOREVER_LICENSE "\
The MIT License (MIT)\n\
\n\
Copyright (c) 2026 Disco Hello\n\
Copyright (c) 2019,2023 Tristan Grimmer\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_IMGUI_STB_TITLE "stb_rect_pack, stb_textedit and stb_truetype" PARENT_SCOPE)
set(GREM_IMGUI_STB_USE "Used by Dear ImGui." PARENT_SCOPE)
set(GREM_IMGUI_STB_LINK "https://github.com/nothings/stb" PARENT_SCOPE)
set(GREM_IMGUI_STB_CREDITS "Sean Barrett" PARENT_SCOPE)
set(GREM_IMGUI_STB_LICENSE "\
This is free and unencumbered software released into the public domain.\n\
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n\
software, either in source code form or as a compiled binary, for any purpose,\n\
commercial or non-commercial, and by any means.\n\
In jurisdictions that recognize copyright laws, the author or authors of this\n\
software dedicate any and all copyright interest in the software to the public\n\
domain. We make this dedication for the benefit of the public at large and to\n\
the detriment of our heirs and successors. We intend this dedication to be an\n\
overt act of relinquishment in perpetuity of all present and future rights to\n\
this software under copyright law.\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n\
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n\
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_IMGUI
	GREM_IMGUI_PROGGY_CLEAN
	GREM_IMGUI_PROGGY_FOREVER
	GREM_IMGUI_STB
	PARENT_SCOPE)

add_library(imgui EXCLUDE_FROM_ALL)
target_sources(imgui
	PUBLIC FILE_SET HEADERS
		BASE_DIRS
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${imgui_SOURCE_DIR}"
		FILES
			"${CMAKE_CURRENT_SOURCE_DIR}/GREM_imgui_config.hpp"
			"${imgui_SOURCE_DIR}/imconfig.h"
			"${imgui_SOURCE_DIR}/imgui_internal.h"
			"${imgui_SOURCE_DIR}/imgui.h"
			"${imgui_SOURCE_DIR}/imstb_rectpack.h"
			"${imgui_SOURCE_DIR}/imstb_textedit.h"
			"${imgui_SOURCE_DIR}/imstb_truetype.h"
	PRIVATE
		"${imgui_SOURCE_DIR}/imgui_demo.cpp"
		"${imgui_SOURCE_DIR}/imgui_draw.cpp"
		"${imgui_SOURCE_DIR}/imgui_tables.cpp"
		"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
		"${imgui_SOURCE_DIR}/imgui.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GREM_imgui_config.cpp")
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=<GREM_imgui_config.hpp>")
if(NOT GREM_IMGUI_ENABLE_OBSOLETE_FUNCTIONS)
	target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
endif()
if(NOT GREM_IMGUI_ENABLE_DEMO_WINDOWS)
	target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEMO_WINDOWS)
endif()
if(NOT GREM_IMGUI_ENABLE_DEBUG_TOOLS)
	target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEBUG_TOOLS)
endif()
if(NOT GREM_USE_SSE_INTRINSICS)
	target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_SSE)
endif()
if(BUILD_SHARED_LIBS AND WIN32)
	target_compile_definitions(imgui
		INTERFACE "IMGUI_API=__declspec(dllimport)"
		PRIVATE "IMGUI_API=__declspec(dllexport)")
endif()
set_target_properties(imgui PROPERTIES SYSTEM ON)

set(GREM_IMGUI_PUBLIC_DEPENDENCIES ${GREM_IMGUI_PUBLIC_DEPENDENCIES} imgui PARENT_SCOPE)
