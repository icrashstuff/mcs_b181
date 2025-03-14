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
 * This file declares the logging/running interface of the tetra dev_console
 * For the rest of the dev_console interface, check tetra/gui/console.h
 */
#ifndef MPH_TETRA_LOG_H
#define MPH_TETRA_LOG_H

/* Copied from stb_sprintf */
#if !defined(STBSP__ATTRIBUTE_FORMAT) && defined(__has_attribute)
#if __has_attribute(format)
#define STBSP__ATTRIBUTE_FORMAT(fmt, va) __attribute__((format(printf, fmt, va)))
#endif
#endif

#ifndef STBSP__ATTRIBUTE_FORMAT
#define STBSP__ATTRIBUTE_FORMAT(fmt, va)
#endif

namespace dev_console
{
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
void add_log(log_level_t lvl, const char* fname, const char* func, int line, const char* fmt, ...) STBSP__ATTRIBUTE_FORMAT(5, 6);

/**
 * Run a registered command
 *
 * This function should only be called from the event thread
 *
 * WARNING: This function is not safe to call from multiple threads
 */
void run_command(const char* fmt, ...) STBSP__ATTRIBUTE_FORMAT(1, 2);
};

#define dc_log_internal(fmt, ...) dev_console::add_log(dev_console::LEVEL_INTERNAL, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log(fmt, ...) dev_console::add_log(dev_console::LEVEL_INFO, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_warn(fmt, ...) dev_console::add_log(dev_console::LEVEL_WARN, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_trace(fmt, ...) dev_console::add_log(dev_console::LEVEL_TRACE, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_error(fmt, ...) dev_console::add_log(dev_console::LEVEL_ERROR, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dc_log_fatal(fmt, ...) dev_console::add_log(dev_console::LEVEL_FATAL, __FILE_NAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#endif
