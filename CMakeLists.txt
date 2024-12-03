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
   main.cpp
   packet.cpp
   java_strings.cpp
   simplex_noise/SimplexNoise.cpp
)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(sdl_net/)

if(WIN32)
    add_executable(mcs_b181_server WIN32 ${mcs_b181_server_SRC})
else()
    add_executable(mcs_b181_server ${mcs_b181_server_SRC})
endif()

target_compile_options(mcs_b181_server PUBLIC -Wall -Wextra)

target_link_libraries(mcs_b181_server ${zlib_LIBRARIES})
target_compile_options(mcs_b181_server PUBLIC ${zlib_CFLAGS_OTHER})

target_link_libraries(mcs_b181_server SDL3::SDL3)
target_link_libraries(mcs_b181_server SDL3_net::SDL3_net-static)
