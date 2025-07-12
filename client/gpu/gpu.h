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

#include "buffer.h"
#include "command_buffer.h"
#include "sampler.h"
#include "subdiv_buffer.h"
#include "texture.h"

#include "volk/volk.h"

/** Die on an error from a function returning VkResult */
#define VK_DIE(_CALL)                                                                              \
    do                                                                                             \
    {                                                                                              \
        VkResult result__ = _CALL;                                                                 \
        if (result__ != VK_SUCCESS)                                                                \
            util::die("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult */
#define VK_TRY(_CALL)                                                                                 \
    do                                                                                                \
    {                                                                                                 \
        VkResult result__ = _CALL;                                                                    \
        if (result__ != VK_SUCCESS)                                                                   \
            dc_log_error("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult, and save the result */
#define VK_TRY_STORE(RESULT, _CALL)                                                               \
    do                                                                                            \
    {                                                                                             \
        RESULT = _CALL;                                                                           \
        if (RESULT != VK_SUCCESS)                                                                 \
            dc_log_error("%s failed with code: %d: %s", #_CALL, RESULT, string_VkResult(RESULT)); \
    } while (0)

namespace gpu
{
/**
 * Init GPU api
 *
 * This will either return on success, or exit the program on failure!
 */
void init();

/**
 * Run a little test app to verify some things are working
 *
 * `gpu::init()` must be called before calling this function
 */
void simple_test_app();

void quit();

extern SDL_Window* window;

extern VkInstance instance;
/** Value passed to VkApplicationInfo::apiVersion */
extern const Uint32 instance_api_version;

struct device_t;

struct frame_t
{
    device_t* device = nullptr;

    Uint32 image_idx = ~0;

    /** Target image */
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;

    /** Command buffer allocated for the graphics queue, from `window_t::graphics_pool` */
    VkCommandBuffer cmd_graphics = VK_NULL_HANDLE;
    bool used_graphics = 0;

    /** Command buffer allocated for the transfer queue, from `window_t::transfer_pool` */
    VkCommandBuffer cmd_transfer = VK_NULL_HANDLE;
    bool used_transfer = 0;

    /** Fence to be associated with command buffer submission */
    VkFence done = VK_NULL_HANDLE;

    /**
     * Acquire a semaphore from the internal buffer
     *
     * NOTE: You should not reference a semaphore outside of the command buffers
     *       provided by this object, as the semaphores may be destroyed/reused at any time
     */
    VkSemaphore acquire_semaphore();

    void reset();
    void free();

private:
    size_t next_semaphore_idx = 0;
    std::vector<VkSemaphore> semaphores;
};

struct window_t
{
    /* TODO: OpenXR handles here */
    // XrSwapchain xr_swapchain = XR_NULL_HANDLE;

    SDL_Window* sdl_window = nullptr;
    VkSurfaceKHR sdl_surface = VK_NULL_HANDLE;
    VkSwapchainKHR sdl_swapchain = VK_NULL_HANDLE;
    VkFence acquire_fence = VK_NULL_HANDLE;
    bool frame_is_pending = 0;

    bool swapchain_rebuild_required = 0;
    VkExtent2D extent {};
    VkSurfaceFormatKHR format {};
    VkPresentModeKHR present_mode {};

    VkCommandPool graphics_pool = VK_NULL_HANDLE;
    VkCommandPool transfer_pool = VK_NULL_HANDLE;

    /**
     * Called when the swapchain format is changed, or when the swapchain is initially created
     *
     * The intended usage is for recreating pipelines that target the swapchain
     *
     * When this callback is fired, `window_t::format` will contain the new format
     *
     * @param format_changed Set to true when the underlying image format has changed
     * @param colorspace_changed Set to true when the colorspace has changed
     * @param userdata Set to the value of `window_t::format_callback_userdata`
     */
    void (*format_callback)(const bool format_changed, const bool colorspace_changed, void* userdata) = nullptr;
    void* format_callback_userdata = nullptr;

    /**
     * Called when the number of swapchain frames changes, or when the swapchain is initially created
     *
     * The intended usage is for informing things like Dear ImGui's Vulkan backend that the number of images changed
     *
     * When this callback is fired, `window_t::frames` will contain the new frames
     *
     * @param num_images Contains the new number of frames
     * @param userdata Set to the value of `window_t::num_images_callback_userdata`
     */
    void (*num_images_callback)(const Uint32 num_images, void* userdata) = nullptr;
    void* num_images_callback_userdata = nullptr;

    std::vector<frame_t> frames;
};

struct device_t
{
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice logical = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

    /* ======================================================== */
    /* ================ Window/Swapchain stuff ================ */

    window_t window {};

    /**
     * Acquire a frame for rendering
     *
     * NOTE: This may resize/reconfigure the swapchain
     *
     * @returns A frame object on success, or NULL on failure/unavailability of a new frame
     */
    frame_t* acquire_next_frame(window_t* const window, const Uint64 timeout);

    /**
     * Submit a frame for presentation
     *
     * @param window the frame was acquired from
     * @param frame frame to submit (You must not reference the frame after calling this function)
     */
    void submit_frame(window_t* const window, frame_t* frame);

    /* ============================================= */
    /* ================ Queue stuff ================ */

    Uint32 graphics_queue_idx = ~0;
    Uint32 transfer_queue_idx = ~0;
    Uint32 present_queue_idx = ~0;

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue transfer_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    SDL_Mutex* graphics_queue_lock = nullptr;
    SDL_Mutex* transfer_queue_lock = nullptr;
    SDL_Mutex* present_queue_lock = nullptr;

    /** Convenience function to call SDL_LockMutex() on each queue lock */
    void lock_all_queues();

    /** Convenience function to call SDL_UnlockMutex() on each queue lock */
    void unlock_all_queues();

    /** Locks all queues, calls vkDeviceWaitIdle(), and then unlocks all queues  */
    void wait_for_device_idle();

    device_t(SDL_Window* sdl_window);
    ~device_t();

    VolkDeviceTable funcs;

private:
    void set_object_name_real(Uint64 object_handle, VkObjectType object_type, const char* fmt, va_list args);

public:
    /**
     * Wrapper around vkSetDebugUtilsObjectNameEXT(3)
     *
     * If vkSetDebugUtilsObjectNameEXT(3) is not available, this function is a safe a no-op
     *
     * @param object_handle Object handle (If object_handle == VK_NULL_HANDLE, this function is a safe a no-op)
     * @param object_type Object type
     * @param fmt stb_sprintf format string
     */
    template <typename T>
    void set_object_name(T object_handle, VkObjectType object_type, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(4)
    {
        va_list args;
        va_start(args, fmt);
        set_object_name_real(reinterpret_cast<Uint64>(object_handle), object_type, fmt, args);
        va_end(args);
    }

    friend struct frame_t;
};

extern device_t* device_new;

/** Convenience function to insert an image layout transition pipeline barrier */
void transition_image(VkCommandBuffer const command_buffer, VkImage const image, const VkImageLayout layout_old, const VkImageLayout layout_new);
}
