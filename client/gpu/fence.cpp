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
#include "fence.h"

#include "gpu.h"

#include "tetra/log.h"
#include <SDL3/SDL_atomic.h>
#include <memory>
#include <vector>

#define DEBUG_USE_AFTER_FREE 0
#if DEBUG_USE_AFTER_FREE
#warning "DEBUG_USE_AFTER_FREE enabled! Serious memory leaks will occur!"
#endif
#if defined(NDEBUG) && DEBUG_USE_AFTER_FREE
#error "DEBUG_USE_AFTER_FREE shouldn't be used on release builds! Serious memory leaks will occur!"
#endif

static SDL_AtomicInt num_active_fences = {};

enum fence_state_t
{
    FENCE_STATE_CREATED,
    FENCE_STATE_CANCELED,
};

namespace gpu
{
struct fence_t
{
    SDL_AtomicInt ref_counter = { 1 };

    /** Boolean value: Set by submissions or cancellations */
    SDL_AtomicInt state = { FENCE_STATE_CREATED };

    VkSemaphore handle = VK_NULL_HANDLE;

    std::vector<std::pair<void (*)(void*), void*>> destruction_callbacks;
};
}

gpu::fence_t* gpu::create_fence()
{
    fence_t* fence = new fence_t {};

    VkSemaphoreTypeCreateInfo cinfo_timeline = {};
    cinfo_timeline.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    cinfo_timeline.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    cinfo_timeline.initialValue = 0;

    VkSemaphoreCreateInfo cinfo = {};
    cinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    cinfo.pNext = &cinfo_timeline;
    VkResult result = VK_RESULT_MAX_ENUM;
    VK_TRY_STORE(result, device->vkCreateSemaphore(&cinfo, &fence->handle));

    if (result != VK_SUCCESS)
    {
        delete fence;
        return nullptr;
    }

    SDL_AddAtomicInt(&num_active_fences, 1);

    return fence;
}

VkSemaphore gpu::get_fence_handle(fence_t* const fence) { return fence->handle; }

void gpu::ref_fence(fence_t* const fence, const Uint32 count)
{
    SDL_assert(SDL_GetAtomicInt(&fence->ref_counter) > 0);

    if (fence && count)
        SDL_AddAtomicInt(&fence->ref_counter, count);
}

void gpu::release_fence(fence_t*& fence, const bool set_to_null, Uint32 count)
{
    if (!fence || !count)
        return;

    if (DEBUG_USE_AFTER_FREE)
        SDL_assert_always(SDL_GetAtomicInt(&fence->ref_counter) > 0);

    if (!(SDL_AddAtomicInt(&fence->ref_counter, -count) == 1))
    {
        if (set_to_null)
            fence = nullptr;
        return;
    }

    if (DEBUG_USE_AFTER_FREE)
        dc_log_warn("Leaking fence!");
    else
    {
        device->vkDestroySemaphore(fence->handle);
        for (auto& it : fence->destruction_callbacks)
            it.first(it.second);
        delete fence;
        SDL_AddAtomicInt(&num_active_fences, -1);
    }

    if (set_to_null)
        fence = nullptr;
}
bool gpu::is_fence_cancelled(fence_t* const fence) { return SDL_GetAtomicInt(&fence->state) == FENCE_STATE_CANCELED; }
bool gpu::is_fence_done(fence_t* const fence)
{
    Uint64 value = 0;
    return SDL_GetAtomicInt(&fence->state) != FENCE_STATE_CANCELED && device->vkGetSemaphoreCounterValue(fence->handle, &value) == VK_SUCCESS && value;
}
bool gpu::wait_for_fence(fence_t* const fence) { return gpu::wait_for_fences(1, &fence, 1); }

void gpu::cancel_fence(fence_t* const fence)
{
    Uint64 value = ~0;
    device->vkGetSemaphoreCounterValue(fence->handle, &value);
    if (value)
        return;
    VkSemaphoreSignalInfo sinfo = {};
    sinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    sinfo.semaphore = fence->handle;
    sinfo.value = 1;
    VK_DIE(device->vkSignalSemaphore(&sinfo));

    SDL_SetAtomicInt(&fence->state, FENCE_STATE_CANCELED);
}

