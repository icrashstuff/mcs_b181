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

#include <vector>

namespace gpu
{
/**
 * Create/Restore a Vulkan pipeline cache
 *
 * @param physical Physical device related to the logical device
 * @param logical Logical device to create a pipeline cache for
 * @param cache Output (Will be set to VK_NULL_HANDLE on failure)
 * @param pipeline_cache_file_data A complete mcs_b181 format pipeline cache file blob to attempt to restore from
 *
 * @returns true on success, false on failure
 */
bool create_pipeline_cache(VkPhysicalDevice const physical, VkDevice const logical, VkPipelineCache& cache, const std::vector<Uint8>& pipeline_cache_file_data);

/**
 * Save a Vulkan Pipeline to a mcs_b181 format pipeline cache blob
 *
 * @param physical Physical device related to the logical device
 * @param logical Logical device associated with the pipeline cache
 * @param cache Pipeline cache to save
 * @param pipeline_cache_file_data Output (Not valid on failure)
 *
 * @returns true on success, false on failure
 */
bool save_pipeline_cache(VkPhysicalDevice const physical, VkDevice const logical, VkPipelineCache const cache, std::vector<Uint8>& pipeline_cache_file_data);
}
