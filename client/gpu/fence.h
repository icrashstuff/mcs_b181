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

#include <SDL3/SDL_stdinc.h>

#include "volk/volk.h"

namespace gpu
{
/* Forward declarations */
struct device_t;
}

/* Fence operations */
namespace gpu
{
/**
 * Fence object
 *
 * TODO: Destruction list for Vulkan objects?
 * TODO: release_fence_async() to handle expensive destruction callbacks?
 *
 * Internally this uses a timeline semaphore, because you can't signal a fence after waiting on it, *sigh*
 */
struct fence_t;

/**
 * Create a fence object
 *
 * @returns A fence object with a reference count of 1, or nullptr on failure
 */
[[nodiscard]] gpu::fence_t* create_fence();

/**
 * Get underlying Vulkan semaphore
 *
 * @param fence Fence to retrieve handle from
 *
 * @returns VkSemaphore handle associated with fence
 */
[[nodiscard]] VkSemaphore get_fence_handle(gpu::fence_t* const fence);

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
 * Increment fence reference counter
 *
 * @param fence Fence to modify (NULL is a safe no-op)
 * @param count Amount to increment reference counter by
 */
void ref_fence(fence_t* const fence, const Uint32 count = 1);

/**
 * Release a fence handle
 *
 * WARNING: This may call destruction callbacks
 *
 * You must not reference the fence after calling this function.
 *
 * @param fence Fence to release (NULL is a safe no-op)
 * @param set_to_null Set fence parameter to NULL
 * @param count Amount to decrement reference counter by
 */
void release_fence(fence_t*& fence, const bool set_to_null = true, const Uint32 count = 1);

/**
 * Add callback to be called when the fence reference counter equals 0
 *
 * @param fence Fence to add callback to
 * @param cb Callback
 * @param userdata Userdata to be passed to the callback
 */
void add_destruction_callback(fence_t* const fence, void (*cb)(void* userdata), void* userdata);

/**
 * Mark a fence as canceled
 *
 * NOTE: This will signal the underlying fence
 *
 * @param fence Fence to cancel
 *
 * @returns true on success, or false on failure
 */
void cancel_fence(fence_t* const fence);

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

/**
 * Very crude tests of basic functionality of fences
 */
bool test_fences();
}
