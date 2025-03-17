/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Portions Copyright (c) 2014-2024 Omar Cornut and Dear ImGui Contributors
 * SPDX-FileCopyrightText: Portions Copyright (c) 2024-2025 Ian Hangartner <icrashstuff at outlook dot com>
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
// Dear ImGui: standalone example application for SDL3 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include <SDL3/SDL.h>

#include "tetra/gui/console.h"
#include "tetra/util/cli_parser.h"
#include "tetra/util/convar.h"
#include "tetra/util/convar_file.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"
#include "tetra_core.h"
#include "tetra_internal.h"

static int init_counter = 0;

ImFont* dev_console::overlay_font = NULL;

std::atomic<float> dev_console::add_log_font_width = { 7.0f };

bool tetra::internal::is_initialized_core() { return init_counter > 0; }

void tetra::init(const char* organization, const char* appname, const char* cfg_path_prefix, int argc, const char** argv)
{
    if (init_counter++)
    {
        dc_log_warn("[tetra_core]: Skipping initialization as tetra_core has already been initialized (You are probably doing something wrong!)");
        return;
    }

    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, appname);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, __DATE__);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, organization);

    dc_log("[tetra_core]: Init started");

    convar_t::atexit_init();
    atexit(convar_t::atexit_callback);

    /* Parse command line */
    cli_parser::parse(argc, argv);

    {
        convar_int_t* dev = (convar_int_t*)convar_t::get_convar("dev");

        /* Set dev before any other variables in case their callbacks require dev */
        if (cli_parser::get_value(dev->get_name()))
            dev->set(1);
        dev->set_pre_callback([=](int, int) -> bool { return false; });
    }

    if (convar_t::dev())
    {
        /* KDevelop fully buffers the output and will not display anything */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fflush(stdout);
        fflush(stderr);
        dc_log("Developer convar set");

        convar_int_t* console_overlay = (convar_int_t*)convar_t::get_convar("console_overlay");

        if (console_overlay)
            console_overlay->set(3);
    }

    PHYSFS_init(argv[0]);
    PHYSFS_setSaneConfig(organization, appname, NULL, 0, 0);

    /* Set convars from config */
    convar_file_parser::set_config_prefix(cfg_path_prefix);
    convar_file_parser::read();

    /* Set convars from command line */
    cli_parser::apply();

    if (cli_parser::get_value("-help") || cli_parser::get_value("help") || cli_parser::get_value("h"))
    {
        dc_log_internal("Usage: %s [ -convar_name [convar_value], ...]", argv[0]);
        dc_log_internal("\n");
        dc_log_internal("Examples of usage (These may or may not contain valid arguments!):");
        dc_log_internal("  %s -dev -r_vsync 1", argv[0]);
        dc_log_internal("  %s -x 0 -y 540 -w 1000 -h 1902 -username icrashstuff", argv[0]);
        dc_log_internal("\n");
        dc_log_internal("List of all console variables *without* the flag CONVAR_FLAG_DEV_ONLY and associated help text (In no particular order)");
        dc_log_internal("=======================================================================================================================");
        std::vector<convar_t*>* cvrs = convar_t::get_convar_list();
        for (size_t i = 0; i < cvrs->size(); i++)
        {
            if (cvrs->at(i)->get_convar_flags() & CONVAR_FLAG_DEV_ONLY)
                continue;
            cvrs->at(i)->log_help();
            dc_log_internal("\n");
        }
        if (convar_t::dev())
        {
            dc_log_internal("List of all console variables with the flag CONVAR_FLAG_DEV_ONLY and associated help text (In no particular order)");
            dc_log_internal("==================================================================================================================");
            for (size_t i = 0; i < cvrs->size(); i++)
            {
                if (!(cvrs->at(i)->get_convar_flags() & CONVAR_FLAG_DEV_ONLY))
                    continue;
                cvrs->at(i)->log_help();
                dc_log_internal("\n");
            }
        }
        else
        {
            dc_log_internal("Console variables with flag CONVAR_FLAG_DEV_ONLY omitted, add `-dev` to the command line to list them.");
        }
        exit(0);
    }

    const PHYSFS_ArchiveInfo** supported_archives = PHYSFS_supportedArchiveTypes();

    for (int i = 0; supported_archives[i] != NULL; i++)
        dc_log("Supported archive: [%s]", supported_archives[i]->extension);
    dc_log("[tetra_core]: Init finished");
}

void tetra::deinit()
{
    init_counter--;

    if (init_counter < 0)
    {
        dc_log_error("[tetra_core]: Init counter is less than 0, resetting to 0");
        init_counter = 0;
        return;
    }

    if (init_counter != 0)
        return;

    dc_log("[tetra_core]: Deinit started");

    convar_file_parser::write();

    convar_t::atexit_callback();

    PHYSFS_deinit();
    dc_log("[tetra_core]: Deinit finished");
}

tetra::iteration_limiter_t::iteration_limiter_t(const int max_iterations_per_second) { set_limit(max_iterations_per_second); }

void tetra::iteration_limiter_t::set_limit(int _limit)
{
    if (_limit >= 0)
        limit = _limit;
}

void tetra::iteration_limiter_t::wait()
{
    Uint64 now = SDL_GetTicksNS();
    if (limit > 0)
    {
        Uint64 elasped_time_ideal = frames_since_reference * 1000ul * 1000ul * 1000ul / limit;
        Sint64 delay = reference_time + elasped_time_ideal - now;
        /* Reset when difference between the reality and ideality gets problematic */
        if (delay < -100l * 1000l * 1000l || 100l * 1000l * 1000l < delay)
        {
            reference_time = now;
            frames_since_reference = 0;
        }
        /* If the delay is less than 1us than the OS scheduler is bound to not return fast enough */
        else if (delay > 1000)
            SDL_DelayNS(delay);
    }
    frames_since_reference += 1;
}
