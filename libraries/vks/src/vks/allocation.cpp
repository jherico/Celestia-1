#include "allocation.hpp"
#include "../vma/vk_mem_alloc.h"


namespace vks {
    struct SimpleAllocation : public Allocation {
        SimpleAllocation(const vk::Device& device) : device(device) {};
        vk::DeviceMemory memory;
        vk::Device device;

        void* map(vk::DeviceSize offset, vk::DeviceSize size) {
            mapped = device.mapMemory(memory, offset, size, {});
        }

        void unmap() {
            device.unmapMemory(memory);
        }
    };
}

