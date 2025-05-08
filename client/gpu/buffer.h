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
 * Create named GPU buffer
 *
 * @param cinfo Creation info
 * @param fmt Buffer name format string (Uses stb_sprintf) (NULL for no name)
 *
 * @returns a buffer handle, or NULL on error
 */
SDL_GPUBuffer* create_buffer(const SDL_GPUBufferCreateInfo& cinfo, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

/**
 * Upload data to a GPU Buffer
 *
 * @param copy_pass Copy pass to upload buffer on
 * @param buffer Buffer to upload to
 * @param offset Offset in GPU buffer
 * @param size Size of data to copy
 * @param copy_callback Callback to fill the transfer buffer with data
 * @param cycle Use SDL GPU resource cycling
 *
 * @returns true on success, false on failure or invalid parameter
 */
bool upload_to_buffer(SDL_GPUCopyPass* const copy_pass, SDL_GPUBuffer* const buffer, const Uint32 offset, const Uint32 size,
    std::function<void(void* tbo_pointer, Uint32 tbo_size)> copy_callback, const bool cycle);

/**
 * Upload data to a GPU Buffer
 *
 * @param copy_pass Copy pass to upload buffer on
 * @param buffer Buffer to upload to
 * @param offset Offset in GPU buffer
 * @param size Size of data to copy
 * @param data Buffer to copy data from
 * @param cycle Use SDL GPU resource cycling
 *
 * @returns true on success, false on failure or invalid parameter
 */
bool upload_to_buffer(
    SDL_GPUCopyPass* const copy_pass, SDL_GPUBuffer* const buffer, const Uint32 offset, const Uint32 size, const void* const data, const bool cycle);

/**
 * Release a GPU buffer
 *
 * You must not reference the buffer after calling this function.
 *
 * @param buffer Buffer to release
 * @param set_buffer_to_null Set buffer parameter to null after releasing
 */
void release_buffer(SDL_GPUBuffer*& buffer, const bool set_buffer_to_null = true);
}
