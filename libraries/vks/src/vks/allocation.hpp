#pragma once

#include <vulkan/vulkan.hpp>
#include "../vma/vk_mem_alloc.h"

namespace vks {

// A wrapper class for an allocation, either an Image or Buffer.  Not intended to be used used directly
// but only as a base class providing common functionality for the classes below.
//
// Provides easy to use mechanisms for mapping, unmapping and copying host data to the device memory
struct Allocation {
    vk::Device device;
    VmaAllocator allocator{ nullptr };
    VmaAllocation allocation{ nullptr };
    VmaAllocationInfo info{ };
    vk::DeviceSize size{ 0 };
    vk::DeviceSize alignment{ 0 };
    vk::DeviceSize allocSize{ 0 };
    void* mapped{ nullptr };
    /** @brief Memory propertys flags to be filled by external source at buffer creation (to query at some later point) */
    vk::MemoryPropertyFlags memoryPropertyFlags;

    template <typename T = void>
    inline T* map(size_t offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) {
        vmaMapMemory(allocator, allocation, &mapped);
        return (T*)mapped;
    }

    inline void unmap() {
        vmaUnmapMemory(allocator, allocation);
        mapped = nullptr;
    }

    inline void copy(size_t size, const void* data, VkDeviceSize offset = 0) const {
        memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
    }

    template <typename T>
    inline void copy(const T& data, VkDeviceSize offset = 0) const {
        copy(sizeof(T), &data, offset);
    }

    template <typename T>
    inline void copy(const std::vector<T>& data, VkDeviceSize offset = 0) const {
        copy(sizeof(T) * data.size(), data.data(), offset);
    }

    /**
        * Invalidate a memory range of the buffer to make it visible to the host
        *
        * @note Only required for non-coherent memory
        *
        * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
        * @param offset (Optional) Byte offset from beginning
        *
        * @return VkResult of the invalidate call
        */
    void invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) {
        VkMemoryPropertyFlags memFlags;
        vmaGetMemoryTypeProperties(allocator, info.memoryType, &memFlags);
        if ((memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
            device.flushMappedMemoryRanges(vk::MappedMemoryRange{ vk::DeviceMemory{ info.deviceMemory }, info.offset + offset, size == VK_WHOLE_SIZE ? info.size : size });
        }
    }

    virtual void destroy() {
        if (nullptr != mapped) {
            unmap();
        }
        if (allocator && allocation) {
            vmaFreeMemory(allocator, allocation);
        }
    }
};
}  // namespace vks
