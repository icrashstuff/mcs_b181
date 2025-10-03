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
 *
 * This backend works a bit differently from the others, in that it does
 * not create the window or initialize the graphics API.
 *
 * This backend has the following requirements:
 * - Dynamic rendering
 *   - Provided by: VK_KHR_dynamic_rendering or Vulkan 1.3
 *
 * This backend has the following optional requirements:
 * - VK_EXT_debug_utils
 */

#pragma once

#include <SDL3/SDL.h>

#include "vulkan/vulkan.h"

#include "tetra/gui/imgui/backends/imgui_impl_vulkan.h"
#include "tetra/util/convar.h"

namespace tetra
{
/**
 * Boolean value for if vsync should be enabled or not
 */
extern convar_int_t r_vsync;

/** Main ImGui context */
extern ImGuiContext* im_ctx_main;

/** Overlay ImGui context */
extern ImGuiContext* im_ctx_overlay;

/**
 * Struct must be zero-initialized
 *
 * @see tetra::init_gui()
 */
struct vulkan_backend_init_info_t
{
    /** Window to use (Ideally this window should be hidden when tetra::init_gui() is called, as this function will move/resize the window) */
    SDL_Window* window;

    Uint32 instance_api_version;

    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;

    Uint32 queue_family;
    VkQueue queue;
    SDL_Mutex* queue_lock;

    /**
     * Number of images in swapchain
     *
     * @see tetra::set_image_count() to change dynamically
     */
    Uint32 image_count;

    /** Optional */
    VkPipelineCache pipeline_cache;

    /** Optional */
    const VkAllocationCallbacks* allocation_callbacks;

    ImGui_ImplVulkan_PipelineInfo pipeline_create_info;

    /** (Optional) Used by `tetra::end_frame()` to insert a debug label region */
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    /** (Optional) Used by `tetra::end_frame()` to end a debug label region */
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
};

/**
 * NOTE: You must call ImGui_ImplVulkan_LoadFunctions() before using this function
 *
 * @param init_info Init info
 *
 * @see tetra::vulkan_backend_init_info_t
 *
 * @returns 0 on successful init, some non-zero value on failure (Though it will likely call util::die() if an error occurs)
 */
int init_gui(const vulkan_backend_init_info_t& init_info);

/**
 * Deinit gui, call this before tetra::deinit()
 */
void deinit_gui();

/**
 * Recreates main ImGui pipeline
 *
 * @see tetra::vulkan_backend_init_info_t::pipeline_create_info
 * @see ImGui_ImplVulkan_CreateMainPipeline()
 */
void set_pipeline_create_info(const ImGui_ImplVulkan_PipelineInfo& info);

/**
 * Sets swapchain image count
 *
 * @see tetra::vulkan_backend_init_info_t::image_count
 * @see ImGui_ImplVulkan_SetMinImageCount()
 */
void set_image_count(const Uint32 image_count);

/**
 * Returns -1 on failure, 0 for application should exit, 1 for application should continue
 *
 * @param event_loop Handle SDL events inside start_frame()
 */
int start_frame(bool event_loop = true);

/**
 * Feed events to ImGui
 *
 * Returns true if application should exit, false otherwise
 */
bool process_event(const SDL_Event& event);

/**
 * Change visibility of main ImGui context
 *
 * This works by not feeding the context any events and discarding all render data
 *
 * NOTE: If the dev console is shown it will take priority over values set through here
 * NOTE: gui_registrar::render_menus() is still called
 */
void show_imgui_ctx_main(bool shown);

/**
 * @returns True if either the main ImGui context is shown or the dev console is forcing it, False otherwise
 */
bool imgui_ctx_main_wants_input();

/**
 * Change visibility of overlay ImGui context
 *
 * This works by discarding all render data
 *
 * NOTE: gui_registrar::render_overlays() is still called
 */
void show_imgui_ctx_overlay(bool shown);

/**
 * Renders the frame
 *
 * @param command_buffer Command buffer to render on, must have an active dynamic rendering pass
 */
void render_frame(VkCommandBuffer const command_buffer);

/**
 * Limits framerate via an instance of tetra::iteration_limiter_t
 *
 * This function will attempt to ensure that two calls are spaced at least '(1000.0f / r_fps_limiter.get())' ms apart
 */
void limit_framerate();
}
