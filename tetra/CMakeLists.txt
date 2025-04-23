# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2024-2025 Ian Hangartner <icrashstuff at outlook dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
cmake_minimum_required(VERSION 3.27)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(tetra CXX C)

option(TETRA_ENABLE_NATIVE_OPTIMIZATION "Enable native CPU specific compiler optimizations" OFF)
option(TETRA_DISABLE_STACK_PROTECTOR "Useful for finding out where stack smashing is occurring (Debug builds only)" OFF )

if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 REQUIRED)
endif()

# We don't necessarily have to disable most of the archivers but we also don't need them either
set(PHYSFS_BUILD_SHARED FALSE)
set(PHYSFS_ARCHIVE_GRP FALSE)
set(PHYSFS_ARCHIVE_WAD FALSE)
set(PHYSFS_ARCHIVE_CSM FALSE)
set(PHYSFS_ARCHIVE_HOG FALSE)
set(PHYSFS_ARCHIVE_MVL FALSE)
set(PHYSFS_ARCHIVE_QPAK FALSE)
set(PHYSFS_ARCHIVE_SLB FALSE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE)
set(PHYSFS_ARCHIVE_VDF FALSE)
set(PHYSFS_ARCHIVE_LECARCHIVES FALSE)
set(PHYSFS_BUILD_DOCS FALSE)
set(PHYSFS_BUILD_TEST FALSE)
add_subdirectory(util/physfs/physfs/)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TETRA_DIR "")

set(IMGUI_DIR "${TETRA_DIR}gui/imgui")

set(imgui_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

set(tetra_core_SRC
    ${TETRA_DIR}gui/styles.cpp
    ${TETRA_DIR}gui/console.cpp
    ${TETRA_DIR}gui/gui_registrar.cpp
    ${TETRA_DIR}gui/imgui_extracts.cpp
    ${TETRA_DIR}gui/physfs_browser.cpp
    ${TETRA_DIR}gui/overlay_loading.cpp
    ${TETRA_DIR}gui/overlay_performance.cpp

    ${TETRA_DIR}util/misc.cpp
    ${TETRA_DIR}util/convar.cpp
    ${TETRA_DIR}util/cli_parser.cpp
    ${TETRA_DIR}util/convar_file.cpp

    ${TETRA_DIR}util/stb/stbi.c
    ${TETRA_DIR}util/stb/stb_sprintf.c
    ${TETRA_DIR}util/stb/stb_rect_pack.c

    ${TETRA_DIR}util/physfs/physfs/extras/physfssdl3.c

    ${TETRA_DIR}tetra_core.cpp

    ${imgui_SRC}
)

set(tetra_gl_SRC
    ${TETRA_DIR}tetra_gl.cpp

    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(tetra_sdl_gpu_SRC
    ${TETRA_DIR}tetra_sdl_gpu.cpp

    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)

function(tetra_common_compile_options target_name)
    target_compile_options(${target_name} PUBLIC -Wall -Wextra -DSDL_DISABLE_OLD_NAMES=1)

    # -WShadow spits out a warning for code like:
    # int i = 0;
    # for(int i = 0; ...)
    # ...
    #
    # If you ever need to find a flag to warn for specific behavior:
    # the clang flag -Weverything is useful for finding them
    target_compile_options(${target_name} PUBLIC -Wshadow)

    if(TETRA_ENABLE_NATIVE_OPTIMIZATION AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE -march=native -mtune=native)
    endif()

    if(TETRA_DISABLE_STACK_PROTECTOR AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT MSVC)
        message(WARNING "Disabling stack protector")
        target_compile_options(${target_name} PUBLIC -fno-stack-protector)
    endif()
endfunction()

option(TETRA_BUILD_GL_COMPONENT "Build tetra::gl (tetra_gl)" OFF)
option(TETRA_BUILD_SDL_GPU_COMPONENT "Build tetra::sdl_gpu (tetra_sdl_gpu)" OFF)

### BEGIN: Target: tetra_core
add_library(tetra_core OBJECT ${tetra_core_SRC})

tetra_common_compile_options(tetra_core)

target_include_directories(tetra_core PRIVATE ../)

target_link_libraries(tetra_core PUBLIC PhysFS::PhysFS-static)
target_link_libraries(tetra_core PUBLIC SDL3::SDL3)

add_library(tetra::core ALIAS tetra_core)
### END: Target: tetra_core

### BEGIN: Target: tetra_gl
if(TETRA_BUILD_GL_COMPONENT)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)

    add_library(tetra_gl OBJECT ${tetra_gl_SRC})

    tetra_common_compile_options(tetra_gl)

    target_include_directories(tetra_gl PRIVATE ../)

    target_link_libraries(tetra_gl PUBLIC OpenGL::GL)
    target_link_libraries(tetra_gl PUBLIC OpenGL::GLU)
    target_link_libraries(tetra_gl PUBLIC GLEW::GLEW)
    target_link_libraries(tetra_gl PUBLIC tetra_core)

    add_library(tetra::gl ALIAS tetra_gl)
endif()
### END: Target: tetra_gl

### BEGIN: Target: tetra_sdl_gpu
if(TETRA_BUILD_SDL_GPU_COMPONENT)
    add_library(tetra_sdl_gpu OBJECT ${tetra_sdl_gpu_SRC})

    tetra_common_compile_options(tetra_sdl_gpu)

    target_include_directories(tetra_sdl_gpu PRIVATE ../)

    target_link_libraries(tetra_sdl_gpu PUBLIC tetra_core)

    add_library(tetra::sdl_gpu ALIAS tetra_sdl_gpu)
endif()
### END: Target: tetra_sdl_gpu
