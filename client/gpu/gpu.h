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

/** Locks all queues, calls vkDeviceWaitIdle, and then unlocks all queues  */
void wait_for_device_idle();

extern SDL_Window* window;
extern VkSurfaceKHR window_surface;
extern VkSwapchainKHR window_swapchain;
extern VkInstance instance;
/** Value passed to VkApplicationInfo::apiVersion */
extern const Uint32 instance_api_version;
extern VkDevice device;

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

    /**
     * Get current swapchain related info for a corresponding physical device and surface combination
     *
     * Silently fails on an error
     */
    swapchain_info_t get_current_swapchain_info(VkSurfaceKHR const surface) const;

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
