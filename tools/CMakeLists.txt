cmake_minimum_required(VERSION 3.28)
project("GREM-tools")

add_executable(GREM-asset-viewer
	"asset_viewer/main.cpp")
target_link_libraries(GREM-asset-viewer PRIVATE
	GREM::core
	GREM::application
	GREM::audio
	GREM::events
	GREM::graphics
	GREM::graphics_2d
	GREM::graphics_3d
	GREM::imgui
	GREM::resource)
target_compile_options(GREM-asset-viewer PRIVATE
	$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wconversion -pedantic -Werror>
	$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wshadow -Wno-shadow-field-in-constructor>
	$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/W4 /permissive- /WX /wd4996>)
if(EMSCRIPTEN)
	set_target_properties(GREM-asset-viewer PROPERTIES SUFFIX ".html")
	target_link_options(GREM-asset-viewer PRIVATE "--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html")
endif()
set_target_properties(GREM-asset-viewer PROPERTIES
	LINKER_LANGUAGE CXX
	CXX_SCAN_FOR_MODULES OFF
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
if(BUILD_SHARED_LIBS)
	set_target_properties(GREM-asset-viewer PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()
GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" GREM-asset-viewer)

install(TARGETS GREM-asset-viewer DESTINATION "bin")
if(BUILD_SHARED_LIBS)
	install(IMPORTED_RUNTIME_ARTIFACTS GREM-asset-viewer RUNTIME_DEPENDENCY_SET GREM_ASSET_VIEWER_DEPENDENCIES)
	install(RUNTIME_DEPENDENCY_SET GREM_ASSET_VIEWER_DEPENDENCIES POST_EXCLUDE_REGEXES "^/usr/lib/" "^/lib/" "system32/.*\\.dll$")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" "\
# Third-party legal notices\n\
\n\
This file contains credits and copyright notices of third-party libraries used in this application.\n\
${GREM_LEGAL_NOTICES}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" TYPE BIN)
