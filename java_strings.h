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
#ifndef MCS_B181_JAVA_STRINGS_H
#define MCS_B181_JAVA_STRINGS_H

#include <SDL3/SDL_stdinc.h>
#include <string>

/**
 * Does garbage in garbage out conversion of ucs-2 encoded text to utf-8
 */
std::string UCS2_to_UTF8(const Uint16* str);

/**
 * Does garbage in garbage out conversion of ucs-2 encoded text to utf-8
 */
SDL_FORCE_INLINE std::string UCS2_to_UTF8(const char16_t* str) { return UCS2_to_UTF8((const Uint16*)str); }

/**
 * Does garbage in garbage out conversion of utf-8 encoded text to what is probably ucs-2 encoded text
 */
std::u16string UTF8_to_UCS2(const char* str);

// TODO: std::vector<char> UTF8_to_MUTF8(const char* str);
// TODO: std::vector<char> MUTF8_to_UTF8(const char* str);

#endif
