#include "GBufferNode.h"

GBufferNode::GBufferNode(const Scene&)
    : RenderGraphNode(GBufferNode::name())
{
}

std::string GBufferNode::name()
{
    return "g-buffer";
}

RenderGraphNode::ExecuteCallback GBufferNode::constructFrame(Registry& reg) const
{
    const RenderTarget& windowTarget = reg.windowRenderTarget();

    Texture& normalTexture = reg.createTexture2D(windowTarget.extent(), Texture::Format::RGBA16F);
    reg.publish("normal", normalTexture);

    Texture& depthTexture = reg.createTexture2D(windowTarget.extent(), Texture::Format::Depth32F);
    reg.publish("depth", depthTexture);

    Texture& baseColorTexture = reg.createTexture2D(windowTarget.extent(), Texture::Format::RGBA8);
    reg.publish("baseColor", baseColorTexture);

    return [&](const AppState& appState, CommandList& cmdList) {};
}
