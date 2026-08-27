cmake_minimum_required(VERSION 3.28)
project("GREM-examples")

add_library(GREM-examples-base INTERFACE)
target_link_libraries(GREM-examples-base INTERFACE GREM::GREM)

function(GREM_configure_example TARGET_NAME)
	target_link_libraries("${TARGET_NAME}" PRIVATE GREM-examples-base)
	target_compile_options("${TARGET_NAME}" PRIVATE
		$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wconversion -pedantic -Werror>
		$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wshadow -Wno-shadow-field-in-constructor>
		$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/W4 /permissive- /WX /w15262 /wd4996>)
	set_target_properties("${TARGET_NAME}" PROPERTIES
		LINKER_LANGUAGE CXX
		CXX_SCAN_FOR_MODULES OFF
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	if(BUILD_SHARED_LIBS)
		set_target_properties("${TARGET_NAME}" PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
	endif()
endfunction()

add_library(GREM-examples-fps-shared
	"fps/game_commands.cpp"
	"fps/game_events.cpp"
	"fps/game_map.cpp"
	"fps/GameState.cpp"
	"fps/Prefab.cpp"
	"fps/Schema.cpp"
	"fps/Snapshot.cpp")
GREM_configure_example(GREM-examples-fps-shared)
target_compile_definitions(GREM-examples-fps-shared
	INTERFACE "FPS_SHARED_API=${GREM_IMPORT_API}"
	PRIVATE "FPS_SHARED_API=${GREM_EXPORT_API}")
target_link_libraries(GREM-examples-fps-shared PUBLIC zstd)

add_executable(GREM-examples-fps
	"fps/GameClient.cpp"
	"fps/GameServer.cpp"
	"fps/GameSystems.cpp"
	"fps/main.cpp")
GREM_configure_example(GREM-examples-fps)
target_link_libraries(GREM-examples-fps PRIVATE GREM-examples-fps-shared)
if(BUILD_SHARED_LIBS)
	file(GLOB_RECURSE GREM_EXAMPLES_FPS_SYSTEM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fps/game_systems/**.cpp")
	foreach(GREM_EXAMPLES_FPS_SYSTEM_SOURCE ${GREM_EXAMPLES_FPS_SYSTEM_SOURCES})
		cmake_path(GET GREM_EXAMPLES_FPS_SYSTEM_SOURCE STEM GREM_EXAMPLES_FPS_SYSTEM_NAME)
		list(APPEND GREM_EXAMPLES_FPS_SYSTEMS "${GREM_EXAMPLES_FPS_SYSTEM_NAME}")
		add_library("${GREM_EXAMPLES_FPS_SYSTEM_NAME}" MODULE "${GREM_EXAMPLES_FPS_SYSTEM_SOURCE}")
		GREM_configure_example("${GREM_EXAMPLES_FPS_SYSTEM_NAME}")
		target_link_libraries("${GREM_EXAMPLES_FPS_SYSTEM_NAME}" PRIVATE GREM-examples-fps-shared)
		set_target_properties("${GREM_EXAMPLES_FPS_SYSTEM_NAME}" PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/fps/systems"
			LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/fps/systems"
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/fps/systems"
			PREFIX ""
			DEBUG_POSTFIX "")
		if(BUILD_SHARED_LIBS)
			set_target_properties("${GREM_EXAMPLES_FPS_SYSTEM_NAME}" PROPERTIES INSTALL_RPATH "$ORIGIN/../../../lib")
		endif()
		install(TARGETS "${GREM_EXAMPLES_FPS_SYSTEM_NAME}" DESTINATION "bin/data/fps/systems")
	endforeach()

	GREM_post_build_copy_dlls("${CMAKE_CURRENT_SOURCE_DIR}/data/fps/systems" ${GREM_EXAMPLES_FPS_SYSTEMS})
endif()
if(EMSCRIPTEN)
	set_target_properties(GREM-examples-fps PROPERTIES SUFFIX ".html")
	target_link_options(GREM-examples-fps PRIVATE
		"--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_2d@data/shared_2d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_3d@data/shared_3d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/fps@data/fps")
endif()

add_executable(GREM-examples-test_game
	"test_game/main.cpp")
GREM_configure_example(GREM-examples-test_game)
if(EMSCRIPTEN)
	set_target_properties(GREM-examples-test_game PROPERTIES SUFFIX ".html")
	target_link_options(GREM-examples-test_game PRIVATE
		"--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_2d@data/shared_2d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_3d@data/shared_3d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/test_game@data/test_game")
endif()

add_executable(GREM-examples-physics
	"physics/main.cpp")
GREM_configure_example(GREM-examples-physics)
if(EMSCRIPTEN)
	set_target_properties(GREM-examples-physics PROPERTIES SUFFIX ".html")
	target_link_options(GREM-examples-physics PRIVATE
		"--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_2d@data/shared_2d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/physics@data/physics")
endif()

add_executable(GREM-examples-rectangle
	"rectangle/main.cpp")
GREM_configure_example(GREM-examples-rectangle)
if(EMSCRIPTEN)
	set_target_properties(GREM-examples-rectangle PROPERTIES SUFFIX ".html")
	target_link_options(GREM-examples-rectangle PRIVATE
		"--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html")
endif()

add_executable(GREM-examples-tiles
	"tiles/main.cpp")
GREM_configure_example(GREM-examples-tiles)
if(EMSCRIPTEN)
	set_target_properties(GREM-examples-tiles PROPERTIES SUFFIX ".html")
	target_link_options(GREM-examples-tiles PRIVATE
		"--shell-file=${CMAKE_SOURCE_DIR}/template/emscripten-shell.html"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/shared_2d@data/shared_2d"
		"--embed-file=${CMAKE_CURRENT_SOURCE_DIR}/data/tiles@data/tiles")
endif()

set(GREM_EXAMPLES
	GREM-examples-fps
	GREM-examples-test_game
	GREM-examples-physics
	GREM-examples-rectangle
	GREM-examples-tiles)

install(TARGETS ${GREM_EXAMPLES} DESTINATION "bin")
if(BUILD_SHARED_LIBS)
	install(IMPORTED_RUNTIME_ARTIFACTS ${GREM_EXAMPLES} RUNTIME_DEPENDENCY_SET GREM_EXAMPLES_DEPENDENCIES)
	install(RUNTIME_DEPENDENCY_SET GREM_EXAMPLES_DEPENDENCIES POST_EXCLUDE_REGEXES "^/usr/lib/" "^/lib/" "system32/.*\\.dll$")
endif()
if(NOT EMSCRIPTEN)
	install(DIRECTORY "data" DESTINATION "bin" FILES_MATCHING
		PATTERN "*"
		PATTERN "systems" EXCLUDE
		PATTERN "systems/*" EXCLUDE)
endif()

if(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan" AND GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
	function(GREM_configure_example_shader_compiler TARGET_NAME COMPILE_TARGET_NAME SHADER_DIR)
		GREM_configure_example("${TARGET_NAME}")
		file(GLOB GREM_EXAMPLE_SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")
		set(GREM_EXAMPLE_SHADER_OUTPUTS "")
		foreach(SHADER_FILEPATH ${GREM_EXAMPLE_SHADERS})
			list(APPEND GREM_EXAMPLE_SHADER_OUTPUTS "${SHADER_FILEPATH}.spv")
		endforeach()
		add_custom_command(
			OUTPUT ${GREM_EXAMPLE_SHADER_OUTPUTS}
			COMMAND "${TARGET_NAME}" "${SHADER_DIR}"
			DEPENDS "${TARGET_NAME}" ${GREM_EXAMPLE_SHADERS}
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			VERBATIM)
		add_custom_target("${COMPILE_TARGET_NAME}" ALL DEPENDS ${GREM_EXAMPLE_SHADER_OUTPUTS})
	endfunction()

	add_executable(GREM-examples-fps-shader-compiler "fps/shader_compiler.cpp")
	GREM_configure_example_shader_compiler(GREM-examples-fps-shader-compiler GREM-examples-fps-compile-shaders "${CMAKE_CURRENT_SOURCE_DIR}/data/fps/shaders")

	add_executable(GREM-examples-test_game-shader-compiler "test_game/shader_compiler.cpp")
	GREM_configure_example_shader_compiler(GREM-examples-test_game-shader-compiler GREM-examples-test_game-compile-shaders "${CMAKE_CURRENT_SOURCE_DIR}/data/test_game/shaders")

	add_executable(GREM-examples-tiles-shader-compiler "tiles/shader_compiler.cpp")
	GREM_configure_example_shader_compiler(GREM-examples-tiles-shader-compiler GREM-examples-tiles-compile-shaders "${CMAKE_CURRENT_SOURCE_DIR}/data/tiles/shaders")
endif()

GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" ${GREM_EXAMPLES})
