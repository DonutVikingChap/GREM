cmake_minimum_required(VERSION 3.28)
project("Your project name here") # TODO: Change project name.

set(APP_TARGET_NAME "app") # TODO: Change app name.

include(FetchContent)

# GREM library options (see GREM's CMakeLists.txt for the full list).
set(GREM_ENABLE_EXECUTION_MODULE ON)
set(GREM_ENABLE_RESOURCE_MODULE ON)
set(GREM_ENABLE_APPLICATION_MODULE ON)
set(GREM_ENABLE_AUDIO_MODULE ON)
set(GREM_ENABLE_EVENTS_MODULE ON)
set(GREM_ENABLE_GRAPHICS_MODULE ON)
set(GREM_ENABLE_GRAPHICS_2D_MODULE ON)
set(GREM_ENABLE_GRAPHICS_3D_MODULE ON)
set(GREM_ENABLE_IMGUI_MODULE ON)
set(GREM_ENABLE_NETWORKING_MODULE ON)
set(GREM_ENABLE_PHYSICS_MODULE ON)
set(GREM_GRAPHICS_BACKEND "OpenGL")

# Download and configure GREM.
FetchContent_Declare(GREM
    GIT_REPOSITORY https://github.com/DonutVikingChap/GREM
    GIT_TAG "origin/main" # TODO: Replace "origin/main" with the commit hash for a specific version.
)
FetchContent_MakeAvailable(GREM)

# Create the build target for the application executable.
add_executable("${APP_TARGET_NAME}"
	# TODO: Add source files here as your project grows.
	"src/main.cpp")

# Link to the GREM library.
target_link_libraries("${APP_TARGET_NAME}" PRIVATE
	GREM::GREM)

# Add compiler-specific warning flags, including warnings-as-errors (-Werror and /WX).
target_compile_options("${APP_TARGET_NAME}" PRIVATE
	# GCC/Clang
	$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wconversion -pedantic -Werror>
	# Clang
	$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wshadow -Wno-shadow-field-in-constructor>
	# MSVC
	$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/W4 /permissive- /WX /w15262 /wd4996>)

if(EMSCRIPTEN)
	# If we're building for web, set up the Emscripten shell and embed the data/ folder in the generated .html file.
	set_target_properties("${APP_TARGET_NAME}" PROPERTIES SUFFIX ".html")
	target_link_options("${APP_TARGET_NAME}" PRIVATE "--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data@data" "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/emscripten-shell.html")
endif()

set_target_properties("${APP_TARGET_NAME}" PROPERTIES
	LINKER_LANGUAGE CXX # The application uses C++.
	CXX_SCAN_FOR_MODULES OFF # We're not using C++20 modules, so skip the scanning step.
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# If we're using dynamic linking, copy the DLLs to the build folder after building.
GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" "${APP_TARGET_NAME}")

# Generate `.gdbinit` in the project root.
file(WRITE "${CMAKE_SOURCE_DIR}/.gdbinit" "${GREM_GDBINIT_SCRIPT}")

# Generate `ThirdPartyLegalNotices.md` in the build folder.
# The contents of GREM_LEGAL_NOTICES is provided by GREM as an illustrative example and does not constitute legal advice.
# Please review the generated file and compare it to your dependencies' respective licenses carefully before distributing the application.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" "\
# Third-party legal notices\n\
\n\
This file contains credits and copyright notices of third-party libraries used in this application.\n\
${GREM_LEGAL_NOTICES}")

# Configure how the application gets packaged by cmake --install.
install(TARGETS "${APP_TARGET_NAME}")
if(BUILD_SHARED_LIBS)
	# If we're using dynamic linking, make sure the DLLs get installed.
	install(IMPORTED_RUNTIME_ARTIFACTS "${APP_TARGET_NAME}" RUNTIME_DEPENDENCY_SET APP_DEPENDENCIES)
	install(RUNTIME_DEPENDENCY_SET APP_DEPENDENCIES POST_EXCLUDE_REGEXES "^/usr/lib/" "^/lib/" "system32/.*\\.dll$")
endif()
if(NOT EMSCRIPTEN)
	# Install the data/ folder next to the executable.
	install(DIRECTORY "data" TYPE BIN)
endif()
# Install `ThirdPartyLegalNotices.md` next to the executable.
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" TYPE BIN)
