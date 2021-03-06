#pragma once

#include "rendering/RenderGraphNode.h"
#include "rendering/camera/FpsCamera.h"
#include "rendering/scene/Model.h"
#include "rendering/scene/Scene.h"

class ShadowMapNode final : public RenderGraphNode {
public:
    explicit ShadowMapNode(Scene&);
    ~ShadowMapNode() override = default;

    static std::string name();

    std::optional<std::string> displayName() const override { return "Shadow Mapping"; }

    ExecuteCallback constructFrame(Registry&) const override;

private:
    Scene& m_scene;
};
