# SPDX-License-Identifier: 0BSD
# SPDX-FileCopyrightText: Copyright 2024 - 2025 Ian Hangartner
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
    client/level.cpp
    client/level_mesh.cpp
    client/entity.cpp
    client/lightmap.cpp
    client/connection.cpp
    client/game.cpp
    client/time_blended_modifer.cpp

    client/jzon/Jzon.cpp

    client/lang/lang.cpp

    client/gui/mc_gui.cpp
    client/gui/panorama.cpp
    client/gui/debug_screen.cpp

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
    add_tetra_executable(${name} "${${name}_SRC};shared/build_info.cpp")

    target_compile_options(${name} PUBLIC -Wall -Wextra -DSDL_DISABLE_OLD_NAMES=1)

    target_link_libraries(${name} ${zlib_LIBRARIES})
    target_compile_options(${name} PUBLIC ${zlib_CFLAGS_OTHER})

    target_link_libraries(${name} SDL3_net::SDL3_net-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)
add_bin_common(mcs_b181_client)

target_link_libraries(mcs_b181_client OpenAL::OpenAL)
target_link_libraries(mcs_b181_client EnTT::EnTT)
target_link_libraries(mcs_b181_client cubiomes_static)

target_link_libraries(mcs_b181_server cubiomes_static)
