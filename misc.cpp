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

std::string format_memory(size_t size, bool rate)
{
    char buf[128];

    if (size < 1000u)
        snprintf(buf, ARR_SIZE(buf), "%zu bytes%s", size, rate ? "/s" : "");
    else if (size < (1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.1f KB%s", (float)size / 1000.0f, rate ? "/s" : "");
    else if (size < (1000u * 1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.2f MB%s", (float)(size / 1000u) / 1000.0f, rate ? "/s" : "");
    else if (size < (1000u * 1000u * 1000u * 1000u))
        snprintf(buf, ARR_SIZE(buf), "%.2f GB%s", (float)(size / (1000u * 1000u)) / 1000.0f, rate ? "/s" : "");
    else
        snprintf(buf, ARR_SIZE(buf), "%.2f TB%s", (float)(size / (1000u * 1000u * 1000u)) / 1000.0f, rate ? "/s" : "");

    return buf;
}
