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

#include "pipeline_cache.h"
#include "shared/misc.h"
#include "tetra/gui/imgui/backends/imgui_impl_vulkan.h"
#include "tetra/tetra_core.h"
#include "tetra/util/convar.h"
#include "tetra/util/misc.h"
#include "tetra/util/physfs/physfs.h"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <set>

SDL_Window* gpu::window = nullptr;

static convar_int_t r_pipeline_cache {
    "r_pipeline_cache",
    true,
    false,
    true,
    "Enable Vulkan pipeline cache",
    CONVAR_FLAG_INT_IS_BOOL,
};

static convar_string_t r_pipeline_cache_path {
    "r_pipeline_cache_path",
    "/mcs_b181.vk_pipeline_cache",
    "PHYSFS Path for vulkan pipeline cache",
    CONVAR_FLAG_SAVE,
};

static convar_int_t r_debug_vulkan {
    "r_debug_vulkan",
    false,
    false,
    true,
    "Attempt to create VkInstance and VkDevice with the validation layers and debug extensions enabled",
    CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY,
};

VkInstance gpu::instance = VK_NULL_HANDLE;

namespace gpu
{
struct swapchain_info_t
{
    std::vector<VkSurfaceFormatKHR> formats;

    std::vector<VkPresentModeKHR> present_modes;

    VkSurfaceCapabilitiesKHR capabilities = {};
};

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

    /** Queue families available the the physical device */
    std::vector<VkQueueFamilyProperties> queue_families;

    bool has_graphics_queue = 0;
    bool has_transfer_queue = 0;
    bool has_present_queue = 0;

    Uint32 graphics_queue_idx = 0;
    Uint32 transfer_queue_idx = 0;
    Uint32 present_queue_idx = 0;

    physical_device_info_t();
};
}

const Uint32 gpu::instance_api_version = VK_API_VERSION_1_2;

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
    ainfo_instance.apiVersion = gpu::instance_api_version;

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
        {
            dc_log("SDL requires instance extension: %s", sdl_ext[i]);
            required_instance_extensions.push_back(sdl_ext[i]);
        }
    }

    if (r_debug_vulkan.get())
    {
        dc_log("Requiring validation layers and %s", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
        if (found)
            dc_log("Found required instance extension: %s", it);
        else
            dc_log_error("Did not find required instance extension: %s", it);
    }

    /* Check that all required instances layers are present */
    for (const char* it : required_instance_layers)
    {
        bool found = 0;
        for (Uint32 i = 0; i < props_lay_num && !found; ++i)
            found = (strcmp(it, props_lay[i].layerName) == 0);
        if (found)
            dc_log("Found required instance layer: %s", it);
        else
            dc_log_error("Did not find required instance layer: %s", it);
    }

    VkInstanceCreateInfo cinfo_instance = {};
    cinfo_instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    /* Enable optional instance extensions */
    for (const auto& it : props_ext)
    {
        if (strcmp(it.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
        {
            dc_log("Found optional instance extension: %s", VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
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

static gpu::swapchain_info_t get_current_swapchain_info(VkPhysicalDevice const device, VkSurfaceKHR const surface)
{
    gpu::swapchain_info_t info {};
    uint32_t surface_formats_count = 0;
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats_count, nullptr));
    info.formats.resize(surface_formats_count);
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats_count, info.formats.data()));
    info.formats.resize(surface_formats_count);

    uint32_t present_modes_count = 0;
    VK_TRY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, nullptr));
    info.present_modes.resize(present_modes_count);
    VK_TRY(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, info.present_modes.data()));
    info.present_modes.resize(present_modes_count);

    VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info.capabilities));

    return info;
}

gpu::physical_device_info_t::physical_device_info_t()
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

static bool is_extension_present(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    for (const auto& it : extensions)
        if (it.extensionName == name)
            return true;
    return false;
}

