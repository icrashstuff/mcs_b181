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

#include "common.h"

namespace gpu
{
struct queue_t;
struct device_t;
struct fence_t;

class singleshot_cmd_buffer_t
{
public:
    /**
     * Creates a singleshot shot command buffer
     *
     * This will call vkBeginCommandBuffer(3)
     *
     * @param queue Queue to create the command buffer for, the queue object must remain valid for the life of this object
     *
     * NOTE: Calls util::die() on failure
     */
    singleshot_cmd_buffer_t(queue_t* const queue);

    /**
     * Ends the command buffer and submits it
     *
     * This will call vkEndCommandBuffer(3)
     *
     * You must *NOT* reference or delete this object after calling this function, as it adds a destruction callback to the returned fence
     *
     * @returns Fence field, calls util::die() on failure
     */
    gpu::fence_t* end_and_submit();

    /**
     * Destructor
     *
     * You must *NOT* call this function if you call end_and_submit()
     */
    ~singleshot_cmd_buffer_t();

    /** Command buffer created under `pool` for `queue` */
    VkCommandBuffer cmd = VK_NULL_HANDLE;

private:
    queue_t* const queue;
    fence_t* fence;
    VkCommandPool pool = VK_NULL_HANDLE;
};
}
