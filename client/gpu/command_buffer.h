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

namespace gpu
{
typedef struct fence_t fence_t;

void init();

void quit();
}

/* Command buffer operations */
namespace gpu
{
/**
 * Acquire command buffer
 *
 * @returns a command buffer, or NULL on failure
 */
[[nodiscard]] SDL_GPUCommandBuffer* acquire_command_buffer();

/**
 * Submit a command buffer
 *
 * @param command_buffer Command buffer to submit
 *
 * @returns true on success, or false on failure
 */
bool submit_command_buffer(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Submit a command buffer and get it's fence
 *
 * @param command_buffer Command buffer to submit
 *
 * @returns the fence handle associated with the command buffer on success, or NULL on failure
 */
[[nodiscard]] fence_t* submit_command_buffer_and_acquire_fence(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Cancel command buffer
 *
 * @param command_buffer Command buffer to cancel
 *
 * @returns true on success, or false on failure
 */
bool cancel_command_buffer(SDL_GPUCommandBuffer* const command_buffer);

/**
 * Get the gpu::fence_t object associated with the command buffer
 *
 * @param command_buffer Commander buffer to get the associated fence for
 *
 * @return a fence handle, or NULL on failure
 */
[[nodiscard]] fence_t* get_command_buffer_fence(const SDL_GPUCommandBuffer* const command_buffer);
}

/* Fence operations */
namespace gpu
{
/**
 * Check if a fence has been signaled
 *
 * NOTE: If a fence was canceled, then this will return false
 *
 * @param fence Fence to check
 *
 * @return true if fence has been signaled, or false if not
 */
[[nodiscard]] bool is_fence_done(fence_t* const fence);

/**
 * Check if the command buffer associated with the fence was canceled
 *
 * @param fence Fence to check
 *
 * @return true if fence has been canceled, or false if not
 */
[[nodiscard]] bool is_fence_cancelled(fence_t* const fence);

/**
 * Release a fence handle acquired by either gpu::submit_command_buffer_and_acquire_fence() or gpu::get_command_buffer_fence()
 *
 * You must not reference the fence after calling this function.
 *
 * @param fence Fence to release (NULL is a safe no-op)
 */
void release_fence(fence_t* const fence);

/**
 * Wait on a single fence
 *
 * Convenience wrapper around gpu::wait_for_fences()
 *
 * @param fence Fence to wait on
 *
 * @returns true on success, or false on failure
 */
bool wait_for_fence(fence_t* const fence);

/**
 * Wait on fence(s)
 *
 * @param wait_all Wait for all fences to be done or cancelled
 * @param fences Array of fences to wait on
 * @param num_fences Size of fences array
 *
 * @returns true on success, or false on failure
 */
bool wait_for_fences(const bool wait_all, fence_t* const* fences, const Uint32 num_fences);
}
