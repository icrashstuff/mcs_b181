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

#pragma once

#include "gui/imgui.h"
#include <SDL3/SDL_stdinc.h>
#include <vector>

namespace tetra
{
struct license_t
{
    /** If true then a copyright line should be added above the license text */
    bool needs_copyright_line;
    /** SPDX-License-Identifier value */
    const char* id;
    /** License text w/o copyright line */
    const char* text;
};

enum project_flags_t
{
    PROJECT_VENDORED = (1 << 0),
};

struct project_t
{
    const char* name;

    const char* copyright;

    Uint32 flags;

    std::vector<license_t> licenses;
};

extern license_t license_MIT;
extern license_t license_Zlib;
extern license_t license_Unlicense;

/**
 * Get projects used by Tetra and Tetra itself
 */
const project_t* get_projects(Uint32& num_projects);

/**
 * Display projects
 *
 * @param projects Array containing the projects to display
 * @param num_projects Size of projects array
 * @param func_header Function to call to display project dropdowns
 * @param func_text_imgui Function to call to display license text
 * @param userdata Userdata to pass to callbacks
 */
void projects_widgets(
    const project_t* projects, const Uint32 num_projects,
    bool (*func_header)(const char* label, void* userdata) = [](const char* label, void*) -> bool { return ImGui::CollapsingHeader(label); },
    void (*func_text)(const char* text, void* userdata) = [](const char* text, void*) -> void { return ImGui::TextUnformatted(text); },
    void* userdata = nullptr);
}
