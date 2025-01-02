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

#include <limits.h>

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

bool argv_from_str(std::vector<std::string>& argv, std::string cmdline, bool parse_quotes, size_t max_argc)
{
    size_t pos = 0;

    argv.clear();

    bool in_quote = 0;

    argv.push_back("");

    char last = 0;

    while (pos < cmdline.length() && argv.size() < max_argc)
    {
        if (parse_quotes && cmdline[pos] == '"')
            in_quote = !in_quote;
        else if (cmdline[pos] == ' ' && !in_quote)
        {
            if (last != ' ')
                argv.push_back("");
        }
        else
        {
            argv[argv.size() - 1].append(1, cmdline[pos]);
        }
        last = cmdline[pos];
        pos++;
    }

    while (pos < cmdline.length())
        argv[argv.size() - 1].append(1, cmdline[pos++]);

    if (!argv[argv.size() - 1].length() && argv.size())
        argv.resize(argv.size() - 1);

    return in_quote == 0;
}

bool long_from_str(std::string str, long& out)
{
    const char* s = str.c_str();
    errno = 0;
    char* endptr;
    long v = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != '\0' || ((v == LONG_MIN || v == LONG_MAX) && errno == ERANGE))
        return 0;

    out = v;
    return 1;
}

bool int_from_str(std::string str, int& out)
{
    long o;
    int r = long_from_str(str, o);
    if (r)
        out = o;
    return r;
}
