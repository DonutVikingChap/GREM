cmake_minimum_required(VERSION 3.28)
project("GREM"
	VERSION 3.0.1
	DESCRIPTION "Game/Rendering Engine Modules for C++"
	LANGUAGES C CXX)

set(GREM_ABI_VERSION 3) # Incremented when ABI-incompatible changes are made.

include(CMakeDependentOption)

################################################################################
# Library options
################################################################################

# GREM can be customized in your project's CMakeLists.txt file, just before the
# commands that fetch/add the library, using `set` commands with the following
# syntax:
#
#   set(GREM_USE_PROFILING ON)
#   set(GREM_GRAPHICS_BACKEND "Vulkan")
#   # etc.
#
# The full list of options and default values is listed below, with syntax that
# can be read like this:
#
#   # Boolean ON/OFF option:
#   option(OPTION_NAME "Description" DEFAULT_VALUE)
#
#   # Boolean ON/OFF option that is only applicable under certain conditions:
#   cmake_dependent_option(OPTION_NAME "Description" DEFAULT_VALUE "CONDITION1;CONDITION2;ETC" FORCED_VALUE_IF_CONDITIONS_ARE_NOT_MET)
#
#   # Non-boolean option:
#   set(OPTION_NAME "DEFAULT_VALUE" CACHE VALUE_TYPE "Description")

# Dependencies
option(GREM_FETCH_DEPENDENCIES "Use FetchContent to download GREM's dependencies and set up the GREM_..._DEPENDENCIES variables" ON)

# Build type
option(GREM_BUILD_LIBS "Build the GREM libraries" ON)

