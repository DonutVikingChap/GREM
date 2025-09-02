cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
project("libdonut"
	VERSION 2.3.6
	DESCRIPTION "Application framework for cross-platform game development in C++20"
	LANGUAGES C CXX)

include(CMakeDependentOption)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(DONUT_ENABLE_LIBRARY "Enable building the library" ON)
cmake_dependent_option(DONUT_ENABLE_EXAMPLES "Enable building examples" ${PROJECT_IS_TOP_LEVEL} "DONUT_ENABLE_LIBRARY" OFF)
cmake_dependent_option(DONUT_ENABLE_TESTING "Enable test suite" ${PROJECT_IS_TOP_LEVEL} "DONUT_ENABLE_LIBRARY" OFF)
option(DONUT_ENABLE_DOCUMENTATION "Enable generation of documentation using Doxygen" ${PROJECT_IS_TOP_LEVEL})

if(DONUT_ENABLE_LIBRARY)
	add_subdirectory(dependencies)

	add_library(donut
		"include/donut/application/Application.hpp"
		"include/donut/application/FrameInfo.hpp"
		"include/donut/application/TickInfo.hpp"

		"include/donut/audio/Error.hpp"
		"include/donut/audio/Listener.hpp"
		"include/donut/audio/Sound.hpp"
		"include/donut/audio/SoundStage.hpp"

		"include/donut/events/Error.hpp"
		"include/donut/events/Event.hpp"
		"include/donut/events/EventPump.hpp"
		"include/donut/events/Input.hpp"
		"include/donut/events/InputManager.hpp"
		"include/donut/events/MessageBox.hpp"

		"include/donut/graphics/Buffer.hpp"
		"include/donut/graphics/Camera.hpp"
		"include/donut/graphics/Error.hpp"
		"include/donut/graphics/Font.hpp"
		"include/donut/graphics/Framebuffer.hpp"
		"include/donut/graphics/Handle.hpp"
		"include/donut/graphics/Image.hpp"
		"include/donut/graphics/Mesh.hpp"
		"include/donut/graphics/Model.hpp"
		"include/donut/graphics/opengl.hpp"
		"include/donut/graphics/Renderer.hpp"
		"include/donut/graphics/RenderPass.hpp"
		"include/donut/graphics/Shader2D.hpp"
		"include/donut/graphics/Shader3D.hpp"
		"include/donut/graphics/ShaderConfiguration.hpp"
		"include/donut/graphics/ShaderParameter.hpp"
		"include/donut/graphics/ShaderProgram.hpp"
		"include/donut/graphics/ShaderStage.hpp"
		"include/donut/graphics/SpriteAtlas.hpp"
		"include/donut/graphics/Text.hpp"
		"include/donut/graphics/Texture.hpp"
		"include/donut/graphics/TexturedQuad.hpp"
		"include/donut/graphics/VertexArray.hpp"
		"include/donut/graphics/Viewport.hpp"
		"include/donut/graphics/Window.hpp"

		"include/donut/modules/fwd/application.hpp"
		"include/donut/modules/fwd/audio.hpp"
		"include/donut/modules/fwd/events.hpp"
		"include/donut/modules/fwd/graphics.hpp"
		"include/donut/modules/fwd/utilities.hpp"

		"include/donut/modules/application.hpp"
		"include/donut/modules/audio.hpp"
		"include/donut/modules/events.hpp"
		"include/donut/modules/graphics.hpp"
		"include/donut/modules/utilities.hpp"

		"include/donut/aliases.hpp"
		"include/donut/AtlasPacker.hpp"
		"include/donut/base64.hpp"
		"include/donut/Color.hpp"
		"include/donut/donut.hpp"
		"include/donut/File.hpp"
		"include/donut/Filesystem.hpp"
		"include/donut/json.hpp"
		"include/donut/LinearAllocator.hpp"
		"include/donut/LinearBuffer.hpp"
		"include/donut/LooseQuadtree.hpp"
		"include/donut/math.hpp"
		"include/donut/obj.hpp"
		"include/donut/Overloaded.hpp"
		"include/donut/random.hpp"
		"include/donut/reflection.hpp"
		"include/donut/shapes.hpp"
		"include/donut/Time.hpp"
		"include/donut/unicode.hpp"
		"include/donut/UniqueHandle.hpp"
		"include/donut/Variant.hpp"
		"include/donut/xml.hpp"
		
		"src/application/Application.cpp"

		"src/audio/Error.cpp"
		"src/audio/Sound.cpp"
		"src/audio/SoundStage.cpp"

		"src/events/EventPump.cpp"
		"src/events/InputManager.cpp"
		"src/events/MessageBox.cpp"

		"src/graphics/Buffer.cpp"
		"src/graphics/Font.cpp"
		"src/graphics/Framebuffer.cpp"
		"src/graphics/Image.cpp"
		"src/graphics/Mesh.cpp"
		"src/graphics/Model.cpp"
		"src/graphics/Renderer.cpp"
		"src/graphics/RenderPass.cpp"
		"src/graphics/Shader2D.cpp"
		"src/graphics/Shader3D.cpp"
		"src/graphics/ShaderParameter.cpp"
		"src/graphics/ShaderProgram.cpp"
		"src/graphics/ShaderStage.cpp"
		"src/graphics/Text.cpp"
		"src/graphics/Texture.cpp"
		"src/graphics/VertexArray.cpp"
		"src/graphics/Window.cpp"

		"src/base64.cpp"
		"src/File.cpp"
		"src/Filesystem.cpp"
		"src/obj.cpp"
		"src/xml.cpp")

	add_library(donut::donut ALIAS donut)

	target_include_directories(donut PUBLIC
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

	target_compile_features(donut PUBLIC cxx_std_20)

	target_compile_options(donut
		PUBLIC
			$<$<CXX_COMPILER_ID:GNU>:   -std=c++20>
			$<$<CXX_COMPILER_ID:Clang>: -std=c++20>
			$<$<CXX_COMPILER_ID:MSVC>:  /std:c++20  /utf-8>
		PRIVATE
			$<$<CXX_COMPILER_ID:GNU>:   -Wall   -Wextra -Wconversion    -Wpedantic              $<$<CONFIG:Debug>:-g3>  $<$<CONFIG:Release>:-O3>  $<$<CONFIG:MinSizeRel>:-Os>   $<$<CONFIG:RelWithDebInfo>:-O3 -g3>>
			$<$<CXX_COMPILER_ID:Clang>: -Wall   -Wextra -Wconversion    -Wpedantic              $<$<CONFIG:Debug>:-g3>  $<$<CONFIG:Release>:-O3>  $<$<CONFIG:MinSizeRel>:-Os>   $<$<CONFIG:RelWithDebInfo>:-O3 -g3>>
			$<$<CXX_COMPILER_ID:MSVC>:  /W4                             /permissive-    /wd4996 $<$<CONFIG:Debug>:/Od>  $<$<CONFIG:Release>:/Ot>  $<$<CONFIG:MinSizeRel>:/Os>   $<$<CONFIG:RelWithDebInfo>:/Ot /Od>>)

	target_link_libraries(donut
		INTERFACE
			${DONUT_INTERFACE_DEPENDENCIES}
		PUBLIC
			${DONUT_PUBLIC_DEPENDENCIES}
		PRIVATE
			${DONUT_PRIVATE_DEPENDENCIES})

	if(BUILD_SHARED_LIBS)
		target_link_libraries(donut PRIVATE ${CMAKE_DL_LIBS})
	endif()

	include(GNUInstallDirs)

	set_target_properties(donut PROPERTIES
		SOVERSION "${PROJECT_VERSION_MAJOR}"
		VERSION "${PROJECT_VERSION}"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_INSTALL_LIBDIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_INSTALL_LIBDIR}"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_INSTALL_BINDIR}")

	if(DONUT_ENABLE_EXAMPLES)
		add_subdirectory(examples)
	endif()

	if(DONUT_ENABLE_TESTING)
		enable_testing()
		add_subdirectory(test)
	endif()
endif()

if(DONUT_ENABLE_DOCUMENTATION)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" COPYONLY)
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" "\nPROJECT_NUMBER = \"${CMAKE_PROJECT_VERSION}\"\nOUTPUT_DIRECTORY = \"${CMAKE_CURRENT_BINARY_DIR}/docs\"\nINPUT += \"${CMAKE_CURRENT_SOURCE_DIR}/include\"\n")
        add_custom_target(donut-generate-documentation
            ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating documentation.")
    else()
        message(WARNING "Doxygen package not found; documentation will not be generated.")
    endif()
endif()
