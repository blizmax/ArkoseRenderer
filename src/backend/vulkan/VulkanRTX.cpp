#include "VulkanRTX.h"

#include "backend/vulkan/VulkanBackend.h"
#include "utility/Logging.h"
#include <vector>

VulkanRTX::VulkanRTX(VulkanBackend& backend, VkPhysicalDevice physicalDevice, VkDevice device)
    : m_backend(backend)
    , m_physicalDevice(physicalDevice)
    , m_device(device)
{
    VkPhysicalDeviceProperties2 deviceProps2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    deviceProps2.pNext = &m_rayTracingProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProps2);

    vkCreateAccelerationStructureNV = reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureNV"));
    vkDestroyAccelerationStructureNV = reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureNV"));
    vkBindAccelerationStructureMemoryNV = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(vkGetDeviceProcAddr(m_device, "vkBindAccelerationStructureMemoryNV"));
    vkGetAccelerationStructureHandleNV = reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureHandleNV"));
    vkGetAccelerationStructureMemoryRequirementsNV = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureMemoryRequirementsNV"));
    vkCmdBuildAccelerationStructureNV = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructureNV"));
    vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesNV"));
    vkGetRayTracingShaderGroupHandlesNV = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(vkGetDeviceProcAddr(m_device, "vkGetRayTracingShaderGroupHandlesNV"));
    vkCmdTraceRaysNV = reinterpret_cast<PFN_vkCmdTraceRaysNV>(vkGetDeviceProcAddr(m_device, "vkCmdTraceRaysNV"));
}

bool VulkanRTX::isSupportedOnPhysicalDevice(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions { extensionCount };
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    bool rayTracing = false;
    bool memRequirements2 = false;

    for (auto& extension : availableExtensions) {
        if (std::strcmp(extension.extensionName, VK_NV_RAY_TRACING_EXTENSION_NAME) == 0) {
            rayTracing = true;
        } else if (std::strcmp(extension.extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0) {
            memRequirements2 = true;
        }

        if (rayTracing && memRequirements2) {
            return true;
        }
    }

    return false;
}

const VkPhysicalDeviceRayTracingPropertiesNV& VulkanRTX::properties() const
{
    return m_rayTracingProperties;
}

VkBuffer VulkanRTX::createInstanceBuffer(const std::vector<RTGeometryInstance>& instances, VmaAllocation& allocation) const
{
    std::vector<VulkanRTX::GeometryInstance> instanceData {};

    for (size_t instanceIdx = 0; instanceIdx < instances.size(); ++instanceIdx) {
        auto& instance = instances[instanceIdx];
        VulkanRTX::GeometryInstance data {};

        data.transform = transpose(instance.transform.worldMatrix());

        auto& vulkanBlas = static_cast<const VulkanBottomLevelAS&>(instance.blas);
        data.accelerationStructureHandle = vulkanBlas.handle;

        // TODO: We already have gl_InstanceID for this running index, and this sets gl_InstanceCustomIndexNV.
        //  Here we instead want some other type of data. Probably something that can be passed in.
        data.instanceId = instance.customInstanceId;

        data.mask = instance.hitMask;
        data.flags = 0; //VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;

        data.instanceOffset = instance.shaderBindingTableOffset;

        instanceData.push_back(data);
    }

    VkDeviceSize totalSize = instanceData.size() * sizeof(VulkanRTX::GeometryInstance);

    VkBufferCreateInfo instanceBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    instanceBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    instanceBufferCreateInfo.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
    instanceBufferCreateInfo.size = totalSize;

    if (vulkanDebugMode) {
        // for nsight debugging & similar stuff)
        instanceBufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VmaAllocationCreateInfo instanceAllocCreateInfo = {};
    instanceAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer instanceBuffer;
    if (vmaCreateBuffer(m_backend.globalAllocator(), &instanceBufferCreateInfo, &instanceAllocCreateInfo, &instanceBuffer, &allocation, nullptr) != VK_SUCCESS) {
        LogError("Could not create RTX instance buffer.\n");
    }

    if (!m_backend.setBufferMemoryUsingMapping(allocation, instanceData.data(), totalSize)) {
        LogError("Could not set RTX instance instance buffer data.\n");
    }

    return instanceBuffer;
}

VkBuffer VulkanRTX::createScratchBufferForAccelerationStructure(VkAccelerationStructureNV accelerationStructure, bool updateInPlace, VmaAllocation& allocation) const
{
    VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
    memoryRequirementsInfo.type = updateInPlace
        ? VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV
        : VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

    VkMemoryRequirements2 scratchMemRequirements2;
    memoryRequirementsInfo.accelerationStructure = accelerationStructure;
    vkGetAccelerationStructureMemoryRequirementsNV(m_backend.device(), &memoryRequirementsInfo, &scratchMemRequirements2);

    VkBufferCreateInfo scratchBufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    scratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    scratchBufferCreateInfo.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
    scratchBufferCreateInfo.size = scratchMemRequirements2.memoryRequirements.size;

    if (vulkanDebugMode) {
        // for nsight debugging & similar stuff)
        scratchBufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VmaAllocationCreateInfo scratchAllocCreateInfo = {};
    scratchAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBuffer scratchBuffer;
    if (vmaCreateBuffer(m_backend.globalAllocator(), &scratchBufferCreateInfo, &scratchAllocCreateInfo, &scratchBuffer, &allocation, nullptr) != VK_SUCCESS) {
        LogError("VulkanBackend::createScratchBufferForAccelerationStructure(): could not create scratch buffer.\n");
    }

    return scratchBuffer;
}