# Modules to enable
cmake_dependent_option(GREM_ENABLE_EXECUTION_MODULE "Enable the execution module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_RESOURCE_MODULE "Enable the resource module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_APPLICATION_MODULE "Enable the application module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_AUDIO_MODULE "Enable the audio module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_EVENTS_MODULE "Enable the events module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_GRAPHICS_MODULE "Enable the graphics module" ON "GREM_BUILD_LIBS;GREM_ENABLE_RESOURCE_MODULE" OFF)
cmake_dependent_option(GREM_ENABLE_GRAPHICS_2D_MODULE "Enable the graphics_2d module" ON "GREM_BUILD_LIBS;GREM_ENABLE_RESOURCE_MODULE;GREM_ENABLE_GRAPHICS_MODULE" OFF)
cmake_dependent_option(GREM_ENABLE_GRAPHICS_3D_MODULE "Enable the graphics_3d module" ON "GREM_BUILD_LIBS;GREM_ENABLE_RESOURCE_MODULE;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE" OFF)
cmake_dependent_option(GREM_ENABLE_IMGUI_MODULE "Enable the imgui module" ON "GREM_BUILD_LIBS;GREM_ENABLE_EVENTS_MODULE;GREM_ENABLE_RESOURCE_MODULE;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE" OFF)
cmake_dependent_option(GREM_ENABLE_NETWORKING_MODULE "Enable the networking module" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_ENABLE_PHYSICS_MODULE "Enable the physics module" ON "GREM_BUILD_LIBS;GREM_ENABLE_EXECUTION_MODULE" OFF)

# General
cmake_dependent_option(GREM_USE_MULTITHREADING "Use multithreading functionality when available" ON "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_USE_SSE_INTRINSICS "Enable explicit use of SSE SIMD intrinsics for vectorization when available" ON "GREM_BUILD_LIBS;NOT EMSCRIPTEN" OFF)
cmake_dependent_option(GREM_USE_PROFILING "Enable performance profiling functionality" OFF "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_USE_RELEASE_ASSERTIONS "Enable basic assertion checks in both debug and NDEBUG (non-debug) builds" OFF "GREM_BUILD_LIBS" OFF)
cmake_dependent_option(GREM_RELEASE_ASSERTIONS_FAST_FAIL "When release assertions are ON, use a quick, code-light method of program termination (usually a trap instruction) on assertion violation, making them less useful for debugging, but reducing overhead significantly when the main goal is UB safety" OFF "GREM_BUILD_LIBS;GREM_USE_RELEASE_ASSERTIONS" OFF)
cmake_dependent_option(GREM_ASSUME_UNCHECKED_ASSERTIONS_NEVER_FAIL "When release assertions are OFF, assume that the unchecked assertions in NDEBUG (non-debug) release builds are never violated, and intentionally treat any code paths that lead to violations as undefined behavior, potentially enabling more compiler optimizations, at the cost of UB safety" ON "GREM_BUILD_LIBS;NOT GREM_USE_RELEASE_ASSERTIONS" OFF)

# Graphics
set(GREM_GRAPHICS_BACKEND "OpenGL" CACHE STRING "Backend to use for the graphics module (OpenGL or Vulkan)")
cmake_dependent_option(GREM_GRAPHICS_OPENGL_USE_ES_PROFILE "Use the OpenGL ES profile in the OpenGL backend instead of the Core profile" OFF "GREM_BUILD_LIBS;GREM_ENABLE_GRAPHICS_MODULE;GREM_GRAPHICS_BACKEND STREQUAL OpenGL" OFF)
cmake_dependent_option(GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION "Enable GLSL shader compilation in the Vulkan backend (NOTE: requires additional dependencies, see README.md)" OFF "GREM_BUILD_LIBS;GREM_ENABLE_GRAPHICS_MODULE;GREM_GRAPHICS_BACKEND STREQUAL Vulkan" OFF)

# IMGUI
cmake_dependent_option(GREM_IMGUI_ENABLE_OBSOLETE_FUNCTIONS "Support obsolete ImGui functions" OFF "GREM_FETCH_DEPENDENCIES;GREM_BUILD_LIBS;GREM_ENABLE_IMGUI_MODULE" OFF)
cmake_dependent_option(GREM_IMGUI_ENABLE_DEMO_WINDOWS "Support ImGui::ShowDemoWindow() and ImGui::ShowStyleEditor()" ON "GREM_FETCH_DEPENDENCIES;GREM_BUILD_LIBS;GREM_ENABLE_IMGUI_MODULE" OFF)
cmake_dependent_option(GREM_IMGUI_ENABLE_DEBUG_TOOLS "Support ImGui::ShowMetricsWindow(), ImGui::ShowDebugLogWindow() and ImGui::ShowIDStackToolWindow()" ON "GREM_FETCH_DEPENDENCIES;GREM_BUILD_LIBS;GREM_ENABLE_IMGUI_MODULE" OFF)

# Physics
cmake_dependent_option(GREM_PHYSICS_USE_DEBUG_VISUALIZATION "Enable physics debug visualization" ON "GREM_BUILD_LIBS;GREM_ENABLE_PHYSICS_MODULE;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE;GREM_ENABLE_GRAPHICS_3D_MODULE" OFF)

# Networking
set(GREM_NETWORKING_SOCKETS_BACKEND "Native" CACHE STRING "Backend to use for sockets in the networking module (Native or Emulated)")

# Extras
option(GREM_BUILD_DOCS "Build the GREM documentation using Doxygen" OFF)
cmake_dependent_option(GREM_BUILD_EXAMPLES "Build the GREM example applications" ${PROJECT_IS_TOP_LEVEL} "GREM_BUILD_LIBS;GREM_ENABLE_APPLICATION_MODULE;GREM_ENABLE_AUDIO_MODULE;GREM_ENABLE_EVENTS_MODULE;GREM_ENABLE_EXECUTION_MODULE;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE;GREM_ENABLE_GRAPHICS_3D_MODULE;GREM_ENABLE_NETWORKING_MODULE;GREM_ENABLE_PHYSICS_MODULE;GREM_ENABLE_IMGUI_MODULE;GREM_ENABLE_RESOURCE_MODULE" OFF)
cmake_dependent_option(GREM_BUILD_TOOLS "Build the GREM tool programs" ${PROJECT_IS_TOP_LEVEL} "GREM_BUILD_LIBS;GREM_ENABLE_APPLICATION_MODULE;GREM_ENABLE_AUDIO_MODULE;GREM_ENABLE_EVENTS_MODULE;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE;GREM_ENABLE_GRAPHICS_3D_MODULE;GREM_ENABLE_IMGUI_MODULE;GREM_ENABLE_RESOURCE_MODULE" OFF)
cmake_dependent_option(GREM_BUILD_TESTS "Build the GREM test suite" ${PROJECT_IS_TOP_LEVEL} "GREM_BUILD_LIBS;NOT EMSCRIPTEN" OFF)
cmake_dependent_option(GREM_GENERATE_GDBINIT "Generate a .gdbinit file for debugging with GDB at the project root" ${PROJECT_IS_TOP_LEVEL} "GREM_BUILD_LIBS;NOT EMSCRIPTEN" OFF)

################################################################################

# Advanced options for GREM maintainers:
cmake_dependent_option(GREM_BUILD_SHADER_BOOTSTRAPPER "Enable the shader bootstrapper (for recompiling the built-in shaders in the graphics_2d/graphics_3d/imgui modules)" OFF "GREM_BUILD_LIBS;GREM_ENABLE_GRAPHICS_MODULE;GREM_ENABLE_GRAPHICS_2D_MODULE;GREM_ENABLE_GRAPHICS_3D_MODULE;GREM_ENABLE_IMGUI_MODULE;GREM_ENABLE_RESOURCE_MODULE;GREM_GRAPHICS_BACKEND STREQUAL Vulkan;GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION;NOT EMSCRIPTEN" OFF)
cmake_dependent_option(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS "Use the unoptimized, debuggable versions of the built-in shaders instead of the optimized ones (for shader debugging in RenderDoc, etc.)" OFF "GREM_BUILD_LIBS;GREM_ENABLE_GRAPHICS_MODULE;GREM_GRAPHICS_BACKEND STREQUAL Vulkan" OFF)
mark_as_advanced(GREM_BUILD_SHADER_BOOTSTRAPPER)
mark_as_advanced(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)

################################################################################

if(NOT GREM_GRAPHICS_BACKEND MATCHES "^(OpenGL|Vulkan)$")
	message(FATAL_ERROR "Invalid GREM_GRAPHICS_BACKEND value (expected \"OpenGL\" or \"Vulkan\").")
endif()

if(NOT GREM_NETWORKING_SOCKETS_BACKEND MATCHES "^(Native|Emulated)$")
	message(FATAL_ERROR "Invalid GREM_NETWORKING_SOCKETS_BACKEND value (expected \"Native\" or \"Emulated\").")
endif()

if(EMSCRIPTEN)
	if(NOT GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
		message(FATAL_ERROR "Invalid GREM_GRAPHICS_BACKEND value for emscripten build (expected \"OpenGL\", currently \"${GREM_GRAPHICS_BACKEND}\").")
	endif()

	if(NOT GREM_GRAPHICS_OPENGL_USE_ES_PROFILE)
		message(FATAL_ERROR "Invalid GREM_GRAPHICS_OPENGL_USE_ES_PROFILE value for emscripten build (expected \"ON\", currently \"${GREM_GRAPHICS_OPENGL_USE_ES_PROFILE}\").")
	endif()

	if(NOT GREM_NETWORKING_SOCKETS_BACKEND STREQUAL "Emulated")
		message(FATAL_ERROR "Invalid GREM_NETWORKING_SOCKETS_BACKEND value for emscripten build (expected \"Emulated\", currently \"${GREM_NETWORKING_SOCKETS_BACKEND}\").")
	endif()
endif()

if(NOT DEFINED GREM_IMPORT_API)
	if(BUILD_SHARED_LIBS)
		if(WIN32)
			set(GREM_IMPORT_API "__declspec(dllimport)")
		elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
			set(GREM_IMPORT_API "__attribute__((visibility(\"default\"))")
		else()
			set(GREM_IMPORT_API "")
		endif()
	else()
		set(GREM_IMPORT_API "")
	endif()
endif()

if(NOT DEFINED GREM_EXPORT_API)
	if(BUILD_SHARED_LIBS)
		if(WIN32)
			set(GREM_EXPORT_API "__declspec(dllexport)")
		elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
			set(GREM_EXPORT_API "__attribute__((visibility(\"default\"))")
		else()
			set(GREM_EXPORT_API "")
		endif()
	else()
		set(GREM_EXPORT_API "")
	endif()
endif()

if(GREM_BUILD_LIBS)
	if(GREM_FETCH_DEPENDENCIES)
		include(FetchContent)
		if(GREM_ENABLE_RESOURCE_MODULE OR GREM_ENABLE_GRAPHICS_MODULE)
			add_subdirectory(dependencies/zstd)
		endif()
		if(GREM_ENABLE_RESOURCE_MODULE)
			add_subdirectory(dependencies/basisu) # Depends on zstd.
			add_subdirectory(dependencies/libjpeg-turbo)
		endif()
		if(GREM_ENABLE_APPLICATION_MODULE)
			add_subdirectory(dependencies/physfs)
		endif()
		if(GREM_ENABLE_GRAPHICS_MODULE)
			if(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
				add_subdirectory(dependencies/Vulkan-Headers)
				add_subdirectory(dependencies/volk) # Depends on Vulkan-Headers.
				add_subdirectory(dependencies/VMA) # Depends on volk.
				if(GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
					add_subdirectory(dependencies/SPIRV-Headers)
					add_subdirectory(dependencies/SPIRV-Tools) # Depends on SPIRV-Headers.
					add_subdirectory(dependencies/glslang) # Depends on SPIRV-Tools.
				endif()
			endif()
		endif()
		if(GREM_ENABLE_GRAPHICS_2D_MODULE)
			add_subdirectory(dependencies/libschrift)
		endif()
		if(GREM_ENABLE_IMGUI_MODULE)
			add_subdirectory(dependencies/imgui)
		endif()
		if(GREM_ENABLE_AUDIO_MODULE OR GREM_ENABLE_EVENTS_MODULE OR GREM_ENABLE_GRAPHICS_MODULE)
			add_subdirectory(dependencies/SDL)
		endif()
		if(GREM_ENABLE_AUDIO_MODULE)
			add_subdirectory(dependencies/SoLoud) # Depends on SDL.
		endif()
	endif()

	message(STATUS "Configuring GREM...")

	add_library(GREM-config INTERFACE)
	add_library(GREM::config ALIAS GREM-config)
	target_sources(GREM-config
		INTERFACE FILE_SET HEADERS
			BASE_DIRS "include"
			FILES "include/GREM/build_config.hpp")
	if(BUILD_SHARED_LIBS)
		target_compile_definitions(GREM-config INTERFACE GREM_SHARED_LIBRARY)
	endif()
	if(GREM_USE_MULTITHREADING)
		target_compile_definitions(GREM-config INTERFACE GREM_USE_MULTITHREADING)
	endif()
	if(GREM_USE_SSE_INTRINSICS)
		target_compile_definitions(GREM-config INTERFACE GREM_USE_SSE_INTRINSICS)
	endif()
	if(GREM_USE_PROFILING)
		target_compile_definitions(GREM-config INTERFACE GREM_USE_PROFILING)
	endif()
	if(GREM_USE_RELEASE_ASSERTIONS)
		target_compile_definitions(GREM-config INTERFACE GREM_USE_RELEASE_ASSERTIONS)
		if(GREM_RELEASE_ASSERTIONS_FAST_FAIL)
			target_compile_definitions(GREM-config INTERFACE GREM_RELEASE_ASSERTIONS_FAST_FAIL)
		endif()
	endif()

	list(APPEND GREM_MODULES GREM-core)
	list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_CORE_PUBLIC_DEPENDENCIES})
	list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_CORE_PRIVATE_DEPENDENCIES})
	add_library(GREM-core)
	add_library(GREM::core ALIAS GREM-core)
	target_sources(GREM-core
		PUBLIC FILE_SET HEADERS
			BASE_DIRS "include"
			FILES
				"include/GREM/core.hpp"

				"include/GREM/core/data/Allocation.hpp"
				"include/GREM/core/data/Any.hpp"
				"include/GREM/core/data/Arena.hpp"
				"include/GREM/core/data/Array.hpp"
				"include/GREM/core/data/ArrayList.hpp"
				"include/GREM/core/data/BitArray.hpp"
				"include/GREM/core/data/BitBuffer.hpp"
				"include/GREM/core/data/Buffer.hpp"
				"include/GREM/core/data/Color.hpp"
				"include/GREM/core/data/ConstantString.hpp"
				"include/GREM/core/data/ConvexPolytope.hpp"
				"include/GREM/core/data/CStringView.hpp"
				"include/GREM/core/data/DoubleEndedQueue.hpp"
				"include/GREM/core/data/DoublyLinkedList.hpp"
				"include/GREM/core/data/Function.hpp"
				"include/GREM/core/data/FunctionView.hpp"
				"include/GREM/core/data/HashMap.hpp"
				"include/GREM/core/data/HashSet.hpp"
				"include/GREM/core/data/Indirect.hpp"
				"include/GREM/core/data/InplaceArrayList.hpp"
				"include/GREM/core/data/InplaceBuffer.hpp"
				"include/GREM/core/data/InplaceDoubleEndedQueue.hpp"
				"include/GREM/core/data/InplaceRingBuffer.hpp"
				"include/GREM/core/data/LinearBoundingVolumeHierarchy.hpp"
				"include/GREM/core/data/LinearBuffer.hpp"
				"include/GREM/core/data/LooseOrthtree.hpp"
				"include/GREM/core/data/Optional.hpp"
				"include/GREM/core/data/OrderedMap.hpp"
				"include/GREM/core/data/OrderedMultimap.hpp"
				"include/GREM/core/data/Pair.hpp"
				"include/GREM/core/data/PriorityQueue.hpp"
				"include/GREM/core/data/MPMCQueue.hpp"
				"include/GREM/core/data/RangeAllocator.hpp"
				"include/GREM/core/data/Reader.hpp"
				"include/GREM/core/data/Registry.hpp"
				"include/GREM/core/data/RingBuffer.hpp"
				"include/GREM/core/data/SharedPointer.hpp"
				"include/GREM/core/data/SinglyLinkedList.hpp"
				"include/GREM/core/data/SmallArrayList.hpp"
				"include/GREM/core/data/SmallBuffer.hpp"
				"include/GREM/core/data/Span.hpp"
				"include/GREM/core/data/SPSCQueue.hpp"
				"include/GREM/core/data/SPSCSlidingWindowQueue.hpp"
				"include/GREM/core/data/SquareAllocator.hpp"
				"include/GREM/core/data/StridedSpan.hpp"
				"include/GREM/core/data/String.hpp"
				"include/GREM/core/data/StringPool.hpp"
				"include/GREM/core/data/StringView.hpp"
				"include/GREM/core/data/Subrange.hpp"
				"include/GREM/core/data/Table.hpp"
				"include/GREM/core/data/TriangleMesh.hpp"
				"include/GREM/core/data/Tuple.hpp"
				"include/GREM/core/data/UniqueHandle.hpp"
				"include/GREM/core/data/UniquePointer.hpp"
				"include/GREM/core/data/Variant.hpp"
				"include/GREM/core/data/Writer.hpp"

				"include/GREM/core/formats/Adler32.hpp"
				"include/GREM/core/formats/ascii.hpp"
				"include/GREM/core/formats/base16.hpp"
				"include/GREM/core/formats/base64.hpp"
				"include/GREM/core/formats/CRC32.hpp"
				"include/GREM/core/formats/deflate.hpp"
				"include/GREM/core/formats/gltf.hpp"
				"include/GREM/core/formats/json.hpp"
				"include/GREM/core/formats/obj.hpp"
				"include/GREM/core/formats/unicode.hpp"
				"include/GREM/core/formats/uri.hpp"
				"include/GREM/core/formats/xml.hpp"

				"include/GREM/core/system/Clock.hpp"
				"include/GREM/core/system/File.hpp"
				"include/GREM/core/system/Filesystem.hpp"
				"include/GREM/core/system/NativeFilesystem.hpp"
				"include/GREM/core/system/NativeInputFile.hpp"
				"include/GREM/core/system/NativeOutputFile.hpp"
				"include/GREM/core/system/SharedLibrary.hpp"
				"include/GREM/core/system/synchronization.hpp"
				"include/GREM/core/system/Thread.hpp"

				"include/GREM/core/algorithms.hpp"
				"include/GREM/core/assertions.hpp"
				"include/GREM/core/attributes.hpp"
				"include/GREM/core/command_line_interface.hpp"
				"include/GREM/core/concepts.hpp"
				"include/GREM/core/control.hpp"
				"include/GREM/core/debug_formatting.hpp"
				"include/GREM/core/Error.hpp"
				"include/GREM/core/extents.hpp"
				"include/GREM/core/formatting.hpp"
				"include/GREM/core/fundamentals.hpp"
				"include/GREM/core/geometry.hpp"
				"include/GREM/core/math.hpp"
				"include/GREM/core/metaprogramming.hpp"
				"include/GREM/core/profiling.hpp"
				"include/GREM/core/randomness.hpp"
				"include/GREM/core/statistics.hpp"
				"include/GREM/core/time.hpp"
				"include/GREM/core/version.hpp"
		PRIVATE
			"src/core/data/ConvexPolytope.cpp"
			"src/core/data/Writer.cpp"

			"src/core/formats/base16.cpp"
			"src/core/formats/base64.cpp"
			"src/core/formats/deflate.cpp"
			"src/core/formats/gltf.cpp"
			"src/core/formats/obj.cpp"
			"src/core/formats/uri.cpp"
			"src/core/formats/xml.cpp"
			
			"src/core/system/NativeFilesystem.cpp"
			"src/core/system/SharedLibrary.cpp"

			"src/core/Error.cpp"
			"src/core/formatting.cpp"
			"src/core/version.cpp")
	target_compile_definitions(GREM-core PRIVATE
		GREM_PRIVATE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		GREM_PRIVATE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		GREM_PRIVATE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
		GREM_PRIVATE_ABI_VERSION=${GREM_ABI_VERSION})
	target_compile_definitions(GREM-core
		INTERFACE "GREM_PRIVATE_API_core=${GREM_IMPORT_API}"
		PRIVATE "GREM_PRIVATE_API_core=${GREM_EXPORT_API}")
	target_link_libraries(GREM-core
		PUBLIC GREM-config ${GREM_CORE_PUBLIC_DEPENDENCIES}
		PRIVATE ${GREM_CORE_PRIVATE_DEPENDENCIES} ${CMAKE_DL_LIBS})
	if(GREM_USE_MULTITHREADING)
		set(THREADS_PREFER_PTHREAD_FLAG ON)
		find_package(Threads REQUIRED)
		target_link_libraries(GREM-core PUBLIC Threads::Threads)
	endif()

	if(GREM_ENABLE_EXECUTION_MODULE)
		list(APPEND GREM_MODULES GREM-execution)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_EXECUTION_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_EXECUTION_PRIVATE_DEPENDENCIES})
		add_library(GREM-execution)
		add_library(GREM::execution ALIAS GREM-execution)
		target_sources(GREM-execution
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/execution.hpp"

					"include/GREM/execution/component_pool.hpp"
					"include/GREM/execution/component.hpp"
					"include/GREM/execution/entity_range.hpp"
					"include/GREM/execution/EntityRegistry.hpp"
					"include/GREM/execution/EntityTable.hpp"
					"include/GREM/execution/Executor.hpp"
					"include/GREM/execution/resource.hpp"
					"include/GREM/execution/ResourceRegistry.hpp"
					"include/GREM/execution/ResourceTable.hpp"
					"include/GREM/execution/Schedule.hpp"
					"include/GREM/execution/Scheduler.hpp"
					"include/GREM/execution/Statistics.hpp"
					"include/GREM/execution/Task.hpp"
			PRIVATE
				"src/execution/Executor.cpp"
				"src/execution/Schedule.cpp"
				"src/execution/Scheduler.cpp")
		target_compile_definitions(GREM-execution PUBLIC GREM_ENABLE_EXECUTION_MODULE)
		target_compile_definitions(GREM-execution
			INTERFACE "GREM_PRIVATE_API_execution=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_execution=${GREM_EXPORT_API}")
		target_link_libraries(GREM-execution
			PUBLIC GREM-config GREM-core ${GREM_EXECUTION_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_EXECUTION_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_RESOURCE_MODULE)
		list(APPEND GREM_MODULES GREM-resource)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_RESOURCE_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_RESOURCE_PRIVATE_DEPENDENCIES})
		add_library(GREM-resource)
		add_library(GREM::resource ALIAS GREM-resource)
		target_sources(GREM-resource
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/resource.hpp"

					"include/GREM/resource/ArrayAtlasPacker.hpp"
					"include/GREM/resource/AtlasPacker.hpp"
					"include/GREM/resource/Error.hpp"
					"include/GREM/resource/Image.hpp"
					"include/GREM/resource/Model.hpp"
			PRIVATE
				"src/resource/Image.cpp"
				"src/resource/Model.cpp")
		target_compile_definitions(GREM-resource PUBLIC GREM_ENABLE_RESOURCE_MODULE)
		target_compile_definitions(GREM-resource
			INTERFACE "GREM_PRIVATE_API_resource=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_resource=${GREM_EXPORT_API}")
		target_link_libraries(GREM-resource
			PUBLIC GREM-config GREM-core ${GREM_RESOURCE_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_RESOURCE_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_APPLICATION_MODULE)
		list(APPEND GREM_MODULES GREM-application)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_APPLICATION_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_APPLICATION_PRIVATE_DEPENDENCIES})
		add_library(GREM-application)
		add_library(GREM::application ALIAS GREM-application)
		target_sources(GREM-application
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/application.hpp"

					"include/GREM/application/Application.hpp"
					"include/GREM/application/FrameInfo.hpp"
					"include/GREM/application/TickInfo.hpp"
					"include/GREM/application/VirtualFilesystem.hpp"
					"include/GREM/application/VirtualInputFile.hpp"
					"include/GREM/application/VirtualOutputFile.hpp"
			PRIVATE
				"src/application/Application.cpp"
				"src/application/VirtualFilesystem.cpp"
				"src/application/VirtualInputFile.cpp"
				"src/application/VirtualOutputFile.cpp")
		target_compile_definitions(GREM-application PUBLIC GREM_ENABLE_APPLICATION_MODULE)
		target_compile_definitions(GREM-application
			INTERFACE "GREM_PRIVATE_API_application=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_application=${GREM_EXPORT_API}")
		target_link_libraries(GREM-application
			PUBLIC GREM-config GREM-core ${GREM_APPLICATION_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_APPLICATION_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_AUDIO_MODULE)
		list(APPEND GREM_MODULES GREM-audio)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_AUDIO_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_AUDIO_PRIVATE_DEPENDENCIES})
		add_library(GREM-audio)
		add_library(GREM::audio ALIAS GREM-audio)
		target_sources(GREM-audio
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/audio.hpp"

					"include/GREM/audio/Error.hpp"
					"include/GREM/audio/filters.hpp"
					"include/GREM/audio/Listener.hpp"
					"include/GREM/audio/Sound.hpp"
					"include/GREM/audio/SoundInstanceID.hpp"
					"include/GREM/audio/SoundMix.hpp"
					"include/GREM/audio/SoundStage.hpp"
			PRIVATE
				"src/audio/Error.cpp"
				"src/audio/filters.cpp"
				"src/audio/Sound.cpp"
				"src/audio/SoundMix.cpp"
				"src/audio/SoundStage.cpp")
		target_compile_definitions(GREM-audio PUBLIC GREM_ENABLE_AUDIO_MODULE)
		target_compile_definitions(GREM-audio
			INTERFACE "GREM_PRIVATE_API_audio=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_audio=${GREM_EXPORT_API}")
		target_link_libraries(GREM-audio
			PUBLIC GREM-config GREM-core ${GREM_AUDIO_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_AUDIO_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_EVENTS_MODULE)
		list(APPEND GREM_MODULES GREM-events)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_EVENTS_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_EVENTS_PRIVATE_DEPENDENCIES})
		add_library(GREM-events)
		add_library(GREM::events ALIAS GREM-events)
		target_sources(GREM-events
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/events.hpp"

					"include/GREM/events/controller.hpp"
					"include/GREM/events/Error.hpp"
					"include/GREM/events/Event.hpp"
					"include/GREM/events/EventPump.hpp"
					"include/GREM/events/ExternalApplication.hpp"
					"include/GREM/events/Input.hpp"
					"include/GREM/events/InputManager.hpp"
					"include/GREM/events/keyboard.hpp"
					"include/GREM/events/mouse.hpp"
					"include/GREM/events/SimpleMessageBox.hpp"
			PRIVATE
				"src/events/Event.cpp"
				"src/events/EventPump.cpp"
				"src/events/ExternalApplication.cpp"
				"src/events/InputManager.cpp"
				"src/events/SimpleMessageBox.cpp")
		target_compile_definitions(GREM-events PUBLIC GREM_ENABLE_EVENTS_MODULE)
		target_compile_definitions(GREM-events
			INTERFACE "GREM_PRIVATE_API_events=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_events=${GREM_EXPORT_API}")
		target_link_libraries(GREM-events
			PUBLIC GREM-config GREM-core ${GREM_EVENTS_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_EVENTS_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_GRAPHICS_MODULE)
		list(APPEND GREM_MODULES GREM-graphics)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_GRAPHICS_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES})
		add_library(GREM-graphics)
		add_library(GREM::graphics ALIAS GREM-graphics)
		target_sources(GREM-graphics
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/graphics.hpp"

					"include/GREM/graphics/buffer_layouts.hpp"
					"include/GREM/graphics/buffers.hpp"
					"include/GREM/graphics/ConstantDescription.hpp"
					"include/GREM/graphics/Device.hpp"
					"include/GREM/graphics/Display.hpp"
					"include/GREM/graphics/Error.hpp"
					"include/GREM/graphics/FeatureSupport.hpp"
					"include/GREM/graphics/FieldDescription.hpp"
					"include/GREM/graphics/Mesh.hpp"
					"include/GREM/graphics/ParameterDescription.hpp"
					"include/GREM/graphics/RenderPass.hpp"
					"include/GREM/graphics/shaders.hpp"
					"include/GREM/graphics/SpriteAtlas.hpp"
					"include/GREM/graphics/Swapchain.hpp"
					"include/GREM/graphics/Texture.hpp"
					"include/GREM/graphics/VertexAttributeDescription.hpp"
					"include/GREM/graphics/Viewport.hpp"
					"include/GREM/graphics/Window.hpp"
			PRIVATE
				"src/graphics/Display.cpp"
				"src/graphics/shaders.cpp"
				"src/graphics/Window.cpp")
		if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
			target_sources(GREM-graphics PRIVATE
				"src/graphics/opengl/buffers.cpp"
				"src/graphics/opengl/Device.cpp"
				"src/graphics/opengl/Mesh.cpp"
				"src/graphics/opengl/RenderPass.cpp"
				"src/graphics/opengl/shaders.cpp"
				"src/graphics/opengl/Swapchain.cpp"
				"src/graphics/opengl/Texture.cpp"
				"src/graphics/opengl/Window.cpp")
			target_compile_definitions(GREM-graphics PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_OPENGL)
			if(GREM_GRAPHICS_OPENGL_USE_ES_PROFILE)
				target_compile_definitions(GREM-graphics PRIVATE GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE)
			endif()
		elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
			target_sources(GREM-graphics PRIVATE
				"src/graphics/vulkan/buffers.cpp"
				"src/graphics/vulkan/Device.cpp"
				"src/graphics/vulkan/Mesh.cpp"
				"src/graphics/vulkan/RenderPass.cpp"
				"src/graphics/vulkan/shaders.cpp"
				"src/graphics/vulkan/Swapchain.cpp"
				"src/graphics/vulkan/Texture.cpp"
				"src/graphics/vulkan/Window.cpp")
			target_compile_definitions(GREM-graphics PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN)
			if(GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
				target_compile_definitions(GREM-graphics PRIVATE GREM_PRIVATE_GRAPHICS_VULKAN_USE_GLSL_COMPILATION)
			endif()
			if(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
				target_compile_definitions(GREM-graphics PRIVATE GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
			endif()
		endif()
		target_compile_definitions(GREM-graphics PUBLIC GREM_ENABLE_GRAPHICS_MODULE)
		target_compile_definitions(GREM-graphics
			INTERFACE "GREM_PRIVATE_API_graphics=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_graphics=${GREM_EXPORT_API}")
		target_link_libraries(GREM-graphics
			PUBLIC GREM-config GREM-core GREM-resource ${GREM_GRAPHICS_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_GRAPHICS_2D_MODULE)
		list(APPEND GREM_MODULES GREM-graphics_2d)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_GRAPHICS_2D_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_2D_PRIVATE_DEPENDENCIES})
		add_library(GREM-graphics_2d)
		add_library(GREM::graphics_2d ALIAS GREM-graphics_2d)
		target_sources(GREM-graphics_2d
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/graphics_2d.hpp"

					"include/GREM/graphics_2d/Camera2D.hpp"
					"include/GREM/graphics_2d/Font2D.hpp"
					"include/GREM/graphics_2d/Instances2D.hpp"
					"include/GREM/graphics_2d/Model2D.hpp"
					"include/GREM/graphics_2d/Renderer2D.hpp"
					"include/GREM/graphics_2d/Text2D.hpp"
			PRIVATE
				"src/graphics_2d/Font2D.cpp"
				"src/graphics_2d/Instances2D.cpp"
				"src/graphics_2d/Renderer2D.cpp"
				"src/graphics_2d/Text2D.cpp")
		if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
			target_compile_definitions(GREM-graphics_2d PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_OPENGL)
		elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
			target_compile_definitions(GREM-graphics_2d PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN)
			if(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
				target_compile_definitions(GREM-graphics_2d PRIVATE GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
			endif()
		endif()
		target_compile_definitions(GREM-graphics_2d PUBLIC GREM_ENABLE_GRAPHICS_2D_MODULE)
		target_compile_definitions(GREM-graphics_2d
			INTERFACE "GREM_PRIVATE_API_graphics_2d=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_graphics_2d=${GREM_EXPORT_API}")
		target_link_libraries(GREM-graphics_2d
			PUBLIC GREM-config GREM-core GREM-resource GREM-graphics ${GREM_GRAPHICS_2D_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_GRAPHICS_2D_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_GRAPHICS_3D_MODULE)
		list(APPEND GREM_MODULES GREM-graphics_3d)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_GRAPHICS_3D_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_3D_PRIVATE_DEPENDENCIES})
		add_library(GREM-graphics_3d)
		add_library(GREM::graphics_3d ALIAS GREM-graphics_3d)
		target_sources(GREM-graphics_3d
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/graphics_3d.hpp"

					"include/GREM/graphics_3d/Camera3D.hpp"
					"include/GREM/graphics_3d/Cubemap3D.hpp"
					"include/GREM/graphics_3d/Decals3D.hpp"
					"include/GREM/graphics_3d/Fog3D.hpp"
					"include/GREM/graphics_3d/Instances3D.hpp"
					"include/GREM/graphics_3d/LightBaker3D.hpp"
					"include/GREM/graphics_3d/LightProbeVolumes3D.hpp"
					"include/GREM/graphics_3d/Lights3D.hpp"
					"include/GREM/graphics_3d/Model3D.hpp"
					"include/GREM/graphics_3d/ReflectionProbes3D.hpp"
					"include/GREM/graphics_3d/Renderer3D.hpp"
					"include/GREM/graphics_3d/Sky3D.hpp"
			PRIVATE
				"src/graphics_3d/Cubemap3D.cpp"
				"src/graphics_3d/Decals3D.cpp"
				"src/graphics_3d/Fog3D.cpp"
				"src/graphics_3d/Instances3D.cpp"
				"src/graphics_3d/LightBaker3D.cpp"
				"src/graphics_3d/LightProbeVolumes3D.cpp"
				"src/graphics_3d/Lights3D.cpp"
				"src/graphics_3d/Model3D.cpp"
				"src/graphics_3d/ReflectionProbes3D.cpp"
				"src/graphics_3d/Renderer3D.cpp"
				"src/graphics_3d/Sky3D.cpp")
		if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
			target_compile_definitions(GREM-graphics_3d PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_OPENGL)
		elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
			target_compile_definitions(GREM-graphics_3d PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN)
			if(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
				target_compile_definitions(GREM-graphics_3d PRIVATE GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
			endif()
		endif()
		target_compile_definitions(GREM-graphics_3d PUBLIC GREM_ENABLE_GRAPHICS_3D_MODULE)
		target_compile_definitions(GREM-graphics_3d
			INTERFACE "GREM_PRIVATE_API_graphics_3d=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_graphics_3d=${GREM_EXPORT_API}")
		target_link_libraries(GREM-graphics_3d
			PUBLIC GREM-config GREM-core GREM-resource GREM-graphics GREM-graphics_2d ${GREM_GRAPHICS_3D_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_GRAPHICS_3D_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_IMGUI_MODULE)
		target_link_libraries(imgui PUBLIC GREM-config GREM-core)

		list(APPEND GREM_MODULES GREM-imgui)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_IMGUI_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_IMGUI_PRIVATE_DEPENDENCIES})
		add_library(GREM-imgui)
		add_library(GREM::imgui ALIAS GREM-imgui)
		target_sources(GREM-imgui
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/imgui.hpp"

					"include/GREM/imgui/Error.hpp"
					"include/GREM/imgui/GraphicalUserInterface.hpp"
			PRIVATE
				"src/imgui/GraphicalUserInterface.cpp")
		if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
			target_compile_definitions(GREM-imgui PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_OPENGL)
		elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
			target_compile_definitions(GREM-imgui PRIVATE GREM_PRIVATE_GRAPHICS_BACKEND_VULKAN)
			if(GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
				target_compile_definitions(GREM-imgui PRIVATE GREM_GRAPHICS_VULKAN_USE_UNOPTIMIZED_DEBUGGABLE_BUILTIN_SHADERS)
			endif()
		endif()
		target_compile_definitions(GREM-imgui PUBLIC GREM_ENABLE_IMGUI_MODULE)
		target_compile_definitions(GREM-imgui
			INTERFACE "GREM_PRIVATE_API_imgui=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_imgui=${GREM_EXPORT_API}")
		target_link_libraries(GREM-imgui
			INTERFACE ${GREM_EXECUTION_IMGUI_DEPENDENCIES}
			PUBLIC GREM-config GREM-core GREM-events GREM-graphics GREM-graphics_2d ${GREM_IMGUI_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_IMGUI_PRIVATE_DEPENDENCIES})
	endif()

	if(GREM_ENABLE_NETWORKING_MODULE)
		list(APPEND GREM_MODULES GREM-networking)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_NETWORKING_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_NETWORKING_PRIVATE_DEPENDENCIES})
		add_library(GREM-networking)
		add_library(GREM::networking ALIAS GREM-networking)
		target_sources(GREM-networking
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/networking.hpp"

					"include/GREM/networking/Endpoint.hpp"
					"include/GREM/networking/Error.hpp"
					"include/GREM/networking/platform.hpp"
					"include/GREM/networking/Socket.hpp"
			PRIVATE
				"src/networking/Endpoint.cpp"
				"src/networking/Error.cpp"
				"src/networking/platform.cpp")
		if(GREM_NETWORKING_SOCKETS_BACKEND STREQUAL "Native")
			target_sources(GREM-networking PRIVATE
				"src/networking/native_sockets/Socket.cpp")
			target_compile_definitions(GREM-networking PRIVATE GREM_PRIVATE_NETWORKING_SOCKETS_BACKEND_NATIVE)
		elseif(GREM_NETWORKING_SOCKETS_BACKEND STREQUAL "Emulated")
			target_sources(GREM-networking PRIVATE
				"src/networking/emulated_sockets/Socket.cpp")
			target_compile_definitions(GREM-networking PRIVATE GREM_PRIVATE_NETWORKING_SOCKETS_BACKEND_EMULATED)
		endif()
		target_compile_definitions(GREM-networking PUBLIC GREM_ENABLE_NETWORKING_MODULE)
		target_compile_definitions(GREM-networking
			INTERFACE "GREM_PRIVATE_API_networking=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_networking=${GREM_EXPORT_API}")
		target_link_libraries(GREM-networking
			PUBLIC GREM-config GREM-core ${GREM_NETWORKING_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_NETWORKING_PRIVATE_DEPENDENCIES})
		if(MINGW)
			target_link_libraries(GREM-networking PRIVATE ws2_32)
		endif()
	endif()

	if(GREM_ENABLE_PHYSICS_MODULE)
		list(APPEND GREM_MODULES GREM-physics)
		list(APPEND GREM_PUBLIC_DEPENDENCIES ${GREM_PHYSICS_PUBLIC_DEPENDENCIES})
		list(APPEND GREM_PRIVATE_DEPENDENCIES ${GREM_PHYSICS_PRIVATE_DEPENDENCIES})
		add_library(GREM-physics)
		add_library(GREM::physics ALIAS GREM-physics)
		target_sources(GREM-physics
			PUBLIC FILE_SET HEADERS
				BASE_DIRS "include"
				FILES
					"include/GREM/physics.hpp"

					"include/GREM/physics/Broadphase.hpp"
					"include/GREM/physics/collision.hpp"
					"include/GREM/physics/DebugVisualization.hpp"
					"include/GREM/physics/EntityID.hpp"
					"include/GREM/physics/Error.hpp"
					"include/GREM/physics/joints.hpp"
					"include/GREM/physics/objects.hpp"
					"include/GREM/physics/quantities.hpp"
					"include/GREM/physics/Shape.hpp"
					"include/GREM/physics/Simulation.hpp"
			PRIVATE
				"src/physics/Broadphase.cpp"
				"src/physics/collision.cpp"
				"src/physics/Shape.cpp"
				"src/physics/simulation_tasks.cpp"
				"src/physics/Simulation.cpp")
		if(GREM_PHYSICS_USE_DEBUG_VISUALIZATION)
			target_compile_definitions(GREM-physics PUBLIC GREM_PHYSICS_USE_DEBUG_VISUALIZATION)
			target_link_libraries(GREM-physics PUBLIC GREM-graphics GREM-graphics_2d GREM-graphics_3d)
		endif()
		target_compile_definitions(GREM-physics PUBLIC GREM_ENABLE_PHYSICS_MODULE)
		target_compile_definitions(GREM-physics
			INTERFACE "GREM_PRIVATE_API_physics=${GREM_IMPORT_API}"
			PRIVATE "GREM_PRIVATE_API_physics=${GREM_EXPORT_API}")
		target_link_libraries(GREM-physics
			PUBLIC GREM-config GREM-core GREM-execution ${GREM_PHYSICS_PUBLIC_DEPENDENCIES}
			PRIVATE ${GREM_PHYSICS_PRIVATE_DEPENDENCIES})
	endif()

	list(REMOVE_DUPLICATES GREM_PUBLIC_DEPENDENCIES)
	list(REMOVE_DUPLICATES GREM_PRIVATE_DEPENDENCIES)

	set(GREM_DEPENDENCIES ${GREM_PUBLIC_DEPENDENCIES} ${GREM_PRIVATE_DEPENDENCIES})
	list(REMOVE_DUPLICATES GREM_DEPENDENCIES)

	function(GREM_post_build_copy_dlls DESTINATION_DIRECTORY MAIN_TARGET_NAME)
		set(HAS_RUNTIME_DLLS "")
		set(COPY_DLLS_COMMAND ${CMAKE_COMMAND} -E copy)
		foreach(TARGET_NAME "${MAIN_TARGET_NAME}" ${ARGN})
			set(HAS_RUNTIME_DLLS "${HAS_RUNTIME_DLLS},$<BOOL:$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>>")
			set(COPY_DLLS_COMMAND ${COPY_DLLS_COMMAND} "$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>")
		endforeach()
		set(HAS_RUNTIME_DLLS "$<AND:1${HAS_RUNTIME_DLLS}>")
		set(COPY_DLLS_COMMAND ${COPY_DLLS_COMMAND} "${DESTINATION_DIRECTORY}")
		add_custom_command(TARGET "${MAIN_TARGET_NAME}" POST_BUILD COMMAND "$<${HAS_RUNTIME_DLLS}:${COPY_DLLS_COMMAND}>" COMMAND_EXPAND_LISTS)
	endfunction()

	add_library(GREM INTERFACE)
	add_library(GREM::GREM ALIAS GREM)
	target_sources(GREM
		INTERFACE FILE_SET HEADERS
			BASE_DIRS "include"
			FILES
				"include/GREM/aliases.hpp"
				"include/GREM/entry_point.hpp"
				"include/GREM/GREM.hpp")
	target_link_libraries(GREM INTERFACE ${GREM_MODULES})

	foreach(MODULE GREM ${GREM_MODULES})
		set_target_properties("${MODULE}" PROPERTIES
			LINKER_LANGUAGE CXX
			CXX_SCAN_FOR_MODULES OFF
			SOVERSION "${GREM_ABI_VERSION}"
			VERSION "${GREM_ABI_VERSION}.${PROJECT_VERSION}")
		if(BUILD_SHARED_LIBS)
			set_target_properties("${MODULE}" PROPERTIES INSTALL_RPATH "$ORIGIN")
		endif()
	endforeach()

	foreach(DEPENDENCY ${GREM_DEPENDENCIES})
		set_target_properties("${DEPENDENCY}" PROPERTIES
			LINKER_LANGUAGE CXX
			CXX_SCAN_FOR_MODULES OFF)
	endforeach()

	install(TARGETS GREM ${GREM_MODULES} ${GREM_PUBLIC_DEPENDENCIES} FILE_SET HEADERS)
	if(BUILD_SHARED_LIBS)
		install(IMPORTED_RUNTIME_ARTIFACTS ${GREM_MODULES} RUNTIME_DEPENDENCY_SET GREM_DEPENDENCY_SET)
		install(RUNTIME_DEPENDENCY_SET GREM_DEPENDENCY_SET POST_EXCLUDE_REGEXES "^/usr/lib/" "^/lib/" "system32/.*\\.dll$")
	endif()

	if(EMSCRIPTEN)
		file(READ "${EMSCRIPTEN_ROOT_PATH}/LICENSE" GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS)
		string(REPLACE "\r\n" "\n" GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS}")
		string(REGEX MATCH "(Copyright \\(c\\) 2010-.... Emscripten authors[^=]*)\n=" GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS}")
		set(GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS "${CMAKE_MATCH_1}")
		string(SUBSTRING "${GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS}" 0 61 GREM_EMSCRIPTEN_COPYRIGHT_FILE_CONTENTS)
	
		file(READ "${EMSCRIPTEN_ROOT_PATH}/LICENSE" GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS)
		string(REPLACE "\r\n" "\n" GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS}")
		string(REGEX MATCH "(Copyright Joyent, Inc\\. and other Node contributors\\..*)    \"\"\"" GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS}")
		set(GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS "${CMAKE_MATCH_1}")
		string(REPLACE "\n        " "\n" GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS}")
		string(SUBSTRING "${GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS}" 0 72 GREM_EMSCRIPTEN_NODE_COPYRIGHT_FILE_CONTENTS)

		file(READ "${EMSCRIPTEN_ROOT_PATH}/system/lib/libc/musl/COPYRIGHT" GREM_EMSCRIPTEN_MUSL_LICENSE_FILE_CONTENTS)
		string(REPLACE "\r\n" "\n" GREM_EMSCRIPTEN_MUSL_LICENSE_FILE_CONTENTS "${GREM_EMSCRIPTEN_MUSL_LICENSE_FILE_CONTENTS}")
		string(REGEX MATCH "Copyright © 2005-.... Rich Felker, et al\\." GREM_EMSCRIPTEN_MUSL_COPYRIGHT_FILE_CONTENTS "${GREM_EMSCRIPTEN_MUSL_LICENSE_FILE_CONTENTS}")

		set(GREM_EMSCRIPTEN_TITLE "Emscripten")
		set(GREM_EMSCRIPTEN_USE "Used by this application.")
		set(GREM_EMSCRIPTEN_LINK "https://emscripten.org/")
		set(GREM_EMSCRIPTEN_COPYRIGHT "${GREM_EMSCRIPTEN_COPYRIGHT_FILE_CONTENTS}")
		set(GREM_EMSCRIPTEN_LICENSE "${GREM_EMSCRIPTEN_LICENSE_FILE_CONTENTS}")

		set(GREM_EMSCRIPTEN_NODE_TITLE "Node.js")
		set(GREM_EMSCRIPTEN_NODE_USE "Used by Emscripten.")
		set(GREM_EMSCRIPTEN_NODE_LINK "https://nodejs.org/en")
		set(GREM_EMSCRIPTEN_NODE_COPYRIGHT "${GREM_EMSCRIPTEN_NODE_COPYRIGHT_FILE_CONTENTS}")
		set(GREM_EMSCRIPTEN_NODE_LICENSE "${GREM_EMSCRIPTEN_NODE_LICENSE_FILE_CONTENTS}")

		set(GREM_EMSCRIPTEN_MUSL_TITLE "musl libc")
		set(GREM_EMSCRIPTEN_MUSL_USE "Used by Emscripten.")
		set(GREM_EMSCRIPTEN_MUSL_LINK "https://musl.libc.org/")
		set(GREM_EMSCRIPTEN_MUSL_COPYRIGHT "${GREM_EMSCRIPTEN_MUSL_COPYRIGHT_FILE_CONTENTS}")
		set(GREM_EMSCRIPTEN_MUSL_LICENSE "${GREM_EMSCRIPTEN_MUSL_LICENSE_FILE_CONTENTS}")

		set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
			GREM_EMSCRIPTEN
			GREM_EMSCRIPTEN_NODE
			GREM_EMSCRIPTEN_MUSL)
	endif()

	if(CMAKE_FIND_ROOT_PATH STREQUAL "/usr/x86_64-w64-mingw32")
		file(READ "/usr/share/licenses/mingw-w64-crt/COPYING.MinGW-w64-runtime.txt" GREM_MINGW_W64_RUNTIME_LICENSE_FILE_CONTENTS)
		string(REPLACE "\r\n" "\n" GREM_MINGW_W64_RUNTIME_LICENSE_FILE_CONTENTS "${GREM_MINGW_W64_RUNTIME_LICENSE_FILE_CONTENTS}")

		set(GREM_MINGW_W64_RUNTIME_TITLE "MinGW-w64 Runtime")
		set(GREM_MINGW_W64_RUNTIME_USE "Used by this application.")
		set(GREM_MINGW_W64_RUNTIME_LINK "https://www.mingw-w64.org/")
		set(GREM_MINGW_W64_RUNTIME_COPYRIGHT "See licenses below.")
		set(GREM_MINGW_W64_RUNTIME_LICENSES "${GREM_MINGW_W64_RUNTIME_LICENSE_FILE_CONTENTS}")

		set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
			GREM_MINGW_W64_RUNTIME)
	endif()

	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" GREM_LICENSE_FILE_CONTENTS)
	string(REPLACE "\r\n" "\n" GREM_LICENSE_FILE_CONTENTS "${GREM_LICENSE_FILE_CONTENTS}")
	string(REGEX MATCH "Copyright \\(c\\) 2022-.... Ivar Härnqvist" GREM_COPYRIGHT_FILE_CONTENTS "${GREM_LICENSE_FILE_CONTENTS}")

	set(GREM_TITLE "GREM")
	set(GREM_USE "Used by this application.")
	set(GREM_LINK "https://github.com/DonutVikingChap/GREM")
	set(GREM_COPYRIGHT "${GREM_COPYRIGHT_FILE_CONTENTS}")
	set(GREM_LICENSE "${GREM_LICENSE_FILE_CONTENTS}")

	set(GREM_DEPENDENCY_PREFIXES GREM ${GREM_DEPENDENCY_PREFIXES})

	foreach(DEPENDENCY_PREFIX ${GREM_DEPENDENCY_PREFIXES})
		if(DEFINED "${DEPENDENCY_PREFIX}_TITLE" AND NOT "${${DEPENDENCY_PREFIX}_TITLE}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n## ${${DEPENDENCY_PREFIX}_TITLE}\n")
		else()
			message(WARNING "No title specified for dependency ${DEPENDENCY_PREFIX}!")
		endif()

		if(DEFINED "${DEPENDENCY_PREFIX}_USE" AND NOT "${${DEPENDENCY_PREFIX}_USE}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n${${DEPENDENCY_PREFIX}_USE}\n")
		else()
			message(WARNING "No use specified for dependency ${DEPENDENCY_PREFIX}!")
		endif()

		if(DEFINED "${DEPENDENCY_PREFIX}_LINK" AND NOT "${${DEPENDENCY_PREFIX}_LINK}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### Link\n\n${${DEPENDENCY_PREFIX}_LINK}\n")
		elseif(DEFINED "${DEPENDENCY_PREFIX}_LINKS" AND NOT "${${DEPENDENCY_PREFIX}_LINKS}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### Links\n\n${${DEPENDENCY_PREFIX}_LINKS}\n")
		else()
			message(WARNING "No link specified for dependency ${DEPENDENCY_PREFIX}!")
		endif()

		if(DEFINED "${DEPENDENCY_PREFIX}_COPYRIGHT" AND NOT "${${DEPENDENCY_PREFIX}_COPYRIGHT}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### Copyright\n\n```\n${${DEPENDENCY_PREFIX}_COPYRIGHT}\n```\n")
		elseif(DEFINED "${DEPENDENCY_PREFIX}_CREDITS" AND NOT "${${DEPENDENCY_PREFIX}_CREDITS}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### Credits\n\n```\n${${DEPENDENCY_PREFIX}_CREDITS}\n```\n")
		else()
			message(WARNING "No copyright specified for dependency ${DEPENDENCY_PREFIX}!")
		endif()

		if(DEFINED "${DEPENDENCY_PREFIX}_LICENSE" AND NOT "${${DEPENDENCY_PREFIX}_LICENSE}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### License\n\n```\n${${DEPENDENCY_PREFIX}_LICENSE}```\n")
		elseif(DEFINED "${DEPENDENCY_PREFIX}_LICENSES" AND NOT "${${DEPENDENCY_PREFIX}_LICENSES}" STREQUAL "")
			string(APPEND GREM_LEGAL_NOTICES "\n### Licenses\n\n```\n${${DEPENDENCY_PREFIX}_LICENSES}```\n")
		else()
			message(WARNING "No license specified for dependency ${DEPENDENCY_PREFIX}!")
		endif()
	endforeach()

	if(PROJECT_IS_TOP_LEVEL)
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" "\
# Third-party legal notices\n\
\n\
This file contains credits and copyright notices of third-party libraries used in this application.\n\
${GREM_LEGAL_NOTICES}")
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ThirdPartyLegalNotices.md" TYPE BIN)
	endif()

	set(GREM_GDBINIT_SCRIPT "\
# This GDB initialization script file is generated by GREM during CMake configuration.\n\
# Its purpose is to configure GDB to improve the experience of debugging code that uses GREM.\n\
# To use it, supply the following command to GDB on launch: source ${CMAKE_SOURCE_DIR}/.gdbinit\n\
\n\
# Register custom pretty printers.\n\
python\n\
import sys\n\
sys.path.insert(0, '${CMAKE_CURRENT_SOURCE_DIR}/scripts')\n\
from grem_pretty_printers import register_grem_pretty_printers\n\
register_grem_pretty_printers(None)\n\
end\n\
\n\
# Skip files containing basic low-level primitive types to make stepping into functions easier when these are passed as arguments.\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Allocation.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Any.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Array.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/ArrayList.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/BitArray.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/BitBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Buffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Color.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/ConstantString.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/CStringView.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/DoubleEndedQueue.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/DoublyLinkedList.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/HashMap.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/HashSet.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Indirect.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/InplaceArrayList.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/InplaceBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/InplaceDoubleEndedQueue.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/InplaceRingBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/LinearBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/LooseOrthtree.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Optional.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/OrderedMap.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/OrderedMultimap.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Pair.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/PriorityQueue.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Registry.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/RingBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SharedPointer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SinglyLinkedList.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SmallArrayList.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SmallBuffer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Span.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SPSCQueue.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/SPSCSlidingWindowQueue.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/StridedSpan.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/String.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/StringView.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Subrange.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/Table.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/UniqueHandle.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/data/UniquePointer.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/extents.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/core/math.hpp\n\
skip file ${CMAKE_CURRENT_SOURCE_DIR}/include/GREM/physics/quantities.hpp\n\
")
	if(GREM_GENERATE_GDBINIT)
		file(WRITE "${CMAKE_SOURCE_DIR}/.gdbinit" "${GREM_GDBINIT_SCRIPT}")
	endif()

	if(GREM_BUILD_SHADER_BOOTSTRAPPER)
		add_subdirectory(shaders/bootstrapper)
	endif()

	if(GREM_BUILD_EXAMPLES)
		add_subdirectory(examples)
	endif()

	if(GREM_BUILD_TOOLS)
		add_subdirectory(tools)
	endif()
	
	if(GREM_BUILD_TESTS)
		enable_testing()
		add_subdirectory(tests)
	endif()

	message(STATUS "Configuring GREM: done.")
	message(STATUS "  Multithreading: ${GREM_USE_MULTITHREADING}")
	message(STATUS "  SSE intrinsics: ${GREM_USE_SSE_INTRINSICS}")
	message(STATUS "  Profiling: ${GREM_USE_PROFILING}")
	get_directory_property(GREM_COMPILE_DEFINITIONS DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" COMPILE_DEFINITIONS)
	string(TOUPPER ${CMAKE_BUILD_TYPE} GREM_UPPERCASE_BUILD_TYPE)
	string(APPEND GREM_COMPILE_DEFINITIONS "${CMAKE_CXX_FLAGS}")
	string(APPEND GREM_COMPILE_DEFINITIONS "${CMAKE_CXX_FLAGS_${GREM_UPPERCASE_BUILD_TYPE}}")
	if(GREM_COMPILE_DEFINITIONS MATCHES "GREM_ASSERT|GREM_UNREACHABLE")
		message(STATUS "  Assertions: Custom")
	elseif(GREM_USE_RELEASE_ASSERTIONS)
		if(GREM_RELEASE_ASSERTIONS_FAST_FAIL)
			message(STATUS "  Assertions: Checked (fast)")
		else()
			message(STATUS "  Assertions: Checked (basic)")
		endif()
	elseif(GREM_COMPILE_DEFINITIONS MATCHES "NDEBUG")
		if(GREM_ASSUME_UNCHECKED_ASSERTIONS_NEVER_FAIL)
			message(STATUS "  Assertions: Unchecked (assumed)")
		else()
			message(STATUS "  Assertions: Unchecked")
		endif()
	else()
		message(STATUS "  Assertions: Checked (debug)")
	endif()
	message(STATUS "Enabled GREM modules:")
	message(STATUS "  GREM-core")
	if(GREM_ENABLE_EXECUTION_MODULE)
		message(STATUS "  GREM-execution")
	endif()
	if(GREM_ENABLE_RESOURCE_MODULE)
		message(STATUS "  GREM-resource")
	endif()
	if(GREM_ENABLE_APPLICATION_MODULE)
		message(STATUS "  GREM-application")
	endif()
	if(GREM_ENABLE_AUDIO_MODULE)
		message(STATUS "  GREM-audio")
	endif()
	if(GREM_ENABLE_EVENTS_MODULE)
		message(STATUS "  GREM-events")
	endif()
	if(GREM_ENABLE_GRAPHICS_MODULE)
		message(STATUS "  GREM-graphics")
		message(STATUS "    Backend: ${GREM_GRAPHICS_BACKEND}")
		if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
			if(GREM_GRAPHICS_OPENGL_USE_ES_PROFILE)
				if(EMSCRIPTEN)
					message(STATUS "    Target WebGL API version: 2.0")
				else()
					message(STATUS "    Target OpenGL API version: ES 3.0")
				endif()
			else()
				message(STATUS "    Target OpenGL API version: 3.3 Core")
			endif()
		elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
			message(STATUS "    Target Vulkan API version: 1.2")
			message(STATUS "    GLSL compilation: ${GREM_GRAPHICS_VULKAN_USE_GLSL_COMPILATION}")
		endif()
	endif()
	if(GREM_ENABLE_GRAPHICS_2D_MODULE)
		message(STATUS "  GREM-graphics_2d")
	endif()
	if(GREM_ENABLE_GRAPHICS_3D_MODULE)
		message(STATUS "  GREM-graphics_3d")
	endif()
	if(GREM_ENABLE_IMGUI_MODULE)
		message(STATUS "  GREM-imgui")
		message(STATUS "    Obsolete functions: ${GREM_IMGUI_ENABLE_OBSOLETE_FUNCTIONS}")
		message(STATUS "    Demo windows: ${GREM_IMGUI_ENABLE_DEMO_WINDOWS}")
		message(STATUS "    Debug tools: ${GREM_IMGUI_ENABLE_DEBUG_TOOLS}")
	endif()
	if(GREM_ENABLE_NETWORKING_MODULE)
		message(STATUS "  GREM-networking")
		message(STATUS "    Sockets backend: ${GREM_NETWORKING_SOCKETS_BACKEND}")
	endif()
	if(GREM_ENABLE_PHYSICS_MODULE)
		message(STATUS "  GREM-physics")
		message(STATUS "    Debug visualization: ${GREM_PHYSICS_USE_DEBUG_VISUALIZATION}")
	endif()
endif()

if(GREM_BUILD_DOCS)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)
		configure_file("${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" COPYONLY)
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" "\nPROJECT_NUMBER = \"${CMAKE_PROJECT_VERSION}\"\nOUTPUT_DIRECTORY = \"${CMAKE_CURRENT_BINARY_DIR}/docs\"\nINPUT += \"${CMAKE_CURRENT_SOURCE_DIR}/include\"\n")
		add_custom_target(GREM-generate-documentation
			${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMENT "Generating documentation.")
	else()
		message(WARNING "Doxygen package not found; documentation will not be generated.")
	endif()
endif()
