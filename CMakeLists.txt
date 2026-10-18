# stuntman - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(stuntman LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The SDK's byte-swap and vector helpers use SSSE3 / SSE4.1 intrinsics.
if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-mssse3 -msse4.1)
endif()

include(generated/rexglue.cmake)

# Sources
set(STUNTMAN_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(stuntman WIN32 ${STUNTMAN_SOURCES})
else()
    add_executable(stuntman ${STUNTMAN_SOURCES})
endif()

rexglue_setup_target(stuntman GPU_PLUGINS xenos)

# The SDK copies its DLLs next to the exe in a POST_BUILD step, which only runs
# when the exe relinks. A change inside the runtime or GPU plugin doesn't
# relink it, so sync them on every build.
if(WIN32)
    add_custom_target(stuntman_sync_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:stuntman>
            $<TARGET_FILE:rexgpu-xenos>
            $<TARGET_FILE_DIR:stuntman>
        DEPENDS stuntman rexgpu-xenos
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()

# The SDK's UI headers include imgui.h. Headers only: ImGui is compiled into
# rexruntime.dll, and a second copy here would have its own context.
target_include_directories(stuntman PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/thirdparty/imgui"
)
