/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Basic example program:
 * main.cpp
 * ========
 * #include "tetra/tetra.h"
 * #include "tetra/log.h"
 * int main(const int argc, const char** argv)
 * {
 *     tetra::init("icrashstuff", "Tetra example", "config_prefix", argc, argv);
 *
 *     dc_log("Hello world!");
 *
 *     tetra::deinit();
 * }
 *
 * CMakeLists.txt
 * ==============
 * cmake_minimum_required(VERSION 3.27)
 *
 * # Make option() honor normal variables
 * cmake_policy(SET CMP0077 NEW)
 *
 * project(tetra_example LANGUAGES CXX C)
 *
 * set(CMAKE_INCLUDE_CURRENT_DIR ON)
 * set(tetra_example_SRC
 *     main.cpp
 * )
 *
 * # Tetra must be in the "tetra" subfolder
 * add_subdirectory(tetra/)
 *
 * add_executable(tetra_example ${tetra_example_SRC})
 *
 * target_link_libraries(tetra_example tetra_core)
 */

#ifndef MCS_B181_TETRA_H
#define MCS_B181_TETRA_H

#include <SDL3/SDL_stdinc.h>

namespace tetra
{
/**
 * Should be called immediately, Can only be called once
 */
void init(const char* organization, const char* appname, const char* cfg_path, int argc, const char** argv, const bool set_sdl_app_metadata = true);

/**
 * Deinit tetra, can only be called once
 */
void deinit();

/** Iteration limiter, because fps limiter sounded too limiting */
struct iteration_limiter_t
{
    iteration_limiter_t() { }

    iteration_limiter_t(const int max_iterations_per_second);

    /**
     * Delays current thread to try to achieve the target iterations per second
     *
     * May call SDL_DelayNS()
     */
    void wait();

    /**
     * Set maximum iterations per second
     *
     * A value of zero disables the limiter
     */
    void set_limit(const int max_iterations_per_second);

private:
    Uint64 reference_time = 0;
    Uint64 frames_since_reference = 0;
    Uint64 limit = 0;
};
};

#endif
