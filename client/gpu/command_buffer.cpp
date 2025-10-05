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
#include "command_buffer.h"

#include "gpu.h"

#include "internal.h"
#include <map>
#include <vector>

#define DEBUG_USE_AFTER_FREE 0
#if DEBUG_USE_AFTER_FREE
#warning "DEBUG_USE_AFTER_FREE enabled!"
#endif
#if defined(NDEBUG) && DEBUG_USE_AFTER_FREE
#error "DEBUG_USE_AFTER_FREE shouldn't be used on release builds!"
#endif

static SDL_AtomicInt num_fences = {};

struct gpu::fence_t
{
    SDL_AtomicInt ref_counter = { 1 };

    /** Boolean value: Set by submissions or cancellations */
    SDL_AtomicInt submitted = { 0 };

    /** When submitted == true this is valid */
    SDL_GPUFence* fence = nullptr;

    fence_t() { SDL_AddAtomicInt(&num_fences, 1); }
    ~fence_t() { SDL_AddAtomicInt(&num_fences, -1); }

    void release(int count)
    {
        if (DEBUG_USE_AFTER_FREE)
            SDL_assert_always(SDL_GetAtomicInt(&ref_counter) > 0);

        if (!(SDL_AddAtomicInt(&ref_counter, -count) == 1))
            return;

        if (DEBUG_USE_AFTER_FREE)
            dc_log_warn("Leaking fence!");
        else
        {
            SDL_ReleaseGPUFence(state::sdl_gpu_device, fence);
            delete this;
        }
    }

    bool is_done() { return SDL_GetAtomicInt(&submitted) && fence != nullptr && SDL_QueryGPUFence(state::sdl_gpu_device, fence); }

    bool is_submitted_but_not_done() { return SDL_GetAtomicInt(&submitted) && fence != nullptr && !SDL_QueryGPUFence(state::sdl_gpu_device, fence); }

    bool is_cancelled() { return SDL_GetAtomicInt(&submitted) && fence == nullptr; }
};

static std::map<const SDL_GPUCommandBuffer*, gpu::fence_t*> fence_map;
static SDL_RWLock* fence_map_lock = nullptr;

void gpu::internal::init_gpu_fences()
{
    internal::quit_gpu_fences();
    fence_map_lock = SDL_CreateRWLock();
}

void gpu::internal::quit_gpu_fences()
{
    if (SDL_GetAtomicInt(&num_fences) != 0)
        dc_log_warn("%d fence(s) were leaked!", SDL_GetAtomicInt(&num_fences));
    SDL_SetAtomicInt(&num_fences, 0);

    if (fence_map.size())
        dc_log_warn("%zu command_buffers(s) were leaked!", fence_map.size());
    fence_map.clear();

    SDL_DestroyRWLock(fence_map_lock);
    fence_map_lock = nullptr;
}

static gpu::fence_t* pop_fence(const SDL_GPUCommandBuffer* const command_buffer)
{
    SDL_LockRWLockForWriting(fence_map_lock);

    auto it = fence_map.find(command_buffer);
    if (it == fence_map.end())
    {
        SDL_UnlockRWLock(fence_map_lock);
        return nullptr;
    }
    gpu::fence_t* fence = it->second;
    fence_map.erase(it);
    SDL_UnlockRWLock(fence_map_lock);

    return fence;
}

SDL_GPUCommandBuffer* gpu::acquire_command_buffer()
{
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state::sdl_gpu_device);

    if (!command_buffer)
        dc_log_error("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());

    if (!command_buffer)
        return command_buffer;

    SDL_LockRWLockForWriting(fence_map_lock);
    fence_map[command_buffer] = new gpu::fence_t();
    SDL_UnlockRWLock(fence_map_lock);

    return command_buffer;
}

bool gpu::cancel_command_buffer(SDL_GPUCommandBuffer* const command_buffer)
{
    bool ret = SDL_CancelGPUCommandBuffer(command_buffer);

    if (!ret)
        dc_log_error("SDL_CancelGPUCommandBuffer failed: %s", SDL_GetError());

    fence_t* fence = pop_fence(command_buffer);
    if (!fence)
        return ret;

    SDL_SetAtomicInt(&fence->submitted, 1);

    /* Fences are created with their ref count set to 1 */
    release_fence(fence);

    return ret;
}

bool gpu::submit_command_buffer(SDL_GPUCommandBuffer* const command_buffer)
{
    gpu::fence_t* fence = submit_command_buffer_and_acquire_fence(command_buffer);

    /* Fences are created with their ref count set to 1 */
    release_fence(fence);

    return fence;
}

gpu::fence_t* gpu::submit_command_buffer_and_acquire_fence(SDL_GPUCommandBuffer* const command_buffer)
{
    gpu::fence_t* fence = pop_fence(command_buffer);

    fence->fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (!fence->fence)
        dc_log_error("SDL_SubmitGPUCommandBufferAndAcquireFence failed: %s", SDL_GetError());
    SDL_SetAtomicInt(&fence->submitted, 1);

    return fence;
}

gpu::fence_t* gpu::get_command_buffer_fence(const SDL_GPUCommandBuffer* const command_buffer)
{
    gpu::fence_t* fence = nullptr;

    SDL_LockRWLockForReading(fence_map_lock);
    auto it = fence_map.find(command_buffer);
    if (it != fence_map.end())
        fence = it->second;
    SDL_assert(fence);
    SDL_UnlockRWLock(fence_map_lock);

    gpu::ref_fence(fence);

    return fence;
}
