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
#include "texture.h"

#include "../state.h"

bool gpu::upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, std::function<void(void* tbo_pointer, Uint32 tbo_size)> copy_callback, const bool cycle)
{
    if (!copy_pass || !tex || !width || !height || !copy_callback || format == SDL_GPU_TEXTUREFORMAT_INVALID)
        return false;

    Uint32 buf_size = SDL_CalculateGPUTextureFormatSize(format, width, height, 1) >> (miplevel * 2);

    SDL_GPUTransferBufferCreateInfo cinfo_tbo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = buf_size, .props = 0 };

    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(state::gpu_device, &cinfo_tbo);
    {
        void* tbo_pointer = SDL_MapGPUTransferBuffer(state::gpu_device, tbo, 0);
        if (!tbo_pointer)
        {
            SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);
            return false;
        }

        copy_callback(tbo_pointer, buf_size);

        SDL_UnmapGPUTransferBuffer(state::gpu_device, tbo);
    }

    SDL_GPUTextureTransferInfo loc_tex = {};
    loc_tex.transfer_buffer = tbo;
    loc_tex.pixels_per_row = width;
    loc_tex.rows_per_layer = height;

    SDL_GPUTextureRegion region_tex = {};
    region_tex.texture = tex;
    region_tex.mip_level = miplevel;
    region_tex.layer = layer;
    region_tex.w = width;
    region_tex.h = height;
    region_tex.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &loc_tex, &region_tex, cycle);

    SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);

    return true;
}

bool gpu::upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, const void* const data, const bool cycle)
{
    Uint32 buf_size = SDL_CalculateGPUTextureFormatSize(format, width, height, 1) >> (miplevel * 2);
    return upload_to_texture2d(
        copy_pass, tex, format, layer, miplevel, width, height,
        [&data, &buf_size](void* tbo_data, Uint32 tbo_size) {
            SDL_assert(tbo_size == buf_size);
            memcpy(tbo_data, data, SDL_min(buf_size, tbo_size));
        },
        cycle);
}
