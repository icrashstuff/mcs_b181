# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright 2024 Ian Hangartner
cmake_minimum_required(VERSION 3.25)
project(mcs_b181 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_VERSION 3)

find_package(SDL3 REQUIRED)
find_package(PkgConfig)
pkg_check_modules(zlib REQUIRED zlib)

set(mcs_b181_server_SRC
    ids.cpp
    main.cpp
    packet.cpp
    java_strings.cpp
    simplex_noise/SimplexNoise.cpp
)

set(mcs_b181_bridge_SRC
    ids.cpp
    packet.cpp
    java_strings.cpp
    main_bridge.cpp
)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(sdl_net/)

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
endfunction()

add_bin_common(mcs_b181_server)
add_bin_common(mcs_b181_bridge)
