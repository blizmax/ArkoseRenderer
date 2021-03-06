#pragma once

#include "backend/Resources.h"
#include "utility/Badge.h"
#include "utility/util.h"
#include <memory>
#include <vector>

class Backend {
public:
    Backend() = default;
    virtual ~Backend() = default;

    enum class Type {
        Vulkan
    };

    enum class Capability {
        RtxRayTracing,
        Shader16BitFloat,
        ShaderTextureArrayDynamicIndexing,
        ShaderBufferArrayDynamicIndexing,
    };

    static std::string capabilityName(Capability capability);
    virtual bool hasActiveCapability(Capability) const = 0;
    virtual bool executeFrame(double elapsedTime, double deltaTime) = 0;

    virtual std::unique_ptr<Buffer> createBuffer(size_t, Buffer::Usage, Buffer::MemoryHint) = 0;
    virtual std::unique_ptr<RenderTarget> createRenderTarget(std::vector<RenderTarget::Attachment>) = 0;
    virtual std::unique_ptr<Texture> createTexture(Texture::TextureDescription) = 0;
    virtual std::unique_ptr<BindingSet> createBindingSet(std::vector<ShaderBinding>) = 0;
    virtual std::unique_ptr<RenderState> createRenderState(const RenderTarget&, const VertexLayout&, const Shader&, std::vector<BindingSet*>,
                                                           const Viewport&, const BlendState&, const RasterState&, const DepthState&)
        = 0;
    virtual std::unique_ptr<BottomLevelAS> createBottomLevelAccelerationStructure(std::vector<RTGeometry>) = 0;
    virtual std::unique_ptr<TopLevelAS> createTopLevelAccelerationStructure(std::vector<RTGeometryInstance>) = 0;
    virtual std::unique_ptr<RayTracingState> createRayTracingState(ShaderBindingTable& sbt, std::vector<BindingSet*>, uint32_t maxRecursionDepth) = 0;
    virtual std::unique_ptr<ComputeState> createComputeState(const Shader&, std::vector<BindingSet*>) = 0;

protected:
    [[nodiscard]] static Badge<Backend> badge()
    {
        return {};
    }
};
