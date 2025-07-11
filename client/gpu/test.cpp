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
#include "gpu.h"

#include "shared/misc.h"
#include "tetra/gui/imgui/backends/imgui_impl_sdl3.h"
#include "tetra/gui/imgui/backends/imgui_impl_vulkan.h"
#include "tetra/tetra_core.h"
#include "tetra/util/convar.h"
#include "tetra/util/misc.h"

void gpu::simple_test_app()
{
    if (!device_new)
        util::die("GPU API not initialized, did you forget to call gpu::init()?");

    ImGui_ImplVulkan_InitInfo cinfo_imgui {};
    cinfo_imgui.ApiVersion = gpu::instance_api_version;
    cinfo_imgui.Instance = gpu::instance;
    cinfo_imgui.PhysicalDevice = gpu::device_new->physical;
    cinfo_imgui.Device = gpu::device_new->logical;

    cinfo_imgui.Queue = gpu::device_new->graphics_queue;
    cinfo_imgui.ImageCount = cinfo_imgui.MinImageCount = 2;

    cinfo_imgui.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1;
    cinfo_imgui.UseDynamicRendering = true;

    cinfo_imgui.QueueLockData = gpu::device_new->graphics_queue_lock;
    cinfo_imgui.QueueLockFn = [](void* m) { SDL_LockMutex(static_cast<SDL_Mutex*>(m)); };
    cinfo_imgui.QueueUnlockFn = [](void* m) { SDL_UnlockMutex(static_cast<SDL_Mutex*>(m)); };

    /* We set the initial format to one that probably won't be supported by the swapchain to force testing of `window_t::format_callback` */
    device_new->window.format.format = VK_FORMAT_R8G8B8A8_UNORM;
    cinfo_imgui.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    cinfo_imgui.PipelineRenderingCreateInfo.viewMask = 0;
    cinfo_imgui.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    cinfo_imgui.PipelineRenderingCreateInfo.pColorAttachmentFormats = &device_new->window.format.format;
    cinfo_imgui.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    cinfo_imgui.PipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    cinfo_imgui.MinAllocationSize = 256 * 1024;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForVulkan(gpu::window))
        util::die("Failed to initialize Dear ImGui SDL3 backend");
    if (!ImGui_ImplVulkan_Init(&cinfo_imgui))
        util::die("Failed to initialize Dear ImGui Vulkan backend");

    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, 1);

    device_new->window.num_images_callback = [](Uint32 image_count, void* userdata) {
        IM_UNUSED(userdata);

        dc_log("Swapchain image count changed");

        /* Tetra's ImGui Vulkan Backend has queue locking and does not requires external synchronization */
        ImGui_ImplVulkan_SetMinImageCount(image_count);
    };

    device_new->window.format_callback = [](const bool format_changed, const bool colorspace_changed, void* userdata) {
        IM_UNUSED(colorspace_changed);
        IM_UNUSED(userdata);
        VkPipelineRenderingCreateInfoKHR rendering_info {};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering_info.viewMask = 0;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &device_new->window.format.format;
        rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        if (format_changed)
            dc_log("Swapchain format changed");

        if (colorspace_changed)
            dc_log("Swapchain colorspace changed");

        /* Tetra's ImGui Vulkan Backend has queue locking and does not requires external synchronization */
        if (format_changed)
            ImGui_ImplVulkan_SetPipelineRenderingCreateInfo(&rendering_info);
    };

    bool done = 0;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                done = 1;
                break;
            }
        }

        frame_t* frame = gpu::device_new->acquire_next_frame(&gpu::device_new->window, UINT64_MAX);
        if (!frame)
            continue;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(nullptr);

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        VkCommandBufferBeginInfo binfo_command_buffer {};
        binfo_command_buffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        binfo_command_buffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        frame->used_graphics = 1;

        VK_DIE(vkBeginCommandBuffer(frame->cmd_graphics, &binfo_command_buffer));

        VkRenderingAttachmentInfo color_attachment {};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment.imageView = frame->image_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfoKHR binfo_rendering {};
        binfo_rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        binfo_rendering.renderArea.extent = device_new->window.extent;
        binfo_rendering.layerCount = 1;
        binfo_rendering.colorAttachmentCount = 1;
        binfo_rendering.pColorAttachments = &color_attachment;

        transition_image(frame->cmd_graphics, frame->image, VK_IMAGE_LAYOUT_UNDEFINED, color_attachment.imageLayout);
        vkCmdBeginRenderingKHR(frame->cmd_graphics, &binfo_rendering);

        ImGui_ImplVulkan_RenderDrawData(draw_data, frame->cmd_graphics, VK_NULL_HANDLE);

        vkCmdEndRenderingKHR(frame->cmd_graphics);
        transition_image(frame->cmd_graphics, frame->image, color_attachment.imageLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(frame->cmd_graphics);

        device_new->submit_frame(&device_new->window, frame);

        static tetra::iteration_limiter_t fps_cap(1);
        fps_cap.wait();
    }

    device_new->wait_for_device_idle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(ImGui::GetCurrentContext());
}
