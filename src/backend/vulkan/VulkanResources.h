#pragma once

#include "rendering/Resources.h"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct VulkanBuffer final : public Buffer {
public:
    VulkanBuffer(Backend&, size_t size, Usage, MemoryHint);
    virtual ~VulkanBuffer() override;

    void updateData(const std::byte* data, size_t size) override;

    VkBuffer buffer;
    VmaAllocation allocation;
};

struct VulkanTexture final : public Texture {
public:
    VulkanTexture() = default;
    VulkanTexture(Backend&, Extent2D, Format, Usage, MinFilter, MagFilter, Mipmap, Multisampling);
    virtual ~VulkanTexture() override;

    void setPixelData(vec4 pixel) override;
    void setData(const std::byte* data, size_t size) override;
    void setData(const float* data, size_t size) override;

    VkImage image;
    VmaAllocation allocation;

    VkFormat vkFormat;
    VkImageView imageView;
    VkSampler sampler;

    VkImageLayout currentLayout;
};

struct VulkanRenderTarget final : public RenderTarget {
public:
    VulkanRenderTarget() = default;
    explicit VulkanRenderTarget(Backend&, std::vector<Attachment> attachments);
    virtual ~VulkanRenderTarget() override;

    VkFramebuffer framebuffer;
    VkRenderPass compatibleRenderPass;

    std::vector<std::pair<const Texture*, VkImageLayout>> attachedTextures;
};

struct VulkanBindingSet : public BindingSet {
public:
    VulkanBindingSet(Backend&, std::vector<ShaderBinding>);
    virtual ~VulkanBindingSet() override;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
};

struct VulkanRenderState final : public RenderState {
public:
    VulkanRenderState(Backend&, const RenderTarget&, VertexLayout, Shader, const std::vector<const BindingSet*>&,
                      Viewport, BlendState, RasterState, DepthState);
    virtual ~VulkanRenderState() override;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    std::vector<const Texture*> sampledTextures;
};

struct VulkanTopLevelAS final : public TopLevelAS {
public:
    VulkanTopLevelAS(Backend&, std::vector<RTGeometryInstance>);
    virtual ~VulkanTopLevelAS() override;

    VkAccelerationStructureNV accelerationStructure;
    VkDeviceMemory memory;
    uint64_t handle;

    std::vector<std::pair<VkBuffer, VmaAllocation>> associatedBuffers;
};

struct VulkanBottomLevelAS final : public BottomLevelAS {
public:
    VulkanBottomLevelAS(Backend&, std::vector<RTGeometry>);
    virtual ~VulkanBottomLevelAS() override;

    VkAccelerationStructureNV accelerationStructure;
    VkDeviceMemory memory;
    uint64_t handle;

    std::vector<std::pair<VkBuffer, VmaAllocation>> associatedBuffers;
};

struct VulkanRayTracingState final : public RayTracingState {
public:
    VulkanRayTracingState(Backend&, ShaderBindingTable, std::vector<const BindingSet*>, uint32_t maxRecursionDepth);
    virtual ~VulkanRayTracingState() override;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkBuffer sbtBuffer;
    VmaAllocation sbtBufferAllocation;

    std::vector<const Texture*> sampledTextures;
    std::vector<const Texture*> storageImages;
};

struct VulkanComputeState final : public ComputeState {
public:
    VulkanComputeState(Backend&, Shader, std::vector<const BindingSet*>);
    virtual ~VulkanComputeState() override;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    std::vector<const Texture*> storageImages;
};
