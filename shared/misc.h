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
#ifndef MCS_B181_MISC_H
#define MCS_B181_MISC_H

#include "tetra/gui/console.h"
#include <SDL3/SDL_stdinc.h>
#include <assert.h>
#include <string>
#include <vector>

typedef Uint8 jubyte;
typedef Uint8 jbool;
typedef Sint8 jbyte;
typedef Sint16 jshort;
typedef Sint32 jint;
typedef Sint64 jlong;
typedef float jfloat;
typedef double jdouble;

#define ARR_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define ARR_SIZE_I(x) ((int)ARR_SIZE(x))

#define ARR_SIZE_S(x) ((short)ARR_SIZE(x))

#ifdef dc_log
#define LOG(fmt, ...) dc_log(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) dc_log_warn(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) dc_log_trace(fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) dc_log_error(fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...) printf("%s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN]: %s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERR]: %s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) printf("[TRACE]: %s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#endif

#ifdef ENABLE_TRACE
#define TRACE(fmt, ...) LOG_TRACE(fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) util::black_hole(fmt, ##__VA_ARGS__)
#endif

#ifndef NDEBUG
#define helpful_assert(x, fmt, ...)                     \
    do                                                  \
    {                                                   \
        if (!(x))                                       \
        {                                               \
            LOG_ERROR("[ASSERT]: " fmt, ##__VA_ARGS__); \
            assert(x);                                  \
        }                                               \
    } while (0)
#else
#define helpful_assert(x, fmt, ...) void(0)
#endif

#define BETWEEN_EXCL(x, a, b) ((a) < (x) && (x) < (b))
#define BETWEEN_INCL(x, a, b) ((a) <= (x) && (x) <= (b))

#define ENUM_SWITCH_CASE(OUT_STR, ENUM) \
    case ENUM:                          \
        OUT_STR = #ENUM;                \
        break;

#define MAX_PLAYERS 20

#define WORLD_HEIGHT 128

#define CHUNK_VIEW_DISTANCE 12

#define CHUNK_UNLOAD_DISTANCE (CHUNK_VIEW_DISTANCE + 2)

#if (WORLD_HEIGHT > 128)
#error "World height cannot exceed 128 (Seriously, there are some fields that will rollover with anything bigger)"
#endif

#if (WORLD_HEIGHT < 0)
#error "World height cannot be below 0 (Will crash the server, there is a vector that expects WORLD_HEIGHT to be non negative)"
#endif

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y WORLD_HEIGHT
#define CHUNK_SIZE_Z 16

#if (CHUNK_SIZE_X != 16) && (CHUNK_SIZE_Z != 16)
#error "Really? You think that things will continue to work?"
#endif

#define SUBCHUNK_SIZE_X 16
#define SUBCHUNK_SIZE_Y 16
#define SUBCHUNK_SIZE_Z 16
#define SUBCHUNK_SIZE_VOLUME ((SUBCHUNK_SIZE_Y) * (SUBCHUNK_SIZE_Z) * (SUBCHUNK_SIZE_X))

#if (SUBCHUNK_SIZE_X != 16) && (SUBCHUNK_SIZE_Y != 16) && (SUBCHUNK_SIZE_Z != 16)
#error "Really? You think that things will continue to work?"
#endif

#define REGION_SIZE_X 32
#define REGION_SIZE_Z 32

#if (REGION_SIZE_X != 32) && (REGION_SIZE_Z != 32)
#error "Really? You think that things will continue to work?"
#endif

/**
 * Maximum number of chances for ore to spawn in an chunk
 */
#define NUM_ORE_CHANCE 384

struct range_t
{
    Uint8 min;
    Uint8 max;
};

/**
 * Format size with one of the following units: [bytes, KB, MB, GB, TB]
 * @param rate Append "/s" to the end of unit
 */
std::string format_memory(const size_t size, const bool rate = false);

/**
 * Take a cmd and split it into separate string components
 */
bool argv_from_str(std::vector<std::string>& argv, const std::string cmdline, const bool parse_quotes = false, const size_t max_argc = -1);

bool long_from_str(const std::string str, long& out);
bool int_from_str(const std::string str, int& out);

#define ROTATE_UINT64(x, amount) (((((Uint64)(x)) << (64 - ((amount) % 64))) | (((Uint64)(x)) >> ((amount) % 64))) & 0xFFFFFFFFFFFFFFFF)

/**
 * Convert boolean value to string of true or false
 */
#define BOOL_S(x) ((x) ? "true" : "false")

SDL_FORCE_INLINE Sint16 cast_to_sint16(Uint64 in) { return *(Sint16*)&in; }
SDL_FORCE_INLINE Sint32 cast_to_sint32(Uint64 in) { return *(Sint32*)&in; }
SDL_FORCE_INLINE Sint64 cast_to_sint64(Uint64 in) { return *(Sint64*)&in; }

namespace util
{
/**
 * Parallelize a for-loop over a range from start (inclusive) to end (exclusive) by splitting it into sub-loops
 *
 * This will utilize then block the calling thread until all sub loops have been called
 *
 * ex. instead of:
 *
 * for(int it = 0; it < 10; it++)
 *     do_something();
 *
 * You write:
 *
 * parallel_for(0, 10, [&](const int _start, const int _end) {
 *     for (int it = _start; it < _end; it++)
 *         do_something();
 * });
 *
 * TODO-OPT: In the future this should probably tap into a job system of some sort
 *
 * @param start Inclusive start of range
 * @param end Inclusive end of range
 * @param func Sub-loop function to call
 */
void parallel_for(const int start, const int end, std::function<void(const int start, const int end)> func);

/**
 * Dummy printf-style function
 */
SDL_FORCE_INLINE SDL_PRINTF_VARARG_FUNC(1) void black_hole(const char*, ...) { }
}

#endif
