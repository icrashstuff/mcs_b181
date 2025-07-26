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
#include "environ_parser.h"

#include <stdlib.h>
#include <string>

#include "tetra/log.h"

#if 0
#define TRACE(fmt, ...) dc_log_trace(fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) \
    do                  \
    {                   \
    } while (0)
#endif

bool environ_parser::apply_to(const char* prefix, SDL_Environment* const environment, convar_t* cvr)
{
    const char* argv0 = cvr->get_name();
    const char* argv1 = SDL_GetEnvironmentVariable(environment, (std::string(prefix) + argv0).c_str());
    const char* argv[3] = { argv0, argv1, NULL };
    if (argv1)
    {
        TRACE("Applying \"%s\"", argv0);
        if (argv1[0] == '\0' && cvr->get_convar_type() == convar_t::CONVAR_TYPE::CONVAR_TYPE_INT && cvr->get_convar_flags() & CONVAR_FLAG_INT_IS_BOOL)
            argv[1] = "true";
        int cvr_cmd_ret = cvr->convar_command(2, argv);
        if (cvr_cmd_ret != 0)
            TRACE("Failed to apply \"%s\", convar_command returned %d", argv0, cvr_cmd_ret);
        return cvr_cmd_ret == 0;
    }
    TRACE("Skipping \"%s\"", argv0);
    return false;
}

void environ_parser::apply(const char* prefix, SDL_Environment* const environment)
{
    dc_log("Environ Begin applying flags");

    /* Apply convars */
    size_t applied = 0;
    for (convar_t* c : *convar_t::get_convar_list())
        applied += environ_parser::apply_to(prefix, environment, c);

    dc_log("Environ Successfully applied %zu flags", applied);
}
