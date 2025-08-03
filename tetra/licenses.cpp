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

#include "licenses.h"

#include "util/stb_sprintf.h"

namespace tetra
{
static project_t tetra_projects[] = {
    { "Tetra", "Copyright (c) 2022, 2024 - 2025 Ian Hangartner", 0, { license_MIT } },
    { "SDL3", "Copyright (c) 1997 - 2025 Sam Lantinga, and others", 0, { license_Zlib } },
    { "Dear ImGui", "Copyright (c) 2014 - 2025 Omar Cornut, and others", PROJECT_VENDORED, { license_MIT } },
    { "PhysicsFS (physfs)", "Copyright (c) 2001 - 2024 Ryan C. Gordon, and others", PROJECT_VENDORED, { license_Zlib } },
    { "stb_image", "Copyright (c) 2006 - 2024 Sean Barrett, and others", PROJECT_VENDORED, { license_MIT, license_Unlicense } },
    { "stb_image_write", "Copyright (c) 2010 - 2024 Sean Barrett, and others", PROJECT_VENDORED, { license_MIT, license_Unlicense } },
    { "stb_sprintf", "Copyright (c) 2015 - 2024 Sean Barrett, and others", PROJECT_VENDORED, { license_MIT, license_Unlicense } },
};
}

const tetra::project_t* tetra::get_projects(Uint32& num_projects)
{
    num_projects = SDL_arraysize(tetra_projects);
    return tetra_projects;
}

static char* f(char* buf, size_t buf_len, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    stbsp_vsnprintf(buf, buf_len, fmt, args);
    va_end(args);
    return buf;
}

void tetra::projects_widgets(const project_t* projects, const Uint32 num_projects, bool (*func_header)(const char* label, void* userdata),
    void (*func_text)(const char* text, void* userdata), void* userdata)
{
    char buf[2048];
    for (Uint32 i = 0; i < num_projects; i++)
    {
        project_t project = projects[i];
        if (func_header(project.name, userdata))
        {
            for (Uint32 j = 0; j < project.licenses.size(); j++)
            {
                license_t license = project.licenses[j];

                if (j > 0 && project.licenses.size() > 1)
                    func_text("---------------- OR ----------------", userdata);

                func_text(f(buf, SDL_arraysize(buf), "SPDX-License-Identifier: %s", license.id), userdata);
                func_text("\n", userdata);
                if (license.needs_copyright_line)
                {
                    func_text(project.copyright, userdata);
                    func_text("\n", userdata);
                }
                func_text(license.text, userdata);
            }
        }
    }
}
