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
#include "java_strings.h"

#include <assert.h>

#include <SDL3/SDL.h>

std::string UCS2_to_UTF8(const Uint16* str)
{
    std::string out;
    for (size_t i = 0; str[i] != 0; i++)
    {
        char buf[4];
        char* buf_end = SDL_UCS4ToUTF8(SDL_Swap16BE(str[i]), buf);

        out.append(buf, buf_end - buf);
    }
    return out;
}

std::u16string UTF8_to_UCS2(const char* str)
{
    std::u16string out;
    const char** pstr = &str;
    while (**pstr != 0)
    {
        Uint32 codepoint = SDL_StepUTF8(pstr, 0);
        assert(!(codepoint > 0xFFFF));
        if (codepoint > 0xFFFF)
            return out;

        out.append(1, SDL_Swap16BE((Uint16)codepoint));
    }

    return out;
}
