# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright 2024 Ian Hangartner
cmake_minimum_required(VERSION 3.25)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(mcs_b181 LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# C++ 14 is solely for binary literals
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED zlib)

set(mcs_b181_server_SRC
    server/main_server.cpp

    shared/ids.cpp
    shared/misc.cpp
    shared/chunk.cpp
    shared/packet.cpp
    shared/java_strings.cpp
    shared/simplex_noise/SimplexNoise.cpp
)

set(mcs_b181_bridge_SRC
    bridge/main_bridge.cpp

    shared/ids.cpp
    shared/packet.cpp
    shared/java_strings.cpp
)

find_package(OpenAL REQUIRED)
set(mcs_b181_client_SRC
    client/main_client.cpp
    client/texture_terrain.cpp
    client/shaders.cpp
    client/level.cpp
    client/entity.cpp
    client/lightmap.cpp
    client/connection.cpp
    client/game.cpp

    client/jzon/Jzon.cpp

    client/gui/mc_gui.cpp
    client/gui/panorama.cpp

    shared/ids.cpp
    shared/misc.cpp
    shared/chunk.cpp
    shared/packet.cpp
    shared/inventory.cpp
    shared/java_strings.cpp
    shared/simplex_noise/SimplexNoise.cpp
)

configure_file(client/shaders/terrain.frag shaders/terrain.frag COPYONLY)
configure_file(client/shaders/terrain.vert shaders/terrain.vert COPYONLY)
configure_file(client/shaders/pano.frag shaders/pano.frag COPYONLY)
configure_file(client/shaders/pano.vert shaders/pano.vert COPYONLY)
configure_file(client/shaders/fbo_blur.frag shaders/fbo_blur.frag COPYONLY)
configure_file(client/shaders/fbo_out.frag shaders/fbo_out.frag COPYONLY)
configure_file(client/shaders/fbo.vert shaders/fbo.vert COPYONLY)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(sdl_net/)

add_subdirectory(tetra/)

function(add_bin_common name)
    add_tetra_executable(${name} "${${name}_SRC}")

    target_compile_options(${name} PUBLIC -Wall -Wextra -DSDL_DISABLE_OLD_NAMES=1)

    target_link_libraries(${name} ${zlib_LIBRARIES})
    target_compile_options(${name} PUBLIC ${zlib_CFLAGS_OTHER})

    target_link_libraries(${name} SDL3_net::SDL3_net-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)
add_bin_common(mcs_b181_client)

target_link_libraries(mcs_b181_client OpenAL::OpenAL)

