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
#include "misc.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

void util::die(const char* fmt, ...)
{
    fflush(stdout);
    fputs("util::die(): >>>>>> Begin message <<<<<<\n", stdout);
    fflush(stdout);

    char buffer[4096] = "A Fatal Error Occurred!\n\n";
    size_t prefix_length = strlen(buffer);
    va_list args;
    va_start(args, fmt);
    stbsp_vsnprintf(buffer + prefix_length, SDL_arraysize(buffer) - prefix_length - 1, fmt, args);
    va_end(args);

    fputs(buffer, stdout);
    fputc('\n', stdout);
    fflush(stdout);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", buffer, NULL);

    fputs("util::die(): >>>>>> End message <<<<<<\n", stdout);
    fflush(stdout);

    abort();
}