/* TODO: Add option to force a device by name? */
static bool select_physical_device(
    VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& required_device_extensions, gpu::physical_device_info_t& out)
{
    std::vector<gpu::physical_device_info_t> devices;
    std::vector<gpu::swapchain_info_t> swapchain_infos;

    {
        Uint32 physical_devices_count = 0;
        VK_DIE(vkEnumeratePhysicalDevices(instance, &physical_devices_count, nullptr));
        std::vector<VkPhysicalDevice> physical_devices(physical_devices_count);
        VK_DIE(vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data()));
        physical_devices.resize(physical_devices_count);

        for (auto& device : physical_devices)
        {
            gpu::physical_device_info_t info = {};
            info.device = device;

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

    /* Dump info on all vulkan devices, while culling unsuitable ones */
    dc_log("Available Vulkan Devices: %zu", devices.size());
    for (auto it_dev = devices.begin(); it_dev != devices.end();)
    {
        dc_log("================ %s (%s) ================", string_VkPhysicalDeviceType(it_dev->props_10.properties.deviceType),
            it_dev->props_10.properties.deviceName);

        gpu::swapchain_info_t it_swap = get_current_swapchain_info(it_dev->device, surface);

        Uint32 api_ver_major = VK_API_VERSION_MAJOR(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_minor = VK_API_VERSION_MINOR(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_patch = VK_API_VERSION_PATCH(it_dev->props_10.properties.apiVersion);
        Uint32 api_ver_variant = VK_API_VERSION_VARIANT(it_dev->props_10.properties.apiVersion);

        dc_log("API Version %u.%u.%u, Variant: %u", api_ver_major, api_ver_minor, api_ver_patch, api_ver_variant);

        dc_log("Driver id: %s", string_VkDriverId(it_dev->props_12.driverID));
        dc_log("Driver name: %s", it_dev->props_12.driverName);
        dc_log("Driver info: %s", it_dev->props_12.driverInfo);

        dc_log("Conformance Version: %u.%u.%u.%u", it_dev->props_12.conformanceVersion.major, it_dev->props_12.conformanceVersion.minor,
            it_dev->props_12.conformanceVersion.subminor, it_dev->props_12.conformanceVersion.patch);

#define UUID_FMT "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define UUID_ARGS(x) x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]

        dc_log("Pipeline cache UUID: " UUID_FMT, UUID_ARGS(it_dev->props_10.properties.pipelineCacheUUID));
        dc_log("Driver UUID: " UUID_FMT, UUID_ARGS(it_dev->props_11.driverUUID));
        dc_log("Device UUID: " UUID_FMT, UUID_ARGS(it_dev->props_11.deviceUUID));

#undef UUID_FMT
#undef UUID_ARGS

        /* stb_sprintf will automatically do SI prefix conversion when '$$' is inserted between the '%' and the format code */
        dc_log("Memory allocation max size: %lu bytes (%$$luB)", it_dev->props_11.maxMemoryAllocationSize, it_dev->props_11.maxMemoryAllocationSize);

        bool suitable = true;

        /* Ensure required extensions are present */
        for (const char* it : required_device_extensions)
        {
            bool found = 0;
            for (Uint32 i = 0; i < it_dev->extensions.size() && !found; ++i)
                found = (strcmp(it, it_dev->extensions[i].extensionName) == 0);
            if (!found)
            {
                dc_log_warn("Does not have: %s", it);
                suitable = false;
            }
            else
                dc_log("Has: %s", it);
        }

        if (it_dev->has_graphics_queue)
            dc_log("Has graphics queue");
        else
        {
            dc_log_warn("Does not have graphics quue");
            suitable = false;
        }

        if (it_dev->has_present_queue)
            dc_log("Has present queue");
        else
        {
            dc_log_warn("Does not have present quue");
            suitable = false;
        }

        if (it_dev->has_transfer_queue)
            dc_log("Has transfer queue");
        else
        {
            dc_log_warn("Does not have transfer quue");
            suitable = false;
        }

        if (it_swap.formats.size())
        {
            dc_log("Surface formats: %zu", it_swap.formats.size());
            for (VkSurfaceFormatKHR it : it_swap.formats)
                dc_log("  %s %s", string_VkColorSpaceKHR(it.colorSpace), string_VkFormat(it.format));
        }
        else
        {
            dc_log_warn("Does not have any surface formats");
            suitable = false;
        }

        if (it_swap.present_modes.size())
        {
            dc_log("Present modes: %zu", it_swap.present_modes.size());
            for (VkPresentModeKHR it : it_swap.present_modes)
                dc_log("  %s", string_VkPresentModeKHR(it));
        }
        else
        {
            dc_log_warn("Does not have any present modes");
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

    for (const gpu::physical_device_info_t& it_dev : devices)
        dc_log("  %s %s", string_VkPhysicalDeviceType(it_dev.props_10.properties.deviceType), it_dev.props_10.properties.deviceName);

    for (const auto type : {
             VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
             VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
             VK_PHYSICAL_DEVICE_TYPE_CPU,
             VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
             VK_PHYSICAL_DEVICE_TYPE_OTHER,
         })
    {
        for (const gpu::physical_device_info_t& it_dev : devices)
        {
            if (it_dev.props_10.properties.deviceType == type)
            {
                out = it_dev;
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

static VkDevice init_device(VkInstance instance, gpu::physical_device_info_t device_info, const std::vector<const char*>& required_device_extensions)
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
    VkPhysicalDeviceDynamicRenderingFeatures features_dynamic_rendering = {};
    VkPhysicalDeviceSynchronization2Features features_sync2 = {};

    features_dynamic_rendering.dynamicRendering = 1;
    features_sync2.synchronization2 = 1;

    features_10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features_dynamic_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    features_sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

    features_10.pNext = &features_11;
    features_11.pNext = &features_12;
    features_12.pNext = &features_dynamic_rendering;
    features_dynamic_rendering.pNext = &features_sync2;

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

/* Technically speaking this should never fail since an unsuitable device should
 * have already been removed, but I don't feel like changing this right now */
static bool pick_swapchain_format(const gpu::swapchain_info_t& info, VkFormat& out_format, VkColorSpaceKHR& out_colorspace)
{
#define SWAPCHAIN_FORMAT_IF(COND)                    \
    for (const VkSurfaceFormatKHR it : info.formats) \
        if (COND)                                    \
        {                                            \
            out_format = it.format;                  \
            out_colorspace = it.colorSpace;          \
            return true;                             \
        }

    SWAPCHAIN_FORMAT_IF(it.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && !vkuFormatIsSRGB(it.format));
    SWAPCHAIN_FORMAT_IF(it.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

    if (info.formats.size())
    {
        out_format = info.formats.front().format;
        out_colorspace = info.formats.front().colorSpace;
        return true;
    }

    return false;
#undef SWAPCHAIN_FORMAT_IF
}

enum present_mode_t
{
    PRESENT_MODE_IMMEDIATE,
    PRESENT_MODE_VSYNC,
    PRESENT_MODE_MOST_RECENT,
};

/**
 * Try to pick an appropriate present mode for a swapchain
 *
 * @param info Swapchain info
 * @param present_mode The GPU API present mode
 * @param out The create info to write to
 *
 * @returns true if a Vulkan present mode matching the gpu present mode is found, false if the FIFO fallback is used
 */
static bool pick_present_mode_and_image_count(const gpu::swapchain_info_t& info, const present_mode_t present_mode, VkSwapchainCreateInfoKHR& out)
{
#define RET_IF_CONTAINS(mode, num_images)                                                                      \
    do                                                                                                         \
    {                                                                                                          \
        if (std::find(info.present_modes.begin(), info.present_modes.end(), mode) != info.present_modes.end()) \
        {                                                                                                      \
            out.presentMode = mode;                                                                            \
            /* We add 1 to num_images, to give the presentation engine some leeway for internal operations */  \
            out.minImageCount = SDL_max(out.minImageCount, num_images + 1);                                    \
            return true;                                                                                       \
        }                                                                                                      \
    } while (0)

    if (present_mode == PRESENT_MODE_IMMEDIATE)
        RET_IF_CONTAINS(VK_PRESENT_MODE_IMMEDIATE_KHR, 1);

    if (present_mode == PRESENT_MODE_VSYNC)
    {
        RET_IF_CONTAINS(VK_PRESENT_MODE_FIFO_RELAXED_KHR, 2);
        RET_IF_CONTAINS(VK_PRESENT_MODE_FIFO_KHR, 2);
    }

    if (present_mode == PRESENT_MODE_MOST_RECENT)
    {
        RET_IF_CONTAINS(VK_PRESENT_MODE_MAILBOX_KHR, 3);
        RET_IF_CONTAINS(VK_PRESENT_MODE_FIFO_LATEST_READY_KHR, 3);
    }

    dc_log_warn("Falling back to vsync");
    out.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    out.minImageCount = SDL_max(out.minImageCount, 2 + 1);
    return false;
#undef RET_IF_CONTAINS
}

/**
 * Create (or recreate) a swapchain
 *
 * NOTE: This function calls gpu::wait_for_device_idle()
 *
 * @param device Device associated with swapchain
 * @param window Window associated with surface
 * @param surface Surface associated with the swapchain
 * @param old_swapchain Pre-existing swapchain to retire and *destroy*
 * @param format On success the selected surface format will be written to this parameter
 * @param size On success the swapchain extent is written to this parameter
 *
 * @returns A new swapchain on success, VK_NULL_HANDLE on error
 */
static VkSwapchainKHR create_swapchain(
    gpu::device_t* const device, SDL_Window* const window, VkSurfaceKHR surface, VkSwapchainKHR old_swapchain, VkSurfaceFormatKHR& format, VkExtent2D& size)
{
    device->wait_for_device_idle();

    gpu::swapchain_info_t swapchain_info = get_current_swapchain_info(device->physical, surface);

    VkSwapchainCreateInfoKHR cinfo {};
    cinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    cinfo.surface = surface;

    pick_present_mode_and_image_count(swapchain_info, PRESENT_MODE_VSYNC, cinfo);
    cinfo.minImageCount = SDL_max(cinfo.minImageCount, swapchain_info.capabilities.minImageCount + 1);

    if (swapchain_info.capabilities.maxImageCount) /* If non-zero then enforce the maximum image count */
        cinfo.minImageCount = SDL_min(cinfo.minImageCount, swapchain_info.capabilities.maxImageCount);

    if (!pick_swapchain_format(swapchain_info, cinfo.imageFormat, cinfo.imageColorSpace))
        return VK_NULL_HANDLE;

    if (swapchain_info.capabilities.currentExtent.width != 0xFFFFFFFF)
        cinfo.imageExtent = swapchain_info.capabilities.currentExtent;
    else /* Find a suitably close window size (At least that is what I think this path is for) */
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);

        const VkExtent2D min_extent = swapchain_info.capabilities.minImageExtent;
        const VkExtent2D max_extent = swapchain_info.capabilities.maxImageExtent;

        cinfo.imageExtent.width = SDL_clamp(static_cast<Uint32>(w), min_extent.width, max_extent.width);
        cinfo.imageExtent.height = SDL_clamp(static_cast<Uint32>(h), min_extent.height, max_extent.height);
    }

    cinfo.imageArrayLayers = 1;
    cinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    /* Determine image sharing mode */
    std::vector<Uint32> queue_families;
    {
        std::set<Uint32> queue_families_set = {
            device->graphics_queue_idx,
            device->present_queue_idx,
            device->transfer_queue_idx,
        };
        for (Uint32 it : queue_families_set)
            queue_families.push_back(it);
    }
    if (queue_families.size() == 1)
        cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    else
    {
        cinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        cinfo.queueFamilyIndexCount = queue_families.size();
        cinfo.pQueueFamilyIndices = queue_families.data();
    }

    cinfo.preTransform = swapchain_info.capabilities.currentTransform;
    cinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    cinfo.clipped = VK_TRUE;
    cinfo.oldSwapchain = old_swapchain;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkResult result;
    VK_TRY_STORE(result, vkCreateSwapchainKHR(device->logical, &cinfo, nullptr, &swapchain));

    if (old_swapchain)
        vkDestroySwapchainKHR(device->logical, old_swapchain, nullptr);

    if (result != VK_SUCCESS)
        return VK_NULL_HANDLE;

    format.format = cinfo.imageFormat;
    format.colorSpace = cinfo.imageColorSpace;

    size = cinfo.imageExtent;

    return swapchain;
}

gpu::device_t* gpu::device_new = nullptr;

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

    /* Init instance */
    instance = init_instance();
    volkLoadInstanceOnly(gpu::instance);

    device_new = new device_t(gpu::window);

    /* Load ImGui functions */
    ImGui_ImplVulkan_LoadFunctions(
        gpu::instance_api_version,
        [](const char* function_name, void* sdl_vkGetInstanceProcAddr) -> PFN_vkVoidFunction {
            return reinterpret_cast<PFN_vkGetInstanceProcAddr>(sdl_vkGetInstanceProcAddr)(gpu::instance, function_name);
        },
        reinterpret_cast<void*>(sdl_vkGetInstanceProcAddr));
};

void gpu::quit()
{
    device_new->wait_for_device_idle();

    internal::quit_gpu_fences();

    delete device_new;

    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;

    volkFinalize();

    SDL_DestroyWindow(window);
    window = nullptr;
}

void gpu::transition_image(VkCommandBuffer const command_buffer, VkImage const image, const VkImageLayout layout_old, const VkImageLayout layout_new)
{
    VkImageMemoryBarrier2 barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    barrier.oldLayout = layout_old;
    barrier.newLayout = layout_new;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.image = image;

    VkDependencyInfo dinfo_image_layout {};
    dinfo_image_layout.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dinfo_image_layout.imageMemoryBarrierCount = 1;
    dinfo_image_layout.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2KHR(command_buffer, &dinfo_image_layout);
}

static bool is_swapchain_result_non_fatal(const VkResult result)
{
    switch (result)
    {
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
    case VK_ERROR_SURFACE_LOST_KHR:
        return true;
    default:
        return false;
    }
}

VkSemaphore gpu::frame_t::acquire_semaphore()
{
    const size_t idx = next_semaphore_idx++;
    if (idx >= semaphores.size())
    {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkSemaphoreCreateInfo cinfo {};
        cinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        cinfo.flags = VK_SEMAPHORE_TYPE_BINARY;
        VK_DIE(device->funcs.vkCreateSemaphore(device->logical, &cinfo, nullptr, &semaphore));
        semaphores.push_back(semaphore);
    }
    return semaphores[idx];
}

void gpu::frame_t::reset()
{
    VK_DIE(device->funcs.vkResetFences(device->logical, 1, &done));
    next_semaphore_idx = 0;
    used_graphics = 0;
    used_transfer = 0;
}

void gpu::frame_t::free()
{
    device->funcs.vkFreeCommandBuffers(device->logical, device->window.graphics_pool, 1, &cmd_graphics);
    device->funcs.vkFreeCommandBuffers(device->logical, device->window.transfer_pool, 1, &cmd_transfer);
    device->funcs.vkDestroyImageView(device->logical, image_view, nullptr);
    device->funcs.vkDestroyFence(device->logical, done, nullptr);
    for (VkSemaphore s : semaphores)
        device->funcs.vkDestroySemaphore(device->logical, s, nullptr);
}

static int thread_func_read_pipeline_cache(void* userdata)
{
    std::vector<Uint8>* pipeline_cache_data = reinterpret_cast<std::vector<Uint8>*>(userdata);

    PHYSFS_File* fd = PHYSFS_openRead(r_pipeline_cache_path.get().c_str());

    if (!fd)
        return 0;

    Sint64 file_length = PHYSFS_fileLength(fd);

    if (file_length > 0)
    {
        pipeline_cache_data->resize(file_length);

        /* If the file length changed then it was probably modified, so out of laziness and paranoia: invalidate the data */
        if (PHYSFS_readBytes(fd, pipeline_cache_data->data(), file_length) != file_length)
        {
            dc_log_error("Error while reading pipeline cache");
            pipeline_cache_data->resize(0);
        }
        else
            dc_log("Pipeline cache read into memory");
    }

    PHYSFS_close(fd);

    return 0;
}

static int thread_func_write_pipeline_cache(void* userdata)
{
    std::vector<Uint8>* pipeline_cache_data = reinterpret_cast<std::vector<Uint8>*>(userdata);

    /* Unfortunately PHYSFS does not support file renaming, so */
    std::string temp_path = r_pipeline_cache_path.get();

    PHYSFS_File* fd = PHYSFS_openWrite(temp_path.c_str());

    if (!fd)
        return 0;

    if (PHYSFS_writeBytes(fd, pipeline_cache_data->data(), pipeline_cache_data->size()) != Sint64(pipeline_cache_data->size()))
    {
        dc_log_error("Failed to save pipeline cache %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        PHYSFS_close(fd);
        PHYSFS_delete(temp_path.c_str());
        return 0;
    }

    /* Calling flush multiple times probably won't do anything, but it doesn't hurt too much to do it anyone */
    for (int i = 0; i < 32; i++)
        PHYSFS_flush(fd);
    PHYSFS_close(fd);

    dc_log("Saved pipeline cache");

    return 0;
}

gpu::device_t::device_t(SDL_Window* sdl_window)
{
    SDL_Thread* pipeline_cache_load_thread = nullptr;
    std::vector<Uint8> pipeline_cache_data;
    if (r_pipeline_cache.get())
        pipeline_cache_load_thread = SDL_CreateThread(thread_func_read_pipeline_cache, "Pipeline Cache file load task", &pipeline_cache_data);

    window.sdl_window = sdl_window;

    if (!SDL_Vulkan_CreateSurface(window.sdl_window, instance, nullptr, &window.sdl_surface))
        util::die("Unable to create vulkan surface! %s", SDL_GetError());

    std::vector<const char*> required_device_extensions = {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, /* Promoted to vulkan 1.3 */
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, /* Promoted to vulkan 1.3 */
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    gpu::physical_device_info_t device_info = {};

    if (!select_physical_device(instance, window.sdl_surface, required_device_extensions, device_info))
        util::die("Unable to find suitable Vulkan Device!\n"
                  "Try updating your Operating System and/or Graphics drivers");

    graphics_queue_idx = device_info.graphics_queue_idx;
    transfer_queue_idx = device_info.transfer_queue_idx;
    present_queue_idx = device_info.present_queue_idx;
    physical = device_info.device;

    logical = init_device(instance, device_info, required_device_extensions);
    set_object_name(logical, VK_OBJECT_TYPE_DEVICE, "gpu::device_t::logical");
    volkLoadDevice(logical);
    volkLoadDeviceTable(&funcs, logical);

    set_object_name(window.sdl_surface, VK_OBJECT_TYPE_SURFACE_KHR, "(Window %u): Surface", SDL_GetWindowID(window.sdl_window));

    funcs.vkGetDeviceQueue(logical, graphics_queue_idx, 0, &graphics_queue);
    funcs.vkGetDeviceQueue(logical, transfer_queue_idx, 0, &transfer_queue);
    funcs.vkGetDeviceQueue(logical, present_queue_idx, 0, &present_queue);

    /* Setup queue locks */
    for (const Uint32 family_idx : std::set<Uint32> { graphics_queue_idx, present_queue_idx, transfer_queue_idx })
    {
        SDL_Mutex* const lock = SDL_CreateMutex();
        if (graphics_queue_idx == family_idx)
            graphics_queue_lock = lock;
        if (transfer_queue_idx == family_idx)
            transfer_queue_lock = lock;
        if (present_queue_idx == family_idx)
            present_queue_lock = lock;
    }

    /* Init command pools */
    {
        VkCommandPoolCreateInfo cinfo_command_pool = {};
        cinfo_command_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cinfo_command_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        cinfo_command_pool.queueFamilyIndex = graphics_queue_idx;
        VK_DIE(funcs.vkCreateCommandPool(logical, &cinfo_command_pool, nullptr, &window.graphics_pool));
        set_object_name(window.graphics_pool, VK_OBJECT_TYPE_COMMAND_POOL, "Graphics pool");

        cinfo_command_pool.queueFamilyIndex = transfer_queue_idx;
        VK_DIE(funcs.vkCreateCommandPool(logical, &cinfo_command_pool, nullptr, &window.transfer_pool));
        set_object_name(window.transfer_pool, VK_OBJECT_TYPE_COMMAND_POOL, "Transfer pool");
    }

    VkFenceCreateInfo cinfo_fence {};
    cinfo_fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_DIE(funcs.vkCreateFence(logical, &cinfo_fence, nullptr, &window.acquire_fence));
    set_object_name(window.acquire_fence, VK_OBJECT_TYPE_FENCE, "(Window %u): Swapchain acquire fence", SDL_GetWindowID(window.sdl_window));

    if (r_pipeline_cache.get())
    {
        if (pipeline_cache_load_thread == nullptr)
            thread_func_read_pipeline_cache(&pipeline_cache_data);
        else
            SDL_WaitThread(pipeline_cache_load_thread, nullptr);

        if (create_pipeline_cache(physical, logical, pipeline_cache, pipeline_cache_data))
            dc_log("Created pipeline cache");
        else
            dc_log_error("Failed to create pipeline cache");

        set_object_name(pipeline_cache, VK_OBJECT_TYPE_PIPELINE_CACHE, "gpu::device_t::pipeline_cache");
    }
}

gpu::device_t::~device_t()
{
    wait_for_device_idle();

    SDL_Thread* pipeline_cache_save_thread = nullptr;
    std::vector<Uint8> pipeline_cache_data;
    if (r_pipeline_cache.get())
    {
        if (save_pipeline_cache(physical, logical, pipeline_cache, pipeline_cache_data))
        {
            dc_log("Prepared pipeline cache file");
            pipeline_cache_save_thread = SDL_CreateThread(thread_func_write_pipeline_cache, "Pipeline Cache file save task", &pipeline_cache_data);
        }
        else
            dc_log_error("Failed to prepare pipeline cache file");
    }

    funcs.vkDestroyPipelineCache(logical, pipeline_cache, nullptr);

    for (frame_t f : window.frames)
        f.free();
    window.frames.resize(0);

    funcs.vkDestroySwapchainKHR(logical, window.sdl_swapchain, nullptr);
    funcs.vkDestroyCommandPool(logical, window.graphics_pool, nullptr);
    funcs.vkDestroyCommandPool(logical, window.transfer_pool, nullptr);
    funcs.vkDestroyFence(logical, window.acquire_fence, nullptr);

    SDL_Vulkan_DestroySurface(instance, window.sdl_surface, nullptr);
    for (SDL_Mutex* m : std::set<SDL_Mutex*> { graphics_queue_lock, transfer_queue_lock, present_queue_lock })
        SDL_DestroyMutex(m);

    funcs.vkDestroyDevice(logical, nullptr);

    if (r_pipeline_cache.get())
    {
        if (pipeline_cache_save_thread == nullptr)
            thread_func_write_pipeline_cache(&pipeline_cache_data);
        else
            SDL_WaitThread(pipeline_cache_save_thread, nullptr);
    }
}

gpu::frame_t* gpu::device_t::acquire_next_frame(window_t* const window, const Uint64 timeout)
{
    if (window->frame_is_pending)
    {
        dc_log_error("Window %u already has a pending frame", SDL_GetWindowID(window->sdl_window));
        return nullptr;
    }

    VkSurfaceFormatKHR swapchain_old_format = window->format;
    Uint32 swapchain_old_num_images = window->frames.size();

    if (window->sdl_swapchain == VK_NULL_HANDLE)
        window->swapchain_rebuild_required = 1;

    int win_size_x = 0, win_size_y = 0;
    SDL_GetWindowSize(window->sdl_window, &win_size_x, &win_size_y);

    const SDL_WindowID window_id = SDL_GetWindowID(window->sdl_window);

    if (win_size_x <= 0 || win_size_y <= 0)
        return nullptr;

    if (Uint32(win_size_x) != window->extent.width || Uint32(win_size_y) != window->extent.height)
        window->swapchain_rebuild_required = 1;

    if (window->swapchain_rebuild_required)
    {
        TRACE("Rebuilding swapchain");

        window->sdl_swapchain = create_swapchain(this, window->sdl_window, window->sdl_surface, window->sdl_swapchain, window->format, window->extent);
        set_object_name(window->sdl_swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "(Window %u): Swapchain", window_id);

        for (frame_t f : window->frames)
            f.free();
        window->frames.resize(0);
    }

    if (window->swapchain_rebuild_required && window->sdl_swapchain != VK_NULL_HANDLE)
    {
        Uint32 image_count = 0;
        VK_DIE(funcs.vkGetSwapchainImagesKHR(logical, window->sdl_swapchain, &image_count, nullptr));
        std::vector<VkImage> swapchain_images(image_count);
        VK_DIE(funcs.vkGetSwapchainImagesKHR(logical, window->sdl_swapchain, &image_count, swapchain_images.data()));
        swapchain_images.resize(image_count);

        window->frames.resize(image_count);
        for (size_t i = 0; i < image_count; i++)
        {
            window->frames[i].device = this;

            window->frames[i].image_idx = i;

            window->frames[i].image = swapchain_images[i];
            set_object_name(window->frames[i].image, VK_OBJECT_TYPE_IMAGE, "(Window %u)(Frame %zu): Swapchain image", window_id, i);

            VkImageViewCreateInfo cinfo_image_view {};
            cinfo_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            cinfo_image_view.image = swapchain_images[i];
            cinfo_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            cinfo_image_view.format = window->format.format;
            cinfo_image_view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            cinfo_image_view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            cinfo_image_view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            cinfo_image_view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            cinfo_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cinfo_image_view.subresourceRange.baseMipLevel = 0;
            cinfo_image_view.subresourceRange.levelCount = 1;
            cinfo_image_view.subresourceRange.baseArrayLayer = 0;
            cinfo_image_view.subresourceRange.layerCount = 1;
            VK_DIE(funcs.vkCreateImageView(logical, &cinfo_image_view, nullptr, &window->frames[i].image_view));
            set_object_name(window->frames[i].image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "(Window %u)(Frame %zu): Swapchain image View", window_id, i);

            VkFenceCreateInfo cinfo_fence {};
            cinfo_fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            cinfo_fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_DIE(funcs.vkCreateFence(logical, &cinfo_fence, nullptr, &window->frames[i].done));
            set_object_name(window->frames[i].done, VK_OBJECT_TYPE_FENCE, "(Window %u)(Frame %zu): Done fence", window_id, i);

            VkCommandBufferAllocateInfo ainfo_command_buffer {};
            ainfo_command_buffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ainfo_command_buffer.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ainfo_command_buffer.commandBufferCount = 1;

            ainfo_command_buffer.commandPool = window->graphics_pool;
            VK_DIE(funcs.vkAllocateCommandBuffers(logical, &ainfo_command_buffer, &window->frames[i].cmd_graphics));
            set_object_name(window->frames[i].cmd_graphics, VK_OBJECT_TYPE_COMMAND_BUFFER, "(Window %u)(Frame %zu): cmd_graphics", window_id, i);

            ainfo_command_buffer.commandPool = window->transfer_pool;
            VK_DIE(funcs.vkAllocateCommandBuffers(logical, &ainfo_command_buffer, &window->frames[i].cmd_transfer));
            set_object_name(window->frames[i].cmd_transfer, VK_OBJECT_TYPE_COMMAND_BUFFER, "(Window %u)(Frame %zu): cmd_transfer", window_id, i);
        }

        bool format_changed = swapchain_old_format.format != window->format.format;
        bool colorspace_changed = swapchain_old_format.colorSpace != window->format.colorSpace;
        if (window->format_callback != nullptr && (format_changed || colorspace_changed))
            window->format_callback(format_changed, colorspace_changed, window->format_callback_userdata);

        if (window->num_images_callback != nullptr && (swapchain_old_num_images != image_count))
            window->num_images_callback(image_count, window->num_images_callback_userdata);

        TRACE("Swapchain has %u images", image_count);

        window->swapchain_rebuild_required = 0;
    }

    if (window->sdl_swapchain == VK_NULL_HANDLE)
        return nullptr;

    Uint32 image_idx = 0;

    VK_DIE(funcs.vkResetFences(logical, 1, &window->acquire_fence));
    VkResult result = funcs.vkAcquireNextImageKHR(logical, window->sdl_swapchain, timeout, VK_NULL_HANDLE, window->acquire_fence, &image_idx);
    if (result == VK_SUBOPTIMAL_KHR)
        window->swapchain_rebuild_required = 1;
    else if (result == VK_TIMEOUT || result == VK_NOT_READY)
        return nullptr;
    else if (is_swapchain_result_non_fatal(result))
    {
        window->swapchain_rebuild_required = 1;
        return nullptr;
    }
    else
        VK_DIE(result; (void)"vkAcquireNextImageKHR");

    window->frame_is_pending = 1;

    /* A semaphore might be a better way to delay this, but this is easier */
    frame_t& frame = window->frames[image_idx];
    VK_DIE(funcs.vkWaitForFences(logical, 1, &window->acquire_fence, 1, UINT64_MAX));
    frame.reset();

    return &frame;
}

void gpu::device_t::submit_frame(window_t* const window, frame_t* frame)
{
    if (!window || !frame)
        return;
    window->frame_is_pending = 0;

    VkSemaphore present_semaphore = frame->acquire_semaphore();

    VkCommandBuffer command_buffers[2] = {};

    VkSubmitInfo sinfo {};
    sinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sinfo.commandBufferCount = 0;
    sinfo.pCommandBuffers = command_buffers;
    sinfo.signalSemaphoreCount = 1;
    sinfo.pSignalSemaphores = &present_semaphore;

    if (frame->used_graphics)
        command_buffers[sinfo.commandBufferCount++] = frame->cmd_graphics;

    if (frame->used_transfer)
        command_buffers[sinfo.commandBufferCount++] = frame->cmd_transfer;

    /* Ensure image layout is properly transitioned, even if it hasn't been used */
    if (sinfo.commandBufferCount == 0)
    {
        VkCommandBufferBeginInfo binfo_command_buffer {};
        binfo_command_buffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        binfo_command_buffer.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_DIE(funcs.vkBeginCommandBuffer(frame->cmd_graphics, &binfo_command_buffer));

        transition_image(frame->cmd_graphics, frame->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_DIE(funcs.vkEndCommandBuffer(frame->cmd_graphics));

        command_buffers[sinfo.commandBufferCount++] = frame->cmd_graphics;
    }

    VkPresentInfoKHR pinfo {};
    pinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pinfo.waitSemaphoreCount = 1;
    pinfo.pWaitSemaphores = &present_semaphore;
    pinfo.swapchainCount = 1;
    pinfo.pSwapchains = &gpu::device_new->window.sdl_swapchain;
    pinfo.pImageIndices = &frame->image_idx;

    if (frame->used_graphics)
        SDL_LockMutex(graphics_queue_lock);
    if (frame->used_transfer)
        SDL_LockMutex(transfer_queue_lock);
    SDL_LockMutex(present_queue_lock);
    VK_DIE(funcs.vkQueueSubmit(graphics_queue, 1, &sinfo, frame->done));
    {
        VkResult result = funcs.vkQueuePresentKHR(present_queue, &pinfo);
        if (is_swapchain_result_non_fatal(result))
            window->swapchain_rebuild_required = 1;
        else
            VK_DIE(result; (void)"vkQueuePresentKHR");
    }
    SDL_UnlockMutex(present_queue_lock);
    if (frame->used_transfer)
        SDL_UnlockMutex(transfer_queue_lock);
    if (frame->used_graphics)
        SDL_UnlockMutex(graphics_queue_lock);
}

void gpu::device_t::lock_all_queues()
{
    SDL_LockMutex(graphics_queue_lock);
    SDL_LockMutex(transfer_queue_lock);
    SDL_LockMutex(present_queue_lock);
}

void gpu::device_t::unlock_all_queues()
{
    SDL_UnlockMutex(graphics_queue_lock);
    SDL_UnlockMutex(transfer_queue_lock);
    SDL_UnlockMutex(present_queue_lock);
}

void gpu::device_t::wait_for_device_idle()
{
    lock_all_queues();
    funcs.vkDeviceWaitIdle(logical);
    unlock_all_queues();
}

void gpu::device_t::set_object_name_real(Uint64 object_handle, VkObjectType object_type, const char* fmt, va_list args)
{
    if (!vkSetDebugUtilsObjectNameEXT || object_handle == (Uint64)VK_NULL_HANDLE)
        return;
    char name[1024] = "";

    stbsp_vsnprintf(name, SDL_arraysize(name), fmt, args);

    VkDebugUtilsObjectNameInfoEXT ninfo = {};
    ninfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    ninfo.objectType = object_type;
    ninfo.objectHandle = object_handle;
    ninfo.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(logical, &ninfo);
}
