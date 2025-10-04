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
#include "tetra/licenses.h"
#include "tetra/tetra_core.h"
#include "tetra/tetra_vulkan.h"
#include "tetra/util/convar.h"
#include "tetra/util/misc.h"

#include "shared/mcs_b181_projects.h"

struct test_image_data_t
{
    VkImage image = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkDescriptorSet imgui_descriptor = VK_NULL_HANDLE;
};

#define TEST_IMAGE_SIZE 32

static test_image_data_t* create_test_image(gpu::device_t* device)
{
    test_image_data_t* data = new test_image_data_t;

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D size = { TEST_IMAGE_SIZE, TEST_IMAGE_SIZE };

    /* Create image */
    {
        VkImageCreateInfo cinfo_image = {};
        cinfo_image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cinfo_image.imageType = VK_IMAGE_TYPE_2D;
        cinfo_image.format = format;
        cinfo_image.extent = { size.width, size.height, 1 };
        cinfo_image.mipLevels = 1;
        cinfo_image.arrayLayers = 1;
        cinfo_image.samples = VK_SAMPLE_COUNT_1_BIT;

        cinfo_image.tiling = VK_IMAGE_TILING_OPTIMAL;
        cinfo_image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        device->queue_sharing.apply(cinfo_image);
        cinfo_image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo cinfo_image_alloc = {};
        cinfo_image_alloc.usage = VMA_MEMORY_USAGE_AUTO;

        VK_DIE(vmaCreateImage(device->allocator, &cinfo_image, &cinfo_image_alloc, &data->image, &data->alloc, nullptr));
        device->set_object_name(data->image, VK_OBJECT_TYPE_IMAGE, "%s: Image", __FUNCTION__);
    }

    /* Create Image View */
    {
        VkImageViewCreateInfo cinfo_image_view = {};
        cinfo_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cinfo_image_view.image = data->image;
        cinfo_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        cinfo_image_view.format = format;
        cinfo_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cinfo_image_view.subresourceRange.baseMipLevel = 0;
        cinfo_image_view.subresourceRange.levelCount = 1;
        cinfo_image_view.subresourceRange.baseArrayLayer = 0;
        cinfo_image_view.subresourceRange.layerCount = 1;

        VK_DIE(device->vkCreateImageView(&cinfo_image_view, &data->view));
        device->set_object_name(data->view, VK_OBJECT_TYPE_IMAGE_VIEW, "%s: Image View", __FUNCTION__);
    }

    /* Create Sampler */
    {
        VkSamplerCreateInfo cinfo_sampler = {};
        cinfo_sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        cinfo_sampler.magFilter = VK_FILTER_NEAREST;
        cinfo_sampler.minFilter = VK_FILTER_LINEAR;
        cinfo_sampler.addressModeU = cinfo_sampler.addressModeV = cinfo_sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VK_DIE(device->vkCreateSampler(&cinfo_sampler, &data->sampler));
        device->set_object_name(data->sampler, VK_OBJECT_TYPE_SAMPLER, "%s: Sampler", __FUNCTION__);
    }

    /* Create command pool */
    VkCommandPoolCreateInfo cinfo_cmd_pool = {};
    cinfo_cmd_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cinfo_cmd_pool.queueFamilyIndex = device->transfer_queue.index;
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VK_DIE(device->vkCreateCommandPool(&cinfo_cmd_pool, &cmd_pool));

    /* Allocate command buffer */
    VkCommandBufferAllocateInfo ainfo_cmd_upload = {};
    ainfo_cmd_upload.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ainfo_cmd_upload.commandPool = cmd_pool;
    ainfo_cmd_upload.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ainfo_cmd_upload.commandBufferCount = 1;
    VkCommandBuffer cmd_upload = VK_NULL_HANDLE;
    VK_DIE(device->vkAllocateCommandBuffers(&ainfo_cmd_upload, &cmd_upload));

    /* Begin Command Buffer */
    VkCommandBufferBeginInfo binfo_cmd_upload = {};
    binfo_cmd_upload.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    binfo_cmd_upload.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_DIE(device->vkBeginCommandBuffer(cmd_upload, &binfo_cmd_upload));

    /* Create staging buffer */
    VkBufferCreateInfo cinfo_buffer = {};
    cinfo_buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cinfo_buffer.size = size.height * size.width * 4;
    cinfo_buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    cinfo_buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    cinfo_buffer.queueFamilyIndexCount = 1;
    cinfo_buffer.pQueueFamilyIndices = &device->transfer_queue.index;

    VmaAllocationCreateInfo cinfo_buffer_vma = {};
    cinfo_buffer_vma.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    // cinfo_buffer_vma.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    // cinfo_buffer_vma.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_buffer_allocation = VK_NULL_HANDLE;

    VK_DIE(vmaCreateBuffer(device->allocator, &cinfo_buffer, &cinfo_buffer_vma, &staging_buffer, &staging_buffer_allocation, nullptr));

    /* Fill staging buffer */
    {
        Uint8* buffer_data = nullptr;
        VK_DIE(vmaMapMemory(device->allocator, staging_buffer_allocation, (void**)&buffer_data));

        Uint64 r_seed = 0xf2776747714404b2;
        for (VkDeviceSize i = 0; i < cinfo_buffer.size; i++)
            buffer_data[i] = SDL_rand_r(&r_seed, 256);

        vmaUnmapMemory(device->allocator, staging_buffer_allocation);
    }

    /* Upload image */
    {
        device->transition_image(cmd_upload, data->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { size.height, size.width, 1 };

        vkCmdCopyBufferToImage(cmd_upload, staging_buffer, data->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        device->transition_image(cmd_upload, data->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VK_DIE(device->vkEndCommandBuffer(cmd_upload));

    /* Create fence for waiting on submit operation */
    VkFenceCreateInfo cinfo_wait_fence = {};
    cinfo_wait_fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence wait_fence = VK_NULL_HANDLE;
    VK_DIE(device->vkCreateFence(&cinfo_wait_fence, &wait_fence));

    /* Submit command buffer */
    VkSubmitInfo sinfo_cmd_upload = {};
    sinfo_cmd_upload.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sinfo_cmd_upload.commandBufferCount = 1;
    sinfo_cmd_upload.pCommandBuffers = &cmd_upload;

    SDL_LockMutex(device->transfer_queue.lock);
    VK_DIE(device->vkQueueSubmit(device->transfer_queue.handle, 1, &sinfo_cmd_upload, wait_fence));
    SDL_UnlockMutex(device->transfer_queue.lock);

    /* Wait for command buffer to complete submission */
    VK_DIE(device->vkWaitForFences(1, &wait_fence, 1, UINT64_MAX));

    /* Now that the image is prepared we can set up a descriptor through ImGui */
    data->imgui_descriptor = ImGui_ImplVulkan_AddTexture(data->sampler, data->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    /* Cleanup */
    device->vkDestroyFence(wait_fence);
    device->vkDestroyCommandPool(cmd_pool);
    vmaDestroyBuffer(device->allocator, staging_buffer, staging_buffer_allocation);

    return data;
}

static void destroy_test_image(gpu::device_t* device, test_image_data_t* data)
{
    if (data == nullptr)
        return;

    ImGui_ImplVulkan_RemoveTexture(data->imgui_descriptor);
    device->vkDestroySampler(data->sampler);
    device->vkDestroyImageView(data->view);
    vmaDestroyImage(device->allocator, data->image, data->alloc);

    delete data;
}

void gpu::simple_test_app()
{
    if (!device_new)
        util::die("GPU API not initialized, did you forget to call gpu::init()?");

    ImGui_ImplVulkan_InitInfo cinfo_imgui {};
    cinfo_imgui.ApiVersion = gpu::instance_api_version;
    cinfo_imgui.Instance = gpu::instance;
    cinfo_imgui.PhysicalDevice = gpu::device_new->physical;
    cinfo_imgui.Device = gpu::device_new->logical;

    cinfo_imgui.Queue = gpu::device_new->graphics_queue.handle;
    cinfo_imgui.QueueFamily = gpu::device_new->graphics_queue.index;
    cinfo_imgui.ImageCount = cinfo_imgui.MinImageCount = 2;

    cinfo_imgui.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1;
    cinfo_imgui.UseDynamicRendering = true;

    cinfo_imgui.PipelineCache = gpu::device_new->pipeline_cache;

    cinfo_imgui.QueueLockData = gpu::device_new->graphics_queue.lock;
    cinfo_imgui.QueueLockFn = [](void* m) { SDL_LockMutex(static_cast<SDL_Mutex*>(m)); };
    cinfo_imgui.QueueUnlockFn = [](void* m) { SDL_UnlockMutex(static_cast<SDL_Mutex*>(m)); };

    /* We set the initial format to one that probably won't be supported by the swapchain to force testing of `window_t::format_callback` */
    device_new->window.format.format = VK_FORMAT_R8G8B8A8_UNORM;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.viewMask = 0;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &device_new->window.format.format;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    cinfo_imgui.PipelineInfoMain.PipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    cinfo_imgui.MinAllocationSize = 256 * 1024;

    ImGuiContext* imgui_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context);

    ImGui::GetIO().IniFilename = nullptr;

    ImGui::StyleColorsDark();
    if (!ImGui_ImplSDL3_InitForVulkan(window))
        util::die("Failed to initialize Dear ImGui SDL3 backend");
    if (!ImGui_ImplVulkan_Init(&cinfo_imgui))
        util::die("Failed to initialize Dear ImGui Vulkan backend");

    /* Test tetra backend */
    {
        tetra::vulkan_backend_init_info_t cinfo_tetra = {};
        cinfo_tetra.window = window;

        cinfo_tetra.instance_api_version = gpu::instance_api_version;

        cinfo_tetra.instance = gpu::instance;
        cinfo_tetra.physical = gpu::device_new->physical;
        cinfo_tetra.device = gpu::device_new->logical;

        cinfo_tetra.queue_family = gpu::device_new->graphics_queue.index;
        cinfo_tetra.queue = gpu::device_new->graphics_queue.handle;
        cinfo_tetra.queue_lock = gpu::device_new->graphics_queue.lock;

        cinfo_tetra.image_count = 2;

        cinfo_tetra.pipeline_cache = gpu::device_new->pipeline_cache;

        cinfo_tetra.allocation_callbacks = gpu::device_new->allocation_callbacks;

        cinfo_tetra.pipeline_create_info = cinfo_imgui.PipelineInfoMain;

        if (vkCmdBeginDebugUtilsLabelEXT && vkCmdEndDebugUtilsLabelEXT)
        {
            cinfo_tetra.vkCmdBeginDebugUtilsLabelEXT = vkCmdBeginDebugUtilsLabelEXT;
            cinfo_tetra.vkCmdEndDebugUtilsLabelEXT = vkCmdEndDebugUtilsLabelEXT;
        }

        if (tetra::init_gui(cinfo_tetra))
            util::die("tetra::init_gui()");
        tetra::show_imgui_ctx_main(false);
    }

    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, 1);

    device_new->window.num_images_callback = [](Uint32 image_count, void* userdata) {
        IM_UNUSED(userdata);

        dc_log("Swapchain image count changed");

        /* Tetra's ImGui Vulkan Backend has queue locking and does not requires external synchronization */
        ImGui_ImplVulkan_SetMinImageCount(image_count);

        tetra::set_image_count(image_count);
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
        {
            ImGui_ImplVulkan_PipelineInfo cinfo = {};
            cinfo.PipelineRenderingCreateInfo = rendering_info;
            ImGui_ImplVulkan_CreateMainPipeline(&cinfo);

            tetra::set_pipeline_create_info(cinfo);
        }
    };

    test_image_data_t* test_image_data = create_test_image(device_new);

    bool done = 0;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            tetra::process_event(event);

            if (!tetra::imgui_ctx_main_wants_input())
            {
                ImGui::SetCurrentContext(imgui_context);
                ImGui_ImplSDL3_ProcessEvent(&event);
            }

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                done = 1;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(window))
                {
                    done = 1;
                    break;
                }
            }
        }

        frame_t* frame = gpu::device_new->acquire_next_frame(&gpu::device_new->window, UINT64_MAX);
        if (!frame)
            continue;

        tetra::start_frame(false);

        ImGui::SetCurrentContext(imgui_context);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        /* ImGui contents */
        {
            bool should_close = !done;

            ImGui::ShowDemoWindow(&should_close);

            ImGui::GetBackgroundDrawList()->AddImage(test_image_data->imgui_descriptor, ImVec2(0, 0), ImGui::GetMainViewport()->Size, ImVec2(0, 0),
                ImGui::GetMainViewport()->Size / ImVec2(TEST_IMAGE_SIZE * 8, TEST_IMAGE_SIZE * 8));

            ImGui::SetNextWindowSize(ImGui::CalcTextSize("x") * ImVec2(80, 30), ImGuiCond_FirstUseEver);
            ImGui::Begin("Licenses");

            tetra::projects_widgets(mcs_b181_projects, SDL_arraysize(mcs_b181_projects));

            tetra::projects_widgets(&tetra::project_tetra, 1);

            Uint32 num_projects = 0;
            tetra::projects_widgets(tetra::get_projects(num_projects), num_projects);

            done = !should_close;

            ImGui::End();
        }

        ImGui::ShowMetricsWindow();

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        VkCommandBufferBeginInfo binfo_command_buffer {};
        binfo_command_buffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        binfo_command_buffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        frame->used_graphics = 1;

        VK_DIE(device_new->vkBeginCommandBuffer(frame->cmd_graphics, &binfo_command_buffer));

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

        device_new->transition_image(frame->cmd_graphics, frame->image, VK_IMAGE_LAYOUT_UNDEFINED, color_attachment.imageLayout);
        device_new->vkCmdBeginRenderingKHR(frame->cmd_graphics, &binfo_rendering);

        ImGui_ImplVulkan_RenderDrawData(draw_data, frame->cmd_graphics, VK_NULL_HANDLE);

        tetra::render_frame(frame->cmd_graphics);

        device_new->vkCmdEndRenderingKHR(frame->cmd_graphics);
        device_new->transition_image(frame->cmd_graphics, frame->image, color_attachment.imageLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        device_new->vkEndCommandBuffer(frame->cmd_graphics);

        device_new->submit_frame(&device_new->window, frame);

        static tetra::iteration_limiter_t fps_cap(1);
        fps_cap.wait();
    }

    device_new->wait_for_device_idle();

    destroy_test_image(device_new, test_image_data);

    ImGui::SetCurrentContext(imgui_context);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(imgui_context);
    imgui_context = nullptr;

    tetra::deinit_gui();
}
