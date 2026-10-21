# stuntman - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(stuntman LANGUAGES CXX RC)

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
    src/frame_rate.cpp
    src/fps_overlay.cpp
    src/pad_input.cpp
    src/pc_menu.cpp
    src/ui_theme.cpp
)
if(WIN32)
    # Icon and the PC menu's fonts
    list(APPEND STUNTMAN_SOURCES src/stuntman.rc)
endif()

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

# Launcher (WPF, launcher/): published as one exe next to stuntman.exe.
# Needs the .NET SDK; skipped without it.
find_program(DOTNET_EXECUTABLE dotnet)
if(WIN32 AND DOTNET_EXECUTABLE)
    file(GLOB LAUNCHER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher/*.cs"
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher/*.xaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/launcher/*.csproj")
    set(LAUNCHER_STAMP "${CMAKE_CURRENT_BINARY_DIR}/launcher.stamp")
    add_custom_command(
        OUTPUT "${LAUNCHER_STAMP}"
        COMMAND ${DOTNET_EXECUTABLE} publish "${CMAKE_CURRENT_SOURCE_DIR}/launcher/StuntmanLauncher.csproj"
            -c Release -o "${CMAKE_CURRENT_BINARY_DIR}/launcher-publish" --nologo -v quiet
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/launcher-publish/StuntmanLauncher.exe" "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch "${LAUNCHER_STAMP}"
        DEPENDS ${LAUNCHER_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/launcher/icon.ico"
        COMMENT "Publishing StuntmanLauncher.exe"
        VERBATIM
    )
    add_custom_target(stuntman_launcher ALL DEPENDS "${LAUNCHER_STAMP}")
else()
    message(STATUS "dotnet not found: StuntmanLauncher.exe will not be built")
endif()

# The SDK's UI headers include imgui.h. Headers only: ImGui is compiled into
# rexruntime.dll, and a second copy here would have its own context.
target_include_directories(stuntman PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk/thirdparty/imgui"
)
