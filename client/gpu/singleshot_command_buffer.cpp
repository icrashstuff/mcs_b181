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

#include "singleshot_command_buffer.h"

#include "gpu.h"

gpu::singleshot_cmd_buffer_t::singleshot_cmd_buffer_t(queue_t* const queue)
    : queue(queue)
    , fence(create_fence())
{
    if (fence == nullptr)
        util::die("Unable to create fence");

    /* Create command pool */
    VkCommandPoolCreateInfo cinfo_pool = {};
    cinfo_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cinfo_pool.queueFamilyIndex = queue->index;
    VK_DIE(device->vkCreateCommandPool(&cinfo_pool, &pool));

    device->set_object_name(pool, VK_OBJECT_TYPE_COMMAND_POOL, "singleshot_cmd_buffer_t::pool");

    /* Allocate command buffer */
    VkCommandBufferAllocateInfo ainfo_cmd_upload = {};
    ainfo_cmd_upload.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ainfo_cmd_upload.commandPool = pool;
    ainfo_cmd_upload.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ainfo_cmd_upload.commandBufferCount = 1;
    VK_DIE(device->vkAllocateCommandBuffers(&ainfo_cmd_upload, &cmd));

    device->set_object_name(cmd, VK_OBJECT_TYPE_COMMAND_BUFFER, "singleshot_cmd_buffer_t::cmd");

    /* Begin Command Buffer */
    VkCommandBufferBeginInfo binfo_cmd_upload = {};
    binfo_cmd_upload.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    binfo_cmd_upload.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_DIE(device->vkBeginCommandBuffer(cmd, &binfo_cmd_upload));
}

gpu::fence_t* gpu::singleshot_cmd_buffer_t::end_and_submit()
{
    VK_DIE(device->vkEndCommandBuffer(cmd));

    /* Submit command buffer */
    const Uint64 signal_value = 1;

    VkTimelineSemaphoreSubmitInfo sinfo_semaphore = {};
    sinfo_semaphore.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    sinfo_semaphore.signalSemaphoreValueCount = 1;
    sinfo_semaphore.pSignalSemaphoreValues = &signal_value;

    VkSubmitInfo sinfo = {};
    sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sinfo.pNext = &sinfo_semaphore;
    sinfo.commandBufferCount = 1;
    sinfo.pCommandBuffers = &cmd;
    sinfo.signalSemaphoreCount = 1;
    sinfo.pSignalSemaphores = &gpu::get_fence_handle(fence);

    VK_DIE(device->vkQueueSubmit(*queue, 1, &sinfo, VK_NULL_HANDLE));

    gpu::add_destruction_callback(fence, [](void* userdata) { delete static_cast<singleshot_cmd_buffer_t*>(userdata); }, this);

    /* Transfer ownership of ::fence to caller */
    gpu::fence_t* ret = fence;
    fence = nullptr;
    return ret;
}

gpu::singleshot_cmd_buffer_t::~singleshot_cmd_buffer_t()
{
    device->vkDestroyCommandPool(pool);
    if (fence)
    {
        gpu::cancel_fence(fence);
        gpu::release_fence(fence);
    }
}
