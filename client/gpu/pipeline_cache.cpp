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
#include "pipeline_cache.h"

#include "gpu.h"

#include "tetra/log.h"
#include <SDL3/SDL.h>
#include <limits.h>
#include <vector>

/*
 * When verifying you *MUST NOT* endian swap anything
 *
 * "It’s not paranoia if they are really out to get you" - Arseny Kapoulkine
 * https://zeux.io/2019/07/17/serializing-pipeline-cache/
 *
 * I'm not sure if I'm being paranoid enough with this - Ian
 */

struct pipeline_cache_device_header_t
{
    Uint32 byte_order;
    Uint32 size_size_t;
    Uint32 size_p_void;
    Uint32 size_p_function;
    Uint32 size_char;
    Uint32 size_this;

    /* VkPhysicalDeviceProperties */
    uint32_t api_version;
    uint32_t driver_version;
    uint32_t vendor_id;
    uint32_t device_id;
    VkPhysicalDeviceType device_type;
    char device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    uint8_t uuid_pipeline_cache[VK_UUID_SIZE];

    /* VkPhysicalDeviceVulkan11Properties/VkPhysicalDeviceIDProperties */
    uint8_t uuid_device[VK_UUID_SIZE];
    uint8_t uuid_driver[VK_UUID_SIZE];

    Uint32 compute_hash(const Uint32 seed = 0) const { return SDL_murmur3_32(this, sizeof(*this), seed); }

    void fill(VkPhysicalDevice const device)
    {
        VkPhysicalDeviceProperties2 props_10 {};
        VkPhysicalDeviceVulkan11Properties props_11 {};

        props_10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;

        props_10.pNext = &props_11;

        vkGetPhysicalDeviceProperties2(device, &props_10);

        memset(this, 0, sizeof(*this));

        byte_order = 0x12345678;
        size_size_t = sizeof(size_t);
        size_p_void = sizeof(void*);
        size_p_function = sizeof(void (*)(void));
        size_char = CHAR_BIT;
        size_this = sizeof(*this);

        api_version = props_10.properties.apiVersion;
        driver_version = props_10.properties.driverVersion;
        vendor_id = props_10.properties.vendorID;
        device_id = props_10.properties.deviceID;
        device_type = props_10.properties.deviceType;
        memcpy(device_name, props_10.properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
        memcpy(uuid_pipeline_cache, props_10.properties.pipelineCacheUUID, VK_UUID_SIZE);

        memcpy(uuid_device, props_11.deviceUUID, VK_UUID_SIZE);
        memcpy(uuid_driver, props_11.driverUUID, VK_UUID_SIZE);
    }
};

#define PIPELINE_CACHE_MAGIC         \
    "\0mcs\0b181\0PIPELINE\0CACHE\0" \
    "V00000"

struct pipeline_cache_header_t
{
    char magic_text[32] = PIPELINE_CACHE_MAGIC;

    pipeline_cache_device_header_t device_info;