void gpu::add_destruction_callback(fence_t* const fence, void (*cb)(void*), void* userdata) { fence->destruction_callbacks.push_back(std::pair(cb, userdata)); }

bool gpu::wait_for_fences(const bool wait_all, fence_t* const* fences, const Uint32 num_fences)
{
    VkSemaphore* handles = SDL_stack_alloc(VkSemaphore, num_fences);
    Uint64* values = SDL_stack_alloc(Uint64, num_fences);

    for (Uint32 i = 0; i < num_fences; i++)
    {
        handles[i] = fences[i]->handle;
        values[i] = 1;
    }

    VkSemaphoreWaitInfo winfo = {};
    winfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    if (!wait_all)
        winfo.flags = VK_SEMAPHORE_WAIT_ANY_BIT;
    winfo.semaphoreCount = num_fences;
    winfo.pSemaphores = handles;
    winfo.pValues = values;

    return device->vkWaitSemaphores(&winfo, UINT64_MAX) == VK_SUCCESS;
}

/* Tests */
#ifndef MCS_B181__STRIP_TESTS
namespace gpu
{
#define tassert(expr)   \
    do                  \
    {                   \
        if (!(expr))    \
            failed = 1; \
    } while (0)

static std::vector<std::pair<bool (*)(void), const char*>> tests;
struct test_t
{
    test_t(bool (*cb)(void), const char* name) { tests.push_back(std::pair(cb, name)); }
};

#define REGISTER_TEST(func) static test_t register_##func(func, #func);

static void destruction_callback_set_bool(void* userdata) { *(bool*)userdata = 1; }

static bool test_fences__callback()
{
    bool failed = 0;
    fence_t* fence = create_fence();

    bool destroyed = 0;
    gpu::add_destruction_callback(fence, destruction_callback_set_bool, &destroyed);

    gpu::release_fence(fence, true);

    tassert(destroyed);

    return failed;
}
REGISTER_TEST(test_fences__callback)

static bool test_fences__refcount()
{
    bool failed = 0;
    fence_t* fence = create_fence();

    bool destroyed = 0;
    gpu::add_destruction_callback(fence, destruction_callback_set_bool, &destroyed);

    gpu::ref_fence(fence);
    gpu::release_fence(fence, false);

    tassert(!destroyed);

    gpu::release_fence(fence, true);

    tassert(destroyed);

    return failed;
}
REGISTER_TEST(test_fences__refcount)

static bool test_fences__release()
{
    bool failed = 0;
    fence_t* fence = nullptr;

    /* Test set_to_null = true */
    fence = create_fence();
    release_fence(fence, true);
    tassert(fence == nullptr);
    fence = nullptr;

    /* Test set_to_null = false */
    fence = create_fence();
    release_fence(fence, false);
    tassert(fence != nullptr);
    fence = nullptr;

    return failed;
}
REGISTER_TEST(test_fences__release)

static bool test_fences__create()
{
    bool failed = 0;
    fence_t* fence = nullptr;

    /* Test set_to_null = true */
    fence = create_fence();
    tassert(fence != nullptr);

    tassert(!gpu::is_fence_cancelled(fence));
    tassert(!gpu::is_fence_done(fence));

    release_fence(fence, true);
    tassert(fence == nullptr);

    return failed;
}
REGISTER_TEST(test_fences__create)

static bool test_fences__cancel()
{
    bool failed = 0;
    fence_t* fence = create_fence();

    gpu::cancel_fence(fence);

    tassert(gpu::is_fence_cancelled(fence));
    tassert(!gpu::is_fence_done(fence));

    release_fence(fence, true);

    return failed;
}
REGISTER_TEST(test_fences__cancel)

static bool test_fences__signaled()
{
    bool failed = 0;
    fence_t* fence = create_fence();

    VkSemaphoreSignalInfo sinfo = {};
    sinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    sinfo.semaphore = fence->handle;
    sinfo.value = 1;
    device->vkSignalSemaphore(&sinfo);

    tassert(!gpu::is_fence_cancelled(fence));
    tassert(gpu::is_fence_done(fence));

    release_fence(fence, true);

    return failed;
}
REGISTER_TEST(test_fences__signaled)

static bool test_fences__waiting()
{
    struct test_state_t
    {
        SDL_AtomicInt counter;

        int timestamp_signal_single;
        int timestamp_signal_multi_wait_one;
        int timestamp_signal_multi_wait_all;

        int timestamp_unwait_single;
        int timestamp_unwait_multi_wait_one;
        int timestamp_unwait_multi_wait_all;

        bool destruction_callback_called;
        fence_t* fences[5];

    } state = {};

    for (fence_t*& f : state.fences)
        f = create_fence();

    std::vector<SDL_Thread*> threads;

    dc_log("%lu: Main thread", SDL_GetCurrentThreadID());

#define TEST_NAME "Test single wait"
    threads.push_back(SDL_CreateThread(
        [](void* userdata) -> int {
            dc_log("%lu: %s", SDL_GetCurrentThreadID(), TEST_NAME);
            test_state_t* state = static_cast<test_state_t*>(userdata);
            wait_for_fence(state->fences[0]);

            state->timestamp_unwait_single = SDL_AddAtomicInt(&state->counter, 1);

            return 0;
        },
        TEST_NAME, &state));
#undef TEST_NAME

#define TEST_NAME "Test multi wait, wait_all = false"
    threads.push_back(SDL_CreateThread(
        [](void* userdata) -> int {
            dc_log("%lu: %s", SDL_GetCurrentThreadID(), TEST_NAME);
            test_state_t* state = static_cast<test_state_t*>(userdata);
            wait_for_fences(false, state->fences + 1, 2);

            state->timestamp_unwait_multi_wait_one = SDL_AddAtomicInt(&state->counter, 1);
            return 0;
        },
        TEST_NAME, &state));
#undef TEST_NAME

#define TEST_NAME "Test multi wait, wait_all = true"
    threads.push_back(SDL_CreateThread(
        [](void* userdata) -> int {
            dc_log("%lu: %s", SDL_GetCurrentThreadID(), TEST_NAME);
            test_state_t* state = static_cast<test_state_t*>(userdata);
            wait_for_fences(true, state->fences, SDL_arraysize(state->fences));

            state->timestamp_unwait_multi_wait_all = SDL_AddAtomicInt(&state->counter, 1);

            return 0;
        },
        TEST_NAME, &state));
#undef TEST_NAME

    for (SDL_Thread* t : threads)
        SDL_DetachThread(t);

    SDL_Delay(1000);

    bool failed = 0;

#define LOG_AND_CHECK(NAME)                                                                                                             \
    do                                                                                                                                  \
    {                                                                                                                                   \
        bool test_failed = !state.timestamp_signal_##NAME + 1 == state.timestamp_unwait_##NAME;                                         \
        if (test_failed)                                                                                                                \
            failed = 1;                                                                                                                 \
        dc_log("Signal: %d, Response: %d, Test failed: %d", state.timestamp_signal_##NAME, state.timestamp_unwait_##NAME, test_failed); \
    } while (0)

    state.timestamp_signal_single = SDL_AddAtomicInt(&state.counter, 1);
    cancel_fence(state.fences[0]);
    SDL_Delay(1000);
    LOG_AND_CHECK(single);

    state.timestamp_signal_multi_wait_one = SDL_AddAtomicInt(&state.counter, 1);
    cancel_fence(state.fences[1]);
    SDL_Delay(1000);
    LOG_AND_CHECK(multi_wait_one);

    state.timestamp_signal_multi_wait_all = SDL_AddAtomicInt(&state.counter, 1);
    for (fence_t* f : state.fences)
        cancel_fence(f);
    SDL_Delay(1000);
    LOG_AND_CHECK(multi_wait_all);

    for (fence_t*& f : state.fences)
        release_fence(f);
#undef LOG_AND_CHECK

    return failed;
}
REGISTER_TEST(test_fences__waiting)
}
#endif /* #ifndef MCS_B181__STRIP_TESTS */

bool gpu::test_fences()
{
    bool failed = 0;
#ifndef MCS_B181__STRIP_TESTS

    for (auto& it : tests)
    {
        dc_log("Test \"%s\" running", it.second);

        if (it.first())
        {
            dc_log("Test \"%s\" failed", it.second);
            failed = 1;
        }
        else
            dc_log("Test \"%s\" passed", it.second);
    }

    if (failed)
        dc_log("Fence tests failed");
    else
        dc_log("Fence tests passed");
#endif /* #ifndef MCS_B181__STRIP_TESTS */

    return !failed;
}
