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

#include "subdiv_buffer.h"

#include "tetra/gui/imgui.h"
#include "tetra/log.h"
#include <algorithm>

static Uint32 ceil(Uint32 in, Uint32 multiple) { return (in + SDL_max(1, multiple) - 1) / multiple * multiple; }

void gpu::subdiv_buffer_allocation_t::release() { parent->release_region(offset); }

gpu::subdiv_buffer_allocation_t::subdiv_buffer_allocation_t(const Uint32 _offset, subdiv_buffer_t* const _parent)
    : offset(_offset)
    , parent(_parent)
{
}

gpu::subdiv_buffer_t::subdiv_buffer_t(
    const SDL_GPUBufferUsageFlags _buffer_flags, const Uint32 _element_size, const Uint32 initial_elements, const Uint32 _round_size)
    : element_size(_element_size)
    , round_size(SDL_max(1, _round_size))
    , buffer_flags(_buffer_flags)

{
    reserve_additional_space(ceil(initial_elements, round_size));
}

gpu::subdiv_buffer_t::~subdiv_buffer_t()
{
    for (auto& it : active_fences)
        gpu::release_fence(it);

    for (auto& it : upload_fences)
        gpu::release_fence(it);

    for (auto& it_region : pending_releases)
        for (auto& it_fence : it_region.second)
            gpu::release_fence(it_fence);

    gpu::release_fence(resize_op_fence);
    gpu::release_buffer(buffer_main);
    gpu::release_buffer(buffer_new);
}

static void erase_expired_fences(std::vector<gpu::fence_t*>& vec)
{
    for (auto it = vec.begin(); it != vec.end();)
    {
        if (gpu::is_fence_cancelled(*it) || gpu::is_fence_done(*it))
        {
            gpu::release_fence(*it);
            it = vec.erase(it);
        }
        else
            ++it;
    }
}

void gpu::subdiv_buffer_t::prune_fences()
{
    erase_expired_fences(upload_fences);
    erase_expired_fences(active_fences);
}

void gpu::subdiv_buffer_t::combine_avail_fragment()
{
    for (auto cur = avail_regions.begin(); cur != avail_regions.end();)
    {
        auto next = std::next(cur);

        if (next == avail_regions.end())
            break;

        if (cur->offset + cur->num_elements != next->offset)
        {
            cur = std::move(next);
            continue;
        }

        cur->num_elements += next->num_elements;

        cur = avail_regions.erase(next);
    }
}

void gpu::subdiv_buffer_t::return_region_to_pool(const mesh_buffer_region_t& region)
{
    avail_regions.push_back(region);

    std::sort(avail_regions.begin(), avail_regions.end(), [](const mesh_buffer_region_t& a, const mesh_buffer_region_t& b) { return a.offset < b.offset; });

    combine_avail_fragment();
}

void gpu::subdiv_buffer_t::perform_pending_operations()
{
    prune_fences();

    if (resize_op_in_progress && !resize_op_fence && !upload_fences.size())
        perform_actual_resize();

    /* Release resize resources */
    if (resize_op_in_progress && resize_op_fence && (gpu::is_fence_cancelled(resize_op_fence) || gpu::is_fence_done(resize_op_fence)))
    {
        if (gpu::is_fence_done(resize_op_fence))
        {
            mesh_buffer_region_t reg = {};
            reg.offset = size_main;
            reg.num_elements = size_new - size_main;

            std::swap(buffer_main, buffer_new);
            std::swap(size_main, size_new);

            avail_regions.push_back(reg);
            combine_avail_fragment();

            /* Since all releases are for the old buffer, they are no longer relevant and can be eliminated */
            for (auto& it_region : pending_releases)
            {
                return_region_to_pool(it_region.first);
                for (auto& it_fence : it_region.second)
                    gpu::release_fence(it_fence);
            }
            pending_releases.clear();
        }

        gpu::release_buffer(buffer_new);
        size_new = 0;

        gpu::release_fence(resize_op_fence);

        resize_op_in_progress = 0;
    }

    /* Release finished releases */
    for (auto it_region = pending_releases.begin(); it_region != pending_releases.end();)
    {
        bool fully_released = true;
        for (auto& it_fence : it_region->second)
        {
            if (!gpu::is_fence_cancelled(it_fence) && !gpu::is_fence_done(it_fence))
            {
                fully_released = false;
                break;
            }
        }

        if (!fully_released)
        {
            ++it_region;
            continue;
        }

        return_region_to_pool(it_region->first);
        for (auto& it_fence : it_region->second)
            gpu::release_fence(it_fence);
        it_region = pending_releases.erase(it_region);
    }
}

