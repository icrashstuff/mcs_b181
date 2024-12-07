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
#ifndef MCS_B181_TETRA_UTIL_CLI_PARSER_H
#define MCS_B181_TETRA_UTIL_CLI_PARSER_H

#include "convar.h"

/**
 * Very crude command line parser
 */
struct cli_parser
{
    /**
     * Parses command line
     *
     * Example
     * Input: argv0 -convar "val1" -convar2 -convar "val3" -convar4
     * Output: convar="val3", convar2="", convar4="", convar5=NULL
     *
     * @param argv NOTE: This array must stay valid for the lifetime of the application, not sure why it wouldn't, but just saying
     */
    static void parse(const int argc, const char** argv);

    /**
     * Returns the argv parameter immediately following a convar or "" if there was none
     * Returns NULL if the convar was not present
     */
    static const char* get_value(const char* name);

    /**
     * Iterate over convar array and apply values
     */
    static void apply();

    /**
     * Find matching cli value and apply to convar
     *
     * @returns true if a match was found, false if not
     */
    static bool apply_to(convar_t* cvr);
};

#endif
