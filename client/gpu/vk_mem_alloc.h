/* clang-format off */
/* [mcs_b181]: Split into source and header */
/* [mcs_b181]: Add VMA compile flags at the top of the file */
//
// Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef AMD_VULKAN_MEMORY_ALLOCATOR_H
#define AMD_VULKAN_MEMORY_ALLOCATOR_H

/* [mcs_b181]: Add VMA compile flags at the top of the file */
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

/** \mainpage Vulkan Memory Allocator

<b>Version 3.3.0</b>

Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved. \n
License: MIT \n
See also: [product page on GPUOpen](https://gpuopen.com/gaming-product/vulkan-memory-allocator/),
[repository on GitHub](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)


<b>API documentation divided into groups:</b> [Topics](topics.html)

<b>General documentation chapters:</b>

- \subpage faq
- \subpage quick_start
  - [Project setup](@ref quick_start_project_setup)
  - [Initialization](@ref quick_start_initialization)
  - [Resource allocation](@ref quick_start_resource_allocation)
- \subpage choosing_memory_type
  - [Usage](@ref choosing_memory_type_usage)
  - [Required and preferred flags](@ref choosing_memory_type_required_preferred_flags)
  - [Explicit memory types](@ref choosing_memory_type_explicit_memory_types)
  - [Custom memory pools](@ref choosing_memory_type_custom_memory_pools)
  - [Dedicated allocations](@ref choosing_memory_type_dedicated_allocations)
- \subpage memory_mapping
  - [Copy functions](@ref memory_mapping_copy_functions)
  - [Mapping functions](@ref memory_mapping_mapping_functions)
  - [Persistently mapped memory](@ref memory_mapping_persistently_mapped_memory)
  - [Cache flush and invalidate](@ref memory_mapping_cache_control)
- \subpage staying_within_budget
  - [Querying for budget](@ref staying_within_budget_querying_for_budget)
  - [Controlling memory usage](@ref staying_within_budget_controlling_memory_usage)
- \subpage resource_aliasing
- \subpage custom_memory_pools
  - [Choosing memory type index](@ref custom_memory_pools_MemTypeIndex)
  - [When not to use custom pools](@ref custom_memory_pools_when_not_use)
  - [Linear allocation algorithm](@ref linear_algorithm)
    - [Free-at-once](@ref linear_algorithm_free_at_once)
    - [Stack](@ref linear_algorithm_stack)
    - [Double stack](@ref linear_algorithm_double_stack)
    - [Ring buffer](@ref linear_algorithm_ring_buffer)
- \subpage defragmentation
- \subpage statistics
  - [Numeric statistics](@ref statistics_numeric_statistics)
  - [JSON dump](@ref statistics_json_dump)
- \subpage allocation_annotation
  - [Allocation user data](@ref allocation_user_data)
  - [Allocation names](@ref allocation_names)
- \subpage virtual_allocator
- \subpage debugging_memory_usage
  - [Memory initialization](@ref debugging_memory_usage_initialization)
  - [Margins](@ref debugging_memory_usage_margins)
  - [Corruption detection](@ref debugging_memory_usage_corruption_detection)
  - [Leak detection features](@ref debugging_memory_usage_leak_detection)
- \subpage other_api_interop
- \subpage usage_patterns
    - [GPU-only resource](@ref usage_patterns_gpu_only)
    - [Staging copy for upload](@ref usage_patterns_staging_copy_upload)
    - [Readback](@ref usage_patterns_readback)
    - [Advanced data uploading](@ref usage_patterns_advanced_data_uploading)
    - [Other use cases](@ref usage_patterns_other_use_cases)
- \subpage configuration
  - [Pointers to Vulkan functions](@ref config_Vulkan_functions)
  - [Custom host memory allocator](@ref custom_memory_allocator)
  - [Device memory allocation callbacks](@ref allocation_callbacks)
  - [Device heap memory limit](@ref heap_memory_limit)
- <b>Extension support</b>
    - \subpage vk_khr_dedicated_allocation
    - \subpage enabling_buffer_device_address
    - \subpage vk_ext_memory_priority
    - \subpage vk_amd_device_coherent_memory
    - \subpage vk_khr_external_memory_win32
- \subpage general_considerations
  - [Thread safety](@ref general_considerations_thread_safety)
  - [Versioning and compatibility](@ref general_considerations_versioning_and_compatibility)
  - [Validation layer warnings](@ref general_considerations_validation_layer_warnings)
  - [Allocation algorithm](@ref general_considerations_allocation_algorithm)
  - [Features not supported](@ref general_considerations_features_not_supported)

\defgroup group_init Library initialization

\brief API elements related to the initialization and management of the entire library, especially #VmaAllocator object.

\defgroup group_alloc Memory allocation

\brief API elements related to the allocation, deallocation, and management of Vulkan memory, buffers, images.
Most basic ones being: vmaCreateBuffer(), vmaCreateImage().

\defgroup group_virtual Virtual allocator

\brief API elements related to the mechanism of \ref virtual_allocator - using the core allocation algorithm
for user-defined purpose without allocating any real GPU memory.

\defgroup group_stats Statistics

\brief API elements that query current status of the allocator, from memory usage, budget, to full dump of the internal state in JSON format.
See documentation chapter: \ref statistics.
*/


