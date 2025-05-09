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
#include "sampler.h"

#include "../state.h"
#include "tetra/util/stb_sprintf.h"

SDL_GPUSampler* gpu::create_sampler(const SDL_GPUSamplerCreateInfo& cinfo, const char* fmt, ...)
{
    SDL_GPUSamplerCreateInfo cinfo_named = cinfo;
    cinfo_named.props = SDL_CreateProperties();
    if (cinfo.props)
        SDL_CopyProperties(cinfo.props, cinfo_named.props);

    if (fmt)
    {
        char name[1024] = "";

        va_list args;
        va_start(args, fmt);
        stbsp_vsnprintf(name, SDL_arraysize(name), fmt, args);
        va_end(args);

        SDL_SetStringProperty(cinfo_named.props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, name);
    }

    SDL_GPUSampler* ret = SDL_CreateGPUSampler(state::gpu_device, &cinfo_named);

    SDL_DestroyProperties(cinfo_named.props);

    return ret;
}

void gpu::release_sampler(SDL_GPUSampler*& sampler, const bool set_sampler_to_null)
{
    SDL_ReleaseGPUSampler(state::gpu_device, sampler);
    if (set_sampler_to_null)
        sampler = nullptr;
}
