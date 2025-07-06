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
 */
void simple_test_app();

void quit();

extern SDL_Mutex* graphics_queue_lock;
extern SDL_Mutex* transfer_queue_lock;
extern SDL_Mutex* present_queue_lock;

extern VkQueue graphics_queue;
extern VkQueue transfer_queue;
extern VkQueue present_queue;

extern SDL_Window* window;

extern VkInstance instance;
/** Value passed to VkApplicationInfo::apiVersion */
extern const Uint32 instance_api_version;

struct device_t;

struct frame_t
{
    device_t* device = nullptr;

    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;

    /** Command buffer allocated for the graphics queue, from `window_t::graphics_pool` */
    VkCommandBuffer cmd_graphics = VK_NULL_HANDLE;
    bool used_graphics = 0;

    /** Command buffer allocated for the transfer queue, from `window_t::transfer_pool` */
    VkCommandBuffer cmd_transfer = VK_NULL_HANDLE;
    bool used_transfer = 0;

    VkFence done = VK_NULL_HANDLE;

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

    /* ======================================================== */
    /* ================ Window/Swapchain stuff ================ */

    window_t window {};

    /**
     * Acquire a frame for rendering
     *
     * WARNING: This may resize/reconfigure the swapchain
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

    friend struct frame_t;
};

extern device_t* device_new;

struct swapchain_info_t
{
    std::vector<VkSurfaceFormatKHR> formats;

    std::vector<VkPresentModeKHR> present_modes;

    VkSurfaceCapabilitiesKHR capabilities = {};
};

struct physical_device_info_t
{
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties2 props_10 = {};
    VkPhysicalDeviceVulkan11Properties props_11 = {};
    VkPhysicalDeviceVulkan12Properties props_12 = {};

    VkPhysicalDeviceFeatures2 features_10 = {};
    VkPhysicalDeviceVulkan11Features features_11 = {};
    VkPhysicalDeviceVulkan12Features features_12 = {};

    std::vector<VkExtensionProperties> extensions;

    /** Queue families available the the physical device */
    std::vector<VkQueueFamilyProperties> queue_families;

    bool has_graphics_queue = 0;
    bool has_transfer_queue = 0;
    bool has_present_queue = 0;

    Uint32 graphics_queue_idx = 0;
    Uint32 transfer_queue_idx = 0;
    Uint32 present_queue_idx = 0;

    physical_device_info_t();
};

/**  Device info associated with gpu::device */
extern physical_device_info_t device_info;
}
