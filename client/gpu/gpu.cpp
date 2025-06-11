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

#include "internal.h"

#include "tetra/util/convar.h"
#include "tetra/util/misc.h"
#include <SDL3/SDL_vulkan.h>
#include <set>

SDL_Window* gpu::window = nullptr;

static convar_int_t r_debug_vulkan {
    "r_debug_vulkan",
    false,
    false,
    true,
    "Attempt to create VkInstance and VkDevice with the validation layers enabled",
    CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY,
};

VkInstance gpu::instance = VK_NULL_HANDLE;
VkPhysicalDevice gpu::physical_device = VK_NULL_HANDLE;
VkDevice gpu::device = VK_NULL_HANDLE;

VkQueue gpu::graphics_queue = VK_NULL_HANDLE;
VkQueue gpu::transfer_queue = VK_NULL_HANDLE;
VkQueue gpu::present_queue = VK_NULL_HANDLE;

#define VK_DIE(_CALL)                                                                              \
    do                                                                                             \
    {                                                                                              \
        VkResult result__ = _CALL;                                                                 \
        if (result__ != VK_SUCCESS)                                                                \
            util::die("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/**
 * @param required_instance_extensions Required instance-level extensions
 * @param required_instance_layers Required instance-level layers (Validation layers will automatically be added if r_debug_vulkan is true)
 *
 * @returns A newly created VkInstance on success, exits the program on failure
 */
static VkInstance init_instance(std::vector<const char*> required_instance_extensions = {}, std::vector<const char*> required_instance_layers = {})
{
    dc_log("Header version: %u", VK_HEADER_VERSION);

    Uint32 instance_version = volkGetInstanceVersion();

    VkApplicationInfo ainfo_instance = {};
    ainfo_instance.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ainfo_instance.pApplicationName = "mcs_b181_client";
    ainfo_instance.pEngineName = "mcs_b181_client";
    ainfo_instance.apiVersion = VK_API_VERSION_1_2;

    Uint32 inst_ver_major = VK_API_VERSION_MAJOR(instance_version);
    Uint32 inst_ver_minor = VK_API_VERSION_MINOR(instance_version);
    Uint32 inst_ver_patch = VK_API_VERSION_PATCH(instance_version);
    Uint32 inst_ver_variant = VK_API_VERSION_VARIANT(instance_version);

    if (instance_version < ainfo_instance.apiVersion)
        util::die("Unsupported Vulkan instance version: %u.%u.%u, Variant %u", inst_ver_major, inst_ver_minor, inst_ver_patch, inst_ver_variant);

    dc_log("Instance version: %u.%u.%u, Variant %u", inst_ver_major, inst_ver_minor, inst_ver_patch, inst_ver_variant);

    {
        Uint32 sdl_ext_count = 0;
        const char* const* sdl_ext = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
        for (Uint32 i = 0; i < sdl_ext_count; ++i)
            required_instance_extensions.push_back(sdl_ext[i]);
    }

    if (r_debug_vulkan.get())
    {
        required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        required_instance_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    /* Get available instance extensions */
    Uint32 props_ext_num = 0;
    VK_DIE(vkEnumerateInstanceExtensionProperties(nullptr, &props_ext_num, nullptr));
    std::vector<VkExtensionProperties> props_ext(props_ext_num);
    VK_DIE(vkEnumerateInstanceExtensionProperties(nullptr, &props_ext_num, props_ext.data()));
    props_ext.resize(props_ext_num);

    /* Get available instance layers */
    Uint32 props_lay_num = 0;
    VK_DIE(vkEnumerateInstanceLayerProperties(&props_lay_num, nullptr));
    std::vector<VkLayerProperties> props_lay(props_lay_num);
    VK_DIE(vkEnumerateInstanceLayerProperties(&props_lay_num, props_lay.data()));
    props_lay.resize(props_lay_num);

    /* Check that all required instances extensions are present */
    for (const char* it : required_instance_extensions)
    {
        bool found = 0;
        for (Uint32 i = 0; i < props_ext_num && !found; ++i)
            found = (strcmp(it, props_ext[i].extensionName) == 0);
        if (!found)
            dc_log_error("Required instance extension %s not found", it);
        else
            dc_log("Required instance extension %s found", it);
    }

    /* Check that all required instances layers are present */
    for (const char* it : required_instance_layers)
    {
        bool found = 0;
        for (Uint32 i = 0; i < props_lay_num && !found; ++i)
            found = (strcmp(it, props_lay[i].layerName) == 0);
        if (!found)
            dc_log("Required instance layer %s found", it);
    }

    VkInstanceCreateInfo cinfo_instance = {};
    cinfo_instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    /* Enable optional instance extensions */
    for (const auto& it : props_ext)
    {
        if (strcmp(it.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
        {
            dc_log("Enabling instance extension: %s", VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            required_instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            cinfo_instance.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
    }

    cinfo_instance.pApplicationInfo = &ainfo_instance;
    cinfo_instance.enabledLayerCount = required_instance_layers.size();
    cinfo_instance.ppEnabledLayerNames = required_instance_layers.data();
    cinfo_instance.enabledExtensionCount = required_instance_extensions.size();
    cinfo_instance.ppEnabledExtensionNames = required_instance_extensions.data();

    VkInstance instance = VK_NULL_HANDLE;

    VK_DIE(vkCreateInstance(&cinfo_instance, nullptr, &instance));

    /* Flush any messages that the vulkan implementation may print out
     * (Mainly for KDevelop when running on my Lenovo T430 - Ian) */
    fflush(stdout);
    fflush(stderr);

    return instance;
}

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

    std::vector<VkQueueFamilyProperties> queue_families;

    bool has_graphics_queue = 0;
    bool has_transfer_queue = 0;
    bool has_present_queue = 0;

    Uint32 graphics_queue_idx = 0;
    Uint32 transfer_queue_idx = 0;
    Uint32 present_queue_idx = 0;

    physical_device_info_t()
    {
        props_10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        props_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

        props_10.pNext = &props_11;
        props_11.pNext = &props_12;

        features_10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        features_10.pNext = &features_11;
        features_11.pNext = &features_12;
    }
};

static bool find_queue(const std::vector<VkQueueFamilyProperties>& v, const VkQueueFlags flags, Uint32& idx)
{
    for (size_t i = 0; i < v.size(); i++)
    {
        if ((v[i].queueFlags & flags) == flags)
        {
            idx = i;
            return true;
        }
    }
    return false;
}

static bool find_present_queue(const std::vector<VkQueueFamilyProperties>& v, VkInstance instance, VkPhysicalDevice device, Uint32& idx)
{
    for (size_t i = 0; i < v.size(); i++)
    {
        if (SDL_Vulkan_GetPresentationSupport(instance, device, i))
        {
            idx = i;
            return true;
        }
    }
    return false;
}

/* TODO: Add option to force a device by name? */
static bool select_physical_device(VkInstance instance, const std::vector<const char*>& required_device_extensions, physical_device_info_t& out)
{
    std::vector<physical_device_info_t> devices;

    {
        Uint32 physical_devices_count = 0;
        VK_DIE(vkEnumeratePhysicalDevices(instance, &physical_devices_count, nullptr));
        std::vector<VkPhysicalDevice> physical_devices(physical_devices_count);
        VK_DIE(vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data()));
        physical_devices.resize(physical_devices_count);

        for (auto& device : physical_devices)
        {
            physical_device_info_t info = {};
            info.device = device;

            assert(info.props_10.pNext == &info.props_11);
            assert(info.props_11.pNext == &info.props_12);
            assert(info.props_12.pNext == nullptr);
            assert(info.features_10.pNext == &info.features_11);
            assert(info.features_11.pNext == &info.features_12);
            assert(info.features_12.pNext == nullptr);

            vkGetPhysicalDeviceProperties2(device, &info.props_10);
            vkGetPhysicalDeviceFeatures2(device, &info.features_10);

            Uint32 extensions_count = 0;
            VK_DIE(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count, nullptr));
            info.extensions.resize(extensions_count);
            VK_DIE(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count, info.extensions.data()));
            info.extensions.resize(extensions_count);

            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
            info.queue_families.resize(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, info.queue_families.data());
            info.queue_families.resize(queue_family_count);

            info.has_graphics_queue = find_queue(info.queue_families, VK_QUEUE_GRAPHICS_BIT, info.graphics_queue_idx);
            info.has_transfer_queue = find_queue(info.queue_families, VK_QUEUE_TRANSFER_BIT, info.transfer_queue_idx);
            info.has_present_queue = find_present_queue(info.queue_families, instance, device, info.present_queue_idx);

            devices.push_back(info);
        }
    }

    /* Dump info on all vulkan devices, and cull unsuitable ones */
    dc_log("Available Vulkan Devices: %zu", devices.size());
    for (auto it_dev = devices.begin(); it_dev != devices.end();)
    {
        dc_log("================ %s (%s) ================", string_VkPhysicalDeviceType(it_dev->props_10.properties.deviceType),
            it_dev->props_10.properties.deviceName);

        Uint32 api_ver_major = VK_API_VERSION_MAJOR(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_minor = VK_API_VERSION_MINOR(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_patch = VK_API_VERSION_PATCH(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_variant = VK_API_VERSION_VARIANT(it_dev->props_10.properties.apiVersion);

        dc_log("API Version %u.%u.%u, Variant: %u", api_ver_major, api_ver_minor, api_ver_patch, api_ver_variant);

        bool suitable = true;

        /* Ensure required extensions are present */
        for (const char* it : required_device_extensions)
        {
            bool found = 0;
            for (Uint32 i = 0; i < it_dev->extensions.size() && !found; ++i)
                found = (strcmp(it, it_dev->extensions[i].extensionName) == 0);
            if (!found)
            {
                dc_log_error("Does not have: %s", it);
                suitable = false;
            }
            else
                dc_log("Has: %s", it);
        }

        if (it_dev->has_graphics_queue)
            dc_log("Has graphics queue");
        else
        {
            dc_log("Does not have graphics quue");
            suitable = false;
        }

        if (it_dev->has_present_queue)
            dc_log("Has present queue");
        else
        {
            dc_log("Does not have present quue");
            suitable = false;
        }

        if (it_dev->has_transfer_queue)
            dc_log("Has transfer queue");
        else
        {
            dc_log("Does not have transfer quue");
            suitable = false;
        }

        /* Check for VK_EXT_tooling_info */
        bool has_VK_EXT_tooling_info = 0;
        for (const auto& it : it_dev->extensions)
        {
            if (strcmp(it.extensionName, VK_EXT_TOOLING_INFO_EXTENSION_NAME) == 0)
                has_VK_EXT_tooling_info = 1;
        }

        /* Dump tooling info */
        if (has_VK_EXT_tooling_info && vkGetPhysicalDeviceToolPropertiesEXT)
        {
            Uint32 num_tools = 0;
            VK_DIE(vkGetPhysicalDeviceToolPropertiesEXT(it_dev->device, &num_tools, nullptr));
            std::vector<VkPhysicalDeviceToolPropertiesEXT> tool_props(num_tools);
            VK_DIE(vkGetPhysicalDeviceToolPropertiesEXT(it_dev->device, &num_tools, tool_props.data()));
            tool_props.resize(num_tools);

            dc_log("Tools (%u):", num_tools);

            for (Uint32 i = 0; i < num_tools; ++i)
            {
                dc_log("  %s", tool_props[i].name);
                dc_log("    Version: %s", tool_props[i].version);
                dc_log("    Description: %s", tool_props[i].description);
                dc_log("    Purposes: %s", string_VkToolPurposeFlags(tool_props[i].purposes).c_str());
                dc_log("    Corresponding Layer: %s", tool_props[i].layer);
            }
        }

        if (!suitable)
        {
            dc_log("Removing unsuitable device %s", it_dev->props_10.properties.deviceName);
            it_dev = devices.erase(it_dev);
        }
        else
            ++it_dev;
    }

    dc_log("Compatible Vulkan Devices: %zu", devices.size());

    for (auto& it : devices)
        dc_log("%s %s", string_VkPhysicalDeviceType(it.props_10.properties.deviceType), it.props_10.properties.deviceName);

    for (const auto type : {
             VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
             VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
             VK_PHYSICAL_DEVICE_TYPE_CPU,
             VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
             VK_PHYSICAL_DEVICE_TYPE_OTHER,
         })
    {
        for (auto& it : devices)
        {
            if (it.props_10.properties.deviceType == type)
            {
                out = it;
                return true;
            }
        }
    }

    if (devices.size())
    {
        out = devices[0];
        return true;
    }

    return false;
}

static VkDevice init_device(VkInstance instance, physical_device_info_t device_info, const std::vector<const char*>& required_device_extensions)
{
    std::set<Uint32> queue_families = { device_info.graphics_queue_idx, device_info.present_queue_idx, device_info.transfer_queue_idx };

    float queue_priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> cinfo_queues;

    for (uint32_t family : queue_families)
    {
        VkDeviceQueueCreateInfo cinfo_queue {};
        cinfo_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        cinfo_queue.queueFamilyIndex = family;
        cinfo_queue.queueCount = 1;
        cinfo_queue.pQueuePriorities = &queue_priority;
        cinfo_queues.push_back(cinfo_queue);
    }

    VkPhysicalDeviceFeatures2 features_10 = {};
    VkPhysicalDeviceVulkan11Features features_11 = {};
    VkPhysicalDeviceVulkan12Features features_12 = {};

    features_10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    features_10.pNext = &features_11;
    features_11.pNext = &features_12;

    std::vector<const char*> required_device_layers;

    if (r_debug_vulkan.get())
        required_device_layers.push_back("VK_LAYER_KHRONOS_validation");

    VkDeviceCreateInfo cinfo_device {};
    cinfo_device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    cinfo_device.pNext = &features_10;
    cinfo_device.queueCreateInfoCount = cinfo_queues.size();
    cinfo_device.pQueueCreateInfos = cinfo_queues.data();
    cinfo_device.enabledExtensionCount = required_device_extensions.size();
    cinfo_device.ppEnabledExtensionNames = required_device_extensions.data();
    cinfo_device.enabledLayerCount = required_device_layers.size();
    cinfo_device.ppEnabledLayerNames = required_device_layers.data();

    VkDevice device = VK_NULL_HANDLE;

    VK_DIE(vkCreateDevice(device_info.device, &cinfo_device, nullptr, &device));

    /* Flush any messages that the vulkan implementation may print out
     * (Mainly for KDevelop when running on my Lenovo T430 - Ian) */
    fflush(stdout);
    fflush(stderr);

    return device;
}

void gpu::init()
{
    internal::init_gpu_fences();

    SDL_ClearError();
    if (!(gpu::window = SDL_CreateWindow("Hello", 1024, 768, SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN)))
        util::die("Failed to create window: %s", SDL_GetError());

    SDL_ClearError();
    PFN_vkGetInstanceProcAddr sdl_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!sdl_vkGetInstanceProcAddr)
        util::die("SDL_Vulkan_GetVkGetInstanceProcAddr() failed: %s", SDL_GetError());

    volkInitializeCustom(sdl_vkGetInstanceProcAddr);

    instance = init_instance();
    volkLoadInstanceOnly(gpu::instance);

    std::vector<const char*> required_device_extensions = {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, /* Promoted to vulkan 1.3 */
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, /* Promoted to vulkan 1.3 */
    };

    physical_device_info_t device_info;
    if (!select_physical_device(instance, required_device_extensions, device_info))
        util::die("Unable to find suitable Vulkan Device!\n"
                  "Try updating your Operating System and/or Graphics drivers");
    physical_device = device_info.device;

    device = init_device(instance, device_info, required_device_extensions);
    volkLoadDevice(device);

    vkGetDeviceQueue(device, device_info.graphics_queue_idx, 0, &graphics_queue);
    vkGetDeviceQueue(device, device_info.transfer_queue_idx, 0, &transfer_queue);
    vkGetDeviceQueue(device, device_info.present_queue_idx, 0, &present_queue);
};

void gpu::quit()
{
    internal::quit_gpu_fences();

    graphics_queue = VK_NULL_HANDLE;
    transfer_queue = VK_NULL_HANDLE;
    present_queue = VK_NULL_HANDLE;

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    physical_device = VK_NULL_HANDLE;

    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;

    volkFinalize();

    SDL_DestroyWindow(window);
    window = nullptr;
}
