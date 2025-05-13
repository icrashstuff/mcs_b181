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

#include "../state.h"
#include "tetra/log.h"
#include "tetra/util/stb_sprintf.h"

#define CREATE_FUNC_DEF(FUNCTION_NAME, RESOURCE_SUB_TYPE, PROP_STRING)                                                          \
    SDL_GPU##RESOURCE_SUB_TYPE* gpu::FUNCTION_NAME(const SDL_GPU##RESOURCE_SUB_TYPE##CreateInfo& cinfo, const char* fmt, ...)   \
    {                                                                                                                           \
        SDL_GPU##RESOURCE_SUB_TYPE##CreateInfo cinfo_named = cinfo;                                                             \
        cinfo_named.props = SDL_CreateProperties();                                                                             \
        if (cinfo.props)                                                                                                        \
            SDL_CopyProperties(cinfo.props, cinfo_named.props);                                                                 \
                                                                                                                                \
        if (fmt)                                                                                                                \
        {                                                                                                                       \
            char name[1024] = "";                                                                                               \
                                                                                                                                \
            va_list args;                                                                                                       \
            va_start(args, fmt);                                                                                                \
            stbsp_vsnprintf(name, SDL_arraysize(name), fmt, args);                                                              \
            va_end(args);                                                                                                       \
                                                                                                                                \
            SDL_SetStringProperty(cinfo_named.props, PROP_STRING, name);                                                        \
        }                                                                                                                       \
                                                                                                                                \
        SDL_GPU##RESOURCE_SUB_TYPE* ret = SDL_CreateGPU##RESOURCE_SUB_TYPE(state::gpu_device, &cinfo_named);                    \
                                                                                                                                \
        if (!ret)                                                                                                               \
            dc_log_error("Failed to acquire %s! SDL_ReleaseGPU%s: %s", #RESOURCE_SUB_TYPE, #RESOURCE_SUB_TYPE, SDL_GetError()); \
                                                                                                                                \
        SDL_DestroyProperties(cinfo_named.props);                                                                               \
                                                                                                                                \
        return ret;                                                                                                             \
    }

#define RELEASE_FUNC_DEF(FUNCTION_NAME, RESOURCE_SUB_TYPE)                                 \
    void gpu::FUNCTION_NAME(SDL_GPU##RESOURCE_SUB_TYPE*& resource, const bool set_to_null) \
    {                                                                                      \
        SDL_ReleaseGPU##RESOURCE_SUB_TYPE(state::gpu_device, resource);                    \
        if (set_to_null)                                                                   \
            resource = nullptr;                                                            \
    }
