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

if(APPLE)
    project(mcs_b181 LANGUAGES CXX C OBJC)
else()
    project(mcs_b181 LANGUAGES CXX C)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# C++ 14 was for binary literals
# C++ 17 was for entt
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MCS_B181_VENDORED_LIBS_DIR "" CACHE STRING "Path to search for vendored libs")
if(NOT "${MCS_B181_VENDORED_LIBS_DIR}" STREQUAL "")
    message(STATUS "MCS_B181_VENDORED_LIBS_DIR: ${MCS_B181_VENDORED_LIBS_DIR}")
endif()

function(ensure_target package target)
    if(TARGET ${target})
        return()
    endif()

    if(NOT "${MCS_B181_VENDORED_LIBS_DIR}" STREQUAL "" AND
    EXISTS "${MCS_B181_VENDORED_LIBS_DIR}/"            AND
    EXISTS "${MCS_B181_VENDORED_LIBS_DIR}/${package}/" AND
    EXISTS "${MCS_B181_VENDORED_LIBS_DIR}/${package}/CMakeLists.txt")
        message(STATUS "${package}: Adding subdirectory")
        add_subdirectory("${MCS_B181_VENDORED_LIBS_DIR}/${package}/" "bin_${package}")
    endif()

    if(TARGET ${target})
        return()
    endif()

    message(STATUS "${package}: Falling back to find package")
    find_package(${package} REQUIRED)
endfunction()

set(SDL_AUDIO ON)
set(SDL_CAMERA OFF)
set(SDL_OPENGL OFF)
# set(SDL_OPENGLES OFF) # SDL Refuses to build for some reason if this is disabled
set(SDL_SHARED OFF)
set(SDL_RENDER OFF)
set(SDL_STATIC ON)
set(SDL_VENDOR_INFO "MCS_B181")

ensure_target(SDL3   SDL3::SDL3)
ensure_target(glm    glm::glm)
ensure_target(ZLIB   ZLIB::ZLIB)
ensure_target(Vulkan Vulkan::Headers)

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

set(mcs_b181_client_SRC
    client/main_client.cpp
    client/texture_terrain.cpp
    client/level.cpp
    client/level_mesh.cpp
    client/lightmap.cpp
    client/connection.cpp
    client/game.cpp
    client/time_blended_modifer.cpp
    client/chunk_cubic.cpp
    client/touch.cpp
    client/task_timer.cpp
    client/textures.cpp

    client/gpu/gpu.cpp
    client/gpu/test.cpp
    client/gpu/buffer.cpp
    client/gpu/texture.cpp
    client/gpu/sampler.cpp
    client/gpu/pipeline.cpp
    client/gpu/pipeline_cache.cpp
    client/gpu/subdiv_buffer.cpp
    client/gpu/command_buffer.cpp
    client/gpu/smol-v/smolv.cpp

    client/gpu/volk/volk.c

    client/sound/stb_vorbis.c
    client/sound/sound_world.cpp
    client/sound/miniaudio_impl.c
    client/sound/sound_resources.cpp

    client/jzon/Jzon.cpp

    client/lang/lang.cpp

    client/gui/mc_gui.cpp
    client/gui/debug_screen.cpp

    client/entity/entity.cpp
    client/entity/entity_physics.cpp

    client/shaders/clouds_shader.cpp
    client/shaders/terrain_shader.cpp
    client/shaders/composite_shader.cpp
    client/shaders/background_shader.cpp

    client/sys/device_state.cpp

    shared/ids.cpp
    shared/misc.cpp
    shared/chunk.cpp
    shared/packet.cpp
    shared/inventory.cpp
    shared/java_strings.cpp
    shared/simplex_noise/SimplexNoise.cpp

    tetra/gui/imgui/backends/imgui_impl_vulkan.cpp
)

if(APPLE)
    list(APPEND mcs_b181_client_SRC client/sys/device_state_apple.mm)
else()
    list(APPEND mcs_b181_client_SRC client/sys/device_state_fallback.cpp)
endif()