#ifdef __cplusplus
extern "C" {
#endif

#if !defined(VULKAN_H_)
#include <vulkan/vulkan.h>
#endif

#if !defined(VMA_VULKAN_VERSION)
    #if defined(VK_VERSION_1_4)
        #define VMA_VULKAN_VERSION 1004000
    #elif defined(VK_VERSION_1_3)
        #define VMA_VULKAN_VERSION 1003000
    #elif defined(VK_VERSION_1_2)
        #define VMA_VULKAN_VERSION 1002000
    #elif defined(VK_VERSION_1_1)
        #define VMA_VULKAN_VERSION 1001000
    #else
        #define VMA_VULKAN_VERSION 1000000
    #endif
#endif

#if defined(__ANDROID__) && defined(VK_NO_PROTOTYPES) && VMA_STATIC_VULKAN_FUNCTIONS
    extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    extern PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    extern PFN_vkAllocateMemory vkAllocateMemory;
    extern PFN_vkFreeMemory vkFreeMemory;
    extern PFN_vkMapMemory vkMapMemory;
    extern PFN_vkUnmapMemory vkUnmapMemory;
    extern PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    extern PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
    extern PFN_vkBindBufferMemory vkBindBufferMemory;
    extern PFN_vkBindImageMemory vkBindImageMemory;
    extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    extern PFN_vkCreateBuffer vkCreateBuffer;
    extern PFN_vkDestroyBuffer vkDestroyBuffer;
    extern PFN_vkCreateImage vkCreateImage;
    extern PFN_vkDestroyImage vkDestroyImage;
    extern PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    #if VMA_VULKAN_VERSION >= 1001000
        extern PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
        extern PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
        extern PFN_vkBindBufferMemory2 vkBindBufferMemory2;
        extern PFN_vkBindImageMemory2 vkBindImageMemory2;
        extern PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
    #endif // #if VMA_VULKAN_VERSION >= 1001000
#endif // #if defined(__ANDROID__) && VMA_STATIC_VULKAN_FUNCTIONS && VK_NO_PROTOTYPES

#if !defined(VMA_DEDICATED_ALLOCATION)
    #if VK_KHR_get_memory_requirements2 && VK_KHR_dedicated_allocation
        #define VMA_DEDICATED_ALLOCATION 1
    #else
        #define VMA_DEDICATED_ALLOCATION 0
    #endif
#endif

#if !defined(VMA_BIND_MEMORY2)
    #if VK_KHR_bind_memory2
        #define VMA_BIND_MEMORY2 1
    #else
        #define VMA_BIND_MEMORY2 0
    #endif
#endif

#if !defined(VMA_MEMORY_BUDGET)
    #if VK_EXT_memory_budget && (VK_KHR_get_physical_device_properties2 || VMA_VULKAN_VERSION >= 1001000)
        #define VMA_MEMORY_BUDGET 1
    #else
        #define VMA_MEMORY_BUDGET 0
    #endif
#endif

// Defined to 1 when VK_KHR_buffer_device_address device extension or equivalent core Vulkan 1.2 feature is defined in its headers.
#if !defined(VMA_BUFFER_DEVICE_ADDRESS)
    #if VK_KHR_buffer_device_address || VMA_VULKAN_VERSION >= 1002000
        #define VMA_BUFFER_DEVICE_ADDRESS 1
    #else
        #define VMA_BUFFER_DEVICE_ADDRESS 0
    #endif
#endif

// Defined to 1 when VK_EXT_memory_priority device extension is defined in Vulkan headers.
#if !defined(VMA_MEMORY_PRIORITY)
    #if VK_EXT_memory_priority
        #define VMA_MEMORY_PRIORITY 1
    #else
        #define VMA_MEMORY_PRIORITY 0
    #endif
#endif

// Defined to 1 when VK_KHR_maintenance4 device extension is defined in Vulkan headers.
#if !defined(VMA_KHR_MAINTENANCE4)
    #if VK_KHR_maintenance4
        #define VMA_KHR_MAINTENANCE4 1
    #else
        #define VMA_KHR_MAINTENANCE4 0
    #endif
#endif

// Defined to 1 when VK_KHR_maintenance5 device extension is defined in Vulkan headers.
#if !defined(VMA_KHR_MAINTENANCE5)
    #if VK_KHR_maintenance5
        #define VMA_KHR_MAINTENANCE5 1
    #else
        #define VMA_KHR_MAINTENANCE5 0
    #endif
#endif


// Defined to 1 when VK_KHR_external_memory device extension is defined in Vulkan headers.
#if !defined(VMA_EXTERNAL_MEMORY)
    #if VK_KHR_external_memory
        #define VMA_EXTERNAL_MEMORY 1
    #else
        #define VMA_EXTERNAL_MEMORY 0
    #endif
#endif

// Defined to 1 when VK_KHR_external_memory_win32 device extension is defined in Vulkan headers.
#if !defined(VMA_EXTERNAL_MEMORY_WIN32)
    #if VK_KHR_external_memory_win32
        #define VMA_EXTERNAL_MEMORY_WIN32 1
    #else
        #define VMA_EXTERNAL_MEMORY_WIN32 0
    #endif
#endif

// Define these macros to decorate all public functions with additional code,
// before and after returned type, appropriately. This may be useful for
// exporting the functions when compiling VMA as a separate library. Example:
// #define VMA_CALL_PRE  __declspec(dllexport)
// #define VMA_CALL_POST __cdecl
#ifndef VMA_CALL_PRE
    #define VMA_CALL_PRE
#endif
#ifndef VMA_CALL_POST
    #define VMA_CALL_POST
#endif

// Define this macro to decorate pNext pointers with an attribute specifying the Vulkan
// structure that will be extended via the pNext chain.
#ifndef VMA_EXTENDS_VK_STRUCT
    #define VMA_EXTENDS_VK_STRUCT(vkStruct)
#endif

// Define this macro to decorate pointers with an attribute specifying the
// length of the array they point to if they are not null.
//
// The length may be one of
// - The name of another parameter in the argument list where the pointer is declared
// - The name of another member in the struct where the pointer is declared
// - The name of a member of a struct type, meaning the value of that member in
//   the context of the call. For example
//   VMA_LEN_IF_NOT_NULL("VkPhysicalDeviceMemoryProperties::memoryHeapCount"),
//   this means the number of memory heaps available in the device associated
//   with the VmaAllocator being dealt with.
#ifndef VMA_LEN_IF_NOT_NULL
    #define VMA_LEN_IF_NOT_NULL(len)
#endif

// The VMA_NULLABLE macro is defined to be _Nullable when compiling with Clang.
// see: https://clang.llvm.org/docs/AttributeReference.html#nullable
#ifndef VMA_NULLABLE
    #ifdef __clang__
        #define VMA_NULLABLE _Nullable
    #else
        #define VMA_NULLABLE
    #endif
#endif

// The VMA_NOT_NULL macro is defined to be _Nonnull when compiling with Clang.
// see: https://clang.llvm.org/docs/AttributeReference.html#nonnull
#ifndef VMA_NOT_NULL
    #ifdef __clang__
        #define VMA_NOT_NULL _Nonnull
    #else
        #define VMA_NOT_NULL
    #endif
#endif

// If non-dispatchable handles are represented as pointers then we can give
// then nullability annotations
#ifndef VMA_NOT_NULL_NON_DISPATCHABLE
    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
        #define VMA_NOT_NULL_NON_DISPATCHABLE VMA_NOT_NULL
    #else
        #define VMA_NOT_NULL_NON_DISPATCHABLE
    #endif
#endif

#ifndef VMA_NULLABLE_NON_DISPATCHABLE
    #if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
        #define VMA_NULLABLE_NON_DISPATCHABLE VMA_NULLABLE
    #else
        #define VMA_NULLABLE_NON_DISPATCHABLE
    #endif
#endif

#ifndef VMA_STATS_STRING_ENABLED
    #define VMA_STATS_STRING_ENABLED 1
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//    INTERFACE
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Sections for managing code placement in file, only for development purposes e.g. for convenient folding inside an IDE.
#ifndef _VMA_ENUM_DECLARATIONS

/**
\addtogroup group_init
@{
*/

/// Flags for created #VmaAllocator.
typedef enum VmaAllocatorCreateFlagBits
{
    /** \brief Allocator and all objects created from it will not be synchronized internally, so you must guarantee they are used from only one thread at a time or synchronized externally by you.

    Using this flag may increase performance because internal mutexes are not used.
    */
    VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT = 0x00000001,
    /** \brief Enables usage of VK_KHR_dedicated_allocation extension.

    The flag works only if VmaAllocatorCreateInfo::vulkanApiVersion `== VK_API_VERSION_1_0`.
    When it is `VK_API_VERSION_1_1`, the flag is ignored because the extension has been promoted to Vulkan 1.1.

    Using this extension will automatically allocate dedicated blocks of memory for
    some buffers and images instead of suballocating place for them out of bigger
    memory blocks (as if you explicitly used #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    flag) when it is recommended by the driver. It may improve performance on some
    GPUs.

    You may set this flag only if you found out that following device extensions are
    supported, you enabled them while creating Vulkan device passed as
    VmaAllocatorCreateInfo::device, and you want them to be used internally by this
    library:

    - VK_KHR_get_memory_requirements2 (device extension)
    - VK_KHR_dedicated_allocation (device extension)

    When this flag is set, you can experience following warnings reported by Vulkan
    validation layer. You can ignore them.

    > vkBindBufferMemory(): Binding memory to buffer 0x2d but vkGetBufferMemoryRequirements() has not been called on that buffer.
    */
    VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT = 0x00000002,
    /**
    Enables usage of VK_KHR_bind_memory2 extension.

    The flag works only if VmaAllocatorCreateInfo::vulkanApiVersion `== VK_API_VERSION_1_0`.
    When it is `VK_API_VERSION_1_1`, the flag is ignored because the extension has been promoted to Vulkan 1.1.

    You may set this flag only if you found out that this device extension is supported,
    you enabled it while creating Vulkan device passed as VmaAllocatorCreateInfo::device,
    and you want it to be used internally by this library.

    The extension provides functions `vkBindBufferMemory2KHR` and `vkBindImageMemory2KHR`,
    which allow to pass a chain of `pNext` structures while binding.
    This flag is required if you use `pNext` parameter in vmaBindBufferMemory2() or vmaBindImageMemory2().
    */
    VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT = 0x00000004,
    /**
    Enables usage of VK_EXT_memory_budget extension.

    You may set this flag only if you found out that this device extension is supported,
    you enabled it while creating Vulkan device passed as VmaAllocatorCreateInfo::device,
    and you want it to be used internally by this library, along with another instance extension
    VK_KHR_get_physical_device_properties2, which is required by it (or Vulkan 1.1, where this extension is promoted).

    The extension provides query for current memory usage and budget, which will probably
    be more accurate than an estimation used by the library otherwise.
    */
    VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT = 0x00000008,
    /**
    Enables usage of VK_AMD_device_coherent_memory extension.

    You may set this flag only if you:

    - found out that this device extension is supported and enabled it while creating Vulkan device passed as VmaAllocatorCreateInfo::device,
    - checked that `VkPhysicalDeviceCoherentMemoryFeaturesAMD::deviceCoherentMemory` is true and set it while creating the Vulkan device,
    - want it to be used internally by this library.

    The extension and accompanying device feature provide access to memory types with
    `VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD` and `VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD` flags.
    They are useful mostly for writing breadcrumb markers - a common method for debugging GPU crash/hang/TDR.

    When the extension is not enabled, such memory types are still enumerated, but their usage is illegal.
    To protect from this error, if you don't create the allocator with this flag, it will refuse to allocate any memory or create a custom pool in such memory type,
    returning `VK_ERROR_FEATURE_NOT_PRESENT`.
    */
    VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT = 0x00000010,
    /**
    Enables usage of "buffer device address" feature, which allows you to use function
    `vkGetBufferDeviceAddress*` to get raw GPU pointer to a buffer and pass it for usage inside a shader.

    You may set this flag only if you:

    1. (For Vulkan version < 1.2) Found as available and enabled device extension
    VK_KHR_buffer_device_address.
    This extension is promoted to core Vulkan 1.2.
    2. Found as available and enabled device feature `VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress`.

    When this flag is set, you can create buffers with `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` using VMA.
    The library automatically adds `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT` to
    allocated memory blocks wherever it might be needed.

    For more information, see documentation chapter \ref enabling_buffer_device_address.
    */
    VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT = 0x00000020,
    /**
    Enables usage of VK_EXT_memory_priority extension in the library.

    You may set this flag only if you found available and enabled this device extension,
    along with `VkPhysicalDeviceMemoryPriorityFeaturesEXT::memoryPriority == VK_TRUE`,
    while creating Vulkan device passed as VmaAllocatorCreateInfo::device.

    When this flag is used, VmaAllocationCreateInfo::priority and VmaPoolCreateInfo::priority
    are used to set priorities of allocated Vulkan memory. Without it, these variables are ignored.

    A priority must be a floating-point value between 0 and 1, indicating the priority of the allocation relative to other memory allocations.
    Larger values are higher priority. The granularity of the priorities is implementation-dependent.
    It is automatically passed to every call to `vkAllocateMemory` done by the library using structure `VkMemoryPriorityAllocateInfoEXT`.
    The value to be used for default priority is 0.5.
    For more details, see the documentation of the VK_EXT_memory_priority extension.
    */
    VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT = 0x00000040,
    /**
    Enables usage of VK_KHR_maintenance4 extension in the library.

    You may set this flag only if you found available and enabled this device extension,
    while creating Vulkan device passed as VmaAllocatorCreateInfo::device.
    */
    VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT = 0x00000080,
    /**
    Enables usage of VK_KHR_maintenance5 extension in the library.

    You should set this flag if you found available and enabled this device extension,
    while creating Vulkan device passed as VmaAllocatorCreateInfo::device.
    */
    VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT = 0x00000100,

    /**
    Enables usage of VK_KHR_external_memory_win32 extension in the library.

    You should set this flag if you found available and enabled this device extension,
    while creating Vulkan device passed as VmaAllocatorCreateInfo::device.
    For more information, see \ref vk_khr_external_memory_win32.
    */
    VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT = 0x00000200,

    VMA_ALLOCATOR_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaAllocatorCreateFlagBits;
/// See #VmaAllocatorCreateFlagBits.
typedef VkFlags VmaAllocatorCreateFlags;

/** @} */

/**
\addtogroup group_alloc
@{
*/

/// \brief Intended usage of the allocated memory.
typedef enum VmaMemoryUsage
{
    /** No intended memory usage specified.
    Use other members of VmaAllocationCreateInfo to specify your requirements.
    */
    VMA_MEMORY_USAGE_UNKNOWN = 0,
    /**
    \deprecated Obsolete, preserved for backward compatibility.
    Prefers `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
    */
    VMA_MEMORY_USAGE_GPU_ONLY = 1,
    /**
    \deprecated Obsolete, preserved for backward compatibility.
    Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` and `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`.
    */
    VMA_MEMORY_USAGE_CPU_ONLY = 2,
    /**
    \deprecated Obsolete, preserved for backward compatibility.
    Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, prefers `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
    */
    VMA_MEMORY_USAGE_CPU_TO_GPU = 3,
    /**
    \deprecated Obsolete, preserved for backward compatibility.
    Guarantees `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, prefers `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`.
    */
    VMA_MEMORY_USAGE_GPU_TO_CPU = 4,
    /**
    \deprecated Obsolete, preserved for backward compatibility.
    Prefers not `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
    */
    VMA_MEMORY_USAGE_CPU_COPY = 5,
    /**
    Lazily allocated GPU memory having `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`.
    Exists mostly on mobile platforms. Using it on desktop PC or other GPUs with no such memory type present will fail the allocation.

    Usage: Memory for transient attachment images (color attachments, depth attachments etc.), created with `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`.

    Allocations with this usage are always created as dedicated - it implies #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
    */
    VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED = 6,
    /**
    Selects best memory type automatically.
    This flag is recommended for most common use cases.

    When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
    you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    in VmaAllocationCreateInfo::flags.

    It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
    vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
    and not with generic memory allocation functions.
    */
    VMA_MEMORY_USAGE_AUTO = 7,
    /**
    Selects best memory type automatically with preference for GPU (device) memory.

    When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
    you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    in VmaAllocationCreateInfo::flags.

    It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
    vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
    and not with generic memory allocation functions.
    */
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE = 8,
    /**
    Selects best memory type automatically with preference for CPU (host) memory.

    When using this flag, if you want to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT),
    you must pass one of the flags: #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    in VmaAllocationCreateInfo::flags.

    It can be used only with functions that let the library know `VkBufferCreateInfo` or `VkImageCreateInfo`, e.g.
    vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo()
    and not with generic memory allocation functions.
    */
    VMA_MEMORY_USAGE_AUTO_PREFER_HOST = 9,

    VMA_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
} VmaMemoryUsage;

/// Flags to be passed as VmaAllocationCreateInfo::flags.
typedef enum VmaAllocationCreateFlagBits
{
    /** \brief Set this flag if the allocation should have its own memory block.

    Use it for special, big resources, like fullscreen images used as attachments.

    If you use this flag while creating a buffer or an image, `VkMemoryDedicatedAllocateInfo`
    structure is applied if possible.
    */
    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT = 0x00000001,

    /** \brief Set this flag to only try to allocate from existing `VkDeviceMemory` blocks and never create new such block.

    If new allocation cannot be placed in any of the existing blocks, allocation
    fails with `VK_ERROR_OUT_OF_DEVICE_MEMORY` error.

    You should not use #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT and
    #VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT at the same time. It makes no sense.
    */
    VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT = 0x00000002,
    /** \brief Set this flag to use a memory that will be persistently mapped and retrieve pointer to it.

    Pointer to mapped memory will be returned through VmaAllocationInfo::pMappedData.

    It is valid to use this flag for allocation made from memory type that is not
    `HOST_VISIBLE`. This flag is then ignored and memory is not mapped. This is
    useful if you need an allocation that is efficient to use on GPU
    (`DEVICE_LOCAL`) and still want to map it directly if possible on platforms that
    support it (e.g. Intel GPU).
    */
    VMA_ALLOCATION_CREATE_MAPPED_BIT = 0x00000004,
    /** \deprecated Preserved for backward compatibility. Consider using vmaSetAllocationName() instead.

    Set this flag to treat VmaAllocationCreateInfo::pUserData as pointer to a
    null-terminated string. Instead of copying pointer value, a local copy of the
    string is made and stored in allocation's `pName`. The string is automatically
    freed together with the allocation. It is also used in vmaBuildStatsString().
    */
    VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT = 0x00000020,
    /** Allocation will be created from upper stack in a double stack pool.

    This flag is only allowed for custom pools created with #VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT flag.
    */
    VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT = 0x00000040,
    /** Create both buffer/image and allocation, but don't bind them together.
    It is useful when you want to bind yourself to do some more advanced binding, e.g. using some extensions.
    The flag is meaningful only with functions that bind by default: vmaCreateBuffer(), vmaCreateImage().
    Otherwise it is ignored.

    If you want to make sure the new buffer/image is not tied to the new memory allocation
    through `VkMemoryDedicatedAllocateInfoKHR` structure in case the allocation ends up in its own memory block,
    use also flag #VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT.
    */
    VMA_ALLOCATION_CREATE_DONT_BIND_BIT = 0x00000080,
    /** Create allocation only if additional device memory required for it, if any, won't exceed
    memory budget. Otherwise return `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
    */
    VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT = 0x00000100,
    /** \brief Set this flag if the allocated memory will have aliasing resources.

    Usage of this flag prevents supplying `VkMemoryDedicatedAllocateInfoKHR` when #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT is specified.
    Otherwise created dedicated memory will not be suitable for aliasing resources, resulting in Vulkan Validation Layer errors.
    */
    VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT = 0x00000200,
    /**
    Requests possibility to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT).

    - If you use #VMA_MEMORY_USAGE_AUTO or other `VMA_MEMORY_USAGE_AUTO*` value,
      you must use this flag to be able to map the allocation. Otherwise, mapping is incorrect.
    - If you use other value of #VmaMemoryUsage, this flag is ignored and mapping is always possible in memory types that are `HOST_VISIBLE`.
      This includes allocations created in \ref custom_memory_pools.

    Declares that mapped memory will only be written sequentially, e.g. using `memcpy()` or a loop writing number-by-number,
    never read or accessed randomly, so a memory type can be selected that is uncached and write-combined.

    \warning Violating this declaration may work correctly, but will likely be very slow.
    Watch out for implicit reads introduced by doing e.g. `pMappedData[i] += x;`
    Better prepare your data in a local variable and `memcpy()` it to the mapped pointer all at once.
    */
    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT = 0x00000400,
    /**
    Requests possibility to map the allocation (using vmaMapMemory() or #VMA_ALLOCATION_CREATE_MAPPED_BIT).

    - If you use #VMA_MEMORY_USAGE_AUTO or other `VMA_MEMORY_USAGE_AUTO*` value,
      you must use this flag to be able to map the allocation. Otherwise, mapping is incorrect.
    - If you use other value of #VmaMemoryUsage, this flag is ignored and mapping is always possible in memory types that are `HOST_VISIBLE`.
      This includes allocations created in \ref custom_memory_pools.

    Declares that mapped memory can be read, written, and accessed in random order,
    so a `HOST_CACHED` memory type is preferred.
    */
    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT = 0x00000800,
    /**
    Together with #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
    it says that despite request for host access, a not-`HOST_VISIBLE` memory type can be selected
    if it may improve performance.

    By using this flag, you declare that you will check if the allocation ended up in a `HOST_VISIBLE` memory type
    (e.g. using vmaGetAllocationMemoryProperties()) and if not, you will create some "staging" buffer and
    issue an explicit transfer to write/read your data.
    To prepare for this possibility, don't forget to add appropriate flags like
    `VK_BUFFER_USAGE_TRANSFER_DST_BIT`, `VK_BUFFER_USAGE_TRANSFER_SRC_BIT` to the parameters of created buffer or image.
    */
    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT = 0x00001000,
    /** Allocation strategy that chooses smallest possible free range for the allocation
    to minimize memory usage and fragmentation, possibly at the expense of allocation time.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT = 0x00010000,
    /** Allocation strategy that chooses first suitable free range for the allocation -
    not necessarily in terms of the smallest offset but the one that is easiest and fastest to find
    to minimize allocation time, possibly at the expense of allocation quality.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT = 0x00020000,
    /** Allocation strategy that chooses always the lowest offset in available space.
    This is not the most efficient strategy but achieves highly packed data.
    Used internally by defragmentation, not recommended in typical usage.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT  = 0x00040000,
    /** Alias to #VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
    /** Alias to #VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_FIRST_FIT_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT,
    /** A bit mask to extract only `STRATEGY` bits from entire set of flags.
    */
    VMA_ALLOCATION_CREATE_STRATEGY_MASK =
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,

    VMA_ALLOCATION_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaAllocationCreateFlagBits;
/// See #VmaAllocationCreateFlagBits.
typedef VkFlags VmaAllocationCreateFlags;

/// Flags to be passed as VmaPoolCreateInfo::flags.
typedef enum VmaPoolCreateFlagBits
{
    /** \brief Use this flag if you always allocate only buffers and linear images or only optimal images out of this pool and so Buffer-Image Granularity can be ignored.

    This is an optional optimization flag.

    If you always allocate using vmaCreateBuffer(), vmaCreateImage(),
    vmaAllocateMemoryForBuffer(), then you don't need to use it because allocator
    knows exact type of your allocations so it can handle Buffer-Image Granularity
    in the optimal way.

    If you also allocate using vmaAllocateMemoryForImage() or vmaAllocateMemory(),
    exact type of such allocations is not known, so allocator must be conservative
    in handling Buffer-Image Granularity, which can lead to suboptimal allocation
    (wasted memory). In that case, if you can make sure you always allocate only
    buffers and linear images or only optimal images out of this pool, use this flag
    to make allocator disregard Buffer-Image Granularity and so make allocations
    faster and more optimal.
    */
    VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT = 0x00000002,

    /** \brief Enables alternative, linear allocation algorithm in this pool.

    Specify this flag to enable linear allocation algorithm, which always creates
    new allocations after last one and doesn't reuse space from allocations freed in
    between. It trades memory consumption for simplified algorithm and data
    structure, which has better performance and uses less memory for metadata.

    By using this flag, you can achieve behavior of free-at-once, stack,
    ring buffer, and double stack.
    For details, see documentation chapter \ref linear_algorithm.
    */
    VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT = 0x00000004,

    /** Bit mask to extract only `ALGORITHM` bits from entire set of flags.
    */
    VMA_POOL_CREATE_ALGORITHM_MASK =
        VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT,

    VMA_POOL_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaPoolCreateFlagBits;
/// Flags to be passed as VmaPoolCreateInfo::flags. See #VmaPoolCreateFlagBits.
typedef VkFlags VmaPoolCreateFlags;

/// Flags to be passed as VmaDefragmentationInfo::flags.
typedef enum VmaDefragmentationFlagBits
{
    /* \brief Use simple but fast algorithm for defragmentation.
    May not achieve best results but will require least time to compute and least allocations to copy.
    */
    VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FAST_BIT = 0x1,
    /* \brief Default defragmentation algorithm, applied also when no `ALGORITHM` flag is specified.
    Offers a balance between defragmentation quality and the amount of allocations and bytes that need to be moved.
    */
    VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT = 0x2,
    /* \brief Perform full defragmentation of memory.
    Can result in notably more time to compute and allocations to copy, but will achieve best memory packing.
    */
    VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FULL_BIT = 0x4,
    /** \brief Use the most roboust algorithm at the cost of time to compute and number of copies to make.
    Only available when bufferImageGranularity is greater than 1, since it aims to reduce
    alignment issues between different types of resources.
    Otherwise falls back to same behavior as #VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FULL_BIT.
    */
    VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT = 0x8,

    /// A bit mask to extract only `ALGORITHM` bits from entire set of flags.
    VMA_DEFRAGMENTATION_FLAG_ALGORITHM_MASK =
        VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FAST_BIT |
        VMA_DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED_BIT |
        VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FULL_BIT |
        VMA_DEFRAGMENTATION_FLAG_ALGORITHM_EXTENSIVE_BIT,

    VMA_DEFRAGMENTATION_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaDefragmentationFlagBits;
/// See #VmaDefragmentationFlagBits.
typedef VkFlags VmaDefragmentationFlags;

/// Operation performed on single defragmentation move. See structure #VmaDefragmentationMove.
typedef enum VmaDefragmentationMoveOperation
{
    /// Buffer/image has been recreated at `dstTmpAllocation`, data has been copied, old buffer/image has been destroyed. `srcAllocation` should be changed to point to the new place. This is the default value set by vmaBeginDefragmentationPass().
    VMA_DEFRAGMENTATION_MOVE_OPERATION_COPY = 0,
    /// Set this value if you cannot move the allocation. New place reserved at `dstTmpAllocation` will be freed. `srcAllocation` will remain unchanged.
    VMA_DEFRAGMENTATION_MOVE_OPERATION_IGNORE = 1,
    /// Set this value if you decide to abandon the allocation and you destroyed the buffer/image. New place reserved at `dstTmpAllocation` will be freed, along with `srcAllocation`, which will be destroyed.
    VMA_DEFRAGMENTATION_MOVE_OPERATION_DESTROY = 2,
} VmaDefragmentationMoveOperation;

/** @} */

/**
\addtogroup group_virtual
@{
*/

/// Flags to be passed as VmaVirtualBlockCreateInfo::flags.
typedef enum VmaVirtualBlockCreateFlagBits
{
    /** \brief Enables alternative, linear allocation algorithm in this virtual block.

    Specify this flag to enable linear allocation algorithm, which always creates
    new allocations after last one and doesn't reuse space from allocations freed in
    between. It trades memory consumption for simplified algorithm and data
    structure, which has better performance and uses less memory for metadata.

    By using this flag, you can achieve behavior of free-at-once, stack,
    ring buffer, and double stack.
    For details, see documentation chapter \ref linear_algorithm.
    */
    VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT = 0x00000001,

    /** \brief Bit mask to extract only `ALGORITHM` bits from entire set of flags.
    */
    VMA_VIRTUAL_BLOCK_CREATE_ALGORITHM_MASK =
        VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT,

    VMA_VIRTUAL_BLOCK_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaVirtualBlockCreateFlagBits;
/// Flags to be passed as VmaVirtualBlockCreateInfo::flags. See #VmaVirtualBlockCreateFlagBits.
typedef VkFlags VmaVirtualBlockCreateFlags;

/// Flags to be passed as VmaVirtualAllocationCreateInfo::flags.
typedef enum VmaVirtualAllocationCreateFlagBits
{
    /** \brief Allocation will be created from upper stack in a double stack pool.

    This flag is only allowed for virtual blocks created with #VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT flag.
    */
    VMA_VIRTUAL_ALLOCATION_CREATE_UPPER_ADDRESS_BIT = VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT,
    /** \brief Allocation strategy that tries to minimize memory usage.
    */
    VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
    /** \brief Allocation strategy that tries to minimize allocation time.
    */
    VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT,
    /** Allocation strategy that chooses always the lowest offset in available space.
    This is not the most efficient strategy but achieves highly packed data.
    */
    VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,
    /** \brief A bit mask to extract only `STRATEGY` bits from entire set of flags.

    These strategy flags are binary compatible with equivalent flags in #VmaAllocationCreateFlagBits.
    */
    VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MASK = VMA_ALLOCATION_CREATE_STRATEGY_MASK,

    VMA_VIRTUAL_ALLOCATION_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VmaVirtualAllocationCreateFlagBits;
/// Flags to be passed as VmaVirtualAllocationCreateInfo::flags. See #VmaVirtualAllocationCreateFlagBits.
typedef VkFlags VmaVirtualAllocationCreateFlags;

/** @} */

#endif // _VMA_ENUM_DECLARATIONS

#ifndef _VMA_DATA_TYPES_DECLARATIONS

/**
\addtogroup group_init
@{ */

/** \struct VmaAllocator
\brief Represents main object of this library initialized.

Fill structure #VmaAllocatorCreateInfo and call function vmaCreateAllocator() to create it.
Call function vmaDestroyAllocator() to destroy it.

It is recommended to create just one object of this type per `VkDevice` object,
right after Vulkan is initialized and keep it alive until before Vulkan device is destroyed.
*/
VK_DEFINE_HANDLE(VmaAllocator)

/** @} */

/**
\addtogroup group_alloc
@{
*/

/** \struct VmaPool
\brief Represents custom memory pool

Fill structure VmaPoolCreateInfo and call function vmaCreatePool() to create it.
Call function vmaDestroyPool() to destroy it.

For more information see [Custom memory pools](@ref choosing_memory_type_custom_memory_pools).
*/
VK_DEFINE_HANDLE(VmaPool)

/** \struct VmaAllocation
\brief Represents single memory allocation.

It may be either dedicated block of `VkDeviceMemory` or a specific region of a bigger block of this type
plus unique offset.

There are multiple ways to create such object.
You need to fill structure VmaAllocationCreateInfo.
For more information see [Choosing memory type](@ref choosing_memory_type).

Although the library provides convenience functions that create Vulkan buffer or image,
allocate memory for it and bind them together,
binding of the allocation to a buffer or an image is out of scope of the allocation itself.
Allocation object can exist without buffer/image bound,
binding can be done manually by the user, and destruction of it can be done
independently of destruction of the allocation.

The object also remembers its size and some other information.
To retrieve this information, use function vmaGetAllocationInfo() and inspect
returned structure VmaAllocationInfo.
*/
VK_DEFINE_HANDLE(VmaAllocation)

/** \struct VmaDefragmentationContext
\brief An opaque object that represents started defragmentation process.

Fill structure #VmaDefragmentationInfo and call function vmaBeginDefragmentation() to create it.
Call function vmaEndDefragmentation() to destroy it.
*/
VK_DEFINE_HANDLE(VmaDefragmentationContext)

/** @} */

/**
\addtogroup group_virtual
@{
*/

/** \struct VmaVirtualAllocation
\brief Represents single memory allocation done inside VmaVirtualBlock.

Use it as a unique identifier to virtual allocation within the single block.

Use value `VK_NULL_HANDLE` to represent a null/invalid allocation.
*/
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VmaVirtualAllocation)

/** @} */

/**
\addtogroup group_virtual
@{
*/

/** \struct VmaVirtualBlock
\brief Handle to a virtual block object that allows to use core allocation algorithm without allocating any real GPU memory.

Fill in #VmaVirtualBlockCreateInfo structure and use vmaCreateVirtualBlock() to create it. Use vmaDestroyVirtualBlock() to destroy it.
For more information, see documentation chapter \ref virtual_allocator.

This object is not thread-safe - should not be used from multiple threads simultaneously, must be synchronized externally.
*/
VK_DEFINE_HANDLE(VmaVirtualBlock)

/** @} */

/**
\addtogroup group_init
@{
*/

/// Callback function called after successful vkAllocateMemory.
typedef void (VKAPI_PTR* PFN_vmaAllocateDeviceMemoryFunction)(
    VmaAllocator VMA_NOT_NULL                    allocator,
    uint32_t                                     memoryType,
    VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
    VkDeviceSize                                 size,
    void* VMA_NULLABLE                           pUserData);

/// Callback function called before vkFreeMemory.
typedef void (VKAPI_PTR* PFN_vmaFreeDeviceMemoryFunction)(
    VmaAllocator VMA_NOT_NULL                    allocator,
    uint32_t                                     memoryType,
    VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
    VkDeviceSize                                 size,
    void* VMA_NULLABLE                           pUserData);

/** \brief Set of callbacks that the library will call for `vkAllocateMemory` and `vkFreeMemory`.

Provided for informative purpose, e.g. to gather statistics about number of
allocations or total amount of memory allocated in Vulkan.

Used in VmaAllocatorCreateInfo::pDeviceMemoryCallbacks.
*/
typedef struct VmaDeviceMemoryCallbacks
{
    /// Optional, can be null.
    PFN_vmaAllocateDeviceMemoryFunction VMA_NULLABLE pfnAllocate;
    /// Optional, can be null.
    PFN_vmaFreeDeviceMemoryFunction VMA_NULLABLE pfnFree;
    /// Optional, can be null.
    void* VMA_NULLABLE pUserData;
} VmaDeviceMemoryCallbacks;

/** \brief Pointers to some Vulkan functions - a subset used by the library.

Used in VmaAllocatorCreateInfo::pVulkanFunctions.
*/
typedef struct VmaVulkanFunctions
{
    /// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
    PFN_vkGetInstanceProcAddr VMA_NULLABLE vkGetInstanceProcAddr;
    /// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
    PFN_vkGetDeviceProcAddr VMA_NULLABLE vkGetDeviceProcAddr;
    PFN_vkGetPhysicalDeviceProperties VMA_NULLABLE vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties VMA_NULLABLE vkGetPhysicalDeviceMemoryProperties;
    PFN_vkAllocateMemory VMA_NULLABLE vkAllocateMemory;
    PFN_vkFreeMemory VMA_NULLABLE vkFreeMemory;
    PFN_vkMapMemory VMA_NULLABLE vkMapMemory;
    PFN_vkUnmapMemory VMA_NULLABLE vkUnmapMemory;
    PFN_vkFlushMappedMemoryRanges VMA_NULLABLE vkFlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges VMA_NULLABLE vkInvalidateMappedMemoryRanges;
    PFN_vkBindBufferMemory VMA_NULLABLE vkBindBufferMemory;
    PFN_vkBindImageMemory VMA_NULLABLE vkBindImageMemory;
    PFN_vkGetBufferMemoryRequirements VMA_NULLABLE vkGetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements VMA_NULLABLE vkGetImageMemoryRequirements;
    PFN_vkCreateBuffer VMA_NULLABLE vkCreateBuffer;
    PFN_vkDestroyBuffer VMA_NULLABLE vkDestroyBuffer;
    PFN_vkCreateImage VMA_NULLABLE vkCreateImage;
    PFN_vkDestroyImage VMA_NULLABLE vkDestroyImage;
    PFN_vkCmdCopyBuffer VMA_NULLABLE vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    /// Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
    PFN_vkGetBufferMemoryRequirements2KHR VMA_NULLABLE vkGetBufferMemoryRequirements2KHR;
    /// Fetch "vkGetImageMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
    PFN_vkGetImageMemoryRequirements2KHR VMA_NULLABLE vkGetImageMemoryRequirements2KHR;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
    /// Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
    PFN_vkBindBufferMemory2KHR VMA_NULLABLE vkBindBufferMemory2KHR;
    /// Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
    PFN_vkBindImageMemory2KHR VMA_NULLABLE vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
    /// Fetch from "vkGetPhysicalDeviceMemoryProperties2" on Vulkan >= 1.1, but you can also fetch it from "vkGetPhysicalDeviceMemoryProperties2KHR" if you enabled extension VK_KHR_get_physical_device_properties2.
    PFN_vkGetPhysicalDeviceMemoryProperties2KHR VMA_NULLABLE vkGetPhysicalDeviceMemoryProperties2KHR;
#endif
#if VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000
    /// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
    PFN_vkGetDeviceBufferMemoryRequirementsKHR VMA_NULLABLE vkGetDeviceBufferMemoryRequirements;
    /// Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
    PFN_vkGetDeviceImageMemoryRequirementsKHR VMA_NULLABLE vkGetDeviceImageMemoryRequirements;
#endif
#if VMA_EXTERNAL_MEMORY_WIN32
    PFN_vkGetMemoryWin32HandleKHR VMA_NULLABLE vkGetMemoryWin32HandleKHR;
#else
    void* VMA_NULLABLE vkGetMemoryWin32HandleKHR;
#endif
} VmaVulkanFunctions;

/// Description of a Allocator to be created.
typedef struct VmaAllocatorCreateInfo
{
    /// Flags for created allocator. Use #VmaAllocatorCreateFlagBits enum.
    VmaAllocatorCreateFlags flags;
    /// Vulkan physical device.
    /** It must be valid throughout whole lifetime of created allocator. */
    VkPhysicalDevice VMA_NOT_NULL physicalDevice;
    /// Vulkan device.
    /** It must be valid throughout whole lifetime of created allocator. */
    VkDevice VMA_NOT_NULL device;
    /// Preferred size of a single `VkDeviceMemory` block to be allocated from large heaps > 1 GiB. Optional.
    /** Set to 0 to use default, which is currently 256 MiB. */
    VkDeviceSize preferredLargeHeapBlockSize;
    /// Custom CPU memory allocation callbacks. Optional.
    /** Optional, can be null. When specified, will also be used for all CPU-side memory allocations. */
    const VkAllocationCallbacks* VMA_NULLABLE pAllocationCallbacks;
    /// Informative callbacks for `vkAllocateMemory`, `vkFreeMemory`. Optional.
    /** Optional, can be null. */
    const VmaDeviceMemoryCallbacks* VMA_NULLABLE pDeviceMemoryCallbacks;
    /** \brief Either null or a pointer to an array of limits on maximum number of bytes that can be allocated out of particular Vulkan memory heap.

    If not NULL, it must be a pointer to an array of
    `VkPhysicalDeviceMemoryProperties::memoryHeapCount` elements, defining limit on
    maximum number of bytes that can be allocated out of particular Vulkan memory
    heap.

    Any of the elements may be equal to `VK_WHOLE_SIZE`, which means no limit on that
    heap. This is also the default in case of `pHeapSizeLimit` = NULL.

    If there is a limit defined for a heap:

    - If user tries to allocate more memory from that heap using this allocator,
      the allocation fails with `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
    - If the limit is smaller than heap size reported in `VkMemoryHeap::size`, the
      value of this limit will be reported instead when using vmaGetMemoryProperties().

    Warning! Using this feature may not be equivalent to installing a GPU with
    smaller amount of memory, because graphics driver doesn't necessary fail new
    allocations with `VK_ERROR_OUT_OF_DEVICE_MEMORY` result when memory capacity is
    exceeded. It may return success and just silently migrate some device memory
    blocks to system RAM. This driver behavior can also be controlled using
    VK_AMD_memory_overallocation_behavior extension.
    */
    const VkDeviceSize* VMA_NULLABLE VMA_LEN_IF_NOT_NULL("VkPhysicalDeviceMemoryProperties::memoryHeapCount") pHeapSizeLimit;

    /** \brief Pointers to Vulkan functions. Can be null.

    For details see [Pointers to Vulkan functions](@ref config_Vulkan_functions).
    */
    const VmaVulkanFunctions* VMA_NULLABLE pVulkanFunctions;
    /** \brief Handle to Vulkan instance object.

    Starting from version 3.0.0 this member is no longer optional, it must be set!
    */
    VkInstance VMA_NOT_NULL instance;
    /** \brief Optional. Vulkan version that the application uses.

    It must be a value in the format as created by macro `VK_MAKE_VERSION` or a constant like: `VK_API_VERSION_1_1`, `VK_API_VERSION_1_0`.
    The patch version number specified is ignored. Only the major and minor versions are considered.
    Only versions 1.0...1.4 are supported by the current implementation.
    Leaving it initialized to zero is equivalent to `VK_API_VERSION_1_0`.
    It must match the Vulkan version used by the application and supported on the selected physical device,
    so it must be no higher than `VkApplicationInfo::apiVersion` passed to `vkCreateInstance`
    and no higher than `VkPhysicalDeviceProperties::apiVersion` found on the physical device used.
    */
    uint32_t vulkanApiVersion;
#if VMA_EXTERNAL_MEMORY
    /** \brief Either null or a pointer to an array of external memory handle types for each Vulkan memory type.

    If not NULL, it must be a pointer to an array of `VkPhysicalDeviceMemoryProperties::memoryTypeCount`
    elements, defining external memory handle types of particular Vulkan memory type,
    to be passed using `VkExportMemoryAllocateInfoKHR`.

    Any of the elements may be equal to 0, which means not to use `VkExportMemoryAllocateInfoKHR` on this memory type.
    This is also the default in case of `pTypeExternalMemoryHandleTypes` = NULL.
    */
    const VkExternalMemoryHandleTypeFlagsKHR* VMA_NULLABLE VMA_LEN_IF_NOT_NULL("VkPhysicalDeviceMemoryProperties::memoryTypeCount") pTypeExternalMemoryHandleTypes;
#endif // #if VMA_EXTERNAL_MEMORY
} VmaAllocatorCreateInfo;

/// Information about existing #VmaAllocator object.
typedef struct VmaAllocatorInfo
{
    /** \brief Handle to Vulkan instance object.

    This is the same value as has been passed through VmaAllocatorCreateInfo::instance.
    */
    VkInstance VMA_NOT_NULL instance;
    /** \brief Handle to Vulkan physical device object.

    This is the same value as has been passed through VmaAllocatorCreateInfo::physicalDevice.
    */
    VkPhysicalDevice VMA_NOT_NULL physicalDevice;
    /** \brief Handle to Vulkan device object.

    This is the same value as has been passed through VmaAllocatorCreateInfo::device.
    */
    VkDevice VMA_NOT_NULL device;
} VmaAllocatorInfo;

/** @} */

/**
\addtogroup group_stats
@{
*/

/** \brief Calculated statistics of memory usage e.g. in a specific memory type, heap, custom pool, or total.

These are fast to calculate.
See functions: vmaGetHeapBudgets(), vmaGetPoolStatistics().
*/
typedef struct VmaStatistics
{
    /** \brief Number of `VkDeviceMemory` objects - Vulkan memory blocks allocated.
    */
    uint32_t blockCount;
    /** \brief Number of #VmaAllocation objects allocated.

    Dedicated allocations have their own blocks, so each one adds 1 to `allocationCount` as well as `blockCount`.
    */
    uint32_t allocationCount;
    /** \brief Number of bytes allocated in `VkDeviceMemory` blocks.

    \note To avoid confusion, please be aware that what Vulkan calls an "allocation" - a whole `VkDeviceMemory` object
    (e.g. as in `VkPhysicalDeviceLimits::maxMemoryAllocationCount`) is called a "block" in VMA, while VMA calls
    "allocation" a #VmaAllocation object that represents a memory region sub-allocated from such block, usually for a single buffer or image.
    */
    VkDeviceSize blockBytes;
    /** \brief Total number of bytes occupied by all #VmaAllocation objects.

    Always less or equal than `blockBytes`.
    Difference `(blockBytes - allocationBytes)` is the amount of memory allocated from Vulkan
    but unused by any #VmaAllocation.
    */
    VkDeviceSize allocationBytes;
} VmaStatistics;

/** \brief More detailed statistics than #VmaStatistics.

These are slower to calculate. Use for debugging purposes.
See functions: vmaCalculateStatistics(), vmaCalculatePoolStatistics().

Previous version of the statistics API provided averages, but they have been removed
because they can be easily calculated as:

\code
VkDeviceSize allocationSizeAvg = detailedStats.statistics.allocationBytes / detailedStats.statistics.allocationCount;
VkDeviceSize unusedBytes = detailedStats.statistics.blockBytes - detailedStats.statistics.allocationBytes;
VkDeviceSize unusedRangeSizeAvg = unusedBytes / detailedStats.unusedRangeCount;
\endcode
*/
typedef struct VmaDetailedStatistics
{
    /// Basic statistics.
    VmaStatistics statistics;
    /// Number of free ranges of memory between allocations.
    uint32_t unusedRangeCount;
    /// Smallest allocation size. `VK_WHOLE_SIZE` if there are 0 allocations.
    VkDeviceSize allocationSizeMin;
    /// Largest allocation size. 0 if there are 0 allocations.
    VkDeviceSize allocationSizeMax;
    /// Smallest empty range size. `VK_WHOLE_SIZE` if there are 0 empty ranges.
    VkDeviceSize unusedRangeSizeMin;
    /// Largest empty range size. 0 if there are 0 empty ranges.
    VkDeviceSize unusedRangeSizeMax;
} VmaDetailedStatistics;

/** \brief  General statistics from current state of the Allocator -
total memory usage across all memory heaps and types.

These are slower to calculate. Use for debugging purposes.
See function vmaCalculateStatistics().
*/
typedef struct VmaTotalStatistics
{
    VmaDetailedStatistics memoryType[VK_MAX_MEMORY_TYPES];
    VmaDetailedStatistics memoryHeap[VK_MAX_MEMORY_HEAPS];
    VmaDetailedStatistics total;
} VmaTotalStatistics;

/** \brief Statistics of current memory usage and available budget for a specific memory heap.

These are fast to calculate.
See function vmaGetHeapBudgets().
*/
typedef struct VmaBudget
{
    /** \brief Statistics fetched from the library.
    */
    VmaStatistics statistics;
    /** \brief Estimated current memory usage of the program, in bytes.

    Fetched from system using VK_EXT_memory_budget extension if enabled.

    It might be different than `statistics.blockBytes` (usually higher) due to additional implicit objects
    also occupying the memory, like swapchain, pipelines, descriptor heaps, command buffers, or
    `VkDeviceMemory` blocks allocated outside of this library, if any.
    */
    VkDeviceSize usage;
    /** \brief Estimated amount of memory available to the program, in bytes.

    Fetched from system using VK_EXT_memory_budget extension if enabled.

    It might be different (most probably smaller) than `VkMemoryHeap::size[heapIndex]` due to factors
    external to the program, decided by the operating system.
    Difference `budget - usage` is the amount of additional memory that can probably
    be allocated without problems. Exceeding the budget may result in various problems.
    */
    VkDeviceSize budget;
} VmaBudget;

/** @} */

/**
\addtogroup group_alloc
@{
*/

/** \brief Parameters of new #VmaAllocation.

To be used with functions like vmaCreateBuffer(), vmaCreateImage(), and many others.
*/
typedef struct VmaAllocationCreateInfo
{
    /// Use #VmaAllocationCreateFlagBits enum.
    VmaAllocationCreateFlags flags;
    /** \brief Intended usage of memory.

    You can leave #VMA_MEMORY_USAGE_UNKNOWN if you specify memory requirements in other way. \n
    If `pool` is not null, this member is ignored.
    */
    VmaMemoryUsage usage;
    /** \brief Flags that must be set in a Memory Type chosen for an allocation.

    Leave 0 if you specify memory requirements in other way. \n
    If `pool` is not null, this member is ignored.*/
    VkMemoryPropertyFlags requiredFlags;
    /** \brief Flags that preferably should be set in a memory type chosen for an allocation.

    Set to 0 if no additional flags are preferred. \n
    If `pool` is not null, this member is ignored. */
    VkMemoryPropertyFlags preferredFlags;
    /** \brief Bitmask containing one bit set for every memory type acceptable for this allocation.

    Value 0 is equivalent to `UINT32_MAX` - it means any memory type is accepted if
    it meets other requirements specified by this structure, with no further
    restrictions on memory type index. \n
    If `pool` is not null, this member is ignored.
    */
    uint32_t memoryTypeBits;
    /** \brief Pool that this allocation should be created in.

    Leave `VK_NULL_HANDLE` to allocate from default pool. If not null, members:
    `usage`, `requiredFlags`, `preferredFlags`, `memoryTypeBits` are ignored.
    */
    VmaPool VMA_NULLABLE pool;
    /** \brief Custom general-purpose pointer that will be stored in #VmaAllocation, can be read as VmaAllocationInfo::pUserData and changed using vmaSetAllocationUserData().

    If #VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT is used, it must be either
    null or pointer to a null-terminated string. The string will be then copied to
    internal buffer, so it doesn't need to be valid after allocation call.
    */
    void* VMA_NULLABLE pUserData;
    /** \brief A floating-point value between 0 and 1, indicating the priority of the allocation relative to other memory allocations.

    It is used only when #VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT flag was used during creation of the #VmaAllocator object
    and this allocation ends up as dedicated or is explicitly forced as dedicated using #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
    Otherwise, it has the priority of a memory block where it is placed and this variable is ignored.
    */
    float priority;
} VmaAllocationCreateInfo;

/// Describes parameter of created #VmaPool.
typedef struct VmaPoolCreateInfo
{
    /** \brief Vulkan memory type index to allocate this pool from.
    */
    uint32_t memoryTypeIndex;
    /** \brief Use combination of #VmaPoolCreateFlagBits.
    */
    VmaPoolCreateFlags flags;
    /** \brief Size of a single `VkDeviceMemory` block to be allocated as part of this pool, in bytes. Optional.

    Specify nonzero to set explicit, constant size of memory blocks used by this
    pool.

    Leave 0 to use default and let the library manage block sizes automatically.
    Sizes of particular blocks may vary.
    In this case, the pool will also support dedicated allocations.
    */
    VkDeviceSize blockSize;
    /** \brief Minimum number of blocks to be always allocated in this pool, even if they stay empty.

    Set to 0 to have no preallocated blocks and allow the pool be completely empty.
    */
    size_t minBlockCount;
    /** \brief Maximum number of blocks that can be allocated in this pool. Optional.

    Set to 0 to use default, which is `SIZE_MAX`, which means no limit.

    Set to same value as VmaPoolCreateInfo::minBlockCount to have fixed amount of memory allocated
    throughout whole lifetime of this pool.
    */
    size_t maxBlockCount;
    /** \brief A floating-point value between 0 and 1, indicating the priority of the allocations in this pool relative to other memory allocations.

    It is used only when #VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT flag was used during creation of the #VmaAllocator object.
    Otherwise, this variable is ignored.
    */
    float priority;
    /** \brief Additional minimum alignment to be used for all allocations created from this pool. Can be 0.

    Leave 0 (default) not to impose any additional alignment. If not 0, it must be a power of two.
    It can be useful in cases where alignment returned by Vulkan by functions like `vkGetBufferMemoryRequirements` is not enough,
    e.g. when doing interop with OpenGL.
    */
    VkDeviceSize minAllocationAlignment;
    /** \brief Additional `pNext` chain to be attached to `VkMemoryAllocateInfo` used for every allocation made by this pool. Optional.

    Optional, can be null. If not null, it must point to a `pNext` chain of structures that can be attached to `VkMemoryAllocateInfo`.
    It can be useful for special needs such as adding `VkExportMemoryAllocateInfoKHR`.
    Structures pointed by this member must remain alive and unchanged for the whole lifetime of the custom pool.

    Please note that some structures, e.g. `VkMemoryPriorityAllocateInfoEXT`, `VkMemoryDedicatedAllocateInfoKHR`,
    can be attached automatically by this library when using other, more convenient of its features.
    */
    void* VMA_NULLABLE VMA_EXTENDS_VK_STRUCT(VkMemoryAllocateInfo) pMemoryAllocateNext;
} VmaPoolCreateInfo;

/** @} */

/**
\addtogroup group_alloc
@{
*/

/**
Parameters of #VmaAllocation objects, that can be retrieved using function vmaGetAllocationInfo().

There is also an extended version of this structure that carries additional parameters: #VmaAllocationInfo2.
*/
typedef struct VmaAllocationInfo
{
    /** \brief Memory type index that this allocation was allocated from.

    It never changes.
    */
    uint32_t memoryType;
    /** \brief Handle to Vulkan memory object.

    Same memory object can be shared by multiple allocations.

    It can change after the allocation is moved during \ref defragmentation.
    */
    VkDeviceMemory VMA_NULLABLE_NON_DISPATCHABLE deviceMemory;
    /** \brief Offset in `VkDeviceMemory` object to the beginning of this allocation, in bytes. `(deviceMemory, offset)` pair is unique to this allocation.

    You usually don't need to use this offset. If you create a buffer or an image together with the allocation using e.g. function
    vmaCreateBuffer(), vmaCreateImage(), functions that operate on these resources refer to the beginning of the buffer or image,
    not entire device memory block. Functions like vmaMapMemory(), vmaBindBufferMemory() also refer to the beginning of the allocation
    and apply this offset automatically.

    It can change after the allocation is moved during \ref defragmentation.
    */
    VkDeviceSize offset;
    /** \brief Size of this allocation, in bytes.

    It never changes.

    \note Allocation size returned in this variable may be greater than the size
    requested for the resource e.g. as `VkBufferCreateInfo::size`. Whole size of the
    allocation is accessible for operations on memory e.g. using a pointer after
    mapping with vmaMapMemory(), but operations on the resource e.g. using
    `vkCmdCopyBuffer` must be limited to the size of the resource.
    */
    VkDeviceSize size;
    /** \brief Pointer to the beginning of this allocation as mapped data.

    If the allocation hasn't been mapped using vmaMapMemory() and hasn't been
    created with #VMA_ALLOCATION_CREATE_MAPPED_BIT flag, this value is null.

    It can change after call to vmaMapMemory(), vmaUnmapMemory().
    It can also change after the allocation is moved during \ref defragmentation.
    */
    void* VMA_NULLABLE pMappedData;
    /** \brief Custom general-purpose pointer that was passed as VmaAllocationCreateInfo::pUserData or set using vmaSetAllocationUserData().

    It can change after call to vmaSetAllocationUserData() for this allocation.
    */
    void* VMA_NULLABLE pUserData;
    /** \brief Custom allocation name that was set with vmaSetAllocationName().

    It can change after call to vmaSetAllocationName() for this allocation.

    Another way to set custom name is to pass it in VmaAllocationCreateInfo::pUserData with
    additional flag #VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT set [DEPRECATED].
    */
    const char* VMA_NULLABLE pName;
} VmaAllocationInfo;

/// Extended parameters of a #VmaAllocation object that can be retrieved using function vmaGetAllocationInfo2().
typedef struct VmaAllocationInfo2
{
    /** \brief Basic parameters of the allocation.

    If you need only these, you can use function vmaGetAllocationInfo() and structure #VmaAllocationInfo instead.
    */
    VmaAllocationInfo allocationInfo;
    /** \brief Size of the `VkDeviceMemory` block that the allocation belongs to.

    In case of an allocation with dedicated memory, it will be equal to `allocationInfo.size`.
    */
    VkDeviceSize blockSize;
    /** \brief `VK_TRUE` if the allocation has dedicated memory, `VK_FALSE` if it was placed as part of a larger memory block.

    When `VK_TRUE`, it also means `VkMemoryDedicatedAllocateInfo` was used when creating the allocation
    (if VK_KHR_dedicated_allocation extension or Vulkan version >= 1.1 is enabled).
    */
    VkBool32 dedicatedMemory;
} VmaAllocationInfo2;

/** Callback function called during vmaBeginDefragmentation() to check custom criterion about ending current defragmentation pass.

Should return true if the defragmentation needs to stop current pass.
*/
typedef VkBool32 (VKAPI_PTR* PFN_vmaCheckDefragmentationBreakFunction)(void* VMA_NULLABLE pUserData);

/** \brief Parameters for defragmentation.

To be used with function vmaBeginDefragmentation().
*/
typedef struct VmaDefragmentationInfo
{
    /// \brief Use combination of #VmaDefragmentationFlagBits.
    VmaDefragmentationFlags flags;
    /** \brief Custom pool to be defragmented.

    If null then default pools will undergo defragmentation process.
    */
    VmaPool VMA_NULLABLE pool;
    /** \brief Maximum numbers of bytes that can be copied during single pass, while moving allocations to different places.

    `0` means no limit.
    */
    VkDeviceSize maxBytesPerPass;
    /** \brief Maximum number of allocations that can be moved during single pass to a different place.

    `0` means no limit.
    */
    uint32_t maxAllocationsPerPass;
    /** \brief Optional custom callback for stopping vmaBeginDefragmentation().

    Have to return true for breaking current defragmentation pass.
    */
    PFN_vmaCheckDefragmentationBreakFunction VMA_NULLABLE pfnBreakCallback;
    /// \brief Optional data to pass to custom callback for stopping pass of defragmentation.
    void* VMA_NULLABLE pBreakCallbackUserData;
} VmaDefragmentationInfo;

/// Single move of an allocation to be done for defragmentation.
typedef struct VmaDefragmentationMove
{
    /// Operation to be performed on the allocation by vmaEndDefragmentationPass(). Default value is #VMA_DEFRAGMENTATION_MOVE_OPERATION_COPY. You can modify it.
    VmaDefragmentationMoveOperation operation;
    /// Allocation that should be moved.
    VmaAllocation VMA_NOT_NULL srcAllocation;
    /** \brief Temporary allocation pointing to destination memory that will replace `srcAllocation`.

    \warning Do not store this allocation in your data structures! It exists only temporarily, for the duration of the defragmentation pass,
    to be used for binding new buffer/image to the destination memory using e.g. vmaBindBufferMemory().
    vmaEndDefragmentationPass() will destroy it and make `srcAllocation` point to this memory.
    */
    VmaAllocation VMA_NOT_NULL dstTmpAllocation;
} VmaDefragmentationMove;

/** \brief Parameters for incremental defragmentation steps.

To be used with function vmaBeginDefragmentationPass().
*/
typedef struct VmaDefragmentationPassMoveInfo
{
    /// Number of elements in the `pMoves` array.
    uint32_t moveCount;
    /** \brief Array of moves to be performed by the user in the current defragmentation pass.

    Pointer to an array of `moveCount` elements, owned by VMA, created in vmaBeginDefragmentationPass(), destroyed in vmaEndDefragmentationPass().

    For each element, you should:

    1. Create a new buffer/image in the place pointed by VmaDefragmentationMove::dstMemory + VmaDefragmentationMove::dstOffset.
    2. Copy data from the VmaDefragmentationMove::srcAllocation e.g. using `vkCmdCopyBuffer`, `vkCmdCopyImage`.
    3. Make sure these commands finished executing on the GPU.
    4. Destroy the old buffer/image.

    Only then you can finish defragmentation pass by calling vmaEndDefragmentationPass().
    After this call, the allocation will point to the new place in memory.

    Alternatively, if you cannot move specific allocation, you can set VmaDefragmentationMove::operation to #VMA_DEFRAGMENTATION_MOVE_OPERATION_IGNORE.

    Alternatively, if you decide you want to completely remove the allocation:

    1. Destroy its buffer/image.
    2. Set VmaDefragmentationMove::operation to #VMA_DEFRAGMENTATION_MOVE_OPERATION_DESTROY.

    Then, after vmaEndDefragmentationPass() the allocation will be freed.
    */
    VmaDefragmentationMove* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(moveCount) pMoves;
} VmaDefragmentationPassMoveInfo;

/// Statistics returned for defragmentation process in function vmaEndDefragmentation().
typedef struct VmaDefragmentationStats
{
    /// Total number of bytes that have been copied while moving allocations to different places.
    VkDeviceSize bytesMoved;
    /// Total number of bytes that have been released to the system by freeing empty `VkDeviceMemory` objects.
    VkDeviceSize bytesFreed;
    /// Number of allocations that have been moved to different places.
    uint32_t allocationsMoved;
    /// Number of empty `VkDeviceMemory` objects that have been released to the system.
    uint32_t deviceMemoryBlocksFreed;
} VmaDefragmentationStats;

/** @} */

/**
\addtogroup group_virtual
@{
*/

/// Parameters of created #VmaVirtualBlock object to be passed to vmaCreateVirtualBlock().
typedef struct VmaVirtualBlockCreateInfo
{
    /** \brief Total size of the virtual block.

    Sizes can be expressed in bytes or any units you want as long as you are consistent in using them.
    For example, if you allocate from some array of structures, 1 can mean single instance of entire structure.
    */
    VkDeviceSize size;

    /** \brief Use combination of #VmaVirtualBlockCreateFlagBits.
    */
    VmaVirtualBlockCreateFlags flags;

    /** \brief Custom CPU memory allocation callbacks. Optional.

    Optional, can be null. When specified, they will be used for all CPU-side memory allocations.
    */
    const VkAllocationCallbacks* VMA_NULLABLE pAllocationCallbacks;
} VmaVirtualBlockCreateInfo;

/// Parameters of created virtual allocation to be passed to vmaVirtualAllocate().
typedef struct VmaVirtualAllocationCreateInfo
{
    /** \brief Size of the allocation.

    Cannot be zero.
    */
    VkDeviceSize size;
    /** \brief Required alignment of the allocation. Optional.

    Must be power of two. Special value 0 has the same meaning as 1 - means no special alignment is required, so allocation can start at any offset.
    */
    VkDeviceSize alignment;
    /** \brief Use combination of #VmaVirtualAllocationCreateFlagBits.
    */
    VmaVirtualAllocationCreateFlags flags;
    /** \brief Custom pointer to be associated with the allocation. Optional.

    It can be any value and can be used for user-defined purposes. It can be fetched or changed later.
    */
    void* VMA_NULLABLE pUserData;
} VmaVirtualAllocationCreateInfo;

/// Parameters of an existing virtual allocation, returned by vmaGetVirtualAllocationInfo().
typedef struct VmaVirtualAllocationInfo
{
    /** \brief Offset of the allocation.

    Offset at which the allocation was made.
    */
    VkDeviceSize offset;
    /** \brief Size of the allocation.

    Same value as passed in VmaVirtualAllocationCreateInfo::size.
    */
    VkDeviceSize size;
    /** \brief Custom pointer associated with the allocation.

    Same value as passed in VmaVirtualAllocationCreateInfo::pUserData or to vmaSetVirtualAllocationUserData().
    */
    void* VMA_NULLABLE pUserData;
} VmaVirtualAllocationInfo;

/** @} */

#endif // _VMA_DATA_TYPES_DECLARATIONS

#ifndef _VMA_FUNCTION_HEADERS

/**
\addtogroup group_init
@{
*/

#ifdef VOLK_HEADER_VERSION
/** \brief Fully initializes `pDstVulkanFunctions` structure with Vulkan functions needed by VMA
using [volk library](https://github.com/zeux/volk).

This function is defined in VMA header only if "volk.h" was included before it.

To use this function properly:

-# Initialize volk and Vulkan:
   -# Call `volkInitialize()`
   -# Create `VkInstance` object
   -# Call `volkLoadInstance()`
   -# Create `VkDevice` object
   -# Call `volkLoadDevice()`
-# Fill in structure #VmaAllocatorCreateInfo, especially members:
   - VmaAllocatorCreateInfo::device
   - VmaAllocatorCreateInfo::vulkanApiVersion
   - VmaAllocatorCreateInfo::flags - set appropriate flags for the Vulkan extensions you enabled
-# Create an instance of the #VmaVulkanFunctions structure.
-# Call vmaImportVulkanFunctionsFromVolk().
   Parameter `pAllocatorCreateInfo` is read to find out which functions should be fetched for
   appropriate Vulkan version and extensions.
   Parameter `pDstVulkanFunctions` is filled with those function pointers, or null if not applicable.
-# Attach the #VmaVulkanFunctions structure to VmaAllocatorCreateInfo::pVulkanFunctions.
-# Call vmaCreateAllocator() to create the #VmaAllocator object.

Example:

\code
VmaAllocatorCreateInfo allocatorCreateInfo = {};
allocatorCreateInfo.physicalDevice = myPhysicalDevice;
allocatorCreateInfo.device = myDevice;
allocatorCreateInfo.instance = myInstance;
allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT |
    VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT |
    VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT;

VmaVulkanFunctions vulkanFunctions;
VkResult res = vmaImportVulkanFunctionsFromVolk(&allocatorCreateInfo, &vulkanFunctions);
// Check res...
allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

VmaAllocator allocator;
res = vmaCreateAllocator(&allocatorCreateInfo, &allocator);
// Check res...
\endcode

Internally in this function, pointers to functions related to the entire Vulkan instance are fetched using global function definitions,
while pointers to functions related to the Vulkan device are fetched using `volkLoadDeviceTable()` for given `pAllocatorCreateInfo->device`.
 */
VMA_CALL_PRE VkResult VMA_CALL_POST vmaImportVulkanFunctionsFromVolk(
    const VmaAllocatorCreateInfo* VMA_NOT_NULL pAllocatorCreateInfo,
    VmaVulkanFunctions* VMA_NOT_NULL pDstVulkanFunctions);
#endif

/// Creates #VmaAllocator object.
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAllocator(
    const VmaAllocatorCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaAllocator VMA_NULLABLE* VMA_NOT_NULL pAllocator);

/// Destroys allocator object.
VMA_CALL_PRE void VMA_CALL_POST vmaDestroyAllocator(
    VmaAllocator VMA_NULLABLE allocator);

/** \brief Returns information about existing #VmaAllocator object - handle to Vulkan device etc.

It might be useful if you want to keep just the #VmaAllocator handle and fetch other required handles to
`VkPhysicalDevice`, `VkDevice` etc. every time using this function.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocatorInfo(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocatorInfo* VMA_NOT_NULL pAllocatorInfo);

/**
PhysicalDeviceProperties are fetched from physicalDevice by the allocator.
You can access it here, without fetching it again on your own.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetPhysicalDeviceProperties(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkPhysicalDeviceProperties* VMA_NULLABLE* VMA_NOT_NULL ppPhysicalDeviceProperties);

/**
PhysicalDeviceMemoryProperties are fetched from physicalDevice by the allocator.
You can access it here, without fetching it again on your own.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetMemoryProperties(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkPhysicalDeviceMemoryProperties* VMA_NULLABLE* VMA_NOT_NULL ppPhysicalDeviceMemoryProperties);

/**
\brief Given Memory Type Index, returns Property Flags of this memory type.

This is just a convenience function. Same information can be obtained using
vmaGetMemoryProperties().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetMemoryTypeProperties(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t memoryTypeIndex,
    VkMemoryPropertyFlags* VMA_NOT_NULL pFlags);

/** \brief Sets index of the current frame.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaSetCurrentFrameIndex(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t frameIndex);

/** @} */

/**
\addtogroup group_stats
@{
*/

/** \brief Retrieves statistics from current state of the Allocator.

This function is called "calculate" not "get" because it has to traverse all
internal data structures, so it may be quite slow. Use it for debugging purposes.
For faster but more brief statistics suitable to be called every frame or every allocation,
use vmaGetHeapBudgets().

Note that when using allocator from multiple threads, returned information may immediately
become outdated.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaCalculateStatistics(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaTotalStatistics* VMA_NOT_NULL pStats);

/** \brief Retrieves information about current memory usage and budget for all memory heaps.

\param allocator
\param[out] pBudgets Must point to array with number of elements at least equal to number of memory heaps in physical device used.

This function is called "get" not "calculate" because it is very fast, suitable to be called
every frame or every allocation. For more detailed statistics use vmaCalculateStatistics().

Note that when using allocator from multiple threads, returned information may immediately
become outdated.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetHeapBudgets(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaBudget* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL("VkPhysicalDeviceMemoryProperties::memoryHeapCount") pBudgets);

/** @} */

/**
\addtogroup group_alloc
@{
*/

/**
\brief Helps to find `memoryTypeIndex`, given `memoryTypeBits` and #VmaAllocationCreateInfo.

This algorithm tries to find a memory type that:

- Is allowed by `memoryTypeBits`.
- Contains all the flags from `pAllocationCreateInfo->requiredFlags`.
- Matches intended usage.
- Has as many flags from `pAllocationCreateInfo->preferredFlags` as possible.

\return Returns `VK_ERROR_FEATURE_NOT_PRESENT` if not found. Receiving such result
from this function or any other allocating function probably means that your
device doesn't support any memory type with requested features for the specific
type of resource you want to use it for. Please check parameters of your
resource, like image layout (`OPTIMAL` versus `LINEAR`) or mip level count.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndex(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t memoryTypeBits,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    uint32_t* VMA_NOT_NULL pMemoryTypeIndex);

/**
\brief Helps to find `memoryTypeIndex`, given `VkBufferCreateInfo` and #VmaAllocationCreateInfo.

It can be useful e.g. to determine value to be used as VmaPoolCreateInfo::memoryTypeIndex.
It may need to internally create a temporary, dummy buffer that never has memory bound.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndexForBufferInfo(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    uint32_t* VMA_NOT_NULL pMemoryTypeIndex);

/**
\brief Helps to find `memoryTypeIndex`, given `VkImageCreateInfo` and #VmaAllocationCreateInfo.

It can be useful e.g. to determine value to be used as VmaPoolCreateInfo::memoryTypeIndex.
It may need to internally create a temporary, dummy image that never has memory bound.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFindMemoryTypeIndexForImageInfo(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkImageCreateInfo* VMA_NOT_NULL pImageCreateInfo,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    uint32_t* VMA_NOT_NULL pMemoryTypeIndex);

/** \brief Allocates Vulkan device memory and creates #VmaPool object.

\param allocator Allocator object.
\param pCreateInfo Parameters of pool to create.
\param[out] pPool Handle to created pool.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreatePool(
    VmaAllocator VMA_NOT_NULL allocator,
    const VmaPoolCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaPool VMA_NULLABLE* VMA_NOT_NULL pPool);

/** \brief Destroys #VmaPool object and frees Vulkan device memory.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaDestroyPool(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NULLABLE pool);

/** @} */

/**
\addtogroup group_stats
@{
*/

/** \brief Retrieves statistics of existing #VmaPool object.

\param allocator Allocator object.
\param pool Pool object.
\param[out] pPoolStats Statistics of specified pool.

Note that when using the pool from multiple threads, returned information may immediately
become outdated.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetPoolStatistics(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NOT_NULL pool,
    VmaStatistics* VMA_NOT_NULL pPoolStats);

/** \brief Retrieves detailed statistics of existing #VmaPool object.

\param allocator Allocator object.
\param pool Pool object.
\param[out] pPoolStats Statistics of specified pool.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaCalculatePoolStatistics(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NOT_NULL pool,
    VmaDetailedStatistics* VMA_NOT_NULL pPoolStats);

/** @} */

/**
\addtogroup group_alloc
@{
*/

/** \brief Checks magic number in margins around all allocations in given memory pool in search for corruptions.

Corruption detection is enabled only when `VMA_DEBUG_DETECT_CORRUPTION` macro is defined to nonzero,
`VMA_DEBUG_MARGIN` is defined to nonzero and the pool is created in memory type that is
`HOST_VISIBLE` and `HOST_COHERENT`. For more information, see [Corruption detection](@ref debugging_memory_usage_corruption_detection).

Possible return values:

- `VK_ERROR_FEATURE_NOT_PRESENT` - corruption detection is not enabled for specified pool.
- `VK_SUCCESS` - corruption detection has been performed and succeeded.
- `VK_ERROR_UNKNOWN` - corruption detection has been performed and found memory corruptions around one of the allocations.
  `VMA_ASSERT` is also fired in that case.
- Other value: Error returned by Vulkan, e.g. memory mapping failure.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCheckPoolCorruption(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NOT_NULL pool);

/** \brief Retrieves name of a custom pool.

After the call `ppName` is either null or points to an internally-owned null-terminated string
containing name of the pool that was previously set. The pointer becomes invalid when the pool is
destroyed or its name is changed using vmaSetPoolName().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetPoolName(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NOT_NULL pool,
    const char* VMA_NULLABLE* VMA_NOT_NULL ppName);

/** \brief Sets name of a custom pool.

`pName` can be either null or pointer to a null-terminated string with new name for the pool.
Function makes internal copy of the string, so it can be changed or freed immediately after this call.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaSetPoolName(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaPool VMA_NOT_NULL pool,
    const char* VMA_NULLABLE pName);

/** \brief General purpose memory allocation.

\param allocator
\param pVkMemoryRequirements
\param pCreateInfo
\param[out] pAllocation Handle to allocated memory.
\param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function vmaGetAllocationInfo().

You should free the memory using vmaFreeMemory() or vmaFreeMemoryPages().

It is recommended to use vmaAllocateMemoryForBuffer(), vmaAllocateMemoryForImage(),
vmaCreateBuffer(), vmaCreateImage() instead whenever possible.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkMemoryRequirements* VMA_NOT_NULL pVkMemoryRequirements,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/** \brief General purpose memory allocation for multiple allocation objects at once.

\param allocator Allocator object.
\param pVkMemoryRequirements Memory requirements for each allocation.
\param pCreateInfo Creation parameters for each allocation.
\param allocationCount Number of allocations to make.
\param[out] pAllocations Pointer to array that will be filled with handles to created allocations.
\param[out] pAllocationInfo Optional. Pointer to array that will be filled with parameters of created allocations.

You should free the memory using vmaFreeMemory() or vmaFreeMemoryPages().

Word "pages" is just a suggestion to use this function to allocate pieces of memory needed for sparse binding.
It is just a general purpose allocation function able to make multiple allocations at once.
It may be internally optimized to be more efficient than calling vmaAllocateMemory() `allocationCount` times.

All allocations are made using same parameters. All of them are created out of the same memory pool and type.
If any allocation fails, all allocations already made within this function call are also freed, so that when
returned result is not `VK_SUCCESS`, `pAllocation` array is always entirely filled with `VK_NULL_HANDLE`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryPages(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkMemoryRequirements* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(allocationCount) pVkMemoryRequirements,
    const VmaAllocationCreateInfo* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(allocationCount) pCreateInfo,
    size_t allocationCount,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(allocationCount) pAllocations,
    VmaAllocationInfo* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) pAllocationInfo);

/** \brief Allocates memory suitable for given `VkBuffer`.

\param allocator
\param buffer
\param pCreateInfo
\param[out] pAllocation Handle to allocated memory.
\param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function vmaGetAllocationInfo().

It only creates #VmaAllocation. To bind the memory to the buffer, use vmaBindBufferMemory().

This is a special-purpose function. In most cases you should use vmaCreateBuffer().

You must free the allocation using vmaFreeMemory() when no longer needed.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryForBuffer(
    VmaAllocator VMA_NOT_NULL allocator,
    VkBuffer VMA_NOT_NULL_NON_DISPATCHABLE buffer,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/** \brief Allocates memory suitable for given `VkImage`.

\param allocator
\param image
\param pCreateInfo
\param[out] pAllocation Handle to allocated memory.
\param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function vmaGetAllocationInfo().

It only creates #VmaAllocation. To bind the memory to the buffer, use vmaBindImageMemory().

This is a special-purpose function. In most cases you should use vmaCreateImage().

You must free the allocation using vmaFreeMemory() when no longer needed.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaAllocateMemoryForImage(
    VmaAllocator VMA_NOT_NULL allocator,
    VkImage VMA_NOT_NULL_NON_DISPATCHABLE image,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/** \brief Frees memory previously allocated using vmaAllocateMemory(), vmaAllocateMemoryForBuffer(), or vmaAllocateMemoryForImage().

Passing `VK_NULL_HANDLE` as `allocation` is valid. Such function call is just skipped.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaFreeMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NULLABLE allocation);

/** \brief Frees memory and destroys multiple allocations.

Word "pages" is just a suggestion to use this function to free pieces of memory used for sparse binding.
It is just a general purpose function to free memory and destroy allocations made using e.g. vmaAllocateMemory(),
vmaAllocateMemoryPages() and other functions.
It may be internally optimized to be more efficient than calling vmaFreeMemory() `allocationCount` times.

Allocations in `pAllocations` array can come from any memory pools and types.
Passing `VK_NULL_HANDLE` as elements of `pAllocations` array is valid. Such entries are just skipped.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaFreeMemoryPages(
    VmaAllocator VMA_NOT_NULL allocator,
    size_t allocationCount,
    const VmaAllocation VMA_NULLABLE* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(allocationCount) pAllocations);

/** \brief Returns current information about specified allocation.

Current parameters of given allocation are returned in `pAllocationInfo`.

Although this function doesn't lock any mutex, so it should be quite efficient,
you should avoid calling it too often.
You can retrieve same VmaAllocationInfo structure while creating your resource, from function
vmaCreateBuffer(), vmaCreateImage(). You can remember it if you are sure parameters don't change
(e.g. due to defragmentation).

There is also a new function vmaGetAllocationInfo2() that offers extended information
about the allocation, returned using new structure #VmaAllocationInfo2.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocationInfo(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VmaAllocationInfo* VMA_NOT_NULL pAllocationInfo);

/** \brief Returns extended information about specified allocation.

Current parameters of given allocation are returned in `pAllocationInfo`.
Extended parameters in structure #VmaAllocationInfo2 include memory block size
and a flag telling whether the allocation has dedicated memory.
It can be useful e.g. for interop with OpenGL.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocationInfo2(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VmaAllocationInfo2* VMA_NOT_NULL pAllocationInfo);

/** \brief Sets pUserData in given allocation to new value.

The value of pointer `pUserData` is copied to allocation's `pUserData`.
It is opaque, so you can use it however you want - e.g.
as a pointer, ordinal number or some handle to you own data.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaSetAllocationUserData(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    void* VMA_NULLABLE pUserData);

/** \brief Sets pName in given allocation to new value.

`pName` must be either null, or pointer to a null-terminated string. The function
makes local copy of the string and sets it as allocation's `pName`. String
passed as pName doesn't need to be valid for whole lifetime of the allocation -
you can free it after this call. String previously pointed by allocation's
`pName` is freed from memory.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaSetAllocationName(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const char* VMA_NULLABLE pName);

/**
\brief Given an allocation, returns Property Flags of its memory type.

This is just a convenience function. Same information can be obtained using
vmaGetAllocationInfo() + vmaGetMemoryProperties().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetAllocationMemoryProperties(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkMemoryPropertyFlags* VMA_NOT_NULL pFlags);


#if VMA_EXTERNAL_MEMORY_WIN32
/**
\brief Given an allocation, returns Win32 handle that may be imported by other processes or APIs.

\param hTargetProcess Must be a valid handle to target process or null. If it's null, the function returns
    handle for the current process.
\param[out] pHandle Output parameter that returns the handle.

The function fills `pHandle` with handle that can be used in target process.
The handle is fetched using function `vkGetMemoryWin32HandleKHR`.
When no longer needed, you must close it using:

\code
CloseHandle(handle);
\endcode

You can close it any time, before or after destroying the allocation object.
It is reference-counted internally by Windows.

Note the handle is returned for the entire `VkDeviceMemory` block that the allocation belongs to.
If the allocation is sub-allocated from a larger block, you may need to consider the offset of the allocation
(VmaAllocationInfo::offset).

If the function fails with `VK_ERROR_FEATURE_NOT_PRESENT` error code, please double-check
that VmaVulkanFunctions::vkGetMemoryWin32HandleKHR function pointer is set, e.g. either by using `VMA_DYNAMIC_VULKAN_FUNCTIONS`
or by manually passing it through VmaAllocatorCreateInfo::pVulkanFunctions.

For more information, see chapter \ref vk_khr_external_memory_win32.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaGetMemoryWin32Handle(VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation, HANDLE hTargetProcess, HANDLE* VMA_NOT_NULL pHandle);
#endif // VMA_EXTERNAL_MEMORY_WIN32

/** \brief Maps memory represented by given allocation and returns pointer to it.

Maps memory represented by given allocation to make it accessible to CPU code.
When succeeded, `*ppData` contains pointer to first byte of this memory.

\warning
If the allocation is part of a bigger `VkDeviceMemory` block, returned pointer is
correctly offsetted to the beginning of region assigned to this particular allocation.
Unlike the result of `vkMapMemory`, it points to the allocation, not to the beginning of the whole block.
You should not add VmaAllocationInfo::offset to it!

Mapping is internally reference-counted and synchronized, so despite raw Vulkan
function `vkMapMemory()` cannot be used to map same block of `VkDeviceMemory`
multiple times simultaneously, it is safe to call this function on allocations
assigned to the same memory block. Actual Vulkan memory will be mapped on first
mapping and unmapped on last unmapping.

If the function succeeded, you must call vmaUnmapMemory() to unmap the
allocation when mapping is no longer needed or before freeing the allocation, at
the latest.

It also safe to call this function multiple times on the same allocation. You
must call vmaUnmapMemory() same number of times as you called vmaMapMemory().

It is also safe to call this function on allocation created with
#VMA_ALLOCATION_CREATE_MAPPED_BIT flag. Its memory stays mapped all the time.
You must still call vmaUnmapMemory() same number of times as you called
vmaMapMemory(). You must not call vmaUnmapMemory() additional time to free the
"0-th" mapping made automatically due to #VMA_ALLOCATION_CREATE_MAPPED_BIT flag.

This function fails when used on allocation made in memory type that is not
`HOST_VISIBLE`.

This function doesn't automatically flush or invalidate caches.
If the allocation is made from a memory types that is not `HOST_COHERENT`,
you also need to use vmaInvalidateAllocation() / vmaFlushAllocation(), as required by Vulkan specification.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaMapMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    void* VMA_NULLABLE* VMA_NOT_NULL ppData);

/** \brief Unmaps memory represented by given allocation, mapped previously using vmaMapMemory().

For details, see description of vmaMapMemory().

This function doesn't automatically flush or invalidate caches.
If the allocation is made from a memory types that is not `HOST_COHERENT`,
you also need to use vmaInvalidateAllocation() / vmaFlushAllocation(), as required by Vulkan specification.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaUnmapMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation);

/** \brief Flushes memory of given allocation.

Calls `vkFlushMappedMemoryRanges()` for memory associated with given range of given allocation.
It needs to be called after writing to a mapped memory for memory types that are not `HOST_COHERENT`.
Unmap operation doesn't do that automatically.

- `offset` must be relative to the beginning of allocation.
- `size` can be `VK_WHOLE_SIZE`. It means all memory from `offset` the the end of given allocation.
- `offset` and `size` don't have to be aligned.
  They are internally rounded down/up to multiply of `nonCoherentAtomSize`.
- If `size` is 0, this call is ignored.
- If memory type that the `allocation` belongs to is not `HOST_VISIBLE` or it is `HOST_COHERENT`,
  this call is ignored.

Warning! `offset` and `size` are relative to the contents of given `allocation`.
If you mean whole allocation, you can pass 0 and `VK_WHOLE_SIZE`, respectively.
Do not pass allocation's offset as `offset`!!!

This function returns the `VkResult` from `vkFlushMappedMemoryRanges` if it is
called, otherwise `VK_SUCCESS`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFlushAllocation(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize offset,
    VkDeviceSize size);

/** \brief Invalidates memory of given allocation.

Calls `vkInvalidateMappedMemoryRanges()` for memory associated with given range of given allocation.
It needs to be called before reading from a mapped memory for memory types that are not `HOST_COHERENT`.
Map operation doesn't do that automatically.

- `offset` must be relative to the beginning of allocation.
- `size` can be `VK_WHOLE_SIZE`. It means all memory from `offset` the the end of given allocation.
- `offset` and `size` don't have to be aligned.
  They are internally rounded down/up to multiply of `nonCoherentAtomSize`.
- If `size` is 0, this call is ignored.
- If memory type that the `allocation` belongs to is not `HOST_VISIBLE` or it is `HOST_COHERENT`,
  this call is ignored.

Warning! `offset` and `size` are relative to the contents of given `allocation`.
If you mean whole allocation, you can pass 0 and `VK_WHOLE_SIZE`, respectively.
Do not pass allocation's offset as `offset`!!!

This function returns the `VkResult` from `vkInvalidateMappedMemoryRanges` if
it is called, otherwise `VK_SUCCESS`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaInvalidateAllocation(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize offset,
    VkDeviceSize size);

/** \brief Flushes memory of given set of allocations.

Calls `vkFlushMappedMemoryRanges()` for memory associated with given ranges of given allocations.
For more information, see documentation of vmaFlushAllocation().

\param allocator
\param allocationCount
\param allocations
\param offsets If not null, it must point to an array of offsets of regions to flush, relative to the beginning of respective allocations. Null means all offsets are zero.
\param sizes If not null, it must point to an array of sizes of regions to flush in respective allocations. Null means `VK_WHOLE_SIZE` for all allocations.

This function returns the `VkResult` from `vkFlushMappedMemoryRanges` if it is
called, otherwise `VK_SUCCESS`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaFlushAllocations(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t allocationCount,
    const VmaAllocation VMA_NOT_NULL* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) allocations,
    const VkDeviceSize* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) offsets,
    const VkDeviceSize* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) sizes);

/** \brief Invalidates memory of given set of allocations.

Calls `vkInvalidateMappedMemoryRanges()` for memory associated with given ranges of given allocations.
For more information, see documentation of vmaInvalidateAllocation().

\param allocator
\param allocationCount
\param allocations
\param offsets If not null, it must point to an array of offsets of regions to flush, relative to the beginning of respective allocations. Null means all offsets are zero.
\param sizes If not null, it must point to an array of sizes of regions to flush in respective allocations. Null means `VK_WHOLE_SIZE` for all allocations.

This function returns the `VkResult` from `vkInvalidateMappedMemoryRanges` if it is
called, otherwise `VK_SUCCESS`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaInvalidateAllocations(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t allocationCount,
    const VmaAllocation VMA_NOT_NULL* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) allocations,
    const VkDeviceSize* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) offsets,
    const VkDeviceSize* VMA_NULLABLE VMA_LEN_IF_NOT_NULL(allocationCount) sizes);

/** \brief Maps the allocation temporarily if needed, copies data from specified host pointer to it, and flushes the memory from the host caches if needed.

\param allocator
\param pSrcHostPointer Pointer to the host data that become source of the copy.
\param dstAllocation   Handle to the allocation that becomes destination of the copy.
\param dstAllocationLocalOffset  Offset within `dstAllocation` where to write copied data, in bytes.
\param size            Number of bytes to copy.

This is a convenience function that allows to copy data from a host pointer to an allocation easily.
Same behavior can be achieved by calling vmaMapMemory(), `memcpy()`, vmaUnmapMemory(), vmaFlushAllocation().

This function can be called only for allocations created in a memory type that has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` flag.
It can be ensured e.g. by using #VMA_MEMORY_USAGE_AUTO and #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or
#VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
Otherwise, the function will fail and generate a Validation Layers error.

`dstAllocationLocalOffset` is relative to the contents of given `dstAllocation`.
If you mean whole allocation, you should pass 0.
Do not pass allocation's offset within device memory block this parameter!
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCopyMemoryToAllocation(
    VmaAllocator VMA_NOT_NULL allocator,
    const void* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(size) pSrcHostPointer,
    VmaAllocation VMA_NOT_NULL dstAllocation,
    VkDeviceSize dstAllocationLocalOffset,
    VkDeviceSize size);

/** \brief Invalidates memory in the host caches if needed, maps the allocation temporarily if needed, and copies data from it to a specified host pointer.

\param allocator
\param srcAllocation   Handle to the allocation that becomes source of the copy.
\param srcAllocationLocalOffset  Offset within `srcAllocation` where to read copied data, in bytes.
\param pDstHostPointer Pointer to the host memory that become destination of the copy.
\param size            Number of bytes to copy.

This is a convenience function that allows to copy data from an allocation to a host pointer easily.
Same behavior can be achieved by calling vmaInvalidateAllocation(), vmaMapMemory(), `memcpy()`, vmaUnmapMemory().

This function should be called only for allocations created in a memory type that has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
and `VK_MEMORY_PROPERTY_HOST_CACHED_BIT` flag.
It can be ensured e.g. by using #VMA_MEMORY_USAGE_AUTO and #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
Otherwise, the function may fail and generate a Validation Layers error.
It may also work very slowly when reading from an uncached memory.

`srcAllocationLocalOffset` is relative to the contents of given `srcAllocation`.
If you mean whole allocation, you should pass 0.
Do not pass allocation's offset within device memory block as this parameter!
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCopyAllocationToMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL srcAllocation,
    VkDeviceSize srcAllocationLocalOffset,
    void* VMA_NOT_NULL VMA_LEN_IF_NOT_NULL(size) pDstHostPointer,
    VkDeviceSize size);

/** \brief Checks magic number in margins around all allocations in given memory types (in both default and custom pools) in search for corruptions.

\param allocator
\param memoryTypeBits Bit mask, where each bit set means that a memory type with that index should be checked.

Corruption detection is enabled only when `VMA_DEBUG_DETECT_CORRUPTION` macro is defined to nonzero,
`VMA_DEBUG_MARGIN` is defined to nonzero and only for memory types that are
`HOST_VISIBLE` and `HOST_COHERENT`. For more information, see [Corruption detection](@ref debugging_memory_usage_corruption_detection).

Possible return values:

- `VK_ERROR_FEATURE_NOT_PRESENT` - corruption detection is not enabled for any of specified memory types.
- `VK_SUCCESS` - corruption detection has been performed and succeeded.
- `VK_ERROR_UNKNOWN` - corruption detection has been performed and found memory corruptions around one of the allocations.
  `VMA_ASSERT` is also fired in that case.
- Other value: Error returned by Vulkan, e.g. memory mapping failure.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCheckCorruption(
    VmaAllocator VMA_NOT_NULL allocator,
    uint32_t memoryTypeBits);

/** \brief Begins defragmentation process.

\param allocator Allocator object.
\param pInfo Structure filled with parameters of defragmentation.
\param[out] pContext Context object that must be passed to vmaEndDefragmentation() to finish defragmentation.
\returns
- `VK_SUCCESS` if defragmentation can begin.
- `VK_ERROR_FEATURE_NOT_PRESENT` if defragmentation is not supported.

For more information about defragmentation, see documentation chapter:
[Defragmentation](@ref defragmentation).
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBeginDefragmentation(
    VmaAllocator VMA_NOT_NULL allocator,
    const VmaDefragmentationInfo* VMA_NOT_NULL pInfo,
    VmaDefragmentationContext VMA_NULLABLE* VMA_NOT_NULL pContext);

/** \brief Ends defragmentation process.

\param allocator Allocator object.
\param context Context object that has been created by vmaBeginDefragmentation().
\param[out] pStats Optional stats for the defragmentation. Can be null.

Use this function to finish defragmentation started by vmaBeginDefragmentation().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaEndDefragmentation(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaDefragmentationContext VMA_NOT_NULL context,
    VmaDefragmentationStats* VMA_NULLABLE pStats);

/** \brief Starts single defragmentation pass.

\param allocator Allocator object.
\param context Context object that has been created by vmaBeginDefragmentation().
\param[out] pPassInfo Computed information for current pass.
\returns
- `VK_SUCCESS` if no more moves are possible. Then you can omit call to vmaEndDefragmentationPass() and simply end whole defragmentation.
- `VK_INCOMPLETE` if there are pending moves returned in `pPassInfo`. You need to perform them, call vmaEndDefragmentationPass(),
  and then preferably try another pass with vmaBeginDefragmentationPass().
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBeginDefragmentationPass(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaDefragmentationContext VMA_NOT_NULL context,
    VmaDefragmentationPassMoveInfo* VMA_NOT_NULL pPassInfo);

/** \brief Ends single defragmentation pass.

\param allocator Allocator object.
\param context Context object that has been created by vmaBeginDefragmentation().
\param pPassInfo Computed information for current pass filled by vmaBeginDefragmentationPass() and possibly modified by you.

Returns `VK_SUCCESS` if no more moves are possible or `VK_INCOMPLETE` if more defragmentations are possible.

Ends incremental defragmentation pass and commits all defragmentation moves from `pPassInfo`.
After this call:

- Allocations at `pPassInfo[i].srcAllocation` that had `pPassInfo[i].operation ==` #VMA_DEFRAGMENTATION_MOVE_OPERATION_COPY
  (which is the default) will be pointing to the new destination place.
- Allocation at `pPassInfo[i].srcAllocation` that had `pPassInfo[i].operation ==` #VMA_DEFRAGMENTATION_MOVE_OPERATION_DESTROY
  will be freed.

If no more moves are possible you can end whole defragmentation.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaEndDefragmentationPass(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaDefragmentationContext VMA_NOT_NULL context,
    VmaDefragmentationPassMoveInfo* VMA_NOT_NULL pPassInfo);

/** \brief Binds buffer to allocation.

Binds specified buffer to region of memory represented by specified allocation.
Gets `VkDeviceMemory` handle and offset from the allocation.
If you want to create a buffer, allocate memory for it and bind them together separately,
you should use this function for binding instead of standard `vkBindBufferMemory()`,
because it ensures proper synchronization so that when a `VkDeviceMemory` object is used by multiple
allocations, calls to `vkBind*Memory()` or `vkMapMemory()` won't happen from multiple threads simultaneously
(which is illegal in Vulkan).

It is recommended to use function vmaCreateBuffer() instead of this one.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindBufferMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkBuffer VMA_NOT_NULL_NON_DISPATCHABLE buffer);

/** \brief Binds buffer to allocation with additional parameters.

\param allocator
\param allocation
\param allocationLocalOffset Additional offset to be added while binding, relative to the beginning of the `allocation`. Normally it should be 0.
\param buffer
\param pNext A chain of structures to be attached to `VkBindBufferMemoryInfoKHR` structure used internally. Normally it should be null.

This function is similar to vmaBindBufferMemory(), but it provides additional parameters.

If `pNext` is not null, #VmaAllocator object must have been created with #VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT flag
or with VmaAllocatorCreateInfo::vulkanApiVersion `>= VK_API_VERSION_1_1`. Otherwise the call fails.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindBufferMemory2(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize allocationLocalOffset,
    VkBuffer VMA_NOT_NULL_NON_DISPATCHABLE buffer,
    const void* VMA_NULLABLE VMA_EXTENDS_VK_STRUCT(VkBindBufferMemoryInfoKHR) pNext);

/** \brief Binds image to allocation.

Binds specified image to region of memory represented by specified allocation.
Gets `VkDeviceMemory` handle and offset from the allocation.
If you want to create an image, allocate memory for it and bind them together separately,
you should use this function for binding instead of standard `vkBindImageMemory()`,
because it ensures proper synchronization so that when a `VkDeviceMemory` object is used by multiple
allocations, calls to `vkBind*Memory()` or `vkMapMemory()` won't happen from multiple threads simultaneously
(which is illegal in Vulkan).

It is recommended to use function vmaCreateImage() instead of this one.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindImageMemory(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkImage VMA_NOT_NULL_NON_DISPATCHABLE image);

/** \brief Binds image to allocation with additional parameters.

\param allocator
\param allocation
\param allocationLocalOffset Additional offset to be added while binding, relative to the beginning of the `allocation`. Normally it should be 0.
\param image
\param pNext A chain of structures to be attached to `VkBindImageMemoryInfoKHR` structure used internally. Normally it should be null.

This function is similar to vmaBindImageMemory(), but it provides additional parameters.

If `pNext` is not null, #VmaAllocator object must have been created with #VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT flag
or with VmaAllocatorCreateInfo::vulkanApiVersion `>= VK_API_VERSION_1_1`. Otherwise the call fails.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaBindImageMemory2(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize allocationLocalOffset,
    VkImage VMA_NOT_NULL_NON_DISPATCHABLE image,
    const void* VMA_NULLABLE VMA_EXTENDS_VK_STRUCT(VkBindImageMemoryInfoKHR) pNext);

/** \brief Creates a new `VkBuffer`, allocates and binds memory for it.

\param allocator
\param pBufferCreateInfo
\param pAllocationCreateInfo
\param[out] pBuffer Buffer that was created.
\param[out] pAllocation Allocation that was created.
\param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function vmaGetAllocationInfo().

This function automatically:

-# Creates buffer.
-# Allocates appropriate memory for it.
-# Binds the buffer with the memory.

If any of these operations fail, buffer and allocation are not created,
returned value is negative error code, `*pBuffer` and `*pAllocation` are null.

If the function succeeded, you must destroy both buffer and allocation when you
no longer need them using either convenience function vmaDestroyBuffer() or
separately, using `vkDestroyBuffer()` and vmaFreeMemory().

If #VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT flag was used,
VK_KHR_dedicated_allocation extension is used internally to query driver whether
it requires or prefers the new buffer to have dedicated allocation. If yes,
and if dedicated allocation is possible
(#VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT is not used), it creates dedicated
allocation for this buffer, just like when using
#VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.

\note This function creates a new `VkBuffer`. Sub-allocation of parts of one large buffer,
although recommended as a good practice, is out of scope of this library and could be implemented
by the user as a higher-level logic on top of VMA.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateBuffer(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pBuffer,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/** \brief Creates a buffer with additional minimum alignment.

Similar to vmaCreateBuffer() but provides additional parameter `minAlignment` which allows to specify custom,
minimum alignment to be used when placing the buffer inside a larger memory block, which may be needed e.g.
for interop with OpenGL.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateBufferWithAlignment(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    VkDeviceSize minAlignment,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pBuffer,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/** \brief Creates a new `VkBuffer`, binds already created memory for it.

\param allocator
\param allocation Allocation that provides memory to be used for binding new buffer to it.
\param pBufferCreateInfo
\param[out] pBuffer Buffer that was created.

This function automatically:

-# Creates buffer.
-# Binds the buffer with the supplied memory.

If any of these operations fail, buffer is not created,
returned value is negative error code and `*pBuffer` is null.

If the function succeeded, you must destroy the buffer when you
no longer need it using `vkDestroyBuffer()`. If you want to also destroy the corresponding
allocation you can use convenience function vmaDestroyBuffer().

\note There is a new version of this function augmented with parameter `allocationLocalOffset` - see vmaCreateAliasingBuffer2().
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingBuffer(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pBuffer);

/** \brief Creates a new `VkBuffer`, binds already created memory for it.

\param allocator
\param allocation Allocation that provides memory to be used for binding new buffer to it.
\param allocationLocalOffset Additional offset to be added while binding, relative to the beginning of the allocation. Normally it should be 0.
\param pBufferCreateInfo
\param[out] pBuffer Buffer that was created.

This function automatically:

-# Creates buffer.
-# Binds the buffer with the supplied memory.

If any of these operations fail, buffer is not created,
returned value is negative error code and `*pBuffer` is null.

If the function succeeded, you must destroy the buffer when you
no longer need it using `vkDestroyBuffer()`. If you want to also destroy the corresponding
allocation you can use convenience function vmaDestroyBuffer().

\note This is a new version of the function augmented with parameter `allocationLocalOffset`.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingBuffer2(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize allocationLocalOffset,
    const VkBufferCreateInfo* VMA_NOT_NULL pBufferCreateInfo,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pBuffer);

/** \brief Destroys Vulkan buffer and frees allocated memory.

This is just a convenience function equivalent to:

\code
vkDestroyBuffer(device, buffer, allocationCallbacks);
vmaFreeMemory(allocator, allocation);
\endcode

It is safe to pass null as buffer and/or allocation.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaDestroyBuffer(
    VmaAllocator VMA_NOT_NULL allocator,
    VkBuffer VMA_NULLABLE_NON_DISPATCHABLE buffer,
    VmaAllocation VMA_NULLABLE allocation);

/// Function similar to vmaCreateBuffer().
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateImage(
    VmaAllocator VMA_NOT_NULL allocator,
    const VkImageCreateInfo* VMA_NOT_NULL pImageCreateInfo,
    const VmaAllocationCreateInfo* VMA_NOT_NULL pAllocationCreateInfo,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pImage,
    VmaAllocation VMA_NULLABLE* VMA_NOT_NULL pAllocation,
    VmaAllocationInfo* VMA_NULLABLE pAllocationInfo);

/// Function similar to vmaCreateAliasingBuffer() but for images.
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingImage(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    const VkImageCreateInfo* VMA_NOT_NULL pImageCreateInfo,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pImage);

/// Function similar to vmaCreateAliasingBuffer2() but for images.
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateAliasingImage2(
    VmaAllocator VMA_NOT_NULL allocator,
    VmaAllocation VMA_NOT_NULL allocation,
    VkDeviceSize allocationLocalOffset,
    const VkImageCreateInfo* VMA_NOT_NULL pImageCreateInfo,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pImage);

/** \brief Destroys Vulkan image and frees allocated memory.

This is just a convenience function equivalent to:

\code
vkDestroyImage(device, image, allocationCallbacks);
vmaFreeMemory(allocator, allocation);
\endcode

It is safe to pass null as image and/or allocation.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaDestroyImage(
    VmaAllocator VMA_NOT_NULL allocator,
    VkImage VMA_NULLABLE_NON_DISPATCHABLE image,
    VmaAllocation VMA_NULLABLE allocation);

/** @} */

/**
\addtogroup group_virtual
@{
*/

/** \brief Creates new #VmaVirtualBlock object.

\param pCreateInfo Parameters for creation.
\param[out] pVirtualBlock Returned virtual block object or `VMA_NULL` if creation failed.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaCreateVirtualBlock(
    const VmaVirtualBlockCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaVirtualBlock VMA_NULLABLE* VMA_NOT_NULL pVirtualBlock);

/** \brief Destroys #VmaVirtualBlock object.

Please note that you should consciously handle virtual allocations that could remain unfreed in the block.
You should either free them individually using vmaVirtualFree() or call vmaClearVirtualBlock()
if you are sure this is what you want. If you do neither, an assert is called.

If you keep pointers to some additional metadata associated with your virtual allocations in their `pUserData`,
don't forget to free them.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaDestroyVirtualBlock(
    VmaVirtualBlock VMA_NULLABLE virtualBlock);

/** \brief Returns true of the #VmaVirtualBlock is empty - contains 0 virtual allocations and has all its space available for new allocations.
*/
VMA_CALL_PRE VkBool32 VMA_CALL_POST vmaIsVirtualBlockEmpty(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock);

/** \brief Returns information about a specific virtual allocation within a virtual block, like its size and `pUserData` pointer.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetVirtualAllocationInfo(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaVirtualAllocation VMA_NOT_NULL_NON_DISPATCHABLE allocation, VmaVirtualAllocationInfo* VMA_NOT_NULL pVirtualAllocInfo);

/** \brief Allocates new virtual allocation inside given #VmaVirtualBlock.

If the allocation fails due to not enough free space available, `VK_ERROR_OUT_OF_DEVICE_MEMORY` is returned
(despite the function doesn't ever allocate actual GPU memory).
`pAllocation` is then set to `VK_NULL_HANDLE` and `pOffset`, if not null, it set to `UINT64_MAX`.

\param virtualBlock Virtual block
\param pCreateInfo Parameters for the allocation
\param[out] pAllocation Returned handle of the new allocation
\param[out] pOffset Returned offset of the new allocation. Optional, can be null.
*/
VMA_CALL_PRE VkResult VMA_CALL_POST vmaVirtualAllocate(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    const VmaVirtualAllocationCreateInfo* VMA_NOT_NULL pCreateInfo,
    VmaVirtualAllocation VMA_NULLABLE_NON_DISPATCHABLE* VMA_NOT_NULL pAllocation,
    VkDeviceSize* VMA_NULLABLE pOffset);

/** \brief Frees virtual allocation inside given #VmaVirtualBlock.

It is correct to call this function with `allocation == VK_NULL_HANDLE` - it does nothing.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaVirtualFree(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaVirtualAllocation VMA_NULLABLE_NON_DISPATCHABLE allocation);

/** \brief Frees all virtual allocations inside given #VmaVirtualBlock.

You must either call this function or free each virtual allocation individually with vmaVirtualFree()
before destroying a virtual block. Otherwise, an assert is called.

If you keep pointer to some additional metadata associated with your virtual allocation in its `pUserData`,
don't forget to free it as well.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaClearVirtualBlock(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock);

/** \brief Changes custom pointer associated with given virtual allocation.
*/
VMA_CALL_PRE void VMA_CALL_POST vmaSetVirtualAllocationUserData(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaVirtualAllocation VMA_NOT_NULL_NON_DISPATCHABLE allocation,
    void* VMA_NULLABLE pUserData);

/** \brief Calculates and returns statistics about virtual allocations and memory usage in given #VmaVirtualBlock.

This function is fast to call. For more detailed statistics, see vmaCalculateVirtualBlockStatistics().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaGetVirtualBlockStatistics(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaStatistics* VMA_NOT_NULL pStats);

/** \brief Calculates and returns detailed statistics about virtual allocations and memory usage in given #VmaVirtualBlock.

This function is slow to call. Use for debugging purposes.
For less detailed statistics, see vmaGetVirtualBlockStatistics().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaCalculateVirtualBlockStatistics(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    VmaDetailedStatistics* VMA_NOT_NULL pStats);

/** @} */

#if VMA_STATS_STRING_ENABLED
/**
\addtogroup group_stats
@{
*/

/** \brief Builds and returns a null-terminated string in JSON format with information about given #VmaVirtualBlock.
\param virtualBlock Virtual block.
\param[out] ppStatsString Returned string.
\param detailedMap Pass `VK_FALSE` to only obtain statistics as returned by vmaCalculateVirtualBlockStatistics(). Pass `VK_TRUE` to also obtain full list of allocations and free spaces.

Returned string must be freed using vmaFreeVirtualBlockStatsString().
*/
VMA_CALL_PRE void VMA_CALL_POST vmaBuildVirtualBlockStatsString(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    char* VMA_NULLABLE* VMA_NOT_NULL ppStatsString,
    VkBool32 detailedMap);

/// Frees a string returned by vmaBuildVirtualBlockStatsString().
VMA_CALL_PRE void VMA_CALL_POST vmaFreeVirtualBlockStatsString(
    VmaVirtualBlock VMA_NOT_NULL virtualBlock,
    char* VMA_NULLABLE pStatsString);

/** \brief Builds and returns statistics as a null-terminated string in JSON format.
\param allocator
\param[out] ppStatsString Must be freed using vmaFreeStatsString() function.
\param detailedMap
*/
VMA_CALL_PRE void VMA_CALL_POST vmaBuildStatsString(
    VmaAllocator VMA_NOT_NULL allocator,
    char* VMA_NULLABLE* VMA_NOT_NULL ppStatsString,
    VkBool32 detailedMap);

VMA_CALL_PRE void VMA_CALL_POST vmaFreeStatsString(
    VmaAllocator VMA_NOT_NULL allocator,
    char* VMA_NULLABLE pStatsString);

/** @} */

#endif // VMA_STATS_STRING_ENABLED

#endif // _VMA_FUNCTION_HEADERS

#ifdef __cplusplus
}
#endif

#endif // AMD_VULKAN_MEMORY_ALLOCATOR_H

/**
\page faq Frequently asked questions

<b>What is VMA?</b>

Vulkan(R) Memory Allocator (VMA) is a software library for developers who use the Vulkan graphics API in their code.
It is written in C++.

<b>What is the license of VMA?</b>

VMA is licensed under MIT, which means it is open source and free software.

<b>What is the purpose of VMA?</b>

VMA helps with handling one aspect of Vulkan usage, which is device memory management -
allocation of `VkDeviceMemory` objects, and creation of `VkBuffer` and `VkImage` objects.

<b>Do I need to use VMA?</b>

You don't need to, but it may be beneficial in many cases.
Vulkan is a complex and low-level API, so libraries like this that abstract certain aspects of the API
and bring them to a higher level are useful.
When developing any non-trivial Vulkan application, you likely need to use a memory allocator.
Using VMA can save time compared to implementing your own.

<b>When should I not use VMA?</b>

While VMA is useful for most applications that use the Vulkan API, there are cases
when it may be a better choice not to use it.
For example, if the application is very simple, e.g. serving as a sample or a learning exercise
to help you understand or teach others the basics of Vulkan,
and it creates only a small number of buffers and images, then including VMA may be an overkill.
Developing your own memory allocator may also be a good learning exercise.

<b>What are the benefits of using VMA?</b>

-# VMA helps in choosing the optimal memory type for your resource (buffer or image).
   In Vulkan, we have a two-level hierarchy of memory heaps and types with different flags,
   and each device can expose a different set of those.
   Implementing logic that would select the best memory type on each platform is a non-trivial task.
   VMA does that, expecting only a high-level description of the intended usage of your resource.
   For more information, see \subpage choosing_memory_type.
-# VMA allocates large blocks of `VkDeviceMemory` and sub-allocates parts of them for your resources.
   Allocating a new block of device memory may be a time-consuming operation.
   Some platforms also have a limit on the maximum number of those blocks (`VkPhysicalDeviceLimits::maxMemoryAllocationCount`)
   as low as 4096, so allocating a separate one for each resource is not an option.
   Sub-allocating parts of a memory block requires implementing an allocation algorithm,
   which is a non-trivial task.
   VMA does that, using an advanced and efficient algorithm that works well in various use cases.
-# VMA offers a simple API that allows creating buffers and textures within one function call.
   In Vulkan, the creation of a resource is a multi-step process.
   You need to create a `VkBuffer` or `VkImage`, ask it for memory requirements,
   allocate a `VkDeviceMemory` object, and finally bind the resource to the memory block.
   VMA does that automatically under a simple API within one function call: vmaCreateBuffer(), vmaCreateImage().

The library is doing much more under the hood.
For example, it respects limits like `bufferImageGranularity`, `nonCoherentAtomSize`,
and `VkMemoryDedicatedRequirements` automatically, so you don't need to think about it.

<b>Which version should I pick?</b>

You can just pick [the latest version from the "master" branch](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).
It is kept in a good shape most of the time, compiling and working correctly,
with no compatibility-breaking changes and no unfinished code.

If you want an even more stable version, you can pick
[the latest official release](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases).
Current code from the master branch is occasionally tagged as a release,
with [CHANGELOG](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/CHANGELOG.md)
carefully curated to enumerate all important changes since the previous version.

The library uses [Semantic Versioning](https://semver.org/),
which means versions that only differ in the patch number are forward and backward compatible
(e.g., only fixing some bugs), while versions that differ in the minor number are backward compatible
(e.g., only adding new functions to the API, but not removing or changing existing ones).

<b>How to integrate it with my code?</b>

VMA is an STB-style single-header C++ library.

You can pull the entire GitHub repository, e.g. using Git submodules.
The repository contains ancillary files like the Cmake script, Doxygen config file,
sample application, test suite, and others.
You can compile it as a library and link with your project.

However, a simpler way is taking the single file "include/vk_mem_alloc.h" and including it in your project.
This extensive file contains all you need: a copyright notice,
declarations of the public library interface (API), its internal implementation,
and even the documentation in form of Doxygen-style comments.

The "STB style" means not everything is implemented as inline functions in the header file.
You need to extract the internal implementation using a special macro.
This means that in every .cpp file where you need to use the library you should
`#include "vk_mem_alloc.h"` to include its public interface,
but additionally in exactly one .cpp file you should `#define VMA_IMPLEMENTATION`
before this `#include` to enable its internal implementation.
For more information, see [Project setup](@ref quick_start_project_setup).

<b>Does the library work with C or C++?</b>

The internal implementation of VMA is written in C++.
It is distributed in the source format, so you need a compiler supporting at least C++14 to build it.

However, the public interface of the library is written in C - using only enums, structs, and global functions,
in the same style as Vulkan, so you can use the library in the C code.

<b>I am not a fan of modern C++. Can I still use it?</b>

Very likely yes.
We acknowledge that many C++ developers, especially in the games industry,
do not appreciate all the latest features that the language has to offer.

- VMA doesn't throw or catch any C++ exceptions.
  It reports errors by returning a `VkResult` value instead, just like Vulkan.
  If you don't use exceptions in your project, your code is not exception-safe,
  or even if you disable exception handling in the compiler options, you can still use VMA.
- VMA doesn't use C++ run-time type information like `typeid` or `dynamic_cast`,
  so if you disable RTTI in the compiler options, you can still use the library.
- VMA uses only a limited subset of standard C and C++ library.
  It doesn't use STL containers like `std::vector`, `map`, or `string`,
  either in the public interface nor in the internal implementation.
  It implements its own containers instead.
- If you don't use the default heap memory allocator through `malloc/free` or `new/delete`
  but implement your own allocator instead, you can pass it to VMA and
  the library will use your functions for every dynamic heap allocation made internally,
  as well as passing it further to Vulkan functions. For details, see [Custom host memory allocator](@ref custom_memory_allocator).

<b>Is it available for other programming languages?</b>

VMA is a C++ library with C interface in similar style as Vulkan.
An object-oriented C++ wrapper or bindings to other programming languages are out of scope of this project,
but they are welcome as external projects.
Some of them are listed in [README.md, "See also" section](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator?tab=readme-ov-file#see-also),
including binding to C++, Python, Rust, and Haskell.
Before using any of them, please check if they are still maintained and updated to use a recent version of VMA.

<b>What platforms does it support?</b>

VMA relies only on Vulkan and some parts of the standard C and C++ library,
so it supports any platform where a C++ compiler and Vulkan are available.
It is developed mostly on Microsoft(R) Windows(R),
but it has been successfully used in Linux(R), MacOS, Android, and even FreeBSD and Raspberry Pi.

<b>Does it only work on AMD GPUs?</b>

No! While VMA is published by AMD, it works on any GPU that supports Vulkan,
whether a discrete PC graphics card, a processor integrated graphics, or a mobile SoC.
It doesn't give AMD GPUs any advantage over any other GPUs.

<b>What Vulkan versions and extensions are supported?</b>

VMA is updated to support the latest versions of Vulkan.
It currently supports Vulkan up to 1.4.
The library also supports older versions down to the first release of Vulkan 1.0.
Defining a higher minimum version support would help simplify the code,
but we acknowledge that developers on some platforms like Android still use older versions,
so the support is provided for all of them.

Among many extensions available for Vulkan, only a few interact with memory management.
VMA can automatically take advantage of them. Some of them are:
VK_EXT_memory_budget, VK_EXT_memory_priority, VK_KHR_external_memory_win32, and VK_KHR_maintenance*
extensions that are later promoted to the new versions of the core Vulkan API.

To use them, it is your responsibility to validate if they are available on the current system and if so,
enable them while creating the Vulkan device object.
You also need to pass appropriate #VmaAllocatorCreateFlagBits to inform VMA that they are enabled.
Then, the library will automatically take advantage of them.
For more information and the full list of supported extensions, see [Enabling extensions](@ref quick_start_initialization_enabling_extensions).

<b>Does it support other graphics APIs, like Microsoft DirectX(R) 12?</b>

No, but we offer an equivalent library for DirectX 12:
[D3D12 Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator).
It uses the same core allocation algorithm.
It also shares many features with VMA, like the support for custom pools and virtual allocator.
However, it is not identical in terms of the features supported.
Its API also looks different, because while the interface of VMA is similar in style to Vulkan,
the interface of D3D12MA is similar to DirectX 12.

<b>Is the library lightweight?</b>

It depends on how you define it.
VMA is implemented with high-performance and real-time applications like video games in mind.
The CPU performance overhead of using this library is low.
It uses a high-quality allocation algorithm called Two-Level Segregated Fit (TLSF),
which in most cases can find a free place for a new allocation in few steps.
The library also doesn't perform too many CPU heap allocations.
In many cases, the allocation happens with 0 new CPU heap allocations performed by the library.
Even the creation of a #VmaAllocation object doesn't typically feature an CPU allocation,
because these objects are returned out of a dedicated memory pool.

On the other hand, however, VMA needs some extra memory and extra time
to maintain the metadata about the occupied and free regions of the memory blocks,
and the algorithms and data structures used must be generic enough to work well in most cases.
If you develop your program for a very resource-constrained platform,
a custom allocator simpler than VMA may be a better choice.

<b>Does it have a documentation?</b>

Yes! VMA comes with full documentation of all elements of the API (functions, structures, enums),
as well as many generic chapters that provide an introduction,
describe core concepts of the library, good practices, etc.
The entire documentation is written in form of code comments inside "vk_mem_alloc.h", in Doxygen format.
You can access it in multiple ways:

- Browsable online: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/
- Local HTML pages available after you clone the repository and open file "docs/html/index.html".
- You can rebuild the documentation in HTML or some other format from the source code using Doxygen.
  Configuration file "Doxyfile" is part of the repository.
- Finally, you can just read the comments preceding declarations of any public functions of the library.

<b>Is it a mature project?</b>

Yes! The library is in development since June 2017, has over 1000 commits, over 400 issue tickets
and pull requests (most of them resolved), and over 70 contributors.
It is distributed together with Vulkan SDK.
It is used by many software projects, including some large and popular ones like Qt or Blender,
as well as some AAA games.
According to the [LunarG 2024 Ecosystem Survey](https://www.lunarg.com/2024-ecosystem-survey-progress-report-released/),
it is used by over 50% of Vulkan developers.

<b>How can I contribute to the project?</b>

If you have an idea for improvement or a feature request,
you can go to [the library repository](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
and create an Issue ticket, describing your idea.
You can also implement it yourself by forking the repository, making changes to the code,
and creating a Pull request.

If you want to ask a question, you can also create a ticket the same way.
Before doing this, please make sure you read the relevant part of the Vulkan specification and VMA documentation,
where you may find the answers to your question.

If you want to report a suspected bug, you can also create a ticket the same way.
Before doing this, please put some effort into the investigation of whether the bug is really
in the library and not in your code or in the Vulkan implementation (the GPU driver) on your platform:

- Enable Vulkan validation layer and make sure it is free from any errors.
- Make sure `VMA_ASSERT` is defined to an implementation that can report a failure and not ignore it.
- Try making your allocation using pure Vulkan functions rather than VMA and see if the bug persists.

<b>I found some compilation warnings. How can we fix them?</b>

Seeing compiler warnings may be annoying to some developers,
but it is a design decision to not fix all of them.
Due to the nature of the C++ language, certain preprocessor macros can make some variables unused,
function parameters unreferenced, or conditional expressions constant in some configurations.
The code of this library should not be bigger or more complicated just to silence these warnings.
It is recommended to disable such warnings instead.
For more information, see [Features not supported](@ref general_considerations_features_not_supported).

However, if you observe a warning that is really dangerous, e.g.,
about an implicit conversion from a larger to a smaller integer type, please report it and it will be fixed ASAP.


\page quick_start Quick start

\section quick_start_project_setup Project setup

Vulkan Memory Allocator comes in form of a "stb-style" single header file.
While you can pull the entire repository e.g. as Git module, there is also Cmake script provided,
you don't need to build it as a separate library project.
You can add file "vk_mem_alloc.h" directly to your project and submit it to code repository next to your other source files.

"Single header" doesn't mean that everything is contained in C/C++ declarations,
like it tends to be in case of inline functions or C++ templates.
It means that implementation is bundled with interface in a single file and needs to be extracted using preprocessor macro.
If you don't do it properly, it will result in linker errors.

To do it properly:

-# Include "vk_mem_alloc.h" file in each CPP file where you want to use the library.
   This includes declarations of all members of the library.
-# In exactly one CPP file define following macro before this include.
   It enables also internal definitions.

\code
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
\endcode

It may be a good idea to create dedicated CPP file just for this purpose, e.g. "VmaUsage.cpp".

This library includes header `<vulkan/vulkan.h>`, which in turn
includes `<windows.h>` on Windows. If you need some specific macros defined
before including these headers (like `WIN32_LEAN_AND_MEAN` or
`WINVER` for Windows, `VK_USE_PLATFORM_WIN32_KHR` for Vulkan), you must define
them before every `#include` of this library.
It may be a good idea to create a dedicate header file for this purpose, e.g. "VmaUsage.h",
that will be included in other source files instead of VMA header directly.

This library is written in C++, but has C-compatible interface.
Thus, you can include and use "vk_mem_alloc.h" in C or C++ code, but full
implementation with `VMA_IMPLEMENTATION` macro must be compiled as C++, NOT as C.
Some features of C++14 are used and required. Features of C++20 are used optionally when available.
Some headers of standard C and C++ library are used, but STL containers, RTTI, or C++ exceptions are not used.


\section quick_start_initialization Initialization

VMA offers library interface in a style similar to Vulkan, with object handles like #VmaAllocation,
structures describing parameters of objects to be created like #VmaAllocationCreateInfo,
and errors codes returned from functions using `VkResult` type.

The first and the main object that needs to be created is #VmaAllocator.
It represents the initialization of the entire library.
Only one such object should be created per `VkDevice`.
You should create it at program startup, after `VkDevice` was created, and before any device memory allocator needs to be made.
It must be destroyed before `VkDevice` is destroyed.

At program startup:

-# Initialize Vulkan to have `VkInstance`, `VkPhysicalDevice`, `VkDevice` object.
-# Fill VmaAllocatorCreateInfo structure and call vmaCreateAllocator() to create #VmaAllocator object.

Only members `physicalDevice`, `device`, `instance` are required.
However, you should inform the library which Vulkan version do you use by setting
VmaAllocatorCreateInfo::vulkanApiVersion and which extensions did you enable
by setting VmaAllocatorCreateInfo::flags.
Otherwise, VMA would use only features of Vulkan 1.0 core with no extensions.
See below for details.

\subsection quick_start_initialization_selecting_vulkan_version Selecting Vulkan version

VMA supports Vulkan version down to 1.0, for backward compatibility.
If you want to use higher version, you need to inform the library about it.
This is a two-step process.

<b>Step 1: Compile time.</b> By default, VMA compiles with code supporting the highest
Vulkan version found in the included `<vulkan/vulkan.h>` that is also supported by the library.
If this is OK, you don't need to do anything.
However, if you want to compile VMA as if only some lower Vulkan version was available,
define macro `VMA_VULKAN_VERSION` before every `#include "vk_mem_alloc.h"`.
It should have decimal numeric value in form of ABBBCCC, where A = major, BBB = minor, CCC = patch Vulkan version.
For example, to compile against Vulkan 1.2:

\code
#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#include "vk_mem_alloc.h"
\endcode

<b>Step 2: Runtime.</b> Even when compiled with higher Vulkan version available,
VMA can use only features of a lower version, which is configurable during creation of the #VmaAllocator object.
By default, only Vulkan 1.0 is used.
To initialize the allocator with support for higher Vulkan version, you need to set member
VmaAllocatorCreateInfo::vulkanApiVersion to an appropriate value, e.g. using constants like `VK_API_VERSION_1_2`.
See code sample below.

\subsection quick_start_initialization_importing_vulkan_functions Importing Vulkan functions

You may need to configure importing Vulkan functions. There are 4 ways to do this:

-# **If you link with Vulkan static library** (e.g. "vulkan-1.lib" on Windows):
   - You don't need to do anything.
   - VMA will use these, as macro `VMA_STATIC_VULKAN_FUNCTIONS` is defined to 1 by default.
-# **If you want VMA to fetch pointers to Vulkan functions dynamically** using `vkGetInstanceProcAddr`,
   `vkGetDeviceProcAddr` (this is the option presented in the example below):
   - Define `VMA_STATIC_VULKAN_FUNCTIONS` to 0, `VMA_DYNAMIC_VULKAN_FUNCTIONS` to 1.
   - Provide pointers to these two functions via VmaVulkanFunctions::vkGetInstanceProcAddr,
     VmaVulkanFunctions::vkGetDeviceProcAddr.
   - The library will fetch pointers to all other functions it needs internally.
-# **If you fetch pointers to all Vulkan functions in a custom way**:
   - Define `VMA_STATIC_VULKAN_FUNCTIONS` and `VMA_DYNAMIC_VULKAN_FUNCTIONS` to 0.
   - Pass these pointers via structure #VmaVulkanFunctions.
-# **If you use [volk library](https://github.com/zeux/volk)**:
   - Define `VMA_STATIC_VULKAN_FUNCTIONS` and `VMA_DYNAMIC_VULKAN_FUNCTIONS` to 0.
   - Use function vmaImportVulkanFunctionsFromVolk() to fill in the structure #VmaVulkanFunctions.
     For more information, see the description of this function.

\subsection quick_start_initialization_enabling_extensions Enabling extensions

VMA can automatically use following Vulkan extensions.
If you found them available on the selected physical device and you enabled them
while creating `VkInstance` / `VkDevice` object, inform VMA about their availability
by setting appropriate flags in VmaAllocatorCreateInfo::flags.

Vulkan extension              | VMA flag
------------------------------|-----------------------------------------------------
VK_KHR_dedicated_allocation   | #VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT
VK_KHR_bind_memory2           | #VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT
VK_KHR_maintenance4           | #VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT
VK_KHR_maintenance5           | #VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT
VK_EXT_memory_budget          | #VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT
VK_KHR_buffer_device_address  | #VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
VK_EXT_memory_priority        | #VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT
VK_AMD_device_coherent_memory | #VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT
VK_KHR_external_memory_win32  | #VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT

Example with fetching pointers to Vulkan functions dynamically:

\code
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

...

VmaVulkanFunctions vulkanFunctions = {};
vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

VmaAllocatorCreateInfo allocatorCreateInfo = {};
allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
allocatorCreateInfo.physicalDevice = physicalDevice;
allocatorCreateInfo.device = device;
allocatorCreateInfo.instance = instance;
allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

VmaAllocator allocator;
vmaCreateAllocator(&allocatorCreateInfo, &allocator);

// Entire program...

// At the end, don't forget to:
vmaDestroyAllocator(allocator);
\endcode


\subsection quick_start_initialization_other_config Other configuration options

There are additional configuration options available through preprocessor macros that you can define
before including VMA header and through parameters passed in #VmaAllocatorCreateInfo.
They include a possibility to use your own callbacks for host memory allocations (`VkAllocationCallbacks`),
callbacks for device memory allocations (instead of `vkAllocateMemory`, `vkFreeMemory`),
or your custom `VMA_ASSERT` macro, among others.
For more information, see: @ref configuration.


\section quick_start_resource_allocation Resource allocation

When you want to create a buffer or image:

-# Fill `VkBufferCreateInfo` / `VkImageCreateInfo` structure.
-# Fill VmaAllocationCreateInfo structure.
-# Call vmaCreateBuffer() / vmaCreateImage() to get `VkBuffer`/`VkImage` with memory
   already allocated and bound to it, plus #VmaAllocation objects that represents its underlying memory.

\code
VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufferInfo.size = 65536;
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
\endcode

Don't forget to destroy your buffer and allocation objects when no longer needed:

\code
vmaDestroyBuffer(allocator, buffer, allocation);
\endcode

If you need to map the buffer, you must set flag
#VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
in VmaAllocationCreateInfo::flags.
There are many additional parameters that can control the choice of memory type to be used for the allocation
and other features.
For more information, see documentation chapters: @ref choosing_memory_type, @ref memory_mapping.


\page choosing_memory_type Choosing memory type

Physical devices in Vulkan support various combinations of memory heaps and
types. Help with choosing correct and optimal memory type for your specific
resource is one of the key features of this library. You can use it by filling
appropriate members of VmaAllocationCreateInfo structure, as described below.
You can also combine multiple methods.

-# If you just want to find memory type index that meets your requirements, you
   can use function: vmaFindMemoryTypeIndexForBufferInfo(),
   vmaFindMemoryTypeIndexForImageInfo(), vmaFindMemoryTypeIndex().
-# If you want to allocate a region of device memory without association with any
   specific image or buffer, you can use function vmaAllocateMemory(). Usage of
   this function is not recommended and usually not needed.
   vmaAllocateMemoryPages() function is also provided for creating multiple allocations at once,
   which may be useful for sparse binding.
-# If you already have a buffer or an image created, you want to allocate memory
   for it and then you will bind it yourself, you can use function
   vmaAllocateMemoryForBuffer(), vmaAllocateMemoryForImage().
   For binding you should use functions: vmaBindBufferMemory(), vmaBindImageMemory()
   or their extended versions: vmaBindBufferMemory2(), vmaBindImageMemory2().
-# If you want to create a buffer or an image, allocate memory for it, and bind
   them together, all in one call, you can use function vmaCreateBuffer(),
   vmaCreateImage().
   <b>This is the easiest and recommended way to use this library!</b>

When using 3. or 4., the library internally queries Vulkan for memory types
supported for that buffer or image (function `vkGetBufferMemoryRequirements()`)
and uses only one of these types.

If no memory type can be found that meets all the requirements, these functions
return `VK_ERROR_FEATURE_NOT_PRESENT`.

You can leave VmaAllocationCreateInfo structure completely filled with zeros.
It means no requirements are specified for memory type.
It is valid, although not very useful.

\section choosing_memory_type_usage Usage

The easiest way to specify memory requirements is to fill member
VmaAllocationCreateInfo::usage using one of the values of enum #VmaMemoryUsage.
It defines high level, common usage types.
Since version 3 of the library, it is recommended to use #VMA_MEMORY_USAGE_AUTO to let it select best memory type for your resource automatically.

For example, if you want to create a uniform buffer that will be filled using
transfer only once or infrequently and then used for rendering every frame as a uniform buffer, you can
do it using following code. The buffer will most likely end up in a memory type with
`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` to be fast to access by the GPU device.

\code
VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufferInfo.size = 65536;
bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
\endcode

If you have a preference for putting the resource in GPU (device) memory or CPU (host) memory
on systems with discrete graphics card that have the memories separate, you can use
#VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE or #VMA_MEMORY_USAGE_AUTO_PREFER_HOST.

When using `VMA_MEMORY_USAGE_AUTO*` while you want to map the allocated memory,
you also need to specify one of the host access flags:
#VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
This will help the library decide about preferred memory type to ensure it has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
so you can map it.

For example, a staging buffer that will be filled via mapped pointer and then
used as a source of transfer to the buffer described previously can be created like this.
It will likely end up in a memory type that is `HOST_VISIBLE` and `HOST_COHERENT`
but not `HOST_CACHED` (meaning uncached, write-combined) and not `DEVICE_LOCAL` (meaning system RAM).

\code
VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
stagingBufferInfo.size = 65536;
stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo stagingAllocInfo = {};
stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer stagingBuffer;
VmaAllocation stagingAllocation;
vmaCreateBuffer(allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr);
\endcode

For more examples of creating different kinds of resources, see chapter \ref usage_patterns.
See also: @ref memory_mapping.

Usage values `VMA_MEMORY_USAGE_AUTO*` are legal to use only when the library knows
about the resource being created by having `VkBufferCreateInfo` / `VkImageCreateInfo` passed,
so they work with functions like: vmaCreateBuffer(), vmaCreateImage(), vmaFindMemoryTypeIndexForBufferInfo() etc.
If you allocate raw memory using function vmaAllocateMemory(), you have to use other means of selecting
memory type, as described below.

\note
Old usage values (`VMA_MEMORY_USAGE_GPU_ONLY`, `VMA_MEMORY_USAGE_CPU_ONLY`,
`VMA_MEMORY_USAGE_CPU_TO_GPU`, `VMA_MEMORY_USAGE_GPU_TO_CPU`, `VMA_MEMORY_USAGE_CPU_COPY`)
are still available and work same way as in previous versions of the library
for backward compatibility, but they are deprecated.

\section choosing_memory_type_required_preferred_flags Required and preferred flags

You can specify more detailed requirements by filling members
VmaAllocationCreateInfo::requiredFlags and VmaAllocationCreateInfo::preferredFlags
with a combination of bits from enum `VkMemoryPropertyFlags`. For example,
if you want to create a buffer that will be persistently mapped on host (so it
must be `HOST_VISIBLE`) and preferably will also be `HOST_COHERENT` and `HOST_CACHED`,
use following code:

\code
VmaAllocationCreateInfo allocInfo = {};
allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
\endcode

A memory type is chosen that has all the required flags and as many preferred
flags set as possible.

Value passed in VmaAllocationCreateInfo::usage is internally converted to a set of required and preferred flags,
plus some extra "magic" (heuristics).

\section choosing_memory_type_explicit_memory_types Explicit memory types

If you inspected memory types available on the physical device and <b>you have
a preference for memory types that you want to use</b>, you can fill member
VmaAllocationCreateInfo::memoryTypeBits. It is a bit mask, where each bit set
means that a memory type with that index is allowed to be used for the
allocation. Special value 0, just like `UINT32_MAX`, means there are no
restrictions to memory type index.

Please note that this member is NOT just a memory type index.
Still you can use it to choose just one, specific memory type.
For example, if you already determined that your buffer should be created in
memory type 2, use following code:

\code
uint32_t memoryTypeIndex = 2;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.memoryTypeBits = 1U << memoryTypeIndex;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
\endcode

You can also use this parameter to <b>exclude some memory types</b>.
If you inspect memory heaps and types available on the current physical device and
you determine that for some reason you don't want to use a specific memory type for the allocation,
you can enable automatic memory type selection but exclude certain memory type or types
by setting all bits of `memoryTypeBits` to 1 except the ones you choose.

\code
// ...
uint32_t excludedMemoryTypeIndex = 2;
VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.memoryTypeBits = ~(1U << excludedMemoryTypeIndex);
// ...
\endcode


\section choosing_memory_type_custom_memory_pools Custom memory pools

If you allocate from custom memory pool, all the ways of specifying memory
requirements described above are not applicable and the aforementioned members
of VmaAllocationCreateInfo structure are ignored. Memory type is selected
explicitly when creating the pool and then used to make all the allocations from
that pool. For further details, see \ref custom_memory_pools.

\section choosing_memory_type_dedicated_allocations Dedicated allocations

Memory for allocations is reserved out of larger block of `VkDeviceMemory`
allocated from Vulkan internally. That is the main feature of this whole library.
You can still request a separate memory block to be created for an allocation,
just like you would do in a trivial solution without using any allocator.
In that case, a buffer or image is always bound to that memory at offset 0.
This is called a "dedicated allocation".
You can explicitly request it by using flag #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
The library can also internally decide to use dedicated allocation in some cases, e.g.:

- When the size of the allocation is large.
- When [VK_KHR_dedicated_allocation](@ref vk_khr_dedicated_allocation) extension is enabled
  and it reports that dedicated allocation is required or recommended for the resource.
- When allocation of next big memory block fails due to not enough device memory,
  but allocation with the exact requested size succeeds.


\page memory_mapping Memory mapping

To "map memory" in Vulkan means to obtain a CPU pointer to `VkDeviceMemory`,
to be able to read from it or write to it in CPU code.
Mapping is possible only of memory allocated from a memory type that has
`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` flag.
Functions `vkMapMemory()`, `vkUnmapMemory()` are designed for this purpose.
You can use them directly with memory allocated by this library,
but it is not recommended because of following issue:
Mapping the same `VkDeviceMemory` block multiple times is illegal - only one mapping at a time is allowed.
This includes mapping disjoint regions. Mapping is not reference-counted internally by Vulkan.
It is also not thread-safe.
Because of this, Vulkan Memory Allocator provides following facilities:

\note If you want to be able to map an allocation, you need to specify one of the flags
#VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
in VmaAllocationCreateInfo::flags. These flags are required for an allocation to be mappable
when using #VMA_MEMORY_USAGE_AUTO or other `VMA_MEMORY_USAGE_AUTO*` enum values.
For other usage values they are ignored and every such allocation made in `HOST_VISIBLE` memory type is mappable,
but these flags can still be used for consistency.

\section memory_mapping_copy_functions Copy functions

The easiest way to copy data from a host pointer to an allocation is to use convenience function vmaCopyMemoryToAllocation().
It automatically maps the Vulkan memory temporarily (if not already mapped), performs `memcpy`,
and calls `vkFlushMappedMemoryRanges` (if required - if memory type is not `HOST_COHERENT`).

It is also the safest one, because using `memcpy` avoids a risk of accidentally introducing memory reads
(e.g. by doing `pMappedVectors[i] += v`), which may be very slow on memory types that are not `HOST_CACHED`.

\code
struct ConstantBuffer
{
    ...
};
ConstantBuffer constantBufferData = ...

VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = sizeof(ConstantBuffer);
bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer buf;
VmaAllocation alloc;
vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, nullptr);

vmaCopyMemoryToAllocation(allocator, &constantBufferData, alloc, 0, sizeof(ConstantBuffer));
\endcode

Copy in the other direction - from an allocation to a host pointer can be performed the same way using function vmaCopyAllocationToMemory().

\section memory_mapping_mapping_functions Mapping functions

The library provides following functions for mapping of a specific allocation: vmaMapMemory(), vmaUnmapMemory().
They are safer and more convenient to use than standard Vulkan functions.
You can map an allocation multiple times simultaneously - mapping is reference-counted internally.
You can also map different allocations simultaneously regardless of whether they use the same `VkDeviceMemory` block.
The way it is implemented is that the library always maps entire memory block, not just region of the allocation.
For further details, see description of vmaMapMemory() function.
Example:

\code
// Having these objects initialized:
struct ConstantBuffer
{
    ...
};
ConstantBuffer constantBufferData = ...

VmaAllocator allocator = ...
VkBuffer constantBuffer = ...
VmaAllocation constantBufferAllocation = ...

// You can map and fill your buffer using following code:

void* mappedData;
vmaMapMemory(allocator, constantBufferAllocation, &mappedData);
memcpy(mappedData, &constantBufferData, sizeof(constantBufferData));
vmaUnmapMemory(allocator, constantBufferAllocation);
\endcode

When mapping, you may see a warning from Vulkan validation layer similar to this one:

<i>Mapping an image with layout VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL can result in undefined behavior if this memory is used by the device. Only GENERAL or PREINITIALIZED should be used.</i>

It happens because the library maps entire `VkDeviceMemory` block, where different
types of images and buffers may end up together, especially on GPUs with unified memory like Intel.
You can safely ignore it if you are sure you access only memory of the intended
object that you wanted to map.


\section memory_mapping_persistently_mapped_memory Persistently mapped memory

Keeping your memory persistently mapped is generally OK in Vulkan.
You don't need to unmap it before using its data on the GPU.
The library provides a special feature designed for that:
Allocations made with #VMA_ALLOCATION_CREATE_MAPPED_BIT flag set in
VmaAllocationCreateInfo::flags stay mapped all the time,
so you can just access CPU pointer to it any time
without a need to call any "map" or "unmap" function.
Example:

\code
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = sizeof(ConstantBuffer);
bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
    VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buf;
VmaAllocation alloc;
VmaAllocationInfo allocInfo;
vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, &allocInfo);

// Buffer is already mapped. You can access its memory.
memcpy(allocInfo.pMappedData, &constantBufferData, sizeof(constantBufferData));
\endcode

\note #VMA_ALLOCATION_CREATE_MAPPED_BIT by itself doesn't guarantee that the allocation will end up
in a mappable memory type.
For this, you need to also specify #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or
#VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
#VMA_ALLOCATION_CREATE_MAPPED_BIT only guarantees that if the memory is `HOST_VISIBLE`, the allocation will be mapped on creation.
For an example of how to make use of this fact, see section \ref usage_patterns_advanced_data_uploading.

\section memory_mapping_cache_control Cache flush and invalidate

Memory in Vulkan doesn't need to be unmapped before using it on GPU,
but unless a memory types has `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` flag set,
you need to manually **invalidate** cache before reading of mapped pointer
and **flush** cache after writing to mapped pointer.
Map/unmap operations don't do that automatically.
Vulkan provides following functions for this purpose `vkFlushMappedMemoryRanges()`,
`vkInvalidateMappedMemoryRanges()`, but this library provides more convenient
functions that refer to given allocation object: vmaFlushAllocation(),
vmaInvalidateAllocation(),
or multiple objects at once: vmaFlushAllocations(), vmaInvalidateAllocations().

Regions of memory specified for flush/invalidate must be aligned to
`VkPhysicalDeviceLimits::nonCoherentAtomSize`. This is automatically ensured by the library.
In any memory type that is `HOST_VISIBLE` but not `HOST_COHERENT`, all allocations
within blocks are aligned to this value, so their offsets are always multiply of
`nonCoherentAtomSize` and two different allocations never share same "line" of this size.

Also, Windows drivers from all 3 PC GPU vendors (AMD, Intel, NVIDIA)
currently provide `HOST_COHERENT` flag on all memory types that are
`HOST_VISIBLE`, so on PC you may not need to bother.


\page staying_within_budget Staying within budget

When developing a graphics-intensive game or program, it is important to avoid allocating
more GPU memory than it is physically available. When the memory is over-committed,
various bad things can happen, depending on the specific GPU, graphics driver, and
operating system:

- It may just work without any problems.
- The application may slow down because some memory blocks are moved to system RAM
  and the GPU has to access them through PCI Express bus.
- A new allocation may take very long time to complete, even few seconds, and possibly
  freeze entire system.
- The new allocation may fail with `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
- It may even result in GPU crash (TDR), observed as `VK_ERROR_DEVICE_LOST`
  returned somewhere later.

\section staying_within_budget_querying_for_budget Querying for budget

To query for current memory usage and available budget, use function vmaGetHeapBudgets().
Returned structure #VmaBudget contains quantities expressed in bytes, per Vulkan memory heap.

Please note that this function returns different information and works faster than
vmaCalculateStatistics(). vmaGetHeapBudgets() can be called every frame or even before every
allocation, while vmaCalculateStatistics() is intended to be used rarely,
only to obtain statistical information, e.g. for debugging purposes.

It is recommended to use <b>VK_EXT_memory_budget</b> device extension to obtain information
about the budget from Vulkan device. VMA is able to use this extension automatically.
When not enabled, the allocator behaves same way, but then it estimates current usage
and available budget based on its internal information and Vulkan memory heap sizes,
which may be less precise. In order to use this extension:

1. Make sure extensions VK_EXT_memory_budget and VK_KHR_get_physical_device_properties2
   required by it are available and enable them. Please note that the first is a device
   extension and the second is instance extension!
2. Use flag #VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT when creating #VmaAllocator object.
3. Make sure to call vmaSetCurrentFrameIndex() every frame. Budget is queried from
   Vulkan inside of it to avoid overhead of querying it with every allocation.

\section staying_within_budget_controlling_memory_usage Controlling memory usage

There are many ways in which you can try to stay within the budget.

First, when making new allocation requires allocating a new memory block, the library
tries not to exceed the budget automatically. If a block with default recommended size
(e.g. 256 MB) would go over budget, a smaller block is allocated, possibly even
dedicated memory for just this resource.

If the size of the requested resource plus current memory usage is more than the
budget, by default the library still tries to create it, leaving it to the Vulkan
implementation whether the allocation succeeds or fails. You can change this behavior
by using #VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT flag. With it, the allocation is
not made if it would exceed the budget or if the budget is already exceeded.
VMA then tries to make the allocation from the next eligible Vulkan memory type.
If all of them fail, the call then fails with `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
Example usage pattern may be to pass the #VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT flag
when creating resources that are not essential for the application (e.g. the texture
of a specific object) and not to pass it when creating critically important resources
(e.g. render targets).

On AMD graphics cards there is a custom vendor extension available: <b>VK_AMD_memory_overallocation_behavior</b>
that allows to control the behavior of the Vulkan implementation in out-of-memory cases -
whether it should fail with an error code or still allow the allocation.
Usage of this extension involves only passing extra structure on Vulkan device creation,
so it is out of scope of this library.

Finally, you can also use #VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT flag to make sure
a new allocation is created only when it fits inside one of the existing memory blocks.
If it would require to allocate a new block, if fails instead with `VK_ERROR_OUT_OF_DEVICE_MEMORY`.
This also ensures that the function call is very fast because it never goes to Vulkan
to obtain a new block.

\note Creating \ref custom_memory_pools with VmaPoolCreateInfo::minBlockCount
set to more than 0 will currently try to allocate memory blocks without checking whether they
fit within budget.


\page resource_aliasing Resource aliasing (overlap)

New explicit graphics APIs (Vulkan and Direct3D 12), thanks to manual memory
management, give an opportunity to alias (overlap) multiple resources in the
same region of memory - a feature not available in the old APIs (Direct3D 11, OpenGL).
It can be useful to save video memory, but it must be used with caution.

For example, if you know the flow of your whole render frame in advance, you
are going to use some intermediate textures or buffers only during a small range of render passes,
and you know these ranges don't overlap in time, you can bind these resources to
the same place in memory, even if they have completely different parameters (width, height, format etc.).

![Resource aliasing (overlap)](../gfx/Aliasing.png)

Such scenario is possible using VMA, but you need to create your images manually.
Then you need to calculate parameters of an allocation to be made using formula:

- allocation size = max(size of each image)
- allocation alignment = max(alignment of each image)
- allocation memoryTypeBits = bitwise AND(memoryTypeBits of each image)

Following example shows two different images bound to the same place in memory,
allocated to fit largest of them.

\code
// A 512x512 texture to be sampled.
VkImageCreateInfo img1CreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
img1CreateInfo.imageType = VK_IMAGE_TYPE_2D;
img1CreateInfo.extent.width = 512;
img1CreateInfo.extent.height = 512;
img1CreateInfo.extent.depth = 1;
img1CreateInfo.mipLevels = 10;
img1CreateInfo.arrayLayers = 1;
img1CreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
img1CreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
img1CreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
img1CreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
img1CreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

// A full screen texture to be used as color attachment.
VkImageCreateInfo img2CreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
img2CreateInfo.imageType = VK_IMAGE_TYPE_2D;
img2CreateInfo.extent.width = 1920;
img2CreateInfo.extent.height = 1080;
img2CreateInfo.extent.depth = 1;
img2CreateInfo.mipLevels = 1;
img2CreateInfo.arrayLayers = 1;
img2CreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
img2CreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
img2CreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
img2CreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
img2CreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VkImage img1;
res = vkCreateImage(device, &img1CreateInfo, nullptr, &img1);
VkImage img2;
res = vkCreateImage(device, &img2CreateInfo, nullptr, &img2);

VkMemoryRequirements img1MemReq;
vkGetImageMemoryRequirements(device, img1, &img1MemReq);
VkMemoryRequirements img2MemReq;
vkGetImageMemoryRequirements(device, img2, &img2MemReq);

VkMemoryRequirements finalMemReq = {};
finalMemReq.size = std::max(img1MemReq.size, img2MemReq.size);
finalMemReq.alignment = std::max(img1MemReq.alignment, img2MemReq.alignment);
finalMemReq.memoryTypeBits = img1MemReq.memoryTypeBits & img2MemReq.memoryTypeBits;
// Validate if(finalMemReq.memoryTypeBits != 0)

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

VmaAllocation alloc;
res = vmaAllocateMemory(allocator, &finalMemReq, &allocCreateInfo, &alloc, nullptr);

res = vmaBindImageMemory(allocator, alloc, img1);
res = vmaBindImageMemory(allocator, alloc, img2);

// You can use img1, img2 here, but not at the same time!

vmaFreeMemory(allocator, alloc);
vkDestroyImage(allocator, img2, nullptr);
vkDestroyImage(allocator, img1, nullptr);
\endcode

VMA also provides convenience functions that create a buffer or image and bind it to memory
represented by an existing #VmaAllocation:
vmaCreateAliasingBuffer(), vmaCreateAliasingBuffer2(),
vmaCreateAliasingImage(), vmaCreateAliasingImage2().
Versions with "2" offer additional parameter `allocationLocalOffset`.

Remember that using resources that alias in memory requires proper synchronization.
You need to issue a memory barrier to make sure commands that use `img1` and `img2`
don't overlap on GPU timeline.
You also need to treat a resource after aliasing as uninitialized - containing garbage data.
For example, if you use `img1` and then want to use `img2`, you need to issue
an image memory barrier for `img2` with `oldLayout` = `VK_IMAGE_LAYOUT_UNDEFINED`.

Additional considerations:

- Vulkan also allows to interpret contents of memory between aliasing resources consistently in some cases.
See chapter 11.8. "Memory Aliasing" of Vulkan specification or `VK_IMAGE_CREATE_ALIAS_BIT` flag.
- You can create more complex layout where different images and buffers are bound
at different offsets inside one large allocation. For example, one can imagine
a big texture used in some render passes, aliasing with a set of many small buffers
used between in some further passes. To bind a resource at non-zero offset in an allocation,
use vmaBindBufferMemory2() / vmaBindImageMemory2().
- Before allocating memory for the resources you want to alias, check `memoryTypeBits`
returned in memory requirements of each resource to make sure the bits overlap.
Some GPUs may expose multiple memory types suitable e.g. only for buffers or
images with `COLOR_ATTACHMENT` usage, so the sets of memory types supported by your
resources may be disjoint. Aliasing them is not possible in that case.


\page custom_memory_pools Custom memory pools

A memory pool contains a number of `VkDeviceMemory` blocks.
The library automatically creates and manages default pool for each memory type available on the device.
Default memory pool automatically grows in size.
Size of allocated blocks is also variable and managed automatically.
You are using default pools whenever you leave VmaAllocationCreateInfo::pool = null.

You can create custom pool and allocate memory out of it.
It can be useful if you want to:

- Keep certain kind of allocations separate from others.
- Enforce particular, fixed size of Vulkan memory blocks.
- Limit maximum amount of Vulkan memory allocated for that pool.
- Reserve minimum or fixed amount of Vulkan memory always preallocated for that pool.
- Use extra parameters for a set of your allocations that are available in #VmaPoolCreateInfo but not in
  #VmaAllocationCreateInfo - e.g., custom minimum alignment, custom `pNext` chain.
- Perform defragmentation on a specific subset of your allocations.

To use custom memory pools:

-# Fill VmaPoolCreateInfo structure.
-# Call vmaCreatePool() to obtain #VmaPool handle.
-# When making an allocation, set VmaAllocationCreateInfo::pool to this handle.
   You don't need to specify any other parameters of this structure, like `usage`.

Example:

\code
// Find memoryTypeIndex for the pool.
VkBufferCreateInfo sampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
sampleBufCreateInfo.size = 0x10000; // Doesn't matter.
sampleBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo sampleAllocCreateInfo = {};
sampleAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

uint32_t memTypeIndex;
VkResult res = vmaFindMemoryTypeIndexForBufferInfo(allocator,
    &sampleBufCreateInfo, &sampleAllocCreateInfo, &memTypeIndex);
// Check res...

// Create a pool that can have at most 2 blocks, 128 MiB each.
VmaPoolCreateInfo poolCreateInfo = {};
poolCreateInfo.memoryTypeIndex = memTypeIndex;
poolCreateInfo.blockSize = 128ULL * 1024 * 1024;
poolCreateInfo.maxBlockCount = 2;

VmaPool pool;
res = vmaCreatePool(allocator, &poolCreateInfo, &pool);
// Check res...

// Allocate a buffer out of it.
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = 1024;
bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.pool = pool;

VkBuffer buf;
VmaAllocation alloc;
res = vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, nullptr);
// Check res...
\endcode

You have to free all allocations made from this pool before destroying it.

\code
vmaDestroyBuffer(allocator, buf, alloc);
vmaDestroyPool(allocator, pool);
\endcode

New versions of this library support creating dedicated allocations in custom pools.
It is supported only when VmaPoolCreateInfo::blockSize = 0.
To use this feature, set VmaAllocationCreateInfo::pool to the pointer to your custom pool and
VmaAllocationCreateInfo::flags to #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.


\section custom_memory_pools_MemTypeIndex Choosing memory type index

When creating a pool, you must explicitly specify memory type index.
To find the one suitable for your buffers or images, you can use helper functions
vmaFindMemoryTypeIndexForBufferInfo(), vmaFindMemoryTypeIndexForImageInfo().
You need to provide structures with example parameters of buffers or images
that you are going to create in that pool.

\code
VkBufferCreateInfo exampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
exampleBufCreateInfo.size = 1024; // Doesn't matter
exampleBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

uint32_t memTypeIndex;
vmaFindMemoryTypeIndexForBufferInfo(allocator, &exampleBufCreateInfo, &allocCreateInfo, &memTypeIndex);

VmaPoolCreateInfo poolCreateInfo = {};
poolCreateInfo.memoryTypeIndex = memTypeIndex;
// ...
\endcode

When creating buffers/images allocated in that pool, provide following parameters:

- `VkBufferCreateInfo`: Prefer to pass same parameters as above.
  Otherwise you risk creating resources in a memory type that is not suitable for them, which may result in undefined behavior.
  Using different `VK_BUFFER_USAGE_` flags may work, but you shouldn't create images in a pool intended for buffers
  or the other way around.
- VmaAllocationCreateInfo: You don't need to pass same parameters. Fill only `pool` member.
  Other members are ignored anyway.


\section custom_memory_pools_when_not_use When not to use custom pools

Custom pools are commonly overused by VMA users.
While it may feel natural to keep some logical groups of resources separate in memory,
in most cases it does more harm than good.
Using custom pool shouldn't be your first choice.
Instead, please make all allocations from default pools first and only use custom pools
if you can prove and measure that it is beneficial in some way,
e.g. it results in lower memory usage, better performance, etc.

Using custom pools has disadvantages:

- Each pool has its own collection of `VkDeviceMemory` blocks.
  Some of them may be partially or even completely empty.
  Spreading allocations across multiple pools increases the amount of wasted (allocated but unbound) memory.
- You must manually choose specific memory type to be used by a custom pool (set as VmaPoolCreateInfo::memoryTypeIndex).
  When using default pools, best memory type for each of your allocations can be selected automatically
  using a carefully design algorithm that works across all kinds of GPUs.
- If an allocation from a custom pool at specific memory type fails, entire allocation operation returns failure.
  When using default pools, VMA tries another compatible memory type.
- If you set VmaPoolCreateInfo::blockSize != 0, each memory block has the same size,
  while default pools start from small blocks and only allocate next blocks larger and larger
  up to the preferred block size.

Many of the common concerns can be addressed in a different way than using custom pools:

- If you want to keep your allocations of certain size (small versus large) or certain lifetime (transient versus long lived)
  separate, you likely don't need to.
  VMA uses a high quality allocation algorithm that manages memory well in various cases.
  Please measure and check if using custom pools provides a benefit.
- If you want to keep your images and buffers separate, you don't need to.
  VMA respects `bufferImageGranularity` limit automatically.
- If you want to keep your mapped and not mapped allocations separate, you don't need to.
  VMA respects `nonCoherentAtomSize` limit automatically.
  It also maps only those `VkDeviceMemory` blocks that need to map any allocation.
  It even tries to keep mappable and non-mappable allocations in separate blocks to minimize the amount of mapped memory.
- If you want to choose a custom size for the default memory block, you can set it globally instead
  using VmaAllocatorCreateInfo::preferredLargeHeapBlockSize.
- If you want to select specific memory type for your allocation,
  you can set VmaAllocationCreateInfo::memoryTypeBits to `(1U << myMemoryTypeIndex)` instead.
- If you need to create a buffer with certain minimum alignment, you can still do it
  using default pools with dedicated function vmaCreateBufferWithAlignment().


\section linear_algorithm Linear allocation algorithm

Each Vulkan memory block managed by this library has accompanying metadata that
keeps track of used and unused regions. By default, the metadata structure and
algorithm tries to find best place for new allocations among free regions to
optimize memory usage. This way you can allocate and free objects in any order.

![Default allocation algorithm](../gfx/Linear_allocator_1_algo_default.png)

Sometimes there is a need to use simpler, linear allocation algorithm. You can
create custom pool that uses such algorithm by adding flag
#VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT to VmaPoolCreateInfo::flags while creating
#VmaPool object. Then an alternative metadata management is used. It always
creates new allocations after last one and doesn't reuse free regions after
allocations freed in the middle. It results in better allocation performance and
less memory consumed by metadata.

![Linear allocation algorithm](../gfx/Linear_allocator_2_algo_linear.png)

With this one flag, you can create a custom pool that can be used in many ways:
free-at-once, stack, double stack, and ring buffer. See below for details.
You don't need to specify explicitly which of these options you are going to use - it is detected automatically.

\subsection linear_algorithm_free_at_once Free-at-once

In a pool that uses linear algorithm, you still need to free all the allocations
individually, e.g. by using vmaFreeMemory() or vmaDestroyBuffer(). You can free
them in any order. New allocations are always made after last one - free space
in the middle is not reused. However, when you release all the allocation and
the pool becomes empty, allocation starts from the beginning again. This way you
can use linear algorithm to speed up creation of allocations that you are going
to release all at once.

![Free-at-once](../gfx/Linear_allocator_3_free_at_once.png)

This mode is also available for pools created with VmaPoolCreateInfo::maxBlockCount
value that allows multiple memory blocks.

\subsection linear_algorithm_stack Stack

When you free an allocation that was created last, its space can be reused.
Thanks to this, if you always release allocations in the order opposite to their
creation (LIFO - Last In First Out), you can achieve behavior of a stack.

![Stack](../gfx/Linear_allocator_4_stack.png)

This mode is also available for pools created with VmaPoolCreateInfo::maxBlockCount
value that allows multiple memory blocks.

\subsection linear_algorithm_double_stack Double stack

The space reserved by a custom pool with linear algorithm may be used by two
stacks:

- First, default one, growing up from offset 0.
- Second, "upper" one, growing down from the end towards lower offsets.

To make allocation from the upper stack, add flag #VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT
to VmaAllocationCreateInfo::flags.

![Double stack](../gfx/Linear_allocator_7_double_stack.png)

Double stack is available only in pools with one memory block -
VmaPoolCreateInfo::maxBlockCount must be 1. Otherwise behavior is undefined.

When the two stacks' ends meet so there is not enough space between them for a
new allocation, such allocation fails with usual
`VK_ERROR_OUT_OF_DEVICE_MEMORY` error.

\subsection linear_algorithm_ring_buffer Ring buffer

When you free some allocations from the beginning and there is not enough free space
for a new one at the end of a pool, allocator's "cursor" wraps around to the
beginning and starts allocation there. Thanks to this, if you always release
allocations in the same order as you created them (FIFO - First In First Out),
you can achieve behavior of a ring buffer / queue.

![Ring buffer](../gfx/Linear_allocator_5_ring_buffer.png)

Ring buffer is available only in pools with one memory block -
VmaPoolCreateInfo::maxBlockCount must be 1. Otherwise behavior is undefined.

\note \ref defragmentation is not supported in custom pools created with #VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT.


\page defragmentation Defragmentation

Interleaved allocations and deallocations of many objects of varying size can
cause fragmentation over time, which can lead to a situation where the library is unable
to find a continuous range of free memory for a new allocation despite there is
enough free space, just scattered across many small free ranges between existing
allocations.

To mitigate this problem, you can use defragmentation feature.
It doesn't happen automatically though and needs your cooperation,
because VMA is a low level library that only allocates memory.
It cannot recreate buffers and images in a new place as it doesn't remember the contents of `VkBufferCreateInfo` / `VkImageCreateInfo` structures.
It cannot copy their contents as it doesn't record any commands to a command buffer.

Example:

\code
VmaDefragmentationInfo defragInfo = {};
defragInfo.pool = myPool;
defragInfo.flags = VMA_DEFRAGMENTATION_FLAG_ALGORITHM_FAST_BIT;

VmaDefragmentationContext defragCtx;
VkResult res = vmaBeginDefragmentation(allocator, &defragInfo, &defragCtx);
// Check res...

for(;;)
{
    VmaDefragmentationPassMoveInfo pass;
    res = vmaBeginDefragmentationPass(allocator, defragCtx, &pass);
    if(res == VK_SUCCESS)
        break;
    else if(res != VK_INCOMPLETE)
        // Handle error...

    for(uint32_t i = 0; i < pass.moveCount; ++i)
    {
        // Inspect pass.pMoves[i].srcAllocation, identify what buffer/image it represents.
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator, pass.pMoves[i].srcAllocation, &allocInfo);
        MyEngineResourceData* resData = (MyEngineResourceData*)allocInfo.pUserData;

        // Recreate and bind this buffer/image at: pass.pMoves[i].dstMemory, pass.pMoves[i].dstOffset.
        VkImageCreateInfo imgCreateInfo = ...
        VkImage newImg;
        res = vkCreateImage(device, &imgCreateInfo, nullptr, &newImg);
        // Check res...
        res = vmaBindImageMemory(allocator, pass.pMoves[i].dstTmpAllocation, newImg);
        // Check res...

        // Issue a vkCmdCopyBuffer/vkCmdCopyImage to copy its content to the new place.
        vkCmdCopyImage(cmdBuf, resData->img, ..., newImg, ...);
    }

    // Make sure the copy commands finished executing.
    vkWaitForFences(...);

    // Destroy old buffers/images bound with pass.pMoves[i].srcAllocation.
    for(uint32_t i = 0; i < pass.moveCount; ++i)
    {
        // ...
        vkDestroyImage(device, resData->img, nullptr);
    }

    // Update appropriate descriptors to point to the new places...

    res = vmaEndDefragmentationPass(allocator, defragCtx, &pass);
    if(res == VK_SUCCESS)
        break;
    else if(res != VK_INCOMPLETE)
        // Handle error...
}

vmaEndDefragmentation(allocator, defragCtx, nullptr);
\endcode

Although functions like vmaCreateBuffer(), vmaCreateImage(), vmaDestroyBuffer(), vmaDestroyImage()
create/destroy an allocation and a buffer/image at once, these are just a shortcut for
creating the resource, allocating memory, and binding them together.
Defragmentation works on memory allocations only. You must handle the rest manually.
Defragmentation is an iterative process that should repreat "passes" as long as related functions
return `VK_INCOMPLETE` not `VK_SUCCESS`.
In each pass:

1. vmaBeginDefragmentationPass() function call:
   - Calculates and returns the list of allocations to be moved in this pass.
     Note this can be a time-consuming process.
   - Reserves destination memory for them by creating temporary destination allocations
     that you can query for their `VkDeviceMemory` + offset using vmaGetAllocationInfo().
2. Inside the pass, **you should**:
   - Inspect the returned list of allocations to be moved.
   - Create new buffers/images and bind them at the returned destination temporary allocations.
   - Copy data from source to destination resources if necessary.
   - Destroy the source buffers/images, but NOT their allocations.
3. vmaEndDefragmentationPass() function call:
   - Frees the source memory reserved for the allocations that are moved.
   - Modifies source #VmaAllocation objects that are moved to point to the destination reserved memory.
   - Frees `VkDeviceMemory` blocks that became empty.

Unlike in previous iterations of the defragmentation API, there is no list of "movable" allocations passed as a parameter.
Defragmentation algorithm tries to move all suitable allocations.
You can, however, refuse to move some of them inside a defragmentation pass, by setting
`pass.pMoves[i].operation` to #VMA_DEFRAGMENTATION_MOVE_OPERATION_IGNORE.
This is not recommended and may result in suboptimal packing of the allocations after defragmentation.
If you cannot ensure any allocation can be moved, it is better to keep movable allocations separate in a custom pool.

Inside a pass, for each allocation that should be moved:

- You should copy its data from the source to the destination place by calling e.g. `vkCmdCopyBuffer()`, `vkCmdCopyImage()`.
  - You need to make sure these commands finished executing before destroying the source buffers/images and before calling vmaEndDefragmentationPass().
- If a resource doesn't contain any meaningful data, e.g. it is a transient color attachment image to be cleared,
  filled, and used temporarily in each rendering frame, you can just recreate this image
  without copying its data.
- If the resource is in `HOST_VISIBLE` and `HOST_CACHED` memory, you can copy its data on the CPU
  using `memcpy()`.
- If you cannot move the allocation, you can set `pass.pMoves[i].operation` to #VMA_DEFRAGMENTATION_MOVE_OPERATION_IGNORE.
  This will cancel the move.
  - vmaEndDefragmentationPass() will then free the destination memory
    not the source memory of the allocation, leaving it unchanged.
- If you decide the allocation is unimportant and can be destroyed instead of moved (e.g. it wasn't used for long time),
  you can set `pass.pMoves[i].operation` to #VMA_DEFRAGMENTATION_MOVE_OPERATION_DESTROY.
  - vmaEndDefragmentationPass() will then free both source and destination memory, and will destroy the source #VmaAllocation object.

You can defragment a specific custom pool by setting VmaDefragmentationInfo::pool
(like in the example above) or all the default pools by setting this member to null.

Defragmentation is always performed in each pool separately.
Allocations are never moved between different Vulkan memory types.
The size of the destination memory reserved for a moved allocation is the same as the original one.
Alignment of an allocation as it was determined using `vkGetBufferMemoryRequirements()` etc. is also respected after defragmentation.
Buffers/images should be recreated with the same `VkBufferCreateInfo` / `VkImageCreateInfo` parameters as the original ones.

You can perform the defragmentation incrementally to limit the number of allocations and bytes to be moved
in each pass, e.g. to call it in sync with render frames and not to experience too big hitches.
See members: VmaDefragmentationInfo::maxBytesPerPass, VmaDefragmentationInfo::maxAllocationsPerPass.

It is also safe to perform the defragmentation asynchronously to render frames and other Vulkan and VMA
usage, possibly from multiple threads, with the exception that allocations
returned in VmaDefragmentationPassMoveInfo::pMoves shouldn't be destroyed until the defragmentation pass is ended.

<b>Mapping</b> is preserved on allocations that are moved during defragmentation.
Whether through #VMA_ALLOCATION_CREATE_MAPPED_BIT or vmaMapMemory(), the allocations
are mapped at their new place. Of course, pointer to the mapped data changes, so it needs to be queried
using VmaAllocationInfo::pMappedData.

\note Defragmentation is not supported in custom pools created with #VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT.


\page statistics Statistics

This library contains several functions that return information about its internal state,
especially the amount of memory allocated from Vulkan.

\section statistics_numeric_statistics Numeric statistics

If you need to obtain basic statistics about memory usage per heap, together with current budget,
you can call function vmaGetHeapBudgets() and inspect structure #VmaBudget.
This is useful to keep track of memory usage and stay within budget
(see also \ref staying_within_budget).
Example:

\code
uint32_t heapIndex = ...

VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

printf("My heap currently has %u allocations taking %llu B,\n",
    budgets[heapIndex].statistics.allocationCount,
    budgets[heapIndex].statistics.allocationBytes);
printf("allocated out of %u Vulkan device memory blocks taking %llu B,\n",
    budgets[heapIndex].statistics.blockCount,
    budgets[heapIndex].statistics.blockBytes);
printf("Vulkan reports total usage %llu B with budget %llu B.\n",
    budgets[heapIndex].usage,
    budgets[heapIndex].budget);
\endcode

You can query for more detailed statistics per memory heap, type, and totals,
including minimum and maximum allocation size and unused range size,
by calling function vmaCalculateStatistics() and inspecting structure #VmaTotalStatistics.
This function is slower though, as it has to traverse all the internal data structures,
so it should be used only for debugging purposes.

You can query for statistics of a custom pool using function vmaGetPoolStatistics()
or vmaCalculatePoolStatistics().

You can query for information about a specific allocation using function vmaGetAllocationInfo().
It fill structure #VmaAllocationInfo.

\section statistics_json_dump JSON dump

You can dump internal state of the allocator to a string in JSON format using function vmaBuildStatsString().
The result is guaranteed to be correct JSON.
It uses ANSI encoding.
Any strings provided by user (see [Allocation names](@ref allocation_names))
are copied as-is and properly escaped for JSON, so if they use UTF-8, ISO-8859-2 or any other encoding,
this JSON string can be treated as using this encoding.
It must be freed using function vmaFreeStatsString().

The format of this JSON string is not part of official documentation of the library,
but it will not change in backward-incompatible way without increasing library major version number
and appropriate mention in changelog.

The JSON string contains all the data that can be obtained using vmaCalculateStatistics().
It can also contain detailed map of allocated memory blocks and their regions -
free and occupied by allocations.
This allows e.g. to visualize the memory or assess fragmentation.


\page allocation_annotation Allocation names and user data

\section allocation_user_data Allocation user data

You can annotate allocations with your own information, e.g. for debugging purposes.
To do that, fill VmaAllocationCreateInfo::pUserData field when creating
an allocation. It is an opaque `void*` pointer. You can use it e.g. as a pointer,
some handle, index, key, ordinal number or any other value that would associate
the allocation with your custom metadata.
It is useful to identify appropriate data structures in your engine given #VmaAllocation,
e.g. when doing \ref defragmentation.

\code
VkBufferCreateInfo bufCreateInfo = ...

MyBufferMetadata* pMetadata = CreateBufferMetadata();

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.pUserData = pMetadata;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buffer, &allocation, nullptr);
\endcode

The pointer may be later retrieved as VmaAllocationInfo::pUserData:

\code
VmaAllocationInfo allocInfo;
vmaGetAllocationInfo(allocator, allocation, &allocInfo);
MyBufferMetadata* pMetadata = (MyBufferMetadata*)allocInfo.pUserData;
\endcode

It can also be changed using function vmaSetAllocationUserData().

Values of (non-zero) allocations' `pUserData` are printed in JSON report created by
vmaBuildStatsString() in hexadecimal form.

\section allocation_names Allocation names

An allocation can also carry a null-terminated string, giving a name to the allocation.
To set it, call vmaSetAllocationName().
The library creates internal copy of the string, so the pointer you pass doesn't need
to be valid for whole lifetime of the allocation. You can free it after the call.

\code
std::string imageName = "Texture: ";
imageName += fileName;
vmaSetAllocationName(allocator, allocation, imageName.c_str());
\endcode

The string can be later retrieved by inspecting VmaAllocationInfo::pName.
It is also printed in JSON report created by vmaBuildStatsString().

\note Setting string name to VMA allocation doesn't automatically set it to the Vulkan buffer or image created with it.
You must do it manually using an extension like VK_EXT_debug_utils, which is independent of this library.


\page virtual_allocator Virtual allocator

As an extra feature, the core allocation algorithm of the library is exposed through a simple and convenient API of "virtual allocator".
It doesn't allocate any real GPU memory. It just keeps track of used and free regions of a "virtual block".
You can use it to allocate your own memory or other objects, even completely unrelated to Vulkan.
A common use case is sub-allocation of pieces of one large GPU buffer.

\section virtual_allocator_creating_virtual_block Creating virtual block

To use this functionality, there is no main "allocator" object.
You don't need to have #VmaAllocator object created.
All you need to do is to create a separate #VmaVirtualBlock object for each block of memory you want to be managed by the allocator:

-# Fill in #VmaVirtualBlockCreateInfo structure.
-# Call vmaCreateVirtualBlock(). Get new #VmaVirtualBlock object.

Example:

\code
VmaVirtualBlockCreateInfo blockCreateInfo = {};
blockCreateInfo.size = 1048576; // 1 MB

VmaVirtualBlock block;
VkResult res = vmaCreateVirtualBlock(&blockCreateInfo, &block);
\endcode

\section virtual_allocator_making_virtual_allocations Making virtual allocations

#VmaVirtualBlock object contains internal data structure that keeps track of free and occupied regions
using the same code as the main Vulkan memory allocator.
Similarly to #VmaAllocation for standard GPU allocations, there is #VmaVirtualAllocation type
that represents an opaque handle to an allocation within the virtual block.

In order to make such allocation:

-# Fill in #VmaVirtualAllocationCreateInfo structure.
-# Call vmaVirtualAllocate(). Get new #VmaVirtualAllocation object that represents the allocation.
   You can also receive `VkDeviceSize offset` that was assigned to the allocation.

Example:

\code
VmaVirtualAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.size = 4096; // 4 KB

VmaVirtualAllocation alloc;
VkDeviceSize offset;
res = vmaVirtualAllocate(block, &allocCreateInfo, &alloc, &offset);
if(res == VK_SUCCESS)
{
    // Use the 4 KB of your memory starting at offset.
}
else
{
    // Allocation failed - no space for it could be found. Handle this error!
}
\endcode

\section virtual_allocator_deallocation Deallocation

When no longer needed, an allocation can be freed by calling vmaVirtualFree().
You can only pass to this function an allocation that was previously returned by vmaVirtualAllocate()
called for the same #VmaVirtualBlock.

When whole block is no longer needed, the block object can be released by calling vmaDestroyVirtualBlock().
All allocations must be freed before the block is destroyed, which is checked internally by an assert.
However, if you don't want to call vmaVirtualFree() for each allocation, you can use vmaClearVirtualBlock() to free them all at once -
a feature not available in normal Vulkan memory allocator. Example:

\code
vmaVirtualFree(block, alloc);
vmaDestroyVirtualBlock(block);
\endcode

\section virtual_allocator_allocation_parameters Allocation parameters

You can attach a custom pointer to each allocation by using vmaSetVirtualAllocationUserData().
Its default value is null.
It can be used to store any data that needs to be associated with that allocation - e.g. an index, a handle, or a pointer to some
larger data structure containing more information. Example:

\code
struct CustomAllocData
{
    std::string m_AllocName;
};
CustomAllocData* allocData = new CustomAllocData();
allocData->m_AllocName = "My allocation 1";
vmaSetVirtualAllocationUserData(block, alloc, allocData);
\endcode

The pointer can later be fetched, along with allocation offset and size, by passing the allocation handle to function
vmaGetVirtualAllocationInfo() and inspecting returned structure #VmaVirtualAllocationInfo.
If you allocated a new object to be used as the custom pointer, don't forget to delete that object before freeing the allocation!
Example:

\code
VmaVirtualAllocationInfo allocInfo;
vmaGetVirtualAllocationInfo(block, alloc, &allocInfo);
delete (CustomAllocData*)allocInfo.pUserData;

vmaVirtualFree(block, alloc);
\endcode

\section virtual_allocator_alignment_and_units Alignment and units

It feels natural to express sizes and offsets in bytes.
If an offset of an allocation needs to be aligned to a multiply of some number (e.g. 4 bytes), you can fill optional member
VmaVirtualAllocationCreateInfo::alignment to request it. Example:

\code
VmaVirtualAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.size = 4096; // 4 KB
allocCreateInfo.alignment = 4; // Returned offset must be a multiply of 4 B

VmaVirtualAllocation alloc;
res = vmaVirtualAllocate(block, &allocCreateInfo, &alloc, nullptr);
\endcode

Alignments of different allocations made from one block may vary.
However, if all alignments and sizes are always multiply of some size e.g. 4 B or `sizeof(MyDataStruct)`,
you can express all sizes, alignments, and offsets in multiples of that size instead of individual bytes.
It might be more convenient, but you need to make sure to use this new unit consistently in all the places:

- VmaVirtualBlockCreateInfo::size
- VmaVirtualAllocationCreateInfo::size and VmaVirtualAllocationCreateInfo::alignment
- Using offset returned by vmaVirtualAllocate() or in VmaVirtualAllocationInfo::offset

\section virtual_allocator_statistics Statistics

You can obtain statistics of a virtual block using vmaGetVirtualBlockStatistics()
(to get brief statistics that are fast to calculate)
or vmaCalculateVirtualBlockStatistics() (to get more detailed statistics, slower to calculate).
The functions fill structures #VmaStatistics, #VmaDetailedStatistics respectively - same as used by the normal Vulkan memory allocator.
Example:

\code
VmaStatistics stats;
vmaGetVirtualBlockStatistics(block, &stats);
printf("My virtual block has %llu bytes used by %u virtual allocations\n",
    stats.allocationBytes, stats.allocationCount);
\endcode

You can also request a full list of allocations and free regions as a string in JSON format by calling
vmaBuildVirtualBlockStatsString().
Returned string must be later freed using vmaFreeVirtualBlockStatsString().
The format of this string differs from the one returned by the main Vulkan allocator, but it is similar.

\section virtual_allocator_additional_considerations Additional considerations

The "virtual allocator" functionality is implemented on a level of individual memory blocks.
Keeping track of a whole collection of blocks, allocating new ones when out of free space,
deleting empty ones, and deciding which one to try first for a new allocation must be implemented by the user.

Alternative allocation algorithms are supported, just like in custom pools of the real GPU memory.
See enum #VmaVirtualBlockCreateFlagBits to learn how to specify them (e.g. #VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT).
You can find their description in chapter \ref custom_memory_pools.
Allocation strategies are also supported.
See enum #VmaVirtualAllocationCreateFlagBits to learn how to specify them (e.g. #VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT).

Following features are supported only by the allocator of the real GPU memory and not by virtual allocations:
buffer-image granularity, `VMA_DEBUG_MARGIN`, `VMA_MIN_ALIGNMENT`.


\page debugging_memory_usage Debugging incorrect memory usage

If you suspect a bug with memory usage, like usage of uninitialized memory or
memory being overwritten out of bounds of an allocation,
you can use debug features of this library to verify this.

\section debugging_memory_usage_initialization Memory initialization

If you experience a bug with incorrect and nondeterministic data in your program and you suspect uninitialized memory to be used,
you can enable automatic memory initialization to verify this.
To do it, define macro `VMA_DEBUG_INITIALIZE_ALLOCATIONS` to 1.

\code
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#include "vk_mem_alloc.h"
\endcode

It makes memory of new allocations initialized to bit pattern `0xDCDCDCDC`.
Before an allocation is destroyed, its memory is filled with bit pattern `0xEFEFEFEF`.
Memory is automatically mapped and unmapped if necessary.

If you find these values while debugging your program, good chances are that you incorrectly
read Vulkan memory that is allocated but not initialized, or already freed, respectively.

Memory initialization works only with memory types that are `HOST_VISIBLE` and with allocations that can be mapped.
It works also with dedicated allocations.

\section debugging_memory_usage_margins Margins

By default, allocations are laid out in memory blocks next to each other if possible
(considering required alignment, `bufferImageGranularity`, and `nonCoherentAtomSize`).

![Allocations without margin](../gfx/Margins_1.png)

Define macro `VMA_DEBUG_MARGIN` to some non-zero value (e.g. 16) to enforce specified
number of bytes as a margin after every allocation.

\code
#define VMA_DEBUG_MARGIN 16
#include "vk_mem_alloc.h"
\endcode

![Allocations with margin](../gfx/Margins_2.png)

If your bug goes away after enabling margins, it means it may be caused by memory
being overwritten outside of allocation boundaries. It is not 100% certain though.
Change in application behavior may also be caused by different order and distribution
of allocations across memory blocks after margins are applied.

Margins work with all types of memory.

Margin is applied only to allocations made out of memory blocks and not to dedicated
allocations, which have their own memory block of specific size.
It is thus not applied to allocations made using #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT flag
or those automatically decided to put into dedicated allocations, e.g. due to its
large size or recommended by VK_KHR_dedicated_allocation extension.

Margins appear in [JSON dump](@ref statistics_json_dump) as part of free space.

Note that enabling margins increases memory usage and fragmentation.

Margins do not apply to \ref virtual_allocator.

\section debugging_memory_usage_corruption_detection Corruption detection

You can additionally define macro `VMA_DEBUG_DETECT_CORRUPTION` to 1 to enable validation
of contents of the margins.

\code
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#include "vk_mem_alloc.h"
\endcode

When this feature is enabled, number of bytes specified as `VMA_DEBUG_MARGIN`
(it must be multiply of 4) after every allocation is filled with a magic number.
This idea is also know as "canary".
Memory is automatically mapped and unmapped if necessary.

This number is validated automatically when the allocation is destroyed.
If it is not equal to the expected value, `VMA_ASSERT()` is executed.
It clearly means that either CPU or GPU overwritten the memory outside of boundaries of the allocation,
which indicates a serious bug.

You can also explicitly request checking margins of all allocations in all memory blocks
that belong to specified memory types by using function vmaCheckCorruption(),
or in memory blocks that belong to specified custom pool, by using function
vmaCheckPoolCorruption().

Margin validation (corruption detection) works only for memory types that are
`HOST_VISIBLE` and `HOST_COHERENT`.


\section debugging_memory_usage_leak_detection Leak detection features

At allocation and allocator destruction time VMA checks for unfreed and unmapped blocks using
`VMA_ASSERT_LEAK()`. This macro defaults to an assertion, triggering a typically fatal error in Debug
builds, and doing nothing in Release builds. You can provide your own definition of `VMA_ASSERT_LEAK()`
to change this behavior.

At memory block destruction time VMA lists out all unfreed allocations using the `VMA_LEAK_LOG_FORMAT()`
macro, which defaults to `VMA_DEBUG_LOG_FORMAT`, which in turn defaults to a no-op.
If you're having trouble with leaks - for example, the aforementioned assertion triggers, but you don't
quite know \em why -, overriding this macro to print out the the leaking blocks, combined with assigning
individual names to allocations using vmaSetAllocationName(), can greatly aid in fixing them.

\page other_api_interop Interop with other graphics APIs

VMA provides some features that help with interoperability with other graphics APIs, e.g. OpenGL.

\section opengl_interop_exporting_memory Exporting memory

If you want to attach `VkExportMemoryAllocateInfoKHR` or other structure to `pNext` chain of memory allocations made by the library:

You can create \ref custom_memory_pools for such allocations.
Define and fill in your `VkExportMemoryAllocateInfoKHR` structure and attach it to VmaPoolCreateInfo::pMemoryAllocateNext
while creating the custom pool.
Please note that the structure must remain alive and unchanged for the whole lifetime of the #VmaPool,
not only while creating it, as no copy of the structure is made,
but its original pointer is used for each allocation instead.

If you want to export all memory allocated by VMA from certain memory types,
also dedicated allocations or other allocations made from default pools,
an alternative solution is to fill in VmaAllocatorCreateInfo::pTypeExternalMemoryHandleTypes.
It should point to an array with `VkExternalMemoryHandleTypeFlagsKHR` to be automatically passed by the library
through `VkExportMemoryAllocateInfoKHR` on each allocation made from a specific memory type.
Please note that new versions of the library also support dedicated allocations created in custom pools.

You should not mix these two methods in a way that allows to apply both to the same memory type.
Otherwise, `VkExportMemoryAllocateInfoKHR` structure would be attached twice to the `pNext` chain of `VkMemoryAllocateInfo`.


\section opengl_interop_custom_alignment Custom alignment

Buffers or images exported to a different API like OpenGL may require a different alignment,
higher than the one used by the library automatically, queried from functions like `vkGetBufferMemoryRequirements`.
To impose such alignment:

You can create \ref custom_memory_pools for such allocations.
Set VmaPoolCreateInfo::minAllocationAlignment member to the minimum alignment required for each allocation
to be made out of this pool.
The alignment actually used will be the maximum of this member and the alignment returned for the specific buffer or image
from a function like `vkGetBufferMemoryRequirements`, which is called by VMA automatically.

If you want to create a buffer with a specific minimum alignment out of default pools,
use special function vmaCreateBufferWithAlignment(), which takes additional parameter `minAlignment`.

Note the problem of alignment affects only resources placed inside bigger `VkDeviceMemory` blocks and not dedicated
allocations, as these, by definition, always have alignment = 0 because the resource is bound to the beginning of its dedicated block.
You can ensure that an allocation is created as dedicated by using #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
Contrary to Direct3D 12, Vulkan doesn't have a concept of alignment of the entire memory block passed on its allocation.

\section opengl_interop_extended_allocation_information Extended allocation information

If you want to rely on VMA to allocate your buffers and images inside larger memory blocks,
but you need to know the size of the entire block and whether the allocation was made
with its own dedicated memory, use function vmaGetAllocationInfo2() to retrieve
extended allocation information in structure #VmaAllocationInfo2.



\page usage_patterns Recommended usage patterns

Vulkan gives great flexibility in memory allocation.
This chapter shows the most common patterns.

See also slides from talk:
[Sawicki, Adam. Advanced Graphics Techniques Tutorial: Memory management in Vulkan and DX12. Game Developers Conference, 2018](https://www.gdcvault.com/play/1025458/Advanced-Graphics-Techniques-Tutorial-New)


\section usage_patterns_gpu_only GPU-only resource

<b>When:</b>
Any resources that you frequently write and read on GPU,
e.g. images used as color attachments (aka "render targets"), depth-stencil attachments,
images/buffers used as storage image/buffer (aka "Unordered Access View (UAV)").

<b>What to do:</b>
Let the library select the optimal memory type, which will likely have `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.

\code
VkImageCreateInfo imgCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
imgCreateInfo.extent.width = 3840;
imgCreateInfo.extent.height = 2160;
imgCreateInfo.extent.depth = 1;
imgCreateInfo.mipLevels = 1;
imgCreateInfo.arrayLayers = 1;
imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
allocCreateInfo.priority = 1.0f;

VkImage img;
VmaAllocation alloc;
vmaCreateImage(allocator, &imgCreateInfo, &allocCreateInfo, &img, &alloc, nullptr);
\endcode

<b>Also consider:</b>
Consider creating them as dedicated allocations using #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
especially if they are large or if you plan to destroy and recreate them with different sizes
e.g. when display resolution changes.
Prefer to create such resources first and all other GPU resources (like textures and vertex buffers) later.
When VK_EXT_memory_priority extension is enabled, it is also worth setting high priority to such allocation
to decrease chances to be evicted to system memory by the operating system.

\section usage_patterns_staging_copy_upload Staging copy for upload

<b>When:</b>
A "staging" buffer than you want to map and fill from CPU code, then use as a source of transfer
to some GPU resource.

<b>What to do:</b>
Use flag #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT.
Let the library select the optimal memory type, which will always have `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`.

\code
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = 65536;
bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
    VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buf;
VmaAllocation alloc;
VmaAllocationInfo allocInfo;
vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, &allocInfo);

...

memcpy(allocInfo.pMappedData, myData, myDataSize);
\endcode

<b>Also consider:</b>
You can map the allocation using vmaMapMemory() or you can create it as persistenly mapped
using #VMA_ALLOCATION_CREATE_MAPPED_BIT, as in the example above.


\section usage_patterns_readback Readback

<b>When:</b>
Buffers for data written by or transferred from the GPU that you want to read back on the CPU,
e.g. results of some computations.

<b>What to do:</b>
Use flag #VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
Let the library select the optimal memory type, which will always have `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
and `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`.

\code
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = 65536;
bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
    VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buf;
VmaAllocation alloc;
VmaAllocationInfo allocInfo;
vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, &allocInfo);

...

const float* downloadedData = (const float*)allocInfo.pMappedData;
\endcode


\section usage_patterns_advanced_data_uploading Advanced data uploading

For resources that you frequently write on CPU via mapped pointer and
frequently read on GPU e.g. as a uniform buffer (also called "dynamic"), multiple options are possible:

-# Easiest solution is to have one copy of the resource in `HOST_VISIBLE` memory,
   even if it means system RAM (not `DEVICE_LOCAL`) on systems with a discrete graphics card,
   and make the device reach out to that resource directly.
   - Reads performed by the device will then go through PCI Express bus.
     The performance of this access may be limited, but it may be fine depending on the size
     of this resource (whether it is small enough to quickly end up in GPU cache) and the sparsity
     of access.
-# On systems with unified memory (e.g. AMD APU or Intel integrated graphics, mobile chips),
   a memory type may be available that is both `HOST_VISIBLE` (available for mapping) and `DEVICE_LOCAL`
   (fast to access from the GPU). Then, it is likely the best choice for such type of resource.
-# Systems with a discrete graphics card and separate video memory may or may not expose
   a memory type that is both `HOST_VISIBLE` and `DEVICE_LOCAL`, also known as Base Address Register (BAR).
   If they do, it represents a piece of VRAM (or entire VRAM, if ReBAR is enabled in the motherboard BIOS)
   that is available to CPU for mapping.
   - Writes performed by the host to that memory go through PCI Express bus.
     The performance of these writes may be limited, but it may be fine, especially on PCIe 4.0,
     as long as rules of using uncached and write-combined memory are followed - only sequential writes and no reads.
-# Finally, you may need or prefer to create a separate copy of the resource in `DEVICE_LOCAL` memory,
   a separate "staging" copy in `HOST_VISIBLE` memory and perform an explicit transfer command between them.

Thankfully, VMA offers an aid to create and use such resources in the the way optimal
for the current Vulkan device. To help the library make the best choice,
use flag #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT together with
#VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT.
It will then prefer a memory type that is both `DEVICE_LOCAL` and `HOST_VISIBLE` (integrated memory or BAR),
but if no such memory type is available or allocation from it fails
(PC graphics cards have only 256 MB of BAR by default, unless ReBAR is supported and enabled in BIOS),
it will fall back to `DEVICE_LOCAL` memory for fast GPU access.
It is then up to you to detect that the allocation ended up in a memory type that is not `HOST_VISIBLE`,
so you need to create another "staging" allocation and perform explicit transfers.

\code
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = 65536;
bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
    VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buf;
VmaAllocation alloc;
VmaAllocationInfo allocInfo;
VkResult result = vmaCreateBuffer(allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, &allocInfo);
// Check result...

VkMemoryPropertyFlags memPropFlags;
vmaGetAllocationMemoryProperties(allocator, alloc, &memPropFlags);

if(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
{
    // The Allocation ended up in a mappable memory.
    // Calling vmaCopyMemoryToAllocation() does vmaMapMemory(), memcpy(), vmaUnmapMemory(), and vmaFlushAllocation().
    result = vmaCopyMemoryToAllocation(allocator, myData, alloc, 0, myDataSize);
    // Check result...

    VkBufferMemoryBarrier bufMemBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufMemBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
    bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.buffer = buf;
    bufMemBarrier.offset = 0;
    bufMemBarrier.size = VK_WHOLE_SIZE;

    // It's important to insert a buffer memory barrier here to ensure writing to the buffer has finished.
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);
}
else
{
    // Allocation ended up in a non-mappable memory - a transfer using a staging buffer is required.
    VkBufferCreateInfo stagingBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    stagingBufCreateInfo.size = 65536;
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocCreateInfo = {};
    stagingAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuf;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingAllocInfo;
    result = vmaCreateBuffer(allocator, &stagingBufCreateInfo, &stagingAllocCreateInfo,
        &stagingBuf, &stagingAlloc, &stagingAllocInfo);
    // Check result...

    // Calling vmaCopyMemoryToAllocation() does vmaMapMemory(), memcpy(), vmaUnmapMemory(), and vmaFlushAllocation().
    result = vmaCopyMemoryToAllocation(allocator, myData, stagingAlloc, 0, myDataSize);
    // Check result...

    VkBufferMemoryBarrier bufMemBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier.buffer = stagingBuf;
    bufMemBarrier.offset = 0;
    bufMemBarrier.size = VK_WHOLE_SIZE;

    // Insert a buffer memory barrier to make sure writing to the staging buffer has finished.
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &bufMemBarrier, 0, nullptr);

    VkBufferCopy bufCopy = {
        0, // srcOffset
        0, // dstOffset,
        myDataSize, // size
    };

    vkCmdCopyBuffer(cmdBuf, stagingBuf, buf, 1, &bufCopy);

    VkBufferMemoryBarrier bufMemBarrier2 = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufMemBarrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufMemBarrier2.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT; // We created a uniform buffer
    bufMemBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufMemBarrier2.buffer = buf;
    bufMemBarrier2.offset = 0;
    bufMemBarrier2.size = VK_WHOLE_SIZE;

    // Make sure copying from staging buffer to the actual buffer has finished by inserting a buffer memory barrier.
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, 1, &bufMemBarrier2, 0, nullptr);
}
\endcode

\section usage_patterns_other_use_cases Other use cases

Here are some other, less obvious use cases and their recommended settings:

- An image that is used only as transfer source and destination, but it should stay on the device,
  as it is used to temporarily store a copy of some texture, e.g. from the current to the next frame,
  for temporal antialiasing or other temporal effects.
  - Use `VkImageCreateInfo::usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT`
  - Use VmaAllocationCreateInfo::usage = #VMA_MEMORY_USAGE_AUTO
- An image that is used only as transfer source and destination, but it should be placed
  in the system RAM despite it doesn't need to be mapped, because it serves as a "swap" copy to evict
  least recently used textures from VRAM.
  - Use `VkImageCreateInfo::usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT`
  - Use VmaAllocationCreateInfo::usage = #VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    as VMA needs a hint here to differentiate from the previous case.
- A buffer that you want to map and write from the CPU, directly read from the GPU
  (e.g. as a uniform or vertex buffer), but you have a clear preference to place it in device or
  host memory due to its large size.
  - Use `VkBufferCreateInfo::usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT`
  - Use VmaAllocationCreateInfo::usage = #VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE or #VMA_MEMORY_USAGE_AUTO_PREFER_HOST
  - Use VmaAllocationCreateInfo::flags = #VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT


\page configuration Configuration

Please check "CONFIGURATION SECTION" in the code to find macros that you can define
before each include of this file or change directly in this file to provide
your own implementation of basic facilities like assert, `min()` and `max()` functions,
mutex, atomic etc.

For example, define `VMA_ASSERT(expr)` before including the library to provide
custom implementation of the assertion, compatible with your project.
By default it is defined to standard C `assert(expr)` in `_DEBUG` configuration
and empty otherwise.

Similarly, you can define `VMA_LEAK_LOG_FORMAT` macro to enable printing of leaked (unfreed) allocations,
including their names and other parameters. Example:

\code
#define VMA_LEAK_LOG_FORMAT(format, ...) do { \
        printf((format), __VA_ARGS__); \
        printf("\n"); \
    } while(false)
\endcode

\section config_Vulkan_functions Pointers to Vulkan functions

There are multiple ways to import pointers to Vulkan functions in the library.
In the simplest case you don't need to do anything.
If the compilation or linking of your program or the initialization of the #VmaAllocator
doesn't work for you, you can try to reconfigure it.

First, the allocator tries to fetch pointers to Vulkan functions linked statically,
like this:

\code
m_VulkanFunctions.vkAllocateMemory = (PFN_vkAllocateMemory)vkAllocateMemory;
\endcode

If you want to disable this feature, set configuration macro: `#define VMA_STATIC_VULKAN_FUNCTIONS 0`.

Second, you can provide the pointers yourself by setting member VmaAllocatorCreateInfo::pVulkanFunctions.
You can fetch them e.g. using functions `vkGetInstanceProcAddr` and `vkGetDeviceProcAddr` or
by using a helper library like [volk](https://github.com/zeux/volk).

Third, VMA tries to fetch remaining pointers that are still null by calling
`vkGetInstanceProcAddr` and `vkGetDeviceProcAddr` on its own.
You need to only fill in VmaVulkanFunctions::vkGetInstanceProcAddr and VmaVulkanFunctions::vkGetDeviceProcAddr.
Other pointers will be fetched automatically.
If you want to disable this feature, set configuration macro: `#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0`.

Finally, all the function pointers required by the library (considering selected
Vulkan version and enabled extensions) are checked with `VMA_ASSERT` if they are not null.


\section custom_memory_allocator Custom host memory allocator

If you use custom allocator for CPU memory rather than default operator `new`
and `delete` from C++, you can make this library using your allocator as well
by filling optional member VmaAllocatorCreateInfo::pAllocationCallbacks. These
functions will be passed to Vulkan, as well as used by the library itself to
make any CPU-side allocations.

\section allocation_callbacks Device memory allocation callbacks

The library makes calls to `vkAllocateMemory()` and `vkFreeMemory()` internally.
You can setup callbacks to be informed about these calls, e.g. for the purpose
of gathering some statistics. To do it, fill optional member
VmaAllocatorCreateInfo::pDeviceMemoryCallbacks.

\section heap_memory_limit Device heap memory limit

When device memory of certain heap runs out of free space, new allocations may
fail (returning error code) or they may succeed, silently pushing some existing_
memory blocks from GPU VRAM to system RAM (which degrades performance). This
behavior is implementation-dependent - it depends on GPU vendor and graphics
driver.

On AMD cards it can be controlled while creating Vulkan device object by using
VK_AMD_memory_overallocation_behavior extension, if available.

Alternatively, if you want to test how your program behaves with limited amount of Vulkan device
memory available without switching your graphics card to one that really has
smaller VRAM, you can use a feature of this library intended for this purpose.
To do it, fill optional member VmaAllocatorCreateInfo::pHeapSizeLimit.



\page vk_khr_dedicated_allocation VK_KHR_dedicated_allocation

VK_KHR_dedicated_allocation is a Vulkan extension which can be used to improve
performance on some GPUs. It augments Vulkan API with possibility to query
driver whether it prefers particular buffer or image to have its own, dedicated
allocation (separate `VkDeviceMemory` block) for better efficiency - to be able
to do some internal optimizations. The extension is supported by this library.
It will be used automatically when enabled.

It has been promoted to core Vulkan 1.1, so if you use eligible Vulkan version
and inform VMA about it by setting VmaAllocatorCreateInfo::vulkanApiVersion,
you are all set.

Otherwise, if you want to use it as an extension:

1 . When creating Vulkan device, check if following 2 device extensions are
supported (call `vkEnumerateDeviceExtensionProperties()`).
If yes, enable them (fill `VkDeviceCreateInfo::ppEnabledExtensionNames`).

- VK_KHR_get_memory_requirements2
- VK_KHR_dedicated_allocation

If you enabled these extensions:

2 . Use #VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT flag when creating
your #VmaAllocator to inform the library that you enabled required extensions
and you want the library to use them.

\code
allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

vmaCreateAllocator(&allocatorInfo, &allocator);
\endcode

That is all. The extension will be automatically used whenever you create a
buffer using vmaCreateBuffer() or image using vmaCreateImage().

When using the extension together with Vulkan Validation Layer, you will receive
warnings like this:

_vkBindBufferMemory(): Binding memory to buffer 0x33 but vkGetBufferMemoryRequirements() has not been called on that buffer._

It is OK, you should just ignore it. It happens because you use function
`vkGetBufferMemoryRequirements2KHR()` instead of standard
`vkGetBufferMemoryRequirements()`, while the validation layer seems to be
unaware of it.

To learn more about this extension, see:

- [VK_KHR_dedicated_allocation in Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap50.html#VK_KHR_dedicated_allocation)
- [VK_KHR_dedicated_allocation unofficial manual](http://asawicki.info/articles/VK_KHR_dedicated_allocation.php5)



\page vk_ext_memory_priority VK_EXT_memory_priority

VK_EXT_memory_priority is a device extension that allows to pass additional "priority"
value to Vulkan memory allocations that the implementation may use prefer certain
buffers and images that are critical for performance to stay in device-local memory
in cases when the memory is over-subscribed, while some others may be moved to the system memory.

VMA offers convenient usage of this extension.
If you enable it, you can pass "priority" parameter when creating allocations or custom pools
and the library automatically passes the value to Vulkan using this extension.

If you want to use this extension in connection with VMA, follow these steps:

\section vk_ext_memory_priority_initialization Initialization

1) Call `vkEnumerateDeviceExtensionProperties` for the physical device.
Check if the extension is supported - if returned array of `VkExtensionProperties` contains "VK_EXT_memory_priority".

2) Call `vkGetPhysicalDeviceFeatures2` for the physical device instead of old `vkGetPhysicalDeviceFeatures`.
Attach additional structure `VkPhysicalDeviceMemoryPriorityFeaturesEXT` to `VkPhysicalDeviceFeatures2::pNext` to be returned.
Check if the device feature is really supported - check if `VkPhysicalDeviceMemoryPriorityFeaturesEXT::memoryPriority` is true.

3) While creating device with `vkCreateDevice`, enable this extension - add "VK_EXT_memory_priority"
to the list passed as `VkDeviceCreateInfo::ppEnabledExtensionNames`.

4) While creating the device, also don't set `VkDeviceCreateInfo::pEnabledFeatures`.
Fill in `VkPhysicalDeviceFeatures2` structure instead and pass it as `VkDeviceCreateInfo::pNext`.
Enable this device feature - attach additional structure `VkPhysicalDeviceMemoryPriorityFeaturesEXT` to
`VkPhysicalDeviceFeatures2::pNext` chain and set its member `memoryPriority` to `VK_TRUE`.

5) While creating #VmaAllocator with vmaCreateAllocator() inform VMA that you
have enabled this extension and feature - add #VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT
to VmaAllocatorCreateInfo::flags.

\section vk_ext_memory_priority_usage Usage

When using this extension, you should initialize following member:

- VmaAllocationCreateInfo::priority when creating a dedicated allocation with #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
- VmaPoolCreateInfo::priority when creating a custom pool.

It should be a floating-point value between `0.0f` and `1.0f`, where recommended default is `0.5F`.
Memory allocated with higher value can be treated by the Vulkan implementation as higher priority
and so it can have lower chances of being pushed out to system memory, experiencing degraded performance.

It might be a good idea to create performance-critical resources like color-attachment or depth-stencil images
as dedicated and set high priority to them. For example:

\code
VkImageCreateInfo imgCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
imgCreateInfo.extent.width = 3840;
imgCreateInfo.extent.height = 2160;
imgCreateInfo.extent.depth = 1;
imgCreateInfo.mipLevels = 1;
imgCreateInfo.arrayLayers = 1;
imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
allocCreateInfo.priority = 1.0f;

VkImage img;
VmaAllocation alloc;
vmaCreateImage(allocator, &imgCreateInfo, &allocCreateInfo, &img, &alloc, nullptr);
\endcode

`priority` member is ignored in the following situations:

- Allocations created in custom pools: They inherit the priority, along with all other allocation parameters
  from the parameters passed in #VmaPoolCreateInfo when the pool was created.
- Allocations created in default pools: They inherit the priority from the parameters
  VMA used when creating default pools, which means `priority == 0.5F`.


\page vk_amd_device_coherent_memory VK_AMD_device_coherent_memory

VK_AMD_device_coherent_memory is a device extension that enables access to
additional memory types with `VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD` and
`VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD` flag. It is useful mostly for
allocation of buffers intended for writing "breadcrumb markers" in between passes
or draw calls, which in turn are useful for debugging GPU crash/hang/TDR cases.

When the extension is available but has not been enabled, Vulkan physical device
still exposes those memory types, but their usage is forbidden. VMA automatically
takes care of that - it returns `VK_ERROR_FEATURE_NOT_PRESENT` when an attempt
to allocate memory of such type is made.

If you want to use this extension in connection with VMA, follow these steps:

\section vk_amd_device_coherent_memory_initialization Initialization

1) Call `vkEnumerateDeviceExtensionProperties` for the physical device.
Check if the extension is supported - if returned array of `VkExtensionProperties` contains "VK_AMD_device_coherent_memory".

2) Call `vkGetPhysicalDeviceFeatures2` for the physical device instead of old `vkGetPhysicalDeviceFeatures`.
Attach additional structure `VkPhysicalDeviceCoherentMemoryFeaturesAMD` to `VkPhysicalDeviceFeatures2::pNext` to be returned.
Check if the device feature is really supported - check if `VkPhysicalDeviceCoherentMemoryFeaturesAMD::deviceCoherentMemory` is true.

3) While creating device with `vkCreateDevice`, enable this extension - add "VK_AMD_device_coherent_memory"
to the list passed as `VkDeviceCreateInfo::ppEnabledExtensionNames`.

4) While creating the device, also don't set `VkDeviceCreateInfo::pEnabledFeatures`.
Fill in `VkPhysicalDeviceFeatures2` structure instead and pass it as `VkDeviceCreateInfo::pNext`.
Enable this device feature - attach additional structure `VkPhysicalDeviceCoherentMemoryFeaturesAMD` to
`VkPhysicalDeviceFeatures2::pNext` and set its member `deviceCoherentMemory` to `VK_TRUE`.

5) While creating #VmaAllocator with vmaCreateAllocator() inform VMA that you
have enabled this extension and feature - add #VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT
to VmaAllocatorCreateInfo::flags.

\section vk_amd_device_coherent_memory_usage Usage

After following steps described above, you can create VMA allocations and custom pools
out of the special `DEVICE_COHERENT` and `DEVICE_UNCACHED` memory types on eligible
devices. There are multiple ways to do it, for example:

- You can request or prefer to allocate out of such memory types by adding
  `VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD` to VmaAllocationCreateInfo::requiredFlags
  or VmaAllocationCreateInfo::preferredFlags. Those flags can be freely mixed with
  other ways of \ref choosing_memory_type, like setting VmaAllocationCreateInfo::usage.
- If you manually found memory type index to use for this purpose, force allocation
  from this specific index by setting VmaAllocationCreateInfo::memoryTypeBits `= 1U << index`.

\section vk_amd_device_coherent_memory_more_information More information

To learn more about this extension, see [VK_AMD_device_coherent_memory in Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_AMD_device_coherent_memory.html)

Example use of this extension can be found in the code of the sample and test suite
accompanying this library.


\page vk_khr_external_memory_win32 VK_KHR_external_memory_win32

On Windows, the VK_KHR_external_memory_win32 device extension allows exporting a Win32 `HANDLE`
of a `VkDeviceMemory` block, to be able to reference the memory on other Vulkan logical devices or instances,
in multiple processes, and/or in multiple APIs.
VMA offers support for it.

\section vk_khr_external_memory_win32_initialization Initialization

1) Make sure the extension is defined in the code by including following header before including VMA:

\code
#include <vulkan/vulkan_win32.h>
\endcode

2) Check if "VK_KHR_external_memory_win32" is available among device extensions.
Enable it when creating the `VkDevice` object.

3) Enable the usage of this extension in VMA by setting flag #VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT
when calling vmaCreateAllocator().

4) Make sure that VMA has access to the `vkGetMemoryWin32HandleKHR` function by either enabling `VMA_DYNAMIC_VULKAN_FUNCTIONS` macro
or setting VmaVulkanFunctions::vkGetMemoryWin32HandleKHR explicitly.
For more information, see \ref quick_start_initialization_importing_vulkan_functions.

\section vk_khr_external_memory_win32_preparations Preparations

You can find example usage among tests, in file "Tests.cpp", function `TestWin32Handles()`.

To use the extension, buffers need to be created with `VkExternalMemoryBufferCreateInfoKHR` attached to their `pNext` chain,
and memory allocations need to be made with `VkExportMemoryAllocateInfoKHR` attached to their `pNext` chain.
To make use of them, you need to use \ref custom_memory_pools. Example:

\code
// Define an example buffer and allocation parameters.
VkExternalMemoryBufferCreateInfoKHR externalMemBufCreateInfo = {
    VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR,
    nullptr,
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
};
VkBufferCreateInfo exampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
exampleBufCreateInfo.size = 0x10000; // Doesn't matter here.
exampleBufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
exampleBufCreateInfo.pNext = &externalMemBufCreateInfo;

VmaAllocationCreateInfo exampleAllocCreateInfo = {};
exampleAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

// Find memory type index to use for the custom pool.
uint32_t memTypeIndex;
VkResult res = vmaFindMemoryTypeIndexForBufferInfo(g_Allocator,
    &exampleBufCreateInfo, &exampleAllocCreateInfo, &memTypeIndex);
// Check res...

// Create a custom pool.
constexpr static VkExportMemoryAllocateInfoKHR exportMemAllocInfo = {
    VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
    nullptr,
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
};
VmaPoolCreateInfo poolCreateInfo = {};
poolCreateInfo.memoryTypeIndex = memTypeIndex;
poolCreateInfo.pMemoryAllocateNext = (void*)&exportMemAllocInfo;

VmaPool pool;
res = vmaCreatePool(g_Allocator, &poolCreateInfo, &pool);
// Check res...

// YOUR OTHER CODE COMES HERE....

// At the end, don't forget to destroy it!
vmaDestroyPool(g_Allocator, pool);
\endcode

Note that the structure passed as VmaPoolCreateInfo::pMemoryAllocateNext must remain alive and unchanged
for the whole lifetime of the custom pool, because it will be used when the pool allocates a new device memory block.
No copy is made internally. This is why variable `exportMemAllocInfo` is defined as `static`.

\section vk_khr_external_memory_win32_memory_allocation Memory allocation

Finally, you can create a buffer with an allocation out of the custom pool.
The buffer should use same flags as the sample buffer used to find the memory type.
It should also specify `VkExternalMemoryBufferCreateInfoKHR` in its `pNext` chain.

\code
VkExternalMemoryBufferCreateInfoKHR externalMemBufCreateInfo = {
    VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR,
    nullptr,
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
};
VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufCreateInfo.size = // Your desired buffer size.
bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
bufCreateInfo.pNext = &externalMemBufCreateInfo;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.pool = pool;  // It is enough to set this one member.

VkBuffer buf;
VmaAllocation alloc;
res = vmaCreateBuffer(g_Allocator, &bufCreateInfo, &allocCreateInfo, &buf, &alloc, nullptr);
// Check res...

// YOUR OTHER CODE COMES HERE....

// At the end, don't forget to destroy it!
vmaDestroyBuffer(g_Allocator, buf, alloc);
\endcode

If you need each allocation to have its own device memory block and start at offset 0, you can still do
by using #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT flag. It works also with custom pools.

\section vk_khr_external_memory_win32_exporting_win32_handle Exporting Win32 handle

After the allocation is created, you can acquire a Win32 `HANDLE` to the `VkDeviceMemory` block it belongs to.
VMA function vmaGetMemoryWin32Handle() is a replacement of the Vulkan function `vkGetMemoryWin32HandleKHR`.

\code
HANDLE handle;
res = vmaGetMemoryWin32Handle(g_Allocator, alloc, nullptr, &handle);
// Check res...

// YOUR OTHER CODE COMES HERE....

// At the end, you must close the handle.
CloseHandle(handle);
\endcode

Documentation of the VK_KHR_external_memory_win32 extension states that:

> If handleType is defined as an NT handle, vkGetMemoryWin32HandleKHR must be called no more than once for each valid unique combination of memory and handleType.

This is ensured automatically inside VMA.
The library fetches the handle on first use, remembers it internally, and closes it when the memory block or dedicated allocation is destroyed.
Every time you call vmaGetMemoryWin32Handle(), VMA calls `DuplicateHandle` and returns a new handle that you need to close.

For further information, please check documentation of the vmaGetMemoryWin32Handle() function.


\page enabling_buffer_device_address Enabling buffer device address

Device extension VK_KHR_buffer_device_address
allow to fetch raw GPU pointer to a buffer and pass it for usage in a shader code.
It has been promoted to core Vulkan 1.2.

If you want to use this feature in connection with VMA, follow these steps:

\section enabling_buffer_device_address_initialization Initialization

1) (For Vulkan version < 1.2) Call `vkEnumerateDeviceExtensionProperties` for the physical device.
Check if the extension is supported - if returned array of `VkExtensionProperties` contains
"VK_KHR_buffer_device_address".

2) Call `vkGetPhysicalDeviceFeatures2` for the physical device instead of old `vkGetPhysicalDeviceFeatures`.
Attach additional structure `VkPhysicalDeviceBufferDeviceAddressFeatures*` to `VkPhysicalDeviceFeatures2::pNext` to be returned.
Check if the device feature is really supported - check if `VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress` is true.

3) (For Vulkan version < 1.2) While creating device with `vkCreateDevice`, enable this extension - add
"VK_KHR_buffer_device_address" to the list passed as `VkDeviceCreateInfo::ppEnabledExtensionNames`.

4) While creating the device, also don't set `VkDeviceCreateInfo::pEnabledFeatures`.
Fill in `VkPhysicalDeviceFeatures2` structure instead and pass it as `VkDeviceCreateInfo::pNext`.
Enable this device feature - attach additional structure `VkPhysicalDeviceBufferDeviceAddressFeatures*` to
`VkPhysicalDeviceFeatures2::pNext` and set its member `bufferDeviceAddress` to `VK_TRUE`.

5) While creating #VmaAllocator with vmaCreateAllocator() inform VMA that you
have enabled this feature - add #VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
to VmaAllocatorCreateInfo::flags.

\section enabling_buffer_device_address_usage Usage

After following steps described above, you can create buffers with `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT*` using VMA.
The library automatically adds `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT*` to
allocated memory blocks wherever it might be needed.

Please note that the library supports only `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT*`.
The second part of this functionality related to "capture and replay" is not supported,
as it is intended for usage in debugging tools like RenderDoc, not in everyday Vulkan usage.

\section enabling_buffer_device_address_more_information More information

To learn more about this extension, see [VK_KHR_buffer_device_address in Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap46.html#VK_KHR_buffer_device_address)

Example use of this extension can be found in the code of the sample and test suite
accompanying this library.

\page general_considerations General considerations

\section general_considerations_thread_safety Thread safety

- The library has no global state, so separate #VmaAllocator objects can be used
  independently.
  There should be no need to create multiple such objects though - one per `VkDevice` is enough.
- By default, all calls to functions that take #VmaAllocator as first parameter
  are safe to call from multiple threads simultaneously because they are
  synchronized internally when needed.
  This includes allocation and deallocation from default memory pool, as well as custom #VmaPool.
- When the allocator is created with #VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT
  flag, calls to functions that take such #VmaAllocator object must be
  synchronized externally.
- Access to a #VmaAllocation object must be externally synchronized. For example,
  you must not call vmaGetAllocationInfo() and vmaMapMemory() from different
  threads at the same time if you pass the same #VmaAllocation object to these
  functions.
- #VmaVirtualBlock is not safe to be used from multiple threads simultaneously.

\section general_considerations_versioning_and_compatibility Versioning and compatibility

The library uses [**Semantic Versioning**](https://semver.org/),
which means version numbers follow convention: Major.Minor.Patch (e.g. 2.3.0), where:

- Incremented Patch version means a release is backward- and forward-compatible,
  introducing only some internal improvements, bug fixes, optimizations etc.
  or changes that are out of scope of the official API described in this documentation.
- Incremented Minor version means a release is backward-compatible,
  so existing code that uses the library should continue to work, while some new
  symbols could have been added: new structures, functions, new values in existing
  enums and bit flags, new structure members, but not new function parameters.
- Incrementing Major version means a release could break some backward compatibility.

All changes between official releases are documented in file "CHANGELOG.md".

\warning Backward compatibility is considered on the level of C++ source code, not binary linkage.
Adding new members to existing structures is treated as backward compatible if initializing
the new members to binary zero results in the old behavior.
You should always fully initialize all library structures to zeros and not rely on their
exact binary size.

\section general_considerations_validation_layer_warnings Validation layer warnings

When using this library, you can meet following types of warnings issued by
Vulkan validation layer. They don't necessarily indicate a bug, so you may need
to just ignore them.

- *vkBindBufferMemory(): Binding memory to buffer 0xeb8e4 but vkGetBufferMemoryRequirements() has not been called on that buffer.*
  - It happens when VK_KHR_dedicated_allocation extension is enabled.
    `vkGetBufferMemoryRequirements2KHR` function is used instead, while validation layer seems to be unaware of it.
- *Mapping an image with layout VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL can result in undefined behavior if this memory is used by the device. Only GENERAL or PREINITIALIZED should be used.*
  - It happens when you map a buffer or image, because the library maps entire
    `VkDeviceMemory` block, where different types of images and buffers may end
    up together, especially on GPUs with unified memory like Intel.
- *Non-linear image 0xebc91 is aliased with linear buffer 0xeb8e4 which may indicate a bug.*
  - It may happen when you use [defragmentation](@ref defragmentation).

\section general_considerations_allocation_algorithm Allocation algorithm

The library uses following algorithm for allocation, in order:

-# Try to find free range of memory in existing blocks.
-# If failed, try to create a new block of `VkDeviceMemory`, with preferred block size.
-# If failed, try to create such block with size / 2, size / 4, size / 8.
-# If failed, try to allocate separate `VkDeviceMemory` for this allocation,
   just like when you use #VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT.
-# If failed, choose other memory type that meets the requirements specified in
   VmaAllocationCreateInfo and go to point 1.
-# If failed, return `VK_ERROR_OUT_OF_DEVICE_MEMORY`.

\section general_considerations_features_not_supported Features not supported

Features deliberately excluded from the scope of this library:

-# **Data transfer.** Uploading (streaming) and downloading data of buffers and images
   between CPU and GPU memory and related synchronization is responsibility of the user.
   Defining some "texture" object that would automatically stream its data from a
   staging copy in CPU memory to GPU memory would rather be a feature of another,
   higher-level library implemented on top of VMA.
   VMA doesn't record any commands to a `VkCommandBuffer`. It just allocates memory.
-# **Recreation of buffers and images.** Although the library has functions for
   buffer and image creation: vmaCreateBuffer(), vmaCreateImage(), you need to
   recreate these objects yourself after defragmentation. That is because the big
   structures `VkBufferCreateInfo`, `VkImageCreateInfo` are not stored in
   #VmaAllocation object.
-# **Handling CPU memory allocation failures.** When dynamically creating small C++
   objects in CPU memory (not Vulkan memory), allocation failures are not checked
   and handled gracefully, because that would complicate code significantly and
   is usually not needed in desktop PC applications anyway.
   Success of an allocation is just checked with an assert.
-# **Code free of any compiler warnings.** Maintaining the library to compile and
   work correctly on so many different platforms is hard enough. Being free of
   any warnings, on any version of any compiler, is simply not feasible.
   There are many preprocessor macros that make some variables unused, function parameters unreferenced,
   or conditional expressions constant in some configurations.
   The code of this library should not be bigger or more complicated just to silence these warnings.
   It is recommended to disable such warnings instead.
-# This is a C++ library with C interface. **Bindings or ports to any other programming languages** are welcome as external projects but
   are not going to be included into this repository.
*/
