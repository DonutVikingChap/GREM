add_executable(GREM-shader-bootstrapper "bootstrapper.cpp")
target_link_libraries(GREM-shader-bootstrapper PRIVATE GREM::GREM)
set_target_properties(GREM-shader-bootstrapper PROPERTIES
	LINKER_LANGUAGE CXX
	CXX_SCAN_FOR_MODULES OFF
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

GREM_post_build_copy_dlls("${CMAKE_CURRENT_BINARY_DIR}" GREM-shader-bootstrapper)

add_custom_target(GREM-bootstrap-shaders ALL
	COMMAND GREM-shader-bootstrapper "${CMAKE_SOURCE_DIR}/shaders" "${CMAKE_SOURCE_DIR}/src"
	DEPENDS GREM-shader-bootstrapper
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	VERBATIM)
