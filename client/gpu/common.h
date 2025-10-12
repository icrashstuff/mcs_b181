/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
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
#pragma once

#include <SDL3/SDL_stdinc.h>

#include "volk/volk.h"

#include "vk_mem_alloc.h"

#include "tetra/util/misc.h" /* util::die() */

/** Die on an error from a function returning VkResult */
#define VK_DIE(_CALL)                                                                              \
    do                                                                                             \
    {                                                                                              \
        VkResult result__ = _CALL;                                                                 \
        if (result__ != VK_SUCCESS)                                                                \
            util::die("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult */
#define VK_TRY(_CALL)                                                                                 \
    do                                                                                                \
    {                                                                                                 \
        VkResult result__ = _CALL;                                                                    \
        if (result__ != VK_SUCCESS)                                                                   \
            dc_log_error("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult, and save the result */
#define VK_TRY_STORE(RESULT, _CALL)                                                               \
    do                                                                                            \
    {                                                                                             \
        RESULT = _CALL;                                                                           \
        if (RESULT != VK_SUCCESS)                                                                 \
            dc_log_error("%s failed with code: %d: %s", #_CALL, RESULT, string_VkResult(RESULT)); \
    } while (0)