    /** Size returned from vkGetPipelineCacheData */
    size_t data_size;
    /** MurmurHash3 value of the data returned by vkGetPipelineCacheData as computed by SDL_murmur3_32() with a seed of device_info.compute_hash(0) */
    Uint32 data_hash;
};

static_assert(sizeof(PIPELINE_CACHE_MAGIC) == sizeof(pipeline_cache_header_t::magic_text));

static Uint32 read_pipeline_cache_field(const Uint8* data, const Uint32 offset)
{
    Uint32 value;
    memcpy(&value, (Uint8*)(data) + offset, 4);
    return SDL_Swap32LE(value);
}

static bool is_pipline_cache_data_suitable(VkPhysicalDevice const device, const std::vector<Uint8>& pipeline_data)
{
    if (pipeline_data.size() < sizeof(pipeline_cache_header_t))
        return false;

    const pipeline_cache_header_t header_file = *reinterpret_cast<const pipeline_cache_header_t*>(pipeline_data.data());

    pipeline_cache_device_header_t header_device_expected {};
    header_device_expected.fill(device);

    /* Check our header */

    if (memcmp(PIPELINE_CACHE_MAGIC, header_file.magic_text, sizeof(header_file.magic_text)) != 0)
        return false;

    if (memcmp(&header_file.device_info, &header_device_expected, sizeof(header_device_expected)) != 0)
        return false;

    if (header_file.data_size + sizeof(pipeline_cache_header_t) != pipeline_data.size())
        return false;

    if (header_file.data_size < 32) /* The cache blob has a minimum size of 32 bytes as per spec */
        return false;

    if (header_file.data_hash
        != SDL_murmur3_32(pipeline_data.data() + sizeof(pipeline_cache_header_t), header_file.data_size, header_file.device_info.compute_hash(0)))
        return false;

    const Uint8* header_vk = ((const Uint8*)(pipeline_data.data() + sizeof(pipeline_cache_header_t)));

    /* Verify the Vulkan header */

    if (read_pipeline_cache_field(header_vk, 4) != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
        return false;

    if (read_pipeline_cache_field(header_vk, 8) != header_file.device_info.vendor_id)
        return false;

    if (read_pipeline_cache_field(header_vk, 12) != header_file.device_info.device_id)
        return false;

    if (memcmp(header_vk + 16, header_file.device_info.uuid_pipeline_cache, VK_UUID_SIZE) != 0)
        return false;

    return true;
}

bool gpu::create_pipeline_cache(
    VkPhysicalDevice const physical, VkDevice const logical, VkPipelineCache& cache, const std::vector<Uint8>& pipeline_cache_file_data)
{
    if (physical == VK_NULL_HANDLE || logical == VK_NULL_HANDLE)
        return false;

    cache = VK_NULL_HANDLE;

    VkPipelineCacheCreateInfo cinfo_cache {};
    cinfo_cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    if (is_pipline_cache_data_suitable(physical, pipeline_cache_file_data))
    {
        cinfo_cache.initialDataSize = reinterpret_cast<const pipeline_cache_header_t*>(pipeline_cache_file_data.data())->data_size;
        cinfo_cache.pInitialData = pipeline_cache_file_data.data() + sizeof(pipeline_cache_header_t);
    }
    else
        dc_log_warn("Existing pipeline cache is unsuitable");

    VkResult result;
    VK_TRY_STORE(result, vkCreatePipelineCache(logical, &cinfo_cache, nullptr, &cache));

    return result == VK_SUCCESS;
}

bool gpu::save_pipeline_cache(
    VkPhysicalDevice const physical, VkDevice const logical, VkPipelineCache const cache, std::vector<Uint8>& pipeline_cache_file_data)
{
    if (physical == VK_NULL_HANDLE || logical == VK_NULL_HANDLE || cache == VK_NULL_HANDLE)
        return false;

    pipeline_cache_header_t header_file {};
    header_file.device_info.fill(physical);

    pipeline_cache_file_data.resize(sizeof(pipeline_cache_header_t));

    VkResult result;
    VK_TRY_STORE(result, vkGetPipelineCacheData(logical, cache, &header_file.data_size, nullptr));
    pipeline_cache_file_data.resize(pipeline_cache_file_data.size() + header_file.data_size);
    Uint8* blob = pipeline_cache_file_data.data() + sizeof(pipeline_cache_header_t);
    VK_TRY_STORE(result, vkGetPipelineCacheData(logical, cache, &header_file.data_size, blob));

    if (result != VK_SUCCESS)
        return false;

    if (pipeline_cache_file_data.size() != header_file.data_size + sizeof(pipeline_cache_header_t))
        return false;

    header_file.data_hash = SDL_murmur3_32(blob, header_file.data_size, header_file.device_info.compute_hash(0));
    memcpy(pipeline_cache_file_data.data(), &header_file, sizeof(pipeline_cache_header_t));

    return true;
}
