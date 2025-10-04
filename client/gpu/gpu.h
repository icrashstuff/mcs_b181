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
#include "sampler.h"
#include "subdiv_buffer.h"
#include "texture.h"

#include "volk/volk.h"

#include "vk_mem_alloc.h"

#include "tetra/util/misc.h" /* util::die() */

/** Die on an error from a function returning VkResult */
#define VK_DIE(_CALL)                                                                              \
    do                                                                                             \
    {                                                                                              \
        VkResult result__ = _CALL;                                                                 \
        if (result__ != VK_SUCCESS)                                                                \
            util::die("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult */
#define VK_TRY(_CALL)                                                                                 \
    do                                                                                                \
    {                                                                                                 \
        VkResult result__ = _CALL;                                                                    \
        if (result__ != VK_SUCCESS)                                                                   \
            dc_log_error("%s failed with code: %d: %s", #_CALL, result__, string_VkResult(result__)); \
    } while (0)

/** Log an error from a function returning VkResult, and save the result */
#define VK_TRY_STORE(RESULT, _CALL)                                                               \
    do                                                                                            \
    {                                                                                             \
        RESULT = _CALL;                                                                           \
        if (RESULT != VK_SUCCESS)                                                                 \
            dc_log_error("%s failed with code: %d: %s", #_CALL, RESULT, string_VkResult(RESULT)); \
    } while (0)

namespace gpu
{
/**
 * Init GPU api
 *
 * This will either return on success, or exit the program on failure!
 */
void init();

/**
 * Run a little test app to verify some things are working
 *
 * `gpu::init()` must be called before calling this function
 */
void simple_test_app();

void quit();

extern SDL_Window* window;

extern VkInstance instance;
/** Value passed to VkApplicationInfo::apiVersion */
extern const Uint32 instance_api_version;

struct device_t;

struct frame_t
{
    device_t* device = nullptr;

    Uint32 image_idx = ~0;

    /** Target image */
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;

    /** Command buffer allocated for the graphics queue, from `window_t::graphics_pool` */
    VkCommandBuffer cmd_graphics = VK_NULL_HANDLE;
    bool used_graphics = 0;

    /** Command buffer allocated for the transfer queue, from `window_t::transfer_pool` */
    VkCommandBuffer cmd_transfer = VK_NULL_HANDLE;
    bool used_transfer = 0;

    /** Fence to be associated with command buffer submission */
    VkFence done = VK_NULL_HANDLE;

    /**
     * Acquire a semaphore from the internal buffer
     *
     * NOTE: You should not reference a semaphore outside of the command buffers
     *       provided by this object, as the semaphores may be destroyed/reused at any time
     */
    VkSemaphore acquire_semaphore();

    void reset();
    void free();

private:
    size_t next_semaphore_idx = 0;
    std::vector<VkSemaphore> semaphores;
};

struct window_t
{
    /* TODO-OPT: OpenXR handles here? */
    // XrSwapchain xr_swapchain = XR_NULL_HANDLE;

    SDL_Window* sdl_window = nullptr;
    VkSurfaceKHR sdl_surface = VK_NULL_HANDLE;
    VkSwapchainKHR sdl_swapchain = VK_NULL_HANDLE;
    VkFence acquire_fence = VK_NULL_HANDLE;
    bool frame_is_pending = 0;

    bool swapchain_rebuild_required = 0;
    VkExtent2D extent {};
    VkSurfaceFormatKHR format {};
    VkPresentModeKHR present_mode {};

    VkCommandPool graphics_pool = VK_NULL_HANDLE;
    VkCommandPool transfer_pool = VK_NULL_HANDLE;

    /**
     * Called when the swapchain format is changed, or when the swapchain is initially created
     *
     * The intended usage is for recreating pipelines that target the swapchain
     *
     * When this callback is fired, `window_t::format` will contain the new format
     *
     * @param format_changed Set to true when the underlying image format has changed
     * @param colorspace_changed Set to true when the colorspace has changed
     * @param userdata Set to the value of `window_t::format_callback_userdata`
     */
    void (*format_callback)(const bool format_changed, const bool colorspace_changed, void* userdata) = nullptr;
    void* format_callback_userdata = nullptr;

    /**
     * Called when the number of swapchain frames changes, or when the swapchain is initially created
     *
     * The intended usage is for informing things like Dear ImGui's Vulkan backend that the number of images changed
     *
     * When this callback is fired, `window_t::frames` will contain the new frames
     *
     * @param num_images Contains the new number of frames
     * @param userdata Set to the value of `window_t::num_images_callback_userdata`
     */
    void (*num_images_callback)(const Uint32 num_images, void* userdata) = nullptr;
    void* num_images_callback_userdata = nullptr;

    std::vector<frame_t> frames;
};

struct queue_t
{
    VkQueue handle = VK_NULL_HANDLE;
    Uint32 index = ~0;
    SDL_Mutex* lock = nullptr;
};

struct device_t
{
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice logical = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    /* ======================================================== */
    /* ================ Window/Swapchain stuff ================ */

    window_t window {};

    /**
     * Acquire a frame for rendering
     *
     * NOTE: This may resize/reconfigure the swapchain
     *
     * @returns A frame object on success, or NULL on failure/unavailability of a new frame
     */
    frame_t* acquire_next_frame(window_t* const window, const Uint64 timeout);

    /**
     * Submit a frame for presentation
     *
     * @param window the frame was acquired from
     * @param frame frame to submit (You must not reference the frame after calling this function)
     */
    void submit_frame(window_t* const window, frame_t* frame);

    /* ============================================= */
    /* ================ Queue stuff ================ */

    struct
    {
        VkSharingMode sharingMode;
        uint32_t queueFamilyIndexCount;
        const uint32_t* pQueueFamilyIndices;

        template <typename T> void apply(T& dest) const
        {
            dest.sharingMode = sharingMode;
            dest.queueFamilyIndexCount = queueFamilyIndexCount;
            dest.pQueueFamilyIndices = pQueueFamilyIndices;
        }
    } queue_sharing;

    queue_t graphics_queue;
    queue_t transfer_queue;
    queue_t present_queue;

    /** Convenience function to call SDL_LockMutex() on each queue lock */
    void lock_all_queues();

    /** Convenience function to call SDL_UnlockMutex() on each queue lock */
    void unlock_all_queues();

    /** Locks all queues, calls vkDeviceWaitIdle(), and then unlocks all queues  */
    void wait_for_device_idle();

    device_t(SDL_Window* sdl_window, const VkAllocationCallbacks* _allocation_callbacks = nullptr);
    ~device_t();

    VolkDeviceTable funcs;

    /** Convenience function to insert an image layout transition pipeline barrier */
    void transition_image(VkCommandBuffer const command_buffer, VkImage const image, const VkImageLayout layout_old, const VkImageLayout layout_new);

private:
    void set_object_name_real(Uint64 object_handle, VkObjectType object_type, const char* fmt, va_list args);

public:
    /**
     * Wrapper around vkSetDebugUtilsObjectNameEXT(3)
     *
     * If vkSetDebugUtilsObjectNameEXT(3) is not available, this function is a safe a no-op
     *
     * @param object_handle Object handle (If object_handle == VK_NULL_HANDLE, this function is a safe a no-op)
     * @param object_type Object type
     * @param fmt stb_sprintf format string
     */
    template <typename T>
    void set_object_name(T object_handle, VkObjectType object_type, SDL_PRINTF_FORMAT_STRING const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(4)
    {
        va_list args;
        va_start(args, fmt);
        set_object_name_real(reinterpret_cast<Uint64>(object_handle), object_type, fmt, args);
        va_end(args);
    }

    friend struct frame_t;

    const VkAllocationCallbacks* allocation_callbacks = nullptr;

    /* Instance functions */
    /* vkCreateDevice(3), vkCreateInstance(3), vkDestroyInstance(3) purposely excluded because they are not needed */

#ifdef VK_VERSION_1_0
    inline VkResult vkEnumeratePhysicalDevices(uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
    {
        return ::vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    }
    inline void vkGetPhysicalDeviceFeatures(VkPhysicalDeviceFeatures* pFeatures) { return ::vkGetPhysicalDeviceFeatures(physical, pFeatures); }
    inline void vkGetPhysicalDeviceFormatProperties(VkFormat format, VkFormatProperties* pFormatProperties)
    {
        return ::vkGetPhysicalDeviceFormatProperties(physical, format, pFormatProperties);
    }
    inline VkResult vkGetPhysicalDeviceImageFormatProperties(VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage,
        VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties)
    {
        return ::vkGetPhysicalDeviceImageFormatProperties(physical, format, type, tiling, usage, flags, pImageFormatProperties);
    }
    inline void vkGetPhysicalDeviceProperties(VkPhysicalDeviceProperties* pProperties) { return ::vkGetPhysicalDeviceProperties(physical, pProperties); }
    inline void vkGetPhysicalDeviceQueueFamilyProperties(uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties)
    {
        return ::vkGetPhysicalDeviceQueueFamilyProperties(physical, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }
    inline void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDeviceMemoryProperties* pMemoryProperties)
    {
        return ::vkGetPhysicalDeviceMemoryProperties(physical, pMemoryProperties);
    }
    inline PFN_vkVoidFunction vkGetInstanceProcAddr(const char* pName) { ::vkGetInstanceProcAddr(instance, pName); }
    inline PFN_vkVoidFunction vkGetDeviceProcAddr(const char* pName) { return ::vkGetDeviceProcAddr(logical, pName); }
    inline VkResult vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
    {
        return ::vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
    }
    inline VkResult vkEnumerateDeviceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
    {
        return ::vkEnumerateDeviceExtensionProperties(physical, pLayerName, pPropertyCount, pProperties);
    }
    inline VkResult vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties)
    {
        return ::vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
    }
    inline VkResult vkEnumerateDeviceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties)
    {
        return ::vkEnumerateDeviceLayerProperties(physical, pPropertyCount, pProperties);
    }
    inline void vkGetPhysicalDeviceSparseImageFormatProperties(VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
        VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties)
    {
        return ::vkGetPhysicalDeviceSparseImageFormatProperties(physical, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    }
#endif

#ifdef VK_VERSION_1_1
    inline VkResult vkEnumerateInstanceVersion(uint32_t* pApiVersion) { return ::vkEnumerateInstanceVersion(pApiVersion); }
    inline VkResult vkEnumeratePhysicalDeviceGroups(uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties)
    {
        return ::vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    }
    inline void vkGetPhysicalDeviceFeatures2(VkPhysicalDeviceFeatures2* pFeatures) { return ::vkGetPhysicalDeviceFeatures2(physical, pFeatures); }
    inline void vkGetPhysicalDeviceProperties2(VkPhysicalDeviceProperties2* pProperties) { return ::vkGetPhysicalDeviceProperties2(physical, pProperties); }
    inline void vkGetPhysicalDeviceFormatProperties2(VkFormat format, VkFormatProperties2* pFormatProperties)
    {
        return ::vkGetPhysicalDeviceFormatProperties2(physical, format, pFormatProperties);
    }
    inline VkResult vkGetPhysicalDeviceImageFormatProperties2(
        const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties)
    {
        return ::vkGetPhysicalDeviceImageFormatProperties2(physical, pImageFormatInfo, pImageFormatProperties);
    }
    inline void vkGetPhysicalDeviceQueueFamilyProperties2(uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties)
    {
        return ::vkGetPhysicalDeviceQueueFamilyProperties2(physical, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }
    inline void vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDeviceMemoryProperties2* pMemoryProperties)
    {
        ::vkGetPhysicalDeviceMemoryProperties2(physical, pMemoryProperties);
    }
    inline void vkGetPhysicalDeviceSparseImageFormatProperties2(
        const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties)
    {
        return ::vkGetPhysicalDeviceSparseImageFormatProperties2(physical, pFormatInfo, pPropertyCount, pProperties);
    }
    inline void vkGetPhysicalDeviceExternalBufferProperties(
        const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties)
    {
        return ::vkGetPhysicalDeviceExternalBufferProperties(physical, pExternalBufferInfo, pExternalBufferProperties);
    }
    inline void vkGetPhysicalDeviceExternalFenceProperties(
        const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties)
    {
        return ::vkGetPhysicalDeviceExternalFenceProperties(physical, pExternalFenceInfo, pExternalFenceProperties);
    }
    inline void vkGetPhysicalDeviceExternalSemaphoreProperties(
        const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties)
    {
        return ::vkGetPhysicalDeviceExternalSemaphoreProperties(physical, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    }
#endif

#ifdef VK_KHR_surface
    inline void vkDestroySurfaceKHR(VkSurfaceKHR surface) { return ::vkDestroySurfaceKHR(instance, surface, allocation_callbacks); }
    inline VkResult vkGetPhysicalDeviceSurfaceSupportKHR(uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported)
    {
        return ::vkGetPhysicalDeviceSurfaceSupportKHR(physical, queueFamilyIndex, surface, pSupported);
    }
    inline VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities)
    {
        return ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, pSurfaceCapabilities);
    }
    inline VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats)
    {
        return ::vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, pSurfaceFormatCount, pSurfaceFormats);
    }
    inline VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes)
    {
        return ::vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, pPresentModeCount, pPresentModes);
    }
#endif

#ifdef VK_KHR_swapchain
    inline VkResult vkGetPhysicalDevicePresentRectanglesKHR(VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects)
    {
        return ::vkGetPhysicalDevicePresentRectanglesKHR(physical, surface, pRectCount, pRects);
    }
#endif

    /* Device functions */
    /* vkDestroyDevice(3) purposely excluded because it is not needed */
    /* vkDeviceWaitIdle(3) purposely excluded because gpu::device_t::wait_for_device_idle(3) achieves the same thing but safely */

#ifdef VK_VERSION_1_0
    inline void vkGetDeviceQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
    {
        return funcs.vkGetDeviceQueue(logical, queueFamilyIndex, queueIndex, pQueue);
    }
    inline VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
    {
        return funcs.vkQueueSubmit(queue, submitCount, pSubmits, fence);
    }
    inline VkResult vkQueueWaitIdle(VkQueue queue) { return funcs.vkQueueWaitIdle(queue); }

    inline VkResult vkAllocateMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory)
    {
        return funcs.vkAllocateMemory(logical, pAllocateInfo, allocation_callbacks, pMemory);
    }
    inline void vkFreeMemory(VkDeviceMemory memory) { return funcs.vkFreeMemory(logical, memory, allocation_callbacks); }
    inline VkResult vkMapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
    {
        return funcs.vkMapMemory(logical, memory, offset, size, flags, ppData);
    }
    inline void vkUnmapMemory(VkDeviceMemory memory) { return funcs.vkUnmapMemory(logical, memory); }
    inline VkResult vkFlushMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
    {
        return funcs.vkFlushMappedMemoryRanges(logical, memoryRangeCount, pMemoryRanges);
    }
    inline VkResult vkInvalidateMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
    {
        return funcs.vkInvalidateMappedMemoryRanges(logical, memoryRangeCount, pMemoryRanges);
    }
    inline void vkGetDeviceMemoryCommitment(VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes)
    {
        return funcs.vkGetDeviceMemoryCommitment(logical, memory, pCommittedMemoryInBytes);
    }
    inline VkResult vkBindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
    {
        return funcs.vkBindBufferMemory(logical, buffer, memory, memoryOffset);
    }
    inline VkResult vkBindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
    {
        return funcs.vkBindImageMemory(logical, image, memory, memoryOffset);
    }
    inline void vkGetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements)
    {
        return funcs.vkGetBufferMemoryRequirements(logical, buffer, pMemoryRequirements);
    }
    inline void vkGetImageMemoryRequirements(VkImage image, VkMemoryRequirements* pMemoryRequirements)
    {
        return funcs.vkGetImageMemoryRequirements(logical, image, pMemoryRequirements);
    }
    inline void vkGetImageSparseMemoryRequirements(
        VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements)
    {
        return funcs.vkGetImageSparseMemoryRequirements(logical, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    inline VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence)
    {
        return funcs.vkQueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
    }
    inline VkResult vkCreateFence(const VkFenceCreateInfo* pCreateInfo, VkFence* pFence)
    {
        return funcs.vkCreateFence(logical, pCreateInfo, allocation_callbacks, pFence);
    }
    inline void vkDestroyFence(VkFence fence) { return funcs.vkDestroyFence(logical, fence, allocation_callbacks); }
    inline VkResult vkResetFences(uint32_t fenceCount, const VkFence* pFences) { return funcs.vkResetFences(logical, fenceCount, pFences); }
    inline VkResult vkGetFenceStatus(VkFence fence) { return funcs.vkGetFenceStatus(logical, fence); }
    inline VkResult vkWaitForFences(uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
    {
        return funcs.vkWaitForFences(logical, fenceCount, pFences, waitAll, timeout);
    }
    inline VkResult vkCreateSemaphore(const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore)
    {
        return funcs.vkCreateSemaphore(logical, pCreateInfo, allocation_callbacks, pSemaphore);
    }
    inline void vkDestroySemaphore(VkSemaphore semaphore) { return funcs.vkDestroySemaphore(logical, semaphore, allocation_callbacks); }
    inline VkResult vkCreateEvent(const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent)
    {
        return funcs.vkCreateEvent(logical, pCreateInfo, allocation_callbacks, pEvent);
    }
    inline void vkDestroyEvent(VkEvent event) { return funcs.vkDestroyEvent(logical, event, allocation_callbacks); }
    inline VkResult vkGetEventStatus(VkEvent event) { return funcs.vkGetEventStatus(logical, event); }
    inline VkResult vkSetEvent(VkEvent event) { return funcs.vkSetEvent(logical, event); }
    inline VkResult vkResetEvent(VkEvent event) { return funcs.vkResetEvent(logical, event); }
    inline VkResult vkCreateQueryPool(const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool)
    {
        return funcs.vkCreateQueryPool(logical, pCreateInfo, allocation_callbacks, pQueryPool);
    }
    inline void vkDestroyQueryPool(VkQueryPool queryPool) { return funcs.vkDestroyQueryPool(logical, queryPool, allocation_callbacks); }
    inline VkResult vkGetQueryPoolResults(
        VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags)
    {
        return funcs.vkGetQueryPoolResults(logical, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    }
    inline VkResult vkCreateBuffer(const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer)
    {
        return funcs.vkCreateBuffer(logical, pCreateInfo, allocation_callbacks, pBuffer);
    }
    inline void vkDestroyBuffer(VkBuffer buffer) { return funcs.vkDestroyBuffer(logical, buffer, allocation_callbacks); }
    inline VkResult vkCreateBufferView(const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView)
    {
        return funcs.vkCreateBufferView(logical, pCreateInfo, allocation_callbacks, pView);
    }
    inline void vkDestroyBufferView(VkBufferView bufferView) { return funcs.vkDestroyBufferView(logical, bufferView, allocation_callbacks); }
    inline VkResult vkCreateImage(const VkImageCreateInfo* pCreateInfo, VkImage* pImage)
    {
        return funcs.vkCreateImage(logical, pCreateInfo, allocation_callbacks, pImage);
    }
    inline void vkDestroyImage(VkImage image) { return funcs.vkDestroyImage(logical, image, allocation_callbacks); }
    inline void vkGetImageSubresourceLayout(VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout)
    {
        return funcs.vkGetImageSubresourceLayout(logical, image, pSubresource, pLayout);
    }
    inline VkResult vkCreateImageView(const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView)
    {
        return funcs.vkCreateImageView(logical, pCreateInfo, allocation_callbacks, pView);
    }
    inline void vkDestroyImageView(VkImageView imageView) { return funcs.vkDestroyImageView(logical, imageView, allocation_callbacks); }
    inline VkResult vkCreateShaderModule(const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule)
    {
        return funcs.vkCreateShaderModule(logical, pCreateInfo, allocation_callbacks, pShaderModule);
    }
    inline void vkDestroyShaderModule(VkShaderModule shaderModule) { return funcs.vkDestroyShaderModule(logical, shaderModule, allocation_callbacks); }
    inline VkResult vkCreatePipelineCache(const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache)
    {
        return funcs.vkCreatePipelineCache(logical, pCreateInfo, allocation_callbacks, pPipelineCache);
    }
    inline void vkDestroyPipelineCache(VkPipelineCache pipelineCache) { return funcs.vkDestroyPipelineCache(logical, pipelineCache, allocation_callbacks); }
    inline VkResult vkGetPipelineCacheData(VkPipelineCache pipelineCache, size_t* pDataSize, void* pData)
    {
        return funcs.vkGetPipelineCacheData(logical, pipelineCache, pDataSize, pData);
    }
    inline VkResult vkMergePipelineCaches(VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches)
    {
        return funcs.vkMergePipelineCaches(logical, dstCache, srcCacheCount, pSrcCaches);
    }
    inline VkResult vkCreateGraphicsPipelines(
        VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines)
    {
        return funcs.vkCreateGraphicsPipelines(logical, pipelineCache, createInfoCount, pCreateInfos, allocation_callbacks, pPipelines);
    }
    inline VkResult vkCreateComputePipelines(
        VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines)
    {
        return funcs.vkCreateComputePipelines(logical, pipelineCache, createInfoCount, pCreateInfos, allocation_callbacks, pPipelines);
    }
    inline void vkDestroyPipeline(VkPipeline pipeline) { return funcs.vkDestroyPipeline(logical, pipeline, allocation_callbacks); }
    inline VkResult vkCreatePipelineLayout(const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout)
    {
        return funcs.vkCreatePipelineLayout(logical, pCreateInfo, allocation_callbacks, pPipelineLayout);
    }
    inline void vkDestroyPipelineLayout(VkPipelineLayout pipelineLayout)
    {
        return funcs.vkDestroyPipelineLayout(logical, pipelineLayout, allocation_callbacks);
    }
    inline VkResult vkCreateSampler(const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler)
    {
        return funcs.vkCreateSampler(logical, pCreateInfo, allocation_callbacks, pSampler);
    }
    inline void vkDestroySampler(VkSampler sampler) { return funcs.vkDestroySampler(logical, sampler, allocation_callbacks); }
    inline VkResult vkCreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout)
    {
        return funcs.vkCreateDescriptorSetLayout(logical, pCreateInfo, allocation_callbacks, pSetLayout);
    }
    inline void vkDestroyDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout)
    {
        return funcs.vkDestroyDescriptorSetLayout(logical, descriptorSetLayout, allocation_callbacks);
    }
    inline VkResult vkCreateDescriptorPool(const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool)
    {
        return funcs.vkCreateDescriptorPool(logical, pCreateInfo, allocation_callbacks, pDescriptorPool);
    }
    inline void vkDestroyDescriptorPool(VkDescriptorPool descriptorPool)
    {
        return funcs.vkDestroyDescriptorPool(logical, descriptorPool, allocation_callbacks);
    }
    inline VkResult vkResetDescriptorPool(VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
    {
        return funcs.vkResetDescriptorPool(logical, descriptorPool, flags);
    }
    inline VkResult vkAllocateDescriptorSets(const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets)
    {
        return funcs.vkAllocateDescriptorSets(logical, pAllocateInfo, pDescriptorSets);
    }
    inline VkResult vkFreeDescriptorSets(VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets)
    {
        return funcs.vkFreeDescriptorSets(logical, descriptorPool, descriptorSetCount, pDescriptorSets);
    }
    inline void vkUpdateDescriptorSets(uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount,
        const VkCopyDescriptorSet* pDescriptorCopies)
    {
        return funcs.vkUpdateDescriptorSets(logical, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }
    inline VkResult vkCreateFramebuffer(const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
    {
        return funcs.vkCreateFramebuffer(logical, pCreateInfo, allocation_callbacks, pFramebuffer);
    }
    inline void vkDestroyFramebuffer(VkFramebuffer framebuffer) { return funcs.vkDestroyFramebuffer(logical, framebuffer, allocation_callbacks); }
    inline VkResult vkCreateRenderPass(const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass)
    {
        return funcs.vkCreateRenderPass(logical, pCreateInfo, allocation_callbacks, pRenderPass);
    }
    inline void vkDestroyRenderPass(VkRenderPass renderPass) { return funcs.vkDestroyRenderPass(logical, renderPass, allocation_callbacks); }
    inline void vkGetRenderAreaGranularity(VkRenderPass renderPass, VkExtent2D* pGranularity)
    {
        return funcs.vkGetRenderAreaGranularity(logical, renderPass, pGranularity);
    }
    inline VkResult vkCreateCommandPool(const VkCommandPoolCreateInfo* pCreateInfo, VkCommandPool* pCommandPool)
    {
        return funcs.vkCreateCommandPool(logical, pCreateInfo, allocation_callbacks, pCommandPool);
    }
    inline void vkDestroyCommandPool(VkCommandPool commandPool) { return funcs.vkDestroyCommandPool(logical, commandPool, allocation_callbacks); }
    inline VkResult vkResetCommandPool(VkCommandPool commandPool, VkCommandPoolResetFlags flags)
    {
        return funcs.vkResetCommandPool(logical, commandPool, flags);
    }
    inline VkResult vkAllocateCommandBuffers(const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
    {
        return funcs.vkAllocateCommandBuffers(logical, pAllocateInfo, pCommandBuffers);
    }
    inline void vkFreeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
    {
        return funcs.vkFreeCommandBuffers(logical, commandPool, commandBufferCount, pCommandBuffers);
    }
    inline VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
    {
        return funcs.vkBeginCommandBuffer(commandBuffer, pBeginInfo);
    }
    inline VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) { return funcs.vkEndCommandBuffer(commandBuffer); }
    inline VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
    {
        return funcs.vkResetCommandBuffer(commandBuffer, flags);
    }
    inline void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
    {
        return funcs.vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
    inline void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
    {
        return funcs.vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
    }
    inline void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
    {
        return funcs.vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
    }
    inline void vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) { return funcs.vkCmdSetLineWidth(commandBuffer, lineWidth); }
    inline void vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
    {
        return funcs.vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    }
    inline void vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4])
    {
        return funcs.vkCmdSetBlendConstants(commandBuffer, blendConstants);
    }
    inline void vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
    {
        return funcs.vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
    }
    inline void vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
    {
        return funcs.vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
    }
    inline void vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
    {
        return funcs.vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
    }
    inline void vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
    {
        return funcs.vkCmdSetStencilReference(commandBuffer, faceMask, reference);
    }
    inline void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet,
        uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
    {
        return funcs.vkCmdBindDescriptorSets(
            commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
    inline void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
    {
        return funcs.vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
    }
    inline void vkCmdBindVertexBuffers(
        VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
    {
        return funcs.vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    }
    inline void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        return funcs.vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }
    inline void vkCmdDrawIndexed(
        VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
    {
        return funcs.vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }
    inline void vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
    {
        return funcs.vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
    }
    inline void vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
    {
        return funcs.vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
    }
    inline void vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        return funcs.vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    }
    inline void vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
    {
        return funcs.vkCmdDispatchIndirect(commandBuffer, buffer, offset);
    }
    inline void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
    {
        return funcs.vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    }
    inline void vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
        uint32_t regionCount, const VkImageCopy* pRegions)
    {
        return funcs.vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }
    inline void vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
        uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter)
    {
        return funcs.vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }
    inline void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount,
        const VkBufferImageCopy* pRegions)
    {
        return funcs.vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }
    inline void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount,
        const VkBufferImageCopy* pRegions)
    {
        return funcs.vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    }
    inline void vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData)
    {
        return funcs.vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    }
    inline void vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
    {
        return funcs.vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
    }
    inline void vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor,
        uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
    {
        return funcs.vkCmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    }
    inline void vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout,
        const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
    {
        return funcs.vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    }
    inline void vkCmdClearAttachments(
        VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects)
    {
        return funcs.vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    }
    inline void vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
        uint32_t regionCount, const VkImageResolve* pRegions)
    {
        return funcs.vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }
    inline void vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
    {
        return funcs.vkCmdSetEvent(commandBuffer, event, stageMask);
    }
    inline void vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
    {
        return funcs.vkCmdResetEvent(commandBuffer, event, stageMask);
    }
    inline void vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
    {
        return funcs.vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers,
            bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }
    inline void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
    {
        return funcs.vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers,
            bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }
    inline void vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
    {
        return funcs.vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
    }
    inline void vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
    {
        return funcs.vkCmdEndQuery(commandBuffer, queryPool, query);
    }
    inline void vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
    {
        return funcs.vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
    }
    inline void vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
    {
        return funcs.vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
    }
    inline void vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer,
        VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
    {
        return funcs.vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    }
    inline void vkCmdPushConstants(
        VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
    {
        return funcs.vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    }
    inline void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
    {
        return funcs.vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    }
    inline void vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) { return funcs.vkCmdNextSubpass(commandBuffer, contents); }
    inline void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) { return funcs.vkCmdEndRenderPass(commandBuffer); }
    inline void vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
    {
        return funcs.vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
    }
