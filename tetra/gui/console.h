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
 */
#ifndef MCS_B181_TETRA_UTILS_CONSOLE_H
#define MCS_B181_TETRA_UTILS_CONSOLE_H

#include "imgui.h"

#include <functional>
#include <stdio.h>

struct dev_console
{
    /*
     * True when console shown, otherwise False
     */
    static bool shown;
    /**
     * Should be called on SDL_KEYDOWN (SDL2) or SDL_EVENT_KEY_DOWN (SDL3)
     * when SDL_SCANCODE_GRAVE is called.
     * This key should probably not be changeable at runtime.
     */
    static void show_hide();
    /*
     * Renders console if shown == true
     * Should be called as close as possible to the imgui render call
     */
    static void render();
    static void add_log(const char* fmt, ...) IM_FMTARGS(1);
    static void add_command(const char* name, std::function<int(const int, const char**)> func);
    static void add_command(const char* name, std::function<int()> func);
    static void run_command(const char* fmt, ...) IM_FMTARGS(1);
};

#define dc_log(fmt, ...) dev_console::add_log("[" __FILE_NAME__ ":%s:%d]: " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define dc_log_warn(fmt, ...) dev_console::add_log("[" __FILE_NAME__ ":%s:%d][warn]: " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define dc_log_trace(fmt, ...) dev_console::add_log("[" __FILE_NAME__ ":%s:%d][trace]: " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define dc_log_error(fmt, ...) dev_console::add_log("[" __FILE_NAME__ ":%s:%d][error]: " fmt, __func__, __LINE__, ##__VA_ARGS__)

#endif