void gpu::subdiv_buffer_t::reserve_additional_space(const Uint32 min_num_elements)
{
    if (resize_op_in_progress || !min_num_elements)
        return;
    resize_op_in_progress = 1;
    resize_op_num_elements = min_num_elements;
}

void gpu::subdiv_buffer_t::perform_actual_resize()
{
    SDL_assert(resize_op_in_progress);
    SDL_assert(resize_op_num_elements);
    SDL_assert(!resize_op_fence);
    SDL_assert(upload_fences.size() == 0);

    size_new = SDL_max(size_main * 5 / 4, size_main + ceil(resize_op_num_elements, round_size));
    size_new = ceil(size_new, round_size);

    SDL_GPUBufferCreateInfo cinfo_buf = {};
    cinfo_buf.usage = buffer_flags;
    cinfo_buf.size = size_new * element_size;

    gpu::release_buffer(buffer_new);
    buffer_new = gpu::create_buffer(cinfo_buf, "subdiv_buffer");

    if (!buffer_new)
    {
        dc_log_error("Failed to resize buffer from %u elements -> %u elements", size_main, size_new);
        return;
    }

    if (!buffer_main) /* No pre-existing buffer, no copy necessary */
    {
        buffer_main = buffer_new;
        size_main = size_new;

        buffer_new = nullptr;
        size_new = 0;
        return;
    }

    /* Pre-existing buffer, copy data */

    SDL_GPUCommandBuffer* command_buffer = gpu::acquire_command_buffer();

    if (!command_buffer)
    {
        gpu::release_buffer(buffer_new);
        dc_log_error("Failed to resize buffer from %u elements -> %u elements", size_main, size_new);
        return;
    }

    SDL_GPUBufferLocation loc_src = {};
    SDL_GPUBufferLocation loc_dest = {};

    loc_src.buffer = buffer_main;
    loc_dest.buffer = buffer_new;

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_CopyGPUBufferToBuffer(copy_pass, &loc_src, &loc_dest, SDL_min(size_main, size_new) * element_size, false);
    SDL_EndGPUCopyPass(copy_pass);

    if (!(resize_op_fence = gpu::submit_command_buffer_and_acquire_fence(command_buffer)))
    {
        gpu::release_buffer(buffer_new);
        dc_log_error("Failed to resize buffer from %u elements -> %u elements", size_main, size_new);
        return;
    }
}

bool gpu::subdiv_buffer_t::acquire_region(Uint32 num_elements, Uint32& offset)
{
    SDL_assert(num_elements);
    if (!num_elements)
        return false;

    perform_pending_operations();

    if (resize_op_fence)
        return false;

    num_elements = ceil(num_elements, round_size);

    for (auto it = avail_regions.begin(); it != avail_regions.end();)
    {
        if (it->num_elements < num_elements)
        {
            ++it;
            continue;
        }

        offset = it->offset;

        it->offset = it->offset + num_elements;
        it->num_elements -= num_elements;

        if (it->num_elements == 0)
            avail_regions.erase(it);

        allocations[offset] = num_elements;

        allocated_space += num_elements;

        return true;
    }

    reserve_additional_space(num_elements);

    return false;
}

void gpu::subdiv_buffer_t::release_region(const Uint32 offset)
{
    auto it = allocations.find(offset);

    SDL_assert(it != allocations.end());
    if (it == allocations.end())
    {
        dc_log_error("Attempt made to free an invalid allocation!: Offset: %0u", offset);
        return;
    }

    prune_fences();

    if (active_fences.size())
    {
        for (auto& it_fence : active_fences)
            gpu::ref_fence(it_fence);

        pending_releases.push_back(std::pair(mesh_buffer_region_t { it->first, it->second }, active_fences));
    }
    else
        return_region_to_pool(mesh_buffer_region_t { it->first, it->second });

    allocated_space -= it->second;

    allocations.erase(it);
}

void gpu::subdiv_buffer_t::mark_as_used_by_command_buffer(const SDL_GPUCommandBuffer* const command_buffer)
{
    active_fences.push_back(gpu::get_command_buffer_fence(command_buffer));

    perform_pending_operations();
}

void gpu::subdiv_buffer_t::mark_upload_from_command_buffer(const SDL_GPUCommandBuffer* const command_buffer)
{
    upload_fences.push_back(gpu::get_command_buffer_fence(command_buffer));

    perform_pending_operations();
}
