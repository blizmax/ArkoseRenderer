#pragma once

#include "../RenderGraphNode.h"
#include "rendering/camera/FpsCamera.h"
#include "rendering/scene/Model.h"
#include "rendering/scene/Scene.h"

class DiffuseGINode final : public RenderGraphNode {
public:
    DiffuseGINode(Scene&);

    static std::string name();
    std::optional<std::string> displayName() const override { return "Diffuse GI"; }

    void constructNode(Registry&) override;
    ExecuteCallback constructFrame(Registry&) const override;

private:
    struct Vertex {
        vec3 position;
        vec2 texCoord;
        vec3 normal;
    };

    VertexLayout vertexLayout {
        sizeof(Vertex),
        { { 0, VertexAttributeType::Float3, offsetof(Vertex, position) },
          { 1, VertexAttributeType::Float2, offsetof(Vertex, texCoord) },
          { 2, VertexAttributeType ::Float3, offsetof(Vertex, normal) } }
    };

    SemanticVertexLayout semanticVertexLayout { VertexComponent::Position3F,
                                                VertexComponent::TexCoord2F,
                                                VertexComponent::Normal3F };

    Scene& m_scene;

    uint32_t getProbeIndexForNextToRender() const;

    Texture* m_irradianceProbes;
    Texture* m_filteredDistanceProbes;
};
