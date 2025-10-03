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

#include "internal.h"

CREATE_FUNC_DEF(create_texture, Texture, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING);
RELEASE_FUNC_DEF(release_texture, Texture);

bool gpu::upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, std::function<void(void* tbo_pointer, Uint32 tbo_size)> copy_callback, const bool cycle)
{
    if (!copy_pass || !tex || !width || !height || !copy_callback || format == SDL_GPU_TEXTUREFORMAT_INVALID)
        return false;

    Uint32 buf_size = SDL_CalculateGPUTextureFormatSize(format, width, height, 1);

    SDL_GPUTransferBufferCreateInfo cinfo_tbo = {};
    cinfo_tbo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    cinfo_tbo.size = buf_size;

    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(state::sdl_gpu_device, &cinfo_tbo);
    if (!tbo)
    {
        dc_log_error("Failed to create transfer buffer! SDL_CreateGPUTransferBuffer: %s", SDL_GetError());
        return false;
    }
    {
        void* tbo_pointer = SDL_MapGPUTransferBuffer(state::sdl_gpu_device, tbo, 0);
        if (!tbo_pointer)
        {
            dc_log_error("Failed to map transfer buffer! SDL_MapGPUTransferBuffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(state::sdl_gpu_device, tbo);
            return false;
        }

        copy_callback(tbo_pointer, buf_size);

        SDL_UnmapGPUTransferBuffer(state::sdl_gpu_device, tbo);
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

    SDL_ReleaseGPUTransferBuffer(state::sdl_gpu_device, tbo);

    return true;
}

bool gpu::upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, const void* const data, const bool cycle)
{
    Uint32 buf_size = SDL_CalculateGPUTextureFormatSize(format, width, height, 1);
    return upload_to_texture2d(
        copy_pass, tex, format, layer, miplevel, width, height,
        [&data, &buf_size](void* tbo_data, Uint32 tbo_size) {
            SDL_assert(tbo_size == buf_size);
            memcpy(tbo_data, data, SDL_min(buf_size, tbo_size));
        },
        cycle);
}
