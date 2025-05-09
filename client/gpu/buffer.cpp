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
#include "buffer.h"

#include "../state.h"
#include "tetra/util/stb_sprintf.h"

SDL_GPUBuffer* gpu::create_buffer(const SDL_GPUBufferCreateInfo& cinfo, const char* fmt, ...)
{
    SDL_GPUBufferCreateInfo cinfo_named = cinfo;
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

        SDL_SetStringProperty(cinfo_named.props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, name);
    }

    SDL_GPUBuffer* ret = SDL_CreateGPUBuffer(state::gpu_device, &cinfo_named);

    SDL_DestroyProperties(cinfo_named.props);

    return ret;
}

bool gpu::upload_to_buffer(SDL_GPUCopyPass* const copy_pass, SDL_GPUBuffer* const buffer, const Uint32 offset, const Uint32 size,
    std::function<void(void* tbo_pointer, Uint32 tbo_size)> copy_callback, const bool cycle)
{
    if (!copy_pass || !buffer || !size || !copy_callback)
        return false;

    SDL_GPUTransferBufferCreateInfo cinfo_tbo = {};
    cinfo_tbo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    cinfo_tbo.size = size;

    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(state::gpu_device, &cinfo_tbo);
    {
        void* tbo_pointer = SDL_MapGPUTransferBuffer(state::gpu_device, tbo, 0);
        if (!tbo_pointer)
        {
            SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);
            return false;
        }

        copy_callback(tbo_pointer, size);

        SDL_UnmapGPUTransferBuffer(state::gpu_device, tbo);
    }

    SDL_GPUTransferBufferLocation loc_buf = {};
    loc_buf.transfer_buffer = tbo;

    SDL_GPUBufferRegion region_buf = {};
    region_buf.buffer = buffer;
    region_buf.offset = offset;
    region_buf.size = size;

    SDL_UploadToGPUBuffer(copy_pass, &loc_buf, &region_buf, cycle);

    SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);

    return true;
}

bool gpu::upload_to_buffer(
    SDL_GPUCopyPass* const copy_pass, SDL_GPUBuffer* const buffer, const Uint32 offset, const Uint32 size, const void* const data, const bool cycle)
{
    return upload_to_buffer(
        copy_pass, buffer, offset, size,
        [&data, &size](void* tbo_data, Uint32 tbo_size) {
            SDL_assert(tbo_size == size);
            memcpy(tbo_data, data, SDL_min(size, tbo_size));
        },
        cycle);
}

void gpu::release_buffer(SDL_GPUBuffer*& buffer, const bool set_buffer_to_null)
{
    SDL_ReleaseGPUBuffer(state::gpu_device, buffer);
    if (set_buffer_to_null)
        buffer = nullptr;
}
