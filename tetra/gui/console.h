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
#ifndef MPH_TETRA_UTILS_CONSOLE_H
#define MPH_TETRA_UTILS_CONSOLE_H

#include "imgui.h"

#include <SDL3/SDL_stdinc.h>
#include <atomic>
#include <functional>
#include <stdio.h>

namespace dev_console
{
/*
 * True when console shown, otherwise False
 *
 * This variable should only be accessed from the event thread
 *
 * WARNING: This variable is not safe to access from multiple threads
 */
extern bool shown;

/**
 * Font to be used for the overlay
 */
extern ImFont* overlay_font;

/**
 * dev_console::add_log will uses this and SDL_utf8strlen to calculate line width
 */
extern std::atomic<float> add_log_font_width;

/**
 * Should be called on SDL_KEYDOWN (SDL2) or SDL_EVENT_KEY_DOWN (SDL3)
 * when SDL_SCANCODE_GRAVE is called.
 * This key should probably not be changeable at runtime.
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void show_hide();

/*
 * Renders console if shown == true
 * Should be called as close as possible to the imgui render call
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void render();

enum log_level_t
{
    LEVEL_INTERNAL_CMD = -2,
    LEVEL_INTERNAL = -1,
    LEVEL_FATAL,
    LEVEL_ERROR,
    LEVEL_WARN,
    LEVEL_INFO,
    LEVEL_TRACE,
};

/**
 * Print a log message to the console
 *
 * You should probably use one of the dc_log macros instead of calling it directly
 *
 * Log messages are limited to 2048 characters including the null terminator
 *
 * Safe to call from any thread
 *
 * @param lvl Log level, a level of LEVEL_INTERNAL disables fname, func, and line
 * @param fname File the log call was made from
 * @param func Function the log call was made from
 * @param line Line the log call was made from
 *
 * @sa dc_log
 * @sa dc_log_warn
 * @sa dc_log_error
 * @sa dc_log_trace
 * @sa dc_log_internal
 */
void add_log(log_level_t lvl, const char* fname, const char* func, int line, const char* fmt, ...) IM_FMTARGS(5);

/**
 * Register a command with the console
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void add_command(const char* name, std::function<int(const int, const char**)> func);

/**
 * Register a command with the console
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void add_command(const char* name, std::function<int()> func);

/**
 * Run a registered command
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void run_command(const char* fmt, ...) IM_FMTARGS(1);
};

#define dc_log_internal(fmt, ...) dev_console::add_log(dev_console::LEVEL_INTERNAL, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log(fmt, ...) dev_console::add_log(dev_console::LEVEL_INFO, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_warn(fmt, ...) dev_console::add_log(dev_console::LEVEL_WARN, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_trace(fmt, ...) dev_console::add_log(dev_console::LEVEL_TRACE, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_error(fmt, ...) dev_console::add_log(dev_console::LEVEL_ERROR, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_fatal(fmt, ...) dev_console::add_log(dev_console::LEVEL_FATAL, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#endif
