# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright 2024 Ian Hangartner
cmake_minimum_required(VERSION 3.25)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(mcs_b181 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_VERSION 3)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED zlib)

set(imgui_SRC
    tetra/gui/imgui-1.91.1/imgui.cpp
    tetra/gui/imgui-1.91.1/imgui_demo.cpp
    tetra/gui/imgui-1.91.1/imgui_draw.cpp
    tetra/gui/imgui-1.91.1/imgui_tables.cpp
    tetra/gui/imgui-1.91.1/imgui_widgets.cpp
    tetra/gui/imgui-1.91.1/misc/cpp/imgui_stdlib.cpp
    tetra/gui/imgui-1.91.1/backends/imgui_impl_sdl3.cpp
    tetra/gui/imgui-1.91.1/backends/imgui_impl_opengl3.cpp
)

set(tetra_SRC
    tetra/tetra.cpp

    tetra/gui/styles.cpp
    tetra/gui/console.cpp
    tetra/gui/gui_registrar.cpp
    tetra/gui/imgui_extracts.cpp
    tetra/gui/physfs_browser.cpp
    tetra/gui/overlay_loading.cpp
    tetra/gui/overlay_performance.cpp

    tetra/util/misc.cpp
    tetra/util/convar.cpp
    tetra/util/cli_parser.cpp
    tetra/util/convar_file.cpp

    ${imgui_SRC}
)

set(mcs_b181_server_SRC
    ids.cpp
    main.cpp
    misc.cpp
    chunk.cpp
    packet.cpp
    java_strings.cpp
    simplex_noise/SimplexNoise.cpp

    ${tetra_SRC}
)

set(mcs_b181_bridge_SRC
    ids.cpp
    packet.cpp
    java_strings.cpp
    main_bridge.cpp

    ${tetra_SRC}
)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(sdl_net/)

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
add_subdirectory(tetra/util/physfs/physfs-74c3054/)


function(add_bin_common name)
    if(WIN32)
        add_executable(${name} WIN32 ${${name}_SRC})
    else()
        add_executable(${name} ${${name}_SRC})
    endif()

    target_compile_options(${name} PUBLIC -Wall -Wextra)

    target_link_libraries(${name} ${zlib_LIBRARIES})
    target_compile_options(${name} PUBLIC ${zlib_CFLAGS_OTHER})

    target_link_libraries(${name} SDL3::SDL3)
    target_link_libraries(${name} SDL3_net::SDL3_net-static)

    target_link_libraries(${name} ${OPENGL_LIBRARIES})
    target_link_libraries(${name} PhysFS::PhysFS-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)

