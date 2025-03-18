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
cmake_minimum_required(VERSION 3.25)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(mcs_b181 LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# C++ 14 was for binary literals
# C++ 17 was for entt
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)
find_package(ZLIB REQUIRED)

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
    client/level.cpp
    client/level_mesh.cpp
    client/lightmap.cpp
    client/connection.cpp
    client/game.cpp
    client/time_blended_modifer.cpp

    client/sound/stb_vorbis.c
    client/sound/sound_world.cpp
    client/sound/sound_resources.cpp

    client/jzon/Jzon.cpp

    client/lang/lang.cpp

    client/gui/mc_gui.cpp
    client/gui/panorama.cpp
    client/gui/debug_screen.cpp

    client/entity/entity.cpp
    client/entity/entity_physics.cpp

    client/shaders/shaders.cpp

    shared/ids.cpp
    shared/misc.cpp
    shared/chunk.cpp
    shared/packet.cpp
    shared/inventory.cpp
    shared/java_strings.cpp
    shared/simplex_noise/SimplexNoise.cpp
)

SET_SOURCE_FILES_PROPERTIES(shared/simplex_noise/SimplexNoise.cpp PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")

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

add_subdirectory(client/entt)

add_subdirectory(shared/cubiomes/)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)

get_git_head_revision(GIT_REFSPEC GIT_SHA1)

git_recent_rev("."      GIT_SHA1_ROOT)
git_recent_rev("client" GIT_SHA1_CLIENT)
git_recent_rev("server" GIT_SHA1_SERVER)
git_recent_rev("bridge" GIT_SHA1_BRIDGE)
git_recent_rev("shared" GIT_SHA1_SHARED)

git_path_is_dirty("."      GIT_DIRTY_ROOT)
git_path_is_dirty("client" GIT_DIRTY_CLIENT)
git_path_is_dirty("server" GIT_DIRTY_SERVER)
git_path_is_dirty("bridge" GIT_DIRTY_BRIDGE)
git_path_is_dirty("shared" GIT_DIRTY_SHARED)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/shared/build_info.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/shared/build_info.cpp" @ONLY)

function(add_bin_common name)
    add_executable(${name} "${${name}_SRC};shared/build_info.cpp")

    tetra_common_compile_options(${name})

    target_compile_options(${name} PUBLIC -Wall -Wextra -DSDL_DISABLE_OLD_NAMES=1)

    target_link_libraries(${name} glm::glm)
    target_link_libraries(${name} tetra_core)
    target_link_libraries(${name} ZLIB::ZLIB)
    target_link_libraries(${name} SDL3_net::SDL3_net-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)
add_bin_common(mcs_b181_client)

target_link_libraries(mcs_b181_client OpenAL::OpenAL)
target_link_libraries(mcs_b181_client EnTT::EnTT)
target_link_libraries(mcs_b181_client cubiomes_static)
target_link_libraries(mcs_b181_client tetra_gl)

target_link_libraries(mcs_b181_bridge tetra_gl)

target_link_libraries(mcs_b181_server cubiomes_static)