SET_SOURCE_FILES_PROPERTIES(shared/simplex_noise/SimplexNoise.cpp PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")

# We don't use Dear ImGui's default volk wrapper or vulkan prototypes
# Since we do weirdness with Volk to make KDevelop's parser happy, we only set IMGUI_IMPL_VULKAN_NO_PROTOTYPES for the file that needs it
SET_SOURCE_FILES_PROPERTIES(tetra/gui/imgui/backends/imgui_impl_vulkan.cpp PROPERTIES COMPILE_DEFINITIONS IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

# chunk_cubic.cpp contains the renderer hint and light flood algorithms and thus needs to be fast
SET_SOURCE_FILES_PROPERTIES(client/chunk_cubic.cpp PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")
# level_mesh.cpp contains the meshing code, and greatly benefits from the release flags
SET_SOURCE_FILES_PROPERTIES(client/level_mesh.cpp PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")

# SET_SOURCE_FILES_PROPERTIES(client/chunk_cubic.cpp PROPERTIES COMPILE_OPTIONS "-fopt-info-vec-all")

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(sdl_net/)

set(TETRA_BUILD_SDL_GPU_COMPONENT ON)
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

    if(APPLE)
        string(TIMESTAMP BUNDLE_TIMESTAMP "%Y.%m.%d-%H%M" UTC)
        string(REPLACE "_" "-" bundled_name ${name}) # Xcode complains otherwise
        set_target_properties(${name} PROPERTIES
            MACOSX_BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.icrashstuff.${bundled_name}"
            MACOSX_BUNDLE_BUNDLE_NAME "${name}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${BUNDLE_TIMESTAMP}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "#${GIT_SHA1_CLIENT}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "#${GIT_SHA1} (${GIT_REFSPEC})"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/info.plist.in"
        )
    endif()

    tetra_common_compile_options(${name})

    target_compile_options(${name} PUBLIC -Wall -Wextra -DSDL_DISABLE_OLD_NAMES=1)

    target_link_libraries(${name} glm::glm)
    target_link_libraries(${name} tetra::core)
    target_link_libraries(${name} ZLIB::ZLIB)
    target_link_libraries(${name} SDL3_net::SDL3_net-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)
add_bin_common(mcs_b181_client)

target_link_libraries(mcs_b181_client EnTT::EnTT)
target_link_libraries(mcs_b181_client cubiomes_static)
target_link_libraries(mcs_b181_client tetra::sdl_gpu)
target_link_libraries(mcs_b181_client Vulkan::Headers)

target_link_libraries(mcs_b181_bridge tetra::sdl_gpu)

target_link_libraries(mcs_b181_server cubiomes_static)

function(ios_resource path)
    if(IOS AND path AND EXISTS "${path}")
        target_sources(mcs_b181_client PRIVATE "${path}")
        set_source_files_properties("${path}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources/)
    endif()
endfunction()

if(IOS)
    set(MCS_B181_BUNDLED_RESOURCES "" CACHE STRING "ZIP/JAR File containing all resources")
    set(MCS_B181_BUNDLED_ASSETS "" CACHE STRING "ZIP File containing all assets")

    message(STATUS "MCS_B181_BUNDLED_ASSETS: ${MCS_B181_BUNDLED_ASSETS}")
    message(STATUS "MCS_B181_BUNDLED_RESOURCES: ${MCS_B181_BUNDLED_RESOURCES}")

    ios_resource("${MCS_B181_BUNDLED_ASSETS}")
    ios_resource("${MCS_B181_BUNDLED_RESOURCES}")
endif()

option(MCS_B181_BUILD_SHADERS "DEPRECATED: Automatically compile shaders" ON)
message(STATUS "MCS_B181_BUILD_SHADERS: ${MCS_B181_BUILD_SHADERS}")
if(NOT MCS_B181_BUILD_SHADERS)
    message(WARNING "Soon(TM) shader compilation will be mandatory")
endif()
if(MCS_B181_BUILD_SHADERS)
    find_package(Vulkan COMPONENTS glslc REQUIRED)

    # TODO-OPT: Allow cross compilation by specifying MCS_B181_smolv_converter_EXECUTABLE?
    if(NOT DEFINED MCS_B181_smolv_converter_EXECUTABLE)
        if(CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "For cross-compiliation builds: Please compile smol-v converter and add \"-DMCS_B181_smolv_converter_EXECUTABLE=/path/to/smolv_converter\" to cmake args")
        else()
            add_executable(smolv_converter "client/gpu/smolv_converter.cpp;client/gpu/smol-v/smolv.cpp")
            set(MCS_B181_smolv_converter_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/smolv_converter")
        endif()
    endif()

    set(mcs_b181_client_COMMON_DEPS
        ${CMAKE_CURRENT_SOURCE_DIR}/client/shaders/compile_shaders.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/client/shaders/shader_header.h
        ${CMAKE_CURRENT_SOURCE_DIR}/client/shaders/lightmap.glsl
        "${MCS_B181_smolv_converter_EXECUTABLE}"
    )

    add_custom_target(mcs_b181_client_shaders DEPENDS ${mcs_b181_client_COMMON_DEPS})

    function(add_shader stage in suffix)
        set(CURRENT_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/client/shaders")
        set(SHADER_HEADERS "")
        set(SHADER_HEADERS "${SHADER_HEADERS};${CURRENT_SHADER_DIR}/compiled/${in}${suffix}.smolv.h")

        add_custom_command(OUTPUT ${SHADER_HEADERS}
            WORKING_DIRECTORY   ${CURRENT_SHADER_DIR}
            COMMAND             ${CMAKE_CURRENT_SOURCE_DIR}/client/shaders/compile_shaders.sh
                                "${MCS_B181_smolv_converter_EXECUTABLE}"
                                "${stage}"
                                "${CURRENT_SHADER_DIR}/${in}"
                                "${CURRENT_SHADER_DIR}/compiled/${in}${suffix}"
                                ${ARGN}
            DEPENDS             "${CURRENT_SHADER_DIR}/${in}" ${mcs_b181_client_COMMON_DEPS}
            COMMENT             "Stage: ${stage} Input: ${in}, Suffix: ${suffix}, glslc opts: ${ARGN}"
            )

        target_sources(mcs_b181_client_shaders PRIVATE ${SHADER_HEADERS})
    endfunction()

    add_shader("vertex"   "terrain.vert" "")
    add_shader("fragment" "terrain.frag" ".opaque"       -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=0)
    add_shader("fragment" "terrain.frag" ".alpha_test"   -DUSE_ALPHA_TEST=1 -DDEPTH_PEELING=0)
    add_shader("fragment" "terrain.frag" ".depth_peel_0" -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=1)
    add_shader("fragment" "terrain.frag" ".depth_peel_n" -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=2)

    add_shader("vertex"   "background.vert" "")
    add_shader("fragment" "background.frag" "")

    add_shader("vertex"   "composite.vert" "")
    add_shader("fragment" "composite.frag" "")

    add_shader("vertex"   "clouds.vert" "")
    add_shader("fragment" "clouds.frag" "")

    add_dependencies(mcs_b181_client mcs_b181_client_shaders)
endif()
