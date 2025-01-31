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
#include "cli_parser.h"

#include <map>
#include <stdlib.h>

#include "tetra/gui/console.h"

#if 0
#define TRACE(fmt, ...) dc_log_trace(fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) \
    do                  \
    {                   \
    } while (0)
#endif

static inline std::map<std::string, const char*>* get_arg_map()
{
    static std::map<std::string, const char*> arg_map;
    return &arg_map;
}

void cli_parser::parse(const int argc, const char** argv)
{
    dc_log("CLI parsing started");
    const char* current_name = NULL;
    bool looking_for_value = false;

    std::map<std::string, const char*>* arg_map = get_arg_map();

    for (int i = 1; i < argc; i++)
    {
        bool is_convar = (argv[i] != NULL && argv[i][0] == '-');

        if (!is_convar && !looking_for_value)
            dc_log_warn("Dangling argument at argv[%d]: \"%s\"", i, argv[i]);

        if (looking_for_value)
        {
            if (current_name != NULL)
            {
                if (is_convar)
                {
                    arg_map->insert(std::make_pair(current_name, ""));
                    TRACE("arg_map[\"%s\"] = \"%s\"", current_name, arg_map->at(current_name));
                }
                else
                {
                    arg_map->insert(std::make_pair(current_name, (argv[i] == NULL ? "" : argv[i])));
                    TRACE("arg_map[\"%s\"] = \"%s\"", current_name, arg_map->at(current_name));
                }
            }
            looking_for_value = false;
        }

        if (is_convar)
        {
            looking_for_value = true;

            // Remove leading - from convar
            if (strlen(argv[i]) > 1)
                current_name = &argv[i][1];
            else
            {
                TRACE("Discarding blank convar");
                current_name = NULL;
                looking_for_value = false;
            }
        }
    }
    if (looking_for_value)
    {
        arg_map->insert(std::make_pair(current_name, ""));
        TRACE("arg_map[\"%s\"] = \"%s\"", current_name, arg_map->at(current_name));
    }
    dc_log("CLI parsing done! Found %zu flags", arg_map->size());
}

const char* cli_parser::get_value(const char* name)
{
    std::map<std::string, const char*>* arg_map = get_arg_map();

    auto it = arg_map->find(name);

    return (it == arg_map->end()) ? NULL : it->second;
}

bool cli_parser::apply_to(convar_t* cvr)
{
    const char* argv0 = cvr->get_name();
    const char* argv1 = cli_parser::get_value(argv0);
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

void cli_parser::apply()
{
    const std::map<std::string, const char*>* const arg_map = get_arg_map();
    std::map<std::string, const char*> ignored_arg_map = *arg_map;
    std::vector<const char*> failed_args;

    /* Apply convars */
    size_t applied = 0;
    for (convar_t* c : *convar_t::get_convar_list())
    {
        applied += cli_parser::apply_to(c);

        auto it = ignored_arg_map.find(c->get_name());
        if (it != ignored_arg_map.end())
            ignored_arg_map.erase(it);
    }

    /* Suppress dev convar dragging down counts */
    if (arg_map->find("dev") != arg_map->end())
        applied++;

    for (auto it : ignored_arg_map)
        dc_log_warn("Ignored parameter \"-%s\" \"%s\"", it.first.c_str(), it.second);

    dc_log("CLI Successfully applied %zu/%zu flags (Ignored %zu)", applied, arg_map->size() - ignored_arg_map.size(), ignored_arg_map.size());
}
