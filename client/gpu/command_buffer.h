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

/* Prevent build errors during migration to vulkan */
#include "fence.h"

namespace gpu
{
typedef struct fence_t fence_t;
}

/* Command buffer operations */
namespace gpu
{
/**
 * Acquire command buffer
 *
 * @returns a command buffer, or NULL on failure
 */
[[nodiscard]] SDL_DEPRECATED SDL_GPUCommandBuffer* acquire_command_buffer();

/**
 * Submit a command buffer
 *
 * @param command_buffer Command buffer to submit
 *
 * @returns true on success, or false on failure
 */
SDL_DEPRECATED bool submit_command_buffer(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Submit a command buffer and get it's fence
 *
 * @param command_buffer Command buffer to submit
 *
 * @returns the fence handle associated with the command buffer on success, or NULL on failure
 */
[[nodiscard]] SDL_DEPRECATED fence_t* submit_command_buffer_and_acquire_fence(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Cancel command buffer
 *
 * @param command_buffer Command buffer to cancel
 *
 * @returns true on success, or false on failure
 */
SDL_DEPRECATED bool cancel_command_buffer(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Get the gpu::fence_t object associated with the command buffer
 *
 * @param command_buffer Commander buffer to get the associated fence for
 *
 * @return a fence handle, or NULL on failure
 */
[[nodiscard]] SDL_DEPRECATED fence_t* get_command_buffer_fence(const SDL_GPUCommandBuffer* const command_buffer);
}
