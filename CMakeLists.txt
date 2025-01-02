# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright 2024 Ian Hangartner
cmake_minimum_required(VERSION 3.25)

# Make option() honor normal variables
cmake_policy(SET CMP0077 NEW)

project(mcs_b181 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# C++ 14 is solely for binary literals
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED zlib)

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

add_subdirectory(tetra/)

function(add_bin_common name)
    add_tetra_executable(${name} "${${name}_SRC}")

    target_compile_options(${name} PUBLIC -Wall -Wextra)

    target_link_libraries(${name} ${zlib_LIBRARIES})
    target_compile_options(${name} PUBLIC ${zlib_CFLAGS_OTHER})

    target_link_libraries(${name} SDL3_net::SDL3_net-static)
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)