#endif

#ifdef VK_VERSION_1_1
    inline VkResult vkBindBufferMemory2(uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos)
    {
        return funcs.vkBindBufferMemory2(logical, bindInfoCount, pBindInfos);
    }
    inline VkResult vkBindImageMemory2(uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos)
    {
        return funcs.vkBindImageMemory2(logical, bindInfoCount, pBindInfos);
    }
    inline void vkGetDeviceGroupPeerMemoryFeatures(
        uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures)
    {
        return funcs.vkGetDeviceGroupPeerMemoryFeatures(logical, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    }
    inline void vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) { return funcs.vkCmdSetDeviceMask(commandBuffer, deviceMask); }
    inline void vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX,
        uint32_t groupCountY, uint32_t groupCountZ)
    {
        return funcs.vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    }
    inline void vkGetImageMemoryRequirements2(const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
    {
        return funcs.vkGetImageMemoryRequirements2(logical, pInfo, pMemoryRequirements);
    }
    inline void vkGetBufferMemoryRequirements2(const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements)
    {
        return funcs.vkGetBufferMemoryRequirements2(logical, pInfo, pMemoryRequirements);
    }
    inline void vkGetImageSparseMemoryRequirements2(
        const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements)
    {
        return funcs.vkGetImageSparseMemoryRequirements2(logical, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    inline void vkTrimCommandPool(VkCommandPool commandPool, VkCommandPoolTrimFlags flags) { return funcs.vkTrimCommandPool(logical, commandPool, flags); }
    inline void vkGetDeviceQueue2(const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) { return funcs.vkGetDeviceQueue2(logical, pQueueInfo, pQueue); }
    inline VkResult vkCreateSamplerYcbcrConversion(const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, VkSamplerYcbcrConversion* pYcbcrConversion)
    {
        return funcs.vkCreateSamplerYcbcrConversion(logical, pCreateInfo, allocation_callbacks, pYcbcrConversion);
    }
    inline void vkDestroySamplerYcbcrConversion(VkSamplerYcbcrConversion ycbcrConversion)
    {
        return funcs.vkDestroySamplerYcbcrConversion(logical, ycbcrConversion, allocation_callbacks);
    }
    inline VkResult vkCreateDescriptorUpdateTemplate(
        const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate)
    {
        return funcs.vkCreateDescriptorUpdateTemplate(logical, pCreateInfo, allocation_callbacks, pDescriptorUpdateTemplate);
    }
    inline void vkDestroyDescriptorUpdateTemplate(VkDescriptorUpdateTemplate descriptorUpdateTemplate)
    {
        return funcs.vkDestroyDescriptorUpdateTemplate(logical, descriptorUpdateTemplate, allocation_callbacks);
    }
    inline void vkUpdateDescriptorSetWithTemplate(VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData)
    {
        return funcs.vkUpdateDescriptorSetWithTemplate(logical, descriptorSet, descriptorUpdateTemplate, pData);
    }
    inline void vkGetDescriptorSetLayoutSupport(const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport)
    {
        return funcs.vkGetDescriptorSetLayoutSupport(logical, pCreateInfo, pSupport);
    }
#endif

#ifdef VK_VERSION_1_2
    inline void vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
        VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
    {
        return funcs.vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    inline void vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
        VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
    {
        return funcs.vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    inline VkResult vkCreateRenderPass2(const VkRenderPassCreateInfo2* pCreateInfo, VkRenderPass* pRenderPass)
    {
        return funcs.vkCreateRenderPass2(logical, pCreateInfo, allocation_callbacks, pRenderPass);
    }
    inline void vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo)
    {
        return funcs.vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    }
    inline void vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo)
    {
        return funcs.vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    }
    inline void vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo)
    {
        return funcs.vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
    }
    inline void vkResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
    {
        return funcs.vkResetQueryPool(logical, queryPool, firstQuery, queryCount);
    }
    inline VkResult vkGetSemaphoreCounterValue(VkSemaphore semaphore, uint64_t* pValue) { return funcs.vkGetSemaphoreCounterValue(logical, semaphore, pValue); }
    inline VkResult vkWaitSemaphores(const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) { return funcs.vkWaitSemaphores(logical, pWaitInfo, timeout); }
    inline VkResult vkSignalSemaphore(const VkSemaphoreSignalInfo* pSignalInfo) { return funcs.vkSignalSemaphore(logical, pSignalInfo); }
    inline VkDeviceAddress vkGetBufferDeviceAddress(const VkBufferDeviceAddressInfo* pInfo) { return funcs.vkGetBufferDeviceAddress(logical, pInfo); }
    inline uint64_t vkGetBufferOpaqueCaptureAddress(const VkBufferDeviceAddressInfo* pInfo) { return funcs.vkGetBufferOpaqueCaptureAddress(logical, pInfo); }
    inline uint64_t vkGetDeviceMemoryOpaqueCaptureAddress(const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo)
    {
        return funcs.vkGetDeviceMemoryOpaqueCaptureAddress(logical, pInfo);
    }
#endif

#ifdef VK_KHR_swapchain
    inline VkResult vkCreateSwapchainKHR(const VkSwapchainCreateInfoKHR* pCreateInfo, VkSwapchainKHR* pSwapchain)
    {
        return funcs.vkCreateSwapchainKHR(logical, pCreateInfo, allocation_callbacks, pSwapchain);
    }
    inline void vkDestroySwapchainKHR(VkSwapchainKHR swapchain) { return funcs.vkDestroySwapchainKHR(logical, swapchain, allocation_callbacks); }
    inline VkResult vkGetSwapchainImagesKHR(VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages)
    {
        return funcs.vkGetSwapchainImagesKHR(logical, swapchain, pSwapchainImageCount, pSwapchainImages);
    }
    inline VkResult vkAcquireNextImageKHR(VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
    {
        return funcs.vkAcquireNextImageKHR(logical, swapchain, timeout, semaphore, fence, pImageIndex);
    }
    inline VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) { return funcs.vkQueuePresentKHR(queue, pPresentInfo); }
    inline VkResult vkGetDeviceGroupPresentCapabilitiesKHR(VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities)
    {
        return funcs.vkGetDeviceGroupPresentCapabilitiesKHR(logical, pDeviceGroupPresentCapabilities);
    }
    inline VkResult vkGetDeviceGroupSurfacePresentModesKHR(VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes)
    {
        return funcs.vkGetDeviceGroupSurfacePresentModesKHR(logical, surface, pModes);
    }
    inline VkResult vkAcquireNextImage2KHR(const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex)
    {
        return funcs.vkAcquireNextImage2KHR(logical, pAcquireInfo, pImageIndex);
    }
#endif

#ifdef VK_KHR_dynamic_rendering
    inline void vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo)
    {
        funcs.vkCmdBeginRenderingKHR(commandBuffer, pRenderingInfo);
    }
    inline void vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) { funcs.vkCmdEndRenderingKHR(commandBuffer); }
#endif

#ifdef VK_KHR_synchronization2
    inline void vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo)
    {
        return funcs.vkCmdSetEvent2KHR(commandBuffer, event, pDependencyInfo);
    }
    inline void vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask)
    {
        return funcs.vkCmdResetEvent2KHR(commandBuffer, event, stageMask);
    }
    inline void vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos)
    {
        return funcs.vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos);
    }
    inline void vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo)
    {
        return funcs.vkCmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo);
    }
    inline void vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
    {
        return funcs.vkCmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
    }
    inline VkResult vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence)
    {
        return funcs.vkQueueSubmit2KHR(queue, submitCount, pSubmits, fence);
    }
#endif
};

extern device_t* device;
}
