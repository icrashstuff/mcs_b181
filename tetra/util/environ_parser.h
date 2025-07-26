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
#ifndef TETRA__UTIL__ENVIRON_PARSER_H
#define TETRA__UTIL__ENVIRON_PARSER_H

#include "convar.h"

#include <SDL3/SDL_stdinc.h> /* SDL_Environment */

/**
 * Very crude environment variable parser
 */
struct environ_parser
{
    /**
     * Iterate over convar array and apply values from an environment
     *
     * @param prefix Prefix for variable names (ex. A prefix of "CVR_" would mean the environment variable "CVR_dev" would map to the convar "dev")
     * @param environment Environment to pull from
     */
    static void apply(const char* prefix, SDL_Environment* const environment);

    /**
     * Find matching environ value and apply to convar
     *
     * @param prefix Prefix for variable names (ex. A prefix of "CVR_" would mean the environment variable "CVR_dev" would map to the convar "dev")
     * @param environment Environment to pull from
     * @param cvr Reference to Convar to apply to
     *
     * @returns true if a match was found, false if not
     */
    static bool apply_to(const char* prefix, SDL_Environment* const environment, convar_t* cvr);
};

#endif
