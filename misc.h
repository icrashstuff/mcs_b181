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

#define ARR_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define ARR_SIZE_I(x) ((int)ARR_SIZE(x))

#define ARR_SIZE_S(x) ((short)ARR_SIZE(x))

#define LOG(fmt, ...) printf("%s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN]: %s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) printf("[TRACE]: %s:%d %s(): " fmt "\n", __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__)

#ifdef ENABLE_TRACE
#define TRACE(fmt, ...) LOG_TRACE(fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) (0)
#endif

#define MAX_PLAYERS 20

#define WORLD_HEIGHT 128

#define BLOCK_ID_MAX 110

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

#define REGION_SIZE_X 32
#define REGION_SIZE_Z 32

#if (REGION_SIZE_X != 32) && (REGION_SIZE_Z != 32)
#error "Really? You think that things will continue to work?"
#endif

#endif
