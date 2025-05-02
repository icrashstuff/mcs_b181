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

#include <SDL3/SDL_gpu.h>
#include <functional>

namespace gpu
{
/**
 * Upload data to a GPU texture layer
 *
 * @param copy_pass Copy pass to upload texture on
 * @param tex Texture to upload to
 * @param format Format of texture data
 * @param layer Texture layer to write to
 * @param miplevel Mip level to upload to
 * @param width Width of texture
 * @param height Height of texture
 * @param copy_callback Callback to fill the transfer buffer with data
 * @param cycle Use SDL GPU resource cycling
 *
 * @returns true on success, false on failure or invalid parameter
 */
bool upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, std::function<void(void* tbo_pointer, Uint32 tbo_size)> copy_callback, const bool cycle);

/**
 * Upload data to a GPU texture layer
 *
 * @param copy_pass Copy pass to upload texture on
 * @param tex Texture to upload to
 * @param format Format of texture data
 * @param layer Texture layer to write to
 * @param miplevel Mip level to upload to
 * @param width Width of texture
 * @param height Height of texture
 * @param data Buffer to copy data from (Must have a size of at least `SDL_CalculateGPUTextureFormatSize(format, width, height, 1) >> (miplevel * 2)`)
 * @param cycle Use SDL GPU resource cycling
 *
 * @returns true on success, false on failure or invalid parameter
 */
bool upload_to_texture2d(SDL_GPUCopyPass* const copy_pass, SDL_GPUTexture* const tex, const SDL_GPUTextureFormat format, const Uint32 layer,
    const Uint32 miplevel, const Uint32 width, const Uint32 height, const void* const data, const bool cycle);
}
