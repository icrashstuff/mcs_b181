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
#include <SDL3/SDL.h>
#include <map>
#include <vector>

namespace gpu
{
/**
 * A GPU buffer that can have portions sub-allocated
 */
struct subdiv_buffer_t
{
    /** Size (in bytes) of a single element */
    const Uint32 element_size;
    /** Number of elements to align allocations to */
    const Uint32 round_size;
    /** Buffer flags to be passed to gpu::create_buffer() */
    const SDL_GPUBufferUsageFlags buffer_flags;

    /**
     * Create a sub-dividable GPU buffer
     *
     * @param buffer_flags Buffer flags associated with the underlying GPU buffer
     * @param element_size Size (in bytes) of a single element
     * @param initial_elements Initial number of elements to create
     * @param round_size Number of elements to align allocations to (Improves sub-allocation reuse)
     */
    subdiv_buffer_t(const SDL_GPUBufferUsageFlags buffer_flags, const Uint32 element_size, const Uint32 initial_elements, const Uint32 round_size = 4);
    ~subdiv_buffer_t();

    /**
     * Allocates a region of the mesh buffer for immediate use
     *
     * @param num_elements Number of elements to allocate for the region
     * @param offset If this function returns true, then this will be set to the region's offset within the buffer (in elements)
     *
     * @returns true on success, false on current unavailability of memory (ie. Completely lack of memory, or a resize is in progress)
     */
    bool acquire_region(Uint32 num_elements, Uint32& offset);

    /**
     * Release a region of the mesh buffer back
     *
     * You must not reference the region after calling this function.
     *
     * Internally the region will not be reallocated until all fences active at the time of release have been signaled
     *
     * @param offset Offset into mesh buffer acquired by acquire_region()
     */
    void release_region(const Uint32 offset);

    /**
     * Mark the buffer as being used a command buffer
     *
     * Internally the fence acquired from the command buffer will be used to determine when released regions can be returned to the pool
     *
     * @param command_buffer Command buffer to acquire fence from
     */
    void mark_as_used_by_command_buffer(const SDL_GPUCommandBuffer* const command_buffer);

    /**
     * Mark the buffer as being used in an upload operation
     *
     * Internally the acquired fence is used to delay a resize copy operation
     *
     * @param command_buffer Command buffer to acquire fence from
     */
    void mark_upload_from_command_buffer(const SDL_GPUCommandBuffer* const command_buffer);

    /**
     * Cleanup any internal resources that were in a state of being deallocated/destroyed
     */
    void perform_pending_operations();

    /**
     * Get underlying buffer
     *
     * NOTE: NEVER USE GPU RESOURCE CYCLING WITH THE UNDERLYING BUFFER!
     */
    inline SDL_GPUBuffer* get_buffer() { return buffer_main; }

    inline Uint32 get_size_in_elements() const { return size_main; }
    inline Uint32 get_size_in_bytes() const { return size_main * element_size; }

    inline Uint32 get_allocations_in_elements() const { return allocated_space; }
    inline Uint32 get_allocations_in_bytes() const { return allocated_space * element_size; }

    inline Uint32 get_num_allocations() const { return allocations.size(); }
    inline Uint32 get_num_pending_releases() const { return pending_releases.size(); }
    inline Uint32 get_num_avail_regions() const { return avail_regions.size(); }

    inline bool get_resize_in_progress() const { return resize_op_in_progress; };

private:
    Uint32 allocated_space = 0;

    SDL_GPUBuffer* buffer_main = nullptr;
    /** Size (in elements) of main buffer */
    Uint32 size_main = 0;

    SDL_GPUBuffer* buffer_new = nullptr;
    /** Size (in elements) of new buffer */
    Uint32 size_new = 0;

    std::vector<fence_t*> active_fences;

    std::vector<fence_t*> upload_fences;

    struct mesh_buffer_region_t
    {
        Uint32 offset;
        Uint32 num_elements;
    };
    std::vector<mesh_buffer_region_t> avail_regions;

    /**
     * Data on mesh buffer regions
     *
     * key: offset
     * value: num_elements
     */
    std::map<Uint32, Uint32> allocations;

    /** Allocations are temporarily disabled when this is not null*/
    fence_t* resize_op_fence = nullptr;

    std::vector<std::pair<mesh_buffer_region_t, std::vector<fence_t*>>> pending_releases;

    void return_region_to_pool(const mesh_buffer_region_t& region);

    /**
     * Reserve additional space
     *
     * @param min_num_elements Minimum number of elements to reserve space for
     */
    void reserve_additional_space(const Uint32 min_num_elements);

    void perform_actual_resize();

    Uint32 resize_op_num_elements = 0;
    bool resize_op_in_progress = false;

    /** Prune inactive fences from active_fences */
    void prune_fences();

    /** Merge bordering available fragments */
    void combine_avail_fragment();
};

/**
 * Convenience class to handle sub-buffer cleanup
 */
struct subdiv_buffer_allocation_t
{
    /** Offset with the parent's buffer (in elements) */
    const Uint32 offset;
    /** Parent buffer */
    subdiv_buffer_t* const parent;

    subdiv_buffer_allocation_t(const Uint32 _offset, subdiv_buffer_t* const _parent);

    /**
     * Release the allocation
     *
     * You must not reference the region, and dispose of this object after calling this function
     */
    void release();
};
}
