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

#include "../state.h"
#include "tetra/log.h"
#include <map>
#include <vector>

#define DEBUG_USE_AFTER_FREE 0
#if DEBUG_USE_AFTER_FREE
#warning "DEBUG_USE_AFTER_FREE enabled!"
#endif
#if defined(NDEBUG) && DEBUG_USE_AFTER_FREE
#error "DEBUG_USE_AFTER_FREE shouldn't be used on release builds!"
#endif

struct gpu::fence_t
{
    SDL_AtomicInt ref_counter = { 1 };

    /** Boolean value: Set by submissions or cancellations */
    SDL_AtomicInt submitted = { 0 };

    /** When submitted == true this is valid */
    SDL_GPUFence* fence = nullptr;

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
            SDL_ReleaseGPUFence(state::gpu_device, fence);
            delete this;
        }
    }

    bool is_done() { return SDL_GetAtomicInt(&submitted) && fence != nullptr && SDL_QueryGPUFence(state::gpu_device, fence); }

    bool is_submitted_but_not_done() { return SDL_GetAtomicInt(&submitted) && fence != nullptr && !SDL_QueryGPUFence(state::gpu_device, fence); }

    bool is_cancelled() { return SDL_GetAtomicInt(&submitted) && fence == nullptr; }
};

static std::map<const SDL_GPUCommandBuffer*, gpu::fence_t*> fence_map;
static SDL_RWLock* fence_map_lock = nullptr;

void gpu::init()
{
    quit();
    fence_map_lock = SDL_CreateRWLock();
}

void gpu::quit()
{
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
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state::gpu_device);

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

void gpu::ref_fence(fence_t* const fence, const Uint32 count)
{
    if (fence && count)
        SDL_AddAtomicInt(&fence->ref_counter, count);
}

void gpu::release_fence(fence_t* const fence, Uint32 count)
{
    if (fence && count)
        fence->release(count);
}
bool gpu::is_fence_cancelled(fence_t* const fence) { return fence->is_cancelled(); }
bool gpu::is_fence_done(fence_t* const fence) { return fence->is_done(); }
bool gpu::wait_for_fence(fence_t* const fence) { return gpu::wait_for_fences(1, &fence, 1); }

/* This isn't a very efficient algorithm, but it works well enough */
bool gpu::wait_for_fences(const bool wait_all, fence_t* const* fences, const Uint32 num_fences)
{
    if (wait_all)
    {
        std::vector<Uint32> to_be_signaled;
        to_be_signaled.resize(num_fences);
        for (Uint32 i = 0; i < num_fences; i++)
            to_be_signaled[i] = i;

        while (to_be_signaled.size())
        {
            for (auto it = to_be_signaled.begin(); it != to_be_signaled.end();)
            {
                if (fences[*it]->is_cancelled() || fences[*it]->is_done())
                    it = to_be_signaled.erase(it);
                else
                    ++it;
            }
            if (to_be_signaled.size())
                SDL_Delay(10);
        }
        return true;
    }
    else
    {
        while (true)
        {
            for (Uint32 i = 0; i < num_fences; i++)
            {
                if (fences[i]->is_cancelled())
                    return true;
                if (fences[i]->is_done())
                    return true;
            }
            SDL_Delay(10);
        }
    }
}
