cmake_minimum_required(VERSION 3.28)
project("GREM-tests")

if(GREM_FETCH_DEPENDENCIES)
	FetchContent_Declare(Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2
		GIT_TAG 29c9844f688acb27c87338c39cd186ebfe41aa19 # v3.13.0
	)
	FetchContent_MakeAvailable(Catch2)
else()
	find_package(Catch2 REQUIRED)
endif()

add_library(GREM-tests-base INTERFACE)
target_link_libraries(GREM-tests-base INTERFACE GREM::GREM Catch2::Catch2WithMain)

function(GREM_configure_test TARGET_NAME)
	target_link_libraries("${TARGET_NAME}" PRIVATE GREM-tests-base)
	target_compile_options("${TARGET_NAME}" PRIVATE
		$<$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wconversion -pedantic -Werror>
		$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wshadow -Wno-shadow-field-in-constructor -Wno-c2y-extensions -Wno-unknown-warning-option>
		$<$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>:/W4 /permissive- /WX /wd4996>)
	set_target_properties("${TARGET_NAME}" PROPERTIES
		LINKER_LANGUAGE CXX
		CXX_SCAN_FOR_MODULES OFF
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

add_executable(GREM-tests-formatting "formatting.cpp")
GREM_configure_test(GREM-tests-formatting)
add_test(NAME GREM-tests-formatting COMMAND GREM-tests-formatting WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(GREM-tests-json "json.cpp")
GREM_configure_test(GREM-tests-json)
add_test(NAME GREM-tests-json COMMAND GREM-tests-json WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" GREM-tests-formatting GREM-tests-json)
