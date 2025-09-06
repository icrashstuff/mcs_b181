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
#ifndef MCS_B181_TETRA_UTIL_MISC_H
#define MCS_B181_TETRA_UTIL_MISC_H

#include "tetra/util/stb_sprintf.h"

#include <SDL3/SDL_endian.h>

#define __ASSERT_Swap(func, type, x)                                     \
    do                                                                   \
    {                                                                    \
        static_assert(sizeof(x) == sizeof(type), "Field size mismatch"); \
        x = func(x);                                                     \
    } while (0)

#define ASSERT_SwapLE16(x) __ASSERT_Swap(SDL_SwapLE16, Uint16, x)
#define ASSERT_SwapLE32(x) __ASSERT_Swap(SDL_SwapLE32, Uint32, x)
#define ASSERT_SwapLE64(x) __ASSERT_Swap(SDL_SwapLE64, Uint64, x)
#define ASSERT_SwapBE16(x) __ASSERT_Swap(SDL_SwapBE16, Uint16, x)
#define ASSERT_SwapBE32(x) __ASSERT_Swap(SDL_SwapBE32, Uint32, x)
#define ASSERT_SwapBE64(x) __ASSERT_Swap(SDL_SwapBE64, Uint64, x)

namespace util
{
/**
 * Prints error message to stdout and an SDL Message box titled "Fatal Error" and then calls abort()
 */
[[noreturn]] void die(const char* fmt, ...) STBSP__ATTRIBUTE_FORMAT(1, 2);
}
#endif
